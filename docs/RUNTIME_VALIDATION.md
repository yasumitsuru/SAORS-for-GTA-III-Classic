# Runtime validation

This document separates machine-observed state from human audible confirmation.
`playing` alone is never presented as proof that sound was heard.

## Test context

| Item | Value |
| --- | --- |
| Date | 2026-07-27 |
| Phase 2C runtime commit | `2dc5b1a` |
| Platform | Windows 10 Pro 25H2, build 26200.8894 |
| Build | MSVC Release, Win32, warnings as errors |
| VLC runtime | libVLC 3.0.23 Vetinari, official Win32 7z |
| Archive SHA-256 | `f148ff49cdac6c0b6b7018ad7c4e6cd24c99bc6c2dea8258d82684261a639017` |
| Configured station | Public Central DJ HTTPS M3U station |

The runtime stayed under the ignored `build/` tree and no VLC file was committed.
Experimental hooks and opt-in network CTest remained disabled.

## Build and automated tests

- MSVC Release Win32 built `SAORSForGTA3.asi` and
  `saors_stream_probe.exe` with libVLC and WinHTTP enabled.
- CTest passed 56 of 56 tests with libVLC.
- The standard no-libVLC MSVC x86 CI suite passed 50 of 50 tests.
- A test-owned server bound to `127.0.0.1` covered direct media, M3U, PLS,
  relative entries, nesting, redirect, cycle, excessive body, and HLS. It
  terminated with each test.
- Fake-client tests cover HTTP policy, MIME detection, `text/plain`, UTF-8/BOM,
  status and transport errors, entry/depth limits, and reconnection selecting a
  changed media URL.
- Linux native host tests passed 45 of 45 with WinHTTP disabled.
- MinGW i686 built both PE32 Intel 80386 artifacts with WinHTTP enabled.

## Public Central DJ resolution

The configured public URL is:

```text
https://www.centraldj.com.br/radios/centraldj/stream.m3u
```

`--resolve-only --allow-http-streams` exited `0` and reported:

```text
Configured resource: HTTPS M3U
Playlist request: success
Playlist content type: audio/x-mpegurl
Playlist entries: 1
Selected entry index: 0
Selected entry: HTTP media
Resolution completed successfully
```

The complete selected URL was neither printed nor retained. Earlier authorized
inspection identified the media as AAC; the transport is HTTP, so this result is
not HTTPS media evidence.

## Integrated playback

The Phase 2C probe ran the configured M3U for 35 seconds with volume `0.5`,
3,000 ms media cache, pause at 8 seconds, and reconnect at 18 seconds. It exited
`0`:

```text
opening -> playing -> paused -> playing
fresh playlist resolution
opening -> playing -> stopped
```

Initial startup was 803 ms, resume polling observed `playing` after 100 ms, and
post-resolution reconnect reached `playing` in 1,106 ms. The process completed
cleanly with no residual probe process.

The first reconnect implementation exposed a genuine ordering problem: libVLC
had no active audio output immediately after stop. The final implementation
stores the desired volume while output is unavailable and applies it on the first
observed `playing` state. The corrected run above is the recorded result.

Integrated Central DJ playback was human-confirmed before pause, after resume,
and after playlist re-resolution/reconnection.

## Sanitization

The final probe file contained only generic resource type, MIME, counts, indices,
schemes, backend/runtime names, state, and timing. An audit found no `http://` or
`https://`, hostname, configured station name, credentials, or sensitive query
key. The ignored validation log was removed after this audit.

Automated tests also cover credential and case-insensitive `token`, `key`, and
`auth` redaction. The libVLC logger remains disabled.

## Phase 3A.1 executable and ASI smoke

On 2026-07-27, the MSVC Release Win32 no-libVLC build was compiled with warnings
as errors and experimental hooks disabled. CTest passed 78 of 78 tests.

The executable probe inspected only the path explicitly supplied by the user. Two
redacted local reports were identical and contained no personal path or raw
bytes. They established the `GTA III Classic local candidate` profile at the
`locally_reproduced` evidence level. The executable is PE32 Intel 386, but its
edition and region remain unverified. Exact profile matching is not an address-map
or hook compatibility result.

For a short ASI smoke test, a no-station INI and the trusted x86 artifact were
placed beside the game executable only after confirming those filenames were
absent. The existing save loaded, a vehicle could be entered, and the game's
original radio UI remained available. The process remained responsive and had
zero TCP connections during the observation. The sanitized ASI log reported:

```text
Fingerprint status: exact fingerprint match
Verification level: locally_reproduced
Hooks: disabled
Gameplay reads: disabled
Audio playback: not started
Network activity: not started
```

The log audit found no SHA-256 value, URL, drive path, or user-directory path.
The automation had no audio channel, so it did not verify audible in-game radio.
The game closed normally through its window, no residual process remained, and
the ASI, test INI, and generated log were removed to restore the prior state.
The installation already contained unrelated third-party loader/mod components,
so this is not a clean-install baseline.

## Prior controlled evidence

Controlled localhost PCM/WAV playback previously verified lifecycle, pause,
reconnect, volume API acceptance, unexpected EOF handling, timed stop, and
console interruption. An authorized AAC/HTTP backend-only run previously
verified audible output and perceived volume changes. Those results remain
useful backend evidence but are superseded by the integrated run for playlist
state and reconnect behavior.

## Phase 3B read-only observer smoke

