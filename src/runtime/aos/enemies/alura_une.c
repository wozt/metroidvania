/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow boss: Alura Une (enemy 0x4D) and her scythe projectiles
 * and orbiting blades.
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API.
 *
 * Native layout (cvaos code_080709D8): EnemyAluraUneCreate (0x0807C0A0)
 * loads tiles 0x081CBF9C, palette 0x0820A674 and frames 0x08218D3C
 * through sub_0807CF20, presets its boxes (sub_0803F2C8(0,0,1)), installs
 * the sub_0807CF6C contact and sub_0807C8FC hurt callbacks through
 * sub_080427B8/sub_0804277C (the choice reads sub_0806D260), snaps to the
 * floor (sub_0806AF98) and enters state 3 under the global 0x8E & 0x40
 * flag, else state 0. EnemyAluraUneUpdate (0x0807CF9C) gates on
 * sub_0806CC20 and the sub_08068AD4 activity box (-0x80, -0x40, 0x120,
 * 0x1F0), then dispatches: state 0 sub_0807C150, state 1 sub_0807C494,
 * state 2 the shared sub_0806AEAC generic death, state 3 the
 * sub_0807C888 materialize sequence; the tail is sub_0806DF20 +
 * sub_0806E314.
 *
 * Both fight states run a 14-entry sub-state machine on + 0x0B with a
 * + 0x0D timer and the + 0x59 bit-0 animation-end gate (sub_0806CF2C
 * runs at the handler head). State 0 is the scythe phase: sub-state 0
 * waits for Soma inside the sub_0806CFFC box (x - 0x5A, y - 0x30,
 * 0xB4 x 0x60) or the sub_0806D54C helper, then the + 0x20 seen flag
 * picks the entry; sub-states 10-13 throw scythe volleys through
 * sub_0807C9A4 (four blades arcing off alternating +/- (0xF + i * 0x10)
 * offsets) with the sub_08011F7C screen pulses and the 0x105 room-event
 * calls. State 1 is the blade phase: sub-state 1 throws the
 * sub_0807C9A4 projectiles with kind 0x6E on a random 0x20-0x3F delay,
 * sub-states 3-6 run the long spin with directional pulses, and
 * sub-state 12 watches the + 0x6E frame word (== 3 / == 4) to emit the
 * sub_0807CC9C orbit blades. State 3 materializes through three
 * animation gates.
 *
 * The thrown scythe (sub_0807C9A4 spawn, sub_0807CAD4 update) delays on
 * + 0x0D (0 or random 0x20-0x3F), launches at timer 8 with random
 * sub_0806DCC4 velocities, flies until its + 0x20 life (0x28 or 0x6E)
 * plays animation 0xD, and ends on the parent check (parent + 0x36 ==
 * 0x4D and state != 2); the orbit blade (sub_0807CC9C spawn,
 * sub_0807CDA8 update) sits at +/- 0x80 -/+ side * 6 of the boss, plays
 * song 0x80 on its + 0x6E == 0x100 frame word, then loops a
 * hidden/reveal cycle keyed on the + 0x18 flip counter. Both children
 * mark + 0x59 bit 3 when the boss dies.
 *
 * Approximations: the 0x105 sub_08013D60/sub_08013DA8/sub_08013E18 room
 * events, the sub_08011F7C screen pulses, the sub_0806D54C aggro helper,
 * the sub_0806D260 callback variant, sub_0806B1FC/sub_0806AF98/
 * sub_0806B04C/sub_08042B8C/sub_080421AC/sub_08042848 helpers and the
 * exact sub_0806DCC4 velocity picks are approximated through the shared
 * collision/animation framework or skipped; there is no emulator
 * comparison. */

#include "../../aos/core/aos_enemy_internal.h"
#include "../../aos/core/aos_audio.h"
#include "../../aos/core/aos_collision.h"
#include "../../aos/math/aos_math.h"

