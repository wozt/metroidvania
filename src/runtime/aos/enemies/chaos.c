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
 * The intro's case 4 also fires sub_080A57BC(e,0,0,0): the
 * phase-2 crawler — one sub_080A5A7C body that crawls a decoded
 * four-group target table on pixel-space ArcTan2 homing, feeds a
 * 120-record motion trail and cycles group attacks; its eight
 * sub_080A62B8 limbs integrate that trail into centipede
 * segments, and its case-3 sub_080A5A7C attack replays the
 * 0x085283CC sequence firing sub_080A65C0/sub_080A6724 mouth
 * shots. Still unported: the ~5000-line helper cluster beyond
 * the routines above, the sub_0803E058 affine channels, the
 * palette fades/restores, the SetPlayerInput locks, the
 * gEwramData story writes and the phase-2 art (tiles
 * 0x08119224/frames 0x08226B58 share the Enemy/chaos kind). */
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
 * flag + 0x20 latches at the table count. The OAM channel write
 * (sub_0803C7B4 through the gEwramData +0x3E0 record-pointer /
 * +0x3F0 last-frame cache pair, channel = entity +0x2C - 0xA —
 * decoded, visual-only) has no runtime counterpart; the emitted
 * value is kept in ch2_anim. */
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
/* sub_080A57BC: the phase-2 crawler body, its eight sub_080A62B8     */
/* limbs and the sub_080A65C0/sub_080A6724 mouth shot.                */
/* ------------------------------------------------------------------ */

/* The target tables sub_080A5A7C memcpy's onto its stack: group 0 the
 * five {x,y} u16 pairs of 0x0811926C, group 1 the eight of
 * 0x08119280, groups 2/3 the eight {x,y,pad} u16 triples of
 * 0x081192C0 (group 3 is a second identical copy natively). The
 * 0x08119264 and 0x081192A0 copies are dead in the ported paths
 * (documented). Targets are whole pixels compared against the
 * +0x42/+0x46 pixel words. */
/* Groups 0/1 sit back-to-back on the native stack frame (sb =
 * sp+0x40, r8 = sp+0x54), so a group-0 index past its five entries
 * reads the head of the group-1 table — mirrored here so the read
 * stays in bounds. */
static const uint8_t CH2_BODY_T0[32] = {
    0x30, 0x00, 0xD0, 0x00, 0x30, 0x00, 0xD0, 0x00, 0xC0, 0x00, 0xD0,
    0x00, 0xC0, 0x00, 0xD0, 0x00, 0x80, 0x00, 0xD0, 0x00,
    0x58, 0x00, 0x50, 0x00, 0x80, 0x00, 0x50, 0x00, 0xA8, 0x00,
    0x50, 0x00};
static const uint8_t CH2_BODY_T1[32] = {
    0x58, 0x00, 0x50, 0x00, 0x80, 0x00, 0x50, 0x00, 0xA8, 0x00, 0x50,
    0x00, 0x80, 0x00, 0x30, 0x00, 0x80, 0x00, 0x70, 0x00, 0x70, 0x00,
    0x40, 0x00, 0xA0, 0x00, 0x60, 0x00, 0x40, 0x00, 0x60, 0x00};
static const uint8_t CH2_BODY_T2[48] = {
    0x30, 0x00, 0x20, 0x00, 0x00, 0x00, 0x30, 0x00, 0x78, 0x00, 0x00,
    0x00, 0xC0, 0x00, 0x20, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x78, 0x00,
    0x00, 0x00, 0x78, 0x00, 0xD0, 0xFF, 0x00, 0x00, 0x78, 0x00, 0xD0,
    0x00, 0x00, 0x00, 0xD0, 0xFF, 0x54, 0x00, 0x00, 0x00, 0x20, 0x01,
    0x54, 0x00, 0x00, 0x00};
#define CH2_BODY_T3 CH2_BODY_T2    /* the native second memcpy is identical */

/* sub_0803F0E4(body, 0x085283CC, 2, 0): the case-3 attack sequence —
 * twelve 4-byte {frame, ticks} records (the leading {0x0B, 0} record
 * is the prime's instant-advance entry). +0x6E is the record index,
 * +0x6F the tick and +0x59 bit 0 the end latch; the port replays the
 * timing on ch2_idx/ch2_frame/ch2_done because frame table
 * 0x08226B58 has no runtime kind (documented). */
static const uint8_t CH2_BODY_ATK[12][2] = {
    {0x0B, 0}, {7, 20}, {8, 10}, {9, 9}, {8, 8}, {9, 7},
    {8, 6}, {9, 5}, {8, 4}, {9, 3}, {7, 30}, {5, 2}};

static uint16_t ch2_body_u16(const uint8_t *t, unsigned off) {
    return (uint16_t)(t[off] | ((uint16_t)t[off + 1] << 8));
}

/* The case-0/1 target acquire: +0x17 picks the table, +0x1B the
 * entry; +0x1C/+0x1E get the u16 pair (strh — +0x1E reads back
 * signed, e.g. the 0xFFD0 entries). */
