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
- GTK4 system libraries: LGPL-2.1-or-later; retain applicable notices.

These files are not relicensed as GPL simply by being referenced or shipped
separately. MIT-licensed code may be incorporated under GPLv3 requirements
while preserving MIT notices.

No static recompilation or emulator runtime is an active dependency. Any future
dependency requires a fresh compatibility and provenance review before code is
copied or linked.

## Proprietary game content

Users must provide their own legitimately obtained supported ROM files locally.
No Nintendo or Konami ROMs, graphics, audio, level data, extracted binaries,
BIOS images, or proprietary game files may be committed or distributed.
Neither this GPL license nor upstream source licenses grant rights to those
works. Distribution of a future patch or game binary requires separate review.

## Upstream references

- <https://www.gnu.org/licenses/gpl-3.0.html>
- <https://www.libsdl.org/license.php>
- <https://www.gtk.org/docs/language-bindings/c/>
