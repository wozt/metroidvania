/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Chaos (true final boss 0x70, both phases).
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

/* EnemyChaosCreate 0x080A1E38 / EnemyChaosUpdate 0x080A4340 (phase 1)
 * and EnemyChaos2Create 0x080A712C / EnemyChaos2Update 0x080A6ED4
 * (phase 2, the room param1 = + 0x32 variant).
 *
 * Phase 1: the + 0x20 record entity carries the drifting core; the
 * three sub_080A2A4C parts fold into AOS_ROLE_CHAOS_PART children.
 * State 0 (sub_080A20D0) is the decoded intro — the SetPlayerInput
 * lock, the palette fades, song 0x32 for the + 0xD = 0x78 beat, song
 * 0x52 with the + 0xD = 0x51 sub_080A3D40 part pulses and the
 * every-eighth-frame sub_08044AA4 positions folded away, then state
 * 1. State 1 (sub_080A1FEC) keeps the record drift (+ 0xC += 0x80,
 * + 0x10 += 0x200) and the three staggered + 0x19/+ 0x1A/+ 0x1B
 * timers gated by the + 0x18 mask / + 0x15 fired flags; the native
 * mask is an external story protocol approximated by the three
 * timers' completion. State 2 (sub_080A43E4) is the one-shot palette
 * restore. State 3 (sub_080A22A0) is the decoded drift machine —
 * song 0x149, the + 0x48/+ 0x4C velocity ramps, the + 0xD phase
 * counter 1/2/3 and the + 0xC == 1 palette-flash exit with song
 * 0x1010. State 4 is the dormant park.
 *
 * Phase 2: hp + 0x34 = 0xBB8 through the sub_080A4408 spawn path;
 * the sub_080A4C0C hurt callback tracks + 0x34 and on depletion runs
 * sub_080683BC, song 0x1010 and state 3. EnemyChaos2Update
 * (0x080A6ED4) dispatches the + 0xA machine: state 0 the
 * sub_080A4728 intro (eleven cases), state 1 the sub_080A4508
 * twelve-case attack selector, state 2 the sub_080A71B0 park and
 * state 3 the sub_080A49E0 death machine. The + 0x20 attack record
 * of sub_080A6F58 carries the + 0xC delay, + 0x10 volley count,
 * + 0x14 armed flag, + 0x18/+ 0x1C/+ 0x20/+ 0x24 script walk driven
 * by sub_080A4CE4 — ported with the four real ROM tables
 * (0x08528374 create, 0x08528381 intro, 0x0852838E volley,
 * 0x0852839B burst); the value byte feeds sub_0803C7B4's OAM
 * channel natively and is recorded in ch2_anim. The spawned type
 * 0x49 entities port as children: the sub_080A5048/sub_080A51C0
 * homing shot (CHAOS_SHOT), the four sub_080A5270/sub_080A537C
 * turrets with the sub_080A6FC0 state-9 blast and sub_080A55F0
 * knockback (CHAOS_TURRET), the sub_080A6C7C/sub_080A6D88 and
 * sub_080A6DEC/sub_080A70F0 orbiting embers (CHAOS_ORB/ORB2), the
 * four sub_080A4D64/sub_080A4E80 intro limbs (CHAOS_LIMB) and the
 * sub_080A56CC/sub_080A6FE4 player-anchored muzzle (CHAOS_MUZZLE).
 * Still unported: the sub_080A57BC eight-limb body spawn, the
 * ~5000-line helper cluster beyond the routines above, the
 * sub_0803E058 affine channels, the palette fades/restores, the
 * SetPlayerInput locks, the gEwramData story writes and the
 * phase-2 art (tiles 0x08119224/frames 0x08226B58 share the
 * Enemy/chaos kind). */
static void ch_emit_parts(AosEnemy *e, AosHitReport *hit) {
    for (unsigned i = 0; i < 3 &&
                        hit->child_count < AOS_ENEMY_MAX_CHILDREN;
         ++i) {
        if (e->spawned_count & (1u << i)) continue;
        e->spawned_count |= (uint8_t)(1u << i);
        AosEnemy c;
        memset(&c, 0, sizeof c);
        c.id = e->id;
        c.x = e->x;
        c.y = e->y;
        c.role = AOS_ROLE_CHAOS_PART;
        c.variant = (uint8_t)i;
        c.parent = e;
        c.static_frame = (int16_t)i;
        hit->children[hit->child_count++] = c;
    }
}

static void ch_part_update(AosEnemy *part, AosEnemy *body) {
    if (!body || body->removed || body->id != 0x70) {
        part->removed = true;
        return;
    }
    /* The sub_080A2A4C parts orbit the drifting core record; the
     * native per-part brains collapse to a fixed orbit offset per
     * variant. */
    static const int32_t offs[3][2] = {
        {0x60000, -0x40000}, {-0x60000, -0x20000}, {0, -0xA0000}};
    part->x = body->ch_cx + offs[part->variant & 3][0];
    part->y = body->ch_cy + offs[part->variant & 3][1];
    part->mirrored = body->mirrored;
}

/* Phase-1 state 0 sub_080A20D0: sub-states on + 0xB. */
static void ch_intro(AosEnemy *enemy) {
    switch (enemy->step) {
    case 0:
        enemy->step = 1;
        /* sub_0803D18C/sub_0803CDF0(0x1F) palette fades are visual
         * only; + 0xD = 0x78, + 0xC = 0 and song 0x32. */
        aos_audio_play_song(0x32);
        enemy->timer = 0x78;
        break;
    case 1:
        /* The native per-frame 0x32 accent folds into the case-0
         * play; the + 0xD countdown runs into the
         * sub_0803CED4/sub_0803D280 restore. */
        if (enemy->timer) {
            enemy->timer--;
            break;
        }
        enemy->step = 2;
        enemy->timer = 0;
        break;
    default:
        /* Case 2: song 0x52 once (the native per-frame call is an
         * idempotent accent), the + 0xD = 0x51 part-pulse marker and
         * the + 0xD > 0xF9 exit into state 1. */
        if (enemy->timer == 0) {
            aos_audio_play_song(0x52);
            enemy->timer = 1;
        }
        if (enemy->substep == 0 && enemy->timer == 0x51)
            enemy->substep = 1; /* sub_080A3D40(.,0/1/2) pulse */
        /* The every-eighth-frame sub_08044AA4 part positions are
         * folded into the orbit approximation. */
        if (enemy->timer > 0xF9) {
            enemy->state = 1;
            enemy->step = 0;
            enemy->substep = 0;
            enemy->timer = 0;
        } else {
            enemy->timer++;
        }
        break;
    }
}

/* Phase-1 state 1 sub_080A1FEC: record drift plus the three
 * staggered timers; exits to state 3 when the mask protocol
 * completes (approximated by all three timers fired). */
