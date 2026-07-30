# SAORS for GTA III Classic

An independent, clean-room project that aims to add online radio stations to the
classic 32-bit Windows release of Grand Theft Auto III.

> [!IMPORTANT]
> This project is in an experimental integration milestone. Configuration,
> playlist parsing, logging, defensive executable fingerprinting, optional
> libVLC, standalone probes, a guarded read-only game-state observer, and a
> calculation-only radio controller, and a separately gated gameplay-stream MVP
> exist. All gameplay features are disabled by default; real playback additionally
> requires an explicit libVLC-enabled build and INI opt-in.

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
| ASI initialization | Safe by default; dry-run and opt-in observer smoke tests passed |
| Defensive PE32 x86 fingerprinting | Implemented; parser and Windows CNG SHA-256 tested |
| Known GTA III executable profiles | One locally reproduced identity profile and separate guarded address map; edition and region unverified |
| Standalone executable probe | Implemented; explicit path, JSON, redaction, no disk search |
| Unsupported executable handling | Implemented; no exact match means no hooks |
| GTA III state observer | Experimental MSVC x86 callback; exact fingerprint, expected bytes, dry-run, and rollback required |
| Snapshot-driven radio decisions | Implemented as an opt-in dry-run with sanitized plans and simulated state |
| Radio station raw mapping research | Recorder, versioned local evidence schema, portable validator, no automatic runtime map |
| GTA III radio replacement | Opt-in external stream MVP; suppression lifecycle is implemented behind two gates, but no production write mechanism is validated |
| Optional libVLC backend | Implemented; Windows x86 build, offline lifecycle, localhost, and authorized AAC/HTTP runtime tested |
| Standalone stream probe | Implemented; real AAC audio, controls, and shutdown manually validated |
| AAC over HTTP playback | Manually validated with an authorized stream |
| MP3 HTTP/HTTPS and AAC HTTPS | Pending authorized direct media URLs |
| Remote M3U/PLS resolution | Implemented; bounded WinHTTP retrieval, relative URLs, nesting, cycles, and real HTTPS M3U validated |
| HLS | Detected and explicitly unsupported |
| SteamOS/Proton runtime validation | Planned |

No production code currently mutes the original game radio. The suppression
interface fails closed through a null controller until a temporary, radio-only,
reversible write mechanism is independently validated for an exact profile.

## Scope and compatibility

The first research target is:

- GTA III Classic for Windows;
- executable version 1.0 US;
- 32-bit process;
- Windows 10/11, or the Windows build running under Proton/Wine.

The version is a research target, not a current compatibility claim. The registry
contains one locally reproduced profile named `GTA III Classic local candidate`,
but the available evidence does not identify its edition or region. An exact
match identifies those file bytes only. Phase 3B adds a separate address map that
is selected only after the exact match, evidence-level check, image-range checks,
and expected-byte validation. See
[Executable fingerprints](docs/EXECUTABLE_FINGERPRINTS.md) and
[Compatibility](docs/COMPATIBILITY.md). Structural compatibility with the
plugin-sdk `GAME_10EN` map is not an edition or region identification.

## How it is intended to work

1. Ultimate ASI Loader loads `SAORSForGTA3.asi` into the 32-bit game process.
2. Its initialization worker fingerprints the host PE32 x86 executable and
   compares it with an exact profile registry.
3. The plugin reads `SAORSForGTA3.ini` and writes `SAORSForGTA3.log`.
4. An optional guarded adapter observes vehicle, pause, raw station, and music
   preference through a consistent read-only snapshot.
5. An optional listener sends each snapshot to a pure `RadioDecisionEngine`.
6. `RadioController` submits sanitized `would-*` plans to a dry-run sink by
   default. A separately compiled and explicitly enabled MVP sink can execute
   the station already selected by the engine through `StreamManager`.
   A second build and INI gate may request original-radio suppression, but the
   production controller is intentionally unavailable until the write and exact
   restoration semantics are proven.
7. An opt-in `RadioStationObservationRecorder` records stable raw transitions only;
   it never names stations or starts network/audio work.
