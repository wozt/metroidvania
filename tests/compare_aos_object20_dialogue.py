#!/usr/bin/env python3
"""Native-vs-runtime comparison: Aria of Sorrow area 0 room 0
post-opening Object20 room dialogue (proximity / action-triggered
windows after the opening story commits).

Protocol (artifacts stay in --workdir; ROM/save-state/frame files are
never committed):

1. Fresh portable save: same schema-v15 payload as
   tests/compare_aos_opening_dialogue.py (FNV-1a checksum).
2. Native side: `aos_mgba_lab` boots the real ROM with the scout key
   schedule to reach gameplay (mode byte 0x02000010 == 4), saves an
   mGBA state = "room entry". From that state a second run replays a
   combined A+B pulse every --pulse frames (mask 3, 3-frame press):
   A edges advance the opening text, B edges trigger the room's
   Object20 action dialogues once free-roam begins. PPM captures plus
   WRAM peeks (player record 0x02000520, input-lock 0x0200042C,
   Object20 gate bytes 0x02000414/0x02000416/0x02000417) sample the
   run; the WIN0 band luminance metric from the opening comparison
   detects window-open frames.
3. Runtime side: `fusion_aria_runtime --save <fresh> --rom <rom>
   --capture out.bmp N 3 --repeat PULSE` under xvfb (or $DISPLAY).
   The constant A+B mask with one released frame per PULSE gives the
   same edge cadence; once free-roam begins each pulse re-opens the
   room dialogue, so the band stays dark ~PULSE-10 frames per cycle
   and the first lit->dark transition after the opening is found by
   probe + binary search.
4. Report: every lit->dark transition frame on both sides, the
   pulse->window latency, and deltas. Frame 0 differs between sides
   (native = first mode-4 frame after the file flow; runtime = first
   room-loop frame after setup), so treat absolute frames as
   approximate; the pulse->window latency is the comparable metric.

Measured 2026-11-15 (ROM abd71fe0..., this checkout, pulse=500,
workdir /tmp/obj20/cmp_test):
  native : post-story room-dialogue windows open every ~500 frames,
           10-30 frames after each combined A+B pulse edge
           (opens 6010/6520/7030/7510/... ~= pulse+10..30; the story
           tail before ~6000 shows irregular windows instead).
           First mode-4 frame ~= 3120 with the scout schedule.
  runtime: --capture buttons=3 --repeat 200 completes "Aria opening
           story" by frame 20000 (scene_state 8/1); Object20
           diagnostics print ("guard npc effect pulse") but no
           room-dialogue window was triggered headlessly - the
           epilogue proximity path needs Soma inside a 0x40px box
           around Mina/Arikado and the capture mask has no direction
           bits. --capture buttons=19 (A+B+RIGHT) walks Soma out of
           room 0 (0/3 -> 0/5 by frame 20000) without a confirmed
           window. See --rt-buttons/--rt-thresh/--rt-lo/--rt-hi.

Usage:
  tests/compare_aos_object20_dialogue.py --rom "roms/Castlevania - Aria of Sorrow (USA).gba" [--keep]

Requires: build/aos_mgba_lab, build/fusion_aria_runtime, xvfb-run (or
DISPLAY), Python 3 + Pillow.
"""
import argparse
import importlib.util
import os
import subprocess
import sys
import tempfile

try:
    from PIL import Image
except ImportError:
    sys.exit("Pillow is required (python3 -m pip install pillow)")

_HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location(
    "compare_aos_opening_dialogue", os.path.join(_HERE,
                                               "compare_aos_opening_dialogue.py"))
_opening = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_opening)

ROM_DEFAULT = _opening.ROM_DEFAULT
LAB_DEFAULT = _opening.LAB_DEFAULT
RUNTIME_DEFAULT = _opening.RUNTIME_DEFAULT

