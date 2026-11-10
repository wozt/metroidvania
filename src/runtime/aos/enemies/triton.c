/* SPDX-License-Identifier: GPL-3.0-only */
/* Triton (id 0x43): EnemyTritonCreate 0x08086AD4 through sub_08087D24,
 * EnemyTritonUpdate 0x080869F4. A large water serpent: its head carries
 * a whirlpool child (sub_080871C0/sub_08087318) that launches during
 * the state-1 attack, orbits the target and docks again; homing bubbles
 * (sub_080878C8/sub_080879E0), horizontal streak jets
 * (sub_08087AD8/sub_08087B84) and splash effects (sub_08087C8C/
 * sub_08087D9C) share the tiles 0x081CBFE4, palette 0x0820A848 and
 * frames 0x0821D4E8.
 *
 *   state 0  sub_08086B78  fourteen-substep swim/aggro machine: the
 *                         0xA entry runs the facing/proximity chain
 *                         into the 10-13 turn-and-launch loop
 *   state 1  sub_08086E98  fifteen-substep attack: a minor bubble
 *                         spit or the whirlpool launch with jet volleys
 *   state 2           inline blink death (+ 0xD = 0x28 -> state 3)
 *   state 3           inline hidden fade -> removed
 *   state 4           sub_0803F17C animation only (0x8E & 0x40)
 *
 * Approximation notes (documented, not silent):
 * - tr_launch is the head's + 0x18, tr_returned mirrors the head
 *   child's + 0x18 (the parent reads it through its + 0x1C pointer
 *   natively — here the child writes the parent's field), tr_saved is
 *   + 0x19, tr_angle/tr_angle_target are the head child's + 0x20/+ 0x50,
 *   tr_jvy is the jet's + 0x14; the bubble homes with the shared
 *   + 0x48/+ 0x4C/+ 0x50/+ 0x54 velocity/accel fields.
 * - The head child is emitted on the first update
 *   (boss_parts_spawned) instead of inside create — the native stores
 *   it at + 0x1C at once and deletes the parent when the spawn fails;
 *   here a failed spawn leaves tr_returned able to complete the attack.
 * - sub_0806AE54's hit-flash countdown and the + 0x2D freeze are the
 *   shared update's; sub_0806D4C0's stun/pause gate maps to hit_flash
 *   (collide-only while set, states other than 2). The hurt pass
 *   sub_08087138 never flinches: + 0x2D = 8 flash on every hit, lethal
 *   adds sub_080683BC, song 0x6D, animation 0, + 0xD = 0x28, state 2 —
 *   wired in aos_enemy.c.
 * - sub_08069CF4 maps to swim_walk(28, -0x1B0000, 0, limit) with the
 *   0x80-result water sink of sub_0806E5FC approximated by
 *   water_below; sub_08069A00 maps to walk_collide(0x10000, 0,
 *   0x10000, 0x20); sub_0806D0B8's cell probe is cell_kind == 3
 *   (water) presetting ay 0x800/0x2000; sub_0806CEF8 is
 *   player_within and sub_0806D54C is hp < stats.hp.
 * - The head child's + 0x65 table index is kind->sprites of the
 *   parent's current animation frame; the sub_08042848/sub_0803E238
 *   affine draw calls, the + 0x5C draw byte, the palette merges, the
 *   sub_0803E654 spawn coin flip and the sub_0806D260 callback-variant
 *   pick are cosmetic or installer details and are not ported.
 * - The bubble's sub_08087C54/sub_08087C84 hurt callbacks fade it out
 *   (+ 0xD = 0xFA) — approximated by immediate removal in aos_enemy.c;
 *   its contact record sub_08087C24 becomes the shared collide. The
 *   jet and splash register no callbacks natively (cosmetic only).
 */
#include "../core/aos_enemy_internal.h"
#include "../core/aos_audio.h"
#include "../core/aos_collision.h"
#include "../core/aos_soma.h"
#include "../math/aos_math.h"

#define collide aos_enemy_collide
#define copy_result aos_enemy_copy_collision_result
#define outside_margins aos_enemy_outside_margins
#define step_anim aos_enemy_step_anim
#define play_loop aos_enemy_play_loop
#define player_in_range aos_enemy_player_in_range
#define swim_walk aos_enemy_swim_walk
#define walk_collide aos_enemy_walk_collide
#define face_player aos_enemy_face_player
#define spawn_drift aos_enemy_spawn_drift
#define target_x aos_enemy_target_x
#define target_y aos_enemy_target_y
#define snap_to_floor aos_enemy_snap_to_floor
#define cell_kind aos_enemy_cell_kind
#define water_below aos_enemy_water_below