static void ch_housekeeping(AosEnemy *enemy, uint32_t (*random)(void)) {
    if (enemy->step != 0)
        return; /* the native early-out tail only */
    enemy->ch_cx += 0x80;
    enemy->ch_cy += 0x200;
    /* The + 0x18 mask is external; the port arms all three slots
     * and lets each fire once per its decoded reload range. */
    if (!(enemy->ch_f18 & 1) && !(enemy->ch_f15 & 1)) {
        if (enemy->ch_t19) enemy->ch_t19--;
        else {
            enemy->ch_t19 = (uint8_t)((random() & 0x7F) + 0x3C);
            enemy->ch_f15 |= 1;
        }
    }
    if (!(enemy->ch_f18 & 2) && !(enemy->ch_f15 & 2)) {
        if (enemy->ch_t1a) enemy->ch_t1a--;
        else {
            enemy->ch_t1a = (uint8_t)((random() & 0x7F) + 0x78);
            enemy->ch_f15 |= 2;
        }
    }
    if (!(enemy->ch_f18 & 4) && !(enemy->ch_f15 & 4)) {
        if (enemy->ch_t1b) enemy->ch_t1b--;
        else {
            enemy->ch_t1b = (uint8_t)((random() & 0x7F) + 0x3C);
            enemy->ch_f15 |= 4;
        }
    }
    /* Native exit: + 0x18 & 7 == 7. The port completes the mask
     * once every slot has fired (documented approximation of
     * the story-driven enable). */
    if ((enemy->ch_f15 & 7) == 7) {
        enemy->ch_f18 |= 7;
        enemy->state = 3;
        enemy->step = 0;
        enemy->substep = 0;
    }
}

/* Phase-1 state 3 sub_080A22A0: the drifting core fight. */
static void ch_drift(AosEnemy *enemy) {
    switch (enemy->step) {
    case 0:
        /* sub_080D7250(x, y) snap; + 0xB = 1, + 0xD = 0x78,
         * + 0x48 = 0x80, + 0x4C = 0x100, + 0x50 = 0 and the story
         * flag writes. */
        enemy->step = 1;
        enemy->timer = 0x78;
        enemy->vx = 0x80;
        enemy->vy = 0x100;
        enemy->ax = 0;
        enemy->ch_cx = enemy->x;
        enemy->ch_cy = enemy->y;
        break;
    case 1:
        if (enemy->timer) {
            enemy->timer--;
            break;
        }
        enemy->step = 2;
        enemy->substep = 0;
        enemy->timer = 0;
        aos_audio_play_song(0x149);
        break;
    default:
        /* Case 2: the + 0x48 / + 0x4C ramps and the + 0xD phase
         * counter stored through + 0x50 (the spin accumulator). */
        if (enemy->vx <= 0x7FF) enemy->vx += 4;
        if (enemy->vy <= 0x3FF) enemy->vy = 0x100;
        enemy->vy += 0x20;
        if (enemy->timer <= 0x3B) {
            enemy->ax = 1;
        } else if (enemy->timer <= 0x59) {
            enemy->ax = 2;
        } else if (enemy->timer <= 0x77) {
            enemy->ax = 3;
        }
        /* The + 0x14 > 0x200 palette flash with song 0x1010 and the
         * sub_08011F44/sub_08011AD0 exit tail are approximated by
         * the spin accumulator crossing the same threshold. */
        if (enemy->substep == 0 && enemy->ch_spin > 0x200) {
            aos_audio_play_song(0x1010);
            enemy->substep = 1;
        }
        enemy->ch_cx += enemy->vx;
        enemy->ch_cy += enemy->vy;
        enemy->ch_spin += enemy->ax;
        enemy->timer++;
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Phase 2 (EnemyChaos2Update 0x080A6ED4 and its type-0x49 children).  */
/* ------------------------------------------------------------------ */

/* sub_080A6F58 script tables (ROM 0x08528374 family): byte 0 the
 * entry count, then count (palette value, frame count) pairs the
 * sub_080A4CE4 walker steps. The value byte is the sub_0803C7B4
 * channel target the native palette fades consume; the port records
 * the last emitted value in ch2_anim and steps the record walk. */
static const uint8_t CH2_TBL_CREATE[] = {
    6, 0x02, 0x14, 0x0D, 0x0A, 0x0E, 0x0A, 0x0F, 0x0A, 0x0D, 0x0A,
    0x02, 0xC8};
static const uint8_t CH2_TBL_INTRO[] = {
    6, 0x02, 0x50, 0x0D, 0x0A, 0x0E, 0x0A, 0x0F, 0x0A, 0x0D, 0x0A,
    0x02, 0xC8};
static const uint8_t CH2_TBL_VOLLEY[] = {
    6, 0x02, 0x08, 0x0D, 0x08, 0x0E, 0x08, 0x0F, 0x08, 0x0D, 0x08,
    0x02, 0xC8};
static const uint8_t CH2_TBL_BURST[] = {
    13, 0x0D, 0x06, 0x0E, 0x06, 0x0F, 0x06, 0x0D, 0x06, 0x0D, 0x06,
    0x0E, 0x06, 0x0F, 0x06, 0x0D, 0x06, 0x0D, 0x06, 0x0E, 0x06,
    0x0F, 0x06, 0x0D, 0x06, 0x02, 0x0A};

/* sub_080A5048's 13-entry ROM table at 0x08119230: {frame, flags}
 * u16 pairs picked by (angle & 0xFFFF) / 0x1555; flags bit 0 mirrors
 * (+ 0x58 0x40), bit 1 vertical-flips (+ 0x58 0x80). */
static const uint16_t CH2_SHOT_DIR[13][2] = {
    {0x16, 1}, {0x15, 1}, {0x14, 1}, {0x13, 0}, {0x14, 0}, {0x15, 0},
    {0x16, 0}, {0x15, 2}, {0x14, 2}, {0x13, 3}, {0x14, 3}, {0x15, 3},
    {0x16, 1}};

/* sub_0806D518(+ 0xD): the death flicker — kind->blink is the ROM
 * 0x08118CE0 table; out-of-range indices return 0. */
static bool ch2_blink(const AosEnemyKind *kind, uint8_t timer) {
    return timer <= 0x27 && (kind->blink[timer] & 1) != 0;
}

static bool ch2_parent_ok(const AosEnemy *e) {
    const AosEnemy *p = e->parent;
    return p && !p->removed && p->id == 0x70 && p->state != 3;
}

/* sub_080A6F58: arm the + 0x20 record's script walk. */
static void ch2_set_table(AosEnemy *e, const uint8_t *table) {
    e->ch2_idx = 0;
    e->ch2_frame = 0;
    e->ch2_done = false;
    e->ch2_anim = 0;
    e->ch2_table = table;
}

/* sub_080A4CE4: one script-walk step. The frame counter + 0x1C
 * advances; on entry rollover the index + 0x18 moves and the done
 * flag + 0x20 latches at the table count. The palette channel write
 * (gEwramData + 0x3F0/0x3E0 through sub_0803C7B4) has no runtime
 * counterpart; the emitted value is kept in ch2_anim. */
static void ch2_script_tick(AosEnemy *e) {
    if (e->ch2_done || !e->ch2_table)
        return;
    const uint8_t *t = e->ch2_table;
    uint8_t count = t[0];
    e->ch2_frame++;
    if (e->ch2_frame >= t[1 + e->ch2_idx * 2]) {
        e->ch2_frame = 0;
        e->ch2_idx++;
        if (e->ch2_idx >= count) {
            e->ch2_done = true;
            return;
        }
    }
    e->ch2_anim = t[1 + e->ch2_idx * 2];
}

/* sub_080A4508 case 0's shared delay reload: + 0xC = (rand & 0x7F) +
 * 0xB4. */
static void ch2_delay(AosEnemy *e, uint32_t (*random)(void)) {
    e->ch2_delay = (int32_t)((random() & 0x7F) + 0xB4);
}

/* The four sub_080A4D64 intro limbs (sub_080A4728 case 3): absolute
 * positions and the index whose bits 0/1 mirror/vflip the sprite
 * (+ 0x58 0x40/0x80). */
static void ch2_spawn_limb(AosEnemy *e, int32_t x, int32_t y, unsigned index,
                           const AosAnimSet *anims,
                           uint32_t (*random)(void), AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN)
        return;
    AosEnemy *c = &report->children[report->child_count++];
    *c = *e;
    c->role = AOS_ROLE_CHAOS_LIMB;
    c->parent = e;
    c->state = c->step = c->substep = 0;
    c->variant = (uint8_t)index;
    c->x = x;
    c->y = y;
    c->vx = c->vy = c->ax = c->ay = 0;
    c->ch2_f6f = (uint8_t)(random() % 6);   /* + 0x6F */
    c->ch2_f6d = 0;                         /* + 0x6D */
    c->ch2_t20 = 0;                         /* + 0x20 u16 */
    c->timer = 0;
    c->static_frame = -1;
    c->hp = 0xBB8;                  /* + 0x34 = 0xBB8 */
    c->hit_flash = 0;
    c->defeated = false;
    c->hidden = false;
    c->removed = false;
    c->ch2_life = 0;
    c->ch2_limb_n = 0;
    c->ch2_hx = c->ch2_hy = 0;
    c->mirrored = (index & 1) != 0;
    c->vflip = (index & 2) != 0;
    c->own_boxes = true;
    c->own_hurt = c->own_attack = (AosBox){-8, -8, 16, 16};
    /* sub_0803F2C8(child, 0, 2, 1): anim 0, loop — the phase-2 frame
     * table 0x08226B58 has no runtime kind, so the Chaos kind's anim
     * set stands in (documented). */
    play_loop(c, anims, 0, true);
}

/* sub_080A5048: the volley shot. (e, ox, oy, angle) — ox/oy are the
 * 16.16 spawn offsets case 2 computes as sin * 0x180000 (24 px), the
 * velocity is sin * 0x30000 (3 px/frame) on + 0x48/+ 0x4C, life
 * + 0xD = 0x4E then a further 0xEF-counted state-1 tail. */
static void ch2_spawn_shot(AosEnemy *e, int32_t ox, int32_t oy,
                           int32_t angle, const AosAnimSet *anims,
                           AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN)
        return;
    AosEnemy *c = &report->children[report->child_count++];
    *c = *e;
    c->role = AOS_ROLE_CHAOS_SHOT;
    c->parent = e;
    c->state = c->step = c->substep = 0;
    c->x = e->x + ox;
    c->y = e->y + oy;
    c->vx = aos_fixed_mul(aos_sine((uint32_t)(angle + 0x4000) & 0xFFFF),
                          0x30000);
    c->vy = aos_fixed_mul(aos_sine((uint32_t)angle & 0xFFFF), 0x30000);
    c->ax = c->ay = 0;
    c->timer = 0x4E;
    c->static_frame = -1;
    c->hp = 1;                  /* native + 0x34 stays 0; the update
                                 * never checks it, so the shot lives
                                 * by timer/margin/parent only */
    c->hit_flash = 0;
    c->defeated = false;
    c->hidden = false;
    c->removed = false;
    c->ch2_life = 0;
    unsigned idx = (unsigned)((angle & 0xFFFF) / 0x1555);
    if (idx > 12) idx = 12;
    c->static_frame = (int16_t)CH2_SHOT_DIR[idx][0];
    c->mirrored = (CH2_SHOT_DIR[idx][1] & 1) != 0;
    c->vflip = (CH2_SHOT_DIR[idx][1] & 2) != 0;
    c->own_boxes = true;
    c->own_hurt = c->own_attack = (AosBox){-6, -6, 12, 12};
    play_loop(c, anims, 0, true);
}

