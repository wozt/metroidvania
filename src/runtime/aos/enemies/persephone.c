/* SPDX-License-Identifier: GPL-3.0-only */
/* Persephone (id 0x24): EnemyPersephoneCreate 0x0807B5D0, resource
 * loader sub_0807BDD0, EnemyPersephoneUpdate 0x0807B47C. A spear nun.
 *
 *   state 0  sub_0807B680  seven-substep approach/retreat machine
 *   state 1  sub_0807BA20  twelve-substep attack: dive, uppercut and
 *                         frame-synced melee strikes (sub_0807BF1C /
 *                         sub_0807BFEC / sub_0807C068)
 *   state 2  inline        blink/sink death, timer delete
 *   state 3  sub_0807C014  hurt knockdown: gravity drop, floor resume
 *   state 4  sub_0807BD70  dormant idle shuffle (story flag 0x8E&0x40)
 *
 * Approximation notes (documented, not silent):
 * - + 0x6E reads are the shared anim.frame, + 0x6C the shared
 *   anim.flags, + 0x20 lives in pr_hit.
 * - sub_08069A00 maps to projectile_collide(0x10000, 0): same flag bits
 *   and 8.0 clamp, without the native probe margins.
 * - sub_0806BD64 (the dive distance) is approximated by |dx| in pixels.
 * - The strike child (sub_0807BFEC) registers contact through the
 *   shared collide path and lives two ticks (+ 0x1C countdown).
 * - The + 0x2D hit-flash freeze runs the contact check alone; the
 *   + 0x5A palette merge, the sub_0806CCE8 draw variant and the
 *   room-event helpers are skipped.
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
#define projectile_collide aos_enemy_projectile_collide
#define clamp_to_room aos_enemy_clamp_to_room
#define face_player aos_enemy_face_player
#define pixel_delta aos_enemy_pixel_delta

static int32_t pr_px(const AosEnemy *e) { return (int32_t)(e->x >> 16); }
static int32_t pr_py(const AosEnemy *e) { return (int32_t)(e->y >> 16); }

/* Emit one melee strike (sub_0807BF1C): a two-tick contact child on
 * the shared tiles/frames, facing and + 0x3C element copied, + 0x14
 * linked to the body and + 0x65 = 0x14. */
static void pr_spawn_strike(AosHitReport *report, AosEnemy *e,
                            const AosAnimSet *anims) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy *c = &report->children[report->child_count++];
    *c = *e;
    c->role = AOS_ROLE_PERSEPHONE_STRIKE;
    c->parent = e;
    c->state = c->step = c->substep = 0;
    c->timer = 0;
    c->walk_timer = 1;              /* + 0x1C lifetime: two updates */
    c->vx = c->vy = c->ax = c->ay = 0;
    c->spin = 0;
    c->angle = 0;
    c->variant = e->variant ? e->variant - 1 : 0;
    c->contact_elements = e->contact_elements;
    c->static_frame = 0x14;
    c->pr_hit = 0;
    c->hp = c->stats.hp;
    c->param0 = c->param1 = 0;
    c->spawned_count = 0;
    c->boss_parts_spawned = true;
    c->removed = false;
    c->defeated = false;
    c->hit_flash = 0;
    c->combat.attack_off = false;
    c->combat.hurt_off = true;
    play_loop(c, anims, 0, true);
}

/* sub_0807BFEC + sub_0807C068: the strike registers its contact
 * (sub_080421AC through the shared collide) and counts + 0x1C down to
 * its delete; a landed hit marks the parent's + 0x20. */
static AosHitReport pr_strike_update(AosEnemy *e, AosSoma *soma,
                                     const AosEnemyKind *kind,
                                     const AosWeaponEntity *weapon,
                                     const AosWeaponFrames *weapon_frames,
                                     int soma_atk, int soma_def) {
    AosHitReport report = {0};
    AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                               soma_atk, soma_def);
    if (hit.soma_hit && e->parent && e->parent->id == AOS_ENEMY_PERSEPHONE)
        e->parent->pr_hit = 1;
    copy_result(&report, &hit);
    /* + 0x1C: the native deletes when the value read is <= 0. */
    int32_t left = e->walk_timer;
    e->walk_timer = left - 1;
    if (left <= 0)
        e->removed = true;
    return report;
}

