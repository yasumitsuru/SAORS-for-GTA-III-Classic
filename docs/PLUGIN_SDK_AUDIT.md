# Pinned plugin-sdk audit

## Dependency decision

The optional compile reference is `DK22Pac/plugin-sdk` component `plugin_III` at
commit:

```text
5da18b6f1956bb20bdfa39dcb07c44863ce26c81
```

The revision exists and its `LICENSE` contains zlib-style terms. The exact
revision and notice are recorded in `THIRD_PARTY_NOTICES.md`. Normal builds do not
fetch or require it. An external checkout must be a Git checkout at the exact
commit; the optional `FetchContent` path also uses that exact commit.

The SDK is a research and compile-time reference. No SDK source, game executable,
dump, disassembly, or proprietary asset is committed or distributed by this
project.

## Profile and variant

The SDK labels the candidate addresses below as `GAME_10EN`. Independent local
validation found that the exact `gta3_classic_local_candidate` image has the
corresponding small code/reference windows and runtime behavior. Therefore the
address profile records:

```text
plugin-sdk GAME_10EN address set; local structural match only
```

This is not evidence that the executable is the GTA III 1.0 US retail edition.
Its edition and region remain unknown. No map is registered for the reserved
`gta3_10_us_candidate` profile, GTA III 1.1, or Steam.

## Audited symbols

| Symbol | SDK candidate / ABI | Local evidence and use |
| --- | --- | --- |
| `plugin::Events::gameProcessEvent` | `0x48E49B`, cdecl `void()` callsite, after original | Exact five-byte `CALL` matched; only location that may be replaced |
| `FindPlayerVehicle()` | `0x4A10C0`, cdecl, returns a vehicle pointer | Exact 17-byte entry window matched; called only when the game-ready flag is true; returned pointer is not dereferenced |
| `FrontEndMenuManager` | instance `0x8F59D8`; `CMenuManager` size `0x564` | Two independent 14-byte reference windows matched |
| `CMenuManager::m_bMenuActive` | one-byte `bool`, offset `0x111` | Compile-time offset probe and real pause active/inactive transitions |
| `CMenuManager::m_bGameNotLoaded` | one-byte `bool`, offset `0x116` | Compile-time offset probe and real menu-to-loaded transition |
| `CMenuManager::m_nPrefsMusicVolume` | 32-bit `int` at `0x5F2E4C` | Exact 14-byte reference window matched; local slider range `0..127`; read only |
| `DMAudio` | `cDMAudio` instance at `0x95CDBE` | Used only as the audited `this` pointer after all code windows match |
| `cDMAudio::GetRadioInCar()` | `0x57CE40`, thiscall, returns `unsigned char` | Exact 12-byte entry window matched; real raw changes stayed in `0..11` |

The project-owned compile probe includes the four relevant SDK headers and checks
Windows x86 plus the two menu offsets. It is isolated as a C++23 object target
because this pinned upstream revision uses current standard-library facilities;
the SAORS core remains C++17 and does not link the probe or SDK.

## Expected-byte policy

Only small exact windows needed to distinguish the audited locations are kept:

- callback: `E8 B0 E3 FF FF`;
- vehicle lookup: 17 bytes at the function entry;
- frontend state: two 14-byte instruction/reference windows;
- radio query: 12 bytes at the function entry;
- music preference: one 14-byte instruction/reference window.

All masks are exact (`FF`) for this profile. The dry-run and installed smoke both
matched every window. A mismatch leaves the observer unavailable and performs no
fallback search, wildcard expansion, memory write, gameplay call, network work,
or audio work.

## Confidence and remaining limits

Confidence is `locally_reproduced` for this exact executable fingerprint and this
minimal read-only observer. It is not independently reproduced on a second legal
game installation and does not establish edition/region identity.

The observed raw radio sequence does not yet justify a public enum-to-station-name
mapping. The volume field is the menu preference, not effective mixer output.
Manual ASI unload safety is also not claimed; the observer remains resident until
normal process termination.
