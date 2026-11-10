/* SPDX-License-Identifier: GPL-3.0-only */
/* Lilith (id 0x25): EnemyLilithCreate 0x08094818 through sub_080952A0,
 * EnemyLilithUpdate 0x08094750. A floating witch.
 *
 *   state 0  sub_080948DC  five-substep hover machine: wake, activate,
 *                         two drift variants and the dash approach
 *   state 1  sub_08094D28  thirteen-substep attack: the anim-7 dash and
 *                         the anim-6 breath bolt (sub_0809508C /
 *                         sub_080951A0 / sub_08095250 / sub_08095288)
 *   state 2  sub_0806AEAC  shared flame death
 *   state 3  sub_0803F17C  dormant shuffle (story flag 0x8E & 0x40)
 *
 * Approximation notes (documented, not silent):
 * - + 0x18/+ 0x19/+ 0x1A/+ 0x1B/+ 0x2C/+ 0x5A live in li_*, + 0x6E is
 *   the shared anim.frame, + 0x6F the shared anim.tick, + 0x65 the
 *   shared static_frame.
 * - sub_08068AD4/sub_08021924 collapse to outside_margins plus the
 *   (-0x10, 0, 0x110, 0xB0) targetable box; sub_0806D128(4) is the
 *   shared margin hide.
 * - sub_0806AD24's stun gate maps to the + 0x2D hit-flash freeze; while
 *   li_inactive (+ 0x1B) the native skips the contact pass entirely.
 * - sub_080212C8 & 0x60 (a Soma state probe) is approximated as set, so
 *   the dash/attack triggers stay live; sub_0806CEC0's facing compare
 *   always holds after face_player.
 * - sub_08069A00 maps to projectile_collide(0x10000, 0) as in the other
 *   ports; the native's r2/r3 probe margins are not reproduced.
 * - The breath bolt (sub_080951A0) keeps its + 0x1C sawtooth speed and
 *   its + 0x72 |= 2 attack-off flag; its two sub_080458E4 trail slots
 *   and the 0x0A0AFBFB parameter pack are not reproduced.
 * - The + 0x5A palette merge of the lethal hit is kept in li_tint only
 *   as a record; the draw-side merge is not ported.
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
#define generic_death aos_enemy_generic_death
#define spawn_drift aos_enemy_spawn_drift

static int32_t li_px(const AosEnemy *e) { return (int32_t)(e->x >> 16); }
static int32_t li_py(const AosEnemy *e) { return (int32_t)(e->y >> 16); }

/* sub_0809508C: the breath bolt — a non-damaging (+ 0x72 |= 2) child on
 * the shared tiles/frames, static frame 0x36, spawned 0x1C pixels above
 * the body; + 0x1C starts 0x8000 (7) facing right, 0 (0x15) mirrored. */
static void li_spawn_bolt(AosHitReport *report, AosEnemy *e,
                          const AosEnemyKind *kind) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy *c = &report->children[report->child_count++];
    aos_enemy_make_static_child(c, e, AOS_ROLE_LILITH_BOLT, 0x36);
    c->x = e->x;
    c->y = e->y - 0x1C0000;
    c->mirrored = e->mirrored;
    c->li_bolt = e->mirrored ? 0 : 0x8000;
    c->timer = e->mirrored ? 0x15 : 7;
    c->state = c->step = c->substep = 0;
    c->hp = c->stats.hp;
    c->param0 = c->param1 = 0;
    c->removed = false;
    c->defeated = false;
    c->hit_flash = 0;
    c->combat.attack_off = true;    /* + 0x72 |= 2 */
    c->combat.hurt_off = false;
    (void)kind;
}

/* sub_080951A0 + sub_08095250/sub_08095288: the bolt's + 0x1C sawtooth
 * (+= 0x400 for the first 0xE ticks, -= 0x400 until 0x1C, timer wrap),
 * the shared collide and the margin delete; a struck bolt dies through
 * state 1/2 (+ 0x59 |= 8). */
