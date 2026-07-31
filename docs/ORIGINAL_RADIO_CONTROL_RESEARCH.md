# Original radio control research

Status: **blocked**

Date: 2026-07-31

This document records a read-only audit performed from the repository state at
`origin/main` commit `7655b5c954c04a970a937c2d1aa48e3fa45560b7`. GTA III was not
executed and no game memory or executable code was modified.

## Scope and exact profile

The repository registers one executable identity:

```text
gta3_classic_local_candidate
file SHA-256: ebb8cd22b88bd84b9a223aee02e67e3dc0b4acbc17d7155951e7cc02f524a343
text SHA-256: 695fe240ba96fc010b3363e64319d7327ca7f171ffa6eba50454ee37d6bbe79b
status: locally_reproduced
evidence: two identical local probe runs
```

The runtime address profile is explicitly described as a `plugin-sdk GAME_10EN`
address set with a local structural match only. It does not establish that the
executable is GTA III 1.0 US, and no audio-control map is registered for this
profile. The pinned SDK reference is `DK22Pac/plugin-sdk` commit
`5da18b6f1956bb20bdfa39dcb07c44863ce26c81`.

Existing expected-byte validation covers the observer callback, vehicle lookup,
frontend state references, `GetRadioInCar`, and the menu music preference. It
does not cover any candidate audio-control entry point or audio-manager global.

## Evidence reviewed

- `docs/PLUGIN_SDK_AUDIT.md` records the profile limits, the read-only
  `CMenuManager::m_nPrefsMusicVolume` observation, and the prior rejection of
  the SDK music-volume methods as a production mechanism.
- `src/GameAddressProfile.cpp` records the exact-profile address map and its
  all-byte expected windows. The map contains `GetRadioInCar` and the menu
  preference, but no `cDMAudio` setter or `cMusicManager` control target.
- `build/gameplay-mvp-msvc-x86/_deps/saors_plugin_sdk-src/plugin_III/game_III/`
  was inspected at the pinned SDK checkout. The checkout is a build dependency,
  not a committed or distributed game dump.
- `docs/REVERSE_ENGINEERING_NOTES.md` and `docs/ARCHITECTURE.md` were checked
  for execution timing, loader-lock constraints, and existing reverse-engineering
  evidence.
- `src/OriginalRadioController.cpp` and
  `include/saors_gta3/OriginalRadioController.hpp` were left unchanged. The
  production factory still returns `NullOriginalRadioController`.

## Candidates evaluated

### 1. `cDMAudio::SetMusicFadeVol(unsigned char)`

**Evidence found:** The pinned SDK declares the method and associates it with
the `GAME_10EN` addresses `0x57C920`, `0x57CC70`, and `0x57CB70` for its three
supported variants. It is a callable game function, so it would avoid a direct
variable write if its semantics were proven.

**Risks and rejection:** The name describes music fade volume, not vehicle radio
volume. No evidence shows that it affects only the original in-car radio rather
than frontend, cutscene, streamed music, or other music paths. There is no
getter or reviewed state contract for the effective value, no exact-profile entry
bytes, and no proof that the operation is temporary and nonpersistent. Calling
it with zero and later restoring a guessed value would violate exact restoration.

**Criteria:** Fails 1, 3, 4, 5, 6, 7, and 9.

### 2. `cDMAudio::SetMusicMasterVolume(unsigned char)`

**Evidence found:** The pinned SDK declares the method and associates it with
the `GAME_10EN` addresses `0x57C8C0`, `0x57CC10`, and `0x57CB10`.

**Risks and rejection:** This is explicitly a music-master control, so it is
broader than the requested vehicle-radio-only effect. The repository has no
exact-profile expected-byte window for the method, no effective-volume getter,
and no proof that the call does not interact with the user's persistent menu
preference. It cannot currently guarantee capture and exact restoration.

**Criteria:** Fails 1, 3, 4, 5, 6, 7, and 9.

### 3. `cDMAudio::ChangeMusicMode` / `cMusicManager::ChangeMusicMode`

**Evidence found:** Both APIs are present in the pinned SDK. The
`cMusicManager` layout exposes `m_nMusicMode`, `m_nCurrentStreamedSound`,
`m_nPreviousStreamedSound`, and the radio-related state fields. The SDK also
exposes the `bRadioOff` global.

**Risks and rejection:** The available declarations do not establish which
music modes are radio-only, nor whether changing mode stops or changes streamed
music, frontend music, announcements, or cutscene audio. Directly changing
`m_nMusicMode` or `bRadioOff` would be an unvalidated game-memory write and
would have no exact-profile expected bytes or proven restoration transaction.
No safe read-preserve-write-readback contract exists.

**Criteria:** Fails 1, 3, 4, 5, 6, 7, and 9.

### 4. `cDMAudio::SetRadioInCar` / `SetRadioChannel`

**Evidence found:** The pinned SDK declares both methods. Their `GAME_10EN`
addresses are `SetRadioInCar=0x57CE60` and `SetRadioChannel=0x57CE80`.
The existing observer reads the current raw station through `GetRadioInCar`.

**Risks and rejection:** These APIs select or reposition a station; they are not
silencing operations. Passing a radio-off value would be an inference from an
enum or name, not proof of a temporary mute. A station change can also lose the
current station and playback position, preventing exact restoration. Neither
entry point has exact-profile expected bytes in the repository.

**Criteria:** Fails 1, 3, 4, 6, 7, and 9.

