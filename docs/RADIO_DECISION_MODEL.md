# Radio decision model

Phase 3C converts an immutable `GameStateSnapshot` into a deterministic,
sanitized `RadioActionPlan`. It does not execute the plan.

```text
GameStateSnapshot
        |
        v
RadioDecisionEngine + ConfigurationData + SimulatedRadioState
        |
        v
RadioActionPlan
```

The engine is portable C++17. It has no dependency on Windows, plugin-sdk,
WinHTTP, libVLC, game files, GTA threads, `StreamManager`, or logging. Repeating
the same three inputs produces the same plan.

## Plans and simulated state

A plan contains only:

- one of `none`, `wouldStart`, `wouldPause`, `wouldResume`, `wouldSwitch`,
  `wouldSetVolume`, or `wouldStop`;
- an explicit decision reason;
- the source snapshot sequence;
- optional raw station, sanitized configuration key, and preference volume;
- whether simulated online audio would remain active.

It never contains a station URL, resolved URL, hostname, credential, token, local
path, pointer, or game address.

`SimulatedRadioState` is separate from real audio state. It records whether an
online station would be active or paused, its raw binding and sanitized key, its
last preference volume, and the last accepted snapshot sequence. It never queries
an audio backend.

## Conservative policy

The following conditions prevent a start and stop existing simulated activity:

| Condition | Reason |
| --- | --- |
| project disabled | `projectDisabled` |
| controller disabled or non-dry-run requested programmatically | `controllerDisabled` |
| observer unavailable | `observerUnavailable` |
| game not ready | `gameNotReady` |
| pause state unavailable | `pauseStateUnavailable` |
| vehicle state unavailable | `vehicleStateUnavailable` |
| player on foot | `playerOnFoot` |
| raw station unavailable | `stationUnavailable` |
| no explicit raw binding | `stationNotBound` |
| bound station disabled | `stationDisabled` |
| bound station URL empty | `stationUrlEmpty` |
| preference volume unavailable | `volumeUnavailable` |

An inactive state produces `none`; an active state produces `wouldStop`. The
original game radio is always the only real audio source.

For a valid bound station:

- inactive becomes `wouldStart`;
- active and newly paused becomes `wouldPause`;
- the same paused station after pause becomes `wouldResume`;
- a different binding becomes `wouldSwitch`;
- the same active station with a material preference change becomes
  `wouldSetVolume`;
- otherwise the plan is `none` with `unchanged`.

An older or duplicate nonzero snapshot sequence produces `none` with
`snapshotOutOfOrder` and cannot mutate simulated state.

## Preference volume

The snapshot field is a normalized menu preference, not effective mixer output
or audible volume. The engine calculates:

```text
clamp(snapshot preference * configured multiplier, 0.0, 1.0)
```

The explicit comparison tolerance is `0.01`. Changes at or below that tolerance
do not generate `wouldSetVolume`; this avoids plan noise from insignificant
floating-point differences.

## Raw bindings

`GameStationRaw` is the only link between a raw game value and a configured
station. Section names never imply raw values, and raw values never imply GTA III
station names. Enabled duplicate bindings are configuration errors. Disabled
duplicates are permitted because they cannot be selected.

Locally observed values remain `0..9` and `11`. Their official station names and
the radio-off value are not established by Phase 3C.
