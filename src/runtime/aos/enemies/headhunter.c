/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Headhunter (boss 0x6A).
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../../aos/core/aos_enemy_internal.h"
#include "../../aos/core/aos_audio.h"

#define accelerate_toward aos_enemy_accelerate_toward
#define accel_move aos_enemy_accel_move
#define collide aos_enemy_collide
#define move_toward aos_enemy_move_toward
#define play aos_enemy_play
#define play_loop aos_enemy_play_loop
#define skeleton_merge aos_enemy_skeleton_merge
#define spawn_flame_at aos_enemy_spawn_flame_at
#define step_anim aos_enemy_step_anim

/* sub_08083860: Headhunter's four-case death machine (+ 0xB). Case 0
 * sets the + 0x1A/+ 0x1B flags, clears the velocities and applies the
 * scroll/palette effect; case 1 falls to the lower screen edge and
 * waits — the three linked heads advance the native + 0xB when their
 * staged endings complete; case 2 plays animation 0x13 and loads the
 * 0x5A timer; case 3 counts it down with the 0x76 stinger, then sets
 * the boss-clear word, reports the sub_080D7308 soul and runs the
 * sub_080D6EF8 burst before the native exit deletes the entity. The
 * gEwramData + 0x348 boss flag, the gUnk_03002CB0 toggles and the
 * sub_080D7198 screen call have no runtime equivalent. */
AosHitReport aos_enemy_headhunter_death(AosEnemy *enemy,
                                      const AosEnemyKind *kind,
                                      int cam_y,
                                      uint32_t (*random)(void)) {
    AosHitReport report = {0};
    switch (enemy->step) {
    case 0:
        enemy->vx = enemy->ax = 0;
        enemy->vy = 0x10000;
        enemy->ay = 0x4000;
        enemy->step = 1;
        break;
    case 1:
        accel_move(enemy, 0x60000);
        if ((enemy->y >> 16) - cam_y > 0xBF) {
            enemy->vx = enemy->ax = enemy->vy = enemy->ay = 0;
            enemy->y = AOS_FIXED(cam_y + 0xC0);
        }
        if (enemy->hh_head_done >= 3) {
            enemy->timer = 0;
            enemy->step = 2;
        }
        break;
    case 2:
        play_loop(enemy, kind->anims, 0x13, false);
        enemy->timer = 0x5A;
        enemy->step = 3;
        /* fall through */
    case 3:
        if ((enemy->timer & 7u) == 0) aos_audio_play_song(0x76);
        if (enemy->timer) {
            enemy->timer--;
        } else {
            enemy->removed = true;
            enemy->step = 4;
            report.special_soul = true;
            /* sub_080D6EF8 burst at the body's + 0x42 position. */
            spawn_flame_at(enemy->x, enemy->y, 0x40000, 0x80000,
                           random, &report);
        }
        break;
    default:
        break;
    }
    return report;
}

/* EnemyHeadhunterCreate's three sub_08083FD0 actors. Native construction
 * places them at screen (240,124), (272,128) and (256,96); their full
 * eight-stage controllers and dedicated OAM resources are a later slice. */
static AosEnemy headhunter_head(AosEnemy *body, unsigned index, int cam_x, int cam_y) {
    static const int16_t screen_x[3] = {240, 272, 256};
    static const int16_t screen_y[3] = {124, 128, 96};
    AosEnemy head = {.id = body->id,
                     .x = AOS_FIXED(cam_x + screen_x[index]),
                     .y = AOS_FIXED(cam_y + screen_y[index]),
                     .role = AOS_ROLE_HEADHUNTER_HEAD,
                     .variant = (uint8_t)index,
                     .parent = body,
                     .static_frame = -1,
                     .combat = {.type = AOS_TYPE_ENEMY, .attack_off = true,
                                .hurt_off = true}};
    head.mirrored = body->mirrored;
    return head;
}

