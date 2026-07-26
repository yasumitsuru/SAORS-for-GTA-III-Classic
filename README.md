# SAORS for GTA III Classic

An independent, clean-room project that aims to add online radio stations to the
classic 32-bit Windows release of Grand Theft Auto III.

> [!IMPORTANT]
> This project is in an early audio-prototype milestone. Configuration, playlist
> parsing, logging, safe ASI initialization, an optional libVLC backend, a
> standalone stream probe, tests, and build automation exist. GTA III radio hooks
> do not.

The final plugin is a Windows x86 DLL named `SAORSForGTA3.asi`. SteamOS support
means running that same Windows build through Proton/Wine; this is not a native
Linux game plugin.

## Current status

| Feature | Status |
| --- | --- |
| INI configuration | Implemented and unit-tested |
| Direct HTTP(S) stream URL parsing | Implemented and unit-tested |
| M3U/M3U8 text playlist parsing | Implemented and unit-tested |
| PLS parsing | Implemented and unit-tested |
| File logging | Implemented |
| ASI initialization | Initial, safe stub |
| Unsupported executable handling | Implemented |
| GTA III radio hooks | Planned; no verified addresses |
| Optional libVLC backend | Implemented; Windows x86 build and offline lifecycle tested |
| Standalone stream probe | Implemented; real-stream validation remains manual |
| MP3/AAC/HTTP(S) playback | Not yet validated with a real stream |
| SteamOS/Proton runtime validation | Planned |

No part of the current code mutes or replaces the original game radio.

## Scope and compatibility

The first research target is:

- GTA III Classic for Windows;
- executable version 1.0 US;
- 32-bit process;
- Windows 10/11, or the Windows build running under Proton/Wine.

The version is a research target, not a current compatibility claim. The plugin
deliberately reports every executable as `unsupported or not yet mapped` until a
reproducible, independently verified version fingerprint and hook map are reviewed.
See [Compatibility](docs/COMPATIBILITY.md).

## How it is intended to work

1. Ultimate ASI Loader loads `SAORSForGTA3.asi` into the 32-bit game process.
2. The plugin reads `SAORSForGTA3.ini` and writes `SAORSForGTA3.log`.
3. A verified game adapter observes vehicle, pause, station, and volume state.
4. `RadioController` selects a configured online station.
5. `StreamManager` owns exactly one stream through either `LibVlcAudioBackend` or
   the safe `NullAudioBackend` fallback.
6. When no safe online replacement is possible, the original radio remains active.

Steps 1, 2, 4, and 5 have initial implementations. Step 3 remains inactive, so the
backend is currently exercised through `saors_stream_probe`, not through gameplay.

## Build

### Windows 10/11 with Visual Studio 2022

Install Git, CMake 3.24 or newer, and the Visual Studio 2022 C++ desktop workload
with x86 tools. The environment checker does not install anything:

```powershell
.\tools\setup_windows.ps1
cmake --preset windows-msvc-x86-debug
cmake --build --preset windows-msvc-x86-debug
ctest --preset windows-msvc-x86-debug
```

Use `windows-msvc-x86-release` for a release build. Detailed instructions are in
[Building on Windows](docs/BUILDING_WINDOWS.md). libVLC is opt-in and requires the
official Win32 7z package or an equivalent user-supplied SDK/runtime layout.

### SteamOS/Arch cross-build

The cross-build requires an `i686-w64-mingw32` compiler and produces a Windows
PE32 DLL:

```bash
./tools/setup_steamos.sh
cmake --preset linux-mingw-x86-release
cmake --build --preset linux-mingw-x86-release
```

Cross-compiled test executables are not run directly. CI builds and runs the
platform-independent tests natively on Linux in a separate build tree. See
[Building on SteamOS](docs/BUILDING_STEAMOS.md).

## Standalone stream probe

The probe does not load GTA III or the ASI:

```powershell
.\build\windows-msvc-x86-release\bin\Release\saors_stream_probe.exe `
  --url "https://radio.example/stream" --duration 30 --volume 0.5 --buffer 3000
```

It reports state changes, supports pause/resume and reconnect checks, and redacts
credentials plus `token`, `key`, and `auth` query values. See
[Stream probe](docs/STREAM_PROBE.md) before testing a stream.

## Installation

There is no supported gameplay release yet. For development smoke tests only:

1. Back up the game installation.
2. Install the 32-bit build of
   [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader)
   according to its documentation.
3. Copy `SAORSForGTA3.asi` into the directory containing `gta3.exe`.
4. Copy `config/SAORSForGTA3.example.ini` to the same directory and rename it
   `SAORSForGTA3.ini`.
5. Start the game and inspect `SAORSForGTA3.log`.

An unrecognized executable is expected and must not cause the game to crash.

## Configuration example

```ini
[General]
Enabled=true
BufferMilliseconds=3000
Reconnect=true
ReconnectDelayMilliseconds=5000
VolumeMultiplier=1.0
LogLevel=info

[Station.HeadRadio]
Enabled=true
Name=Online Radio
URL=https://example.com/stream
```

Do not put credentials or private tokens in a configuration file shared with bug
reports.

## Limitations

- libVLC is optional and is not bundled or enabled by default.
- There are no installed game hooks or memory patches.
- AAC, MP3, HTTP(S), pause, reconnect, volume, and buffering have not yet been
  validated against a real stream.
- M3U8 parsing extracts absolute HTTP(S) entries; it is not an HLS implementation.
- plugin-sdk is not downloaded or linked yet.
- Proton/Wine installation has documentation but no verified compatibility result.

## Roadmap

- Add network and decoder tests for MP3/AAC over HTTP and HTTPS.
- Establish legal, reproducible GTA III 1.0 US executable fingerprints.
- Implement read-only game-state observation behind `GameIntegration`.
- Add guarded radio suppression/restoration with failure rollback.
- Validate Windows 10, Windows 11, Wine, and Proton.
- Add more executable adapters only after independent verification.

## Contributing

Read [CONTRIBUTING.md](CONTRIBUTING.md) before proposing code. Contributions must
be clean-room work and must not include GTA III files, proprietary SAORS source,
private streams, or unverified memory addresses.

## License and third-party software

Original project code is available under the [MIT License](LICENSE). Catch2 is
downloaded only for test builds under the Boost Software License 1.0. Optional
libVLC binaries and modules retain their own licenses and are not distributed by
this project. Details are in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Non-affiliation

This project is not affiliated with, endorsed by, or sponsored by Rockstar Games,
Take-Two Interactive, the authors of the original SAORS project, or the maintainers
of third-party modding tools. Grand Theft Auto and related marks belong to their
respective owners. Users must supply their own legally obtained game copy and
runtime tools.
