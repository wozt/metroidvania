/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Beam Skeleton (enemy 0x59), its beam and bones.
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../aos_enemy_internal.h"

#include <stdlib.h>

#define clamp_to_room aos_enemy_clamp_to_room
#define collide aos_enemy_collide
#define face_player aos_enemy_face_player
#define ground_walk aos_enemy_ground_walk
#define play aos_enemy_play
#define play_loop aos_enemy_play_loop
#define player_dy aos_enemy_player_dy
#define player_in_centred aos_enemy_player_in_centred
#define step_anim aos_enemy_step_anim
#define target_x aos_enemy_target_x

/* The beam skeleton's walker probes at 0x08528B04 are the kind's. Its
 * bones (0x08528B17, 6 bytes each): x and y offsets, sprite frame, vx *
 * 0x1000, vy * -0x1000 (minus 0x10); the last byte (+ 0x0D) is unused by
 * their update sub_0806C5AC. */
static const int8_t beam_bones[7][5] = {
    {0, -8, 0x1E, 20, 50}, {0, -6, 0x1B, 16, 49}, {0, -4, 0x18, 17, 48},
    {4, -6, 0x1A, 12, 56}, {4, -3, 0x19, 18, 51}, {-4, -6, 0x1D, 24, 50},
    {-4, -3, 0x1C, 12, 40},
};

/* sub_080CB1B4: seven bones of sub_0806C5AC at its position (+ 0x5C and
 * facing its own), + 0x54 0x3000; with the player to its right
 * (sub_0806BBC4 > 0) their x offsets and vx point left. */
void aos_enemy_beam_shatter(AosEnemy *e, const AosSoma *soma,
                            AosHitReport *report) {
    bool right = e->x < target_x(soma);
    for (int i = 0; i < 7 && report->child_count < AOS_ENEMY_MAX_CHILDREN; ++i) {
        AosEnemy b = {0};
        b.id = e->id;
        b.role = AOS_ROLE_DEBRIS;
        b.static_frame = (uint8_t)beam_bones[i][2];
        b.mirrored = e->mirrored;
        b.combat.attack_off = b.combat.hurt_off = true;
        int dx = beam_bones[i][0];
        b.vx = (int32_t)(uint8_t)beam_bones[i][3] << 12;
        b.vy = -(beam_bones[i][4] << 12) - 0x10;
        b.ay = 0x3000;
        if (right) {
            b.vx = -abs(b.vx);
            dx = -abs(dx);
        }
        b.x = (int32_t)((uint32_t)((e->x >> 16) + dx) << 16) | (e->x & 0xFFFF);
        b.y = (int32_t)((uint32_t)((e->y >> 16) + beam_bones[i][1]) << 16) | (e->y & 0xFFFF);
        report->children[report->child_count++] = b;
    }
}

/* sub_080CA770: it patrols at 0.375 (the probe walker, mode 12), turning
 * 40 pixels from its home column or past the patrol bounds, at a wall or
 * the room's edge; with the player in the 0xC0 x 0x80 box centred on it,
 * it stops and closes in (state 1). */
static void beam_patrol(AosEnemy *e, const AosSoma *soma, const AosCollision *layer,
                        const AosEnemyKind *kind) {
    if (e->step == 0) {
        if (e->anim.id != 1) play(e, kind->anims, 1);
        e->step = 1;
        return;
    }
    int16_t x = (int16_t)(e->x >> 16);
    int16_t d = (int16_t)(e->home - x);
    if (e->mirrored) {
        if (e->home_right < x || d < -0x28) e->mirrored = false;
    } else if (e->home_left > x || d > 0x28) {
        e->mirrored = true;
    }
    bool turn = ground_walk(e, soma, layer, &kind->probes, 0x6000, 12) & 3;
    if (clamp_to_room(e, layer, 1)) turn = true;
    if (turn) e->mirrored = !e->mirrored;
    if (player_in_centred(e, soma, 0xC0, 0x80)) {
        e->vx = 0;
        e->state = 1;
        e->substep = e->step = 0;
    }
}

/* sub_080CA884: with the player more than 32 pixels above or below it
 * patrols again; otherwise it waits r & 0x1F frames before it may fire
 * (+ 0x1C) and steps 20 frames at a random pace ((r & 0x3FFF) + 0x8000,
 * braked by 1/16 a frame) toward the player, or away when he is in the
 * ((r & 0x7F) + 0x40) x 0x80 box centred on it. Then, the wait over and
 * the player in its 100 x 40 box, it fires (state 3); otherwise its +
 * 0x19 grows, and r % 40 below it with the player more than 48 pixels
 * away clears it and may pause (r % 50 <= 14: state 2), or halves it near
 * him, before another step. */