/* sub_0807B680 state 0: the seven-substep approach/retreat machine. */
static void pr_state0(AosEnemy *e, AosSoma *soma,
                      const AosCollision *layer,
                      const AosEnemyKind *kind,
                      int cam_x, int cam_y,
                      uint32_t (*random)(void)) {
    int32_t px = pr_px(e), py = pr_py(e);
    switch (e->step) {
    case 0:
        if (player_in(soma, px - 0x78, py - 0x30, 0xF0, 0x60) ||
            e->hp < e->stats.hp) {
            e->step = 1;
            face_player(e, soma);
            play(e, kind->anims, 1);
            e->timer = 0;
        }
        break;
    case 1:
        e->timer++;
        if (e->timer == 5 &&
            (e->param0 != 0 || (random() & 0x7F) <= 0x3F))
            aos_audio_play_song(0x1C9);
        if (e->cycle_done) {
            e->step = 2;
            face_player(e, soma);
            play(e, kind->anims, 2);
            e->vx = e->mirrored ? 0x10000 : -0x10000;
            e->vy = 0x10000;
            e->ay = 0x2800;
            e->timer = 0;
        }
        break;
    case 2: {
        uint8_t old = e->timer;
        e->timer++;
        if (old > 0x1D) {
            face_player(e, soma);
            e->vx = e->mirrored ? 0x10000 : -0x10000;
            e->vy = 0x10000;
            e->ay = 0x2800;
            e->timer = 0;
        }
        int flags = projectile_collide(e, layer, 0x10000, 0);
        if ((flags & 4) &&
            player_in(soma, px - 0x28, py - 0x30, 0x50, 0x60)) {
            e->step = 3;
            play(e, kind->anims, 3);
        }
        break;
    }
    case 3:
        if (e->cycle_done) {
            e->state = 1;
            e->step = 0;
            e->timer = 0;
        }
        break;
    case 4:
        break;
    case 5: {
        uint8_t old = e->timer;
        e->timer++;
        if (old > 0x1D) {
            face_player(e, soma);
            e->timer = 0;
        }
        e->vx = e->mirrored ? 0x8000 : -0x8000;
        e->vy = 0x10000;
        e->ay = 0x2800;
        projectile_collide(e, layer, 0x10000, 0);
        if (player_in(soma, px - 0x28, py - 0x30, 0x50, 0x60)) {
            if (random() & 1) {
                e->step = 6;
                e->timer = 0;
            } else {
                e->state = 1;
                e->step = 0;
                e->timer = 0;
            }
        } else if (clamp_to_room(e, layer, 1)) {
            e->step = 6;
            e->timer = 0;
        } else if (outside_margins(kind, 4, px - cam_x, py - cam_y)) {
            play(e, kind->anims, 0);
            e->step = 0;
        }
        break;
    }
    case 6:
        face_player(e, soma);
        e->vx = e->mirrored ? -0x8000 : 0x8000;
        e->vy = 0x10000;
        e->ay = 0x2800;
        projectile_collide(e, layer, 0x10000, 0);
        if (!player_in(soma, px - 0x3C, py - 0x30, 0x78, 0x60)) {
            e->step = 5;
            e->timer = 0;
        } else if (clamp_to_room(e, layer, 1)) {
            e->step = 5;
            e->timer = 0;
        } else if (player_in(soma, px - 0x28, py - 0x30, 0x50, 0x60) &&
                   !(random() & 1)) {
            e->state = 1;
            e->step = 0;
            e->timer = 0;
        }
        break;
    default:
        break;
    }
}

/* sub_0807BA20 state 1: the attack machine. + 0x6E gates read the
 * shared anim.frame; sub_0807BD68 is the shared no-op exit. */
static void pr_state1(AosHitReport *report, AosEnemy *e, AosSoma *soma,
                      const AosCollision *layer,
                      const AosEnemyKind *kind,
                      uint32_t (*random)(void)) {
    int32_t px = pr_px(e), py = pr_py(e);
    int32_t dist = pixel_delta(soma->x, e->x);
    if (dist < 0) dist = -dist;
    switch (e->step) {
    case 0:
        play(e, kind->anims, 4);
        e->step = 1;
        e->pr_hit = 0;
        face_player(e, soma);
        if (dist > 0x20 && (random() & 1))
            e->step = 0xA;
        break;
    case 1:
        if (e->anim.frame <= 1) break;
        aos_audio_play_song(0x1CA);
        e->step = 2;
        e->timer++;
        {
            int32_t v;
            if (dist <= 7) v = -0x10000;
            else if (dist <= 0xF) v = -0x8000;
            else if (dist <= 0x1F) v = 0x10000;
            else if (dist <= 0x2F) v = 0x34000;
            else v = 0x100000;
            e->vx = e->mirrored ? v : -v;
            e->vy = 0x10000;
            e->ay = 0x2800;
        }
        break;
    case 2: {
        if (e->anim.frame > 4)
            e->vx = 0;
        /* sub_0806D490(e, 8): the 7/8 friction pass */
        e->vx = (int32_t)(((int64_t)e->vx * 7) / 8);
        int flags = projectile_collide(e, layer, 0x10000, 0);
        bool landed = (flags & 4) && e->cycle_done &&
                      (e->timer > 1 || e->pr_hit == 1);
        if (!landed) {
            if (e->anim.frame == 3) {
                pr_spawn_strike(report, e, kind->anims);
                aos_audio_play_song(0x85);
            }
        } else {
            e->step = 3;
            e->timer = 0;
            e->vy = 0x10000;
            e->ay = 0x2800;
            play(e, kind->anims, 5);
        }
        break;
    }
    case 3:
        face_player(e, soma);
        e->vx = e->mirrored ? -0x8000 : 0x8000;
        e->vy = 0x10000;
        e->ay = 0x2800;
        projectile_collide(e, layer, 0x10000, 0);
        if (!player_in(soma, px - 0x3C, py - 0x30, 0x78, 0x60) ||
            clamp_to_room(e, layer, 1)) {
            e->state = 0;
            e->step = 5;
            e->timer = 0;
        } else {
            uint8_t old = e->timer;
            e->timer++;
            if (old > 0x3B) {
                e->state = 0;
                e->step = 6;
                e->timer = 0;
            } else {
                break;
            }
        }
        e->vy = 0x10000;
        e->ay = 0x2800;
        break;
    case 4: case 5: case 6: case 7: case 8: case 9:
        break;                      /* sub_0807BD68 shared exit */
    case 0xA:
        if (e->anim.frame == 0) break;
        aos_audio_play_song(0x1CA);
        e->step = 0xB;
        e->timer++;
        e->vx = e->mirrored ? 0x140000 : -0x140000;
        e->vy = -0x20000;
        e->ay = 0x2000;
        projectile_collide(e, layer, 0x10000, 0);
        break;
    case 0xB: {
        if (e->anim.frame > 4)
            e->anim.flags |= 2;
        int flags = projectile_collide(e, layer, 0x10000, 0);
        if (flags & 4) {
            if (e->timer > 1 || e->pr_hit == 1) {
                e->step = 3;
                e->timer = 0;
                e->vy = 0x10000;
                e->ay = 0x2800;
                play(e, kind->anims, 5);
            } else {
                e->step = 0;
            }
        } else if (e->anim.frame == 3) {
            pr_spawn_strike(report, e, kind->anims);
            aos_audio_play_song(0x85);
        }
        break;
    }
    default:
        break;
    }
}