/* sub_0806D3AC: the bare x += vx, y += vy move — no acceleration,
 * unlike aos_enemy_integrate which also applies ax/ay (the bubble's
 * ax/ay are its homing parameters, not motion). */
static void tr_move(AosEnemy *e) {
    e->x += e->vx;
    e->y += e->vy;
}

/* sub_0806D490 (vx) and sub_0806D4A8 (vy): v * (n - 1) / n through the
 * signed division — 0x10 -> *15/16, 0x20 -> *31/32. */
static void tr_friction_x(AosEnemy *e, int n) {
    e->vx = (int32_t)(((int64_t)e->vx * (n - 1)) / n);
}
static void tr_friction_y(AosEnemy *e, int n) {
    e->vy = (int32_t)(((int64_t)e->vy * (n - 1)) / n);
}
static void tr_friction(AosEnemy *e, int n) {
    tr_friction_x(e, n);
    tr_friction_y(e, n);
}

/* The sub_08069A00(e, 0x10000, 0, 0x10000, 0x20) move shared by the
 * state-0 turn steps and every state-1 step. */
static int tr_drift(AosEnemy *e, const AosCollision *layer) {
    return walk_collide(e, layer, 0x10000, 0, 0x10000, 0x20);
}

/* sub_0806D0B8(room x, room y) == 3 (water cell) presets the + 0x54
 * buoyancy acceleration: 0x800 in water, 0x2000 out of it. */
static void tr_terrain_ay(AosEnemy *e, const AosCollision *layer) {
    e->ay = cell_kind(layer, e->x >> 16, e->y >> 16) == 3 ? 0x800 : 0x2000;
}

/* The case-0/1 water sink: when the swim result lacks the 0x80 water
 * flag, sub_0806E5FC probes the water depth 0x1B px above the body and
 * adds it to the pixel y (the + 0x46 high word of + 0x44). */
static void tr_water_sink(AosEnemy *e, const AosCollision *layer, int result) {
    if (result & 0x80) return;
    int depth = water_below(layer, (int)(e->x >> 16), (int)(e->y >> 16) - 0x1B);
    e->y += (int32_t)depth << 16;
}

/* The facing-refresh + compare of cases 10/11: the stored facing bit
 * is compared against sub_0806CF2C's result — a change diverts to
 * step 0xC (the animation-5 turn). Returns true when diverted. */
static bool tr_facing_divert(AosEnemy *e, AosSoma *soma) {
    int was = e->mirrored ? 1 : 0;
    face_player(e, soma);
    return was != (e->mirrored ? 1 : 0);
}

/* 0x08118F2C: the 32 signed byte Y offsets (pixels, << 16 applied)
 * indexing the whirlpool child's attach height by the parent's + 0x65
 * sprite frame. */
static const int8_t tr_attach_y[32] = {
    -48, -47, -48, -47, -48, -47, -48, -47,
    -48, -47, -48, -47, -48, -46, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -47, -46, -45, -44, -43, -42, -40, -37,
};

/* sub_080871C0 through the first-update emission: the linked whirlpool
 * head — position copied, animation (0xA, 0, 0) stepped once, + 0x20 =
 * 0x1C000, + 0x59 |= 0x10 (scaled), contact element + 0x3C copied,
 * attack off / hurt off (no native callbacks), parent pointer set. The
 * + 0x58 sprite-variant bits, the palette merge and the sub_0803E654
 * spawn coin flip are cosmetic and not ported. */
static void tr_spawn_head(AosHitReport *report, AosEnemy *e,
                          const AosAnimSet *anims) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy *c = &report->children[report->child_count++];
    *c = *e;
    c->role = AOS_ROLE_TRITON_HEAD;
    c->parent = e;
    c->state = c->step = c->substep = 0;
    c->timer = 0;
    c->vx = c->vy = c->ax = c->ay = 0;
    c->hp = 1;
    c->hit_flash = 0;
    c->defeated = false;
    c->hidden = false;
    c->cycle_done = false;
    c->removed = false;
    c->tr_launch = c->tr_returned = c->tr_saved = 0;
    c->tr_angle = 0x1C000;
    c->tr_angle_target = 0;
    c->tr_jvy = 0;
    c->param0 = c->param1 = 0;
    c->spawned_count = 0;
    c->boss_parts_spawned = true;
    c->static_frame = -1;
    c->scaled = true;
    c->translucent = false;
    c->combat.attack_off = true;
    c->combat.hurt_off = true;
    c->contact_elements = e->contact_elements;
    play_loop(c, anims, 0xA, false);
    step_anim(c, anims);
}

