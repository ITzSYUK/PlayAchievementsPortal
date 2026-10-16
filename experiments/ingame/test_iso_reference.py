"""Read-only reference parser checks using generated ISO data, not game files."""
import hashlib
from pathlib import Path
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'scripts'))
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'host'))
from inspect_psp_iso import inspect
from test_hash_probe import create_iso, record, SECTOR

with tempfile.TemporaryDirectory() as temporary:
    path = Path(temporary) / 'test.iso'
    sfo, exe = b'test-sfo', b'~PSP' + b'\0' * 6 + b'TestModule\0' + b'x' * 3000
    data = bytearray(create_iso(path, sfo, exe))
    # Append UMD metadata at the end, plus a matching root directory entry.
    sector = len(data) // SECTOR
    umd = b'UCUS-98662'
    entry = record(b'UMD_DATA.BIN;1', sector, len(umd))
    root = data[20 * SECTOR:21 * SECTOR]
    offset = 0
    while root[offset]: offset += root[offset]
    data[20 * SECTOR + offset:20 * SECTOR + offset + len(entry)] = entry
    data.extend(umd + b'\0' * (SECTOR - len(umd)))
    path.write_bytes(data)
    before = path.read_bytes()
    result = inspect(path)
    assert result['streamed_ra_hash'] == hashlib.md5(sfo + exe).hexdigest()
    assert result['files']['PSP_GAME/PARAM.SFO']['md5'] == hashlib.md5(sfo).hexdigest()
    assert result['module_name'] == 'TestModule' and result['umd_id'] == 'UCUS-98662'
    assert path.read_bytes() == before
    path.write_bytes(data[:17 * SECTOR])
    try:
        inspect(path)
        raise AssertionError('truncated image accepted')
    except ValueError:
        pass
print('PASS: reference hashes/metadata from synthetic ISO; no image modification; short read rejected')
