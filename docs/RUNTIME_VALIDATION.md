# Runtime validation

This document records only runtime checks that were actually executed. It does
not convert a libVLC `playing` state into an audible-output claim.

## Test context

| Item | Value |
| --- | --- |
| Dates | 2026-07-26 local fixtures; 2026-07-27 authorized AAC stream |
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
| Windows x86 | HTTPS M3U to HTTP media | AAC | yes | human-confirmed | human-confirmed | `0.0`, `0.3`, `1.0` human-confirmed | human-confirmed | timed and interrupted shutdown passed | real AAC/HTTP passed; HTTPS media not tested |
| Wine win32 prefix | pending | pending | pending | pending | pending | pending | pending | pending | not executed |
| Separate Proton prefix | pending | pending | pending | pending | pending | pending | pending | pending | not executed |

## Build and automated tests

- `SAORSForGTA3.asi` and `saors_stream_probe.exe` were confirmed as Windows
  PE32 Intel i386 artifacts.
- The MSVC Release Win32 build with libVLC completed successfully.
- CTest passed 33 of 33 tests, including backend lifecycle, repeated stop,
  URL validation, playlist parsing, and log sanitization.
- The standard pull-request workflows do not use a private stream URL.

## Authorized AAC stream

An authorized station supplied as a generic **Authorized HTTPS M3U station A**
was tested without retaining or publishing its URL or final hostname.

The outer playlist returned HTTP `200` as `audio/x-mpegurl`, redirected, and
contained one usable entry. Direct probe playback of the playlist followed
`opening -> error` and exited with `3`, confirming that the current product path
still does not resolve remote playlists.

For backend validation only, the entry was resolved outside the product code.
`ffprobe` identified the final media as AAC (`format_name=aac`); this result was
not inferred from a filename. The final media transport was HTTP, despite the
outer playlist using HTTPS.

| Check | Sanitized state sequence | Startup | Exit | Human result |
| --- | --- | --- | --- | --- |
| Basic, 30 seconds | `opening -> playing -> stopped` | 703 ms | `0` | continuous audio confirmed |
| Pause/resume | `opening -> playing -> paused -> playing -> stopped` | 803 ms; resume 100 ms | `0` | pause and return without overlap confirmed |
| Reconnect, 40 seconds | `opening -> playing -> stopped -> opening -> playing -> stopped` | 804 ms; reopen 602 ms | `0` | audio returned without simultaneous playback |
| Volume `0.0` | `opening -> playing -> stopped` | not recorded | `0` | silence confirmed |
| Volume `0.3` | `opening -> playing -> stopped` | not recorded | `0` | lower than `0.5` confirmed in immediate A/B repeat |
| Volume `1.0` | `opening -> playing -> stopped` | not recorded | `0` | louder than `0.5`; no perceived distortion |

The first isolated `0.3` run was not perceived as clearly lower than an earlier
`0.5` run. An immediate `0.5` then `0.3` A/B repeat was performed; both exited
with `0`, and the human listener confirmed that `0.3` was lower. This repeat is
the basis for the volume result above.

An automated `CTRL_BREAK_EVENT` was also sent after this real stream reached
`playing`. The cooperative handler stopped the backend on the main thread,
printed `stopped`, exited with `130`, and left no residual process.

Probe output was captured only in ignored temporary files so the URL was not
echoed during testing. The resolved URL and every raw probe/inspection log were
deleted after the sanitized facts above were collected.

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

The same interruption path was subsequently repeated while the authorized AAC
stream was active, with the same clean exit `130` and no residual process.

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

The authorized real HTTPS M3U behaved the same way when passed directly to the
probe: `opening -> error`, exit `3`. Resolving its one entry externally enabled
the AAC backend test above but did not add playlist support to the product.

The recommended next audio-layer phase is **Phase 2C — RemotePlaylistResolver**.
It should resolve authorized remote playlist content before passing one final
HTTP(S) media URL to the backend, without adding that larger HTTP responsibility
to this pull request.

## Pending real-stream validation

Authorized real AAC over HTTP, audible output, pause/resume, reconnect, volume,
and cooperative interruption are validated. Playlist retrieval over HTTPS and
an outer redirect were observed, but the media itself used HTTP; this is not
evidence of HTTPS media playback.

MP3 over HTTP, MP3 over HTTPS, AAC over HTTPS, and libVLC media TLS/redirect
behavior remain pending because no matching authorized direct media URLs were
supplied. GTA III integration, radio replacement, Wine, and Proton also remain
pending.
