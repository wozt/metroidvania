/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Needles (enemy 0x3F) and its pieces.
 *
 * This family is a separate translation unit and uses only the prefixed
 * internal enemy-framework API. */

#include "../aos_enemy_internal.h"

#include "../aos_audio.h"

/* sub_080CB5E0: the animation loops with a pause of 0 to 127 frames
 * (RandomNumberGenerator & 0x7F) after every cycle; with parameter 0 set
 * it bobs: y += sin(phase + 0x4000) / 4 (rounded toward zero), phase +=
 * 0x400. */
static void needles_idle(AosEnemy *n, const AosAnimSet *anims, uint32_t (*random)(void)) {
    if (n->step == 0) {
        if (n->anim.id != 0) aos_enemy_play(n, anims, 0);
        n->cycle_done = true;
        n->step = 1;
        n->substep = 0;
    }
    if (n->substep == 0) {
        if (n->cycle_done) {
            n->substep = 1;
            n->timer = (uint8_t)(random() & 0x7F);
            n->cycle_done = false;
            n->anim.flags |= 2;
        }
    } else if (--n->timer == 0xFF) {
        n->substep = 0;
        n->anim.flags &= (uint8_t)~2;
    }
    if (!n->param0) return;
    int32_t s = aos_sine((uint32_t)n->phase + 0x4000);
    int32_t dy = (s < 0 ? s + 3 : s) >> 2;
    n->phase = (uint16_t)(n->phase + 0x400);
    n->x += n->vx;
    n->y += dy;
    n->vx += n->ax;
    n->vy = dy + n->ay;
}

/* sub_080CB6BC: five pieces from 0x08528B44 (s8 dx, s8 dy, sprite frame,
 * s8 vx >> 12, s8 vy >> 12 upward, frames), mirrored with the body. */
static void needles_shatter(const AosEnemy *n, AosHitReport *report) {
    static const int8_t pieces[5][6] = {
        {-4, -4, 5, -8, 48, 60}, {0, 0, 5, 8, 24, 30}, {0, 0, 4, -8, 40, 38},
        {0, 0, 3, 8, 40, 48}, {0, 0, 2, 0, 48, 48},
    };
    for (int i = 0; i < 5 && report->child_count < AOS_ENEMY_MAX_CHILDREN; ++i) {
        AosEnemy p = {0};
        p.id = n->id;
        p.role = AOS_ROLE_NEEDLE_PIECE;
        p.static_frame = pieces[i][2];
        p.mirrored = n->mirrored;
        int dx = pieces[i][0];
        p.vx = pieces[i][3] * 0x1000;
        if (p.mirrored) {
            p.vx = -p.vx;
            dx = -dx;
        }
        p.x = n->x + (int32_t)((uint32_t)dx << 16);
        p.y = n->y + (int32_t)((uint32_t)pieces[i][1] << 16);
        p.vy = -(pieces[i][4] * 0x1000);
        p.timer = (uint8_t)pieces[i][5];
        p.combat.attack_off = p.combat.hurt_off = true;
        report->children[report->child_count++] = p;
    }
}

/* sub_080CB7C8: a piece is deleted outside sub_08068AD4(e, -128, -64,
 * 0x1F0, 0x120), when its timer runs out or below screen Y 0xAA; otherwise
 * it moves and falls (vy += 0x3000). */
void aos_enemy_needle_piece_update(AosEnemy *p, int cam_x, int cam_y) {
    int sx = (p->x >> 16) - cam_x, sy = (p->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120 || --p->timer == 0xFF ||
        sy > 0xAA) {
        p->removed = true;
        return;
    }
    p->x += p->vx;
    p->y += p->vy;
    p->vy += 0x3000;
}

/* EnemyNeedlesUpdate: state 3 steps the animation; state 2 (sub_080CB894)
 * hides it, throws its pieces (sound 0x70; sub_080683BC is not ported) and
 * deletes it once they are gone (+ 0x13); otherwise, inside the activity
 * window, sub_080CB5E0, the animation step and the collision pass. */
AosHitReport aos_enemy_needles_update(
    AosEnemy *n, AosSoma *soma, const AosEnemyKind *kind,
    const AosWeaponEntity *weapon, const AosWeaponFrames *weapon_frames,
    int soma_atk, int soma_def, int cam_x, int cam_y,
    uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (n->state == 3) {
        aos_enemy_step_anim(n, kind->anims);
        return report;
    }
    if (n->state == 2) {
        if (n->step == 0) {
            /* sub_080CB894 step 0: sound 0x70. */
            aos_audio_play_song(0x70);
            n->hidden = true;
            needles_shatter(n, &report);
            n->spawned_count = report.child_count;
            n->step = 1;
        }
        if (n->spawned_count == 0) n->removed = true;
        return report;
    }
    int sx = (n->x >> 16) - cam_x, sy = (n->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
    needles_idle(n, kind->anims, random);
    aos_enemy_step_anim(n, kind->anims);
    sx = (n->x >> 16) - cam_x;
    sy = (n->y >> 16) - cam_y;
    if (!n->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
        report = aos_enemy_collide(n, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
    aos_combat_tick(&n->combat);
    return report;
}
