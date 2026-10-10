#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build a deterministic native-source inventory from pinned decompilations."""
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
SCHEMA = "metroidvania-native-inventory-v4"
RECORD_SCHEMA = "metroidvania-native-inventory-records-v4"
GENERATOR_VERSION = 4
DEFAULT_OUTPUT = ROOT / "data/native_parity/inventory.json"
RECORD_COLLECTIONS = ("routines", "declarations", "data_symbols", "types", "constants")
MAX_SHARD_BYTES = 3_000_000
SOURCE_SUFFIXES = {".c", ".h", ".s", ".inc"}
CALL_RE = re.compile(r"\b([A-Za-z_]\w*)\s*\(")
INDIRECT_TABLE_CALL_RE = re.compile(
    r"\b([A-Za-z_]\w*)\s*\[[^\]\n]+\]\s*\(")
MEMBER_WRITE_RE = re.compile(
    r"(?:\.|->)\s*([A-Za-z_]\w*)\s*=\s*"
    r"(?:\(\s*[A-Za-z_][\w\s\*]*\)\s*)?([A-Za-z_]\w*)\s*;")
MEMBER_INIT_RE = re.compile(r"\.\s*([A-Za-z_]\w*)\s*=\s*([A-Za-z_]\w*)")
FP_MEMBER_RE = re.compile(r"\(\s*\*\s*([A-Za-z_]\w*)\s*\)\s*\(")
TYPEDEF_FP_RE = re.compile(
    r"\btypedef\s+[^;{}]*\(\s*\*\s*([A-Za-z_]\w*)\s*\)\s*\([^;{}]*\)\s*;")
ASM_BRANCH_RE = re.compile(r"^\s*(?:bl|blx|b)\s+([A-Za-z_.$]\w*)", re.MULTILINE)
ASM_FUNCTION_RE = re.compile(
    r"^\s*(?:(?:thumb|arm)_func_start\s+([A-Za-z_.$]\w*)|"
    r"\.type\s+([A-Za-z_.$]\w*)\s*,\s*%?function)\s*$",
    re.MULTILINE,
)
ASM_OBJECT_RE = re.compile(
    r"^\s*\.type\s+([A-Za-z_.$]\w*)\s*,\s*%?object\s*$", re.MULTILINE)
TYPE_RE = re.compile(r"\b(struct|union|enum)\s+([A-Za-z_]\w*)\s*\{")
ANONYMOUS_TYPEDEF_RE = re.compile(r"\btypedef\s+(struct|union|enum)\s*\{")
DEFINE_RE = re.compile(
    r"^\s*#\s*define\s+([A-Za-z_]\w*)\b(?![ \t]*\()", re.MULTILINE)
CONTROL_WORDS = {"if", "for", "while", "switch", "return", "sizeof"}
TYPE_WORDS = {
    "const", "volatile", "static", "extern", "register", "signed",
    "unsigned", "struct", "union", "enum", "void", "char", "short",
    "int", "long", "float", "double",
}

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


def _mask_preprocessor(text: str) -> str:
    """Mask preprocessor directives without changing offsets or line numbers."""
    result = list(text)
    lines = text.splitlines(keepends=True)
    offset = 0
    continued = False
    for line in lines:
        directive = continued or line.lstrip().startswith("#")
        continued = directive and line.rstrip("\r\n").endswith("\\")
        if directive:
            for index in range(offset, offset + len(line)):
                if result[index] not in "\r\n":
                    result[index] = " "
        offset += len(line)
    return "".join(result)


def _matching_brace(masked: str, opening: int) -> int | None:
    depth = 0
    for index in range(opening, len(masked)):
        if masked[index] == "{":
            depth += 1
        elif masked[index] == "}":
            depth -= 1
            if depth == 0:
                return index
    return None


def _source_address(symbol: str) -> str | None:
    match = re.search(r"(?:^|_)(?:0x)?(08[0-9A-Fa-f]{6})$", symbol)
    return f"0x{match.group(1).upper()}" if match else None


def _top_level_statements(masked: str) -> list[tuple[int, int]]:
    statements = []
    depth = 0
    start = 0
    for index, character in enumerate(masked):
        if character == "{":
            depth += 1
        elif character == "}" and depth:
            depth -= 1
        elif character == ";" and depth == 0:
            statements.append((start, index + 1))
            start = index + 1
    return statements


