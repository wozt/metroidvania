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
| MZM rooms | Partial, systematically audited | 331 descriptors: 330 partial renders, 1 unsupported sentinel, 0 errors |
| Aria rooms | Partial, systematically audited | 343 descriptors: 342 partial renders, 1 unsupported affine/non-text room, 0 errors |
| Character assets | Partial | local verified Samus and Soma animation extraction |
| GTK4 editor | Active | shared room documents, native-data overlays, catalog and area/world render audits |
| Headless backend | Active foundation | stable CLI envelope plus room, tile, collision, door, transition, entity, story and audit operations |
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

1. Complete GTK property/link forms and add project platforms.
2. Define project-owned object/ability/soul schemas and validated encoders.
3. Complete trigger extraction and attach typed events to room overlays.
4. Add native audio inventory/preview and cutscene timeline operations.
5. Define the two native engine adapter contracts around validated exported data.

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

## Patch 0067 - source-faithful Aria global map and copyable GTK4 diagnostics

- Fixed Aria global-map generation for `MV_AOS_WORLD_2`: the importer stores
  original minimap occupancy as `rooms[*].map_cells`, rather than the
  previously expected top-level `map_cells`. Existing private `world.json`
  catalogs now work without re-importing the ROM.
- The generator validates original 64x35 coordinates, flags, unique occupied
  positions, and the imported `mapped_cells` total; it never infers footprints.
  Existing optional top-level cell catalogs are still accepted.
- Made informational/status/error/help labels selectable and Ctrl+C-copyable
  across Global maps, room browsers and creation dialogs, native room editor,
  story/cutscene workspace, Explorer, Inspector, and ROM visuals.
- Increased the Global maps diagnostic excerpt from 500 to 2048 characters.
- Added source-only Aria map-contract regression cases and GTK browser label
  selection checks. Real Debian GTK4 compilation/CTest is still required.

## Patch 0068 - truthful multi-cell global-map room spans

- Aria rooms sharing native minimap cell identity render as spanning rectangles
  in the GTK grid rather than six repeated identical full-room thumbnails.
  Irregular original shapes are partitioned into exact occupied rectangles;
  no missing cell is filled and clicking any segment selects the whole room.
- Zero Mission original minimap mapX/mapY are offsets from local player
  screen-grid coordinates (pinned src/minimap.c). If verified local Clipdata
  RLE can be decoded, its 16px block width/height supplies a conservative
  rectangular room footprint in 15x10 block units. All ambiguous/overlapping,
  missing or out-of-bounds candidate spans revert to original anchor-only.
- Source-provenance is preserved per generated map row (0=MZM anchor only,
  1=validated native MZM clipdata extent, 2=original Aria minimap cell).
  Existing six-field indexes remain readable by the GTK browser.
- Preview images are shown at most once for a complete rectangular room;
  nonrectangular fragments never repeat a misleading full-room screenshot.
- Python tests cover 2x3 spans, coordinate conflict fallback, bounds,
  Aria L-shaped cells and provenance. Runtime GTK4 tests remain mandatory.

## Patch 0069 - original global-map structure and automatic previews

- Replaced MZM's misleading spatial reconstruction with original per-area
  32x32 pause-screen minimap occupancy from private LZ77 sources, when
  available. Decode from pinned raw extraction or a locally verified ROM,
  never from guessed rectangles. Original room anchors remain separate from
  map tiles with **unknown room ownership**; double-click cannot import an
  arbitrary room for unassigned minimap tiles.
- Aria defaults to an all-areas 64x35 castle map; individual areas remain
  filterable, but one region can no longer masquerade as the global castle.
- The complete 64x35 grid uses homogeneous GTK sizing and vertical scrolling.
- Overview generation automatically indexes on world selection and then
  incrementally processes each area's original graphics previews, keeping
  the UI responsive. Generated output stays under ignored private assets.
- Source and GTK4 regression checks required before final commit/push.

## Patch 0070 - draggable world maps and dedicated map creation tab

- Added left-button drag panning across the true GTK4 scrolled grid, with
  clamped scroll adjustments and unchanged room double-click activation.
- Every occupied or empty map cell has a right-click context popover with
  coordinate copying and project-map creation navigation; verified room cells
  additionally offer Open room editor. Spanning Aria rooms report the exact
  clicked minimap case, not just the rectangle origin. Unknown MZM tiles never
  open arbitrary ROM room IDs.
- Reopening Global maps reuses the existing Map creation tab instead of
  creating duplicates.
