/* SPDX-License-Identifier: GPL-3.0-only */
/* Arachne (id 0x17): EnemyArachneCreate 0x080799FC through sub_0807A970,
 * EnemyArachneUpdate 0x080798F0. The ceiling crawler carries FIVE linked
 * leg entities (sub_0806DFF8 with the sub_0807A590 callback) whose frames
 * come from the 36-byte table 0x08118E5C; it rains web projectiles
 * (sub_0807A848/sub_0807A900, contact sub_0807A940) in state 1, charges
 * and hops in state 0, is knocked into state 3 by hits, and dies in
 * state 2 (sub_0807A438). State 4 is the dormant 0x8E & 0x40 form.
 *
 * Approximation notes (documented, not silent):
 * - sub_08069A00's collision move maps to projectile_collide(0x10000,
 *   0, 0x80000): same wall/ceiling/floor flag bits (1/2/8/4) and the
 *   same +/-8.0 velocity clamp; the native's extra probe margin (mode
 *   0xC) is not reproduced.
 * - + 0x18/+ 0x1C (spin/walk_timer here) are the native's speed and
 *   acceleration accumulators; they drive both the leg swing and, as
 *   an approximation, the horizontal velocity (e->vx = spin each
 *   tick), because the native consumer inside sub_08069A00's elided
 *   head could not be confirmed.
 * - sub_0806AE54/sub_0806D4C0 (sleep and stun gates) map to the shared
 *   hit_flash freeze used by the other ports.
 * - sub_08013D60/sub_08013DA8/sub_08013E18(0x82) room events and the
 *   + 0x5A palette merge are cosmetic and skipped.
 * - The web's + 0xD lifetime is approximated to 0x60 ticks (the native
 *   initial byte was not recovered); contact damage uses the shared
 *   collide path with the parent's stats.
 */
#include "../core/aos_enemy_internal.h"
#include "../core/aos_audio.h"
#include "../core/aos_collision.h"

#define collide aos_enemy_collide
#define copy_result aos_enemy_copy_collision_result
#define outside_margins aos_enemy_outside_margins
#define step_anim aos_enemy_step_anim
#define play_loop aos_enemy_play_loop
#define play aos_enemy_play
#define player_in aos_enemy_player_in
#define player_near aos_enemy_player_near
#define projectile_collide aos_enemy_projectile_collide
#define clamp_to_room aos_enemy_clamp_to_room
#define face_player aos_enemy_face_player
#define spawn_drift aos_enemy_spawn_drift
#define sine aos_sine
#define fixed_mul aos_fixed_mul

/* 0x08118E5C: the 36 leg frames (15 x8, 16 x8, 17 x16, 18 x4). */
static const uint8_t ar_leg_frames[0x24] = {
    15,15,15,15,15,15,15,15, 16,16,16,16,16,16,16,16,
    17,17,17,17,17,17,17,17, 17,17,17,17,17,17,17,17,
    18,18,18,18,
};

static int32_t ar_px(const AosEnemy *e) { return (int32_t)(e->x >> 16); }
static int32_t ar_py(const AosEnemy *e) { return (int32_t)(e->y >> 16); }

/* The speed accumulator wrap of sub_08079B38/_08079EF0: + 0x18 +=
 * + 0x1C, then folded back into [-0x240000, 0x23FFFF]. */
static void ar_speed_tick(AosEnemy *e) {
    e->spin += e->walk_timer;
    if (e->spin > 0x23FFFF) e->spin -= 0x240000;
    if (e->spin < 0) e->spin += 0x240000;
}

/* sub_08079B38 state 0: step 0 waits for Soma in the 0xA0 box (or a
 * damaged body), picks the approach mode from two probes; step 1
 * charges with a ramping speed; step 2 hops with the decaying one. */