static void ch2_body_target(AosEnemy *e) {
    unsigned i = e->ch2_blmb;
    const uint8_t *t;
    unsigned stride;
    switch (e->ch2_bgrp) {
    case 0:  t = CH2_BODY_T0; stride = 4; break;
    case 1:  t = CH2_BODY_T1; stride = 4; break;
    case 2:  t = CH2_BODY_T2; stride = 6; break;
    default: t = CH2_BODY_T3; stride = 6; break;
    }
    e->ch2_t20 = ch2_body_u16(t, i * stride);
    e->ch2_tgy = (int16_t)ch2_body_u16(t, i * stride + 2);
}

/* The +0x24 heap trail: 120 6-byte records shifted one slot toward
 * the top (newest at index 0), then record 0 written as
 * {b0, b1, frame-byte, +0x5C, +0x20 hi, +0x20 lo}. The sub_080A62B8
 * limbs sample record 8*index+8 — limb 7 the oldest, limb 0 the
 * newest — which is what makes the body a centipede. */
static void ch2_body_trail(AosEnemy *e, uint8_t b0, uint8_t b1,
                           uint8_t frame_byte) {
    memmove(e->ch2_trail + 6, e->ch2_trail, 714);
    e->ch2_trail[0] = b0;
    e->ch2_trail[1] = b1;
    e->ch2_trail[2] = frame_byte;
    e->ch2_trail[3] = e->ch2_bcnt;
    e->ch2_trail[4] = (uint8_t)(e->ch2_bang >> 8);
    e->ch2_trail[5] = (uint8_t)e->ch2_bang;
}

/* The shared limb-index redraw of the dist <= 2 transitions: a
 * (rand % 3) == 0 gate picks (rand & 3) + 4, otherwise rand & 3,
 * redrawn until it differs from +0x1B. */
static void ch2_body_pick(AosEnemy *e, uint32_t (*random)(void)) {
    for (;;) {
        if (aos_enemy_umod(random(), 3) == 0) {
            uint8_t r = (uint8_t)((random() & 3) + 4);
            if (r != e->ch2_blmb) {
                e->ch2_blmb = r;
                return;
            }
        } else {
            uint8_t r = (uint8_t)(random() & 3);
            if (r != e->ch2_blmb) {
                e->ch2_blmb = r;
                return;
            }
        }
    }
}

/* sub_080A57BC(e, 0, 0, 0): the crawler body — one type-0x49 entity
 * (update sub_080A5A7C) spawned at the parent's +0x40/+0x44 plus the
 * (0,0) offsets, +0x5C = 0x1E, +0x65 = 0x0B, +0x5B = 2,
 * +0x3C = 0x55, +0x20 = 0x4000, +0x17 = 0, +0x1B = 4 and the
 * +0x18/+0x19 spawn-origin bytes: sub_080A57BC runs LDRH on the
 * gEwramData + 0xA094 +6/+0xA u16s and STRB truncates to the low
 * byte — the room-record scroll origin's low byte, published
 * through aos_enemy_set_room_origin (the same bgInfo[1] view the
 * Object20 In block reads).
 * The eight sub_080A62B8 limbs are emitted by the body's first
 * update; natively they are created inside the same call (a
 * one-frame difference, documented). The affine channels
 * (sub_0803E654/sub_0803E058), the palette merges and the
 * sub_0804277C/sub_08042848 box preset 0x1010F8F8 have no runtime
 * counterpart. */
static void ch2_spawn_body(AosEnemy *e, const AosAnimSet *anims,
                           AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN)
        return;
    uint8_t body_slot = report->child_count;
    AosEnemy *c = &report->children[report->child_count++];
    *c = *e;
    c->role = AOS_ROLE_CHAOS_BODY;
    /* sub_080A4408's tail registers the new body entity itself
     * through sub_08034498 (then the + 0x42C story writes, which
     * have no modeled owner); the report resolves the child index
     * to the appended room record in the host. */
    report->escort_op = 3;
    report->escort_child = body_slot;
    c->parent = e;
    c->state = c->step = c->substep = 0;
    c->variant = 0;
    c->x = e->x;
    c->y = e->y;
    c->vx = c->vy = c->ax = c->ay = 0;
    c->timer = 0;
    c->static_frame = -1;
    c->hp = 0xBB8;
    c->hit_flash = 0;
    c->defeated = false;
    c->hidden = false;
    c->removed = false;
    c->boss_parts_spawned = false;
    c->ch2_bgrp = 0;                    /* +0x17 */
    c->ch2_blmb = 4;                    /* +0x1B */
    /* +0x18/+0x19: the low byte of the bgInfo[1] +6/+0xA u16s
     * (LDRH+STRB truncation in sub_080A57BC), kept <= 0xFF so the
     * limb's +0x18 refresh delta collapses like the native one. */
    int32_t room_ox = 0, room_oy = 0;
    aos_enemy_room_origin(&room_ox, &room_oy);
    c->ch2_box = (uint16_t)(uint8_t)room_ox;
    c->ch2_oy = (uint16_t)(uint8_t)room_oy;
    c->ch2_lox = (uint8_t)c->ch2_box;
    c->ch2_loy = (uint8_t)c->ch2_oy;
    c->ch2_turn = 0;                    /* +0x14 */
    c->ch2_t20 = 0;                     /* +0x1C */
    c->ch2_tgy = 0;                     /* +0x1E */
    c->ch2_bang = 0x4000;               /* +0x20 = 0x80 << 7 */
    c->ch2_t22 = 0;                     /* +0x22 */
    c->ch2_bcnt = 0x1E;                 /* +0x5C */
    c->ch2_banim = 0x0B;                /* +0x65 */
    c->ch2_bdx = c->ch2_bdy = 0;
    c->ch2_idx = c->ch2_frame = c->ch2_anim = 0;
    c->ch2_done = false;
    /* sub_080A57BC fills all 120 heap records at creation with
     * {0, 0, +0x65 = 0x0B, +0x5C = 0x1E, +0x20 = 0x4000}. */
    for (int i = 0; i < 120; ++i) {
        c->ch2_trail[i * 6 + 0] = 0;
        c->ch2_trail[i * 6 + 1] = 0;
        c->ch2_trail[i * 6 + 2] = 0x0B;
        c->ch2_trail[i * 6 + 3] = 0x1E;
        c->ch2_trail[i * 6 + 4] = 0x40;
        c->ch2_trail[i * 6 + 5] = 0x00;
    }
    c->stats.contact = 0x55;            /* +0x3C */
    c->own_boxes = true;
    c->own_hurt = (AosBox){-16, -16, 32, 32};
    c->own_attack = (AosBox){-8, -8, 16, 16};
    c->combat.attack_off = false;       /* +0x72 = (old | 4) & 0xFD:
                                         * attack on, hurt off */
    c->combat.hurt_off = true;
    /* sub_0803B924 frames 0x08226B58 share the Chaos kind's art —
     * the Chaos anim set stands in (documented). */
    play_loop(c, anims, 0, true);
}