PULSE_DEFAULT = 500          # frames between combined A+B edges
STORY_A_EVERY = 60           # finer A cadence while the story runs
NATIVE_RUN_FRAMES = 16000    # post-state run length
CAP_EVERY = 30               # native capture cadence inside the sweep
CAP_FROM = 4000              # skip the opening window itself
CAP_TO = 15000


def post_story_keys(pulse: int, frames: int) -> str:
    """Scout schedule for boot, then combined A+B 3-frame pulses every
    `pulse` frames. A alone every STORY_A_EVERY frames keeps text
    advancing between pulses (mirrors the manual schedules used in
    exploration: mask-1 taps plus mask-2/3 pulses)."""
    events = [tuple(e) for e in
              (tuple(map(int, t.split(":")))
               for t in _opening.scout_keys().split(","))]
    for f in range(3000, frames, STORY_A_EVERY):
        events.append((f, 1))
        events.append((f + 3, 0))
    for f in range(3000, frames, pulse):
        events.append((f, 3))
        events.append((f + 3, 0))
    events.sort(key=lambda e: e[0])
    # collapse duplicate frames keeping the last mask (later event wins,
    # matching aos_mgba_parse_keys order)
    merged = {}
    for f, m in events:
        merged[f] = m
    return ",".join(f"{f}:{m}" for f, m in sorted(merged.items()))


def band_mean_native(ppm_path: str) -> float:
    return _opening.band_mean_native(ppm_path)


def band_mean_bmp(bmp_path: str) -> float:
    return _opening.band_mean_bmp(bmp_path)


def classify_dark(series, thresh=90.0):
    """Return list of (open_frame, close_frame) dark intervals."""
    intervals = []
    open_f = None
    for f, m in series:
        dark = m < thresh
        if dark and open_f is None:
            open_f = f
        elif not dark and open_f is not None:
            intervals.append((open_f, f))
            open_f = None
    if open_f is not None:
        intervals.append((open_f, series[-1][0] if series else open_f))
    return intervals


# --- native measurement ---------------------------------------------------

def native_peek_spec(frames_from: int, frames_to: int,
                     every: int = 120) -> str:
    addrs = [(0x02000520, 16), (0x0200042C, 1),
             (0x02000414, 1), (0x02000416, 1), (0x02000417, 1)]
    peeks = []
    for f in range(frames_from, frames_to, every):
        for a, s in addrs:
            peeks.append(f"{f}:0x{a:08x}:{s}")
    return ",".join(peeks)


def parse_peek_blocks(text: str):
    """Parse `peek ADDR size N:` hex dumps into {addr: [bytes,...]} in
    output order."""
    import re
    pat = re.compile(r"peek ([0-9a-f]{8}) size (\d+):\n"
                     r"((?:  [0-9a-f]{8}  .*\n)+)")
    out = {}
    for m in pat.finditer(text):
        addr = int(m.group(1), 16)
        size = int(m.group(2))
        data = bytearray()
        for line in m.group(3).splitlines():
            hexpart = line.split("  ", 2)[-1]
            for t in hexpart.split():
                if re.fullmatch(r"[0-9a-fA-F]{2}", t):
                    data.append(int(t, 16))
        out.setdefault(addr, []).append(bytes(data[:size]))
    return out


def align_peeks(blocks, addrs, frames):
    """blocks[addr] lists are per-address in request order; frames were
    requested addr-major per frame, so index i of each list corresponds
    to the i-th frame that produced a result for that address. Missing
    results make exact alignment ambiguous; we return per-frame dicts
    only where every address has a result at the same ordinal."""
    idx = {a: 0 for a in addrs}
    rows = []
    for f in frames:
        row = {"frame": f}
        ok = True
        for a in addrs:
            lst = blocks.get(a, [])
            if idx[a] < len(lst):
                row[a] = lst[idx[a]]
            else:
                ok = False
            idx[a] += 1
        if ok:
            rows.append(row)
    return rows


