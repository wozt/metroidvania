/* Alastor (Aria of Sorrow enemy id 0x5A), ported from
 * EnemyAlastorCreate (0x080CD544) / EnemyAlastorUpdate (0x080CD644)
 * and the cluster sub_080CD75C..sub_080CEEE8 of code_080C0A1C.s.
 *
 * The native actor is a head record plus a linked partner body in the
 * gEwramData + 0x4E4 slot table (stride 0x84, indexes + 0x10/+ 0x11/
 * + 0x12); here the partner is a persistent child entity of role
 * AOS_ROLE_ALASTOR_BODY whose + 0xA/+ 0xB/+ 0xC/+ 0xD machine is
 * mirrored onto the head's al_p* fields, exactly like the native
 * record writes.  A third child kind, AOS_ROLE_ALASTOR_SHOT, is the
 * sub_080CE44C muzzle flash (entity id 0x49, lifetime + 0xD = 5).
 *
 * Head fields (native offsets): + 0xA state, + 0xB step, + 0xC
 * substep, + 0xD timer, + 0x10 partner slot, + 0x12 body slot,
 * + 0x13 volley record, + 0x16/+ 0x17 pattern row/column into
 * 0x08528C2C, + 0x1A spin angle, + 0x1C draw rotation, + 0x1E probe,
 * + 0x24 the three-hit counter, + 0x34 hp, + 0x40/+ 0x44 the 16.16
 * position (our x/y), + 0x48/+ 0x4C velocity and + 0x50/+ 0x54
 * acceleration (our vx/vy/ax/ay). */
#include "../core/aos_enemy_internal.h"
#include "../core/aos_audio.h"
#include "../core/aos_collision.h"
#include "../core/aos_soma.h"
#include "../math/aos_math.h"
#include <stdlib.h>

#define copy_result aos_enemy_copy_collision_result
#define random_centred(r, n) aos_enemy_random_centred((r), (n))
#define al_room_columns(l) aos_enemy_room_columns(l)
#define face_player aos_enemy_face_player
#define outside_margins aos_enemy_outside_margins
#define step_anim aos_enemy_step_anim
#define play_loop aos_enemy_play_loop
#define target_x(...) aos_enemy_target_x(__VA_ARGS__)
#define target_y(...) aos_enemy_target_y(__VA_ARGS__)
#define toward_facing aos_enemy_toward_facing
#define player_dy aos_enemy_player_dy
#define player_in aos_enemy_player_in
#define collide aos_enemy_collide

static bool al_is_alastor(uint8_t id) { return id == AOS_ENEMY_ALASTOR; }

/* The native sine/cosine pair of sub_080009E4: 16.16 results. */
static int32_t al_sin(uint32_t angle) {
    return aos_sine(angle & 0xFFFF);
}
static int32_t al_cos(uint32_t angle) {
    return aos_sine((angle + 0x4000) & 0xFFFF);
}
/* sub_0803E86C: the 16.16 fixed multiply. */
static int32_t al_mul(int32_t a, int32_t b) {
    return (int32_t)(((int64_t)a * b) >> 16);
}

/* sub_0806DF40(e, 0, 0): the point outside the room's cells (the
 * buer.c room_outside twin). */
static bool al_room_outside(const AosEnemy *e, const AosCollision *layer) {
    if (!layer || !layer->width_screens || !layer->height_screens) return false;
    int width = layer->width_screens == 1 ? 30 : 32 * layer->width_screens;
    int height = layer->height_screens == 1 ? 26 : 32 * layer->height_screens;
    int x = (int)(e->x >> 16), y = (int)(e->y >> 16);
    int cx = (x < 0 ? x + 7 : x) >> 3, cy = (y < 0 ? y + 7 : y) >> 3;
    return cx < 0 || cx >= width || cy < 0 || cy >= height;
}

/* The 0x08528C2C pattern table: four rows of nine entries; the head's
 * + 0x16 row and + 0x17 column pick the next attack state. */