/* The eight sub_080A62B8 limbs sub_080A57BC creates (indices 7
 * down to 0): linked through +0x10, position and room-band byte
 * copied, +0x5C = body +0x5C + 1, +0x1A = +0x0D = index, +0x3C =
 * 0x55, +0x65 = 0x0E (index 7 gets 5 plus its own affine channel —
 * unported) and +0x72 = (old & 0xFD) | 4: attack on, hurt off. */
static void ch2_body_emit_limbs(AosEnemy *body, const AosAnimSet *anims,
                                AosHitReport *report) {
    for (int i = 7; i >= 0; --i) {
        if (report->child_count >= AOS_ENEMY_MAX_CHILDREN)
            return;
        AosEnemy *c = &report->children[report->child_count++];
        *c = *body;
        c->role = AOS_ROLE_CHAOS_BODY_LIMB;
        c->parent = body;
        c->state = c->step = c->substep = 0;
        c->variant = (uint8_t)i;
        c->ch2_lidx = (uint8_t)i;       /* +0x1A (s8) */
        c->x = body->x;
        c->y = body->y;
        c->vx = c->vy = c->ax = c->ay = 0;
        c->timer = (uint8_t)i;          /* +0xD = the spawn index */
        c->static_frame = -1;
        c->ch2_anim = (i == 7) ? 5 : 0x0E;  /* +0x65 */
        c->hp = 0;
        c->hit_flash = 0;
        c->defeated = false;
        c->hidden = false;
        c->removed = false;
        c->boss_parts_spawned = false;
        c->ch2_lox = (uint8_t)body->ch2_box;  /* +0x18/+0x19 */
        c->ch2_loy = (uint8_t)body->ch2_oy;
        c->ch2_lcnt = (uint8_t)(body->ch2_bcnt + 1);  /* +0x5C */
        c->own_boxes = true;
        c->own_hurt = (AosBox){-8, -8, 16, 16};
        c->own_attack = (AosBox){-6, -6, 12, 12};
        c->combat.attack_off = false;   /* +0x72: attack on */
        c->combat.hurt_off = true;      /*          hurt off */
        play_loop(c, anims, 0, true);
    }
}

/* sub_080A65C0(body, angle): the case-3 mouth shot — a type-0x49
 * entity (update sub_080A6724) at the body's position plus the
 * decoded sine offsets, velocity sin * 0x30000 on +0x48/+0x4C,
 * contact +0x3C = 0x82, attack-only +0x72, anim 7 loop and
 * +0x65 = 0x1E. r8 = angle - 0x1300, r5 = r8 + 0x4000. */
