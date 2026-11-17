/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Big Golem (boss 0x45).
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../../aos/core/aos_enemy_internal.h"
#include "../../aos/core/aos_audio.h"
#include "../../aos/math/aos_math.h"

#include <string.h>

#define accelerate_toward aos_enemy_accelerate_toward
#define accel_move aos_enemy_accel_move
#define collide aos_enemy_collide
#define face_player aos_enemy_face_player
#define move_toward aos_enemy_move_toward
#define play aos_enemy_play
#define play_loop aos_enemy_play_loop
#define player_in_range aos_enemy_player_in_range
#define skeleton_merge aos_enemy_skeleton_merge
#define spawn_drift aos_enemy_spawn_drift
#define spawn_flame_at aos_enemy_spawn_flame_at
#define step_anim aos_enemy_step_anim

#define BG_PARTS 14

/* The sub_080AC6E8 decode: the four... fourteen sub_0806DFF8
 * children of EnemyBigGolemCreate (one per part index of the
 * identity table 0x0811930C, loop bound 0xD) each run the child
 * brain on the boss's + 0x1C container of fourteen 0x38-byte
 * records.  Ported here: the record bake (the 0x08528414 home
 * pairs and the 0x0852858C angle dwords picked by the boss + 0x20
 * variant byte), the attached-state position/aim machine, the
 * state-2 launch into the flight machine and its delete.  The
 * create tables 0x081192F0 (the + 0x44 limb-art byte — the
 * 0x0825EB0C/0x08266E78 sheet is not exported), the + 0x5A/+ 0x5C
 * palette and priority merges, the parent + 0x22 per-part attack
 * bitfield and the + 0x72 flag writes, the sub_08042848/428B4/
 * 429D0 palette flashes and the sub_0803E058/sub_0803E6BC affine
 * rotation setup (the renderer has no BG rotation entry) are not
 * modeled.  The sub_080AC014 record stepper, the sub_080AC468/
 * sub_080AC314 armers, the sub_080ACFA0/sub_080ACF6C wrappers and
 * the sub_080ACEC4/sub_080ACF00/sub_080ACF10/sub_080ACF30/
 * sub_080ACF58/sub_080ACFD4/sub_080ACFF0/sub_080AC298 helpers are
 * ported below as aos_enemy_bg_*; the state-0/state-1 case
 * machines that drive them stay collapsed, so the stepper is not
 * wired into the per-frame update yet (the native machines only
 * step records they armed, and the collapsed cycles arm none — an
 * unarmed record's + 0x28 counter would drive the idle ease
 * unstable).  The sub_0806D518
 * blink folds into kind->blink and the sub_0806D5C0 shot spawn
 * into spawn_flame_at. */

/* 0x081192FE: the child + 0x21 aim-target record index. */
static const uint8_t bg_tgt_tab[BG_PARTS] = {
    0, 1, 0, 3, 5, 5, 6, 7, 6, 9, 10, 11, 12, 12,
};
/* 0x08119352: the launch timer byte (read as int8, stored as a
 * byte — the high values are long flights). */
static const int8_t bg_timer_tab[BG_PARTS] = {
    -52, -78, -66, -84, -106, 68, -56, -76,
    -64, -122, 124, 108, 80, 90,
};
/* 0x08119360: the int16 launch-release distance offsets added to
 * the parent y pixel plus the bgInfo[1] scroll y. */
static const int16_t bg_dist_tab[BG_PARTS] = {
    -12, -32, -48, -24, -4, -4, -12, -32, -48, -24, -4, -4, -12, -32,
};
/* 0x08528414: the fourteen home (x, y) pixel pairs stored << 16;
 * the 0x0852844C pointer table selects this one table for every
 * variant. */
static const int16_t bg_home[BG_PARTS][2] = {
    {10, -5}, {6, -74}, {8, -53}, {6, -74},
    {2, -80}, {-26, -124}, {10, -5}, {6, -74},
    {8, -53}, {-9, -111}, {-9, -78}, {-9, -94},
    {-9, -71}, {-9, -27},
};
/* 0x0852858C: thirty 8-byte rows picked by the boss + 0x20 variant
 * byte; the create stores the first dword into record 5's + 8 and
 * the second into record 12's + 8. */
