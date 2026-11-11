/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Chaos (true final boss 0x70, both phases).
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../../aos/core/aos_enemy_internal.h"
#include "../../aos/core/aos_audio.h"

#include <string.h>

#define collide aos_enemy_collide
#define play_loop aos_enemy_play_loop
#define skeleton_merge aos_enemy_skeleton_merge
#define spawn_flame_at aos_enemy_spawn_flame_at
#define step_anim aos_enemy_step_anim

/* EnemyChaosCreate 0x080A1E38 / EnemyChaosUpdate 0x080A4340 (phase 1)
 * and EnemyChaos2Create 0x080A712C / EnemyChaos2Update 0x080A6ED4
 * (phase 2, the room param1 = + 0x32 variant).
 *
 * Phase 1: the + 0x20 record entity carries the drifting core; the
 * three sub_080A2A4C parts fold into AOS_ROLE_CHAOS_PART children.
 * State 0 (sub_080A20D0) is the decoded intro — the SetPlayerInput
 * lock, the palette fades, song 0x32 for the + 0xD = 0x78 beat, song
 * 0x52 with the + 0xD = 0x51 sub_080A3D40 part pulses and the
 * every-eighth-frame sub_08044AA4 positions folded away, then state
 * 1. State 1 (sub_080A1FEC) keeps the record drift (+ 0xC += 0x80,
 * + 0x10 += 0x200) and the three staggered + 0x19/+ 0x1A/+ 0x1B
 * timers gated by the + 0x18 mask / + 0x15 fired flags; the native
 * mask is an external story protocol approximated by the three
 * timers' completion. State 2 (sub_080A43E4) is the one-shot palette
 * restore. State 3 (sub_080A22A0) is the decoded drift machine —
 * song 0x149, the + 0x48/+ 0x4C velocity ramps, the + 0xD phase
 * counter 1/2/3 and the + 0xC == 1 palette-flash exit with song
 * 0x1010. State 4 is the dormant park.
 *
 * Phase 2: hp + 0x34 = 0xBB8 through the sub_080A4408 spawn path;
 * the sub_080A4C0C hurt callback tracks + 0x34 and on depletion runs
 * sub_080683BC, song 0x1010 and state 3. States 0/1/3
 * (sub_080A4728/sub_080A4508/sub_080A49E0) collapse to a documented
 * intro/attack/death cycle — the ~5000-line helper cluster
 * (sub_080A4CE4 through sub_080A6D88) is not ported. The phase-2
 * tiles 0x08119224/frames 0x08226B58 have no second runtime kind,
 * so the phase-2 form draws the phase-1 art. */
static void ch_emit_parts(AosEnemy *e, AosHitReport *hit) {
    for (unsigned i = 0; i < 3 &&
                        hit->child_count < AOS_ENEMY_MAX_CHILDREN;
         ++i) {
        if (e->spawned_count & (1u << i)) continue;
        e->spawned_count |= (uint8_t)(1u << i);
        AosEnemy c;
        memset(&c, 0, sizeof c);
        c.id = e->id;
        c.x = e->x;
        c.y = e->y;
        c.role = AOS_ROLE_CHAOS_PART;
        c.variant = (uint8_t)i;
        c.parent = e;
        c.static_frame = (int16_t)i;
        hit->children[hit->child_count++] = c;
    }
}

static void ch_part_update(AosEnemy *part, AosEnemy *body) {
    if (!body || body->removed || body->id != 0x70) {
        part->removed = true;
        return;
    }
    /* The sub_080A2A4C parts orbit the drifting core record; the
     * native per-part brains collapse to a fixed orbit offset per
     * variant. */
    static const int32_t offs[3][2] = {
        {0x60000, -0x40000}, {-0x60000, -0x20000}, {0, -0xA0000}};
    part->x = body->ch_cx + offs[part->variant & 3][0];
    part->y = body->ch_cy + offs[part->variant & 3][1];
    part->mirrored = body->mirrored;
}