def _top_level_data_statements(masked: str) -> list[tuple[int, int]]:
    """Return top-level semicolon statements while skipping function bodies."""
    statements = []
    depth = 0
    start = 0
    index = 0
    while index < len(masked):
        character = masked[index]
        if character == "{" and depth == 0:
            if _function_header(masked[start:index]) is not None:
                closing = _matching_brace(masked, index)
                if closing is None:
                    break
                start = closing + 1
                index = closing + 1
                continue
            depth = 1
        elif character == "{" and depth:
            depth += 1
        elif character == "}" and depth:
            depth -= 1
        elif character == ";" and depth == 0:
            statements.append((start, index + 1))
            start = index + 1
        index += 1
    return statements


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


def _discover_declarations(game: str, root: Path, path: Path) -> list[dict]:
    source = path.read_text(encoding="utf-8", errors="replace")
    masked = _mask_preprocessor(_mask_c(source))
    relative = path.relative_to(root)
    declarations = []
    for start, end in _top_level_statements(masked):
        statement = masked[start:end - 1].strip()
        parsed = _function_header(statement)
        if parsed is None:
            continue
        symbol, prefix = parsed
        position = masked.rfind(symbol, start, end)
        declarations.append({
            "id": f"{game}:declaration:{relative.as_posix()}:{symbol}",
            "symbol": symbol,
            "source": relative.as_posix(),
            "line": masked.count("\n", 0, position) + 1,
            "category": _category(game, relative),
            "linkage": "internal" if re.search(r"\bstatic\b", prefix) else "external",
        })
    return declarations


def _split_top_level_commas(text: str) -> list[str]:
    """Split on commas that are not nested in parentheses, braces or brackets."""
    parts = []
    depth = 0
    start = 0
    for index, character in enumerate(text):
        if character in "({[":
            depth += 1
        elif character in ")}]" and depth:
            depth -= 1
        elif character == "," and depth == 0:
            parts.append(text[start:index])
            start = index + 1
    parts.append(text[start:])
    return parts


def _declarator_name(declarator: str) -> str | None:
    """Extract the declared identifier from one comma-separated declarator."""
    pointer = re.search(r"\(\s*\*\s*([A-Za-z_]\w*)\s*\)", declarator)
    if pointer:
        return pointer.group(1)
    declarator = declarator.split("=", 1)[0]
    declarator = re.sub(r"(?:\s*\[[^]]*\]\s*)+$", "", declarator).rstrip()
    declarator = declarator.lstrip("*").rstrip()
    match = re.search(r"([A-Za-z_]\w*)\s*$", declarator)
    if not match or match.group(1) in TYPE_WORDS:
        return None
    return match.group(1)


def _type_members(kind: str, body: str) -> list[str]:
    if kind == "enum":
        members = []
        for part in _split_top_level_commas(body):
            match = re.match(r"\s*([A-Za-z_]\w*)", part)
            if match:
                members.append(match.group(1))
        return members
    members = []
    for statement in body.split(";"):
        for declarator in _split_top_level_commas(statement):
            pointer = re.search(r"\(\s*\*\s*([A-Za-z_]\w*)\s*\)", declarator)
            if pointer:
                members.append(pointer.group(1))
                continue
            declarator = declarator.split(":", 1)[0]
            identifiers = re.findall(r"\b([A-Za-z_]\w*)\b", declarator)
            candidates = [item for item in identifiers if item not in TYPE_WORDS]
            if candidates:
                members.append(candidates[-1])
    return members


def _function_pointer_members(kind: str, body: str,
                              fp_typedefs: frozenset[str]) -> list[str]:
    if kind == "enum":
        return []
    members = set(FP_MEMBER_RE.findall(body))
    for statement in body.split(";"):
        if "(" in statement:
            continue
        identifiers = re.findall(r"\b([A-Za-z_]\w*)\b", statement.split(":", 1)[0])
        candidates = [item for item in identifiers if item not in TYPE_WORDS]
        if len(candidates) >= 2 and candidates[0] in fp_typedefs:
            members.add(candidates[-1])
    return sorted(members)


