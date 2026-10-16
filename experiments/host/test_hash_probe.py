"""Compare real rcheevos ISO hashing with streamed files and an independent MD5.

All fixtures are generated nonsense, never actual game or achievement data.
"""
from __future__ import annotations

import hashlib
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

SECTOR = 2048
LIMIT = 64 * 1024 * 1024


def record(name: bytes, sector: int, size: int, directory: bool = False) -> bytes:
    length = 33 + len(name) + (1 if len(name) % 2 == 0 else 0)
    data = bytearray(length)
    data[0] = length
    data[2:10] = struct.pack("<I", sector) + struct.pack(">I", sector)
    data[10:18] = struct.pack("<I", size) + struct.pack(">I", size)
    data[18:25] = bytes([126, 10, 13, 0, 0, 0, 0])
    data[25] = 2 if directory else 0
    data[28:32] = b"\x01\x00\x00\x01"
    data[32] = len(name)
    data[33:33 + len(name)] = name
    return bytes(data)


def create_iso(path: Path, sfo: bytes, eboot: bytes, include_eboot: bool = True) -> bytes:
    sfo_sector = 23
    eboot_sector = sfo_sector + (len(sfo) + SECTOR - 1) // SECTOR
    sectors = eboot_sector + (len(eboot) + SECTOR - 1) // SECTOR
    image = bytearray(sectors * SECTOR)
    pvd = bytearray(SECTOR)
    pvd[:7] = b"\x01CD001\x01"
    pvd[80:88] = struct.pack("<I", sectors) + struct.pack(">I", sectors)
    pvd[128:132] = b"\x00\x08\x08\x00"
    pvd[156:190] = record(b"\0", 20, SECTOR, True)
    image[16 * SECTOR:17 * SECTOR] = pvd
    image[17 * SECTOR:17 * SECTOR + 7] = b"\xffCD001\x01"
    directories = {
        20: [(b"\0", 20, SECTOR, True), (b"\1", 20, SECTOR, True),
             (b"PSP_GAME", 21, SECTOR, True)],
        21: [(b"\0", 21, SECTOR, True), (b"\1", 20, SECTOR, True),
             (b"SYSDIR", 22, SECTOR, True), (b"PARAM.SFO;1", sfo_sector, len(sfo), False)],
        22: [(b"\0", 22, SECTOR, True), (b"\1", 21, SECTOR, True)],
    }
    if include_eboot:
        directories[22].append((b"EBOOT.BIN;1", eboot_sector, len(eboot), False))
    for sector, entries in directories.items():
        data = b"".join(record(*entry) for entry in entries)
        image[sector * SECTOR:sector * SECTOR + len(data)] = data
    image[sfo_sector * SECTOR:sfo_sector * SECTOR + len(sfo)] = sfo
    image[eboot_sector * SECTOR:eboot_sector * SECTOR + len(eboot)] = eboot
    path.write_bytes(image)
    return bytes(image)


def run(exe: Path, *args: str | Path, success: bool = True) -> str:
    result = subprocess.run([str(exe), *(str(arg) for arg in args)],
                            capture_output=True, text=True, timeout=30)
    if success:
        assert result.returncode == 0, result.stderr
        return result.stdout.strip()
    assert result.returncode != 0, "Invalid input was accepted"
    assert not result.stdout.strip(), "Failure must not emit a usable hash"
    return result.stderr


def main() -> None:
    exe = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="hash-probe-", dir=exe.parent) as temporary:
        root = Path(temporary)
        sfo_file, eboot_file = root / "PARAM.SFO", root / "EBOOT.BIN"
        iso = root / "fixture.iso"
        # Non-aligned payloads spanning several sectors catch ordering and padding errors.
        sfo = b"synthetic-sfo\0" + bytes(range(256)) * 13
        eboot = b"~PSP" + bytes(reversed(range(256))) * 31 + b"tail"
        sfo_file.write_bytes(sfo)
        eboot_file.write_bytes(eboot)
        image = create_iso(iso, sfo, eboot)
        expected = hashlib.md5(sfo + eboot).hexdigest()
        assert run(exe, "--iso", iso) == expected
        assert run(exe, "--files", sfo_file, eboot_file) == expected
        assert run(exe, "--files", eboot_file, sfo_file) != expected
        eboot_file.write_bytes(eboot + b"changed")
        assert run(exe, "--files", sfo_file, eboot_file) != expected
        run(exe, "--files", sfo_file, root / "missing.bin", success=False)
        run(exe, "--iso", root / "missing.iso", success=False)
        run(exe, "--iso", root / "unsupported.pbp", success=False)
        iso.write_bytes(image[:len(image) - SECTOR])
        run(exe, "--iso", iso, success=False)
        create_iso(iso, sfo, eboot, include_eboot=False)
        run(exe, "--iso", iso, success=False)
        iso.write_bytes(b"not an ISO")
        run(exe, "--iso", iso, success=False)
        # Sparse synthetic executable tests the upstream per-file 64 MiB cap.
        with eboot_file.open("wb") as file:
            file.seek(LIMIT)
            file.write(b"must-not-be-hashed")
        capped = hashlib.md5(sfo)
        for _ in range(64):
            capped.update(bytes(1024 * 1024))
        assert run(exe, "--files", sfo_file, eboot_file) == capped.hexdigest()
    print("PASS PSP hash: official ISO = streamed files = independent MD5; invalid inputs and 64 MiB cap")


if __name__ == "__main__":
    main()