8. `saors_radio_map_tool` validates and compares sanitized local evidence. The
   reviewed registry and pure `RadioStationResolver` can resolve only exact-profile
   Phase 3F consumes that resolver only in the dry-run decision path: explicit raw
   bindings win, otherwise an exact-profile resolved identity may select an explicit
   identity binding. No resolver result can start real audio or networking.
   raw values; neither has a gameplay consumer.
9. `PlaylistResolver`, `StreamManager`, and audio backends remain isolated from
   gameplay and are exercised through `saors_stream_probe`.
10. The original radio remains the only real in-game audio source.

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
The game observer, pinned plugin-sdk compile probe, and radio-controller dry-run
are separate opt-in research modes documented in
[Read-only game-state observation](docs/GAME_STATE_OBSERVATION.md) and
[Radio controller dry-run](docs/RADIO_CONTROLLER_DRY_RUN.md).

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
  --url "https://www.centraldj.com.br/radios/centraldj/stream.m3u" `
  --allow-http-streams --duration 30 --volume 0.5 --buffer 3000
```

It reports state changes, supports pause/resume and reconnect checks, and redacts
credentials plus `token`, `key`, and `auth` query values. See
[Stream probe](docs/STREAM_PROBE.md) before testing a stream. The current
[runtime evidence](docs/RUNTIME_VALIDATION.md) covers controlled localhost
fixtures plus one authorized AAC/HTTP stream with human audio confirmation. It
is not proof of MP3 or HTTPS media playback.

See [Remote playlists](docs/REMOTE_PLAYLISTS.md) for WinHTTP limits, relative and
nested playlist behavior, HLS rejection, and the distinction between a blocked
HTTPS-to-HTTP redirect and an explicitly permitted HTTP media entry.

## Standalone executable probe

The executable probe never searches for a game and never loads the inspected file:

```powershell
.\build\windows-msvc-x86-release\bin\Release\saors_exe_probe.exe `
  --exe "C:\Games\GTA3\gta3.exe" --redact-path --compare-known
```

Use `--json` for JSON on stdout or `--output` for an ignored local report under
`research/local/`. The built-in registry contains one locally reproduced GTA III
Classic identity profile. It is not a version, region, address-map, or hook
compatibility claim. See
[Executable research workflow](docs/EXECUTABLE_RESEARCH_WORKFLOW.md).

## Installation

There is no supported gameplay release yet. For safe development smoke tests
only, keep the example's observer disabled and dry-run enabled:

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
ResolveRemotePlaylists=true
PlaylistConnectTimeoutMilliseconds=5000
PlaylistReceiveTimeoutMilliseconds=10000
PlaylistMaximumBytes=262144
PlaylistMaximumEntries=128
PlaylistMaximumRedirects=5
PlaylistMaximumDepth=3
VolumeMultiplier=1.0
LogLevel=info

[Experimental]
EnableGameObserver=false
ObserverDryRun=true
LogStateTransitions=true
EnableRadioController=false
RadioControllerDryRun=true
LogRadioDecisions=true
EnableGameplayAudioExecutor=false
MuteOriginalRadioDuringGameplayAudio=false

[Station.HeadRadio]
Enabled=true
Name=Rádio Central DJ
URL=https://www.centraldj.com.br/radios/centraldj/stream.m3u
AllowHttp=true
; GameStationRaw must be set only after local validation.
; Or bind an exact resolver identity instead. Do not set both keys.
; GameStationIdentity=riseFm
```

Do not put credentials or private tokens in a configuration file shared with bug
reports.

## Radio station mapping research

Phase 3D adds a portable, versioned evidence model and a recorder gated by the
separate `[Research]` section. The recorder is disabled by default, requires the
experimental observer build, records only stable raw transitions while the player
is in a vehicle, and does not feed `RadioDecisionEngine`. Two separate manual
sessions for the exact `gta3_classic_local_candidate` profile produced eleven
conflict-free, locally reproduced relationships for raws `0..9` and `11`. The
registry is limited to that exact profile and remains disconnected from gameplay.
Manual HUD labels belong in ignored `research/local/` reports and are not
published. Raw 10 remains unobserved, unknown, and absent from the registry. The
plugin-sdk `eRadioStations.h` list is corroborative only.

