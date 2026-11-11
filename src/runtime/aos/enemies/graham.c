/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Graham Jones (boss 0x6F).
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../../aos/core/aos_enemy_internal.h"
#include "../../aos/core/aos_audio.h"

#include <string.h>

#define collide aos_enemy_collide
#define play_loop aos_enemy_play_loop
#define skeleton_merge aos_enemy_skeleton_merge
#define spawn_flame_at aos_enemy_spawn_flame_at
#define step_anim aos_enemy_step_anim

/* EnemyGrahamCreate 0x080D2424 / EnemyGrahamUpdate 0x080D5DF4.
 *
 * The main entity is a conductor: sub_080D4168 spawns the shadow
 * clone (tiles 0x081FB2BC, palette 0x0820C090, frames 0x08253D60,
 * hp 0x5DC) and sub_080D31D0 the weak point (tiles 0x081FD2C0,
 * palette 0x0820C158, frames 0x08254D84); both fold into the
 * AOS_ROLE_GRAHAM_CLONE/GRAHAM_CORE children. State 3
 * (sub_080D27C8) is the decoded intro: song 0x1DD once on the
 * + 0x16 & 4 gate, the sub_080D31D0 setup and the partner-death
 * wait into state 1 — the native partner-death gate into 0xF0 is
 * approximated by the lethal-hit path. State 1 (sub_080D26A8) manages the partner
 * slots of the gEwramData + 0x4E4 table — approximated by the
 * children. State 0xF0 (sub_080D2BDC) is the seven-case scripted
 * victory sequence: song 0x1DE (or 0x1DB on the + 0x13266 == 1
 * variant), the + 0xD = 0x8C timer, the sub_080D5F88/sub_080D2850
 * spawns and the SetPlayerInput locks; the port keeps the case
 * cadence, the song accents and flame-folded shots. States 2 and
 * 0xD0 are native stubs (bx lr) and stay no-ops. */
static void gr_emit_parts(AosEnemy *e, AosHitReport *hit) {
    static const struct {
        uint8_t role;
        int16_t hp;
    } parts[2] = {{AOS_ROLE_GRAHAM_CLONE, 0x5DC},
                 {AOS_ROLE_GRAHAM_CORE, 0x5DC}};
    for (unsigned i = 0; i < 2 &&
                        hit->child_count < AOS_ENEMY_MAX_CHILDREN;
         ++i) {
        if (e->spawned_count & (1u << i)) continue;
        e->spawned_count |= (uint8_t)(1u << i);
        AosEnemy c;
        memset(&c, 0, sizeof c);
        c.id = e->id;
        c.x = e->x;
        c.y = e->y;
        c.role = parts[i].role;
        c.variant = (uint8_t)i;
        c.parent = e;
        c.static_frame = (int16_t)i;
        c.hp = parts[i].hp;
        hit->children[hit->child_count++] = c;
    }
}

static void gr_part_update(AosEnemy *part, AosEnemy *body) {
    if (!body || body->removed || body->id != 0x6F) {
        part->removed = true;
        return;
    }
    /* The clone mirrors the conductor; the core hovers at its hit
     * point (the native + 0x40/+ 0x44 anchor offsets collapse to a
     * fixed offset). */
    if (part->role == AOS_ROLE_GRAHAM_CLONE) {
        part->x = body->x + (body->mirrored ? 0x80000 : -0x80000);
        part->y = body->y;
    } else {
        part->x = body->x;
        part->y = body->y - 0xA00000;
    }
    part->mirrored = body->mirrored;
}

/* State 3 sub_080D27C8: case 0 plays song 0x1DD and runs the
 * sub_080D31D0 setup once the + 0x16 & 4 gate passes; case 1 waits
 * for the partner's + 0x36 match and + 0x3E & 2 dead flag, then
 * sub_080683BC and state 0xF0. Without the partner table the port
 * waits a documented beat, then enters 0xF0. */
static void gr_state3(AosEnemy *enemy, uint32_t (*random)(void)) {
    (void)random;
    if (enemy->step == 0) {
        if (enemy->timer == 0)
            aos_audio_play_song(0x1DD);
        if (++enemy->timer > 0x40) {
            enemy->timer = 0;
            enemy->step = 1;
        }
    } else {
        /* The native case 1 waits on the partner slot's + 0x36 match
         * and dead flag before sub_080683BC; the port hands the
         * fight to state 1 once the intro beat ends. */
        enemy->state = 1;
        enemy->step = 0;
        enemy->timer = 0;
    }
}

/* State 1 sub_080D26A8: the partner-slot machine — case 0 claims a
 * partner and positions it, case 1 locks the player input for the
 * + 0xD = 0x50 countdown, cases 2/3 re-arm through sub_080D2584 and
 * the partner-state checks. The port emits the clone/core once and
 * keeps the timer cadence. */
