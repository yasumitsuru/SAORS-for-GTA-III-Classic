# Stream probe

`saors_stream_probe` validates playlist resolution and audio without GTA III, an
ASI loader, hooks, memory reads, or game files.

## Usage

```powershell
saors_stream_probe.exe --help
saors_stream_probe.exe `
  --url "https://www.centraldj.com.br/radios/centraldj/stream.m3u" `
  --allow-http-streams --resolve-only
saors_stream_probe.exe `
  --url "https://www.centraldj.com.br/radios/centraldj/stream.m3u" `
  --allow-http-streams --duration 30 --volume 0.5 --buffer 3000 `
  --pause-after 10 --reconnect-after 20 --log-file probe.log
```

| Option | Meaning |
| --- | --- |
| `--url` | Required configured absolute HTTP(S) resource |
| `--duration` | Total playback time in seconds; default `10` |
| `--volume` | `0.0` through `1.0`; default `1.0` |
| `--buffer` | Media network cache in milliseconds; default `3000` |
| `--pause-after` | Pause once, resume one second later |
| `--reconnect-after` | Stop and reconnect once after fresh resolution |
| `--resolve-playlists` | Enable remote resolution; this is the default |
| `--no-resolve-playlists` | Send the configured URL directly to the backend |
| `--allow-http-streams` | Permit HTTP media selected from an HTTPS playlist |
| `--resolve-only` | Resolve and validate without opening an audio backend |
| `--log-file` | Duplicate sanitized output to a file |
| `--help` | Print help without initializing audio |

The probe returns `0` only after successful resolution-only validation or after
playback for the requested duration and clean shutdown. Argument errors, resolver
or backend failures, startup timeout, unexpected stop, and exceptions return
nonzero. A handled console interruption exits with `130`.

## Backend setup

A default build reports `Backend: null` and cannot play audio. To use libVLC,
build with `SAORS_ENABLE_LIBVLC=ON` and either set `SAORS_LIBVLC_ROOT` to a
complete Win32 runtime or place it under `vlc/` beside the probe. Never combine a
32-bit probe with Win64 DLLs.

Playlist resolution requires `SAORS_ENABLE_WINHTTP=ON`, which is the Windows
default. Native Linux host tests compile the resolver with fake clients and do
not require WinHTTP.

## Resolution and state behavior

The default path resolves the configured resource before opening the backend.
Diagnostics include playlist type, Content-Type, entry count, selected index, and
selected scheme only. The final media URL is never printed.

After open/play, the probe waits up to 15 seconds for `playing` and reports state
changes plus startup time. Pause/resume must return to `playing`. Reconnect stops
the current media, downloads and resolves the configured playlist again, opens
the new selection, and must also return to `playing`.

`--resolve-only` is useful for checking HTTPS, redirects, MIME detection, parsing,
relative entries, nesting, and policy without producing audio. It does not prove
decoder or audio-device behavior. Conversely, `State: playing` alone is not an
audible-output claim.

## HTTP policy

HTTPS-to-HTTP redirects are blocked. An HTTP entry inside an HTTPS playlist is a
different operation and is blocked unless `--allow-http-streams` is present. That
flag accepts unencrypted final audio and should be used only for a station whose
policy is understood. HLS is detected but not implemented.

See [Remote playlists](REMOTE_PLAYLISTS.md) for formats, limits, and reconnect
semantics.

## Cooperative shutdown

On Windows, console events only set an atomic stop flag. The main thread stops
`StreamManager` and owns libVLC destruction. Console close waits at most four
seconds for cleanup; no libVLC or WinHTTP function runs in the callback.

On non-Windows hosts, `SIGINT` and `SIGTERM` set a signal-safe flag and use the
same main-thread cleanup path.

## URL safety

Output redacts user info and `token`, `key`, or `auth` query values. The libVLC
logger is disabled. The selected media hostname, path, query, complete URL, and
playlist body are not logged. Do not publish private command histories or raw
third-party diagnostics.

## Validation status

The public Central DJ HTTPS M3U resolved automatically to one explicitly allowed
HTTP media entry. On Windows x86 with libVLC 3.0.23, the integrated probe
completed a 35-second run, pause/resume, fresh resolution on reconnect, volume
`0.5`, and clean shutdown. Human confirmation for that exact integrated run is
tracked separately in [Runtime validation](RUNTIME_VALIDATION.md).

MP3 HTTP/HTTPS, AAC HTTPS, Wine, and Proton remain pending.