/* sub_080A5270: one of the four case-11 turrets. (e, dx, dy, vx, vy)
 * — dx/dy are pixel offsets added to the parent position (the calls
 * pass 0), vx/vy the 16.16 launch velocities (±0x60000/±0x30000). */
static void ch2_spawn_turret(AosEnemy *e, int32_t dx, int32_t dy,
                             int32_t vx, int32_t vy,
                             const AosAnimSet *anims,
                             uint32_t (*random)(void),
                             AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN)
        return;
    AosEnemy *c = &report->children[report->child_count++];
    *c = *e;
    c->role = AOS_ROLE_CHAOS_TURRET;
    c->parent = e;
    c->state = c->step = c->substep = 0;
    c->x = e->x + dx * 0x10000;
    c->y = e->y + dy * 0x10000;
    c->vx = vx;
    c->vy = vy;
    c->ax = c->ay = 0;
    c->ch2_f6f = (uint8_t)(random() % 6);   /* + 0x6F */
    c->ch2_life = 0;                        /* + 0x14 lifetime */
    c->substep = 0;                         /* + 0xC settle flag */
    c->timer = 0;
    c->static_frame = -1;
    c->hp = 0x258;                  /* + 0x34 = 0x96 << 2 */
    c->hit_flash = 0;
    c->defeated = false;
    c->hidden = false;
    c->removed = false;
    c->ch2_hx = c->ch2_hy = 0;
    c->own_boxes = true;
    c->own_hurt = (AosBox){-12, -12, 24, 24};
    c->own_attack = (AosBox){-8, -8, 16, 16};
    /* sub_0803F2C8(child, 6, 2, 1): anim 6 loop on the phase-2 table;
     * the Chaos kind's anim set stands in (documented). */
    play_loop(c, anims, 6, true);
}

/* sub_080A6C7C: an ORB ember. (e, angle16, speed) — the caller draws
 * angle = rand & 0xFFFF and speed = (rand & 0x1FFFF) + 0x40000; the
 * child leaves from the parent's + 0x40/+ 0x44 and gets
 * sub_0806D2F8's angle/speed velocity plus the + 0x50 spin
 * accumulator (rand & 0x3FF) - 0x200. */
