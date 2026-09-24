#!/usr/bin/env python3
"""Package the game data directory into a versioned release bundle.

The bundle is the whole playable data catalog that tm-gf-import produces into
`data/databases/default`: the SQLite database, the media trees and the manifest.
It is what a release ships instead of shipping raw TM data.

Two assets are produced, so a source build can be completed from the Releases
page (faces/ is ~660 MB and ships as a separate bundle):

  1. read manifest.json next to the DB (schema_version / data_version /
     snapshot_id / sha256);
  2. verify the DB schema: PRAGMA user_version must equal databaseSchemaVersion
     and match manifest.schema_version;
  3. zip `data/databases/default` (excluding faces/ and .gitignore) into
     dist/GameplayFootball-data-<data_version>.zip;
  4. zip `data/databases/default/faces` into
     dist/GameplayFootball-faces-<data_version>.zip;
  5. record both bundles in `data-versions.json` at the repo root.

`--faces-only` builds just the faces bundle and adds its checksum to the
existing registry entry; use it to attach faces to an already-published catalog
(the catalog zip is not byte-reproducible, so it must not be rebuilt then).

Usage:
    python tools/release/package_data.py                 # defaults: data/databases/default -> dist/
    python tools/release/package_data.py --data <dir> --out <dir> --game-version <ver>
    python tools/release/package_data.py --faces-only

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

FACES_DIR = "faces"
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


def _zip_tree(root: Path, arc_base: Path, archive: Path, exclude_dirs=frozenset()) -> None:
    """Zip `root`, storing paths relative to `arc_base` so the bundle unpacks
    straight into `databases/default/`."""
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        for dirpath, dirs, files in os.walk(root):
            dirs[:] = [d for d in dirs if d not in exclude_dirs]
            for name in files:
                if name in EXCLUDE_FILES:
                    continue
                full = Path(dirpath) / name
                zf.write(full, full.relative_to(arc_base))


def _registry_entry(manifest: dict, sha256: str, faces_sha256, game_version: str) -> dict:
    entry = {
        "version": manifest.get("data_version"),
        "snapshot_id": manifest.get("snapshot_id"),
        "sha256": sha256,
        "game_version": game_version,
        "date": datetime.date.today().isoformat(),
    }
    if faces_sha256 is not None:
        entry["faces_sha256"] = faces_sha256
    return entry


def _faces_bundle(data_dir: Path, out_dir: Path, data_version: str):
    faces_dir = data_dir / FACES_DIR
    if not faces_dir.is_dir():
        return None
    archive = out_dir / f"GameplayFootball-faces-{data_version}.zip"
    _zip_tree(faces_dir, data_dir, archive)
    return archive, _sha256(archive)


def _merge_faces_checksum(manifest: dict, faces_sha256: str) -> Path:
    """Set faces_sha256 on the existing registry entry, leaving its catalog
    sha256 untouched (the published catalog bundle is not rebuilt)."""
    reg = ROOT / "data-versions.json"
    rows = json.loads(reg.read_text(encoding="utf-8")) if reg.exists() else []
    version, snapshot = manifest.get("data_version"), manifest.get("snapshot_id")
    for row in rows:
        if row.get("version") == version and row.get("snapshot_id") == snapshot:
            row["faces_sha256"] = faces_sha256
            reg.write_text(json.dumps(rows, ensure_ascii=False, indent=2) + "\n",
                           encoding="utf-8")
            return reg
    sys.exit(f"no data-versions.json entry for {version} / {snapshot} — "
             f"publish the catalog bundle first or run without --faces-only")


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
    ap.add_argument("--faces-only", action="store_true",
                    help="build only the faces bundle and add its checksum to the "
                         "existing registry entry")
    args = ap.parse_args()

    data_dir = Path(args.data)
    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    manifest = _manifest(data_dir)
    _verify_schema(data_dir, manifest)
    data_version = manifest.get("data_version") or "unknown"
    game_version = args.game_version or _git_describe()

    if args.faces_only:
        faces = _faces_bundle(data_dir, out_dir, data_version)
        if faces is None:
            sys.exit(f"faces directory not found: {data_dir / FACES_DIR}")
        faces_archive, faces_sha = faces
        reg = _merge_faces_checksum(manifest, faces_sha)
        print(f"faces bundle: {faces_archive}")
        print(f"  data_version = {data_version}")
        print(f"  faces_sha256 = {faces_sha}")
        print(f"  registry: {reg}")
        return 0

    archive = out_dir / f"GameplayFootball-data-{data_version}.zip"
    _zip_tree(data_dir, data_dir, archive, exclude_dirs={FACES_DIR})
    sha = _sha256(archive)

    faces = _faces_bundle(data_dir, out_dir, data_version)
    faces_sha = faces[1] if faces is not None else None

    entry = _registry_entry(manifest, sha, faces_sha, game_version)
    reg = _update_registry(entry)

    print(f"bundle: {archive}")
    print(f"  data_version   = {data_version}")
    print(f"  snapshot_id    = {manifest.get('snapshot_id')}")
    print(f"  schema_version = {manifest.get('schema_version')}")
    print(f"  sha256         = {sha}")
    if faces is not None:
        print(f"faces bundle: {faces[0]}")
        print(f"  faces_sha256   = {faces_sha}")
    else:
        print(f"faces bundle: skipped ({data_dir / FACES_DIR} not found)")
    print(f"  game_version   = {game_version}")
    print(f"  registry: {reg}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())