### 5. `cMusicManager::bRadioOff` or other `cMusicManager` field writes

**Evidence found:** The pinned SDK exposes `bRadioOff` with
`GAME_10EN` global addresses `0x650B89`, `0x650B89`, and `0x660B91`. The class
also exposes radio and in-car state fields, including `m_nRadioStation`,
`m_nRadioPosition`, and `m_nRadioInCar`.

**Risks and rejection:** This is direct memory mutation, which is prohibited for
this phase. The address is not validated against the exact local candidate, no
expected bytes or equivalent target validation exist, and the field's behavior
across vehicle entry, exit, announcements, and service ticks is unknown.
Writing or restoring a guessed field value could leave the radio permanently
disabled or alter station state.

**Criteria:** Fails 3, 4, 5, 6, 7, 8, and 9.

### 6. Vehicle/audio lifecycle functions

**Evidence found:** The SDK exposes `cAudioManager::PlayerJustGotInCar()` at
`0x56AD10` and `PlayerJustLeftCar()` at `0x56AD20`, plus
`cMusicManager::ServiceGameMode()` at the `GAME_10EN` address `0x57D690`.
The existing project callback runs after the original game-process callback and
captures snapshots on the game thread; it also documents that teardown remains
outside `DllMain` and the loader lock.

**Risks and rejection:** These are lifecycle or service paths, not a proven
radio-only mute mechanism. Hooking or calling them could duplicate game work,
interfere with normal state transitions, or run before/after the effective mixer
state changes. No exact-profile bytes, radio-only contract, state capture, or
rollback proof exists for them.

**Criteria:** Fails 1, 3, 4, 5, 6, 7, and 9.

## Volume and persistence distinction

`CMenuManager::m_nPrefsMusicVolume` at `0x005F2E4C` is the observed menu
preference with a `0..127` range. The project intentionally reads it only. It is
not evidence of the effective vehicle-radio mixer volume and must not be set to
zero or used as the restoration value. `SetMusicFadeVol` and
`SetMusicMasterVolume` are separate SDK calls whose scope and state semantics
remain unproven.

## Execution timing and thread safety

The current observer callback is reached from the game-process callback and
dispatches the original function before the observer snapshot. That provides a
possible game-thread execution point for a future controller, but it does not
prove the correct point relative to vehicle-entry/exit audio service. The
controller would need an explicitly reviewed ordering rule for:

- entering a vehicle and the first radio service tick;
- leaving a vehicle, changing vehicles, pause, reload, and game-not-ready;
- online stream start/switch completion and original-radio mute;
- stream failure, exception, shutdown, and restoration.

The existing lifetime design keeps the ASI resident until process termination and
keeps explicit teardown outside `DllMain`/loader-lock execution. Any future game
function call must preserve that rule and must not be issued from an arbitrary
worker thread.

## Viability decision

No candidate satisfies all ten required criteria. Therefore there is **no
recommended implementation candidate** in this phase, and the status is
**blocked**. The production factory must remain `NullOriginalRadioController`.

The closest research leads are `SetMusicFadeVol` and the `cMusicManager` radio
state, but neither is a recommendation: the first is not proven radio-only and
the second would require prohibited, unvalidated memory writes.

## Missing evidence before implementation

All of the following are still required for one candidate on
`gta3_classic_local_candidate`:

1. An independently reproduced, legally obtained exact executable identity and
   a target-specific address/ABI review; the current profile is only locally
   reproduced and structurally resembles `GAME_10EN`.
2. A disassembly or equivalent clean-room evidence package showing the candidate
   affects only the original vehicle radio and not general music, effects,
   frontend, announcements, or cutscenes.
3. Exact entry bytes, or an equivalent target validation artifact, for every game
   function or global used by the controller. Generic `GAME_10EN` addresses are
   insufficient.
4. A read/preserve/restore contract for the effective mixer state, including
   interrupted transitions and repeated vehicle entry/exit.
5. Proof that the operation is transient and cannot modify the saved menu
   configuration or player preferences.
6. A game-thread call point outside loader lock with a documented ordering rule
   relative to the game's audio service.
7. Offline failure-injection tests showing that every failed mute or restore path
   fails closed without leaving the original radio permanently muted.
8. Manual validation on a disposable, legally obtained installation. This is
   currently blocked and was not attempted.

## Future implementation and rollback plan

1. Keep `OriginalRadioController` unchanged until the evidence above is complete
   and independently reviewed.
2. Add a profile-scoped target description containing the function/global,
   calling convention, exact expected bytes or equivalent validation, and a
   readback contract. Do not use generic `GAME_10EN` values directly.
3. Implement the smallest temporary controller behind the existing factory;
   reject unsupported profiles before any call or write.
4. Capture the effective state before muting, verify the muted state, and make
   restoration idempotent. Never restore from a guessed default or the menu
   preference.
5. Invoke only from the validated game-thread point, outside `DllMain` and the
   loader lock, after successful stream activation and before any required radio
   service transition.
6. On any validation, call, readback, stream, exception, or shutdown failure,
   stop the external stream and attempt exact restoration. If restoration cannot
   be verified, remain unavailable and report a failed-closed state.
7. Roll back by removing the controller integration and returning the factory to
   `NullOriginalRadioController`; no persistent configuration or executable
   patch should remain.

## Final indication

**blocked** — research is complete for the currently available repository and
pinned SDK evidence, but a safe reversible original-radio control mechanism has
not been proven. No production write, persistent preference change, GTA III
execution, or menu automation was performed.
