# Current decisions

## D-001 - Native PC target

The shipping target is a native C11/SDL3 game. Emulation may be used externally
for comparison, but no emulator-backed frontend is part of the active product.

## D-002 - Two gameplay engines

Zero Mission and Aria retain separate movement, collision, combat and entity
rules. Shared systems coordinate presentation and progression without merging
the two engines into a compromise model.

## D-003 - GTK4 content editor

The project editor uses GTK4. It works on project-owned metadata and ignored
local room overrides; it never modifies source ROMs or upstream submodules.

## D-004 - User-supplied assets

Only exact supported USA ROMs supplied locally by the user may feed importers.
ROMs, saves and extracted proprietary assets are never tracked or distributed.

## D-005 - Pinned upstream evidence

`metroidret/mzm` and `testyourmine/cvaos` remain pinned submodules and are not
modified. Technical claims must cite source symbols or exact ROM structures.
Unknown values stay explicitly unverified.

## D-006 - GPL-3.0-only project code

Original project code and documentation use GPL-3.0-only. Third-party source
and locally generated output retain their own legal status. PolyForm components
are not integrated into the GPL project.

## D-007 - English repository language

Commits, documentation, code comments, diagnostics and UI text are English.
Conversation with the project owner remains French.

## D-008 - Story-gated world travel

The final campaign does not provide unrestricted global world switching.
Prologue portals, the Interzone and explicitly linked save rooms govern travel.

## D-009 - Honest implementation status

Data extraction, design records and editor previews are not described as a
playable engine. A feature becomes integrated only after its native runtime
behavior is implemented and tested.

## D-010 - Local legacy archive

Superseded prototypes are removed from the active build and copied to ignored
`legacy/` storage before deletion. They remain recoverable locally and through
Git history but cannot silently influence current targets or tests.
