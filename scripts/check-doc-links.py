"""Check tracked Markdown encoding, repository paths and GitHub heading anchors.

Run from any directory. --online also checks external HTTP links;
--release TAG includes the published GitHub release notes (requires gh).
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import html
import posixpath
from pathlib import Path
import re
import subprocess
from urllib.parse import unquote, urlsplit
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parents[1]
REPO = "heilmic/CoverPlayer"


def git(*args):
    return subprocess.check_output(["git", "-C", str(ROOT), *args])


def links(source):
    source = re.sub(r"(?ms)^\s*(`{3,}|~{3,})[^\n]*\n.*?^\s*\1\s*$", "", source)
    source = re.sub(r"`[^`\n]+`", "", source)
    found = re.findall(r"\]\(\s*(<[^>]+>|[^\s)]+)", source)
    found += re.findall(r"(?m)^\s*\[[^\]]+\]:\s*(<[^>]+>|\S+)", source)
    found += re.findall(r"(?:href|src)=[\"']([^\"']+)[\"']", source)
    found += re.findall(r"https?://[^\s<>\"`\)]+", source)
    return sorted({html.unescape(value.strip("<>")).rstrip(".,;") for value in found})


def anchors(source):
    result, counts = set(), {}
    for heading in re.findall(r"(?m)^#{1,6}\s+(.+?)\s*#*\s*$", source):
        heading = re.sub(r"!?\[([^\]]+)\]\([^)]*\)", r"\1", heading)
        heading = re.sub(r"<[^>]+>", "", html.unescape(heading)).lower()
        slug = "".join(c for c in heading if c.isalnum() or c in "_- ").replace(" ", "-")
        count = counts.get(slug, 0)
        counts[slug] = count + 1
        result.add(slug + (f"-{count}" if count else ""))
    result.update(re.findall(r'(?:id|name)=[\"\']([^\"\']+)[\"\']', source))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--online", action="store_true")
    parser.add_argument("--release", help="Check published release notes too")
    args = parser.parse_args()
    tracked = set(git("ls-files", "-z").decode("utf-8").split("\0")) - {""}
    trees = {"main": tracked}
    errors, external, checked = [], set(), 0

    def contents(path, ref="main"):
        if ref == "main":
            return (ROOT / path).read_text(encoding="utf-8")
        return git("show", f"{ref}:{path}").decode("utf-8")

    def check(document, source):
        nonlocal checked
        for href in links(source):
            checked += 1
            url = urlsplit(href)
            ref, target = "main", None
            if url.scheme in ("http", "https"):
                parts = unquote(url.path).strip("/").split("/")
                if url.netloc == "github.com" and parts[:2] == REPO.split("/") and len(parts) >= 5 and parts[2] in ("blob", "tree"):
                    ref, target = parts[3], "/".join(parts[4:])
                elif url.netloc == "raw.githubusercontent.com" and parts[:2] == REPO.split("/") and len(parts) >= 4:
                    ref, target = parts[2], "/".join(parts[3:])
                else:
                    external.add(href.split("#", 1)[0])
                    continue
            elif url.scheme:
                continue
            else:
                target = posixpath.normpath(posixpath.join(posixpath.dirname(document), unquote(url.path))) if url.path else document
            try:
                if ref not in trees:
                    trees[ref] = set(git("ls-tree", "-r", "--name-only", "-z", ref).decode("utf-8").split("\0")) - {""}
                tree = trees[ref]
                if target not in tree and not any(p.startswith(target.rstrip("/") + "/") for p in tree):
                    errors.append(f"{document}: missing {href}")
                elif url.fragment and target.endswith(".md"):
                    fragment = unquote(url.fragment)
                    if fragment not in anchors(contents(target, ref)):
                        errors.append(f"{document}: missing anchor {href}")
            except (UnicodeError, OSError, subprocess.CalledProcessError) as error:
                errors.append(f"{document}: cannot validate {href}: {error}")

    for name in sorted(tracked):
        if name.endswith(".md"):
            try:
                check(name, contents(name))
            except UnicodeError:
                errors.append(f"{name}: invalid UTF-8")
    if args.release:
        source = subprocess.check_output(["gh", "release", "view", args.release, "--repo", REPO, "--json", "body", "--jq", ".body"], encoding="utf-8")
        check(f"release:{args.release}", source)
    if args.online:
        def fetch(url):
            try:
                request = Request(url, headers={"User-Agent": "CoverPlayer-doc-link-check"})
                with urlopen(request, timeout=15) as response:
                    if response.status >= 400:
                        return f"HTTP {response.status}: {url}"
            except Exception as error:
                return f"{url}: {error}"
            return None
        with ThreadPoolExecutor(max_workers=6) as pool:
            errors.extend(error for error in pool.map(fetch, sorted(external)) if error)
    for error in errors:
        print(error)
    print(f"Checked {checked} references in tracked Markdown" + (f" and release {args.release}" if args.release else "") + f"; {len(errors)} error(s).")
    if not args.online:
        print(f"External URLs: {len(external)} unique; use --online to check HTTP availability.")
    return bool(errors)


if __name__ == "__main__":
    raise SystemExit(main())
