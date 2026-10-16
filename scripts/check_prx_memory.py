"""Validate final PRX PT_LOAD sizes against the loader's memory budget."""
import struct


def check_memory(data, limit):
    assert data[:6] == b'\x7fELF\x01\x01', 'expected ELF32 little endian PRX'
    table = struct.unpack_from('<I', data, 28)[0]
    width, count = struct.unpack_from('<HH', data, 42)
    assert width >= 32 and 0 < count <= 128, 'invalid program header count/size'
    assert table <= len(data) and count * width <= len(data) - table, 'truncated headers'
    total = 0
    ranges = []
    for i in range(count):
        kind, offset, address, _, filesz, memsz, _, _ = struct.unpack_from('<8I', data, table+i*width)
        if kind != 1:
            continue
        assert 0 < memsz and filesz <= memsz, 'invalid PT_LOAD size'
        assert offset <= len(data) and filesz <= len(data)-offset, 'truncated PT_LOAD'
        assert address + memsz <= 0x100000000, 'wrapping PT_LOAD'
        assert all(address >= end or address+memsz <= start for start, end in ranges), 'overlapping PT_LOAD'
        ranges.append((address, address+memsz))
        total += memsz
    assert ranges and total <= limit, f'PT_LOAD memory {total} exceeds loader limit {limit}'
    return total
