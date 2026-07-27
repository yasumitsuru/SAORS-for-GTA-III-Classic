# Security policy

## Supported versions

The project has no stable release yet. Security fixes are applied to the current
`main` branch.

## Reporting a vulnerability

Do not open a public issue for a vulnerability that could compromise users, expose
credentials, enable unsafe executable matching, or corrupt the game process.

Use GitHub's private vulnerability reporting or a private maintainer contact method
when one is enabled for the repository. Include:

- affected commit;
- environment and architecture;
- minimal reproduction without GTA III files or private URLs;
- impact;
- suggested mitigation if known.

Do not attach game executables, memory dumps containing copyrighted or private
content, tokens, or third-party binaries.

## Security posture

The current milestone can perform bounded remote-playlist requests through
WinHTTP, with normal TLS validation and explicit HTTP-media policy. Fingerprinting
itself performs no network request and never uploads an executable.

The project installs no game hooks. Phase 3A parses bounded PE32 x86 metadata and
uses Windows CNG SHA-256, but the built-in executable registry remains empty.
Unknown, modified, partially matching, or unreadable executables receive no
gameplay read or memory write. Future adapters must verify exact fingerprints and
expected in-memory bytes before any operation, then roll back completely on
failure.

This policy is not a promise of compatibility or a warranty; see the MIT License.
