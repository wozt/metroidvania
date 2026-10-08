# Decision log

## D-001 - Two exclusive backends

Decision: keep separate Metroid and Castlevania backends. Only one receives
simulation and rendering calls. This preserves each game's rules and feel
instead of creating a compromise that reproduces neither.

## D-002 - ROMs required by the prototype

Decision updated on 2026-10-08 at the project owner's request: neither room
starts unless both supported USA ROMs are valid. Unit tests remain independent
of proprietary files. Requiring ROMs does not make the simulated rooms an
authentic port, and the UI says so explicitly.

## D-003 - Upstream sources as submodules

Decision: pin `mzm` and `cvaos` as submodules without modifying their contents.
This preserves verifiable provenance, history, and license boundaries.

## D-004 - SDL3 and C11 for the host

Decision: use C11 and SDL3 for the Linux controller. A C++ layer will only be
introduced if a selected recompilation runtime requires it.

## D-005 - Aria recompilation not adopted yet

Decision: audit `ariaOfSorrow-recomp`, but do not integrate it until its Linux
behavior is reproduced and the PolyForm Noncommercial constraint is accepted.
Upstream claims do not count as local validation.

## D-006 - Official Ghidra distribution

Decision: use the official Ghidra 12.1.4 release with SHA-256
`ddac49f903da9d5bac833e5cc79395098b9c33cfd3279be5f31bd00387d2d4db`,
installed under `/opt/ghidra_12.1.4_PUBLIC`.

## D-007 - English project language

Decision updated on 2026-10-08 at the project owner's request: commits,
documentation, code comments, UI text, and diagnostics are written in English.
