/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Big Golem (boss 0x45).
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../../aos/core/aos_enemy_internal.h"
#include "../../aos/core/aos_audio.h"

#define accelerate_toward aos_enemy_accelerate_toward
#define accel_move aos_enemy_accel_move
#define collide aos_enemy_collide
#define face_player aos_enemy_face_player
#define move_toward aos_enemy_move_toward
#define play aos_enemy_play
#define play_loop aos_enemy_play_loop
#define player_in_range aos_enemy_player_in_range
#define skeleton_merge aos_enemy_skeleton_merge
#define spawn_drift aos_enemy_spawn_drift
#define spawn_flame_at aos_enemy_spawn_flame_at
#define step_anim aos_enemy_step_anim

/* EnemyBigGolemUpdate 0x080A8224 state 2: the native death machine.
 * While the + 0xD timer sits in [0x79, 0xD6) every fourth frame fires
 * a sub_0806D7F0 particle at a random x offset (the port uses the
 * dark-smoke drift); the + 0x59 bit-3 animation-end flag plus the
 * + 0x1C room record gates the sub_0806C250 delete. The update tail
 * clears + 0x1A on a 1/8 random roll. */
static AosHitReport bg_death(AosEnemy *enemy, uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (enemy->timer >= 0x79 && enemy->timer < 0xD6 &&
        (enemy->timer & 3) == 0) {
        int32_t px = enemy->x +
                     (int32_t)(random() % 0x280000) - 0x140000;
        spawn_drift(AOS_EFFECT_DARK_SMOKE, px, enemy->y - 0x80000,
                    &report);
    }
    if (enemy->timer)
        enemy->timer--;
    /* The + 0x59 bit-3 flag: animation end while the timer sits in
     * its tail deletes through the native + 0x1C room record. */
    if (enemy->cycle_done && enemy->timer <= 0x10) {
        enemy->cycle_done = false;
        enemy->removed = true;
    }
    return report;
}

/* sub_080A89F8 state 0: thirteen cases in four repeating groups — a
 * short pose case (song 0x7C attack call), a long chase case built on
 * the sub_0806CDB8/D0B8/D188 wall and distance probes, and a settle
 * case. The port collapses the probes into the shared helpers and
 * cycles the documented beats: chase, attack pose, settle, then the
 * case-12 exit into state 1. */
static void bg_state0(AosEnemy *enemy, AosSoma *soma,
                      const AosAnimSet *anims,
                      uint32_t (*random)(void)) {
    (void)random;
    switch (enemy->step) {
    case 0:
    case 4:
    case 7:
    case 10:
        /* Pose/settle beats between attacks. */
        if (++enemy->timer > 0x20) {
            enemy->timer = 0;
            enemy->step++;
        }
        break;
    case 1:
    case 5:
    case 8:
    case 11:
        /* Attack call: song 0x7C once, animation 1, small lunge. */
        aos_audio_play_song(0x7C);
        play_loop(enemy, anims, 1, false);
        enemy->vx = enemy->mirrored ? 0x40000 : -0x40000;
        enemy->timer = 0;
        enemy->step++;
        break;
    case 2:
    case 3:
    case 6:
    case 9:
        /* Chase: face the player and close in under the speed cap. */
        if (soma)
            face_player(enemy, soma);
        if (soma)
            move_toward(enemy, soma->x, soma->y - 0x80000, 0x10);
        accel_move(enemy, 0x60000);
        if (++enemy->timer > 0x60) {
            enemy->timer = 0;
            enemy->step++;
        }
        break;
    case 12:
        /* The native exit: enter the state-1 attack machine. */
        enemy->state = 1;
        enemy->step = 0;
        enemy->timer = 0;
        break;
    default:
        enemy->step = 0;
        break;
    }
}

/* sub_080AA7A8 state 1: forty cases of attack combinations — windup
 * song 0x86, strike song 0x71, sub_0806DCC4 shots and the
 * sub_080ABFA4/sub_080ACDD0 helpers. The port keeps the documented
 * beat order: windup, two strike volleys, recover, back to state 0. */