/* sub_080878C8(parent, dx, dy): the homing bubble — position offset,
 * facing refreshed toward the target, + 0x48 = facing ? +0x8000 :
 * -0x8000, + 0x50 = 0x800, + 0x54 = 0x30000, animation 7 one-shot,
 * attack and hurt enabled (+ 0x72 bits 1-2 cleared), + 0x3C = the
 * parent's element * 3 / 4, parent pointer + 0x1C. */
static void tr_spawn_bubble(AosHitReport *report, AosEnemy *e,
                            const AosAnimSet *anims, AosSoma *soma,
                            int32_t dx, int32_t dy) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy *b = &report->children[report->child_count++];
    *b = *e;
    b->role = AOS_ROLE_TRITON_BUBBLE;
    b->parent = e;
    b->state = b->step = b->substep = 0;
    b->timer = 0;
    b->walk_timer = 0;
    b->x = e->x + dx;
    b->y = e->y + dy;
    face_player(b, soma);
    b->vx = b->mirrored ? 0x8000 : (int32_t)0xFFFF8000;
    b->vy = 0;
    b->ax = 0x800;
    b->ay = 0x30000;
    b->hp = 1;
    b->hit_flash = 0;
    b->defeated = false;
    b->hidden = false;
    b->cycle_done = false;
    b->removed = false;
    b->tr_launch = b->tr_returned = b->tr_saved = 0;
    b->tr_angle = 0;
    b->tr_angle_target = 0;
    b->tr_jvy = 0;
    b->param0 = b->param1 = 0;
    b->spawned_count = 0;
    b->boss_parts_spawned = true;
    b->static_frame = -1;
    b->scaled = false;
    b->translucent = true;      /* + 0x59 |= 2 */
    b->combat.attack_off = false;
    b->combat.hurt_off = false;
    b->contact_elements = (uint16_t)((e->contact_elements * 3) >> 2);
    play_loop(b, anims, 7, false);
    step_anim(b, anims);
}

/* sub_080879E0: the bubble update — animation 8 once animation 7's
 * cycle ends, homing after timer 0x31 (dead zone 0x10000, vx stepped
 * by ax clamped to ay, vy stepped by ax clamped to ay/2), integrate,
 * step animation, delete past timer 0xB3, contact on odd timers. */
static AosHitReport tr_bubble_update(AosEnemy *b, AosSoma *soma,
                                     const AosEnemyKind *kind,
                                     const AosWeaponEntity *weapon,
                                     const AosWeaponFrames *weapon_frames,
                                     int soma_atk, int soma_def) {
    AosHitReport report = {0};
    if (b->anim.id == 7 && b->cycle_done) play_loop(b, kind->anims, 8, true);
    if (b->timer > 0x31) {
        int32_t dx = target_x(soma) - b->x;
        int32_t dy = target_y(soma) - (b->y + 0x100000);
        if (dx < (int32_t)0xFFFF0000) {
            b->vx -= b->ax;
            if (b->vx < -b->ay) b->vx = -b->ay;
        } else if (dx > 0x10000) {
            b->vx += b->ax;
            if (b->vx > b->ay) b->vx = b->ay;
        }
        int32_t ay2 = (b->ay + (b->ay >> 31)) / 2;
        if (dy < (int32_t)0xFFFF0000) {
            b->vy -= b->ax;
            if (b->vy < -ay2) b->vy = -ay2;
        } else if (dy > 0x10000) {
            b->vy += b->ax;
            if (b->vy > ay2) b->vy = ay2;
        }
    }
    tr_move(b);
    step_anim(b, kind->anims);
    uint8_t old = b->timer;
    b->timer = old + 1;
    if (old > 0xB3) {
        b->removed = true;
        return report;
    }
    if (b->timer & 1) {
        AosHitReport hit = collide(b, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        copy_result(&report, &hit);
    }
    return report;
}

/* sub_08087AD8(parent, dx, dy): the streak jet — animation 6 loop,
 * + 0x58 bits 0/6 cleared, + 0x59 |= 2, + 0x14 = facing ? 0x1F0000 :
 * 0x190000 and + 0xD = facing ? 0x18 : 8 (the parent's facing at
 * spawn). No callbacks natively — cosmetic only. */
static void tr_spawn_jet(AosHitReport *report, AosEnemy *e,
                         const AosAnimSet *anims, int32_t dx, int32_t dy) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy *j = &report->children[report->child_count++];
    *j = *e;
    j->role = AOS_ROLE_TRITON_JET;
    j->parent = e;
    j->state = j->step = j->substep = 0;
    j->timer = e->mirrored ? 0x18 : 8;
    j->walk_timer = 0;
    j->x = e->x + dx;
    j->y = e->y + dy;
    j->mirrored = false;
    j->vx = j->vy = j->ax = j->ay = 0;
    j->tr_jvy = e->mirrored ? 0x1F0000 : 0x190000;
    j->hp = 1;
    j->hit_flash = 0;
    j->defeated = false;
    j->hidden = false;
    j->cycle_done = false;
    j->removed = false;
    j->tr_launch = j->tr_returned = j->tr_saved = 0;
    j->tr_angle = 0;
    j->tr_angle_target = 0;
    j->param0 = j->param1 = 0;
    j->spawned_count = 0;
    j->boss_parts_spawned = true;
    j->static_frame = -1;
    j->scaled = false;
    j->translucent = true;      /* + 0x59 |= 2 */
    j->combat.attack_off = true;
    j->combat.hurt_off = true;
    play_loop(j, anims, 6, true);
    step_anim(j, anims);
}