static void gr_state1(AosEnemy *enemy, AosHitReport *report) {
    if (enemy->step == 0) {
        gr_emit_parts(enemy, report);
        enemy->timer = 0x50;
        enemy->step = 1;
    } else if (enemy->step == 1) {
        if (enemy->timer)
            enemy->timer--;
        if (!enemy->timer)
            enemy->step = 2;
    } else {
        /* Cases 2/3 idle: the clone/core children carry the fight
         * (their native callbacks sub_080D42A8/sub_080D32F8 are the
         * ~7000-line combat cluster); the conductor stays put until
         * a lethal hit enters 0xF0. */
    }
}

/* State 0xF0 sub_080D2BDC: seven cases — case 1 sets + 0x16 |= 0x20,
 * loads + 0xD = 0x8C and plays song 0x1DE (0x1DB on the variant),
 * case 2 fires the sub_080D5F88 spawn at the + 0x40/+ 0x44 anchor
 * when + 0xD hits 0x20, the later cases run the sub_080D2850/
 * sub_080D2584 scripted emits under the SetPlayerInput locks and
 * the machine ends through sub_0800C5A8(1). The port keeps the case
 * order, the song accents, two flame-folded volleys and the delete
 * at the end of the sequence. */
static AosHitReport gr_cinematic(AosEnemy *enemy, AosHitReport *report,
                                 uint32_t (*random)(void)) {
    switch (enemy->step) {
    case 0:
        enemy->timer = 0;
        enemy->step = 1;
        break;
    case 1:
        /* + 0x16 |= 0x20, + 0xD = 0x8C, song 0x1DE (0x1DB variant). */
        aos_audio_play_song(0x1DE);
        enemy->timer = 0x8C;
        enemy->step = 2;
        break;
    case 2:
        if (enemy->timer == 0x20 &&
            report->child_count < AOS_ENEMY_MAX_CHILDREN - 1) {
            /* The sub_080D5F88 anchor spawn, flame-folded. */
            spawn_flame_at(enemy->x, enemy->y - 0x80000, 0x80000,
                           0x40000, random, report);
        }
        if (enemy->timer)
            enemy->timer--;
        if (!enemy->timer) {
            enemy->timer = 0x40;
            enemy->step = 3;
        }
        break;
    case 3:
        /* Second scripted beat: song 0xEF accent family. */
        if (enemy->timer == 0)
            aos_audio_play_song(0xEF);
        if (++enemy->timer > 0x30) {
            enemy->timer = 0;
            enemy->step = 4;
        }
        break;
    case 4:
        if (enemy->timer == 0 &&
            report->child_count < AOS_ENEMY_MAX_CHILDREN) {
            spawn_flame_at(enemy->x + (enemy->mirrored ? -0x80000
                                                      : 0x80000),
                           enemy->y - 0x40000, 0x60000, 0x80000,
                           random, report);
        }
        if (++enemy->timer > 0x28) {
            enemy->timer = 0;
            enemy->step = 5;
        }
        break;
    default:
        /* Cases 5/6 tail: the sub_0800C5A8(1) release — delete on
         * the animation end like the other boss deaths. */
        if (enemy->cycle_done) {
            enemy->cycle_done = false;
            enemy->removed = true;
        }
        break;
    }
    return *report;
}

AosHitReport aos_enemy_graham_update(
    AosEnemy *enemy, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind,
    const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0};
    (void)soma;
    (void)layer;
    (void)weapon;
    (void)weapon_frames;
    (void)soma_atk;
    (void)soma_def;
    (void)cam_x;
    (void)cam_y;
    if (enemy->role == AOS_ROLE_GRAHAM_CLONE ||
        enemy->role == AOS_ROLE_GRAHAM_CORE) {
        gr_part_update(enemy, enemy->parent);
        return report;
    }
    if (!enemy->boss_parts_spawned) {
        gr_emit_parts(enemy, &report);
        enemy->boss_parts_spawned = true;
    }
    if (enemy->state == 0xF0) {
        gr_cinematic(enemy, &report, random);
        skeleton_merge(&report, &(AosHitReport){0});
        step_anim(enemy, kind->anims);
        return report;
    }
    switch (enemy->state) {
    case 1:
        gr_state1(enemy, &report);
        break;
    case 3:
        gr_state3(enemy, random);
        break;
    default:
        /* States 2 and 0xD0 are native stubs (bx lr). */
        break;
    }
    step_anim(enemy, kind->anims);
    AosHitReport hit = collide(enemy, soma, kind, weapon, weapon_frames,
                               soma_atk, soma_def);
    skeleton_merge(&report, &hit);
    /* The hurt callbacks sub_080D5E44/sub_080D5E6C/sub_080D5E88 fold
     * into the shared collide; a lethal hit enters the 0xF0 victory
     * sequence like the native sub_080683BC path. */
    if (!enemy->defeated && enemy->hp <= 0 && enemy->state != 0xF0) {
        enemy->defeated = true;
        enemy->state = 0xF0;
        enemy->step = 0;
        enemy->timer = 0;
        play_loop(enemy, kind->anims, 3, false);
    }
    aos_combat_tick(&enemy->combat);
    return report;
}
