# Building on Windows

## Requirements

- Windows 10 or Windows 11;
- Git;
- CMake 3.24 or newer;
- Visual Studio 2022 or Build Tools 2022;
- the Desktop development with C++ workload;
- MSVC x86/x64 build tools and a Windows SDK;
- Ninja only if using a custom local preset.

Run the read-only environment check from PowerShell:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\tools\setup_windows.ps1
```

The script reports missing software and never installs it.

## Debug

```powershell
cmake --preset windows-msvc-x86-debug
cmake --build --preset windows-msvc-x86-debug
ctest --preset windows-msvc-x86-debug
```

The multi-configuration Visual Studio build writes the plugin under:

```text
build/windows-msvc-x86-debug/bin/Debug/SAORSForGTA3.asi
```

## Release

```powershell
cmake --preset windows-msvc-x86-release
cmake --build --preset windows-msvc-x86-release
ctest --preset windows-msvc-x86-release
```

The release artifact is:

```text
build/windows-msvc-x86-release/bin/Release/SAORSForGTA3.asi
```

The preset selects the Visual Studio `Win32` platform. CMake also rejects an ASI
build when the target pointer size is not 32 bits.

## CMake options

| Option | Default | Meaning |
| --- | --- | --- |
| `SAORS_BUILD_TESTS` | `ON` | Build Catch2 host tests |
| `SAORS_ENABLE_WARNINGS_AS_ERRORS` | `OFF` | Promote warnings to errors |
| `SAORS_BUILD_ASI` | `ON` for Windows targets | Build `SAORSForGTA3.asi` |
| `SAORS_ENABLE_EXPERIMENTAL_HOOKS` | `OFF` | Compile guarded experimental code |

Enabling the last option currently installs no hooks because there is no verified
address map. It exists to make later experimental work an explicit build choice.

## Runtime library

MSVC builds use the matching static multithreaded runtime (`/MT` or `/MTd`) across
project targets. Third-party backends added later must use a compatible allocation
boundary or provide explicit create/destroy functions.

## Visual Studio and VS Code

Visual Studio 2022 can open the repository folder and consume
`CMakePresets.json`. VS Code can use the CMake Tools extension. Local paths and
machine-specific settings belong in ignored `CMakeUserPresets.json`, never in the
shared presets.
