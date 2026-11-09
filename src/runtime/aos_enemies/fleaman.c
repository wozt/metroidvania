/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Fleaman (enemy 0x18).
 *
 * Part of aos_enemy.c, which includes this file after the shared enemy
 * framework: it is not a separate translation unit and uses that
 * file's static helpers. */

/* Sets the Fleaman's animation frame by hand, paused (+ 0x6E, + 0x6F, + 0x6C
 * bit 1). */
static void flea_frame(AosEnemy *f, uint8_t frame) {
    f->anim.frame = frame;
    f->anim.tick = 0;
    f->anim.flags |= 2;
}

/* sub_080C1FBC: step 0 sets animation 0 (paused) and vx 0.5 toward its
 * facing, then step 1 (a carried Fleaman, waiting for whatever carries it:
 * the check of its slot record never passes) or 4. Step 2 falls (animation
 * 3, frame 3) until its cell is solid, snaps to the floor and rests 16
 * frames (step 3). Step 4 (on the ground) moves, faces the player and
 * jumps by r % 30: below 10 a hop (0.8125, -3.0, 0xA000, animation 0,
 * step 6), 10 a long jump (1.625, -3.0, 0x2800), 11 to 13 a high one
 * (0.8125, -5.0, 0x3C00), otherwise straight up (-3.0, 0x5000, frame 1);
 * the last three play animation 3 (step 5), paused. In the air it falls
 * (step 5 sets frames 1 to 3 by vy), is pushed out of ceilings
 * (sub_08001C1C at y - 10) and walls (8 pixels ahead, 8 up) and lands on a
 * solid cell (step 4, frame 0). */
static void flea_hop(AosEnemy *f, const AosSoma *soma, const AosCollision *layer,
                     const AosAnimSet *anims, uint32_t (*random)(void)) {
    switch (f->step) {
    case 0:
        play(f, anims, 0);
        f->anim.flags |= 2;
        f->vx = f->mirrored ? 0x8000 : -0x8000;
        f->step = f->dropped ? 1 : 4;
        f->vy = f->ay = 0x2000;
        return;
    case 1:
        f->vy = f->ay = 0x2000;
        return;
    case 2:
        if (f->anim.id != 3) {
            play(f, anims, 3);
            flea_frame(f, 3);
        }
        f->x += f->vx;
        f->y += f->vy;
        f->vy += f->ay;
        if (!aos_collision_cell(layer, f->x >> 16, f->y >> 16)) return;
        snap_to_floor(f, layer);
        f->step = 3;
        f->timer = 0;
        return;
    case 3:
        flea_frame(f, 0);
        if (f->timer++ <= 0xF) return;
        f->step = 4;
        play(f, anims, 0);
        return;
    case 4: {
        f->x += f->vx;
        f->mirrored = (f->x >> 16) < (target_x(soma) >> 16);
        uint32_t pick = random() % 0x1E;
        if (pick <= 9) {
            f->vx = 0xD000;
            f->vy = -0x30000;
            f->ay = 0xA000;
            play(f, anims, 0);
            f->step = 6;
        } else {
            if (pick == 10) {
                f->vx = 0x1A000;
                f->vy = -0x30000;
                f->ay = 0x2800;
            } else if (pick <= 13) {
                f->vx = 0xD000;
                f->vy = -0x50000;
                f->ay = 0x3C00;
            } else {
                f->vx = 0;
                f->vy = -0x30000;
                f->ay = 0x5000;
            }
            play(f, anims, 3);
            if (pick > 13) flea_frame(f, 1);
            f->anim.flags |= 2;
            f->step = 5;
        }
        if (!f->mirrored) f->vx = -f->vx;
        return;
    }
    case 5:
    case 6: {
        f->x += f->vx;
        f->y += f->vy;
        f->vy += f->ay;
        if (f->step == 5) {
            if (-f->ay <= f->vy && f->vy <= f->ay) flea_frame(f, 2);
            else if (f->anim.frame == 0) flea_frame(f, 1);
            else if (f->anim.frame == 2) flea_frame(f, 3);
        }
        int32_t x = f->x >> 16, y = f->y >> 16;
        int depth = aos_ceiling_depth(layer, x, y - 10, 0, false);
        if (depth) f->y += (int32_t)((uint32_t)depth << 16);
        if (f->vx > 0) {
            int push = aos_wall_push_left(layer, x + 8, y - 8);
            if (push) f->x += (int32_t)((uint32_t)push << 16);
        }
        if (f->vx < 0) {
            int push = aos_wall_push_right(layer, x - 8, y - 8);
            if (push) f->x += (int32_t)((uint32_t)push << 16);
        }
        if (aos_collision_cell(layer, f->x >> 16, f->y >> 16)) {
            if (f->step == 5) {
                f->anim.frame = 0;
                f->anim.tick = 0;
            }
            f->step = 4;
        }
        return;
    }
    default:
        return;
    }
}

/* EnemyFleamanUpdate: state 3 steps animation 0; otherwise sub_0806AE54
 * counts + 0x2D down and while it is set only the collision pass runs
 * (not while dying); state 1 hops, 0xFF dies (sub_080C2388: sound 0x72,
 * animation 2, then sub_0806BE74(e, 15, 8.0, 6.0) from timer 50), then
 * the animation step and, unless dying, the collision pass. Contact
 * sub_080C242C: type 0. */
static AosHitReport flea_update(AosEnemy *f, AosSoma *soma, const AosCollision *layer,
                                const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
                                uint32_t (*random)(void)) {
    AosHitReport report = {0}, effects = {0};
    if (f->state == 3) {
        step_anim(f, kind->anims);
        return report;
    }
    if (f->state != 0xFF && f->hit_flash) f->hit_flash--;
    if (f->hit_flash && f->state != 0xFF) {
        report = collide(f, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        aos_combat_tick(&f->combat);
        return report;
    }
    if (f->state == 1) {
        flea_hop(f, soma, layer, kind->anims, random);
    } else if (f->state == 0xFF) {
        if (f->step == 0) {
            aos_audio_play_song(0x72);
            play(f, kind->anims, 2);
            f->timer = 0x32;
            f->vx = f->ax = f->vy = 0;
            f->ay = 0x2000;
            f->step = 2;
        }
        if (death_flames(f, kind, 15, 0x80000, 0x60000, random, &effects)) return effects;
    }
    step_anim(f, kind->anims);
    if (f->state != 0xFF) report = collide(f, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    aos_combat_tick(&f->combat);
    take_effects(&report, &effects);
    return report;
}
