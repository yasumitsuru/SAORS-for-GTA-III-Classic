# Project status

Last milestone completed before this PR: Phase 3C — connect GameStateSnapshot to RadioController in dry-run.

Current phase: Phase 3D — reproducible raw station mapping.

Base: a23fd18b3ecc48fa4aa5a02f42ef8b222a67fff8
Branch: research/radio-station-map
Commit: b2faa0a feat: add reproducible radio station mapping infrastructure
Draft PR: #6 https://github.com/yasumitsuru/SAORS-for-GTA-III-Classic/pull/6
Publication: branch pushed to origin/research/radio-station-map; initial PR CI passed on Windows x86 and Linux/MinGW; no merge, release, or tag created.

## Phase 3D.1 implementation status

- Recorder: implemented, pure, bounded, stable-frame filtered, disabled by default.
- Evidence schema: implemented as version 1 with privacy validation.
- Portable validator: implemented as `saors_radio_map_tool`.
- Registry: contains eleven reviewed `locallyReproduced` relationships only for `gta3_classic_local_candidate`; no controller consumption.
- Configuration: `[Research]` is opt-in and does not enable audio or network activity.
- Local validation: two manual sessions completed in separate processes against the same exact executable profile; the second used a different vehicle.
- Station map evidence: raws `0..9` and `11` matched normalized identities and conflict-free visible-label annotations in both sessions.
- Raw 10: unobserved, unknown, and unverified; it is absent from the registry.
- Independent reproduction: pending; no second source is claimed.

## Safety and non-regression

The controller remains calculation-only and explicitly bound by GameStationRaw. The ASI does not construct a stream manager, playlist resolver, WinHTTP client, or audio backend for gameplay. No station map is inferred from plugin-sdk names. The original radio remains untouched. No game executable, dump, audio, URL, path, hostname, or raw local report is added by this phase.

## Validation status

- Linux/portable host equivalent: `build/phase3d-make-host-final` compiled with Clang 22 and warnings as errors; CMake target build passed before local-map promotion.
- Host test suite: `ctest --test-dir build/phase3d-make-tests --output-on-failure` passed 129/129 tests before local-map promotion.
- Portable tool smoke test: validate, summarize, and redact passed with a synthetic raw-3 and unknown raw-10 document.
- MSVC x86: clean Release x86 promotion build passed 130/130 tests with warnings as errors; fresh GitHub Actions Windows x86 CI passed `build`, `plugin-sdk compile probe`, and `experimental observer and controller dry-run` on the promotion commit.
- MinGW i686 and Linux host: fresh GitHub Actions `Build Linux MinGW Windows x86` workflow passed `cross-build` and `test-host` on the promotion commit.
- Gameplay validation: two controlled manual sessions completed with the observer and recorder only; no gameplay audio, network, playlist, or controller execution was enabled.
- Synthetic unit tests cover recorder, evidence conflicts, JSON privacy, parser truncation/ranges, research configuration, and exact-profile registry behavior.

## Evidence status

- Executable verification: `locally_reproduced` remains an exact-profile property and is not an edition or region claim.
- Station map: raws `0..9` and `11` are `locallyReproduced` from two separate, conflict-free local sessions.
- Raw 10: not observed; remains unknown and unverified.
- Visible labels were reviewed locally only; raw reports and annotations remain ignored under `research/local/`.
- Independent reproduction: not claimed; a second legal installation or independent source remains required.
- Plugin-sdk comparison: documentation-only corroboration; no automatic map entries are created.


Next gate: final diff review and pull-request review. No online audio, SAORS network activity, playlist resolution, merge, release, or tag is authorized.