static void ch2_spawn_orb(AosEnemy *e, int32_t angle, int32_t speed,
                          const AosAnimSet *anims,
                          uint32_t (*random)(void), AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN)
        return;
    AosEnemy *c = &report->children[report->child_count++];
    *c = *e;
    c->role = AOS_ROLE_CHAOS_ORB;
    c->parent = e;
    c->state = c->step = c->substep = 0;
    c->x = e->x & 0xFFFF0000;       /* native writes the parent pixel
                                     * words; keep the low word 0 like
                                     * the + 0x40/+ 0x44 u16 copies */
    c->y = e->y & 0xFFFF0000;
    c->static_frame = 0x4A;         /* + 0x65 = 0x4A */
    c->hp = 0;
    c->hit_flash = 0;
    c->defeated = false;
    c->hidden = false;
    c->removed = false;
    /* sub_0803E654(1) variant roll then the + 0x50 draw: consume the
     * same two RNG values (the palette variant has no runtime
     * counterpart). */
    (void)random();
    c->ax = (int32_t)((random() & 0x3FF) - 0x200);   /* + 0x50 spin */
    /* sub_0806D2F8(e, angle, speed): the 0x40000 special case stores
     * the raw sines. */
    if (speed == 0x40000) {
        c->vx = aos_sine((uint32_t)(angle + 0x4000) & 0xFFFF);
        c->vy = aos_sine((uint32_t)angle & 0xFFFF);
    } else {
        c->vx = aos_fixed_mul(aos_sine((uint32_t)(angle + 0x4000) & 0xFFFF),
                              speed);
        c->vy = aos_fixed_mul(aos_sine((uint32_t)angle & 0xFFFF), speed);
    }
    c->ay = 0;
    c->combat.attack_off = true;
    c->combat.hurt_off = true;
    play_loop(c, anims, 0, true);
}

/* sub_080A6DEC: an ORB2 ember — same velocity scheme, but the frame
 * is rand % 3 + 0x4B, the spin draw follows, then the mirror/vflip
 * draws (+ 0x58 0x40/0x80). */
static void ch2_spawn_orb2(AosEnemy *e, int32_t angle, int32_t speed,
                           const AosAnimSet *anims,
                           uint32_t (*random)(void), AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN)
        return;
    AosEnemy *c = &report->children[report->child_count++];
    *c = *e;
    c->role = AOS_ROLE_CHAOS_ORB2;
    c->parent = e;
    c->state = c->step = c->substep = 0;
    c->x = e->x & 0xFFFF0000;
    c->y = e->y & 0xFFFF0000;
    c->static_frame = (int16_t)(random() % 3 + 0x4B);
    c->hp = 0;
    c->hit_flash = 0;
    c->defeated = false;
    c->hidden = false;
    c->removed = false;
    c->ax = (int32_t)((random() & 0x3FF) - 0x200);   /* + 0x50 spin */
    c->mirrored = (random() & 1) != 0;
    c->vflip = (random() & 1) != 0;
    if (speed == 0x40000) {
        c->vx = aos_sine((uint32_t)(angle + 0x4000) & 0xFFFF);
        c->vy = aos_sine((uint32_t)angle & 0xFFFF);
    } else {
        c->vx = aos_fixed_mul(aos_sine((uint32_t)(angle + 0x4000) & 0xFFFF),
                              speed);
        c->vy = aos_fixed_mul(aos_sine((uint32_t)angle & 0xFFFF), speed);
    }
    c->ay = 0;
    c->combat.attack_off = true;
    c->combat.hurt_off = true;
    play_loop(c, anims, 0, true);
}

/* sub_080A56CC: the player-anchored muzzle the state-9 turret emits
 * every fourth timer tick. One RNG draw picks (rand & 0xFF) << 8 as
 * the launch angle; the + 0x14/+ 0x18 home offset is sin * 48 - 16 px
 * and the + 0x48..+ 0x54 drift is the velocity/accel chain decoded
 * from the routine (each stage truncates /16 like the ROM). */
static void ch2_spawn_muzzle(AosEnemy *e, const AosAnimSet *anims,
                             uint32_t (*random)(void),
                             AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN)
        return;
    AosEnemy *c = &report->children[report->child_count++];
    *c = *e;
    c->role = AOS_ROLE_CHAOS_MUZZLE;
    c->parent = e;
    c->state = c->step = c->substep = 0;
    c->x = c->y = 0xFF800000;       /* + 0x40/+ 0x44 = -8.0 */
    c->timer = 0x14;
    c->static_frame = -1;
    c->hp = 0;
    c->hit_flash = 0;
    c->defeated = false;
    c->hidden = false;
    c->removed = false;
    uint32_t angle = (random() & 0xFF) << 8;
    int32_t sx = aos_sine((angle + 0x4000) & 0xFFFF);
    int32_t sy = aos_sine(angle & 0xFFFF);
    c->ch2_hx = sx * 48;                    /* + 0x14 */
    c->ch2_hy = sy * 48 - 0x100000;         /* + 0x18 */
    c->vx = (int32_t)(-((int64_t)sx * 20) / 16);   /* + 0x48 */
    c->vy = (int32_t)(-((int64_t)sy * 20) / 16);   /* + 0x4C */
    c->ax = c->vx / 16;                     /* + 0x50 */
    c->ay = c->vy / 16;                     /* + 0x54 */
    c->combat.attack_off = true;
    c->combat.hurt_off = true;
    /* The muzzle reads tiles 0x081C15F4/palette 0x0820ED60/frames
     * 0x085283B8 natively — none are runtime assets; the Chaos kind's
     * anim set stands in (documented). */
    play_loop(c, anims, 0, true);
}

/* sub_080A6BE8: the case-10 burst — a one-shot animation entity at
 * the parent's pixel position (the + 0x42/+ 0x46 u16 copies), anim
 * id = the case's 9..16 parameter, deleted when the anim ends
 * (+ 0x59 bit 0 through sub_080A70CC). */
static void ch2_spawn_burst(AosEnemy *e, unsigned anim_id,
                            const AosAnimSet *anims,
                            AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN)
        return;
    AosEnemy *c = &report->children[report->child_count++];
    *c = *e;
    c->role = AOS_ROLE_CHAOS_BURST;
    c->parent = e;
    c->state = c->step = c->substep = 0;
    c->x = (int64_t)(uint16_t)(e->x >> 16) << 16;
    c->y = (int64_t)(uint16_t)(e->y >> 16) << 16;
    c->vx = c->vy = c->ax = c->ay = 0;
    c->timer = 0;
    c->static_frame = -1;
    c->hp = 0;
    c->hit_flash = 0;
    c->defeated = false;
    c->hidden = false;
    c->removed = false;
    c->combat.attack_off = true;
    c->combat.hurt_off = true;
    play_loop(c, anims, anim_id, false);
}