- A dedicated Map creation tab now launches the same validated Zero/Aria
  room draft dialogs; coordinates selected from an empty map cell are shown as
  **informational only**. Map placement and new-room gameplay adapters remain
  deliberately unavailable until a separate validated placement contract.
- Generated or original ROM data are never overwritten by the UI.
- GTK4 CMake build, CTest and no-proprietary guard are required on Debian.

## Patch 0071 - two-level workbench and unified map panning

- The right sidebar also separates permanent ROM visuals / Inspector from
  transient native metatile tool palettes in a nested notebook.
- Permanent navigation workspaces (Zero rooms, Global maps, Map creation,
  events, cutscenes and Aria rooms) stay in the upper GTK4 notebook.
- Native room documents open in a SECOND GtkNotebook inside the permanent
  Open editors workspace. Their closable tabs never clutter navigation tabs;
  the existing save/discard/cancel contract and detachable document tabs remain.
- Focusing a room document activates both its nested tab and the upper Open
  editors tab, even when the document was opened from a global map.
- The current world badge follows the selected native editor, not merely
  the permanent Open editors tab.
- Panning uses a capture-phase gesture on a padded scrollable surface for both
  original maps. Padding leaves cell coordinates/sizes and source occupancy
  unchanged, including the native 32x32 Zero Mission minimap.
- GTK4 regression covers nested focus/close without ROM assets. Build/CTest
  and physical mouse navigation require validation on the Debian workstation.

## Patch 0071 - GTK4 nested focus recovery

- GTK4 notebook page widgets can have internal parents rather than the
  notebook itself; walking direct parent-child pairs skips registered pages.
- Native `focus_page()` now selects the containing registered page of each
  ancestor GtkNotebook. Opening a room activates both permanent and nested
  editor tabs, including the separate native palette dock.
- The existing GTK lifecycle test covers both outer notebooks, document close
  and preservation of permanent tabs. No ROM data is needed for the test.
- Compilation, CTest and proprietary guard must pass before commit/push.

## Patch 0071 - nested focus source-contract recovery

- After the GTK4 notebook ancestry fix, the original source-only Python test
  was still asserting that the removed direct-parent traversal existed.
- Its assertion now checks the real registered GtkNotebook page lookup and
  the descendant relationship used by `focus_page()`; the GTK runtime test
  remains the authority for actual parent/child notebook activation.
- Existing room content, generated assets, and Git history remain unchanged.

## Patch 0072 - dock allocation and splitter-aware responsiveness

- Enclosed each existing GTK4 notebook (Explorer, main editor, Inspector) in
  a scrollable, clipped viewport so oversized toolbars or notebook pages can
  no longer overdraw another splitter pane. Tab ownership and detach groups
  are unchanged.
- Both nested paned splitters explicitly support shrinking their children;
  horizontal dragging reallocates the middle workspace instead of obscuring it.
- The optional right Inspector now responds to actual space left by Explorer,
  not just window-wide breakpoints, and returns when space is restored.
- Added source-contract regressions for pane boundaries, resize behavior,
  and preservation of the permanent/temporary notebook arrangement.
- GTK4 interaction testing on the developer workstation remains necessary.

## Patch 0073 - private spatial placements for authored rooms

- Versioned, ignored, project-owned `assets/extracted/authored_rooms/placements.json`.
- Only validated existing draft identities may be placed/moved; the native
  Zero 32x32 per-area and Aria 64x35 shared minimap bounds are enforced.
- Refuse overlaps with original ROM minimap cells or other project rooms;
  missing original overview data fails closed rather than inventing geometry.
- Add a functional private placement panel to Map creation, including a
  refreshable draft selector and coordinates from the global-map right click.
- Render project drafts with distinct dashed borders; clicking one never opens
  an unrelated original ROM room. No playable engine export is implied.
- Python placement tests and full local GTK4 build/CTest required before push.

## Patch 0074 - source-joined MZM footprints and map interaction polish

- Zero Mission global-map ownership now joins `RoomEntryRom` origins, exact
  Clipdata playable dimensions and original pause-minimap occupancy. The
  engine's two-block guard border on each side is removed before converting to
  15x10-block screens; missing native minimap cells are never filled.
- Rooms sharing an origin are reported as progression variants. Overlapping
  candidate geometry without unique origin evidence remains explicitly
  unassigned, with a private JSON audit containing candidate rooms/origins.