static const int32_t bg_ang_tab[30][2] = {
    {0x10000, 0x11000}, {0x10000, 0x11000}, {0x10800, 0x13800},
    {0x10000, 0x15000}, {0x10800, 0x0F800}, {0x11000, 0x0E000},
    {0x10800, 0x13800}, {0x11800, 0x13800}, {0x10800, 0x13400},
    {0x10800, 0x13800}, {0x10800, 0x13800}, {0x11800, 0x13800},
    {0x10800, 0x13400}, {0x10800, 0x13800}, {0x10800, 0x15200},
    {0x11800, 0x15200}, {0x10800, 0x15200}, {0x10800, 0x15000},
    {0x10800, 0x15200}, {0x11800, 0x15200}, {0x10800, 0x15200},
    {0x10800, 0x15000}, {0x11000, 0x17C00}, {0x11800, 0x17C00},
    {0x12000, 0x15000}, {0x12000, 0x15000}, {0x11000, 0x17C00},
    {0x11800, 0x17C00}, {0x12000, 0x15000}, {0x12000, 0x15000},
};

/* The ROM's sign-fix >> 16 on the record deltas: truncation toward
 * zero, like C division (the cs_px convention). */
static int32_t bg_px(int32_t v) { return v / 0x10000; }

/* ---- the + 0x1C record engine ----
 *
 * sub_080AC014 steps one record per frame: an unarmed record
 * (+ 0x0C/+ 0x10 both zero) eases x/y toward its 0x08528414 home
 * pair — x negated while the body + 0x58 bit 0x40 mirror is set —
 * over the + 0x28 counter (a zero counter divides by one, then the
 * field keeps decrementing, exactly like the ROM); records 5 and
 * 0x0C also ease their + 8 angle toward the 0x0852858C variant
 * pair.  An armed record interpolates + 0x10 toward + 0x0C over the
 * same divisor, then places x/y on the sine/cosine of the new angle
 * at the + 0x24 radius around either the + 0x14/+ 0x18 fixed point
 * or the + 0x18 record's position when + 0x14 is 0x80000000.  The
 * + 0x34 phase walks 0 (drift) -> 1 (attack arc: + 0x28 counted
 * down to half of + 0x2C) -> 2 (+ 0x28 held while + 0x2C climbs to
 * + 0x30) -> 3 (held).  Only records 5 and 0x0C update + 8. */
static int32_t bg_div(int32_t v, int32_t d) {
    /* __divsi3 by zero is not reachable in the native choreography
     * (the counters stay positive while a record is stepped); the
     * guard keeps the port defined if a caller steps a record whose
     * counter already ran out. */
    return d ? v / d : 0;
}

