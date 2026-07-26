# Runtime validation

This document records only runtime checks that were actually executed. It does
not convert a libVLC `playing` state into an audible-output claim.

## Test context

| Item | Value |
| --- | --- |
| Date | 2026-07-26 |
| Runtime commit | `14b4a7d32e0f2695fcbda7979dea4405c5c54eba` |
| Platform | Windows 10 Pro 25H2, build 26200.8894 |
| Build | MSVC Release, Win32 |
| Artifacts | Windows PE32, Intel i386 |
| VLC runtime | libVLC 3.0.23 Vetinari, Win32 |
| Archive | Official `vlc-3.0.23-win32.7z` |
| Archive SHA-256 | `f148ff49cdac6c0b6b7018ad7c4e6cd24c99bc6c2dea8258d82684261a639017` |

The archive hash was recomputed before testing. The runtime layout contained
both main DLLs, `plugins/`, the public headers, and the x86 import library. All
VLC files remained under the ignored `build/` tree and were not committed.

The dedicated build enabled the ASI, tests, stream probe, and libVLC backend.
Network tests and experimental hooks remained disabled.

## Result matrix

| Platform | Transport | Codec | Playing state | Audible | Pause | Volume | Reconnect | Clean shutdown | Result |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Windows x86 | Controlled local HTTP | PCM/WAV | yes | not human-confirmed | state control passed | API accepted `0.0`, `0.3`, `1.0`; perceived levels pending | stop/reopen passed | timed stop and console interruption passed | local control path passed |
| Windows x86 | Controlled local HTTP drop | PCM/WAV | yes before drop | not evaluated | not evaluated | `0.0` | not evaluated | yes | unexpected stop detected; exit `3` |
| Windows x86 | Controlled local HTTP M3U/PLS | playlist text | no | not applicable | not applicable | not applicable | not applicable | yes | all five fixtures rejected during playback |
| Windows x86 | Authorized HTTP stream | MP3 | pending | pending | pending | pending | pending | pending | no authorized URL supplied |
| Windows x86 | Authorized HTTPS stream | MP3 | pending | pending | pending | pending | pending | pending | no authorized URL supplied |
| Windows x86 | Authorized HTTP(S) stream | AAC/HE-AAC | pending | pending | pending | pending | pending | pending | no authorized URL supplied |
| Wine win32 prefix | pending | pending | pending | pending | pending | pending | pending | pending | not executed |
| Separate Proton prefix | pending | pending | pending | pending | pending | pending | pending | pending | not executed |

## Build and automated tests

- `SAORSForGTA3.asi` and `saors_stream_probe.exe` were confirmed as Windows
  PE32 Intel i386 artifacts.
- The MSVC Release Win32 build with libVLC completed successfully.
- CTest passed 33 of 33 tests, including backend lifecycle, repeated stop,
  URL validation, playlist parsing, and log sanitization.
- The standard pull-request workflows do not use a private stream URL.

## Controlled local playback

A low-amplitude PCM/WAV fixture was served only on localhost. It verified the
libVLC runtime path without claiming MP3, AAC, TLS, internet-stream, or audible
support.

| Check | Sanitized state sequence | Startup | Exit |
| --- | --- | --- | --- |
| Basic playback | `opening -> playing -> stopped` | 101 ms | `0` |
| Pause/resume | `opening -> playing -> paused -> playing -> stopped` | 101 ms; resume 100 ms | `0` |
| Reconnect | `opening -> playing -> stopped -> opening -> playing -> stopped` | 100 ms; reopen 101 ms | `0` |
| Volume `0.0` | `opening -> playing -> stopped` | 100 ms | `0` |
| Volume `0.3` | `opening -> playing -> stopped` | 100 ms | `0` |
| Volume `1.0` | `opening -> playing -> stopped` | 100 ms | `0` |

These volume runs prove parameter acceptance, state continuity, and crash-free
shutdown only. Silence, reduced volume, maximum perceived volume, and absence of
audible distortion remain pending human confirmation.

## Network failure and shutdown

For the network-drop check, a throttled localhost HTTP stream reached
`opening -> playing` in 6133 ms. The controlled server was then terminated. VLC
reported `stopped` rather than `error`; the probe treated that unexpected stop as
a failure, printed `audio backend stopped before playback completed`, exited
with `3`, and left no residual process or listener.

The exact `error` transition requested by the validation plan therefore did not
occur for this abrupt-EOF fixture. The observed behavior is recorded rather than
being reclassified.

During active local playback, an automated Windows `CTRL_BREAK_EVENT` used the
same cooperative handler branch as `CTRL_C_EVENT`. The main thread called
`AudioBackend::stop()`, printed `stopped`, exited with `130`, and left no residual
process. The console-close handler performs no libVLC work: it sets the stop flag
and waits at most four seconds for the main thread to finish. The exact
`CTRL_CLOSE_EVENT` path was not separately exercised.

## Sanitization

A synthetic `auth` query value was used only against a closed localhost port.
The probe printed `[redacted]`, the synthetic secret was absent from its file
log, the failure exited with `3`, and no process remained. Automated tests also
cover credentials and case-insensitive `token`, `key`, and `auth` values. No
synthetic URL was sent to the internet, and raw logs remain ignored.

## Remote M3U and PLS experiment

The probe accepted each playlist location as an absolute HTTP URL, but direct
libVLC playback failed for:

- M3U with one absolute entry;
- M3U with one relative entry;
- M3U with multiple entries;
- PLS with one absolute entry;
- PLS with one relative entry.

Every case followed `opening -> error` and exited with `3`. The probe currently
uses `PlaylistParser::isSupportedUrl()` only to validate its input. It does not
download playlist text or call `PlaylistParser::parse()`, so relative entries,
final-stream redirection, and multiple-station selection are not available in
this path.

The recommended next audio-layer phase is **Phase 2C — RemotePlaylistResolver**.
It should resolve authorized remote playlist content before passing one final
HTTP(S) media URL to the backend, without adding that larger HTTP responsibility
to this pull request.

## Pending real-stream validation

No authorized MP3 HTTP, MP3 HTTPS, or known AAC/HE-AAC URL was supplied. Those
three 30-second tests, TLS and redirect checks, human audible confirmation,
perceived volume behavior, audible pause/resume, and audible reconnection remain
pending. GTA III integration, radio replacement, Wine, and Proton also remain
pending.
