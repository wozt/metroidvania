# Editor CLI reference

## Product boundary

`fusion_editor_cli` is the display-independent entry point to the same validated
project backend used by editor subprocesses. It does not initialize GTK and it
does not require `DISPLAY` or `WAYLAND_DISPLAY`. The GTK executable also enters
this path before GTK initialization when its first arguments contain
`--headless`.

The CLI never writes either source ROM. Current writes are limited to validated,
project-owned room drafts and explicitly requested private audit reports under
the ignored `assets/extracted/` tree.

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
- `--format=json|text`: select the stable machine envelope or concise output;
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
| `list-worlds` | Available | none |
| `list-areas` | Available | `world` |
| `list-rooms`, `room-list` | Available | `world`, `area`, `source` |
| `room-inspect` | Available | `source`, `world`, `area`, `room` |
| `room-create` | Draft only | `world`, `area`, `slug`, `name`, dimensions |
| `room-validate` | Draft only | `room` draft identity |
| `room-render` | Partial native diagnostic | `world`, `area`, `room` |
| `room-audit` | Partial native diagnostic | `world`, `area`, `workers`, `report` |
| tile/collision/door/object/event/cutscene mutation | Unavailable | backend schema or extraction pending |
| audio inventory/rendering | Unavailable | native inventory/decoder pending |
| native gameplay | Unavailable | both engine adapters pending |

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
