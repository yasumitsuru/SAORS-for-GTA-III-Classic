# Project status

Last milestone completed: Phase 3D — reproducible raw station mapping, merged into `main` at `cf1f12ce069679cc67813438c7eff77fb1a86066`.

Current phase: Phase 3E — pure raw radio-station resolution.

Base: `cf1f12ce069679cc67813438c7eff77fb1a86066`

## Phase 3D implementation status

- Recorder: implemented, pure, bounded, stable-frame filtered, disabled by default.
- Evidence schema: implemented as version 1 with privacy validation.
- Portable validator: implemented as `saors_radio_map_tool`.
- Registry: contains eleven reviewed `locallyReproduced` relationships only for `gta3_classic_local_candidate`; no controller consumption.
- Configuration: `[Research]` is opt-in and does not enable audio or network activity.
- Local validation: two manual sessions completed in separate processes against the same exact executable profile; the second used a different vehicle.
- Station map evidence: raws `0..9` and `11` matched normalized identities and conflict-free visible-label annotations in both sessions.
- Raw 10: unobserved, unknown, and unverified; it is absent from the registry.
- Independent reproduction: pending; no second source is claimed.

## Phase 3E scope

- `RadioStationResolver` is a portable, pure lookup from an `ExecutableProfileId` and raw value to an explicit resolution result.
- Only the exact reviewed profile can resolve raws `0..9` and `11`; raw `10` remains unknown and unverified.
- Invalid raws and unsupported profiles produce explicit results without fallback, inference, configuration reads, or gameplay consumption.
- `GameStationRaw` remains the manual controller binding. A possible Phase 3F may evaluate an identity binding in dry-run only after separate review.

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


Next gate: review the pure resolver diff and its validation. No online audio, SAORS network activity, playlist resolution, gameplay binding, merge, release, or tag is authorized.
