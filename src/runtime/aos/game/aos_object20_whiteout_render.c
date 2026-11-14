/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_object20_whiteout_render.h"
#include <string.h>

/* ------------------------------------------------------------------
 * sub_08064518 @ 0x08064518 — decoded literally from
 * third_party/cvaos/asm/code/code_08060B98.s:7286-8782.
 *
 * Prologue (every call):
 *   r8 = gEwramData + 0x786C + (((gEwramData[0x7864] >> 3) & 1) ^ 1)
 *        * 0xA00            — affine bank select (two 0xA00 banks)
 *   HBlankEffectSetup(0, 0xA0, 0x10, 0x04000020)
 *   switch ((uint8)+0x15) through the jump table at 0x08064584;
 *   values > 10 exit immediately. Every path ends at the shared
 *   epilogue sub_080650CE: gEwramData[0x7864] |= 1, return.
 *
 * RandomNumberGenerator (third_party/cvaos/src/code_080009A0.c):
 *   state = (state >> 8) * 0x3243F6AD + 0x1B0CB175  (u32 wrap)
 * ------------------------------------------------------------------ */

static uint32_t wo_rng(uint32_t *state) {
    *state = (*state >> 8) * 0x3243F6ADu + 0x1B0CB175u;
    return *state;
}

/* ------------------------------------------------------------------
 * Committed ROM tables.
 *
 * 0x08118B34: five words = five Thumb update-function pointers.
 * Every spawn site ldm's the same 20-byte block to sp+0x1C..0x2F
 * and passes ONE word as EntityCreateInRange(0x1B, 0x48, fn)'s
 * updateFunc — the +0x20/+0x22 screen position is then written
 * to the child's +0x42/+0x46. aux is the word index 0..4.
 * ------------------------------------------------------------------ */
static const uint32_t wo_spawn_table_118b34[5] = {
    0x08063421, /* sub_08063420 — aux 0 (state-2 +0x24 == 0x40) */
    0x08063559, /* sub_08063558 — aux 1 (state-2 +0x24 == 0x11F,
                *          first spawn) */
    0x08063601, /* sub_08063600 — aux 2 (+0x24 == 0x1FE and the
                *          second 0x11F spawn) */
    0x08063698, /* sub_08063698 — aux 3 (+0x24 == 0xC0) */
    0x08063741, /* sub_08063740 — aux 4 (state4/5 init) */
};

/* 0x08526924: six 8-byte records {u32 data, u16 tile, u16 dwell}
 * consumed by sub_0806447C. `data` is always 0x08119FDC — the
 * same packed tile/palette blob the state-0 palette_copy loads
 * through sub_0803C7B4; `tile` selects the 0x20-byte record
 * inside it (sub_0803C918/0803C7B4 index data + 4 + tile*0x20)
 * and `dwell` is the +0x36 threshold. */
typedef struct {
    uint32_t data;
    uint16_t tile;
    uint16_t dwell;
} WoCycleEntry;

static const WoCycleEntry wo_cycle_table_8526924[6] = {
    {0x08119FDC, 0, 64},
    {0x08119FDC, 5, 16},
    {0x08119FDC, 6, 16},
    {0x08119FDC, 7, 16},
    {0x08119FDC, 8, 16},
    {0x08119FDC, 9, 16},
};

/* __modsi3/__divsi3 semantics: C truncated division. The native
 * divisor is entity +0xD; a zero divisor would fault in the ROM,
 * so the port treats mod-by-zero as "no match" and documents it. */
static int32_t wo_mod(int32_t a, int32_t b) {
    return b != 0 ? a % b : 1;
}
static int32_t wo_div(int32_t a, int32_t b) {
    return b != 0 ? a / b : 0;
}

/* Simple 160-bar strip shared by states 0, 3, 6/7, 8, 9, 10:
 * x = +0x20 (+ sb, always 0 on these paths), y = +0x22 - i,
 * bgx = +0x1C, bgy = +0x1E, intensity = +0x14 << 8. */