- The current verified local input resolves 1,015 of 1,180 native cells;
  37 cells remain geometry-ambiguous and 128 have no geometry claim. Sixteen
  grouped anchors intentionally absent from the pause minimap remain navigable.
- Both shared room editors now expose a read-only collision/wall overlay:
  native MZM Clipdata at 16x16 resolution and Aria BG1 collision at 8x8.
  Original values are visualized diagnostically and never written to ROM.
- Ctrl+mouse-wheel zooms both global maps and native room canvases. Map panning
  now uses stationary scroller coordinates and coalesces adjustment updates to
  one per GTK frame, removing the feedback jitter/flicker caused by dragging a
  moving content widget.
- Aria global-map semantics remain unchanged. Python contracts, GTK4 build,
  CTest and the proprietary-data guard are required before push.

## Patch 0075 - native room records and shared object catalog

- Native room imports now produce a bounded private annotation table alongside
  each work file. Zero Mission records preserve spriteset identity, graphics
  slot and event variant; Aria records preserve entity identity, parameters,
  flags and room-transition targets.
- Every room editor exposes independent Walls, Objects, Doors and Events
  overlays plus a Room data inspector. Trigger controls are present but
  disabled and labelled unavailable until native trigger structures are
  decoded; original records remain read-only.
- Added a permanent two-column Object catalog for both games. It inventories
  206 Zero Mission primary sprite types with decoded stat expressions and 174
  Aria entity types present in the verified local world catalog, including
  placement counts and known boss health.
- Missing object images use an explicit diagnostic icon. Create/clone actions
  are disabled until a project-owned object schema and validated engine
  encoders exist; the editor does not claim native mutation or sprite support.
- Nested palette notebooks now resolve their actual owning notebook during
  close and detach operations, preserving the GTK document-lifetime contract.
- Validation: 160 Python tests and all six CTest targets pass; the GTK4 editor
  was smoke-tested under Xvfb with the object catalog, Brinstar 023 annotations
  and object overlays rendered from local private data.

## Patch 0076 - semantic object identities and scroll-bounded MZM placement

- Aria catalog and room overlays no longer expose opaque `kind/id` labels.
  Enemy IDs are resolved through the hash-verified ROM constructor table and
  named pinned cvaos symbols; bosses, ordinary enemies, special objects,
  candles and conditional pickup classes now have semantic display names.
- Zero Mission object labels remain based on the pinned primary-sprite enum,
  with numeric variants formatted for readability and unused types sorted away
  from the main catalog entries.
- Zero Mission global-map ownership now uses all 138 native custom-scroll room
  records before falling back to Clipdata rectangles. Door and sprite
  coordinates provide direct evidence where source-derived regions overlap.
- With the current verified private input, owned native minimap cells increase
  from 1,015 to 1,065 of 1,180. Ambiguous cells fall from 37 to 8; the remaining
  overlaps stay explicitly unassigned instead of receiving a guessed room.
- The GTK catalog and corrected Brinstar overview were smoke-tested under Xvfb.
  Validation passes 165 Python tests, all six CTest targets and the
  proprietary-data guard.

## Patch 0077 - native room-record context menus

- Visible objects, doors/transitions, events and decoded triggers now accept a
  secondary click directly on the room canvas. Hit testing follows the active
  overlay and current zoom, preferring the topmost visible native record.
- The context menu shows the semantic label, native identity, variant,
  geometry and decoded parameters. It can open the full information window,
  locate the matching row in **Room data**, or launch a type-specific editor
  shell.
- The same context menu is attached to every **Room data** entry. The selected
  canvas marker receives a yellow outline so overlapping records remain clear.
- Editor shells are intentionally read-only and keep Apply disabled until a
  versioned project schema and validated per-engine encoder exist. They never
  modify the original ROM or private imported annotations.
- Object-menu and editor-window interaction was smoke-tested under Xvfb with
  Brinstar 023. Validation passes 166 Python tests, all six CTest targets and
  the proprietary-data guard.

## Patch 0078 - semantic, independent native entity overlays

- MZM room sprites and Aria entity records use explicit ENEMY/ITEM/OBJECT/DOOR/OTHER
  annotation kinds. No conditional MZM sprite variant is relabelled as an
  EVENT: the original variant and event identifiers remain in the details.
- Items and enemies can be filtered independently of doors, scenery, events,
  triggers and unknown native actors in the shared GTK room editor.
- Zero Mission classification consults pinned native sprite identities and
  source sprite health/damage fields. Unknown types stay OTHER; no fake item
  or enemy identities are invented. Unparsed special item/trigger tables are
  not claimed to be decoded. Existing legacy ENTITY files remain viewable
  under OTHER until their private native annotations are regenerated.