def _discover_types(game: str, root: Path, path: Path,
                    fp_typedefs: frozenset[str] = frozenset()) -> list[dict]:
    source = path.read_text(encoding="utf-8", errors="replace")
    masked = _mask_preprocessor(_mask_c(source))
    relative = path.relative_to(root)
    records = []
    seen = set()
    for match in TYPE_RE.finditer(masked):
        kind, symbol = match.groups()
        opening = masked.find("{", match.start(), match.end())
        closing = _matching_brace(masked, opening)
        if closing is None or (kind, symbol) in seen:
            continue
        seen.add((kind, symbol))
        records.append({
            "id": f"{game}:type:{relative.as_posix()}:{kind}:{symbol}",
            "symbol": symbol,
            "kind": kind,
            "source": relative.as_posix(),
            "line": masked.count("\n", 0, match.start()) + 1,
            "category": _category(game, relative),
            "members": _type_members(kind, masked[opening + 1:closing]),
            "function_pointer_members": _function_pointer_members(
                kind, masked[opening + 1:closing], fp_typedefs),
        })
    for match in ANONYMOUS_TYPEDEF_RE.finditer(masked):
        kind = match.group(1)
        opening = masked.find("{", match.start(), match.end())
        closing = _matching_brace(masked, opening)
        if closing is None:
            continue
        name = re.match(r"\s*([A-Za-z_]\w*)\s*;", masked[closing + 1:])
        if name is None or ("typedef", name.group(1)) in seen:
            continue
        seen.add(("typedef", name.group(1)))
        records.append({
            "id": f"{game}:type:{relative.as_posix()}:{kind}:{name.group(1)}",
            "symbol": name.group(1),
            "kind": kind,
            "source": relative.as_posix(),
            "line": masked.count("\n", 0, match.start()) + 1,
            "category": _category(game, relative),
            "anonymous": True,
            "members": _type_members(kind, masked[opening + 1:closing]),
            "function_pointer_members": _function_pointer_members(
                kind, masked[opening + 1:closing], fp_typedefs),
        })
    return records


def _discover_constants(game: str, root: Path, path: Path) -> list[dict]:
    source = path.read_text(encoding="utf-8", errors="replace")
    relative = path.relative_to(root)
    return [{
        "id": f"{game}:constant:{relative.as_posix()}:{match.group(1)}",
        "symbol": match.group(1),
        "source": relative.as_posix(),
        "line": source.count("\n", 0, match.start()) + 1,
        "category": _category(game, relative),
    } for match in DEFINE_RE.finditer(source)]


def _data_declarators(statement: str) -> list[tuple[str, str, str]]:
    """Split one top-level data statement into (symbol, prefix, initializer)."""
    stripped = statement.strip().rstrip(";").strip()
    if not stripped or re.search(r"\b(?:typedef|extern)\b", stripped):
        return []
    declarators = _split_top_level_commas(stripped)
    first_head = declarators[0].split("=", 1)[0].rstrip()
    if _function_header(first_head) is not None:
        return []
    pointer = re.search(r"\(\s*\*\s*([A-Za-z_]\w*)\s*\)", first_head)
    if pointer:
        prefix = first_head[:pointer.start()]
    else:
        trimmed = re.sub(r"(?:\s*\[[^]]*\]\s*)+$", "", first_head).rstrip()
        trimmed = trimmed.rstrip("*").rstrip()
        match = re.search(r"([A-Za-z_]\w*)\s*$", trimmed)
        if not match or match.group(1) in TYPE_WORDS:
            return []
        prefix = first_head[:match.start()]
    if not re.search(r"[A-Za-z_]", prefix):
        return []
    records = []
    for index, declarator in enumerate(declarators):
        head = declarator.split("=", 1)[0].rstrip()
        if index and re.search(r"[A-Za-z_]\w*\s*\([^()]*\)\s*$", head) \
                and "(" not in prefix:
            continue
        symbol = _declarator_name(declarator)
        if symbol is None:
            continue
        initializer = declarator.split("=", 1)[1] if "=" in declarator else ""
        records.append((symbol, prefix, initializer))
    return records


def _discover_c_data(game: str, root: Path, path: Path) -> list[dict]:
    source = path.read_text(encoding="utf-8", errors="replace")
    masked = _mask_preprocessor(_mask_c(source))
    relative = path.relative_to(root)
    records = []
    for start, end in _top_level_data_statements(masked):
        statement = masked[start:end]
        linkage = None
        for symbol, prefix, initializer in _data_declarators(statement):
            if linkage is None:
                linkage = "internal" if re.search(r"\bstatic\b", prefix) else "external"
            position = masked.find(symbol, start, end)
            records.append({
                "id": f"{game}:data:{relative.as_posix()}:{symbol}",
                "symbol": symbol,
                "kind": "c_data",
                "source": relative.as_posix(),
                "line": masked.count("\n", 0, position) + 1,
                "linkage": linkage,
                "category": _category(game, relative),
                "address": _source_address(symbol),
                "_raw_references": sorted(set(re.findall(r"\b([A-Za-z_]\w*)\b", initializer))),
                "_raw_member_inits": sorted(set(MEMBER_INIT_RE.findall(initializer))),
            })
    return records