def measure_native(rom, state, workdir, lab, pulse, frames,
                   cap_from=CAP_FROM, cap_to=CAP_TO):
    caps = list(range(cap_from, cap_to, CAP_EVERY))
    capspec = ",".join(map(str, caps))
    prefix = os.path.join(workdir, "obj20nat")
    keys = post_story_keys(pulse, frames)
    out = _opening.run_lab(rom, frames, keys, workdir,
                           ["--state-in", state,
                            "--capture", capspec, "--shot", prefix,
                            "--peek", native_peek_spec(cap_from, cap_to)])
    series = []
    for f in caps:
        p = f"{prefix}_{f:05d}.ppm"
        if os.path.exists(p):
            series.append((f, band_mean_native(p)))
    intervals = classify_dark(series)
    # gate-byte / position timeline (coarse)
    blocks = parse_peek_blocks(out)
    addrs = [0x02000520, 0x0200042C, 0x02000414,
             0x02000416, 0x02000417]
    pframes = list(range(cap_from, cap_to, 120))
    rows = align_peeks(blocks, addrs, pframes)
    gate_trace = []
    for r in rows:
        gate_trace.append({
            "frame": r["frame"],
            "pos": r.get(0x02000520, b"").hex(),
            "lock": r.get(0x0200042C, b"").hex(),
            "g414": r.get(0x02000414, b"").hex(),
            "g416": r.get(0x02000416, b"").hex(),
            "g417": r.get(0x02000417, b"").hex(),
        })
    return {"series": series, "intervals": intervals,
            "gate_trace": gate_trace, "raw_out": out}


# --- runtime measurement ---------------------------------------------------

def runtime_band(runtime, rom, save, target, workdir, frames,
                 buttons=3, repeat=PULSE_DEFAULT):
    out = os.path.join(workdir, f"rt_{frames}.bmp")
    inner = [runtime, "--save", save, "--save-target", target,
             "--rom", rom, "--repeat", str(repeat),
             "--capture", out, str(frames), str(buttons)]
    cmd = inner if os.environ.get("DISPLAY") else ["xvfb-run", "-a"] + inner
    proc = subprocess.run(cmd, capture_output=True, text=True)
    completed = "opening story completed" in proc.stdout
    if not os.path.exists(out):
        return None, completed, proc.stdout + proc.stderr
    return band_mean_bmp(out), completed, proc.stdout + proc.stderr


