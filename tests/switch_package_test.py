"""Check version derivation and the independent check of the built NACP."""
import io
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tarfile
import tempfile
import unittest
import zipfile

REPOSITORY = Path(__file__).resolve().parents[1]


class SwitchPackageTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        for name in ("scripts/package-switch.py", "cmake/version.hpp.in", "packaging/switch/README.md",
                     "packaging/switch/rebuild.sh", "LICENSE", "THIRD_PARTY_NOTICES.md", "CMakeLists.txt"):
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(REPOSITORY / name, target)
        # Deliberately use another version: every derived output must follow it.
        (self.root / "VERSION").write_text("2.3.4\n7\n", encoding="ascii")
        font = self.root / "assets/fonts/RobotoMono-Bold.ttf"
        font.parent.mkdir(parents=True)
        font.write_bytes(b"fixture font")
        self.build = self.root / "build/switch-alpha"
        (self.build / "third-party").mkdir(parents=True)
        (self.build / "third-party/package-versions.txt").write_text("fixture packages\n")
        elf = bytearray(64)
        elf[:6] = b"\x7fELF\x02\x01"
        struct.pack_into("<H", elf, 18, 183)
        (self.build / "coverplayer.elf").write_bytes(elf)
        self.write_nro("2.3.4-sw-a7")

    def write_nro(self, version):
        header = bytearray(32)
        header[16:20] = b"NRO0"
        struct.pack_into("<I", header, 24, len(header))
        nacp = bytearray(0x4000)
        nacp[:11] = b"CoverPlayer"
        encoded = version.encode("ascii")
        nacp[0x3060:0x3060 + len(encoded)] = encoded
        icon, font = b"\xff\xd8", b"fixture font"
        assets = b"ASET" + bytes(4) + b"".join(struct.pack("<QQ", offset, size) for offset, size in (
            (56, len(icon)), (56 + len(icon), len(nacp)), (56 + len(icon) + len(nacp), len(font))))
        (self.build / "CoverPlayer.nro").write_bytes(header + assets + icon + nacp + font)

    def package(self):
        return subprocess.run([sys.executable, str(self.root / "scripts/package-switch.py"),
                               "--docker-image", "fixture-image"], capture_output=True, text=True)

    def test_metadata_drives_outputs_and_source_archive(self):
        result = self.package()
        self.assertEqual(result.returncode, 0, result.stderr)
        release = self.root / "build/release"
        name = "CoverPlayer-Switch-2.3.4-alpha7"
        self.assertTrue((release / f"{name}.nro").is_file())
        self.assertTrue((release / f"{name}.zip.sha256").is_file())
        with zipfile.ZipFile(release / f"{name}.zip") as archive:
            self.assertIn(b"CoverPlayer 2.3.4 Switch alpha 7", archive.read("BUILD.txt"))
            with tarfile.open(fileobj=io.BytesIO(archive.read("relink/coverplayer-source.tar.gz"))) as source:
                self.assertIn("VERSION", source.getnames())
                self.assertIn("cmake/version.hpp.in", source.getnames())

    def test_stale_built_nacp_is_rejected(self):
        self.write_nro("2.3.4-sw-a6")
        result = self.package()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Built NACP version differs from VERSION", result.stderr)
        self.assertEqual(list((self.root / "build/release").iterdir()), [])


if __name__ == "__main__":
    unittest.main()
