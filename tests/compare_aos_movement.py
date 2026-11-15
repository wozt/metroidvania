#!/usr/bin/env python3
"""Native-vs-runtime comparison: Aria of Sorrow player movement/physics.

Cheapest gameplay-layer comparison: hold RIGHT (GBA mask 16 ==
AOS_KEY_RIGHT) while tapping A every --a-every frames (the taps keep
room/epilogue dialogue windows advancing so gameplay input is not
masked — pure direction holds leave +0x1C masked indefinitely) and
record the player position every frame on both sides.

Protocol (artifacts stay in --workdir; ROM/save-state/frame files are
never committed):

1. Native side: `aos_mgba_lab` boots the real ROM with the scout key
   schedule to reach gameplay (mode byte 0x02000010 == 4), saves an
   mGBA state = "room entry". A second run replays the proven
   post-story key schedule from compare_aos_object20_dialogue.py
   (A taps every 60 frames + combined A+B pulses every --pulse frames;
   the B/UP edges are required by the epilogue proximity talks) for
   --story-frames and saves "free.state". A third "settle" run of A
   taps closes leftover text windows -> "clean.state". A fourth "walk"
   run holds RIGHT with the same A-tap cadence for --walk-frames,
   walking Soma through the room 0/0 exit into room 0/3 -> "entered.state".
   A fifth "entry settle" run of pure A taps for --entry-settle-frames
   lets the native room-entry sequence finish (the fade and the wooden
   door's pass/close hold player input via gEwramData +0x42C for ~170
   frames after the transition commits) -> "moved.state". Measuring
   before that settle compares settled runtime movement against a
   native side that is still input-locked.
2. Native measurement: from moved.state, RIGHT held from frame 0 with
   A taps every --a-every frames; per-frame peeks of 0x02000524:8 give
   posX,posY as 16.16 fixed point (entityData[0] +0x40/+0x44; see
   docs/AOS_SOMA.md). The session samples a peek after the emulated
   frame whose index matches the request, so peek frame f ~= runtime
   "after f+1 updates".
3. Runtime side: `fusion_aria_runtime --simulate N --simulate-script
   S --save <fresh> --area 0 --room 3 --spawn X Y --mute`. The
   simulate path is headless (no SDL), deterministic and prints the
   final integer Soma position; running it once per N in [1,frames]
   yields the per-frame trajectory. The script file encodes the same
   RIGHT + periodic-A schedule via timed spans. --spawn matches the
   native position measured at moved.state (integer pixels; the
   native 16.16 subpixel remainder is dropped, <=1px).
4. Report: per-frame posX series for both sides, per-frame deltas,
   acceleration profile, max speed, and divergence metrics.

Native movement reference (docs/AOS_SOMA.md, sub_0801B0D8 case 0):
walk speed 1.5 px/frame (4.0 with the fast-walk flag), position is
16.16 fixed point integrated per frame.

Measured 2026-11-15 (see comparison.json in workdir for numbers).

Usage:
  tests/compare_aos_movement.py --rom "roms/Castlevania - Aria of Sorrow (USA).gba" [--keep]

Requires: build/aos_mgba_lab, build/fusion_aria_runtime, Python 3.
"""
import argparse
import importlib.util
import os
import re
import struct
import subprocess
import sys
import tempfile

_HERE = os.path.dirname(os.path.abspath(__file__))


def _load(name, filename):
    spec = importlib.util.spec_from_file_location(
        name, os.path.join(_HERE, filename))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


_opening = _load("compare_aos_opening_dialogue",
                 "compare_aos_opening_dialogue.py")
_obj20 = _load("compare_aos_object20_dialogue",
               "compare_aos_object20_dialogue.py")

ROM_DEFAULT = _opening.ROM_DEFAULT
LAB_DEFAULT = _opening.LAB_DEFAULT
RUNTIME_DEFAULT = _opening.RUNTIME_DEFAULT

