"""Reproduce/check one public trust anchor from the unchanged pinned CA bundle."""
import base64
import hashlib
import json
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[1]
bundle = (root / 'vendor/ca/cacert.crt').read_bytes()
lock = json.loads((root / 'dependencies.lock.json').read_text())
assert hashlib.sha256(bundle).hexdigest() == lock['ca_bundle']['sha256'], 'CA bundle pin mismatch'
match = re.search(rb'GTS Root R4\r?\n=+\r?\n(-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----)', bundle, re.S)
assert match, 'GTS Root R4 missing'
der = base64.b64decode(re.sub(rb'-----[A-Z ]+-----|\s', b'', match[1]), validate=True)
assert hashlib.sha256(der).hexdigest() == '349dfa4058c5e263123b398ae795573c4e1313c83fe68f93556cd5e8031b3c7d'
header = '/* GTS Root R4, extracted from pinned vendor/ca/cacert.crt; MPL-2.0. */\n'
header += 'static const unsigned char ra_tls_root[] = {\n'
for offset in range(0, len(der), 16):
    header += '    ' + ','.join(f'0x{byte:02X}' for byte in der[offset:offset+16]) + ',\n'
header += '};\n'
outputs = {'gts-root-r4.der': der, 'gts_root.h': header.encode()}
for name, data in outputs.items():
    path = root / 'experiments/tls/compact' / name
    if '--write' in sys.argv:
        if not path.exists() or path.read_bytes() != data:
            path.write_bytes(data)
    else:
        assert path.read_bytes() == data, f'{name} differs; run with --write to regenerate'
print('PASS pinned CA extraction: GTS Root R4, DER 525 bytes, SHA256 verified')
