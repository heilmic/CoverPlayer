import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("switch_deploy", Path(__file__).resolve().parents[1] / "scripts/deploy-switch.py")
deploy = importlib.util.module_from_spec(spec)
spec.loader.exec_module(deploy)


class FakeFtp:
    def __init__(self, corrupt=False, fail_rename=False):
        self.files = {"CoverPlayer.nro": b"old", "CoverPlayer.nro.previous": b"older"}
        self.corrupt = corrupt
        self.fail_rename = fail_rename

    def cwd(self, path):
        if path != "/switch/CoverPlayer":
            raise RuntimeError("Wrong destination")

    def storbinary(self, command, stream, blocksize):
        self.files[command[5:]] = stream.read()

    def retrbinary(self, command, callback, blocksize):
        callback(b"corrupt" if self.corrupt else self.files[command[5:]])

    def nlst(self):
        return list(self.files)

    def delete(self, name):
        del self.files[name]

    def rename(self, source, target):
        if self.fail_rename and ".upload-" in source:
            raise RuntimeError("Rename failed")
        if target in self.files:
            raise RuntimeError("Target exists")
        self.files[target] = self.files.pop(source)


class SwitchDeployTest(unittest.TestCase):
    def test_success_retains_backup(self):
        ftp = FakeFtp()
        deploy.upload(ftp, "/switch/CoverPlayer/CoverPlayer.nro", b"new")
        self.assertEqual(ftp.files, {"CoverPlayer.nro": b"new", "CoverPlayer.nro.previous": b"old"})

    def test_corrupt_upload_preserves_existing_files(self):
        ftp = FakeFtp(corrupt=True)
        with self.assertRaisesRegex(RuntimeError, "SHA-256"):
            deploy.upload(ftp, "/switch/CoverPlayer/CoverPlayer.nro", b"new")
        self.assertEqual(ftp.files, {"CoverPlayer.nro": b"old", "CoverPlayer.nro.previous": b"older"})

    def test_failed_promotion_rolls_back(self):
        ftp = FakeFtp(fail_rename=True)
        with self.assertRaisesRegex(RuntimeError, "restored"):
            deploy.upload(ftp, "/switch/CoverPlayer/CoverPlayer.nro", b"new")
        self.assertEqual(ftp.files, {"CoverPlayer.nro": b"old"})

    def test_first_install(self):
        ftp = FakeFtp()
        ftp.files.clear()
        deploy.upload(ftp, "/switch/CoverPlayer/CoverPlayer.nro", b"new")
        self.assertEqual(ftp.files, {"CoverPlayer.nro": b"new"})


if __name__ == "__main__":
    unittest.main()
