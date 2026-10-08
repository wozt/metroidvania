# Source and provenance audit

Observed and pinned on **2026-10-08**.

## Metroid: Zero Mission

- Upstream: `https://github.com/metroidret/mzm`
- Local path: `third_party/mzm`
- Pinned commit: `43b7fd52f552e4d38c1521ff9d4df5ee57e61493`
- Role: room descriptors, doors, sprite/OAM structures, graphics references,
  event constants and gameplay behavior evidence.
- Asset requirement: exact user-supplied USA ROM.

The active tools parse source declarations and locally decode selected ROM
structures. The upstream checkout remains unmodified.

## Castlevania: Aria of Sorrow

- Upstream: `https://github.com/testyourmine/cvaos`
- Local path: `third_party/cvaos`
- Pinned commit: `bc23d849d578c35ae12a5cec4e66549c3021a5be`
- License: MIT for the upstream source repository.
- Role: map flag semantics, room-pointer directory, engine region names,
  character sprite evidence and partial event/music symbols.
- Asset requirement: exact user-supplied USA ROM.

Current naming coverage is incomplete. Boss room/entity identities remain
unverified unless the project inventory states otherwise.

## Project tools

- Ghidra 12.1.4 is installed locally under `/opt/ghidra_12.1.4_PUBLIC`.
- SDL3 is used by the native room viewer and remains the target presentation
  library for the future game.
- GTK4 is used by the project editor.
- Python importers and tests use only the standard library.

## Provenance policy

Tracked files may contain project code, schemas, symbolic identifiers, offsets,
hashes and factual metadata. They may not contain ROM payload, extracted
graphics/audio/maps, save data or generated working assets. All proprietary
output stays under ignored local directories and is checked by
`scripts/check_no_proprietary.py` before commits.

Historical evaluations and abandoned prototypes are not authoritative sources.
When present locally, they live only under ignored `legacy/` storage.
