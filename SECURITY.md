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

The current milestone performs no network requests and installs no game hooks.
Future networking must validate TLS and bound input sizes. Future executable
adapters must verify fingerprints and expected bytes before any memory operation,
then roll back completely on failure.

This policy is not a promise of compatibility or a warranty; see the MIT License.