POS_ADDR = 0x02000524          # entityData[0]+0x40 posX, +0x44 posY (16.16)
ROOM_ADDR = 0x02000090         # area=byte14, room=byte15

STORY_FRAMES = 30000           # post-story key schedule length
SETTLE_FRAMES = 900            # A-tap run closing leftover windows
WALK_FRAMES = 420              # RIGHT+A walk into room 0/3
ENTRY_SETTLE_FRAMES = 420      # A-tap run letting the room entry settle
MEASURE_FRAMES = 200           # measured RIGHT+A run length
A_EVERY = 90                   # A-tap cadence while walking/measuring

RIGHT = 16                     # mGBA mask bit == AOS_KEY_RIGHT (1<<4)
LEFT = 32                      # == AOS_KEY_LEFT (1<<5)
A = 1


def tap_keys(frames: int, base: int, every: int, tap_mask: int,
             tap_len: int = 3) -> str:
    """`base` mask held throughout; `tap_mask` OR-ed in for tap_len
    frames every `every` frames (merged, later event wins)."""
    events = [(0, base)]
    for f in range(every, frames, every):
        events.append((f, base | tap_mask))
        events.append((f + tap_len, base))
    events.sort(key=lambda e: e[0])
    merged = {}
    for f, m in events:
        merged[f] = m
    return ",".join(f"{f}:{m}" for f, m in sorted(merged.items()))


def settle_keys(frames: int) -> str:
    """Pure A taps every 30 frames (3-frame press); advances/closes any
    open text window without retriggering room/epilogue talks."""
    return tap_keys(frames, 0, 30, A)


def parse_peek_blocks(text: str):
    """Parse `peek ADDR size N:` hex dumps into (addr, bytes) pairs in
    request order (same shape as the object20 comparison parser)."""
    pat = re.compile(r"peek ([0-9a-f]{8}) size (\d+):\n"
                     r"((?:  [0-9a-f]{8}  .*\n)+)")
    out = []
    for m in pat.finditer(text):
        data = bytearray()
        for line in m.group(3).splitlines():
            hexpart = line.split("  ", 2)[-1]
            for t in hexpart.split():
                if re.fullmatch(r"[0-9a-fA-F]{2}", t):
                    data.append(int(t, 16))
        out.append((int(m.group(1), 16), bytes(data)))
    return out


def pos_from_block(block: bytes):
    """8-byte peek at 0x02000524 -> (posX 16.16, posY 16.16)."""
    if not block or len(block) < 8:
        return None
    x, y = struct.unpack_from("<II", block, 0)
    return x, y


def run_lab(rom, frames, keys, workdir, extra):
    return _opening.run_lab(rom, frames, keys, workdir, extra)


def band_dark(ppm_path: str) -> bool:
    return _opening.band_mean_native(ppm_path) < 90.0