On 2026-07-27, the experimental MSVC Release Win32 build used the exact pinned
plugin-sdk reference and warnings as errors. The executable remained identified
only as `gta3_classic_local_candidate` at `locally_reproduced`; its edition and
region were not inferred.

The final local matrix passed with warnings as errors: the default, plugin-sdk
compile-probe, and experimental-observer MSVC x86 configurations each passed
`95/95` tests; the Linux host configuration passed `86/86`; and the MinGW i686
build produced PE32 Intel 80386 ASI, stream-probe, and executable-probe binaries
without new warnings.

The required dry-run ran first with the observer disabled. The exact fingerprint,
separate address profile, image ranges, patch definition, and all expected-byte
windows matched. Hook writes were `false`. The game and save loaded, the original
radio remained available, the mod started no audio or network activity, and the
process closed without a residual process.

The subsequent opt-in run installed one five-byte game-process callback. It
called the original target and published read-only snapshots. Sanitized transition
evidence covered:

```text
gameReady: false/unavailable startup -> true after save load
pause menu: inactive -> active -> inactive
player: on foot -> in vehicle -> on foot -> in vehicle
radioStationRaw: 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11
radioVolume: available
```

The values above are raw observations, not station names. The test did not
establish which raw value means radio off in every context, so no public station
enum was added. The audited music-preference integer followed the menu slider's
`0..127` range and is normalized only as a preference; transition logs
intentionally record availability rather than its value.

The original in-game radio remained selectable and functional. `Enabled=false`,
remote playlist resolution disabled, and no station sections ensured no online
audio path could start. Process inspection observed zero GTA TCP connections
before and after gameplay interactions. The ASI reported no playlist retrieval,
stream open, or network start. The game closed normally with no residual process.
The temporary ASI, INI, and log were removed afterward.

Synthetic tests separately cover first-write, partial-write, post-write,
verification, and rollback failures because forcing those failures in a real game
process would be unsafe. Normal game exit reclaims the process-lifetime observer;
manual ASI unload remains unsupported.

## Phase 3C radio controller dry-run smoke

On 2026-07-28, the experimental MSVC Release Win32 build connected observer
snapshots to the calculation-only radio controller. The local INI explicitly
bound raw value `3` to the sanitized test key `LocalTest`. This value remains
only a local test binding; no official GTA III station name was inferred.

The save loaded on foot, a vehicle was entered, and the original radio remained
selectable and functional while raw station values changed. Sanitized controller
evidence covered:

```text
would start station key=LocalTest raw=3
would pause
would resume
would set preference-volume=0.18
would stop reason=station-not-bound
would stop reason=player-on-foot
```

Returning to raw `3` after an unbound value produced a fresh `would start`.
Opening and closing the pause menu produced one plan for each transition rather
than one per frame. A material slider change produced a normalized
`preference-volume` within `0.0..1.0`; it did not claim effective mixer or audible
volume. Repeated transitions were deduplicated and the whole run remained under
the sink's message limit.

The ASI reported that no gameplay audio executor was available and that audio
and network activity were not started. No backend, playlist resolver, gameplay
`StreamManager`, or libVLC process was used. Process inspection found zero
connections owned by the GTA process and no residual GTA or libVLC process
after normal exit.

The local log contained 93 lines and 15 decision transitions. It contained no
URL, hostname, personal path, user name, address, pointer, full executable hash,
token, credential, or playlist body. Expected-byte validation was represented
only by a matched status; no expected byte sequence was logged. The raw log was
not published. The temporary ASI, INI, and log were removed after the sanitized
summary was collected, and the game directory returned to its prior state.

## Current matrix

| Capability | Result |
| --- | --- |
| Remote M3U retrieval | implemented and real URL validated |
| Remote PLS retrieval | implemented and localhost validated |
| Relative playlist entries | implemented and localhost validated |
| Nested playlists | implemented with depth and cycle limits |
| HLS | detected, explicitly unsupported |
| Central DJ automatic resolution | passed |
| Central DJ automatic AAC playback state | passed for 35 seconds |
| Central DJ integrated audible confirmation | passed before pause, after resume, and after reconnect |
| Pause/resume state | passed |
| Reconnect with fresh resolution | passed |
| Volume `0.5` application | API/state and integrated human perception passed |
| Clean shutdown | passed |
| MP3 HTTP/HTTPS and AAC HTTPS | pending |
| Wine/Proton | pending |
| GTA III exact identity detection | locally reproduced; edition and region unverified |
| GTA III no-hook ASI load and vehicle smoke | passed in one existing third-party loader/mod environment |
| GTA III observer dry-run | all ranges and expected-byte checks passed; no byte sequence logged |
| GTA III guarded callback installation | passed on exact local candidate |
| GTA III game-ready and pause observation | passed |
| GTA III on-foot/in-vehicle observation | passed |
| GTA III raw radio observation | values `0..9` and `11`; names and radio-off mapping unverified |
| GTA III music preference | normalized dry-run change to `0.18` observed; effective output not claimed |
| GTA III radio controller dry-run | start, pause, resume, volume, unbound-stop, and on-foot-stop passed |
| GTA III observer network/audio isolation | zero SAORS connections and no online audio |
| GTA III in-game audible validation | not executed by the automation |
| GTA III original radio | remained intact during the observer smoke |

### Radio station research

Phase 3D keeps RadioStationObservationRecorder and the versioned evidence model on a side path from RadioDecisionEngine. The reviewed map registry is scoped to its exact profile and is never used to infer an INI binding. Phase 3E adds a pure resolver with no gameplay consumer.
