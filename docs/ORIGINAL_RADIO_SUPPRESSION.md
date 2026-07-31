# Original radio suppression

This phase adds an opt-in suppression lifecycle without adding a production game
write. Both defaults remain off:

```text
SAORS_ENABLE_ORIGINAL_RADIO_SUPPRESSION=OFF
```

```ini
[Experimental]
MuteOriginalRadioDuringGameplayAudio=false
```

The build gate requires `SAORS_ENABLE_GAMEPLAY_STREAM_EXECUTOR=ON`. Runtime
activation additionally requires the gameplay executor, the INI opt-in, the
exact locally reproduced executable profile, and an available validated
`OriginalRadioController`. If any condition is absent, the controller is not
called and the original radio remains untouched.

## Evidence decision

The pinned plugin-sdk declares music fade/master-volume methods, but the
available material does not establish all properties required for a safe write:

- exact entry bytes for the matched executable profile;
- radio-only scope rather than all music or frontend audio;
- a readable effective value that can be restored exactly;
- temporary, nonpersistent behavior;
- safe call timing and failure semantics.

The audited menu music-volume address is a user preference and is read only.
Writing zero there could alter saved settings and would not prove control of the
effective mixer. The project therefore does not add a Windows x86 implementation,
new address, offset, signature, or memory patch. The production factory always
returns `NullOriginalRadioController`, whose `available()` is false.

## Lifecycle contract

Fake-backed offline tests cover the integration contract for a future validated
controller:

- mute is requested only after stream start or switch succeeds;
- online-to-online switches, pause, resume, and volume updates do not repeat the
  mute write;
- stop, online-to-original transition, player exit, unbound/disabled station,
  game-not-ready state, explicit shutdown, and destruction restore;
- stream, playlist, backend, controller, and exception failures stop playback
  and restore before remaining failed closed;
- an unsupported profile or unavailable mechanism is never written;
- shutdown and destruction are idempotent and run outside `DllMain`.

The ASI runtime objects retain process lifetime, so their destructors are not
invoked under the Windows loader lock. Explicit `shutdown()` is available for
owned lifetimes and offline tests.

## Logging

Only fixed, non-sensitive messages describe this feature:

```text
Original radio suppression: unavailable
Original radio suppression: enabled
Original radio: muted
Original radio: restored
Original radio suppression: failed closed
```

Configured URLs, tokens, paths, raw memory values, and backend error details are
not included.

## Blocked manual validation

There is no safe GTA III suppression test to perform yet. Manual validation is
blocked until a temporary radio-only mechanism has exact-profile entry-byte
validation, independently reviewed semantics, and a proven way to capture and
restore the effective state exactly. Enabling both gates today still performs no
game write and will log that suppression is unavailable.
