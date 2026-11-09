/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Bone Pillar (enemy 0x08), its skulls and fireballs.
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../aos_enemy_internal.h"
#include "../aos_audio.h"

#include <string.h>

#define collide aos_enemy_collide
#define play aos_enemy_play
#define play_loop aos_enemy_play_loop
#define player_near aos_enemy_player_near
#define pillar_smoke aos_enemy_pillar_smoke
#define random_centred aos_enemy_random_centred
#define skeleton_merge aos_enemy_skeleton_merge
#define step_anim aos_enemy_step_anim
#define target_x aos_enemy_target_x

/* sub_080B3A68: a skull falls (gravity 0x1000) onto the next live skull
 * below it, or the controller, 22 pixels above it. */
static void pillar_settle(AosEnemy *s, const AosEnemy *base, const AosEnemy *below) {
    int floor_y = (below ? (below->y >> 16) : (base->y >> 16)) - 0x16;
    s->y += s->vy;
    if ((int16_t)(s->y >> 16) >= floor_y) {
        s->y = AOS_FIXED(floor_y) | (s->y & 0xFFFF);
        s->vy = 0x1000;
    }
    s->vy += 0x1000;
}

/* sub_080B3954 (state 2): sound 0x6B, three falling pieces (sprite frames
 * 0x17 to 0x15 of 0x081193DC's entry; sub_080B3EE8: 39/40 of vx, gravity
 * 0x2000, gone beyond screen margin 4), then 9 hidden frames. */
static bool pillar_shatter(AosEnemy *s, uint32_t (*random)(void), AosHitReport *report) {
    if (s->step == 0) {
        aos_audio_play_song(0x6B);
        for (int frame = 0x17; frame >= 0x15 && report->child_count < AOS_ENEMY_MAX_CHILDREN;
             --frame) {
            AosEnemy p = {0};
            p.id = s->id;
            p.role = AOS_ROLE_PILLAR_PIECE;
            p.static_frame = (int16_t)frame;
            p.x = AOS_FIXED(s->x >> 16);
            p.y = AOS_FIXED(s->y >> 16);
            p.vx = random_centred(random, 16) << 13;
            p.vy = (random_centred(random, 16) << 12) - 0x30000;
            p.ay = 0x2000;
            p.mirrored = s->mirrored;
            p.combat.attack_off = p.combat.hurt_off = true;
            report->children[report->child_count++] = p;
        }
        s->timer = 8;
        s->step = 1;
        return false;
    }
    s->hidden = true;
    if (s->timer == 0) return true;
    s->timer--;
    return false;
}

/* sub_080B33C8: the fire breath (animation 4 once) 4 pixels ahead and 10
 * up; sound 0x6E; its box (-50, -8, 50, 16) of 0x1032F8CE attacks from its
 * frame 4 (contact sub_080B3EB0: element 2, a knockback); gone when the
 * animation ends or its skull dies. */
static AosEnemy pillar_breath(const AosEnemy *s) {
    AosEnemy b = {0};
    b.id = s->id;
    b.role = AOS_ROLE_PILLAR_BREATH;
    b.static_frame = -1;
    b.x = AOS_FIXED((s->x >> 16) + (s->mirrored ? 4 : -4));
    b.y = AOS_FIXED((s->y >> 16) - 10);
    b.mirrored = s->mirrored;
    b.translucent = true;
    b.stats = s->stats;
    b.hp = 1;
    b.combat.type = AOS_TYPE_ENEMY;
    b.combat.attack_off = b.combat.hurt_off = true;
    b.own_boxes = true;
    b.own_hurt = b.own_attack = (AosBox){-50, -8, 50, 16};
    b.contact_elements = 2;
    return b;
}

/* sub_080B350C: the fireball (animation 5 looping) 8 pixels behind and 12
 * up, at 2.0 ahead; sound 0x7A; one hit point, type 0xA, both boxes
 * (-4, -4, 8, 8), contact element 2 with a knockback; struck it bursts into
 * a flame; every eighth frame a smoke puff; gone beyond screen margin 1. */
static AosEnemy pillar_fireball(const AosEnemy *s) {
    AosEnemy b = {0};
    b.id = s->id;
    b.role = AOS_ROLE_PILLAR_FIREBALL;
    b.static_frame = -1;
    b.mirrored = s->mirrored;
    b.x = AOS_FIXED((s->x >> 16) + (s->mirrored ? 8 : -8));
    b.y = AOS_FIXED((s->y >> 16) - 12);
    b.vx = s->mirrored ? 0x20000 : -0x20000;
    b.stats = s->stats;
    b.hp = 1;
    b.combat.type = 0x0A;
    b.own_boxes = true;
    b.own_hurt = b.own_attack = (AosBox){-4, -4, 8, 8};
    b.contact_elements = 2;
    return b;
}

