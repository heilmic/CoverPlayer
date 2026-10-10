"""Upload a Switch NRO through FTP, verify it and retain the previous binary."""
import argparse
from ftplib import FTP
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import struct
import sys
import uuid


def exists(ftp, name):
    # Listing, rather than a failed SIZE request, distinguishes missing files
    # from other server errors before we move the running application's binary.
    return any(PurePosixPath(entry).name == name for entry in ftp.nlst())


def upload(ftp, remote_path, data):
    target = PurePosixPath(remote_path)
    if not target.is_absolute() or ".." in target.parts or target.suffix.lower() != ".nro":
        raise ValueError("remotePath must be an absolute .nro path without '..'")
    if any(c in remote_path for c in "\r\n"):
        raise ValueError("Invalid FTP path")
    ftp.cwd(str(target.parent))
    name = target.name
    temporary = name + ".upload-" + uuid.uuid4().hex
    backup = name + ".previous"
    old_moved = False
    try:
        ftp.storbinary("STOR " + temporary, io.BytesIO(data), blocksize=65536)
        received = hashlib.sha256()
        ftp.retrbinary("RETR " + temporary, received.update, blocksize=65536)
        if received.digest() != hashlib.sha256(data).digest():
            raise RuntimeError("Uploaded NRO failed SHA-256 verification; original preserved")
        if exists(ftp, name):
            if exists(ftp, backup):
                ftp.delete(backup)
            ftp.rename(name, backup)
            old_moved = True
        try:
            ftp.rename(temporary, name)
        except Exception as error:
            if old_moved:
                try:
                    ftp.rename(backup, name)
                except Exception as recovery_error:
                    raise RuntimeError(f"Replacement and rollback failed; restore '{backup}' to '{name}'") from recovery_error
            raise RuntimeError("Replacement failed; previous NRO restored") from error
    finally:
        try:
            ftp.delete(temporary)
        except Exception:
            pass  # The upload is normally gone after the successful rename.


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path)
    parser.add_argument("--host")
    parser.add_argument("--port", type=int)
    parser.add_argument("--nro", required=True, type=Path)
    args = parser.parse_args()
    config = json.loads(args.config.read_text(encoding="utf-8-sig")) if args.config and args.config.exists() else {}
    address = args.host or config.get("host")
    if not address:
        parser.error("Specify --host or configure scripts/deploy-switch.local.json")
    port = args.port if args.port is not None else config.get("port", 5000)
    if not isinstance(port, int) or not 1 <= port <= 65535:
        parser.error("FTP port must be between 1 and 65535")
    data = args.nro.read_bytes()
    if len(data) < 128 or data[16:20] != b"NRO0" or not 128 <= struct.unpack_from("<I", data, 24)[0] <= len(data):
        raise ValueError("Local file is not a valid NRO")
    remote = config.get("remotePath", "/switch/CoverPlayer/CoverPlayer.nro")
    with FTP() as ftp:
        ftp.connect(address, port, timeout=30)
        ftp.login(config.get("username", "anonymous"), config.get("password", ""))
        ftp.set_pasv(True)
        upload(ftp, remote, data)
    print(f"Uploaded and SHA-256 verified: {address}:{port}{remote}")
    print("Previous NRO retained as CoverPlayer.nro.previous. Close FTP and start CoverPlayer yourself.")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"Switch deployment failed: {error}", file=sys.stderr)
        sys.exit(1)