#define collide aos_enemy_collide
#define outside_margins aos_enemy_outside_margins
#define play_loop aos_enemy_play_loop
#define play aos_enemy_play
#define step_anim aos_enemy_step_anim
#define generic_death aos_enemy_generic_death
#define merge_hit aos_enemy_merge_hit
#define player_in aos_enemy_player_in
#define player_in_centred aos_enemy_player_in_centred

/* sub_0803F2C8(e, anim, 0, loop): play an animation, loop flag in arg3. */
static void au_anim(AosEnemy *e, const AosEnemyKind *kind, unsigned id,
                    bool loop) {
    if (kind) play_loop(e, kind->anims, id, loop);
}

/* sub_0807C9A4: spawn a scythe child (type 0x1B/0x48, update
 * sub_0807CAD4) at the boss's pixel x + xoff. delay -1 picks the random
 * 0x20-0x3F launch delay and delay 0 launches immediately (state 2,
 * animation 7); life is the + 0x20 flight counter (0x28 or 0x6E). */
static void au_throw_scythe(AosEnemy *e, const AosEnemyKind *kind,
                            AosHitReport *report, int xoff, int delay,
                            uint16_t life, uint32_t (*random)(void)) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy *c = &report->children[report->child_count++];
    *c = (AosEnemy){0};
    c->id = e->id;
    c->role = AOS_ROLE_ALURA_SCYTHE;
    c->x = e->x + ((int32_t)xoff << 16);
    c->y = e->y;
    c->state = delay == 0 ? 2 : 0;
    c->step = 0;
    c->timer = delay < 0 ? (uint8_t)(0x20 + (random() & 0x1F))
                       : (uint8_t)delay;
    c->au_life = life;
    c->parent = e;
    c->mirrored = e->mirrored;
    c->stats = e->stats;
    c->contact_elements = e->contact_elements;
    c->hp = 1;
    if (delay == 0) au_anim(c, kind, 7, false);
}

/* sub_0807CC9C: spawn an orbit blade child (type 0x1B/0x48, update
 * sub_0807CDA8) at +/- 0x80 -/+ side * 6 of the boss; facing carries
 * the parent's + 0x58 bit-6 value and its complement on the two calls. */
static void au_throw_blade(AosEnemy *e, const AosEnemyKind *kind,
                           AosHitReport *report, int side, int facing) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy *c = &report->children[report->child_count++];
    *c = (AosEnemy){0};
    c->id = e->id;
    c->role = AOS_ROLE_ALURA_BLADE;
    int32_t px = e->x >> 16;
    c->x = ((int32_t)(e->mirrored ? px - 0x80 + side * 6
                                  : px + 0x80 - side * 6)) << 16;
    c->y = e->y;
    c->state = 0;
    c->step = 0;
    c->timer = 0;
    c->parent = e;
    c->mirrored = (facing & 1) != 0;
    c->stats = e->stats;
    c->contact_elements = e->contact_elements;
    c->hp = 1;
    au_anim(c, kind, 9, false);
    c->spawned_count = 0;
}

/* The hurt callback sub_0807C8FC folded into the collision pass: the
 * + 0x2D hit flash, the once-only 0x6A song, the + 0x21 reaction owed
 * and the lethal path (state 2, timer 0x28, song 0x6E). */
static void au_hurt(AosEnemy *e, bool killed) {
    e->hit_flash = 8;
    if (!e->au_hurted) {
        e->au_hurted = 1;
        aos_audio_play_song(0x6A);
    }
    e->au_react = 1;
    if (killed) {
        e->state = 2;
        e->step = 0;
        e->timer = 0x28;
        aos_audio_play_song(0x6E);
    }
}