/* sub_08087B84: the jet update — state 0 only; + 0x14 accelerates by
 * 0x400 for timer 1-15, decelerates for 16-29 and resets at 30;
 * sub_0806D2F8(e, + 0x14, 0x10000) takes the raw-sine path
 * (vx = sine(angle + 0x4000), vy = sine(angle)) — the + 0x14 values
 * are whole turns plus a +-0x400 wobble, so the streak flies right
 * with a slight vertical undulation; blink on 0x3C - age, delete past
 * age 0x3B or outside margin 1. */
static void tr_jet_update(AosEnemy *j, const AosEnemyKind *kind) {
    if (j->state != 0) return;
    uint8_t t = j->timer + 1;
    j->timer = t;
    if (t <= 0xF) {
        j->tr_jvy += 0x400;
    } else if (t <= 0x1E) {
        j->tr_jvy -= 0x400;
        if (t == 0x1E) j->timer = 0;
    }
    j->vx = aos_sine((uint32_t)j->tr_jvy + 0x4000);
    j->vy = aos_sine((uint32_t)j->tr_jvy);
    tr_move(j);
    int age = j->walk_timer;
    j->walk_timer = age + 1;
    int bi = 0x3C - age;
    j->hidden = bi >= 0 && bi <= 0x27 && (kind->blink[bi] & 1) != 0;
    if (age > 0x3B) {
        j->removed = true;
        return;
    }
    if (outside_margins(kind, 1, (int)(j->x >> 16), (int)(j->y >> 16)))
        j->removed = true;
}

/* sub_08087C8C(src, dx, dy): the splash effect — animation 9 one-shot,
 * facing copied from the source, + 0x59 |= 2, no callbacks. */
static void tr_spawn_splash(AosHitReport *report, AosEnemy *src,
                            const AosAnimSet *anims, int32_t dx,
                            int32_t dy) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy *s = &report->children[report->child_count++];
    *s = *src;
    s->role = AOS_ROLE_TRITON_SPLASH;
    s->parent = src->parent ? src->parent : src;
    s->state = s->step = s->substep = 0;
    s->timer = 0;
    s->walk_timer = 0;
    s->x = src->x + dx;
    s->y = src->y + dy;
    s->vx = s->vy = s->ax = s->ay = 0;
    s->hp = 1;
    s->hit_flash = 0;
    s->defeated = false;
    s->hidden = false;
    s->cycle_done = false;
    s->removed = false;
    s->tr_launch = s->tr_returned = s->tr_saved = 0;
    s->tr_angle = 0;
    s->tr_angle_target = 0;
    s->tr_jvy = 0;
    s->param0 = s->param1 = 0;
    s->spawned_count = 0;
    s->boss_parts_spawned = true;
    s->static_frame = -1;
    s->scaled = false;
    s->translucent = true;      /* + 0x59 |= 2 */
    s->combat.attack_off = true;
    s->combat.hurt_off = true;
    play_loop(s, anims, 9, false);
    step_anim(s, anims);
}

