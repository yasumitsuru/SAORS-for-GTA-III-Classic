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

Human audio confirmation for this exact integrated Phase 2C run is pending the
listener's response. Earlier backend-only AAC/HTTP tests, where the entry was
resolved outside the product, had human-confirmed continuous audio, pause/resume,
reconnect, and volume differences; those earlier results are not silently reused
as confirmation of the new integrated run.

## Sanitization

The final probe file contained only generic resource type, MIME, counts, indices,
schemes, backend/runtime names, state, and timing. An audit found no `http://` or
`https://`, hostname, configured station name, credentials, or sensitive query
key. The ignored validation log was removed after this audit.

Automated tests also cover credential and case-insensitive `token`, `key`, and
`auth` redaction. The libVLC logger remains disabled.

## Prior controlled evidence

Controlled localhost PCM/WAV playback previously verified lifecycle, pause,
reconnect, volume API acceptance, unexpected EOF handling, timed stop, and
console interruption. An authorized AAC/HTTP backend-only run previously
verified audible output and perceived volume changes. Those results remain
useful backend evidence but are superseded by the integrated run for playlist
state and reconnect behavior.

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
| Central DJ integrated audible confirmation | pending listener response |
| Pause/resume state | passed |
| Reconnect with fresh resolution | passed |
| Volume `0.5` application | API/state passed; integrated perception pending |
| Clean shutdown | passed |
| MP3 HTTP/HTTPS and AAC HTTPS | pending |
| Wine/Proton | pending |
| GTA III hooks | not implemented |