- Aria native kind/id controls enemy, pickup, special object and door/gate
  semantics, while separately parsed screen transitions remain DOOR records.
- The pre-existing GTK source contract is updated to assert all eight overlay
  labels and the still-disabled native trigger decoder.
- All records and original graphics remain read-only. No ROM bytes are altered.

## Patch 0079 - editable private room entity markers

- Shared Zero Mission / Aria room editor: right-click empty room pixels to
  create a project enemy, item or world-object marker; the selected world and
  verified room geometry are enforced by the project entity validator.
- Native source annotations are never modified. Private authored entities live
  in versioned, ignored `assets/extracted/overrides/{metroid,aria}/entities/`
  JSON documents with stable monotonically increasing IDs and atomic saves.
- Select tool supports direct mouse drag of project entities. The context menu
  also offers Move (click destination) and Delete. Coordinates are 16-pixel
  aligned and bounded to the current native room canvas.
- Separate dashed white borders and `[PROJECT]` labels distinguish authored
  markers from read-only native records. Existing native collision and door
  annotations remain untouched. Creation deliberately does NOT promise gameplay
  execution or a native-ROM encoder.
- Python tests validate round-trip create/move/delete, cross-world identity,
  schema/geometry constraints, symlinks, and non-mutation of original annotations.
- All tests, GTK4 compilation and proprietary-file guard must pass on Debian
  before Auto AI PyPatch commits/pushes this change.

## Patch 0080 - validated native constructor binding for project markers

- Zero and Aria project ENEMY/ITEM/OBJECT markers now select a native definition
  from the matching source catalog instead of accepting invented type strings.
- MZM uses source-derived primary-sprite roles; Aria uses native kind/ID and
  excludes door constructors from the generic OBJECT editor.
- Existing project markers can be rebound through their context menu. The
  project-owned room JSON retains stable ID, room and coordinates, and gains a
  validated reference plus source display name; originals are untouched.
- Catalog absence falls back to a clearly unassigned project marker. No
  sprite preview or gameplay encoder is claimed where none exists.
- ROM-free Python regression tests cover category boundaries, assignments and
  fail-closed wrong-game/wrong-role IDs. GTK4 build/CTest required on Debian.


## Patch 0081 - Aria item controls and native-image-aware selectors

- Native-definition dropdown rows render a 28px image from private, verified
  `assets/extracted/sprite_previews/{aria,mzm}/<native-token>.png` when present.
  Undecoded monster images explicitly show `image-missing-symbolic`, never
  misleading cropped backgrounds or unrelated community/fan sprites.
- Aria lists all documented pickup subtypes (money, consumable, weapons,
  armor/accessories, red/blue/yellow/ability souls), with normal, hard-mode
  and all-souls-found families, even if never placed on a vanilla world map.
- Aria authored items support bounded item ID, two native-sized u16 metadata
  parameters and u8 flags. Existing schema-v1 documents remain readable; item
  settings are optional and validated in both CLI and Python API. These are
  project settings, not a claim of engine export/behavior.
- The optional local-ROM thumbnail decoder only accepts source-verified icon
  pages with an explicit 0x2000-byte header and refuses unknown page wrappers;
  no ROM bytes or generated assets are committed. PNG icons are illustrative
  of item subtype; the item-ID-specific previews are cached privately.
- A complete enemy sprite extraction / OAM compositor is not implemented in
  this patch. Genuine enemy PNGs, when already decoded into the documented
  cache, display in the same dropdown factory.

## Patch 0082 - named Aria item choices and per-record thumbnails

- Dropdown offers individual named US-ROM item/soul records (weapon, armor,
  consumables and four soul types), not merely pickup-category labels.
  Hard Mode / All Souls variants remain visible as separate condition families.
- Names are decoded from the verified owner's ROM text pointer table; native
  constructor subtype and item ID remain separate for schema-v1 compatibility.
- Aria item thumbnails resolve by selected subtype AND item ID, directly in
  GTK4 dropdown rows. Actual monster sprite/OAM decoding remains future work.
- Missing graphics use a neutral diagnostic symbol, not a fake sprite. The
  original ROM, extraction data and untouched native records remain immutable.

## Patch 0083 - authentic Aria graphics wrappers and name controls

- Fixes the v0081 icon extractor: GBA pages use byte-encoded GfxWrapper
  descriptors (raw or GBA LZ10), not a uint32 0x2000 prefix.
