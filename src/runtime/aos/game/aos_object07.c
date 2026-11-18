/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_object07.h"

#include <math.h>
#include <string.h>

/* sub_080009E4 / sSineTable: sine of a 16-bit angle (0x10000 per
 * turn) returned as 16.16. Kept local to this file — the shared
 * port lives in aos_math.c but the standalone fusion_aos_object07_
 * tests target links only this translation unit, the same
 * self-contained pattern aos_intro_entities.c uses. The generated
 * entries are identical to sSineTable (floor(sin(i*pi/2048) *
 * 65536), verified against all 1024 ROM entries in aos_math.c). */
static uint16_t sine_table[0x400];

static void build_sine(void) {
    if (sine_table[1]) return;
    for (int i = 0; i < 0x400; ++i)
        sine_table[i] = (uint16_t)floor(
            sin(i * 3.14159265358979323846 / 2048.0) * 65536.0);
}

static int32_t object07_sine(uint32_t angle) {
    build_sine();
    uint32_t a = (angle + (0x80 >> 4)) & 0xFFFF;
    int32_t sign = 1;
    if (a & 0x8000) {
        a -= 0x8000;
        sign = -1;
    }
    if (a & 0x4000) {
        if (a & 0xFFFFBFF0u)
            return sign * (int32_t)sine_table[(0x800F - a) >> 4];
        return sign * 0x10000;
    }
    return sign * (int32_t)sine_table[a >> 4];
}

/* 0x085255AC[param1]: ambient child counts. */
static const uint16_t ambient_counts[] = {12, 15, 12, 12, 12, 2};
/* 0x0852559C[layer]: BG-scroll parallax factors (>> 8). */
static const int32_t parallax[4] = {192, 160, 96, 48};
/* The four sub_0803F2C8(layer, 0, 1) animation sequences in
 * 0x0820FE44/0x08210DB0. The two resources share their timing. */
static const uint8_t mote_anim_count[4] = {3, 3, 3, 2};
static const uint8_t mote_anim_ticks[4][3] = {
    {12, 14, 10}, {9, 11, 7}, {6, 9, 6}, {4, 4, 0}};
/* 0x085255DC: the four anchored-child records {x bias, y bias, aux}. */
static const uint8_t anchor_rec[4][3] = {
    {16, 80, 11}, {128, 88, 12}, {48, 72, 13}, {96, 56, 14}};
/* 0x085255C4[param0]: u16 x/y position pairs for the anchored
 * children (only param0 0 carries non-zero values in the ROM dump). */
static const uint16_t anchor_pos[6][2] = {
    {0xFFB4, 0xFFC8}, {0, 0}, {0, 0}, {0xFFB4, 0}, {0xFFB4, 0}, {0, 0}};

static uint32_t rnd(uint32_t (*rng)(void)) { return (*rng)(); }

