# Editor CLI reference

## Product boundary

`fusion_editor_cli` is the display-independent entry point to the same validated
project backend used by editor subprocesses. It does not initialize GTK and it
does not require `DISPLAY` or `WAYLAND_DISPLAY`. The GTK executable also enters
this path before GTK initialization when its first arguments contain
`--headless`.

The CLI never writes either source ROM. Current writes are limited to validated
project-owned drafts, placements, entity documents, story TOML, native room
overrides and explicitly requested private reports. ROM-derived workrooms and
all private overrides remain under the ignored `assets/extracted/` tree.

## Invocation

```sh
./build/fusion_editor_cli --command=project-info --format=json
./build/fusion_map_editor --headless --command=list-worlds --format=json
python3 scripts/editor_cli.py --command=room-audit \
  --world=zero_mission --workers=4 --report=zero_mission_ci --format=text
```

Both `--option value` and `--option=value` are accepted. Commands reject fields
that are not part of their contract. Boolean values are exactly `true` or
`false`.

Common options:

- `--command=<name>`: execute one operation;
- `--batch=<path>`: execute a bounded JSON operation list instead;
- `--format=json|text|tsv`: select the stable machine envelope, concise output,
  or the bounded row transport used by GTK lists;
- `--dry-run=true|false`: validate without persistent project writes;
- `--atomic=true|false`: require an atomic batch contract;
- `--output=<path>`: atomically copy the CLI response to a non-symlink path;
- `--root=<path>`: select an existing non-symlink project root;
- `--verbose=true|false`: reserved for additional diagnostics;
- `--version`: print the backend interface version.

Atomic batches containing persistent mutations currently fail closed. The same
batch is accepted with `--dry-run=true` because no project document is written.

## Commands and capability state

| Command | State | Important options |
|---|---|---|
| `capabilities` | Available | none |
| `project-info` | Available | none |
| `project-validate` | Available | none |
| `project-audit` | Available | `workers` |
| `list-worlds` | Available | none |
| `list-areas` | Available | `world` |
| `list-rooms`, `room-list` | Available | `world`, `area`, `source` |
| `list-assets` | Native reference metadata | optional `world` |
| `room-inspect` | Available | `source`, `world`, `area`, `room` |
| `room-open` | Private native workroom | `world`, `area`, `room` |
| `room-create` | Draft only | `world`, `area`, `slug`, `name`, dimensions |
| `room-validate` | Draft only | `room` draft identity |
| `room-render` | Partial native diagnostic | `world`, `area`, `room` |
| `room-audit` | Partial native diagnostic | `world`, `area`, `workers`, `report` |
| `room-place`, `room-move` | Draft only | `room`, `x`, `y` |
| `room-unplace` | Draft only | `room`, `confirm=true` |
| `placement-list` | Draft only | optional `world` |
| `layer-list`, `tile-get` | Private native workroom | room identity, layer/coordinates |
| `tile-set`, `tile-fill` | Private native override | room identity, layer/coordinates, `tile-id` |
| `entity-list`, `entity-inspect` | Project markers | room scope and geometry |
| `entity-create`, `entity-move`, `entity-update`, `entity-assign` | Project markers | room scope and entity fields |
| `entity-delete` | Project markers | room scope, `id`, `confirm=true` |
| `entity-catalog`, `entity-item-settings` | Native references/project metadata | world, kind/entity scope |
| `story-validate`, `story-save` | Project data | `kind`, `input`, and save `target` |
| `collision-capabilities` | Project/native reference metadata | `world` |
| `collision-list`, `collision-get`, `collision-validate` | Project room data | room scope and optional cell |
| `collision-set`, `collision-fill`, `collision-stroke`, `collision-clear` | Project room data | cell/rectangle/freehand points, semantic type, confirmation for clear |
| `door-list`, `door-inspect` | Project room data | room scope and optional `id` |
| `door-create`, `door-update`, `door-delete` | Project room data | geometry, type, facing; confirmation for delete |
| `door-link`, `transition-create`, `transition-update` | Project room data | source door and validated native destination |
| `transition-list`, `transition-validate`, `transition-delete` | Project room data | room scope; confirmation for delete |
| native collision/door encoding and object-definition/event/cutscene authoring | Unavailable | engine schema or extraction pending |
| audio inventory/rendering | Unavailable | native inventory/decoder pending |
| native gameplay | Unavailable | both engine adapters pending |

Native BG1/BG2 commands require a workroom created by `room-open`. `tile-set`
and `tile-fill` write only the ignored project override; `tile-set --tile-id=0`
is the headless equivalent of the GTK eraser. The 255-cell per-axis limit and
6,144-cell per-layer limit are enforced by the shared C parser. BG3, collision
and engine export are not implied by these visual workroom operations.

## Current interface parity

