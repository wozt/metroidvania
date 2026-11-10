/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Zombie (enemy 0x01): drips, puddles and gore.
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../../aos/core/aos_enemy_internal.h"
#include "../../aos/core/aos_audio.h"

#include <string.h>

#define flag_8e_40 aos_enemy_flag_8e_40
#define append_effects aos_enemy_append_effects
#define collide aos_enemy_collide
#define generic_death aos_enemy_generic_death
#define make_gib aos_enemy_make_gib
#define on_slope aos_enemy_on_slope
#define outside_margins aos_enemy_outside_margins
#define play aos_enemy_play
#define player_near aos_enemy_player_near
#define play_loop aos_enemy_play_loop
#define snap_to_floor aos_enemy_snap_to_floor
#define step_anim aos_enemy_step_anim
#define target_x aos_enemy_target_x
#define walk_collide aos_enemy_walk_collide

/* The removal of a zombie: state 1 with a one-frame death (HP 0). */
static void zombie_vanish(AosEnemy *zombie) {
    zombie->state = 1;
    zombie->timer = 1;
    zombie->hp = 0;
}

/* EnemyZombieCreate (the facing of sub_0806CF2C; sub_0807B404 graphics). */
void aos_enemy_zombie_create(AosEnemy *zombie, const AosSoma *soma, const AosCollision *layer,
                          const AosAnimSet *anims) {
    if (zombie->param1) {
        /* A spawner: hidden, no boxes, state 3. */
        zombie->state = 3;
        zombie->hidden = true;
        zombie->combat.attack_off = zombie->combat.hurt_off = true;
        snap_to_floor(zombie, layer);
        return;
    }
    play_loop(zombie, anims, 0, false);
    step_anim(zombie, anims);
    if (flag_8e_40) {
        /* State 2 (sub_0807B034) without the collision block, the floor
         * snap or the facing. */
        play_loop(zombie, anims, 0, false);
        zombie->state = 2;
        return;
    }
    int32_t y0 = zombie->y;
    snap_to_floor(zombie, layer);
    zombie->state = 0;
    zombie->mirrored = (zombie->x >> 16) < (target_x(soma) >> 16);
    if (on_slope(layer, zombie->x >> 16, zombie->y >> 16)) zombie_vanish(zombie);
    if (zombie->y <= y0 - 0x80000) zombie_vanish(zombie);
}

/* Constructor and update must agree on validity of ROM-local Buer probes. */

/* A child of the zombie showing one of its sprite frames, mirrored like it. */
static AosEnemy zombie_child(const AosEnemy *z, uint8_t role, int16_t frame, int32_t x,
                             int32_t y) {
    AosEnemy c = {0};
    c.id = z->id;
    c.role = role;
    c.static_frame = frame;
    c.x = x;
    c.y = y;
    c.mirrored = z->mirrored;
    c.combat.attack_off = c.combat.hurt_off = true;
    return c;
}

/* sub_0807B1CC: a drip (sub_0806DFF8(sub_0807B310)), sprite frame 0xD,
 * falling with ay 0xA00 (sub_0806D384 / sub_0806D3CC, clamped to 8.0) for
 * 31 updates (+ 0x0D from 30). */
static void zombie_drip(const AosEnemy *z, int32_t x, int32_t y, AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy d = zombie_child(z, AOS_ROLE_DRIP, 0xD, x, y);
    d.timer = 0x1E;
    d.ay = 0xA00;
    report->children[report->child_count++] = d;
}

/* sub_0807B33C: a puddle at the zombie's feet, sprite frame 0, behind it
 * (priority + 1), for 121 updates (+ 0x1C from 120). */
static void zombie_puddle(const AosEnemy *z, AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy p = zombie_child(z, AOS_ROLE_PUDDLE, 0, z->x, z->y);
    p.walk_timer = 0x78;
    report->children[report->child_count++] = p;
}

/* sub_0807B310 / sub_0807B3C8 */
void aos_enemy_drip_update(AosEnemy *d) {
    d->vy += d->ay;
    if (d->vy > 0x80000) d->vy = 0x80000;
    d->y += d->vy;
    if ((int8_t)d->timer-- <= 0) d->removed = true;
}

