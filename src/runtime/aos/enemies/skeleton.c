/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Skeleton (enemy 0x02): its parts, bones and update.
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../../aos_enemy_internal.h"
#include "../../aos_audio.h"

#include <stdlib.h>

#define clamp_to_room aos_enemy_clamp_to_room
#define collide aos_enemy_collide
#define copy_collision_result aos_enemy_copy_collision_result
#define face_player aos_enemy_face_player
#define ground_walk aos_enemy_ground_walk
#define play_loop aos_enemy_play_loop
#define player_dy aos_enemy_player_dy
#define player_in_centred aos_enemy_player_in_centred
#define skeleton_corner aos_enemy_skeleton_corner
#define skeleton_merge aos_enemy_skeleton_merge
#define skeleton_place aos_enemy_skeleton_place
#define step_anim aos_enemy_step_anim
#define target_x aos_enemy_target_x

void aos_enemy_make_static_child(AosEnemy *child, const AosEnemy *owner,
                                 uint8_t role, int frame) {
    *child = (AosEnemy){.id = owner->id,
                        .x = owner->x,
                        .y = owner->y,
                        .role = role,
                        .static_frame = (int16_t)frame,
                        .mirrored = owner->mirrored,
                        .stats = owner->stats};
    child->combat.type = AOS_TYPE_ENEMY;
}
#define make_static_child aos_enemy_make_static_child

/* Skeleton's thrown bone, sub_080C3BD8. The native attack frame 2
 * creates a projectile with its own OAM frame, sound, 8x8 hurt box and a
 * 2x2 contact box. Its loot and experience must never be awarded: only
 * AOS_ROLE_ENEMY is eligible in aria_runtime.c. */
/* 0x0852892C: per body animation, the sub_0806C3F8 component (always the
 * last) and the offset a head or bone in hand adds to it (x mirrored). */
static const int8_t skeleton_hand[7][2] = {
    {-6, 16}, {-6, 16}, {11, -8}, {11, -8}, {11, -8}, {0, 0}, {-6, 16},
};

/* sub_0806C3F8(body, 0): the far corner of the body's current frame's last
 * OAM component, mirrored with its facing, as an integer offset. */
void aos_enemy_skeleton_corner(const AosEnemy *body, const AosEnemyKind *kind, int *dx, int *dy) {
    int anim = body->anim.id < AOS_ENEMY_MAX_ANIMS ? body->anim.id : 0;
    int frame = body->anim.frame < AOS_ENEMY_MAX_FRAMES ? body->anim.frame : 0;
    int ex = kind->edges[anim][frame][0], ey = kind->edges[anim][frame][1];
    *dx = body->mirrored ? -ex : ex;
    *dy = body->vflip ? -ey : ey;
}

/* A child at the body's position plus `dx`, `dy` (whole pixels). */
void aos_enemy_skeleton_place(AosEnemy *child, const AosEnemy *body, int dx, int dy) {
    child->x = (int32_t)((uint32_t)(uint16_t)((body->x >> 16) + dx) << 16) | (child->x & 0xFFFF);
    child->y = (int32_t)((uint32_t)(uint16_t)((body->y >> 16) + dy) << 16) | (child->y & 0xFFFF);
}

/* sub_080C46AC: a child sharing the body's + 0x3C and HP; the body is
 * hidden until sub_0806CC20 shows it again. */
static void skeleton_child(AosEnemy *body, uint8_t role, AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy c = {0};
    c.id = body->id;
    c.role = role;
    c.static_frame = -1;
    c.x = body->x;
    c.y = body->y;
    c.mirrored = body->mirrored;
    c.stats = body->stats;
    c.hp = body->hp;
    c.combat.attack_off = c.combat.hurt_off = true;
    report->children[report->child_count++] = c;
    body->hidden = true;
}

/* sub_080C4148: the head on the body's neck (state 1, at the corner of its
 * last component) or in its hand (state 2, plus the 0x0852892C offset),
 * animation 0x10, or 7 while the body laughs (+ 0x1A bit 5). It goes with
 * the body or once neither head bit is set. */
