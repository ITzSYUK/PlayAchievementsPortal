"""Read-only ISO9660 diagnostics; emit hashes/metadata, never extract game data."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

SECTOR = 2048
LIMIT = 64 * 1024 * 1024


def inspect(path):
    with path.open('rb') as image:
        total = path.stat().st_size

        def read_at(offset, size):
            if offset < 0 or size < 0 or offset + size > total:
                raise ValueError('ISO extent outside image')
            image.seek(offset)
            data = image.read(size)
            if len(data) != size:
                raise ValueError('short ISO read')
            return data

        def extent(record):
            if len(record) < 34 or record[0] != len(record) or record[25] & 0x80:
                raise ValueError('invalid or multi-extent ISO record')
            if struct.unpack_from('<I', record, 2)[0] != struct.unpack_from('>I', record, 6)[0]:
                raise ValueError('inconsistent extent')
            if struct.unpack_from('<I', record, 10)[0] != struct.unpack_from('>I', record, 14)[0]:
                raise ValueError('inconsistent size')
            return struct.unpack_from('<I', record, 2)[0], struct.unpack_from('<I', record, 10)[0]

        pvd = read_at(16 * SECTOR, SECTOR)
        if pvd[:7] != b'\x01CD001\x01' or pvd[128:132] != b'\x00\x08\x08\x00':
            raise ValueError('only standard uncompressed ISO9660/2048 supported')
        root = extent(pvd[156:156 + pvd[156]])

        def find(name):
            current = root
            for part in name.split('/'):
                sector, length = current
                if length > 4 * 1024 * 1024:
                    raise ValueError('directory too large')
                directory = read_at(sector * SECTOR, length)
                position = 0
                found = None
                while position < len(directory):
                    size = directory[position]
                    if not size:
                        position = (position // SECTOR + 1) * SECTOR
                        continue
                    record = directory[position:position + size]
                    if len(record) < 34 or 33 + record[32] > size:
                        raise ValueError('bad directory record')
                    identifier = record[33:33 + record[32]].split(b';')[0]
                    if identifier == part.encode('ascii'):
                        found = extent(record)
                        break
                    position += size
                if found is None:
                    raise ValueError('file missing: ' + name)
                current = found
            return current

        combined = hashlib.md5()
        result = {'iso_size': total, 'files': {}}
        for name in ('PSP_GAME/PARAM.SFO', 'PSP_GAME/SYSDIR/EBOOT.BIN'):
            sector, size = find(name)
            file_hash = hashlib.md5()
            remaining = min(size, LIMIT)
            cursor = sector * SECTOR
            prefix = read_at(cursor, min(size, 64))
            while remaining:
                amount = min(65536, remaining)
                data = read_at(cursor, amount)
                combined.update(data)
                file_hash.update(data)
                cursor += amount
                remaining -= amount
            result['files'][name] = {'size': size, 'hashed_size': min(size, LIMIT),
                                   'md5': file_hash.hexdigest(), 'magic': prefix[:4].hex()}
            if name.endswith('EBOOT.BIN') and prefix[:4] == b'~PSP':
                result['module_name'] = prefix[10:38].split(b'\0')[0].decode('ascii')
        sector, size = find('UMD_DATA.BIN')
        result['umd_id'] = read_at(sector * SECTOR, min(size, 10)).decode('ascii')
        result['streamed_ra_hash'] = combined.hexdigest()
        return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('iso', type=Path)
    args = parser.parse_args()
    print(json.dumps(inspect(args.iso), indent=2))
