/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Harpy (enemy 0x38) and Siren (0x0F, the same code) with their volleys.
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../../aos_enemy_internal.h"
#include "../../aos_audio.h"

#define collide aos_enemy_collide
#define copy_collision_result aos_enemy_copy_collision_result
#define face_player aos_enemy_face_player
#define falling_fragment aos_enemy_falling_fragment
#define generic_death aos_enemy_generic_death
#define integrate aos_enemy_integrate
#define make_static_child aos_enemy_make_static_child
#define move_toward aos_enemy_move_toward
#define play aos_enemy_play
#define play_loop aos_enemy_play_loop
#define step_anim aos_enemy_step_anim
#define stop aos_enemy_stop
#define target_x aos_enemy_target_x
#define target_y aos_enemy_target_y
#define toward_facing aos_enemy_toward_facing

/* sub_08075EDC: the five Harpy feathers (frames 12 down to 8), and
 * sub_08076054: the Siren's three notes (frames 14, 13, 14). */
static void harpy_shot(AosEnemy *owner, int variant, AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    static const uint16_t harpy_angles[2][5] = {
        {0x4000, 0x5000, 0x6000, 0x7000, 0x8000},
        {0x4000, 0x3000, 0x2000, 0x1000, 0x0000},
    };
    static const uint16_t siren_angles[2][3] = {
        {0x438E, 0x6000, 0x7C72},
        {0x3C72, 0x2000, 0x038E},
    };
    bool siren = owner->id == AOS_ENEMY_SIREN;
    int count = siren ? 3 : 5;
    if (variant < 0 || variant >= count) return;
    AosEnemy child;
    make_static_child(&child, owner, siren ? AOS_ROLE_SIREN_SHOT : AOS_ROLE_HARPY_SHOT,
                      siren ? (variant == 1 ? 13 : 14) : 12 - variant);
    child.variant = (uint8_t)variant;
    child.angle = siren ? siren_angles[owner->mirrored][variant]
                        : harpy_angles[owner->mirrored][variant];
    int32_t speed = siren ? 0x8000 : 0xC000;
    child.vx = aos_fixed_mul(aos_sine(child.angle + 0x4000u), speed);
    child.vy = aos_fixed_mul(aos_sine(child.angle), speed);
    child.x += owner->mirrored ? 0x10000 : -0x10000;
    child.y += 0x20000;
    child.own_boxes = true;
    child.own_hurt = child.own_attack = (AosBox){-3, -3, 6, 6};
    report->children[report->child_count++] = child;
}

AosHitReport aos_enemy_harpy_shot_update(AosEnemy *shot, AosSoma *soma,
                                      const AosEnemyKind *kind,
                                      const AosWeaponEntity *weapon,
                                      const AosWeaponFrames *weapon_frames, int soma_atk,
                                      int soma_def, int cam_x, int cam_y) {
    AosHitReport report = {0};
    if (shot->role == AOS_ROLE_SIREN_SHOT) {
        /* sub_080761B8: bend for 16 frames in one direction, then for 16
         * in the other, repeating the angular wave. */
        if ((shot->timer++ & 0x1F) < 16)
            shot->angle += 0x400;
        else
            shot->angle -= 0x400;
        shot->vx = aos_fixed_mul(aos_sine(shot->angle + 0x4000u), 0x8000);
        shot->vy = aos_fixed_mul(aos_sine(shot->angle), 0x8000);
    }
    integrate(shot);
    int sx = (shot->x >> 16) - cam_x, sy = (shot->y >> 16) - cam_y;
    if (sx < -32 || sx > 272 || sy < -32 || sy > 192) {
        shot->removed = true;
        return report;
    }
    report = collide(shot, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    if (report.enemy_hit) shot->removed = true;
    aos_combat_tick(&shot->combat);
    return report;
}

void aos_enemy_falling_fragment(AosEnemy *owner, int frame,
                                AosHitReport *report,
                                uint32_t (*random)(void)) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy child;
    make_static_child(&child, owner, AOS_ROLE_GORGON_DEBRIS, frame);
    child.vx = (int32_t)(random() & 0xFFFF) - 0x8000;
    child.vy = frame == 8 ? (int32_t)(random() & 0x3FFFF) - 0x20000 : 0;
    child.ay = 0x1400;
    child.timer = (uint8_t)((random() & 0xF) + 0x28);
    child.combat.attack_off = child.combat.hurt_off = true;
    report->children[report->child_count++] = child;
}

void aos_enemy_falling_fragment_update(AosEnemy *piece, int cam_x, int cam_y) {
    integrate(piece);
    int sx = (piece->x >> 16) - cam_x, sy = (piece->y >> 16) - cam_y;
    piece->hidden = piece->timer < 12 && (piece->timer & 1);
    if (--piece->timer == 0 || sx < -32 || sx > 272 || sy > 192) piece->removed = true;
}

