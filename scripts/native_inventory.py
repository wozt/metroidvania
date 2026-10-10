#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build a deterministic function inventory from the pinned decompilations."""
from __future__ import annotations

import argparse
from collections import defaultdict
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SCHEMA = "metroidvania-native-inventory-v1"
GENERATOR_VERSION = 1
DEFAULT_OUTPUT = ROOT / "data/native_parity/inventory.json"
SOURCE_SUFFIXES = {".c", ".h", ".s", ".inc"}
CALL_RE = re.compile(r"\b([A-Za-z_]\w*)\s*\(")
ASM_BRANCH_RE = re.compile(r"^\s*(?:bl|blx|b)\s+([A-Za-z_.$]\w*)", re.MULTILINE)
ASM_FUNCTION_RE = re.compile(
    r"^\s*(?:(?:thumb|arm)_func_start\s+([A-Za-z_.$]\w*)|"
    r"\.type\s+([A-Za-z_.$]\w*)\s*,\s*%?function)\s*$",
    re.MULTILINE,
)
CONTROL_WORDS = {"if", "for", "while", "switch", "return", "sizeof"}

GAMES = {
    "mzm": {
        "title": "Metroid: Zero Mission",
        "path": Path("third_party/mzm"),
    },
    "aos": {
        "title": "Castlevania: Aria of Sorrow",
        "path": Path("third_party/cvaos"),
    },
}

MZM_BOSSES = {
    "acid_worm", "boss_statues", "chozo_statue", "crocomire", "deorem",
    "imago", "imago_cocoon", "kraid", "mecha_ridley", "mother_brain",
    "ridley", "ruins_test",
}


def _mask_c(text: str) -> str:
    """Replace comments and literals with spaces while preserving line offsets."""
    result = list(text)
    index = 0
    state = "code"
    while index < len(text):
        current = text[index]
        following = text[index + 1] if index + 1 < len(text) else ""
        if state == "code" and current == "/" and following == "*":
            result[index] = result[index + 1] = " "
            state = "block_comment"
            index += 2
            continue
        if state == "code" and current == "/" and following == "/":
            result[index] = result[index + 1] = " "
            state = "line_comment"
            index += 2
            continue
        if state == "code" and current in {'"', "'"}:
            result[index] = " "
            state = "string" if current == '"' else "character"
            index += 1
            continue
        if state in {"string", "character"}:
            if current == "\\":
                result[index] = " "
                if index + 1 < len(text):
                    if text[index + 1] != "\n":
                        result[index + 1] = " "
                    index += 2
                    continue
            expected = '"' if state == "string" else "'"
            if current == expected:
                result[index] = " "
                state = "code"
            elif current != "\n":
                result[index] = " "
            index += 1
            continue
        if state == "block_comment":
            if current == "*" and following == "/":
                result[index] = result[index + 1] = " "
                state = "code"
                index += 2
                continue
            if current != "\n":
                result[index] = " "
            index += 1
            continue
        if state == "line_comment":
            if current == "\n":
                state = "code"
            else:
                result[index] = " "
            index += 1
            continue
        index += 1
    return "".join(result)


def _function_header(header: str) -> tuple[str, str] | None:
    stripped = header.rstrip()
    if not stripped.endswith(")"):
        return None
    depth = 0
    opening = None
    for index in range(len(stripped) - 1, -1, -1):
        if stripped[index] == ")":
            depth += 1
        elif stripped[index] == "(":
            depth -= 1
            if depth == 0:
                opening = index
                break
    if opening is None:
        return None
    match = re.search(r"([A-Za-z_]\w*)\s*$", stripped[:opening])
    if not match or match.group(1) in CONTROL_WORDS:
        return None
    prefix = stripped[:match.start(1)]
    if "=" in prefix or re.search(r"\btypedef\b", prefix):
        return None
    return match.group(1), prefix


def _brief_and_address(game: str, source: str, header_start: int) -> tuple[str | None, str | None]:
    comment_start = source.rfind("/**", max(0, header_start - 1200), header_start)
    if comment_start < 0:
        return None, None
    comment_end = source.find("*/", comment_start, header_start)
    if comment_end < 0:
        return None, None
    gap = source[comment_end + 2:header_start]
    if ";" in gap or "}" in gap:
        return None, None
    comment = source[comment_start:comment_end + 2]
    match = re.search(r"@brief\s+([0-9A-Fa-f]+)\s*\|\s*([^\n*]*)", comment)
    if not match:
        return None, None
    offset = int(match.group(1), 16)
    description = match.group(2).strip() or None
    if offset < 0x08000000:
        offset += 0x08000000
    return description, f"0x{offset:08X}"


