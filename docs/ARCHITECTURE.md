# Architecture

## Design goals

The project separates code that can be tested on a normal host from code that must
run inside GTA III. The default path performs no memory writes and preserves the
game's original radio when any prerequisite is missing.

```text
INI / playlists
       |
       v
Configuration + PlaylistParser
       |
       v
RadioController <---- GameIntegration (verified adapters only)
       |
       v
StreamManager ----> AudioBackendFactory ----> LibVlcAudioBackend (optional)
                         |                \--> NullAudioBackend (safe fallback)
                         |
saors_stream_probe ------+
       |
       +---- backend failure ----> original game radio remains active
```

## Components

### Configuration

`Configuration` parses a small documented INI subset without a runtime dependency.
It starts from safe defaults, reports invalid typed values, warns about unknown or
missing sections, and never evaluates environment variables or includes other files.

### PlaylistParser

`PlaylistParser` accepts direct HTTP/HTTPS URLs, line-oriented M3U/M3U8 content,
and numbered PLS `FileN` entries. It performs syntax-level URL validation only.
Downloading content, MIME detection, redirects, decoding, and TLS are delegated to
the selected audio backend. HLS interpretation is not implemented by the parser.

### AudioBackend

The interface owns operations for open, play, pause, stop, volume, state, and
errors. Version 0.2.0-dev keeps `NullAudioBackend` as the default and adds an
optional dynamically loaded libVLC 3 backend.

`AudioBackendFactory` contains all external-library initialization failures. With
libVLC support compiled, it searches an explicitly configured root and then
`vlc/` beside the executable. A missing DLL, incompatible architecture, incomplete
plugin directory, or libVLC initialization error produces a sanitized warning and
the null fallback.

The core resolves the libVLC C API with `LoadLibraryExW` and `GetProcAddress`.
Consequently, the ASI has no loader-time import dependency on `libvlc.dll`.
Headers and the import library are still validated as part of the supplied SDK so
the build uses one complete, versioned package. Runtime loading is restricted to
an absolute path and safe Windows DLL search flags.

The backend creates one instance and one media player, disables video, applies
per-media network caching, maps volume from `0.0–1.0` to `0–100`, and serializes
all public calls with a mutex. State is currently polled through the documented
player state API. Event callbacks were deliberately deferred so callback lifetime
cannot outlive an ASI-owned object.

The native libVLC logger is unset. This prevents diagnostic messages outside the
central URL sanitizer from exposing user info or sensitive query values.

### Stream probe

`saors_stream_probe` links only to `saors_core`. It never loads GTA III or
`GameIntegration`. It is the required validation surface for real streams,
pause/resume, reconnect, shutdown, and Wine/Proton before gameplay integration.

### StreamManager

`StreamManager` serializes access to one backend and prevents two active streams.
It tracks the selected URL, volume, errors, and an explicit reconnect operation.
Timed reconnect and buffer policy will be added alongside the real backend.

### GameIntegration

This is the only component allowed to know executable fingerprints, addresses,
calling conventions, or hook implementations. Its current table is intentionally
empty. Every query returns a safe sentinel and hook installation returns `false`.

plugin-sdk is a candidate implementation aid for future verified GTA III adapters.
It is not required for parsing, configuration, tests, or the current stub ASI, and
therefore is not fetched yet.

### RadioController

The controller maps the known station order to configuration keys such as
`HeadRadio` and `DoubleClef`. Its update path is dormant because `GameIntegration`
returns no in-game state. It never mutes the original radio in this milestone.

### ASI entry point

`DllMain` disables thread notifications and starts a short initialization worker.
The worker resolves paths relative to the plugin module, reads configuration, logs
the version and unsupported-executable state, asks `AudioBackendFactory` for a
backend, and leaves hooks disabled. External-library exceptions are contained so
that initialization failure cannot escape into the host process.

## Threading and lifetime

The initialization worker does not wait inside `DllMain`. Logging opens the file
for each message and contains all exceptions. No teardown hook runs under the
loader lock in the current milestone. A future long-lived controller must have an
explicit stop signal and bounded shutdown before unload.

## Security boundaries

- Playlist and INI text are untrusted input.
- Only absolute HTTP(S) URLs cross the backend boundary.
- libVLC owns protocol, TLS, redirect, decoder, and audio-device behavior; each
  release configuration still needs real-stream validation.
- Executable detection must use verified fingerprints before any address is read.
- Hook installation must be transactional and support rollback.
- Logs must not record credentials embedded in stream URLs.
