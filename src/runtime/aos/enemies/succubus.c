/* SPDX-License-Identifier: GPL-3.0-only */
/* Succubus (id 0x5F): EnemySuccubusCreate 0x08093D24 through
 * sub_08094710, EnemySuccubusUpdate 0x08093C34. A floating succubus that
 * shares Lilith's tiles/palette/frames.
 *
 *   state 0  sub_08093E04  five-substep hover machine: aggro boxes,
 *                         anim wait, two drift variants and the dash
 *   state 1  sub_08094238  twelve-substep attack: dash, chain-homing
 *                         pounce (sub_0801D028 grab probe) and return
 *   state 2  sub_0806AEAC  shared flame death
 *   state 3  sub_0803F17C  dormant shuffle (story flag 0x8E & 0x40)
 *
 * Approximation notes (documented, not silent):
 * - + 0x18/+ 0x19/+ 0x1A/+ 0x1B/+ 0x1C/+ 0x1D/+ 0x1E/+ 0x1F/+ 0x20/+ 0x21
 *   live in su_*, + 0x2C in su_pal, + 0x5A in su_tint; + 0x6E is the
 *   shared anim.frame, + 0x6F the shared anim.tick, + 0x65 the shared
 *   static_frame.
 * - sub_08068AD4/sub_08021924 collapse to outside_margins plus the
 *   (-0x10, 0, 0x110, 0xB0) targetable box; sub_0806D128(4) is the
 *   shared margin hide.
 * - sub_0806AD24's stun gate maps to the + 0x2D hit-flash freeze; while
 *   su_inactive (+ 0x1B) the native skips the contact pass entirely.
 * - sub_0806CF2C/sub_0806CEC0's facing probes collapse to face_player;
 *   the native's facing compare always holds afterwards.
 * - sub_0806D490(e, n) is vx * (n - 1) / n (signed): 0x10 -> *15/16,
 *   0x18 -> *23/24; sub_08069A00 maps to walk_collide(0x10000,
 *   -0x200000, 0x40000, 8) and sub_0806B120 to clamp_to_room.
 * - sub_0806CFFC is player_near on room-pixel boxes off the + 0x42/+ 0x46
 *   integer position; sub_080421AC is the shared collide.
 * - The sub_08021654 contact records of the homing pounce (state 1
 *   step 10) become positional elemental hits like the dryad seed's;
 *   the record's + 0x3C attack byte maps to contact_elements.
 * - sub_0801D028(0xE, 0, 0, 0), the native grab probe that also plays
 *   player animation 0xE, is not ported and returns 0; the pounce then
 *   ends only through the + 0x1F/+ 0x1E counters (documented
 *   divergence).
 * - The + 0x1C/+ 0x65 box-refresh compare of the update tail is implicit
 *   in the shared collide; sub_0806D260's callback-variant pick and the
 *   sub_0804277C/sub_080427B8 registrations are not ported.
 * - The hurt pass (sub_08094620) never flinches: + 0x2D = 8 flash, + 0x18
 *   = 1, hp -= damage; lethal adds sub_080683BC, song 0x1CC, animation
 *   9, the + 0x5A palette merge (kept in su_tint only), + 0xD = 0x28 and
 *   state 2. su_gate mirrors + 0x18 through the hit flash.
 */
#include "../core/aos_enemy_internal.h"
#include "../core/aos_audio.h"
#include "../core/aos_collision.h"
#include "../core/aos_soma.h"

#define collide aos_enemy_collide
#define copy_result aos_enemy_copy_collision_result
#define outside_margins aos_enemy_outside_margins
#define step_anim aos_enemy_step_anim
#define play_loop aos_enemy_play_loop
#define play aos_enemy_play
#define player_in aos_enemy_player_in
#define player_near aos_enemy_player_near
#define walk_collide aos_enemy_walk_collide
#define clamp_to_room aos_enemy_clamp_to_room
#define face_player aos_enemy_face_player
#define generic_death aos_enemy_generic_death
#define spawn_drift aos_enemy_spawn_drift
#define target_x aos_enemy_target_x
#define target_y aos_enemy_target_y

