#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Constant C tables of the pinned Zero Mission decompilation.

The game's data tables (``src/data/*.c``) are designated initializers built
from enums and a few macros. ``table()`` evaluates one of them with the
values of every ``MAKE_ENUM`` of ``include/constants`` (implicit numbering,
flags, ``CLIP_BEHAVIOR_MAKE_CATEGORY``) and the macros the tables use
(``CONVERT_SECONDS`` with single-precision rounding, ``BEHAVIOR_TO_*``,
``TRUE``/``FALSE``, ``UCHAR_MAX``/``USHORT_MAX``). Nothing is stored in this
repository: callers export the values privately.
"""
from __future__ import annotations

from functools import lru_cache
from pathlib import Path
import re
import struct

ROOT = Path(__file__).resolve().parents[1]
DECOMP = ROOT / "third_party/mzm"
ENUM_RE = re.compile(r"MAKE_ENUM\(\s*\w+\s*,\s*(\w+)\s*\)[^{;]*\{(.*?)\};", re.S)
CONSTANTS = {"TRUE": 1, "FALSE": 0, "UCHAR_MAX": 0xFF, "USHORT_MAX": 0xFFFF, "NULL": 0}
MACROS = {"BEHAVIOR_TO_BLOCK": "CLIP_BEHAVIOR_BLOCK_START",
          "BEHAVIOR_TO_TANK": "CLIP_BEHAVIOR_TANK_START",
          "BEHAVIOR_TO_DOOR": "CLIP_BEHAVIOR_DOOR_START"}


def _float32(value: float) -> float:
    return struct.unpack("<f", struct.pack("<f", value))[0]


def _strip_comments(text: str) -> str:
    return re.sub(r"//[^\n]*|/\*.*?\*/", "", text, flags=re.S)


@lru_cache(maxsize=1)
def simple_defines(decomp: Path = DECOMP) -> dict[str, str]:
    """Object-like #defines of include/types.h and include/macros.h."""
    defines = {}
    for header in ("types.h", "macros.h"):
        path = decomp / "include" / header
        if not path.is_file():
            continue
        for name, body in re.findall(r"^#define\s+([A-Z_][A-Z0-9_]*)\s+(.+)$",
                                     _strip_comments(path.read_text(encoding="utf-8")),
                                     re.M):
            defines[name] = body.strip()
    return defines


def evaluate(expression: str, names: dict[str, int]) -> int:
    """An integer constant expression of the decompilation."""
    text = _strip_comments(expression).strip()
    defines = simple_defines()
    for _ in range(8):
        expanded = re.sub(r"\b[A-Z_][A-Z0-9_]*\b",
                          lambda m: f"({defines[m.group(0)]})"
                          if m.group(0) in defines and m.group(0) not in names
                          and m.group(0) not in CONSTANTS else m.group(0), text)
        if expanded == text:
            break
        text = expanded
    # CONVERT_SECONDS(s): (s32)(s * 60) in single precision.
    def seconds(match: re.Match) -> str:
        inner = re.sub(r"(\d+\.\d*|\.\d+|\d+)f\b", r"\1", match.group(1))
        value = _float32(eval(inner, {"__builtins__": {}}, {}))   # noqa: S307
        return str(int(_float32(value * 60.0)))
    # SAMPLES_PER_SECOND(s): REFRESH_RATE * s + 0.5 in single precision,
    # REFRESH_RATE being CLOCKRATE (16.f * 1024 * 1024) / FRAME_DRAW_CYCLES.
    def samples(match: re.Match) -> str:
        refresh = _float32(16.0 * 1024 * 1024 / 280896)
        return str(int(_float32(_float32(refresh * int(match.group(1), 0)) + 0.5)))
    text = re.sub(r"SAMPLES_PER_SECOND\(\s*(\w+)\s*\)", samples, text)
    previous = None
    while previous != text:
        previous = text
        text = re.sub(r"CONVERT_SECONDS\(([^()]*)\)", seconds, text)
        for macro, start in MACROS.items():
            text = re.sub(rf"{macro}\(\s*(\w+)\s*\)", rf"(\1 - {start})", text)
    def name(match: re.Match) -> str:
        word = match.group(0)
        if word in CONSTANTS:
            return str(CONSTANTS[word])
        if word in names:
            return str(names[word])
        raise ValueError(f"unknown name {word} in {expression!r}")
    text = re.sub(r"\b[A-Za-z_]\w*\b", name, text)
    if not re.fullmatch(r"[\d\s()+\-*/|&<>~^xXa-fA-F]*", text):
        raise ValueError(f"unsupported expression {expression!r}")
    value = eval(text.replace("/", "//"), {"__builtins__": {}}, {})   # noqa: S307
    return int(value)


@lru_cache(maxsize=1)
def enum_values(decomp: Path = DECOMP) -> dict[str, int]:
    """Every MAKE_ENUM value of include/constants."""
    names: dict[str, int] = {}
    for header in sorted((decomp / "include/constants").glob("*.h")):
        for _, body in ENUM_RE.findall(header.read_text(encoding="utf-8")):
            body = _strip_comments(body)
            body = re.sub(
                r"CLIP_BEHAVIOR_MAKE_CATEGORY\(\s*(\w+)\s*,\s*(\w+)\s*,\s*(\w+)\s*\)",
                r"CLIP_BEHAVIOR_\1_START = \2, CLIP_BEHAVIOR_\1_END = \3,", body)
            current = -1
            for item in body.split(","):
                item = item.strip()
                if not item:
                    continue
                key, _, expression = (part.strip() for part in item.partition("="))
                current = evaluate(expression, names) if expression else current + 1
                names[key] = current
    return names


