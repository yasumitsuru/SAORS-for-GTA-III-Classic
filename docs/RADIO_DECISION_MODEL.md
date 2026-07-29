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

## Phase 3F station bindings

A configured station can bind one of two ways: `GameStationRaw=0..255` or
`GameStationIdentity=<normalized identity>`. The two keys are mutually exclusive.
Enabled duplicates of a raw or identity are configuration errors; disabled
duplicates are allowed. Parsing never queries the station map registry.

`GameStationRaw > GameStationIdentity`. The engine checks exact raw bindings first.
A matching raw binding is authoritative even when disabled. Only with no raw binding
does the explicitly supplied `ExecutableProfileId` and `RadioStationResolver` run.
Only `resolved` can select an identity binding; there is no profile fallback.

The identity-related conservative reasons are `stationRawInvalid`,
`stationProfileUnsupported`, `stationIdentityUnknown`, and
`stationIdentityNotBound`. Raw `10` remains `stationIdentityUnknown` with no
identity, and cannot activate `policeRadio` or `radioOff`.

Plans and simulated state record binding kind and optional normalized identity in
addition to the raw value and sanitized key. This remains calculation-only: no
plan executes audio, networking, playlist resolution, game writes, station changes,
volume changes, or original-radio suppression.