static void ch2_spawn_body_shot(AosEnemy *e, uint32_t angle,
                                const AosAnimSet *anims,
                                AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN)
        return;
    AosEnemy *c = &report->children[report->child_count++];
    *c = *e;
    c->role = AOS_ROLE_CHAOS_BODY_SHOT;
    c->parent = e;
    c->state = c->step = c->substep = 0;
    c->variant = 0;
    uint32_t r8 = angle + 0xFFFFED00u;      /* angle - 0x1300 */
    uint32_t r5 = r8 + 0x4000;              /* + 0x80 << 7 */
    int32_t s5 = aos_sine(r5 & 0xFFFF);
    int32_t s8 = aos_sine(r8 & 0xFFFF);
    /* Position offsets mul(sin, 0xE0 << 13 = 0x1C0000); velocities
     * mul(sin, 0xC0 << 10 = 0x30000). */
    c->x = e->x + aos_fixed_mul(s5, 0x1C0000);
    c->y = e->y + aos_fixed_mul(s8, 0x1C0000);
    c->vx = aos_fixed_mul(s5, 0x30000);
    c->vy = aos_fixed_mul(s8, 0x30000);
    c->ch2_lcnt = (uint8_t)(e->ch2_bcnt + 1);  /* +0x5C = body + 1 */
    c->ax = c->ay = 0;
    c->timer = 0;
    c->static_frame = -1;
    c->ch2_anim = 0x1E;                 /* +0x65 */
    c->hp = 0;
    c->hit_flash = 0;
    c->defeated = false;
    c->hidden = false;
    c->removed = false;
    c->boss_parts_spawned = false;
    c->stats.contact = 0x82;            /* +0x3C */
    c->own_boxes = true;
    c->own_hurt = (AosBox){-6, -6, 12, 12};
    c->own_attack = (AosBox){-6, -6, 12, 12};
    c->combat.attack_off = false;       /* +0x72: attack on */
    c->combat.hurt_off = true;          /*          hurt off */
    play_loop(c, anims, 7, true);       /* sub_0803F2C8(.,7,2,1) */
}

/* The case-2 crawl (sub_080A5A7C's +0xB == 2 block): pixel-space
 * homing on +0x1C/+0x1E with the ArcTan2 aim, the +0x14 turn-speed
 * ramp and u16 angle wrap, the +0x5C band counter and +0x65 frame
 * rules per group, the distance-banded +0x50 speed ramp against
 * +0x54, the sin/cos * speed velocity, the motion deltas feeding
 * the trail record, the dist <= 2 group transitions, the +0x22 >
 * 0x12B stuck watchdog and the parent-state-3 death entry. The
 * sub_0803E058 affine calls are visual-only (unported). */