/* sub_0804F374: spawn table[param1] ambient motes. */
static void spawn_ambient(AosObject07 *o, uint32_t (*rng)(void),
                          int32_t cam_x, int32_t cam_y) {
    (void)cam_y;    /* layer-3 anchors use cam_x only in the native code */
    int count = o->param1 < 6 ? ambient_counts[o->param1] : 0;
    if (count > AOS_OBJECT07_MAX_AMBIENT) count = AOS_OBJECT07_MAX_AMBIENT;
    for (int i = 0; i < count; ++i) {
        AosObject07Mote *m = &o->motes[o->ambient_count++];
        memset(m, 0, sizeof(*m));
        m->alive = true;
        m->state = 0;
        uint32_t roll = rnd(rng) & 0xF;
        /* roll > 8 -> layer 2, > 5 -> layer 1, > 1 -> layer 0,
         * else layer 3 (camera-relative). */
        if (roll > 8)      { m->layer = 2; m->variant = 0x1D; }
        else if (roll > 5) { m->layer = 1; m->variant = 0x1A; }
        else if (roll > 1) { m->layer = 0; m->variant = 0x19; }
        else               { m->layer = 3; m->variant = 0x1D; m->kind3 = 1; }
        if (!m->kind3) {
            m->sx = (int16_t)(((rnd(rng) & 0x3F) << 2) - 0x20);
            m->sy = (int16_t)((rnd(rng) & 0x7F) + 1);
            m->speed_x = (int32_t)(((rnd(rng) & 0x3FFF) + 0xC000) >>
                                   m->layer);
            m->speed_y = (int32_t)(((rnd(rng) & 0x1FFF) + 0x8000) >>
                                   m->layer);
            /* The native + 0x58 bit-0x40 negate of + 0x50 is dead
             * code here: EntityCreateInRange zeroes the entity and
             * nothing sets + 0x58 before sub_0804F374 reads it. */
        } else {
            m->sx = (int16_t)((rnd(rng) & 0x3F) + 0x8C - (cam_x >> 2));
            m->sy = (int16_t)((rnd(rng) & 0x3F) + 0x21);
            m->speed_x = (int32_t)(((rnd(rng) & 0x7F) << 4) + 0x800);
            m->speed_y = (int32_t)(((rnd(rng) & 0x7F) << 4) + 0x800);
            m->phase2 = (uint32_t)(rnd(rng) & 0x1F) << 0xC;   /* + 0x18 */
        }
        /* Common spawn tail: + 0x14 = (rng & 0x1F) << 0xC. + 0x48/
         * + 0x4C stay zero until the first state-1 update derives
         * them from + 0x50/+ 0x54 through the sine. */
        m->timer = (uint32_t)(rnd(rng) & 0x1F) << 0xC;
        m->fx = (int64_t)m->sx << 16;
        m->fy = (int64_t)m->sy << 16;
    }
}

/* sub_0804F5C4: the four anchored children. */
static void spawn_anchors(AosObject07 *o, int param0, int32_t cam_x,
                          int32_t cam_y) {
    for (int i = 0; i < AOS_OBJECT07_ANCHORED; ++i) {
        AosObject07Anchor *a = &o->anchors[o->anchor_count++];
        memset(a, 0, sizeof(*a));
        a->alive = true;
        a->aux = anchor_rec[i][2];
        /* x = rec.x + 0x50 - ((cam_x * 0x40) >> 6) + pos[param0].x —
         * the + 0xA094 factor bytes are approximated by 0x40 (1:1). */
        a->fx = ((int64_t)(anchor_rec[i][0] + 0x50 - cam_x +
                           (int16_t)anchor_pos[param0 < 6 ? param0 : 0][0])
                 << 16);
        a->fy = ((int64_t)(anchor_rec[i][1] - cam_y +
                           (int16_t)anchor_pos[param0 < 6 ? param0 : 0][1])
                 << 16);
        a->vx = 0x80;
        a->vy = 0;
    }
}

void aos_object07_create(AosObject07 *o, int param0, int param1,
                         int32_t cam_x, int32_t cam_y,
                         uint32_t (*rng)(void)) {
    memset(o, 0, sizeof(*o));
    o->param0 = (uint8_t)param0;
    o->param1 = (uint8_t)param1;
    o->last_cam_x = cam_x;
    o->last_cam_y = cam_y;
    o->win48 = 0xE4;             /* gDisplayRegisters + 0x48 = 0xE40 */
    switch (param0) {
    case 0:
        spawn_ambient(o, rng, cam_x, cam_y);
        spawn_anchors(o, param0, cam_x, cam_y);
        break;
    case 1:
        o->frame_index = 0;
        break;
    case 2:
        spawn_ambient(o, rng, cam_x, cam_y);
        break;
    case 3:
        o->clear_4c4 = true;
        o->set_42c_800 = true;
        break;
    case 4:
        if (param1) o->call_13eec = true;
        break;
    default:
        break;
    }
}

/* sub_0803F17C for Object07's mode-0 sprite scripts. The state-0
 * setup seeds +0x6E with RNG&1 before its first step; subsequent
 * active frames step once after movement. */
