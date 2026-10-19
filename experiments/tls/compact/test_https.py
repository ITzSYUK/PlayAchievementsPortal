"""Loopback only; no account, RA condition logic or external API calls."""
from pathlib import Path
import sys
import tempfile
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import ec

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from test_tls import certificate, write_pair, run


def main():
    binary = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='ra-compact-') as temporary:
        root = Path(temporary)
        key = ec.generate_private_key(ec.SECP384R1())
        ca = certificate(key, 'compact test root', None, None, ca=True)
        ca_file = root / 'root.der'
        ca_file.write_bytes(ca.public_bytes(serialization.Encoding.DER))
        leaf_key = ec.generate_private_key(ec.SECP256R1())
        valid = write_pair(root, 'valid', leaf_key, certificate(leaf_key, 'localhost', ca.subject, key))
        expired = write_pair(root, 'expired', leaf_key, certificate(leaf_key, 'localhost', ca.subject, key, expired=True))
        unknown = write_pair(root, 'unknown', leaf_key, certificate(leaf_key, 'localhost', None, None))
        cases = [
            ('verified body', valid, {}, 0),
            ('chunked body', valid, {'response': b'HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n2\r\nOK\r\n0\r\n\r\n'}, 0),
            ('extensions and trailers', valid, {'response': b'HTTP/1.1 200 OK\r\nX_Test: DIAG_SECRET\r\nTransfer-Encoding: chunked\r\n\r\n2;id="ignored"\r\nOK\r\n0;end\r\nX-Check: DIAG_SECRET\r\n\r\n'}, 0),
            ('authenticated close body', valid, {'response': b'HTTP/1.1 200 OK\r\n\r\nOK', 'clean_close': True}, 0),
            ('unannounced close', valid, {'response': b'HTTP/1.1 200 OK\r\n\r\nOK'}, 1),
            ('wrong hostname', valid, {'hostname': 'wrong.local'}, 1),
            ('expired', expired, {}, 1),
            ('untrusted', unknown, {}, 1),
            ('clock unavailable', valid, {'utc_epoch': -1}, 1),
            ('entropy unavailable', valid, {'entropy_failure': True}, 1),
            ('handshake deadline', valid, {'silent': True, 'timeout': 200}, 1),
            ('timeout cleanup then fresh retry', valid, {'retry': True, 'timeout': 200}, 0),
            ('body overflow', valid, {'response': b'HTTP/1.1 200 OK\r\nContent-Length: 262145\r\n\r\n'}, 1),
            ('PSP observed chunk', valid, {'response': b'HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n' + f'{129891:x}\r\n'.encode() + b'x' * 129891 + b'\r\n0\r\n\r\n'}, 0),
            ('maximum length body', valid, {'response': b'HTTP/1.1 200 OK\r\nContent-Length: 262144\r\n\r\n' + b'x' * 262144}, 0),
            ('maximum chunk body', valid, {'response': b'HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n40000\r\n' + b'x' * 262144 + b'\r\n0\r\n\r\n'}, 0),
            ('chunk overflow', valid, {'response': b'HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n40001\r\n'}, 1),
            ('aggregate chunk overflow', valid, {'response': b'HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n3fff6\r\n' + b'x' * 262134 + b'\r\n14\r\n'}, 1),
            ('truncated body', valid, {'response': b'HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nOK', 'clean_close': True}, 1),
            ('ambiguous framing', valid, {'response': b'HTTP/1.1 200 OK\r\nContent-Length: 2\r\nTransfer-Encoding: chunked\r\n\r\n'}, 1),
            ('compressed response', valid, {'response': b'HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\nContent-Length: 2\r\n\r\nOK'}, 1),
            ('unknown encoding redaction', valid, {'response': b'HTTP/1.1 200 OK\r\nContent-Encoding: DIAG_SECRET\r\n\r\n'}, 1),
            ('long header', valid, {'response': b'HTTP/1.1 200 OK\r\nX-Secret: DIAG_SECRET' + b'x' * 2000 + b'\r\n\r\n'}, 1),
            ('header budget', valid, {'response': b'HTTP/1.1 200 OK\r\n' + (b'X: ' + b'x' * 895 + b'\r\n') * 5 + b'\r\n'}, 1),
            ('trailer framing', valid, {'response': b'HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n2\r\nOK\r\n0\r\nContent-Length: 2\r\n\r\n'}, 1),
            ('body deadline', valid, {'response': b'HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK', 'slow': True, 'timeout': 200}, 1),
        ]
        diagnostics = {
            'timeout cleanup then fresh retry': ['RETRY_AFTER_TIMEOUT code=-35203 remaining=0', 'probe_code=0'],
            'body overflow': ['probe_code=-35202', 'http_status=200', 'error=body_limit', 'declared=262145', 'received=0'],
            'PSP observed chunk': ['error=none', 'received=129891', 'chunks=1'],
            'maximum length body': ['error=none', 'received=262144', 'declared=262144'],
            'maximum chunk body': ['error=none', 'received=262144', 'chunks=1'],
            'chunk overflow': ['error=body_limit', 'last_chunk=262145', 'received=0'],
            'aggregate chunk overflow': ['error=body_limit', 'last_chunk=20', 'received=262134'],
            'truncated body': ['probe_code=-35207', 'http_status=200', 'error=truncated', 'declared=10', 'received=2'],
            'unannounced close': ['http_status=200', 'received=2'],
            'ambiguous framing': ['probe_code=-35205', 'error=framing_conflict', 'http_status=200'],
            'compressed response': ['probe_code=-35206', 'error=content_encoding', 'encoding=gzip'],
            'unknown encoding redaction': ['error=content_encoding', 'encoding=other'],
            'long header': ['error=line_limit', 'line_bytes=1023'],
            'header budget': ['error=header_limit', 'headers=4097'],
            'trailer framing': ['error=trailer_framing', 'received=2'],
            'extensions and trailers': ['error=none', 'phase=done', 'chunked=1', 'chunks=1'],
        }
        for label, pair, options, expected in cases:
            process, state, elapsed = run(binary, pair, ca_file, **options)
            assert process.returncode == expected, (label, process.stdout, process.stderr)
            if options.get('retry'):
                assert state.get('first_closed') and state['request_received'] and elapsed < 4, (label, state, elapsed)
            if label in ('wrong hostname', 'expired', 'untrusted', 'clock unavailable', 'entropy unavailable'):
                assert not state['request_received'], label
            if 'deadline' in label:
                assert elapsed < 2, (label, elapsed)
            if expected == 0:
                expected_bytes = {'PSP observed chunk': 129891, 'maximum length body': 262144,
                                  'maximum chunk body': 262144}.get(label, 2)
                assert f'bytes={expected_bytes} ' in process.stdout, process.stdout
            for field in diagnostics.get(label, []):
                assert field in process.stdout, (label, field, process.stdout)
            if label == 'unannounced close':
                # Depending on TCP FIN/RST, the BIO reports EOF or a native
                # receive error. Neither is an authenticated complete body.
                assert any(value in process.stdout for value in ('error=unauthenticated_close', 'error=transport_error'))
            assert 'DIAG_SECRET' not in process.stdout + process.stderr, (label, 'server content leaked')
            print('PASS:', label, process.stdout.strip())
    print('Compact HTTPS: PASS; 64 KiB tracked allocator limit, zero remaining allocations')


if __name__ == '__main__':
    main()
