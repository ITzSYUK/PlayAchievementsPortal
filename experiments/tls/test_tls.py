"""Loopback TLS tests; ephemeral credentials stay in a deleted temp directory.
Uses the already installed cryptography package and Python's OpenSSL server.
No RetroAchievements account or external connection is involved.
"""
from datetime import datetime, timedelta, timezone
from pathlib import Path
import os
import socket
import ssl
import subprocess
import sys
import tempfile
import threading
import time

from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, rsa
from cryptography.x509.oid import NameOID


def certificate(key, name, issuer, issuer_key, expired=False, ca=False):
    now = datetime.now(timezone.utc)
    subject = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, name)])
    builder = (
        x509.CertificateBuilder().subject_name(subject).issuer_name(issuer or subject)
        .public_key(key.public_key()).serial_number(x509.random_serial_number())
        .not_valid_before(now - timedelta(days=3))
        .not_valid_after(now - timedelta(days=1) if expired else now + timedelta(days=3))
        .add_extension(x509.BasicConstraints(ca=ca, path_length=None), critical=True)
    )
    if not ca:
        builder = builder.add_extension(x509.SubjectAlternativeName([x509.DNSName(name)]), critical=False)
    return builder.sign(issuer_key or key, hashes.SHA256())


def write_pair(root, label, key, cert):
    key_path, cert_path = root / (label + '.key'), root / (label + '.crt')
    key_path.write_bytes(key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8,
                                         serialization.NoEncryption()))
    cert_path.write_bytes(cert.public_bytes(serialization.Encoding.PEM))
    return key_path, cert_path


def run(binary, pair, ca_file, *, hostname='localhost', response=None, silent=False,
        entropy_failure=False, timeout=3000, slow=False, utc_epoch=None, clean_close=False):
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.maximum_version = ssl.TLSVersion.TLSv1_2
    context.load_cert_chain(str(pair[1]), str(pair[0]))
    server = socket.socket()
    server.bind(('127.0.0.1', 0))
    server.listen(1)
    server.settimeout(5)
    port = server.getsockname()[1]
    state = {'request_received': False, 'errors': []}

    def serve():
        try:
            raw, _ = server.accept()
            raw.settimeout(4)
            if silent:
                with raw:
                    time.sleep(timeout / 1000 + .2)
                return
            with context.wrap_socket(raw, server_side=True) as connection:
                request = connection.recv(4096)
                state['request_received'] = bool(request)
                # The client writes headers and POST separately. Drain the
                # complete request so closing the test socket cannot reset a
                # large response because unread POST bytes remain queued.
                while request and b'\r\n\r\n' not in request:
                    part = connection.recv(4096)
                    if not part:
                        break
                    request += part
                    if len(request) > 16384:
                        raise ValueError('test request headers too large')
                headers, separator, body = request.partition(b'\r\n\r\n')
                length = 0
                for line in headers.split(b'\r\n')[1:]:
                    name, colon, value = line.partition(b':')
                    if colon and name.lower() == b'content-length':
                        length = int(value.strip())
                if not 0 <= length <= 8192:
                    raise ValueError('test request body too large')
                while separator and len(body) < length:
                    part = connection.recv(length - len(body))
                    if not part:
                        break
                    body += part
                if response is None:
                    connection.sendall(b'HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nOK')
                elif slow:
                    for value in response:
                        connection.sendall(bytes([value]))
                        time.sleep(.05)
                else:
                    connection.sendall(response)
                if clean_close:
                    connection.unwrap().close()
        except (ssl.SSLError, ConnectionError, TimeoutError, BrokenPipeError):
            pass  # The client is expected to abort several test connections.
        except Exception as exc:
            state['errors'].append(str(exc))
        finally:
            server.close()

    thread = threading.Thread(target=serve)
    thread.start()
    environment = dict(os.environ)
    if entropy_failure:
        environment['RA_TEST_ENTROPY_FAILURE'] = '1'
    if utc_epoch is not None:
        environment['RA_TEST_UTC_EPOCH'] = str(utc_epoch)
    before = time.monotonic()
    process = subprocess.run([str(binary), '127.0.0.1', str(port), hostname, str(ca_file), str(timeout)],
                             text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                             env=environment, timeout=10)
    elapsed = time.monotonic() - before
    thread.join(6)
    assert not thread.is_alive(), 'server thread leaked'
    assert not state['errors'], state['errors']
    assert 'remaining=0' in process.stdout, process.stdout + process.stderr
    assert 'AddressSanitizer' not in process.stderr and 'runtime error:' not in process.stderr, process.stderr
    return process, state, elapsed