static void update_mote_anim(AosObject07Mote *m) {
    uint8_t layer = m->layer < 4 ? m->layer : 0;
    uint8_t count = mote_anim_count[layer];
    if (m->anim_frame >= count) m->anim_frame = 0;
    if (++m->anim_tick >= mote_anim_ticks[layer][m->anim_frame]) {
        m->anim_tick = 0;
        m->anim_frame = (uint8_t)((m->anim_frame + 1) % count);
    }
}

/* sub_0804F690: the param0 == 1 BG-anim cycler. The 8-byte records
 * at 0x08525D9C are {u16 count, u16 arg, u32 frame-table ptr}; each
 * frame-table entry is {u32 sprite, u16 duration, u16 mod}. The tile
 * DMA (sub_0803C918) is not modelled — only + 0x10/+ 0x12 advance. */
static const struct { uint16_t count, arg; } anim_records[6] = {
    {2, 6}, {2, 6}, {2, 6}, {4, 5}, {2, 4}, {2, 7}};
static const uint16_t anim_mod[6][4] = {
    {2, 2, 0, 0}, {2, 2, 0, 0}, {2, 2, 0, 0}, {5, 5, 5, 5}, {2, 2, 0, 0}, {3, 3, 0, 0}};

static void update_anim(AosObject07 *o) {
    uint16_t rec = o->param0 < 6 ? o->param0 : 0;
    uint16_t mod = anim_mod[rec][o->frame_index < 4 ? o->frame_index : 0];
    if (mod == 0) mod = 1;
    o->sub_index = (uint16_t)((o->sub_index + 1) % mod);
    if (o->sub_index == 0) {
        uint16_t count = anim_records[rec].count;
        if (!count) count = 1;
        o->frame_index = (uint8_t)((o->frame_index + 1) % count);
    }
}