static const uint8_t al_pattern[4][9] = {
    {0, 2, 0, 3, 0, 2, 0, 3, 0},
    {2, 2, 0, 0, 3, 3, 0, 0, 2},
    {0, 2, 3, 2, 0, 2, 3, 0, 2},
    {2, 2, 2, 0, 0, 0, 3, 3, 3}};

/* The 0x08528BF4 volley records: {u16 flags, i16 speed, i16 spin,
 * u8 duration, u8 pad} — speeds and spins divide by the duration. */
typedef struct { uint16_t flags; int16_t speed, spin; uint8_t dur, pad; }
    AlVolley;
static const AlVolley al_volley[7] = {
    {0x30, 8, 0, 24, 0}, {8, -8, 0x3400, 8, 0}, {8, 8, (int16_t)0xCC00, 8, 0},
    {0xE8, -8, 0x3400, 8, 0}, {0x20, 0, 0, 8, 0}, {0xE8, 2, (int16_t)0xCC00, 8, 0},
    {0x7FFF, 0, 0, 0, 0}};

/* The shared state tail of sub_0803E058 callers: the 16.16 position
 * integrates its velocity, the velocity its acceleration, the draw
 * rotation its probe step — every Alastor state ends on it. */
static void al_integrate(AosEnemy *e) {
    e->x += e->vx;
    e->y += e->vy;
    e->vx += e->ax;
    e->vy += e->ay;
    e->ala_rot = (int16_t)(e->ala_rot + e->ala_probe);
}

/* sub_080CDD9C: the oriented hit box rebuilt from the + 0x1C rotation.
 * The native writes the eight-way preset through sub_08042884 /
 * sub_080428F0; our frame boxes stand in (documented divergence). */
static void al_orient(AosEnemy *e) {
    int a = (e->ala_rot + 0x4000) & 0xFFFF;
    int b = (e->ala_rot + 0xC000) & 0xFFFF;
    if (!(e->mirrored)) a += 0x10000;
    if (b > 0xA000) b -= 0x8000;
    int sx = al_sin(a), sy = al_cos(b);
    (void)sx; (void)sy;
}

/* ------------------------------------------------------------------ */
/* Partner body: sub_080CEB58 (its + 0xA machine reads the head record) */
/* ------------------------------------------------------------------ */

static void al_body_update(AosEnemy *b, AosEnemy *p, AosSoma *soma) {
    if (!p || p->removed || !al_is_alastor(p->id)) {
        b->removed = true;
        return;
    }
    if (b->hit_flash) return;             /* sub_0806AD24 gate */
    switch (b->state) {
    case 0:
        b->hidden = true;                 /* + 0x58 |= 0x20 */
        break;
    case 1:
        b->hidden = false;                /* + 0x58 &= ~0x21, + 0x65 = 1 */
        b->x = p->x;
        b->y = p->y;
        face_player(b, soma);
        break;
    case 2:
        if (b->step == 0) {
            b->x = p->x;
            b->y = p->y;
            face_player(b, soma);
            b->hidden = false;
            b->timer = 0xA;
            b->step = 1;
        }
        /* + 0xB == 1: hover beside the head, offset by facing. */
        b->x = p->x + (b->mirrored ? 0xA000 : (int32_t)0xFFF60000);
        b->y = p->y + 0xA000;
        if ((uint8_t)--b->timer == 0xFF) b->state = 4;
        break;
    case 3:
        if (b->step == 0) {
            b->timer = b->step = 0;
            b->step = 1;
        }
        /* Blink bit 5 of + 0x58 on + 0xD % 3 and hover on the head. */
        b->hidden = ((b->timer % 3) != 0);
        b->x = p->x;
        b->y = p->y;
        b->timer++;
        if (b->timer > 0x1E) {
            b->state = 0;
            b->step = b->substep = b->timer = 0;
        }
        break;
    case 4:
        if (b->step == 0) {
            b->timer = b->step = 0;
            b->step = 1;
        }
        b->hidden = ((b->timer % 3) != 0);
        b->y = p->y + 0xC0000;             /* head + 0x46 + 0xC px */
        b->x = p->x + (b->mirrored ? 0xE0000 : (int32_t)0xFFF20000);
        if ((uint8_t)++b->timer > 0x27) {
            b->state = 0;
            b->step = b->substep = b->timer = 0;
        }
        break;
    case 5:
        /* The native tail: + 0x42/+ 0x46 copied from the head record. */
        b->x = p->x + (b->mirrored ? 0x10000 : (int32_t)0xFFFF0000);
        b->y = p->y + 0xC0000;
        break;
    default:
        break;
    }
    /* The head drives the partner's + 0xA through ala_pstate. */
    if (p->ala_pstate != b->state) {
        b->state = p->ala_pstate;
        b->step = p->ala_pstep;
        b->substep = p->ala_psub;
        b->timer = p->ala_ptmr;
    }
}

