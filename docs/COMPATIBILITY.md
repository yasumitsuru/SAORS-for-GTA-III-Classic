# Compatibility

## Compatibility matrix

| Target | Build support | Runtime status |
| --- | --- | --- |
| Windows x86, MSVC | Configured | Release Win32 no-hook ASI smoke passed on the local candidate |
| Windows x86, MinGW cross-build | Configured | Cross-build only; CI is authoritative |
| GTA III Classic local candidate | Identity only | Exact, locally reproduced; edition/region and address map unverified |
| GTA III 1.0 US | Research target | Unsupported; no registered fingerprint or address map |
| GTA III 1.1 | Future | Unsupported |
| Legacy Steam executable | Future | Unsupported |
| Patched executables | Future adapters | Unsupported |
| Proton/Wine | Windows artifact documented | Not validated |

“Configured” means build files exist; it is not proof that every local toolchain or
runtime environment has passed. CI results are the authoritative build evidence.

## Executable detection policy

Filename, file size, timestamp, and Windows version resources are not strong
enough to enable hooks. Phase 3A now requires exact full-file SHA-256, `.text`
SHA-256, and PE metadata equality merely to identify a profile. A supported
adapter will additionally require:

1. a cryptographic fingerprint for a legally obtained, unmodified executable;
2. independent confirmation of relevant code/data locations;
3. documented calling conventions and expected bytes;
4. tests for signature mismatch and partial installation;
5. rollback behavior for every modified location;
6. opt-in experimental testing before default enablement.

The repository must never contain the executable or extracted proprietary code.
Public documentation may contain the minimum non-copyrightable facts required for
interoperability after legal review.

## Failure behavior

Unknown or modified executables must:

- be logged as unsupported;
- receive no memory read, write, detour, or patch;
- retain the original radio behavior;
- allow the game process to continue.

The built-in registry contains one exact identity profile,
`gta3_classic_local_candidate`, at the `locally_reproduced` evidence level. Its
edition and region are unverified. The associated adapter is deliberately
unmapped, installs no hooks, and exposes no gameplay state. The reserved GTA III
1.0 US candidate enum remains a research target rather than a compatibility
claim.
