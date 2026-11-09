/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Dead Crusader (enemy 0x52).
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../aos_enemy_internal.h"
#include "../aos_audio.h"

#define clamp_to_room aos_enemy_clamp_to_room
#define collide aos_enemy_collide
#define generic_death aos_enemy_generic_death
#define outside_margins aos_enemy_outside_margins
#define play aos_enemy_play
#define play_loop aos_enemy_play_loop
#define player_near aos_enemy_player_near
#define soldier_face aos_enemy_soldier_face
#define step_anim aos_enemy_step_anim
#define take_effects aos_enemy_take_effects
#define walk_collide aos_enemy_walk_collide

/* The dead crusader's guard: Soma's + 0x10 flags & 0x360 (sub_080212C8:
 * attacking) while it stands on the floor raise its shield (animation 7
 * once, step 5, + 0x21 set, no speed). */
static bool crusader_guard(AosEnemy *k, const AosSoma *soma, const AosAnimSet *anims) {
    if (!(soma->flags & 0x360u)) return false;
    play_loop(k, anims, 7, false);
    k->step = 5;
    k->timer = 0;
    k->guarding = true;
    k->vx = k->ax = 0;
    k->vy = 0x10000;
    k->ay = 0x2800;
    return true;
}

/* The dead crusader walking on (animation 1 kept) toward its facing. */
static void crusader_turn(AosEnemy *k, const AosSoma *soma, uint8_t step) {
    soldier_face(k, soma);
    k->step = step;
    k->timer = 0;
    k->vy = 0x10000;
    k->ay = 0x2800;
}

/* sub_0808F364: the skeleton knight's walk at 0.75 with a guard. Step 0
 * waits for the player in (x - 90, y - 48, 180, 96) or a wound, then walks
 * (animation 1); step 1 advances, facing him again every 60 frames; on the
 * floor it guards while he attacks, attacks him in (x - 40, y - 40, 80, 80)
 * (r & 0x7F <= 0x40) or else backs away (step 2), backs away at the room's
 * edge (sub_0806B120(e, 1)) and waits again beyond the screen margins.
 * Step 2 backs away and advances again once he leaves (x - 60, y - 40,
 * 120, 80), after 60 frames, at a wall or the room's edge. Step 5 guards,
 * facing him, for 60 frames on the floor, then walks on. Moves use
 * sub_08069A00(e, 1.0, -32, 8.0, 8). */
static void crusader_walk(AosEnemy *k, const AosSoma *soma, const AosCollision *layer,
                          const AosEnemyKind *kind, int cam_x, int cam_y,
                          uint32_t (*random)(void)) {
    int32_t x = k->x >> 16, y = k->y >> 16;
    switch (k->step) {
    case 0:
        if (!player_near(soma, (int16_t)(x - 0x5A), (int16_t)(y - 0x30), 0xB4, 0x60) &&
            k->hp >= k->stats.hp)
            return;
        k->step = 1;
        play(k, kind->anims, 1);
        k->vy = 0x10000;
        k->ay = 0x2800;
        return;
    case 1:
    case 2: {
        if (k->timer++ > 0x3B) {
            k->timer = 0;
            soldier_face(k, soma);
        }
        bool forward = k->step == 1;
        k->vx = (k->mirrored == forward) ? 0xC000 : -0xC000;
        int result = walk_collide(k, layer, 0x10000, (int32_t)0xFFE00000, 0x80000, 8);
        if (!(result & 4) || crusader_guard(k, soma, kind->anims)) return;
        x = k->x >> 16;
        y = k->y >> 16;
        bool close = player_near(soma, (int16_t)(x - 0x28), (int16_t)(y - 0x28), 0x50, 0x50);
        if (forward) {
            if (close) {
                if ((random() & 0x7F) <= 0x40) {
                    k->state = 1;
                    k->step = 0;
                    k->timer = 0;
                    return;
                }
                k->step = 2;
                k->timer = 0;
                k->vy = 0x10000;
                k->ay = 0x2800;
            } else if (clamp_to_room(k, layer, 1)) {
                k->step = 2;
                k->timer = 0;
                k->vy = 0x10000;
                k->ay = 0x2800;
            } else if (outside_margins(kind, 4, x - cam_x, y - cam_y)) {
                play(k, kind->anims, 0);
                k->step = 0;
            }
            return;
        }
        if (!player_near(soma, (int16_t)(x - 0x3C), (int16_t)(y - 0x28), 0x78, 0x50) ||
            k->timer > 0x3B || clamp_to_room(k, layer, 1) || (result & 3))
            crusader_turn(k, soma, 1);
        return;
    }
    case 5: {
        soldier_face(k, soma);
        k->guarding = true;
        int result = walk_collide(k, layer, 0x10000, (int32_t)0xFFE00000, 0x80000, 8);
        if (!(result & 4) || k->timer++ <= 0x3B) return;
        play(k, kind->anims, 1);
        k->step = 1;
        k->timer = 0;
        k->vy = 0x10000;
        k->ay = 0x2800;
        return;
    }
    default:
        return;
    }
}

