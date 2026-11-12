/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_object07.h"

#include <string.h>

/* 0x085255AC[param1]: ambient child counts. */
static const uint16_t ambient_counts[] = {12, 15, 12, 12, 12, 2};
/* 0x0852559C[layer]: BG-scroll parallax factors (>> 8). */
static const int32_t parallax[4] = {192, 160, 96, 48};
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
            /* + 0x58 bit 0x40 negates + 0x50: approximated by the
             * parent's screen-half flag passed through cam_x. */
            m->timer = (rnd(rng) & 0x1F) << 0xC;
        } else {
            m->sx = (int16_t)((rnd(rng) & 0x3F) + 0x8C - (cam_x >> 2));
            m->sy = (int16_t)((rnd(rng) & 0x3F) + 0x21);
            m->speed_x = (int32_t)(((rnd(rng) & 0x7F) << 4) + 0x800);
            m->speed_y = (int32_t)(((rnd(rng) & 0x7F) << 4) + 0x800);
            m->timer = (rnd(rng) & 0x1F) << 0xC;
        }
        m->fx = (int64_t)m->sx << 16;
        m->fy = (int64_t)m->sy << 16;
        m->vx = m->speed_x;
        m->vy = m->speed_y;
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
                         int32_t scroll_x, int32_t scroll_y,
                         uint32_t (*rng)(void)) {
    (void)cam_x;
    (void)cam_y;
    if (o->param0 == 1) {
        update_anim(o);
        return;                 /* native still writes + 0x48 = 0xE40 */
    }
    /* Ambient motes: state 0 attaches the shared art (approximated —
     * the sprite load is a renderer concern), state 1 drifts. */
    for (int i = 0; i < o->ambient_count; ++i) {
        AosObject07Mote *m = &o->motes[i];
        if (!m->alive) continue;
        if (m->state == 0) { m->state = 1; continue; }
        /* pos += vel; the + 0x48 = + 0x50 / + 0x4C = f(+ 0x54, + 0x14)
         * split is approximated by the raw speeds (the sine lookup
         * sub_080009E4 is not modelled). */
        m->fx += (int64_t)m->vx << 16;
        m->fy += (int64_t)m->vy << 16;
        /* Parallax: fixed pos -= scroll * factor[layer] >> 8. */
        m->fx -= ((int64_t)scroll_x * parallax[m->layer]) >> 8;
        m->fy -= ((int64_t)scroll_y * parallax[m->layer]) >> 8;
        /* Screen-anchor wrap (sub_0804F160 tail): re-randomize the
         * anchor on each edge. */
        if (m->sx > 0x110) {
            m->sy = (int16_t)(rnd(rng) & 0xBF);
            m->sx = -0x20;
        } else if (m->sx < -0x20) {
            m->sy = (int16_t)(rnd(rng) & 0x7F);
            m->sx = 0x110;
        }
        if (m->sy > 0xC0) {
            m->sy = -0x10;
            m->sx = (int16_t)(rnd(rng) & 0xBF);
        } else if (m->sy < -0x20) {
            m->sy = 0xB0;
            m->sx = (int16_t)(rnd(rng) & 0xBF);
        }
    }
    /* Anchored children: state 1 pos += vel, scroll / 0xC subtracted. */
    for (int i = 0; i < o->anchor_count; ++i) {
        AosObject07Anchor *a = &o->anchors[i];
        if (!a->alive || a->state == 0) { a->state = 1; continue; }
        a->fx += (int64_t)a->vx << 16;
        a->fy += (int64_t)a->vy << 16;
        a->fx -= scroll_x / 0xC;
        a->fy -= scroll_y / 0xC;
    }
}
