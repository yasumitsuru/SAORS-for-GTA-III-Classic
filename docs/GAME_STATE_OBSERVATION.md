# Read-only game-state observation

Phase 3B adds an experimental, opt-in observer for the exact
`gta3_classic_local_candidate` fingerprint. The executable's edition and region
remain unverified. The observer does not classify it as GTA III 1.0 US, 1.1, or
Steam.

## Safety gates

The callback is available only in an MSVC Windows x86 build configured with:

```text
SAORS_ENABLE_PLUGIN_SDK=ON
SAORS_ENABLE_EXPERIMENTAL_GAME_OBSERVER=ON
```

Runtime installation additionally requires all of the following:

1. `EnableGameObserver=true`;
2. an exact full-file, `.text`, and PE-metadata fingerprint match;
3. verification level `locally_reproduced` or stronger;
4. a separate `GameAddressProfile` for that executable profile;
5. every required address inside the main image;
6. every small expected-byte window matching;
7. successful transactional installation and post-write verification.

Shared configuration remains safe:

```ini
[Experimental]
EnableGameObserver=false
ObserverDryRun=true
LogStateTransitions=true
```

With those defaults no callback is installed. A build without the experimental
feature also has no Windows process-memory implementation.

## Dry-run

Dry-run performs the fingerprint, profile, image-range, expected-byte, and patch
definition checks without writing a byte. It reports compatibility, the failed
symbol when known, and `Hook writes performed: false`. It does not try another
address or weaken a signature after a mismatch.

The local dry-run passed for `gta3_classic_local_candidate`: the exact fingerprint
and every expected-byte window matched, the save loaded, the original radio
remained available, zero hook writes occurred, no playlist or stream started,
and the process exited without a residual game process.

## Snapshot contract

`GameStateSnapshot` distinguishes a real `false` value from an unavailable field:

| Field | Meaning |
| --- | --- |
| `observerAvailable` | the state source has the audited runtime access it needs |
| `gameReady` | the audited frontend flag says gameplay is loaded |
| `pauseMenuActive` | audited frontend menu flag, or unavailable |
| `playerInVehicle` | null vehicle, plausible vehicle pointer, or unavailable |
| `radioStationRaw` | raw value in the locally observed `0..11` range |
| `radioVolume` | normalized music-preference value, or unavailable |
| `sequence` | monotonically increasing capture sequence |

Each read is contained separately, so a failed vehicle call does not erase a
valid pause or volume result. A short mutex publishes and returns a consistent
copy. No exception can escape `capture()` or the callback.

`radioStationRaw` is deliberately not mapped to station names. The local smoke
observed values `0` through `9` and `11`, including vehicle transitions and radio
cycling, but did not establish a public name-to-value mapping for every context.

The music preference is a 32-bit integer locally observed over the menu slider's
`0..127` range. `radioVolume` normalizes that preference to `0.0..1.0`; it is not
proof of the mixer level, current audible output, pause attenuation, or any
online-stream volume. The observer never writes the preference.

## Callback and transaction

The only code change in the game image is a five-byte `CALL` replacement at the
audited game-process event callsite. The replacement calls the original function
first and then captures the snapshot on the game thread.

Before writing, `PatchTransaction` validates every patch and saves every original
byte. The Windows implementation temporarily changes protection only around the
write, flushes the instruction cache, restores the original protection, and reads
the replacement back. Any write or verification failure restores applied patches
in reverse order. Install and removal are idempotent.

Unit tests use synthetic memory for correct and incorrect signatures, masks,
range overflow, dry-run, duplicate install/remove, first-write failure,
post-write failure, partial installation, verification failure, and rollback.
They never load a game executable.

Manual ASI unload is not supported. The integration object remains allocated for
the process lifetime so callback-owned code is not torn down under the loader
lock. Normal process exit reclaims it. An explicit idempotent `stop()` and byte
restoration path exist for controlled use and tests, but no risky detach teardown
is run from `DllMain`.

## Runtime observations

The opt-in local smoke observed:

- initial unavailable gameplay state followed by `gameReady=true` after loading;
- pause menu active and inactive;
- on-foot, in-vehicle, on-foot, and in-vehicle transitions;
- raw radio values changing while stations were cycled;
- the music-preference field remaining readable;
- the original radio continuing to operate;
- no online audio, playlist retrieval, or SAORS network connection;
- clean game exit with no residual process.

Logs contain transitions only, have a 1 MiB cap, and omit pointers, addresses,
bytes, fingerprints, personal paths, URLs, and per-frame snapshots.

## Non-goals

This phase does not connect the snapshot to playback decisions. It does not mute
the original radio, change stations or volume, alter controls or HUD, access
vehicle internals, resolve playlists during gameplay, or start network/audio
work. `RadioController` can accept a snapshot for tests, but the ASI does not
drive it from gameplay in Phase 3B.