- Icon table entries pack 1-based sprite index and +4 palette number;
  palette pages have a four-byte descriptor and multiple 16-color palettes.
  Regenerate private PNG caches using correct source colors and indices.
- Aria string decoder handles known embedded formatting/control opcodes and
  the complete documented USA extended-character range rather than marking
  any formatted item name undecodable. Provides item-name diagnostic counts.
- No attempt to fake enemy sprites, souls without verifiable icons, or
  executable ROM changes. All modified assets remain private and ignored.

## Patch 0084 - Aria item-name CLI and GTK4 widget lifecycle

- Move `scripts.aria_item_details`' `__main__` entrypoint below all native
  decoder functions; direct `python3 -m scripts.aria_item_details` no longer
  crashes with an undefined `item_name` before reporting genuine ROM status.
- Track `Room data` list/status widgets with their document lifetime, including
  closing or detaching their right-side palette notebook page while a room is open.
- Guard popup selection callbacks with weak-tracked child widgets; disconnect
  callbacks when the form is destroyed to prevent stale GTK4 label accesses.
- Add ROM-free execution tests for direct module entrypoint and GTK lifecycle
  source contracts; no item placement or commercial source data is changed.
- GTK4 runtime still requires local testing after Auto AI PyPatch compilation.

## Patch 0085 - color legend and authentic cached room sprites

- Every room tab displays an eight-role color legend, including the decoder status
  of native triggers and the distinction between native and authored entities.
- Enemy and item category toggles now control BOTH their colored outlines and
  native 1D sprite preview PNGs on the room canvas (Cairo nearest-neighbor).
  Original hitboxes, room collision and read-only native annotations remain intact.
- Exact Aria item icons (when decoded locally) are matched using pickup subtype
  plus project item ID or original native placement parameter 0. Conditional
  Aria enemy ID references and MZM PrimarySprite identities resolve private
  preview paths without mixing unrelated enemies and items.
- A per-room bounded, negatively cached Cairo PNG cache avoids re-reading every
  image each frame; reopening or project edits invalidate cache safely.
- Native enemy OAM decomposition is NOT finished. Existing genuine previews are
  shown; an opt-in validated PNG import helper accepts accurately identified
  enemy sprites from the user's legally extracted assets. Missing sprites leave
  the color square visible, without substituting unrelated sprites.
- All original ROM bytes and private copyrighted resources stay untouched.

## Patch 0086 - source dock in place of Explorer and compact room legend

- Retired Explorer: it only duplicated existing GTK navigation buttons and did
  not own any data or editing tool. Its left dock now contains actual persistent
  source tabs: Zero rooms, Aria rooms (immediately adjacent), Global maps,
  Map creation, Object catalog, Events and Cutscenes. ROM visuals and room
  palettes stay in the right dock.
- Kept the center for Open editors and its nested, closable native room tabs;
  permanent source tabs and ephemeral documents retain separate drag groups.
  Source and center panes remain independently resizable. Opening a room while
  in narrow single-pane mode focuses the center editor.
- World badge responds to source-tab and document selection without relying on
  the former Explorer shortcuts.
- Compact, left-aligned, non-expanding colored legend uses short labels and
  small native-color squares; it wraps if needed rather than stretching.
  Original independent overlays, sprite cache and color hitboxes are unchanged.
- Updated layout source tests, including the legacy object-catalog UI check
  to assert that its widget is now constructed in the left source dock.
  ROM source assets and authoring data untouched.

## Patch 0087 - vertical workspaces and selected central view

- Replaces the wrongly horizontal/source-tabs layout of 0086 with one compact
  GTK4 navigation list on the left, listing actual permanent center pages from
  Zero rooms and Aria rooms through Map creation, story tools and Open editors.
- Each selection activates exactly one full-sized central page; center notebook
  tab labels are hidden. Source browser pages are no longer squeezed into the
  narrow navigation column. Navigation also synchronizes on programmatic
  center-page changes and when opening a native room document.
- Right side retains Inspector and Room palettes, removes redundant ROM visuals
  UI and its now-unused static callbacks. Private extracted graphics and all
  actual ROM sprite importers are untouched.
- Earlier 0086 source-layout tests are updated to assert the corrected design;
  a dedicated regression asserts navigation mapping, room focus and removal of
  the ROM visuals page. GTK4/CMake, CTest, all Python tests and proprietary-file
  guard remain validation gates.

## Patch 0088 - exhaustive render audit and shared headless backend