/* ------------------------------------------------------------------ */
/* Phase-2 child updates (the type-0x49 entities' own routines).       */
/* ------------------------------------------------------------------ */

/* sub_080A51C0: the shot. State 0 counts + 0xD down from 0x4E; at
 * <= 1 it enters state 1 with song 0x146 (0xA3 << 1). Both states
 * delete when the parent leaves state 3 or dies; state 1 integrates,
 * runs the contact pass, culls on the margin table entry 1 and caps
 * the + 0xD counter at 0xEF. */
static void ch2_shot_update(AosEnemy *e, const AosEnemyKind *kind,
                            int cam_x, int cam_y,
                            uint32_t (*random)(void)) {
    (void)random;
    if (e->state == 0) {
        e->timer--;
        if ((uint8_t)e->timer <= 1) {
            e->state = 1;
            aos_audio_play_song(0x146);
        }
        if (!ch2_parent_ok(e))
            e->removed = true;
        return;
    }
    if (e->state == 1) {
        e->x += e->vx;
        e->y += e->vy;
        int sx = (int)(e->x >> 16) - cam_x, sy = (int)(e->y >> 16) - cam_y;
        if (aos_enemy_outside_margins(kind, 1, sx, sy)) {
            e->removed = true;
            return;
        }
        uint8_t old = e->timer;
        e->timer = (uint8_t)(old + 1);
        if (old > 0xEF)
            e->removed = true;
        else if (e->parent && e->parent->state == 3)
            e->removed = true;
    }
}

/* sub_080A4E80: an intro limb. State 0 keeps the parent/room watch
 * (parent + 0x36 byte and + 0xA != 3 — ported as id/state/removed),
 * the + 0x6D == 1 + 0x20 countdown replays anim 0 when the old u16
 * sign-extends <= 0, contact runs every frame and the anim steps on
 * (rand & 0xF) != 0. State 0xA is the sub_080A4FA0/sub_080A4EFE
 * death tail: flame puffs every 4th timer tick, the blink table on
 * + 0xD and EntityDelete at expiry. */
static void ch2_limb_update(AosEnemy *e, AosSoma *soma,
                            const AosEnemyKind *kind,
                            const AosWeaponEntity *weapon,
                            const AosWeaponFrames *weapon_frames,
                            int soma_atk, int soma_def,
                            uint32_t (*random)(void),
                            AosHitReport *report) {
    if (e->state == 0) {
        if (e->hit_flash)
            e->hit_flash--;             /* sub_0806AE54 stun tick */
        if (e->ch2_f6d == 1) {
            uint16_t old = e->ch2_t20;
            e->ch2_t20 = (uint16_t)(old - 1);
            if ((int16_t)old <= 0)
                play_loop(e, kind->anims, 0, true);
        }
        if (ch2_parent_ok(e)) {
            if ((random() & 0xF) != 0)
                step_anim(e, kind->anims);
        } else {
            /* _080A4EFE: orphaned or parent-dying limb dies — the
             * native tail steps the anim once and skips the contact
             * pass. */
            e->state = 0xA;
            e->step = 0;
            e->hp = 0;
            e->timer = 0x28;
            e->hit_flash = 0;
            step_anim(e, kind->anims);
            return;
        }
    } else if (e->state == 0xA) {
        if ((e->timer & 3) == 0)
            spawn_flame_at(e->x, e->y - 0x60000, 0x100000, 0xC00000,
                           random, report);
        e->hidden = ch2_blink(kind, e->timer);
        e->timer--;
        if ((int8_t)e->timer <= 0) {
            e->removed = true;
            return;
        }
        step_anim(e, kind->anims);
        return;
    }
    AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                               soma_atk, soma_def);
    skeleton_merge(report, &hit);
    /* sub_080A4FA0's tail: every damaging hit decrements the parent
     * record's + 0x1C limb countdown while it is nonzero. */
    if (hit.enemy_hit && e->parent && e->parent->ch2_limb_n)
        e->parent->ch2_limb_n--;
    if (e->hp <= 0 && e->state == 0) {
        /* sub_080A4FA0's lethal tail: song 0x6E, state 0xA, timer
         * 0x28 and the parent's + 0x1C limb countdown. */
        aos_audio_play_song(0x6E);
        e->state = 0xA;
        e->step = 0;
        e->timer = 0x28;
        e->hit_flash = 0;
    }
}

/* sub_080A537C: a turret. State 0 re-arms the parent record's + 0x14
 * every frame, steers toward the + 0x13168 aim entity (4 px above
 * its centre), damps velocity by 23/24, settles the + 0xC flag when
 * |vx| <= 0x1FF, steps the anim on (rand & 0xF) != 0 and fizzles to
 * state 0xA past the + 0x14 > 0x5DB lifetime or on parent loss.
 * State 9 is the sub_080A6FC0/sub_080A56CC blast: a muzzle every 4th
 * tick, then the point-blank sub_08021654 packet and the parent's
 * disarm. State 0xA is the blink-and-die tail. */
