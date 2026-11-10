/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Zombie Soldier (enemy 0x0C) and its grenade.
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../../aos/core/aos_enemy_internal.h"
#include "../../aos/core/aos_audio.h"

#include <string.h>

#define collide aos_enemy_collide
#define generic_death aos_enemy_generic_death
#define grenade_particles aos_enemy_grenade_particles
#define make_gib aos_enemy_make_gib
#define outside_margins aos_enemy_outside_margins
#define play aos_enemy_play
#define player_near aos_enemy_player_near
#define play_loop aos_enemy_play_loop
#define projectile_collide aos_enemy_projectile_collide
#define step_anim aos_enemy_step_anim
#define take_effects aos_enemy_take_effects
#define target_x aos_enemy_target_x
#define walk_collide aos_enemy_walk_collide

/* sub_08092BC0: a grenade at the soldier's position plus (dx, dy), thrown
 * at (vx, vy) with gravity 0x1800; it shows sprite frame 18, carries the
 * soldier's contact power and has a 4 x 4 box for both roles. */
static AosEnemy make_grenade(const AosEnemy *soldier, int32_t dx, int32_t dy, int32_t vx,
                             int32_t vy) {
    AosEnemy g = {0};
    g.id = soldier->id;
    g.role = AOS_ROLE_GRENADE;
    g.static_frame = 18;
    g.x = soldier->x + dx;
    g.y = soldier->y + dy;
    g.vx = vx;
    g.vy = vy;
    g.ay = 0x1800;
    g.mirrored = soldier->mirrored;
    g.stats = soldier->stats;
    g.hp = 1;
    g.combat.type = AOS_TYPE_ENEMY;
    g.own_boxes = true;
    g.own_hurt = g.own_attack = (AosBox){-2, -2, 4, 4};
    return g;
}

/* sub_08092CCC: flight (a floor hit bounces at a third of the speed, sound
 * 0x11B; a second floor hit or a ceiling makes it explode, sound 0x76),
 * the harmless burst of state 1 (7 frames) and the explosion of state 2,
 * whose attack box grows to 2 + t / 2 pixels around it until t = 14, hits
 * until t = 18 and ends at t = 24. The burst and the explosion's first 8
 * frames create the particles of grenade_particles. */
