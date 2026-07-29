# Project status

Last milestone completed: Phase 3E — pure radio-station resolution, merged into `main` at `35a8a13a3ef6ae5173e64307e9fde5ee51c64dfd`.

Current phase: Phase 3F — dry-run radio identity binding.

Base: `35a8a13a3ef6ae5173e64307e9fde5ee51c64dfd`

## Phase 3F scope

- `StationConfiguration` supports either `GameStationRaw` or normalized `GameStationIdentity`; one station cannot configure both.
- Parsing rejects unknown identities, `unknown`, and enabled duplicate raw or identity bindings without consulting the registry.
- `GameStationRaw > GameStationIdentity`: an exact raw binding is authoritative, including a disabled binding.
- Without a raw binding, `RadioDecisionEngine` accepts an explicit `ExecutableProfileId` and `RadioStationResolver`; only `resolved` may select an identity binding.
- Invalid raws, unsupported profiles, unknown raws, and unbound identities return distinct dry-run reasons; no profile fallback exists.
- Raw `10` remains unknown and cannot select `policeRadio` or `radioOff`.

## Safety and non-regression

The controller remains calculation-only. `RadioActionPlan`, `SimulatedRadioState`, and `DryRunRadioActionSink` record only simulated `would-*` transitions. The ASI does not construct a stream manager, playlist resolver, WinHTTP client, libVLC runtime, audio backend, or gameplay executor. It does not start audio or networking, write game memory, change the game station or volume, alter the HUD or controls, suppress the original radio, or claim an executable edition or region. Independent playback remains pending.

## Validation status

- WSL/Clang host build with warnings as errors passed 138/138 tests.
- MSVC Win32 Release with warnings as errors passed 147/147 tests, including the plugin-sdk compile probe.
- Fresh GitHub Actions Build Linux MinGW Windows x86 passed cross-build and test-host.
- Fresh GitHub Actions Build Windows x86 passed build, plugin-sdk compile probe, and experimental observer and controller dry-run.
- SAORSForGTA3.asi was verified as PE32 x86 (14C machine) with no direct winhttp.dll or libvlc.dll imports.
- Phase 3E published the pure resolver for the exact reviewed profile only; raw `10` remains absent from the registry.

## Evidence status

- The registry contains only reviewed locally reproduced relationships for the exact `gta3_classic_local_candidate` profile.
- No profile fallback or automatic map expansion is authorized.
- Independent reproduction and any real gameplay audio remain out of scope.

Next gate: final review and an explicitly authorized squash merge. No real executor, gameplay audio, networking, playlist activity, game writes, release, or tag is authorized.
