"""Create and verify the self-contained Switch alpha installation ZIP."""
import argparse
import hashlib
import io
from pathlib import Path
import struct
import tarfile
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument("--docker-image", required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
build = root / "build/switch-alpha"
release = root / "build/release"
release.mkdir(parents=True, exist_ok=True)
nro = (build / "CoverPlayer.nro").read_bytes()
assert nro[16:20] == b"NRO0", "Invalid NRO magic"
nro_size = struct.unpack_from("<I", nro, 24)[0]
assert nro[nro_size:nro_size + 4] == b"ASET", "Missing hbmenu asset section"
sections = [struct.unpack_from("<QQ", nro, nro_size + 8 + 16*i) for i in range(3)]
for offset, size in sections:
    assert offset >= 56 and size > 0 and nro_size + offset + size <= len(nro)
icon_offset, icon_size = sections[0]
assert nro[nro_size + icon_offset:nro_size + icon_offset + 2] == b"\xff\xd8"
nacp_offset, nacp_size = sections[1]
nacp = nro[nro_size + nacp_offset:nro_size + nacp_offset + nacp_size]
assert nacp_size == 0x4000
assert nacp[:512].split(b"\0", 1)[0] == b"CoverPlayer"
assert nacp[0x3060:0x3070].split(b"\0", 1)[0] == b"1.0.0-sw-a5"
font = (root / "assets/fonts/RobotoMono-Bold.ttf").read_bytes()
romfs_offset, romfs_size = sections[2]
assert font in nro[nro_size + romfs_offset:nro_size + romfs_offset + romfs_size]
elf = (build / "coverplayer.elf").read_bytes()
assert elf[:6] == b"\x7fELF\x02\x01"
assert struct.unpack_from("<H", elf, 18)[0] == 183, "ELF is not AArch64"

files = {
    "switch/CoverPlayer/CoverPlayer.nro": nro,
    "README.md": (root / "packaging/switch/README.md").read_bytes(),
    "LICENSE": (root / "LICENSE").read_bytes(),
    "THIRD_PARTY_NOTICES.md": (root / "THIRD_PARTY_NOTICES.md").read_bytes(),
    "licenses/RobotoMono-Apache-2.0.txt": (root / "LICENSE").read_bytes(),
    "licenses/package-versions.txt": (build / "third-party/package-versions.txt").read_bytes(),
    "relink/rebuild.sh": (root / "packaging/switch/rebuild.sh").read_bytes(),
}
for dirname in ("licenses", "relink"):
    for path in sorted((build / "third-party" / dirname).rglob("*")):
        if path.is_file():
            if path.suffix == ".tmp":
                continue
            assert path.stat().st_size > 0, f"Empty third-party file: {path}"
            files[f"{dirname}/{path.relative_to(build / 'third-party' / dirname).as_posix()}"] = path.read_bytes()

# Include the actual working source, including the new Switch files. Nothing
# depends on an unpushed Git revision or a mutable remote source checkout.
source_paths = [root / name for name in ("CMakeLists.txt", "LICENSE", "THIRD_PARTY_NOTICES.md")]
for dirname in ("include", "src", "assets/fonts", "packaging/switch"):
    source_paths.extend(p for p in (root / dirname).rglob("*") if p.is_file())
source_buffer = io.BytesIO()
with tarfile.open(fileobj=source_buffer, mode="w:gz") as archive:
    for path in sorted(source_paths):
        archive.add(path, arcname=path.relative_to(root).as_posix())
files["relink/coverplayer-source.tar.gz"] = source_buffer.getvalue()
files["BUILD.txt"] = (f"CoverPlayer Switch alpha 5\nDocker image: {args.docker_image}\n"
                      "Hardware: user confirmed collection creation and MP3 playback in alpha 2 on Switch; Lite test pending.\n").encode()
files["SHA256SUMS.txt"] = "".join(
    f"{hashlib.sha256(data).hexdigest()}  {name}\n" for name, data in sorted(files.items())
).encode()
target = release / "CoverPlayer-Switch-1.0.0-alpha5.zip"
with zipfile.ZipFile(target, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
    for name, data in sorted(files.items()):
        archive.writestr(name, data)
    archive.writestr("media/", b"")
with zipfile.ZipFile(target) as archive:
    assert archive.testzip() is None
    for name, data in files.items():
        assert archive.read(name) == data
    assert all(not name.startswith("/") and ".." not in Path(name).parts for name in archive.namelist())
(release / "CoverPlayer-Switch-1.0.0-alpha5.nro").write_bytes(nro)
(release / "CoverPlayer-Switch-1.0.0-alpha5.zip.sha256").write_text(
    f"{hashlib.sha256(target.read_bytes()).hexdigest()}  {target.name}\n", encoding="ascii")
print(f"Verified Switch NRO, embedded assets and {len(files)} package files: {target}")
