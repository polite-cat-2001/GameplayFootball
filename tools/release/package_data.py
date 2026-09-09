#!/usr/bin/env python3
"""Package the game data directory into a versioned release bundle.

The bundle is the whole playable data catalog that tm-gf-import produces into
`data/databases/default`: the SQLite database, the media trees and the manifest.
It is what a release ships instead of shipping raw TM data.

Steps:
  1. read manifest.json next to the DB (schema_version / data_version /
     snapshot_id / sha256);
  2. verify the DB schema: PRAGMA user_version must equal databaseSchemaVersion
     and match manifest.schema_version;
  3. zip `data/databases/default` (excluding faces/ and .gitignore) into
     dist/GameplayFootball-data-<data_version>.zip;
  4. record the bundle in `data-versions.json` at the repo root.

Usage:
    python tools/release/package_data.py                 # defaults: data/databases/default -> dist/
    python tools/release/package_data.py --data <dir> --out <dir> --game-version <ver>

`game_version` is read from `git describe --tags` when not given.
"""

from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import os
import sqlite3
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DATABASE_SCHEMA_VERSION = 2  # src/gamedefines.hpp: databaseSchemaVersion

EXCLUDE_DIRS = {"faces"}
EXCLUDE_FILES = {".gitignore"}


def _git_describe() -> str:
    try:
        out = subprocess.run(
            ["git", "describe", "--tags", "--abbrev=0"],
            cwd=ROOT, capture_output=True, text=True, timeout=10,
        )
        if out.returncode == 0 and out.stdout.strip():
            return out.stdout.strip()
    except (OSError, subprocess.SubprocessError):
        pass
    return "dev"


def _sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def _manifest(data_dir: Path) -> dict:
    path = data_dir / "manifest.json"
    if not path.exists():
        sys.exit(f"manifest.json not found at {path} — run tm-gf-import first")
    return json.loads(path.read_text(encoding="utf-8"))


def _verify_schema(data_dir: Path, manifest: dict) -> None:
    db_path = data_dir / "database.sqlite"
    con = sqlite3.connect(db_path)
    try:
        user_version = con.execute("PRAGMA user_version").fetchone()[0]
    finally:
        con.close()
    schema_version = manifest.get("schema_version")
    if user_version != DATABASE_SCHEMA_VERSION:
        sys.exit(f"database schema mismatch: PRAGMA user_version={user_version}, "
                 f"expected {DATABASE_SCHEMA_VERSION} (run tm-gf-import)")
    if schema_version != DATABASE_SCHEMA_VERSION:
        sys.exit(f"manifest schema_version={schema_version}, expected "
                 f"{DATABASE_SCHEMA_VERSION}")


def _add_bundle(data_dir: Path, archive: Path) -> None:
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        for root, dirs, files in os.walk(data_dir):
            dirs[:] = [d for d in dirs if d not in EXCLUDE_DIRS]
            for name in files:
                if name in EXCLUDE_FILES:
                    continue
                full = os.path.join(root, name)
                rel = os.path.relpath(full, data_dir)
                zf.write(full, rel)


def _registry_entry(manifest: dict, sha256: str, game_version: str) -> dict:
    return {
        "version": manifest.get("data_version"),
        "snapshot_id": manifest.get("snapshot_id"),
        "sha256": sha256,
        "game_version": game_version,
        "date": datetime.date.today().isoformat(),
    }


def _update_registry(entry: dict) -> Path:
    reg = ROOT / "data-versions.json"
    rows = []
    if reg.exists():
        rows = json.loads(reg.read_text(encoding="utf-8"))
    rows = [r for r in rows if not (
        r.get("version") == entry["version"] and r.get("snapshot_id") == entry["snapshot_id"]
    )]
    rows.append(entry)
    rows.sort(key=lambda r: str(r.get("version")))
    reg.write_text(json.dumps(rows, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return reg


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--data", default=str(ROOT / "data" / "databases" / "default"))
    ap.add_argument("--out", default=str(ROOT / "dist"))
    ap.add_argument("--game-version", default=None)
    args = ap.parse_args()

    data_dir = Path(args.data)
    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    manifest = _manifest(data_dir)
    _verify_schema(data_dir, manifest)
    data_version = manifest.get("data_version") or "unknown"
    game_version = args.game_version or _git_describe()

    archive = out_dir / f"GameplayFootball-data-{data_version}.zip"
    _add_bundle(data_dir, archive)
    sha = _sha256(archive)

    entry = _registry_entry(manifest, sha, game_version)
    reg = _update_registry(entry)

    print(f"bundle: {archive}")
    print(f"  data_version   = {data_version}")
    print(f"  snapshot_id    = {manifest.get('snapshot_id')}")
    print(f"  schema_version = {manifest.get('schema_version')}")
    print(f"  sha256         = {sha}")
    print(f"  game_version   = {game_version}")
    print(f"  registry: {reg}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())