static void wo_draw_simple(const AosWoRender *s, AosWoRenderOut *out) {
    out->bars_drawn = AOS_WO_BAR_COUNT;
    for (unsigned i = 0; i < AOS_WO_BAR_COUNT; ++i) {
        AosWoBar *b = &out->bars[i];
        b->x = (uint16_t)(s->field_20 + 0);      /* + sb == 0 */
        b->y = (uint16_t)(s->field_22 - i);
        b->bgx = s->field_1c;
        b->bgy = s->field_1e;
        b->intensity = (uint16_t)(s->field_14 << 8);
    }
}

static void wo_spawn(AosWoRenderOut *out, uint8_t kind,
                     int16_t x, int16_t y, uint8_t aux) {
    if (out->spawn_count >= AOS_WO_MAX_EVENTS)
        return;
    AosWoSpawn *sp = &out->spawns[out->spawn_count++];
    sp->kind = kind;
    sp->scr_x = x;
    sp->scr_y = y;
    sp->aux = aux;
    sp->fn_addr = (kind == AOS_WO_SPAWN_TABLE_118B34 && aux < 5)
        ? wo_spawn_table_118b34[aux] : 0;
}

static void wo_particle(AosWoRenderOut *out, int32_t x, int32_t y,
                        uint8_t dir, uint8_t variant) {
    if (out->particle_count >= AOS_WO_MAX_EVENTS)
        return;
    AosWoParticle *p = &out->particles[out->particle_count++];
    p->x = (int16_t)x;
    p->y = (int16_t)y;
    p->dir = dir;
    p->variant = variant;
}

static void wo_fade(AosWoRenderOut *out, uint32_t a3,
                    uint32_t s0, uint32_t s1) {
    /* sub_0803D18C + sub_0803CDF0 always fire as a pair with
     * r=g=b=0x1F; only arg3 and the two stack args vary. At most
     * one pair fires per frame (all trigger values are distinct). */
    out->fade_pair = true;
    out->fade_a3 = a3;
    out->fade_s0 = s0;
    out->fade_s1 = s1;
}

/* sub_0806447C child-position decode used by the state-2 init
 * spawn (sub_08064370): +0x42 = s8(+0x26) - 0x30 + +0x20,
 *          +0x46 = s8(+0x27) + 8 + +0x22. */
static void wo_fx64198_pos(const AosWoRender *s,
                           int16_t *x, int16_t *y) {
    *x = (int16_t)((int8_t)s->field_26 - 0x30 + (int32_t)s->field_20);
    *y = (int16_t)((int8_t)s->field_27 + 8 + (int32_t)s->field_22);
}

/* sub_0806447C: the +0x34/+0x36 palette-cycle driver. +0x34
 * indexes the six 8-byte records at 0x08526924 (values above 5
 * early-out BEFORE the dwell counter moves). Each call stores
 * +0x36+1 then compares the OLD counter (sign-extended) against
 * the entry dwell with a signed ble: the block runs when
 * (s16)old > dwell, i.e. every dwell+2 calls. On expiry it
 * resets +0x36, copies tile records through sub_0803C918
 * (slot 0) and sub_0803C7B4 (slot 0xE) from the OLD index's
 * entry, increments +0x34, and — only while the new index is
 * <= 5 — arms the sequence pair sub_0803D2D0(channel 0) and
 * sub_0803CF34(channel +0x2C) on the NEW index's entry with
 * scale 0x10000 and speed = dwell. The four engine calls are
 * DMA/sequence writes, reported as Out flags. */