void aos_enemy_bg_step_record(AosEnemy *e, unsigned idx) {
    if (idx >= BG_PARTS)
        return;
    AosBgRec *r = &e->bg_rec[idx];
    int32_t var = (int8_t)e->bg_var;
    const int32_t *angp = bg_ang_tab[var >= 0 && var < 30 ? var : 0];
    int32_t sl = r->cnt ? r->cnt : 1;
    r->cnt -= 1;
    bool mir = e->mirrored;
    if (r->a_lo == 0 && r->a_hi == 0) {
        int32_t hx = (int32_t)bg_home[idx][0] << 16;
        int32_t hy = (int32_t)bg_home[idx][1] << 16;
        if (mir)
            hx = -hx;
        r->x += bg_div(hx - r->x, sl);
        r->y += bg_div(hy - r->y, sl);
        if (idx != 5 && idx != 0x0C)
            return;
        int32_t tgt = idx == 5 ? angp[0] : angp[1];
        if (mir)
            tgt = 0x20000 - tgt;
        r->ang += bg_div(tgt - r->ang, sl);
        return;
    }
    if (r->phase == 1) {
        if (r->cnt <= r->cnt2 / 2)
            r->phase = 2;
    } else if (r->phase == 2) {
        r->cnt += 1;
        r->cnt2 += 1;
        sl = r->cnt;
        if (r->cnt2 >= r->lim)
            r->phase = 3;
    }
    r->a_hi += bg_div(r->a_lo - r->a_hi, sl);
    int32_t ox = aos_fixed_mul(aos_sine((uint32_t)(r->a_hi + 0x4000)),
                               r->dist);
    int32_t oy = aos_fixed_mul(aos_sine((uint32_t)r->a_hi), r->dist);
    if (r->anchor == (int32_t)0x80000000) {
        AosBgRec *a = &e->bg_rec[r->anchor2];
        r->x = a->x + ox;
        r->y = a->y + oy;
    } else {
        r->x = r->anchor + ox;
        r->y = r->anchor2 + oy;
    }
    if (idx != 5 && idx != 0x0C)
        return;
    int32_t tgt = idx == 5 ? angp[0] : angp[1];
    if (mir)
        tgt = 0x20000 - tgt;
    r->ang += bg_div(tgt - r->ang, sl);
}

/* sub_080ACF30: step the fourteen records in the caller's signed
 * byte order, only while the + 0x0D timer byte is nonzero. */
void aos_enemy_bg_step_order(AosEnemy *e, const int8_t *order) {
    if (e->timer == 0)
        return;
    for (unsigned i = 0; i < BG_PARTS; ++i)
        aos_enemy_bg_step_record(e, (unsigned)(int8_t)order[i]);
}

/* sub_080ACF10: same gate, natural index order. */
void aos_enemy_bg_step_all(AosEnemy *e) {
    if (e->timer == 0)
        return;
    for (unsigned i = 0; i < BG_PARTS; ++i)
        aos_enemy_bg_step_record(e, i);
}

/* sub_080AC468 (and sub_080AC314 with rel): arm record idx toward
 * record other — + 0x34 phase 0, + 0x28/+ 0x2C the duration, + 0x14
 * the record-index anchor flag, + 0x18 the target record, + 0x24 the
 * orbit radius (computed from the record delta through
 * sub_0803E86C/Sqrt when dist is zero), + 0x10 the ArcTan2 bearing
 * plus 0x10000 and + 0x0C the mirrored/unmirrored angle bound.
 * sub_080AC314 additionally copies + 0x0C into + 0x10 of the slot
 * at byte offset (other*8 - idx)*8 — with a computed distance that
 * is record idx itself (a no-op copy), with an explicit distance it
 * is record other's — ported literally. */
static void bg_arm_rec(AosEnemy *e, unsigned idx, unsigned other,
                       int32_t da, int32_t dist, int32_t dur,
                       bool rel) {
    if (idx >= BG_PARTS || other >= BG_PARTS)
        return;
    AosBgRec *r = &e->bg_rec[idx];
    AosBgRec *t = &e->bg_rec[other];
    r->phase = 0;
    r->cnt = dur;
    r->cnt2 = dur;
    r->anchor = (int32_t)0x80000000;
    r->anchor2 = (int32_t)other;
    if (dist)
        r->dist = dist;
    else {
        int32_t dx = t->x - r->x;
        int32_t dy = t->y - r->y;
        int32_t sq = aos_fixed_mul(dx, dx) + aos_fixed_mul(dy, dy);
        r->dist = (int32_t)(aos_isqrt((uint32_t)sq) << 8);
    }
    int32_t dx = t->x - r->x;
    int32_t dy = t->y - r->y;
    uint16_t ang = aos_arctan2((int16_t)bg_px(dx), (int16_t)bg_px(dy));
    int32_t base = (int32_t)ang + 0x10000;
    r->a_hi = base;
    int32_t a = da < 0 ? -da : da;
    if (a <= 0xFFFF) {
        r->a_lo = e->mirrored ? base - da : base + da;
    } else if (e->mirrored) {
        r->a_lo = da > 0x18000 ? 0x38000 - da : 0x28000 - da;
    } else {
        r->a_lo = da;
    }
    if (rel) {
        int32_t off = ((dist ? (int32_t)idx : (int32_t)other) * 8 -
                       (int32_t)idx) * 8;
        int32_t *p = (int32_t *)((char *)e->bg_rec + off);
        p[4] = p[3];
    }
}