static void ar_state0(AosEnemy *e, AosSoma *soma,
                      const AosCollision *layer,
                      uint32_t (*random)(void)) {
    int32_t px = ar_px(e), py = ar_py(e);
    switch (e->step) {
    case 0:
        face_player(e, soma);
        if (player_in(soma, px - 0x50, py - 0x50, 0xA0, 0xA0) ||
            e->hp < e->stats.hp) {
            e->step = 1;
            e->ar_mode = 1;
            e->spin = 0;
            e->walk_timer = 0;
            e->vx = 0;
            e->timer = 0;
            if (player_in(soma, px - 0x3C, py - 0x50, 0x78, 0xA0))
                e->ar_mode = 2;
            else if (player_in(soma, px - 0x64, py - 0x50, 0xC8, 0xA0)) {
                e->vy = 0x40000;
                e->ay = 0x180000;
            }
        }
        break;
    case 1:
        if (e->walk_timer <= 0xFFFF) e->walk_timer += 0x2000;
        ar_speed_tick(e);
        e->vx = e->spin;
        e->timer++;
        if (e->timer > 0x1D) {
            face_player(e, soma);
            e->timer = 0;
        }
        e->ax = e->mirrored ? 0x1000 : -0x1000;
        if ((e->mirrored && e->vx > 0x7FFF) ||
            (!e->mirrored && e->vx < -0x8000))
            e->ax = 0;
        projectile_collide(e, layer, 0x10000, 0);
        if (player_in(soma, px - 0x3C, py - 0x50, 0x78, 0xA0) ||
            clamp_to_room(e, layer, 1)) {
            e->step = 2;
            e->ar_mode = 2;
            e->timer = 0;
            e->vy = 0x10000;
            e->ay = 0x180000;
        } else if (e->ar_cool) {
            e->ar_cool--;
        } else if ((random() & 0x1F) == 0 &&
                   player_in(soma, px - 0x50, py - 0x50, 0xA0, 0xA0)) {
            e->ar_mode = 3;
            e->state = 1;
            e->step = 0;
            e->timer = 0;
        }
        break;
    case 2:
        if (e->walk_timer > -0x10000) e->walk_timer -= 0x2000;
        ar_speed_tick(e);
        e->vx = e->spin;
        face_player(e, soma);
        e->ax = e->mirrored ? -0x1000 : 0x1000;
        if ((e->mirrored && e->vx > 0x7FFF) ||
            (!e->mirrored && e->vx < -0x8000))
            e->ax = 0;
        int flags = projectile_collide(e, layer, 0x10000, 0);
        if ((flags & 3) == 0 &&
            (player_in(soma, px - 0x64, py - 0x50, 0xC8, 0xA0) ||
             clamp_to_room(e, layer, 1))) {
            e->ar_mode = 1;
            e->step = 1;
            e->timer = 0;
        } else if ((random() & 0x1F) == 0 &&
                   player_in(soma, px - 0x50, py - 0x50, 0xA0, 0xA0)) {
            e->ar_mode = 3;
            e->state = 1;
            e->step = 0xA;
            e->timer = 0;
        } else if (e->ar_cool) {
            e->ar_cool--;
        }
        break;
    default:
        break;
    }
}

/* Emit one web child (sub_0807A848's EntityCreateInRange 0x49/0xCF
 * with update sub_0807A900): launch angle + 0x14 = 0x2800 mirrored /
 * 0x5800 else, speed 0x80000 through sub_0806D2F8. */
static void ar_spawn_web(AosHitReport *report, AosEnemy *e,
                         const AosAnimSet *anims) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy *w = &report->children[report->child_count++];
    *w = *e;
    w->role = AOS_ROLE_ARACHNE_WEB;
    w->parent = e;
    w->state = w->step = w->substep = 0;
    w->timer = 0x60;
    w->angle = e->mirrored ? 0x2800 : 0x5800;
    w->vx = fixed_mul(sine(w->angle + 0x4000), 0x80000);
    w->vy = fixed_mul(sine(w->angle), 0x80000);
    w->ax = w->ay = 0;
    w->spin = w->walk_timer = 0;
    w->ar_leg = w->ar_mode = w->ar_cool = w->ar_pal = 0;
    w->ar_drop = 0;
    w->hp = w->stats.hp;
    w->param0 = w->param1 = 0;
    w->spawned_count = 0;
    w->boss_parts_spawned = true;
    w->static_frame = -1;
    w->combat.attack_off = false;
    w->combat.hurt_off = true;
    play_loop(w, anims, 0, true);
}

/* sub_08079EF0 state 1: the web rain. Steps 0/0xA bleed the speed off,
 * step 1 waits out animation 1, step 2 fires a web every fourth tick
 * for 0x3C ticks then returns to the charge with a random cooldown. */
static void ar_state1(AosEnemy *e, AosSoma *soma,
                      const AosCollision *layer,
                      const AosEnemyKind *kind,
                      uint32_t (*random)(void),
                      AosHitReport *report) {
    (void)soma;
    switch (e->step) {
    case 0:
    case 0xA:
        if (e->walk_timer < 0) e->walk_timer += 0x2000;
        else e->walk_timer -= 0x2000;
        ar_speed_tick(e);
        e->vx = e->spin;
        e->vx = (int32_t)(((int64_t)e->vx * 7) / 8);   /* sub_0806D490(8) */
        projectile_collide(e, layer, 0x10000, 0);
        e->timer++;
        if (e->timer > 7) {
            e->step = 1;
            play(e, kind->anims, 1);
        }
        break;
    case 1:
        if (e->cycle_done) {
            e->step = 2;
            play(e, kind->anims, 2);
            e->angle = e->mirrored ? 0x2800 : 0x5800;
        }
        break;
    case 2:
        e->timer++;
        if ((e->timer & 3) == 0)
            ar_spawn_web(report, e, kind->anims);
        if (e->timer > 0x3B) {
            e->state = 0;
            e->step = 1;
            e->timer = 0;
            e->ar_cool = (uint8_t)((random() & 0x3F) + 0x3C);
            e->vx = 0;
            e->ax = 0;
            e->vy = 0x40000;
            e->ay = 0x180000;
            play(e, kind->anims, 0);
        }
        break;
    default:
        break;
    }
}

