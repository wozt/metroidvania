#!/usr/bin/env python3
"""First native-vs-runtime comparison: Aria of Sorrow area 0 room 0
opening-scene dialogue window timing.

Protocol (all artifacts stay in --workdir, default a fresh temp dir;
ROM/save-state/frame files are never committed):

1. Fresh portable save: the runtime's opening story only runs when
   `has_active_save` is true and event 0x20 is clear
   (src/runtime/aria_runtime.c `opening_needed`). This script writes a
   schema-v15 save equivalent to `aos_save_init_new` + `aos_save_store`
   (FNV-1a checksum, verified against aos_save.c).
2. Native side: `aos_mgba_lab` boots the real ROM with the same
   scout-style key schedule `src/tools/aos_lab_worker.c` uses to reach
   gameplay (mode byte at 0x02000010 == 4). A peek sweep every 60
   frames between 2400-4400 finds the first mode-4 frame; a second run
   stops there and saves an mGBA state = "room entry".
3. Native measurement: from that state, no keys, PPM captures every
   5 frames in [180,260). The dialogue window's BLDY darkening drops
   the mean luminance of the WIN0 band (native x 16..224, y 96..150)
   from ~117 (lit scene) to ~62 (dimmed chrome). The first frame below
   the midpoint is the window-open frame.
4. Runtime side: `fusion_aria_runtime --save <fresh> --rom <rom>
   --capture out.bmp N 0` under xvfb (or $DISPLAY). The same band
   metric is read from the 4x BMP (x 64..896, y 384..600). Binary
   search finds the smallest N whose band is dark = window-open frame.
5. Report: t_scene (first lit frame) and t_window for both sides and
   the deltas. Frame 0 definitions differ (native = first mode-4 frame
   after the file flow; runtime = first room-loop frame after setup),
   so treat deltas as approximate; the metric that matters is
   t_window - t_scene, the reveal-pan + window-open latency.

Measured 2026-11-14 (ROM abd71fe0..., libmgba system, this checkout):
  native : t_scene ~= 40,  t_window ~= 210  (latency ~170)
  runtime: t_scene ~= 1,   t_window ~= 177  (latency ~176)
  -> after porting sub_0805CED0's case-0/1 gating (spawn camera wait,
     sub_0805CB74 MOSAIC transition) plus sub_0805D410's +0xB==0
     0x40-frame text suspend, the runtime window latency is within
     ~6 frames of native (was ~200 frames early before the port).
     Remaining delta: frame-0 origins differ and the +0xB==1 beat-1
     pan+tail suspend and later +0xB scene substates are only
     approximated by aos_opening_actors_blocks_script.

Usage:
  tests/compare_aos_opening_dialogue.py --rom "roms/Castlevania - Aria of Sorrow (USA).gba" [--keep]

Requires: build/aos_mgba_lab, build/fusion_aria_runtime, xvfb-run (or
DISPLAY), Python 3 + Pillow.
"""
import argparse
import os
import subprocess
import sys
import tempfile

try:
    from PIL import Image
except ImportError:
    sys.exit("Pillow is required (python3 -m pip install pillow)")

ROM_DEFAULT = "roms/Castlevania - Aria of Sorrow (USA).gba"
LAB_DEFAULT = "build/aos_mgba_lab"
RUNTIME_DEFAULT = "build/fusion_aria_runtime"

# --- fresh portable save (aos_save_init_new, schema v15) -----------------

SAVE_PAYLOAD = """metroidvania-aos-save-v15
slot=0
area=0
room=0
spawn_x=120
spawn_y=141
hp=320
max_hp=320
moves=0
play_frames=0
name=20,16,14,2,0,0,0,0
character=0
challenge_flags=0
experience=0
gold=0
weapon=0
armor=0
accessory=255
inventory=0000000000000000000000000000000010000000000000000000000000000000000000000000000000000000000100000000000000000000000000000000000000000000
souls=000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
souls_collected=0
souls_equipped=0,0,0
mp=-1
buttons=1,0,3,2
bestiary=00000000,00000000,00000000,00000000
drops=00000000,00000000,00000000,00000000,00000000,00000000,00000000,00000000
events=00000000,00000000,00000000,00000000,00000000,00000000,00000000,00000000
placed_items=00000000,00000000,00000000,00000000,00000000,00000000
special_souls=0000
map=00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
secrets=0000
progress_flags=00000000
"""


def fnv1a(data: bytes) -> int:
    h = 2166136261
    for b in data:
        h ^= b
        h = (h * 16777619) & 0xFFFFFFFF
    return h


