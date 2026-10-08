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
Upstream claims do not count as local validation. The Linux build, framebuffer,
zero-fallback path, and 36 configured tests were reproduced on 2026-10-08. The
technical validation is complete, and the project owner has confirmed a
noncommercial intent. Project-authored code now uses the same PolyForm
Noncommercial 1.0.0 terms as `gbarecomp`, so that runtime may be evaluated for
integration. The evaluated root repository still has no license file covering
its own host code; none of that host code may be copied. The integration must
use a project-owned adapter around licensed components.

## D-006 - Official Ghidra distribution

Decision: use the official Ghidra 12.1.4 release with SHA-256
`ddac49f903da9d5bac833e5cc79395098b9c33cfd3279be5f31bd00387d2d4db`,
installed under `/opt/ghidra_12.1.4_PUBLIC`.

## D-007 - English project language

Decision updated on 2026-10-08 at the project owner's request: commits,
documentation, code comments, UI text, and diagnostics are written in English.

## D-008 - PolyForm Noncommercial 1.0.0

Decision updated on 2026-10-08 at the project owner's request:
project-authored code and documentation use PolyForm Noncommercial 1.0.0. This
matches `gbarecomp` and the project's personal, hobby, research, and
noncommercial intent. Third-party code and generated ROM-derived output retain
their own terms and are not relicensed.

## D-009 - GPL-3.0-only migration (supersedes D-008)

Decision: license original project-authored code and documentation under
GPL-3.0-only instead of PolyForm Noncommercial. Historical D-005/D-008
explain earlier decisions but no longer authorize integrating `gbarecomp`.
The PolyForm runtime is incompatible with a combined GPLv3-only release
without a separate compatible grant. Do not integrate it. The unlicensed
Aria host frontend remains excluded. Preserve all third-party licenses, and
require user-supplied ROMs without redistribution of copyrighted game data.

## D-010 - mGBA authentic execution proof

Decision: use the MPL-2.0 mGBA shared library as an interim, GPL-compatible
system dependency for authentic ROM execution. This path is explicitly labeled
emulated and does not settle the final native backend strategy. It uses only
locally SHA-1-validated user ROMs, disables automatic save handling, commits no
game data, and steps only the active world's runtime. The PolyForm runtime and
unlicensed Aria frontend remain excluded.

Implementation update: authentic execution is owned by `FusionBackend`
instances and follows the same enter/tick/render/leave contract as the
diagnostic implementations. The controller contains no separate mGBA frame
loop. Held GBA input and backend failure reporting are part of the shared
contract.

## D-011 - Memory-only authentic snapshots

Decision: expose versioned, world-typed backend snapshots for authentic mGBA
runtimes, but keep them in memory and outside `SessionState`. Capture and
restore require the runtime to be active and the payload to match mGBA's exact
state size under a 16 MiB ceiling. F5/F9 remain disabled in authentic mode.
Persistent or cross-version savestates require a separate format, ROM identity,
integrity checks, migration rules, and a proprietary-data policy.

## D-012 - Read-only verified engine state first

Decision: expose verified engine fields through backend-owned, read-only views
before designing any cross-engine state mutation. Runtime memory access is
limited to active GBA WRAM, bounded to 4 KiB per call, and unavailable for ROM
or hardware I/O ranges. The first view covers Zero Mission mode, location,
Samus movement/pose, and equipment fields matched to the pinned source and an
exact local build.

The view reports native units and separates raw decoding, plausibility, and
gameplay readiness. It does not modify `SessionState`, save files, or emulated
memory. A write or transition adapter requires a later decision defining field
ownership, conversion, validation, and rollback.
