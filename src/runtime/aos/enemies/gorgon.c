/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Gorgon (enemy 0x33) and Catoblepas (0x1C, the same code) with their breath.
 *
 * This family is a separate translation unit and uses only the prefixed
 * internal enemy-framework API. */

#include "../../aos/core/aos_enemy_internal.h"
#include "../../aos/core/aos_audio.h"

static void gorgon_breath(AosEnemy *owner, AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy child;
    aos_enemy_make_static_child(&child, owner, AOS_ROLE_GORGON_BREATH, 25);
    child.x += owner->mirrored ? 0x2A0000 : -0x2A0000;
    child.y -= 0x120000;
    child.angle = owner->phase;
    child.vx = aos_fixed_mul(aos_sine(child.angle + 0x4000u), 0xC000);
    child.vy = aos_fixed_mul(aos_sine(child.angle), 0xC000);
    child.own_boxes = true;
    child.own_hurt = child.own_attack = (AosBox){-7, -7, 14, 14};
    child.contact_elements = 0x100;    /* sub_080899CC: the breath petrifies */
    report->children[report->child_count++] = child;
    owner->phase = (uint16_t)(owner->phase + 0x400);
}

AosHitReport aos_enemy_gorgon_breath_update(AosEnemy *breath, AosSoma *soma,
                                         const AosEnemyKind *kind,
                                         const AosWeaponEntity *weapon,
                                         const AosWeaponFrames *weapon_frames, int soma_atk,
                                         int soma_def, int cam_x, int cam_y) {
    AosHitReport report = {0};
    aos_enemy_integrate(breath);
    if (++breath->timer == 25) breath->static_frame = 26;
    int sx = (breath->x >> 16) - cam_x, sy = (breath->y >> 16) - cam_y;
    if (breath->timer > 40 || sx < -32 || sx > 272 || sy < -32 || sy > 192) {
        breath->removed = true;
        return report;
    }
    if (breath->timer & 1)
        report = aos_enemy_collide(breath, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    if (report.enemy_hit) breath->removed = true;
    aos_combat_tick(&breath->combat);
    return report;
}

AosHitReport aos_enemy_gorgon_update(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                                  const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                  const AosWeaponFrames *weapon_frames, int soma_atk,
                                  int soma_def, int cam_x, int cam_y,
                                  uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (e->state == 3) {
        aos_enemy_step_anim(e, kind->anims);
        return report;
    }
    int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
    if (e->state == 2) {
        if (e->step == 0) {
            for (int frame = 17; frame <= 22; ++frame)
                aos_enemy_falling_fragment(e, frame, &report, random);
            e->step = 1;
        }
        aos_enemy_generic_death(e, kind, random, &report);
        aos_combat_tick(&e->combat);
        return report;
    }
    if (e->state == 1) {
        if (e->step == 0) {
            aos_enemy_play_loop(e, kind->anims, 2, false);
            e->step = 1;
            e->timer = 0;
            e->phase = e->mirrored ? 0x4000 : 0xC000;
        } else if (e->step == 1) {
            if (e->cycle_done || ++e->timer >= 48) {
                e->step = 2;
                e->timer = 0;
            }
        } else if (e->step == 2) {
            /* sub_08089560: sound 0x67 at + 0x0D 0x1E and 0x32. */
            if (e->timer == 0x1E || e->timer == 0x32) aos_audio_play_song(0x67);
            if (e->timer >= 30 && e->timer <= 70 && !(e->timer & 1))
                gorgon_breath(e, &report);
            if (++e->timer > 76) {
                aos_enemy_play(e, kind->anims, 0);
                e->state = 0;
                e->step = 1;
                e->timer = 0;
            }
        }
    } else {
        if (e->step == 0) {
            aos_enemy_snap_to_floor(e, layer);
            e->step = 1;
            aos_enemy_face_player(e, soma);
        }
        if (!e->vx) e->vx = aos_enemy_toward_facing(e, 0x8000);
        if (aos_enemy_walk_collide(e, layer, 0x10000, (int32_t)0xFFE00000, 0x8000, 12) & 3) {
            e->mirrored = !e->mirrored;
            e->vx = aos_enemy_toward_facing(e, 0x8000);
        }
        int dx = (int)((aos_enemy_target_x(soma) - e->x) >> 16);
        int dy = (int)((aos_enemy_target_y(soma) - e->y) >> 16);
        if (dx < 0) dx = -dx;
        if (dy < 0) dy = -dy;
        if (++e->timer >= 45 && dx <= 0x50 && dy <= 0x18 && random() % 100 <= 50) {
            aos_enemy_stop(e);
            aos_enemy_face_player(e, soma);
            e->state = 1;
            e->step = 0;
            e->timer = 0;
        }
    }
    aos_enemy_step_anim(e, kind->anims);
    sx = (e->x >> 16) - cam_x;
    sy = (e->y >> 16) - cam_y;
    if (!e->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0) {
        AosHitReport hit = aos_enemy_collide(e, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        if (!report.child_count) report = hit;
        else {
            aos_enemy_copy_collision_result(&report, &hit);
        }
    }
    aos_combat_tick(&e->combat);
    return report;
}