/* sub_0807A2E4 state 3: the hit knockback — bleed the speed off,
 * crawl away, resume the attack (state 1 step 1) near a room edge. */
static void ar_state3(AosEnemy *e, AosSoma *soma,
                      const AosCollision *layer,
                      const AosEnemyKind *kind) {
    int32_t px = ar_px(e), py = ar_py(e);
    switch (e->step) {
    case 0:
        e->ar_mode = 2;
        e->step = 1;
        e->walk_timer = -0x20000;
        break;
    case 1:
        if (e->walk_timer < 0) e->walk_timer += 0x2000;
        else e->walk_timer -= 0x2000;
        ar_speed_tick(e);
        e->vx = e->spin;
        face_player(e, soma);
        e->ax = e->mirrored ? -0x4000 : 0x4000;
        if ((e->mirrored && e->vx <= -0x20000) ||
            (!e->mirrored && e->vx >= 0x1FFFF))
            e->ax = 0;
        if (e->cycle_done)
            play(e, kind->anims, 0);
        int flags = projectile_collide(e, layer, 0x10000, 0);
        if ((flags & 3) == 0 &&
            (player_in(soma, px - 0x64, py - 0x50, 0xC8, 0xA0) ||
             clamp_to_room(e, layer, 1))) {
            e->ar_mode = 1;
            e->state = 1;
            e->step = 1;
            e->timer = 0;
            e->vy = 0x40000;
            e->ay = 0x180000;
            play(e, kind->anims, 0);
        }
        break;
    default:
        break;
    }
}

/* sub_0807A438 state 2: the death — a drift every fourth tick, the
 * 0x08118CE0 blink on + 0x2C, a slow sink and removal at timer 0. */
static void ar_death(AosEnemy *e, const AosEnemyKind *kind,
                     AosHitReport *report) {
    if ((e->timer & 3) == 0 && report->child_count < AOS_ENEMY_MAX_CHILDREN)
        spawn_drift(0, ar_px(e) - 0x10, ar_py(e) - 0x20 - 0x180,
                    report);
    e->hidden = (kind->blink[e->timer & 0x27] & 1) != 0;
    if ((uint8_t)(e->timer - 9) <= 0x50)
        e->y += 0x20000;
    if (--e->timer == 0 || (int8_t)e->timer < 0)
        e->removed = true;
}

/* sub_0807A590: a leg copies its parent's position, facing and palette,
 * picks its frame from the 0x08118E5C table by the parent's mode and
 * speed accumulator, and collapses when the parent dies (state 2). */
static void ar_leg_update(AosEnemy *e) {
    AosEnemy *p = e->parent;
    if (!p || p->id != AOS_ENEMY_ARACHNE) {
        e->removed = true;
        return;
    }
    if (e->state != 0) {
        /* Dying leg: collapse frames while the 0x5A timer runs out. */
        if (e->step == 0) {
            e->step = 1;
            e->hit_flash |= 2;
        }
        e->static_frame = 0x15 + ((e->timer >> 4) & 3);
        if (e->ar_leg & 1) e->static_frame += 4;
        if (--e->timer == 0)
            e->removed = true;
        return;
    }
    e->x = p->x;
    e->y = p->y + e->ar_drop;
    e->mirrored = p->mirrored;
    int32_t swing = p->spin >> 16;
    int idx = swing - (e->ar_leg >> 1) * 7;
    uint8_t frame;
    if (p->ar_mode == 0) {
        e->step = 0;
        frame = 0x15;
    } else if (p->ar_mode == 1) {
        if (e->ar_mode == 2) e->step = 1;
        if (e->step == 0 && idx >= 0) e->step = 1;
        if (idx < 0) idx = 0;
        frame = ar_leg_frames[idx & 0x23];
    } else if (p->ar_mode == 2) {
        if (e->ar_mode == 1) e->step = 1;
        if (e->step == 0) {
            if (idx - 1 <= 0x15) e->step = 1;
            else idx = 0;
        }
        if (idx < 0) idx += 0x24;
        frame = ar_leg_frames[idx & 0x23];
    } else {
        if (idx < 0) idx += 0x24;
        frame = ar_leg_frames[idx & 0x23];
    }
    e->ar_mode = p->ar_mode;
    if (e->ar_leg & 1) frame += 4;
    e->static_frame = frame;
    if (p->state == 2) {
        e->state = 1;
        e->step = 0;
        e->timer = 0x5A;
    }
}

