"""Decode bounded CODE windows from client-bootstrap.log; no network/account access.

Print only audit records and a small MIPS subset useful for caller/counter
inspection. Unknown instructions stay hexadecimal. This is not a decompiler
or proof that an instruction executes or belongs to a frame loop.
"""
import argparse
from pathlib import Path
import re


def instruction(pc, word):
    op, rs, rt, rd = word >> 26, (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31
    immediate = word & 65535
    signed = immediate if immediate < 32768 else immediate - 65536
    if word == 0:
        return 'nop'
    if op in (2, 3):
        return f'{"jal" if op == 3 else "j"} 0x{((pc + 4) & 0xF0000000) | ((word & 0x03FFFFFF) << 2):08X}'
    if op == 0 and word & 63 == 8:
        return f'jr r{rs}'
    if op == 0 and word & 63 == 9:
        return f'jalr r{rd},r{rs}'
    if op == 15:
        return f'lui r{rt},0x{immediate:04X}'
    if op in (8, 9):
        return f'{"addi" if op == 8 else "addiu"} r{rt},r{rs},{signed}'
    if op == 13:
        return f'ori r{rt},r{rs},0x{immediate:04X}'
    if op == 0 and word & 63 in (33, 37, 42):
        names = {33: 'addu', 37: 'or', 42: 'slt'}
        return f'{names[word & 63]} r{rd},r{rs},r{rt}'
    if op in (4, 5):
        return f'{"beq" if op == 4 else "bne"} r{rs},r{rt},0x{(pc + 4 + signed * 4) & 0xFFFFFFFF:08X}'
    names = {32: 'lb', 33: 'lh', 35: 'lw', 36: 'lbu', 37: 'lhu', 40: 'sb', 41: 'sh', 43: 'sw'}
    if op in names:
        return f'{names[op]} r{rt},{signed}(r{rs})'
    return f'.word 0x{word:08X}'


def report(path):
    if path.name.lower() != 'client-bootstrap.log' or path.stat().st_size > 131072:
        raise ValueError('expected bounded client-bootstrap.log')
    words = 0
    for line in path.read_text(encoding='ascii').splitlines():
        match = re.fullmatch(r'CODE ([0-9A-F]{8}) ([0-9A-F]{8})', line)
        if match:
            words += 1
            if words > 3072:
                raise ValueError('too many CODE records')
            pc, word = (int(value, 16) for value in match.groups())
            print(f'{pc:08X}: {word:08X}  {instruction(pc, word)}')
        elif line.startswith(('GAME_CODE_AUDIT ', 'GAME_IMPORT ', 'GAME_CALL ', 'GAME_BLOCK ', 'GAME_CANDIDATE_CODE ', 'GAME_CANDIDATE_CALL ', 'GAME_CALLBACK_REFS ', 'GAME_CALLBACK_PAIR ', 'GAME_CALLBACK_SLOT ', 'GAME_FLOW_', 'GAME_TABLE_', 'GAME_METHOD_', 'GAME_LIST_', 'GAME_PAUSE_')):
            print(line)
    print(f'Decoded {words} instruction records. Static candidates only; frame binding remains unproven.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    report(args.log)