/* Phase-1 state 0 sub_080A20D0: sub-states on + 0xB. */
static void ch_intro(AosEnemy *enemy) {
    switch (enemy->step) {
    case 0:
        enemy->step = 1;
        /* sub_0803D18C/sub_0803CDF0(0x1F) palette fades are visual
         * only; + 0xD = 0x78, + 0xC = 0 and song 0x32. */
        aos_audio_play_song(0x32);
        enemy->timer = 0x78;
        break;
    case 1:
        /* The native per-frame 0x32 accent folds into the case-0
         * play; the + 0xD countdown runs into the
         * sub_0803CED4/sub_0803D280 restore. */
        if (enemy->timer) {
            enemy->timer--;
            break;
        }
        enemy->step = 2;
        enemy->timer = 0;
        break;
    default:
        /* Case 2: song 0x52 once (the native per-frame call is an
         * idempotent accent), the + 0xD = 0x51 part-pulse marker and
         * the + 0xD > 0xF9 exit into state 1. */
        if (enemy->timer == 0) {
            aos_audio_play_song(0x52);
            enemy->timer = 1;
        }
        if (enemy->substep == 0 && enemy->timer == 0x51)
            enemy->substep = 1; /* sub_080A3D40(.,0/1/2) pulse */
        /* The every-eighth-frame sub_08044AA4 part positions are
         * folded into the orbit approximation. */
        if (enemy->timer > 0xF9) {
            enemy->state = 1;
            enemy->step = 0;
            enemy->substep = 0;
            enemy->timer = 0;
        } else {
            enemy->timer++;
        }
        break;
    }
}

/* Phase-1 state 1 sub_080A1FEC: record drift plus the three
 * staggered timers; exits to state 3 when the mask protocol
 * completes (approximated by all three timers fired). */
static void ch_housekeeping(AosEnemy *enemy, uint32_t (*random)(void)) {
    if (enemy->step != 0)
        return; /* the native early-out tail only */
    enemy->ch_cx += 0x80;
    enemy->ch_cy += 0x200;
    /* The + 0x18 mask is external; the port arms all three slots
     * and lets each fire once per its decoded reload range. */
    if (!(enemy->ch_f18 & 1) && !(enemy->ch_f15 & 1)) {
        if (enemy->ch_t19) enemy->ch_t19--;
        else {
            enemy->ch_t19 = (uint8_t)((random() & 0x7F) + 0x3C);
            enemy->ch_f15 |= 1;
        }
    }
    if (!(enemy->ch_f18 & 2) && !(enemy->ch_f15 & 2)) {
        if (enemy->ch_t1a) enemy->ch_t1a--;
        else {
            enemy->ch_t1a = (uint8_t)((random() & 0x7F) + 0x78);
            enemy->ch_f15 |= 2;
        }
    }
    if (!(enemy->ch_f18 & 4) && !(enemy->ch_f15 & 4)) {
        if (enemy->ch_t1b) enemy->ch_t1b--;
        else {
            enemy->ch_t1b = (uint8_t)((random() & 0x7F) + 0x3C);
            enemy->ch_f15 |= 4;
        }
    }
    /* Native exit: + 0x18 & 7 == 7. The port completes the mask
     * once every slot has fired (documented approximation of
     * the story-driven enable). */
    if ((enemy->ch_f15 & 7) == 7) {
        enemy->ch_f18 |= 7;
        enemy->state = 3;
        enemy->step = 0;
        enemy->substep = 0;
    }
}

/* Phase-1 state 3 sub_080A22A0: the drifting core fight. */
static void ch_drift(AosEnemy *enemy) {
    switch (enemy->step) {
    case 0:
        /* sub_080D7250(x, y) snap; + 0xB = 1, + 0xD = 0x78,
         * + 0x48 = 0x80, + 0x4C = 0x100, + 0x50 = 0 and the story
         * flag writes. */
        enemy->step = 1;
        enemy->timer = 0x78;
        enemy->vx = 0x80;
        enemy->vy = 0x100;
        enemy->ax = 0;
        enemy->ch_cx = enemy->x;
        enemy->ch_cy = enemy->y;
        break;
    case 1:
        if (enemy->timer) {
            enemy->timer--;
            break;
        }
        enemy->step = 2;
        enemy->substep = 0;
        enemy->timer = 0;
        aos_audio_play_song(0x149);
        break;
    default:
        /* Case 2: the + 0x48 / + 0x4C ramps and the + 0xD phase
         * counter stored through + 0x50 (the spin accumulator). */
        if (enemy->vx <= 0x7FF) enemy->vx += 4;
        if (enemy->vy <= 0x3FF) enemy->vy = 0x100;
        enemy->vy += 0x20;
        if (enemy->timer <= 0x3B) {
            enemy->ax = 1;
        } else if (enemy->timer <= 0x59) {
            enemy->ax = 2;
        } else if (enemy->timer <= 0x77) {
            enemy->ax = 3;
        }
        /* The + 0x14 > 0x200 palette flash with song 0x1010 and the
         * sub_08011F44/sub_08011AD0 exit tail are approximated by
         * the spin accumulator crossing the same threshold. */
        if (enemy->substep == 0 && enemy->ch_spin > 0x200) {
            aos_audio_play_song(0x1010);
            enemy->substep = 1;
        }
        enemy->ch_cx += enemy->vx;
        enemy->ch_cy += enemy->vy;
        enemy->ch_spin += enemy->ax;
        enemy->timer++;
        break;
    }
}