/* sub_080B32F4: a smoke puff (sprite frame 0x14), translucent, both flips
 * at random, 16-31 frames of the accelerated move (sub_0806D3D8, 8.0). */
void aos_enemy_pillar_smoke(const AosEnemy *from, int32_t x, int32_t y, int32_t vx, int32_t vy,
                         int32_t ax, int32_t ay, uint32_t (*random)(void),
                         AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy p = {0};
    p.id = from->id;
    p.role = AOS_ROLE_PILLAR_SMOKE;
    p.static_frame = 0x14;
    p.x = x;
    p.y = y;
    p.vx = vx;
    p.vy = vy;
    p.ax = ax;
    p.ay = ay;
    p.translucent = true;
    p.mirrored = random() & 1;
    p.vflip = random() & 1;
    p.timer = (uint8_t)((random() & 0xF) + 0x10);
    p.combat.attack_off = p.combat.hurt_off = true;
    report->children[report->child_count++] = p;
}

/* sub_080B36A0 (state 1): it opens its jaw (animation 1); then (animation
 * 3) an even skull breathes fire at frame 2 (three smoke puffs 24, 36 and
 * 48 pixels ahead at its tick 0x28), an odd one spits a fireball; 32
 * frames later, facing away from its target, it turns (animation 2) and
 * rests, else it rests 128 frames (animation 0). */
static void pillar_attack(AosEnemy *s, const AosSoma *soma, const AosAnimSet *anims,
                          uint32_t (*random)(void), AosHitReport *report) {
    switch (s->step) {
    case 0:
        play_loop(s, anims, 1, false);
        s->step = 1;
        break;
    case 1:
        if (!(s->anim.flags & AOS_ANIM_ENDED)) break;
        play_loop(s, anims, 3, false);
        s->step = (s->variant & 1) ? 3 : 2;
        break;
    case 2:
    case 3:
        if (s->anim.frame == 2 && s->anim.tick == 0 &&
            report->child_count < AOS_ENEMY_MAX_CHILDREN) {
            report->children[report->child_count++] =
                s->step == 2 ? pillar_breath(s) : pillar_fireball(s);
            aos_audio_play_song(s->step == 2 ? 0x6E : 0x7A);
        }
        if (s->step == 2) {
            if (s->anim.frame == 2 && s->anim.tick == 0x28) {
                int32_t d = s->mirrored ? 12 : -12;
                for (int k = 0, o = 2 * d; k < 3; ++k, o += d) {
                    int32_t x = AOS_FIXED((s->x >> 16) + o + random_centred(random, 16));
                    int32_t y = AOS_FIXED(random_centred(random, 16) - 0x10 + (s->y >> 16));
                    int32_t vy = random_centred(random, 0x4000) - 0x1000;
                    pillar_smoke(s, x, y, 0, vy, 0, -0x800, random, report);
                }
            }
            s->timer++;
        }
        if (s->anim.flags & AOS_ANIM_ENDED) {
            s->timer = 0x20;
            s->step = 4;
        }
        break;
    case 4: {
        if (s->timer) {
            s->timer--;
            break;
        }
        bool right = s->x <= target_x(soma);
        if (s->mirrored != right) {
            play_loop(s, anims, 2, false);
            s->step = 5;
        } else {
            play(s, anims, 0);
            s->step = 6;
            s->timer = 0x80;
        }
        break;
    }
    case 5:
        if (!(s->anim.flags & AOS_ANIM_ENDED)) break;
        s->mirrored = !s->mirrored;
        s->state = s->step = s->substep = 0;
        play(s, anims, 0);
        break;
    case 6:
        if (s->timer) s->timer--;
        else s->state = s->step = s->substep = 0;
        break;
    default: break;
    }
}

/* sub_080B3AEC: a skull does nothing while its controller is outside its
 * window; it settles, only collides while flashing from a blow, waits
 * (state 0) for its target inside the 160 x 120 box around it, attacks
 * (1), shatters (2) or only animates (3, the global 0x8E & 0x40); then the
 * collision pass (not while shattering) and the animation step. */
