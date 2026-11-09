/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Winged Skeleton (enemy 0x14) and its bones.
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../../aos_enemy_internal.h"
#include "../../aos_audio.h"

#include <stdlib.h>

#define cell_kind aos_enemy_cell_kind
#define collide aos_enemy_collide
#define generic_death aos_enemy_generic_death
#define outside_margins aos_enemy_outside_margins
#define pixel_delta aos_enemy_pixel_delta
#define play_loop aos_enemy_play_loop
#define player_dy aos_enemy_player_dy
#define projectile_collide aos_enemy_projectile_collide
#define skeleton_merge aos_enemy_skeleton_merge
#define step_anim aos_enemy_step_anim
#define target_x aos_enemy_target_x
#define target_y aos_enemy_target_y

/* The animation position + 0x6E (frame and tick) at its start. */
static bool anim_at_start(const AosEnemy *e) { return e->anim.frame == 0 && e->anim.tick == 0; }

/* sub_08074C24's wing beat: animation 2 restarts as animation 0 once in
 * three, animation 0 at its end loops animation 2 again. */
static void winged_beat(AosEnemy *e, const AosAnimSet *anims, uint32_t (*random)(void)) {
    if (e->anim.id == 2) {
        if (anim_at_start(e) && random() % 3 == 0) {
            play_loop(e, anims, 0, false);
            e->cycle_done = false;
        }
    } else if (e->anim.id == 0 && e->cycle_done) {
        play_loop(e, anims, 2, true);
    }
}

/* sub_08074C24's aim: without + 0x1C, Soma more than 16 pixels off its
 * height but on a diagonal (the horizontal distance + 8 within 31 of the
 * vertical) and ahead lets it look at the point 21 pixels ahead and 9 up:
 * not a wall (sub_0806D0B8), it sets + 0x1C. With + 0x1C, at the start of
 * an animation it attacks (state 1, `step`) while Soma stays ahead, else
 * clears + 0x1C. */
static void winged_aim(AosEnemy *e, const AosSoma *soma, const AosCollision *layer,
                       uint8_t step) {
    int dx = (int16_t)((e->x >> 16) - (target_x(soma) >> 16));
    if (!e->rest) {
        int dy = abs(player_dy(e, soma));
        if (dy <= 0x10) return;
        if (abs(abs(dx) + 8 - dy) > 0x1F) return;
        if (e->mirrored ? dx >= 0 : dx <= 0) return;
        int32_t x = (e->x >> 16) + (e->mirrored ? 0x15 : -0x15);
        if (cell_kind(layer, x, (e->y >> 16) - 9) != 2) e->rest = 1;
        return;
    }
    if (!anim_at_start(e)) return;
    if (e->mirrored ? dx < 0 : dx > 0) {
        e->state = 1;
        e->step = step;
    } else {
        e->rest = 0;
    }
}

/* sub_08074C24 (state 0): it flies level (sub_0806D3AC). Step 0 beats its
 * wings and, Soma on a steep enough line, swoops (animation 3, step 1,
 * then animation 4 looping, step 2); step 2 turns to step 3 once past his
 * x by 16; steps 0, 2 and 3 aim (attack step 10 from steps 0 and 3, 0 from
 * step 2); step 4 only flies. */
static void winged_fly(AosEnemy *e, const AosSoma *soma, const AosCollision *layer,
                       const AosAnimSet *anims, uint32_t (*random)(void)) {
    switch (e->step) {
    case 0:
        e->x += e->vx;
        e->y += e->vy;
        winged_beat(e, anims, random);
        if (anim_at_start(e)) {
            int dx = abs((int16_t)((e->x >> 16) - (target_x(soma) >> 16)));
            if (dx <= abs(player_dy(e, soma)) + 0x20) {
                e->step = 1;
                play_loop(e, anims, 3, false);
                e->cycle_done = false;
                return;
            }
        }
        winged_aim(e, soma, layer, 10);
        return;
    case 1:
        if (e->cycle_done) {
            play_loop(e, anims, 4, true);
            e->step = 2;
        }
        e->x += e->vx;
        e->y += e->vy;
        return;
    case 2:
        e->x += e->vx;
        e->y += e->vy;
        if (anim_at_start(e)) {
            int d = (int16_t)((target_x(soma) >> 16) - (e->x >> 16));
            if (e->vx > 0 ? d < -0x10 : d > 0x10) {
                e->step = 3;
                play_loop(e, anims, 2, true);
                e->cycle_done = false;
                return;
            }
        }
        winged_aim(e, soma, layer, 0);
        return;
    case 3:
        e->x += e->vx;
        e->y += e->vy;
        winged_beat(e, anims, random);
        winged_aim(e, soma, layer, 10);
        return;
    case 4:
        e->x += e->vx;
        e->y += e->vy;
        return;
    default:
        return;
    }
}