static void beam_close_in(AosEnemy *e, const AosSoma *soma, const AosCollision *layer,
                          const AosEnemyKind *kind, uint32_t (*random)(void)) {
    switch (e->step) {
    case 0: {
        int dy = player_dy(e, soma);
        if (abs(dy) > 0x20) {
            e->state = e->substep = e->step = 0;
            return;
        }
        e->rest = (uint16_t)(random() & 0x1F);
        e->step = 1;
        e->substep = 0;
    }
        /* fall through */
    case 1:
        face_player(e, soma);
        e->timer = 0x14;
        e->speed = (int32_t)(random() & 0x3FFF) + 0x8000;
        if (player_in_centred(e, soma, (int)(random() & 0x7F) + 0x40, 0x80)) e->speed = -e->speed;
        if (e->anim.id != 1) play(e, kind->anims, 1);
        e->step = 2;
        e->substep = 0;
        /* fall through */
    case 2:
        if (e->rest) e->rest--;
        e->speed = e->speed > 0 ? e->speed - (e->speed >> 4) : e->speed + (-e->speed >> 4);
        if (e->timer) {
            e->timer--;
        } else {
            if (e->rest == 0 && player_in_centred(e, soma, 0x64, 0x28)) {
                e->state = 3;
                e->substep = e->step = 0;
                return;
            }
            e->patience++;
            if (random() % 0x28 < e->patience) {
                int dx = (int16_t)((e->x >> 16) - (target_x(soma) >> 16));
                if (abs(dx) > 0x30) {
                    e->patience = 0;
                    if (random() % 0x32 <= 0x0E) {
                        e->state = 2;
                        e->substep = e->step = 0;
                    }
                    return;
                }
                e->patience >>= 1;
            }
            e->step--;
            e->substep = 0;
        }
        ground_walk(e, soma, layer, &kind->probes, e->speed, 12);
        return;
    default:
        return;
    }
}

/* sub_080CA9E0: it stands (animation 0) for 60 frames, then closes in. */
static void beam_pause(AosEnemy *e, const AosAnimSet *anims) {
    if (e->step == 0) {
        if (e->anim.id != 0) play(e, anims, 0);
        e->timer = 0x3C;
        e->step = 1;
    }
    if (--e->timer == 0) {
        e->state = 1;
        e->substep = e->step = 0;
    }
}

/* sub_080CAC34: the muzzle flash (sub_080CB0D0: frame 0x15, translucent)
 * and the beam (sub_080CAE74: frame 0x16, + 0x5C 9, its own box (-4, -6,
 * 8, 12) for both, 3/2 of the skeleton's contact attack) 11 pixels ahead
 * of it and 17 above, sharing its affine slot. */
static void beam_fire(const AosEnemy *e, AosHitReport *report) {
    for (int i = 0; i < 2 && report->child_count < AOS_ENEMY_MAX_CHILDREN; ++i) {
        AosEnemy c = {0};
        c.id = e->id;
        c.role = i == 0 ? AOS_ROLE_BEAM_FLASH : AOS_ROLE_BEAM;
        c.static_frame = i == 0 ? 0x15 : 0x16;
        c.mirrored = e->mirrored;
        c.x = e->x + (e->mirrored ? 0xB0000 : -0xB0000);
        c.y = e->y - 0x110000;
        c.hp = 1;
        c.scaled = true;
        c.scale = e->scale;
        c.stats = e->stats;
        c.stats.contact = (uint8_t)(e->stats.contact * 3 >> 1);
        if (i == 0) {
            c.translucent = true;
            c.combat.attack_off = c.combat.hurt_off = true;
        } else {
            c.combat.type = AOS_TYPE_ENEMY;
            c.combat.hurt_off = true;
            c.own_boxes = true;
            c.own_hurt = c.own_attack = (AosBox){-4, -6, 8, 12};
        }
        report->children[report->child_count++] = c;
    }
}

/* sub_080CAA2C: animation 2 to its end; animation 3 for 33 frames (its
 * slot's vertical scale + 0x20 at 0), then + 0x1A bit 0; animation 4 at
 * full scale until sprite frame 0x13 fires the beam; animation 5 for 43
 * frames, the scale then shrinking by 0x400 a frame to 0; 9 more frames
 * with the beam gone (+ 0x1A cleared), then it closes in again. */
