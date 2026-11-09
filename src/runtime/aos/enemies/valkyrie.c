/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Valkyrie (enemy 0x29) and Erinys (0x62, the same code).
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../../aos_enemy_internal.h"
#include "../../aos_audio.h"

#define accel_move aos_enemy_accel_move
#define collide aos_enemy_collide
#define generic_death aos_enemy_generic_death
#define head_toward aos_enemy_head_toward
#define play aos_enemy_play
#define player_in_range aos_enemy_player_in_range
#define play_loop aos_enemy_play_loop
#define soldier_face aos_enemy_soldier_face
#define step_anim aos_enemy_step_anim
#define take_effects aos_enemy_take_effects
#define target_x aos_enemy_target_x
#define target_y aos_enemy_target_y

/* sub_0806E7A8: step 0 faces the player and waits until he is within 120
 * pixels or it is hurt; step 1 heads at 2.0 for the point 64 pixels in
 * front of him and 48 above, attacking (state 1) when it gets there or
 * when it is within 60 pixels of him on both axes and more than 32 above. */
static void valkyrie_hover(AosEnemy *v, const AosSoma *soma) {
    soldier_face(v, soma);
    if (v->step == 0) {
        if (player_in_range(v, soma, 0x780000) || v->hp < v->stats.hp) v->step = 1;
        return;
    }
    int32_t tx = target_x(soma) + (v->mirrored ? (int32_t)0xFFC00000 : 0x400000);
    if (head_toward(v, tx, target_y(soma) + (int32_t)0xFFD00000, 0x20000)) {
        v->state = 1;
        v->step = 0;
        return;
    }
    int32_t dx = target_x(soma) - v->x, dy = target_y(soma) - v->y;
    if ((dx < 0 ? -dx : dx) <= 0x3BFFFF && v->y < target_y(soma) - 0x200000 &&
        (dy < 0 ? -dy : dy) <= 0x3BFFFF) {
        v->state = 1;
        v->step = 0;
    }
}

/* sub_0806E8AC: animation 1 once; it slows (5/6) and holds frame 2 for 16
 * frames, then (frame 3, sound 0x87) charges at 3.0 forward and 2.5 down,
 * pulled up by 0x4000 from frame 8, plays animation 0 when the attack
 * ends, slows by 31/32 for 20 frames (or until the player leaves 120
 * pixels), then hovers back (7/8) for 48 frames to state 0, step 1. All
 * moves go through sub_0806D3D8(e, 3.0). */
static void valkyrie_attack(AosEnemy *v, const AosSoma *soma, const AosAnimSet *anims) {
    switch (v->step) {
    case 0:
        play_loop(v, anims, 1, false);
        v->step = 1;
        v->ax = v->ay = 0;
        v->timer = 0;
        /* fall through */
    case 1:
        v->vx = v->vx * 5 / 6;
        v->vy = v->vy * 5 / 6;
        accel_move(v, 0x30000);
        if (v->anim.frame != 2) return;
        if (v->timer++ > 0xF) {
            v->anim.tick = 0x64;
            v->step = 2;
        } else {
            v->anim.tick = 0;
        }
        return;
    case 2:
        v->vx = v->vx * 5 / 6;
        v->vy = v->vy * 5 / 6;
        accel_move(v, 0x30000);
        if (v->anim.frame <= 2) return;
        v->step = 3;
        aos_audio_play_song(0x87);
        v->vx = v->mirrored ? 0x30000 : -0x30000;
        v->vy = 0x28000;
        v->ax = v->ay = 0;
        /* fall through */
    case 3:
        accel_move(v, 0x30000);
        if (v->anim.frame <= 7) return;
        v->step = 4;
        v->ay = -0x4000;
        return;
    case 4:
        accel_move(v, 0x30000);
        if (!v->cycle_done) return;
        v->step = 5;
        play(v, anims, 0);
        v->walk_timer = 0;
        return;
    case 5:
        accel_move(v, 0x30000);
        v->vx = v->vx * 31 / 32;
        if (!player_in_range(v, soma, 0x780000)) {
            v->step = 6;
            v->walk_timer = 0;
            v->ay = 0;
        }
        if (++v->walk_timer > 0x13) {
            v->step++;
            v->walk_timer = 0;
            v->ay = 0;
        }
        return;
    case 6:
        soldier_face(v, soma);
        v->vx = v->vx * 7 / 8;
        v->vy = v->vy * 7 / 8;
        accel_move(v, 0x30000);
        if (++v->walk_timer > 0x2F) {
            v->state = 0;
            v->step = 1;
        }
        return;
    default:
        return;
    }
}

/* sub_0806EA8C: the recoil, 3.0 up and away from the player, slowing by
 * 11/12, facing him again below -1.25 and back to state 0 above -0.125. */
static void valkyrie_recoil(AosEnemy *v, const AosSoma *soma) {
    if (v->step == 0) {
        v->mirrored = !((v->x >> 16) < (target_x(soma) >> 16));
        v->vx = v->mirrored ? 0x30000 : -0x30000;
        v->vy = -0x30000;
        v->step = 1;
        return;
    }
    v->vx = v->vx * 11 / 12;
    v->vy = v->vy * 11 / 12;
    v->x += v->vx;
    v->y += v->vy;
    if (v->vy > (int32_t)0xFFFEC000) soldier_face(v, soma);
    if (v->vy > (int32_t)0xFFFFE000) v->state = v->step = 0;
}

/* EnemyValkyrieUpdate (no activity window): sub_0806AD24 stuns it; state
 * 0 hovers, 1 attacks, 2 dies (9/10 of vx, falling, the generic death), 3
 * recoils, 4 only steps its animation; then the animation step and the
 * collision pass. */
AosHitReport aos_enemy_valkyrie_update(AosEnemy *v, AosSoma *soma, const AosEnemyKind *kind,
                                    const AosWeaponEntity *weapon,
                                    const AosWeaponFrames *weapon_frames, int soma_atk,
                                    int soma_def, int cam_x, int cam_y,
                                    uint32_t (*random)(void)) {
    AosHitReport report = {0}, effects = {0};
    if (v->hp > 0 && v->hit_flash) {
        v->hit_flash--;
        if (v->state != 2) report = collide(v, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        aos_combat_tick(&v->combat);
        return report;
    }
    switch (v->state) {
    case 0: valkyrie_hover(v, soma); break;
    case 1: valkyrie_attack(v, soma, kind->anims); break;
    case 2:
        v->vx = v->vx * 9 / 10;
        accel_move(v, 0x30000);
        generic_death(v, kind, random, &effects);
        if (v->removed) return effects;
        break;
    case 3: valkyrie_recoil(v, soma); break;
    case 4:
        step_anim(v, kind->anims);
        return report;
    default:
        break;
    }
    step_anim(v, kind->anims);
    int sx = (v->x >> 16) - cam_x, sy = (v->y >> 16) - cam_y;
    if (!v->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
        report = collide(v, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    aos_combat_tick(&v->combat);
    take_effects(&report, &effects);
    return report;
}