static int32_t su_px(const AosEnemy *e) { return (int16_t)(e->x >> 16); }
static int32_t su_py(const AosEnemy *e) { return (int16_t)(e->y >> 16); }

/* sub_0806D490(e, n): vx * (n - 1) / n through the signed division. */
static void su_friction(AosEnemy *e, int n) {
    e->vx = (int32_t)(((int64_t)e->vx * (n - 1)) / n);
}

/* The two sub_080458E4 trail marks of the dash entries: frame 0x38 at
 * (+0xC, +0x10) and (+0x6, +0x10) — approximated as drift effects at the
 * body position plus the native offsets. */
static void su_trails(AosEnemy *e, AosHitReport *report) {
    spawn_drift(0, e->x + 0xC0000, e->y + 0x100000, report);
    spawn_drift(0, e->x + 0x60000, e->y + 0x100000, report);
}

/* The shared drift tail of state 0 steps 2 and 3: the facing refresh,
 * the 0x64 x 0xA0 dash-trigger box off (+0x32, +0x50), the + 0x19
 * attempt flag and the vx = mirrored ? -0x28000 : 0x28000 dash into
 * step 4. The native dash fires only while + 0x19 is clear, the Soma
 * probe (sub_080212C8 & 0x60, approximated as set) holds and RNG & 1
 * is 0; a failed roll only marks + 0x19. Returns true when it fired. */
static bool su_dash_check(AosEnemy *e, AosSoma *soma,
                          const AosCollision *layer, AosHitReport *report,
                          uint32_t (*random)(void)) {
    face_player(e, soma);
    if (player_near(soma, su_px(e) - 0x32, su_py(e) - 0x50, 0x64, 0xA0) &&
        e->su_try == 0) {
        if (random() & 1) {
            e->su_try = 1;
        } else {
            e->vx = e->mirrored ? -0x28000 : 0x28000;
            e->step = 4;
            e->timer = 0;
            su_trails(e, report);
            if ((random() & 0x7F) <= 0x2F)
                aos_audio_play_song(0x1C8);
            return true;
        }
    }
    walk_collide(e, layer, 0x10000, -0x200000, 0x40000, 8);
    return false;
}

/* sub_08093E04 state 0: the hover machine. */
static void su_state0(AosHitReport *report, AosEnemy *e, AosSoma *soma,
                      const AosCollision *layer,
                      const AosEnemyKind *kind,
                      uint32_t (*random)(void)) {
    int32_t px = su_px(e), py = su_py(e);
    switch (e->step) {
    case 0:
        face_player(e, soma);
        if (!e->su_aggro &&
            player_near(soma, px - 0x5A, py - 0x40, 0xB4, 0x46)) {
            e->su_aggro = 1;
            aos_audio_play_song(0x1C7);
        }
        if (e->su_gate == 1 ||
            player_near(soma, px - 0x1E, py - 0x40, 0x3C, 0x46)) {
            e->step++;
            play(e, kind->anims, 0);
        }
        break;
    case 1:
        if (e->anim.frame == 5)
            aos_audio_play_song(0x1C8);
        if (e->cycle_done) {
            play_loop(e, kind->anims, 1, true);
            e->step++;
            e->timer = 0;
            e->su_touch = 0;
            e->su_inactive = 0;
        }
        break;
    case 2: {
        uint8_t old = e->timer;
        e->timer++;
        if (old > 0x1D) {
            e->timer = 0;
            face_player(e, soma);
            e->vx = e->mirrored ? -0x6000 : 0x6000;
        }
        if (su_dash_check(e, soma, layer, report, random))
            return;
        if (player_near(soma, su_px(e) - 0x28, su_py(e) - 0x50, 0x50,
                        0xA0)) {
            if (random() & 1) {
                e->step = 3;
                e->timer = 0;
                e->vy = 0x10000;
                e->ay = 0x1800;
            } else {
                e->state = 1;
                e->step = 0;
                e->timer = 0;
            }
        } else if (clamp_to_room(e, layer, 4)) {
            e->step = 3;
            e->timer = 0;
            e->vy = 0x10000;
            e->ay = 0x1800;
        }
        break;
    }
    case 3: {
        uint8_t old = e->timer;
        e->timer++;
        if (old > 0x1D) {
            e->timer = 0;
            face_player(e, soma);
            e->vx = e->mirrored ? -0x6000 : 0x6000;
        }
        if (su_dash_check(e, soma, layer, report, random))
            return;
        if (player_near(soma, su_px(e) - 0x46, su_py(e) - 0x50, 0x8C,
                        0xA0) || clamp_to_room(e, layer, 4)) {
            e->step = 2;
            e->timer = 0;
            e->vy = 0x10000;
            e->ay = 0x1800;
        } else if (player_near(soma, su_px(e) - 0x28, su_py(e) - 0x50,
                              0x50, 0xA0) && !(random() & 1)) {
            e->state = 1;
            e->step = 0;
            e->timer = 0;
        }
        break;
    }
    case 4:
        su_friction(e, 0x18);
        walk_collide(e, layer, 0x10000, -0x200000, 0x40000, 8);
        clamp_to_room(e, layer, 4);
        if (e->timer++ > 0x27) {
            e->step = 2;
            e->timer = 0;
            e->vy = 0x10000;
            e->ay = 0x1800;
            e->su_try = 0;
        }
        break;
    default:
        break;
    }
}

