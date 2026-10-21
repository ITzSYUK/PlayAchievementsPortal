"""Check PSP syslib export retention after --gc-sections (ELF32 little endian)."""
from pathlib import Path
import struct
import sys
import re
from check_prx_memory import check_memory


def check(path, kernel):
    data = path.read_bytes()
    assert data[:6] == b'\x7fELF\x01\x01'
    table = struct.unpack_from('<I', data, 32)[0]
    width, count, names_id = struct.unpack_from('<HHH', data, 46)
    sections = [struct.unpack_from('<10I', data, table + i * width) for i in range(count)]
    names = sections[names_id]

    def name_at(offset):
        end = data.index(0, offset)
        return data[offset:end].decode()

    by_name = {name_at(names[4]+s[0]): s for s in sections}

    def offset(address, size=4):
        for s in sections:
            if s[1] != 8 and s[3] <= address and size <= s[5] - (address-s[3]):
                return s[4] + address-s[3]
        raise AssertionError(f'address outside file: {address:08x}')

    info = by_name['.rodata.sceModuleInfo'][4]
    attr, minor, major = struct.unpack_from('<HBB', data, info)
    assert (attr, minor, major) == (0x1000 if kernel else 0, 51, 0)
    expected = 'RAClientBootstrap' if kernel else 'RAClientUser'
    assert name_at(info+4) == expected
    first, last = struct.unpack_from('<II', data, info+36)
    assert last-first == 16, 'syslib export table missing or malformed'
    entry = offset(first, 16)
    length, variables, functions, pointers = struct.unpack_from('<BBHI', data, entry+8)
    assert (length, variables, functions) == (4, 1, 3)
    nid_table = offset(pointers, 32)
    nids = struct.unpack_from('<4I', data, nid_table)
    reboot_nid = 0xADF12745 if kernel else 0x2F064FA6
    assert set(nids) == {0xD632ACDB, 0xCEE8593C, reboot_nid, 0xF01D73A7}, nids
    targets = struct.unpack_from('<4I', data, nid_table+16)
    text = by_name['.text']
    for nid, target in zip(nids, targets):
        if nid != 0xF01D73A7:
            assert target % 4 == 0 and text[3] <= target < text[3]+text[5]
    if kernel:
        # Keep the 0.42 bootstrap dependency surface: exit diagnostics must
        # not introduce a hard kernel sceIoSync import before module_start.
        stubs = by_name['.lib.stub']
        cursor, end = stubs[4], stubs[4] + stubs[5]
        while cursor < end:
            lib, _, _, words, _, count, nids, _ = struct.unpack_from('<IHHBBHII', data, cursor)
            assert words >= 5 and cursor + words*4 <= end
            imported = struct.unpack_from(f'<{count}I', data, offset(nids, count*4))
            assert not (name_at(offset(lib)) == 'IoFileMgrForKernel' and 0xAB96437F in imported), 'new kernel sceIoSync dependency'
            cursor += words*4
    if not kernel:
        sym = by_name['.symtab']
        strings = sections[sym[6]]
        found = False
        for index in range(sym[5] // sym[9]):
            symbol = sym[4] + index*sym[9]
            key, address = struct.unpack_from('<II', data, symbol)
            if name_at(strings[4]+key) == 'sce_newlib_heap_kb_size':
                assert struct.unpack_from('<I', data, offset(address))[0] == 2048
                found = True
        assert found, 'heap pin missing'
    if not kernel:
        shared = Path(__file__).resolve().parents[1] / 'experiments/ingame/client_shared.h'
        limit = int(re.search(r'#define RA_CLIENT_MODULE_LIMIT (\d+)u', shared.read_text()).group(1))
        used = check_memory(path.with_suffix('.prx').read_bytes(), limit)
        print(f'PASS final PRX PT_LOAD: {used} bytes, loader limit={limit}, margin={limit-used}')
    print('PASS PSP metadata:', expected, '0.51, start/stop/reboot_phase/info exports, no kernel sceIoSync' if kernel else '0.51, start/stop/reboot_before/info exports, heap')


check(Path(sys.argv[1]), True)
check(Path(sys.argv[2]), False)
