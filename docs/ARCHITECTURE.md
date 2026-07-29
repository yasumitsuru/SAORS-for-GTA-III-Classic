# Architecture

## Design goals

The project separates code that can be tested on a normal host from code that must
run inside GTA III. The default path performs no memory writes and preserves the
game's original radio when any prerequisite is missing.

```text
explicit file / ASI host
       |
       v
PeImageReader ---> FileHasher / Windows CNG
       |                    |
       +----> ExecutableFingerprint ---> ExecutableProfileRegistry
                                           |
                                           v
                                  GameIntegration
                                        |
                                        v
                         GameAddressProfile + expected bytes
                                        |
                                        v
                      experimental callback --> GameStateSnapshot
                                                       |
                                                       v
                                              snapshot listener
                                                       |
                                                       v
                                           RadioDecisionEngine
                                                       |
                                                       v
                                             RadioActionPlan
                                                       |
                                                       v
                                          DryRunRadioActionSink
                                                       |
                                                       v
                                           SimulatedRadioState

INI station URL
       |
       v
Configuration ------------------------------------> decision inputs

standalone probe / future executor only
       |
       v
StreamManager ----> PlaylistResolver ----> HttpClient ----> WinHTTP
       |                    |
       |                    \----> PlaylistParser + URI resolution
       v
AudioBackendFactory ----> LibVlcAudioBackend (optional)
       |              \---> NullAudioBackend (safe fallback)
       |
saors_stream_probe
       |
       +---- backend failure ----> original game radio remains active
```

## Components

### Configuration

`Configuration` parses a small documented INI subset without a runtime dependency.
It starts from safe defaults, reports invalid typed values, warns about unknown or
missing sections, and never evaluates environment variables or includes other files.

### PlaylistParser

`PlaylistParser` accepts direct HTTP/HTTPS URLs, safe absolute or relative
M3U/M3U8 entries, and numbered PLS `FileN` entries. It remains network-independent.
HLS interpretation is not implemented.

### PlaylistResolver and HttpClient

`PlaylistResolver` owns remote-resource classification and bounded playlist
resolution. It detects playlists by extension and Content-Type, validates UTF-8,
rejects HLS, resolves relative entries against the final post-redirect URL, limits
entries and nesting, detects cycles, and selects the first usable entry.

`HttpClient` keeps this logic testable on Linux and with deterministic fakes.
Windows production builds use `WinHttpClient`, a synchronous RAII wrapper around
WinHTTP with system TLS validation, timeouts, response-size and redirect limits,
and cooperative cancellation checks. HTTPS-to-HTTP redirects are blocked. An
HTTP entry from an HTTPS playlist is a separate per-station policy.

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
`GameIntegration`. It resolves playlists by default and supports resolution-only
and direct-backend modes. It is the required validation surface for real streams,
pause/resume, reconnect, shutdown, and Wine/Proton before gameplay integration.

### StreamManager

`StreamManager` serializes access to one backend and prevents two active streams.
It stores the configured URL separately from the in-memory selected media URL.
Starting a configured resource resolves it before opening the backend. Reconnect
stops playback, downloads and resolves again under the same mutex, and opens the
new selection. No body or resolved URL is written to disk.

### GameIntegration

This is the only component that may select executable-specific runtime access.
Executable identity, evidence status, address profiles, expected bytes, hook
state, and current gameplay state remain separate types.

The shared build has no observer implementation. The experimental MSVC x86 build
can select `GameAddressProfile` only after an exact identity match and a
`locally_reproduced` or stronger status. Dry-run validates all ranges and byte
windows without writing. Opt-in installation replaces one audited five-byte call,
calls the original function, captures read-only state, and rolls back on any
write or verification failure.

`GameStateSource` has unavailable, fake, and audited runtime implementations.
Each capture publishes one mutex-protected `GameStateSnapshot`; individual fields
use `std::optional` so one failed read does not create an ambiguous sentinel.

### Executable fingerprinting

`PeImageReader` defensively parses only the bounded PE32 x86 metadata needed for
identity. `FileHasher` abstracts full-file and range SHA-256; Windows uses
incremental CNG/BCrypt while Linux tests inject deterministic fakes.

`ExecutableProfileRegistry` requires exact full-file hash, `.text` hash, and
metadata equality. A structural-only match is diagnostic and unsupported.
`saors_exe_probe` accepts one explicit path, produces text or JSON, and supports
path redaction. It never searches disks, loads the image, or copies it.

plugin-sdk is an optional pinned compile and research reference for the Phase 3B
adapter. It is not required for parsing, configuration, Linux host tests, audio,
networking, or either probe, and fetching remains disabled by default. It is not
linked into the ASI.

### RadioController

The controller implements `GameStateSnapshotListener` and owns a configuration
copy plus simulated state. `RadioDecisionEngine` is a pure portable state machine.
It accepts an explicit raw binding or exact-profile resolved identity binding and produces a
sanitized `RadioActionPlan`. No hardcoded GTA station-name map exists.

`DryRunRadioActionSink` applies plans only to its own simulated state and logs
deduplicated transitions. Neither the controller nor its sink includes or
references `StreamManager`, a playlist resolver, WinHTTP, libVLC, or a gameplay
audio API.

### ASI entry point

`DllMain` disables thread notifications and starts a short initialization worker.
The worker resolves the host executable path, creates its fingerprint, compares
the profile registry, and logs only sanitized match state. The shared
configuration leaves the observer and controller disabled. When both opt-ins are
present, process-lifetime controller and sink objects are registered before the
observer is installed. The ASI may construct the pure resolver for dry-run decisions, but does not construct a stream manager,
audio backend, or WinHTTP client. Initialization exceptions are contained.

## Threading and lifetime

The initialization worker does not wait inside `DllMain`. Logging contains all
exceptions, records transitions only, and stops growing at 1 MiB. No teardown
hook runs under the loader lock. The observer, optional listener/controller,
dry-run sink, and logger state intentionally live until process exit because safe
manual ASI unload has not been proven. The hook transaction still exposes
idempotent removal for controlled use and tests.

## Security boundaries

- Playlist and INI text are untrusted input.
- WinHTTP owns playlist HTTP/TLS; normal certificate and hostname validation is
  never disabled.
- Only a validated final absolute HTTP(S) media URL crosses the backend boundary.
- libVLC owns media transport, decoding, and audio-device behavior; each release
  configuration still needs real-stream validation.
- Executable identity requires exact file, `.text`, and structural matching; the
  fingerprint still does not validate any future offset.
- Unknown, partially matched, or modified executables receive no gameplay read or
  memory write.
- Hook installation validates all definitions before writing, verifies the final
  bytes, restores page protection, and rolls back in reverse order.
- The observer reads gameplay state only on the game thread and never dereferences
  the returned vehicle pointer.
- The callback has no route to WinHTTP, libVLC, `PlaylistResolver`, or
  `StreamManager`.
- Logs must not record credentials embedded in stream URLs.

### Radio station research

Phase 3D keeps RadioStationObservationRecorder and the versioned evidence model on a side path from RadioDecisionEngine. The reviewed map registry is scoped to its exact profile and is never used during INI parsing. Phase 3F passes an explicit profile and pure resolver only to the calculation-only dry-run controller: `GameStationRaw > GameStationIdentity`, no profile fallback, and raw 10 remains unknown. No audio, network, playlist, game-write, or original-radio operation is added.
