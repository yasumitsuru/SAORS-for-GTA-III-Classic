# Clean-room reverse-engineering notes

## Current verified map

The fingerprinting mechanism is implemented, but no real GTA III fingerprint,
address, signature, offset, or hook is verified in this repository.

| Executable adapter | Fingerprint | Evidence | Hook status |
| --- | --- | --- | --- |
| GTA III 1.0 US | Registry empty | Explicit legal path and reproduction pending | Disabled |

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
6. Keep `SAORS_ENABLE_EXPERIMENTAL_HOOKS=OFF` as the shared default.
7. Perform game smoke tests on a disposable backup before requesting review.

Never fill a missing address with an estimate or a value copied without provenance.

Phase 3B may evaluate pinned `plugin_III` support from plugin-sdk only after an
exact commit and license review. Every consumed symbol and address must be audited,
validated on an exactly recognized executable, and protected by a safe fallback.
