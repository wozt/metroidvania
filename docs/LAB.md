# Aria of Sorrow — Autonomous Learning Lab

Design document for the emulator laboratory in `src/tools/`. The lab's
enduring objective: **finish Castlevania: Aria of Sorrow on its own**,
playing strictly glitchless casual routes (no zips, no 0 HP exploit, no
sequence breaks), using only the real ROM, real inputs and local
instrumentation. Everything is C + SQLite; no ROM data or proprietary
assets are committed.

## Reference route (glitchless)

Campaign skeleton used as progress prior:
Chapel → Manticore room → Study → Great Armor → Arena → Balore →
Claimh → Top Floor Graham → Chaos. Waypoints are discovered, not
hardcoded: the fleet advances by reward, rooms and checkpoints.

## Game knowledge encoded in the code

`aos_lab_worker.c` embeds the rules the AI needs:

- **Controls** (GBA masks): A=1 attack / advance text, B=2 jump /
  cancel, SELECT=4, START=8 pause menu, arrows 16/32/64/128
  (right/left/up/down), R=256, L=512 ability buttons.
- **Menu discipline**: START only before gameplay mode 4; never during
  play; B closes accidental menus; dialogue advances with A.
- **Movement policy**: pulses biased RIGHT (early route pushes
  right), LEFT ~30% for backtracking; B jump every 5th chunk; A attack
  every 4th chunk while moving; UP tap every 6th chunk because doors
  open with UP.
- **WRAM map** (verified against the decomp):
  gameMode `0x02000010`, song/area/room block `0x02000090`
  (area=+14, room=+15), player pointer via
  `gEwramData->unk_13110` → entity `+0x524` (unreliable — motion is
  measured from the framebuffer instead), level `0x02013279`,
  HP/MP `0x0201327A..80`, XP/gold `0x0201328C`, items `0x02013294`,
  souls `0x0201331C`. VRAM `0x06000000` (96 KiB) feeds tilemap OCR.

## Architecture

- `aos_mgba_session` — synchronous mGBA core wrapper: state blob
  load/save, key schedules, memory peeks, framebuffer mirror, video
  register snapshot (`DISPCNT`/`BGCNT0..3` from `memory.io[]`).
- `aos_lab_vision` — per-frame signatures: scene/HUD tile hashes,
  ink rows, tile stats, RGB histogram, center-screen motion +
  direction, 32×32 tilemap hashes per text BG, glyph bitmap dumps.
- `aos_lab_db` — SQLite (WAL, effectively RAM-resident:
  `synchronous=OFF`, `temp_store=MEMORY`, `mmap_size=1 GiB`,
  64 MiB page cache). Tables: meta, runs, states (blobs), actions,
  weights, trials. Meta keys coordinate the fleet: `lab.rooms`
  (global room registry), `lab.dead.<state>` (lethal branches),
  `lab.macros`, `worker.N.*` (phase/status/hb/rsum/rn/speed/launch),
  `lab.control.N` (pause/resume/stop/best), `lab.bg`.
- `aos_lab_worker` — two modes:
  - **scout**: boots the ROM, plays chunks (text advance → directional
    pulse → room/scene capture), validates free gameplay via
    framebuffer motion (≥0.8% center pixels, 2 consecutive chunks in
    mode 4), saves `scout_play_*` checkpoints and capped scene dumps.
  - **trials**: bandit over actions weighted by mean reward; waypoint
    restarts (~25%, prefers `play` states, skips lethal branches);
    smart restart hops to the fleet's best-value checkpoint.
- `aos_lab_viewer` — GTK4 supervisor: 4×2 live framebuffer grid,
  per-worker Pause/Stop/Play/Best + speed, fleet controls, state
  picker (launch-worthy states only), live registry tables, in-memory
  log, settings (background mode), best-worker green highlight.

## Reward model

Trial reward = action probe score (0/0.5/1) + exploration bonus,
clamped to [-1.5, 4.0]. Bonus tags:

| Tag | Trigger | Points |
|---|---|---|
| new_room | first fleet visit to (area,room) | +0.8 (+depth) |
| clean_traverse | room change, no damage | +0.5 |
| new_area_song | song/map change | +0.6 |
| new_area | area byte change | +0.5 |
| mode_change | gameMode change | +0.5 |
| item_found / soul_gained | inventory diff | +0.6 / +0.5 |
| level_up / exp_gain / gold_gain | progression | +1.0 / +0.5 / +0.3 |
| healed / ability_used | HP/MP gain | +0.4 / +0.1 |
| new_scene | unseen visual hash | +0.4 |
| clean_move | screen motion, no damage | +0.2 |
| damage | HP loss fraction | −0.3 → −1.0 |
| death | HP reads 0 at end of run | −1.0 + branch ban |
| exit_gameplay | leaves mode 4 (game over / menu) | −0.6 |
| enter_gameplay | enters mode 4 | +0.3 |
| gameover_screen | lands on a hash in lab.deathscenes | −0.8 |
| room_revisit | enters an already-known room | −0.05 |
| mp_empty | MP fully drained | −0.1 |
| no_progress | no motion, no reward, no penalty | −0.1 |
| exp_big | XP jump > 64 (kills / boss) | +0.4 extra |
| enemy_down | entity slots emptied inside a stable scene (kills; scene transitions are despawns, not kills) | +0.3 each, +0.6 if >2 |
| enemy_spawn | more entity slots filled than emptied (stable scenes only) | −0.1 |
| tile_change | VRAM tilemap diff (text/HUD) when scene unchanged | +0.25 |
| lab.progress | rooms / max level / distance / weighted score | campaign metric |
| lab.progress_score | weighted advancement events (see branch mgmt) | consolidation trigger |

The bandit ranks actions by `reward_sum/attempts`, not binary success.
Explore rate is spread per slot (10–25% by worker id, `AOS_LAB_EXPLORE`
override) so the fleet covers both exploit and explore regimes instead
of running four identical bandits. `explore-input` (−4, random key
mashing) is excluded from exploitation — it wins reward averages by
luck and starves real actions; it only competes inside the random
explore pool and the per-action warmup floor.

## Branch management

- **Weighted progress score** (`lab.progress_score`): advancement
  only — new_room/new_area +10, new_area_song +4, level_up +5,
  exp_big +4, exp_gain/item/soul +1, enemy_down/healed +0.5;
  damage/death/room_revisit/no_progress subtract. Free-roam motion,
  tile churn and item vacuuming score zero — wandering is not
  progress. Scouts add +10 per genuinely new room.
- **Frontier consolidation**: when `lab.progress_score` grows
  ~10% +5 since the last consolidation, `aos_labdb_consolidate` keeps only
  the promising branch — ancestors of the Elo-best state (via
  `parent_id`), states whose trials scored >0.5, and the newest 80
  checkpoints — drops every other state and orphaned trial, caps the
  log at 2500 rows, then publishes `lab.reset_state=<best id>`.
  Every worker (scouts included) polls that key and relocates to the
  promoted branch, so the whole fleet continues from the frontier.
- **Waypoint restarts**: ~25% of trials reload a random stored state
  (60% preference for `scout_play_*` when present).
- **Smart restart**: every 60 trials (and on demand via the Best
  button / `lab.control.N=best`) the worker compares its reward EMA to
  `aos_labdb_best_state` (state whose trials reached max reward) and
  hops when clearly behind; a 400-trial barren streak also hops
  (max 8 hops) before declaring stuck.
- **Lethal branches**: a trial ending in `death`, or leaving
  gameplay mode with damage (game overs reload the save so HP alone
  misses them), marks `lab.dead.<state_id>`; waypoint picks and smart
  hops skip those states. The exact `(action, params)` combo is also
  counted in `lab.lethal.*` — `best_params` subtracts a full point per
  proven kill so killer combos sink below neutral rivals.
- **Mode-tagged states**: scout and auto saves now record
  `mode=N` in the state note; waypoint picks prefer `mode=4`
  (gameplay) states so trials stop wasting runs inside menus and
  cutscenes.
- **Mode byte fix**: `compute_bonus` now reads only the low byte of
  the gameMode peek — the upper bytes are frame counters that made
  every trial look like a mode change and masked enter/exit_gameplay.