AosHitReport aos_enemy_grenade_update(AosEnemy *g, AosSoma *soma, const AosCollision *layer,
                                   const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                   const AosWeaponFrames *weapon_frames, int soma_atk,
                                   int soma_def, int cam_x, int cam_y,
                                   uint32_t (*random)(void)) {
    AosHitReport report = {0}, particles = {0};
    switch (g->state) {
    case 0: {
        int flags = projectile_collide(g, layer, g->vy / 3, (int32_t)0xFFFC0000);
        bool explode = false;
        if (flags & 4) {
            g->vy = -g->vy;
            explode = ++g->timer > 1;
            if (!explode) aos_audio_play_song(0x11B);
        }
        if (explode || (flags & 8)) {
            g->state = 2;
            aos_audio_play_song(0x76);
            break;
        }
        if (outside_margins(kind, 2, (g->x >> 16) - cam_x, (g->y >> 16) - cam_y)) {
            g->removed = true;
            return report;
        }
        report = collide(g, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        break;
    }
    case 1:
        g->hidden = true;
        ++g->timer;
        grenade_particles(g, random, &report);
        if (g->timer > 6) g->removed = true;
        break;
    case 2:
        g->hidden = true;
        ++g->timer;
        if (g->timer <= 8) grenade_particles(g, random, &particles);
        if (g->timer <= 14) {
            int r = 2 + g->timer / 2;
            g->own_attack = (AosBox){(int8_t)-r, (int8_t)-r, (uint8_t)(2 * r), (uint8_t)(2 * r)};
            g->combat.hurt_off = true;
        }
        if (g->timer > 0x17) {
            g->removed = true;
            return report;
        }
        if (g->timer <= 0x12) report = collide(g, soma, kind, weapon, weapon_frames, soma_atk,
                                               soma_def);
        else g->combat.attack_off = true;
        take_effects(&report, &particles);
        break;
    default:
        break;
    }
    aos_combat_tick(&g->combat);
    return report;
}

/* sub_0806CF2C: facing the player, mirrored when he is to the right. */
void aos_enemy_soldier_face(AosEnemy *z, const AosSoma *soma) {
    z->mirrored = (z->x >> 16) < (target_x(soma) >> 16);
}
#define soldier_face aos_enemy_soldier_face

/* sub_08092A28 steps 1 / 0xB: back to walking. */
static void soldier_walk_again(AosEnemy *z, const AosAnimSet *anims) {
    play(z, anims, 0);
    z->state = z->step = 0;
    z->vx = 0;
    z->vy = 0x10000;
    z->ay = 0x2800;
}

/* sub_080928FC: at the start of walk frames 1 and 5 the soldier faces the
 * player, then attacks him at close range (an 80 x 70 box ahead, one
 * chance in two: animation 1), or throws a grenade (one chance in four,
 * a 220 x 70 box: animation 2), or steps at 0.25 toward its facing; frames
 * 0 and 4 stop it. It moves with the walker collision of the zombie. */
static void soldier_walk(AosEnemy *z, const AosSoma *soma, const AosCollision *layer,
                         const AosAnimSet *anims, uint32_t (*random)(void)) {
    if (z->step != 0) return;
    uint8_t frame = z->anim.frame;
    bool frame_start = z->anim.tick == 0;
    if ((frame == 1 || frame == 5) && frame_start) {
        soldier_face(z, soma);
        int32_t x = z->x >> 16, y = z->y >> 16;
        if (player_near(soma, (int16_t)(x - 0x28), (int16_t)(y - 0x40), 0x50, 0x46) &&
            (random() & 0x7F) <= 0x3F) {
            z->state = 1;
            z->step = 0;
            play_loop(z, anims, 1, false);
            return;
        }
        if ((random() & 0x7F) <= 0x1F &&
            player_near(soma, (int16_t)(x - 0x6E), (int16_t)(y - 0x40), 0xDC, 0x46)) {
            z->state = 1;
            z->step = 0xA;
            play_loop(z, anims, 2, false);
            return;
        }
        z->vx = z->mirrored ? 0x4000 : -0x4000;
        z->vy = 0x10000;
        z->ay = 0x2800;
    }
    if ((frame == 0 || frame == 4) && frame_start) z->vx = 0;
    walk_collide(z, layer, 0x10000, (int32_t)0xFFE00000, 0x80000, 8);
}

/* sub_08092A28: the close attack (sound 0x85 at frame 2) and the throw: at
 * the start of frame 10 a grenade leaves 16 pixels ahead and 37 up, at
 * vy -2.0 and vx = (player x - its x) / 44 frames, at most 1.25. */
static void soldier_attack(AosEnemy *z, const AosSoma *soma, const AosAnimSet *anims,
                           AosHitReport *report) {
    switch (z->step) {
    case 0:
        z->step = 1;
        break;
    case 1:
        if (z->anim.frame == 2 && z->anim.tick == 0) aos_audio_play_song(0x85);
        if (z->cycle_done) soldier_walk_again(z, anims);
        break;
    case 0xA:
        soldier_face(z, soma);
        if (z->anim.frame == 10 && z->anim.tick == 0) {
            int32_t offset = z->mirrored ? 0x100000 : (int32_t)0xFFF00000;
            int32_t vx = (target_x(soma) - (z->x + offset)) / 0x2C;
            if (z->mirrored ? vx > 0x13FFF : vx <= (int32_t)0xFFFEC000)
                vx = z->mirrored ? 0x14000 : (int32_t)0xFFFEC000;
            report->children[report->child_count++] =
                make_grenade(z, offset, (int32_t)0xFFDB0000, vx, (int32_t)0xFFFE0000);
            z->step = 0xB;
        }
        break;
    case 0xB:
        if (z->cycle_done) soldier_walk_again(z, anims);
        break;
    default:
        break;
    }
}

/* The X speed of a gib: 0.5 to 1.0, backward when mirrored. */
int32_t aos_enemy_gib_speed(const AosEnemy *z, uint32_t (*random)(void)) {
    int32_t r = (int32_t)(random() & 0x7FFF);
    return z->mirrored ? -0x8000 - r : r + 0x8000;
}
#define gib_speed aos_enemy_gib_speed

/* EnemyZombieSoldierUpdate state 2 at timer 0x28: two gibs, frames 0x13
 * (dx -8..8, dy -1..-17, vy -1.5..-3) and 0x14 (dy 0..-24), the random
 * draws in the order dx, dy, vx, vy. */
static void soldier_gibs(const AosEnemy *z, uint32_t (*random)(void), AosHitReport *report) {
    for (int i = 0; i < 2; ++i) {
        int32_t dx = (int32_t)(random() & 0xFFFFF) - 0x80000;
        int32_t dy = i == 0 ? -0x10000 - (int32_t)(random() & 0xFFFFF)
                            : -(int32_t)(random() % 0x180000u);
        int32_t vx = gib_speed(z, random);
        int32_t vy = -(int32_t)(random() % 0x18000u) - 0x18000;
        if (report->child_count < AOS_ENEMY_MAX_CHILDREN)
            report->children[report->child_count++] =
                make_gib(z, dx, dy, vx, vy, (int16_t)(i == 0 ? 0x13 : 0x14));
    }
}

/* sub_08093078: sub_0806D3D8(e, 8.0) (velocity, clamped, then position),
 * deleted below the soldier's y. */
void aos_enemy_gib_update(AosEnemy *g) {
    g->vx += g->ax;
    if (g->ax >= 0 ? g->vx > 0x80000 : g->vx < -0x80000) g->vx = g->ax >= 0 ? 0x80000 : -0x80000;
    g->vy += g->ay;
    if (g->ay >= 0 ? g->vy > 0x80000 : g->vy < -0x80000) g->vy = g->ay >= 0 ? 0x80000 : -0x80000;
    g->x += g->vx;
    g->y += g->vy;
    if (g->y > g->walk_timer) g->removed = true;
}

/* EnemyZombieSoldierUpdate: within the activity window of sub_0806CC20,
 * state 0 walks, 1 attacks, 2 dies (gibs on its first frame, then the
 * generic death), then the animation step and the on-screen collision
 * pass; state 3 only steps its animation. */
AosHitReport aos_enemy_soldier_update(AosEnemy *z, AosSoma *soma, const AosCollision *layer,
                                   const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                   const AosWeaponFrames *weapon_frames, int soma_atk,
                                   int soma_def, int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    int sx = (z->x >> 16) - cam_x, sy = (z->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
    switch (z->state) {
    case 0: soldier_walk(z, soma, layer, kind->anims, random); break;
    case 1: soldier_attack(z, soma, kind->anims, &spawned); break;
    case 2:
        if (z->timer == 0x28) soldier_gibs(z, random, &spawned);
        generic_death(z, kind, random, &spawned);
        break;
    case 3:
        step_anim(z, kind->anims);
        return report;
    default: break;
    }
    if (!z->removed) {
        step_anim(z, kind->anims);
        sx = (z->x >> 16) - cam_x;
        sy = (z->y >> 16) - cam_y;
        if (!z->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
            report = collide(z, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        aos_combat_tick(&z->combat);
    }
    report.child_count = spawned.child_count;
    memcpy(report.children, spawned.children, sizeof spawned.children[0] * spawned.child_count);
    take_effects(&report, &spawned);
    return report;
}
