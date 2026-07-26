# Stream probe

`saors_stream_probe` validates the audio layer without GTA III, an ASI loader,
hooks, memory reads, or game files.

## Usage

```powershell
saors_stream_probe.exe --help
saors_stream_probe.exe --url "https://authorized.example/stream"
saors_stream_probe.exe --url "https://authorized.example/stream" --duration 30
saors_stream_probe.exe --url "https://authorized.example/stream" `
  --duration 30 --volume 0.5 --buffer 3000 `
  --pause-after 10 --reconnect-after 20 `
  --log-file probe.log
```

| Option | Meaning |
| --- | --- |
| `--url` | Required absolute HTTP(S) stream location |
| `--duration` | Total test time in seconds; default `10` |
| `--volume` | `0.0` through `1.0`; default `1.0` |
| `--buffer` | Network cache in milliseconds; default `3000` |
| `--pause-after` | Pause once at this elapsed second, resume one second later |
| `--reconnect-after` | Stop, reopen, and replay once at this elapsed second |
| `--log-file` | Duplicate sanitized probe output to a file |
| `--help` | Print CLI help without initializing audio |

The probe returns `0` only after confirmed playback for the requested duration and
clean shutdown. Argument errors, backend failures, timeout, unexpected stop, and
exceptions return nonzero.

## Backend setup

A default build reports `Backend: null` and exits nonzero for playback. To use
libVLC, build with `SAORS_ENABLE_LIBVLC=ON` and either:

1. set `SAORS_LIBVLC_ROOT` to the complete extracted Win32 runtime; or
2. put that runtime under `vlc/` beside the probe executable.

Never mix a 32-bit probe with Win64 DLLs. See
[Building on Windows](BUILDING_WINDOWS.md).

## State and timeout behavior

After open/play, the probe waits up to 15 seconds for `playing`. It prints state
changes only. `error`, an early stop/end, or timeout fails the run. If requested,
pause/resume and reconnect must each return to `playing`.

The runtime version is printed when libVLC confirms it. The probe does not infer
or print a codec name. Record the codec and transport from an independently known
test-stream description.

## URL safety

Console and file output redact user info and query values named `token`, `key`, or
`auth`. The libVLC default logger is disabled because it can otherwise print the
original media location. Do not publish private stream URLs or command histories.

## Manual validation matrix

Run each row separately and keep the raw result private if the URL is private:

| Platform | Transport | Codec | Pause | Volume | Reconnect | Shutdown while playing |
| --- | --- | --- | --- | --- | --- | --- |
| Windows x86 | HTTP | MP3 | pending | pending | pending | pending |
| Windows x86 | HTTPS | MP3 | pending | pending | pending | pending |
| Windows x86 | HTTP or HTTPS | AAC | pending | pending | pending | pending |
| Wine win32 prefix | repeat applicable rows | | pending | pending | pending | pending |
| Separate Proton prefix | repeat applicable rows | | pending | pending | pending | pending |

For every run record:

- exact probe commit and build type;
- exact VLC version and archive hash;
- OS, Wine, or Proton version;
- known stream transport and codec;
- sanitized state sequence and exit code;
- audible output confirmation;
- pause/resume, volume, reconnect, and process-exit behavior.

The automated network test is disabled by default. It runs only when configured
with both `SAORS_ENABLE_NETWORK_TESTS=ON` and a private
`SAORS_TEST_STREAM_URL`. Standard pull-request workflows never enable it.

No real stream or Wine/Proton result has been recorded for 0.2.0-dev yet.
