#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Generate the data of the development dashboard (``dashboard/``).

The dashboard observes the project; it never drives it. This generator only
reads: the native parity annotations (``data/native_parity/annotations.tsv``,
the registry of the two original-game reconstructions), the project feature
registry (``data/dashboard/features.tsv``: Metroidvania, story, tools, assets
and platforms), ``CMakeLists.txt``, the tracked native inventory, the rebuild
graph and the git history. With ``--run-tests`` it also runs CTest (in an
existing build directory) and the Python suite and records their results.

Every feature carries two kinds of facts, kept apart in the output:

* declared: its status, fidelity and limitations, written by a person in one
  of the two registries;
* verified: what this run checked itself (its source files exist, its tests
  exist and are registered, and, with ``--run-tests``, whether they passed).

Percentages are shares of registered features only, with their denominators;
features nobody registered are not counted, so no figure claims project
completeness. Outputs are ``dashboard/data.json`` and ``dashboard/data.js``
(the same data as a script, so ``index.html`` opens from ``file://`` without
``fetch``). They never contain ROM data, extracted assets or local paths.
"""
from __future__ import annotations

import argparse
from collections import Counter
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from scripts import native_inventory, native_parity  # noqa: E402

SCHEMA = "metroidvania-dashboard-v1"
REGISTRY = ROOT / "data/dashboard/features.tsv"
OUTPUT_DIR = ROOT / "dashboard"
REGISTRY_HEADER = ("area", "id", "category", "status", "fidelity", "title", "local_sources",
                   "tests", "dependencies", "limitations", "last_validated")
AREAS = {
    "mzm": "Metroid: Zero Mission",
    "aos": "Castlevania: Aria of Sorrow",
    "metroidvania": "Metroidvania",
    "story": "Story and events",
    "tools": "Tools and editors",
    "assets": "Asset reconstruction",
    "platform": "Platforms",
}
# Ordered from least to most advanced; "blocked" and "unknown" are apart.
STATUSES = (
    ("not_started", "Not started", "No implementation."),
    ("in_progress", "In progress", "Work started; nothing usable yet."),
    ("partial", "Partially functional", "Part of the feature works."),
    ("functional", "Functional", "Works for its intended use; fidelity to the original is "
                                 "not claimed."),
    ("faithful", "Faithful", "Matches the original game's behavior as traced."),
    ("validated", "Validated", "Faithful or complete, and covered by passing tests."),
    ("blocked", "Blocked", "Cannot progress until something else is resolved."),
    ("unknown", "Unknown", "Status not established or not verified."),
)
STATUS_IDS = tuple(item[0] for item in STATUSES)
FUNCTIONAL_OR_BETTER = {"functional", "faithful", "validated"}
FIDELITIES = ("n/a", "unknown", "approximate", "partial", "faithful")
# Native annotation statuses (scripts/native_parity.py) on the dashboard scale.
ANNOTATION_STATUS = {
    "missing": "not_started",
    "partial": "partial",
    "validated": "validated",
    "research": "unknown",
    "blocked": "blocked",
}
INDICATORS = (
    ("global", "Overall", None),
    ("mzm", "Zero Mission engine", ("mzm",)),
    ("aos", "Aria of Sorrow engine", ("aos",)),
    ("assets", "Asset reconstruction", ("assets",)),
    ("tools", "Tools and editors", ("tools",)),
    ("metroidvania", "Metroidvania gameplay", ("metroidvania",)),
    ("story", "Story and events", ("story",)),
    ("platform", "Platforms", ("platform",)),
)
DATE_RE = re.compile(r"^\d{4}-\d{2}-\d{2}$")
# In a registry "tests" column: the whole CTest and Python suite of the host
# that generates the data.
ALL_TESTS = "@all-tests"


def _split(value: str) -> list[str]:
    return [item for item in value.split("|") if item]


def load_registry(path: Path = REGISTRY) -> list[dict]:
    """The project feature registry, validated."""
    lines = path.read_text(encoding="utf-8").splitlines()
    if not lines or tuple(lines[0].split("\t")) != REGISTRY_HEADER:
        raise ValueError(f"{path}: unexpected header")
    features = []
    seen = set()
    for number, line in enumerate(lines[1:], start=2):
        if not line.strip():
            continue
        fields = line.split("\t")
        if len(fields) != len(REGISTRY_HEADER):
            raise ValueError(f"{path}:{number}: expected {len(REGISTRY_HEADER)} columns")
        row = dict(zip(REGISTRY_HEADER, fields))
        if row["area"] not in AREAS or row["area"] in ("mzm", "aos"):
            raise ValueError(f"{path}:{number}: unknown area {row['area']!r} (native "
                             "features belong in the parity annotations)")
        if not row["id"].startswith(("mv." if row["area"] == "metroidvania"
                                     else row["area"] + ".")):
            raise ValueError(f"{path}:{number}: id {row['id']!r} does not match its area")
        if row["id"] in seen:
            raise ValueError(f"{path}:{number}: duplicate id {row['id']}")
        seen.add(row["id"])
        if row["status"] not in STATUS_IDS:
            raise ValueError(f"{path}:{number}: unknown status {row['status']!r}")
        if row["fidelity"] not in FIDELITIES:
            raise ValueError(f"{path}:{number}: unknown fidelity {row['fidelity']!r}")
        if row["last_validated"] and not DATE_RE.match(row["last_validated"]):
            raise ValueError(f"{path}:{number}: last_validated must be YYYY-MM-DD")
        features.append({
            "id": row["id"], "area": row["area"], "category": row["category"],
            "title": row["title"], "status": row["status"], "fidelity": row["fidelity"],
            "local_sources": _split(row["local_sources"]), "tests": _split(row["tests"]),
            "dependencies": _split(row["dependencies"]), "native_routines": [],
            "limitations": row["limitations"], "next_action": "",
            "last_validated": row["last_validated"] or None,
            "source": "data/dashboard/features.tsv",
        })
    return features


def load_native_features() -> list[dict]:
    """The parity annotations of both reconstructions, on the dashboard scale."""
    annotations = native_parity._load(native_parity.ANNOTATIONS)
    features = []
    for game in ("mzm", "aos"):
        for item in annotations["games"][game]["features"]:
            status = ANNOTATION_STATUS.get(item["status"], "unknown")
            features.append({
                "id": item["id"], "area": game, "category": item["category"],
                "title": item["title"], "status": status,
                "fidelity": "partial" if status == "partial" else
                            "faithful" if status == "validated" else "unknown",
                "local_sources": list(item.get("local_sources", [])),
                "tests": list(item.get("tests", [])),
                "dependencies": list(item.get("dependencies", [])),
                "native_routines": list(item.get("native_symbols", [])),
                "limitations": " ".join(filter(None, (item.get("divergences", ""),
                                                      item.get("notes", "")))),
                "next_action": item.get("next_action", ""),
                "last_validated": None,
                "source": "data/native_parity/annotations.tsv",
            })
    return features


def cmake_tests(cmake: Path = ROOT / "CMakeLists.txt") -> dict[str, dict]:
    """CTest names with the test sources of their executables."""
    text = re.sub(r"#[^\n]*", "", cmake.read_text(encoding="utf-8"))
    sources: dict[str, list[str]] = {}
    for match in re.finditer(r"add_executable\(\s*([\w-]+)([^)]*)\)", text):
        sources[match.group(1)] = re.findall(r"tests/[\w./-]+", match.group(2))
    tests = {}
    for match in re.finditer(r"add_test\(\s*NAME\s+([\w-]+)([^)]*)\)", text):
        body = match.group(2)
        files = set(re.findall(r"tests/[\w./-]+", body))
        for target in re.findall(r"\$<TARGET_FILE:([\w-]+)>|COMMAND\s+([\w-]+)", body):
            files.update(sources.get(target[0] or target[1], []))
        tests[match.group(1)] = {"name": match.group(1), "files": sorted(files)}
    return tests


def python_test_modules(tests_dir: Path = ROOT / "tests") -> dict[str, dict]:
    modules = {}
    for path in sorted(tests_dir.glob("test_*.py")):
        text = path.read_text(encoding="utf-8")
        modules[path.relative_to(ROOT).as_posix()] = {
            "file": path.relative_to(ROOT).as_posix(),
            "module": f"tests.{path.stem}",
            "test_count": len(re.findall(r"^\s+def test_\w+", text, re.MULTILINE)),
        }
    return modules


def run_ctest(build: Path) -> dict[str, str]:
    result = subprocess.run(["ctest", "--test-dir", str(build)], cwd=ROOT,
                            capture_output=True, text=True, check=False)
    outcomes = {}
    for match in re.finditer(r"Test\s+#\d+:\s+([\w-]+)\s+\.+\s*(\*{0,3}\s*\w[^\n]*?)\s+[\d.]+ sec",
                             result.stdout):
        outcomes[match.group(1)] = "passed" if "Passed" in match.group(2) else "failed"
    return outcomes


def run_python_tests() -> dict[str, dict[str, str]]:
    result = subprocess.run([sys.executable, "-m", "unittest", "discover", "-s", "tests",
                             "-t", ".", "-v"], cwd=ROOT, capture_output=True, text=True,
                            check=False)
    outcomes: dict[str, dict[str, str]] = {}
    pattern = re.compile(r"^(test\w*) \((tests\.[\w.]+?)\.\w+(?:\.\w+)?\)[^\n]*?\.\.\. "
                         r"(ok|FAIL|ERROR|skipped[^\n]*|expected failure)", re.MULTILINE)
    for name, module, outcome in pattern.findall(result.stderr):
        module = module.split(".")[0] + "." + module.split(".")[1]
        state = ("passed" if outcome in ("ok", "expected failure") else
                 "skipped" if outcome.startswith("skipped") else "failed")
        outcomes.setdefault(module, {})[name] = state
    return outcomes


def _verify(feature: dict, ctest: dict[str, dict], python: dict[str, dict],
            ctest_results: dict[str, str] | None,
            python_results: dict[str, dict[str, str]] | None) -> dict:
    """What this run checked about one feature."""
    missing_sources = [path for path in feature["local_sources"] if not (ROOT / path).exists()]
    test_runs = []
    for path in feature["tests"]:
        if path == ALL_TESTS:
            # The whole suite on this host (platform features).
            outcomes = list((ctest_results or {}).values()) + [
                state for module in (python_results or {}).values() for state in module.values()]
            outcome = None
            if ctest_results is not None and python_results is not None:
                outcome = "failed" if "failed" in outcomes else "passed"
            test_runs.append({"test": f"{ALL_TESTS} on {sys.platform}", "kind": "suite",
                              "registered": True, "outcome": outcome})
            continue
        if path in python:
            module = python[path]["module"]
            outcome = None
            if python_results is not None:
                states = python_results.get(module, {}).values()
                outcome = ("failed" if "failed" in states else
                           "passed" if "passed" in states else "not run")
            test_runs.append({"test": path, "kind": "python", "registered": True,
                              "outcome": outcome})
        else:
            names = [name for name, item in ctest.items() if path in item["files"]]
            outcome = None
            if ctest_results is not None and names:
                states = [ctest_results.get(name, "not run") for name in names]
                outcome = ("failed" if "failed" in states else
                           "passed" if all(state == "passed" for state in states)
                           else "not run")
            test_runs.append({"test": path, "kind": "ctest", "registered": bool(names),
                              "ctest": names, "outcome": outcome,
                              "exists": (ROOT / path).exists()})
    ran = [run["outcome"] for run in test_runs if run["outcome"] is not None]
    if missing_sources:
        verdict = "missing_sources"
    elif not test_runs:
        verdict = "untested"
    elif any(not run["registered"] for run in test_runs):
        verdict = "unregistered_tests"
    elif not ran:
        verdict = "tests_not_run"
    elif "failed" in ran:
        verdict = "failing"
    elif all(outcome == "passed" for outcome in ran):
        verdict = "passing"
    else:
        verdict = "tests_not_run"
    warnings = []
    if feature["status"] == "validated" and verdict != "passing":
        warnings.append("declared validated, but no passing test was observed in this run")
    if feature["status"] in FUNCTIONAL_OR_BETTER and not feature["local_sources"]:
        warnings.append("declared working without any source file")
    if missing_sources:
        warnings.append("missing sources: " + ", ".join(missing_sources))
    return {"sources_found": not missing_sources, "missing_sources": missing_sources,
            "tests": test_runs, "verdict": verdict, "warnings": warnings}


def _share(part: int, total: int) -> float | None:
    return None if total == 0 else round(100.0 * part / total, 1)


def indicator(feature_list: list[dict]) -> dict:
    counts = Counter(item["status"] for item in feature_list)
    total = len(feature_list)
    working = sum(counts[status] for status in FUNCTIONAL_OR_BETTER)
    passing = sum(1 for item in feature_list if item["verification"]["verdict"] == "passing")
    return {
        "total": total,
        "by_status": {status: counts.get(status, 0) for status in STATUS_IDS},
        "functional_or_better": working,
        "functional_or_better_percent": _share(working, total),
        "with_passing_tests": passing,
        "with_passing_tests_percent": _share(passing, total),
        "unknown_or_blocked": counts.get("unknown", 0) + counts.get("blocked", 0),
    }


def _git(*arguments: str) -> str:
    result = subprocess.run(["git", *arguments], cwd=ROOT, capture_output=True, text=True,
                            check=False)
    return result.stdout.strip() if result.returncode == 0 else ""


def timeline(limit: int = 80) -> list[dict]:
    entries = []
    for line in _git("log", f"-n{limit}", "--date=short",
                     "--pretty=format:%h%x09%ad%x09%s").splitlines():
        parts = line.split("\t", 2)
        if len(parts) != 3:
            continue
        scope = re.match(r"^\w+(?:\(([\w-]+)\))?:", parts[2])
        entries.append({"commit": parts[0], "date": parts[1], "subject": parts[2],
                        "scope": (scope.group(1) or "project") if scope else "project"})
    return entries


def inventory_summary() -> dict:
    try:
        inventory = native_inventory.load_inventory()
    except (OSError, ValueError) as exc:
        return {"available": False, "error": str(exc)}
    annotations = native_parity._load(native_parity.ANNOTATIONS)
    games = {}
    for game in ("mzm", "aos"):
        data = inventory["games"][game]
        cited = {symbol for item in annotations["games"][game]["features"]
                 for symbol in item.get("native_symbols", [])}
        statistics = data["statistics"]
        games[game] = {
            "title": data["title"],
            "routines": statistics["routines"],
            "by_category": statistics["by_category"],
            "by_category_evidence": statistics.get("by_category_evidence", {}),
            "routines_cited_by_features": len(cited),
            "call_edges": statistics.get("resolved_call_edges", 0),
        }
    return {"available": True, "schema": inventory["schema"], "games": games,
            "not_yet_indexed": inventory["games"]["aos"]["coverage"]["not_yet_indexed"]}


def rebuild_tasks() -> list[dict]:
    from scripts import rebuild
    return [{"name": task.name, "group": task.group, "dependencies": list(task.dependencies),
             "requires": [path.relative_to(ROOT).as_posix() for path in task.requires]}
            for task in rebuild.TASKS.values()]


def build(run_tests: bool = False, build_dir: Path = ROOT / "build",
          now: datetime | None = None) -> dict:
    native = load_native_features()
    project = load_registry()
    features = native + project
    ids = {item["id"] for item in features}
    for item in features:
        unknown = [dependency for dependency in item["dependencies"] if dependency not in ids]
        if unknown and item["source"] == "data/dashboard/features.tsv":
            raise ValueError(f"{item['id']}: unknown dependencies {unknown}")
    ctest = cmake_tests()
    python = python_test_modules()
    ctest_results = python_results = None
    if run_tests:
        if (build_dir / "CTestTestfile.cmake").is_file():
            ctest_results = run_ctest(build_dir)
        python_results = run_python_tests()
    generated = (now or datetime.now(timezone.utc)).replace(microsecond=0)
    for item in features:
        item["verification"] = _verify(item, ctest, python, ctest_results, python_results)
        item["verification"]["verified_on"] = (generated.date().isoformat()
                                               if item["verification"]["verdict"] == "passing"
                                               else None)
    features.sort(key=lambda item: (list(AREAS).index(item["area"]), item["id"]))
    indicators = []
    for key, title, areas in INDICATORS:
        selected = [item for item in features if areas is None or item["area"] in areas]
        indicators.append({"id": key, "title": title, "areas": list(areas or AREAS),
                           **indicator(selected),
                           "features": [item["id"] for item in selected]})
    tests = {
        "ran": run_tests,
        "host": sys.platform,
        "ctest": [{**item, "outcome": None if ctest_results is None
                   else ctest_results.get(item["name"], "not run")}
                  for item in ctest.values()],
        "python": [{**item, "outcome": None if python_results is None else
                    ("failed" if "failed" in python_results.get(item["module"], {}).values()
                     else "passed" if python_results.get(item["module"]) else "not run"),
                    "passed": None if python_results is None else
                    sum(state == "passed" for state in
                        python_results.get(item["module"], {}).values())}
                   for item in python.values()],
    }
    if ctest_results is not None or python_results is not None:
        outcomes = ([item["outcome"] for item in tests["ctest"]] +
                    [state for module in (python_results or {}).values()
                     for state in module.values()])
        tests["summary"] = {"passed": outcomes.count("passed"),
                            "failed": outcomes.count("failed"),
                            "other": len(outcomes) - outcomes.count("passed")
                            - outcomes.count("failed")}
    return {
        "schema": SCHEMA,
        "generated_at": generated.isoformat(),
        "git": {"commit": _git("rev-parse", "--short", "HEAD"),
                "branch": _git("rev-parse", "--abbrev-ref", "HEAD"),
                "dirty": bool(_git("status", "--porcelain", "--untracked-files=no"))},
        "areas": [{"id": key, "title": title} for key, title in AREAS.items()],
        "statuses": [{"id": key, "label": label, "description": text}
                     for key, label, text in STATUSES],
        "method": ("Percentages are shares of registered features: 'functional or better' "
                   "counts the functional, faithful and validated statuses declared in the "
                   "registries; 'passing tests' counts features whose declared tests all "
                   "passed when this data was generated. Unregistered work is not counted."),
        "indicators": indicators,
        "features": features,
        "tests": tests,
        "timeline": timeline(),
        "inventory": inventory_summary(),
        "rebuild_tasks": rebuild_tasks(),
    }


def write(data: dict, output_dir: Path = OUTPUT_DIR) -> list[Path]:
    output_dir.mkdir(parents=True, exist_ok=True)
    text = json.dumps(data, indent=1, ensure_ascii=False, sort_keys=False)
    for forbidden in ("assets/extracted", str(ROOT)):
        if forbidden in text:
            raise ValueError(f"dashboard data would contain {forbidden!r}")
    json_path = output_dir / "data.json"
    js_path = output_dir / "data.js"
    json_path.write_text(text + "\n", encoding="utf-8")
    js_path.write_text("// Generated by scripts/dashboard.py; do not edit.\n"
                       f"window.METROIDVANIA_DASHBOARD = {text};\n", encoding="utf-8")
    return [json_path, js_path]


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--run-tests", action="store_true",
                        help="run CTest (in --build) and the Python suite and record results")
    parser.add_argument("--build", type=Path, default=ROOT / "build")
    parser.add_argument("--output", type=Path, default=OUTPUT_DIR)
    args = parser.parse_args(argv)
    try:
        data = build(run_tests=args.run_tests, build_dir=args.build)
        paths = write(data, args.output)
    except (OSError, ValueError) as exc:
        parser.error(str(exc))
    overall = data["indicators"][0]
    print(f"Dashboard data: {overall['total']} features, "
          f"{overall['functional_or_better']} functional or better, "
          f"{overall['with_passing_tests']} with passing tests")
    for path in paths:
        print("Wrote", path.relative_to(ROOT) if path.is_relative_to(ROOT) else path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