def ensure_native_states(rom, workdir, lab, story_frames, pulse,
                         settle_frames, walk_frames, direction,
                         entry_settle_frames):
    """Boot to gameplay and walk to a clean measured room. Returns
    (moved_state_path, pos16.16, room "a/r")."""
    room_state = os.path.join(workdir, "room0.state")
    free_state = os.path.join(workdir, "free.state")
    clean_state = os.path.join(workdir, "clean.state")
    entered_state = os.path.join(workdir, "entered.state")
    moved_state = os.path.join(workdir, "moved.state")
    keys = _opening.scout_keys()
    f4 = _opening.first_mode4_frame(rom, keys, workdir)
    if f4 < 0:
        sys.exit("never reached gameplay mode 4 with the scout schedule")
    print(f"[native] first mode-4 frame ~= {f4}")
    _opening.LAB = lab
    run_lab(rom, f4 + 1, keys, workdir, ["--state-out", room_state])
    if not os.path.exists(room_state):
        sys.exit("room-entry state-out failed")
    skeys = _obj20.post_story_keys(pulse, story_frames)
    run_lab(rom, story_frames, skeys, workdir,
            ["--state-in", room_state, "--state-out", free_state])
    if not os.path.exists(free_state):
        sys.exit("free-roam state-out failed")
    run_lab(rom, settle_frames, settle_keys(settle_frames), workdir,
            ["--state-in", free_state, "--state-out", clean_state])
    base = RIGHT if direction == "right" else LEFT
    wkeys = tap_keys(walk_frames, base, A_EVERY, A)
    out = run_lab(rom, walk_frames, wkeys, workdir,
                  ["--state-in", clean_state, "--state-out", entered_state])
    if not os.path.exists(entered_state):
        sys.exit("entered state-out failed")
    # The walk ends the frame the room-0/3 transition commits, so the
    # saved state still sits inside the native entry sequence: the fade
    # (gEwramData +0x42C bit 15) suppresses all input for ~130 frames
    # and the wooden door's pass/close then masks player input for
    # ~40 more (Object00 sets +0x42C bits 0x101 while Soma passes).
    # Settle with pure A taps so moved.state matches the settled
    # gameplay the runtime --simulate path models.
    ekeys = settle_keys(entry_settle_frames)
    out = run_lab(rom, entry_settle_frames, ekeys, workdir,
                  ["--state-in", entered_state, "--state-out", moved_state,
                   "--peek", f"-1:{POS_ADDR:x}:8,-1:{ROOM_ADDR:x}:16"])
    if not os.path.exists(moved_state):
        sys.exit("moved state-out failed")
    pos = room = None
    for a, d in parse_peek_blocks(out):
        if a == POS_ADDR:
            pos = pos_from_block(d)
        elif a == ROOM_ADDR and len(d) >= 16:
            room = f"{d[14]}/{d[15]}"
    print(f"[native] moved state pos={pos} room={room}")
    return moved_state, pos, room


def native_measure(rom, state, workdir, lab, frames, direction):
    """Hold `direction` with A taps every A_EVERY frames; per-frame
    posX/posY peeks. Returns list of (frame, x16.16, y16.16)."""
    base = RIGHT if direction == "right" else LEFT
    keys = tap_keys(frames, base, A_EVERY, A)
    peeks = ",".join(f"{f}:{POS_ADDR:x}:8" for f in range(frames))
    out = run_lab(rom, frames, keys, workdir,
                  ["--state-in", state, "--peek", peeks])
    blocks = parse_peek_blocks(out)
    series = []
    pos_blocks = [d for a, d in blocks if a == POS_ADDR]
    for f, data in enumerate(pos_blocks[:frames]):
        p = pos_from_block(data)
        if p:
            series.append((f, p[0], p[1]))
    return series, out


def write_sim_script(path: str, frames: int, direction: str) -> None:
    """Runtime --simulate-script matching tap_keys(): base direction
    held, A OR-ed in for 3 frames every A_EVERY frames. Timed spans
    override the base; 'tap 19 3' (RIGHT|A) then the base resumes, so
    each cycle is 'tap <dir|A> 3' followed by 'tap <dir> <every-3>' to
    keep the cursor aligned while holding the base mask."""
    base = 16 if direction == "right" else 32
    lines = [f"hold {base}"]
    cursor = 0
    f = A_EVERY
    while f < frames:
        # advance cursor to f with base mask held
        gap = f - cursor
        if gap > 0:
            lines.append(f"tap {base} {gap}")
            cursor = f
        lines.append(f"tap {base | A} 3")
        cursor += 3
        f += A_EVERY
    with open(path, "w") as fh:
        fh.write("\n".join(lines) + "\n")