void aos_enemy_bg_arm(AosEnemy *e, unsigned idx, unsigned other,
                      int32_t da, int32_t dist, int32_t dur) {
    bg_arm_rec(e, idx, other, da, dist, dur, false);
}

void aos_enemy_bg_arm_rel(AosEnemy *e, unsigned idx, unsigned other,
                          int32_t da, int32_t dist, int32_t dur) {
    bg_arm_rec(e, idx, other, da, dist, dur, true);
}

/* sub_080ACFA0 / sub_080ACF6C: the attack wrappers — arm with half
 * the duration as the + 0x28/+ 0x2C seed, then + 0x34 phase 1 and
 * + 0x30 the full duration (sub_080ACFA0 uses sub_080AC468,
 * sub_080ACF6C sub_080AC314). */
static void bg_arm_attack(AosEnemy *e, unsigned idx, unsigned other,
                          int32_t da, int32_t dist, int32_t dur,
                          bool rel) {
    bg_arm_rec(e, idx, other, da, dist, dur / 2, rel);
    if (idx < BG_PARTS) {
        e->bg_rec[idx].phase = 1;
        e->bg_rec[idx].lim = dur;
    }
}

void aos_enemy_bg_arm_attack(AosEnemy *e, unsigned idx, unsigned other,
                             int32_t da, int32_t dist, int32_t dur) {
    bg_arm_attack(e, idx, other, da, dist, dur, false);
}

void aos_enemy_bg_arm_attack_rel(AosEnemy *e, unsigned idx,
                                 unsigned other, int32_t da,
                                 int32_t dist, int32_t dur) {
    bg_arm_attack(e, idx, other, da, dist, dur, true);
}

/* sub_080ACF58: clear the + 0x0C/+ 0x10 angle pair (disarm). */
void aos_enemy_bg_disarm(AosEnemy *e, unsigned idx) {
    if (idx >= BG_PARTS)
        return;
    e->bg_rec[idx].a_lo = 0;
    e->bg_rec[idx].a_hi = 0;
}

/* sub_080ACEC4: shift the body x by half the sum of two records and
 * pull every record x back by the same amount (re-centre the limb
 * layout on records i/j). */
void aos_enemy_bg_centroid(AosEnemy *e, unsigned i, unsigned j) {
    if (i >= BG_PARTS || j >= BG_PARTS)
        return;
    int32_t d = (e->bg_rec[i].x + e->bg_rec[j].x) / 2;
    e->x += d;
    for (unsigned k = 0; k < BG_PARTS; ++k)
        e->bg_rec[k].x -= d;
}

/* sub_080ACF00: write a record's y. */
void aos_enemy_bg_set_y(AosEnemy *e, unsigned idx, int32_t y) {
    if (idx < BG_PARTS)
        e->bg_rec[idx].y = y;
}

/* sub_080ACFD4 / sub_080ACFF0: save and restore every record's
 * position through + 0x1C/+ 0x20. */
void aos_enemy_bg_store_pos(AosEnemy *e) {
    for (unsigned i = 0; i < BG_PARTS; ++i) {
        e->bg_rec[i].px = e->bg_rec[i].x;
        e->bg_rec[i].py = e->bg_rec[i].y;
    }
}

void aos_enemy_bg_load_pos(AosEnemy *e) {
    for (unsigned i = 0; i < BG_PARTS; ++i) {
        e->bg_rec[i].x = e->bg_rec[i].px;
        e->bg_rec[i].y = e->bg_rec[i].py;
    }
}