def write_fresh_save(path: str) -> None:
    payload = SAVE_PAYLOAD.encode()
    with open(path, "wb") as f:
        f.write(payload)
        f.write(b"checksum=%08x\n" % fnv1a(payload))


# --- scout key schedule (mirrors aos_lab_worker.c scout phase 1+2) ------

def scout_keys() -> str:
    events = []
    for chunk in range(8):  # enough chunks to pass title/file/intro
        base = chunk * 600
        events.append((base, 0))
        if chunk % 3 == 0:
            events.append((base + 40, 8))
            events.append((base + 55, 0))
        for f in (120, 260, 400):
            events.append((base + f, 1))
            events.append((base + f + 12, 0))
        b2 = base + 520
        pulse = 16 if chunk % 10 < 7 else 32
        events.append((b2, pulse))
        if chunk % 6 == 5:
            events.append((b2 + 20, 64))
            events.append((b2 + 34, pulse))
        if chunk % 5 == 4:
            events.append((b2 + 30, 2 | pulse))
            events.append((b2 + 40, pulse))
        if chunk % 4 == 3:
            events.append((b2 + 45, 1 | pulse))
            events.append((b2 + 57, pulse))
        events.append((b2 + 79, 0))
    events.sort(key=lambda e: e[0])
    return ",".join(f"{f}:{m}" for f, m in events)


def run_lab(rom: str, frames: int, keys: str, workdir: str,
            extra: list) -> str:
    cmd = [LAB, "--rom", rom, "--frames", str(frames)]
    if keys:
        cmd += ["--keys", keys]
    cmd += extra
    proc = subprocess.run(cmd, capture_output=True, text=True)
    return proc.stdout + proc.stderr


def first_mode4_frame(rom: str, keys: str, workdir: str) -> int:
    """Peek gameMode byte every 60 frames in [2400,4400); return the
    first frame whose low byte reads 4 (gameplay), or -1."""
    lo, hi, step = 2400, 4400, 60
    peeks = ",".join(f"{f}:0x02000010:1" for f in range(lo, hi, step))
    out = run_lab(rom, hi, keys, workdir, ["--peek", peeks])
    modes = []
    for line in out.splitlines():
        if "02000010" in line:
            toks = line.split()
            # tokens after the address are hex bytes
            idx = toks.index("02000010")
            vals = [t for t in toks[idx + 1:] if all(c in "0123456789abcdefABCDEF" for c in t) and len(t) <= 2]
            if vals:
                modes.append(int(vals[0], 16))
    if len(modes) < 2:
        sys.exit("could not parse gameMode peeks from aos_mgba_lab output")
    for i, m in enumerate(modes):
        if m == 4:
            return lo + i * step
    return -1


# --- band metric ----------------------------------------------------------

def band_mean_native(ppm_path: str) -> float:
    im = Image.open(ppm_path).convert("RGB")
    band = im.crop((16, 96, 224, 150)).convert("L")
    px = list(band.getdata())
    return sum(px) / len(px)


def band_mean_bmp(bmp_path: str) -> float:
    im = Image.open(bmp_path).convert("RGB")
    # 960x640 capture = 4x native; WIN0 band x 16..224, y 96..150
    band = im.crop((64, 384, 896, 600)).convert("L")
    px = list(band.getdata())
    return sum(px) / len(px)


