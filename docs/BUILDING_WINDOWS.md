# Building on Windows

## Requirements

- Windows 10 or Windows 11;
- Git;
- CMake 3.24 or newer;
- Visual Studio 2022 or Build Tools 2022;
- the Desktop development with C++ workload;
- MSVC x86/x64 build tools and a Windows SDK;
- Ninja only if using a custom local preset.

Run the read-only environment check from PowerShell:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\tools\setup_windows.ps1
```

The script reports missing software and never installs it.

## Debug

```powershell
cmake --preset windows-msvc-x86-debug
cmake --build --preset windows-msvc-x86-debug
ctest --preset windows-msvc-x86-debug
```

The multi-configuration Visual Studio build writes the plugin under:

```text
build/windows-msvc-x86-debug/bin/Debug/SAORSForGTA3.asi
```

## Release

```powershell
cmake --preset windows-msvc-x86-release
cmake --build --preset windows-msvc-x86-release
ctest --preset windows-msvc-x86-release
```

The release artifact is:

```text
build/windows-msvc-x86-release/bin/Release/SAORSForGTA3.asi
```

The preset selects the Visual Studio `Win32` platform. CMake also rejects an ASI
build when the target pointer size is not 32 bits.

## CMake options

| Option | Default | Meaning |
| --- | --- | --- |
| `SAORS_BUILD_TESTS` | `ON` | Build Catch2 host tests |
| `SAORS_ENABLE_WARNINGS_AS_ERRORS` | `OFF` | Promote warnings to errors |
| `SAORS_BUILD_ASI` | `ON` for Windows targets | Build `SAORSForGTA3.asi` |
| `SAORS_BUILD_STREAM_PROBE` | `ON` | Build the standalone stream probe |
| `SAORS_ENABLE_LIBVLC` | `OFF` | Compile the optional dynamic libVLC backend |
| `SAORS_LIBVLC_ROOT` | empty | Root of a supplied Win32 SDK/runtime |
| `SAORS_ENABLE_NETWORK_TESTS` | `OFF` | Enable private, opt-in stream tests |
| `SAORS_TEST_STREAM_URL` | empty | Private URL used only by opt-in tests |
| `SAORS_ENABLE_EXPERIMENTAL_HOOKS` | `OFF` | Compile guarded experimental code |

Enabling the last option currently installs no hooks because there is no verified
address map. It exists to make later experimental work an explicit build choice.

## Optional libVLC Windows x86 build

The official VLC 3.0.23 Win32 **7z** archive contains both the runtime and
`sdk/`. The ZIP archive deliberately omits the SDK and is insufficient for this
build. Download the archive from the
[official VideoLAN directory](https://downloads.videolan.org/pub/videolan/vlc/3.0.23/win32/)
and verify:

```text
vlc-3.0.23-win32.7z
SHA-256 f148ff49cdac6c0b6b7018ad7c4e6cd24c99bc6c2dea8258d82684261a639017
```

Extract it outside the source tree or under the ignored `build/` directory. The
selected root must contain:

```text
vlc-3.0.23/
  libvlc.dll
  libvlccore.dll
  plugins/
  sdk/include/vlc/vlc.h
  sdk/lib/libvlc.lib
```

Configure a separate Win32 tree:

```powershell
$vlcRoot = "C:\path\to\vlc-3.0.23"
cmake -S . -B build/windows-msvc-x86-libvlc -A Win32 `
  -DSAORS_BUILD_ASI=ON `
  -DSAORS_BUILD_TESTS=ON `
  -DSAORS_BUILD_STREAM_PROBE=ON `
  -DSAORS_ENABLE_LIBVLC=ON `
  "-DSAORS_LIBVLC_ROOT=$vlcRoot"
cmake --build build/windows-msvc-x86-libvlc --config Release --parallel
ctest --test-dir build/windows-msvc-x86-libvlc -C Release --output-on-failure
```

CMake reads the DLL PE header and rejects a non-x86 runtime. It does not download
VLC. No VLC file is copied into source control or included in project artifacts.

For runtime use, either set `SAORS_LIBVLC_ROOT` or place the complete extracted
runtime in a `vlc/` directory beside `saors_stream_probe.exe` or the ASI host
executable. Do not copy only the two top-level DLLs; the plugin modules are
required.

See [Audio backends](AUDIO_BACKENDS.md) and [Stream probe](STREAM_PROBE.md).

## Opt-in network test

Only enable this with a stream you are authorized to use:

```powershell
$privateUrl = Read-Host "Private test stream URL"
cmake -S . -B build/windows-msvc-x86-network -A Win32 `
  -DSAORS_ENABLE_LIBVLC=ON `
  "-DSAORS_LIBVLC_ROOT=$vlcRoot" `
  -DSAORS_ENABLE_NETWORK_TESTS=ON `
  "-DSAORS_TEST_STREAM_URL=$privateUrl"
cmake --build build/windows-msvc-x86-network --config Debug --parallel
ctest --test-dir build/windows-msvc-x86-network -C Debug `
  --output-on-failure -L network
```

The value can exist in the ignored build tree and process environment; never
publish that tree or its test metadata.

## Runtime library

MSVC builds use the matching static multithreaded runtime (`/MT` or `/MTd`) across
project targets. Third-party backends added later must use a compatible allocation
boundary or provide explicit create/destroy functions.

## Visual Studio and VS Code

Visual Studio 2022 can open the repository folder and consume
`CMakePresets.json`. VS Code can use the CMake Tools extension. Local paths and
machine-specific settings belong in ignored `CMakeUserPresets.json`, never in the
shared presets.