- Added a real decoder audit for all discovered rooms, with bounded area-level
  multiprocessing and structured per-room identities, resource references,
  layer status, expected dimensions, unresolved references, duration and errors.
- Corrected MZM native RLE bounds/tails, source-table tileset aliases and test
  resource paths. Corrected Aria palette resource units as a final row index.
  Current private audit: MZM 331 rooms (330 partial, 1 unsupported, 0 errors),
  Aria 343 rooms (342 partial, 1 unsupported, 0 errors).
- Added `scripts/editor_backend.py`, the stable `fusion_editor_cli` launcher and
  `fusion_map_editor --headless`. Both run without GTK/display initialization,
  use strict command options, JSON/error envelopes, bounded batch validation,
  dry-run behavior and honest unavailable capabilities.
- Both GTK room browsers now run selected-area or complete-world audits through
  the shared backend and retain detailed reports only in ignored private output.
- Rendering caches reuse parsed MZM source/tilesets and decoded Aria resources
  within a worker. Audits exercise the renderer without producing hundreds of
  preview images.
- Native gameplay, full rendering, audio decoding and engine export remain
  unavailable; the capability matrix and CLI reference state these boundaries.
- Validation: all 222 Python tests and all eight CTest targets pass, including
  both no-display native entry points and GTK4 lifecycle coverage. The private
  exhaustive audits above completed with zero decoder errors, and the tracked
  proprietary-file guard passed.

## Patch 0089 - complete current-surface backend parity

- Routed GTK draft creation/listing, global-map placement, native room opening
  and rendering, object inventory, project entity editing, and story validation
  and storage through the shared editor backend. The old feature-specific Python
  subprocess entry points remain compatibility tools, not GTK business logic.
- Added strict headless contracts for draft placement/removal, project entity
  list/inspect/create/move/assign/delete, named catalogs, Aria item parameters,
  story validation/save, project audit, native workroom opening and asset lists.
  Destructive removals require explicit confirmation; dry runs validate without
  persistence; GTK row consumers use bounded TSV from the same backend.
- Added `fusion_native_map_cli`, an internal adapter around the exact C
  `native_map_load/edit/save` core used by GTK. Public `layer-list`, `tile-get`,
  `tile-set` and `tile-fill` operations now provide no-display BG1/BG2 parity
  without duplicating native-map rules. Writes target only ignored overrides.
- Raised the artificial native-workroom axis cap from 128 to 255 cells while
  retaining the 6,144-cell per-layer bound. This opens verified wide/tall rooms
  such as MZM Brinstar room 003 (`19x134`) without weakening allocation bounds.
- Added a functional CMake adapter round trip (read, set, reload, bounds error),
  Python draft/entity/story round trips, confirmation/dry-run contracts and a
  documented GUI/backend/CLI feature matrix. Collision, doors, native entity
  encoding, custom object definitions, graphical event/cutscene authoring and
  audio remain explicitly unavailable rather than simulated.
- Validation: all 230 Python tests and all nine CTest targets pass, including
  the no-display native entry points, GTK lifecycle tests, native-map adapter
  round trip and proprietary-file guard. The 0088 exhaustive private audit
  remains MZM 330 partial + 1 unsupported and Aria 342 partial + 1 unsupported,
  with zero decoder errors.

## Patch 0090 - unified project collision, doors and transitions

- Upgraded the existing per-room private entity document to the unified
  `metroidvania.project-room-data` version 2 schema instead of creating separate
  sidecar files. It now owns entity markers, sparse semantic collision cells,
  project doors and transition references. Version 1 documents migrate in
  memory and remain untouched until an explicit save.
- Added strict collision list/get/set/fill/clear/validate operations. Zero
  Mission uses 16px project cells and Aria uses 8px project cells; bounds,
  duplicate coordinates, semantic types and destructive confirmation are
  validated. These are project semantics, not rewritten native collision bytes.
- Added project door list/inspect/create/update/delete and transition
  list/create/update/delete/validate operations, plus `door-link`. Door geometry,
  IDs, types, facing, source uniqueness and native target-room existence are
  checked. Non-zero destination-door IDs remain explicitly unverified without
  target project geometry, and both engine adapters remain unavailable.
- GTK room documents load the same backend TSV, render project collision above
  the immutable native diagnostic overlay, and expose context actions to set or
  clear collision cells and create/view/delete project doors. Full GTK property
  and transition-link forms remain pending; the complete operations are already
  available through the shared headless backend.