def _category(game: str, relative: Path) -> str:
    path = relative.as_posix().lower()
    stem = relative.stem.lower()
    if any(token in path for token in ("m4a", "audio", "music", "sound")):
        return "audio"
    if any(token in path for token in ("save", "sram")):
        return "saves"
    if "input" in path:
        return "input"
    if game == "aos":
        if stem in {"agb_print", "agb_multi_sio_sync"}:
            return "technical"
        if path.startswith("src/data/"):
            return "data"
        return "unclassified"
    if path.startswith("src/menus/"):
        return "menus"
    if path.startswith("src/cutscenes/") or any(
            token in stem for token in ("intro", "ending", "gallery")):
        return "cutscenes"
    if path.startswith("src/sprites_ai/"):
        return "bosses" if stem in MZM_BOSSES else "enemies"
    if path.startswith("src/data/"):
        return "data"
    if "samus" in stem:
        return "player"
    if "projectile" in stem:
        return "weapons"
    if any(token in stem for token in ("room", "connection", "scroll", "clipdata", "block")):
        return "rooms"
    if any(token in stem for token in ("hud", "minimap", "display", "oam")):
        return "interface"
    if any(token in stem for token in ("event", "escape")):
        return "events"
    return "technical"


def _discover_c(game: str, root: Path, path: Path) -> list[dict]:
    source = path.read_text(encoding="utf-8", errors="replace")
    masked = _mask_c(source)
    relative = path.relative_to(root)
    routines = []
    depth = 0
    statement_start = 0
    body_start = None
    current = None
    for index, character in enumerate(masked):
        if character == "{" and depth == 0:
            parsed = _function_header(masked[statement_start:index])
            if parsed:
                symbol, prefix = parsed
                name_position = masked.rfind(symbol, statement_start, index)
                description, address = _brief_and_address(game, source, name_position)
                current = {
                    "id": f"{game}:c:{relative.as_posix()}:{symbol}",
                    "symbol": symbol,
                    "kind": "c_function",
                    "source": relative.as_posix(),
                    "line": masked.count("\n", 0, name_position) + 1,
                    "linkage": "internal" if re.search(r"\bstatic\b", prefix) else "external",
                    "category": _category(game, relative),
                    "description": description,
                    "address": address,
                    "_body_start": index + 1,
                }
                body_start = index + 1
            depth = 1
        elif character == "{" and depth > 0:
            depth += 1
        elif character == "}" and depth > 0:
            depth -= 1
            if depth == 0:
                if current is not None and body_start is not None:
                    body = masked[body_start:index]
                    current["_raw_calls"] = sorted(set(CALL_RE.findall(body)) - CONTROL_WORDS)
                    routines.append(current)
                current = None
                body_start = None
                statement_start = index + 1
        elif character == ";" and depth == 0:
            statement_start = index + 1
    return routines


def _discover_asm(game: str, root: Path, path: Path) -> list[dict]:
    source = path.read_text(encoding="utf-8", errors="replace")
    relative = path.relative_to(root)
    discovered = {}
    for match in ASM_FUNCTION_RE.finditer(source):
        symbol = match.group(1) or match.group(2)
        discovered.setdefault(symbol, match)
    functions = sorted(discovered.items(), key=lambda item: item[1].start())
    routines = []
    for index, (symbol, match) in enumerate(functions):
        label = re.search(
            rf"^\s*{re.escape(symbol)}:\s*(?:[@;].*)?$",
            source[match.end():], re.MULTILINE)
        if label is None:
            continue
        body_start = match.end() + label.end()
        body_end = functions[index + 1][1].start() if index + 1 < len(functions) else len(source)
        routines.append({
            "id": f"{game}:asm:{relative.as_posix()}:{symbol}",
            "symbol": symbol,
            "kind": "asm_function",
            "source": relative.as_posix(),
            "line": source.count("\n", 0, match.start()) + 1,
            "linkage": "external",
            "category": _category(game, relative),
            "description": None,
            "address": None,
            "_raw_calls": sorted(set(ASM_BRANCH_RE.findall(source[body_start:body_end]))),
        })
    return routines


def _revision(path: Path) -> str:
    result = subprocess.run(
        ["git", "-C", str(path), "rev-parse", "HEAD"], check=True,
        capture_output=True, text=True)
    return result.stdout.strip()