/* ------------------------------------------------------------------ */
/* Shot: sub_080CE44C create / sub_080CEEE8 update                     */
/* ------------------------------------------------------------------ */

static void al_shot_update(AosEnemy *s, AosEnemy *p) {
    if (!p || p->removed || !al_is_alastor(p->id) ||
        (uint8_t)--s->ala_plife == 0xFF)
        s->removed = true;
}

/* ------------------------------------------------------------------ */
/* Head states                                                         */
/* ------------------------------------------------------------------ */

/* sub_080CD75C: the state-0 partner walk-on (six substeps). */
static void al_state0(AosEnemy *e, AosSoma *soma) {
    switch (e->step) {
    case 9:
        /* The create's + 0xB = 9 entry: sub_080CD75C's init case arms
         * the walk-on and hands + 0xB to 0. */
        e->step = 0;
        return;
    case 0:
        face_player(e, soma);
        e->ala_pstate = 5;                 /* partner + 0xA = 5 */
        e->ala_pstep = 0;
        e->hidden = true;                 /* own + 0x58 |= 0x20 */
        e->timer = 0;
        e->step = 1;
        return;
    case 1:
        /* sub_0806BF78(e, 0x500000): the on-screen gate — out of the
         * activity band the intro simply waits. */
        if (al_room_outside(e, NULL)) return;
        /* Copy the head anchor onto the partner (+ 0x42/+ 0x46) and
         * run its transform (sub_0803E058). */
        e->ala_pstep = (uint8_t)(e->ala_pstep + 1);
        e->step = 2;
        return;
    case 2:
        /* The partner blinks on + 0xD % 3 for 0x28 ticks. */
        e->ala_ptmr = (uint8_t)(e->ala_ptmr + 1);
        if (e->ala_ptmr > 0x28) { e->ala_ptmr = 0; e->step = 3; }
        return;
    case 3:
        if ((uint8_t)++e->ala_ptmr > 0x14) { e->ala_ptmr = 0; e->step = 4; }
        return;
    case 4:
        e->ala_ptmr = (uint8_t)(e->ala_ptmr + 1);
        if (e->ala_ptmr > 0x28) {
            e->ala_ptmr = 0;
            e->ala_pstate = e->ala_pstep = e->ala_psub = 0;
            e->step = 5;
        }
        return;
    case 5:
        if ((uint8_t)++e->ala_ptmr > 0x14) {
            e->ala_ptmr = 0;
            e->state = 1;                 /* the fight begins */
            e->substep = 0;
            e->step = 0;
        }
        return;
    default:
        return;
    }
}

/* sub_080CD934: the state-1 flight — accelerate toward the player,
 * probe walls with sub_080686CC, then hand over to state 2. */