/* State 0 sub_0807C150: the scythe phase. */
static void au_state0(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                      const AosEnemyKind *kind, AosHitReport *report,
                      uint32_t (*random)(void)) {
    (void)layer;
    /* sub_0806CF2C: the animation step raising the + 0x59 bit-0 gate. */
    step_anim(e, kind->anims);
    /* The + 0x58 facing dance copies bit 0 into bit 6; approximated by
     * leaving the framework's mirrored flag alone. */
    if (e->step > 13) return;
    switch (e->step) {
    case 0: {
        /* Aggro: sub_0806CFFC box (x - 0x5A, y - 0x30, 0xB4 x 0x60) or
         * the sub_0806D54C helper (approximated by the centred box). */
        int32_t px = e->x >> 16, py = e->y >> 16;
        bool near = player_in(soma, px - 0x5A, py - 0x30, 0xB4, 0x60) ||
                    player_in_centred(e, soma, 0xB4, 0x60);
        if (!near) return;
        if (!e->au_seen) {
            e->au_seen = 1;
            e->step = 10;
            e->timer = 0;
            return;
        }
        switch (random() % 3) {
        case 0:
            e->step++;
            au_anim(e, kind, 1, false);
            break;
        case 1:
            e->state = 1;
            e->step = 0;
            break;
        default:
            e->state = 1;
            e->step = 10;
            break;
        }
        break;
    }
    case 1:
        if (!e->cycle_done) return;
        e->step++;
        au_anim(e, kind, 3, false);
        break;
    case 2:
        if (!e->cycle_done) return;
        e->step++;
        e->timer = (uint8_t)(0x3C + (random() % 0x3C));
        au_anim(e, kind, 4, false);
        break;
    case 3: {
        if (e->au_react) {
            e->timer = 0;
            e->au_react = 0;
        }
        if (e->timer) {
            e->timer--;
            return;
        }
        switch (random() % 3) {
        case 0:
            e->step++;
            au_anim(e, kind, 0xB, false);
            break;
        case 1:
            e->state = 1;
            e->step = 1;
            break;
        default:
            e->state = 1;
            e->step = 0xB;
            break;
        }
        break;
    }
    case 4:
        if (!e->cycle_done) return;
        au_anim(e, kind, 0xA, false);
        e->step++;
        e->timer = 0;
        break;
    case 5:
        if (!e->cycle_done) return;
        au_anim(e, kind, 0, true);
        e->step++;
        e->timer = 0;
        break;
    case 6:
        if (++e->timer > 0x3B) {
            e->step = 0;
            e->timer = 0;
        }
        break;
    case 10:
        e->step++;
        e->timer = 0;
        /* Four scythe blades arcing off alternating offsets. */
        for (int i = 0; i < 4; ++i) {
            au_throw_scythe(e, kind, report,
                            (int)(random() & 0xF) + i * 0x10, 0, 0x28,
                            random);
            au_throw_scythe(e, kind, report,
                            -((int)(random() & 0xF) + i * 0x10), 0,
                            0x28, random);
        }
        break;
    case 11:
        /* The sub_08011F7C screen pulse every eighth tick and the 0x105
         * room event are not ported. */
        au_anim(e, kind, 1, false);
        e->step++;
        e->timer = 0;
        break;
    case 12:
        if ((e->timer & 3) == 0) {
            /* sub_08011F7C(0, r4, r2) random directional pulse. */
        }
        if (e->cycle_done) {
            e->step++;
            e->timer = 0;
            au_anim(e, kind, 2, true);
        }
        break;
    case 13:
        if ((e->timer & 3) == 0) {
            /* sub_08011F7C(0, r4, r2) random directional pulse. */
        }
        e->timer++;
        if (e->timer <= 0x1D) return;
        e->state = 0;
        e->step = 2;
        e->timer = 0;
        au_anim(e, kind, 3, false);
        break;
    default:
        break;
    }
}

/* State 1 sub_0807C494: the blade phase. cam_x feeds the
 * sub_0806D128(e, 4) margin checks of the long spin's exit. */