def runtime_measure(runtime, rom, save, workdir, frames, direction,
                    spawn, area, room):
    """Per-frame integer positions via repeated headless --simulate
    runs (deterministic; final 'Soma x,y' line is the position after N
    frames)."""
    script = os.path.join(workdir, "sim_move.txt")
    write_sim_script(script, frames, direction)
    series = []
    # N=0 is not a valid --simulate run (0 falls through to the SDL
    # path); simulate N frames == native peek frame N-1, so N in
    # [1, frames] yields samples labelled 0..frames-1.
    for n in range(1, frames + 1):
        proc = subprocess.run(
            [runtime, "--simulate", str(n), "--simulate-script", script,
             "--save", save, "--area", str(area), "--room", str(room),
             "--spawn", str(spawn[0]), str(spawn[1]), "--mute"],
            capture_output=True, text=True)
        m = re.search(r"Soma (\d+),(\d+)", proc.stdout)
        if not m:
            sys.exit(f"runtime simulate failed at N={n}:\n"
                     f"{proc.stdout}\n{proc.stderr}")
        series.append((n - 1, int(m.group(1)), int(m.group(2))))
    return series


def summarize(series, fixed=False):
    """series: list of (frame, x, y); x,y are 16.16 when fixed else px."""
    if not series:
        return {}
    shift = 16 if fixed else 0
    xs = [s[1] >> shift for s in series]
    ys = [s[2] >> shift for s in series]
    dx = [xs[i + 1] - xs[i] for i in range(len(xs) - 1)]
    dy = [ys[i + 1] - ys[i] for i in range(len(ys) - 1)]
    return {
        "n": len(series),
        "x0": xs[0], "y0": ys[0],
        "xend": xs[-1], "yend": ys[-1],
        "dx_first10": dx[:10],
        "dx_last10": dx[-10:],
        "dy_first10": dy[:10],
        "max_dx": max(dx) if dx else 0,
        "min_dx": min(dx) if dx else 0,
        "max_abs_dy": max(abs(d) for d in dy) if dy else 0,
        "total_dx": xs[-1] - xs[0],
        "total_dy": ys[-1] - ys[0],
        "xs": xs, "ys": ys,
    }


