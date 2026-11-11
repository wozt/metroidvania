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
#define face_player aos_enemy_face_player
#define move_toward aos_enemy_move_toward
#define play aos_enemy_play
#define play_loop aos_enemy_play_loop
#define player_in_range aos_enemy_player_in_range
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

/* sub_08082FFC: Headhunter's state-1 attack machine (+ 0xB case,
 * + 0xD timer, + 0x6E the shared animation frame). Cases 2-9, 15-19,
 * 25, 26, 28, 29, 35, 36, 38, 39, 45 and 46 are native no-ops; the
 * frame-gated waits (11, 13, 21, 24, 31, 34, 41 and the head of 44)
 * never write + 0xB natively — the linked head actors advance it — so
 * this port advances them on the animation end (cycle_done), a
 * documented approximation. The dash cases 22/32/42 compute a
 * player-tracking velocity and fire sub_080858B8 with song 0x67
 * (only case 22's stinger was decoded); the spawn folds into
 * spawn_flame_at. Cases 12/14 and 37/43/44/47 hand control back to
 * state 0 with their decoded step writes. */
static void headhunter_attack(AosEnemy *enemy, const AosEnemyKind *kind,
                              const AosSoma *soma, AosHitReport *report,
                              uint32_t (*random)(void)) {
    switch (enemy->step) {
    case 0:
        /* sub_0806CF2C face player; + 0x58 bit 6 the facing, then
         * sub_0803F2C8 animation 3 once and vx = 0. */
        face_player(enemy, soma);
        play_loop(enemy, kind->anims, 3, false);
        enemy->vx = 0;
        enemy->step = 1;
        break;
    case 1:
        /* Leap: at animation frame 2, vx = +/-0x18000 by the + 0x58
         * facing, vy 0x10000, ay 0x2800 and song 0x85; each frame the
         * native sub_0806D490/sub_0806D288 damp and shove are
         * approximated by the shared integrator. The + 0x59 bit-0
         * animation-end gate (or the undecoded + 0x19) returns the
         * body to state 0, step 3, timer 0x3C, animation 2. */
        if (enemy->anim.frame == 2 && enemy->anim.tick == 0) {
            enemy->vx = enemy->mirrored ? 0x18000 : -0x18000;
            enemy->vy = 0x10000;
            enemy->ay = 0x2800;
            enemy->ax = 0;
            aos_audio_play_song(0x85);
        }
        accel_move(enemy, 0x60000);
        if (enemy->cycle_done) {
            enemy->cycle_done = false;
            enemy->state = 0;
            enemy->step = 3;
            enemy->timer = 0x3C;
            play_loop(enemy, kind->anims, 2, false);
        }
        break;
    case 10:
        /* rng % 100 <= 0x31 enters case 11, else animation 7 once and
         * case 13. */
        if (random() % 100 <= 0x31) {
            enemy->step = 11;
        } else {
            play_loop(enemy, kind->anims, 7, false);
            enemy->step = 13;
        }
        enemy->timer = 0;
        enemy->ax = enemy->ay = 0;
        break;
    case 11:
    case 13:
        /* Movement-only cases: vx/vy damped by sub_0806D490/4A8 (8)
         * then sub_0806D3D8(0x60000); + 0xB advances externally. */
        enemy->vx -= enemy->vx >> 3;
        enemy->vy -= enemy->vy >> 3;
        accel_move(enemy, 0x60000);
        if (enemy->cycle_done) {
            enemy->cycle_done = false;
            enemy->step++;
        }
        break;
    case 12:
    case 14:
        /* + 0xD counts to 0x63, then state 0 step 0xC with every
         * velocity cleared and animation 6. */
        if (++enemy->timer > 0x63) {
            enemy->state = 0;
            enemy->step = 0xC;
            enemy->vx = enemy->vy = enemy->ax = enemy->ay = 0;
            play_loop(enemy, kind->anims, 6, false);
        }
        break;
    case 20:
        /* rng % 100 <= 3 enters case 21, else case 24 with
         * animation 0xD once. */
        if (random() % 100 <= 3) {
            enemy->step = 21;
        } else {
            enemy->step = 24;
            play_loop(enemy, kind->anims, 0xD, false);
        }
        enemy->timer = 0;
        break;
    case 21:
        /* Frame gate (> 1) then the external advance; the port uses
         * the animation end once the frame has passed. */
        if (enemy->anim.frame > 1 && enemy->cycle_done) {
            enemy->cycle_done = false;
            enemy->step++;
        }
        break;
    case 22:
        /* Dash: once + 0xD > 0x13 on a multiple of 4, a player-tracking
         * sub_080858B8 shot with song 0x67; + 0xD > 0x2D advances. */
        if (enemy->timer > 0x13 && (enemy->timer & 3) == 0) {
            int32_t dx = ((soma->x - enemy->x) >> 16) +
                         (int32_t)(random() & 0xFFFF) - 0x8000;
            int32_t dy = ((soma->y - enemy->y) >> 16) +
                         (int32_t)(random() & 0xFFFF) - 0x8000;
            (void)dx; (void)dy;
            spawn_flame_at(enemy->x + 0x100000, enemy->y, 0x60000,
                           0x80000, random, report);
            aos_audio_play_song(0x67);
        }
        if (++enemy->timer > 0x2D) {
            enemy->step++;
            enemy->timer = 0;
        }
        break;
    case 23:
        if (++enemy->timer > 0x1D) {
            enemy->step++;
            enemy->timer = 0;
        }
        break;
    case 24:
        if (enemy->anim.frame > 1 && enemy->cycle_done) {
            enemy->cycle_done = false;
            enemy->step++;
        }
        break;
    case 27:
        enemy->state = 0;
        enemy->step = 0x28;
        break;
    case 30:
        if (random() % 100 <= 0x31) enemy->step++;
        else enemy->step = 0x22;
        break;
    case 31:
        if (enemy->anim.frame > 1 && enemy->cycle_done) {
            enemy->cycle_done = false;
            enemy->step++;
        }
        break;
    case 32:
        if (enemy->timer > 0x13 && (enemy->timer & 3) == 0) {
            spawn_flame_at(enemy->x + 0x100000, enemy->y, 0x60000,
                           0x80000, random, report);
        }
        if (++enemy->timer > 0x2D) {
            enemy->step++;
            enemy->timer = 0;
        }
        break;
    case 33:
        if (++enemy->timer > 0x1D) {
            enemy->step++;
            enemy->timer = 0;
        }
        break;
    case 34:
        if (enemy->anim.frame > 1 && enemy->cycle_done) {
            enemy->cycle_done = false;
            enemy->step++;
        }
        break;
    case 37:
        /* Flee: state 0 step 0x1E, + 0x58 bit 7 (vflip), animation 0xE
         * once when beyond the screen margins. */
        enemy->state = 0;
        enemy->step = 0x1E;
        enemy->timer = 0;
        enemy->vflip = true;
        play_loop(enemy, kind->anims, 0xE, false);
        break;
    case 40:
        if (random() % 100 <= 0x31) enemy->step++;
        else enemy->step = 0x2C;
        break;
    case 41:
        if (enemy->anim.frame > 1 && enemy->cycle_done) {
            enemy->cycle_done = false;
            enemy->step++;
        }
        break;
    case 42:
        if (enemy->timer > 0x13 && (enemy->timer & 3) == 0) {
            spawn_flame_at(enemy->x + 0x100000, enemy->y, 0x60000,
                           0x80000, random, report);
        }
        if (++enemy->timer > 0x2D) {
            enemy->step++;
            enemy->timer = 0;
        }
        break;
    case 43:
        if (++enemy->timer > 0x1D) {
            enemy->state = 0;
            enemy->step = 0x32;
        }
        break;
    case 44:
        if (enemy->anim.frame > 1 && enemy->cycle_done) {
            enemy->cycle_done = false;
            enemy->step++;
            enemy->timer = 0;
            /* The native tail aims a velocity at the player; the port
             * keeps the shared drift instead. */
        }
        break;
    case 47:
        enemy->state = 0;
        enemy->step = 0x32;
        break;
    default:
        break;
    }
}