static void au_state1(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                      const AosEnemyKind *kind, AosHitReport *report,
                      int cam_x, uint32_t (*random)(void)) {
    (void)soma;
    (void)layer;
    step_anim(e, kind->anims);
    if (e->step > 13) return;
    switch (e->step) {
    case 0:
        au_anim(e, kind, 1, false);
        e->step++;
        break;
    case 1:
        if (!e->cycle_done) return;
        {
            int skip_a = (int)(random() % 6), skip_b = (int)(random() % 6);
            for (int i = 0; i <= 6; ++i) {
                if (i == skip_a || i == skip_b) continue;
                au_throw_scythe(e, kind, report, 0x28 + i * 0x10, -1,
                                0x6E, random);
            }
            skip_a = (int)(random() % 6);
            skip_b = (int)(random() % 6);
            for (int i = 0; i <= 6; ++i) {
                if (i == skip_a || i == skip_b) continue;
                au_throw_scythe(e, kind, report, -0x28 - i * 0x10, -1,
                                0x6E, random);
            }
        }
        e->step++;
        e->timer = 0;
        au_anim(e, kind, 3, false);
        break;
    case 2:
        e->timer++;
        if ((e->timer & 3) == 0) {
            /* sub_08013DA8(0x105) room event not ported. */
        }
        if (!e->cycle_done) return;
        au_anim(e, kind, 4, true);
        e->step++;
        e->timer = 0;
        break;
    case 3:
        e->step++;
        /* fall through: native _0807C620 increments the sub-state then
         * runs the case-4 body on the same tick. */
        /* fallthrough */
    case 4: {
        if ((e->timer & 3) == 0) {
            /* sub_08013DA8(0x105) room event not ported. */
        }
        uint8_t t = e->timer;
        if (t > 0x1D) {
            if (t <= 0x5A) {
                /* sub_08011F7C(0, 0, 1) / (0, 0, 0) every eighth tick. */
            } else {
                if (t == 0x6E) au_anim(e, kind, 2, true);
                if ((t & 3) == 0) {
                    /* sub_08011F7C(0, r4, r2) random directional pulse. */
                }
            }
        }
        e->timer++;
        if (e->timer <= 0xFC) break;
        /* sub_08013E18(0x105) + sub_0806D128(e, 4): off-screen resets
         * the fight, on-screen picks the next pattern. */
        {
            int sx = (int)(e->x >> 16) - cam_x;
            int sy = (int)(e->y >> 16);
            if (outside_margins(kind, 4, sx, sy)) {
                e->state = 0;
                e->step = 0;
                au_anim(e, kind, 0, true);
                break;
            }
        }
        switch (random() % 3) {
        case 0:
            e->step = 10;
            au_anim(e, kind, 0xA, false);
            break;
        case 1:
            e->state = 0;
            e->step = 2;
            au_anim(e, kind, 3, false);
            break;
        default:
            au_anim(e, kind, 0xB, false);
            e->step = 0xB;
            break;
        }
        break;
    }
    case 5:
        if (!e->cycle_done) return;
        au_anim(e, kind, 0, true);
        e->step++;
        e->timer = 0;
        break;
    case 6:
        e->timer++;
        if (e->timer > 0x59) {
            e->state = 0;
            e->step = 0;
            e->timer = 0;
        }
        break;
    case 10:
        au_anim(e, kind, 1, false);
        e->step++;
        break;
    case 11:
        if (!e->cycle_done) return;
        e->au_react = 0;
        e->step++;
        e->timer = 0;
        au_anim(e, kind, 5, false);
        break;
    case 12:
        if (e->cycle_done) {
            e->step++;
            e->timer = 0;
            au_anim(e, kind, 6, true);
            break;
        }
        /* The + 0x6E frame word gates the orbit blade spawn: native
         * ldrh == 3 (side 0) / == 4 (side 1), two blades per trigger
         * with the facing bit and its complement. The frame edge is
         * tracked on au_prev_frame (the native tick-0 halfword). */
        if ((e->anim.frame == 3 || e->anim.frame == 4) &&
            e->au_prev_frame != e->anim.frame) {
            int side = e->anim.frame == 3 ? 0 : 1;
            int face = e->mirrored ? 1 : 0;
            au_throw_blade(e, kind, report, side, face);
            au_throw_blade(e, kind, report, side, face ^ 1);
        }
        e->au_prev_frame = e->anim.frame;
        break;
    case 13:
        e->timer++;
        if (e->timer <= 0xEF) break;
        /* sub_0806D128(e, 4) margin check + sub_08013E18(0x105). */
        {
            int sx = (int)(e->x >> 16) - cam_x;
            int sy = (int)(e->y >> 16);
            if (outside_margins(kind, 4, sx, sy)) {
                e->state = 0;
                e->step = 0;
                e->timer = 0;
                au_anim(e, kind, 0, true);
                break;
            }
        }
        switch (random() % 3) {
        case 0:
            e->step = 5;
            e->timer = 0;
            au_anim(e, kind, 0xA, false);
            break;
        case 1:
            e->state = 0;
            e->step = 2;
            au_anim(e, kind, 3, false);
            break;
        default:
            au_anim(e, kind, 0xB, false);
            e->step = 0xB;
            break;
        }
        break;
    default:
        break;
    }
}

