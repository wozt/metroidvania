# Handoff: finishing the Aria enemy and soul code reorganisation

Audience: the next coding agent (Codex) continuing this refactor. Read
`AGENTS.md` first; its cycle, registry and commit rules apply unchanged.

## Why

All 63 ported Aria monsters lived in one file, `src/runtime/aos_enemy.c`
(16,586 lines), while 26 files named after monsters
(`aos_skill_*.c`, `aos_guardian_*.c`) are not monsters at all: they are the
**souls** Soma gets from them (red-soul attacks and blue-soul familiars).
Part of the soul code also sits inline in `src/runtime/aria_runtime.c`
(about 8,100 lines). The goal is one obvious place per monster and per soul.

## Done in the first pass (patch 0885)

1. **Monsters split by file, behaviour unchanged.**
   `aos_enemy.c` now holds only the shared framework, in this order:
   - the soul-attack, decoy and burst state, particles, flames, math
     helpers, `play`/`step_anim`, targeting;
   - the bat (included from `aos_enemies/bat.c`), the zombie's prototypes
     and the big `enemy_create`;
   - the collision pass (`frame_boxes`, `collide`) and `generic_death`;
   - the common movement helpers that used to be scattered between
     monsters (walker collision, aiming, `integrate`/`stop`, projectile and
     fall collision, flight, ground probes, water probes,
     `copy_collision_result`, `merge_enemy_hit`);
   - `#include "aos_enemies/<family>.c"` for each of the 52 monster-family
     files, in their original order;
   - the update dispatch (`early_enemy_*`, `aos_enemy_update`,
     `enemy_update`).
   The fragments are **not** translation units. They are included by
   `aos_enemy.c` and use its `static` helpers. They are not listed in
   CMake. Each starts with a comment saying so.
   Families sharing native code stay together: `crow.c` (blue and red
   crow), `harpy.c` (Harpy and Siren), `gorgon.c` (Gorgon and Catoblepas),
   `curly.c` (Curly and Durga), `valkyrie.c` (Valkyrie and Erinys),
   `gargoyle.c` (Gargoyle and Gremlin), `skull_millione.c` (Skull
   Millione, Cagnazzo, Lubicant), `worm.c` (Giant and Poison Worm),
   `minotaur.c` (both Minotaurs), `tiny_devil.c` (Tiny Devil and Imp).
   `killer_doll.c` also holds the Lightning Doll's piece table (it shares
   one comment with the Killer Doll's).
2. **Proof of no change.** Both versions were compiled at `-O0` and every
   function's size and normalised disassembly was compared: all 488
   symbols are identical. The only difference is `angle_to_target`'s stack
   padding, because its callee `aos_arctan2` moved to another file.
   Procedure to repeat for every later step:
   ```sh
   git show HEAD~1:src/runtime/aos_enemy.c > /tmp/old_enemy.c   # adjust the revision
   cc -Iinclude -Isrc/runtime -std=c11 -O0 -c /tmp/old_enemy.c -o /tmp/old.o
   cc -Iinclude -Isrc/runtime -std=c11 -O0 -c src/runtime/aos_enemy.c -o /tmp/new.o
   for v in old new; do
     objdump -d --no-show-raw-insn -r /tmp/$v.o | awk '/^[0-9a-f]+ <.*>:$/{n=$2}
       /^ /{sub(/^ *[0-9a-f]+:\t/,""); gsub(/[0-9a-f]+ <[^>]*>/,"X"); print n"\t"$0}' |
       sort > /tmp/$v.txt
     nm -S /tmp/$v.o | awk 'NF==4{print $4,$2}' | sort > /tmp/$v.sizes
   done
   diff /tmp/old.sizes /tmp/new.sizes && diff -q /tmp/old.txt /tmp/new.txt
   ```
   Once functions move to separate object files, compare the union of all
   the new objects' symbols with the old object's, as was done for
   `aos_math`.
3. **`aos_math.{c,h}`** now holds `aos_sine` (with `sSineTable`) and
   `aos_arctan2`. Every soul module that included `aos_enemy.h` only for
   them now includes `aos_math.h`. `aos_enemy.h` includes `aos_math.h`, so
   existing callers still compile. `aos_math.c` was added next to every
   `aos_enemy.c` in `CMakeLists.txt`.
4. **Registries.** The `local_sources` of the monster rows in
   `data/native_parity/annotations.tsv` list `aos_enemy.c` plus their
   family file. The soul rows also list `aos_math.c`. Four
   `data/dashboard/features.tsv` rows (White Dragon, Peeping Eye) list
   their family file.

