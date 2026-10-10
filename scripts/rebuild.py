#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Run the dependency-aware reconstruction tasks available today.

Native tasks (``inventory``, ``checklist``) read only the pinned
decompilations and write tracked, redistributable files. Asset tasks read the
user's ROMs and write private files below the ignored ``assets/extracted/``;
each one runs an existing extractor unchanged. A task whose ROM (or other
required local file) is missing is skipped, and so are the tasks depending on
it, so the native tasks keep working on a ROM-free checkout.
"""
from __future__ import annotations

import argparse
import ast
from dataclasses import dataclass, field
import hashlib
import json
from pathlib import Path
import subprocess
import sys
from typing import Callable

from scripts import export_index, native_inventory, native_parity, verify_roms
from scripts.asset_layout import (
    ARIA_METADATA, ARIA_ROOMS, ARIA_SOMA_RUNTIME, ARIA_SPRITES, IMPORT_CATALOG,
    METROID_PROJECTILES_RUNTIME, METROID_ROOMS, METROID_SAMUS_RUNTIME, SHARED_MANIFESTS,
    private_path)
from scripts.sprite_library import write_atomic

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
    # Local files the task cannot run without (ROMs, a built ELF): when one is
    # missing the task and its dependents are skipped, not failed.
    requires: tuple[Path, ...] = ()
    # Extra digest material, such as the pinned decompilation revisions.
    stamps: Callable[[], list[str]] = field(default=lambda: [])
    # Problems with the current outputs beyond their recorded digest (missing
    # or modified files listed by an export index); any problem reruns it.
    validate: Callable[[], list[str]] | None = None
    group: str = "native"


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


ROMS = {
    "aria": ROOT / "roms/Castlevania - Aria of Sorrow (USA).gba",
    "metroid": ROOT / "roms/Metroid - Zero Mission (USA).gba",
}
DECOMPS = {"aria": ROOT / "third_party/cvaos", "metroid": ROOT / "third_party/mzm"}
MZM_ELF = ROOT / "third_party/mzm/mzm_us.elf"


def _module_sources(module: str) -> list[Path]:
    """The extractor module and every ``scripts`` module it imports."""
    seen = set()
    stack = [module]
    while stack:
        name = stack.pop()
        path = ROOT / "scripts" / f"{name}.py"
        if name in seen or not path.is_file():
            continue
        seen.add(name)
        for node in ast.walk(ast.parse(path.read_text(encoding="utf-8"))):
            if isinstance(node, ast.ImportFrom) and node.module == "scripts":
                stack.extend(alias.name for alias in node.names)
            elif isinstance(node, ast.ImportFrom) and (node.module or "").startswith("scripts."):
                stack.append(node.module.split(".", 1)[1])
            elif isinstance(node, ast.Import):
                stack.extend(alias.name.split(".", 1)[1] for alias in node.names
                             if alias.name.startswith("scripts."))
    return sorted(ROOT / "scripts" / f"{name}.py" for name in seen)


def _decomp_stamp(game: str) -> list[str]:
    """Pinned revision and tracked-file state of a decompilation."""
    path = DECOMPS[game]
    revision = subprocess.run(["git", "-C", str(path), "rev-parse", "HEAD"],
                              check=True, capture_output=True, text=True).stdout.strip()
    status = subprocess.run(["git", "-C", str(path), "status", "--porcelain",
                             "--untracked-files=no"],
                            check=True, capture_output=True, text=True).stdout
    return [f"{game}:{revision}", hashlib.sha256(status.encode("utf-8")).hexdigest()]


def _run_module(module: str, *arguments: str) -> Callable[[], None]:
    def action() -> None:
        result = subprocess.run([sys.executable, "-m", f"scripts.{module}", *arguments],
                                cwd=ROOT, check=False)
        if result.returncode:
            raise ValueError(f"scripts.{module} failed with exit code {result.returncode}")
    return action


def _verify_rom(game: str) -> Callable[[], None]:
    def action() -> None:
        valid, message = verify_roms.validate(game, ROMS[game])
        if not valid:
            raise ValueError(message)
        output = private_path(ROOT, SHARED_MANIFESTS / f"rom_{game}.json", create=True)
        label, sha1 = verify_roms.EXPECTED[game]
        write_atomic(output, json.dumps({"game": label, "sha1": sha1,
                                         "file": ROMS[game].name}, indent=2) + "\n")
    return action


def _sprite_validator(*indexes: Path) -> Callable[[], list[str]]:
    return lambda: [problem for index in indexes
                    for problem in export_index.sprite_library_problems(index)]


def _export_validator(index: Path) -> Callable[[], list[str]]:
    return lambda: export_index.problems(index)


def _asset_task(name: str, game: str, module: str, arguments: tuple[str, ...],
                outputs: tuple[Path, ...], dependencies: tuple[str, ...],
                validate: Callable[[], list[str]] | None = None,
                requires: tuple[Path, ...] = (), version: int = 1) -> Task:
    games = ("aria", "metroid") if game == "both" else (game,)
    required = tuple(ROMS[item] for item in games) + requires
    return Task(
        name=name, version=version, dependencies=dependencies,
        inputs=lambda: sorted({*_module_sources(module), *required}),
        outputs=outputs, action=_run_module(module, *arguments), requires=required,
        stamps=lambda: [stamp for item in games for stamp in _decomp_stamp(item)],
        validate=validate, group="assets")


ARIA_ROOM_INDEX = ROOT / ARIA_ROOMS / "runtime" / export_index.NAME
MZM_ROOM_INDEX = ROOT / METROID_ROOMS / "runtime" / export_index.NAME
ARIA_OBJECTS = ROOT / ARIA_SPRITES / "objects/runtime/runtime_index.tsv"
ARIA_WEAPONS = ROOT / ARIA_SPRITES / "weapons/runtime/runtime_index.tsv"
ARIA_SOMA = ROOT / ARIA_SOMA_RUNTIME / "runtime_index.tsv"
MZM_SAMUS = ROOT / METROID_SAMUS_RUNTIME / "runtime_index.tsv"
MZM_PROJECTILES = ROOT / METROID_PROJECTILES_RUNTIME / "runtime_index.tsv"

ASSET_TASKS = {
    "aria_rom": Task(
        name="aria_rom", version=1, dependencies=(),
        inputs=lambda: [ROMS["aria"], *_module_sources("verify_roms")],
        outputs=(ROOT / SHARED_MANIFESTS / "rom_aria.json",), action=_verify_rom("aria"),
        requires=(ROMS["aria"],), group="assets"),
    "metroid_rom": Task(
        name="metroid_rom", version=1, dependencies=(),
        inputs=lambda: [ROMS["metroid"], *_module_sources("verify_roms")],
        outputs=(ROOT / SHARED_MANIFESTS / "rom_metroid.json",),
        action=_verify_rom("metroid"), requires=(ROMS["metroid"],), group="assets"),
    "raw_import": _asset_task(
        "raw_import", "both", "import_game_assets", ("--scope", "all"),
        (ROOT / IMPORT_CATALOG,), ("aria_rom", "metroid_rom")),
    "aria_world": _asset_task(
        "aria_world", "aria", "import_aos_world", (),
        (ROOT / "assets/extracted/rooms/aria/world.json",), ("aria_rom",)),
    "aria_rooms": _asset_task(
        "aria_rooms", "aria", "aos_runtime_room", ("--all",), (ARIA_ROOM_INDEX,),
        ("aria_rom",), _export_validator(ARIA_ROOM_INDEX)),
    "aria_soma": _asset_task(
        "aria_soma", "aria", "aos_soma_pipeline", (), (ARIA_SOMA,), ("aria_rom",),
        _sprite_validator(ARIA_SOMA)),
    "aria_objects": _asset_task(
        "aria_objects", "aria", "aos_object_sprites", (),
        (ARIA_OBJECTS, ROOT / ARIA_METADATA / "enemy_frames.tsv",
         ROOT / ARIA_METADATA / "enemies.tsv"), ("aria_rom",),
        _sprite_validator(ARIA_OBJECTS)),
    "aria_weapons": _asset_task(
        "aria_weapons", "aria", "aos_weapons", (),
        (ARIA_WEAPONS, ROOT / ARIA_METADATA / "weapons.tsv",
         ROOT / ARIA_METADATA / "weapon_frames.tsv"), ("aria_rom",),
        _sprite_validator(ARIA_WEAPONS)),
    "metroid_samus": _asset_task(
        "metroid_samus", "metroid", "mzm_samus_pipeline", (),
        (MZM_SAMUS, MZM_PROJECTILES, ROOT / METROID_SAMUS_RUNTIME / "animation_map.tsv"),
        ("metroid_rom",), _sprite_validator(MZM_SAMUS, MZM_PROJECTILES), requires=(MZM_ELF,)),
    "metroid_rooms": _asset_task(
        "metroid_rooms", "metroid", "mzm_runtime_room", ("--all",), (MZM_ROOM_INDEX,),
        ("raw_import",), _export_validator(MZM_ROOM_INDEX)),
}
TASKS.update(ASSET_TASKS)


def _task_outputs(task: Task) -> list[Path]:
    return list(task.outputs() if callable(task.outputs) else task.outputs)


def _digest(task: Task, dependency_digests: list[str]) -> str:
    digest = hashlib.sha256()
    digest.update(f"{task.name}:{task.version}\n".encode("utf-8"))
    for dependency_digest in dependency_digests:
        digest.update(dependency_digest.encode("ascii"))
        digest.update(b"\n")
    for stamp in task.stamps():
        digest.update(stamp.encode("utf-8"))
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


def _output_digest(outputs: list[Path]) -> str | None:
    """Hash of the declared outputs, or None when one is missing."""
    digest = hashlib.sha256()
    for path in sorted(outputs):
        if not path.is_file():
            return None
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
    pending = set()
    skipped = set()
    for name in _ordered(selected):
        task = TASKS[name]
        missing = [path for path in task.requires if not path.is_file()]
        if missing or skipped.intersection(task.dependencies):
            skipped.add(name)
            reason = (f"missing {missing[0].relative_to(ROOT).as_posix()}" if missing
                      else "dependency skipped")
            results.append((name, f"skipped ({reason})"))
            continue
        if dry_run and pending.intersection(task.dependencies):
            # A dependency would be regenerated first, so this task's inputs
            # are not final yet (they may even be missing).
            pending.add(name)
            current_digests[name] = ""
            results.append((name, "would rebuild"))
            continue
        dependency_digests = [current_digests[item] for item in task.dependencies]
        digest = _digest(task, dependency_digests)
        current_digests[name] = digest
        recorded = state["tasks"].get(name, {})
        outputs = _task_outputs(task)
        output_digest = _output_digest(outputs)
        run = (force or recorded.get("digest") != digest or output_digest is None
               or recorded.get("output_digest") != output_digest
               or bool(task.validate and task.validate()))
        if run:
            pending.add(name)
        if run and not dry_run:
            task.action()
            outputs = _task_outputs(task)
            output_digest = _output_digest(outputs)
            if output_digest is None:
                raise ValueError(f"{name}: task completed without all declared outputs")
            problems = task.validate() if task.validate else []
            if problems:
                raise ValueError(f"{name}: invalid outputs after the run: {problems[0]}")
            state["tasks"][name] = {
                "digest": digest,
                "output_digest": output_digest,
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
                           help="run every registered task (asset tasks without their ROM "
                                "are skipped)")
    selection.add_argument("--native", action="store_true",
                           help="run the ROM-free native tasks (inventory, checklist)")
    selection.add_argument("--assets", action="store_true",
                           help="run the private asset tasks that read the ROMs")
    selection.add_argument("--inventory", action="store_true",
                           help="rebuild only the source inventory")
    selection.add_argument("--checklist", action="store_true",
                           help="rebuild the checklist and its dependencies")
    selection.add_argument("--task", action="append", metavar="NAME",
                           help="run one task and its dependencies (repeatable)")
    parser.add_argument("--force", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--list", action="store_true")
    args = parser.parse_args(argv)
    if args.list:
        for task in TASKS.values():
            dependencies = ", ".join(task.dependencies) or "none"
            requires = ", ".join(path.relative_to(ROOT).as_posix() for path in task.requires)
            print(f"{task.name}: group={task.group} dependencies={dependencies}"
                  + (f" requires={requires}" if requires else ""))
        return 0
    if args.inventory:
        selected = {"inventory"}
    elif args.checklist:
        selected = {"checklist"}
    elif args.task:
        selected = set(args.task)
    elif args.native or args.assets:
        group = "native" if args.native else "assets"
        selected = {name for name, task in TASKS.items() if task.group == group}
    else:
        selected = set(TASKS)
    try:
        results = rebuild(selected, force=args.force, dry_run=args.dry_run)
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        parser.error(str(exc))
    for name, status in results:
        print(f"{name}: {status}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