/* State 3 sub_0807C888: the materialize sequence. */
static void au_state3(AosEnemy *e, const AosEnemyKind *kind) {
    switch (e->step) {
    case 0:
        if (++e->timer > 0x3B) {
            e->step = 1;
            au_anim(e, kind, 1, false);
        }
        break;
    case 1:
        if (!e->cycle_done) break;
        e->step = 2;
        au_anim(e, kind, 3, false);
        break;
    case 2:
        if (!e->cycle_done) break;
        e->step = 3;
        au_anim(e, kind, 4, true);
        break;
    default:
        break;
    }
}

AosHitReport aos_enemy_alura_une_update(
    AosEnemy *e, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    int cam_x, int cam_y, uint32_t (*random)(void)) {
    (void)weapon_frames;
    (void)cam_y;
    AosHitReport report = {0};
    switch (e->state) {
    case 0:
        au_state0(e, soma, layer, kind, &report, random);
        break;
    case 1:
        au_state1(e, soma, layer, kind, &report, cam_x, random);
        break;
    case 2:
        generic_death(e, kind, random, &report);
        return report;
    case 3:
        au_state3(e, kind);
        step_anim(e, kind->anims);
        return report;
    default:
        break;
    }
    /* The state handlers already ran sub_0806CF2C at their head; the
     * sub_0806DF20/sub_0806E314 tail maps to the shared collide pass. */
    if (e->state != 2) {
        int sx = (int)(e->x >> 16) - cam_x, sy = (int)(e->y >> 16) - cam_y;
        if (!outside_margins(kind, 4, sx, sy)) {
            AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                                       soma_atk, soma_def);
            merge_hit(&report, hit);
            if (hit.enemy_hit) au_hurt(e, hit.killed);
        }
    }
    return report;
}

/* The native parent liveness check of sub_0807CAD4/sub_0807CDA8:
 * parent + 0x36 == 0x4D and state != 2. */
static bool au_parent_gone(const AosEnemy *c) {
    const AosEnemy *p = c->parent;
    return !p || p->removed || p->id != AOS_ENEMY_ALURA_UNE || p->state == 2;
}