void aos_enemy_skeleton_head_update(AosEnemy *h, const AosEnemyKind *kind) {
    const AosEnemy *body = h->parent;
    if (!body || body->removed || !(body->skel_flags & 3)) {
        h->removed = true;
        return;
    }
    h->hidden = body->hidden;
    if (h->state == 0) {
        play_loop(h, kind->anims, 0x10, true);
        step_anim(h, kind->anims);
        h->state = 1;
        h->hidden = false;
    }
    int dx, dy;
    skeleton_corner(body, kind, &dx, &dy);
    h->mirrored = body->mirrored;
    if (h->state == 1) {
        skeleton_place(h, body, dx, dy);
        if (!(body->skel_flags & 1)) h->state = 2;
    } else {
        int anim = body->anim.id < 7 ? body->anim.id : 0;
        dx += body->mirrored ? -skeleton_hand[anim][0] : skeleton_hand[anim][0];
        dy += skeleton_hand[anim][1];
        skeleton_place(h, body, dx, dy);
    }
    unsigned anim = (body->skel_flags & 0x20) ? 7 : 0x10;
    if (h->anim.id != anim) play_loop(h, kind->anims, anim, true);
    step_anim(h, kind->anims);
}

/* sub_080C43C4: the bone in the body's hand (animation 8) while + 0x1A
 * bit 2 is set: shown at frame 3 of the body's animation 6 (the pick-up),
 * gone if the body hides while it shows. */
void aos_enemy_skeleton_held_update(AosEnemy *b, const AosEnemyKind *kind) {
    const AosEnemy *body = b->parent;
    if (!body || body->removed || !(body->skel_flags & 4)) {
        b->removed = true;
        return;
    }
    if (!b->hidden && body->hidden) b->hidden = true;
    if (b->state == 0) {
        play_loop(b, kind->anims, 8, true);
        b->state = 1;
        step_anim(b, kind->anims);
        return;
    }
    b->state = 2;
    if (body->anim.id == 6 && body->anim.frame == 3) b->hidden = false;
    if (!b->hidden && body->hidden) {
        b->removed = true;
        return;
    }
    int dx, dy;
    skeleton_corner(body, kind, &dx, &dy);
    int anim = body->anim.id < 7 ? body->anim.id : 0;
    b->mirrored = body->mirrored;
    dx += body->mirrored ? -skeleton_hand[anim][0] : skeleton_hand[anim][0];
    dy += skeleton_hand[anim][1];
    skeleton_place(b, body, dx, dy);
    step_anim(b, kind->anims);
}

/* sub_080C3BD8 states 0-1: the projectile 30 pixels above the body's feet,
 * a bone (frame 0x12, sound 0x88) or its head (+ 0x18, frame 0x1B, sound
 * 0x89), vx +/- 0x8AAA, vy -3.0, turning with the body's affine slot; its
 * boxes are 0x1010F8F8 (hurt) and 0x0202FFFF (attack), type 0xA, one HP. */
static void skeleton_throw(AosEnemy *body, bool head, AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy p = {0};
    p.id = body->id;
    p.role = AOS_ROLE_SKELETON_BONE;
    p.variant = head;
    p.static_frame = head ? 0x1B : 0x12;
    p.mirrored = body->mirrored;
    p.x = (int32_t)((uint32_t)(uint16_t)(body->x >> 16) << 16);
    p.y = (int32_t)((uint32_t)(uint16_t)((body->y >> 16) - 0x1E) << 16);
    p.vx = body->mirrored ? 0x8AAA : (int32_t)0xFFFF7556;
    p.vy = (int32_t)0xFFFD0000;
    p.hp = 1;
    p.state = 2;
    p.spinning = true;
    p.angle = body->skel_spin;
    p.stats = body->stats;
    p.combat.type = AOS_TYPE_ENEMY;
    p.own_boxes = true;
    p.own_hurt = (AosBox){-8, -8, 16, 16};
    p.own_attack = (AosBox){-1, -1, 2, 2};
    aos_audio_play_song(head ? 0x89 : 0x88);
    report->children[report->child_count++] = p;
    body->spawned_count++;
}