static void ch2_turret_update(AosEnemy *e, AosSoma *soma,
                              const AosEnemyKind *kind,
                              const AosWeaponEntity *weapon,
                              const AosWeaponFrames *weapon_frames,
                              int soma_atk, int soma_def,
                              int cam_x, int cam_y,
                              const AosAnimSet *anims,
                              uint32_t (*random)(void),
                              AosHitReport *report) {
    AosEnemy *p = e->parent;
    if (p && p->ch2_table)
        p->ch2_armed = true;            /* record + 0x14 = 1 */
    if (e->state == 0) {
        /* sub_080421AC's contact pass plus the weapon hit: a lethal
         * blow runs the sub_080A6FC0 state-9 blast entry (the
         * sub_080A55F0 knockback is folded into the shared collide —
         * documented). */
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        skeleton_merge(report, &hit);
        if (e->hp <= 0) {
            e->state = 9;
            e->step = 0;
            e->timer = 0x3C;
            e->hidden = true;           /* + 0x58 |= 0x20 */
            aos_audio_play_song(0x67);
            return;
        }
        if (!ch2_parent_ok(e)) {
            e->state = 0xA;
            e->step = 0;
            e->hp = 0;
            e->timer = 0x28;
            e->hit_flash = 0;
            return;
        }
        /* Aim at the + 0x13168 entity (aos_enemy_target_*): the
         * pixel deltas truncate toward zero like the ROM's
         * negate-shift-negate pairs, ArcTan2(x, y) then + 0x4000. */
        int32_t tx = aos_enemy_target_x(soma);
        int32_t ty = aos_enemy_target_y(soma);
        int32_t dx = tx - e->x;
        int32_t dy = (ty - 0x40000) - e->y;
        int32_t px = dx >= 0 ? (dx >> 16) : -((-dx) >> 16);
        int32_t py = dy >= 0 ? (dy >> 16) : -((-dy) >> 16);
        uint16_t ang = aos_arctan2((int16_t)px, (int16_t)py);
        int32_t mx = aos_fixed_mul(aos_sine((ang + 0x4000) & 0xFFFF),
                                   0x3000);
        int32_t my = aos_fixed_mul(aos_sine(ang & 0xFFFF), 0x3000);
        e->x += mx;
        e->y += my;
        e->vx = e->vx * 23 / 24;        /* sub_0806D490(.,0x18) */
        e->vy = e->vy * 23 / 24;        /* sub_0806D4A8(.,0x18) */
        e->x += e->vx;                  /* sub_0806D3AC */
        e->y += e->vy;
        if (e->substep == 0) {
            int32_t avx = e->vx < 0 ? -e->vx : e->vx;
            if (avx <= 0x1FF) {
                e->substep = 1;
                e->own_boxes = false;   /* + 0x72 bits 1/2 cleared */
            }
        }
        if ((random() & 0xF) != 0)
            step_anim(e, kind->anims);
        int32_t old = e->ch2_life;
        e->ch2_life = old + 1;
        if (old > 0x5DB) {
            e->state = 0xA;
            e->step = 0;
            e->hp = 0;
            e->timer = 0x28;
            e->hit_flash = 0;
        }
        return;
    }
    if (e->state == 9) {
        if ((e->timer & 3) == 0)
            ch2_spawn_muzzle(e, anims, random, report);
        e->timer--;
        if ((int8_t)e->timer <= 0) {
            /* + 0x59 |= 8: the native inactive husk — the port
             * removes it (documented; the flag stops the update
             * natively and has no other effect). */
            e->removed = true;
            /* The sub_08021654 blast packet at the turret's
             * position. */
            int dmg = soma
                ? aos_soma_take_hit(soma, e->stats.contact, soma_def,
                                    e->x, 0)
                : 0;
            if (dmg > 0) {
                report->soma_hit = true;
                report->soma_damage += dmg;
            }
            if (ch2_parent_ok(e))
                e->parent->ch2_armed = false;
            return;
        }
        step_anim(e, kind->anims);
        return;
    }
    if (e->state == 0xA) {
        e->hidden = ch2_blink(kind, e->timer);
        e->timer--;
        if ((int8_t)e->timer <= 0) {
            e->removed = true;
            if (ch2_parent_ok(e))
                e->parent->ch2_armed = false;
        }
        return;
    }
    (void)cam_x;
    (void)cam_y;
}

/* sub_080A70CC + the ORB/ORB2 movers: pure visual embers — angle
 * spin on + 0x18 (u16), position += velocity, margin entry 2 culls.
 * They carry no native hurt/contact callbacks, so no collide. */
static void ch2_orb_update(AosEnemy *e, const AosEnemyKind *kind,
                           int cam_x, int cam_y) {
    /* + 0x18 is a u16: the spin accumulator + 0x50 adds into it
     * (strh), then sub_0803E058 would affine-rotate — the affine
     * channel is unported, so only the angle bookkeeping remains. */
    e->ch2_rot = (uint16_t)(e->ch2_rot + (uint16_t)e->ax);
    e->x += e->vx;
    e->y += e->vy;
    int sx = (int)(e->x >> 16) - cam_x, sy = (int)(e->y >> 16) - cam_y;
    if (aos_enemy_outside_margins(kind, 2, sx, sy))
        e->removed = true;
}

/* sub_080A70F0 (ORB2): position += velocity, margin entry 2 culls. */
static void ch2_orb2_update(AosEnemy *e, const AosEnemyKind *kind,
                            int cam_x, int cam_y) {
    e->x += e->vx;
    e->y += e->vy;
    int sx = (int)(e->x >> 16) - cam_x, sy = (int)(e->y >> 16) - cam_y;
    if (aos_enemy_outside_margins(kind, 2, sx, sy))
        e->removed = true;
}

/* sub_080A6FE4: the muzzle — position = aim entity + the + 0x14/+ 0x18
 * offset, the offset integrates + 0x48/+ 0x4C which integrate
 * + 0x50/+ 0x54, anim step and + 0xD countdown delete. */
static void ch2_muzzle_update(AosEnemy *e, AosSoma *soma,
                              const AosEnemyKind *kind) {
    int32_t tx = aos_enemy_target_x(soma);
    int32_t ty = aos_enemy_target_y(soma);
    e->x = tx + e->ch2_hx;
    e->y = ty + e->ch2_hy;
    e->vx += e->ax;
    e->vy += e->ay;
    e->ch2_hx += e->vx;
    e->ch2_hy += e->vy;
    step_anim(e, kind->anims);
    e->timer--;
    if ((int8_t)e->timer <= 0)
        e->removed = true;
}

/* sub_080A70CC: the burst — anim step, delete on + 0x59 bit 0 (the
 * anim-ended flag). The native routine has no parent check. */
static void ch2_burst_update(AosEnemy *e, const AosEnemyKind *kind) {
    step_anim(e, kind->anims);
    if (e->anim.flags & AOS_ANIM_ENDED)
        e->removed = true;
}

/* ------------------------------------------------------------------ */
/* The + 0xA state machines.                                           */
/* ------------------------------------------------------------------ */

/* sub_080A4728: the intro — eleven cases on + 0xB. The SetPlayerInput
 * lock, palette fades/restores, the gEwramData + 0xA074/+ 0x42C story
 * writes and the sub_080A57BC eight-limb body spawn have no runtime
 * counterpart (documented); case 10's sub_0803D06C gate is treated as
 * nonzero so the early-exit stays dormant. */
static void ch2_intro(AosEnemy *e, const AosAnimSet *anims,
                      uint32_t (*random)(void), AosHitReport *report) {
    switch (e->step) {
    case 0:
        e->step = 1;
        e->substep = 0;
        e->timer = 0;
        ch2_set_table(e, CH2_TBL_INTRO);
        break;
    case 1:
        ch2_script_tick(e);
        {
            uint8_t old = e->timer;
            e->timer = (uint8_t)(old + 1);
            if (old > 0x3B) {
                e->step = 2;
                e->timer = 0;
            }
        }
        break;
    case 2:
        ch2_script_tick(e);
        {
            uint8_t old = e->timer;
            e->timer = (uint8_t)(old + 1);
            if (old > 0x77) {
                e->step = 3;
                e->timer = 0;
            }
        }
        break;
    case 3:
        /* No script tick in this case natively. */
        {
            uint8_t old = e->timer;
            e->timer = (uint8_t)(old + 1);
            if (old > 0x27) {
                e->step = 4;
                e->timer = 0;
                /* The four sub_080A4D64 limbs at the decoded
                 * absolute positions; index bits mirror/vflip. */
                ch2_spawn_limb(e, 0x180000, 0x280000, 0, anims, random,
                               report);
                ch2_spawn_limb(e, 0xD80000, 0x280000, 1, anims, random,
                               report);
                ch2_spawn_limb(e, 0x180000, 0x780000, 2, anims, random,
                               report);
                ch2_spawn_limb(e, 0xD80000, 0x780000, 3, anims, random,
                               report);
                e->ch2_limb_n = 4;      /* record + 0x1C countdown */
            }
        }
        break;
    case 4:
        {
            uint8_t old = e->timer;
            e->timer = (uint8_t)(old + 1);
            if (old > 0x27) {
                /* sub_0803D270/sub_0803CED4 restores and the story
                 * writes fold away; sub_080A57BC(e,0,0,0) — the
                 * eight-limb body spawn — is unported (documented). */
                e->state = 1;
                e->step = 0;
                e->substep = 0;
                e->timer = 0;
            }
        }
        break;
    case 10:
        /* sub_0803D06C() == 0 would take the same exit as case 4;
         * the gEwramData + 0x12430 word is unmodelled, so the gate
         * is treated as nonzero (documented). */
        break;
    default:
        break;
    }
}

