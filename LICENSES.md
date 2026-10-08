# Licenses and rights

## Code in this repository

Project-authored code and documentation are licensed under PolyForm
Noncommercial 1.0.0; see `LICENSE`. This matches the license of the selected
`gbarecomp` runtime and permits the project's intended personal, research,
experimental, and hobby use. It does not permit commercial use and does not
relicense any dependency, ROM-derived output, or proprietary game content.

## Submodules

- `third_party/mzm`: MIT license, copyright YohannDR 2025; see its `LICENSE`
  file at the pinned revision.
- `third_party/cvaos`: MIT license, copyright testyourmine 2026; see its
  `LICENSE` file at the pinned revision.
- SDL3: used as a system library; refer to the zlib license shipped with the
  installed version.

A decompilation license covers code published by its authors, not Nintendo or
Konami ROMs, graphics, audio, or other copyrighted data. This repository grants
no rights to those works.

## References not integrated

The audit reproduced `ariaOfSorrow-recomp`. Its root checkout has no license
file covering its host code, while its `gbarecomp` runtime uses the PolyForm
Noncommercial 1.0.0 license. The audit also identified
`metroid-zero-mission-pc-edition`, a VBA-M derivative under GPLv2 with additional
components. Neither is a dependency of this repository. Any integration would
require file-by-file review and a license compatibility decision.

The project owner has confirmed a noncommercial project intent and selected the
same PolyForm Noncommercial 1.0.0 terms for project-authored code. This resolves
the direct license mismatch with `gbarecomp`. It does not grant permission to
copy the unlicensed `ariaOfSorrow-recomp` root host code. Integration must use
our own adapter around the licensed `gbarecomp` and MIT-licensed `cvaos`
components, while preserving all third-party notices.

Authoritative references:

- <https://polyformproject.org/licenses/noncommercial/1.0.0/>.