static AosHitReport li_bolt_update(AosEnemy *e, AosSoma *soma,
                                   const AosEnemyKind *kind,
                                   const AosWeaponEntity *weapon,
                                   const AosWeaponFrames *weapon_frames,
                                   int soma_atk, int soma_def,
                                   int cam_x, int cam_y) {
    AosHitReport report = {0};
    if (e->state == 0) {
        uint8_t old = e->timer;
        e->timer++;
        if (old <= 0xE)
            e->li_bolt += 0x400;
        else if (old <= 0x1C)
            e->li_bolt -= 0x400;
        if (old > 0x1B)
            e->timer = 0;
        e->vx = e->mirrored ? -e->li_bolt : e->li_bolt;
        e->vy = 0;
        e->x += e->vx;
        int sx = li_px(e) - cam_x, sy = li_py(e) - cam_y;
        if (outside_margins(kind, 1, sx, sy)) {
            e->removed = true;
            return report;
        }
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        if (hit.enemy_hit)
            e->removed = true;      /* sub_08095288 -> state 1 -> + 0x59|8 */
        copy_result(&report, &hit);
        return report;
    }
    /* states 1/2: the native only raises + 0x59 |= 8 and frees the two
     * trail slots — the bolt is gone. */
    e->removed = true;
    return report;
}

/* sub_080948DC state 0: the hover machine. Shared drift tail keeps
 * ax = mirrored ? +0x800 : -0x800 and vy = 0x10000 / ay = 0x1800. */
static void li_hover_drift(AosEnemy *e, const AosCollision *layer) {
    e->ax = e->mirrored ? 0x800 : -0x800;
    e->vy = 0x10000;
    projectile_collide(e, layer, 0x10000, 0);
    e->ay = 0x1800;
}

static void li_state0(AosHitReport *report, AosEnemy *e, AosSoma *soma,
                      const AosCollision *layer,
                      const AosEnemyKind *kind,
                      uint32_t (*random)(void)) {
    int32_t px = li_px(e), py = li_py(e);
    switch (e->step) {
    case 0:
        face_player(e, soma);
        if (player_in(soma, px - 0x32, py - 0x28, 0x64, 0x50)) {
            e->step = 1;
            play(e, kind->anims, 4);
            e->hidden = false;         /* + 0x58 &= ~0x20 */
            aos_audio_play_song(0x1C8);
        }
        break;
    case 1:
        if (e->cycle_done) {
            play_loop(e, kind->anims, 5, true);
            e->step = 2;
            e->timer = 0;
            e->li_inactive = 0;        /* + 0x1B = 0: tangible now */
        }
        break;
    case 2: {
        uint8_t old = e->timer;
        e->timer++;
        if (old > 0x3B) {
            face_player(e, soma);
            e->timer = 0;
        }
        if (player_in(soma, px - 0x32, py - 0x28, 0x64, 0x50)) {
            /* + 0x19 gate: clear -> dash, rng fail -> + 0x19 = 1,
             * already set -> plain drift (no extra coin). */
            if (e->li_flag == 0 && !(random() & 1)) {
                e->vx = e->mirrored ? -0x28000 : 0x28000;
                e->step = 4;
                e->timer = 0;
                e->ax = 0;
                spawn_drift(0, px - 0x10, py - 0x20, report);
                spawn_drift(0, px + 0x10, py - 0x10, report);
                if ((random() & 0x7F) > 0x2F)
                    aos_audio_play_song(0x1C8);
                return;
            }
            e->li_flag = 1;
        }
        li_hover_drift(e, layer);
        if (player_in(soma, px - 0x28, py - 0x28, 0x50, 0x50)) {
            if (random() & 1) {
                e->step = 3;
                e->timer = 0;
            } else {
                e->state = 1;
                e->step = 0xA;
                e->timer = 1;
            }
        } else if (clamp_to_room(e, layer, 4)) {
            e->step = 3;
            e->timer = 0;
        } else {
            uint8_t old_a = e->li_approach;
            e->li_approach++;
            if (old_a > 0x3B)
                e->li_approach = 0;
            if (player_in(soma, px - 0xC8, py - 0x28, 0x190, 0x50) &&
                !(random() & 1)) {
                e->state = 1;
                e->step = 0xA;
                e->timer = 0;
            }
        }
        e->vy = 0x10000;
        e->ay = 0x1800;
        break;
    }
    case 3: {
        face_player(e, soma);
        if (player_in(soma, px - 0x32, py - 0x28, 0x64, 0x50)) {
            if (e->li_flag == 0 && !(random() & 1)) {
                e->vx = e->mirrored ? -0x28000 : 0x28000;
                e->step = 4;
                e->timer = 0;
                e->ax = 0;
                spawn_drift(0, px - 0x10, py - 0x20, report);
                spawn_drift(0, px + 0x10, py - 0x10, report);
                return;
            }
            e->li_flag = 1;
        }
        li_hover_drift(e, layer);
        if (player_in(soma, px - 0x46, py - 0x28, 0x8C, 0x50)) {
            if (!clamp_to_room(e, layer, 4)) {
                if (player_in(soma, px - 0x28, py - 0x28, 0x50, 0x50) &&
                    !(random() & 1)) {
                    e->state = 1;
                    e->step = 0xA;
                    e->timer = 1;
                } else {
                    uint8_t old_a = e->li_approach;
                    e->li_approach++;
                    if (old_a > 0x3B)
                        e->li_approach = 0;
                    if (player_in(soma, px - 0xC8, py - 0x28, 0x190,
                                  0x50) && !(random() & 1)) {
                        e->state = 1;
                        e->step = 0xA;
                        e->timer = 0;
                    }
                }
            } else {
                e->step = 2;
                e->timer = 0;
            }
        } else if (random() & 1) {
            e->state = 1;
            e->step = 0xA;
            e->timer = 0;
        } else {
            e->step = 2;
            e->timer = 0;
        }
        if (e->state == 0 && e->step == 2) {
            e->vy = 0x10000;
            e->ay = 0x1800;
        }
        break;
    }
    case 4: {
        /* sub_0806D490(e, 0x18): the 7/8 friction pass */
        e->vx = (int32_t)(((int64_t)e->vx * 7) / 8);
        e->vy = 0x10000;
        projectile_collide(e, layer, 0x10000, 0);
        clamp_to_room(e, layer, 4);
        uint8_t old = e->timer;
        e->timer++;
        if (old > 0x27) {
            e->step = 2;
            e->timer = 0;
            e->vx = e->ax = 0;
            e->vy = 0x10000;
            e->ay = 0x1800;
            e->li_flag = 0;
        }
        break;
    }
    default:
        break;
    }
}

