# Licenses and third-party boundaries

## Project-owned files

Original project-authored code and documentation are licensed under
**GNU General Public License version 3.0 only** (`GPL-3.0-only`). See `LICENSE`.
Copyright (c) 2026 wozt and contributors, for their respective contributions.
This change applies only to material the project contributors are authorized
to license. Previously distributed copies retain their earlier license grants.

## Reviewed dependencies

- `third_party/mzm`: MIT; retain its original copyright and license.
- `third_party/cvaos`: MIT; retain its original copyright and license.
- SDL3 system library: zlib license; retain applicable notices.

These files are not relicensed as GPL simply by being referenced or shipped
separately. MIT-licensed code may be incorporated under GPLv3 requirements
while preserving MIT notices.

## Research-only / blocked integrations

- `sergiomanzur/gbarecomp`: audited revision uses **PolyForm Noncommercial
  1.0.0**. Its restriction on commercial purposes is incompatible with
  distributing a combined GPLv3-only program under GPL terms. **Do not copy,
  compile into, link with, or distribute its runtime as part of this project**
  unless a compatible license or explicit sufficient permission is obtained.
  Standalone experiments must remain separate and respect upstream terms.
- `sergiomanzur/ariaOfSorrow-recomp`: root frontend has no confirmed license
  authorizing reuse; do not import its unlicensed host implementation.
- `metroid-zero-mission-pc-edition`: emulator-based derivative; not a dependency.
  Requires separate file-by-file license review before any contemplated reuse.

Technical research results in `docs/` are not a license grant. For an authentic
Aria integration, prefer independently written GPL-compatible hardware/runtime
abstractions, appropriately licensed source ports, or ask the relevant authors
for compatible licensing. Review any generated output and other transitive
components separately.

## Proprietary game content

Users must provide their own legitimately obtained supported ROM files locally.
No Nintendo or Konami ROMs, graphics, audio, level data, extracted binaries,
BIOS images, or proprietary game files may be committed or distributed.
Neither this GPL license nor upstream source licenses grant rights to those
works. Distribution of a future patch or game binary requires separate review.

## Upstream references

- <https://www.gnu.org/licenses/gpl-3.0.html>
- <https://polyformproject.org/licenses/noncommercial/1.0.0/>