/* sub_08075230: the bone 21 pixels ahead and 9 up, at 2.0 forward and 2.0
 * down (animation 8, attack box 0x0404FC00, type 8, a knockback contact). */
static void winged_throw(const AosEnemy *e, AosHitReport *report) {
    aos_audio_play_song(0x85);
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy b = {0};
    b.id = e->id;
    b.role = AOS_ROLE_WINGED_BONE;
    b.static_frame = -1;
    b.mirrored = e->mirrored;
    b.x = (int32_t)((uint32_t)(uint16_t)((e->x >> 16) + (e->mirrored ? 0x15 : -0x15)) << 16);
    b.y = (int32_t)((uint32_t)(uint16_t)((e->y >> 16) - 9) << 16);
    b.vx = e->mirrored ? 0x20000 : (int32_t)0xFFFE0000;
    b.vy = 0x20000;
    b.hp = 1;
    b.stats = e->stats;
    b.combat.type = AOS_TYPE_ENEMY;
    b.combat.hurt_off = true;
    b.own_boxes = true;
    b.own_hurt = b.own_attack = (AosBox){0, -4, 4, 4};
    report->children[report->child_count++] = b;
}

/* sub_08075078 (state 1): it hangs still; from step 0 animation 5 (Soma
 * beyond 80 pixels, sub_0806D044) then 6, past frame 2 the bone; from
 * step 10 animation 7, past frame 2 the bone; each throw plays 0x85, and
 * at the animation's end it flies on (animation 1, state 0 step 4). */
static void winged_attack(AosEnemy *e, const AosSoma *soma, const AosAnimSet *anims,
                          AosHitReport *report) {
    switch (e->step) {
    case 0: {
        int32_t dx = pixel_delta(target_x(soma), e->x), dy = pixel_delta(target_y(soma), e->y);
        bool far = (int16_t)aos_isqrt((uint32_t)(dx * dx + dy * dy)) > 0x50;
        e->step = far ? 1 : 2;
        play_loop(e, anims, far ? 5 : 6, false);
        e->cycle_done = false;
        return;
    }
    case 1:
        if (e->cycle_done) {
            e->step = 2;
            play_loop(e, anims, 6, false);
            e->cycle_done = false;
        }
        return;
    case 2:
    case 11:
        if (e->anim.frame <= 2) return;
        e->step++;
        e->cycle_done = false;
        winged_throw(e, report);
        return;
    case 3:
    case 12:
        if (e->cycle_done) {
            play_loop(e, anims, 1, true);
            e->state = 0;
            e->step = 4;
        }
        return;
    case 10:
        play_loop(e, anims, 7, true);
        e->step = 11;
        return;
    default:
        return;
    }
}

/* sub_0807540C: seven pieces (sprite frames 0x24-0x2A) at its position,
 * vx (r & 0x3FFFF) - 2.0, vy -0.25 - (r & 0xFFFF), gravity 0x1800, for
 * 40-43 frames. */
static void winged_shatter(const AosEnemy *e, uint32_t (*random)(void), AosHitReport *report) {
    for (int i = 0; i < 7; ++i) {
        AosEnemy p = {0};
        p.id = e->id;
        p.role = AOS_ROLE_WINGED_PIECE;
        p.x = e->x;
        p.y = e->y;
        p.vx = (int32_t)(random() & 0x3FFFF) - 0x20000;
        p.vy = -(int32_t)(random() & 0xFFFF) - 0x4000;
        p.ay = 0x1800;
        p.static_frame = (int16_t)(0x24 + i);
        p.mirrored = e->mirrored;
        p.timer = (uint8_t)((random() & 3) + 0x28);
        p.combat.attack_off = p.combat.hurt_off = true;
        if (report->child_count < AOS_ENEMY_MAX_CHILDREN)
            report->children[report->child_count++] = p;
    }
}