- **Adaptive traversal** (`kind=traverse`): the run plays as
  ~60-frame segments holding a direction with a 720-frame minimum
  budget (~12 s — a real room crossing with fights); a frozen
  segment (screen motion < 0.3%) climbs a reaction ladder — jump,
  then attack, then UP for doors — and every 4th cruise segment also
  holds UP since Aria doors open on UP and smooth walks never stall.
  Every 3rd cruise segment also attacks proactively to clear the
  path, and each segment's HP peek feeds a damage reaction: a drop
  forces a counter-attack segment — real reactive combat from WRAM,
  no image analysis needed. Reward peeks merge across segments so
  `compute_bonus` sees the whole run.
- **Entity activity**: a 9th reward peek diffs the 8 KiB entity
  table (0x02000400) — `entity_active` (+0.1) when enemies/AI are
  churning, `entity_still` (−0.05) when gameplay mode shows a dead
  world.
- **Depth bias**: half of play-checkpoint waypoint picks take the
  deepest `scout_play_NNN` suffix available — trials push the
  frontier instead of re-farming early rooms.
- **Branch Elo**: branch strength is a *relative* rating, not an
  absolute score. Every trial is a match between its branch and the
  previous trial's branch (K=32, start 1500); winners gain, losers
  drop. `lab.beststate="id:elo"` tracks the champion and
  `aos_labdb_elo_best` feeds smart hops and waypoint picks — a
  consistently good branch beats a one-off lucky reward, and the
  whole fleet converges on the Elo leader.
- **Rewind**: any trial scoring < −0.10 earns one corrective retry —
  the branch state is reloaded and an evasive input (attack tap then
  an alternating direction hold) replays the same frame budget. The
  retry logs as its own trial (`action_id −99`, `rewind:evasive`) so
  failures stay on record while recovered runs still bank reward.
- **Failure memory**: `weights.reward_sum` keeps every tried combo's
  score; the bandit ranks params by *mean reward* (not just success
  rate) and the purge only deletes combos tried ≤2 times with zero
  reward — negative rows are the "do not retry" map.
- **Death-scene registry**: `lab.deathscenes` keeps hashes of screens
  seen at lethal outcomes; landing on a known game-over screen costs
  −0.8 (`gameover_screen`). This is the visual half of death
  detection — the OCR glyph decoder will upgrade it to real text
  recognition.
- **Auto-purge** (every 200 trials + manual/CLI): zero-reward fail
  trials, hopeless weight combos, capped scene states, dedup of
  same-named checkpoints, top-300 states by value
  (reward + scout_play/manual bonus − scene-dump penalty), orphaned
  trials removed, trial log capped at 8000 rows.

## Macro synthesis (long sequences from short ones)

Every 250 trials `maybe_make_macro` fuses the two best actions by
accumulated reward into a reusable `script` action named
`macro_<a>+<b>`: hold-style params become `frame:mask` events, the
second sequence is offset by the first's duration + 20 frames. The
bandit weighs macros like any action, so proven primitives (walk,
jump, attack, menu taps) compound into longer campaigns. Capped at 40
macros (`lab.macros`).

## Viewer lifecycle

Workers are child processes tracked by pid per slot. Closing the
window sends cooperative stop + SIGTERM to all children **unless** the
Settings "Background workers" switch (`lab.bg`, off by default) is on —
then workers keep running unsupervised and a later session reattaches
through the database and `/dev/shm/aoslab_fb_*` segments. Play buttons
respawn dead workers from `worker.N.launch`.

## Known gaps / roadmap

- **OCR validation**: tilemap hashes + glyph dumps exist; decoding
  glyphs→characters needs a dialogue-visible state and correlation
  with known Aria text (decomp `charmap.txt` is empty — rebuild the
  table by observation).
- **Boss/monster/item detectors**: WRAM entity tables and kill
  detection are not yet instrumented; rewards currently infer progress
  from vitals/inventory/rooms only.
- **Completion metric**: `lab.progress` tracks rooms visited + max
  level fleet-wide and shows in the viewer score line; boss kills and
  area milestones are not yet folded in.
- **Worker culling**: implemented — every 300+ trials a worker ranks
  its mean reward against the fleet (worker.N.rsum/rn metas) and stops
  itself when bottom-half, clearly behind the best (>0.5) and
  producing nothing (<0.15 mean), freeing the slot for a better
  candidate.
- **Macro v2**: macros are unordered pairs today; sequencing with
  conditions ("if room X then…") and macro-of-macros are future work.