/* sub_0807C014 state 3: the hurt knockdown — gravity drop that
 * resumes the approach (state 0, step 6) on the floor. */
static void pr_state3(AosEnemy *e, const AosCollision *layer,
                      const AosEnemyKind *kind) {
    e->ay = 0x2800;
    e->vx = 0;
    e->ax = 0;
    e->vy = 0x10000;
    int flags = projectile_collide(e, layer, 0x10000, 0);
    if (flags & 4) {
        e->state = 0;
        e->step = 6;
        e->timer = 0;
        e->vy = 0x10000;
        e->ay = 0x2800;
        play(e, kind->anims, 5);
    }
}

/* sub_0807BD70 state 4: the dormant shuffle — wait ~0x78 ticks on
 * anim 1, then loop anim 0 and re-seed the timer (sub_0803F17C tail). */
static void pr_state4(AosEnemy *e, const AosEnemyKind *kind,
                      uint32_t (*random)(void)) {
    if (e->step == 0) {
        uint8_t old = e->timer;
        e->timer++;
        if (old > 0x77) {
            e->step = 1;
            play(e, kind->anims, 1);
        }
    } else if (e->step == 1) {
        if (e->cycle_done) {
            play(e, kind->anims, 0);
            e->step = 0;
            e->timer = random() & 0x1F;
        }
    }
}

AosHitReport aos_enemy_persephone_update(AosEnemy *e, AosSoma *soma,
                                        const AosCollision *layer,
                                        const AosEnemyKind *kind,
                                        const AosWeaponEntity *weapon,
                                        const AosWeaponFrames *weapon_frames,
                                        int soma_atk, int soma_def, int cam_x,
                                        int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (e->role == AOS_ROLE_PERSEPHONE_STRIKE)
        return pr_strike_update(e, soma, kind, weapon, weapon_frames,
                                soma_atk, soma_def);
    /* The + 0x2D freeze: hit-flash outside states 2/3 runs the contact
     * check alone (sub_080421AC), skipping the state machine and the
     * shared tail. */
    if (e->hit_flash && e->state != 2 && e->state != 3) {
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        copy_result(&report, &hit);
        return report;
    }
    int sx = pr_px(e) - cam_x, sy = pr_py(e) - cam_y;
    if (e->state == 4) {
        /* dormant shuffle keeps its own hide/targetable handling */
    } else {
        e->hidden = outside_margins(kind, 4, sx, sy);
        if (!e->defeated && (uint16_t)(sx + 0x10) <= 0x110 &&
            (uint16_t)sy <= 0xB0)
            report.targetable = true;
    }
    switch (e->state) {
    case 0:
        pr_state0(e, soma, layer, kind, cam_x, cam_y, random);
        break;
    case 1:
        pr_state1(&report, e, soma, layer, kind, random);
        break;
    case 2:
        /* inline death: gravity, blink strobe, timer delete */
        e->ay = 0x2800;
        e->vx = 0;
        e->ax = 0;
        projectile_collide(e, layer, 0x10000, 0);
        e->hidden = (kind->blink[e->timer & 0x27] & 1) != 0;
        if (--e->timer <= 0)
            e->removed = true;
        break;
    case 3:
        pr_state3(e, layer, kind);
        break;
    case 4:
        pr_state4(e, kind, random);
        break;
    default:
        break;
    }
    step_anim(e, kind->anims);
    return report;
}