def main():
    binary = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='ra-tls-') as temporary:
        root = Path(temporary)
        ca_key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
        ca = certificate(ca_key, 'RA test CA', None, None, ca=True)
        ca_path = write_pair(root, 'ca', ca_key, ca)[1]
        rsa_key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
        valid = write_pair(root, 'valid', rsa_key, certificate(rsa_key, 'localhost', ca.subject, ca_key))
        expired = write_pair(root, 'expired', rsa_key, certificate(rsa_key, 'localhost', ca.subject, ca_key, expired=True))
        unknown = write_pair(root, 'unknown', rsa_key, certificate(rsa_key, 'localhost', None, None))
        ec_key = ec.generate_private_key(ec.SECP256R1())
        ec_valid = write_pair(root, 'ecdsa', ec_key, certificate(ec_key, 'localhost', ca.subject, ca_key))
        cases = [
            ('valid RSA', valid, {}, 0, None),
            ('valid ECDSA', ec_valid, {}, 0, None),
            ('hostname mismatch', valid, {'hostname': 'wrong.local'}, 1, 'flags=0x00000004'),
            ('expired', expired, {}, 1, 'flags=0x00000001'),
            ('untrusted/self-signed', unknown, {}, 1, 'flags=0x00000008'),
            ('PSP 0.1 clock regression (1970)', valid, {'utc_epoch': 46616}, 1, 'flags=0x00000200'),
            ('UTC clock unavailable', valid, {'utc_epoch': -1}, 1, 'code=-31005'),
            ('entropy unavailable', valid, {'entropy_failure': True}, 1, 'strong entropy / CTR_DRBG seed'),
            ('handshake timeout', valid, {'silent': True, 'timeout': 200}, 1, 'code=-31001'),
            ('HTTP header overflow', valid, {'response': b'HTTP/1.1 200 OK\r\nX: ' + b'a' * 9000}, 1, 'code=-31002'),
            ('truncated HTTP headers', valid, {'response': b'HTTP/1.1 200 OK\r\nX: a'}, 1, 'HTTP headers'),
            ('invalid HTTP status', valid, {'response': b'HTTP/1.1 2x0 OK\r\n\r\n'}, 1, 'code=-31003'),
            ('HTTP header total deadline', valid, {'response': b'HTTP/1.1 200 OK\r\n\r\n', 'slow': True, 'timeout': 200}, 1, 'code=-31001'),
        ]
        for name, pair, options, expected_rc, marker in cases:
            process, state, elapsed = run(binary, pair, ca_path, **options)
            assert process.returncode == expected_rc, (name, process.stdout, process.stderr)
            if marker:
                assert marker in process.stdout, (name, process.stdout)
            if name in ('hostname mismatch', 'expired', 'untrusted/self-signed', 'entropy unavailable',
                        'PSP 0.1 clock regression (1970)', 'UTC clock unavailable'):
                assert not state['request_received'], 'HTTP sent before peer verification: ' + name
            if 'timeout' in name or 'deadline' in name:
                assert elapsed < 2, (name, elapsed)
            print('PASS:', name)
        malformed = root / 'malformed.crt'
        malformed.write_text('not a certificate')
        oversized = root / 'oversized.crt'
        oversized.write_bytes(b'a' * (256 * 1024 + 1))
        for label, path in [('malformed CA', malformed), ('oversized CA', oversized)]:
            process, state, _ = run(binary, valid, path)
            assert process.returncode == 1 and not state['request_received'], process.stdout
            print('PASS:', label)
        # The real trust bundle must parse completely, but cannot trust our CA.
        public_ca = Path(__file__).resolve().parents[2] / 'vendor/ca/cacert.crt'
        process, state, _ = run(binary, valid, public_ca)
        assert process.returncode == 1 and not state['request_received'], process.stdout
        assert 'tls_stage=TLS handshake' in process.stdout and 'flags=0x00000008' in process.stdout, process.stdout
        print('PASS: complete pinned CA bundle parses, rejects private test CA')
    print('TLS verification, deadlines, bounds and cleanup: PASS')


if __name__ == '__main__':
    main()
