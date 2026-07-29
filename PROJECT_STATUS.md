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
- Portable validator: implemented as saors_radio_map_tool.
- Registry: implemented separately and empty by default; no controller consumption.
- Configuration: [Research] is opt-in and does not enable audio or network activity.
- Raw values observed by prior research: 0..9 and 11.
- Raw 10: not observed; identity remains unknown and no value is forced.
- HUD labels: no new local gameplay sessions were performed in this environment.
- Local reproduction: pending two complete sessions and sanitized report review; no GTA III session was run in this environment.
- Independent reproduction: pending; no second source is claimed.

## Safety and non-regression

The controller remains calculation-only and explicitly bound by GameStationRaw. The ASI does not construct a stream manager, playlist resolver, WinHTTP client, or audio backend for gameplay. No station map is inferred from plugin-sdk names. The original radio remains untouched. No game executable, dump, audio, URL, path, hostname, or raw local report is added by this phase.

## Validation status

- Linux/portable host equivalent: `build/phase3d-make-host-final` compiled with Clang 22 and warnings as errors; CMake target build passed.
- Host test suite: `ctest --test-dir build/phase3d-make-tests --output-on-failure` passed 129/129 tests.
- Portable tool smoke test: validate, summarize, and redact passed with a synthetic raw-3 and unknown raw-10 document.
- MSVC x86: CI passed build, plugin-sdk compile probe, and experimental observer/controller dry-run; local execution was not available in this environment.
- MinGW i686: CI cross-build passed; native Linux host tests passed; no local i686 MinGW compiler was available.
- Gameplay smoke test: not run; no GTA III executable or session was used.
- Synthetic unit tests cover recorder, evidence conflicts, JSON privacy, parser truncation/ranges, research configuration, and registry non-inference.

## Evidence status

- Raw values from prior research: 0..9 and 11.
- Raw 10: not observed; remains unknown and unverified.
- HUD-confirmed labels in this PR: none; no local GTA III sessions were performed.
- Local reproduction: not claimed; two complete sessions remain required.
- Independent reproduction: pending; no second legal installation or independent source was available.
- Plugin-sdk comparison: documentation-only corroboration; no automatic map entries are created.


Next gate: review the clean published diff before two authorized manual sessions. CI is green, but no gameplay session, online audio, SAORS network activity, playlist resolution, merge, release, or tag is authorized yet.