/* Phase-2 (Chaos2) collapsed machine: state 0 intro beat, state 1
 * the attack cycle, state 3 the death cinematic. */
static void ch2_update(AosEnemy *enemy, AosHitReport *report,
                       uint32_t (*random)(void)) {
    switch (enemy->state) {
    case 0:
        if (enemy->step == 0) {
            aos_audio_play_song(0x149);
            enemy->timer = 0x78;
            enemy->step = 1;
        } else if (enemy->timer) {
            enemy->timer--;
        } else {
            enemy->state = 1;
            enemy->step = 0;
            enemy->timer = 0;
        }
        break;
    case 1:
        /* The sub_080A4508 twelve-case machine keeps its decoded
         * real cases as a documented cycle: RNG pick, windup,
         * volley and recover with flame-folded shots. */
        switch (enemy->step) {
        case 0:
            if ((random() & 0xFF) < 0x40) {
                aos_audio_play_song(0x167);
                enemy->timer = 0x3B;
                enemy->step = 1;
            } else {
                enemy->timer = 0x14;
                enemy->step = 2;
            }
            break;
        case 1:
            if (enemy->timer) enemy->timer--;
            if (!enemy->timer &&
                report->child_count < AOS_ENEMY_MAX_CHILDREN - 1) {
                spawn_flame_at(enemy->x + (enemy->mirrored ? -0x80000
                                                          : 0x80000),
                               enemy->y - 0x40000, 0x60000, 0x80000,
                               random, report);
                enemy->step = 10;
                enemy->timer = 0x28;
            }
            break;
        case 2:
            if (enemy->timer) enemy->timer--;
            if (!enemy->timer) {
                enemy->step = 0;
                enemy->timer = 0;
            }
            break;
        case 10:
            if (enemy->timer) enemy->timer--;
            if (!enemy->timer) {
                enemy->step = 0;
                enemy->timer = 0;
            }
            break;
        default:
            enemy->step = 0;
            break;
        }
        break;
    case 3:
        /* sub_080A49E0 death: song 0x1010 accent, the + 0xD = 0x78
         * tail and the delete on the cycle flag. */
        if (enemy->step == 0) {
            aos_audio_play_song(0x1010);
            enemy->timer = 0x78;
            enemy->step = 1;
        } else if (enemy->timer) {
            enemy->timer--;
        } else if (enemy->cycle_done) {
            enemy->cycle_done = false;
            enemy->removed = true;
        }
        break;
    default:
        break;
    }
}

AosHitReport aos_enemy_chaos_update(
    AosEnemy *enemy, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind,
    const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0};
    (void)soma;
    (void)layer;
    (void)weapon;
    (void)weapon_frames;
    (void)soma_atk;
    (void)soma_def;
    (void)cam_x;
    (void)cam_y;
    if (enemy->role == AOS_ROLE_CHAOS_PART) {
        ch_part_update(enemy, enemy->parent);
        return report;
    }
    bool phase2 = enemy->param1 != 0;
    if (!enemy->boss_parts_spawned) {
        ch_emit_parts(enemy, &report);
        enemy->boss_parts_spawned = true;
    }
    if (phase2) {
        ch2_update(enemy, &report, random);
    } else {
        switch (enemy->state) {
        case 0:
            ch_intro(enemy);
            break;
        case 1:
            ch_housekeeping(enemy, random);
            break;
        case 2:
            /* sub_080A43E4: the one-shot + 0xB = 1 palette restore. */
            if (enemy->step == 0)
                enemy->step = 1;
            break;
        case 3:
            ch_drift(enemy);
            break;
        default:
            /* State 4 and above: the dormant park (bx lr tail). */
            break;
        }
    }
    step_anim(enemy, kind->anims);
    AosHitReport hit = collide(enemy, soma, kind, weapon, weapon_frames,
                               soma_atk, soma_def);
    skeleton_merge(&report, &hit);
    /* The sub_080A4C0C hurt callback on phase 2 tracks + 0x34 (the
     * 0xBB8 hp); the shared collide already applies record damage,
     * so a lethal hit enters the state-3 death cinematic like the
     * native sub_080683BC + PlaySong 0x1010 path. Phase 1's 9999 hp
     * makes it unreachable in practice. */
    if (phase2 && !enemy->defeated && enemy->hp <= 0 &&
        enemy->state != 3) {
        enemy->defeated = true;
        enemy->state = 3;
        enemy->step = 0;
        enemy->timer = 0;
        play_loop(enemy, kind->anims, 3, false);
    }
    aos_combat_tick(&enemy->combat);
    return report;
}
