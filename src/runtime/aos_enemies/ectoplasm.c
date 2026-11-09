/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Ectoplasm (enemy 0x1B).
 *
 * This family is a separate translation unit and uses only the prefixed
 * internal enemy-framework API. */

#include "../aos_enemy_internal.h"

#include "../aos_audio.h"

/* sub_08099690: state 0 of the ectoplasm drift. Step 0 hovers at the home
 * point (+ 0x14/+ 0x18, here in world pixels — the gEwramData + 0xA094
 * camera offsets cancel): x is pinned, y is set to the target plus a
 * 3-pixel sine wobble of phase + 0x1C (+= 0x200), the timer counts and the
 * player box (+ 0x42 - 0x5A, + 0x46 - 0x64, 0xB4 x 0xC8 room pixels)
 * advances to step 1. Step 1 picks a destination around the aim target:
 * x +/- (16 + RNG % 768) pixels by RNG & 1, y = target y - RNG % 768 -
 * 16 or - 32 pixels alternating through + 0x20, then falls into step 2
 * the same frame. Step 2 steers: acceleration 0x400 toward the target
 * beyond 4 pixels on each axis, velocity damped 31/32 (sub_0806D490 /
 * sub_0806D4A8 with 0x20), sub_0806D3D8(e, 1.0), repeated once while
 * sub_080212C8's Soma + 0x10 flags & 0x360 are set; back to step 1 within
 * 0x3FFFF on both axes or after the u8 timer passes 0x77. */
static void ectoplasm_drift(AosEnemy *enemy, const AosSoma *soma, uint32_t (*random)(void)) {
    if (enemy->step == 0) {
        enemy->x = enemy->target_x;
        enemy->y = enemy->target_y +
                   (int32_t)(((int64_t)aos_sine(enemy->walk_timer + 0x4000) * 3));
        enemy->walk_timer += 0x200;
        enemy->timer++;
        if (aos_enemy_player_near(soma, (int16_t)((enemy->x >> 16) - 0x5A),
                                  (int16_t)((enemy->y >> 16) - 0x64),
                                  0xB4, 0xC8)) {
            enemy->step = 1;
            enemy->timer = 0;
        }
        return;
    }
    if (enemy->step > 2) return;
    if (enemy->step == 1) {
        int32_t ybias;
        if (!enemy->ec_dir) {
            enemy->ec_dir = 1;
            ybias = (int32_t)0xFFF00000;      /* -16 pixels */
        } else {
            enemy->ec_dir = 0;
            ybias = (int32_t)0xFFE00000;      /* -32 pixels */
        }
        int32_t xoff = (int32_t)(aos_enemy_umod(random(), 0x3000000) + 0x100000);
        enemy->target_x = aos_enemy_target_x(soma) + ((random() & 1) ? xoff : -xoff);
        enemy->target_y = aos_enemy_target_y(soma) -
                          (int32_t)aos_enemy_umod(random(), 0x3000000) + ybias;
        enemy->step = 2;
        enemy->timer = 0;
    }
    int32_t dx = enemy->target_x - enemy->x, dy = enemy->target_y - enemy->y;
    enemy->ax = dx > 0x40000 ? 0x400 : dx < -0x40000 ? -0x400 : 0;
    enemy->ay = dy > 0x40000 ? 0x400 : dy < -0x40000 ? -0x400 : 0;
    enemy->vx = enemy->vx * 31 / 32;
    enemy->vy = enemy->vy * 31 / 32;
    aos_enemy_accel_move(enemy, 0x10000);
    if (soma->flags & 0x360) {
        enemy->vx = enemy->vx * 31 / 32;
        enemy->vy = enemy->vy * 31 / 32;
        aos_enemy_accel_move(enemy, 0x10000);
    }
    if ((dx < 0 ? -dx : dx) <= 0x3FFFF && (dy < 0 ? -dy : dy) <= 0x3FFFF)
        enemy->step = 1;
    if (enemy->timer++ > 0x77) enemy->step = 1;
}