static void ch2_body_crawl(AosEnemy *e, AosSoma *soma,
                           const AosEnemyKind *kind,
                           const AosWeaponEntity *weapon,
                           const AosWeaponFrames *weapon_frames,
                           int soma_atk, int soma_def,
                           uint32_t (*random)(void),
                           AosHitReport *report) {
    int32_t px = (int16_t)(e->x >> 16);
    int32_t py = (int16_t)(e->y >> 16);
    int32_t tx = (int16_t)e->ch2_t20;
    int32_t ty = e->ch2_tgy;
    int32_t dx = tx - px, dy = ty - py;
    uint32_t d2 = (uint32_t)dx * (uint32_t)dx +
                  (uint32_t)dy * (uint32_t)dy;
    uint32_t dist = (uint16_t)aos_isqrt(d2);
    if (dist == 0) {
        /* Arrival snap: the +0x42/+0x46 pixel words take the target
         * and the +0x40/+0x44 low words zero (native strh r2 with
         * d2 == 0); the +0x50 speed drops — the shared tail still
         * runs (ArcTan2(0,0), zero velocity). */
        e->x = (int32_t)(uint16_t)e->ch2_t20 << 16;
        e->y = (int32_t)(uint16_t)e->ch2_tgy << 16;
        e->ax = 0;
        px = tx;
        py = ty;
    }
    uint8_t oldx = (uint8_t)px, oldy = (uint8_t)py;
    dx = tx - px;
    dy = ty - py;
    uint16_t ang = aos_arctan2((int16_t)dx, (int16_t)dy);
    uint16_t diff = (uint16_t)(ang - e->ch2_bang);
    /* The +0x14 turn speed: dist > 5 ramps +8 capped at 0x280;
     * inside 5 px it adds the raw diff (or 0x20 past 0x20). */
    uint16_t turn;
    if (dist > 5)
        turn = e->ch2_turn > 0x27F ? 0x280 : (uint16_t)(e->ch2_turn + 8);
    else if (diff > 0x20)
        turn = (uint16_t)(e->ch2_turn + 0x20);
    else
        turn = (uint16_t)(e->ch2_turn + diff);
    e->ch2_turn = turn;
    /* The u16 angle integrate with the native dead-zone wrap. */
    uint16_t bang = e->ch2_bang;
    if (diff & 0x8000) {
        if (diff >= (uint16_t)(0x10000 - turn)) {
            if (diff < (uint16_t)(0x10000 - (turn >> 1)))
                bang = (uint16_t)(bang + diff);
        } else {
            bang = (uint16_t)(bang - turn);
        }
    } else {
        if (diff > turn)
            bang = (uint16_t)(bang + turn);
        else if (diff > (turn >> 1))
            bang = (uint16_t)(bang + diff);
    }
    e->ch2_bang = bang;
    /* The +0x5C band counter counts down only in group 0 past 0x11. */
    if (e->ch2_bgrp == 0 && e->ch2_bcnt > 0x11)
        e->ch2_bcnt--;
    /* The +0x65 frame rules per group on the +0xD timer. */
    switch (e->ch2_bgrp) {
    case 0:
        if (e->timer > 5) {
            if (e->ch2_banim == 0xA) e->ch2_banim = 0xB;
        } else if (e->ch2_banim == 5) {
            e->ch2_banim = 0xA;
        }
        break;
    case 1:
    case 2:
        if (e->timer > 9) {
            if (e->ch2_banim == 0xA) e->ch2_banim = 5;
        } else if (e->ch2_banim == 5) {
            e->ch2_banim = 0xA;
        }
        break;
    default:
        if (e->timer > 9 && e->ch2_banim == 6) e->ch2_banim = 5;
        break;
    }
    /* The distance-banded +0x50 speed ramp against the +0x54 rate
     * (0x800 from the case-0 entry); the two smallest bands share
     * the 0x1000 floor. */
    int32_t rate = e->ay;
    if (dist > 0x28) {
        if (e->ax <= 0x27FFF) e->ax += rate * 2;
    } else if (dist > 0x20) {
        if (e->ax > 0x20000) e->ax -= rate * 3;
    } else if (dist > 0x18) {
        if (e->ax > 0x18000) e->ax -= rate * 2;
    } else if (dist > 0xC) {
        if (e->ax > 0x10000) e->ax -= rate * 2;
    } else if (dist > 4) {
        if (e->ax > 0x8000) e->ax -= rate * 2;
    } else {
        if (e->ax > 0x1000) e->ax -= rate * 2;
        else if (e->ax <= 0xFFF) e->ax = 0x1000;
    }
    /* Velocity = the angle's sine pair times the speed (the native
     * sub_0803E058 affine between the lookups and the multiply is
     * visual-only — unported). */
    e->vx = aos_fixed_mul(aos_sine((bang + 0x4000) & 0xFFFF), e->ax);
    e->vy = aos_fixed_mul(aos_sine(bang & 0xFFFF), e->ax);
    e->x += e->vx;                      /* sub_0806D3AC */
    e->y += e->vy;
    e->timer++;
    e->ch2_t22++;
    /* The +0x42/+0x46 low-byte deltas feed the trail record: the
     * new pixel low byte minus the sign-extended stored one. */
    int32_t npx = (int16_t)(e->x >> 16), npy = (int16_t)(e->y >> 16);
    e->ch2_bdx = (uint8_t)((uint8_t)npx - (uint8_t)(int8_t)oldx);
    e->ch2_bdy = (uint8_t)((uint8_t)npy - (uint8_t)(int8_t)oldy);
    ch2_body_trail(e, e->ch2_bdx, e->ch2_bdy, e->ch2_banim);
    if (dist <= 2) {
        /* Arrival: the group transition table. grp 0 -> 2 with a
         * rand & 7 re-aim and +0x65 = 5; grp 1 -> the case-3
         * attack; grp 2 -> 3 with +0x5C = 0x19, +0x72 |= 6 and the
         * dual-mode pick; grp 3 rolls the 0x20/0x40 gates into
         * groups 0/1/2 with +0x5C = 0x11 and +0x72 &= ~6. All
         * re-arm +0x14 = 0. */
        switch (e->ch2_bgrp) {
        case 0: {
            uint8_t old = e->ch2_blmb;
            e->step = 1;
            e->ch2_bgrp = 2;
            do {
                e->ch2_blmb = (uint8_t)(random() & 7);
            } while (e->ch2_blmb == old);
            e->ch2_banim = 5;
            break;
        }
        case 1:
            e->step = 3;
            e->timer = 0;
            e->substep = 0;
            break;
        case 2:
            e->step = 1;
            e->ch2_bcnt = 0x19;
            e->combat.attack_off = true;    /* +0x72 |= 6 */
            e->combat.hurt_off = true;
            e->ch2_bgrp = 3;
            ch2_body_pick(e, random);
            break;
        default:
            e->step = 1;
            if (e->ch2_blmb <= 3 && (random() & 0x7F) <= 0x20) {
                e->ch2_bcnt = 0x11;
                e->combat.attack_off = false;   /* +0x72 &= ~6 */
                e->combat.hurt_off = false;
                e->ch2_bgrp = 0;
            } else {
                e->ch2_bcnt = 0x11;
                e->combat.attack_off = false;
                e->combat.hurt_off = false;
                if ((random() & 0x7F) <= 0x40) {
                    e->ch2_bgrp = 1;
                } else {
                    e->ch2_bgrp = 2;
                }
                ch2_body_pick(e, random);
            }
            break;
        }
        e->ch2_turn = 0;                /* +0x14 = 0 */
    } else if (e->ch2_t22 > 0x12B) {
        /* The stuck watchdog: re-aim +0x1B at rand & 3 (no +0x14
         * reset, no group change). */
        e->step = 1;
        uint8_t r;
        do {
            r = (uint8_t)(random() & 3);
        } while (r == e->ch2_blmb);
        e->ch2_blmb = r;
    }
    /* _080A6104: the parent's +0xA == 3 death entry — velocity and
     * speed zeroed, +0x54 = 0x400000, +0xD = 0x28. The case-3
     * attack skips this check natively (it finishes first). */
    if (e->parent && e->parent->state == 3) {
        e->state = 0xA;
        e->step = 0;
        e->vx = e->vy = e->ax = 0;
        e->ay = 0x400000;
        e->timer = 0x28;
    }
    (void)soma;
    (void)kind;
    (void)weapon;
    (void)weapon_frames;
    (void)soma_atk;
    (void)soma_def;
    (void)report;
}