/* sub_080A4508: the state-1 attack selector — twelve cases on + 0xB.
 * Cases 3-9 are empty jump slots (documented: they fall through to
 * the routine end like the native table). */
static void ch2_attacks(AosEnemy *e, const AosEnemyKind *kind,
                        const AosAnimSet *anims,
                        uint32_t (*random)(void), AosHitReport *report) {
    switch (e->step) {
    case 0:
        if (e->ch2_delay) {
            e->ch2_delay--;
            break;
        }
        ch2_delay(e, random);
        if (random() & 1) {
            /* Volley arm: record + 0x10 = 0, table 0x0852838E,
             * + 0xD = 0x38, + 0x50 the ±0x1555/0x1556 angle step and
             * + 0x48 = 0x4000 the firing angle. */
            e->step = 1;
            e->ch2_volley = 0;
            ch2_set_table(e, CH2_TBL_VOLLEY);
            e->timer = 0x38;
            e->ch2_astep = (random() & 1) ? (int32_t)0xFFFFEAAB : 0x1556;
            e->ch2_angle = 0x4000;
        } else if (!e->ch2_armed) {
            e->step = 10;
            ch2_set_table(e, CH2_TBL_BURST);
            e->timer = 0;
        } else {
            /* record + 0x14 armed: the delay reloads again. */
            ch2_delay(e, random);
        }
        break;
    case 1:
        ch2_script_tick(e);
        e->timer--;
        if ((int8_t)e->timer <= 0) {
            e->step = 2;
            e->timer = 0xA;
        }
        break;
    case 2: {
        /* The + 0xD gate: the native stores old+1 then fires on any
         * nonzero old — after the case-1 0xA seed it fires every
         * other frame, twelve volleys in all. */
        uint8_t old = e->timer;
        e->timer = (uint8_t)(old + 1);
        if (old == 0)
            break;
        e->timer = 0;
        /* The native adds 0x4000 only inside the sine lookups; the
         * stored + 0x48 angle is untouched until the += step below. */
        int32_t ox = aos_fixed_mul(aos_sine((e->ch2_angle + 0x4000) &
                                            0xFFFF), 0x180000);
        int32_t oy = aos_fixed_mul(aos_sine(e->ch2_angle & 0xFFFF),
                                   0x180000);
        ch2_spawn_shot(e, ox, oy, e->ch2_angle, anims, report);
        aos_audio_play_song(0x145);
        e->ch2_angle += e->ch2_astep;
        e->ch2_volley++;
        if (e->ch2_volley > 0xB) {
            e->step = 0;
            ch2_delay(e, random);
        }
        break;
    }
    case 10:
        ch2_script_tick(e);
        if (e->timer <= 0x3B) {
            e->timer++;
            if ((e->timer & 3) == 0)
                ch2_spawn_burst(e, ((e->timer >> 2) & 7) + 9, anims,
                                report);
        }
        if (e->ch2_done)
            e->step = 0xB;
        break;
    case 11:
        /* The four sub_080A5270 turrets: vx ±0x60000, vy ∓0x30000 in
         * the decoded call order, then record armed, delay reload,
         * step 0 and song 0x85. */
        ch2_spawn_turret(e, 0, 0, (int32_t)0xFFFA0000, (int32_t)0xFFFD0000,
                         anims, random, report);
        ch2_spawn_turret(e, 0, 0, 0x60000, (int32_t)0xFFFD0000,
                         anims, random, report);
        ch2_spawn_turret(e, 0, 0, (int32_t)0xFFFA0000, 0x30000,
                         anims, random, report);
        ch2_spawn_turret(e, 0, 0, 0x60000, 0x30000,
                         anims, random, report);
        e->ch2_armed = true;
        e->step = 0;
        e->timer = 0;
        ch2_delay(e, random);
        aos_audio_play_song(0x85);
        break;
    default:
        break;
    }
    (void)kind;
}

/* sub_080A49E0: the death machine — four cases on + 0xB. The
 * + 0x15 gate is the phase-1 fired-flags byte the native reuses;
 * the port arms it on lethal entry (documented: no native writer was
 * traced inside the ported cluster). */
static void ch2_death(AosEnemy *e, AosSoma *soma, const AosEnemyKind *kind,
                      const AosAnimSet *anims,
                      int soma_def, uint32_t (*random)(void),
                      AosHitReport *report) {
    switch (e->step) {
    case 0:
        ch2_script_tick(e);
        if (e->ch_f15 != 0) {
            e->step = 1;
            play_loop(e, anims, 0x11, false);
        }
        break;
    case 1:
        /* The + 0x6F/+ 0x6E song gate: + 0x6F clear and + 0x6E in
         * {1,3,4,5} plays 0x167 — those bytes are written by the
         * unported sub_080A57BC limb machine, so the gate is dormant
         * in the port (documented). The step then waits on + 0x59
         * bit 0 — the anim-ended marker of the 0x11 play — ported as
         * AOS_ANIM_ENDED. */
        if (e->ch2_f6f == 0 &&
            (e->ch2_f6e == 1 || e->ch2_f6e == 3 || e->ch2_f6e == 4 ||
             e->ch2_f6e == 5))
            aos_audio_play_song(0x167);
        if (!(e->anim.flags & AOS_ANIM_ENDED))
            break;
        e->step = 2;
        e->timer = 0x3C;
        aos_audio_play_song(0x168);
        for (int i = 0; i < 3; ++i) {
            int32_t angle = (int32_t)(random() & 0xFFFF);
            int32_t speed = (int32_t)((random() & 0x1FFFF) + 0x40000);
            ch2_spawn_orb(e, angle, speed, anims, random, report);
        }
        for (int i = 0; i < 5; ++i) {
            int32_t angle = (int32_t)(random() & 0xFFFF);
            int32_t speed = (int32_t)((random() & 0x1FFFF) + 0x40000);
            ch2_spawn_orb2(e, angle, speed, anims, random, report);
        }
        break;
    case 2:
        /* While + 0xD > 0x38 every frame emits one ORB and three
         * ORB2 with fresh random angle/speed; the counter then runs
         * out through the shared tail. */
        if (e->timer > 0x38) {
            int32_t angle = (int32_t)(random() & 0xFFFF);
            int32_t speed = (int32_t)((random() & 0x1FFFF) + 0x40000);
            ch2_spawn_orb(e, angle, speed, anims, random, report);
            for (int i = 0; i < 3; ++i) {
                angle = (int32_t)(random() & 0xFFFF);
                speed = (int32_t)((random() & 0x1FFFF) + 0x40000);
                ch2_spawn_orb2(e, angle, speed, anims, random, report);
            }
        }
        e->timer--;
        if ((int8_t)e->timer <= 0) {
            /* Palette fades and the story/room writes fold away;
             * song 0x1010 and step 3 with + 0xD = 0x3A. */
            aos_audio_play_song(0x1010);
            e->step = 3;
            e->timer = 0x3A;
        }
        break;
    case 3:
        /* The native tail counts + 0xD down and runs the gEwramData
         * story/room writes at <= 1 — the entity's removal is left
         * to the room teardown natively; the port removes the husk
         * when the counter first reaches 0 (documented). */
        e->timer--;
        if (e->timer == 0)
            e->removed = true;
        break;
    default:
        break;
    }
    (void)soma;
    (void)kind;
    (void)soma_def;
}

