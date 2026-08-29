## Gameplay Football

Football game. This repository is a **fork of [vi3itor/GameplayFootball](https://github.com/vi3itor/GameplayFootball)**,
itself a fork of the discontinued [GameplayFootball](https://github.com/BazkieBumpercar/GameplayFootball)
written by [Bastiaan Konings Schuiling](http://www.properlydecent.com/).

In 2019, Google Brain picked up the game and created a Reinforcement Learning environment based on it —
[Google Research Football](https://github.com/google-research/football). They improved the game and
updated the libraries, but threw away everything (menus, audio, HUD) that was not necessary for their RL task.

### What this fork adds

- Modern build: CMake 3.16+, C++17, a single static `blunted2` engine library, `sources.cmake` removed.
  On Windows the dependencies are declared in `vcpkg.json` (manifest mode) and installed automatically
  by CMake — no manual `vcpkg install` is needed; `data/` is copied next to the binary automatically
  (POST_BUILD) on every platform; the game builds as a GUI app (WIN32 subsystem) by default, with an
  option (`GAMEPLAYFOOTBALL_WINDOWS_SUBSYSTEM=OFF`) to keep a console for debugging.
- SDL2 → SDL3 (SDL3_image/SDL3_ttf; SDL_gfx dropped).
- Renderer migrated to **OpenGL 3.2 core profile**: legacy fixed-function pipeline
  (`glBegin`/`glEnd`, `glLightfv`, matrix stack) removed; rendering runs through the shader
  pipeline (`#version 150`, VAO/VBO). This is a prerequisite for future OpenGL ES
  (mobile) support.
- Builds and runs on Windows (MSVC + vcpkg), Linux (gcc) and macOS (verified on MacBook Air M2,
  2026-08-13; rendering runs on the main thread as AppKit requires, the scheduler on a helper thread).
- **Gamepad input reworked** (SDL3 `SDL_Gamepad`): semantic `SDL_GAMEPAD_BUTTON_*`/`AXIS_*` indices
  instead of raw joystick numbers (this fixes Xbox Series and any modern controller), PES/FIFA layout
  presets switched on the controller-select screen (LB/RB), menu navigation from stick and D-pad,
  and hot-plug (plug/unplug mid-match pauses and opens controller select). Menus use the standard
  layout (A = confirm, B = back) even with a single gamepad, and hot-plug also works on the
  controller-select screen before a match (device add/remove is tracked even when the window is
  not focused).
- Determinism tooling: `tools/determinism` runs the match headless and fingerprints the simulation
  (SHA-1) to catch unintended gameplay changes. The mechanism — `EnvState` serialization, fixed
  10 ms timestep, seeded RNG, mock renderer/audio — is ported from Google Research Football
  (the `google-brain` branch, GRF v2.10.1) without importing its gameplay changes. Platform
  references live in `tools/determinism/`.
- Project documentation lives in the wiki: `docs/wiki/index.md`.

## Branches

- `master` — the baseline fork: upstream GameplayFootball plus the build/modernization work described
  above (CMake, SDL3, OpenGL core profile, determinism tooling).
- `develop` — **the home for all personal game changes** on top of `master`: new gameplay modes,
  the Transfermarkt data pipeline, updated rosters, and anything that is not part of the upstream
  baseline.
- Feature branches (e.g. `squads-update`) are cut from `develop` and merged back into it.
- Remote branches (`google_brain`, `windows`, etc.) are upstream/experimental lines — do not merge
  them wholesale into `master` or `develop`.

## Building from source

### Linux

Install required dependencies (SDL3 apt packages exist since Ubuntu 26.04 LTS):
```bash
sudo apt-get install git cmake build-essential libgl1-mesa-dev libsdl3-dev \
libsdl3-image-dev libsdl3-ttf-dev libopenal-dev libboost-all-dev \
libsqlite3-dev
```

Build and run:
```bash
# Clone the repository
git clone https://github.com/Churikov0112/GameplayFootball.git
cd GameplayFootball

# Configure and build (data/ is copied next to the binary automatically)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Run from the build directory (the game uses relative data paths)
cd build
./gameplayfootball
```

The game also runs under WSL2 via WSLg (window and audio work; rendering is software, so the FPS is
low). On machines with an AMD GPU, the WSLg display server can crash on startup — workaround: add
`[wsl2] gpuSupport=false` to `%UserProfile%\.wslconfig` and restart WSL (`wsl --shutdown`).

### macOS

**Status**: verified on MacBook Air M2 (2026-08-13) — window, menu, match, textures and UI render,
clean exit. Window/GL-context creation and the SDL event pump live on the main thread (required by
AppKit); on low-RAM machines build with a single job (`cmake --build build --parallel 1`).

```bash
# Install dependencies (requires brew)
brew install git cmake sdl3 sdl3_image sdl3_ttf boost openal-soft

# Clone and build (data/ is copied next to the binary automatically)
git clone https://github.com/Churikov0112/GameplayFootball.git
cd GameplayFootball
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix)"
cmake --build build --parallel 1

# Run from the build directory (the game uses relative data paths)
cd build
./gameplayfootball
```

To get a ready-to-run app without building, download the macOS zip from the
[Releases](https://github.com/Churikov0112/GameplayFootball/releases) page — it is a
double-clickable `GameplayFootball.app` with all data and runtime libraries bundled.
The bundle is ad-hoc signed and **not notarized** (no Apple Developer account), so
macOS shows a one-time warning on first launch: right-click the app → **Open**, or
run `xattr -cr GameplayFootball.app`.

### Windows

Download and install:
- [Visual Studio](https://visualstudio.microsoft.com/downloads/) (2019 or newer),
- [Git](https://git-scm.com/download/win),
- [CMake](https://cmake.org/download/) (make sure to add it to the system PATH).

Install [`vcpkg`](https://github.com/microsoft/vcpkg) as explained in the
[Quick Start Guide](https://github.com/microsoft/vcpkg#quick-start-windows). Dependencies are
declared in `vcpkg.json` (manifest mode) and installed automatically by CMake — no manual
`vcpkg install` is needed.

Build and run:
```bat
git clone https://github.com/Churikov0112/GameplayFootball.git
cd GameplayFootball

cmake -B build -A Win32 -DVCPKG_TARGET_TRIPLET=x86-windows -DCMAKE_TOOLCHAIN_FILE=C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_WINDOWS_EXPORT_ALL_SYMBOLS=TRUE
cmake --build build --config Release --parallel
```

The `data/` directory is copied next to the binary automatically (POST_BUILD), so `gameplayfootball.exe`
inside `build\Release` can be run directly (the game uses relative data paths). By default the game is
built as a GUI application (WIN32 subsystem); pass `-DGAMEPLAYFOOTBALL_WINDOWS_SUBSYSTEM=OFF` to keep a
console for debugging.

## Verified dependency versions

Each platform pins its own dependency versions; watch for behavior differences when migrating
(e.g. the SDL3 semantic button names A/B aliases vs SOUTH/EAST exist only in some versions):

- vcpkg `x86-windows`: sdl3 3.4.12, sdl3-image 3.4.4, sdl3-ttf 3.2.2, openal-soft 1.25.1,
  boost 1.91.0, sqlite3 3.53.4
- Ubuntu 26.04: libsdl3-dev 3.4.2, libsdl3-image-dev 3.4.0, libsdl3-ttf-dev 3.2.2,
  libopenal-dev 1.25.1, boost 1.90.0, libsqlite3-dev 3.46.1
- brew (MacBook Air M2, 2026-08-13): sdl3 3.4.14, sdl3_image, sdl3_ttf, boost 1.90, openal-soft

## Releases

Prebuilt binaries for Windows (x86/x64), Linux and macOS are published on the
[Releases](https://github.com/Churikov0112/GameplayFootball/releases) page —
self-contained archives, no installation required. Packaging scripts live in
`tools/release/` (see `tools/release/README.md`).

## Problems?

If you have any problems, please open an issue.

### Donate

If you want to thank Bastiaan for his great work, consider a donation to his Bitcoin address 1JHnTe2QQj8RL281fXFiyvK9igj2VhPh2t