- Added functional migration, collision, door, dependency, persistence and CLI
  round-trip tests. Validation: all 234 Python tests and all nine CTest targets
  pass, including GTK lifecycle/no-display entry points and the proprietary-file
  guard. No ROM, native annotation or extracted proprietary asset is modified.

## Patch 0091 - show native Zero Mission doors by default

- Fix GTK room documents opening with every native annotation overlay hidden.
  Enemy, item, object, door and event markers are now visible on first open;
  walls and unknown records stay opt-in, and undecoded triggers stay disabled.
  Users can still independently hide/show any enabled category.
- Ignore the real `DOOR_TYPE_NONE` MZM table sentinel instead of drawing a
  non-door at (0,0). Preserve original door indices for destinations.
- Add an actual GTK toggle interaction regression and a synthetic native MZM
  door-table parser test. No ROM, extracted image or private override is changed.
- Native doors are purple diagnostic outlines with D indices; decoding and
  displaying their authentic graphical hatch sprites is still separate work.

## Patch 0092 - Zero Mission original world-grid reconstruction

- Corrected the native GBA minimap occupancy check to mask palette and flip bits
  before comparing against tile ID `0x140`; preserves full raw tile words.
- Native door/sprite evidence may identify an otherwise unowned original minimap
  case only when exactly one source-room family matches. Ambiguous cases remain
  unassigned instead of inventing room membership.
- Added a separate source-derived native-door index in original minimap
  coordinates, without treating doorless sentinels as actual doors. GTK shows
  native door markers in a dedicated toggleable global-map overlay.
- MZM global-map previews are now individual 240x160 playable screens cropped
  from BG1 after the genuine 32px Clipdata guard, downsampled to native-aspect
  60x40 thumbnails, not whole-room images stretched over arbitrary shapes.
- Aria multi-cell geometry and previews remain unchanged.
- Fixed prefetch skipping the first map area after a zero-budget indexing pass;
  bounded asynchronous batches continue while new previews are generated.
- Added ROM-independent mapping/preview regressions; run the local CMake and
  full test suite before pushing. This does not complete animated graphics,
  BG0/BG3 blending or full gameplay rendering.

## Patch 0093 - compile and legacy minimap regression repair

- Correct GTK global-map preview prefetch callback to use its existing `out` subprocess output buffer, fixing two undeclared `output` references.
- Update the earlier minimap-source regression to expect provenance 1 for an original ROM-backed room anchor, consistent with patch 0092.
- No changes to native minimap coordinates, private renders, ROMs or project data; validate with CMake, CTest and the proprietary guard.

## Patch 0096 - verified Zero Mission door transitions in global map

- Escape separator pipes in compound native door type labels, and extend
  the ignored private `world_overview/mzm_doors.tsv` with destination
  room and destination door only for native intra-area door-table targets.
  Out-of-area links, invalid indices and sentinel targets remain unresolved.
- Keep backward compatibility for earlier six-column TSV indexes; regenerate
  after import to populate target-room navigation.
- Add a read-only, expandable GTK door inspector for the selected MZM room.
  Open verified destination rooms directly in the native workroom editor;
  never create guessed area links or modify original ROM data.
- Native map door labels now include known destination IDs in their tooltips.
  Rendering authentic hatch sprites remains separate work.

## Patch 0097 - native MZM doors in individual rooms

- Fix the native room annotation TSV parser: the details field contains the
  original door bitflag expression with a literal `|`. GTK previously split
  such lines into eleven fields and discarded the entire native door.
- Preserve the full native expression by splitting only the first nine `|`
  separators, including for already-extracted private annotation files.
- Add a real GTK importer test with both original doors of a synthetic
  Brinstar 010 room (the source table has two corresponding entries).
- Keep optional global door-number badges switched off by default. The
  global map's verified door-navigation panel remains unchanged.
- Native room door markers are still read-only diagnostics; authentic
  animated hatch sprite rendering remains future work.

## Patch 0098 - original native Zero Mission hatch BG1 graphics

- Decode the original shared 0x400-based metatile table and common graphics/
  palette from the owner's verified US ROM or ignored extracted raw resources.
  Generate private transparent hatch PNGs for all five hatch types and both
  orientations, with a separate Mothership graphics family.
- Identify real hatch type and facing from native Clipdata (not guessed colors),
  using the exact engine values. Place each hatch one block beside its door
  transition as ConnectionLoadDoors does. Unknown entries remain read-only
  diagnostic boxes, with no fabricated hatch image.
