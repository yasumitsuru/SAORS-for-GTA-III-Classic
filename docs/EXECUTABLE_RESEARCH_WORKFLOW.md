# Executable research workflow

## Non-negotiable rules

- Work only with a legally obtained executable.
- Require the user to supply its path explicitly.
- Never search drives or Steam libraries automatically.
- Never upload the executable or send hashes to a third-party service.
- Never commit an executable, dump, disassembly, section bytes, or raw local
  report.
- Never infer a version from the filename or version resource alone.
- Keep hooks disabled throughout Phase 3A.

## Generate a local report

Build the Windows x86 probe, then pass the path explicitly:

```powershell
.\build\windows-msvc-x86-release\bin\Release\saors_exe_probe.exe `
  --exe "C:\Games\GTA3\gta3.exe" `
  --redact-path --compare-known
```

Print JSON to the console:

```powershell
.\build\windows-msvc-x86-release\bin\Release\saors_exe_probe.exe `
  --exe "C:\Games\GTA3\gta3.exe" `
  --json --redact-path --compare-known
```

Write a redacted JSON report to the ignored default:

```powershell
.\build\windows-msvc-x86-release\bin\Release\saors_exe_probe.exe `
  --exe "C:\Games\GTA3\gta3.exe" `
  --output --redact-path --compare-known
```

The default file is
`research/local/executable.exe-profile.local.json`. A custom path may follow
`--output`. The repository ignores `research/local/**` and
`*.exe-profile.local.json`, but ignore rules are a last defense rather than
permission to share raw research.

The probe opens the source only for reading and never copies it.

## Review the result

1. Confirm `PE32` and Intel 386.
2. Record the complete file SHA-256 and `.text` SHA-256 privately.
3. Review timestamp, entry-point RVA, image size, checksum, file size, and section
   layout.
4. Check whether known patches or loaders changed the file or `.text`.
5. Run the report twice and require identical results.
6. Classify a first reproduction as `candidate`.
7. Promote only to `locally_reproduced` after the same maintainer obtains an
   identical result again.
8. Keep the profile hook-disabled even when its hashes match locally.
9. Obtain independent reproduction before considering stronger status.

An exact fingerprint does not establish future offsets or calling conventions.
Those need separate evidence and memory-signature validation in a later phase.

## Publication boundary

After legal and privacy review, a minimal public interoperability record may
contain:

- complete and `.text` SHA-256;
- PE architecture and structural numeric fields;
- edition label qualified by its evidence status;
- date, tool version, and reproducibility method;
- sanitized test outcomes.

Never publish:

- executable or section bytes;
- executable, memory, or crash dumps;
- disassembly or decompiled proprietary functions;
- personal filesystem paths, Windows user names, volume IDs, or disk serials;
- raw reports containing those values;
- private stream URLs, credentials, or unrelated machine data.

## Adding a profile

Add a profile only after reviewing a redacted report and evidence record. A profile
must include:

- an explicit `ExecutableProfileId`;
- descriptive name;
- both exact SHA-256 values;
- all required PE metadata;
- verification level;
- evidence origin;
- verification date.

Tests must prove that changes to the full hash, `.text` hash, or any metadata field
fail exact matching. Do not mark `verified` without independent reproduction.

## Manual ASI smoke test

Only after the user explicitly authorizes a legal game path:

1. preserve the raw report locally;
2. generate a redacted report;
3. copy the trusted ASI artifact beside the game according to loader guidance;
4. start the game and inspect the sanitized log;
5. confirm the detected profile or safe `unsupported` result;
6. confirm no audio or network request starts from fingerprinting;
7. confirm `Hooks: disabled`;
8. confirm the game starts and closes normally;
9. confirm no residual process remains.

The Phase 3A.1 smoke test completed on 2026-07-27 after the user supplied an
explicit game directory. Two redacted reports were stable, the registered local
candidate matched exactly, the ASI loaded an existing save, and vehicle gameplay
continued without a crash. The log reported `Hooks: disabled`, `Gameplay reads:
disabled`, `Audio playback: not started`, and `Network activity: not started`;
the game process had no TCP connections during the observation. The original
radio UI remained available, but the automation had no audio channel and
therefore did not add audible evidence. The game closed without a residual
process, and the ASI, test INI, and generated log were removed because none
existed before the test.

The installation already contained unrelated third-party loaders or mods. This
result is therefore a narrow coexistence and safe-initialization observation, not
a clean-install baseline, edition claim, or hook compatibility result. Do not
disable antivirus protection permanently if it intervenes; record only the
behavior and use a trusted CI artifact.

## Phase 3B planning

plugin-sdk is not a Phase 3A dependency. If Phase 3B adopts its GTA III
`plugin_III` support:

- pin one exact reviewed commit rather than tracking `master`;
- review the license and packaging impact;
- audit every symbol, signature, offset, and address used;
- validate each operation only on the recognized executable;
- keep expected-byte checks and transactional rollback;
- retain the no-hook fallback for every unknown or modified executable.