| Operation | Backend/CLI | GTK | Zero Mission | Aria |
|---|---|---|---|---|
| Project validation and exhaustive audit | Yes | Yes | Yes | Yes |
| Native room list, inspect, render and open | Yes | Yes | Partial decoder | Partial decoder |
| Draft create/list and global-map placement | Yes | Yes | Project data | Project data |
| BG1/BG2 tile read, draw, erase and fill | Yes | Same C core | Private override | Private override |
| Native object catalog | Read-only metadata | Read-only metadata | Yes | Yes |
| Project entity create/update/move/assign/delete | Yes | Yes | Label, 16px position, native reference | Label, 8px position, native reference + item fields |
| Project collision authoring | Full cell/rectangle/stroke commands + capability matrix | Seven freehand brushes + cell context actions | 16px semantic cells | 8px semantic cells |
| Project doors | Full CRUD + destination links | Full property form + saved target picker | Project data only | Project data only |
| Project transitions | Full CRUD + target validation | Destination, unlink and return-link planning | Native target references | Native target references |
| Timeline/cutscene TOML validate and save | Yes | Yes | Shared project data | Shared project data |
| Native collision, door and entity encoding | Unavailable | Native overlays remain read-only | Pending | Pending |
| Audio inventory, decoding and playback | Unavailable | Unavailable | Pending | Pending |

The table describes real current behavior, not the target feature list. A GUI
control that changes project data must use the backend contract, or in the case
of the interactive tile canvas, the exact same C core exposed by the backend.

Project room authoring uses one versioned private document per native workroom.
Version 2 groups entity markers, sparse semantic collision cells, doors and
transitions instead of multiplying room-side files. Version 1 entity-only files
are migrated in memory and rewritten only after the next explicit save.

Collision coordinates are cell coordinates: 16px in Zero Mission workrooms and
8px in Aria workrooms. Accepted project semantics are `solid`, `one_way`,
`hazard`, `slope_up`, `slope_down`, `water` and `air`; absence means `empty`.
Explicit `air` masks a native collision cell without changing the ROM, while
`collision-clear` removes the project override and reveals native provenance
again. These values are authoring intent only. They do not claim equivalence
with either ROM's native
collision bytes and are not playable until the corresponding engine adapter is
implemented.

`collision-capabilities --world=<world>` makes cross-world parity explicit.
All seven project semantics are authorable in both modes. Its native references
confirm solid, pass-through platforms, damage, floor slopes, water and air in
both source formats, while `native_encoder` remains `unavailable`. A verified
native reference is not a claim that the editor can rewrite either ROM.

```sh
./build/fusion_editor_cli --command=collision-fill \
  --world=zero_mission --area=Brinstar --room=3 --width=304 --height=2144 \
  --x=2 --y=4 --fill-width=6 --fill-height=1 --type=solid --format=json

./build/fusion_editor_cli --command=collision-stroke \
  --world=zero_mission --area=Brinstar --room=3 --width=304 --height=2144 \
  --points='2,4;3,4;4,5' --type=water --format=json

./build/fusion_editor_cli --command=door-create \
  --world=zero_mission --area=Brinstar --room=3 --width=304 --height=2144 \
  --x=0 --y=64 --door-width=16 --door-height=32 \
  --label="Project gate" --door-type=portal --facing=left --format=json

./build/fusion_editor_cli --command=door-link \
  --world=zero_mission --area=Brinstar --room=3 --width=304 --height=2144 \
  --source-door-id=1 --target-world=aria --target-area=0 --target-room=2 \
  --target-door-id=0 --spawn-x=32 --spawn-y=48 --format=json
```

Transition creation verifies that the referenced native target room exists.
`target-door-id=0` explicitly means unspecified. Non-zero target door identities
remain marked unverified until the target room also has project geometry; the
engine adapter remains unavailable in either case.

Example native workroom inspection and non-persistent edit validation:

```sh
./build/fusion_editor_cli --command=room-open \
  --world=zero_mission --area=Brinstar --room=3 --format=json
./build/fusion_editor_cli --command=layer-list \
  --world=zero_mission --area=Brinstar --room=3 --format=json
./build/fusion_editor_cli --command=tile-set \
  --world=zero_mission --area=Brinstar --room=3 \
  --layer=bg1 --x=12 --y=8 --tile-id=42 --dry-run=true --format=json
```

`room-audit` runs the real bounded room decoders without writing hundreds of
preview bitmaps. Supplying a safe lowercase `report` identifier persists the
detailed JSON result under `assets/extracted/audits/<identifier>.json`. These
reports contain ROM-derived metadata and remain private and ignored.

## Batch format

Batch files are non-symlink JSON files no larger than 1 MiB and contain one to
100 operations:

```json
{
  "operations": [
    {"command": "project-info"},
    {"command": "list-areas", "world": "aria"}
  ]
}
```

Unknown top-level fields, missing commands and unknown operation fields are
errors. The current backend does not promise rollback for a non-atomic batch.

## JSON envelope and exit codes

Success responses use:

```json
{"success":true,"command":"project-info","data":{},"warnings":[],"created_paths":[]}
```

Failures use the same top-level structure plus an `error` object with `type`
and `message`. Diagnostics are also written to standard error.

| Exit code | Meaning |
|---:|---|
| 0 | Operation succeeded |
| 2 | CLI usage or option contract error |
| 3 | Known capability is unavailable |
| 4 | Valid request failed during the operation |
| 127 | Native launcher could not start Python |

The backend interface is versioned independently from authored data schemas.
Scripts should inspect `backend_version` and `capabilities` instead of assuming
that an unavailable editor feature is implemented.
