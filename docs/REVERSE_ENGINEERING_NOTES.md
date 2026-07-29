# Clean-room reverse-engineering notes

## Current verified map

The fingerprinting mechanism and one real GTA III Classic identity record are
implemented. Phase 3B separately registers the smallest runtime map needed for
read-only observation after local expected-byte and behavior reproduction.

| Executable adapter | Fingerprint | Evidence | Hook status |
| --- | --- | --- | --- |
| GTA III Classic local candidate | Exact profile registered | `locally_reproduced`; edition/region unverified; plugin-sdk `GAME_10EN` structural match only | Shared default disabled; experimental five-byte callback validated |
| GTA III 1.0 US | No exact profile | Research target; independent evidence pending | Disabled |

The map covers only the game-process callback, `FindPlayerVehicle`,
`FrontEndMenuManager` readiness/pause fields, `DMAudio.GetRadioInCar`, and the
music-preference integer. Addresses, calling conventions, layouts, and minimal
expected-byte windows are recorded in
[Pinned plugin-sdk audit](PLUGIN_SDK_AUDIT.md). The executable fingerprint and
address profile remain separate registries.

## Research rules

- Use a legally obtained copy of the game.
- Do not consult or copy proprietary SAORS source code.
- Do not upload executables, disassemblies, decompiled functions, or game assets.
- Record observations and independently reproduce them.
- Prefer stable signatures plus validation over bare absolute addresses.
- Treat community documentation as a lead, not proof.
- Have another contributor reproduce important results before enabling a hook.

## Evidence template

Use this template in a private research note before proposing minimal public facts:

```text
Researcher:
Date:
Legally obtained executable edition:
Local SHA-256 (do not attach executable):
Tool and version:
Observed behavior:
Candidate function/data:
Calling convention:
Expected bytes or invariants:
Independent reproduction:
Failure/rollback test:
Copyright and disclosure review:
```

## Adapter workflow

1. Add a new executable identifier without enabling hooks.
2. Add read-only detection tests using synthetic PE fixtures where possible.
3. Implement the adapter behind `GameIntegration`.
4. Verify every signature before reading or writing the target location.
5. Install all hooks transactionally; rollback on any mismatch.
6. Keep `SAORS_ENABLE_EXPERIMENTAL_GAME_OBSERVER=OFF` as the shared default.
7. Perform game smoke tests on a disposable backup before requesting review.

Never fill a missing address with an estimate or a value copied without provenance.

Phase 3B evaluated `plugin_III` only at the exact commit recorded in
`THIRD_PARTY_NOTICES.md`. The dependency remains optional, and every consumed
symbol is independently gated by the exact local fingerprint, minimum evidence
level, image bounds, and expected bytes. A failure keeps the observer unavailable
without trying another game-version map.

Phase 3C does not add or infer any game address or station-name mapping.
`GameStationRaw` is a user-supplied configuration binding for one observed raw
value. It is not evidence that the raw value has an official GTA III station name
or represents radio off. Independent reproduction is deferred to Phase 3D.

### Radio station research

Phase 3D keeps RadioStationObservationRecorder and the versioned evidence model on a side path from RadioDecisionEngine. The reviewed map registry is scoped to its exact profile and is never used to infer an INI binding. Phase 3E adds a pure resolver with no gameplay consumer.