AosHitReport aos_enemy_harpy_update(AosEnemy *e, AosSoma *soma, const AosEnemyKind *kind,
                                 const AosWeaponEntity *weapon,
                                 const AosWeaponFrames *weapon_frames, int soma_atk,
                                 int soma_def, int cam_x, int cam_y,
                                 uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (e->state == 4) {
        step_anim(e, kind->anims);
        return report;
    }
    int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
    if (e->state == 2) {
        if (e->step == 0) {
            for (int i = 0; i < 4; ++i) falling_fragment(e, 8, &report, random);
            face_player(e, soma);
            e->vx = e->mirrored ? -0x10000 : 0x10000;
            e->vy = -0x20000;
            e->ay = 0x2000;
            e->step = 1;
        }
        integrate(e);
        generic_death(e, kind, random, &report);
        aos_combat_tick(&e->combat);
        return report;
    }
    if (e->state == 3) {
        if (e->step == 0) {
            face_player(e, soma);
            e->mirrored = !e->mirrored;
            e->vx = toward_facing(e, 0xC000);
            e->vy = -0x30000;
            e->ay = 0x2000;
            e->timer = 0;
            e->step = 1;
        }
        integrate(e);
        if (++e->timer >= 24) {
            stop(e);
            play(e, kind->anims, 0);
            e->state = 0;
            e->step = 10;
            e->timer = 0;
        }
    } else if (e->state == 0) {
        face_player(e, soma);
        /* sub_080756FC plays the shared Harpy/Siren wing sound while its
         * idle flight advances. This controller's hover is source-adapted,
         * so connect it to the corresponding animation boundary. */
        if (e->anim.frame == 1 && e->anim.tick == 0) aos_audio_play_song(0x8B);
        e->y += aos_sine(e->phase) / 32;
        e->phase = (uint16_t)(e->phase + 0x180);
        int dx = (int)((target_x(soma) - e->x) >> 16);
        if (dx < 0) dx = -dx;
        if (++e->timer >= 30 && dx <= 0x59) {
            stop(e);
            e->state = 1;
            e->step = 0;
            e->timer = 0;
        }
    } else if (e->state == 1) {
        face_player(e, soma);
        if (e->step == 0) {
            if (random() & 1) {
                /* sub_0807594C steps 0..3: fly to Soma's X and 24 pixels
                 * above him, then brake back into the idle state. */
                play_loop(e, kind->anims, 2, false);
                aos_audio_play_song(0x87);
                e->walk_timer = target_x(soma);
                e->speed = target_y(soma) - 0x180000;
                e->step = 1;
            } else {
                play(e, kind->anims, 1);
                e->step = 10;
            }
            e->timer = 0;
        } else if (e->step == 1) {
            if (move_toward(e, e->walk_timer, e->speed, 0xC000)) {
                e->step = 2;
                e->timer = 0;
            }
            integrate(e);
        } else if (e->step == 2) {
            e->vx -= e->vx / 8;
            e->vy -= e->vy / 8;
            integrate(e);
            int32_t avx = e->vx < 0 ? -e->vx : e->vx;
            int32_t avy = e->vy < 0 ? -e->vy : e->vy;
            if (++e->timer > 60 || (avx <= 0x7FFF && avy <= 0x7FFF)) e->step = 3;
        } else if (e->step == 3) {
            e->vx -= e->vx / 4;
            e->vy -= e->vy / 4;
            integrate(e);
            int32_t avx = e->vx < 0 ? -e->vx : e->vx;
            int32_t avy = e->vy < 0 ? -e->vy : e->vy;
            if (avx <= 0x7FFF && avy <= 0x7FFF) {
                stop(e);
                play(e, kind->anims, 0);
                e->state = 0;
                e->step = 10;
                e->timer = 0;
            }
        } else if (e->step == 10 && ++e->timer > 3) {
            for (int i = 0; i < 3; ++i)
                harpy_shot(e, e->id == AOS_ENEMY_SIREN ? i : i * 2, &report);
            aos_audio_play_song(0x89);
            e->step = 11;
            e->timer = 0;
        } else if (e->step == 11 && ++e->timer > 7) {
            if (e->id == AOS_ENEMY_HARPY) {
                harpy_shot(e, 1, &report);
                harpy_shot(e, 3, &report);
                aos_audio_play_song(0x89);
            }
            e->step = 12;
            e->timer = 0;
        } else if (e->step == 12 && ++e->timer > 3) {
            play(e, kind->anims, 0);
            e->state = 0;
            e->step = 10;
            e->timer = 0;
        }
    }
    step_anim(e, kind->anims);
    sx = (e->x >> 16) - cam_x;
    sy = (e->y >> 16) - cam_y;
    if (!e->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0) {
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        if (!report.child_count) report = hit;
        else {
            copy_collision_result(&report, &hit);
        }
    }
    aos_combat_tick(&e->combat);
    return report;
}
