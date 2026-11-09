/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Merman (enemy 0x03), its producer and shot.
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../../aos_enemy_internal.h"
#include "../../aos_audio.h"

#define collide aos_enemy_collide
#define face_player aos_enemy_face_player
#define generic_death aos_enemy_generic_death
#define merge_enemy_hit aos_enemy_merge_hit
#define play aos_enemy_play
#define play_loop aos_enemy_play_loop
#define step_anim aos_enemy_step_anim
#define stop aos_enemy_stop
#define toward_facing aos_enemy_toward_facing
#define walk_collide aos_enemy_walk_collide

/* sub_08071648 / sub_08071764: the projectile emitted after Merman's
 * animation-3 wind-up. */
static void merman_shot(const AosEnemy *owner, const AosEnemyKind *kind,
                        AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy shot = {0};
    shot.id = owner->id;
    shot.role = AOS_ROLE_MERMAN_SHOT;
    shot.x = owner->x + (owner->mirrored ? AOS_FIXED(5) : -AOS_FIXED(5));
    shot.y = owner->y - AOS_FIXED(24);
    shot.vx = owner->mirrored ? 0x20000 : -0x20000;
    shot.mirrored = owner->mirrored;
    shot.static_frame = -1;
    shot.stats = owner->stats;
    shot.hp = owner->hp;
    shot.combat.type = AOS_TYPE_ENEMY;
    play(&shot, kind->anims, 6);
    report->children[report->child_count++] = shot;
}

AosHitReport aos_enemy_merman_shot_update(AosEnemy *shot, AosSoma *soma,
                                       const AosEnemyKind *kind,
                                       const AosWeaponEntity *weapon,
                                       const AosWeaponFrames *weapon_frames, int soma_atk,
                                       int soma_def, int cam_x, int cam_y,
                                       uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (shot->state == 2) {
        generic_death(shot, kind, random, &report);
        return report;
    }
    shot->x += shot->vx;
    step_anim(shot, kind->anims);
    int sx = (shot->x >> 16) - cam_x, sy = (shot->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x10) > 0x110 || (uint16_t)sy > 0xB0) {
        shot->removed = true;
        return report;
    }
    report = collide(shot, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    if ((report.enemy_hit || report.soma_hit) && !shot->removed) {
        shot->state = 2;
        shot->timer = 40;
        shot->combat.attack_off = shot->combat.hurt_off = true;
    }
    aos_combat_tick(&shot->combat);
    return report;
}

/* EnemyMermanUpdate, sub_08070E68 and sub_080714A4. The collision layer
 * does not encode the separate room-water height, so the native surface
 * crossings are represented by their exact timed phases around the spawn
 * Y while the solid probes remain native BG1 probes. */
AosHitReport aos_enemy_merman_update(AosEnemy *e, AosSoma *soma,
                                  const AosCollision *layer, const AosEnemyKind *kind,
                                  const AosWeaponEntity *weapon,
                                  const AosWeaponFrames *weapon_frames, int soma_atk,
                                  int soma_def, int cam_x, int cam_y,
                                  uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (e->state == 4) {
        step_anim(e, kind->anims);
        return report;
    }
    if (e->state == 2) {
        e->vy += e->ay;
        e->y += e->vy;
        generic_death(e, kind, random, &report);
        aos_combat_tick(&e->combat);
        return report;
    }
    if (e->state == 3) {
        if ((random() & 7) == 0 && e->spawned_count < 3 && e->param0 == 1) {
            report.spawn = true;
            report.spawn_x = cam_x + 0x90 + (int32_t)(random() & 0xFF);
            report.spawn_y = (e->y >> 16) + 0x20;
            report.spawn_param1 = 0;
            e->spawned_count++;
            e->timer = 60;
        }
        return report;
    }

    int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
    if (e->state == 1) {
        if (e->step == 0) {
            play_loop(e, kind->anims, 3, false);
            e->step = 1;
        } else if (e->step == 1 && e->cycle_done) {
            /* sub_080714A4: sound 0x7A, then the shot. */
            aos_audio_play_song(0x7A);
            merman_shot(e, kind, &report);
            e->step = 2;
        } else if (e->step == 2 && e->cycle_done) {
            e->state = 0;
            e->step = e->param0 ? 0 : 6;
            e->timer = 0;
            e->vy = -0x40000;
            e->ay = 0x1800;
            e->vx = toward_facing(e, 0x8000);
            play(e, kind->anims, 2);
        }
    } else if (e->step == 0) {
        if (++e->timer > 60) {
            e->timer = 0;
            e->step = 1;
            e->vy = -0x8000;
        }
    } else if (e->step == 1) {
        e->y += e->vy;
        e->vy -= 0x8000;
        if (++e->timer >= 8) {
            e->step = 2;
            e->timer = 0;
        }
    } else if (e->step == 2) {
        face_player(e, soma);
        e->vx = toward_facing(e, 0x8000);
        e->vy = -0x30000;
        e->ay = 0x1800;
        play(e, kind->anims, 2);
        e->step = 6;
    } else if (e->step == 4) {
        if (++e->timer > 32) {
            e->step = 2;
            e->timer = 0;
        }
    } else { /* Steps 5, 6, 9 and 10 are the native ballistic phases. */
        int collision = walk_collide(e, layer, 0x10000, (int32_t)0xFFE00000, 0, 8);
        if (collision & 4) {
            if (++e->timer > 60) {
                e->timer = 0;
                if ((random() & 3) == 0) {
                    e->state = 1;
                    e->step = 0;
                    stop(e);
                } else {
                    face_player(e, soma);
                    e->vx = toward_facing(e, 0x8000);
                    e->vy = -0x30000;
                    e->ay = 0x1800;
                }
            }
        }
    }

    step_anim(e, kind->anims);
    sx = (e->x >> 16) - cam_x;
    sy = (e->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0) {
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        merge_enemy_hit(&report, hit);
    }
    aos_combat_tick(&e->combat);
    return report;
}
