/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemy: Mudman (enemy 0x56) — the wall-creature that
 * patrols, rears up and hurls its arm.
 *
 * EnemyMudmanCreate (0x08072670) / EnemyMudmanUpdate (0x08071904), the
 * state handlers sub_080719F4 (patrol/walk), sub_08071BA4 (attack),
 * sub_08071DBC (death bubble burst), sub_08071C30 (hurt flinch), the
 * hurt sub_08071F74, the arm spawn sub_08072038 with its update
 * sub_08072120, the bubble spawn sub_08072504 with its update
 * sub_080725C0 and the bubble/arm initialisers sub_08072608 /
 * sub_08072434. The sub_0806D288 chase probe of the arm, the
 * sub_0806D518 blink of the death and the sub_080421AC room query are
 * approximated (see docs/native_parity/annotations.tsv). */

#include "../../aos/core/aos_enemy_internal.h"
#include "../../aos/core/aos_audio.h"

#define collide aos_enemy_collide
#define outside_margins aos_enemy_outside_margins
#define play_loop aos_enemy_play_loop
#define player_in aos_enemy_player_in
#define spawn_drift aos_enemy_spawn_drift
#define step_anim aos_enemy_step_anim
#define umod aos_enemy_umod
#define walk_collide aos_enemy_walk_collide
#define accel_move aos_enemy_accel_move

/* The whole-pixel positions the native + 0x42/+ 0x46 fields cache. */
static int16_t md_px(const AosEnemy *e) { return (int16_t)(e->x >> 16); }
static int16_t md_py(const AosEnemy *e) { return (int16_t)(e->y >> 16); }
static int16_t md_tpx(const AosSoma *s) { return (int16_t)(aos_enemy_target_x(s) >> 16); }

/* sub_0806CF2C: 1 when the target stands right of the entity; the
 * handlers store it as + 0x58 bit 6 (`mirrored`). */
static int md_target_right(const AosEnemy *e, const AosSoma *s) {
    return md_px(e) < md_tpx(s);
}

/* sub_08072504: a mud bubble child — position and velocities absolute,
 * gravity 0x2800, the + 0x65 direct frame 0x12/0x15 by the sign of vx,
 * + 0x58 bit 7 (vflip) by the sign of vy and + 0x59 bit 1 (the ghost
 * translucency, approximated by `translucent`). Its + 0x5C copies the
 * parent's minus one. */