static void wo_cycle_6447c(AosWoRender *s, AosWoRenderOut *out) {
    out->cycle_6447c = true;
    out->cycle.ran = true;
    uint8_t idx = s->field_34;
    if (idx > 5)
        return;                     /* bhi _08064506: early out,
                                     * +0x36 untouched */
    const WoCycleEntry *e = &wo_cycle_table_8526924[idx];
    int16_t old = (int16_t)s->field_36;
    s->field_36 = (uint16_t)(old + 1);
    if (old <= (int16_t)e->dwell)
        return;                     /* ble _08064506 */
    s->field_36 = 0;
    out->cycle.step = true;
    out->cycle.index = idx;
    out->cycle.data = e->data;
    out->cycle.tile = e->tile;
    out->cycle.dwell = e->dwell;
    out->cycle.copy_3c918 = true;   /* sub_0803C918(data,tile,1,0) */
    out->cycle.copy_3c7b4 = true;   /* sub_0803C7B4(data,tile,1,0xE) */
    uint8_t next = (uint8_t)(idx + 1);
    s->field_34 = next;
    out->cycle.next_index = next;
    if (next > 5)
        return;                     /* no sequence pair at 6 */
    const WoCycleEntry *n = &wo_cycle_table_8526924[next];
    out->cycle.next_data = n->data;
    out->cycle.next_tile = n->tile;
    out->cycle.next_dwell = n->dwell;
    out->cycle.seq_arg0 = s->field_2c;   /* entity +0x2C byte */
    out->cycle.seq_3d2d0 = true;    /* sub_0803D2D0(0, n->data,
                                     * n->tile, 0x10000, 0,
                                     * n->dwell) */
    out->cycle.seq_3cf34 = true;    /* sub_0803CF34(+0x2C, n->data,
                                     * n->tile, 0x10000, 0,
                                     * n->dwell) */
}