/* sub_080C3BD8 states 2-4: it falls (vy += 0x1800) turning with the body
 * and collides; struck (sub_080C3B2C, the weapon) a bone drops (vy 0.25,
 * sound 0x151) and a head flies back twelve times faster (vy negated,
 * 0x15D), both without collisions (state 3). Below screen row 0xAA it is
 * gone (state 4) and the body counts it back. */
AosHitReport aos_enemy_skeleton_bone_update(AosEnemy *p, AosSoma *soma, const AosEnemyKind *kind,
                                         const AosWeaponEntity *weapon,
                                         const AosWeaponFrames *weapon_frames, int soma_atk,
                                         int soma_def, int cam_x, int cam_y) {
    AosHitReport report = {0};
    AosEnemy *body = (AosEnemy *)p->parent;
    if (p->state == 4) {
        if (body && body->spawned_count) body->spawned_count--;
        p->removed = true;
        return report;
    }
    if (p->hit_flash) p->hit_flash--;
    if (body && p->spinning) p->angle = body->skel_spin;
    p->x += p->vx;
    p->y += p->vy;
    p->vy += 0x1800;
    if (p->state != 3) {
        report = collide(p, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        if (report.enemy_hit) {
            /* sub_080C3B2C */
            p->hp = 0;
            p->spinning = false;
            p->hit_flash = 8;
            if (!p->variant) {
                p->timer = 0x32;
                p->vx = p->ax = 0;
                p->vy = 0x4000;
                p->ay = 0x2000;
                aos_audio_play_song(0x151);
            } else {
                int32_t speed = abs(p->vx) * 12;
                p->vx = p->x < target_x(soma) ? -speed : speed;
                p->vy = -p->vy;
                aos_audio_play_song(0x15D);
            }
            p->state = 3;
            report.enemy_hit = false;
            report.killed = false;
        }
        aos_combat_tick(&p->combat);
    }
    if ((int16_t)((p->y >> 16) - cam_y) > 0xAA) p->state = 4;
    (void)cam_x;
    return report;
}

/* sub_080C3E18: the seven pieces of 0x08528952 (x, y offsets, sprite
 * frame, vx * 0x1000, vy * -0x1000 - 0x10, + 0x0D), gravity 0x3000, the
 * first (the head) only while the head is on or in hand; with the player
 * to its right they fly left. */
static const int8_t skeleton_pieces[7][5] = {
    {0, -30, 0x1B, 0x10, 0x40}, {0, -5, 0x17, 0x10, 0x31}, {0, -3, 0x19, 0x11, 0x30},
    {4, -5, 0x15, 0x0C, 0x38}, {4, -2, 0x16, 0x12, 0x33}, {-4, -5, 0x18, 0x18, 0x32},
    {-4, -2, 0x1A, 0x0C, 0x28},
};

void aos_enemy_skeleton_shatter(AosEnemy *e, const AosSoma *soma, AosHitReport *report) {
    e->hidden = true;
    bool right = e->x < target_x(soma);
    for (int i = (e->skel_flags & 3) ? 0 : 1; i < 7 && report->child_count < AOS_ENEMY_MAX_CHILDREN;
         ++i) {
        AosEnemy b = {0};
        b.id = e->id;
        b.role = AOS_ROLE_DEBRIS;
        b.static_frame = (uint8_t)skeleton_pieces[i][2];
        b.mirrored = e->mirrored;
        b.combat.attack_off = b.combat.hurt_off = true;
        int dx = skeleton_pieces[i][0];
        b.vx = (int32_t)(uint8_t)skeleton_pieces[i][3] << 12;
        b.vy = -(skeleton_pieces[i][4] << 12) - 0x10;
        b.ay = 0x3000;
        if (right) {
            b.vx = -abs(b.vx);
            dx = -abs(dx);
        }
        b.x = (int32_t)((uint32_t)((e->x >> 16) + dx) << 16) | (e->x & 0xFFFF);
        b.y = (int32_t)((uint32_t)((e->y >> 16) + skeleton_pieces[i][1]) << 16) | (e->y & 0xFFFF);
        report->children[report->child_count++] = b;
    }
}

/* sub_080C35D0: the patrol between the body's home +/- 32 (turning 40
 * pixels from home) with the 0x08528948 walker at 0.375 (mode 12), turning
 * at walls and room edges; Soma in the 512 x 128 box starts state 1. */
static void skeleton_patrol(AosEnemy *e, const AosSoma *soma, const AosCollision *layer,
                            const AosEnemyKind *kind) {
    if (e->step == 0) {
        if (e->anim.id != 1) play_loop(e, kind->anims, 1, true);
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
    if (player_in_centred(e, soma, 0x200, 0x80)) {
        e->vx = 0;
        e->state = 1;
        e->substep = e->step = 0;
    }
}

/* sub_080C36E8: far from Soma (64 high and 512 wide) it patrols again;
 * otherwise it faces him and walks at 0.5 (backing off within the random
 * box), decaying by 1/16 a frame, for 1 to 8 frames at a time, choosing
 * between the 60-frame pause (state 2), the pick-up (3) and the throw (4). */
static void skeleton_close_in(AosEnemy *e, const AosSoma *soma, const AosCollision *layer,
                              const AosEnemyKind *kind, uint32_t (*random)(void)) {
    switch (e->step) {
    case 0: {
        int dy = abs(player_dy(e, soma));
        int dx = abs((int16_t)((e->x >> 16) - (target_x(soma) >> 16)));
        if (dy > 0x40 && dx > 0x200) {
            e->state = e->substep = e->step = 0;
            return;
        }
        e->rest = (uint16_t)(random() & 0x1F);
        if ((e->skel_flags & 0xA) == 0xA) e->rest = 0x40;
        e->rest = 1;
        e->step = 1;
        e->substep = 0;
    }
        /* fall through */
    case 1: {
        face_player(e, soma);
        e->speed = 0x8000;
        if (!e->mirrored) e->vx = -e->vx;
        if (player_in_centred(e, soma, (int)(random() & 0x3F) + 0x80, 0x80)) e->speed = -e->speed;
        unsigned anim = (e->skel_flags & 8) ? 2 : 1;
        if (e->anim.id != anim) play_loop(e, kind->anims, anim, true);
        if ((e->skel_flags & 0xA) == 0xA) e->vx = 0;
        e->timer = (uint8_t)((random() & 7) + 1);
        e->step = 2;
        e->substep = 0;
    }
        /* fall through */
    case 2:
        if (e->rest) e->rest--;
        if ((e->skel_flags & 6) && (random() & 0xF) == 0) e->skel_flags |= 0x20;
        else e->skel_flags &= (uint8_t)~0x20;
        e->speed -= e->speed >> 4;
        if ((e->skel_flags & 0xA) == 0xA && e->rest == 0) {
            e->state = (random() & 3) ? 4 : 2;
            e->substep = e->step = 0;
            return;
        }
        if (e->timer) {
            e->timer--;
        } else {
            if (e->rest == 0 && player_in_centred(e, soma, 0xC4, 0x80)) {
                e->state = (e->skel_flags & 6) ? 4 : 3;
                e->substep = e->step = 0;
                e->vx = 0;
                return;
            }
            e->patience++;
            if (random() % 0x28 < e->patience) {
                int dx = abs((int16_t)((e->x >> 16) - (target_x(soma) >> 16)));
                if (dx > 0x30) {
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

/* The collision pass's report with its children (the death pieces). */
void aos_enemy_skeleton_merge(AosHitReport *report, const AosHitReport *hit) {
    copy_collision_result(report, hit);
    report->targetable |= hit->targetable;
    report->wake_spawner |= hit->wake_spawner;
    for (int i = 0; i < hit->child_count && report->child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
        report->children[report->child_count++] = hit->children[i];
    for (int i = 0; i < hit->effect_count && report->effect_count < AOS_ENEMY_MAX_EFFECTS; ++i)
        report->effects[report->effect_count++] = hit->effects[i];
}

/* EnemySkeletonUpdate: the projectile's affine slot turns 0x708 a frame
 * with the facing; state 5 (inert) loops animation 0, 0xFF waits 256
 * frames after the shattering; otherwise, within the activity window
 * (sub_0806CC20, which shows it on screen) and not flashing from a blow,
 * + 0x12 takes a random value and states 0-4 run, then the animation step
 * and the collision pass. The first update creates the head and the bone
 * in hand (EnemySkeletonCreate's children). */
AosHitReport aos_enemy_skeleton_update(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                                    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                    const AosWeaponFrames *weapon_frames, int soma_atk,
                                    int soma_def, int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0};
    e->skel_spin = (uint16_t)(e->skel_spin + (e->mirrored ? 0x708 : -0x708));
    if (e->state == 5) {
        if (e->step == 0) {
            play_loop(e, kind->anims, 0, true);
            e->hidden = false;
            e->step = 1;
        }
        step_anim(e, kind->anims);
        return report;
    }
    if (e->state == 0xFF) {
        if (--e->timer == 0xFF) e->removed = true;
        return report;
    }
    if (!e->helper_spawned) {
        e->helper_spawned = true;
        skeleton_child(e, AOS_ROLE_SKELETON_HEAD, &report);
        skeleton_child(e, AOS_ROLE_SKELETON_HELD, &report);
    }
    int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
    /* sub_0806E4F8(e, -16): the record's + 0x20 is 0. */
    e->hidden = (int16_t)sy < -16;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
    if (e->hit_flash) {
        e->hit_flash--;
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        skeleton_merge(&report, &hit);
        aos_combat_tick(&e->combat);
        return report;
    }
    (void)random();     /* + 0x12 = 40 + r % 80 */
    switch (e->state) {
    case 0: skeleton_patrol(e, soma, layer, kind); break;
    case 1: skeleton_close_in(e, soma, layer, kind, random); break;
    case 2:
        /* sub_080C390C */
        if (e->step == 0) {
            if (e->anim.id != 0) play_loop(e, kind->anims, 0, true);
            e->timer = 0x3C;
            e->step = 1;
        }
        if (--e->timer == 0) {
            e->state = 1;
            e->substep = e->step = 0;
        }
        break;
    case 3:
        /* sub_080C3958: one chance in 8 the head goes to its hand
         * (animation 5), else it picks a bone (animation 6). */
        if (e->step == 0) {
            if ((random() & 7) == 0 && (e->skel_flags & 1)) {
                play_loop(e, kind->anims, 5, false);
                e->skel_flags = (uint8_t)((e->skel_flags & ~1) | 2);
            } else {
                play_loop(e, kind->anims, 6, false);
                skeleton_child(e, AOS_ROLE_SKELETON_HELD, &report);
                e->skel_flags |= 4;
            }
            e->step = 1;
        } else if (e->anim.flags & AOS_ANIM_ENDED) {
            e->state = 1;
            e->substep = e->step = 0;
        }
        break;
    case 4:
        /* sub_080C39F4: animation 3 (+ 0x1A bit 3) or 4; at its frame 2 the
         * head in hand or the bone flies. */
        if (e->step == 0) e->step = 1;
        if (e->step == 1) {
            if (e->skel_flags & 8) {
                play_loop(e, kind->anims, 3, false);
                e->skel_flags &= (uint8_t)~8;
            } else {
                play_loop(e, kind->anims, 4, false);
            }
            e->step = 2;
        }
        if (e->step == 2 && e->anim.frame == 2) {
            bool head = e->skel_flags & 2;
            skeleton_throw(e, head, &report);
            e->skel_flags &= (uint8_t)(head ? ~2 : ~4);
            e->step = 3;
        } else if (e->step == 3 && (e->anim.flags & AOS_ANIM_ENDED)) {
            e->state = 1;
            e->substep = e->step = 0;
        }
        break;
    default:
        break;
    }
    step_anim(e, kind->anims);
    sx = (e->x >> 16) - cam_x;
    sy = (e->y >> 16) - cam_y;
    if (!e->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0) {
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        skeleton_merge(&report, &hit);
    }
    aos_combat_tick(&e->combat);
    return report;
}