/* sub_08094238 state 1: the attack machine. */
static void su_state1(AosHitReport *report, AosEnemy *e, AosSoma *soma,
                      const AosCollision *layer,
                      const AosEnemyKind *kind,
                      int soma_def, uint32_t (*random)(void)) {
    (void)random;
    switch (e->step) {
    case 0:
        su_friction(e, 0x10);
        walk_collide(e, layer, 0x10000, -0x200000, 0x40000, 8);
        clamp_to_room(e, layer, 4);
        if (e->timer++ > 0x27) {
            play(e, kind->anims, 2);
            e->step++;
            e->timer = 0;
            e->vx = 0;
            aos_audio_play_song(0x1C7);
        }
        break;
    case 1:
        face_player(e, soma);
        su_friction(e, 0x18);
        walk_collide(e, layer, 0x10000, -0x200000, 0x40000, 8);
        clamp_to_room(e, layer, 4);
        if (e->anim.frame > 2) {
            e->vx = e->mirrored ? -0x30000 : 0x30000;
            e->step++;
            e->timer = 0;
            e->su_dash = 1;
            e->su_chain = 0;
            su_trails(e, report);
        }
        break;
    case 2:
        su_friction(e, 0x18);
        walk_collide(e, layer, 0x10000, -0x200000, 0x40000, 8);
        clamp_to_room(e, layer, 4);
        if (e->su_chain) {
            e->step = 10;
            e->timer = 0x10;
            e->su_volley = 0;
            e->su_emit = 1;
            e->su_age = 0;
            break;
        }
        if (e->timer++ > 0x1D) {
            play_loop(e, kind->anims, 1, true);
            e->state = 0;
            e->step = 2;
            e->timer = 0;
            e->vy = 0x10000;
            e->ay = 0x1800;
            e->su_dash = 0;
            e->su_volley = 0;
            e->su_emit = 0;
        }
        break;
    case 10: {
        face_player(e, soma);
        walk_collide(e, layer, 0x10000, -0x200000, 0x40000, 8);
        clamp_to_room(e, layer, 4);
        /* The homing pounce: while the + 0xD countdown lives, move a
         * fraction of the remaining distance to Soma (biased by the
         * facing) each tick — the native divides by the countdown. */
        if (e->timer) {
            int32_t bias = e->mirrored ? -0xC0000 : 0xC0000;
            e->x += ((target_x(soma) + bias) - e->x) / e->timer;
            e->y += (target_y(soma) - e->y) / e->timer;
            e->timer--;
        }
        e->su_emit++;
        if (e->su_emit > 0x2C) {
            e->su_emit = 0;
            e->su_volley++;
            if (e->su_volley <= 4) {
                /* sub_08021654 contact record at the body, element 0. */
                if (player_in(soma, su_px(e) - 8, su_py(e) - 8, 16, 16)) {
                    int dmg = aos_soma_take_elemental_hit(
                        soma, e->stats.contact, soma_def, e->x, 0,
                        e->contact_elements);
                    if (dmg > 0) {
                        report->soma_hit = true;
                        report->soma_damage += dmg;
                    }
                }
                aos_audio_play_song(0x7B);
                e->x += e->mirrored ? 0x10000 : -0x10000;
            }
        } else if (e->su_emit == 0xA) {
            e->x += e->mirrored ? -0x10000 : 0x10000;
        }
        /* The native + 0x1E counter runs while the game-state probe
         * (gEwramData + 0x1E & 0x3FF) holds — approximated as set. */
        if (e->su_age <= 0x63)
            e->su_age++;
        /* sub_0801D028(0xE, 0, 0, 0) grab probe: not ported, returns 0. */
        if (e->su_volley > 4 || e->su_age > 0x1D) {
            play(e, kind->anims, 3);
            e->vx = e->mirrored ? -0x28000 : 0x28000;
            e->ax = 0;
            e->vy = 0x10000;
            e->ay = 0x1800;
            e->step++;
            e->timer = 0;
            e->su_try = 1;
            e->su_dash = 0;
            e->su_chain = 0;
            e->su_inactive = 0;
            e->su_age = 0;
        }
        break;
    }
    case 11:
        su_friction(e, 0x18);
        walk_collide(e, layer, 0x10000, -0x200000, 0x40000, 8);
        clamp_to_room(e, layer, 4);
        if (e->timer++ > 0x27) {
            play_loop(e, kind->anims, 1, true);
            e->state = 0;
            e->step = 2;
            e->timer = 0;
            e->vy = 0x10000;
            e->ay = 0x1800;
            e->su_try = 0;
        }
        break;
    default:
        break;                      /* cases 3-9: the shared exit */
    }
}

