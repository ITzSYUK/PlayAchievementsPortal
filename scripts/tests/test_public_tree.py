"""Protect both staged and unstaged files without rejecting ignored build output."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("public_tree", Path(__file__).parents[1] / "check_public_tree.py")
public_tree = importlib.util.module_from_spec(spec)
spec.loader.exec_module(public_tree)


class PublicTreeTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        subprocess.run(["git", "init", "-q", str(self.root)], check=True)
        (self.root / ".gitignore").write_text("build/\n.work/\n*.o\n", encoding="utf-8")
        (self.root / "source.c").write_text("int example;\n", encoding="utf-8")

    def tearDown(self):
        self.temporary.cleanup()

    def test_untracked_image_is_rejected(self):
        (self.root / "game.iso").write_bytes(b"synthetic")
        self.assertTrue(any("game.iso" in item for item in public_tree.scan(self.root)))

    def test_ignored_build_output_is_allowed(self):
        (self.root / "build").mkdir()
        (self.root / "build" / "game.iso").write_bytes(b"synthetic")
        (self.root / "source.o").write_bytes(b"synthetic")
        self.assertEqual(public_tree.scan(self.root), [])

    def test_force_tracked_build_output_is_rejected(self):
        (self.root / "source.o").write_bytes(b"synthetic")
        subprocess.run(["git", "-C", str(self.root), "add", "-f", "source.o"], check=True)
        self.assertTrue(any("source.o" in item for item in public_tree.scan(self.root)))


if __name__ == "__main__":
    unittest.main()