def main():
    global A_EVERY
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--rom", default=ROM_DEFAULT)
    ap.add_argument("--lab", default=LAB_DEFAULT)
    ap.add_argument("--runtime", default=RUNTIME_DEFAULT)
    ap.add_argument("--workdir", default=None)
    ap.add_argument("--keep", action="store_true")
    ap.add_argument("--story-frames", type=int, default=STORY_FRAMES)
    ap.add_argument("--settle-frames", type=int, default=SETTLE_FRAMES)
    ap.add_argument("--walk-frames", type=int, default=WALK_FRAMES)
    ap.add_argument("--entry-settle-frames", type=int,
                    default=ENTRY_SETTLE_FRAMES,
                    help="A-tap frames letting the room-0/3 entry "
                         "transition/door pass settle before moved.state")
    ap.add_argument("--measure-frames", type=int, default=MEASURE_FRAMES)
    ap.add_argument("--a-every", type=int, default=A_EVERY,
                    help="frames between A taps while walking")
    ap.add_argument("--pulse", type=int, default=500,
                    help="A+B pulse period inside the story schedule")
    ap.add_argument("--direction", default="right",
                    choices=["right", "left"])
    ap.add_argument("--skip-native", action="store_true")
    ap.add_argument("--skip-runtime", action="store_true")
    ap.add_argument("--state", default=None,
                    help="reuse an existing moved.state mGBA state")
    args = ap.parse_args()

    A_EVERY = args.a_every

    for tool in (args.lab, args.runtime):
        if not os.path.exists(tool):
            sys.exit(f"missing tool: {tool}")
    if not os.path.exists(args.rom):
        sys.exit(f"missing ROM: {args.rom}")

    workdir = args.workdir or tempfile.mkdtemp(prefix="aos_move_cmp_")
    os.makedirs(workdir, exist_ok=True)
    _opening.LAB = args.lab
    save = os.path.join(workdir, "fresh.sav")
    _opening.write_fresh_save(save)

    report = {"workdir": workdir, "direction": args.direction,
              "measure_frames": args.measure_frames,
              "a_every": args.a_every}

    moved_state = args.state
    native_pos = None
    room_str = None
    if not args.skip_native:
        if moved_state:
            out = run_lab(args.rom, 1, "", workdir,
                          ["--state-in", moved_state, "--peek",
                           f"-1:{POS_ADDR:x}:8,-1:{ROOM_ADDR:x}:16"])
            for a, d in parse_peek_blocks(out):
                if a == POS_ADDR:
                    native_pos = pos_from_block(d)
                elif a == ROOM_ADDR and len(d) >= 16:
                    room_str = f"{d[14]}/{d[15]}"
        else:
            moved_state, native_pos, room_str = ensure_native_states(
                args.rom, workdir, args.lab, args.story_frames,
                args.pulse, args.settle_frames, args.walk_frames,
                args.direction, args.entry_settle_frames)
        print(f"[native] moved state={moved_state} pos={native_pos} "
              f"room={room_str}")

        nat_series, nat_raw = native_measure(
            args.rom, moved_state, workdir, args.lab,
            args.measure_frames, args.direction)
        with open(os.path.join(workdir, "native_peeks.txt"), "w") as fh:
            fh.write(nat_raw)
        report["native"] = summarize(nat_series, fixed=True)
        report["native_room"] = room_str
        ns = report["native"]
        print(f"[native] {ns['n']} samples: x {ns['x0']}->{ns['xend']} "
              f"y {ns['y0']}->{ns['yend']} total_dx={ns['total_dx']} "
              f"max_dx={ns['max_dx']} min_dx={ns['min_dx']}")
        print(f"[native] dx head={ns['dx_first10']} tail={ns['dx_last10']}")
        print(f"[native] dy head={ns['dy_first10']}")

    if not args.skip_runtime:
        if native_pos is None:
            sys.exit("runtime side needs the native position; run "
                     "native first or pass --state")
        spawn = (native_pos[0] >> 16, native_pos[1] >> 16)
        area, room = (0, 3)
        if room_str:
            try:
                area, room = (int(t) for t in room_str.split("/"))
            except ValueError:
                pass
        rt_series = runtime_measure(args.runtime, args.rom, save,
                                    workdir, args.measure_frames,
                                    args.direction, spawn, area, room)
        report["runtime"] = summarize(rt_series, fixed=False)
        rs = report["runtime"]
        print(f"[runtime] room={area}/{room} spawn={spawn} "
              f"{rs['n']} samples: x {rs['x0']}->{rs['xend']} "
              f"y {rs['y0']}->{rs['yend']} total_dx={rs['total_dx']} "
              f"max_dx={rs['max_dx']} min_dx={rs['min_dx']}")
        print(f"[runtime] dx head={rs['dx_first10']} tail={rs['dx_last10']}")

    if "native" in report and "runtime" in report:
        ns, rs = report["native"], report["runtime"]
        n = min(len(ns["xs"]), len(rs["xs"]))
        diffs = [ns["xs"][i] - rs["xs"][i] for i in range(n)]
        report["divergence"] = {
            "x_diff_first": diffs[0] if diffs else None,
            "x_diff_last": diffs[-1] if diffs else None,
            "x_diff_max_abs": max(abs(d) for d in diffs) if diffs else None,
            "frames_compared": n,
        }
        print("\n=== divergence (native_x - runtime_x, integer px) ===")
        print(f"  first={diffs[0] if diffs else None} "
              f"last={diffs[-1] if diffs else None} "
              f"max_abs={max(abs(d) for d in diffs) if diffs else None}")

    out_path = os.path.join(workdir, "comparison.json")
    import json
    with open(out_path, "w") as fh:
        json.dump(report, fh, indent=2, default=str)
    print(f"\nartifacts in {workdir} (report: {out_path})")


if __name__ == "__main__":
    main()
