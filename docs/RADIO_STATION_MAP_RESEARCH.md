# Radio station map research

Phase 3D adds a reproducible research path:

    GameStateSnapshot -> RadioStationObservationRecorder -> ignored local JSON
    -> saors_radio_map_tool -> reviewed RadioStationMapRegistry

The recorder is pure and testable. It ignores unavailable/not-ready snapshots, requires a player in a vehicle and a raw value in 0..255, rejects out-of-order sequences, filters transition noise with a configurable stable-frame threshold, records each raw transition once per session, and enforces an observation limit. It has no logger, network, URL, audio, controller, or stream-manager dependency. The threshold is a filter rather than an FPS-based identity rule; the default is 15 frames and the accepted range is 1..600.

## Configuration

```ini
[Research]
EnableRadioStationMapRecorder=false
RadioStationMinimumStableFrames=15
LogRadioStationObservations=false
```

The section is disabled by default. Enabling it does not enable the observer, controller, audio, networking, playlist resolution, or station bindings. Runtime activation is available only in the experimental observer build. Sanitized log lines contain only ordinal, raw, and stable-frame count.

## Evidence workflow

Run two complete, separate local sessions with the same exact profile. In each session, walk the natural radio cycle manually, wait for the raw value to stabilize, and copy only the visible HUD name into the ignored JSON report afterwards. Do not force raw 10, write memory, call SetRadioInCar, identify music, automate input after manual control, or share the executable/dumps. Compare reports and promote only conflict-free relationships to locallyReproduced. A second legal installation or independent collaborator may promote matching entries to independentlyReproduced; otherwise that level remains pending.

## Portable tool

`saors_radio_map_tool` needs no GTA III, plugin-sdk, libVLC, WinHTTP, network, or real executable. It supports `--validate`, `--compare`, `--summarize`, `--redact`, and `--help`.

## Reviewed local map

Two manually operated sessions in separate processes, using the same exact
`gta3_classic_local_candidate` profile and conflict-free reports, reproduced the
relationships for raws `0..9` and `11`. The built-in registry contains only those
eleven normalized identities with `locallyReproduced` evidence, scoped only to
that exact profile. The second session used a different vehicle. Raw 10 was not
observed and remains unknown, unverified, and absent from the registry.

The registry is diagnosis-only: it never populates `GameStationRaw`, changes
`RadioDecisionEngine`, creates a controller binding, reads local reports, or
provides a fallback map for another executable profile. Raw report files remain
ignored and no visible-label annotation, executable path, log, hash, or other
local evidence is published. The SDK list remains corroborative documentation,
not runtime truth. Independent reproduction remains pending.

## Pure resolution

Phase 3E introduces `RadioStationResolver`, which accepts only an
`ExecutableProfileId` and a raw value. It returns `resolved`, `unknownRaw`,
`unsupportedProfile`, or `invalidRaw` without reading configuration or touching
gameplay. The resolver uses the reviewed registry only for the exact
`gta3_classic_local_candidate` profile: raws `0..9` and `11` resolve, while raw
`10` and all other unmapped valid raws remain unknown. Unsupported profiles have
no fallback. The resolver is not consumed by `RadioDecisionEngine`,
`RadioController`, the observer, or INI binding; `GameStationRaw` remains manual
pending a separately reviewed dry-run binding phase.
