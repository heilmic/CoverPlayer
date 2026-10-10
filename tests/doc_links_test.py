"""Regression checks for broken links that Windows file lookup can conceal."""
import contextlib
import importlib.util
import io
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("doc_links", Path(__file__).resolve().parents[1] / "scripts/check-doc-links.py")
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)


class DocumentationLinksTest(unittest.TestCase):
    def run_check(self, markdown):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            subprocess.run(["git", "init", "-q", directory], check=True)
            (root / "README.md").write_bytes(markdown)
            (root / "Guide.md").write_text("# Übersicht\n## Install\n## Install\n", encoding="utf-8")
            (root / "local-only.md").write_text("# Local", encoding="utf-8")
            subprocess.run(["git", "-C", directory, "add", "README.md", "Guide.md"], check=True)
            output = io.StringIO()
            with patch.object(checker, "ROOT", root), patch("sys.argv", ["check-doc-links.py"]), contextlib.redirect_stdout(output):
                result = checker.main()
            return result, output.getvalue()

    def test_unicode_duplicate_anchors_and_code_examples(self):
        result, output = self.run_check("[Guide](Guide.md#übersicht)\n[Second](Guide.md#install-1)\n```md\n[example](absent.md)\n```\n".encode("utf-8"))
        self.assertFalse(result, output)

    def test_case_sensitive_paths_and_untracked_files(self):
        result, output = self.run_check(b"[Wrong case](guide.md)\n[Local](local-only.md)\n")
        self.assertTrue(result)
        self.assertIn("missing guide.md", output)
        self.assertIn("missing local-only.md", output)

    def test_missing_heading(self):
        result, output = self.run_check(b"[Missing](Guide.md#not-there)\n")
        self.assertTrue(result)
        self.assertIn("missing anchor", output)

    def test_invalid_utf8(self):
        result, output = self.run_check(b"# Resolution 1280 \xd7 720\n")
        self.assertTrue(result)
        self.assertIn("invalid UTF-8", output)


if __name__ == "__main__":
    unittest.main()
