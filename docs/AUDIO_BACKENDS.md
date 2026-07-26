# Audio backends

## Selection and safe fallback

`NullAudioBackend` is always compiled and remains the default safe path.
`LibVlcAudioBackend` exists only when CMake is configured with
`SAORS_ENABLE_LIBVLC=ON`.

The factory accepts these environment variables:

| Variable | Values | Purpose |
| --- | --- | --- |
| `SAORS_AUDIO_BACKEND` | `auto`, `libvlc`, `null` | Request a backend |
| `SAORS_LIBVLC_ROOT` | filesystem path | Select a complete runtime root |

With libVLC support compiled, `auto` requests libVLC. The factory tries the
explicit root, then `vlc/` beside the current executable, then the executable
directory itself. Any failure is contained and returns the null backend.

The ASI log records only the request, result, and a sanitized reason:

```text
Audio backend requested: libVLC
Audio backend selected: libVLC
```

or:

```text
Audio backend requested: libVLC
Audio backend unavailable: libVLC runtime layout is incomplete
Falling back to NullAudioBackend
```

No backend failure enables hooks or suppresses the original game radio.

## Required libVLC layout

Use a Windows x86 package for the 32-bit ASI and probe:

```text
vlc-root/
  libvlc.dll
  libvlccore.dll
  plugins/
    access/
    audio_output/
    codec/
    ...
  sdk/
    include/vlc/vlc.h
    lib/libvlc.lib
```

The official VLC 3.0.23 Win32 7z archive has this layout. Its ZIP omits `sdk/`.
CMake validates headers, version macros, import library, both runtime DLLs, an HTTP
plugin module, and the x86 PE machine type. The import library is a build-time SDK
integrity check; the project intentionally does not link it because loader-time
failure inside GTA III would defeat the null fallback.

At runtime the root is considered incomplete when either main DLL or `plugins/`
is absent. Plugin modules must stay together. Copying only `libvlc.dll` is not a
supported installation.

## Backend behavior

- `libvlc_new` creates one process-owned instance.
- `libvlc_media_player_new` creates one audio player.
- `--no-video`, `--no-video-title-show`, and `--intf=dummy` disable video/UI.
- `libvlc_media_new_location` accepts validated HTTP(S) locations.
- `:network-caching=<milliseconds>` is attached as a trusted media option.
- `libvlc_audio_set_volume` receives `0–100` after validating `0.0–1.0`.
- Player state is polled and mapped to the project state enum.
- `libvlc_media_player_set_pause`, play, and stop implement transport controls.
- Every public operation is mutex-protected and catches exceptions.
- Repeated stop and destruction are safe.
- The native libVLC logger is unset to prevent unsanitized URL output.

Event APIs were reviewed, but callbacks are not registered in this phase. Polling
keeps callback and unload lifetimes out of the host process until the ASI owns a
long-lived controller with an explicit shutdown protocol.

## URL privacy

The central sanitizer redacts:

```text
https://user:password@host/live
https://host/live?token=value
https://host/live?key=value
https://host/live?auth=value
```

Key matching is case-insensitive. Sanitization protects project logs, console
output, and libVLC error text. Avoid private URLs anyway: shell history, debugger
state, build caches, or operating-system telemetry remain outside this guarantee.

## Licensing and distribution

The backend is optional and no VLC binary is distributed. libVLC is
LGPL-2.1-or-later, while modules can introduce additional terms. Review
[Third-party notices](../THIRD_PARTY_NOTICES.md) before any packaging change.

Windows x86 initialization, offline lifecycle, and controlled localhost PCM
playback were tested with 3.0.23. Pause/resume, stop/reopen, volume API calls,
unexpected network stop handling, timed shutdown, and console interruption
completed without a residual process. These checks do not establish audible
output, codec decoding, TLS, or internet-stream compatibility.

Real MP3, AAC, HTTP, HTTPS, Wine, and Proton behavior remain unverified until
authorized stream-probe runs are completed. See
[Runtime validation](RUNTIME_VALIDATION.md).