/* sub_080AC298 (with sub_0806CF2C): flip the + 0x58 bit-0x40 mirror
 * when the body's pixel x sits on the wrong side of the player's —
 * negate every record x and mirror the + 8 angle of records
 * {2,4,5,8,0x0C,0x0D} as 0x20000 - ang.  Called once by
 * EnemyBigGolemCreate; the port runs it when the part records are
 * first baked. */
void aos_enemy_bg_face_flip(AosEnemy *e, const AosSoma *soma) {
    if (!soma)
        return;
    int want = (int32_t)(int16_t)(e->x >> 16) -
               (int32_t)(int16_t)(soma->x >> 16) < 0;
    if (want == (e->mirrored ? 1 : 0))
        return;
    e->mirrored = !e->mirrored;
    for (unsigned i = 0; i < BG_PARTS; ++i)
        e->bg_rec[i].x = -e->bg_rec[i].x;
    static const uint8_t flip_idx[] = {2, 4, 5, 8, 0x0C, 0x0D};
    for (unsigned i = 0; i < sizeof flip_idx; ++i)
        e->bg_rec[flip_idx[i]].ang = 0x20000 - e->bg_rec[flip_idx[i]].ang;
}


/* Emit the fourteen part children over the first updates under the
 * room child cap, like the Creaking Skull bones and the Great Armor
 * parts.  The first call also bakes the + 0x1C records: the 0x08528414
 * home pairs for every record, record 5's + 8 from the variant row's
 * first dword and record 12's + 8 from the second. */
static void bg_emit_parts(AosEnemy *e, AosHitReport *hit) {
    if (e->spawned_count == 0) {
        unsigned var = e->variant < 30 ? e->variant : 0;
        e->bg_var = (uint8_t)var;
        for (unsigned i = 0; i < BG_PARTS; ++i) {
            e->bg_rec[i].x = (int32_t)bg_home[i][0] << 16;
            e->bg_rec[i].y = (int32_t)bg_home[i][1] << 16;
            e->bg_rec[i].ang = 0x10000;
        }
        e->bg_rec[5].ang = bg_ang_tab[var][0];
        e->bg_rec[12].ang = bg_ang_tab[var][1];
    }
    while (e->spawned_count < BG_PARTS &&
           hit->child_count < AOS_ENEMY_MAX_CHILDREN) {
        unsigned i = e->spawned_count++;
        AosEnemy c;
        memset(&c, 0, sizeof c);
        c.id = e->id;
        c.x = e->x;
        c.y = e->y;
        c.role = AOS_ROLE_BG_PART;
        c.variant = (uint8_t)i;
        c.bg_tgt = bg_tgt_tab[i];
        c.parent = e;
        c.static_frame = -1;
        c.hp = 1;
        c.combat = (AosCombat){.type = AOS_TYPE_ENEMY, .attack_off = true,
                               .hurt_off = true};
        hit->children[hit->child_count++] = c;
    }
}

/* sub_080AC6E8 state 0 (the + 0x0A == 0 attached part): the position
 * is the parent's plus the record offset; records {2,4,8,0xD} aim at
 * their + 0x21 target record through ArcTan2 and write the held
 * angle back to the record's + 8 (record 4 adds 0x2700 mirrored /
 * 0x5900 unmirrored, the others -0x4000), records {5,0xC} hold it.
 * A state-2 parent launches the part into the flight machine below;
 * the parent + 0x22 bitfield palette beats, the + 0x5A/+ 0x58 merges
 * beyond the mirror bit and the sub_0803E058 affine setup are not
 * modeled. */
