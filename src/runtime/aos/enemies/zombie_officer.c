/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Zombie Officer (enemy 0x20).
 *
 * This family is a separate translation unit and uses only the prefixed
 * internal enemy-framework API. */

#include "../../aos_enemy_internal.h"

#include "../../aos_audio.h"

#include <string.h>

/* The officer's walk speed toward its facing (0.625). */
static void officer_step(AosEnemy *z) {
    z->vx = z->mirrored ? 0xA000 : -0xA000;
    z->vy = 0x10000;
    z->ay = 0x2800;
}

/* sub_080923E4: at the start of walk frames 3 and 7 the officer faces the
 * player and attacks him within the 100 x 70 box centred on it
 * (sub_0806CFFC(x - 50, y - 64, 100, 70)), or steps at 0.625; frames 2 and
 * 6 stop it; it moves with the zombie's walker collision, half width 10. */
static void officer_walk(AosEnemy *z, const AosSoma *soma, const AosCollision *layer) {
    if (z->step != 0) return;
    uint8_t frame = z->anim.frame;
    bool frame_start = z->anim.tick == 0;
    if ((frame == 3 || frame == 7) && frame_start) {
        aos_enemy_soldier_face(z, soma);
        int32_t x = z->x >> 16, y = z->y >> 16;
        if (aos_enemy_player_near(soma, (int16_t)(x - 0x32),
                                  (int16_t)(y - 0x40), 0x64, 0x46)) {
            z->state = 1;
            z->step = 0;
            return;
        }
        officer_step(z);
    }
    if ((frame == 2 || frame == 6) && frame_start) z->vx = 0;
    aos_enemy_walk_collide(z, layer, 0x10000, (int32_t)0xFFE00000,
                           0x80000, 10);
}

/* sub_080924AC: animation 1 once (sound 0x85 at frame 4), then back to the
 * walk. */
static void officer_attack(AosEnemy *z, const AosAnimSet *anims) {
    if (z->step == 0) {
        aos_enemy_play_loop(z, anims, 1, false);
        z->step = 1;
        return;
    }
    if (z->anim.frame == 4 && z->anim.tick == 0) aos_audio_play_song(0x85);
    if (z->cycle_done) {
        aos_enemy_play(z, anims, 0);
        z->state = z->step = 0;
        officer_step(z);
    }
}

/* EnemyZombieOfficerUpdate state 2 at timer 0x28: gibs 0x10 and 0x11 as
 * the soldier's, then 0x0F at (-6 / +6 mirrored, -13) with vy -1.0 -
 * (r & 0xFFFF) (sub_080925B4, graphics 0x081CC01C / 0x08220F84). */
static void officer_gibs(const AosEnemy *z, uint32_t (*random)(void), AosHitReport *report) {
    for (int i = 0; i < 3; ++i) {
        int32_t dx, dy;
        if (i < 2) {
            dx = (int32_t)(random() & 0xFFFFF) - 0x80000;
            dy = i == 0 ? -0x10000 - (int32_t)(random() & 0xFFFFF)
                        : -(int32_t)(random() % 0x180000u);
        } else {
            dx = z->mirrored ? 0x60000 : -0x60000;
            dy = (int32_t)0xFFF30000;
        }
        int32_t vx = aos_enemy_gib_speed(z, random);
        int32_t vy = i < 2 ? -(int32_t)(random() % 0x18000u) - 0x18000
                           : -0x10000 - (int32_t)(random() & 0xFFFF);
        if (report->child_count < AOS_ENEMY_MAX_CHILDREN)
            report->children[report->child_count++] =
                aos_enemy_make_gib(
                    z, dx, dy, vx, vy,
                    (int16_t)(i == 0 ? 0x10 : i == 1 ? 0x11 : 0x0F));
    }
}

/* EnemyZombieOfficerUpdate: within the activity window, state 0 walks, 1
 * attacks, 2 dies (gibs, then the generic death), then the animation step
 * and the on-screen collision pass; state 3 steps its animation. The
 * contact callback sub_080926CC is type 0, element 0. */
AosHitReport aos_enemy_zombie_officer_update(
    AosEnemy *z, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    int sx = (z->x >> 16) - cam_x, sy = (z->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
    switch (z->state) {
    case 0: officer_walk(z, soma, layer); break;
    case 1: officer_attack(z, kind->anims); break;
    case 2:
        if (z->timer == 0x28) officer_gibs(z, random, &spawned);
        aos_enemy_generic_death(z, kind, random, &spawned);
        break;
    case 3:
        aos_enemy_step_anim(z, kind->anims);
        return report;
    default: break;
    }
    if (!z->removed) {
        aos_enemy_step_anim(z, kind->anims);
        sx = (z->x >> 16) - cam_x;
        sy = (z->y >> 16) - cam_y;
        if (!z->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
            report = aos_enemy_collide(z, soma, kind, weapon, weapon_frames,
                                       soma_atk, soma_def);
        aos_combat_tick(&z->combat);
    }
    report.child_count = spawned.child_count;
    memcpy(report.children, spawned.children, sizeof spawned.children[0] * spawned.child_count);
    aos_enemy_take_effects(&report, &spawned);
    return report;
}
