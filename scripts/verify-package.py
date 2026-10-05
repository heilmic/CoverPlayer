from __future__ import annotations

import argparse
import hashlib
from pathlib import PurePosixPath
from zipfile import ZipFile


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("archive")
    args = parser.parse_args()

    with ZipFile(args.archive) as archive:
        members = archive.namelist()
        names = set(members)
        if len(names) != len(members):
            raise SystemExit("package contains duplicate member names")
        for name in names:
            path = PurePosixPath(name)
            if path.is_absolute() or ".." in path.parts or "\\" in name:
                raise SystemExit(f"unsafe package member: {name}")
        manifests = sorted(name for name in names if name.endswith("MANIFEST.sha256"))
        if "MANIFEST.sha256" not in manifests:
            raise SystemExit("package contains no root MANIFEST.sha256")

        for name in names:
            if name.endswith(".sh") or "/bin/coverplayer" in f"/{name}":
                mode = archive.getinfo(name).external_attr >> 16
                if mode & 0o111 == 0:
                    raise SystemExit(f"not executable: {name}")
            if name.endswith(".sh") and b"\r\n" in archive.read(name):
                raise SystemExit(f"CRLF launcher: {name}")

        for manifest_name in manifests:
            root = PurePosixPath(manifest_name).parent
            covered = set()
            for line in archive.read(manifest_name).decode("utf-8").splitlines():
                expected, relative = line.split("  ", 1)
                relative_path = PurePosixPath(relative)
                if relative_path.is_absolute() or ".." in relative_path.parts or "\\" in relative:
                    raise SystemExit(f"unsafe manifest member: {relative}")
                member = str(root / relative_path)
                if member not in names:
                    raise SystemExit(f"manifest member missing: {member}")
                if member in covered:
                    raise SystemExit(f"duplicate manifest member: {member}")
                covered.add(member)
                actual = hashlib.sha256(archive.read(member)).hexdigest()
                if actual != expected:
                    raise SystemExit(f"manifest mismatch: {member}")
            if manifest_name == "MANIFEST.sha256":
                uncovered = names - covered - {manifest_name}
                if uncovered:
                    raise SystemExit(f"files missing from root manifest: {', '.join(sorted(uncovered))}")

        print(f"verified {args.archive}: {len(names)} files, {len(manifests)} manifest(s)")


if __name__ == "__main__":
    main()