/* sub_080998E8: every hit knocks the ectoplasm back from the attacker at
 * 1.0 pixel per frame along the angle to it + 0x4000; a lethal blow marks
 * it defeated (sub_080683BC, done by the caller), plays song 0x6D,
 * switches to animation 1, state 1, step 0, timer 0x28, clears
 * vx/vy/ax and sets ay 0x1000. The + 0x2C/+ 0x5A palette bits are not
 * ported. */
void aos_enemy_ectoplasm_struck(AosEnemy *enemy, const AosSoma *soma,
                                const AosAnimSet *anims) {
    int32_t dx = aos_enemy_pixel_delta(enemy->x, soma->x);
    int32_t dy = aos_enemy_pixel_delta(enemy->y, soma->y);
    uint16_t angle = aos_arctan2((int16_t)dx, (int16_t)dy);
    enemy->vx = aos_fixed_mul(aos_sine(angle + 0x4000u), 0x10000);
    enemy->vy = aos_fixed_mul(aos_sine(angle), 0x10000);
    if (enemy->hp > 0) return;
    enemy->defeated = true;
    aos_audio_play_song(0x6D);
    aos_enemy_play_loop(enemy, anims, 1, true);
    enemy->state = 1;
    enemy->step = 0;
    enemy->timer = 0x28;
    enemy->vx = enemy->vy = enemy->ax = 0;
    enemy->ay = 0x1000;
}

/* EnemyEctoplasmUpdate (sub_080994E0): outside margin 4 it freezes; the
 * sub_08068AD4 activity box only feeds the sub_08021924 target table
 * (not ported). + 0x58 bit 0x20 is the margin flag, sub_0806AE54 the hit
 * flash (skipped in state 1), sub_0806D4C0's global pause 0x4BE is taken
 * as 0. State 0 runs the drift, state 1 sinks with sub_0806AEAC wisps and
 * the blink table then deletes, state 2 parks at home with the wobble and
 * only steps its animation. States 0 and 1 finish with the animation
 * step and the collision pass (skipped once defeated). */
AosHitReport aos_enemy_ectoplasm_update(
    AosEnemy *enemy, AosSoma *soma, const AosEnemyKind *kind,
    const AosWeaponEntity *weapon, const AosWeaponFrames *weapon_frames,
    int soma_atk, int soma_def, int cam_x, int cam_y,
    uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    int sx = (enemy->x >> 16) - cam_x, sy = (enemy->y >> 16) - cam_y;
    if (enemy->state != 1 && aos_enemy_outside_margins(kind, 4, sx, sy)) {
        aos_combat_tick(&enemy->combat);
        return report;
    }
    enemy->hidden = aos_enemy_outside_margins(kind, 4, sx, sy);
    if (enemy->state != 1 && enemy->hp > 0 && enemy->hit_flash)
        enemy->hit_flash--;
    switch (enemy->state) {
    case 0:
        ectoplasm_drift(enemy, soma, random);
        break;
    case 1:
        aos_enemy_accel_move(enemy, 0x40000);
        /* sub_0806AEAC: a wisp (the shared 0x49/0xCF flame spawn) every
         * 4th tick in a 16 x 3 pixel area, the blink table below 0x28,
         * deleted when the u8 timer wraps. */
        if ((enemy->timer & 3) == 0)
            aos_enemy_spawn_flame_at(enemy->x, enemy->y - 0xA8000,
                                     0x100000, 0x30000, random, &spawned);
        enemy->hidden = enemy->timer <= 0x27 ? kind->blink[enemy->timer] & 1 : false;
        if ((int8_t)--enemy->timer <= 0) enemy->removed = true;
        break;
    case 2:
        if (enemy->step == 0) {
            enemy->x = enemy->target_x;
            enemy->y = enemy->target_y +
                       (int32_t)(((int64_t)aos_sine(enemy->walk_timer + 0x4000) * 3));
            enemy->walk_timer += 0x200;
            enemy->timer++;
        }
        aos_enemy_step_anim(enemy, kind->anims);
        aos_combat_tick(&enemy->combat);
        return report;
    default:
        break;
    }
    aos_enemy_step_anim(enemy, kind->anims);
    if (!enemy->defeated)
        report = aos_enemy_collide(enemy, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
    aos_combat_tick(&enemy->combat);
    aos_enemy_take_effects(&report, &spawned);
    return report;
}
