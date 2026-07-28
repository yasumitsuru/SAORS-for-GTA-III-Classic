# Compatibility

## Compatibility matrix

| Target | Build support | Runtime status |
| --- | --- | --- |
| Windows x86, MSVC | Configured | Default, plugin-sdk compile, dry-run, and opt-in read-only observer modes locally passed |
| Windows x86, MinGW cross-build | Configured | Cross-build only; CI is authoritative |
| GTA III Classic local candidate | Exact identity plus guarded map | Locally reproduced read-only observer; edition/region remain unverified |
| GTA III 1.0 US | Research target | Unsupported; no registered fingerprint or address map |
| GTA III 1.1 | Future | Unsupported |
| Legacy Steam executable | Future | Unsupported |
| Patched executables | Future adapters | Unsupported; expected-byte mismatch disables the observer |
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
edition and region are unverified. A separate address profile is structurally
compatible with the pinned plugin-sdk `GAME_10EN` map and was reproduced against
that exact fingerprint. This does not identify the executable as GTA III 1.0 US.

Shared builds and configuration install no callback. The experimental observer
requires MSVC x86, explicit build and INI opt-ins, exact fingerprinting, the
minimum evidence level, image-range checks, exact expected-byte windows, and a
successful transaction. Failure leaves gameplay fields unavailable and preserves
the original radio. The reserved GTA III 1.0 US candidate enum remains unmapped.