static void md_bubble(AosEnemy *e, int32_t x, int32_t y, int32_t vx,
                      int32_t vy, AosHitReport *spawned) {
    if (spawned->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy c = {0};
    c.id = e->id;
    c.role = AOS_ROLE_MUDMAN_BUBBLE;
    c.x = x;
    c.y = y;
    c.vx = vx;
    c.vy = vy;
    c.ay = 0x2800;
    c.stats = e->stats;
    c.hp = 1;
    c.static_frame = vx > 0 ? 0x15 : 0x12;
    c.timer = 0;
    c.vflip = vy > 0;
    c.translucent = true;
    c.parent = e;
    c.md_spawns = e->md_spawns ? e->md_spawns - 1 : 0;
    spawned->children[spawned->child_count++] = c;
}

/* sub_080725C0: the bubble update — + 0x0D counts up, the shared
 * accel mover runs with the 0x80000 limit, the direct frame advances
 * at counts 6 and 0xC and the entity deletes past 0x11. */
void aos_enemy_mudman_bubble_update(AosEnemy *e) {
    e->timer++;
    accel_move(e, 0x80000);
    if (e->timer == 6 || e->timer == 0xC) e->static_frame++;
    if (e->timer > 0x11) e->removed = true;
}

/* sub_08072038: the arm child — sub_08072608 initialises it (the
 * shared tiles/frames, animation 5, the OAM callback) and sub_08072434
 * registers its contact/hurt pair; it inherits the parent's + 0x5C
 * minus one, facing, palette bytes and sits one pixel right/left and
 * 0x18 above depending on + 0x58 bit 6, thrown outward at vx ±0x20000,
 * vy -0x10000 with 0x1800 gravity. */
static void md_spawn_arm(AosEnemy *e, AosHitReport *spawned,
                         const AosEnemyKind *kind) {
    if (spawned->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy c = {0};
    c.id = e->id;
    c.role = AOS_ROLE_MUDMAN_ARM;
    c.stats = e->stats;
    c.hp = 1;
    c.static_frame = -1;
    c.translucent = true;
    c.parent = e;
    c.md_spawns = e->md_spawns ? e->md_spawns - 1 : 0;
    play_loop(&c, kind->anims, 5, false);
    step_anim(&c, kind->anims);
    c.mirrored = e->mirrored;
    if (e->mirrored) {
        c.x = e->x + 0x100000;
        c.vx = 0x20000;
    } else {
        c.x = e->x - 0x100000;
        c.vx = -0x20000;
    }
    c.y = e->y - 0x180000;
    c.vy = -0x10000;
    c.ay = 0x1800;
    /* The state-2 follow anchor (+ 0x18/+ 0x1C): the offset to the
     * parent captured at spawn, re-applied every frame. */
    c.target_x = c.x - e->x;
    c.target_y = c.y - e->y;
    spawned->children[spawned->child_count++] = c;
}

/* sub_080719F4 state 0: the patrol. Step 0 refreshes the facing bit
 * and runs the two aggro probes of sub_0806CFFC — the wide 0x78x0x50
 * box at (px-0x3C, py-0x28) when + 0x32 is 0, else the remembered-side
 * 0x3C box (+0x10 right, -0x4C left) that clears + 0x32 on a hit.
 * Step 1 waits for the animation cycle (+ 0x59 bit 0) then arms
 * animation 2, timer 0x64, vy 0x20000 and clears + 0x59 bit 1. Step 2
 * walks: sub_08069A00(0x20000, -0x200000, 0x80000, 8) each frame; on
 * the floor bit the + 0x0D counter wraps at 0x3B, the facing refreshes
 * and rng&3 == 0 enters state 1, else vx = ±0x4000 by facing; off the
 * margin-4 screen it drops back to animation 0 step 0. */
static void md_patrol(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                      const AosEnemyKind *kind, AosHitReport *spawned,
                      uint32_t (*random)(void), int cam_x, int cam_y) {
    (void)spawned;
    switch (e->step) {
    case 0: {
        e->mirrored = md_target_right(e, soma);
        bool near;
        if (e->param1 == 0) {
            near = player_in(soma, (int16_t)(md_px(e) - 0x3C),
                             (int16_t)(md_py(e) - 0x28), 0x78, 0x50);
        } else if (e->param1 == 1) {
            near = player_in(soma, (int16_t)(md_px(e) + 0x10),
                             (int16_t)(md_py(e) - 0x28), 0x3C, 0x50);
        } else if (e->param1 == 2) {
            near = player_in(soma, (int16_t)(md_px(e) - 0x4C),
                             (int16_t)(md_py(e) - 0x28), 0x3C, 0x50);
        } else {
            return;
        }
        if (!near) return;
        e->step = 1;
        e->anim.flags &= ~2;
        e->cycle_done = false;
        if (e->param1 != 0) e->param1 = 0;
        return;
    }
    case 1:
        if (!(e->cycle_done)) return;
        e->step = 2;
        e->cycle_done = false;
        play_loop(e, kind->anims, 2, true);
        e->timer = 0x64;
        e->vy = 0x20000;
        return;
    case 2: {
        int probe = walk_collide(e, layer, 0x20000, (int32_t)0xFFE00000,
                                 0x80000, 8);
        if (probe & 4) {
            e->timer++;
            if (e->timer > 0x3B) {
                e->timer = 0;
                e->mirrored = md_target_right(e, soma);
                if (!(random() & 3)) {
                    e->state = 1;
                    e->step = 0;
                    return;
                }
                e->vx = e->mirrored ? 0x4000 : (int32_t)0xFFFFC000;
            }
        }
        if (outside_margins(kind, 4, md_px(e) - cam_x, md_py(e) - cam_y)) {
            play_loop(e, kind->anims, 0, true);
            e->step = 0;
        }
        return;
    }
    default:
        return;
    }
}

/* sub_08071BA4 state 1: the attack. Step 0 plays animation 3; step 1
 * waits for anim frame > 1 then arms the arm child (sub_08072038),
 * timer 0 and song 0x85; step 2 counts to 0x3B then returns to state
 * 0 step 2 — the walk resumes — replaying animation 2, clearing +
 * 0x59 bit 1, timer 0x64, vy 0x20000. */
static void md_attack(AosEnemy *e, AosSoma *soma,
                      const AosEnemyKind *kind, AosHitReport *spawned,
                      uint32_t (*random)(void)) {
    (void)soma; (void)random;
    switch (e->step) {
    case 0:
        play_loop(e, kind->anims, 3, false);
        e->step = 1;
        return;
    case 1:
        if (e->anim.frame <= 1) return;
        e->step = 2;
        e->timer = 0;
        md_spawn_arm(e, spawned, kind);
        aos_audio_play_song(0x85);
        return;
    case 2:
        e->timer++;
        if (e->timer <= 0x3B) return;
        e->state = 0;
        e->step = 2;
        play_loop(e, kind->anims, 2, true);
        e->cycle_done = false;
        e->timer = 0x64;
        e->vy = 0x20000;
        return;
    default:
        return;
    }
}

/* sub_08071DBC state 2: the death. Step 0 zeroes the velocities, sets
 * vy 0x20000 / ay 0x2800 and bursts eight bubbles — two per side at
 * y-0x10 with the two vy bands — then shares the step-1 probe; the
 * probe's floor bit plus the animation cycle advances to step 2 with
 * timer 0x28; step 2 blinks + 0x58 bit 5 off sub_0806D518(timer) and,
 * when the counter expires, sets + 0x59 bit 3 (md_flags bit 3), the
 * mark the shared tail reads to delete the corpse. */
static void md_death(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                     const AosEnemyKind *kind, AosHitReport *spawned,
                     uint32_t (*random)(void)) {
    (void)soma;
    switch (e->step) {
    case 0: {
        e->mirrored = md_target_right(e, soma);
        e->vx = e->ax = 0;
        e->vy = 0x20000;
        e->ay = 0x2800;
        e->step = 1;
        e->timer = 0;
        int32_t y = e->y - 0x100000;
        for (int i = 0; i < 4; ++i) {
            int left = !(i & 1);
            int32_t x = e->x + (left ? -0x60000 : 0x60000);
            int32_t vx, vy;
            if (i < 2) {
                vx = left ? (int32_t)0xFFFFF000 - (int32_t)(random() & 0xFFFF)
                          : (int32_t)(random() & 0xFFFF) + 0x1000;
                vy = (int32_t)0xFFFF0000 - (int32_t)(random() & 0x7FFF);
            } else {
                vx = left ? (int32_t)0xFFFFF000 - (int32_t)(random() & 0xFFFF)
                          : (int32_t)(random() & 0xFFFF) + 0x1000;
                vy = (int32_t)0xFFFE8000 - (int32_t)(random() & 0x7FFF);
            }
            md_bubble(e, x, y, vx, vy, spawned);
        }
        /* fall through to the shared probe below */
    }
    /* fall through */
    case 1:
        if (walk_collide(e, layer, 0x40000, (int32_t)0xFFE00000, 0x80000, 8) & 4 &&
            e->cycle_done) {
            e->step = 2;
            e->timer = 0x28;
        }
        return;
    case 2: {
        /* sub_0806D518(timer) & 1 toggles the + 0x58 bit-5 blink; the
         * native helper is a per-timer parity not ported — the timer's
         * low bits stand in. */
        e->hidden = (e->timer & 1) != 0;
        e->timer--;
        if (e->timer > 0) return;
        e->md_flags |= 8;
        if (e->cycle_done) e->removed = true;
        (void)kind;
        return;
    }
    default:
        return;
    }
}

/* sub_08071C30 state 3: the hurt flinch — entered by sub_08071F74 on a
 * non-lethal weapon blow (+ 0x70 = 3 or 4). Step 0 zeroes the
 * velocities, sets vy 0x20000 / ay 0x2800 and spits two bubbles off
 * the facing side (the + 0x58 bit-6 mirrored pair of offset/velocity
 * tables), then shares the step-1 probe which, on the floor bit and
 * the animation cycle, returns to state 0 step 0 with animation 0. */
static void md_flinch(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                      const AosEnemyKind *kind, AosHitReport *spawned,
                      uint32_t (*random)(void)) {
    (void)kind;
    switch (e->step) {
    case 0: {
        e->mirrored = md_target_right(e, soma);
        e->vx = e->ax = 0;
        e->vy = 0x20000;
        e->ay = 0x2800;
        e->step = 1;
        if (e->mirrored) {
            int32_t x = e->x - 0x60000, y = e->y - 0x120000;
            md_bubble(e, x, y,
                      (int32_t)0xFFFFF000 - (int32_t)(random() & 0xFFFF),
                      (int32_t)0xFFFF0000 - (int32_t)(random() & 0x7FFF),
                      spawned);
            md_bubble(e, x, y,
                      (int32_t)0xFFFFF000 - (int32_t)(random() & 0x7FFF),
                      (int32_t)0xFFFE8000 - (int32_t)(random() & 0x7FFF),
                      spawned);
        } else {
            int32_t x = e->x + 0x60000, y = e->y - 0x120000;
            md_bubble(e, x, y,
                      (int32_t)(random() & 0xFFFF) + 0x1000,
                      (int32_t)0xFFFF0000 - (int32_t)(random() & 0x7FFF),
                      spawned);
            md_bubble(e, x, y,
                      (int32_t)(random() & 0x7FFF) + 0x1000,
                      (int32_t)0xFFFE8000 - (int32_t)(random() & 0x7FFF),
                      spawned);
        }
        /* fall through to the shared probe below */
    }
    /* fall through */
    case 1:
        if (walk_collide(e, layer, 0x40000, (int32_t)0xFFE00000, 0x80000, 8) & 4 &&
            e->cycle_done) {
            e->state = 0;
            e->step = 0;
            play_loop(e, kind->anims, 0, true);
        }
        return;
    default:
        return;
    }
}

/* sub_08072120: the arm update. State 0 chases Soma — sub_0806D288 is
 * approximated by a homing drift — shedding a bubble per frame while
 * the native probe reports its 0xC bits and entering state 2 on the
 * 0x3 bits; state 1 (the hurt callback sub_08072658) emits one last
 * bubble and deletes; state 2 re-attaches to the parent offset and
 * alternates bubbles on its substeps until the parent's animation
 * cycle completes. */
AosHitReport aos_enemy_mudman_arm_update(AosEnemy *e, AosSoma *soma,
                                  const AosCollision *layer,
                                  const AosEnemyKind *kind,
                                  const AosWeaponEntity *weapon,
                                  const AosWeaponFrames *weapon_frames,
                                  int soma_atk, int soma_def, int cam_x,
                                  int cam_y, uint32_t (*random)(void)) {
    (void)layer; (void)weapon; (void)weapon_frames; (void)soma_atk;
    (void)soma_def; (void)cam_y; (void)random;
    AosHitReport report = {0}, spawned = {0};
    if (e->hp <= 0 || e->defeated) {
        /* sub_08072658 -> state 1: one last bubble, then gone. */
        md_bubble(e, e->x, e->y, -e->vx / 2, 1, &spawned);
        e->removed = true;
    } else if (e->state == 0) {
        /* Homing drift toward Soma (sub_0806D288 approximation): the
         * native probe also reports wall/target bits; the 0xC bubble
         * window is approximated by a fixed trail cadence and the 0x3
         * arrival by proximity. */
        int32_t dx = (int32_t)((md_tpx(soma) - md_px(e)) << 16);
        if (dx > 0 && e->vx < 0x40000) e->vx += 0x4000;
        if (dx < 0 && e->vx > -0x40000) e->vx -= 0x4000;
        if (!e->timer) {
            md_bubble(e, e->x, e->y, e->vx / 2, -e->vy, &spawned);
            e->timer = 4;
        } else {
            e->timer--;
        }
        e->x += e->vx;
        e->y += e->vy;
        e->vy += e->ay;
        e->spin++;
        if ((dx > -0x180000 && dx < 0x180000) || e->spin > 0x60) {
            e->state = 2;
            e->step = 0;
            play_loop(e, kind->anims, 6, true);
            e->translucent = false;
        }
        if (outside_margins(kind, 1, md_px(e) - cam_x, md_py(e) - cam_y))
            e->removed = true;
        step_anim(e, kind->anims);
    } else if (e->state == 1) {
        md_bubble(e, e->x, e->y, -e->vx / 2, 1, &spawned);
        e->removed = true;
    } else if (e->state == 2) {
        if (e->step == 0) {
            md_bubble(e, e->x, e->y, -e->vx / 2, 1, &spawned);
            e->step = 1;
        } else {
            md_bubble(e, e->x, e->y, e->vx / 2, 1, &spawned);
            e->step = 0;
        }
        if (e->parent && !e->parent->removed) {
            e->x = e->parent->x + e->target_x;
            e->y = e->parent->y + e->target_y;
        } else {
            e->removed = true;
        }
        if (e->parent && e->parent->cycle_done) e->removed = true;
        step_anim(e, kind->anims);
    } else {
        step_anim(e, kind->anims);
    }
    report = collide(e, soma, kind, weapon, weapon_frames, soma_atk,
                     soma_def);
    for (int i = 0; i < spawned.child_count &&
                    report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
        report.children[report.child_count++] = spawned.children[i];
    return report;
}

/* EnemyMudmanUpdate (0x08071904): the standard preamble — state != 2
 * runs the sub_08068AD4 activity and sub_08021924 marks (not ported),
 * + 0x58 bit 5 follows sub_0806D128(4) (`hidden`), state != 2 runs
 * sub_0806AE54 and the sub_0806D4C0 pause gate (not ported); while
 * + 0x2D (hit_flash) stands, states 0/1/4 skip the dispatch for the
 * sub_080421AC room query (not ported). The jump table runs states
 * 0-3 through their handlers then the sub_0806DF20/sub_0806E314 tail
 * (the shared collide pass); state 4 is animation-only. */
AosHitReport aos_enemy_mudman_update(AosEnemy *e, AosSoma *soma,
                                     const AosCollision *layer,
                                     const AosEnemyKind *kind,
                                     const AosWeaponEntity *weapon,
                                     const AosWeaponFrames *weapon_frames,
                                     int soma_atk, int soma_def, int cam_x,
                                     int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    int sx = md_px(e) - cam_x, sy = md_py(e) - cam_y;
    e->hidden = outside_margins(kind, 4, sx, sy);
    bool run = !e->hit_flash || e->state == 2 || e->state == 3;
    if (run) {
        switch (e->state) {
        case 0:
            md_patrol(e, soma, layer, kind, &spawned, random, cam_x, cam_y);
            break;
        case 1:
            md_attack(e, soma, kind, &spawned, random);
            break;
        case 2:
            md_death(e, soma, layer, kind, &spawned, random);
            break;
        case 3:
            md_flinch(e, soma, layer, kind, &spawned, random);
            break;
        case 4:
            step_anim(e, kind->anims);
            break;
        default:
            break;
        }
        if (!e->removed && e->state != 4) step_anim(e, kind->anims);
    }
    if (run && !e->removed && e->state != 4) {
        if (!outside_margins(kind, 4, sx, sy)) {
            report = collide(e, soma, kind, weapon, weapon_frames,
                             soma_atk, soma_def);
        }
    }
    for (int i = 0; i < spawned.child_count &&
                    report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
        report.children[report.child_count++] = spawned.children[i];
    return report;
}