## Remaining work, in order

1. **Make the fragments real translation units.**
   - *(Started in patch 0891.)* `sky_fish.c` is now a real translation unit.
     `aos_enemy_internal.h` exposes its seven required framework helpers
     with collision-safe `aos_enemy_` names, and `AOS_ENEMY_SOURCES` adds it
     to every target. The old monolithic object and the union of the two new
     objects have identical sizes and normalized disassembly for all 488
     functions.
   - *(Continued in patch 0892.)* `ectoplasm.c` is also independent, including
     its collision callback. Its object-union proof is identical for all 488
     functions.
   - *(Continued in patch 0893.)* `ghost_dancer.c` is independent with only
     its update entry point exported. The object-union proof remains identical
     for all 488 functions.
   - *(Continued in patch 0894.)* `medusa_head.c` is independent and the
     generic death helper joins the prefixed internal API. Its object-union
     proof remains identical for all 488 functions.
   - *(Continued in patch 0895.)* `ghost.c` is independent; target facing and
     report merging join the internal API. Its object-union proof remains
     identical for all 488 functions.
   - *(Continued in patch 0896.)* `zombie_officer.c` is independent; the
     walker and gib helpers are shared through the internal API. Its union
     proof remains identical for all 488 functions.
   - *(Continued in patch 0897.)* the shared Curly/Durga `curly.c` family is
     independent; its slope and edge probes join the internal API. Its union
     proof remains identical for all 488 functions.
   - *(Continued in patch 0898.)* `needles.c` is independent with body and
     fragment entry points. Its union proof remains identical for all 488
     functions.
   - *(Continued in patch 0899.)* the Gorgon/Catoblepas family and breath are
     independent. Its union proof remains identical for all 488 functions.
   - *(Continued in patch 0900.)* `dead_warrior.c` is independent. Its union
     proof remains identical for all 488 functions.
   - *(Continued in patch 0901.)* `mimic.c` is independent with its hidden
     trigger. Its union proof remains identical for all 488 functions.
   - *(Continued in patch 0902.)* `fleaman.c` is independent. Its union proof
     remains identical for all 488 functions.
   - *(Continued in patch 0903.)* `skeleton_knight.c` is independent. All 488
     function sizes match; normalized instructions differ only in GCC's inert
     `knight_turn` stack padding (from `0x18` to `0x20`).
   - *(Continued in patch 0904.)* `killer_doll.c` is independent and exports
     its read-only piece tables for the still-included Lightning Doll family.
     Its union proof remains identical for all 488 functions.
   - *(Continued in patch 0905.)* `lightning_doll.c` is independent with its
     shatter producer and burning-piece update. After normalizing GCC's
     renumbered function-local static symbols, its union proof remains
     identical for all 488 functions.
   - *(Continued in patch 0906.)* `beam_skeleton.c` is independent with its
     beam, muzzle flash and seven-bone shatter producer. Its union proof
     remains identical for all 488 functions.
   - *(Continued in patch 0907.)* `skull_millione.c` is independent with its
     shared Skull Millione, Cagnazzo and Lubicant body update plus the fin
     child update; `move_toward` joins the internal API. Its union proof is
     identical for all 488 symbols.
   - *(Continued in patch 0908.)* `dead_crusader.c` is independent with its
     update entry point exported; only GCC's inert `crusader_turn` stack
     padding differs.
   - *(Continued in patch 0909.)* `gargoyle.c` is independent with its
     shared Gargoyle/Gremlin update exported; the union proof is identical
     for all 488 symbols.
   - *(Continued in patch 0910.)* `axe_armor.c` is independent with its
     body and axe-child updates exported; `player_ahead_within` joins the
     internal API. The union proof is identical for all 488 symbols.
   - *(Continued in patch 0911.)* `rock_armor.c` is independent with its
     three updates exported; its probes, speed, walk and death helpers are
     shared with the still-included `bomber_armor.c` through the internal
     API. The union proof is identical for all 488 symbols.
   - *(Continued in patch 0912.)* `skull_archer.c` is independent with its
     three updates and `make_debris` exported for `waiter_skeleton.c`.
     The union proof is identical for all 488 symbols.
   - *(Continued in patch 0913.)* `skeleton.c` is independent; its merge,
     corner, place and shatter helpers are shared with nine still-included
     families through the internal API. Only GCC stack-layout artifacts
     differ. The other 30 included families remain.
   - *(Continued in patch 0914.)* `killer_fish.c` is independent with its
     four updates and struck callback exported; nine water/swim helpers
     join the internal API. The union proof is identical for all 488
     symbols. The other 29 included families remain. Continue with
     `winged_skeleton.c`; keep one family per commit and repeat the union
     proof.
   - *(Continued in patch 0915.)* `winged_skeleton.c` is independent
     with its three updates exported; `projectile_collide` joins the
     internal API. The union proof is identical for all 488 symbols.
     The other 28 included families remain. Continue with
     `zombie_soldier.c`; keep one family per commit and repeat the union
     proof.
   - *(Continued in patch 0916.)* `harpy.c` is independent with its
     three updates exported; the Gorgon debris producer stays here and
     the Gorgon/Catoblepas rows list the file. The union proof is
     identical for all 488 symbols. The other 27 included families
     remain. Continue with `tiny_devil.c`; keep one family per commit
     and repeat the union proof.
   - *(Continued in patch 0917.)* `tiny_devil.c` is independent with
     its four updates exported. The union proof is identical for all
     488 symbols. The other 26 included families remain. Continue with
     `ripper.c`; keep one family per commit and repeat the union proof.
   - *(Continued in patch 0918.)* `ripper.c` is independent with its
     two updates exported. The union proof is identical for all 488
     symbols. The other 25 included families remain. Continue with
     `merman.c`; keep one family per commit and repeat the union proof.
   - *(Continued in patch 0919.)* `merman.c` is independent with its
     two updates exported. The union proof is identical for all 488
     symbols. The other 24 included families remain. Continue with
     `peeping_eye.c`; keep one family per commit and repeat the union
     proof.
   - *(Continued in patch 0920.)* `peeping_eye.c` is independent with
     its three entry points exported; `flag_8e_40` became an exported
     variable (same normalized symbol, only linkage changed). The union
     proof is identical for all 488 symbols. The other 23 included
     families remain. Continue with `buer.c`; keep one family per
     commit and repeat the union proof.
   - *(Continued in patch 0921.)* `buer.c` is independent with its
     three updates and the shared window probe exported; `player_dx`
     joins the internal API. The union proof is identical for all 488
     symbols except one GCC stack-padding artifact in
     `buer_player_in_window`. The other 22 included families remain.
     Continue with `white_dragon.c`; keep one family per commit and
     repeat the union proof.
   - *(Continued in patch 0922.)* `white_dragon.c` is independent with
     its four entry points exported; `trunc16` joins the internal API
     for Giant Ghost. The union proof is identical for all 488 symbols
     except GCC stack-padding artifacts in `dragon_tx`/`dragon_ty`.
     The other 21 included families remain. Continue with
     `bone_pillar.c`; keep one family per commit and repeat the union
     proof.
   - *(Continued in patch 0923.)* `bone_pillar.c` is independent with
     its two updates and the smoke producer exported. The union proof
     is identical for all 488 symbols. The other 20 included families
     remain. Continue with `giant_ghost.c`; keep one family per commit
     and repeat the union proof.
   - *(Continued in patch 0924.)* `giant_ghost.c` is independent with
     its two updates exported. The union proof is identical for all
     488 symbols. The other 19 included families remain. Continue with
     `minotaur.c`; keep one family per commit and repeat the union
     proof.
   - *(Continued in patch 0925.)* `minotaur.c` is independent with its
     update exported. The union proof is identical for all 488
     symbols. The other 18 included families remain. Continue with
     `worm.c`; keep one family per commit and repeat the union proof.
   - *(Continued in patch 0926.)* `worm.c` is independent with its
     three entry points exported. The union proof is identical for all
     488 symbols. The other 17 included families remain. Continue with
     `biphron.c`; keep one family per commit and repeat the union
     proof.
   - *(Continued in patch 0927.)* `biphron.c` is independent with its
     two updates exported. The union proof is identical for all 488
     symbols. The other 16 included families remain. Continue with
     `bomber_armor.c`; keep one family per commit and repeat the union
     proof.
   - *(Continued in patch 0928.)* `bomber_armor.c` is independent with
     its three updates exported; `burst_source`/`burst_source_count`
     became exported variables (same normalized symbols). The union
     proof is identical for all 488 symbols. The other 15 included
     families remain. Continue with `evil_butcher.c`; keep one family
     per commit and repeat the union proof.
   - *(Continued in patch 0929.)* `evil_butcher.c` is independent with
     its four updates exported; `append_effects` joins the internal
     API. The union proof is identical for all 488 symbols. The other
     14 included families remain. Continue with `killer_mantle.c`;
     keep one family per commit and repeat the union proof.
   - *(Continued in patch 0930.)* `killer_mantle.c` is independent
     with its three updates exported; `random_direction` joins the
     internal API. The union proof is identical for all 488 symbols.
     The other 13 included families remain. Continue with `dryad.c`;
     keep one family per commit and repeat the union proof.
   - *(Continued in patch 0931.)* `dryad.c` is independent with its
     two updates exported. The union proof is identical for all 488
     symbols. The other 12 included families remain. Continue with
     `tsuchinoko.c`; keep one family per commit and repeat the union
     proof.
   - *(Continued in patch 0932.)* `tsuchinoko.c` is independent with
     its body and child updates exported; `spawn_particle` joined the
     internal API for it. The union proof is identical for all 488
     symbols. The other 11 included families remain. Continue with
     `kicker_skeleton.c`; keep one family per commit and repeat the
     union proof.
   - *(Continued in patch 0933.)* `kicker_skeleton.c` is independent
     with its update exported. The union proof is identical for all
     488 symbols. The other 10 included families remain (`bat.c` is
     included early at the top of `aos_enemy.c`; convert it last).
     Continue with `waiter_skeleton.c`; keep one family per commit
     and repeat the union proof.
   - *(Continued in patch 0934.)* `waiter_skeleton.c` is independent
     with its body, tray and companion updates exported. The union
     proof is identical for all 488 symbols. The other 9 included
     families remain (`bat.c` is included early at the top of
     `aos_enemy.c`; convert it last). Continue with `cockatrice.c`;
     keep one family per commit and repeat the union proof.
   - *(Continued in patch 0935.)* `cockatrice.c` is independent with
     its body, shadow and rock updates exported. The union proof is
     identical for all 488 symbols. The other 8 included families
     remain (`bat.c` is included early at the top of `aos_enemy.c`;
     convert it last). Continue with `chronomage.c`; keep one family
     per commit and repeat the union proof.
   - *(Continued in patch 0936.)* `chronomage.c` is independent with
     its body and wisp updates exported; `spawn_flame` joined the
     internal API. The union proof is identical for all 488 symbols.
     The other 7 included families remain (`bat.c` is included early
     at the top of `aos_enemy.c`; convert it last). Continue with
     `headhunter.c`; keep one family per commit and repeat the union
     proof.
   - *(Continued in patch 0937.)* `headhunter.c` is independent with
     its body, death and head updates exported; `accelerate_toward`
     joined the internal API. The union proof is identical for all
     488 symbols. The other 6 included families remain (`bat.c` is
     included early at the top of `aos_enemy.c`; convert it last).
     Continue with `zombie.c`; keep one family per commit and repeat
     the union proof.
   - *(Continued in patch 0938.)* `zombie.c` is independent with its
     create, body, drip and puddle updates exported; `on_slope`
     joined the internal API and the stale prototypes were removed
     from `aos_enemy.c`. The union proof is identical for all 488
     symbols. The other 5 included families remain (`bat.c` is
     included early at the top of `aos_enemy.c`; convert it last).
     Continue with `nemesis.c`; keep one family per commit and
     repeat the union proof.
   - *(Continued in patch 0939.)* `nemesis.c` is independent with six
     entry points exported; `angle_to_target`, `turn_angle` and
     `no_target` joined the internal API. The union proof keeps all
     488 symbol sizes; only a GCC stack-slot padding difference in
     `helper_face` differs in the disassembly. The other 4 included
     families remain (`bat.c` is included early at the top of
     `aos_enemy.c`; convert it last). Continue with `crow.c`; keep
     one family per commit and repeat the union proof.
   - *(Continued in patch 0940.)* `crow.c` is independent with its
     body and feather updates exported; `spawn_blood` joined the
     internal API. The union proof is identical for all 488 symbols.
     The other 3 included families remain (`bat.c` is included early
     at the top of `aos_enemy.c`; convert it last). Continue with
     `zombie_soldier.c`; keep one family per commit and repeat the
     union proof.
   - *(Continued in patch 0941.)* `zombie_soldier.c` is independent
     with its grenade, gib and soldier updates exported;
     `grenade_particles` joined the internal API and the
     `soldier_face` alias moved into `aos_enemy.c` for `valkyrie.c`.
     The union proof keeps all 488 symbol sizes; only GCC
     stack-slot renumbering differs in `soldier_face` and
     `valkyrie_recoil`. The other 2 included families remain
     (`bat.c` is included early at the top of `aos_enemy.c`; convert
     it last). Continue with `valkyrie.c`; keep one family per
     commit and repeat the union proof.
   - Create `src/runtime/aos_enemy_internal.h`. Declare in it every
     framework helper the fragments use (currently `static` in
     `aos_enemy.c`; there are 109 `static` definitions there).
   - Make those helpers non-static with an `aos_enemy_` prefix, or another
     distinct one. Generic names such as `play`, `stop`, `integrate`,
     `whole`, `place` and `target_x` would clash at link time in
     `fusion_metroidvania`, which links both games' runtimes.
   - Move the shared mutable state behind accessors or `extern`
     declarations in the internal header: `flag_8e_40`, `soul_attacks`,
     `soul_attack_count`, `soul_contact`, `burst_source`,
     `burst_source_count`, `no_target`, `aim_self` (all near the top of
     `aos_enemy.c`). Do not duplicate them.
   - Each family file keeps its own helpers `static`. It exposes only its
     create, update and child-update entry points through the internal
     header, as `enemy_create`, `collide` and `enemy_update` call them.
   - `enemy_create` (the long per-id constructor) and `collide` reach into
     many families' fields. You can leave them in `aos_enemy.c`, or turn
     them into per-family `*_create` functions. Do it one family at a time.
   - Add each new `.c` file to every CMake target that lists
     `src/runtime/aos_enemy.c` (16 places). A small CMake variable such as
     `AOS_ENEMY_SOURCES` would avoid repeating the list.
   - Verify with the comparison above, then with `fusion_aos_enemy_tests`
     and the full cycle.