- Display original 16x64 metatile graphics at actual room scale via the
  existing cached sprite-preview surface; no gameplay or ROM mutation.
- Add synthetic pixel/PNG and Clipdata identity regressions.
- Opening/closing animation and event-dependent palette switching remain future
  work; CMake/CTest and private-ROM guard still required locally.

## Patch 0099 - native Zero Mission hatch animation preview

- Reconstruct the original hatch opening/closing BG1 metatile indices from
  `ConnectionUpdateHatchAnimation` (four opening frames, three intermediate
  closing frames, return to the closed native tilemap).
- Export private ROM-derived PNG frames using the same verified common source
  graphics as the closed hatch, without inferred animation art.
- Add an optional, read-only `Animate hatches` toggle to MZM workrooms only.
  The GTK animation uses a short preview loop, not gameplay/event simulation;
  the timer is cleaned up when a room closes.
- Keep closed hatch artwork as fallback for missing original frames; preserve
  Aria workrooms, map overview and authored collision/door overrides.
- Add native index/pixel regression tests and a GTK document-timer test.

## Patch 0100 - native Zero Mission hatch lock/event preview

- Decode the pinned original hatch event tables by *hatch slot*, not the global
  door number. Add event conditions to read-only native door metadata only
  while the prior hatch slot ordering can be verified from original Clipdata.
- Add GTK4 per-room manual preview modes: original, all named native events
  off/on, fully open frame, temporary lock and permanent security lock.
  BEFORE and AFTER conditions follow the decomp's event-bit polarity; the
  security hatch graphic uses native common metatile 0x2A.
- Only source-derived graphic PNGs may be displayed; unsupported frames fall
  back to the original native closed hatch. A manual scenario suspends the
  animated preview; Aria and the global map remain unchanged.
- This is a visual, hypothetical all-events snapshot, not a saved game state.
  More complete event engine state/door runtime mutation is future work.
- Source-only Python regressions cover event flags and security tile indices.

## Patch 0101 - Aria original screen-transition geometry

- Use 256x256 screen/map blocks rather than the 240x160 LCD viewport.
- Preserve native screen identity. Draw approximate edge markers only where
  room extent and boundary can be determined; never assert pixel precision.
- Invalid native coordinates are inspector-only, not fabricated exits.
- Aria-only import, no ROM edits or changes to Zero Mission.

## Patch 0102 - shared project-door property editor

- Replace the read-only project-door editor shell with a dedicated GTK4 form
  for label, geometry, semantic type and facing in both Zero Mission and Aria.
  The same validated headless backend implements door-update and linking.
- Choose an existing native target world, area and room and save project-owned
  transitions; edit or remove an existing link. Original source doors remain
  immutable and cannot open a project-edit form.
- All edits stay in the ignored versioned project room document; cross-world
  targets are validated by the existing backend, not assumed playable.
  `target_door_id=0` explicitly means unspecified.
- Scope: target room is chosen in the form, not by clicking the global map.
  True map-click destination picking, bidirectional link assistance and engine
  adapters remain follow-ups. The patch does not alter existing map layout,
  tileset renderer, ROM, native annotations or story data.

## Patch 0104 - native door project overrides

- Context Edit on an original Zero Mission or Aria DOOR now creates/reuses a
  project-owned override bound to the original annotation's verified index,
  variant and type. The backend validates that identity against the private
  imported room annotation TSV and refuses arbitrary/invented source geometry.
- The unchanged original remains in the ignored private source annotation file;
  its canvas marker is temporarily superseded by the editable override.
  Deleting the private override reveals the source door again.
- Existing project-door edit and cross-world destination form are reused;
  legacy version-2 room documents and project doors without native source
  references remain compatible. This is not a native engine door encoder.
- Real Aria door sprite graphics and frame-accurate transition positions still
  require separate native research; screen-boundary anchors remain approximate.

## Patch 0106 - native room picker for project door targets

- One-shot, cancellable Global maps room selection from the shared GTK door form.
  Both Aria and Zero Mission use verified original map ownership; unknown tiles,
  private drafts and unassigned cells are refused.
- Selection fills target world/area/room only. Target project door ID and spawn
  point reset to zero (safe unspecified values); explicit Save destination still
  runs existing backend validation before persisting. Nothing is saved on click.
- Closing a form abandons its callback; closing a map cancels pending
  selection. Weak pointers prevent dangling map/form/workspace references.
- No ROM, project data format, runtime engine, minimap grouping or room tile
  rendering changes. Bidirectional linking is not automatic.
