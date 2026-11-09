/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Blue Crow (enemy 0x09) and Red Crow (0x2D).
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../../aos_enemy_internal.h"
#include "../../aos_audio.h"

#define collide aos_enemy_collide
#define death_flames aos_enemy_death_flames
#define face_player aos_enemy_face_player
#define integrate aos_enemy_integrate
#define move_toward aos_enemy_move_toward
#define play aos_enemy_play
#define player_within aos_enemy_player_within
#define play_loop aos_enemy_play_loop
#define spawn_blood aos_enemy_spawn_blood
#define step_anim aos_enemy_step_anim
#define stop aos_enemy_stop
#define target_x aos_enemy_target_x
#define target_y aos_enemy_target_y

/* sub_080C9AF4: perch until the player comes within 60 pixels (sound
 * 0x8B), then keep returning to a point 42 pixels above him and 32 pixels
 * behind him whenever he is more than 72 pixels away horizontally and 16
 * vertically. Step 2 (a glide from the current velocity) is not entered by
 * any code read and is not ported. */
static void crow_fly(AosEnemy *c, const AosSoma *soma, const AosAnimSet *anims) {
    int32_t ty = target_y(soma) - 0x2A0000;
    switch (c->step) {
    case 0:
        c->phase = 0;
        face_player(c, soma);
        if (c->anim.id != 0) play(c, anims, 0);
        c->step = 1;
        c->substep = c->timer = 0;
        /* fallthrough */
    case 1:
        if (player_within(c, soma, 0x3C0000)) {
            c->step = 3;
            c->substep = 0;
            aos_audio_play_song(0x8B);
        }
        break;
    case 3:
        face_player(c, soma);
        switch (c->substep) {
        case 0:
            c->phase = (uint8_t)(c->phase + 1);
            if (c->anim.id != 2) play(c, anims, 2);
            stop(c);
            c->timer = 0x20;
            c->substep = 1;
            /* fallthrough */
        case 1:
            if (--c->timer == 0xFF) c->substep = 2;
            break;
        case 2: {
            int32_t dx = c->x - target_x(soma), dy = c->y - target_y(soma);
            if ((dx < 0 ? -dx : dx) > 0x480000 && (dy < 0 ? -dy : dy) > 0x100000) c->substep = 3;
            break;
        }
        case 3: {
            if (c->anim.id != 1) play(c, anims, 1);
            /* The player's + 0x58 bit 0x40: behind him is to his right. */
            int32_t tx = target_x(soma) + (soma->facing_left ? 0x200000 : -0x200000);
            if (move_toward(c, tx, ty, 0x1A000)) {
                if (c->anim.id != 2) play(c, anims, 2);
                stop(c);
                c->substep = 2;
            }
            break;
        }
        default:
            break;
        }
        break;
    default:
        break;
    }
    integrate(c);
}

/* sub_080C9E2C: two feathers (EntityCreateInRange(0x1B, 0x48,
 * sub_080C9ED4)), sprite frame 8 of the crow, at (x + 8, y + 4) and
 * (x - 8, y - 4) in whole pixels, parameter 0 = 0 / 1. */
static void crow_feathers(const AosEnemy *c, AosHitReport *report) {
    for (int i = 0; i < 2 && report->child_count < AOS_ENEMY_MAX_CHILDREN; ++i) {
        AosEnemy f = {0};
        f.id = c->id;
        f.role = AOS_ROLE_FEATHER;
        f.static_frame = 8;
        f.param0 = (int16_t)i;
        f.x = (int32_t)((uint32_t)((c->x >> 16) + (i ? -8 : 8)) << 16);
        f.y = (int32_t)((uint32_t)((c->y >> 16) + (i ? -4 : 4)) << 16);
        f.combat.attack_off = f.combat.hurt_off = true;
        report->children[report->child_count++] = f;
    }
}

/* sub_080C9ED4: a feather drifts at 0.25 to the side (parameter 1:
 * mirrored, to the left) and 0.125 down, its X speed decreasing by 0x300
 * per frame; after 49 frames (state 1) and 25 more (state 2) it is
 * deleted; it blinks on the timer's bit 0. The palette bits set at the end
 * of state 1 (+ 0x5A = 2) are not ported. */
void aos_enemy_feather_update(AosEnemy *f) {
    switch (f->state) {
    case 0:
        f->vx = 0x4000;
        f->vy = 0x2000;
        f->ax = -0x300;
        if (f->param0) {
            f->vx = -f->vx;
            f->ax = -f->ax;
            f->mirrored = true;
        }
        f->timer = 0x30;
        f->state = 1;
        /* fall through */
    case 1:
        if (--f->timer == 0xFF) {
            f->timer = 0x18;
            f->state = 2;
        }
        break;
    case 2:
        if (--f->timer == 0xFF) f->removed = true;
        break;
    default:
        break;
    }
    f->x += f->vx;
    f->y += f->vy;
    f->vx += f->ax;
    f->vy += f->ay;
    f->hidden = f->timer & 1;
}

/* sub_080C9D68: the feathers, animation 3, a blood splash of
 * sub_0806E664 at the crow, then a fall at 0.5 per frame with a splash
 * 4 pixels lower every 16 frames and sub_0806BE74 over 64 frames (a flame
 * every 6 frames over 8 x 8 pixels), then the deletion. Sound 0x72, the
 * palette bits of + 0x5A and sub_080683BC are not ported. */
