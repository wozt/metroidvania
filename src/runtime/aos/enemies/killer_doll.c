/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Killer Doll (enemy 0x1F) and both dolls' piece tables.
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. The piece tables are shared with Lightning Doll. */

#include "../../aos/core/aos_enemy_internal.h"

/* sub_080C86A0: Killer Doll flight stays inside the room. Crossing a room
 * edge reflects that speed; a wall, floor or ceiling zeros it. Horizontal
 * contacts also turn the sprite. */
static void killer_bounds(AosEnemy *d, const AosCollision *layer) {
    int32_t x = d->x >> 16, y = d->y >> 16;
    int32_t column = x < 0 ? (x + 7) >> 3 : x >> 3;
    if (column < 0 || column >= aos_enemy_room_columns(layer)) {
        d->vx = -d->vx;
        d->x += d->vx * 2;
        d->mirrored = !d->mirrored;
    } else {
        int push = d->vx >= 0 ? aos_wall_push_left(layer, x + 8, y - 16)
                              : aos_wall_push_right(layer, x - 8, y - 16);
        if (push) {
            d->x += (int32_t)((uint32_t)push << 16);
            d->vx = 0;
            d->mirrored = !d->mirrored;
        }
    }
    x = d->x >> 16;
    int32_t row = y < 0 ? (y + 7) >> 3 : y >> 3;
    int rows = layer->height_cells == 32 ? 0x1A : layer->height_cells;
    if (row < 0 || row >= rows) {
        d->vy = -d->vy;
        d->y += d->vy * 2;
        return;
    }
    int depth = d->vy >= 0 ? aos_floor_depth(layer, x, y + 1, 0, false)
                           : aos_ceiling_depth(layer, x, y - 32, 0, false);
    if (depth) {
        d->y += (int32_t)((uint32_t)depth << 16);
        d->vy = 0;
    }
}

/* sub_080C7CE4: + 0x14, + 0x16 and + 0x18 count down. The doll wakes
 * within 60 pixels on both axes, rises through animations 1 and 2, then enters
 * flight. A flight leg is either 1.0 horizontally away from the player
 * (one chance in four) or 0.53125 diagonally toward him. It periodically
 * pauses in animation 3, attacks toward his upper body in animation 5,
 * and lands in animation 6 once its flight timer ends. */
