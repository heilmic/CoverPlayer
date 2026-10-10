"""Add the optional category to a verified regular Knulli archive."""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import subprocess
import sys
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo

def main() -> None:
    repo = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", type=Path, default=repo / "build/release/CoverPlayer-Knulli.zip")
    parser.add_argument("--output", type=Path, default=repo / "build/release/CoverPlayer-Knulli-Category.zip")
    args = parser.parse_args()
    if args.base.resolve() == args.output.resolve():
        parser.error("base and output must differ")
    verifier = repo / "scripts/verify-package.py"
    subprocess.run([sys.executable, str(verifier), str(args.base)], check=True)
    with ZipFile(args.base) as base:
        files = {name: base.read(name) for name in base.namelist()}
    if "roms/ports/CoverPlayer/bin/coverplayer" not in files:
        parser.error("base must be the regular Knulli package")
    overlay = repo / "packaging/knulli-category"
    for path in sorted(overlay.rglob("*")):
        if path.is_file():
            name = path.relative_to(overlay).as_posix()
            if name in files:
                parser.error(f"overlay would replace base file: {name}")
            data = path.read_bytes()
            files[name] = data.replace(b"\r\n", b"\n") if path.suffix == ".sh" else data
    guide = (repo / "docs/knulli-category.md").read_text(encoding="utf-8")
    guide = guide.replace("screenshots/knulli-category.png", "CoverPlayer-Category.png")
    guide = guide.replace("screenshots/knulli-category-icon.png", "CoverPlayer-Category-Icon.png")
    files["CoverPlayer-Category-README.md"] = guide.encode("utf-8")
    for source, destination in (
        ("knulli-category.png", "CoverPlayer-Category.png"),
        ("knulli-category-icon.png", "CoverPlayer-Category-Icon.png"),
    ):
        files[destination] = (repo / "docs/screenshots" / source).read_bytes()
    files.pop("MANIFEST.sha256")
    files["MANIFEST.sha256"] = "".join(
        f"{hashlib.sha256(data).hexdigest()}  {name}\n"
        for name, data in sorted(files.items())
    ).encode("utf-8")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with ZipFile(args.output, "w", compression=ZIP_DEFLATED, compresslevel=9) as archive:
        for name, data in sorted(files.items()):
            info = ZipInfo(name)
            info.create_system = 3
            executable = name.endswith(".sh") or "/bin/coverplayer" in f"/{name}"
            info.external_attr = (0o755 if executable else 0o644) << 16
            archive.writestr(info, data, compress_type=ZIP_DEFLATED, compresslevel=9)
    subprocess.run([sys.executable, str(verifier), str(args.output)], check=True)
    print(f"Category package: {args.output}")


if __name__ == "__main__":
    main()