static void crow_die(AosEnemy *c, const AosEnemyKind *kind, uint32_t (*random)(void),
                     AosHitReport *report) {
    if (c->step == 0) {
        /* sub_080C9D68 / sub_080CA308 step 0: sound 0x72. */
        aos_audio_play_song(0x72);
        crow_feathers(c, report);
        if (c->anim.id != 3) play_loop(c, kind->anims, 3, false);
        stop(c);
        c->vy = 0x8000;
        c->timer = 0x40;
        c->step = 1;
        c->substep = 0;
        spawn_blood(c->x >> 16, c->y >> 16, random, report);
    }
    c->y += c->vy;
    if ((c->timer & 0xF) == 0) spawn_blood(c->x >> 16, (c->y >> 16) + 4, random, report);
    if (death_flames(c, kind, 6, 0x80000, 0x80000, random, report)) return;
    step_anim(c, kind->anims);
}

/* sub_080CA0D4: the red crow perches facing the player until he is within
 * 120 pixels on both axes (sub_0806BF78); then it hovers (animation 2,
 * still) for 128 frames, turning to him (odd rounds, + 0x14) or away (even
 * ones) halfway, and swoops (animation 1, sound 0x8B): vx 0.5 toward its
 * facing with + 0x50 0x1000 that way, vy 2.0 with + 0x54 -0x1000, for 64
 * frames, then hovers again; after an odd round other than the first,
 * with the player beyond 120 pixels, it plays animation 4 once and
 * perches again. It always moves by its velocity, then accelerates. */
static void red_crow_fly(AosEnemy *c, const AosSoma *soma, const AosAnimSet *anims) {
    switch (c->step) {
    case 0:
        c->volley_count = 0;
        face_player(c, soma);
        if (c->anim.id != 0) play(c, anims, 0);
        c->step = 1;
        c->substep = 0;
        c->timer = 0;
        /* fall through */
    case 1:
        if (player_within(c, soma, 0x780000)) {
            c->step = 3;
            c->substep = 0;
        }
        break;
    case 2:
        if (c->substep == 0) {
            if (c->anim.id != 1) play(c, anims, 1);
            c->vx = 0x8000;
            c->vy = 0x20000;
            c->ax = 0x1000;
            c->ay = -0x1000;
            if (!c->mirrored) {
                c->vx = -0x8000;
                c->ax = -0x1000;
            }
            c->timer = (uint8_t)((c->vy / c->ay < 0 ? -(c->vy / c->ay) : c->vy / c->ay) * 2);
            c->substep = 1;
            aos_audio_play_song(0x8B);
        }
        if (--c->timer == 0xFF) {
            c->step = 3;
            c->substep = 0;
        }
        break;
    case 3:
        if (c->substep == 0) {
            c->volley_count++;
            if (c->anim.id != 2) play(c, anims, 2);
            c->vx = c->vy = c->ax = c->ay = 0;
            c->timer = 0x80;
            c->substep = 1;
        }
        if (c->timer == 0x40) {
            if (c->volley_count & 1) face_player(c, soma);
            else c->mirrored = !c->mirrored;
        }
        if (--c->timer == 0xFF) {
            c->step = 2;
            c->substep = 0;
            if ((c->volley_count & 1) && c->volley_count != 1 &&
                !player_within(c, soma, 0x780000)) {
                c->step = 4;
                c->cycle_done = false;
            }
        }
        break;
    case 4:
        if (c->substep == 0) {
            if (c->cycle_done) c->substep = 1;
        } else if (c->substep == 1) {
            if (c->anim.id != 4) play_loop(c, anims, 4, false);
            c->substep = 2;
            if (c->cycle_done) c->step = c->substep = 0;
        } else if (c->cycle_done) {
            c->step = c->substep = 0;
        }
        break;
    default:
        break;
    }
    c->x += c->vx;
    c->y += c->vy;
    c->vx += c->ax;
    c->vy += c->ay;
}

/* EnemyBlueCrowUpdate: no activity window and no on-screen test before the
 * collision pass (sub_080421AC). sub_0806AD24 stuns it while alive: with
 * + 0x2D (set to 8 by a hit) non-zero, sub_0806AE54 decrements it and the
 * flight and the animation step are skipped; the palette flash it writes,
 * the global pause 0x4BE (taken as 0), the + 0x38 bit 0x100 stop and the
 * tracking list of sub_08021924 are not ported. */
AosHitReport aos_enemy_crow_update(AosEnemy *c, AosSoma *soma, const AosEnemyKind *kind,
                                const AosWeaponEntity *weapon, const AosWeaponFrames *weapon_frames,
                                int soma_atk, int soma_def, uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (c->state == 3) {
        /* sub_080C9FFC: animation 0 (unless playing), stepped each frame;
         * no collision pass. */
        if (c->step == 0) {
            if (c->anim.id != 0) play(c, kind->anims, 0);
            c->step = 1;
        }
        step_anim(c, kind->anims);
        return report;
    }
    if (c->state == 2) {
        crow_die(c, kind, random, &report);
    } else if (c->hp > 0 && c->hit_flash) {
        c->hit_flash--;
        report = collide(c, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    } else {
        if (c->id == AOS_ENEMY_RED_CROW) red_crow_fly(c, soma, kind->anims);
        else crow_fly(c, soma, kind->anims);
        step_anim(c, kind->anims);
        report = collide(c, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    }
    aos_combat_tick(&c->combat);
    return report;
}
