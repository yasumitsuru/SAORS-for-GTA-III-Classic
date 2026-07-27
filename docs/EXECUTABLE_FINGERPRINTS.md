# Executable fingerprints

## Purpose and safety boundary

Phase 3A identifies a Windows PE executable before any future game adapter can
consider internal data. Identification does not install a hook, read gameplay
state, or prove that a future address map is correct. The filename `gta3.exe`,
file size, timestamp, and Windows version resources are never sufficient by
themselves.

The ASI obtains only the host executable path supplied by Windows. It performs the
inspection on its initialization worker, not in `DllMain`, and does not search any
drive. An invalid, modified, unregistered, or unreadable executable remains
unsupported and receives no memory access.

## Fingerprint model

An `ExecutableFingerprint` contains:

- SHA-256 of the complete file;
- SHA-256 of the `.text` section;
- PE machine and optional-header magic;
- COFF timestamp;
- entry-point RVA;
- image size and PE checksum;
- file size;
- section names, virtual RVAs/sizes, raw offsets/sizes, and SHA-256 values.

These are file-layout facts. No loaded pointer or process virtual address is
persisted. A fingerprint is also different from a future memory signature:

- a fingerprint identifies exact file bytes and structural metadata;
- a memory signature would validate the expected bytes around one future access
  site after the recognized image is loaded.

Both controls will be required before hooks can be considered. Phase 3A implements
only the first.

## Defensive PE reader

`PeImageReader` reads the DOS header, PE/COFF header, bounded PE32 optional header,
and section table without loading or executing the inspected file. It rejects:

- missing DOS or PE signatures;
- `e_lfanew` below the DOS header, above 1 MiB, or outside the file;
- non-Intel-386 machines and PE32+ images;
- zero or more than 96 sections;
- truncated or inconsistent optional headers and section tables;
- zero alignments, invalid image bounds, and invalid `SizeOfHeaders`;
- raw or virtual range overflow, out-of-file data, and overlapping regions;
- unsafe section names;
- a missing, duplicate, or empty `.text` section;
- files larger than the defensive PE32 limit of `UINT32_MAX` bytes.

The parser uses explicit little-endian reads and checked arithmetic. It never calls
`LoadLibrary`.

## SHA-256

Windows builds use the operating system CNG API through BCrypt. Files are opened
read-only with Unicode paths and hashed incrementally in 64 KiB blocks. Full-file
and section-range hashes use RAII wrappers for file, algorithm, and hash handles.
Provider work buffers and digest buffers are cleared before release. The project
does not implement its own cryptographic algorithm.

`FileHasher` keeps parsing and matching portable. Linux host tests inject a
deterministic implementation and never require a Windows crypto API.

## Exact and partial matching

A usable profile must contain both lowercase 64-character hashes, all expected PE
metadata, an evidence origin, a verification date, and a non-unsupported ID.

- `exact fingerprint match`: full SHA-256, `.text` SHA-256, and every registered
  metadata field match.
- `candidate structural match`: metadata matches but one or both hashes differ.
- `no exact profile match`: metadata differs, the registry is empty, or no usable
  profile matches.

Only an exact match can select a named profile. Partial matching is diagnostic and
never enables support. A changed full-file hash can indicate an overlay or resource
change; a changed `.text` hash indicates changed code. Both remain unsupported.

## Verification levels and current registry

Profiles support four evidence levels:

| Level | Meaning |
| --- | --- |
| `candidate` | One explicitly supplied legal copy produced a plausible report |
| `locally_reproduced` | The same maintainer reproduced the result |
| `independently_reproduced` | Another contributor reproduced it independently |
| `verified` | Evidence and compatibility review accepted it for the stated scope |

The enum reserves `gta3_10_us_candidate`, but the built-in registry intentionally
contains **zero profiles**. No legal GTA III executable path or reproducible
fingerprint was supplied for this phase, so nothing is marked `candidate` or
`verified`.

## ASI behavior and logging

The initialization worker fingerprints the host and compares it with the registry.
Normal logs contain only architecture, profile name, match booleans, match status,
and `Hooks: disabled`. Full hashes and executable paths are not written to the
normal ASI log.

Configuration, the audio backend, and the playlist resolver may be constructed
later in the worker, but fingerprinting itself starts no network request or audio.
`installHooks()` still returns `false`; all gameplay query methods retain their
safe placeholder behavior.

## Controlled validation

Synthetic PE fixtures cover valid PE32 x86 input, truncation, PE32+, wrong machine,
missing `.text`, overflow, range overlap, read failure, Unicode paths, hashing
failure, exact and partial matching, an empty registry, and report redaction.
Windows tests verify the standard SHA-256 digest for `abc` and process a 1 MiB file
through the incremental BCrypt path.

The standalone probe was also run against its own project-built PE32 executable
with `--json --redact-path --compare-known`. It exited successfully, emitted no
local path, and reported profile `none`. This is not a GTA III compatibility test.

Real-game validation remains pending until the user explicitly supplies a path to
a legally obtained `gta3.exe`.
