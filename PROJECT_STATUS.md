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

- Phase 3F validation is pending the portable and MSVC x86 builds for this branch.
- Phase 3E published the pure resolver for the exact reviewed profile only; raw `10` remains absent from the registry.

## Evidence status

- The registry contains only reviewed locally reproduced relationships for the exact `gta3_classic_local_candidate` profile.
- No profile fallback or automatic map expansion is authorized.
- Independent reproduction and any real gameplay audio remain out of scope.