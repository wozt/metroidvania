#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Headless command-line interface for the shared Metroidvania editor backend."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import sys
import tempfile
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from scripts.editor_backend import (  # noqa: E402
    BACKEND_VERSION,
    COMMAND_FIELDS,
    MUTATING_COMMANDS,
    execute,
)

EXIT_SUCCESS = 0
EXIT_USAGE = 2
EXIT_UNAVAILABLE = 3
EXIT_OPERATION = 4
TSV_COMMANDS = {
    "room-list", "list-rooms", "placement-list", "entity-list",
    "entity-catalog", "entity-item-settings", "list-assets",
}


class CliUsageError(ValueError):
    pass


class StrictParser(argparse.ArgumentParser):
    def error(self, message: str) -> None:
        raise CliUsageError(message)


def _symlink_component(path: Path, *, include_leaf: bool = True) -> Path | None:
    absolute = path if path.is_absolute() else Path.cwd() / path
    parts = absolute.parts[1:] if include_leaf else absolute.parts[1:-1]
    current = Path(absolute.anchor)
    for part in parts:
        current /= part
        if current.is_symlink():
            return current
    return None


def _boolean(value: str) -> bool:
    if value == "true":
        return True
    if value == "false":
        return False
    raise argparse.ArgumentTypeError("expected true or false")


def _parser() -> argparse.ArgumentParser:
    parser = StrictParser(description=__doc__)
    parser.add_argument("--version", action="version", version=BACKEND_VERSION)
    parser.add_argument("--command")
    parser.add_argument("--batch", type=Path)
    parser.add_argument("--atomic", type=_boolean, default=False)
    parser.add_argument("--format", choices=("json", "text", "tsv"), default="text")
    parser.add_argument("--verbose", type=_boolean, default=False)
    parser.add_argument("--dry-run", type=_boolean, default=False)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--world")
    parser.add_argument("--area")
    parser.add_argument("--room")
    parser.add_argument("--source")
    parser.add_argument("--slug")
    parser.add_argument("--name")
    parser.add_argument("--width-screens", dest="width_screens")
    parser.add_argument("--height-screens", dest="height_screens")
    parser.add_argument("--workers")
    parser.add_argument("--report")
    parser.add_argument("--x")
    parser.add_argument("--y")
    parser.add_argument("--width")
    parser.add_argument("--height")
    parser.add_argument("--id")
    parser.add_argument("--kind")
    parser.add_argument("--label")
    parser.add_argument("--native-type", dest="native_type")
    parser.add_argument("--item-id", dest="item_id")
    parser.add_argument("--parameter-0", dest="parameter_0")
    parser.add_argument("--parameter-1", dest="parameter_1")
    parser.add_argument("--flags")
    parser.add_argument("--preview", type=_boolean)
    parser.add_argument("--confirm", type=_boolean)
    parser.add_argument("--input")
    parser.add_argument("--target")
    parser.add_argument("--layer")
    parser.add_argument("--tile-id", dest="tile_id")
    return parser


def _clean_options(values: dict[str, Any]) -> dict[str, Any]:
    common = {"command", "batch", "atomic", "format", "verbose", "dry_run",
              "output", "root"}
    return {key: value for key, value in values.items()
            if key not in common and value is not None}


def _read_batch(path: Path) -> list[dict[str, Any]]:
    symlink = _symlink_component(path)
    if (".." in path.parts or symlink is not None or not path.is_file() or
            path.stat().st_size > 1024 * 1024):
        raise CliUsageError("batch must be a non-symlink JSON file no larger than 1 MiB")
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise CliUsageError(f"invalid batch JSON: {exc}") from exc
    if not isinstance(document, dict) or set(document) != {"operations"}:
        raise CliUsageError("batch must contain only an operations array")
    operations = document["operations"]
    if not isinstance(operations, list) or not 1 <= len(operations) <= 100:
        raise CliUsageError("batch operations must contain 1..100 entries")
    if any(not isinstance(item, dict) for item in operations):
        raise CliUsageError("every batch operation must be an object")
    return operations


def _run_operation(operation: dict[str, Any], root: Path, dry_run: bool) -> dict:
    if "command" not in operation or not isinstance(operation["command"], str):
        raise CliUsageError("operation command is required")
    command = operation["command"]
    options = {key: value for key, value in operation.items() if key != "command"}
    return execute(command, options, root=root, dry_run=dry_run)


def _safe_output(path: Path, payload: str) -> None:
    if ".." in path.parts:
        raise CliUsageError("output path traversal refused")
    absolute = path if path.is_absolute() else Path.cwd() / path
    symlink = _symlink_component(absolute)
    if symlink is not None:
        raise CliUsageError(f"symlink output path refused: {symlink}")
    absolute.parent.mkdir(parents=True, exist_ok=True)
    temporary: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
                mode="w", encoding="utf-8", dir=absolute.parent,
                prefix=f".{absolute.name}.", suffix=".tmp", delete=False) as handle:
            handle.write(payload)
            handle.flush()
            os.fsync(handle.fileno())
            temporary = Path(handle.name)
        temporary.replace(absolute)
    finally:
        if temporary is not None and temporary.exists():
            temporary.unlink()