def _source_digest(paths: list[Path], root: Path) -> str:
    digest = hashlib.sha256()
    for path in paths:
        digest.update(path.relative_to(root).as_posix().encode("utf-8"))
        digest.update(b"\0")
        digest.update(path.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()


def inventory_game(game: str, root: Path) -> dict:
    paths = sorted(
        path for path in root.rglob("*")
        if path.is_file() and path.suffix.lower() in SOURCE_SUFFIXES)
    routines = []
    for path in paths:
        if path.suffix.lower() == ".c":
            routines.extend(_discover_c(game, root, path))
        elif path.suffix.lower() == ".s":
            routines.extend(_discover_asm(game, root, path))

    c_definitions = {
        (str(Path(item["source"]).with_suffix("")), item["symbol"]): item
        for item in routines if item["kind"] == "c_function"
    }
    c_by_symbol: dict[str, list[dict]] = defaultdict(list)
    for item in routines:
        if item["kind"] == "c_function" and item["linkage"] == "external":
            c_by_symbol[item["symbol"]].append(item)
    unique_routines = []
    for routine in routines:
        source_stem = str(Path(routine["source"]).with_suffix(""))
        matching_c = c_definitions.get((source_stem, routine["symbol"]))
        if matching_c is None and routine["kind"] == "asm_function":
            external_matches = c_by_symbol.get(routine["symbol"], [])
            if len(external_matches) == 1:
                matching_c = external_matches[0]
        if routine["kind"] == "asm_function" and matching_c is not None:
            matching_c.setdefault("source_variants", []).append(routine["source"])
            continue
        unique_routines.append(routine)
    routines = unique_routines

    by_symbol: dict[str, list[dict]] = defaultdict(list)
    for routine in routines:
        by_symbol[routine["symbol"]].append(routine)
    reverse: dict[str, set[str]] = defaultdict(set)
    for routine in routines:
        resolved = set()
        ambiguous = set()
        for symbol in routine.pop("_raw_calls"):
            candidates = by_symbol.get(symbol, [])
            if len(candidates) == 1:
                resolved.add(candidates[0]["id"])
            elif len(candidates) > 1:
                same_file = [item for item in candidates if item["source"] == routine["source"]]
                if len(same_file) == 1:
                    resolved.add(same_file[0]["id"])
                else:
                    ambiguous.add(symbol)
        routine["calls"] = sorted(resolved)
        routine["ambiguous_calls"] = sorted(ambiguous)
        for target in resolved:
            reverse[target].add(routine["id"])
    for routine in routines:
        routine["called_by"] = sorted(reverse[routine["id"]])
    routines.sort(key=lambda item: item["id"])

    counts = defaultdict(int)
    for routine in routines:
        counts[routine["category"]] += 1
    return {
        "title": GAMES[game]["title"],
        "source_root": GAMES[game]["path"].as_posix(),
        "source_revision": _revision(root),
        "source_digest_sha256": _source_digest(paths, root),
        "coverage": {
            "definitions": "C definitions and exported assembly labels",
            "calls": "resolved direct lexical C calls and direct assembly branches",
            "not_yet_indexed": [
                "header-only declarations", "indirect function-pointer edges",
                "data tables and structure fields", "runtime-observed dependencies",
            ],
        },
        "files": {
            "c": sum(path.suffix.lower() == ".c" for path in paths),
            "headers": sum(path.suffix.lower() == ".h" for path in paths),
            "assembly": sum(path.suffix.lower() == ".s" for path in paths),
            "includes": sum(path.suffix.lower() == ".inc" for path in paths),
        },
        "statistics": {
            "routines": len(routines),
            "with_source_address": sum(item["address"] is not None for item in routines),
            "resolved_call_edges": sum(len(item["calls"]) for item in routines),
            "by_category": dict(sorted(counts.items())),
        },
        "routines": routines,
    }


def build_inventory(root: Path = ROOT) -> dict:
    games = {}
    for game, configuration in GAMES.items():
        source_root = root / configuration["path"]
        if not source_root.is_dir():
            raise ValueError(f"missing pinned source tree: {source_root}")
        games[game] = inventory_game(game, source_root)
    return {
        "schema": SCHEMA,
        "generator_version": GENERATOR_VERSION,
        "evidence": "automatic_static_discovery",
        "games": games,
    }


def write_inventory(inventory: dict, output: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(output.suffix + ".tmp")
    temporary.write_text(
        json.dumps(inventory, sort_keys=True, separators=(",", ":")) + "\n",
        encoding="utf-8")
    temporary.replace(output)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args(argv)
    try:
        result = build_inventory(args.root.resolve())
        if args.write:
            write_inventory(result, args.output)
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        parser.error(str(exc))
    for game, data in result["games"].items():
        stats = data["statistics"]
        print(f"{game}: {stats['routines']} routines, "
              f"{stats['resolved_call_edges']} resolved call edges")
    if args.write:
        print(f"Inventory: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