/* sub_0808F6E8: it faces the player and raises its sword (animation 2) for
 * 38 frames, then (animation 3) one chance in two (r & 0x7F <= 0x3F) to
 * thrust (step 2: sound 0x85 at the 12th frame, 31 frames, animation 4 for
 * 21) or else to slash (step 5: sound 0x85 at the 12th frame, 13 frames,
 * then animation 6 three times 13 frames); then it walks on (animation 1,
 * state 0, step 1). */
static void crusader_attack(AosEnemy *k, const AosSoma *soma, const AosAnimSet *anims,
                            uint32_t (*random)(void)) {
    switch (k->step) {
    case 0:
        soldier_face(k, soma);
        play_loop(k, anims, 2, false);
        k->step = 1;
        k->timer = 0;
        return;
    case 1:
        if (k->timer++ <= 0x25) return;
        play_loop(k, anims, 3, false);
        k->timer = 0;
        k->step = (random() & 0x7F) <= 0x3F ? 2 : 5;
        return;
    case 2:
        if (k->timer == 0x0B) aos_audio_play_song(0x85);
        if (k->timer++ <= 0x1D) return;
        play_loop(k, anims, 4, false);
        k->step = 3;
        k->timer = 0;
        return;
    case 3:
        if (k->timer++ <= 0x13) return;
        break;
    case 5:
        if (k->timer == 0x0B) aos_audio_play_song(0x85);
        if (k->timer++ <= 0x0B) return;
        play_loop(k, anims, 6, false);
        k->step = 6;
        k->substep = 0;
        k->timer = 0;
        return;
    case 6:
        if (k->timer++ <= 0x0B) return;
        if (k->substep++ <= 2) {
            play_loop(k, anims, 6, false);
            k->timer = 0;
            return;
        }
        break;
    default:
        return;
    }
    play(k, anims, 1);
    k->state = 0;
    k->step = 1;
    k->timer = 0;
    k->vy = 0x10000;
    k->ay = 0x2800;
}

/* EnemyDeadCrusaderUpdate: + 0x21 cleared each frame; alive and beyond
 * the margins of sub_0806D128(e, 4) it does nothing. Then state 0 walks,
 * 1 attacks, 2 runs the generic death (hidden beyond the margins), 3 only
 * steps its animation; then the animation step and the collision pass.
 * The hit stun of sub_0806D4C0 needs the global pause 0x4BE (taken as 0)
 * and the tracking list of sub_08021924 is not ported. */
AosHitReport aos_enemy_crusader_update(AosEnemy *k, AosSoma *soma, const AosCollision *layer,
                                    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                    const AosWeaponFrames *weapon_frames, int soma_atk,
                                    int soma_def, int cam_x, int cam_y,
                                    uint32_t (*random)(void)) {
    AosHitReport report = {0}, effects = {0};
    k->guarding = false;
    bool outside = outside_margins(kind, 4, (k->x >> 16) - cam_x, (k->y >> 16) - cam_y);
    if (k->state != 2 && outside) return report;
    k->hidden = outside;
    switch (k->state) {
    case 0: crusader_walk(k, soma, layer, kind, cam_x, cam_y, random); break;
    case 1: crusader_attack(k, soma, kind->anims, random); break;
    case 2:
        generic_death(k, kind, random, &effects);
        if (k->removed) return effects;
        break;
    case 3:
        step_anim(k, kind->anims);
        return report;
    default:
        break;
    }
    step_anim(k, kind->anims);
    int sx = (k->x >> 16) - cam_x, sy = (k->y >> 16) - cam_y;
    if (!k->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
        report = collide(k, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    aos_combat_tick(&k->combat);
    take_effects(&report, &effects);
    return report;
}
