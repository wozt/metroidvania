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
| `entity-create`, `entity-move`, `entity-assign` | Project markers | room scope and entity fields |
| `entity-delete` | Project markers | room scope, `id`, `confirm=true` |
| `entity-catalog`, `entity-item-settings` | Native references/project metadata | world, kind/entity scope |
| `story-validate`, `story-save` | Project data | `kind`, `input`, and save `target` |
| collision/door/object-definition/event/cutscene authoring | Unavailable | backend schema or extraction pending |
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
| Project entity create/move/assign/delete | Yes | Yes | Markers only | Markers + item fields |
| Timeline/cutscene TOML validate and save | Yes | Yes | Shared project data | Shared project data |
| Collision, doors and native entity encoding | Unavailable | Read-only overlays | Pending | Pending |
| Audio inventory, decoding and playback | Unavailable | Unavailable | Pending | Pending |

The table describes real current behavior, not the target feature list. A GUI
control that changes project data must use the backend contract, or in the case
of the interactive tile canvas, the exact same C core exposed by the backend.

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