static void killer_act(AosEnemy *d, const AosSoma *soma, const AosCollision *layer,
                       const AosAnimSet *anims, uint32_t (*random)(void)) {
    if ((int16_t)d->attack_wait > 0) d->attack_wait--;
    if (d->leg > 0) d->leg--;
    if ((int16_t)d->flight > 0) d->flight--;
    switch (d->step) {
    case 0:
        if (d->anim.id != 0) aos_enemy_play(d, anims, 0);
        if (aos_enemy_player_within(d, soma, 0x3C0000) &&
            (aos_enemy_target_x(soma) != d->x ||
             aos_enemy_target_y(soma) != d->y)) {
            if (d->anim.id != 1) aos_enemy_play_loop(d, anims, 1, false);
            d->timer = 0;
            d->step = 1;
        }
        break;
    case 1:
        if (d->cycle_done) {
            if (d->anim.id != 2) aos_enemy_play_loop(d, anims, 2, false);
            d->vy = -0x4000;
            d->timer = 0;
            d->step = 2;
        }
        break;
    case 2:
        if (d->timer++ <= 0x20) break;
        if (d->anim.id != 4) aos_enemy_play(d, anims, 4);
        d->vy = 0;
        d->timer = 0;
        d->step = 4;
        d->substep = 0;
        break;
    case 3:
        if (d->substep == 0) {
            d->vx = d->vy = d->ax = d->ay = 0;
            int dx = (int16_t)((d->x >> 16) -
                               (aos_enemy_target_x(soma) >> 16));
            if ((random() & 3) == 0) {
                aos_enemy_face_player(d, soma);
                d->mirrored = !d->mirrored;
                d->vx = 0x10000;
                if (dx < 0) d->vx = -d->vx;
            } else {
                d->vx = d->vy = 0x8800;
                if (dx > 0) d->vx = -d->vx;
                if (aos_enemy_player_dy(d, soma) > 0) d->vy = -d->vy;
            }
            if (d->anim.id != 4) aos_enemy_play(d, anims, 4);
            d->substep = 1;
            break;
        }
        if (!d->mirrored && d->vx > 0) d->mirrored = true;
        if (d->mirrored && d->vx < 0) d->mirrored = false;
        killer_bounds(d, layer);
        break;
    case 4:
        if (d->substep != 0) break;
        d->vx = d->vy = d->ax = d->ay = 0;
        if (d->anim.id != 3) aos_enemy_play_loop(d, anims, 3, false);
        if (!d->cycle_done) break;
        d->step = 3;
        d->attack_wait = (uint16_t)(d->attack_wait + 0x20);
        d->flight = (uint16_t)(d->flight + 0x20);
        d->leg = (int16_t)((random() & 0xFF) + 0x20);
        aos_enemy_face_player(d, soma);
        break;
    case 5:
        switch (d->substep) {
        case 0: {
            if (d->anim.id != 5) aos_enemy_play_loop(d, anims, 5, false);
            int16_t dx = (int16_t)((aos_enemy_target_x(soma) >> 16) -
                                   (d->x >> 16));
            int16_t dy = (int16_t)((aos_enemy_target_y(soma) >> 16) -
                                   (d->y >> 16) - 0x10);
            uint16_t angle = aos_arctan2(dx, dy);
            d->vx = aos_fixed_mul(aos_sine(angle + 0x14000u), 0x10000);
            d->vy = aos_fixed_mul(aos_sine(angle + 0x10000u), 0x10000);
            int32_t frames = d->vx
                                 ? (aos_enemy_target_x(soma) - d->x) / d->vx
                                 : 0;
            d->timer = (uint8_t)(frames < 0 ? -frames : frames);
            d->substep = 1;
            break;
        }
        case 1: {
            uint8_t before = d->timer--;
            if (before == 0) d->substep = 2;
            break;
        }
        case 2: {
            int dx = (int16_t)((d->x >> 16) -
                               (aos_enemy_target_x(soma) >> 16));
            bool ahead = d->mirrored ? dx < 0 : dx > 0;
            d->step = ahead ? 3 : 4;
            d->substep = 0;
            d->attack_wait = (uint16_t)((random() & 0x7FF) + 0x80);
            d->leg = (int16_t)(d->leg + 0x20);
            d->flight = (uint16_t)(d->flight + 0x20);
            break;
        }
        default:
            break;
        }
        break;
    case 6:
        if (d->substep == 2) {
            if (d->timer-- == 0) {
                d->attack_wait = (uint16_t)(d->attack_wait + 0x20);
                d->leg = (int16_t)(d->leg + 0x20);
                d->flight = (uint16_t)((random() & 0x3FF) + 0x200);
                if (d->anim.id != 1)
                    aos_enemy_play_loop(d, anims, 1, false);
                d->step = 1;
                d->substep = 0;
            }
            break;
        }
        if (d->substep == 0) d->vx = d->vy = d->ax = d->ay = 0;
        if (!aos_collision_cell(layer, d->x >> 16, d->y >> 16)) {
            d->y += 0x10000;
            break;
        }
        if (d->anim.id != 6) aos_enemy_play_loop(d, anims, 6, false);
        d->timer = (uint8_t)((random() & 0x3F) + 0x40);
        d->substep++;
        break;
    default:
        break;
    }
    d->x += d->vx;
    d->y += d->vy;
    d->vx += d->ax;
    d->vy += d->ay;
    if (d->step != 3) return;
    if ((int16_t)d->flight == 0) {
        d->step = 6;
        d->substep = 0;
    }
    if (d->leg == 0) {
        d->step = 4;
        d->substep = 0;
    }
    if ((int16_t)d->attack_wait != 0) return;
    int dx = (int16_t)((d->x >> 16) - (aos_enemy_target_x(soma) >> 16));
    bool ahead = d->mirrored ? dx < 0 : dx > 0;
    if (ahead && aos_enemy_player_within(d, soma, 0x280000)) {
        d->step = 5;
        d->substep = 0;
    }
}

/* sub_080C8914: the inert global-flag state only loops animation 0. */
static void killer_cycle(AosEnemy *d, const AosAnimSet *anims) {
    if (d->step == 0) {
        aos_enemy_play(d, anims, 0);
        d->step = 1;
    }
    aos_enemy_step_anim(d, anims);
}

/* EnemyKillerDollUpdate: the active AI, animation step and collision pass;
 * its hit callback sub_080C81AC creates the five pieces through
 * sub_080C8968 and waits in state 0xFF until they leave. */
AosHitReport aos_enemy_killer_doll_update(
    AosEnemy *d, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (d->state == 2) {
        killer_cycle(d, kind->anims);
        return report;
    }
    if (d->hp > 0 && d->hit_flash && d->state != 0xFF) {
        d->hit_flash--;
        report = aos_enemy_collide(d, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        aos_combat_tick(&d->combat);
        return report;
    }
    if (d->state == 1) killer_act(d, soma, layer, kind->anims, random);
    if (d->state == 0xFF) {
        if (!d->spawned_count) d->removed = true;
        return report;
    }
    aos_enemy_step_anim(d, kind->anims);
    report = aos_enemy_collide(d, soma, kind, weapon, weapon_frames,
                               soma_atk, soma_def);
    aos_combat_tick(&d->combat);
    return report;
}

/* The Killer Doll's (0x08528AC4) and Lightning Doll's (0x08528AE4)
 * pieces, 6 bytes each: x and y offsets (the y one places the glow),
 * sprite frame, vx / 0x1000, -vy / 0x1000, timer. */
const int8_t aos_enemy_killer_doll_pieces[5][6] = {
    {4, -8, 13, 8, 24, 60}, {4, 0, 14, 4, 24, -128}, {4, 8, 15, 0, 24, 46},
    {4, 8, 16, -4, 24, 86}, {4, 0, 17, -8, 24, 80},
};
const int8_t aos_enemy_lightning_doll_pieces[5][6] = {
    {4, -8, 20, 8, 24, 60}, {4, 0, 21, 4, 24, -128}, {4, 8, 22, 0, 24, 46},
    {4, 8, 23, -4, 24, 86}, {4, 0, 24, -8, 24, 80},
};
