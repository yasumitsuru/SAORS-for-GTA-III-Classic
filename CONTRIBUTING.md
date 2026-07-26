# Contributing

Thank you for helping build an independent online-radio plugin for GTA III Classic.

## Clean-room requirement

Contributions must be original work. Do not submit:

- proprietary or leaked SAORS source;
- GTA III executables, assets, headers copied from the game, or decompiled code;
- third-party binaries without an explicit redistribution review;
- guessed addresses or signatures without reproducible evidence;
- credentials, private stream URLs, usernames, or personal filesystem paths.

If prior exposure to proprietary implementation details could affect a contribution,
disclose that to maintainers before working on the same component.

## Development workflow

1. Open an issue for significant behavior or executable research.
2. Create a focused branch from `main`.
3. Configure with a shared CMake preset or an ignored `CMakeUserPresets.json`.
4. Add or update tests for host-independent behavior.
5. Run the relevant build and CTest preset.
6. Update status and compatibility documentation without overstating validation.
7. Open a pull request using the repository template.

Keep commits focused and use conventional prefixes such as `feat:`, `fix:`,
`test:`, `ci:`, `docs:`, and `chore:`.

## Code style

- C++17, RAII, and explicit ownership;
- four-space indentation and the repository `.clang-format`;
- no exceptions escaping DLL boundaries or initialization workers;
- no network or file work under the Windows loader lock;
- no direct game-memory access outside `GameIntegration`;
- errors must preserve original game behavior whenever possible.

## Tests

Host tests must not require GTA III, an ASI loader, internet access, Wine, or
proprietary files. Runtime tests must document the executable fingerprint and
environment but must not attach copyrighted files.

## Dependency changes

A dependency proposal must document license, exact version, Windows x86 support,
HTTP/HTTPS behavior where relevant, MP3/AAC coverage, Wine/Proton evidence,
transitive components, binary size, and redistribution obligations.

By contributing, you agree that your original contribution is licensed under the
repository's MIT License.