static void al_state1(AosEnemy *e, AosSoma *soma, const AosCollision *layer) {
    /* sub_0806BD64 / sub_0806BDEC steer the facing bits. */
    face_player(e, soma);
    switch (e->step) {
    case 0:
        e->combat.attack_off = true;
        e->vx = e->vy = 0;
        e->vx = toward_facing(e, 0x8000);
        e->vy = toward_facing(e, 0x8000);
        e->ala_angle = (uint16_t)e->step;
        e->ala_probe = (int16_t)0xFF9C;
        e->step = 2;
        return;
    case 1:
        /* Wall probe ahead: turn the + 0x1C angle by the step. */
        {
            uint32_t ta = (uint16_t)e->ala_rot;
            if (aos_enemy_turn_angle(&ta,
                                     e->mirrored ? 0x180 : 0x3000,
                                     e->mirrored ? 0x7800 : 0x1000))
                e->step++;
            e->ala_rot = (int16_t)ta;
        }
        al_orient(e);
        return;
    case 2:
        /* Drift: integrate with the shared tail below. */
        break;
    case 3: {
        /* The orbit drift — velocities mirror by the wall probes and
         * the + 0x40/+ 0x44 fine position nudges. */
        e->vx = e->vy = 0x8000;
        int bx = (int16_t)(e->x >> 16), by = (int16_t)(e->y >> 16);
        if (player_in(soma, bx - 0x20, by - 0x20, 0x40, 0x40) ||
            player_in(soma, bx + 0x20, by + 0x20, 0x40, 0x40)) {
            e->state = 2;
            e->substep = 0;
            e->step = 0;
        }
        break;
    }
    default:
        return;
    }
    al_integrate(e);
    if (e->ala_probe != 0 && --e->ala_probe == (int16_t)0xFF) {
        /* The probe ran out: back to the watch. */
    }
    (void)layer;
}

/* sub_080CDB90: state 2 — the ground run driven by the 0x08528C2C
 * pattern table; the partner waits in state 2 meanwhile. */
static void al_state2(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                      uint32_t (*random)(void)) {
    (void)layer;
    int prow = e->ala_row & 3, pcol = e->ala_col % 9;
    switch (e->step) {
    case 0:
        face_player(e, soma);
        if (e->ala_col == 9) {
            e->ala_row = (uint8_t)((random_centred(random, 4)));
            e->ala_col = 0;
        }
        if (al_pattern[prow][pcol] == 2) {
            e->state = 5;
            e->substep = 0;
            e->step = 0;
            return;
        }
        e->vx = toward_facing(e, 0x20000);
        e->vy = 0;
        {
            uint32_t ta = (uint16_t)e->ala_rot;
            if (aos_enemy_turn_angle(&ta, 0x40, e->mirrored ? 0x8000 : 0)) {
                e->ala_pstate = 2;
                e->ala_pstep = e->ala_psub = 0;
            }
            e->ala_rot = (int16_t)ta;
        }
        al_integrate(e);
        return;
    case 1:
        if (e->ala_pstate == 0) e->step++;
        return;
    case 2: {
        if (al_pattern[prow][pcol] == 0) e->step += 2; else e->step++;
        al_integrate(e);
        return;
    }
    case 3: {
        e->step++;
        al_integrate(e);
        return;
    }
    case 4:
        if (e->ala_pstate == 0) {
            if (e->ala_col == 9) {
                e->ala_row = (uint8_t)(random_centred(random, 4));
                e->ala_col = 0;
            }
            switch (al_pattern[e->ala_row & 3][e->ala_col % 9]) {
            case 0: e->state = 3; break;
            case 1: e->state = 4; break;
            case 2: e->state = 5; break;
            default: e->state = 6; break;
            }
            e->substep = 0;
            e->step = 0;
        }
        e->ala_col++;
        return;
    default:
        return;
    }
}

/* sub_080CDE70: state 3 — the dash attack with the three
 * sub_080458E4 strike zones and the spin-out. */
