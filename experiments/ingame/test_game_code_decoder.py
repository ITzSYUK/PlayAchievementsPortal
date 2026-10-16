from pathlib import Path
import importlib.util
import contextlib
import io
import tempfile

path = Path(__file__).resolve().parents[2] / 'scripts/analyze_game_code.py'
spec = importlib.util.spec_from_file_location('decoder', path)
decoder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(decoder)
assert decoder.instruction(0x0FFFFFFC, 0x0C000001) == 'jal 0x10000004'
assert decoder.instruction(0x08804000, 0x27BDFFF0) == 'addiu r29,r29,-16'
assert decoder.instruction(0x08804000, 0x8C82FFFC) == 'lw r2,-4(r4)'
assert decoder.instruction(0x08804000, 0xAC820004) == 'sw r2,4(r4)'
assert decoder.instruction(0x08804000, 0x1440FFFE) == 'bne r2,r0,0x08803FFC'
assert decoder.instruction(0x08804000, 0x03E00008) == 'jr r31'
assert decoder.instruction(0x08804000, 0) == 'nop'
assert decoder.instruction(0x08804000, 0x34854180) == 'ori r5,r4,0x4180'
assert decoder.instruction(0x08804000, 0x0085182A) == 'slt r3,r4,r5'
with tempfile.TemporaryDirectory(prefix='code-audit-', dir=path.parents[1] / 'build') as temporary:
    log = Path(temporary) / 'client-bootstrap.log'
    log.write_text('ignored-synthetic-content\nGAME_CODE_AUDIT result=0\nGAME_TABLE_FIELD name=input_descriptor index=5 kind=code address=0x08842D90\nGAME_PAUSE_EDGE pc=0x08844444 target=0x08845000 lexical=1\nCODE 08804000 27BDFFF0\n', encoding='ascii')
    output = io.StringIO()
    with contextlib.redirect_stdout(output):
        decoder.report(log)
    assert 'ignored-synthetic-content' not in output.getvalue()
    assert 'addiu r29,r29,-16' in output.getvalue()
    assert 'GAME_TABLE_FIELD name=input_descriptor' in output.getvalue()
    assert 'GAME_PAUSE_EDGE pc=0x08844444' in output.getvalue()
    log.write_text('GAME_CALLBACK_PAIR target=0x088428B0 lexical=1\n' +
                   'CODE 08804000 00000000\n' * 3072, encoding='ascii')
    with contextlib.redirect_stdout(io.StringIO()):
        decoder.report(log)
    log.write_text(log.read_text() + 'CODE 08804000 00000000\n', encoding='ascii')
    try:
        with contextlib.redirect_stdout(io.StringIO()):
            decoder.report(log)
        raise AssertionError('too many instructions accepted')
    except ValueError:
        pass
    try:
        decoder.report(Path(temporary) / 'auth.txt')
        raise AssertionError('unexpected file accepted')
    except ValueError:
        pass
    log.write_text('x' * 131073, encoding='ascii')
    try:
        decoder.report(log)
        raise AssertionError('oversize file accepted')
    except ValueError:
        pass
print('PASS bounded code decoder: jump regions, branch/delay PC, signed arithmetic and load/store offsets')
