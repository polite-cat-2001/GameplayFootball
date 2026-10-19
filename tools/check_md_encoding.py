#!/usr/bin/env python3
"""Check that Markdown files are UTF-8 without BOM and free of mojibake.

Usage:
  python tools/check_md_encoding.py             # all tracked *.md
  python tools/check_md_encoding.py --staged    # staged *.md (pre-commit hook)
  python tools/check_md_encoding.py PATH ...    # specific files
"""
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

MOJIBAKE_TOKENS = ("тАФ", "тАУ", "тАж", "тЖТ", "тЖР")


def has_box_cyrillic(line: str) -> bool:
    """Box-drawing next to Cyrillic is CP866 mojibake; diagrams have spaces there."""
    for i, ch in enumerate(line):
        if not "\u2500" <= ch <= "\u257f":
            continue
        if i > 0 and "\u0400" <= line[i - 1] <= "\u04ff":
            return True
        if i + 1 < len(line) and "\u0400" <= line[i + 1] <= "\u04ff":
            return True
    return False


def git_paths(*args: str) -> list[str]:
    out = subprocess.run(["git", *args], capture_output=True, check=True).stdout
    return [p for p in out.decode("utf-8").split("\0") if p]


def staged_paths() -> list[str]:
    return git_paths("diff", "--cached", "--name-only", "--diff-filter=ACMR", "-z", "--", "*.md")


def tracked_paths() -> list[str]:
    return git_paths("ls-files", "-z", "--", "*.md")


def staged_bytes(path: str) -> bytes:
    return subprocess.run(["git", "show", f":{path}"], capture_output=True, check=True).stdout


def check(data: bytes) -> list[str]:
    problems: list[str] = []
    if data.startswith(b"\xff\xfe") or data.startswith(b"\xfe\xff"):
        problems.append("UTF-16 BOM (file must be UTF-8)")
    if data.startswith(b"\xef\xbb\xbf"):
        problems.append("UTF-8 BOM (repo convention is no BOM)")
    if b"\x00" in data:
        problems.append("NUL bytes (binary/UTF-16 content)")
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError as exc:
        problems.append(f"invalid UTF-8 at byte {exc.start}")
        return problems
    for i, ch in enumerate(text):
        cp = ord(ch)
        if (cp < 0x20 and ch not in "\t\n\r") or cp == 0x7F:
            line = text.count("\n", 0, i) + 1
            problems.append(f"line {line}: control character U+{cp:04X} (escape artifact)")
            break
    for i, ch in enumerate(text):
        if ch == "\r" and (i + 1 == len(text) or text[i + 1] != "\n"):
            line = text.count("\n", 0, i) + 1
            problems.append(f"line {line}: stray carriage return (escape artifact)")
            break
    for lineno, line in enumerate(text.splitlines(), 1):
        if has_box_cyrillic(line):
            problems.append(f"line {lineno}: box-drawing mixed with Cyrillic (mojibake)")
            break
        token = next((t for t in MOJIBAKE_TOKENS if t in line), None)
        if token:
            problems.append(f"line {lineno}: mojibake marker {token!r}")
            break
    return problems


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--staged", action="store_true", help="check staged *.md instead of all tracked")
    ap.add_argument("paths", nargs="*")
    args = ap.parse_args()

    if args.paths:
        paths = args.paths
        reader = lambda p: Path(p).read_bytes()
    elif args.staged:
        paths = staged_paths()
        reader = staged_bytes
    else:
        paths = tracked_paths()
        reader = lambda p: Path(p).read_bytes()

    failed = 0
    for path in paths:
        try:
            data = reader(path)
        except (OSError, subprocess.CalledProcessError) as exc:
            print(f"{path}: cannot read ({exc})", file=sys.stderr)
            failed += 1
            continue
        problems = check(data)
        for msg in problems:
            print(f"{path}: {msg}", file=sys.stderr)
        failed += bool(problems)

    if failed:
        print(f"markdown encoding check failed for {failed} file(s)", file=sys.stderr)
        print("Docs must be UTF-8 without BOM; see AGENTS.md.", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