void aos_object07_update(AosObject07 *o, int32_t cam_x, int32_t cam_y,
                         uint32_t (*rng)(void)) {
    /* gEwramData + 0xA0A0/+0xA0A4 are per-frame 16.16 scroll
     * velocities, not the absolute BG1 position. Feeding the absolute
     * camera here made every mote re-apply the whole camera offset on
     * every frame and race across the room. */
    int32_t scroll_x = (int32_t)((uint32_t)(cam_x - o->last_cam_x) << 16);
    int32_t scroll_y = (int32_t)((uint32_t)(cam_y - o->last_cam_y) << 16);
    o->last_cam_x = cam_x;
    o->last_cam_y = cam_y;
    if (o->param0 == 1) {
        update_anim(o);
        return;                 /* native still writes + 0x48 = 0xE40 */
    }
    /* Ambient motes: state 0 attaches the shared art (approximated —
     * the sprite load is a renderer concern), state 1 drifts. */
    for (int i = 0; i < o->ambient_count; ++i) {
        AosObject07Mote *m = &o->motes[i];
        if (!m->alive) continue;
        if (m->state == 0) {
            m->state = 1;
            m->anim_frame = (uint8_t)(rnd(rng) & 1);
            m->anim_tick = 0;
            update_mote_anim(m);
            continue;
        }
        /* sub_0804F160 state 1 (_0804F224). The native gate
         * gEwramData + 0x4BE != 0 skips only the integrate/anim step
         * below; the parallax and wrap still run — the port has no
         * + 0x4BE input and always integrates. sub_0803F17C between
         * them is the sprite-script/animation step (dispatch on
         * + 0x5B) and does not touch + 0x40..+ 0x4C. */
        uint32_t phase = m->timer + 0x800;    /* + 0x14 += 0x800 */
        m->timer = phase;
        if (!m->kind3) {
            /* layer != 3: + 0x48 = + 0x50 verbatim, + 0x4C =
             * (sine(+ 0x14) * + 0x54) >> 16. The ROM adds 0xFFFF to
             * a negative product before the arithmetic shift —
             * i.e. truncation toward zero, which C division gives. */
            m->vx = m->speed_x;
            /* muls wraps mod 2^32 — sine(±0x10000) * speed_y can
             * exceed s32, so multiply through u32. */
            m->vy = (int32_t)((uint32_t)object07_sine(phase) *
                              (uint32_t)m->speed_y) / 0x10000;
        } else {
            /* layer 3: + 0x18 += 0xA00; + 0x48 = (sine(+ 0x14 old +
             * 0x4800) * + 0x50) >> 0xC = sine(phase + 0x4000), and
             * + 0x4C = (sine(+ 0x18) * + 0x54) >> 0xC, both with the
             * 0xFFF negative-product adjust (truncation toward
             * zero). */
            m->phase2 += 0xA00;
            m->vx = (int32_t)((uint32_t)object07_sine(phase + 0x4000) *
                              (uint32_t)m->speed_x) / 0x1000;
            m->vy = (int32_t)((uint32_t)object07_sine(m->phase2) *
                              (uint32_t)m->speed_y) / 0x1000;
        }
        /* + 0x40 += + 0x48, + 0x44 += + 0x4C — velocities are
         * already 16.16, so the position words take them verbatim. */
        m->fx += m->vx;
        m->fy += m->vy;
        update_mote_anim(m);
        /* Parallax (_0804F2BA): + 0x40 -= (scroll * factor[layer])
         * >> 8 with the ROM's 32-bit muls wrap and the + 0xFF
         * negative-product adjust — again truncation toward zero. */
        int32_t prod = (int32_t)((uint32_t)scroll_x *
                                 (uint32_t)parallax[m->layer]);
        m->fx -= prod / 0x100;
        prod = (int32_t)((uint32_t)scroll_y *
                         (uint32_t)parallax[m->layer]);
        m->fy -= prod / 0x100;
        /* Screen-anchor wrap on the s16 integer halves + 0x42/+ 0x46
         * of + 0x40/+ 0x44 (_0804F2BA tail): x > 0x110 reseeds
         * + 0x46 = (rng & 0x7F) + 1 and teleports + 0x42 to -0x20;
         * x < -0x20 does the same reseed with + 0x42 = 0x110. Only
         * when x is in range is y tested: y > 0xC0 -> -0x10 and
         * y < -0x20 -> 0xB0, each reseeding + 0x42 = rng & 0xBF.
         * One wrap per frame; the strh writes keep the 16.16
         * fraction, mirrored by masking the low word back in. */
        int32_t px = (int16_t)(m->fx >> 16), py = (int16_t)(m->fy >> 16);
        if (px > 0x110) {
            py = (int32_t)(rnd(rng) & 0x7F) + 1;
            px = -0x20;
        } else if (px < -0x20) {
            py = (int32_t)(rnd(rng) & 0x7F) + 1;
            px = 0x110;
        } else if (py > 0xC0) {
            py = -0x10;
            px = (int32_t)(rnd(rng) & 0xBF);
        } else if (py < -0x20) {
            py = 0xB0;
            px = (int32_t)(rnd(rng) & 0xBF);
        } else {
            continue;
        }
        m->fx = (m->fx & 0xFFFF) |
                (int32_t)((uint32_t)(uint16_t)px << 16);
        m->fy = (m->fy & 0xFFFF) |
                (int32_t)((uint32_t)(uint16_t)py << 16);
        m->sx = (int16_t)px;
        m->sy = (int16_t)py;
    }
    /* Anchored children (sub_0804F504 state 1): + 0x48 = + 0x50
     * (= 0x80), + 0x40 += + 0x50, + 0x44 += + 0x4C (= 0 — the field
     * is never written after the zeroed create), then scroll / 0xC
     * subtracted per axis through __divsi3 (truncating, like C /). */
    for (int i = 0; i < o->anchor_count; ++i) {
        AosObject07Anchor *a = &o->anchors[i];
        if (!a->alive || a->state == 0) { a->state = 1; continue; }
        a->fx += a->vx;
        a->fy += a->vy;
        a->fx -= scroll_x / 0xC;
        a->fy -= scroll_y / 0xC;
    }
}