static void bg_state1(AosEnemy *enemy, AosSoma *soma,
                      const AosAnimSet *anims, AosHitReport *report,
                      uint32_t (*random)(void)) {
    switch (enemy->step) {
    case 0:
        face_player(enemy, soma);
        play_loop(enemy, anims, 2, false);
        enemy->timer = 0;
        enemy->step = 1;
        break;
    case 1:
        /* Windup: song 0x86, hold the pose. */
        if (enemy->timer == 0)
            aos_audio_play_song(0x86);
        if (++enemy->timer > 0x30) {
            enemy->timer = 0;
            enemy->step = 2;
        }
        break;
    case 2:
    case 4:
        /* Strike volleys: song 0x71 and flame-folded shots. */
        if (enemy->timer == 0) {
            aos_audio_play_song(0x71);
            for (int i = 0; i < 2 &&
                            report->child_count < AOS_ENEMY_MAX_CHILDREN;
                 ++i) {
                int32_t sx = enemy->x + (enemy->mirrored ? -0x80000
                                                        : 0x80000);
                spawn_flame_at(sx, enemy->y - 0x40000, 0x60000, 0x80000,
                               random, report);
            }
        }
        if (++enemy->timer > 0x20) {
            enemy->timer = 0;
            enemy->step++;
        }
        break;
    case 3:
    case 5:
        /* Recover between strikes. */
        if (++enemy->timer > 0x28) {
            enemy->timer = 0;
            enemy->step++;
        }
        break;
    default:
        /* Return to the state-0 cycle. */
        enemy->state = 0;
        enemy->step = 0;
        enemy->timer = 0;
        break;
    }
}

/* The four sub_0806DFF8 children of EnemyBigGolemCreate (callback
 * sub_080AC6E8, ~1000 lines undecoded): approximated as slow tracking
 * parts that mirror the body pose at fixed offsets. */
static void bg_child_update(AosEnemy *child, AosEnemy *body) {
    if (!body || body->removed || body->id != 0x45) {
        child->removed = true;
        return;
    }
    static const int32_t ox[4] = {-0x500000, 0x500000, -0x300000,
                                  0x300000};
    static const int32_t oy[4] = {-0x600000, -0x600000, -0x200000,
                                  -0x200000};
    unsigned v = child->variant < 4 ? child->variant : 0;
    child->x = body->x + ox[v];
    child->y = body->y + oy[v];
    child->mirrored = body->mirrored;
}

static AosEnemy bg_child(AosEnemy *body, unsigned index) {
    AosEnemy c = {.id = body->id,
                  .x = body->x,
                  .y = body->y,
                  .role = (uint8_t)(AOS_ROLE_BG_CHILD0 + index),
                  .variant = (uint8_t)index,
                  .parent = body,
                  .static_frame = -1,
                  .combat = {.type = AOS_TYPE_ENEMY, .attack_off = true,
                             .hurt_off = true}};
    return c;
}

AosHitReport aos_enemy_big_golem_update(
    AosEnemy *enemy, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind,
    const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0};
    (void)layer;
    (void)cam_x;
    (void)cam_y;
    if (enemy->role >= AOS_ROLE_BG_CHILD0 &&
        enemy->role <= AOS_ROLE_BG_CHILD3) {
        bg_child_update(enemy, enemy->parent);
        return report;
    }
    if (!enemy->boss_parts_spawned) {
        for (unsigned i = 0; i < 4 &&
                        report.child_count < AOS_ENEMY_MAX_CHILDREN;
             ++i)
            report.children[report.child_count++] = bg_child(enemy, i);
        enemy->spawned_count = 4;
        enemy->boss_parts_spawned = true;
    }
    if (enemy->state == 2) {
        AosHitReport hit = bg_death(enemy, random);
        skeleton_merge(&report, &hit);
        step_anim(enemy, kind->anims);
        return report;
    }
    switch (enemy->state) {
    case 0:
        bg_state0(enemy, soma, kind->anims, random);
        break;
    case 1:
        bg_state1(enemy, soma, kind->anims, &report, random);
        break;
    default:
        /* State 3 dormant (sub_0803F17C tail only). */
        break;
    }
    step_anim(enemy, kind->anims);
    AosHitReport hit = collide(enemy, soma, kind, weapon, weapon_frames,
                               soma_atk, soma_def);
    skeleton_merge(&report, &hit);
    /* Lethal: the shared hurt path applied the damage; the native
     * state-2 machine is the decoded death above. */
    if (!enemy->defeated && enemy->hp <= 0 && enemy->state != 2) {
        enemy->defeated = true;
        enemy->state = 2;
        enemy->step = 0;
        enemy->timer = 0xD6;
        play_loop(enemy, kind->anims, 3, false);
    }
    aos_combat_tick(&enemy->combat);
    return report;
}