/* sub_08094D28 state 1: the attack machine. + 0x6E gates read the shared
 * anim.frame, + 0x6F the shared anim.tick. */
static void li_state1(AosHitReport *report, AosEnemy *e, AosSoma *soma,
                      const AosCollision *layer,
                      const AosEnemyKind *kind) {
    switch (e->step) {
    case 0:
        play(e, kind->anims, 7);
        e->step = 1;
        e->ax = 0;
        e->timer = 0;
        /* fallthrough */
    case 1:
        e->vx = (int32_t)(((int64_t)e->vx * 7) / 8);
        e->vy = 0x10000;
        projectile_collide(e, layer, 0x10000, 0);
        clamp_to_room(e, layer, 4);
        if (e->anim.frame > 3) {
            e->vx = e->mirrored ? -0x30000 : 0x30000;
            e->step = 2;
            e->timer = 0;
            spawn_drift(0, li_px(e) - 0x10, li_py(e) - 0x20, report);
            spawn_drift(0, li_px(e) + 0x10, li_py(e) - 0x10, report);
            e->li_breath = 0;
            aos_audio_play_song(0x85);
        }
        break;
    case 2:
        e->vx = (int32_t)(((int64_t)e->vx * 7) / 8);
        e->vy = 0x10000;
        projectile_collide(e, layer, 0x10000, 0);
        clamp_to_room(e, layer, 4);
        if (e->li_breath)
            e->anim.tick = 0x64;       /* + 0x6F = 0x64 */
        if (e->anim.frame > 0xC) {
            face_player(e, soma);
            if (!e->li_breath) {
                /* disengage: state 0 step 3, anim 5 */
                e->state = 0;
                e->step = 3;
                e->timer = 0;
                e->vx = e->ax = 0;
                e->vy = 0x10000;
                e->ay = 0x1800;
                play_loop(e, kind->anims, 5, true);
                break;
            }
            e->vx = e->mirrored ? -0x20000 : 0x20000;
            e->step = 3;
            e->timer = 0;
            e->ax = 0;
            e->anim.tick = 0;
        }
        break;
    case 3: {
        e->vx = (int32_t)(((int64_t)e->vx * 7) / 8);
        e->vy = 0x10000;
        projectile_collide(e, layer, 0x10000, 0);
        clamp_to_room(e, layer, 4);
        uint8_t old = e->timer;
        e->timer++;
        if (old > 0x1D) {
            e->state = 0;
            e->step = 2;
            e->timer = 0;
            e->vx = e->ax = 0;
            e->vy = 0x10000;
            e->ay = 0x1800;
            e->li_flag = 0;
            play_loop(e, kind->anims, 5, true);
        }
        break;
    }
    case 10:
        play(e, kind->anims, 6);
        e->step = 11;
        e->ax = 0;
        e->timer = 0;
        /* fallthrough */
    case 11:
        e->vx = (int32_t)(((int64_t)e->vx * 7) / 8);
        e->vy = 0x10000;
        projectile_collide(e, layer, 0x10000, 0);
        clamp_to_room(e, layer, 4);
        if (e->anim.frame > 4) {
            li_spawn_bolt(report, e, kind);
            e->step = 12;
            aos_audio_play_song(0x7B);
        }
        break;
    case 12:
        e->vx = (int32_t)(((int64_t)e->vx * 7) / 8);
        e->vy = 0x10000;
        projectile_collide(e, layer, 0x10000, 0);
        clamp_to_room(e, layer, 4);
        if (e->cycle_done) {
            face_player(e, soma);
            e->state = 0;
            e->step = 2;
            e->timer = 0;
            e->vx = e->ax = 0;
            e->vy = 0x10000;
            e->ay = 0x1800;
            play_loop(e, kind->anims, 5, true);
        }
        break;
    default:
        break;                      /* cases 4-9: the shared exit */
    }
}