/* The case-3 attack (sub_080A5A7C's +0xB == 3 block): the
 * 0x085283CC sequence on +0x6D/+0x6E/+0x6F/+0x59 replayed through
 * ch2_idx/ch2_frame/ch2_done; +0x6E == 9 (the 30-tick mouth-open
 * record) fires a sub_080A65C0 shot and song 0x147 every frame;
 * the sequence end latches +0x59 bit 0 which the next frame turns
 * into grp 2 / step 1 / +0x1B = (rand & 3) + 4. The trail record
 * writes {0, 0, 5, +0x5C, angle} every frame. */
static void ch2_body_attack(AosEnemy *e, const AosAnimSet *anims,
                            uint32_t (*random)(void),
                            AosHitReport *report) {
    if (e->substep == 0) {
        /* sub_0803F0E4 prime: it runs one sub_0803F17C step — +0x65
         * takes record 0's frame (0x0B) and the zero-tick record
         * advances +0x6E straight to 1; song 0x77 plays. */
        e->substep = 1;
        e->ch2_idx = 0;
        e->ch2_frame = 0;
        e->ch2_done = false;
        e->ch2_banim = CH2_BODY_ATK[0][0];
        e->ch2_frame = 1;
        if (e->ch2_frame >= CH2_BODY_ATK[0][1]) {
            e->ch2_frame = 0;
            e->ch2_idx = 1;
        }
        play_loop(e, anims, 1, true);   /* stand-in for 0x08226B58 */
        aos_audio_play_song(0x77);
    }
    if (e->ch2_idx == 9) {
        /* +0x6E == 9: the {9,3} record's three frames each fire a
         * sub_080A65C0 shot and song 0x147. */
        ch2_spawn_body_shot(e, e->ch2_bang, anims, report);
        aos_audio_play_song(0x147);
    }
    if (e->ch2_done) {
        /* +0x59 bit 0: grp 2, step 1, +0xC = 0 and +0x1B =
         * (rand & 3) + 4 — then the shared trail/collide tail. */
        e->ch2_bgrp = 2;
        e->step = 1;
        e->substep = 0;
        e->ch2_blmb = (uint8_t)((random() & 3) + 4);
    }
    /* sub_0803F17C / sub_0803EC34: +0x65 takes the current record's
     * frame, +0x6F ticks against its duration and +0x6E advances;
     * the end pins +0x6E at 11, sets +0x6C bit 2 and latches
     * +0x59 bit 0. */
    if (!e->ch2_done) {
        e->ch2_banim = CH2_BODY_ATK[e->ch2_idx][0];
        e->ch2_frame++;
        if (e->ch2_frame >= CH2_BODY_ATK[e->ch2_idx][1]) {
            e->ch2_frame = 0;
            e->ch2_idx++;
            if (e->ch2_idx >= 12) {
                e->ch2_done = true;
                e->ch2_idx = 11;
            }
        }
    }
    ch2_body_trail(e, 0, 0, 5);
}

/* sub_080A5A7C: the crawler body update. +0xA 0 alive / 0xA the
 * death tail; +0xB the crawl substate (0/1 the target acquire, 2
 * the crawl, 3 the attack). The top watch deletes into the death
 * entry when the parent record is gone or inactive — natively the
 * room-byte compare on +0x36, ported as id/removed (documented);
 * a missing parent never reaches collide. */
static void ch2_body_update(AosEnemy *e, AosSoma *soma,
                            const AosEnemyKind *kind,
                            const AosWeaponEntity *weapon,
                            const AosWeaponFrames *weapon_frames,
                            int soma_atk, int soma_def,
                            uint32_t (*random)(void),
                            AosHitReport *report,
                            const AosAnimSet *anims) {
    /* The limbs spawn with the body natively (same sub_080A57BC
     * call); the port emits them on the first update. */
    if (!e->boss_parts_spawned) {
        ch2_body_emit_limbs(e, anims, report);
        e->boss_parts_spawned = true;
    }
    if (e->state == 0xA) {
        /* _080A622C: flame every 4th tick, sub_0806D3D8(0x70000),
         * the blink on +0xD and the heap free / palette restore at
         * expiry (visual parts folded, documented). */
        if ((e->timer & 3) == 0)
            spawn_flame_at(e->x, e->y - 0x60000, 0x100000, 0xC00000,
                           random, report);
        aos_enemy_accel_move(e, 0x70000);
        e->hidden = ch2_blink(kind, e->timer);
        e->timer--;
        if ((int8_t)e->timer <= 0)
            e->removed = true;
        return;
    }
    AosEnemy *p = e->parent;
    if (!p || p->removed || p->id != 0x70) {
        /* _080A61F0: the orphaned-body death entry — +0x34 = 0,
         * +0xD = 0x28, +0x50 = 0, +0x54 = 0x400000; no collide. */
        e->state = 0xA;
        e->step = 0;
        e->hp = 0;
        e->timer = 0x28;
        e->hit_flash = 0;
        e->ax = 0;                  /* +0x50; +0x48/+0x4C keep
                                     * their velocity natively */
        e->ay = 0x400000;           /* +0x54 */
        return;
    }
    switch (e->step) {
    case 0:
        /* +0x54 = 0x80 << 4 then the shared acquire below takes
         * +0xB straight to 2. */
        e->ay = 0x800;
        /* FALLTHROUGH */
    case 1:
        ch2_body_target(e);
        e->step = 2;
        e->timer = 0;               /* +0xD */
        e->ch2_t22 = 0;             /* +0x22 */
        ch2_body_trail(e, 0, 0, e->ch2_banim);
        break;
    case 2:
        ch2_body_crawl(e, soma, kind, weapon, weapon_frames,
                       soma_atk, soma_def, random, report);
        break;
    case 3:
        ch2_body_attack(e, anims, random, report);
        break;
    default:
        break;
    }
    /* _080A61DE: the contact pass runs for every alive substate. */
    AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                               soma_atk, soma_def);
    skeleton_merge(report, &hit);
    step_anim(e, anims);            /* stand-in for the +0x65 OAM
                                     * frames (documented) */
}