def _discover_asm_data(game: str, root: Path, path: Path) -> list[dict]:
    source = path.read_text(encoding="utf-8", errors="replace")
    relative = path.relative_to(root)
    return [{
        "id": f"{game}:data:{relative.as_posix()}:{match.group(1)}",
        "symbol": match.group(1),
        "kind": "asm_data",
        "source": relative.as_posix(),
        "line": source.count("\n", 0, match.start()) + 1,
        "linkage": "external",
        "category": _category(game, relative),
        "address": _source_address(match.group(1)),
        "_raw_references": [],
    } for match in ASM_OBJECT_RE.finditer(source)]


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
                address = address or _source_address(symbol)
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
                    current["_raw_indirect_tables"] = sorted(
                        set(INDIRECT_TABLE_CALL_RE.findall(body)))
                    current["_raw_member_writes"] = sorted(
                        set(MEMBER_WRITE_RE.findall(body)))
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
            "address": _source_address(symbol),
            "_raw_calls": sorted(set(ASM_BRANCH_RE.findall(source[body_start:body_end]))),
            "_raw_indirect_tables": [],
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
    declarations = []
    data_symbols = []
    types = []
    constants = []
    fp_typedefs = frozenset(
        match.group(1)
        for path in paths
        if path.suffix.lower() in {".c", ".h"}
        for match in TYPEDEF_FP_RE.finditer(_mask_preprocessor(
            _mask_c(path.read_text(encoding="utf-8", errors="replace")))))
    for path in paths:
        if path.suffix.lower() == ".c":
            routines.extend(_discover_c(game, root, path))
            data_symbols.extend(_discover_c_data(game, root, path))
            types.extend(_discover_types(game, root, path, fp_typedefs))
            constants.extend(_discover_constants(game, root, path))
        elif path.suffix.lower() == ".h":
            declarations.extend(_discover_declarations(game, root, path))
            types.extend(_discover_types(game, root, path, fp_typedefs))
            constants.extend(_discover_constants(game, root, path))
        elif path.suffix.lower() == ".s":
            routines.extend(_discover_asm(game, root, path))
            data_symbols.extend(_discover_asm_data(game, root, path))
        elif path.suffix.lower() == ".inc":
            constants.extend(_discover_constants(game, root, path))

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

    c_data_definitions = {
        (str(Path(item["source"]).with_suffix("")), item["symbol"]): item
        for item in data_symbols if item["kind"] == "c_data"
    }
    c_data_by_symbol: dict[str, list[dict]] = defaultdict(list)
    for item in data_symbols:
        if item["kind"] == "c_data" and item["linkage"] == "external":
            c_data_by_symbol[item["symbol"]].append(item)
    unique_data = []
    for item in data_symbols:
        if item["kind"] == "asm_data":
            source_stem = str(Path(item["source"]).with_suffix(""))
            matching_c = c_data_definitions.get((source_stem, item["symbol"]))
            if matching_c is None:
                candidates = c_data_by_symbol.get(item["symbol"], [])
                if len(candidates) == 1:
                    matching_c = candidates[0]
            if matching_c is not None:
                matching_c.setdefault("source_variants", []).append(item["source"])
                continue
        unique_data.append(item)
    data_symbols = unique_data

    by_symbol: dict[str, list[dict]] = defaultdict(list)
    for routine in routines:
        by_symbol[routine["symbol"]].append(routine)

    def resolve_unique(symbol: str, source: str) -> str | None:
        candidates = by_symbol.get(symbol, [])
        if len(candidates) == 1:
            return candidates[0]["id"]
        same_file = [item for item in candidates if item["source"] == source]
        return same_file[0]["id"] if len(same_file) == 1 else None

    fp_member_types: dict[str, list[str]] = defaultdict(list)
    for record in types:
        for member in record.get("function_pointer_members", []):
            fp_member_types[member].append(record["id"])
    fp_member_types = {member: sorted(ids) for member, ids in fp_member_types.items()}

    member_callback_edges = 0
    data_by_symbol: dict[str, list[dict]] = defaultdict(list)
    for item in data_symbols:
        targets = set()
        for symbol in item.pop("_raw_references"):
            resolved = resolve_unique(symbol, item["source"])
            if resolved is not None:
                targets.add(resolved)
        item["pointer_targets"] = sorted(targets)
        if targets:
            item["pointer_table"] = True
        member_callbacks = []
        for member, symbol in item.pop("_raw_member_inits", []):
            if member not in fp_member_types:
                continue
            resolved = resolve_unique(symbol, item["source"])
            if resolved is None:
                continue
            member_callback_edges += 1
            member_callbacks.append({
                "member": member,
                "target": resolved,
                "types": fp_member_types[member],
            })
        if member_callbacks:
            item["member_callbacks"] = member_callbacks
        data_by_symbol[item["symbol"]].append(item)

    indirect_edges = 0
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
        indirect_calls = []
        for symbol in routine.pop("_raw_indirect_tables"):
            tables = [item for item in data_by_symbol.get(symbol, [])
                      if item.get("pointer_targets")]
            if len(tables) > 1:
                same_file = [item for item in tables
                             if item["source"] == routine["source"]]
                tables = same_file if len(same_file) == 1 else []
            if len(tables) != 1:
                continue
            targets = tables[0]["pointer_targets"]
            resolved.update(targets)
            indirect_edges += len(targets)
            indirect_calls.append({
                "table": tables[0]["id"],
                "targets": targets,
            })
        routine["calls"] = sorted(resolved)
        routine["ambiguous_calls"] = sorted(ambiguous)
        routine["indirect_calls"] = indirect_calls
        member_callbacks = []
        for member, symbol in routine.pop("_raw_member_writes", []):
            if member not in fp_member_types:
                continue
            resolved = resolve_unique(symbol, routine["source"])
            if resolved is None:
                continue
            member_callback_edges += 1
            member_callbacks.append({
                "member": member,
                "target": resolved,
                "types": fp_member_types[member],
            })
        if member_callbacks:
            routine["member_callbacks"] = member_callbacks
    routines.sort(key=lambda item: item["id"])

    definition_ids = defaultdict(list)
    for routine in routines:
        definition_ids[routine["symbol"]].append(routine["id"])
    for declaration in declarations:
        declaration["definition_ids"] = sorted(
            definition_ids.get(declaration["symbol"], []))
    declarations.sort(key=lambda item: item["id"])
    data_symbols.sort(key=lambda item: item["id"])
    types.sort(key=lambda item: item["id"])
    constants.sort(key=lambda item: item["id"])

    counts = defaultdict(int)
    for routine in routines:
        counts[routine["category"]] += 1
    return {
        "title": GAMES[game]["title"],
        "source_root": GAMES[game]["path"].as_posix(),
        "source_revision": _revision(root),
        "source_digest_sha256": _source_digest(paths, root),
        "coverage": {
            "definitions": "C definitions and marked assembly functions",
            "declarations": "header function declarations linked by symbol",
            "data": "top-level C definitions and assembly object markers",
            "types": "named struct, union and enum definitions with lexical members",
            "constants": "object-like preprocessor definitions",
            "calls": "direct lexical calls, assembly branches and proven table dispatch",
            "member_callbacks": "lexically proven function-pointer member writes",
            "not_yet_indexed": [
                "callbacks assigned through computed values or assembly stores",
                "runtime-observed dependencies",
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
            "declarations": len(declarations),
            "declarations_without_definition": sum(
                not item["definition_ids"] for item in declarations),
            "data_symbols": len(data_symbols),
            "pointer_tables": sum(item.get("pointer_table", False)
                                  for item in data_symbols),
            "types": len(types),
            "constants": len(constants),
            "with_source_address": sum(item.get("address") is not None
                                       for item in routines),
            "resolved_call_edges": sum(len(item["calls"]) for item in routines),
            "resolved_indirect_call_edges": indirect_edges,
            "resolved_member_callback_edges": member_callback_edges,
            "by_category": dict(sorted(counts.items())),
        },
        "routines": routines,
        "declarations": declarations,
        "data_symbols": data_symbols,
        "types": types,
        "constants": constants,
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


def _compact(value):
    if isinstance(value, dict):
        result = {}
        for key, item in value.items():
            compacted = _compact(item)
            if compacted is None or compacted == [] or compacted == {}:
                continue
            result[key] = compacted
        return result
    if isinstance(value, list):
        return [_compact(item) for item in value]
    return value


def _encoded(value) -> bytes:
    return (json.dumps(_compact(value), sort_keys=True, separators=(",", ":"))
            + "\n").encode("utf-8")


def _atomic_write(path: Path, payload: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_bytes(payload)
    temporary.replace(path)


def _record_chunks(records: list[dict]) -> list[list[dict]]:
    chunks = []
    current = []
    current_size = 0
    for record in records:
        record_size = len(_encoded(record))
        if current and current_size + record_size > MAX_SHARD_BYTES:
            chunks.append(current)
            current = []
            current_size = 0
        current.append(record)
        current_size += record_size
    if current:
        chunks.append(current)
    return chunks


def write_inventory(inventory: dict, output: Path) -> None:
    manifest = {key: value for key, value in inventory.items() if key != "games"}
    manifest["games"] = {}
    expected = set()
    for game, game_data in inventory["games"].items():
        summary = {key: value for key, value in game_data.items()
                   if key not in RECORD_COLLECTIONS}
        summary["record_files"] = []
        for collection in RECORD_COLLECTIONS:
            records = game_data[collection]
            for index, chunk in enumerate(_record_chunks(records)):
                name = f"inventory.{game}.{collection}.{index:03d}.json"
                path = output.parent / name
                payload = _encoded({
                    "schema": RECORD_SCHEMA,
                    "game": game,
                    "collection": collection,
                    "records": chunk,
                })
                if len(payload) > 4 * 1024 * 1024:
                    raise ValueError(f"inventory shard exceeds repository limit: {name}")
                _atomic_write(path, payload)
                digest = hashlib.sha256(payload).hexdigest()
                summary["record_files"].append({
                    "path": name,
                    "collection": collection,
                    "count": len(chunk),
                    "sha256": digest,
                })
                expected.add(path.resolve())
        manifest["games"][game] = summary
    for path in output.parent.glob("inventory.*.*.*.json"):
        if path.resolve() not in expected:
            path.unlink()
    _atomic_write(output, _encoded(manifest))


def inventory_output_paths(output: Path = DEFAULT_OUTPUT) -> list[Path]:
    paths = [output]
    if not output.is_file():
        return paths
    manifest = json.loads(output.read_text(encoding="utf-8"))
    for game_data in manifest.get("games", {}).values():
        for record_file in game_data.get("record_files", []):
            paths.append(output.parent / record_file["path"])
    return paths


def load_inventory(path: Path = DEFAULT_OUTPUT) -> dict:
    try:
        manifest = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ValueError(f"cannot load {path}: {exc}") from exc
    if manifest.get("schema") != SCHEMA:
        raise ValueError("unsupported native inventory schema")
    inventory = {key: value for key, value in manifest.items() if key != "games"}
    inventory["games"] = {}
    for game, summary in manifest.get("games", {}).items():
        game_data = {key: value for key, value in summary.items()
                     if key != "record_files"}
        for collection in RECORD_COLLECTIONS:
            game_data[collection] = []
        for descriptor in summary.get("record_files", []):
            shard_path = path.parent / descriptor["path"]
            try:
                payload = shard_path.read_bytes()
                shard = json.loads(payload)
            except (OSError, json.JSONDecodeError) as exc:
                raise ValueError(f"cannot load inventory shard {shard_path}: {exc}") from exc
            if hashlib.sha256(payload).hexdigest() != descriptor["sha256"]:
                raise ValueError(f"inventory shard checksum mismatch: {shard_path}")
            collection = descriptor["collection"]
            if (shard.get("schema") != RECORD_SCHEMA or shard.get("game") != game
                    or shard.get("collection") != collection):
                raise ValueError(f"invalid inventory shard metadata: {shard_path}")
            records = shard.get("records")
            if not isinstance(records, list) or len(records) != descriptor["count"]:
                raise ValueError(f"invalid inventory shard count: {shard_path}")
            game_data[collection].extend(records)
        inventory["games"][game] = game_data
    return inventory


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
              f"{stats['declarations']} declarations, "
              f"{stats['data_symbols']} data symbols, "
              f"{stats['resolved_call_edges']} resolved call edges")
    if args.write:
        print(f"Inventory: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