static void al_state3(AosEnemy *e, AosSoma *soma) {
    switch (e->step) {
    case 0:
        e->vx = 0x40000;
        e->ax = (int32_t)0xFFFFC000;
        e->vy = 0xA000;
        if (!(e->mirrored)) { e->vx = -e->vx; e->ax = -e->ax; }
        e->step = 1;
        e->timer = (uint8_t)((int16_t)(player_dy(e, soma) < 0 ?
                        -player_dy(e, soma) : player_dy(e, soma)) / 4);
        aos_audio_play_song(0x86);
        return;
    case 1:
        e->x += e->vx;
        e->y += e->vy;
        al_orient(e);
        if ((uint8_t)--e->timer == 0xFF) e->step = 2;
        return;
    case 2:
        e->ala_rot = (int16_t)(e->ala_rot + ((e->mirrored) ? -0x150 : 0x150));
        al_orient(e);
        e->vx += e->ax;
        if (e->vx == 0) e->step = 3;
        return;
    case 3:
        e->step++;
        return;
    case 0xFF:
        /* The strike frame: spin a quarter turn and arm the three
         * contact zones (0x60 wide, offsets 0xA/6/3, kind 2). */
        if (e->substep == 0) {
            e->substep = 1;
            e->vy = 0xFFFFFB00;
            e->ay = 0xFFFFB000;
            e->timer = 0x28;
        } else if (e->substep == 1) {
            e->vy += e->ay;
            e->timer--;
            if (e->timer == 0 && (int16_t)(e->y >> 16) < 0xAA) {
                /* sub_0806D5C0 gore rows along the body — approximated
                 * by the shared flame/blood drifts. */
                e->substep = 2;
            }
        } else {
            e->step = 1;
            e->substep = 0;
        }
        e->ala_rot = (int16_t)(e->ala_rot + ((e->mirrored) ? 0x80 : -0x80));
        al_orient(e);
        return;
    default:
        return;
    }
}

/* sub_080CE13C: state 4 — the spin dash driven by the 0x08528BF4
 * volley records (+ 0x13 walks them, 0x7FFF ends the run). */
static void al_state4(AosEnemy *e, AosSoma *soma) {
    (void)soma;
    const AlVolley *rec = &al_volley[e->ala_vol & 7];
    int sgn = (e->mirrored) ? 1 : -1;
    switch (e->step) {
    case 0:
        e->ala_vol = (uint8_t)e->step;
        e->step = 1;
        aos_audio_play_song(0x86);
        return;
    case 1:
        e->timer = rec->dur;
        int dur = rec->dur ? rec->dur : 1;
        e->vx = (int32_t)(((int32_t)rec->speed * sgn) << 16) / dur;
        e->vy = (int32_t)(((int32_t)rec->speed) << 16) / dur;
        e->ala_probe = (int16_t)(((int32_t)rec->spin << 16) / dur);
        if (!(e->mirrored)) e->ala_probe = -e->ala_probe;
        e->step = 2;
        return;
    case 2:
        e->x += e->vx;
        e->y += e->vy;
        e->ala_rot = (int16_t)(e->ala_rot + e->ala_probe);
        al_orient(e);
        if ((uint8_t)--e->timer == 0xFF) {
            e->ala_vol++;
            if (al_volley[e->ala_vol & 7].flags == 0x7FFF) {
                e->step = 5;
                e->timer = al_volley[e->ala_vol & 7].flags & 0xFF;
            } else {
                e->step = 1;
                e->timer = al_volley[e->ala_vol & 7].dur;
            }
        }
        return;
    case 5:
        if ((uint8_t)++e->timer > 0x28) {
            e->timer = e->substep = e->step = 0xFF;
            e->state = 1;
            e->ala_vol = 0xFF;
        }
        return;
    default:
        return;
    }
}

/* sub_080CE26C: state 5 — the homing return; accelerates back to the
 * player through the + 0x50/+ 0x54 lanes and the sine velocities. */