/* sub_080A62B8: a crawler limb. +0xA 0 alive / 1 the death tail
 * (the native writes state 1, not 0xA). Alive: the body watch
 * (id/removed — the +0x36 room byte has no runtime counterpart),
 * the +0x58 bit-5 copy, +0xD++ and the trail sample — record
 * 8*index+8's bytes drive the +0x4A/+0x4E steps integrated into
 * the +0x42/+0x46 pixel words, the +0x5C counter, the +0x72 combat
 * bits (both off unless the sampled counter is 0x12), the
 * +0x18/+0x19 refresh, the index-7 affine read (unported), the
 * +0x65 frame select, the byte-0 facing sign and the every-
 * eighth-tick contact pass. Body +0xA == 0xA enters the tail:
 * +0x50/+0x54 = 0/0x400000, +0xD = (rand & 3) + 0x3C then the
 * sub_0806D3D8/blink/countdown like sub_080A4E80's. */
static void ch2_body_limb_update(AosEnemy *e, AosSoma *soma,
                                 const AosEnemyKind *kind,
                                 const AosWeaponEntity *weapon,
                                 const AosWeaponFrames *weapon_frames,
                                 int soma_atk, int soma_def,
                                 uint32_t (*random)(void),
                                 AosHitReport *report) {
    AosEnemy *b = e->parent;
    if (e->state != 0) {
        /* The +0xA != 0 tail: +0xB latches once (+0x59 |= 2 has no
         * runtime field — documented), then integrate/blink/
         * countdown; index 7's palette restore is visual-only. */
        if (e->step == 0)
            e->step = 1;
        aos_enemy_accel_move(e, 0x70000);
        e->hidden = ch2_blink(kind, e->timer);
        e->timer--;
        if ((int8_t)e->timer <= 0)
            e->removed = true;
        return;
    }
    if (!b || b->removed || b->id != e->id) {
        e->removed = true;          /* EntityDelete (+0x36 band
                                     * compare, ported as id) */
        return;
    }
    e->hidden = b->hidden;          /* +0x58 bit 5 copy */
    e->timer++;                     /* +0xD++ */
    /* The trail sample: limb index * 48 bytes into the heap puts
     * record 8*index+8 at offsets +0x30..+0x35. */
    const uint8_t *s = b->ch2_trail + e->variant * 48;
    int32_t stepx = (int8_t)s[0x30] +
        ((int32_t)e->ch2_lox - (int32_t)b->ch2_box);
    int32_t stepy = (int8_t)s[0x31] +
        ((int32_t)e->ch2_loy - (int32_t)b->ch2_oy);
    /* +0x42/+0x46 += the s16 steps — u16 pixel words of +0x40/+0x44. */
    uint16_t px = (uint16_t)((e->x >> 16) & 0xFFFF);
    uint16_t py = (uint16_t)((e->y >> 16) & 0xFFFF);
    px = (uint16_t)(px + (uint16_t)stepx);
    py = (uint16_t)(py + (uint16_t)stepy);
    e->x = (e->x & 0xFFFF) | ((int32_t)(uint32_t)px << 16);
    e->y = (e->y & 0xFFFF) | ((int32_t)(uint32_t)py << 16);
    e->ch2_lcnt = (uint8_t)(s[0x33] + 1);    /* +0x5C */
    if (e->ch2_lcnt == 0x12) {
        e->combat.attack_off = false;       /* +0x72 &= ~6 */
        e->combat.hurt_off = false;
    } else {
        e->combat.attack_off = true;        /* +0x72 |= 6 */
        e->combat.hurt_off = true;
    }
    e->ch2_lox = (uint8_t)b->ch2_box;       /* +0x18/+0x19 refresh */
    e->ch2_loy = (uint8_t)b->ch2_oy;
    int8_t sel = (int8_t)s[0x32];            /* the +0x65 select byte */
    if (e->variant == 7) {
        /* The affine read (record +0x34/+0x35 through sub_0803E058)
         * is unported; the +0x65 map is 5->5, 6->0xA, 0xA->6, 0xB->6. */
        if (sel == 5) e->ch2_anim = 5;
        else if (sel == 6) e->ch2_anim = 0xA;
        else if (sel == 0xA || sel == 0xB) e->ch2_anim = 6;
    } else {
        if (sel == 5) e->ch2_anim = 0x0E;
        else if (sel == 0xB) e->ch2_anim = 0x0F;
    }
    /* The byte-0 sign drives +0x58 bit 6 for indices 0-6 only
     * (index 7 skips the facing block natively); the negative side
     * clears bits 0x40|0x01 — bit 0 has no runtime field
     * (documented). */
    if (e->variant != 7) {
        int8_t dir = (int8_t)s[0x30];
        if (dir > 0) {
            e->mirrored = true;
        } else if (dir < 0) {
            e->mirrored = false;
        }
    }
    if ((e->timer & 7) == 0) {
        AosHitReport hit = collide(e, soma, kind, weapon,
                                   weapon_frames, soma_atk, soma_def);
        skeleton_merge(report, &hit);
    }
    if (b->state == 0xA) {
        /* Body death: +0xA = 1, +0xB = 0, +0x50 = 0,
         * +0x54 = 0x400000, +0xD = (rand & 3) + 0x3C. */
        e->state = 1;
        e->step = 0;
        e->ax = 0;
        e->ay = 0x400000;
        e->timer = (uint8_t)((random() & 3) + 0x3C);
    }
}

