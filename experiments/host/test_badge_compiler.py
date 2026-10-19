"""Exercise the upstream PBAD writer against the online reader without network/Pillow.

Image download/decoding is stubbed; the existing extraction, header, records,
checksums and atomic file writer run unchanged.
"""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import types

root = Path(__file__).resolve().parents[2]
pil = types.ModuleType("PIL")
pil.Image = types.ModuleType("PIL.Image")
pil.ImageOps = types.ModuleType("PIL.ImageOps")
sys.modules["PIL"] = pil
spec = importlib.util.spec_from_file_location("upstream_badges", root / "tools/compile_badges.py")
module = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = module
spec.loader.exec_module(module)
module.download_badge = lambda *args: Path("synthetic-unused.png")
module.image_to_rgb565 = lambda *args: b"\x1f\0" * 1024
with tempfile.TemporaryDirectory(prefix="badge-compat-", dir=root / "build/host") as tmp:
    directory = Path(tmp)
    raw = directory / "raw.json"
    raw.write_text(json.dumps({"Sets": [{"Achievements": [
        {"ID": 1, "BadgeName": "00001"}, {"ID": 2, "BadgeName": "00002"}
    ]}]}))
    pack = directory / "UCUS-98662.pbad"
    module.build_pack(raw, "UCUS-98662", pack, directory, 1, False, False)
    subprocess.run([str(Path(sys.argv[1]).resolve()), str(pack)], check=True)
print("PASS upstream PBAD compiler -> online loader: existing extraction/writer/checksum; synthetic pixels, no network")