/* sub_08075614: a piece keeps 7/8 of its vx (sub_0806D490), moves
 * (sub_0806D3D8, 8.0), blinks below 41 frames (sub_0806D518) and goes at 0. */
void aos_enemy_winged_piece_update(AosEnemy *p, const AosEnemyKind *kind) {
    p->vx = p->vx * 7 / 8;
    p->vy += p->ay;
    if (p->vy > 0x80000) p->vy = 0x80000;
    p->x += p->vx;
    p->y += p->vy;
    if (p->timer <= 0x28) p->hidden = p->timer < 40 && (kind->blink[p->timer] & 1);
    if (--p->timer == 0) p->removed = true;
}

/* sub_08075314: the bone flies (sub_08069770 with floor speed 1.0 and its
 * head 32 up), cycling its attack box through (0, -4), (12, -16) and
 * (24, -28) (4 x 4); beyond the margins of entry 1 it is gone; meeting a
 * wall, ceiling or floor it rests 60 frames without collisions, blinking
 * from 40, then goes. */
AosHitReport aos_enemy_winged_bone_update(AosEnemy *b, AosSoma *soma, const AosCollision *layer,
                                       const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                       const AosWeaponFrames *weapon_frames, int soma_atk,
                                       int soma_def, int cam_x, int cam_y) {
    AosHitReport report = {0};
    if (b->state == 0) {
        static const AosBox boxes[3] = {{0, -4, 4, 4}, {12, -16, 4, 4}, {24, -28, 4, 4}};
        b->timer++;
        b->own_attack = boxes[b->timer % 3];
        int result = projectile_collide(b, layer, 0x10000, (int32_t)0xFFFE0000);
        int sx = (b->x >> 16) - cam_x, sy = (b->y >> 16) - cam_y;
        if (outside_margins(kind, 1, sx, sy)) {
            b->removed = true;
            return report;
        }
        if (result & 7) {
            b->state = 1;
            b->timer = 0x3C;
            b->combat.attack_off = b->combat.hurt_off = true;
            return report;
        }
        report = collide(b, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        aos_combat_tick(&b->combat);
    } else {
        int8_t t = (int8_t)b->timer--;
        if (t <= 0x28) {
            b->hidden = (uint8_t)b->timer < 40 && (kind->blink[(uint8_t)b->timer] & 1);
            if ((int8_t)b->timer <= 0) {
                b->removed = true;
                return report;
            }
        }
    }
    step_anim(b, kind->anims);
    return report;
}

/* EnemyWingedSkeletonUpdate (states 0, 1, 2, 4; 3 is the room producer):
 * beyond the margins of entry 4 it vanishes (state 2); flashing from a
 * blow it only collides; state 2 shatters it at 0 HP and runs the generic
 * death with + 0x0D 1 (the producer's count drops with it). */
AosHitReport aos_enemy_winged_update(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                                  const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                  const AosWeaponFrames *weapon_frames, int soma_atk,
                                  int soma_def, int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0};
    int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
    if (e->state != 2 && e->state != 3) {
        if ((uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0) report.targetable = true;
    }
    if (e->state != 3 && outside_margins(kind, 4, sx, sy)) {
        e->state = 2;
        e->timer = 1;
    }
    if (e->hp > 0 && e->hit_flash) {
        e->hit_flash--;
        if (e->state != 2) {
            AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
            skeleton_merge(&report, &hit);
        }
        aos_combat_tick(&e->combat);
        return report;
    }
    switch (e->state) {
    case 0: winged_fly(e, soma, layer, kind->anims, random); break;
    case 1: winged_attack(e, soma, kind->anims, &report); break;
    case 2:
        e->timer = 1;
        if (e->hp <= 0 && !e->spawned_count) {
            winged_shatter(e, random, &report);
            e->spawned_count = 1;
        }
        generic_death(e, kind, random, &report);
        aos_combat_tick(&e->combat);
        return report;
    case 4:
        step_anim(e, kind->anims);
        return report;
    default:
        return report;
    }
    step_anim(e, kind->anims);
    if (!e->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0) {
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        skeleton_merge(&report, &hit);
    }
    aos_combat_tick(&e->combat);
    return report;
}