2. *(Done in patch 0886.)* The bat is in `aos_enemies/bat.c`, included
   before `enemy_create`. The zombie's `zombie_vanish` and `zombie_create`
   are in `zombie.c`; since patch 0938 the file is an independent
   translation unit and `enemy_create` calls the exported
   `aos_enemy_zombie_create`.
3. **Souls.**
   - *(Done in patch 0888.)* The red-soul modules are now
     `aos_soul_red_*` (formerly `aos_skill_*`) and the blue-soul ones
     `aos_soul_blue_*` (formerly `aos_guardian_*`), with their tests, CTest
     names, include guards and registry sources. The shared blue-soul API
     of `aos_soul` (`aos_guardian_end`, `aos_guardian_power`,
     `aos_guardian_try`, `aos_guardian_upkeep`) kept its names.
   - Move the souls coded inline in `aria_runtime.c` into such modules:
     `update_skills` (around line 1800) has cases such as `SKILL_BAT`,
     `SKILL_ARROW`, `SKILL_BOW`, `SKILL_AXE`, `SKILL_DAGGER`, `SKILL_HARPY`,
     `SKILL_GLADIATOR` and `SKILL_DECOY`; only 7 of them call a module.
     Patch 0890 moved `SKILL_ARROW` as the pattern to follow: a pure
     submodel in `aos_soul_red_arrow.{c,h}` (create and update, no SDL, its
     own ROM-free test), stored in `AriaSkill`, with the runtime copying
     back the fields other code reads (here `x`, `vx`, `state`, `timer`,
     used by the collision pass and the blink).
   - *(Done in patch 0889.)* Ten soul tests no longer link `aos_enemy.c`.
     The throw, Fleaman and Gladiator soul tests still do: their modules
     keep their state in an `AosEnemy`. Some soul tests may still list
     other enemy-side sources they do not need; check them the same way.

4. *(Done in patches 0886 and 0887.)* `aos_math` now also holds
   `aos_isqrt` and `aos_fixed_mul`, and `aos_title_screen.c` uses it instead
   of its own copies.
5. *(Done in patch 0887.)* The first comment of `aos_enemy.h` now only
   describes the framework and the layout.

## Rules and pitfalls

- Move code only. Do not change behaviour in the same commit, and keep
  every native routine name and comment.
- One family or one step per commit. Run the comparison, the targeted
  tests (`ctest --test-dir build -R aos_enemy` and the soul tests), then
  the full `AGENTS.md` cycle before committing.
- Keep the registries in sync in the same commit: `annotations.tsv`
  `local_sources`, `features.tsv`, `aos_monster_aspects.tsv` if it names
  files, and the documents. Do not change any status.
- Never add Julius mode or SOUL TRADE (see `AGENTS.md`).
- Another session ports monsters into these files. Pull before starting
  and keep each refactor commit small to limit conflicts.