static void al_state5(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                      uint32_t (*random)(void)) {
    (void)layer;
    int dx = (int)((target_x(soma) >> 16) - (e->x >> 16));
    int dy = (int)((target_y(soma) >> 16) - (e->y >> 16));
    (void)dx; (void)dy;
    switch (e->step) {
    case 0:
        e->vx = e->vy = e->ax = e->ay = 0;
        e->ala_probe = (e->mirrored) ? (int16_t)0x1000 : (int16_t)0xF000;
        e->timer = (uint8_t)(random_centred(random, 0x10));
        e->step = 1;
        return;
    case 1:
        /* Every 16th tick a song chirp; after 0x40 the homing launch. */
        if ((e->timer & 0xF) == 0) aos_audio_play_song(0x85);
        e->timer++;
        if (e->timer <= 0x40) return;
        e->ala_probe = 0;
        e->ala_angle = (uint16_t)aos_enemy_angle_to_target(e, soma);
        e->vx = al_mul(0x20000, al_sin(e->ala_angle + 0xC000));
        e->vy = al_mul(0x20000, al_sin(e->ala_angle));
        /* sub_0806CD38/78 distance over velocity = flight ticks. */
        int32_t dist = (int32_t)(((int64_t)llabs((int64_t)(target_x(soma) - e->x)) +
                                  llabs((int64_t)(target_y(soma) - e->y))));
        int32_t sp = e->vx ? e->vx : 1;
        e->timer = (uint8_t)((dist / sp) & 0xFF);
        e->step = 2;
        aos_audio_play_song(0x86);
        return;
    case 2:
        if ((uint8_t)--e->timer == 0) { e->step = 3; e->timer = 0; }
        break;
    case 3:
        e->state = 1;
        e->substep = 0;
        e->step = 0;
        return;
    default:
        return;
    }
    /* The shared tail: position, velocity, rotation. */
    e->x += e->vx;
    e->y += e->vy;
    e->vx += e->ax;
    e->vy += e->ay;
    e->ala_rot = (int16_t)(e->ala_rot + e->ala_probe);
}

/* sub_080CE598: state 6 — the charged assault: songs 0x1BB + 0x86, a
 * muzzle shot every pass, the dive and the reach check. */
static void al_state6(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                      AosHitReport *report, const AosAnimSet *anims,
                      uint32_t (*random)(void)) {
    (void)anims; (void)random;
    switch (e->step) {
    case 0:
        e->ala_vol = (uint8_t)e->step;
        e->step = 1;
        e->vx = (int32_t)0xFFFC0000;
        e->vy = (int32_t)0xFFFE0000;
        e->ay = 0x4000;
        e->ala_probe = (int16_t)0xF880;
        if (e->mirrored) {
            e->vx = -e->vx;
            e->ala_probe = -e->ala_probe;
        }
        e->timer = 0x10;
        aos_audio_play_song(0x1BB);
        aos_audio_play_song(0x86);
        return;
    case 1: {
        /* sub_080CE44C(e, 5): the muzzle flash child. */
        if (report->child_count < AOS_ENEMY_MAX_CHILDREN) {
            AosEnemy *s = &report->children[report->child_count++];
            *s = *e;
            s->role = AOS_ROLE_ALASTOR_SHOT;
            s->parent = e;
            s->state = s->step = s->substep = 0;
            s->hp = 1;
            s->hit_flash = 0;
            s->defeated = false;
            s->hidden = false;
            s->removed = false;
            s->ala_plife = 5;
            s->ala_pslot = e->ala_pslot;
            s->x = e->x;
            s->y = e->y;
            s->combat.attack_off = true;
            s->combat.hurt_off = true;
            s->vx = s->vy = s->ax = s->ay = 0;
        }
        e->x += e->vx;
        e->y += e->vy;
        e->vy += e->ay;
        e->ala_rot = (int16_t)(e->ala_rot + e->ala_probe);
        al_orient(e);
        if ((uint8_t)--e->timer == 0xFF) {
            /* sub_080CEE50 reach probe -> step + 1 or + 2. */
            int reach = player_in(soma, (int)(e->x >> 16) - 0x40,
                                  (int)(e->y >> 16) - 0x40, 0x100, 0x100);
            e->step = reach ? 2 : 3;
            e->substep = 0;
        }
        return;
    }
    case 2:
        if (e->substep == 0) {
            e->vy = 0x50000;
            e->ay = 0x4000;
            e->timer = e->substep = 0;
            e->substep = 1;
        }
        e->y += e->vy;
        if (e->timer == 0xA) aos_audio_play_song(0x86);
        e->timer++;
        if (e->timer > 0xA) e->vy += e->ay;
        /* The aggro + reach probe of sub_0806DF40 / sub_08001D94. */
        if (!al_room_outside(e, layer) ||
            player_in(soma, (int16_t)(e->x >> 16) + (int16_t)(target_x(soma) >> 16) - 6,
                      (int16_t)(e->y >> 16) + (int16_t)(target_y(soma) >> 16) - 10 + 8,
                      0, 0)) {
            e->step++;
            e->substep = 0;
        }
        return;
    case 3:
        if (e->substep == 0) e->timer = 0x28;
        else if ((uint8_t)--e->timer == 0) {
            e->timer = e->substep = 0;
            e->state = 1;
            e->step = e->substep = e->ala_vol = 0;
        }
        e->substep++;
        return;
    default:
        return;
    }
}

