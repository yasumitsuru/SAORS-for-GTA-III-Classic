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
StreamManager ----> AudioBackend interface ----> future backend
       |
       +---- failure ----> original game radio remains active
```

## Components

### Configuration

`Configuration` parses a small documented INI subset without a runtime dependency.
It starts from safe defaults, reports invalid typed values, warns about unknown or
missing sections, and never evaluates environment variables or includes other files.

### PlaylistParser

`PlaylistParser` accepts direct HTTP/HTTPS URLs, line-oriented M3U/M3U8 content,
and numbered PLS `FileN` entries. It performs syntax-level URL validation only.
Downloading content, MIME detection, HLS segment handling, redirects, and TLS are
responsibilities of a future network/audio backend.

### AudioBackend

The interface owns operations for open, play, pause, stop, volume, state, and
errors. Version 0.1.0 ships only `NullAudioBackend`, which fails explicitly instead
of pretending to play audio.

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
the version and unsupported-executable state, creates the null backend, and leaves
hooks disabled. Exceptions are contained so that initialization failure does not
escape into the host process.

## Threading and lifetime

The initialization worker does not wait inside `DllMain`. Logging opens the file
for each message and contains all exceptions. No teardown hook runs under the
loader lock in the current milestone. A future long-lived controller must have an
explicit stop signal and bounded shutdown before unload.

## Security boundaries

- Playlist and INI text are untrusted input.
- A future network backend must enforce TLS verification, redirect limits, size
  limits, timeouts, supported schemes, and safe metadata handling.
- Executable detection must use verified fingerprints before any address is read.
- Hook installation must be transactional and support rollback.
- Logs must not record credentials embedded in stream URLs.