void aos_enemy_puddle_update(AosEnemy *p) {
    if (p->walk_timer-- <= 0) p->removed = true;
}

/* sub_0807B0DC at 0 HP: two pieces of sub_0807B258 (the gib motion of
 * sub_0806D3D8 with ay 0x1800, sprite frame 0xD, deleted below the
 * zombie's y) at x - 4 + (r & 0x7FFFF), y - 26 + (r & 0x3FFFF); each
 * created piece draws vx = (r & 0x1FFFF) - 1.0 and vy = -1.0 - (r & 0xFFFF). */
static void zombie_gore(const AosEnemy *z, uint32_t (*random)(void), AosHitReport *report) {
    for (int i = 0; i < 2; ++i) {
        int32_t x = (int32_t)(random() & 0x7FFFF) - 0x40000 + z->x;
        int32_t y = (int32_t)(random() & 0x3FFFF) - 0x1A0000 + z->y;
        if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) continue;
        int32_t vx = (int32_t)(random() & 0x1FFFF) - 0x10000;
        int32_t vy = -0x10000 - (int32_t)(random() & 0xFFFF);
        report->children[report->child_count++] = make_gib(z, x - z->x, y - z->y, vx, vy, 0xD);
    }
}

/* The drip offsets of sub_0807AD28 (walking) and sub_0807B034 (state 2):
 * walk animation 4 at (+/- 9, -24), animation 1 at (-9 / +7, -22) in the
 * walk, (-7 / +7, -22) in state 2; the first value is unmirrored. */
static void zombie_drip_at_frame(const AosEnemy *z, bool walking, AosHitReport *report) {
    int32_t dx, dy;
    if (z->anim.id == 4) {
        dx = z->mirrored ? (int32_t)0xFFF70000 : 0x90000;
        dy = (int32_t)0xFFE80000;
    } else {
        dx = z->mirrored ? 0x70000 : (walking ? (int32_t)0xFFF70000 : (int32_t)0xFFF90000);
        dy = (int32_t)0xFFEA0000;
    }
    zombie_drip(z, z->x + dx, z->y + dy, report);
}

/* sub_0807B034 (state 2, under the global 0x8E & 0x40): once the rise
 * ends, walk animation 1 or 4 at random in place, a drip at the start of
 * every cycle; no collision pass. */
static void zombie_idle(AosEnemy *z, const AosAnimSet *anims, uint32_t (*random)(void),
                        AosHitReport *report) {
    if (z->step == 0) {
        if (!z->cycle_done) return;
        z->step = 1;
        play(z, anims, (random() & 1) ? 1 : 4);
        return;
    }
    if (z->anim.frame == 0 && z->anim.tick == 0) zombie_drip_at_frame(z, false, report);
}

static void zombie_walk(AosEnemy *z, const AosSoma *soma, const AosCollision *layer,
                        const AosAnimSet *anims, uint32_t (*random)(void), AosHitReport *report) {
    switch (z->step) {
    case 0:
        if (!z->cycle_done) return;
        z->cycle_done = false;
        z->step = 1;
        play(z, anims, (random() & 1) ? 1 : 4);
        z->vx = z->mirrored ? 0x20000 : (int32_t)0xFFFE0000;
        z->vy = 0x10000;
        z->ay = 0x2800;
        z->walk_timer = (int32_t)(random() & 0x1FF) + 600;
        return;
    case 1: {
        if (z->anim.tick == 0) {
            z->vx = z->mirrored ? 0x20000 : (int32_t)0xFFFE0000;
            z->vy = 0x10000;
            z->ay = 0x2800;
        }
        z->vx = z->vx * 3 / 4;                  /* sub_0806D490(e, 4) */
        int32_t x0 = z->x, y0 = z->y;
        bool stop = walk_collide(z, layer, 0x10000, (int32_t)0xFFE00000, 0x80000, 8) & 3;
        if (!stop) stop = on_slope(layer, z->x >> 16, z->y >> 16);
        if (!stop) stop = z->walk_timer-- <= 0;
        if (stop) {
            z->step = 2;
            z->anim.flags |= 2;                 /* + 0x6C bit 1 pauses it */
            z->x = x0;
            z->y = y0;
            z->timer = 0;
            return;
        }
        /* At the start of each cycle a drip, at frames 0 and 2 a puddle
         * (once each, re-armed on frames 1 and 3). */
        if (z->anim.frame == 0 && !z->dropped) {
            z->dropped = true;
            zombie_drip_at_frame(z, true, report);
        }
        if ((z->anim.frame == 0 || z->anim.frame == 2) && !z->puddled) {
            z->puddled = true;
            zombie_puddle(z, report);
        }
        if (z->anim.frame == 1 || z->anim.frame == 3) z->dropped = z->puddled = false;
        /* Near the player (80 x 64 in front), the animation runs faster. */
        int32_t sx = z->x >> 16, sy = (z->y >> 16) - 0x20;
        bool near = z->mirrored ? player_near(soma, sx, sy, 0x50, 0x40)
                                : player_near(soma, sx - 0x50, sy, 0x50, 0x40);
        if (near && !(z->walk_timer & 1)) z->anim.tick++;
        return;
    }
    case 2:
        z->anim.flags |= 2;
        if (z->timer++ > 0x3B) {
            play_loop(z, anims, 2, false);
            z->timer = 0;
            z->step = 3;
        }
        return;
    case 3:
        if (z->cycle_done) {
            z->cycle_done = false;
            zombie_vanish(z);
        }
        return;
    default:
        return;
    }
}