/* sub_08087D9C: the splash update — step animation, delete when the
 * + 0x59 cycle bit sets. */
static void tr_splash_update(AosEnemy *s, const AosEnemyKind *kind) {
    step_anim(s, kind->anims);
    if (s->cycle_done) s->removed = true;
}

/* sub_08087318: the whirlpool head child. Cases 0-6 run only in state
 * 0; the shared tail blinks/fades it while the parent dies (state 1),
 * deletes it on the state-2 timer and reports contact in state 0. */
static void tr_head_update(AosEnemy *c, AosSoma *soma,
                           const AosEnemyKind *kind,
                           const AosWeaponEntity *weapon,
                           const AosWeaponFrames *weapon_frames,
                           int soma_atk, int soma_def, int cam_x,
                           int cam_y, AosHitReport *report) {
    AosEnemy *p = c->parent;
    if (!p || p->id != AOS_ENEMY_TRITON || p->removed) {
        c->removed = true;
        return;
    }
    if (p->state == 2 && c->state == 0) {
        c->state = 1;
        c->timer = 0x29;
        c->cycle_done = true;   /* + 0x59 |= 2 approximated */
    }
    if (c->state == 0 && c->step <= 6) {
        switch (c->step) {
        case 0:
            /* Attach: position copied, Y offset from the + 0x65 table,
             * X offset by facing and frame 0xD. */
            c->x = p->x;
            {
                unsigned idx = kind->sprites[p->anim.id][p->anim.frame];
                c->y = p->y + ((int32_t)tr_attach_y[idx] << 16);
                if (p->mirrored)
                    c->x += idx == 0xD ? 0xC0000 : 0x140000;
                else
                    c->x += idx == 0xD ? (int32_t)0xFFF50000 : (int32_t)0xFFED0000;
            }
            if (p->tr_launch) {
                c->step = 1;
                c->timer = 0;
                c->cycle_done = true;   /* + 0x59 |= 2 approximated */
            }
            break;
        case 1:
            /* Rise to y <= 0x200000, then spin-up (song 0x86). */
            if (p->hit_flash) break;
            c->y += (int32_t)0xFFFE8000;
            if (c->y <= 0x200000) {
                c->step++;
                c->timer = 0;
                aos_audio_play_song(0x86);
            }
            break;
        case 2:
            /* Spin the + 0x20 angle toward the turn window, then aim:
             * + 0x50 = ArcTan2(dx, dy) + 0x10000 (facing) / + 0x20000. */
            if (p->hit_flash) break;
            if (p->mirrored) {
                c->tr_angle = (uint32_t)((int32_t)c->tr_angle - 0x1000);
                if ((int32_t)c->tr_angle > 0xC000) break;
                aos_audio_play_song(0x86);
                c->tr_angle = (uint32_t)((int32_t)c->tr_angle + 0x10000);
            } else {
                c->tr_angle = (uint32_t)((int32_t)c->tr_angle + 0x1000);
                if ((int32_t)c->tr_angle <= 0x2BFFF) break;
                aos_audio_play_song(0x86);
                c->tr_angle = (uint32_t)((int32_t)c->tr_angle - 0x10000);
            }
            {
                uint8_t old = c->timer;
                c->timer = old + 1;
                if (old <= 2) break;
                c->step++;
                c->timer = 0;
                int32_t dx = target_x(soma) - c->x;
                int32_t dy = target_y(soma) - (c->y + 0x180000);
                uint16_t angle = aos_arctan2((int16_t)(dx >> 16),
                                             (int16_t)(dy >> 16));
                c->tr_angle_target = (int32_t)angle +
                                     (p->mirrored ? 0x10000 : 0x20000);
            }
            break;
        case 3:
            /* Turn the angle onto + 0x50 at 0x1000 per tick. */
            if (p->hit_flash) break;
            if (p->mirrored) {
                c->tr_angle = (uint32_t)((int32_t)c->tr_angle - 0x1000);
                if ((int32_t)c->tr_angle > c->tr_angle_target) break;
            } else {
                c->tr_angle = (uint32_t)((int32_t)c->tr_angle + 0x1000);
                if ((int32_t)c->tr_angle < c->tr_angle_target) break;
            }
            c->tr_angle = (uint32_t)c->tr_angle_target;
            c->step++;
            break;
        case 4:
            /* Splash at timer 0xB, launch the return flight past 0x3B. */
            if (p->hit_flash) break;
            c->timer++;
            if (c->timer == 0xB) {
                int32_t dx = aos_fixed_mul(
                    aos_sine(c->tr_angle + 0x4000), 0xB0000);
                int32_t dy = aos_fixed_mul(aos_sine(c->tr_angle), 0xB0000);
                tr_spawn_splash(report, c, kind->anims, dx, dy);
            }
            if (c->timer <= 0x3B) break;
            c->step++;
            c->vx = aos_fixed_mul(aos_sine(c->tr_angle + 0x4000), 0x40000);
            c->vy = aos_fixed_mul(aos_sine(c->tr_angle), 0x40000);
            aos_audio_play_song(0x85);
            break;
        case 5:
            /* Fly out on the case-4 velocity until outside margin 2,
             * then snap back above the parent. */
            if (p->hit_flash) break;
            tr_move(c);
            if (!outside_margins(kind, 2, (int)(c->x >> 16) - cam_x,
                                 (int)(c->y >> 16) - cam_y))
                break;
            c->step++;
            c->x = p->x + (p->mirrored ? 0x140000 : (int32_t)0xFFED0000);
            c->y = 0xF00000;
            c->tr_angle = 0x1C000;
            break;
        case 6:
            /* Dock: snap X, rise 0x18000 per tick until 0x300000 above
             * the parent, then signal the parent's tr_returned. */
            if (p->hit_flash) break;
            c->x = p->x + (p->mirrored ? 0x140000 : (int32_t)0xFFED0000);
            c->y += (int32_t)0xFFFE8000;
            if (p->y - 0x300000 < c->y) break;
            p->tr_returned = 1;
            p->tr_launch = 0;
            c->step = 0;
            c->cycle_done = true;   /* + 0x59 bits 0-1 cleared approx */
            break;
        default:
            break;
        }
    }
    /* Shared tail: state 1 blinks on the countdown and follows the
     * parent's death into state 2; state 2 deletes on the timer. */
    if (c->state == 1) {
        uint8_t old = c->timer;
        c->timer = old - 1;
        c->hidden = old <= 0x27 && (kind->blink[old] & 1) != 0;
        if (p->state == 3) {
            c->state = 2;
            c->timer = 4;
        }
    }
    if (c->state == 2) {
        c->timer--;
        if (c->timer == 0) {
            spawn_drift(0, c->x, c->y, report);
            c->removed = true;  /* + 0x59 |= 8 approximated */
        }
    }
    if (c->state == 0) {
        AosHitReport hit = collide(c, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        copy_result(report, &hit);
    }
}

/* sub_08086B78: state 0 — the swim/aggro machine. */
static void tr_state0(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                      const AosEnemyKind *kind, uint32_t (*random)(void)) {
    switch (e->step) {
    case 0:
        /* sub_08086BDC: face, + 0x19 = 0, swim (28, -0x1B, 0, 4.0),
         * water sink, then the shared proximity/damage advance. */
        face_player(e, soma);
        e->tr_saved = 0;
        tr_water_sink(e, layer,
                      swim_walk(e, layer, 28, (int32_t)0xFFE50000, 0,
                                0x40000));
        if (player_in_range(e, soma, 0x5A00000) || e->hp < e->stats.hp)
            e->step = 1;
        break;
    case 1:
        /* sub_08086C40: face, ax +-0x400, swim limit 1.0, water sink —
         * the follow state (no step advance natively). */
        face_player(e, soma);
        e->ax = e->mirrored ? 0x400 : (int32_t)0xFFFFFC00;
        tr_water_sink(e, layer,
                      swim_walk(e, layer, 28, (int32_t)0xFFE50000, 0,
                                0x10000));
        break;
    case 10:
        /* sub_08086CB4: + 0x19 = 0xA, facing divert to 0xC, else the
         * 0x6400000 proximity/damage advance to 11. */
        e->tr_saved = 0xA;
        if (tr_facing_divert(e, soma)) {
            e->step = 0xC;
            break;
        }
        if (player_in_range(e, soma, 0x6400000) || e->hp < e->stats.hp)
            e->step++;
        break;
    case 11:
        /* sub_08086CEE: facing divert to 0xC, else ax +-0x400, the
         * drift move, the 0x3B-tick aggro roll into state 1, terrain
         * ay. */
        if (tr_facing_divert(e, soma)) {
            e->step = 0xC;
            break;
        }
        e->ax = e->mirrored ? 0x400 : (int32_t)0xFFFFFC00;
        int flags = tr_drift(e, layer);
        uint8_t old = e->timer;
        e->timer = old + 1;
        if (old > 0x3B) {
            e->timer = 0;
            if (player_in_range(e, soma, 0x6400000) &&
                (random() & 0x7F) <= 0x5F && (flags & 4)) {
                e->state = 1;
                e->step = 0;
            }
        }
        tr_terrain_ay(e, layer);
        break;
    case 12:
        /* sub_08086D9E: animation 5 once, step 13, velocities cleared,
         * friction /16, drift move, terrain ay. */
        play_loop(e, kind->anims, 5, false);
        e->step++;
        e->ax = e->ay = 0;
        tr_friction(e, 0x10);
        tr_drift(e, layer);
        tr_terrain_ay(e, layer);
        break;
    case 13:
        /* sub_08086E04: friction /16, drift move, on the animation
         * cycle restart animation 1, flip the facing and return to
         * step 11; terrain ay. */
        tr_friction(e, 0x10);
        tr_drift(e, layer);
        if (e->cycle_done) {
            play_loop(e, kind->anims, 1, true);
            e->mirrored = !e->mirrored;
            e->step = 0xB;
        }
        tr_terrain_ay(e, layer);
        break;
    default:
        break;
    }
}

/* sub_08086E98: state 1 — the attack machine. r5 (the + 0x1C child)
 * reads/writes map to tr_launch/tr_returned on the parent. */
static void tr_state1(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                      const AosEnemyKind *kind, AosHitReport *report,
                      uint32_t (*random)(void)) {
    switch (e->step) {
    case 0:
        /* sub_08086EF4: drift move, then (rng & 0x7F) <= 0x1F launches
         * the whirlpool at step 0xA — otherwise step 1, the minor
         * bubble spit. */
        tr_drift(e, layer);
        if ((random() & 0x7F) <= 0x1F) {
            e->step = 0xA;
            e->ax = e->ay = 0;
        } else {
            e->step++;
        }
        break;
    case 1:
        /* sub_08086F22: drift move; on anim frame 1 spawn the bubble
         * (mirrored: -0xC0000/-0xA0000, else +0xE0000/-0xA0000) and
         * advance to the state-0 exit. */
        tr_drift(e, layer);
        if (e->anim.frame == 1) {
            int32_t dx = e->mirrored ? (int32_t)0xFFF40000 : 0xE0000;
            tr_spawn_bubble(report, e, kind->anims, soma, dx,
                            (int32_t)0xFFF60000);
            e->step = 2;
        }
        break;
    case 2:
        /* sub_08086F7C: drift move, then _08087126 — state 0 on the
         * saved step. */
        tr_drift(e, layer);
        e->state = 0;
        e->step = e->tr_saved;
        e->timer = 0;
        break;
    case 10:
        /* sub_08086F92: friction /32, drift; on anim frame 0 play
         * animation 3 once, timer 0, step 11. */
        tr_friction(e, 0x20);
        tr_drift(e, layer);
        if (e->anim.frame == 0) {
            play_loop(e, kind->anims, 3, false);
            e->timer = 0;
            e->step++;
        }
        break;
    case 11:
        /* sub_08086FD2: friction /32, drift; on the animation cycle
         * play animation 2 loop, timer 0, step 12, song 0x8A. */
        tr_friction(e, 0x20);
        tr_drift(e, layer);
        if (e->cycle_done) {
            play_loop(e, kind->anims, 2, true);
            e->timer = 0;
            e->step++;
            aos_audio_play_song(0x8A);
        }
        break;
    case 12:
        /* sub_08087024: friction /32, drift; past timer 0x1D raise
         * tr_launch, clear tr_returned, step 13. */
        tr_friction(e, 0x20);
        tr_drift(e, layer);
        {
            uint8_t old = e->timer;
            e->timer = old + 1;
            if (old > 0x1D) {
                e->tr_launch = 1;
                e->tr_returned = 0;
                e->step++;
            }
        }
        break;
    case 13:
        /* sub_08087060: friction /32, drift; every 32 ticks song 0x8A
         * and a jet (mirrored: +0xE0000/-0x3A0000, else
         * -0xC0000/-0x3A0000); while tr_returned is clear the step
         * holds — on it, tr_launch clears, animation 4 once, step 14. */
        tr_friction(e, 0x20);
        tr_drift(e, layer);
        e->timer++;
        if ((e->timer & 0x1F) == 0) {
            aos_audio_play_song(0x8A);
            int32_t dx = e->mirrored ? 0xE0000 : (int32_t)0xFFF40000;
            tr_spawn_jet(report, e, kind->anims, dx,
                         (int32_t)0xFFC60000);
        }
        if (e->tr_returned) {
            e->tr_launch = 0;
            play_loop(e, kind->anims, 4, false);
            e->step++;
        }
        break;
    case 14:
        /* sub_080870E8: friction /32, drift; on the animation cycle
         * play animation 1 loop and _08087126 — state 0 on the saved
         * step. */
        tr_friction(e, 0x20);
        tr_drift(e, layer);
        if (e->cycle_done) {
            play_loop(e, kind->anims, 1, true);
            e->state = 0;
            e->step = e->tr_saved;
            e->timer = 0;
        }
        break;
    default:
        /* cases 3-9: the drift-less return of the native table. */
        break;
    }
}

AosHitReport aos_enemy_triton_update(AosEnemy *enemy, AosSoma *soma,
                                     const AosCollision *layer,
                                     const AosEnemyKind *kind,
                                     const AosWeaponEntity *weapon,
                                     const AosWeaponFrames *weapon_frames,
                                     int soma_atk, int soma_def, int cam_x,
                                     int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    if (enemy->role == AOS_ROLE_TRITON_HEAD) {
        tr_head_update(enemy, soma, kind, weapon, weapon_frames,
                       soma_atk, soma_def, cam_x, cam_y, &report);
        return report;
    }
    if (enemy->role == AOS_ROLE_TRITON_BUBBLE)
        return tr_bubble_update(enemy, soma, kind, weapon,
                                weapon_frames, soma_atk, soma_def);
    if (enemy->role == AOS_ROLE_TRITON_JET) {
        tr_jet_update(enemy, kind);
        return report;
    }
    if (enemy->role == AOS_ROLE_TRITON_SPLASH) {
        tr_splash_update(enemy, kind);
        return report;
    }
    /* EnemyTritonUpdate head: state != 2 steps the hit flash (shared);
     * the sub_0806D4C0 stun/pause gate freezes AI outside state 2 —
     * approximated by hit_flash — reporting contact only. */
    if (enemy->hit_flash && enemy->state != 2) {
        AosHitReport hit = collide(enemy, soma, kind, weapon,
                                   weapon_frames, soma_atk, soma_def);
        copy_result(&report, &hit);
        return report;
    }
    if (!enemy->boss_parts_spawned) {
        enemy->boss_parts_spawned = true;
        tr_spawn_head(&spawned, enemy, kind->anims);
    }
    switch (enemy->state) {
    case 0:
        tr_state0(enemy, soma, layer, kind, random);
        break;
    case 1:
        tr_state1(enemy, soma, layer, kind, &spawned, random);
        break;
    case 2:
        /* inline: blink on + 0xD through sub_0806D518 (kind->blink),
         * decrement, then state 3 timer 0xA hidden. */
        enemy->hidden = enemy->timer <= 0x27
                            ? (kind->blink[enemy->timer] & 1) != 0
                            : false;
        enemy->timer--;
        if ((int8_t)enemy->timer <= 0) {
            enemy->state = 3;
            enemy->timer = 0xA;
            enemy->hidden = true;
        }
        break;
    case 3:
        /* inline: + 0x58 |= 0x20 hidden every tick, timer down to the
         * + 0x59 |= 8 removal. */
        enemy->hidden = true;
        enemy->timer--;
        if ((int8_t)enemy->timer <= 0) enemy->removed = true;
        break;
    case 4:
        /* sub_0803F17C only — dormant under the 0x8E & 0x40 flag. */
        step_anim(enemy, kind->anims);
        break;
    default:
        break;
    }
    if (enemy->state <= 3) step_anim(enemy, kind->anims);
    if (enemy->state <= 3 && !enemy->defeated && !enemy->removed) {
        int sx = (int)(enemy->x >> 16) - cam_x;
        int sy = (int)(enemy->y >> 16) - cam_y;
        if ((uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0) {
            AosHitReport hit = collide(enemy, soma, kind, weapon,
                                       weapon_frames, soma_atk,
                                       soma_def);
            copy_result(&report, &hit);
        }
    }
    for (unsigned i = 0; i < spawned.child_count &&
         report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
        report.children[report.child_count++] = spawned.children[i];
    return report;
}
