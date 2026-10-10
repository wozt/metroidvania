# SPDX-License-Identifier: GPL-3.0-only
"""Index all locally exported native Samus animations in a runtime-ready catalogue.

No ROM contents or graphics are committed; the catalogue records local BMP paths.
Compositions are diagnostic, and fallback body-only frames have no cannon.
"""
import argparse
import json
from pathlib import Path

from scripts.asset_layout import (
    EXTRACTED,
    METROID_SAMUS_BODY_SOURCE,
    METROID_SAMUS_CATALOG_CACHE,
    METROID_SAMUS_COMPOSED_SOURCE,
    METROID_SAMUS_SPECIAL_SOURCE,
    private_path,
)

SUITS = ("PowerSuit", "VariaSuit", "GravitySuit", "FullSuit", "Suitless")
SOURCES = ((METROID_SAMUS_COMPOSED_SOURCE, "composed", "diagnostic-composed"),
           (METROID_SAMUS_SPECIAL_SOURCE, "special",
            "body-only-diagnostic-composed"))


def inside(root, relative):
    relative = Path(relative)
    if not relative.is_relative_to(EXTRACTED):
        raise ValueError("catalogue directory is outside private assets")
    return private_path(Path(root), relative)


def validate_frames(root, origin, frames, field):
    result = []
    for i, frame in enumerate(frames):
        rel = frame.get(field)
        tick = frame.get("duration_ticks")
        if frame.get("index") != i or type(tick) is not int or not 1 <= tick <= 255:
            raise ValueError("invalid frame index or duration in " + str(origin))
        if not isinstance(rel, str) or rel.startswith("/") or ".." in Path(rel).parts:
            raise ValueError("unsafe BMP path in " + str(origin))
        path = origin / rel
        if path.is_symlink() or not path.is_file() or path.suffix.lower() != ".bmp":
            raise ValueError("missing or unsafe BMP: " + str(path))
        if not path.resolve().is_relative_to(root / "assets/extracted"):
            raise ValueError("BMP escapes private assets")
        result.append({"index": i, "bmp": path.relative_to(root).as_posix(),
                       "duration_ticks": tick})
    return result


def build(root, strict=False):
    root = Path(root).resolve()
    library = inside(root, METROID_SAMUS_BODY_SOURCE)
    sources = []
    entries = {}
    for folder, label, status in SOURCES:
        source = inside(root, folder)
        manifest = source / "manifest.json"
        if not manifest.is_file() or manifest.is_symlink():
            if strict:
                raise ValueError("missing source manifest " + str(manifest))
            continue
        data = json.loads(manifest.read_text(encoding="utf-8"))
        for name, item in data.get("sequences", {}).items():
            if item.get("status") != status:
                continue
            if not name.replace("_", "").isalnum():
                raise ValueError("invalid animation identifier")
            frames = validate_frames(root, source, item["frames"], "bmp")
            if frames:
                entries["PowerSuit/" + name] = {
                    "source": label, "fidelity": status,
                    "frames": frames, "frame_count": len(frames)}
        sources.append(label)
    manifest = library / "manifest.json"
    if manifest.is_file() and not manifest.is_symlink():
        data = json.loads(manifest.read_text(encoding="utf-8"))
        if data.get("schema") != "metroidvania-mzm-samus-body-export-v1":
            raise ValueError("unexpected body library schema")
        for name, item in data.get("animations", {}).items():
            if item.get("body_status") != "exported-body-only":
                continue
            suit = next((s for s in SUITS if name.startswith("sSamusAnim_" + s + "_")), None)
            if suit is None:
                continue
            key = suit + "/" + name[len("sSamusAnim_" + suit + "_"):].lower()
            if key in entries:
                continue
            frames = validate_frames(root, library, item["frames"], "body_bmp")
            if frames:
                entries[key] = {"source": "body", "fidelity": "body-only",
                                "frames": frames, "frame_count": len(frames)}
        sources.append("body")
    elif strict:
        raise ValueError("missing body library: run scripts.mzm_samus_export_0150")
    if not sources:
        raise ValueError("no animation sources found")
    counts = {s: sum(k.startswith(s + "/") for k in entries) for s in SUITS}
    return {"schema": "metroidvania-mzm-samus-runtime-library-v1",
            "warning": "Native extraction diagnostics, not verified game-accurate state/palette transitions",
            "sources": sources, "suits": counts, "sequences": dict(sorted(entries.items()))}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--strict", action="store_true", help="require all three source manifests")
    args = p.parse_args()
    root = Path(__file__).resolve().parents[1]
    try:
        output = inside(root, METROID_SAMUS_CATALOG_CACHE)
        report = build(root, args.strict)
        output.mkdir(parents=True, exist_ok=True)
        manifest = output / "manifest.json"
        if manifest.is_symlink():
            raise ValueError("refusing symlink output")
        data = json.dumps(report, sort_keys=True, indent=2) + "\n"
        if not manifest.exists() or manifest.read_text(encoding="utf-8") != data:
            manifest.write_text(data, encoding="utf-8")
    except (OSError, ValueError, KeyError, TypeError) as exc:
        p.error(str(exc))
    print("Indexed animations:", len(report["sequences"]))
    print("Per suit:", report["suits"])
    print("Sources:", ", ".join(report["sources"]))
    print("Manifest:", manifest)


if __name__ == "__main__":
    main()