/* EnemyZombieUpdate state 3: a spawner creates a zombie one frame in 32 at
 * a random screen X at least 32 pixels from the player, up to param0. */
static void zombie_spawner(AosEnemy *spawner, const AosSoma *soma, int cam_x,
                           uint32_t (*random)(void), AosHitReport *report) {
    if (random() & 0x1F) return;
    if (spawner->spawned_count >= spawner->param0) return;
    int32_t x;
    int guard = 0;
    do {
        x = (int32_t)(random() % 0xF0) + cam_x;
        int32_t dx = soma->x - (x << 16);
        if ((dx < 0 ? -dx : dx) > 0x200000) break;
    } while (++guard < 64);
    spawner->spawned_count++;
    report->spawn = true;
    report->spawn_x = x;
    report->spawn_y = spawner->y >> 16;
}

AosHitReport aos_enemy_zombie_update(AosEnemy *z, AosSoma *soma, const AosCollision *layer,
                                  const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                  const AosWeaponFrames *weapon_frames, int soma_atk,
                                  int soma_def, int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    if (z->state != 3) {
        /* sub_0806D128(e, 4): out of the screen margins, it vanishes. */
        int sx = (z->x >> 16) - cam_x, sy = (z->y >> 16) - cam_y;
        if (outside_margins(kind, 4, sx, sy))
            zombie_vanish(z);
    }
    /* sub_0806AD24: alive and flashing (+ 0x2D), sub_0806AE54 counts down
     * and only the collision pass runs (not while dying). */
    if (z->hp > 0 && z->hit_flash) {
        z->hit_flash--;
        if (z->state != 1) report = collide(z, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        aos_combat_tick(&z->combat);
        return report;
    }
    switch (z->state) {
    case 0: zombie_walk(z, soma, layer, kind->anims, random, &spawned); break;
    case 1: generic_death(z, kind, random, &spawned); break;
    case 2:
        zombie_idle(z, kind->anims, random, &spawned);
        step_anim(z, kind->anims);
        return spawned;
    case 3: zombie_spawner(z, soma, cam_x, random, &report); return report;
    default: return report;
    }
    if (z->removed) return spawned;
    step_anim(z, kind->anims);
    z->attack_off = z->state == 0 && (z->step == 0 || z->step == 3);
    int sx = (z->x >> 16) - cam_x, sy = (z->y >> 16) - cam_y;
    if (!z->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
        report = collide(z, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    if (report.killed) {
        /* sub_0807B0DC: sound 0x70, then the gore. */
        aos_audio_play_song(0x70);
        zombie_gore(z, random, &spawned);
    }
    aos_combat_tick(&z->combat);
    report.child_count = spawned.child_count;
    memcpy(report.children, spawned.children, sizeof spawned.children[0] * spawned.child_count);
    /* Keep sub_0806E218's contact art. Zombie's walking particles and
     * death flames are additional effects; an empty secondary report must
     * not erase the native splash from a surviving hit. */
    append_effects(&report, &spawned);
    return report;
}
