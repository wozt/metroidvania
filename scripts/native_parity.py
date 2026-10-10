#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Validate parity annotations and generate the native parity checklist.

The human-edited source of truth is ``data/native_parity/annotations.tsv``:
one tab-separated row per feature with a fixed header. TSV is used instead of
CSV because free-text notes routinely contain commas and semicolons, while
tabs never appear in the tracked content; the file stays editable in any
spreadsheet or text editor without quoting rules. Multi-value columns
(``native_routines``, ``local_sources``, ``tests``, ``dependencies``) join
their entries with ``|``. The Markdown checklist and any future views are
generated from this single source and must never be edited by hand.
"""
from __future__ import annotations

import argparse
from collections import defaultdict
from pathlib import Path

from scripts import native_inventory

ROOT = Path(__file__).resolve().parents[1]
INVENTORY = ROOT / "data/native_parity/inventory.json"
ANNOTATIONS = ROOT / "data/native_parity/annotations.tsv"
CHECKLIST = ROOT / "docs/NATIVE_PARITY_CHECKLIST.md"
INVENTORY_SCHEMA = native_inventory.SCHEMA

ANNOTATION_COLUMNS = (
    "game",
    "id",
    "category",
    "status",
    "title",
    "native_routines",
    "local_sources",
    "tests",
    "dependencies",
    "divergences",
    "notes",
    "next_action",
)
LIST_COLUMNS = ("native_routines", "local_sources", "tests", "dependencies")

CATEGORIES = (
    ("startup", "Startup and initialization"),
    ("title", "Title screen"),
    ("menus", "Menus and options"),
    ("input", "Controls and input"),
    ("interface", "HUD and interface"),
    ("player", "Player state machine"),
    ("physics", "Physics"),
    ("animation", "Animations"),
    ("collision", "Collisions, hitboxes and hurtboxes"),
    ("weapons", "Weapons and projectiles"),
    ("upgrades", "Upgrades and equipment"),
    ("abilities", "Powers and abilities"),
    ("enemies", "Enemies"),
    ("bosses", "Bosses"),
    ("items", "Items and drops"),
    ("environment", "Environment interactions"),
    ("rooms", "Rooms, areas and maps"),
    ("camera", "Camera and scrolling"),
    ("transitions", "Transitions"),
    ("events", "Events and scripts"),
    ("cutscenes", "Cutscenes"),
    ("audio", "Music and sound effects"),
    ("inventory", "Inventory and progression data"),
    ("progression", "Progression and unlock conditions"),
    ("saves", "Save and load"),
    ("death", "Death and game over"),
    ("victory", "Victory conditions"),
    ("endings", "Endings and credits"),
    ("technical", "GBA-specific technical systems"),
    ("data", "Native data definitions"),
    ("unclassified", "Unclassified source routines"),
)

STATUS_MARKS = {
    "missing": "[ ]",
    "partial": "[~]",
    "validated": "[x]",
    "research": "[?]",
    "blocked": "[!]",
}


def load_annotations(path: Path = ANNOTATIONS) -> dict:
    """Parse the canonical annotation TSV into the internal feature mapping."""
    try:
        text = path.read_text(encoding="utf-8")
    except OSError as exc:
        raise ValueError(f"cannot load {path}: {exc}") from exc
    lines = text.split("\n")
    if not lines or lines[0].split("\t") != list(ANNOTATION_COLUMNS):
        raise ValueError(
            f"{path}: header must be exactly: {'\t'.join(ANNOTATION_COLUMNS)}")
    games: dict[str, list[dict]] = defaultdict(list)
    previous_id = ""
    for number, line in enumerate(lines[1:], start=2):
        if not line:
            continue
        fields = line.split("\t")
        if len(fields) != len(ANNOTATION_COLUMNS):
            raise ValueError(
                f"{path}:{number}: expected {len(ANNOTATION_COLUMNS)} columns, "
                f"got {len(fields)}")
        row = dict(zip(ANNOTATION_COLUMNS, fields))
        for column, value in row.items():
            if column not in LIST_COLUMNS and "|" in value:
                raise ValueError(f"{path}:{number}: '|' is not allowed in {column}")
        feature = {
            "id": row["id"],
            "category": row["category"],
            "status": row["status"],
            "title": row["title"],
            "native_symbols": [item for item in row["native_routines"].split("|") if item],
            "local_sources": [item for item in row["local_sources"].split("|") if item],
            "tests": [item for item in row["tests"].split("|") if item],
            "dependencies": [item for item in row["dependencies"].split("|") if item],
            "divergences": row["divergences"],
            "notes": row["notes"],
            "next_action": row["next_action"],
        }
        if row["game"] not in ("mzm", "aos"):
            raise ValueError(f"{path}:{number}: unknown game: {row['game']!r}")
        if not feature["id"].startswith(f"{row['game']}."):
            raise ValueError(
                f"{path}:{number}: feature id {feature['id']!r} must start with "
                f"{row['game']!r}.")
        if feature["id"] <= previous_id:
            raise ValueError(
                f"{path}:{number}: rows must be sorted by unique id; "
                f"{feature['id']!r} is out of order or duplicated")
        previous_id = feature["id"]
        games[row["game"]].append(feature)
    return {"games": {game: {"features": features}
                      for game, features in sorted(games.items())}}


def format_annotations(annotations: dict) -> str:
    """Serialize annotations back to the canonical deterministic TSV."""
    lines = ["\t".join(ANNOTATION_COLUMNS)]
    rows = []
    for game, game_annotations in sorted(annotations.get("games", {}).items()):
        for feature in game_annotations.get("features", []):
            row = {
                "game": game,
                "id": feature["id"],
                "category": feature["category"],
                "status": feature["status"],
                "title": feature["title"],
                "native_routines": "|".join(feature.get("native_symbols", [])),
                "local_sources": "|".join(feature.get("local_sources", [])),
                "tests": "|".join(feature.get("tests", [])),
                "dependencies": "|".join(feature.get("dependencies", [])),
                "divergences": feature.get("divergences", ""),
                "notes": feature.get("notes", ""),
                "next_action": feature.get("next_action", ""),
            }
            rows.append("\t".join(row[column] for column in ANNOTATION_COLUMNS))
    lines.extend(sorted(rows, key=lambda row: row.split("\t")[1]))
    return "\n".join(lines) + "\n"


def _load(path: Path) -> dict:
    if path.name == INVENTORY.name:
        return native_inventory.load_inventory(path)
    if path.name == ANNOTATIONS.name:
        return load_annotations(path)
    raise ValueError(f"cannot load unrecognized parity document: {path}")


def validate(inventory: dict, annotations: dict, root: Path = ROOT) -> list[str]:
    if inventory.get("schema") != INVENTORY_SCHEMA:
        raise ValueError("unsupported native inventory schema")
    diagnostics = []
    category_ids = {item[0] for item in CATEGORIES}
    feature_ids = {
        feature.get("id", "")
        for game_data in annotations.get("games", {}).values()
        for feature in game_data.get("features", [])
    }
    seen_ids = set()
    for game, game_annotations in annotations.get("games", {}).items():
        if game not in inventory.get("games", {}):
            raise ValueError(f"annotations reference unknown game: {game}")
        symbols = defaultdict(list)
        for routine in inventory["games"][game]["routines"]:
            symbols[routine["symbol"]].append(routine["id"])
        for feature in game_annotations.get("features", []):
            feature_id = feature.get("id", "")
            if not feature_id or feature_id in seen_ids:
                raise ValueError(f"missing or duplicate feature id: {feature_id!r}")
            seen_ids.add(feature_id)
            if feature.get("status") not in STATUS_MARKS:
                raise ValueError(f"{feature_id}: invalid status")
            if feature.get("category") not in category_ids:
                raise ValueError(f"{feature_id}: invalid category")
            for symbol in feature.get("native_symbols", []):
                matches = symbols.get(symbol, [])
                if not matches:
                    raise ValueError(
                        f"{feature_id}: native symbol not found (renamed or removed "
                        f"from the inventory): {symbol}")
                if len(matches) > 1:
                    diagnostics.append(
                        f"{feature_id}: symbol {symbol} has {len(matches)} definitions")
            for dependency in feature.get("dependencies", []):
                if dependency not in feature_ids:
                    raise ValueError(
                        f"{feature_id}: unknown dependency feature id: {dependency}")
            for field in ("local_sources", "tests"):
                for relative in feature.get(field, []):
                    if not (root / relative).is_file():
                        raise ValueError(f"{feature_id}: missing {field} path: {relative}")
            if feature.get("status") == "validated" and not feature.get("tests"):
                raise ValueError(f"{feature_id}: validated status requires tests")
    return diagnostics


def _link_paths(paths: list[str]) -> str:
    return ", ".join(f"[`{path}`](../{path})" for path in paths) or "none recorded"


def _symbol_ids(game_data: dict, symbols: list[str]) -> list[str]:
    wanted = set(symbols)
    return [routine["id"] for routine in game_data["routines"]
            if routine["symbol"] in wanted]


def render(inventory: dict, annotations: dict) -> str:
    lines = [
        "# Native parity checklist",
        "",
        "This file is generated by `python3 -m scripts.rebuild --checklist` from",
        "the automatic source inventory and the canonical human annotations in",
        "`data/native_parity/annotations.tsv`. Do not edit it directly; edit the",
        "TSV (one row per feature, tab-separated columns, `|`-joined lists) and",
        "regenerate this view.",
        "",
        "> Scope warning: this static inventory indexes C/assembly functions, header",
        "> declarations, top-level data, named aggregate types, object-like constants",
        "> and function-pointer table dispatch when it is lexically provable. Dynamic",
        "> callbacks and runtime observations remain open. Counts are discovery",
        "> coverage, not game fidelity percentages.",
        "",
        "## Status legend",
        "",
        "- `[ ]` not implemented",
        "- `[~]` partially implemented",
        "- `[x]` implemented and behaviorally validated",
        "- `[?]` research or identification required",
        "- `[!]` blocked or known problem",
        "",
        "## Automatic inventory summary",
        "",
        "| Game | Source revision | Routines | Addressed | Call edges (indirect) | Annotated features |",
        "|---|---:|---:|---:|---:|---:|",
    ]
    for game in ("mzm", "aos"):
        game_data = inventory["games"][game]
        stats = game_data["statistics"]
        feature_count = len(annotations["games"][game]["features"])
        lines.append(
            f"| {game_data['title']} | `{game_data['source_revision'][:12]}` | "
            f"{stats['routines']} | {stats['with_source_address']} | "
            f"{stats['resolved_call_edges']} "
            f"({stats['resolved_indirect_call_edges']}) | {feature_count} |")
    lines.extend([
        "",
        "| Game | Header declarations | Data symbols | Pointer tables | Named types | Constants |",
        "|---|---:|---:|---:|---:|---:|",
    ])
    for game in ("mzm", "aos"):
        game_data = inventory["games"][game]
        stats = game_data["statistics"]
        lines.append(
            f"| {game_data['title']} | {stats['declarations']} | "
            f"{stats['data_symbols']} | {stats['pointer_tables']} | "
            f"{stats['types']} | {stats['constants']} |")
    lines.extend([
        "",
        "The machine-readable manifest and its checksummed record fragments, stable",
        "routine IDs and call edges are rooted at",
        "[`data/native_parity/inventory.json`](../data/native_parity/inventory.json).",
        "",
    ])

    for game in ("mzm", "aos"):
        game_data = inventory["games"][game]
        game_annotations = annotations["games"][game]
        features_by_category = defaultdict(list)
        for feature in game_annotations["features"]:
            features_by_category[feature["category"]].append(feature)
        category_counts = game_data["statistics"]["by_category"]
        lines.extend([
            f"## {game_data['title']}",
            "",
            f"Pinned source: `{game_data['source_root']}` at "
            f"`{game_data['source_revision']}`.",
            "",
        ])
        for category, title in CATEGORIES:
            count = category_counts.get(category, 0)
            features = sorted(features_by_category.get(category, []),
                              key=lambda item: item["id"])
            if not features and count == 0 and category in {"data", "unclassified"}:
                continue
            lines.extend([
                f"### {title}",
                "",
                f"[?] **`{game}.{category}.inventory-boundary` — source coverage.** "
                f"The current classifier assigns {count} discovered routine(s) to "
                "this category. Category completeness has not been established.",
                "",
            ])
            for feature in features:
                symbol_ids = _symbol_ids(game_data, feature.get("native_symbols", []))
                lines.append(
                    f"{STATUS_MARKS[feature['status']]} **`{feature['id']}` — "
                    f"{feature['title']}.** {feature['notes']}")
                lines.append("")
                lines.append(
                    f"Native evidence: {', '.join(f'`{item}`' for item in symbol_ids) or 'not attached yet'}.  ")
                lines.append(f"Local implementation: {_link_paths(feature.get('local_sources', []))}.  ")
                lines.append(f"Tests: {_link_paths(feature.get('tests', []))}.")
                if feature.get("dependencies"):
                    lines.append(
                        "Dependencies: "
                        + ", ".join(f"`{item}`" for item in feature["dependencies"]) + ".  ")
                if feature.get("divergences"):
                    lines.append(f"Known divergences: {feature['divergences']}  ")
                if feature.get("next_action"):
                    lines.append(f"Next action: {feature['next_action']}")
                lines.append("")

        missing = game_data["coverage"]["not_yet_indexed"]
        lines.extend([
            "### Known inventory gaps",
            "",
        ])
        lines.extend(f"- [!] {item}." for item in missing)
        lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def write_checklist(text: str, output: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(output.suffix + ".tmp")
    temporary.write_text(text, encoding="utf-8")
    temporary.replace(output)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--inventory", type=Path, default=INVENTORY)
    parser.add_argument("--annotations", type=Path, default=ANNOTATIONS)
    parser.add_argument("--output", type=Path, default=CHECKLIST)
    parser.add_argument("--check", action="store_true",
                        help="fail when the generated checklist differs")
    args = parser.parse_args(argv)
    try:
        inventory = _load(args.inventory)
        annotations = _load(args.annotations)
        diagnostics = validate(inventory, annotations, args.root.resolve())
        generated = render(inventory, annotations)
        if args.check:
            existing = args.output.read_text(encoding="utf-8")
            if existing != generated:
                raise ValueError(f"generated checklist is stale: {args.output}")
        else:
            write_checklist(generated, args.output)
    except ValueError as exc:
        parser.error(str(exc))
    for diagnostic in diagnostics:
        print(f"warning: {diagnostic}")
    print(f"Checklist {'checked' if args.check else 'written'}: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