def _text_result(result: dict) -> str:
    command = result["command"]
    if result["success"]:
        data = result["data"]
        if command == "room-audit":
            return (f"{command}: {data['room_count']} rooms; " + ", ".join(
                f"{key}={value}" for key, value in data["summary"].items()))
        return f"{command}: success\n{json.dumps(data, indent=2, ensure_ascii=False)}"
    return f"{command}: {result['error']['message']}"


def _safe_tsv_field(value: Any) -> str:
    field = str(value)
    if len(field) > 1000 or any(character in field for character in ("\t", "\n", "\r")):
        raise CliUsageError("unsafe TSV field returned by backend")
    return field


def _tsv_result(command: str, data: dict) -> str:
    rows: list[list[Any]]
    if command in ("room-list", "list-rooms"):
        if data["source"] != "draft":
            raise CliUsageError("TSV room listing is available only for draft rooms")
        rows = [[room["id"], room["name"], room["geometry"]["width_screens"],
                 room["geometry"]["height_screens"]] for room in data["rooms"]]
    elif command == "placement-list":
        rows = [[item["id"], item["name"], item["world"], item["area"],
                 item["x"], item["y"], item["width_screens"],
                 item["height_screens"]] for item in data["placements"]]
    elif command == "entity-list":
        rows = []
        for entity in data["entities"]:
            row = [entity["id"], entity["kind"], entity["x"], entity["y"],
                   entity["label"], entity["native_type"]]
            if data["preview"]:
                row.append(entity.get("settings", {}).get("item_id", -1))
            rows.append(row)
    elif command == "entity-catalog":
        rows = [[item["native_type"], item["name"], item["category"],
                 item.get("item_id", -1)] for item in data["definitions"]]
    elif command == "entity-item-settings":
        settings = data["settings"]
        rows = [[settings.get("item_id", 0), settings.get("parameter_0", 0),
                 settings.get("parameter_1", 0), settings.get("flags", 0)]]
    elif command == "list-assets":
        rows = [[item["world"], item["native_id"], item["native_type"],
                 item["name"], item["category"], item["summary"],
                 item["placements"] if item["placements"] is not None else "-",
                 1 if item["editable"] else 0] for item in data["assets"]]
    else:
        raise CliUsageError(f"TSV output is unavailable for {command}")
    return "".join("\t".join(_safe_tsv_field(field) for field in row) + "\n"
                   for row in rows)


def main(argv: list[str] | None = None) -> int:
    parser = _parser()
    output_format = "text"
    command = "unknown"
    try:
        args = parser.parse_args(argv)
        output_format = args.format
        if bool(args.command) == bool(args.batch):
            raise CliUsageError("provide exactly one of --command or --batch")
        if args.batch:
            if output_format == "tsv":
                raise CliUsageError("TSV output is unavailable for batches")
            operations = _read_batch(args.batch)
            if args.atomic and not args.dry_run and any(
                    item.get("command") in MUTATING_COMMANDS for item in operations):
                raise NotImplementedError(
                    "atomic persistent mutations are unavailable; use --dry-run=true")
            command = "batch"
            data = {"atomic": args.atomic, "dry_run": args.dry_run, "results": [
                {"command": item.get("command"),
                 "data": _run_operation(item, args.root, args.dry_run)}
                for item in operations]}
        else:
            command = args.command
            if output_format == "tsv" and command not in TSV_COMMANDS:
                raise CliUsageError(f"TSV output is unavailable for {command}")
            options = _clean_options(vars(args))
            allowed = COMMAND_FIELDS.get(command)
            if allowed is not None:
                unknown = set(options) - allowed
                if unknown:
                    raise CliUsageError(
                        f"unknown option(s) for {command}: {', '.join(sorted(unknown))}")
            data = execute(command, options, root=args.root, dry_run=args.dry_run)
        result = {"success": True, "command": command, "data": data,
                  "warnings": [], "created_paths": []}
        if isinstance(data, dict):
            for key in ("path", "report_path"):
                if data.get(key):
                    result["created_paths"].append(data[key])
            result["created_paths"].extend(data.get("created_paths", []))
        if output_format == "json":
            payload = json.dumps(result, separators=(",", ":"), ensure_ascii=False) + "\n"
        elif output_format == "tsv":
            payload = _tsv_result(command, data)
        else:
            payload = _text_result(result) + "\n"
        if args.output:
            _safe_output(args.output, payload)
        print(payload, end="")
        return EXIT_SUCCESS
    except CliUsageError as exc:
        code = EXIT_USAGE
        error_type = "usage"
        error_message = str(exc)
    except NotImplementedError as exc:
        code = EXIT_UNAVAILABLE
        error_type = "unavailable"
        error_message = str(exc)
    except (OSError, ValueError, KeyError, IndexError, TypeError) as exc:
        code = EXIT_OPERATION
        error_type = "operation"
        error_message = str(exc)
    result = {"success": False, "command": command, "data": None,
              "warnings": [], "created_paths": [],
              "error": {"type": error_type, "message": error_message}}
    print(error_message, file=sys.stderr)
    if output_format == "json":
        print(json.dumps(result, separators=(",", ":"), ensure_ascii=False))
    else:
        print(_text_result(result))
    return code


if __name__ == "__main__":
    raise SystemExit(main())
