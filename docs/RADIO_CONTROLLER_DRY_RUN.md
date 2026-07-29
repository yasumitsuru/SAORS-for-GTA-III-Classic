# Radio controller dry-run

Phase 3C connects the read-only observer to a calculation-only radio controller.
No plan has a gameplay executor.

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
```

The ASI runtime path additionally requires the build option
`SAORS_ENABLE_RADIO_CONTROLLER_DRY_RUN=ON`. It is `OFF` by default. Setting
`RadioControllerDryRun=false` in an INI produces a warning and remains dry-run;
there is no real sink to select.

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

## ASI isolation

The ASI gameplay entry point does not construct:

- `AudioBackend` or `AudioBackendFactory`;
- `WinHttpClient`;
- `PlaylistResolver`;
- `StreamManager`;
- a libVLC gameplay runtime.

Those components remain available to the standalone stream probe and their unit
tests. The Phase 3C ASI import audit must not contain `winhttp.dll` or
`libvlc.dll`.

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
