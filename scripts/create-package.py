from __future__ import annotations

import argparse
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo


def is_executable(relative_path: str) -> bool:
    return relative_path.endswith(".sh") or "/bin/coverplayer" in f"/{relative_path}"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    source = args.source.resolve()
    files = sorted(path for path in source.rglob("*") if path.is_file())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with ZipFile(args.output, "w", compression=ZIP_DEFLATED, compresslevel=9) as archive:
        for path in files:
            relative = path.relative_to(source).as_posix()
            info = ZipInfo(relative)
            info.create_system = 3
            info.external_attr = ((0o755 if is_executable(relative) else 0o644) & 0xFFFF) << 16
            info.compress_type = ZIP_DEFLATED
            archive.writestr(info, path.read_bytes(), compress_type=ZIP_DEFLATED, compresslevel=9)


if __name__ == "__main__":
    main()
