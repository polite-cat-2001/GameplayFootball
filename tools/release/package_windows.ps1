# Packages a portable Windows build into dist/GameplayFootball-<ver>-win32-<arch>.zip.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File tools/release/package_windows.ps1 -Version 0.3.0 -Arch x86
#   powershell -ExecutionPolicy Bypass -File tools/release/package_windows.ps1 -Version 0.3.0 -Arch x64
#
# Assumes the game was already built (build\Release for x86, build-x64\Release for x64).
# Dependencies come from vcpkg in manifest mode and live in
# <build-dir>\vcpkg_installed\<triplet>\bin; the classic install root
# ($VcpkgRoot\installed\<triplet>) is used as a fallback.

param(
  [Parameter(Mandatory = $true)][string]$Version,
  [ValidateSet('x86', 'x64')][string]$Arch = 'x86',
  [string]$BuildDir = '',
  [string]$VcpkgRoot = 'C:\dev\vcpkg',
  [string]$OutDir = 'dist'
)

$ErrorActionPreference = 'Stop'

if ($Arch -eq 'x86') {
  $triplet = 'x86-windows'
  $redistArch = 'x86'
  if (-not $BuildDir) { $BuildDir = 'build' }
} else {
  $triplet = 'x64-windows'
  $redistArch = 'x64'
  if (-not $BuildDir) { $BuildDir = 'build-x64' }
}

$exe = Join-Path $BuildDir 'Release\gameplayfootball.exe'
if (-not (Test-Path $exe)) { Write-Error "Not found: $exe (build first)" }

$stage = Join-Path $env:TEMP ("GameplayFootball-win32-" + $Arch)
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Path $stage | Out-Null

# binary + data (relative paths in the game resolve against the exe dir).
# Data comes from the build dir, where the POST_BUILD copy_data.cmake already
# applied data/ without the unused player photos (databases/default/faces).
Copy-Item $exe $stage
$release = Join-Path $BuildDir 'Release'
Copy-Item -Recurse -Force (Join-Path $release 'databases') $stage
Copy-Item -Recurse -Force (Join-Path $release 'media') $stage
Copy-Item -Force (Join-Path $release 'football.config') $stage

# dependency DLLs (transitive closure, resolved with dumpbin during the first
# release; boost/VC names carry the compiler + toolset so match by wildcard)
$vcpkgBin = Join-Path $BuildDir "vcpkg_installed\$triplet\bin"
if (-not (Test-Path $vcpkgBin)) {
  $vcpkgBin = Join-Path $VcpkgRoot "installed\$triplet\bin"
}
$dllNames = @(
  'SDL3.dll', 'SDL3_image.dll', 'SDL3_ttf.dll', 'OpenAL32.dll', 'sqlite3.dll',
  'jpeg62.dll', 'libpng16.dll', 'freetype.dll', 'z.dll', 'bz2.dll',
  'brotlidec.dll', 'brotlicommon.dll', 'fmt.dll',
  'boost_thread-*.dll', 'boost_filesystem-*.dll'
)
foreach ($pattern in $dllNames) {
  $files = Get-ChildItem $vcpkgBin -Filter $pattern -ErrorAction Stop
  if (-not $files) { Write-Error "Missing $pattern in $vcpkgBin" }
  foreach ($f in $files) { Copy-Item $f.FullName $stage }
}

# Visual C++ runtime (so the game runs without the VC++ redist installed;
# the api-ms-win-crt-* set ships with Windows 10+)
$redist = Get-ChildItem "C:\Program Files (x86)\Microsoft Visual Studio\2022\*\VC\Redist\MSVC\*\$redistArch\Microsoft.VC143.CRT\*.dll" `
  -ErrorAction Stop | Where-Object { $_.Name -in @('msvcp140.dll', 'msvcp140_atomic_wait.dll', 'vcruntime140.dll') }
foreach ($f in $redist) { Copy-Item $f.FullName $stage }

# short readme
@"
Gameplay Football - portable Windows build ($Arch)

Run: double-click gameplayfootball.exe, or from a terminal: gameplayfootball.exe

Controls:
  - Menu / navigation: arrow keys + Enter, Esc
  - Match: keyboard or gamepad. Left stick / arrows move, A / Enter = pass,
    B / X = kick (PES and FIFA presets on the controller screen, switch LB/RB)
  - Pause: Esc (keyboard) / Options (gamepad)
  - Quit: Esc from the main menu, close the window, or F12

Standalone folder: no installation required. Saves/league database live next to
the executable (databases/).

System requirements: Windows 10 64-bit or newer, OpenGL 3.2+ capable GPU.
"@ | Set-Content -Path (Join-Path $stage 'README.txt') -Encoding ASCII

$zipName = "GameplayFootball-v$Version-win32-$Arch.zip"
$zipPath = Join-Path $OutDir $zipName
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
if (Test-Path $zipPath) { Remove-Item $zipPath }
Compress-Archive -Path "$stage\*" -DestinationPath $zipPath
Write-Output "Created $zipPath ($([math]::Round((Get-Item $zipPath).Length / 1MB, 1)) MB)"