AosHitReport aos_enemy_succubus_update(AosEnemy *e, AosSoma *soma,
                                       const AosCollision *layer,
                                       const AosEnemyKind *kind,
                                       const AosWeaponEntity *weapon,
                                       const AosWeaponFrames *weapon_frames,
                                       int soma_atk, int soma_def, int cam_x,
                                       int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0 };
    if (e->hit_flash)
        e->su_gate = 1;             /* sub_08094620: + 0x18 = 1 on a hit */
    int sx = su_px(e) - cam_x, sy = su_py(e) - cam_y;
    if (e->state != 2) {
        e->hidden = outside_margins(kind, 4, sx, sy);
        if (!e->defeated && (uint16_t)(sx + 0x10) <= 0x110 &&
            (uint16_t)sy <= 0xB0)
            report.targetable = true;
    } else {
        e->hidden = outside_margins(kind, 4, sx, sy);
    }
    /* The + 0x2D stun gate (sub_0806AD24): a flashing live body runs the
     * contact check alone — state 2 and + 0x1B skip even that. */
    if (e->hit_flash) {
        if (e->state == 2 || e->su_inactive)
            return report;
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        copy_result(&report, &hit);
        return report;
    }
    switch (e->state) {
    case 0:
        su_state0(&report, e, soma, layer, kind, random);
        break;
    case 1:
        su_state1(&report, e, soma, layer, kind, soma_def, random);
        break;
    case 2:
        generic_death(e, kind, random, &report);
        break;
    case 3:
        /* dormant: the native runs only the animation step */
        step_anim(e, kind->anims);
        return report;
    default:
        break;
    }
    step_anim(e, kind->anims);
    if (e->state != 2 && !e->su_inactive) {
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        copy_result(&report, &hit);
    }
    return report;
}