AosHitReport aos_enemy_headhunter_update(AosEnemy *enemy, AosSoma *soma,
                                      const AosEnemyKind *kind,
                                      const AosWeaponEntity *weapon,
                                      const AosWeaponFrames *weapon_frames,
                                      int soma_atk, int soma_def, int cam_x, int cam_y,
                                      uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (!enemy->boss_parts_spawned) {
        for (unsigned i = 0; i < 3; ++i)
            report.children[report.child_count++] =
                headhunter_head(enemy, i, cam_x, cam_y);
        enemy->spawned_count = 3;
        enemy->boss_parts_spawned = true;
    }
    /* sub_08082FFC: state 1 runs the attack machine instead of the
     * state-0 intro/idle handlers. */
    if (enemy->state == 1) {
        headhunter_attack(enemy, kind, soma, &report, random);
        step_anim(enemy, kind->anims);
        AosHitReport hit = collide(enemy, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        skeleton_merge(&report, &hit);
        aos_combat_tick(&enemy->combat);
        return report;
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
        /* sub_08081EBC's aggro probes (the sub_0806CFFC box test)
         * hand the body to state 1, case 0, once Soma closes in;
         * the port uses the shared range probe. */
        if (player_in_range(enemy, soma, 0x500000)) {
            enemy->state = 1;
            enemy->step = enemy->substep = 0;
            enemy->timer = 0;
        }
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
