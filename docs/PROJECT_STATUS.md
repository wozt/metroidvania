# Project status

Last updated: **2026-10-09**.

## Current product state

The repository is a native-content reconstruction and editor project. It is not
yet a playable crossover and contains no active simulated or emulator-backed
game frontend.

| Area | State | Verified boundary |
|---|---|---|
| Story | Data baseline | Complete campaign bible and dependency-checked parallel timeline |
| Boss inventory | Verified native identities | 9 MZM encounters and all 11 Aria campaign bosses source/ROM-mapped |
| Savepoints | Verified metadata | 29 MZM platform rooms and 17 Aria map-flag rooms |
| MZM rooms | Partial | descriptors, atlas, BG1/BG2 editing and experimental BG3 |
| Aria rooms | Structural inventory + preview | 343 descriptors, 725 transitions, 2,336 entities; experimental background renderer |
| Character assets | Partial | local verified Samus and Soma animation extraction |
| GTK4 editor | Active | native MZM documents, safe tab lifetime, atlas and asset previews |
| Native gameplay engines | Not started | no player physics, combat, entities or room runtime yet |
| Dual-character AI | Not started | story/design requirement only |
| Cross-world travel | Not started | savepoint metadata exists; no native runtime loader |

## Patch 0047 - active-tree and documentation cleanup

- Copied 94 superseded files to local ignored `legacy/` storage with their
  original paths preserved.
- Removed the rectangle-room demo, sample tilemaps/world graph, simulated
  backends, mGBA diagnostic frontend, transition experiments and their tests
  from the active tree.
- Removed superseded feasibility/recompilation/side-project documents and
  replaced accumulated prototype documentation with concise current references.
- Reduced CMake to the native editor core, GTK4 editor, SDL3 room viewer,
  active native tests and the Python extraction/data suite.
- Removed obsolete graph/tile/demo/capture controls from GTK4 while retaining
  native room tabs, atlas browsing and local authentic sprite previews.

## Patch 0048 - Aria native room inventory

- Expanded the hash-gated Aria importer from the global map into all 343 native
  room descriptors, including three background records, resource references,
  entity placements and 725 resolved room transitions.
- Bounded every pointer walk, variant chain and terminated list; the private
  generated catalog contains metadata and ROM addresses, never payload bytes.
- Matched all eleven campaign bosses to native rooms and enemy-table identities.
  Chaos has two explicit phase rooms; repeated ordinary boss-type enemies are
  not misclassified as campaign encounters.
- Recorded raw enemy-table health values and named cvaos create/update symbols
  in the tracked boss inventory.

## Verified commands