/* sub_0807A900: the web flies on its launch velocity until the floor
 * or a wall takes it (sub_0806D288 flags), then sub_0807A940's box
 * reports the contact through the shared collide path. */
static AosHitReport ar_web_update(AosEnemy *e, AosSoma *soma,
                                  const AosCollision *layer,
                                  const AosEnemyKind *kind,
                                  const AosWeaponEntity *weapon,
                                  const AosWeaponFrames *weapon_frames,
                                  int soma_atk, int soma_def) {
    AosHitReport report = {0};
    if (e->timer == 0) {
        e->removed = true;
        return report;
    }
    e->timer--;
    int flags = projectile_collide(e, layer, 0, 0);
    if (flags & 7) {
        e->removed = true;
        return report;
    }
    AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                               soma_atk, soma_def);
    copy_result(&report, &hit);
    return report;
}

/* The five legs of EnemyArachneCreate: facing alternates on the index,
 * + 0x20 the slot, + 0x48 of a leg its vertical drop, animation 0x19 on
 * odd slots and 0x15 on even ones, all on the shared resources. */
static void ar_spawn_legs(AosHitReport *report, AosEnemy *e,
                          const AosAnimSet *anims) {
    for (int i = 1; i <= 5; ++i) {
        if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
        AosEnemy *c = &report->children[report->child_count++];
        *c = *e;
        c->role = AOS_ROLE_ARACHNE_LEG;
        c->parent = e;
        c->id = AOS_ENEMY_ARACHNE;
        c->state = 0;
        c->step = c->substep = 0;
        c->timer = 0;
        c->ar_leg = (uint8_t)i;
        c->ar_mode = e->ar_mode;
        c->ar_cool = 0;
        c->ar_pal = e->ar_pal;
        c->ar_drop = ((i >> 1) * 7) << 16;
        c->mirrored = e->mirrored ^ ((i & 1) != 0);
        c->vx = c->vy = c->ax = c->ay = 0;
        c->spin = c->walk_timer = 0;
        c->angle = 0;
        c->hp = c->stats.hp;
        c->param0 = c->param1 = 0;
        c->spawned_count = 0;
        c->boss_parts_spawned = true;
        c->static_frame = (i & 1) ? 0x19 : 0x15;
        c->combat.attack_off = true;
        c->combat.hurt_off = true;
        c->removed = false;
        c->defeated = false;
        c->hit_flash = 0;
        play_loop(c, anims, 0, true);
    }
}

AosHitReport aos_enemy_arachne_update(AosEnemy *e, AosSoma *soma,
                                      const AosCollision *layer,
                                      const AosEnemyKind *kind,
                                      const AosWeaponEntity *weapon,
                                      const AosWeaponFrames *weapon_frames,
                                      int soma_atk, int soma_def, int cam_x,
                                      int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    if (e->role == AOS_ROLE_ARACHNE_LEG) {
        ar_leg_update(e);
        return report;
    }
    if (e->role == AOS_ROLE_ARACHNE_WEB)
        return ar_web_update(e, soma, layer, kind, weapon, weapon_frames,
                             soma_atk, soma_def);

    int sx = ar_px(e) - cam_x, sy = ar_py(e) - cam_y;
    if (!e->boss_parts_spawned) {
        ar_spawn_legs(&report, e, kind->anims);
        e->boss_parts_spawned = true;
    }
    if (e->state == 2) {
        e->hidden = true;
    } else {
        e->hidden = outside_margins(kind, 4, sx, sy);
        if (!e->defeated && (uint16_t)(sx + 0x10) <= 0x110 &&
            (uint16_t)sy <= 0xB0)
            report.targetable = true;
    }
    /* sub_0806AE54/sub_0806D4C0 and the + 0x2D freeze: a flashing body
     * only reports contact until the flash ends (states 2-4 exempt,
     * documented approximation of the native gate order). */
    bool run = e->state == 2 || e->state == 3 || e->state == 4 ||
               !e->hit_flash;
    if (run) {
        switch (e->state) {
        case 0:
            ar_state0(e, soma, layer, random);
            break;
        case 1:
            ar_state1(e, soma, layer, kind, random, &spawned);
            break;
        case 2:
            ar_death(e, kind, &spawned);
            break;
        case 3:
            ar_state3(e, soma, layer, kind);
            break;
        default:
            break;
        }
    }
    step_anim(e, kind->anims);
    if (!e->defeated && e->state != 2 &&
        (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0) {
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        copy_result(&report, &hit);
    }
    for (unsigned i = 0; i < spawned.child_count &&
         report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
        report.children[report.child_count++] = spawned.children[i];
    return report;
}