/* EnemyChaos2Update's + 0xA dispatch plus the role children. */
static void ch2_update(AosEnemy *enemy, AosHitReport *report,
                       AosSoma *soma, const AosEnemyKind *kind,
                       const AosWeaponEntity *weapon,
                       const AosWeaponFrames *weapon_frames,
                       int soma_atk, int soma_def,
                       int cam_x, int cam_y,
                       uint32_t (*random)(void)) {
    const AosAnimSet *anims = kind->anims;
    switch (enemy->state) {
    case 0:
        ch2_intro(enemy, anims, random, report);
        break;
    case 1:
        ch2_attacks(enemy, kind, anims, random, report);
        break;
    case 2:
        /* sub_080A71B0: bx lr — the dormant park. */
        break;
    case 3:
        ch2_death(enemy, soma, kind, anims, soma_def, random, report);
        break;
    default:
        break;
    }
    (void)weapon;
    (void)weapon_frames;
    (void)soma_atk;
    (void)cam_x;
    (void)cam_y;
}

AosHitReport aos_enemy_chaos_update(
    AosEnemy *enemy, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind,
    const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0};
    (void)layer;
    if (enemy->role == AOS_ROLE_CHAOS_PART) {
        ch_part_update(enemy, enemy->parent);
        return report;
    }
    /* The phase-2 type-0x49 children dispatched by role (the native
     * per-entity update pointers sub_080A51C0/sub_080A537C/
     * sub_080A6D88/sub_080A70F0/sub_080A4E80/sub_080A6FE4/
     * sub_080A70CC). */
    switch (enemy->role) {
    case AOS_ROLE_CHAOS_SHOT:
        ch2_shot_update(enemy, kind, cam_x, cam_y, random);
        aos_combat_tick(&enemy->combat);
        return report;
    case AOS_ROLE_CHAOS_TURRET:
        ch2_turret_update(enemy, soma, kind, weapon, weapon_frames,
                          soma_atk, soma_def, cam_x, cam_y, kind->anims,
                          random, &report);
        aos_combat_tick(&enemy->combat);
        return report;
    case AOS_ROLE_CHAOS_ORB:
        ch2_orb_update(enemy, kind, cam_x, cam_y);
        return report;
    case AOS_ROLE_CHAOS_ORB2:
        ch2_orb2_update(enemy, kind, cam_x, cam_y);
        return report;
    case AOS_ROLE_CHAOS_LIMB:
        ch2_limb_update(enemy, soma, kind, weapon, weapon_frames,
                        soma_atk, soma_def, random, &report);
        aos_combat_tick(&enemy->combat);
        return report;
    case AOS_ROLE_CHAOS_MUZZLE:
        ch2_muzzle_update(enemy, soma, kind);
        return report;
    case AOS_ROLE_CHAOS_BURST:
        ch2_burst_update(enemy, kind);
        return report;
    default:
        break;
    }
    bool phase2 = enemy->param1 != 0;
    if (phase2 && enemy->state != 3 && enemy->hit_flash)
        enemy->hit_flash--;     /* EnemyChaos2Update's sub_0806AE54 */
    /* The sub_080A2A4C orbit parts are a phase-1-only spawn; the
     * phase-2 body comes from the unported sub_080A57BC machine. */
    if (!phase2 && !enemy->boss_parts_spawned) {
        ch_emit_parts(enemy, &report);
        enemy->boss_parts_spawned = true;
    }
    if (phase2) {
        /* EnemyChaos2Create's sub_080A6F58 arms the 0x08528374
         * create script on the record; the intro's case 0 replaces
         * it with 0x08528381 on the first tick, like the native
         * order. */
        if (!enemy->ch2_table)
            ch2_set_table(enemy, CH2_TBL_CREATE);
        ch2_update(enemy, &report, soma, kind, weapon, weapon_frames,
                   soma_atk, soma_def, cam_x, cam_y, random);
    } else {
        switch (enemy->state) {
        case 0:
            ch_intro(enemy);
            break;
        case 1:
            ch_housekeeping(enemy, random);
            break;
        case 2:
            /* sub_080A43E4: the one-shot + 0xB = 1 palette restore. */
            if (enemy->step == 0)
                enemy->step = 1;
            break;
        case 3:
            ch_drift(enemy);
            break;
        default:
            /* State 4 and above: the dormant park (bx lr tail). */
            break;
        }
    }
    step_anim(enemy, kind->anims);
    AosHitReport hit = collide(enemy, soma, kind, weapon, weapon_frames,
                               soma_atk, soma_def);
    skeleton_merge(&report, &hit);
    /* The sub_080A4C0C hurt callback on phase 2 tracks + 0x34 (the
     * 0xBB8 hp); the shared collide already applies record damage,
     * so a lethal hit enters the state-3 death machine like the
     * native sub_080683BC + PlaySong 0x1010 path (+ 0xD = 0x78,
     * + 0xB = 0). The native callback clears + 0x15 and no writer in
     * the ported cluster re-arms it; the port sets it so the
     * sub_080A49E0 step-0 gate opens (documented). Phase 1's 9999 hp
     * makes it unreachable in practice. */
    if (phase2 && !enemy->defeated && enemy->hp <= 0 &&
        enemy->state != 3) {
        enemy->defeated = true;
        enemy->state = 3;
        enemy->step = 0;
        enemy->timer = 0x78;
        enemy->ch_f15 = 1;
        aos_audio_play_song(0x1010);    /* sub_080A4C0C's PlaySong */
    }
    aos_combat_tick(&enemy->combat);
    return report;
}
