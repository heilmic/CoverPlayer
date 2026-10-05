"""Release archive integrity checks, including the Knulli launcher."""

from __future__ import annotations

import hashlib
import subprocess
import sys
import tempfile
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo


VERIFIER = Path(__file__).resolve().parents[1] / "scripts" / "verify-package.py"


def write_archive(path: Path, *, tamper_launcher: bool = False, add_unlisted: bool = False) -> None:
    launcher = b"#!/bin/sh\nexit 0\n"
    manifest = f"{hashlib.sha256(launcher).hexdigest()}  roms/ports/CoverPlayer.sh\n".encode()
    with ZipFile(path, "w", compression=ZIP_DEFLATED) as archive:
        for name, payload, mode in (
            ("roms/ports/CoverPlayer.sh", b"#!/bin/sh\nexit 1\n" if tamper_launcher else launcher, 0o755),
            ("MANIFEST.sha256", manifest, 0o644),
        ):
            info = ZipInfo(name)
            info.create_system = 3
            info.external_attr = mode << 16
            archive.writestr(info, payload)
        if add_unlisted:
            archive.writestr("roms/ports/unlisted.txt", b"unexpected")


def verify(path: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run([sys.executable, str(VERIFIER), str(path)], capture_output=True, text=True)


def main() -> int:
    with tempfile.TemporaryDirectory() as directory:
        archive = Path(directory) / "CoverPlayer-Knulli.zip"
        write_archive(archive)
        if verify(archive).returncode != 0:
            print("valid archive was rejected", file=sys.stderr)
            return 1

        write_archive(archive, tamper_launcher=True)
        if "manifest mismatch" not in verify(archive).stderr:
            print("modified launcher was accepted", file=sys.stderr)
            return 1

        write_archive(archive, add_unlisted=True)
        if "files missing from root manifest" not in verify(archive).stderr:
            print("unlisted file was accepted", file=sys.stderr)
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