def _body(text: str, start: int) -> str:
    opening = text.index("{", start)
    depth = 0
    for index in range(opening, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1:index]
    raise ValueError("unterminated initializer")


def _items(body: str) -> list[str]:
    """Top-level comma-separated items of an initializer body."""
    items, depth, current = [], 0, []
    for char in body:
        if char in "{(":
            depth += 1
        elif char in "})":
            depth -= 1
        if char == "," and depth == 0:
            items.append("".join(current).strip())
            current = []
        else:
            current.append(char)
    if "".join(current).strip():
        items.append("".join(current).strip())
    return items


def _value(text: str, names: dict[str, int]):
    text = text.strip()
    if text.startswith("{"):
        inner = _body(text, 0)
        items = _items(inner)
        if items and items[0].startswith("."):
            fields = {}
            for item in items:
                key, _, value = item.partition("=")
                fields[key.strip()[1:]] = _value(value, names)
            return fields
        return _array(inner, names)
    return evaluate(text, names)


def _array(body: str, names: dict[str, int]) -> dict[int, object]:
    result, index = {}, 0
    for item in _items(_strip_comments(body)):
        designated = re.match(r"\[([^\]]+)\]\s*=\s*", item)
        if designated:
            index = evaluate(designated.group(1), names)
            item = item[designated.end():]
        result[index] = _value(item, names)
        index += 1
    return result


def table(source: str, name: str, decomp: Path = DECOMP,
          extra: dict[str, int] | None = None) -> dict[int, object]:
    """The initializer of `name` in src/data/<source> by index (nested
    arrays as dicts, structs as dicts of fields); `extra` adds names its
    indices use."""
    text = (decomp / "src/data" / source).read_text(encoding="utf-8")
    match = re.search(rf"\b{name}\s*(\[[^\]]*\])+\s*=", text)
    if not match:
        raise ValueError(f"{name} not found in {source}")
    return _array(_body(text, match.end()), {**enum_values(decomp), **(extra or {})})


def header_enums(headers: list[str], decomp: Path = DECOMP) -> dict[str, int]:
    """The MAKE_ENUM values of other headers (relative to include/)."""
    names = dict(enum_values(decomp))
    found: dict[str, int] = {}
    for header in headers:
        text = (decomp / "include" / header).read_text(encoding="utf-8")
        for _, body in ENUM_RE.findall(text):
            current = -1
            for item in _strip_comments(body).split(","):
                item = item.strip()
                if not item:
                    continue
                key, _, expression = (part.strip() for part in item.partition("="))
                current = evaluate(expression, {**names, **found}) if expression else current + 1
                found[key] = current
    return found


def multi_sprite_data(source: str, name: str, headers: list[str],
                      decomp: Path = DECOMP) -> list[tuple[int, list[list[int]]]]:
    """A MultiSpriteData table of src/data/<source>: each frame's timer and
    its parts' (OAM index, y offset, x offset), up to the terminator."""
    extra = header_enums(headers, decomp)
    text = (decomp / "src/data" / source).read_text(encoding="utf-8")
    match = re.search(rf"\b{name}\s*\[[^\]]*\]\s*=", text)
    if not match:
        raise ValueError(f"{name} not found in {source}")
    body = _strip_comments(_body(text, match.end()))
    names = {**enum_values(decomp), **extra}
    frames = []
    pairs = re.findall(r"\.pData\s*=\s*(\w+)\s*,\s*\.timer\s*=\s*([^}]+?)\s*\}", body)
    if not pairs:
        # The positional form: pData, timer, ... up to NULL.
        items = [item.strip() for item in body.split(",") if item.strip()]
        pairs = []
        for data, timer in zip(items[0::2], items[1::2]):
            if data == "NULL":
                break
            pairs.append((data, timer))
    for data, timer in pairs:
        parts = table(source, data, decomp, extra)
        count = max(parts) + 1 if parts else 0
        frames.append((evaluate(timer.strip().rstrip(",").strip(), names),
                       [[int(parts[p].get(e, 0)) for e in range(3)] for p in range(count)]))
    return frames


def max_areas(decomp: Path = DECOMP) -> int:
    """MAX_AMOUNT_OF_AREAS (include/structs/connection.h)."""
    text = (decomp / "include/structs/connection.h").read_text(encoding="utf-8")
    return int(re.search(r"#define\s+MAX_AMOUNT_OF_AREAS\s+(\d+)", text).group(1))


def struct_initializer(source: str, name: str, decomp: Path = DECOMP) -> dict:
    """The fields of a single struct initializer `name` in src/data/<source>."""
    text = (decomp / "src/data" / source).read_text(encoding="utf-8")
    match = re.search(rf"\b{name}\s*=", text)
    if not match:
        raise ValueError(f"{name} not found in {source}")
    return _value("{" + _body(text, match.end()) + "}", enum_values(decomp))


def dense(values: dict[int, object], count: int, default=0) -> list:
    return [values.get(i, default) for i in range(count)]