def measure_runtime(runtime, rom, save, workdir, lo, hi,
                    buttons=3, repeat=PULSE_DEFAULT, thresh=90.0):
    """Find the first frame in [lo,hi] whose WIN0 band is dark.
    Assumes lit before the room-dialogue era and (near-)permanently
    dark after it starts, because each pulse re-opens the window."""
    target = os.path.join(workdir, "rt_save.sav")
    cache = {}

    def probe(f):
        if f in cache:
            return cache[f]
        m, completed, log = runtime_band(runtime, rom, save, target,
                                       workdir, f, buttons, repeat)
        cache[f] = (m, completed)
        return cache[f]

    # exponential probe for first dark
    first_dark = None
    prev = None
    f = lo
    step = max(1, (hi - lo) // 12)
    while f <= hi:
        m, _ = probe(f)
        if m is not None and m < thresh:
            first_dark = f
            break
        prev = f
        f += step
    if first_dark is None:
        return {"error": f"no dark frame in [{lo},{hi}]",
                "probes": {k: v[0] for k, v in cache.items()}}
    # binary search left edge between prev (lit) and first_dark
    a = prev if prev is not None else lo
    b = first_dark
    while b - a > 1:
        mid = (a + b) // 2
        m, _ = probe(mid)
        if m is not None and m < thresh:
            b = mid
        else:
            a = mid
    completed_frames = sorted(f for f, (_, c) in cache.items() if c)
    return {"t_window": b, "probes": {k: v[0] for k, v in cache.items()},
            "completed_seen_at": completed_frames}


def pulse_latency(open_frame, pulse):
    """Frames from the most recent pulse edge (multiples of `pulse`)
    to the window-open frame."""
    if open_frame is None:
        return None
    return open_frame - (open_frame // pulse) * pulse


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--rom", default=ROM_DEFAULT)
    ap.add_argument("--lab", default=LAB_DEFAULT)
    ap.add_argument("--runtime", default=RUNTIME_DEFAULT)
    ap.add_argument("--workdir", default=None)
    ap.add_argument("--keep", action="store_true")
    ap.add_argument("--pulse", type=int, default=PULSE_DEFAULT)
    ap.add_argument("--native-frames", type=int, default=NATIVE_RUN_FRAMES)
    ap.add_argument("--cap-from", type=int, default=CAP_FROM)
    ap.add_argument("--cap-to", type=int, default=CAP_TO)
    ap.add_argument("--rt-lo", type=int, default=4000)
    ap.add_argument("--rt-hi", type=int, default=30000)
    ap.add_argument("--rt-buttons", type=lambda s: int(s, 0),
                    default=3,
                    help="runtime --capture mask (default 3 = A+B; "
                         "add direction bits, e.g. 19 = A+B+RIGHT)")
    ap.add_argument("--rt-thresh", type=float, default=100.0,
                    help="runtime dark-band threshold (lit ~=117, "
                         "window chrome ~=98)")
    ap.add_argument("--skip-native", action="store_true")
    ap.add_argument("--skip-runtime", action="store_true")
    args = ap.parse_args()

    for tool in (args.lab, args.runtime):
        if not os.path.exists(tool):
            sys.exit(f"missing tool: {tool}")
    if not os.path.exists(args.rom):
        sys.exit(f"missing ROM: {args.rom}")

    workdir = args.workdir or tempfile.mkdtemp(prefix="aos_obj20_cmp_")
    os.makedirs(workdir, exist_ok=True)
    _opening.LAB = args.lab  # run_lab/first_mode4_frame read this global
    save = os.path.join(workdir, "fresh.sav")
    state = os.path.join(workdir, "room0.state")
    _opening.write_fresh_save(save)

    report = {"pulse": args.pulse, "workdir": workdir}

    if not args.skip_native:
        keys = _opening.scout_keys()
        f4 = _opening.first_mode4_frame(args.rom, keys, workdir)
        if f4 < 0:
            sys.exit("never reached gameplay mode 4 with the scout schedule")
        print(f"[native] first mode-4 frame ~= {f4}")
        _opening.run_lab(args.rom, f4 + 1, keys, workdir,
                         ["--state-out", state])
        if not os.path.exists(state):
            sys.exit("state-out failed")
        nat = measure_native(args.rom, state, workdir, args.lab,
                             args.pulse, args.native_frames,
                             args.cap_from, args.cap_to)
        report["native"] = {
            "intervals": nat["intervals"],
            "gate_trace_tail": nat["gate_trace"][-10:],
        }
        print("\n=== native dark intervals (band<90) ===")
        for o, c in nat["intervals"]:
            print(f"  open~{o} close~{c} "
                  f"pulse_latency={pulse_latency(o, args.pulse)}")

    if not args.skip_runtime:
        rt = measure_runtime(args.runtime, args.rom, save, workdir,
                             args.rt_lo, args.rt_hi,
                             buttons=args.rt_buttons,
                             repeat=args.pulse, thresh=args.rt_thresh)
        report["runtime"] = rt
        print("\n=== runtime ===")
        print(rt.get("t_window") and
              f"t_window={rt['t_window']} "
              f"pulse_latency={pulse_latency(rt['t_window'], args.pulse)}"
              or rt)
        if rt.get("completed_seen_at"):
            print(f"'story completed' first seen at probe "
                  f"{rt['completed_seen_at'][0]}")

    out_path = os.path.join(workdir, "comparison.txt")
    import json
    with open(out_path, "w") as fh:
        json.dump(report, fh, indent=2, default=str)
    print(f"\nartifacts in {workdir} (report: {out_path})")


if __name__ == "__main__":
    main()