static void bg_attached(AosEnemy *child, AosEnemy *body,
                        uint32_t (*random)(void), int cam_y) {
    unsigned idx = child->variant < BG_PARTS ? child->variant : 0;
    if (!body || body->removed || body->id != 0x45) {
        child->removed = true;
        return;
    }
    /* The + 0x36 compare is the entity id byte on both sides (0x45). */
    child->mirrored = body->mirrored;
    child->timer++; /* + 0xD frame counter */
    AosBgRec *r = &body->bg_rec[idx];
    child->x = body->x + r->x;
    child->y = body->y + r->y;
    if (idx == 2 || idx == 4 || idx == 8 || idx == 0x0D) {
        unsigned t = child->bg_tgt < BG_PARTS ? child->bg_tgt : 0;
        int32_t dx = body->bg_rec[t].x - r->x;
        int32_t dy = body->bg_rec[t].y - r->y;
        int32_t ang = (int32_t)(aos_arctan2((int16_t)bg_px(dx),
                                            (int16_t)bg_px(dy)) & 0xFFFF);
        if (idx == 4)
            ang += child->mirrored ? 0x2700 : 0x5900;
        else
            ang -= 0x4000;
        child->vx = ang;      /* + 0x48 held angle */
        r->ang = ang;
    } else if (idx == 5 || idx == 0x0C) {
        child->vx = r->ang;
    }
    if (body->state == 2) {
        /* The launch: + 0x18 takes the held + 0x48, + 0x10 a random
         * spin, + 0x24 the release threshold (parent y pixel plus the
         * bgInfo[1] scroll y plus the 0x08119360 offset — cam_y is the
         * documented scroll approximation), + 0x54 gravity 0x1800,
         * + 0x48 a random spread angle and + 0xD the 0x08119352 timer. */
        child->state = 1;
        child->step = 0;
        child->angle = (uint32_t)child->vx;
        child->spin = (int32_t)((random ? random() : 0) & 0xFF) - 0x80;
        child->bg_dist = (int32_t)(int16_t)(body->y >> 16) +
                         (int32_t)(int16_t)cam_y +
                         bg_dist_tab[idx];
        child->ax = 0;
        child->ay = 0x1800;
        child->vx = (int32_t)((random ? random() : 0) & 0x3FFF) - 0x2000;
        child->vy = 0;
        child->timer = (uint8_t)bg_timer_tab[idx];
    }
}

/* sub_080AC6E8 state 1 (the launched part): the + 0x0B substeps
 * (0 -> 1, record 0xD to 0xA), the sub_0806D518 blink into + 0x58
 * bit 5, the last-0x3C-frames accel_move and + 0x18 spin drift, the
 * step-1 release into step 2 (zeroed motion, song 0x71 unless the
 * part is 1/2/3/7), the step-2 sub_0806D5C0 flame spawn while the
 * parent's + 0x30 param is 0, the timer delete with the record-5
 * song 0x6E stinger and the + 0x58 bit-5 latch when the parent's y
 * pixel leaves [-0x18, 0xF0].  A launched part outlives its parent
 * like the native entity; only a foreign occupant of the parent
 * slot ends it early. */
static void bg_launched(AosEnemy *child, AosEnemy *body,
                        const AosEnemyKind *kind,
                        uint32_t (*random)(void), AosHitReport *report,
                        int cam_y) {
    unsigned idx = child->variant < BG_PARTS ? child->variant : 0;
    bool body_ok = body && body->id == 0x45;
    if (child->step == 0) {
        child->step = 1;
        if (idx == 0x0D)
            child->step = 0x0A;
    }
    child->hidden = child->timer <= 0x27 &&
                    (kind->blink[child->timer] & 1) != 0;
    if (child->timer <= 0x3C) {
        accel_move(child, 0x60000);
        child->angle = (uint32_t)((int32_t)child->angle + child->spin);
        if (child->step == 1 && body_ok) {
            int32_t sum = (int32_t)(int16_t)(body->y >> 16) +
                          (int32_t)(int16_t)cam_y;
            if (sum >= child->bg_dist) {
                child->vx = child->ax = child->vy = child->ay = 0;
                child->spin = 0;
                child->step = 2;
                if (!(idx == 1 || idx == 2 || idx == 3 || idx == 7))
                    aos_audio_play_song(0x71);
            }
        }
    }
    if (child->step == 2 && body_ok && body->param0 == 0 &&
        (child->timer & 7) == 0)
        spawn_flame_at(child->x, child->y - 0x40000, 0x400000, 0x400000,
                       random, report);
    uint8_t t = (uint8_t)(child->timer - 1);
    child->timer = t;
    if (t <= 1) {
        if (idx == 5)
            aos_audio_play_song(0x6E);
        child->removed = true;
        return;
    }
    if (!child->hidden && body_ok) {
        int32_t py = (int32_t)(int16_t)(body->y >> 16);
        if (py < -0x18 || py > 0xF0)
            child->hidden = true;
    }
}