def classify(series):
    """series: list of (frame, mean) sorted by frame. The window band is
    lit early (scene visible, no chrome) and dark once the dialogue
    window's BLDY dim lands; the first third of the sweep is always
    pre-window, the last frames always post-window."""
    head = [m for _, m in series[: max(3, len(series) // 3)]]
    tail = [m for _, m in series[-6:]]
    head.sort()
    tail.sort()
    lit_med = head[len(head) // 2]
    dark_med = tail[len(tail) // 2]
    thresh_dark = lit_med - 0.5 * (lit_med - dark_med)
    floor = 20.0
    t_scene = next((f for f, m in series if m > floor), None)
    t_window = next((f for f, m in series if m < thresh_dark), None)
    return t_scene, t_window, lit_med, dark_med


# --- native measurement ---------------------------------------------------

def measure_native(rom: str, state: str, workdir: str) -> dict:
    caps = list(range(40, 280, 5)) + list(range(280, 520, 10))
    capspec = ",".join(map(str, caps))
    prefix = os.path.join(workdir, "nat")
    run_lab(rom, 520, "", workdir,
            ["--state-in", state, "--capture", capspec, "--shot", prefix])
    series = []
    for f in caps:
        p = f"{prefix}_{f:05d}.ppm"
        if os.path.exists(p):
            series.append((f, band_mean_native(p)))
    t_scene, t_window, lit, dark = classify(series)
    return {"t_scene": t_scene, "t_window": t_window,
            "lit": round(lit, 1), "dark": round(dark, 1),
            "series_head": series[:8], "series_tail": series[-8:]}


# --- runtime measurement ---------------------------------------------------

def runtime_band(runtime: str, rom: str, save: str, target: str,
                 workdir: str, frames: int) -> float:
    out = os.path.join(workdir, f"rt_{frames}.bmp")
    inner = [runtime, "--save", save, "--save-target", target, "--rom", rom,
             "--capture", out, str(frames), "0"]
    cmd = inner if os.environ.get("DISPLAY") else ["xvfb-run", "-a"] + inner
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if not os.path.exists(out):
        sys.exit(f"runtime capture failed at {frames} frames:\n{proc.stdout}\n{proc.stderr}")
    return band_mean_bmp(out)


def measure_runtime(runtime: str, rom: str, save: str, workdir: str) -> dict:
    target = os.path.join(workdir, "rt_save.sav")
    # exponential probe then binary search for first dark frame
    def dark(f):
        return runtime_band(runtime, rom, save, target, workdir, f) < 90.0

    probes = [1, 2, 3, 4, 6, 10, 20, 40, 80, 160, 240, 320]
    first_dark = None
    prev = None
    for f in probes:
        if dark(f):
            first_dark = f
            break
        prev = f
    if first_dark is None:
        return {"error": "window never detected within 320 frames"}
    lo = prev if prev is not None else 1
    hi = first_dark
    while hi - lo > 1:
        mid = (lo + hi) // 2
        if dark(mid):
            hi = mid
        else:
            lo = mid
    t_window = hi
    # t_scene: first frame whose band is lit (>20); window darkens it,
    # so probe the same binary-search style upward from 1
    def lit(f):
        return runtime_band(runtime, rom, save, target, workdir, f) > 20.0
    lo, hi = 1, min(t_window, 40)
    # find first lit frame in [1, hi]; if frame1 already lit, t_scene=1
    if lit(1):
        t_scene = 1
    else:
        while hi - lo > 1:
            mid = (lo + hi) // 2
            if lit(mid):
                hi = mid
            else:
                lo = mid
        t_scene = hi
    return {"t_scene": t_scene, "t_window": t_window}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--rom", default=ROM_DEFAULT)
    ap.add_argument("--lab", default=LAB_DEFAULT)
    ap.add_argument("--runtime", default=RUNTIME_DEFAULT)
    ap.add_argument("--workdir", default=None)
    ap.add_argument("--keep", action="store_true")
    args = ap.parse_args()

    global LAB
    LAB = args.lab
    for tool in (args.lab, args.runtime):
        if not os.path.exists(tool):
            sys.exit(f"missing tool: {tool}")
    if not os.path.exists(args.rom):
        sys.exit(f"missing ROM: {args.rom}")

    workdir = args.workdir or tempfile.mkdtemp(prefix="aos_dialogue_cmp_")
    os.makedirs(workdir, exist_ok=True)
    save = os.path.join(workdir, "fresh.sav")
    state = os.path.join(workdir, "room0.state")
    write_fresh_save(save)

    keys = scout_keys()
    f4 = first_mode4_frame(args.rom, keys, workdir)
    if f4 < 0:
        sys.exit("never reached gameplay mode 4 with the scout schedule")
    print(f"[native] first mode-4 frame ~= {f4}")
    run_lab(args.rom, f4 + 1, keys, workdir,
            ["--state-out", state])
    if not os.path.exists(state):
        sys.exit("state-out failed")

    nat = measure_native(args.rom, state, workdir)
    rt = measure_runtime(args.runtime, args.rom, save, workdir)

    print("\n=== comparison: area 0 room 0 opening dialogue window ===")
    print(f"native : t_scene={nat['t_scene']} t_window={nat['t_window']} "
          f"(lit={nat['lit']} dark={nat['dark']})")
    print(f"runtime: t_scene={rt.get('t_scene')} t_window={rt.get('t_window')}")
    if nat["t_scene"] and nat["t_window"] and rt.get("t_window"):
        n_lat = nat["t_window"] - nat["t_scene"]
        r_lat = rt["t_window"] - rt["t_scene"]
        print(f"latency (t_window - t_scene): native {n_lat} vs runtime {r_lat} "
              f"-> delta {n_lat - r_lat:+d} frames")
    report = os.path.join(workdir, "comparison.txt")
    with open(report, "w") as f:
        f.write(f"native={nat}\nruntime={rt}\n")
    print(f"artifacts in {workdir} (report: {report})")
    if not args.keep and not args.workdir:
        pass  # keep tempdir for inspection; print path above


if __name__ == "__main__":
    main()