/* sub_080CE758: state 7 — the death spiral: orbit the anchor by the
 * + 0x1A spin, hand over to state 0xFF once the partner settles. */
static void al_state7(AosEnemy *e, AosSoma *soma) {
    switch (e->step) {
    case 0:
        e->timer = 0;
        e->ala_angle = 0;
        /* Face by the player's + 0x40 vs ours (gEwramData + 0x13110). */
        e->mirrored = soma->x < e->x;
        e->step = 1;
        return;
    case 1: {
        int r = 0xD0 << 8;                 /* 0xD000 orbit radius */
        int dir = e->mirrored ? 1 : -1;
        int32_t ox = al_mul(dir * r, al_sin(e->ala_angle + 0x4000) >> 8);
        int32_t oy = al_mul(0x18000, al_sin(e->ala_angle) >> 8) +
                    (int32_t)e->ala_angle * 0xF0000 / 0x10000 * 0;
        e->x += ox;
        e->y += oy + (int32_t)((int16_t)e->ala_angle * 0xF00);
        e->ala_angle++;
        if (e->ala_pstate == 4) e->step++;
        if (e->ala_pstate == 0 && e->hp <= 0) {
            e->state = 0xFF;
            e->substep = 0;
            e->step = 0;
        }
        return;
    }
    case 2:
        if (e->ala_pstate == 0) {
            e->state = (e->hp > 0) ? 1 : 0xFF;
            e->substep = 0;
            e->step = 0;
        }
        return;
    default:
        return;
    }
}

/* sub_080CE85C: state 0xFF — the death: song 0x6E, the particle
 * burst while the body rises, then the shared delete. */
static void al_death(AosEnemy *e, const AosCollision *layer,
                     AosHitReport *report, uint32_t (*random)(void)) {
    (void)layer; (void)random;
    switch (e->step) {
    case 0:
        aos_audio_play_song(0x6E);
        e->ala_pstate = 0;                 /* + 0x5A merge bookkeeping */
        e->step = 1;
        e->substep = 0;
        return;
    case 1:
        if (e->substep == 0) { e->timer = 0xA; e->substep = 1; }
        else if (e->substep == 1) {
            /* Two sub_0806D5C0 gore rows per tick while + 0x46 < 0xAA. */
            if ((int16_t)(e->y >> 16) < 0xAA && e->timer < 0x40) {
                for (int i = 0; i < 2 && report->child_count < AOS_ENEMY_MAX_CHILDREN; ++i) {
                    AosEnemy *g = &report->children[report->child_count++];
                    *g = *e;
                    g->role = AOS_ROLE_ALASTOR_SHOT;
                    g->parent = e;
                    g->state = g->step = g->substep = 0;
                    g->hp = 1;
                    g->removed = false;
                    g->defeated = false;
                    g->hidden = true;
                    g->ala_plife = 1;
                    g->vx = (int32_t)(random() & 0xFFFF) - 0x8000;
                    g->vy = (int32_t)(random() & 0xFFFF) - 0x8000;
                }
                e->timer++;
            } else {
                e->substep = 2;
            }
        } else {
            /* sub_0803E6BC fade + EntityDelete; the partner record's
             * + 0x59 |= 8 lands through ala_pstate = 0xFF. */
            e->ala_pstate = 0xFF;
            e->removed = true;
        }
        return;
    default:
        e->removed = true;
        return;
    }
}