/* EnemyBigGolemUpdate 0x080A8224 state 2: the native death machine.
 * While the + 0xD timer sits in [0x79, 0xD6) every fourth frame fires
 * a sub_0806D7F0 particle at a random x offset (the port uses the
 * dark-smoke drift); the + 0x59 bit-3 animation-end flag plus the
 * + 0x1C room record gates the sub_0806C250 delete. The update tail
 * clears + 0x1A on a 1/8 random roll. */
static AosHitReport bg_death(AosEnemy *enemy, uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (enemy->timer >= 0x79 && enemy->timer < 0xD6 &&
        (enemy->timer & 3) == 0) {
        int32_t px = enemy->x +
                     (int32_t)(random() % 0x280000) - 0x140000;
        spawn_drift(AOS_EFFECT_DARK_SMOKE, px, enemy->y - 0x80000,
                    &report);
    }
    if (enemy->timer)
        enemy->timer--;
    /* The + 0x59 bit-3 flag: animation end while the timer sits in
     * its tail deletes through the native + 0x1C room record. */
    if (enemy->cycle_done && enemy->timer <= 0x10) {
        enemy->cycle_done = false;
        enemy->removed = true;
    }
    return report;
}

/* sub_080A89F8 state 0: thirteen cases in four repeating groups — a
 * short pose case (song 0x7C attack call), a long chase case built on
 * the sub_0806CDB8/D0B8/D188 wall and distance probes, and a settle
 * case. The port collapses the probes into the shared helpers and
 * cycles the documented beats: chase, attack pose, settle, then the
 * case-12 exit into state 1. */
static void bg_state0(AosEnemy *enemy, AosSoma *soma,
                      const AosAnimSet *anims,
                      uint32_t (*random)(void)) {
    (void)random;
    switch (enemy->step) {
    case 0:
    case 4:
    case 7:
    case 10:
        /* Pose/settle beats between attacks. */
        if (++enemy->timer > 0x20) {
            enemy->timer = 0;
            enemy->step++;
        }
        break;
    case 1:
    case 5:
    case 8:
    case 11:
        /* Attack call: song 0x7C once, animation 1, small lunge. */
        aos_audio_play_song(0x7C);
        play_loop(enemy, anims, 1, false);
        enemy->vx = enemy->mirrored ? 0x40000 : -0x40000;
        enemy->timer = 0;
        enemy->step++;
        break;
    case 2:
    case 3:
    case 6:
    case 9:
        /* Chase: face the player and close in under the speed cap. */
        if (soma)
            face_player(enemy, soma);
        if (soma)
            move_toward(enemy, soma->x, soma->y - 0x80000, 0x10);
        accel_move(enemy, 0x60000);
        if (++enemy->timer > 0x60) {
            enemy->timer = 0;
            enemy->step++;
        }
        break;
    case 12:
        /* The native exit: enter the state-1 attack machine. */
        enemy->state = 1;
        enemy->step = 0;
        enemy->timer = 0;
        break;
    default:
        enemy->step = 0;
        break;
    }
}

/* sub_080AA7A8 state 1: forty cases of attack combinations — windup
 * song 0x86, strike song 0x71, sub_0806DCC4 shots and the
 * sub_080ABFA4/sub_080ACDD0 helpers. The port keeps the documented
 * beat order: windup, two strike volleys, recover, back to state 0. */
