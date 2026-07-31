# Radio controller dry-run

Phase 3C connects the read-only observer to a calculation-only radio controller.
The original default path has no gameplay executor.

```text
GameStateSnapshot
        |
        v
RadioDecisionEngine
        |
        v
RadioActionPlan
        |
        v
DryRunRadioActionSink --> SimulatedRadioState
```

`RadioController` implements the optional `GameStateSnapshotListener`. The
observer calls the original game function first, captures and publishes a
consistent snapshot, and then notifies the listener on the game thread. The
controller calculates one plan and submits it to the dry-run sink.

## Safe configuration

Shared defaults are:

```ini
[Experimental]
EnableGameObserver=false
ObserverDryRun=true
LogStateTransitions=true
EnableRadioController=false
RadioControllerDryRun=true
LogRadioDecisions=true
EnableGameplayAudioExecutor=false
MuteOriginalRadioDuringGameplayAudio=false
```

The ASI runtime path additionally requires the build option
`SAORS_ENABLE_RADIO_CONTROLLER_DRY_RUN=ON`. It is `OFF` by default. Setting
`RadioControllerDryRun=false` in an INI produces a warning and remains dry-run.
The separate `EnableGameplayAudioExecutor` opt-in is `false` by default and is
available only in a build with `SAORS_ENABLE_GAMEPLAY_STREAM_EXECUTOR=ON`.
Original-radio suppression additionally requires
`SAORS_ENABLE_ORIGINAL_RADIO_SUPPRESSION=ON`, the INI opt-in shown above, an
exact profile, and an available validated controller.

Station binding is explicit and optional:

```ini
[Station.LocalTest]
Enabled=true
Name=Local Test Station
URL=https://example.invalid/stream
; GameStationRaw must be set only after local validation.
```

`GameStationRaw` accepts `0..255`. Negative, overflowing, and nonnumeric values
are errors. Two enabled stations cannot use the same value. No binding is
inferred from `LocalTest`, `HeadRadio`, or any other section name.

## Sink behavior and logging

`DryRunRadioActionSink` changes only `SimulatedRadioState`. It has no URL, network,
audio, game-memory, mixer, or `StreamManager` API. `NullRadioActionSink` discards
plans; tests use a fake sink.

Only non-`none` plan transitions are logged. Logs are deduplicated, limited to
256 messages, and rate-limited to one message per 250 ms. Public station keys
accept only a short alphanumeric, underscore, or hyphen form; unsafe keys become
generic values such as `StationRaw3`. Logs can contain a raw value and normalized
preference volume, but never the configured URL.

## Gameplay stream MVP

When both the build gate and INI opt-in are enabled, `GameplayRadioActionSink`
receives the already selected `StationConfiguration` from a `RadioActionPlan`.
It does not inspect raw values, resolve identities, or repeat binding logic.
It uses `StreamManager`, `PlaylistResolver`, and the configured audio backend to
map `wouldStart`, `wouldSwitch`, `wouldPause`, `wouldResume`,
`wouldSetVolume`, and `wouldStop` to real playback operations.

An empty URL, invalid URL, rejected HTTP setting, resolution error, backend
failure, out-of-order plan, or shutdown is fail-closed: playback is stopped and
the executor remains stopped for the process lifetime without retrying. No URL
or backend error details are logged. If the configured playback backend is
unavailable during initialization, the controller remains in dry-run mode.
Suppression integration mutes only after a successful stream start and restores
on every inactive or fail-closed transition. The production suppression
controller is currently unavailable, so no game write occurs and the original
radio remains audible. There is no fade, advanced reconnect behavior, raw-10
mapping, or `policeRadio` behavior in this MVP.

## Default ASI isolation

The ASI gameplay entry point does not construct:

- `AudioBackend` or `AudioBackendFactory`;
- `WinHttpClient`;
- `PlaylistResolver`;
- `StreamManager`;
- a libVLC gameplay runtime.

Those components remain available to the standalone stream probe and their unit
tests. Default Phase 3C/3F ASI builds must not contain `winhttp.dll` or
`libvlc.dll`; the separately gated gameplay MVP is intentionally the exception.

The controller, sink, configuration copy, and game integration use process
lifetime when the listener is enabled. This avoids a listener pointing to stack
objects after the initialization worker exits. Manual ASI unload remains
unsupported; normal process exit reclaims the objects.

## Non-goals

This phase does not start, pause, resume, switch, set volume, or stop real audio.
It does not download a playlist, load libVLC intentionally, mute the original
radio, change a game station, write gameplay state, or identify raw station
names. Those verbs describe plans only.

### Radio station research

Phase 3D keeps RadioStationObservationRecorder and the versioned evidence model on a side path from RadioDecisionEngine. The reviewed map registry is scoped to its exact profile and is never used to infer an INI binding. Phase 3E adds a pure resolver with no gameplay consumer.

## Phase 3F identity bindings

A station may have exactly one optional gameplay binding:

```ini
[Station.Rise]
Enabled=true
GameStationIdentity=riseFm
```

`GameStationIdentity` accepts one normalized known identity other than `unknown`.
It cannot be combined with `GameStationRaw`. Enabled duplicates of either binding
kind are configuration errors; a disabled duplicate remains valid. Parsing stays
pure and does not query the registry.

Binding resolution is deterministic: `GameStationRaw > GameStationIdentity`. The
controller first finds an exact raw binding, and that binding remains authoritative
even if disabled. Only without a raw binding does it use the explicitly supplied
`ExecutableProfileId` and `RadioStationResolver`. Only a `resolved` identity may
select an identity binding. Unsupported profiles and unknown raws have no fallback.
Raw `10` remains unknown, unbound, and cannot select `policeRadio` or `radioOff`.

The resolver, controller, plans, and sink remain dry-run only. They do not construct
or call `StreamManager`, `PlaylistResolver`, WinHTTP, libVLC, an audio backend, or
a game-write executor. The original radio remains intact and independent playback
is pending.
