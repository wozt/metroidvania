# Roadmap

Every milestone follows the reconstruction principle of
[`ARCHITECTURE.md`](ARCHITECTURE.md#reconstruction-principle-permanent-architectural-constraint):
native resources are rebuilt from the ROMs, native behavior is ported from the
decompilations, and project changes are independent overlays.

## P0 - Editor stabilization and systematic render audit

- keep the vertical workspace navigation and eliminate GTK lifecycle regressions;
- audit every discovered MZM and Aria room through the real decoders;
- expose exact success/partial/unsupported/error diagnostics in headless and GTK;
- correct renderer assumptions only from pinned source or verified ROM evidence.

Exit criterion: both complete room catalogs produce deterministic structured
reports with no unexplained decoder errors. **Met for the current private input:**
MZM 330 partial + 1 unsupported; Aria 342 partial + 1 unsupported; zero errors.

## P1 - Shared backend and automation interface

- route non-visual editor operations through one validated backend;
- maintain stable CLI commands, JSON envelopes, batch validation and exit codes;
- keep GUI/headless project validation and private draft creation behavior aligned;
- add backend contracts before enabling unavailable editor controls.

Exit criterion: every enabled non-visual operation is scriptable without GTK,
with functional no-display tests and explicit capability reporting. **Met for
the currently enabled editor surface:** project/world/area/room queries, native
room open/render/audit, draft placement, project entity editing, story writes
and BG1/BG2 tile editing are available through the shared backend. New P2/P3
features must add their backend contract before their GTK controls are enabled.

## P2 - Complete room, object and event authoring

- finish native tile, collision, door, object, trigger and event extraction;
- define versioned project-owned schemas and validated per-world encoders;
- support create/edit/delete, undo/redo, validation and deterministic export;
- retain native source records as immutable references.

Current progress: one versioned private room document covers validated
collision cells, editable project/native door overrides, cross-world references,
project entities, typed project event/trigger regions and 8px Aria / 16px Zero
Mission placement. GTK provides a unified room editor, Grab, destination
selection on the global map, a shared chronological Undo/Redo and Save/Discard
for staged room changes. Project enemy/item/object forms atomically edit label,
snapped position, native reference and available typed item fields in both
workroom modes. Event/trigger forms edit their region, activation type, action,
stable reference and one-shot behavior. Typed action references are resolved
against timeline, cutscene, entity, event, transition and checkpoint targets
before new data is saved. Events also support bounded All/Any condition groups,
typed targets and individual negation in both workroom modes. Native ROM encoders, native trigger
extraction, object behavior definitions and a deterministic engine-neutral
export remain pending. The engine-neutral collision brush suite covers solid,
one-way, hazard, two floor-slope directions, water and air in both workroom
modes, with a machine-readable native-reference matrix.

Exit criterion: representative project rooms for both worlds can be authored,
validated and exported to an engine-neutral package without ROM mutation.

## P3 - Story, cutscene and audio workspaces

- connect typed event conditions/actions to future runtime adapters;
- add cutscene tracks, keyframes, preview and deterministic export;
- inventory native music/SFX, decode private previews and author project audio cues;
- expose all supported operations through the shared CLI.

## P4 - Two native gameplay kernels

- define separate MZM and Aria engine adapters;
- implement native player movement, collision, damage and room lifecycle;
- preserve each world's timing and mechanical rules;
- implement native Samus in MZM and native Soma in Aria first.

Current progress: an experimental SDL3 MZM room runtime loads the Brinstar 033
diagnostic room, native Clipdata collision, a content-addressed 1,384-sequence
Samus library composed from the native tables for all five suits and simultaneous keyboard/SDL3 gamepad input. Samus is driven by
an SDL-free pose controller ported from the pinned decompilation: native
movement constants, per-pose block hitboxes, jump/spin/wall-jump/Space
Jump/Screw Attack rules, crouch and Morph Ball transitions, Power Grip ledge
hanging and pulls, hurt/death poses and suit damage reduction. Animation
frames and transition endings come from the controller's native counters and
a generated 600-row semantic registry. Collision uses native Clipdata types for every exported room, and doors,
transitions and hatches link rooms through the native door tables. Samus's
box sweep still replaces the original point probes; slope speed, Speed
Booster/Shinespark,
weapons beyond the Power Beam and missiles, entities, effect overlays, hatch animation, area connections and the Aria kernel
remain pending.

Exit criterion: each native character can complete a source-authentic test path
in its own engine without emulation.

## P5 - Crossover prologues

- implement Samus against Creaking Skull in Aria rules;
- implement Soma against mandatory Deorem in MZM rules;
- add both portal rewards and the authored Interzone scene;
- persist order-independent prologue completion.

## P6 - Duo campaign

- instantiate both protagonists in one active world engine;
- add character switching, companion AI and softlock recovery;
- implement cross-equipment and alien soul mappings;
- connect verified save rooms through authored world links;
- implement the full concurrent timeline and both secret epilogues.

## P7 - Production

- complete content verification and balancing;
- version persistent saves and migrations;
- add accessibility, input configuration, credits and combined statistics;
- ship only source/tools and require local extraction of proprietary assets.