static void bg_state1(AosEnemy *enemy, AosSoma *soma,
                      const AosAnimSet *anims, AosHitReport *report,
                      uint32_t (*random)(void)) {
    switch (enemy->step) {
    case 0:
        face_player(enemy, soma);
        play_loop(enemy, anims, 2, false);
        enemy->timer = 0;
        enemy->step = 1;
        break;
    case 1:
        /* Windup: song 0x86, hold the pose. */
        if (enemy->timer == 0)
            aos_audio_play_song(0x86);
        if (++enemy->timer > 0x30) {
            enemy->timer = 0;
            enemy->step = 2;
        }
        break;
    case 2:
    case 4:
        /* Strike volleys: song 0x71 and flame-folded shots. */
        if (enemy->timer == 0) {
            aos_audio_play_song(0x71);
            for (int i = 0; i < 2 &&
                            report->child_count < AOS_ENEMY_MAX_CHILDREN;
                 ++i) {
                int32_t sx = enemy->x + (enemy->mirrored ? -0x80000
                                                        : 0x80000);
                spawn_flame_at(sx, enemy->y - 0x40000, 0x60000, 0x80000,
                               random, report);
            }
        }
        if (++enemy->timer > 0x20) {
            enemy->timer = 0;
            enemy->step++;
        }
        break;
    case 3:
    case 5:
        /* Recover between strikes. */
        if (++enemy->timer > 0x28) {
            enemy->timer = 0;
            enemy->step++;
        }
        break;
    default:
        /* Return to the state-0 cycle. */
        enemy->state = 0;
        enemy->step = 0;
        enemy->timer = 0;
        break;
    }
}

/* sub_080AC6E8 entry: state 0 rides the record, any other state is
 * the launched flight machine. */
static void bg_child_update(AosEnemy *child, AosEnemy *body,
                            const AosEnemyKind *kind,
                            uint32_t (*random)(void),
                            AosHitReport *report, int cam_y) {
    if (child->state == 0)
        bg_attached(child, body, random, cam_y);
    else
        bg_launched(child, body, kind, random, report, cam_y);
}

AosHitReport aos_enemy_big_golem_update(
    AosEnemy *enemy, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind,
    const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0};
    (void)layer;
    (void)cam_x;
    if (enemy->role == AOS_ROLE_BG_PART ||
        (enemy->role >= AOS_ROLE_BG_CHILD0 &&
         enemy->role <= AOS_ROLE_BG_CHILD3)) {
        bg_child_update(enemy, enemy->parent, kind, random, &report,
                        cam_y);
        return report;
    }
    if (!enemy->boss_parts_spawned) {
        bg_emit_parts(enemy, &report);
        if (enemy->spawned_count >= BG_PARTS) {
            enemy->boss_parts_spawned = true;
            /* EnemyBigGolemCreate's sub_080AC298 tail: face the
             * records once the bake is complete. */
            aos_enemy_bg_face_flip(enemy, soma);
        }
    }
    if (enemy->state == 2) {
        AosHitReport hit = bg_death(enemy, random);
        skeleton_merge(&report, &hit);
        step_anim(enemy, kind->anims);
        return report;
    }
    switch (enemy->state) {
    case 0:
        bg_state0(enemy, soma, kind->anims, random);
        break;
    case 1:
        bg_state1(enemy, soma, kind->anims, &report, random);
        break;
    default:
        /* State 3 dormant (sub_0803F17C tail only). */
        break;
    }
    step_anim(enemy, kind->anims);
    AosHitReport hit = collide(enemy, soma, kind, weapon, weapon_frames,
                               soma_atk, soma_def);
    skeleton_merge(&report, &hit);
    /* Lethal: the shared hurt path applied the damage; the native
     * state-2 machine is the decoded death above. */
    if (!enemy->defeated && enemy->hp <= 0 && enemy->state != 2) {
        enemy->defeated = true;
        enemy->state = 2;
        enemy->step = 0;
        enemy->timer = 0xD6;
        play_loop(enemy, kind->anims, 3, false);
    }
    aos_combat_tick(&enemy->combat);
    return report;
}