/* sub_080A6724: the mouth shot — position += velocity, the contact
 * pass, the ±8/+9 px bounce while +0xD <= 0x63, the margin entry-1
 * cull, the +0xD > 0xEF cap and the Chaos watch: the grandparent
 * record must be active, in the same room band and out of state 3
 * or the shot deletes. The body itself is not checked natively —
 * shots outlive it (documented). */
static void ch2_body_shot_update(AosEnemy *e, AosSoma *soma,
                                 const AosEnemyKind *kind,
                                 const AosWeaponEntity *weapon,
                                 const AosWeaponFrames *weapon_frames,
                                 int soma_atk, int soma_def,
                                 int cam_x, int cam_y,
                                 const AosAnimSet *anims,
                                 AosHitReport *report) {
    e->x += e->vx;                      /* sub_0806D3AC */
    e->y += e->vy;
    AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                               soma_atk, soma_def);
    skeleton_merge(report, &hit);
    if ((uint8_t)e->timer <= 0x63) {
        if ((uint32_t)(e->x - 0x80000) > 0xE00000u)
            e->vx = -e->vx;
        if ((uint32_t)(e->y - 0x80000) > 0x900000u)
            e->vy = -e->vy;
    }
    int sx = (int)(e->x >> 16) - cam_x, sy = (int)(e->y >> 16) - cam_y;
    if (aos_enemy_outside_margins(kind, 1, sx, sy)) {
        e->removed = true;
        return;
    }
    uint8_t old = e->timer;
    e->timer = (uint8_t)(old + 1);
    if (old > 0xEF) {
        e->removed = true;
        return;
    }
    AosEnemy *gp = e->parent ? e->parent->parent : NULL;
    if (!gp || gp->removed || gp->id != 0x70 || gp->state == 3) {
        e->removed = true;
        return;
    }
    step_anim(e, anims);
}

/* ------------------------------------------------------------------ */
/* The + 0xA state machines.                                           */
/* ------------------------------------------------------------------ */

/* sub_080A4728: the intro — eleven cases on + 0xB. The SetPlayerInput
 * lock, palette fades/restores and the gEwramData + 0xA074/+ 0x42C
 * story writes have no runtime counterpart (documented); case 10's
 * sub_0803D06C gate is treated as nonzero so the early-exit stays
 * dormant. */
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
                 * writes fold away; sub_080A57BC(e,0,0,0) spawns
                 * the crawler body (its limbs follow on its first
                 * update). */
                ch2_spawn_body(e, anims, report);
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
         * {1,3,4,5} plays 0x167 — natively those bytes are driven
         * through the phase-2 children's shared +0x6C/+0x6D/+0x6E/
         * +0x6F anim-sequencer fields (the sub_080A57BC body runs
         * its own copy on its own record); no ported path writes
         * them on the Chaos record, so the gate stays dormant
         * (documented). The step then waits on + 0x59 bit 0 — the
         * anim-ended marker of the 0x11 play — ported as
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
    case AOS_ROLE_CHAOS_BODY:
        ch2_body_update(enemy, soma, kind, weapon, weapon_frames,
                        soma_atk, soma_def, random, &report, kind->anims);
        aos_combat_tick(&enemy->combat);
        return report;
    case AOS_ROLE_CHAOS_BODY_LIMB:
        ch2_body_limb_update(enemy, soma, kind, weapon, weapon_frames,
                             soma_atk, soma_def, random, &report);
        aos_combat_tick(&enemy->combat);
        return report;
    case AOS_ROLE_CHAOS_BODY_SHOT:
        ch2_body_shot_update(enemy, soma, kind, weapon, weapon_frames,
                             soma_atk, soma_def, cam_x, cam_y,
                             kind->anims, &report);
        aos_combat_tick(&enemy->combat);
        return report;
    default:
        break;
    }
    bool phase2 = enemy->param1 != 0;
    if (phase2 && enemy->state != 3 && enemy->hit_flash)
        enemy->hit_flash--;     /* EnemyChaos2Update's sub_0806AE54 */
    /* The sub_080A2A4C orbit parts are a phase-1-only spawn; the
     * phase-2 crawler body comes from sub_080A57BC via the intro's
     * case-4 trigger. */
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
    if (phase2 && (hit.killed || (!enemy->defeated && enemy->hp <= 0)) &&
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