Run after each change:

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j2
ctest --test-dir build --output-on-failure
python3 scripts/check_no_proprietary.py
```

The latest exact result is recorded in the corresponding commit handoff.

Cleanup validation on 2026-10-08: the reduced CMake project built without
warnings, CTest passed `5/5`, the consolidated Python suite passed `82/82`, the
GTK4 editor survived a three-second Xvfb smoke run, and the proprietary-file
guard passed. The active editor binary has no mGBA linkage.

Patch 0048 validation on 2026-10-08: CTest passed `5/5`, the Python suite passed
`85/85`, the exact-ROM Aria import completed with 343 rooms and 2,336 entities,
and the proprietary-file guard passed.

## Patch 0049 - Aria native room previews

- Added a verified, generated index for all native Aria rooms, with save/boss markers.
- Added a read-only GTK4 browser with asynchronous on-demand graphics decoding,
  BG1/BG2/BG3 selection, composite preview and diagnostic collision preview.
- A room lacking BG1 may now composite another successfully decoded text layer.
- This does not imply the graphics reconstruction is complete; unsupported
  affine modes, animation, blending and entities remain explicitly unfinished.

## Patch 0052 - deterministic accessibility backend for GTK tests

- The unsaved-close dialog test is run with GTK_A11Y=test instead of
  trying to connect to the desktop AT-SPI bus under Xvfb. This is
  restricted to the GTK test binary/CTest, never the production editor.
- The modal close workflow itself remains tested; GTK criticals and
  memory errors are still fatal. Real GTK4 validation is pending.

## Immediate priorities

1. Decode Aria tilemap/collision payloads into private native room work files.
2. Attach verified transition/access metadata to both games' savepoint records.
3. Complete MZM collision/entity extraction and verify room overrides.
4. Define the native engine contracts around decoded room data.
5. Implement the two prologues before general dual-character traversal.

## Non-negotiable constraints

- Never track ROMs, saves or extracted proprietary assets.
- Never mutate ROMs or upstream submodules.
- Never substitute fake geometry for missing original data without an explicit
  diagnostic label.
- Never call extracted metadata a completed native gameplay feature.

## Patch 0051 - confirm close and live tile stamp

- Native MZM and Aria documents display their real selected metatile as a semi-transparent cursor preview on hover; no painting occurs before mouse press.
- Picking a metatile from a room or the palette switches back to the Pencil stamp tool.
- Unsaved tab close offers Cancel, Discard changes, or Save and close in a GTK4 modal window; failed writes keep the document open.
- Additional GTK lifecycle test covers cancel, save and discard; private ROM-derived assets and overrides are never committed.

## Patch 0053 - common native world and narrative workspaces

- One original-coordinate *cell grid* for MZM and Aria. Aria cells are
  original 64x35 map occupancy; MZM currently exposes only room anchors.
- Bounded private ROM pixel-preview generation starts when a world or region is
  opened; only supported source data produces thumbnails.
- Native room tabs use one toolset for both worlds, and Ctrl+W routes through
  unsaved-edit confirmation with dialog teardown before close.
- GTK4 now exposes three-track event orchestration and a TOML-based shared
  cutscene editor with validate-before-save. These are **data editing tools**;
  no world-specific native cutscene runtime or automatic adapter exists yet.
- Keep room collision, precise Zero Mission map masks, source-accurate layer
  compositing, keyframe UI and native engine adapters on the outstanding list.

## Patch 0054 - GTK native document lifecycle hardening

- Tracked GTK page and label pointers with widget-owned document references
  so external notebook destruction and detached tools cannot retain stale labels.
- Closing callbacks now ignore documents during teardown; workspace shutdown
  detaches outstanding notebook pages before releasing manager ownership.
- Added lifecycle regressions for external page removal, repeated close/reopen,
  shutdown with an unsaved modal, and detached notebook cleanup.
- Build, CTest and on-device GUI verification must be recorded after running
  the patch locally; no result is claimed here.

## Patch 0055 - GTK4 notebook owner resolution

- Fixed closing, selecting and moving native room tabs by finding the owning
  GtkNotebook in the widget ancestor chain rather than requiring it as the
  immediate widget parent.
- Tightened GTK lifecycle tests: they now assert that pages are actually
  removed from *both* dock notebooks, including detached docks, not merely
  removed from the document manager's bookkeeping.
- Build, CTest and real GTK4 reproduction still require local validation.

## Patch 0056 - one room browser per world, one implementation

- **Zero rooms** and **Aria rooms** are instances of the same GTK4 room browser:
  area selection, source metadata, layer previews, async rendering, double-click
  to open the same editable native document tools, and identical close behavior.
- The right-click menu exposes the future **Create room** entry as disabled.
  It must not create pretend playable rooms: a verified authored-room schema,
  spatial/door validation, export adapter and consuming native gameplay runtime
  are required before it can be enabled for either world.
- The Aria global map is indexed separately (preview budget 0) before costly
  room preview generation; subprocess decoder errors are shown in the GUI.
  Existing map generation remains asynchronous, private and original-coordinate.
- Shared room browser GUI regression is added to CTest under Xvfb.

## Unified player systems - accepted requirements, pending implementation

- One character-independent capability interface: acquired powers, abilities,
  inventory flags, unlock conditions and per-world game mechanics.
- Player-specific presentation and rules: Samus upgrades, ammo/energy and
  equipment versus Soma souls, MP/HP, attributes, items and abilities.
- One status/menu framework with different views, stats and controls depending
  on which playable character is active. No fabricated equivalence of stats.
- Native controller support, configurable mapping, keyboard/controller parity,
  in-editor input preview and runtime gamepad navigation; SDL3 input backend
  must be attached only once engine input/action contracts exist.
- Cross-world abilities, room requirements and cutscene/event conditions must
  reference a shared canonical schema with world-specific adapters and checks.
- Export only project-authored assets. Never write ROMs or claim unfinished
  player systems exist in native gameplay yet.

## Patch 0057 - GTK4 room browser context lifetime

- Fixed a GTK4 ownership violation in the shared Zero/Aria browser: the
  future Create room popover was parented directly to GtkListBox. GTK4 expects
  GtkListBoxRow children, and filtering may dereference non-row widgets.
- A GtkOverlay now owns the popover, while the scrolled room list remains the
  overlay's regular child. Right-click popup coordinates are translated from
  the list into the overlay's coordinate space.
- The browser GTK regression now checks popover parenting for both games and
  repeats browser creation/destruction. Local GTK4 build and CTest must confirm.

## Patch 0058 - native room context menu ownership

- Changed the shared room browsers to use `GtkMenuButton` as the popover host,
  instead of placing a `GtkPopover` in a `GtkOverlay` allocation slot.
- The right-click shortcut opens the same anchored menu. Authored-room creation
  remains disabled until its native engine adapter and schema exist.
- The GTK test now emits progress diagnostics around both browsers' creation
  and removal, to identify the failing phase if a crash persists.
- A successful GTK4 build/CTest run on Debian is required before committing.

## Patch 0059 - GTK4 room browser test type correction

- Fixed the GTK4 room browser test's `-Werror=compare-distinct-pointer-types`
  by comparing `GtkWidget *` to `GtkWidget *` explicitly.
- The previous CTest run used the old test executable because the new test
  failed to link. It is not evidence of a remaining crash in patch 0058.
- The browser implementation is unchanged; rebuild must succeed before CTest
  results are considered meaningful. GTK4 runtime verification is pending.

## Patch 0060 - correct GTK popover return type

- GTK4's `gtk_menu_button_get_popover()` returns `GtkPopover *`, not
  `GtkWidget *`. Compare against `GTK_POPOVER(popover)` in the browser GTK
  regression test so `-Werror=compare-distinct-pointer-types` no longer blocks
  the test binary from linking.
- Correct the incorrect patch 0059 assertion and retain the 0058 diagnostics.
- The GTK browser crash is **not confirmed fixed** until the newly linked
  test executable passes on Debian. Do not rely on a stale CTest binary.

## Patch 0061 - GTK4 browser teardown ownership

- The shared Aria/Zero browser explicitly retains the filtered list and its
  referenced dropdowns, status labels, preview and menu button until the
  notebook page finalizes. GTK can otherwise destroy a toolbar before the
  GtkListBox callbacks that still read its controls.
- The page's state finalizer disconnects signal handlers and removes the list
  filter before releasing widget references; this is GTK lifetime hardening,
  not a change to private room data or playable world geometry.
- The browser GTK contract now selects synthetic rows before closing both tabs,
  exercising the close path without requiring local extracted ROM assets.
- The supplied GDB trace stopped during GTK settings initialization, not the
  CTest Aria tab-removal crash; run the debugger with CTest's isolated
  XDG_CONFIG_HOME if another crash persists. Local GTK4 validation pending.

## Patch 0062 - GTK4 dropdown model ownership

- Fixed a double `g_object_unref()` in the shared Aria/Zero room browser.
  `gtk_drop_down_new()` takes ownership of the passed `GListModel`,
  so an additional unref of the `GtkStringList` invalidated its model.
- The page teardown crash was reproduced in GDB at
  `g_list_model_get_n_items()`, via the browser's dropdown release.
- The GTK regression now checks both area models (8 and 13 entries)
  and changes their selections before removing the two tabs.
- Build, all six CTest cases, and the proprietary-file guard require
  validation on the developer's Debian GTK4 environment.

## Patch 0063 - shared private authored-room draft contract

- Added a strict versioned `metroidvania.authored-room` schema shared by Zero
  Mission and Aria, with stable world/area/slug identity and explicit metadata
  separating project-authored geometry from verified original ROM content.
- New `scripts.authored_rooms` can create, validate and list draft documents
  under ignored `assets/extracted/authored_rooms/<world>/<area>/`. Files are
  create-only, with duplicate rejection, input bounds and symlink guards.
- Drafts specify intended screen dimensions but contain no invented original
  assets, collision, entities or source-room data. They are deliberately marked
  `draft_not_playable`; game export fails closed until native engine adapters
  and spatial/collision validation exist.
- Added ROM-independent tests for both worlds, all areas, path traversal,
  duplicate IDs, malformed drafts and engine-readiness misrepresentation.
- **The GTK Create room action is intentionally still disabled**. The next
  step is connecting the common authoring form and editable project-owned
  room layers to this contract, then validated native engine adapters.
- Developer GTK4, CTest and proprietary-file verification remain required.

## Patch 0064 - GTK4 create-room draft form

- Enabled Create room in the shared Zero/Aria room browser context menu.
- A common GTK4 dialog collects area, slug, name and 1..8 screen dimensions;
  the active area is preselected independently for each game.
- Saving invokes `python3 -m scripts.authored_rooms create` with a separate
  argument vector (no shell) and asynchronous completion/error feedback.
- Duplicate IDs and private output paths are validated by the existing
  create-only Python schema; no ROM, original room or engine assets are changed.
- This creates **unplayable project drafts**, not native renderable/playable
  rooms. Existing draft browsing, editing and engine adapters remain pending.
- Added an Xvfb GTK4 form-opening test for both worlds without writing assets.
- Run local build, CTest and proprietary-file guard before considering verified.

## Patch 0065 - GTK4 button activation in create-room test

- GTK4 does not expose the old `gtk_button_clicked()` GTK3 helper.
- The GTK4 regression test uses `g_signal_emit_by_name(action, "clicked")`
  to exercise the real create-room action signal without proprietary assets.
- Only test code and documentation changed; the existing editor dialog and
  Python authored-room contract are not modified.
- Run the developer's GTK4 build, CTest and proprietary-file guard to verify.

## Patch 0066 - private authored rooms in GTK4 browsers

- Both Zero and Aria room browsers asynchronously list **validated** private
  drafts through `scripts.authored_rooms list --world ... --format tsv`.
- Draft entries are marked `[DRAFT]`, show their stable identity and intended
  screen dimensions, and participate in the original area filter.
- A Refresh drafts button reloads the validated catalog without duplicating
  entries; successful Create room jobs also refresh the corresponding browser.
- Selecting or activating a draft cannot invoke the original-room import or
  renderer. Open/Render controls are disabled for drafts; draft editing and
  engine export remain unsupported until authoring adapters are implemented.
- Async listing uses a strong page reference, page-attachment check, and
  monotonic refresh generation to avoid late results changing closed tabs.
- The GTK parser is tested with synthetic Zero and Aria draft rows only; the
  CLI TSV contract is independently tested with isolated temporary files.
- GTK4 CMake build, CTest and proprietary-file guard still require local
  verification on the developer's Debian workstation.
