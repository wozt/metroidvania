#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Run the dependency-aware native reconstruction tasks available today."""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
from typing import Callable

from scripts import native_inventory, native_parity

ROOT = Path(__file__).resolve().parents[1]
STATE_PATH = ROOT / ".cache/rebuild/state.json"
STATE_SCHEMA = "metroidvania-rebuild-state-v1"


@dataclass(frozen=True)
class Task:
    name: str
    version: int
    dependencies: tuple[str, ...]
    inputs: Callable[[], list[Path]]
    outputs: tuple[Path, ...] | Callable[[], list[Path]]
    action: Callable[[], None]


def _source_inputs() -> list[Path]:
    paths = [Path(native_inventory.__file__).resolve()]
    for configuration in native_inventory.GAMES.values():
        source_root = ROOT / configuration["path"]
        paths.extend(
            path for path in source_root.rglob("*")
            if path.is_file() and path.suffix.lower() in native_inventory.SOURCE_SUFFIXES)
    return sorted(set(paths))


def _checklist_inputs() -> list[Path]:
    return sorted({
        Path(native_parity.__file__).resolve(),
        native_parity.ANNOTATIONS,
        *native_inventory.inventory_output_paths(native_parity.INVENTORY),
    })


def _write_inventory() -> None:
    native_inventory.write_inventory(
        native_inventory.build_inventory(ROOT), native_inventory.DEFAULT_OUTPUT)


def _write_checklist() -> None:
    inventory = native_parity._load(native_parity.INVENTORY)
    annotations = native_parity._load(native_parity.ANNOTATIONS)
    native_parity.validate(inventory, annotations, ROOT)
    native_parity.write_checklist(
        native_parity.render(inventory, annotations), native_parity.CHECKLIST)


TASKS = {
    "inventory": Task(
        name="inventory",
        version=2,
        dependencies=(),
        inputs=_source_inputs,
        outputs=lambda: native_inventory.inventory_output_paths(
            native_inventory.DEFAULT_OUTPUT),
        action=_write_inventory,
    ),
    "checklist": Task(
        name="checklist",
        version=3,
        dependencies=("inventory",),
        inputs=_checklist_inputs,
        outputs=(native_parity.CHECKLIST,),
        action=_write_checklist,
    ),
}


def _task_outputs(task: Task) -> list[Path]:
    return list(task.outputs() if callable(task.outputs) else task.outputs)


def _digest(task: Task, dependency_digests: list[str]) -> str:
    digest = hashlib.sha256()
    digest.update(f"{task.name}:{task.version}\n".encode("utf-8"))
    for dependency_digest in dependency_digests:
        digest.update(dependency_digest.encode("ascii"))
        digest.update(b"\n")
    for path in task.inputs():
        if not path.is_file():
            raise ValueError(f"{task.name}: missing input: {path}")
        try:
            relative = path.relative_to(ROOT).as_posix()
        except ValueError:
            relative = path.as_posix()
        digest.update(relative.encode("utf-8"))
        digest.update(b"\0")
        digest.update(path.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()


def _load_state(path: Path) -> dict:
    if not path.exists():
        return {"schema": STATE_SCHEMA, "tasks": {}}
    try:
        state = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ValueError(f"cannot read rebuild state: {exc}") from exc
    if state.get("schema") != STATE_SCHEMA or not isinstance(state.get("tasks"), dict):
        raise ValueError("unsupported rebuild state schema")
    return state


def _write_state(path: Path, state: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(
        json.dumps(state, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    temporary.replace(path)


def _ordered(selected: set[str]) -> list[str]:
    result = []
    visiting = set()

    def visit(name: str) -> None:
        if name in result:
            return
        if name in visiting:
            raise ValueError(f"rebuild task dependency cycle at {name}")
        visiting.add(name)
        for dependency in TASKS[name].dependencies:
            visit(dependency)
        visiting.remove(name)
        result.append(name)

    for name in sorted(selected):
        visit(name)
    return result


def rebuild(selected: set[str], *, force: bool = False,
            dry_run: bool = False, state_path: Path = STATE_PATH) -> list[tuple[str, str]]:
    unknown = selected - TASKS.keys()
    if unknown:
        raise ValueError(f"unknown rebuild task(s): {', '.join(sorted(unknown))}")
    state = _load_state(state_path)
    results = []
    current_digests = {}
    changed = False
    for name in _ordered(selected):
        task = TASKS[name]
        dependency_digests = [current_digests[item] for item in task.dependencies]
        digest = _digest(task, dependency_digests)
        current_digests[name] = digest
        previous = state["tasks"].get(name, {}).get("digest")
        outputs = _task_outputs(task)
        outputs_exist = all(path.is_file() for path in outputs)
        run = force or previous != digest or not outputs_exist
        if run and not dry_run:
            task.action()
            outputs = _task_outputs(task)
            if not all(path.is_file() for path in outputs):
                raise ValueError(f"{name}: task completed without all declared outputs")
            state["tasks"][name] = {
                "digest": digest,
                "outputs": [path.relative_to(ROOT).as_posix() for path in outputs],
                "version": task.version,
            }
            changed = True
        results.append((name, "would rebuild" if run and dry_run
                        else "rebuilt" if run else "up to date"))
    if changed:
        _write_state(state_path, state)
    return results


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument("--all", action="store_true",
                           help="run every currently registered reconstruction task")
    selection.add_argument("--inventory", action="store_true",
                           help="rebuild only the source inventory")
    selection.add_argument("--checklist", action="store_true",
                           help="rebuild the checklist and its dependencies")
    parser.add_argument("--force", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--list", action="store_true")
    args = parser.parse_args(argv)
    if args.list:
        for task in TASKS.values():
            dependencies = ", ".join(task.dependencies) or "none"
            print(f"{task.name}: dependencies={dependencies}")
        return 0
    selected = ({"inventory"} if args.inventory else {"checklist"}
                if args.checklist else set(TASKS))
    try:
        results = rebuild(selected, force=args.force, dry_run=args.dry_run)
    except (OSError, ValueError) as exc:
        parser.error(str(exc))
    for name, status in results:
        print(f"{name}: {status}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