static void beam_attack(AosEnemy *e, const AosEnemyKind *kind, AosHitReport *report) {
    const AosAnimSet *anims = kind->anims;
    switch (e->step) {
    case 0:
        if (e->anim.id != 2) play_loop(e, anims, 2, false);
        if (e->cycle_done) {
            e->step = 1;
            e->substep = 0;
        }
        return;
    case 1:
        if (e->substep == 0) {
            if (e->anim.id != 3) play(e, anims, 3);
            e->timer = 0x20;
            e->substep = 1;
            e->scale = 0;
        }
        if (--e->timer == 0xFF) {
            e->firing = true;
            e->step = 2;
            e->substep = 0;
        }
        return;
    case 2:
        e->scale = 0xFFFF;
        if (e->anim.id != 4) play_loop(e, anims, 4, false);
        if (e->anim.id < AOS_ENEMY_MAX_ANIMS && e->anim.frame < AOS_ENEMY_MAX_FRAMES &&
            kind->sprites[e->anim.id][e->anim.frame] == 0x13) {
            beam_fire(e, report);
            e->step = 3;
            e->substep = 0;
        }
        return;
    case 3:
        if (e->substep == 0) {
            if (e->anim.id != 5) play(e, anims, 5);
            e->timer = 0x2A;
            e->substep = 1;
        }
        if (e->substep == 1) {
            e->scale = e->scale + 0x2000 > 0xFFFF ? 0xFFFF : (uint16_t)(e->scale + 0x2000);
            if (--e->timer == 0xFF) e->substep = 2;
            return;
        }
        e->scale = e->scale < 0x400 ? 0 : (uint16_t)(e->scale - 0x400);
        if (e->scale == 0) {
            e->timer = 8;
            e->step = 4;
            e->substep = 0;
        }
        return;
    case 4:
        e->firing = false;
        if (--e->timer == 0xFF) {
            e->state = 1;
            e->substep = e->step = 0;
        }
        return;
    default:
        return;
    }
}

/* sub_080CAE74 / sub_080CB0D0: the beam collides while its owner's scale
 * is above 0x400; it waits at the eye one frame, then two more once the
 * scale is full, and shoots at 12.0 (frame 0x17 after its first frame),
 * back to the eye beyond 92 pixels of the owner. Drawn with the owner's
 * vertical scale unless full, it stops hurting (+ 0x72 bit 1) once the
 * scale shrinks. The flash only follows the scale. Both are deleted with
 * + 0x1A clear or the owner hidden. The sound 0x66 loop is not ported. */
AosHitReport aos_enemy_beam_update(
    AosEnemy *b, AosSoma *soma, const AosEnemyKind *kind,
    const AosWeaponEntity *weapon, const AosWeaponFrames *weapon_frames,
    int soma_atk, int soma_def) {
    AosHitReport report = {0};
    const AosEnemy *o = b->parent;
    if (!o) return report;
    if (b->role == AOS_ROLE_BEAM) {
        if (o->scale > 0x400)
            report = collide(b, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        switch (b->state) {
        case 0:
            b->x = o->x + (b->mirrored ? 0xB0000 : -0xB0000);
            b->static_frame = 0x16;
            b->vx = b->mirrored ? 0xC0000 : -0xC0000;
            b->state = 1;
            /* fall through */
        case 1:
            if (--b->timer == 0xFF) {
                b->timer = 2;
                b->state = 2;
            }
            break;
        case 2:
            if (o->scale > 0xFFFE && --b->timer == 0xFF) {
                b->state = 3;
                b->timer = 1;
            }
            break;
        case 3:
            if (--b->timer == 0xFF && b->static_frame == 0x16) b->static_frame = 0x17;
            b->x += b->vx;
            b->vx += b->ax;
            if (abs((int16_t)((b->x >> 16) - (o->x >> 16))) > 0x5C) {
                b->x = o->x + (b->mirrored ? 0xB0000 : -0xB0000);
                b->static_frame = 0x16;
                b->vx = b->mirrored ? 0xC0000 : -0xC0000;
                b->timer = 1;
            }
            break;
        default:
            break;
        }
        if (o->scale != 0xFFFF) b->combat.attack_off = true;
        aos_combat_tick(&b->combat);
    }
    b->scale = o->scale;
    b->scaled = o->scale != 0xFFFF;
    if (!o->firing || o->hidden || o->removed) b->removed = true;
    return report;
}

/* EnemyBeamSkeletonUpdate: state 4 plays animation 0; 0xFF (killed) is
 * deleted; 3 fires without the activity window; otherwise, inside it (and
 * after a RandomNumberGenerator draw for + 0x1E, which nothing reads), 0
 * patrols, 1 closes in, 2 pauses. Then the animation step and the
 * collision pass (sub_0806E314). */
AosHitReport aos_enemy_beam_skeleton_update(
    AosEnemy *e, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    if (e->state == 4) {
        if (e->step == 0) {
            play(e, kind->anims, 0);
            e->step = 1;
        }
        step_anim(e, kind->anims);
        return report;
    }
    if (e->state == 0xFF) {
        e->hidden = true;
        e->removed = true;
        return report;
    }
    if (e->state == 3) {
        beam_attack(e, kind, &spawned);
    } else {
        int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
        if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
        (void)random();
        switch (e->state) {
        case 0: beam_patrol(e, soma, layer, kind); break;
        case 1: beam_close_in(e, soma, layer, kind, random); break;
        case 2: beam_pause(e, kind->anims); break;
        default: break;
        }
    }
    step_anim(e, kind->anims);
    int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
    if (!e->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
        report = collide(e, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    aos_combat_tick(&e->combat);
    for (int i = 0; i < spawned.child_count && report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
        report.children[report.child_count++] = spawned.children[i];
    return report;
}
