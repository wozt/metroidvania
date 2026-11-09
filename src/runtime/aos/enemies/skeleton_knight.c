/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Skeleton Knight (enemy 0x0D).
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../../aos_enemy_internal.h"
#include "../../aos_audio.h"

/* The skeleton knight's turn to the player, then walking (or backing)
 * at the given pace with vy 1.0 and + 0x54 = 0x2800. */
static void knight_turn(AosEnemy *k, const AosSoma *soma, uint8_t step) {
    aos_enemy_soldier_face(k, soma);
    k->step = step;
    k->timer = 0;
    k->vy = 0x10000;
}

/* sub_0808ECF8: step 0 waits for the player in (x - 90, y - 48, 180, 96)
 * or a wound, then walks (animation 1); step 1 advances at 0.5, facing him
 * again every 60 frames; on the floor, with him in (x - 40, y - 40, 80,
 * 80) it attacks (r & 0x7F <= 0x40) or backs away (step 2), at the room's
 * edge (sub_0806B120(e, 1)) it backs away, and beyond the screen margins it
 * waits again. Step 2 backs away at 0.5 and advances again once he leaves
 * (x - 60, y - 40, 120, 80), after 60 frames, at a wall or the room's edge,
 * or attacks him within reach after 60 frames. Moves use sub_08069A00(e,
 * 1.0, -32, 8.0, 8). */
static void knight_walk(AosEnemy *k, const AosSoma *soma, const AosCollision *layer,
                        const AosEnemyKind *kind, int cam_x, int cam_y, uint32_t (*random)(void)) {
    int32_t x = k->x >> 16, y = k->y >> 16;
    if (k->step == 0) {
        if (!aos_enemy_player_near(soma, (int16_t)(x - 0x5A),
                                   (int16_t)(y - 0x30), 0xB4, 0x60) &&
            k->hp >= k->stats.hp)
            return;
        k->step = 1;
        k->timer = 0;
        aos_enemy_play(k, kind->anims, 1);
        k->vy = 0x10000;
        k->ay = 0x2800;
        return;
    }
    if (k->timer++ > 0x3B) {
        k->timer = 0;
        aos_enemy_soldier_face(k, soma);
    }
    bool forward = k->step == 1;
    k->vx = (k->mirrored == forward) ? 0x8000 : -0x8000;
    int result = aos_enemy_walk_collide(
        k, layer, 0x10000, (int32_t)0xFFE00000, 0x80000, 8);
    k->ay = 0x2800;
    if (!(result & 4)) return;
    x = k->x >> 16;
    y = k->y >> 16;
    bool close = aos_enemy_player_near(
        soma, (int16_t)(x - 0x28), (int16_t)(y - 0x28), 0x50, 0x50);
    if (forward) {
        if (close) {
            if ((random() & 0x7F) <= 0x40) {
                k->state = 1;
                k->step = 0;
                k->timer = 0;
                return;
            }
            knight_turn(k, soma, 2);
        } else if (aos_enemy_clamp_to_room(k, layer, 1)) {
            knight_turn(k, soma, 2);
        } else if (aos_enemy_outside_margins(
                       kind, 4, x - cam_x, y - cam_y)) {
            aos_enemy_play(k, kind->anims, 0);
            k->step = 0;
        }
        return;
    }
    if (!aos_enemy_player_near(soma, (int16_t)(x - 0x3C),
                               (int16_t)(y - 0x28), 0x78, 0x50) ||
        k->timer > 0x3B) {
        knight_turn(k, soma, 1);
    } else if (aos_enemy_clamp_to_room(k, layer, 1) || (result & 3)) {
        knight_turn(k, soma, 1);
    } else if (close && k->timer > 0x3B && (random() & 0x7F) <= 0x40) {
        k->state = 1;
        k->step = 0;
        k->timer = 0;
    }
}

/* sub_0808EFD8: it faces the player and plays animation 2 for 45 frames,
 * animation 3 for 31 (sound 0x85 at its 12th), animation 4 for 21, then
 * walks again (animation 1, step 1). */
static void knight_attack(AosEnemy *k, const AosSoma *soma, const AosAnimSet *anims) {
    switch (k->step) {
    case 0:
        aos_enemy_soldier_face(k, soma);
        aos_enemy_play_loop(k, anims, 2, false);
        k->step = 1;
        k->timer = 0;
        return;
    case 1:
        if (k->timer++ <= 0x2B) return;
        aos_enemy_play_loop(k, anims, 3, false);
        k->step = 2;
        k->timer = 0;
        return;
    case 2:
        if (k->timer == 0x0B) aos_audio_play_song(0x85);
        if (k->timer++ <= 0x1D) return;
        aos_enemy_play_loop(k, anims, 4, false);
        k->step = 3;
        k->timer = 0;
        return;
    case 3:
        if (k->timer++ <= 0x13) return;
        aos_enemy_play(k, anims, 1);
        k->state = 0;
        k->step = 1;
        k->timer = 0;
        k->vy = 0x10000;
        k->ay = 0x2800;
        return;
    default:
        return;
    }
}

/* EnemySkeletonKnightUpdate: inside the activity window, state 0 walks, 1
 * attacks, 2 runs the generic death, 3 only steps its animation; then the
 * animation step and the collision pass. Contact sub_0808F16C: type 0. */
AosHitReport aos_enemy_skeleton_knight_update(
    AosEnemy *k, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, effects = {0};
    int sx = (k->x >> 16) - cam_x, sy = (k->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
    switch (k->state) {
    case 0: knight_walk(k, soma, layer, kind, cam_x, cam_y, random); break;
    case 1: knight_attack(k, soma, kind->anims); break;
    case 2:
        aos_enemy_generic_death(k, kind, random, &effects);
        if (k->removed) return effects;
        break;
    case 3:
        aos_enemy_step_anim(k, kind->anims);
        return report;
    default:
        break;
    }
    aos_enemy_step_anim(k, kind->anims);
    sx = (k->x >> 16) - cam_x;
    sy = (k->y >> 16) - cam_y;
    if (!k->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
        report = aos_enemy_collide(k, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
    aos_combat_tick(&k->combat);
    aos_enemy_take_effects(&report, &effects);
    return report;
}
