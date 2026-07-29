# Radio station evidence model

Phase 3D keeps raw station research separate from gameplay decisions. A report is an observation record, not a compatibility claim and not a runtime station map.

## Privacy boundary

The version-1 JSON schema contains only schemaVersion, a registered executableProfile identifier, short session/source identifiers, observation ordinal, snapshot sequence, raw value, stable-frame count, playerInVehicle, and optional manually checked HUD visibleLabel plus normalized identity. It must not contain paths, URLs, hostnames, executable hashes, addresses, pointers, bytes, usernames, machine identifiers, or audio content. The portable parser rejects forbidden sensitive fields. Local reports belong under ignored research/local/ and must never be published raw.

## Evidence levels

`unverified` is reserved for hypotheses and incomplete records. `observedOnce` means one valid session. `locallyReproduced` requires the same raw/identity relationship in two complete sessions of the same exact executable profile, with separate reports and no conflict. `independentlyReproduced` requires at least two independent sanitized sources; two runs from one installation are not enough.

An entry is omitted or remains unknown when labels conflict, identities conflict, or a raw value has not been observed. Raw 10 is intentionally not synthesized from the plugin-sdk hypothesis.

## Validation

The validator rejects unknown schemas, unsupported profiles, empty sessions, duplicate ordinals, raw values outside 0..255, empty labels, non-vehicle observations, and raw/label many-to-many conflicts. The JSON parser also rejects sensitive field names. Session comparison reports profile and raw/identity conflicts without filling missing observations.

The plugin-sdk eRadioStations.h enumeration is corroborative documentation only. Its upstream MP3_PLAEYR spelling is normalized internally to mp3Player, while its provenance remains documented as a hypothesis.