void aos_wo_render_update(AosWoRender *s,
                          const AosWoRenderIn *in,
                          AosWoRenderOut *out) {
    uint32_t rng = in->rng;
    memset(out, 0, sizeof *out);
    out->song = -1;
    out->hblank_setup = true;           /* every call */
    out->tail_7864_or1 = true;          /* sub_080650CE tail */

    switch (s->field_15) {
    case 0: /* _080645B0 */
        if (s->field_16 == 0) {
            s->field_20 = 0xb8;
            s->field_22 = 0x70;
            s->field_1c = 0xb0;
            s->field_1e = 0xb0;
            s->field_14 = 0;
            s->scr_42 = 0xb8;           /* strh +0x42 = 0xB8 */
            s->scr_46 = (int16_t)s->field_22; /* +0x46 = +0x22 */
            out->palette_copy = true;   /* sub_0803C7B4(0x08119FDC,
                                         * 0,1,0xE) */
            s->field_2c = 0x0e;
            s->field_24 = 0x10;
            s->field_16 = 1;
        }
        wo_draw_simple(s, out);
        s->field_24 = (int16_t)(s->field_24 - 1);
        if ((int16_t)s->field_24 != -1)
            break;                      /* bl sub_080650CE: exit */
        /* _08064644: countdown finished. */
        out->song = 0x166;              /* PlaySong(0xB3 << 1) */
        s->field_24 = 0;
        s->field_15 = (uint8_t)(s->field_15 + 2);
        s->field_16 = 0;
        break;

    case 1: /* _080650CE itself: no state work, tail only. */
        break;

    case 2: /* _08064664 — the long whiteout sweep (0x29E frames). */
        if (s->field_16 == 0) {
            s->field_24 = 0x29e;
            s->field_16 = 1;
            wo_spawn(out, AOS_WO_SPAWN_FX_63DF8, 0, 0, 0);
            {
                int16_t x, y;
                wo_fx64198_pos(s, &x, &y);
                wo_spawn(out, AOS_WO_SPAWN_FX_64198, x, y, 0);
            }
            s->state_d = 1;
        }
        /* _08064682: per-frame jitter bytes. mask/base pick on the
         * SIGNED +0x24: > 0xFF -> mask 1 base 1, else mask 3
         * base 2. Two RNG draws per frame, always. */
        {
            int base, mask;
            if ((int16_t)s->field_24 > 0xff) {
                mask = 1; base = 1;
            } else {
                mask = 3; base = 2;
            }
            s->field_26 = (uint8_t)(base - (int)(wo_rng(&rng) & (uint32_t)mask));
            s->field_27 = (uint8_t)(base - (int)(wo_rng(&rng) & (uint32_t)mask));
        }
        /* Draw loop _080646C8: odd +0x24 adds the s8 jitter and
         * keeps the bytes; even +0x24 clears +0x26/+0x27 inside
         * the loop (net effect: cleared once, no jitter). */
        out->bars_drawn = AOS_WO_BAR_COUNT;
        if ((s->field_24 & 1) == 0) {
            s->field_26 = 0;
            s->field_27 = 0;
        }
        for (unsigned i = 0; i < AOS_WO_BAR_COUNT; ++i) {
            AosWoBar *b = &out->bars[i];
            int32_t x = s->field_20;
            int32_t y = (int32_t)s->field_22 - (int32_t)i;
            if (s->field_24 & 1) {
                x += (int8_t)s->field_26;
                y += (int8_t)s->field_27;
            }
            b->x = (uint16_t)x;
            b->y = (uint16_t)y;
            b->bgx = s->field_1c;
            b->bgy = s->field_1e;
            b->intensity = (uint16_t)(s->field_14 << 8);
        }
        /* Scheduled events on the PRE-decrement +0x24. */
        if (s->field_24 == 0x1fe) {         /* _0806475A first */
            wo_spawn(out, AOS_WO_SPAWN_TABLE_118B34,
                     (int16_t)s->field_20, (int16_t)s->field_22, 2);
        }
        if (s->field_24 == 0x11f) {         /* _08064796: two spawns */
            wo_spawn(out, AOS_WO_SPAWN_TABLE_118B34,
                     (int16_t)s->field_20, (int16_t)s->field_22, 1);
            wo_spawn(out, AOS_WO_SPAWN_TABLE_118B34,
                     (int16_t)s->field_20, (int16_t)s->field_22, 2);
            s->state_d = 1;
        }
        if (s->field_24 == 0xff)
            wo_fade(out, 0, 0x10000, 0x10);
        if (s->field_24 == 0xef)
            wo_fade(out, 0, 0x10000, 0x20);
        /* _0806482A: +0x24 window [0xA1,0xFE] drives +0x14. */
        if ((uint16_t)(s->field_24 - 0xa1) <= 0x5d) {
            uint8_t r4 = (uint8_t)(s->field_14 - 1);
            if (r4 > 0xf7) {
                if (wo_mod((int32_t)s->field_24, s->state_d) == 0) {
                    s->field_14 = r4;
                    s->state_d = (uint8_t)(s->state_d + 1);
                }
            } else if ((s->field_24 & 0xf) == 0) {
                s->field_14 = r4;
            }
        }
        if (s->field_24 == 0xc0) {          /* _080648A2 region */
            wo_spawn(out, AOS_WO_SPAWN_TABLE_118B34,
                     (int16_t)s->field_20, (int16_t)s->field_22, 3);
            s->state_d = 1;
        }
        if (s->field_24 == 0xa0)
            wo_fade(out, 0, 0x10000, 0x10);
        if (s->field_24 == 0x98)
            wo_fade(out, 0, 0x10000, 0x20);
        if ((int16_t)s->field_24 <= 0x9f) { /* _08064902 */
            if ((int16_t)s->field_24 > 0x9c) {
                s->field_14 = (uint8_t)(s->field_14 + 1);
            } else if ((s->field_24 & 0x1f) == 0) {
                s->field_14 = (uint8_t)(s->field_14 + 1);
                s->state_d = (uint8_t)(s->state_d + 1);
            }
        }
        if ((s->field_24 & 0xf) == 0 && s->field_1c != 0xd0) {
            s->field_1c = (uint16_t)(s->field_1c + 1);
            s->field_1e = (uint16_t)(s->field_1e + 1);
        }
        if (wo_mod((int32_t)s->field_24, 0x19) == 0 &&
            (int16_t)s->field_20 > 0x78)
            s->field_20 = (uint16_t)(s->field_20 - 1);
        if (wo_mod((int32_t)s->field_24, 0x32) == 0 &&
            (int16_t)s->field_22 > 0x50)
            s->field_22 = (uint16_t)(s->field_22 - 1);
        if (s->field_24 == 0x40)             /* _08064980 */
            wo_spawn(out, AOS_WO_SPAWN_TABLE_118B34,
                     (int16_t)s->field_20, (int16_t)s->field_22, 0);
        if (s->field_24 == 0x20)             /* _080649E0: s1 = r5 =
                                              * ldrsh +0x24 = 0x20 */
            wo_fade(out, 0, 0x10000, 0x20);
        if (s->field_24 == 0x30)             /* _08064A0E */
            wo_fade(out, 0, 0x10000, 0x20);
        s->field_24 = (int16_t)(s->field_24 - 1);
        if ((int16_t)s->field_24 == -1) {    /* _08064A24 */
            s->field_24 = 0;
            s->state_d = 0;
            s->field_15 = (uint8_t)(s->field_15 + 1);
            s->field_16 = 0;
        }
        break;

    case 3: /* _08064A38 — hold/scroll until +0x1C hits 0xD0. */
        if (s->field_16 == 0) {
            s->state_d = 0;
            s->field_36 = 0;
            s->field_34 = 0;
            s->field_16 = 1;
        }
        wo_draw_simple(s, out);
        if (s->state_d & 1) {
            if (s->field_1c == 0xd0) {
                s->field_18 = 0;
                s->field_15 = 5;
                s->field_16 = 0;
            } else {
                s->field_1c = (uint16_t)(s->field_1c + 1);
                s->field_1e = (uint16_t)(s->field_1e + 1);
            }
        }
        s->state_d = (uint8_t)(s->state_d + 1);
        break;

    case 4: /* _08064AB8 — shared with case 5: the wipe-out. */
    case 5:
        if (s->field_16 == 0) {
            wo_spawn(out, AOS_WO_SPAWN_TABLE_118B34,
                     (int16_t)s->field_20, (int16_t)s->field_22, 4);
            wo_fade(out, 0, 0x10000, 0xff);
            out->purge_547 = true;      /* sub_08000EA8(0x80) */
            s->field_14 = 0;
            s->state_d = 0;
            s->field_38 = 5;
            s->field_16 = 1;
        }
        /* _08064B2C: +0x26/+0x27 cleared; jitter kept in a stack
         * slot (jx/jy) and only while (u8)(+0x14-1) > 0xE5. */
        s->field_26 = 0;
        s->field_27 = 0;
        int32_t jx = 0, jy = 0;
        if ((uint8_t)(s->field_14 - 1) > 0xe5) {
            jx = 2 - (int32_t)(wo_rng(&rng) & 3);
            jy = 2 - (int32_t)(wo_rng(&rng) & 3);
            s->field_26 = (uint8_t)jx;  /* native stores low bytes */
            s->field_27 = (uint8_t)jy;
        }
        /* _08064B72 draw loop: per-bar X wedge driven by +0x18 and
         * the adjusted-bar counter r6 (1..). The native offset is
         * sb = sb + (sb << 1) i.e. x3, then x = +0x20 +/- that. */
        out->bars_drawn = AOS_WO_BAR_COUNT;
        {
            int r6 = 0;
            for (unsigned i = 0; i < AOS_WO_BAR_COUNT; ++i) {
                AosWoBar *b = &out->bars[i];
                int32_t x = s->field_20;
                int32_t y = (int32_t)s->field_22 - (int32_t)i;
                int32_t thresh = (int32_t)(int16_t)s->field_22 - 0x54;
                if ((int32_t)i > thresh) {
                    int32_t sb = 0;
                    r6++;
                    if (r6 <= 0x4f) {
                        if (r6 < (int32_t)s->field_18)
                            sb = (int32_t)s->field_18 - r6;
                        sb = sb + (sb << 1);     /* native x3 */
                        x = (int32_t)s->field_20 + sb;
                    } else {
                        int32_t t = 0xa0 - r6;
                        if (t < (int32_t)s->field_18)
                            sb = (int32_t)s->field_18 - 0xa0 + r6;
                        sb = sb + (sb << 1);
                        x = (int32_t)s->field_20 - sb;
                    }
                }
                if (s->field_24 & 1) {      /* _08064BE8 jitter */
                    x += jx;
                    y += jy;
                }
                b->x = (uint16_t)x;
                b->y = (uint16_t)y;
                b->bgx = s->field_1c;
                b->bgy = s->field_1e;
                b->intensity = (uint16_t)(s->field_14 << 8);
            }
        }
        /* Post-loop schedule on the CURRENT +0x24. */
        if (wo_mod((int32_t)s->field_24, 5) == 0 &&
            (int16_t)s->field_20 > 0x78)
            s->field_20 = (uint16_t)(s->field_20 - 1);
        if (wo_mod((int32_t)s->field_24, 0xa) == 0 &&
            (int16_t)s->field_22 > 0x50)
            s->field_22 = (uint16_t)(s->field_22 - 1);
        if (s->field_24 == 0x1e)             /* arg3 = 0x80<<9 */
            wo_fade(out, 0x10000, 0, 0x60);
        if (s->field_24 == 0x9e) {
            wo_fade(out, 0, 0x10000, 0x40);
            out->purge_547 = true;          /* sub_08000EA8(0x80) */
            out->purge_flagged = true;      /* +0x63==1 && +0x58&1
                                             * scan, 0xE0 entries */
            out->burst_63aec = true;        /* sub_08063AEC(self,
                                             * +0x20,+0x22) */
            out->burst_x = (int16_t)s->field_20;
            out->burst_y = (int16_t)s->field_22;
        }
        bool exited = false;
        if ((int16_t)s->field_24 <= 0x9f) {
            /* _08064CF6 particle/sparkle section (states 4/5,
             * +0x24 in [0,0x9F]). */
            if (s->field_18 <= 0x53 && (s->field_24 & 1) == 0)
                s->field_18 = (uint16_t)(s->field_18 + 1);
            if (wo_mod((int32_t)s->field_24, 5) == 0 &&
                (uint8_t)(s->field_14 - 1) > 0xbf)
                s->field_14 = (uint8_t)(s->field_14 - 1);
            if (wo_mod((int32_t)s->field_24, 0xa) == 0 &&
                (int16_t)s->field_24 <= 0x4f) {
                int32_t q = wo_div((int32_t)s->field_24, 10);
                wo_particle(out,
                            (int16_t)(q + 0x88 - (int32_t)(wo_rng(&rng) & 0xf)),
                            (int16_t)(0x26 - (int32_t)(wo_rng(&rng) & 0xf)),
                            0, 0);
                wo_particle(out,
                            (int16_t)(0x80 - (int32_t)(wo_rng(&rng) & 0x1f) - q),
                            (int16_t)(0x92 - (int32_t)(wo_rng(&rng) & 0x1f)),
                            1, 0);
            }
            if ((uint16_t)(s->field_24 - 0x48) <= 0x10) {
                wo_particle(out,
                            (int16_t)(0xb2 - (int32_t)(wo_rng(&rng) & 0xf)),
                            (int16_t)(0x80 - (int32_t)(wo_rng(&rng) & 0xf)),
                            1, 0);
            }
            if (s->field_24 == 0x60) {      /* _08064DDE: fixed spawn */
                wo_spawn(out, AOS_WO_SPAWN_SUB_6384C, 0, 0, 0);
            }
            if ((uint16_t)(s->field_24 - 0x70) <= 8) {
                wo_particle(out,
                            (int16_t)(0x9e - (int32_t)(wo_rng(&rng) & 0xf)),
                            (int16_t)(0x70 - (int32_t)(wo_rng(&rng) & 7)),
                            1, 0);
            }
            if ((uint16_t)(s->field_24 - 0x80) <= 0x10 &&
                (wo_rng(&rng) & 1) == 0) {
                wo_particle(out,
                            (int16_t)(0x48 - (int32_t)(wo_rng(&rng) & 7)),
                            (int16_t)(0x4e - (int32_t)(wo_rng(&rng) & 7)),
                            0, 0);
            }
            if ((s->field_24 & 3) == 0 &&
                (int16_t)s->field_24 > 0x70 &&
                (int16_t)s->field_24 <= 0x97) {
                int32_t q = wo_div((int32_t)s->field_24, 10);
                wo_particle(out,
                            (int16_t)(q + 0x88 - (int32_t)(wo_rng(&rng) & 0xf)),
                            (int16_t)(0x4e - (int32_t)(wo_rng(&rng) & 0xf)),
                            0, 0);
                wo_particle(out,
                            (int16_t)(0x78 - (int32_t)(wo_rng(&rng) & 0xf) - q),
                            (int16_t)(0x62 - (int32_t)(wo_rng(&rng) & 0xf)),
                            1, 0);
            }
        } else {
            /* _08064F0C: the +0x24 > 0x9F tail. Even frames either
             * exit the state (when +0x14 has ramped down to
             * <= 0xC0) or decrement +0x14. */
            if ((s->field_24 & 1) == 0) {
                uint8_t r = (uint8_t)(s->field_14 - 1);
                if (r <= 0xbf) {
                    s->field_18 = 0;
                    s->field_24 = 0;
                    s->state_d = 0;
                    s->field_15 = (uint8_t)(s->field_15 + 2);
                    s->field_3c = (uint8_t)(s->field_3c | 2);
                    exited = true;
                } else {
                    s->field_14 = r;
                }
            }
            if (!exited) {
                /* _08064F40: +0x1C += 0x80 sat 0x4000; +0x1E -= 4
                 * while nonzero. */
                uint32_t t = (uint32_t)s->field_1c + 0x80;
                s->field_1c = (t > 0x3fff) ? 0x4000 : (uint16_t)t;
                if (s->field_1e != 0)
                    s->field_1e = (uint16_t)(s->field_1e - 4);
            }
        }
        /* _08064F64: +0x24++ then sub_0806447C — reached on every
         * path EXCEPT the state-exit branch, which goes straight
         * to sub_080650CE. */
        if (!exited) {
            s->field_24 = (int16_t)(s->field_24 + 1);
            wo_cycle_6447c(s, out);     /* _08064F64 tail call */
        }
        break;

    case 6: /* _08064F72 — shared with case 7: settle + advance. */
    case 7:
        wo_draw_simple(s, out);
        if (s->field_1c <= 0xfffe)
            s->field_1c = (uint16_t)(s->field_1c + 0x640);
        s->state_d = (uint8_t)(s->state_d - 1);
        if (s->state_d == 0xff) {           /* wrapped from 0 */
            s->field_18 = 0;
            s->field_15 = (uint8_t)(s->field_15 + 1);
            s->state_d = 0;
        }
        break;

    case 8: /* _08064FE0 — final zoom; exits to +0x15 == 9. */
        wo_draw_simple(s, out);
        if (s->state_d == 0) {
            uint32_t t = (uint32_t)s->field_1c + 0x2000;
            s->field_1c = (t > 0x4000) ? 0x4000 : (uint16_t)t;
            {
                uint32_t e = (uint32_t)s->field_1e + 0x180;
                if (e > 0x4000) {
                    s->field_1e = 0x4000;
                    s->field_15 = (uint8_t)(s->field_15 + 1);
                    s->state_d = 0x10;
                } else {
                    s->field_1e = (uint16_t)e;
                }
            }
        } else {
            s->state_d = (uint8_t)(s->state_d - 1);
        }
        break;

    case 9: /* _08065060 — one more counter wrap to +0x15 == 0xA. */
        s->state_d = (uint8_t)(s->state_d - 1);
        if (s->state_d == 0xff) {
            s->state_d = 0;
            s->field_15 = (uint8_t)(s->field_15 + 1);
        }
        break;

    case 10: /* _0806507A — terminal hold; +0x15 stays 0xA, which is
              * the value sub_080628AC's case-3 gate waits for. */
        wo_draw_simple(s, out);
        {
            uint8_t old = s->state_d;
            s->state_d = (uint8_t)(old + 1);
            s->field_1e = (old & 4) ? 0x4000 : 0x3000;
        }
        break;

    default:
        break;      /* +0x15 > 10: tail only, like the native
                     * out-of-range jump-table fallthrough. */
    }

    out->rng = rng;
}