AosHitReport aos_enemy_headhunter_update(AosEnemy *enemy, AosSoma *soma,
                                      const AosEnemyKind *kind,
                                      const AosWeaponEntity *weapon,
                                      const AosWeaponFrames *weapon_frames,
                                      int soma_atk, int soma_def, int cam_x, int cam_y) {
    AosHitReport report = {0};
    if (!enemy->boss_parts_spawned) {
        for (unsigned i = 0; i < 3; ++i)
            report.children[report.child_count++] =
                headhunter_head(enemy, i, cam_x, cam_y);
        enemy->spawned_count = 3;
        enemy->boss_parts_spawned = true;
    }
    /* sub_08081EBC cases 0..2: begin the first head phase, run the
     * 100-frame appearance fade while sequence 24 completes, return to
     * sequence 0, then hold it for 29 frames. */
    switch (enemy->step) {
    case 0:
        enemy->hh_phase = 1;
        enemy->timer = 100;
        enemy->substep = 0;
        play(enemy, kind->anims, 0x18);
        enemy->step = 1;
        break;
    case 1:
        if (enemy->timer) enemy->timer--;
        if (enemy->cycle_done) {
            enemy->cycle_done = false;
            play(enemy, kind->anims, 0);
            enemy->timer = 0;
            enemy->step = 2;
        }
        break;
    case 2:
        if (enemy->timer++ > 27) {
            enemy->step = 3;
            enemy->timer = 60;
        }
        break;
    default:
        break;
    }
    step_anim(enemy, kind->anims);
    AosHitReport hit = collide(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    skeleton_merge(&report, &hit);
    aos_combat_tick(&enemy->combat);
    return report;
}

void aos_enemy_headhunter_head_update(AosEnemy *head, const AosEnemyKind *kind,
                                   uint32_t (*random)(void)) {
    AosEnemy *body = head->parent;
    if (!body || body->removed || body->id != AOS_BOSS_HEADHUNTER) {
        head->removed = true;
        return;
    }
    head->mirrored = body->mirrored;
    /* sub_08083FD0 cases 0-4: wait for this head's phase, make two
     * accelerating arcs above the body, then approach its phase-specific
     * attack perch at one pixel per frame. */
    if (head->step == 0 && body->hh_phase == (uint8_t)(head->variant + 1u)) {
        head->step = 1;
        head->substep = 0;
        head->timer = 0;
        return;
    }
    switch (head->step) {
    case 1:
        head->step = 2;
        /* fall through */
    case 2: {
        unsigned n = 2u - head->substep;
        uint32_t maximum = n << 19, minimum = n << 18;
        uint32_t offset;
        do offset = random() % maximum; while (offset < minimum);
        head->target_x = body->x + (int32_t)(offset - minimum);
        head->target_y = body->y - (int32_t)((n * 5u << 18) + 0x280000u);
        head->step = 3;
        break;
    }
    case 3: {
        (void)accelerate_toward(head, head->target_x, head->target_y, 0x2000);
        head->vy = head->ay * 8;
        head->ay = 0;
        int32_t old_vx = head->vx;
        accel_move(head, 0x40000);
        if (((uint32_t)(old_vx ^ head->vx) >> 31) != 0) {
            if (++head->substep <= 1) head->step = 2;
            else {
                head->step = 4;
                head->substep = 0;
            }
        }
        break;
    }
    case 4: {
        static const int32_t mirrored_x[3] = {0x200000, 0x480000, 0x400000};
        static const int32_t normal_x[3] = {-0x20000, -0x120000, -0x100000};
        static const int32_t y_offset[3] = {-0x340000, -0x2A0000, -0x280000};
        unsigned variant = head->variant < 3 ? head->variant : 0;
        if (head->substep == 0) {
            head->substep = 1;
            head->target_x = body->x +
                             (body->mirrored ? mirrored_x[variant] : normal_x[variant]);
            head->target_y = body->y + y_offset[variant];
        }
        if (move_toward(head, head->target_x, head->target_y, 0x10000)) {
            head->step = 5;
            head->timer = 0;
        } else {
            accel_move(head, 0x40000);
        }
        break;
    }
    case 5:
        head->timer++;
        /* sub_08083C1C emits four phase-specific attack actors at timers
         * 2, 5, 20 and 28. Their dedicated resources are not ported yet. */
        if (head->timer == 90) head->step = 6;
        if (head->timer > 59) {
            head->y += 0x4000;
            if (head->variant)
                head->x += head->mirrored ? -0x2000 : 0x2000;
        }
        break;
    case 6:
        aos_audio_play_song(0x11D);
        if (head->variant == 0) {
            body->step++;
            body->hh_head_done++;
            play(body, kind->anims, 1);
            step_anim(body, kind->anims);
            head->removed = true;
        } else {
            /* Native cases 1 and 2 emit ten shared visual particles here. */
            head->timer = 5;
            head->step = 7;
        }
        break;
    case 7:
        if (head->variant == 1 && head->timer && --head->timer == 0) {
            play(body, kind->anims, 5);
            step_anim(body, kind->anims);
            body->step++;
            body->hh_head_done++;
            body->y -= 0x130000;
            head->removed = true;
        } else if (head->variant == 2 && head->timer && --head->timer == 0) {
            play(body, kind->anims, 9);
            step_anim(body, kind->anims);
            body->step++;
            body->hh_head_done++;
            body->y += 0xB0000;
            head->removed = true;
        }
        break;
    default:
        break;
    }
}