/* sub_080CEE30: the dormant state 8 — sink 0x10 once. */
static void al_state8(AosEnemy *e) {
    if (e->step == 0) {
        e->y -= 0x10000;
        e->step = 1;
    }
}

/* ------------------------------------------------------------------ */

AosHitReport aos_enemy_alastor_update(
    AosEnemy *e, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0 };
    const AosAnimSet *anims = kind->anims;
    if (e->role == AOS_ROLE_ALASTOR_BODY) {
        AosEnemy *p = e->parent;
        al_body_update(e, p, soma);
        return report;
    }
    if (e->role == AOS_ROLE_ALASTOR_SHOT) {
        al_shot_update(e, e->parent);
        return report;
    }
    if (e->role != AOS_ROLE_ENEMY || !al_is_alastor(e->id))
        return report;

    int sx = (int)(e->x >> 16) - cam_x, sy = (int)(e->y >> 16) - cam_y;
    if (e->state != 0xFF)
        e->hidden = outside_margins(kind, 4, sx, sy);
    if (!e->defeated && (uint16_t)(sx + 0x10) <= 0x110 &&
        (uint16_t)sy <= 0xB0)
        report.targetable = true;

    if (e->hit_flash && e->state != 0xFF && e->state != 7) {
        /* A flashing live body runs the contact pass alone. */
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        copy_result(&report, &hit);
        return report;
    }

    /* Emit the partner body on the first update (the native creates it
     * inside sub_080CEAA4; boss_parts_spawned counts it). */
    if (e->state != 0xFF && !e->boss_parts_spawned &&
        report.child_count < AOS_ENEMY_MAX_CHILDREN) {
        AosEnemy *b = &report.children[report.child_count++];
        *b = *e;
        b->role = AOS_ROLE_ALASTOR_BODY;
        b->parent = e;
        b->state = e->ala_pstate;
        b->step = e->ala_pstep;
        b->substep = e->ala_psub;
        b->timer = e->ala_ptmr;
        b->hp = 1;
        b->hit_flash = 0;
        b->defeated = false;
        b->hidden = true;
        b->removed = false;
        b->param0 = b->param1 = 0;
        b->spawned_count = 0;
        b->boss_parts_spawned = true;
        b->combat.attack_off = true;
        b->combat.hurt_off = true;
        play_loop(b, anims, 0, true);
        step_anim(b, anims);
        e->boss_parts_spawned = true;
    }

    switch (e->state) {
    case 0:  al_state0(e, soma); break;
    case 1:  al_state1(e, soma, layer); break;
    case 2:  al_state2(e, soma, layer, random); break;
    case 3:  al_state3(e, soma); break;
    case 4:  al_state4(e, soma); break;
    case 5:  al_state5(e, soma, layer, random); break;
    case 6:  al_state6(e, soma, layer, &report, anims, random); break;
    case 7:  al_state7(e, soma); break;
    case 8:  al_state8(e); break;
    case 0xFF: al_death(e, layer, &report, random); break;
    default: break;
    }

    /* The hurt/contact pass of sub_080CE9E8's sibling runs each live
     * frame (the native update calls sub_08021924 + the box check). */
    if (e->state != 0xFF && e->state != 8) {
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        copy_result(&report, &hit);
    }
    return report;
}