AosHitReport aos_enemy_pillar_skull_update(AosEnemy *s, AosSoma *soma, const AosEnemyKind *kind,
                                        const AosWeaponEntity *weapon,
                                        const AosWeaponFrames *weapon_frames, int soma_atk,
                                        int soma_def, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    AosEnemy *base = (AosEnemy *)s->parent;
    if (!base || base->removed) {
        s->removed = true;
        return report;
    }
    if (s->state != 2) {
        /* The controller's slot of this skull (+ 0x18 + i). */
        if (s->variant < 8) base->bp_skull[s->variant] = s;
        if (!base->bp_active) return report;
        const AosEnemy *below = NULL;
        for (int v = s->variant - 1; v >= 0 && !below; --v)
            if (base->bp_skull[v] && base->bp_skull[v]->state != 2) below = base->bp_skull[v];
        pillar_settle(s, base, below);
        if (base->bp_top == s->variant) report.targetable = true;
    } else if (s->variant < 8) {
        base->bp_skull[s->variant] = NULL;
    }
    if (s->hp > 0 && s->hit_flash) {
        s->hit_flash--;
        if (s->state != 2) {
            AosHitReport hit = collide(s, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
            skeleton_merge(&report, &hit);
        }
        aos_combat_tick(&s->combat);
        return report;
    }
    switch (s->state) {
    case 0:
        if (s->step == 0 &&
            player_near(soma, (s->x >> 16) - 0x50, (s->y >> 16) - 0x3C, 0xA0, 0x78)) {
            s->state = 1;
            s->step = s->substep = 0;
        }
        break;
    case 1: pillar_attack(s, soma, kind->anims, random, &spawned); break;
    case 2:
        if (pillar_shatter(s, random, &spawned)) s->removed = true;
        report.child_count = spawned.child_count;
        memcpy(report.children, spawned.children, sizeof spawned.children[0] * spawned.child_count);
        return report;
    case 3:
        step_anim(s, kind->anims);
        return report;
    default: break;
    }
    AosHitReport hit = collide(s, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    bool targetable = report.targetable;
    skeleton_merge(&report, &hit);
    report.targetable = targetable;
    aos_combat_tick(&s->combat);
    step_anim(s, kind->anims);
    for (int c = 0; c < spawned.child_count && report.child_count < AOS_ENEMY_MAX_CHILDREN; ++c)
        report.children[report.child_count++] = spawned.children[c];
    return report;
}

/* sub_080B3DCC: the controller marks itself active (+ 0x15) inside the
 * screen box (-32, -32, 0x130, 32 * + 0x30 + 0xE0); it forgets its
 * shattered skulls, puts the topmost live one in the homing list
 * (sub_08021278), and is deleted with none left. Its skulls (EnemyBone
 * PillarCreate's) are made by its first update. */
AosHitReport aos_enemy_pillar_update(AosEnemy *b, const AosEnemyKind *kind, int cam_x, int cam_y) {
    AosHitReport report = {0};
    if (!b->helper_spawned) {
        b->helper_spawned = true;
        int32_t y = (b->y >> 16) - 0x16;    /* the bottom skull's place */
        for (int i = 1; i <= b->param0 && report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i) {
            AosEnemy s = {0};
            s.id = b->id;
            s.role = AOS_ROLE_BONE_PILLAR_SEGMENT;
            s.static_frame = -1;
            s.variant = (uint8_t)(i - 1);
            s.x = b->x;
            s.y = AOS_FIXED(y - (i - 1) * 0x16);
            s.mirrored = (i & 1) ? b->mirrored : !b->mirrored;
            s.stats = b->stats;
            s.hp = b->stats.hp;
            s.combat.type = AOS_TYPE_ENEMY;
            s.own_boxes = true;
            s.own_hurt = s.own_attack = (AosBox){-14, -20, 28, 20};
            if (b->state == 3) {
                s.state = 3;
                s.combat.attack_off = s.combat.hurt_off = true;
            }
            play(&s, kind->anims, 0);
            step_anim(&s, kind->anims);
            report.children[report.child_count++] = s;
        }
        b->spawned_count = report.child_count;
        return report;
    }
    int sx = (b->x >> 16) - cam_x, sy = (b->y >> 16) - cam_y;
    b->bp_active = (uint16_t)(sx + 0x20) <= 0x130 &&
                   (uint16_t)(sy + 0x20) <= (uint16_t)(b->param0 * 0x20 + 0xE0);
    if (!b->bp_active) return report;
    b->bp_top = -1;
    for (int v = 7; v >= 0 && b->bp_top < 0; --v)
        if (b->bp_skull[v] && b->bp_skull[v]->state != 2) b->bp_top = (int8_t)v;
    if (b->bp_top < 0 && b->bp_seen) b->removed = true;
    if (b->bp_top >= 0) b->bp_seen = true;
    return report;
}