AosHitReport aos_enemy_lilith_update(AosEnemy *e, AosSoma *soma,
                                     const AosCollision *layer,
                                     const AosEnemyKind *kind,
                                     const AosWeaponEntity *weapon,
                                     const AosWeaponFrames *weapon_frames,
                                     int soma_atk, int soma_def, int cam_x,
                                     int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (e->role == AOS_ROLE_LILITH_BOLT)
        return li_bolt_update(e, soma, kind, weapon, weapon_frames,
                              soma_atk, soma_def, cam_x, cam_y);
    /* The + 0x2D freeze: a stunned Lilith outside state 2 runs the contact
     * check alone — unless + 0x1B still holds, which skips it too. */
    if (e->hit_flash && e->state != 2) {
        if (!e->li_inactive) {
            AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                                       soma_atk, soma_def);
            copy_result(&report, &hit);
        }
        return report;
    }
    int sx = li_px(e) - cam_x, sy = li_py(e) - cam_y;
    if (e->state == 3) {
        /* dormant: the native runs only the animation step */
        e->hidden = outside_margins(kind, 4, sx, sy);
        step_anim(e, kind->anims);
        return report;
    }
    e->hidden = outside_margins(kind, 4, sx, sy);
    if (!e->defeated && (uint16_t)(sx + 0x10) <= 0x110 &&
        (uint16_t)sy <= 0xB0)
        report.targetable = true;
    switch (e->state) {
    case 0:
        li_state0(&report, e, soma, layer, kind, random);
        break;
    case 1:
        li_state1(&report, e, soma, layer, kind);
        break;
    case 2:
        generic_death(e, kind, random, &report);
        break;
    default:
        break;
    }
    step_anim(e, kind->anims);
    if (e->state != 2 && !e->li_inactive) {
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        if (hit.soma_hit && e->state == 1)
            e->li_breath = 1;       /* sub_080952EC contact callback */
        copy_result(&report, &hit);
    }
    return report;
}
