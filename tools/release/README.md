# Release packaging

Builds a runnable binary on each platform and packages it into a self-contained
archive that can be downloaded and double-clicked. GitHub attaches the source
tarballs automatically when a release is created.

## Artifacts

| Platform | Asset | Machine |
|---|---|---|
| Windows x86 | `GameplayFootball-v<ver>-win32-x86.zip` | this machine (MSVC + vcpkg `x86-windows`) |
| Windows x64 | `GameplayFootball-v<ver>-win32-x64.zip` | this machine (MSVC + vcpkg `x64-windows`) |
| Linux | `GameplayFootball-v<ver>-linux-x86_64.tar.gz` | WSL2 Ubuntu (gcc) |
| macOS | `GameplayFootball-v<ver>-macos-<arch>.zip` | Mac (brew) |
| Data (catalog) | `GameplayFootball-data-<data_version>.zip` | `python tools/release/package_data.py` |
| Data (faces) | `GameplayFootball-faces-<data_version>.zip` | `python tools/release/package_data.py --faces-only` |

## Workflow

1. Build each target on its machine (see README.md). Windows x86 lives in
   `build/`, Windows x64 in `build-x64/` (both gitignored).
2. Package (run from the repo root):
   - Windows: `powershell -ExecutionPolicy Bypass -File tools/release/package_windows.ps1 -Version <ver> -Arch x86` (and `-Arch x64`)
   - Linux: `./tools/release/package_linux.sh <ver> <build-dir>`
   - macOS: `/opt/homebrew/bin/bash tools/release/package_macos.sh <ver> <build-dir>` (run on the Mac;
     the script needs bash 4 for `declare -A`, macOS system bash is 3.2)
3. Verify each archive: unpack it on a clean machine, double-click / run the
   binary, play a match, exit cleanly.
4. Publish (from this machine, `gh` authed):
   ```bash
   git tag v<ver> && git push origin v<ver>
   gh release create v<ver> dist/GameplayFootball-v<ver>-* --title "Gameplay Football v<ver>" --generate-notes
   ```
5. Data assets (needed so a source build gets the database, images and faces):
   `python tools/release/package_data.py` writes both bundles to `dist/` and records their
   checksums in `data-versions.json`. The catalog zip is **not** byte-reproducible, so to
   attach faces to an already-published catalog use
   `python tools/release/package_data.py --faces-only` — it builds only the faces bundle and
   never rebuilds the catalog. Both bundles unpack **into** `databases/default/`
   (`unzip ... -d <build-dir>/databases/default`); the game's README documents this for users.

## Notes

- Windows packages carry the transitive DLL closure (SDL3, SDL3_image,
  SDL3_ttf, OpenAL, Boost, sqlite3, image codecs, VC++ runtime), so they run
  without any installation. Re-check the closure with `dumpbin /dependents`
  when dependency versions change.
- The macOS bundle is ad-hoc signed and **not notarized**: first launch needs
  right-click → Open (or `chmod -R u+w` + `xattr -cr GameplayFootball.app`; a bare
  `xattr -dr` fails on Homebrew's read-only dylibs). Notarization would
  require an Apple Developer account and is out of scope for now.
- The macOS game resolves paths relative to the CWD, so the bundle ships a launcher:
  the real binary is `Contents/MacOS/gameplayfootball-bin` and
  `Contents/MacOS/gameplayfootball` is a script that chdir's into
  `Contents/Resources/data` before exec — that is what makes the .app
  double-clickable from Finder.
- Linux depends on system SDL3/OpenAL/Boost/sqlite3 packages (listed in the
  archive's README); a fully portable AppImage is a future option.
- `dist/` is gitignored; release archives are not committed.