Use `saors_radio_map_tool --validate`, `--compare`, `--summarize`, or `--redact`
without GTA III, plugin-sdk, libVLC, WinHTTP, network, or audio.

Phase 3E adds `RadioStationResolver`, a pure C++17 lookup from an exact
executable profile and raw value to `resolved`, `unknownRaw`,
`unsupportedProfile`, or `invalidRaw`. It resolves only the reviewed raws `0..9`
and `11` for `gta3_classic_local_candidate`; raw `10` remains unknown.

Phase 3F consumes the resolver only in the calculation-only controller. A station
may use `GameStationRaw` or `GameStationIdentity`, never both.
`GameStationRaw > GameStationIdentity`: an exact raw binding is authoritative even
when disabled. Only without a raw binding does the controller consult the explicit
profile and resolver; only `resolved` identities can locate an identity binding.
There is no profile fallback, raw `10` cannot select `policeRadio` or `radioOff`,
and no audio, network, playlist, game-memory, or original-radio operation occurs.

## Limitations

- libVLC is optional and is not bundled or enabled by default.
- Shared builds install no game hook. An experimental MSVC x86 build can install
  one five-byte callback only after explicit INI opt-in and all validation gates.
- One exact GTA III Classic fingerprint is registered at the
  `locally_reproduced` evidence level; filename-only detection is still rejected.
- The registered executable's edition and region remain unverified. Its address
  map is locally reproduced for the exact fingerprint only and must not be
  generalized to GTA III 1.0 US, 1.1, or Steam.
- Pause, reconnect, volume API calls, network-failure handling, and cooperative
  shutdown passed with a controlled localhost PCM fixture.
- AAC over HTTP, audible pause/resume, reconnect, volume, and cooperative
  shutdown passed with an authorized real stream.
- MP3 over HTTP/HTTPS, AAC over HTTPS, and libVLC media TLS behavior remain
  unverified.
- Simple M3U8 radio lists are resolved; HLS is detected and rejected.
- The target station permits final HTTP media per station. That audio is not
  protected by TLS even though the configured playlist uses HTTPS.
- plugin-sdk is optional, fixed to one reviewed revision, and never linked into
  the ASI. Fetching is disabled by default.
- Raw station values have no public name mapping. Normalized volume represents
  the `0..127` menu preference, not effective audible output.
- The default radio-controller path produces plans and simulated state only. The
  separately gated gameplay MVP uses `StreamManager`, playlist resolution, and
  libVLC only after explicit build and INI opt-in.
- Original-radio suppression has an additional build gate and runtime opt-in,
  both disabled by default. Its lifecycle and fail-safe restoration are covered
  with a fake controller, but the production factory returns an unavailable null
  controller and performs no game write.
- Manual ASI unload is unsupported; the observer stays resident until normal
  process exit.
- Proton/Wine installation has documentation but no verified compatibility result.

## Roadmap

- Add authorized network and decoder evidence for MP3 over HTTP/HTTPS and AAC
  over HTTPS.
- Independently reproduce the local GTA III Classic candidate and separately
  establish evidence for the GTA III 1.0 US research target.
- Independently confirm the raw station map without starting online audio.
- Independently reproduce the local address map before widening support.
- Validate a temporary radio-only mixer mechanism, exact entry bytes, and
  restoration semantics before enabling any production suppression write.
- Validate Windows 10, Windows 11, Wine, and Proton.
- Add more executable adapters only after independent verification.

## Contributing

Read [CONTRIBUTING.md](CONTRIBUTING.md) before proposing code. Contributions must
be clean-room work and must not include GTA III files, proprietary SAORS source,
private streams, or unverified memory addresses.

## License and third-party software

Original project code is available under the [MIT License](LICENSE). Catch2 is
downloaded only for test builds under the Boost Software License 1.0. Optional
libVLC binaries and modules retain their own licenses. The optional pinned
plugin-sdk reference uses zlib-style terms. Neither dependency is distributed by
this project. Details are in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Non-affiliation

This project is not affiliated with, endorsed by, or sponsored by Rockstar Games,
Take-Two Interactive, the authors of the original SAORS project, or the maintainers
of third-party modding tools. Grand Theft Auto and related marks belong to their
respective owners. Users must supply their own legally obtained game copy and
runtime tools.