/* sub_0807CAD4: the thrown scythe, five states on + 0x0A. */
AosHitReport aos_enemy_alura_scythe_update(
    AosEnemy *e, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    int cam_x, int cam_y, uint32_t (*random)(void)) {
    (void)weapon_frames;
    (void)soma_atk;
    (void)soma_def;
    (void)cam_y;
    (void)layer;
    AosHitReport report = {0};
    step_anim(e, kind->anims);
    if (e->au_blade_flags & 8) {
        e->removed = true;
        return report;
    }
    switch (e->state) {
    case 0:
        if ((int8_t)--e->timer <= 0) {
            au_anim(e, kind, 0xC, false);
            e->mirrored = false;
            e->hidden = false;
            e->state = 1;
        }
        break;
    case 1:
        if (!e->cycle_done) break;
        if (!au_parent_gone(e)) {
            e->state = 2;
            e->timer = 0;
            au_anim(e, kind, 7, false);
        } else {
            e->au_blade_flags |= 8;
        }
        break;
    case 2:
        if (e->timer == 8) {
            /* sub_0806DCC4 random launch velocities in 16.16: vx in
             * [-1, +2) px/frame, vy in [-3, -2) px/frame. */
            e->vx = (int32_t)(random() & 0x1FFFF) - 0x10000;
            e->vy = 0xFFFE0000 - (int32_t)(random() & 0x1FFFF);
        }
        e->timer++;
        e->x += e->vx;
        e->y += e->vy;
        if (e->cycle_done) {
            e->state = 3;
            e->timer = 0;
            au_anim(e, kind, 8, true);
            e->attack_off = false;
            e->au_blade_flags = (e->au_blade_flags & ~2) | 4;
        }
        break;
    case 3:
        /* sub_080421AC box query is approximated away; the + 0x20 life
         * only swaps to animation 0xD, the state ends on the parent
         * check alone. */
        e->timer++;
        if (e->timer >= (int16_t)e->au_life) au_anim(e, kind, 0xD, false);
        if (au_parent_gone(e)) e->state = 4;
        break;
    case 4:
        if (e->cycle_done) e->removed = true;
        break;
    default:
        e->removed = true;
        break;
    }
    {
        int sx = (int)(e->x >> 16) - cam_x, sy = (int)(e->y >> 16) - cam_y;
        if (!outside_margins(kind, 4, sx, sy)) {
            AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                                       soma_atk, soma_def);
            merge_hit(&report, hit);
        }
    }
    return report;
}

/* sub_0807CDA8: the orbit blade, two states on + 0x0A. */
AosHitReport aos_enemy_alura_blade_update(
    AosEnemy *e, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    int cam_x, int cam_y, uint32_t (*random)(void)) {
    (void)weapon_frames;
    (void)soma_atk;
    (void)soma_def;
    (void)cam_y;
    (void)layer;
    (void)random;
    AosHitReport report = {0};
    step_anim(e, kind->anims);
    if (e->au_blade_flags & 8) {
        e->removed = true;
        return report;
    }
    bool parent_gone = au_parent_gone(e);
    switch (e->state) {
    case 0:
        /* The + 0x6E frame word == 0x100 (frame 0, tick 1) plays song
         * 0x80 once. */
        if (e->anim.frame == 0 && e->anim.tick == 1 &&
            !(e->au_blade_flags & 0x10)) {
            e->au_blade_flags |= 0x10;
            aos_audio_play_song(0x80);
        }
        if (e->cycle_done) {
            e->state = 1;
            if (++e->spawned_count == 3 || e->spawned_count == 4) {
                e->mirrored = !e->mirrored;
                int32_t px = e->x >> 16;
                e->x = ((int32_t)(px + (e->mirrored ? -0x28 : 0x28))) << 16;
            }
            e->hidden = true;
            e->timer = 0;
            e->attack_off = false;
        }
        if (parent_gone) e->au_blade_flags |= 8;
        break;
    case 1:
        if (++e->timer > 5) {
            au_anim(e, kind, 9, false);
            e->hidden = false;
            e->state = 0;
            e->au_blade_flags &= ~2;
        }
        if (parent_gone) e->au_blade_flags |= 8;
        break;
    default:
        e->removed = true;
        break;
    }
    {
        int sx = (int)(e->x >> 16) - cam_x, sy = (int)(e->y >> 16) - cam_y;
        if (!outside_margins(kind, 4, sx, sy)) {
            AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                                       soma_atk, soma_def);
            merge_hit(&report, hit);
        }
    }
    return report;
}
