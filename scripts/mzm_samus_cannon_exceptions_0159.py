# SPDX-License-Identifier: GPL-3.0-only
"""Explain exceptional Samus/body-to-cannon links without inventing compositions."""
import argparse
import json
from pathlib import Path

from scripts.mzm_samus_cannon_links_0158 import body_family

RUN_AIMS = ("None", "Forward", "DiagonalUp", "DiagonalDown")


def classify_missing(body_name, body_frames, symbols):
    pair = body_family(body_name)
    if pair is None:
        raise ValueError("unexpected animation family: " + body_name)
    suit, tail = pair
    cannon = "Suitless" if suit == "Suitless" else "Suit"
    sides = ("Left", "Right")
    matches = []
    reason = "no explicit exceptional native counterpart"
    if tail.endswith("_Running") and not tail.endswith("_Speedboosting"):
        side = tail[:-len("_Running")]
        if side in sides:
            names = [f"sArmCannonAnim_{cannon}_{side}_{aim}_Running" for aim in RUN_AIMS]
            reason = "native aim-dependent running arrays; aim must be selected by gameplay state"
        else:
            names = []
    elif tail.endswith("_GettingKnockedBack"):
        side = tail[:-len("_GettingKnockedBack")]
        names = ([f"sArmCannonAnim_{cannon}_{side}_GettingHurt"]
                 if side in sides else [])
        reason = "different pose names; gameplay alias not proven"
    elif tail.endswith("_Dying"):
        side = tail[:-len("_Dying")]
        names = ([f"sArmCannonAnim_{cannon}_Dying"] if side in sides else [])
        reason = "direction-independent dying array; absence is not proof of invisible cannon"
    else:
        names = []
    for name in names:
        if name not in symbols:
            continue
        item = symbols[name]
        matches.append({"symbol": name, "address": item["address"],
                        "frames_in_elf": item["frames_in_elf"],
                        "body_frame_count": body_frames,
                        "frame_count_matches": body_frames == item["frames_in_elf"],
                        "status": "candidate-not-composed"})
    return {"candidates": matches, "reason": reason,
            "status": "exceptional-candidates" if matches else "unmapped-no-native-counterpart"}


def build(links):
    if links.get("schema") != "metroidvania-mzm-samus-cannon-links-v1":
        raise ValueError("unsupported link schema")
    if not isinstance(links.get("unmapped"), list) or not isinstance(links.get("symbols"), dict):
        raise ValueError("incomplete links input")
    result = {"schema": "metroidvania-mzm-samus-cannon-exceptions-v1",
              "original_candidates": len(links["links"]) - len(links["unmapped"]),
              "exceptions": {}, "still_unmapped": [],
              "warning": "symbolic candidates are NOT native control-flow proof or complete sprites"}
    for name in links["unmapped"]:
        record = links["links"][name]
        item = classify_missing(name, record["body_frame_count"], links["symbols"])
        result["exceptions"][name] = item
        if not item["candidates"]:
            result["still_unmapped"].append(name)
    result["exceptional_candidates"] = sum(bool(x["candidates"]) for x in result["exceptions"].values())
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--links", type=Path,
                        default=Path("assets/extracted/samus_cannon/links.json"))
    parser.add_argument("--output", type=Path,
                        default=Path("assets/extracted/samus_cannon/exceptions.json"))
    args = parser.parse_args(argv)
    root = Path(__file__).resolve().parent.parent
    allowed = root / "assets/extracted"
    output = args.output.absolute()
    if not allowed.is_dir() or allowed.resolve() != allowed or allowed not in output.parents or output.suffix != ".json":
        parser.error("output must be JSON below assets/extracted")
    if any(p.is_symlink() for p in (output, *output.parents)
           if p == allowed or allowed in p.parents):
        parser.error("symlink output refused")
    try:
        links = json.loads(args.links.read_text(encoding="utf-8"))
        result = build(links)
        output.parent.mkdir(parents=True, exist_ok=True)
        data = json.dumps(result, indent=2, sort_keys=True) + "\n"
        if not output.exists() or output.read_text(encoding="utf-8") != data:
            output.write_text(data, encoding="utf-8")
    except (OSError, ValueError, KeyError, TypeError) as exc:
        parser.error(str(exc))
    print(f"Exceptional candidate mappings: {result['exceptional_candidates']}/{len(result['exceptions'])}.")
    print(f"Still unmapped: {len(result['still_unmapped'])}. No new sprites composed.")
    print("Report:", output)


if __name__ == "__main__":
    main()
