/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_object20_whiteout_render.h"
#include <math.h>
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
    0x08063699, /* sub_08063698 — aux 3 (+0x24 == 0xC0); the ROM
                * word at 0x08118B34+0xC is 0x08063699 (thumb
                * pointer to the even sub_08063698 label) */
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

/* 0x081189A0 — sub_08063024's +0x65 pick table (variant 0):
 * three u32 words {0xA, 0xB, 0xC} indexed by rng % 3, low byte
 * stored. Verified against the ROM dump; sits 12 bytes ahead of
 * the 0x081189AC burst-particle table. */
static const uint8_t wo_spark_field65_1189a0[3] = {0x0a, 0x0b, 0x0c};

static void wo_particle(AosWoRenderOut *out, int32_t x, int32_t y,
                        uint8_t dir, uint8_t variant,
                        uint32_t *rng) {
    if (out->particle_count >= AOS_WO_MAX_EVENTS)
        return;
    AosWoParticle *p = &out->particles[out->particle_count++];
    p->x = (int16_t)x;
    p->y = (int16_t)y;
    p->dir = dir;
    p->variant = variant;
    /* sub_08063024's post-alloc RNG draw for +0x65 (the draw is
     * skipped natively only when sub_08062B60(0) returns NULL —
     * the renderer assumes the alloc, documented in the header). */
    p->field_65 = variant == 0
        ? wo_spark_field65_1189a0[wo_rng(rng) % 3]
        : (uint8_t)((wo_rng(rng) % 7) + 2);
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

/* ------------------------------------------------------------------
 * 0x08118B34 spawn-child update functions — sub_08063420 (aux 0),
 * sub_08063558 (aux 1), sub_08063600 (aux 2), sub_08063698 (aux 3),
 * sub_08063740 (aux 4). Decoded literally from
 * third_party/cvaos/asm/code/code_08060B98.s:5109-5655.
 *
 * Every routine memcpy's its entry table onto the stack and walks
 * the same +0xA/+0xC/+0xD machine (see the header for the full
 * contract). Entry records are 8 bytes: s16 dx @+0, s16 dy @+2,
 * u8 delay @+4 (loaded into entity +0xD); bytes +5..+7 are copied
 * but never read, so only the three used fields are committed.
 * Tables were decoded from the ROM at their native addresses; the
 * six spawn-related tables sit contiguously at 0x081189AC..0x08118B47.
 * ------------------------------------------------------------------ */
typedef struct {
    int16_t  dx;      /* record +0: fire x offset from +0x42 */
    int16_t  dy;      /* record +2: fire y offset from +0x46 */
    uint8_t  delay;   /* record +4: entity +0xD init value */
} WoChildEntry;

/* 0x081189CC — sub_08063420, aux 0, 15 entries (0x78 bytes). */
static const WoChildEntry wo_child_1189cc[15] = {
    {0, 0, 0}, {-20, -50, 5}, {10, -20, 5}, {-30, 30, 3},
    {30, 20, 3}, {8, 0, 2}, {-20, -42, 2}, {18, -20, 1},
    {-30, 38, 1}, {38, 20, 1}, {0, 8, 1}, {-12, -50, 1},
    {10, -12, 1}, {-22, 30, 1}, {30, 28, 1},
};

/* 0x08118A44 — sub_08063558, aux 1, 10 entries (0x50 bytes). */
static const WoChildEntry wo_child_118a44[10] = {
    {42, 10, 1}, {29, 55, 1}, {39, 45, 1}, {24, 50, 1}, {47, 35, 1},
    {42, 30, 1}, {57, 25, 1}, {47, 15, 1}, {34, 40, 1}, {52, 20, 1},
};

/* 0x08118A94 — sub_08063600, aux 2, 5 entries (0x28 bytes). */
static const WoChildEntry wo_child_118a94[5] = {
    {0, 0, 0x20}, {-20, -50, 0x20}, {10, -20, 0x20},
    {-30, 30, 0x20}, {30, 20, 0x20},
};

/* 0x08118ABC — sub_08063698, aux 3, 10 entries (0x50 bytes). */
static const WoChildEntry wo_child_118abc[10] = {
    {-45, 8, 1}, {-40, 20, 1}, {-30, -10, 1}, {-40, 0, 1},
    {-55, 18, 1}, {-50, 10, 1}, {-35, 38, 1}, {-35, -18, 1},
    {-30, 30, 1}, {-45, 28, 1},
};

/* 0x08118B0C — sub_08063740, aux 4, 5 entries (0x28 bytes). */
static const WoChildEntry wo_child_118b0c[5] = {
    {0, 0, 8}, {-20, -30, 8}, {10, -20, 8}, {-30, 30, 8}, {30, 20, 8},
};

static const WoChildEntry *const wo_child_tables[5] = {
    wo_child_1189cc, wo_child_118a44, wo_child_118a94,
    wo_child_118abc, wo_child_118b0c,
};

bool aos_wo_spawn_child_init(AosWoSpawnChild *c,
                             const AosWoSpawn *sp) {
    if (!c || !sp || sp->kind != AOS_WO_SPAWN_TABLE_118B34 ||
        sp->aux >= 5)
        return false;
    memset(c, 0, sizeof *c);
    c->aux = sp->aux;
    c->scr_x = sp->scr_x;
    c->scr_y = sp->scr_y;
    return true;
}

void aos_wo_spawn_child_update(AosWoSpawnChild *c,
                               const AosWoSpawnChildIn *in,
                               AosWoSpawnChildOut *out) {
    uint32_t rng = in->rng;
    memset(out, 0, sizeof *out);
    const WoChildEntry *t = wo_child_tables[c->aux];
    const uint8_t count = (uint8_t)(c->aux == 0 ? 15 :
                          c->aux == 1 || c->aux == 3 ? 10 : 5);
    if (c->field_c >= count) {
        /* Native keeps cycling on bytes past the copied stack
         * table until the host deletes the entity on +0x59 bit3;
         * the port treats the walk as finished (documented
         * approximation — see header). */
        out->done = (c->field_59 & 8) != 0;
        out->rng = rng;
        return;
    }
    const WoChildEntry *e = &t[c->field_c];
    if (c->field_a == 0) {
        /* _08063430/_08063572/_08063640/_080636D2/_0806378A:
         * delay init from record +4. */
        c->field_d = e->delay;
        c->field_a = 1;
    } else if (c->field_a == 1) {
        /* Delay countdown: store +0xD-1, advance only when the
         * u8 result wraps to 0xFF (old +0xD == 0). */
        uint8_t nd = (uint8_t)(c->field_d - 1);
        c->field_d = nd;
        if (nd == 0xff)
            c->field_a = 2;
    } else {
        /* Fire branch (+0xA >= 2; == 2 in the native machine).
         * Position math: s16 wrap after the adds/subtracts. */
        int32_t x = (int32_t)(int16_t)(c->scr_x + e->dx);
        int32_t y = (int32_t)(int16_t)(c->scr_y + e->dy);
        if (c->aux == 0 || c->aux == 4) {
            /* sub_08063420/_08063740: parity of +0xA picks the
             * callee — odd -> sub_08063168(x,y); even ->
             * EntityCreateInRange(0x1B,0x48,sub_08065835) with
             * sub_0806E0D0 gate, grandchild +0x42/+0x46 = fire
             * pos, +0x5C = 0, sub_0803F2C8(arg1 = RNG & 3),
             * sub_0803F17C. */
            if (c->field_a & 1) {
                out->burst_63168 = true;
                out->burst_x = (int16_t)x;
                out->burst_y = (int16_t)y;
            } else {
                out->carrier_spawn = true;
                out->carrier_x = (int16_t)x;
                out->carrier_y = (int16_t)y;
                out->carrier_variant = (uint8_t)(wo_rng(&rng) & 3);
            }
        } else if (c->aux == 2) {
            /* sub_08063600: sub_08063168(x,y), no RNG in-routine. */
            out->burst_63168 = true;
            out->burst_x = (int16_t)x;
            out->burst_y = (int16_t)y;
        } else {
            /* aux 1/3 — sub_08063558/_08063698: two RNG draws,
             * x -= (rng&3)-2 and y -= (rng&3)-2 (s16 wrap),
             * then sub_080632B0(x,y). */
            x = (int32_t)(int16_t)(x - ((int32_t)(wo_rng(&rng) & 3) - 2));
            y = (int32_t)(int16_t)(y - ((int32_t)(wo_rng(&rng) & 3) - 2));
            out->burst_632b0 = true;
            out->burst_632b0_x = (int16_t)x;
            out->burst_632b0_y = (int16_t)y;
        }
        c->field_a = 0;
        c->field_c = (uint8_t)(c->field_c + 1);
    }
    /* Shared tail: +0xC == count -> entity +0x59 |= 8. Runs on
     * every call, including the init/countdown phases. */
    if (c->field_c == count)
        c->field_59 = (uint8_t)(c->field_59 | 8);
    out->done = (c->field_59 & 8) != 0;
    out->rng = rng;
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
            out->burst_parent = in->field10;/* caller +0x10 ->
                                             * particle +0x11 */
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
                /* Native draw order per spawn is x, y, then the
                 * +0x65 pick inside sub_08063024 — hoisted into
                 * locals so the unspecified C argument-evaluation
                 * order cannot reorder the x/y draws. */
                int32_t rx = wo_rng(&rng) & 0xf;
                int32_t ry = wo_rng(&rng) & 0xf;
                wo_particle(out,
                            (int16_t)(q + 0x88 - rx),
                            (int16_t)(0x26 - ry), 0, 0, &rng);
                rx = wo_rng(&rng) & 0x1f;
                ry = wo_rng(&rng) & 0x1f;
                wo_particle(out,
                            (int16_t)(0x80 - rx - q),
                            (int16_t)(0x92 - ry), 1, 0, &rng);
            }
            if ((uint16_t)(s->field_24 - 0x48) <= 0x10) {
                int32_t rx = wo_rng(&rng) & 0xf;
                int32_t ry = wo_rng(&rng) & 0xf;
                wo_particle(out,
                            (int16_t)(0xb2 - rx),
                            (int16_t)(0x80 - ry), 1, 0, &rng);
            }
            if (s->field_24 == 0x60) {      /* _08064DDE: fixed spawn */
                wo_spawn(out, AOS_WO_SPAWN_SUB_6384C, 0, 0, 0);
            }
            if ((uint16_t)(s->field_24 - 0x70) <= 8) {
                int32_t rx = wo_rng(&rng) & 0xf;
                int32_t ry = wo_rng(&rng) & 7;
                wo_particle(out,
                            (int16_t)(0x9e - rx),
                            (int16_t)(0x70 - ry), 1, 0, &rng);
            }
            if ((uint16_t)(s->field_24 - 0x80) <= 0x10 &&
                (wo_rng(&rng) & 1) == 0) {
                int32_t rx = wo_rng(&rng) & 7;
                int32_t ry = wo_rng(&rng) & 7;
                wo_particle(out,
                            (int16_t)(0x48 - rx),
                            (int16_t)(0x4e - ry), 0, 0, &rng);
            }
            if ((s->field_24 & 3) == 0 &&
                (int16_t)s->field_24 > 0x70 &&
                (int16_t)s->field_24 <= 0x97) {
                int32_t q = wo_div((int32_t)s->field_24, 10);
                int32_t rx = wo_rng(&rng) & 0xf;
                int32_t ry = wo_rng(&rng) & 0xf;
                wo_particle(out,
                            (int16_t)(q + 0x88 - rx),
                            (int16_t)(0x4e - ry), 0, 0, &rng);
                rx = wo_rng(&rng) & 0xf;
                ry = wo_rng(&rng) & 0xf;
                wo_particle(out,
                            (int16_t)(0x78 - rx - q),
                            (int16_t)(0x62 - ry), 1, 0, &rng);
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

/* ------------------------------------------------------------------
 * Whiteout burst callees — sub_080630E8 (particle update),
 * sub_08063168 / sub_080632B0 (carrier + particle burst spawners)
 * and sub_08065834 (carrier update; Thumb pointer 0x08065835).
 * Decoded literally from third_party/cvaos/asm/code/code_08060B98.s:
 *   sub_080630E8  :4720-4786
 *   sub_08063168  :4788-4938
 *   sub_080632B0  :4940-5107
 *   sub_08065834  :9678-9698
 * sub_0806E0D0 (the carrier resource gate, :27568-27607) is engine
 * code — its low-byte result is injected through AosWoBurstIn.
 * ------------------------------------------------------------------ */

/* Committed ROM tables.
 *
 * 0x081189AC: the first three u32 words {0x0000000C, 0x0000000B,
 * 0x0000000C}. Each burst particle draws RNG % 3 and stores the
 * chosen word's LOW BYTE into the particle's +0x65. The next five
 * words at 0x081189B8 {0,2,3,2,3} are sub_080632B0's carrier
 * variant table (sub_0803F2C8 arg1, s16). Both tables sit
 * contiguously ahead of the 0x081189CC child table. */
static const uint8_t wo_burst_field65_1189ac[3] = {0x0c, 0x0b, 0x0c};
static const int16_t wo_burst_variant_1189b8[5] = {0, 2, 3, 2, 3};

/* The particle prologue shared by the sub_08063168/_080632B0
 * loops (code_08060B98.s:4850-4924 and :5017-5092). Per particle:
 *   sub_08062B60(0) — EntityCreateInRange(0x1B,0x48,
 *     EntityUpdateNothing) + graphics 0x08260390 load + frames
 *     0x08267EE4 anim 0xE + draw sub_0803B9D0 + +0x2C = 0xE +
 *     +0x5C = 0x11. NULL (pool or graphics failure) skips the
 *     particle BEFORE any RNG draw — modelled by
 *     AosWoBurstIn::particle_alloc_fail. The engine writes are
 *     host-side; the port starts from the zeroed entity block
 *     (documented approximation — the native entity pool provides
 *     the cleared state).
 *   updateFunc (+0) = sub_080630E8 (implicit: this struct IS that
 *     update), then six RNG draws:
 *     +0x42 = (u16)(x + 4 - (rng & 7)),  +0x46 likewise for y
 *     +0x4C = trunc-div-8 of (0x14000 - (rng & 0x7FFF))
 *     +0x54 = ((rng & 0x7FF) + 0x800) >> 3
 *     +0x5C = 3,  +0x65 = table1189AC[rng % 3] low byte,
 *     +0x63 = 0x80,  +0x58 |= 0x20
 *     +0xD  = delay_kind 0: (rng % 0x14) + 0x20  (sub_08063168)
 *             delay_kind 1: rng & 7            (sub_080632B0)
 *   The strh +0x42/+0x46 writes are the HIGH words of the
 *   +0x40/+0x44 16.16 position pair — field_40/44 start at
 *   scr << 16 with the fresh-entity low words 0.
 *   +0xA stays 0 (fresh entity) so the first sub_080630E8 call
 *   enters the delay-countdown phase. */
static void wo_burst_particle_init(AosWoBurstParticle *p,
                                   int16_t x, int16_t y,
                                   uint32_t *rng, int delay_kind) {
    memset(p, 0, sizeof *p);
    p->scr_42 = (int16_t)(uint16_t)((uint32_t)(uint16_t)x + 4 -
                                    (wo_rng(rng) & 7));
    p->scr_46 = (int16_t)(uint16_t)((uint32_t)(uint16_t)y + 4 -
                                    (wo_rng(rng) & 7));
    int32_t vy = 0x14000 - (int32_t)(wo_rng(rng) & 0x7fffu);
    int32_t ay = (int32_t)(wo_rng(rng) & 0x7ffu) + 0x800;
    /* Native fixup: negative +0x4C adds 7 before the arithmetic
     * >>3 (C-style truncation toward zero). vy is always positive
     * here (0xC001..0x14000) but the sequence is kept literal. */
    if (vy < 0)
        vy += 7;
    p->field_4c = vy >> 3;
    p->field_54 = ay >> 3;
    p->field_5c = 3;
    p->field_65 = wo_burst_field65_1189ac[wo_rng(rng) % 3];
    p->field_63 = 0x80;
    p->field_58 = (uint8_t)(p->field_58 | 0x20);
    p->field_d = delay_kind == 0
        ? (uint8_t)((wo_rng(rng) % 0x14) + 0x20)
        : (uint8_t)(wo_rng(rng) & 7);
    /* The native strh +0x42/+0x46 writes land on the HIGH words
     * of the +0x40/+0x44 16.16 position pair (+0x42/+0x46 are
     * aliases, not separate fields): the fresh entity's low
     * words stay 0, so +0x40 = scr_x << 16, +0x44 = scr_y << 16.
     * scr_42/scr_46 keep the same values for callers that read
     * the spawn words. */
    p->field_40 = (int32_t)((uint32_t)(uint16_t)p->scr_42 << 16);
    p->field_44 = (int32_t)((uint32_t)(uint16_t)p->scr_46 << 16);
}

bool aos_wo_carrier_spawn(AosWoBurstCarrier *c, int16_t x,
                          int16_t y, int16_t variant,
                          const AosWoBurstIn *in,
                          AosWoBurstOut *out) {
    memset(out, 0, sizeof *out);
    out->rng = in->rng;         /* the creation sequence draws no
                                 * RNG of its own */
    if (!in->carrier_alloc_ok)
        return false;           /* EntityCreateInRange -> NULL:
                                 * native skips setup, particles
                                 * still spawn */
    if (!in->gate_6e0d0_ok) {
        out->carrier_deleted = true;    /* EntityDelete(entity) */
        return false;
    }
    memset(c, 0, sizeof *c);
    c->scr_42 = x;              /* strh +0x42 / +0x46 */
    c->scr_46 = y;
    c->field_5c = 0;            /* strb +0x5C = 0 */
    c->variant = variant;       /* sub_0803F2C8 arg1 (s16) */
    out->carrier_spawned = true;
    out->carrier_f2c8 = true;   /* sub_0803F2C8(entity, variant,0,0) */
    out->carrier_f17c = true;   /* sub_0803F17C(entity) */
    return true;
}

void aos_wo_burst_63168(AosWoBurst *b, int16_t x, int16_t y,
                        const AosWoBurstIn *in,
                        AosWoBurstOut *out) {
    uint32_t rng = in->rng;
    memset(out, 0, sizeof *out);
    memset(b, 0, sizeof *b);
    /* Carrier first (code_08060B98.s:4794-4846): sub_0803F2C8
     * arg1 = 0, no RNG inside the routine itself. */
    if (aos_wo_carrier_spawn(&b->carrier, x, y, 0, in, out))
        b->carrier_valid = true;
    /* r5 = 2..0: three particles, six RNG draws each. */
    for (unsigned i = 0; i < AOS_WO_BURST_MAX_PARTICLES; ++i) {
        if (in->particle_alloc_fail & (1u << i))
            continue;                    /* sub_08062B60 -> NULL:
                                          * skip before any draw */
        wo_burst_particle_init(&b->particles[b->particle_count],
                               x, y, &rng, 0);
        b->particle_count++;
    }
    out->rng = rng;
}

void aos_wo_burst_632b0(AosWoBurst *b, int16_t x, int16_t y,
                        const AosWoBurstIn *in,
                        AosWoBurstOut *out) {
    uint32_t rng = in->rng;
    memset(out, 0, sizeof *out);
    memset(b, 0, sizeof *b);
    /* RNG % 5 over the 0x081189B8 word table picks the carrier's
     * sub_0803F2C8 variant — drawn BEFORE EntityCreateInRange
     * (code_08060B98.s:4966-4971), so it is consumed even when
     * the alloc or the sub_0806E0D0 gate fails. */
    int16_t variant = wo_burst_variant_1189b8[wo_rng(&rng) % 5];
    if (aos_wo_carrier_spawn(&b->carrier, x, y, variant, in, out))
        b->carrier_valid = true;
    /* r6 = 1..0: two particles, six RNG draws each; the +0xD
     * delay is rng & 7 (no +0x20 bias, no modulo). */
    for (unsigned i = 0; i < 2; ++i) {
        if (in->particle_alloc_fail & (1u << i))
            continue;
        wo_burst_particle_init(&b->particles[b->particle_count],
                               x, y, &rng, 1);
        b->particle_count++;
    }
    out->rng = rng;
}

void aos_wo_carrier_update(AosWoBurstCarrier *c,
                           AosWoBurstCarrierOut *out) {
    /* sub_08065834 (code_08060B98.s:9678-9698): +0x59 bit0 set
     * marks bit3 for deletion; otherwise the sub_0803F17C
     * animation step runs. */
    memset(out, 0, sizeof *out);
    if (c->flags59 & 1) {
        c->flags59 = (uint8_t)(c->flags59 | 8);
        out->done = true;
    } else {
        out->anim_step = true;
    }
}

void aos_wo_particle_update(AosWoBurstParticle *p,
                            const AosWoBurstParticleIn *in,
                            AosWoBurstParticleOut *out) {
    /* sub_080630E8 (code_08060B98.s:4720-4786). */
    memset(out, 0, sizeof *out);
    if (p->field_a == 0) {
        /* Delay countdown: store +0xD-1; only the u8 wrap to
         * 0xFF (old +0xD == 0) advances — it also clears +0x58
         * bit5 (the spawn-held flag) and sets +0xA = 1. */
        uint8_t nd = (uint8_t)(p->field_d - 1);
        p->field_d = nd;
        if (nd == 0xff) {
            p->field_58 = (uint8_t)(p->field_58 & (uint8_t)~0x20);
            p->field_a = 1;
        }
    } else if (p->field_a == 1) {
        /* Integrate: position uses the OLD velocity, then the
         * velocity picks up the acceleration (s32 wrap). */
        p->field_40 += p->field_48;
        p->field_44 += p->field_4c;
        p->field_48 += p->field_50;
        p->field_4c += p->field_54;
        out->probe_ran = true;
        /* sub_08068AD4(entity, -0x80, -0x40, 0x1F0, 0x120):
         * nonzero keeps the particle; zero sets +0x59 bit3. */
        if (!in->probe_inside) {
            p->field_59 = (uint8_t)(p->field_59 | 8);
            out->probe_missed = true;
        }
    }
    /* +0xA >= 2: native falls straight to the epilogue. */
    out->done = (p->field_59 & 8) != 0;
}

/* ------------------------------------------------------------------
 * sub_08063024 / sub_08065858 — the renderer's sparkle particles
 * (code_08060B98.s:4624-4719 spawn, :9701-9746 update).
 *
 * sub_08063024 runs the shared sub_08062B60(0) alloc prologue
 * (EntityCreateInRange(0x1B, 0x48, EntityUpdateNothing) +
 * graphics 0x08260390 + frames 0x08267EE4 anim 0xE + draw
 * sub_0803B9D0 + +0x2C = 0xE + +0x5C = 0x11), then on success:
 *   +0    = sub_08065858          +0x42/+0x46 = s16 x / s16 y
 *   +0x48 = 0x30000, +0x4C = -0x8000, +0x50 = 0x800, +0x54 = -0x100
 *          (all four negated when dir != 0)
 *   +0xD  = 0x40                  +0x5C = 3 (over the 0x11)
 *   +0x65 = variant ? (rng % 7) + 2 : table1189A0[rng % 3] low byte
 *   +0x63 = 0x80
 * The strh +0x42/+0x46 writes are the HIGH words of the
 * +0x40/+0x44 16.16 position pair (+0x42/+0x46 alias them), so
 * field_40/44 start at x/y << 16. The +0x65 RNG draw is
 * consumed inside the renderer's wo_particle (the alloc is
 * host-side; see the header note).
 *
 * sub_08065858 integrates +0x40/+0x44 with +0x48/+0x4C, adds
 * +0x50/+0x54 into the velocities, then probes
 * sub_08068AD4(entity, -0x80, -0x40, 0x1F0, 0x120) on +0x42/+0x46;
 * a miss sets +0x59 bit 3 (the host delete mark). No +0xA delay
 * phase, no lifetime counter — +0xD = 0x40 is written at spawn
 * but never read by this update.
 * ------------------------------------------------------------------ */
void aos_wo_spark_spawn(AosWoSpark *p, const AosWoParticle *ev) {
    memset(p, 0, sizeof *p);
    p->scr_42 = ev->x;              /* strh +0x42 / +0x46 */
    p->scr_46 = ev->y;
    /* The fixed quad, negated as a unit when dir != 0. */
    int32_t vx = 0x30000, vy = -0x8000, ax = 0x800, ay = -0x100;
    if (ev->dir) {
        vx = -vx; vy = -vy; ax = -ax; ay = -ay;
    }
    p->field_48 = vx;
    p->field_4c = vy;
    p->field_50 = ax;
    p->field_54 = ay;
    p->field_d = 0x40;
    p->field_5c = 3;
    p->field_65 = ev->field_65;
    p->field_63 = 0x80;
    /* sub_08063024's strh +0x42/+0x46 writes are the HIGH words
     * of the +0x40/+0x44 16.16 position pair — the "screen
     * words" are aliases, so the spawn position lands in
     * field_40/44 as x/y << 16 with the fresh-entity low words
     * 0 (the previous "+0x40 stays 0" note misread the alias). */
    p->field_40 = (int32_t)((uint32_t)(uint16_t)ev->x << 16);
    p->field_44 = (int32_t)((uint32_t)(uint16_t)ev->y << 16);
}

void aos_wo_spark_update(AosWoSpark *p, const AosWoSparkIn *in,
                         AosWoSparkOut *out) {
    /* sub_08065858 (code_08060B98.s:9701-9746): integrate then
     * probe every call — no state byte gates either half. */
    memset(out, 0, sizeof *out);
    p->field_40 += p->field_48;
    p->field_44 += p->field_4c;
    p->field_48 += p->field_50;
    p->field_4c += p->field_54;
    /* sub_08068AD4(entity, -0x80, -0x40, 0x1F0, 0x120): nonzero
     * keeps the spark; zero sets +0x59 bit3. */
    if (!in->probe_inside) {
        p->field_59 = (uint8_t)(p->field_59 | 8);
        out->probe_missed = true;
    }
    out->done = (p->field_59 & 8) != 0;
}


/* ------------------------------------------------------------------
 * sub_08063AEC / sub_08063944 — the state-4/5 +0x24 == 0x9E burst
 * (code_08060B98.s:5990-6152 spawn, :5781-5987 update, :28044-
 * 28133 + :16448-16497 the sub_0806E43C/sub_08068CDC homing
 * step). See the header for the field map.
 * ------------------------------------------------------------------ */

/* The committed 0x08118B4C record table (20 x 8 bytes, decoded
 * from the ROM; only the four used fields are kept). */
static const AosWo63AECRec wo_63aec_table_118b4c[AOS_WO63AEC_MAX] = {
    {0x0000, 2, 2, 0x0800}, {0x2000, 2, 2, 0x0800},
    {0x4000, 2, 2, 0x0800}, {0x6000, 2, 2, 0x0800},
    {0x8000, 2, 2, 0x0800}, {0xA000, 2, 2, 0x0800},
    {0xC000, 2, 2, 0x0800}, {0xE000, 2, 2, 0x0800},
    {0x1000, 4, 4, 0x0800}, {0x4000, 2, 2, 0x0800},
    {0x7000, 4, 4, 0x0800}, {0x9000, 4, 4, 0x0800},
    {0xC000, 2, 2, 0x0800}, {0xF000, 4, 4, 0x0800},
    {0x0800, 6, 6, 0x0800}, {0x7800, 6, 6, 0x0800},
    {0x8800, 6, 6, 0x0800}, {0xF800, 6, 6, 0x0800},
    {0x0000, 8, 8, 0x0600}, {0x8000, 8, 8, 0x0600},
};

/* sub_080009E4's sSineTable[0x400] — the u16 Q16 sine quadrant
 * table committed verbatim from cvaos code_080009A0.c (the
 * decompilation's Q_16_16(float) literals evaluated to their
 * compiled u16 values). */
static const uint16_t wo_sine_table[0x400] = {
    0, 100, 201, 301, 402, 502, 603, 703, 804, 904, 1005, 1105, 1206, 1306, 1407, 1507,
    1608, 1708, 1809, 1909, 2010, 2110, 2211, 2311, 2412, 2512, 2613, 2713, 2814, 2914, 3014, 3115,
    3215, 3316, 3416, 3516, 3617, 3717, 3818, 3918, 4018, 4119, 4219, 4319, 4420, 4520, 4620, 4720,
    4821, 4921, 5021, 5121, 5222, 5322, 5422, 5522, 5622, 5722, 5823, 5923, 6023, 6123, 6223, 6323,
    6423, 6523, 6623, 6723, 6823, 6923, 7023, 7123, 7223, 7323, 7423, 7523, 7623, 7722, 7822, 7922,
    8022, 8122, 8221, 8321, 8421, 8520, 8620, 8720, 8819, 8919, 9019, 9118, 9218, 9317, 9417, 9516,
    9616, 9715, 9814, 9914, 10013, 10113, 10212, 10311, 10410, 10510, 10609, 10708, 10807, 10906, 11006, 11105,
    11204, 11303, 11402, 11501, 11600, 11699, 11797, 11896, 11995, 12094, 12193, 12292, 12390, 12489, 12588, 12686,
    12785, 12884, 12982, 13081, 13179, 13278, 13376, 13474, 13573, 13671, 13769, 13868, 13966, 14064, 14162, 14260,
    14359, 14457, 14555, 14653, 14751, 14849, 14946, 15044, 15142, 15240, 15338, 15435, 15533, 15631, 15728, 15826,
    15923, 16021, 16118, 16216, 16313, 16411, 16508, 16605, 16702, 16800, 16897, 16994, 17091, 17188, 17285, 17382,
    17479, 17576, 17672, 17769, 17866, 17963, 18059, 18156, 18253, 18349, 18446, 18542, 18638, 18735, 18831, 18927,
    19024, 19120, 19216, 19312, 19408, 19504, 19600, 19696, 19792, 19888, 19983, 20079, 20175, 20270, 20366, 20461,
    20557, 20652, 20748, 20843, 20938, 21034, 21129, 21224, 21319, 21414, 21509, 21604, 21699, 21794, 21889, 21983,
    22078, 22173, 22267, 22362, 22456, 22551, 22645, 22739, 22833, 22928, 23022, 23116, 23210, 23304, 23398, 23492,
    23586, 23679, 23773, 23867, 23960, 24054, 24147, 24241, 24334, 24427, 24521, 24614, 24707, 24800, 24893, 24986,
    25079, 25172, 25265, 25357, 25450, 25543, 25635, 25728, 25820, 25913, 26005, 26097, 26189, 26281, 26373, 26465,
    26557, 26649, 26741, 26833, 26925, 27016, 27108, 27199, 27291, 27382, 27473, 27565, 27656, 27747, 27838, 27929,
    28020, 28111, 28201, 28292, 28383, 28473, 28564, 28654, 28745, 28835, 28925, 29015, 29105, 29196, 29285, 29375,
    29465, 29555, 29645, 29734, 29824, 29913, 30003, 30092, 30181, 30271, 30360, 30449, 30538, 30627, 30715, 30804,
    30893, 30982, 31070, 31159, 31247, 31335, 31424, 31512, 31600, 31688, 31776, 31864, 31952, 32039, 32127, 32215,
    32302, 32390, 32477, 32564, 32651, 32738, 32826, 32912, 32999, 33086, 33173, 33260, 33346, 33433, 33519, 33605,
    33692, 33778, 33864, 33950, 34036, 34122, 34208, 34293, 34379, 34465, 34550, 34635, 34721, 34806, 34891, 34976,
    35061, 35146, 35231, 35316, 35400, 35485, 35569, 35654, 35738, 35822, 35906, 35990, 36074, 36158, 36242, 36326,
    36409, 36493, 36576, 36660, 36743, 36826, 36909, 36992, 37075, 37158, 37241, 37324, 37406, 37489, 37571, 37653,
    37736, 37818, 37900, 37982, 38064, 38146, 38227, 38309, 38390, 38472, 38553, 38634, 38716, 38797, 38878, 38958,
    39039, 39120, 39201, 39281, 39362, 39442, 39522, 39602, 39682, 39762, 39842, 39922, 40002, 40081, 40161, 40240,
    40319, 40399, 40478, 40557, 40636, 40714, 40793, 40872, 40950, 41029, 41107, 41185, 41263, 41342, 41419, 41497,
    41575, 41653, 41730, 41808, 41885, 41962, 42040, 42117, 42194, 42271, 42347, 42424, 42501, 42577, 42653, 42730,
    42806, 42882, 42958, 43034, 43110, 43185, 43261, 43336, 43412, 43487, 43562, 43637, 43712, 43787, 43862, 43936,
    44011, 44085, 44160, 44234, 44308, 44382, 44456, 44530, 44603, 44677, 44750, 44824, 44897, 44970, 45043, 45116,
    45189, 45262, 45335, 45407, 45480, 45552, 45624, 45696, 45768, 45840, 45912, 45984, 46055, 46127, 46198, 46269,
    46340, 46411, 46482, 46553, 46624, 46695, 46765, 46835, 46906, 46976, 47046, 47116, 47186, 47255, 47325, 47394,
    47464, 47533, 47602, 47671, 47740, 47809, 47878, 47946, 48015, 48083, 48151, 48219, 48288, 48355, 48423, 48491,
    48558, 48626, 48693, 48760, 48828, 48895, 48961, 49028, 49095, 49161, 49228, 49294, 49360, 49426, 49492, 49558,
    49624, 49690, 49755, 49820, 49886, 49951, 50016, 50081, 50146, 50210, 50275, 50339, 50403, 50468, 50532, 50596,
    50660, 50723, 50787, 50850, 50914, 50977, 51040, 51103, 51166, 51229, 51291, 51354, 51416, 51478, 51541, 51603,
    51665, 51726, 51788, 51850, 51911, 51972, 52033, 52095, 52155, 52216, 52277, 52338, 52398, 52458, 52518, 52579,
    52639, 52698, 52758, 52818, 52877, 52936, 52996, 53055, 53114, 53172, 53231, 53290, 53348, 53407, 53465, 53523,
    53581, 53639, 53696, 53754, 53811, 53869, 53926, 53983, 54040, 54097, 54153, 54210, 54266, 54323, 54379, 54435,
    54491, 54546, 54602, 54658, 54713, 54768, 54823, 54879, 54933, 54988, 55043, 55097, 55152, 55206, 55260, 55314,
    55368, 55422, 55475, 55529, 55582, 55635, 55688, 55741, 55794, 55847, 55899, 55952, 56004, 56056, 56108, 56160,
    56212, 56263, 56315, 56366, 56417, 56468, 56519, 56570, 56621, 56671, 56722, 56772, 56822, 56872, 56922, 56972,
    57022, 57071, 57120, 57170, 57219, 57268, 57316, 57365, 57414, 57462, 57510, 57558, 57606, 57654, 57702, 57750,
    57797, 57844, 57892, 57939, 57986, 58032, 58079, 58125, 58172, 58218, 58264, 58310, 58356, 58402, 58447, 58493,
    58538, 58583, 58628, 58673, 58718, 58762, 58807, 58851, 58895, 58939, 58983, 59027, 59070, 59114, 59157, 59200,
    59243, 59286, 59329, 59372, 59414, 59457, 59499, 59541, 59583, 59625, 59666, 59708, 59749, 59790, 59831, 59872,
    59913, 59954, 59994, 60035, 60075, 60115, 60155, 60195, 60235, 60274, 60313, 60353, 60392, 60431, 60470, 60508,
    60547, 60585, 60624, 60662, 60700, 60737, 60775, 60813, 60850, 60887, 60924, 60961, 60998, 61035, 61071, 61108,
    61144, 61180, 61216, 61252, 61288, 61323, 61359, 61394, 61429, 61464, 61499, 61533, 61568, 61602, 61637, 61671,
    61705, 61738, 61772, 61805, 61839, 61872, 61905, 61938, 61971, 62003, 62036, 62068, 62100, 62133, 62164, 62196,
    62228, 62259, 62291, 62322, 62353, 62384, 62414, 62445, 62475, 62506, 62536, 62566, 62596, 62625, 62655, 62684,
    62714, 62743, 62772, 62800, 62829, 62858, 62886, 62914, 62942, 62970, 62998, 63026, 63053, 63080, 63108, 63135,
    63162, 63188, 63215, 63241, 63268, 63294, 63320, 63346, 63371, 63397, 63422, 63447, 63473, 63498, 63522, 63547,
    63571, 63596, 63620, 63644, 63668, 63692, 63715, 63739, 63762, 63785, 63808, 63831, 63854, 63876, 63899, 63921,
    63943, 63965, 63987, 64009, 64030, 64051, 64073, 64094, 64115, 64135, 64156, 64176, 64197, 64217, 64237, 64257,
    64276, 64296, 64315, 64334, 64353, 64372, 64391, 64410, 64428, 64447, 64465, 64483, 64501, 64518, 64536, 64553,
    64571, 64588, 64605, 64622, 64638, 64655, 64671, 64687, 64703, 64719, 64735, 64751, 64766, 64781, 64796, 64811,
    64826, 64841, 64855, 64870, 64884, 64898, 64912, 64926, 64939, 64953, 64966, 64979, 64992, 65005, 65018, 65030,
    65043, 65055, 65067, 65079, 65091, 65102, 65114, 65125, 65136, 65147, 65158, 65169, 65179, 65190, 65200, 65210,
    65220, 65230, 65239, 65249, 65258, 65267, 65276, 65285, 65294, 65302, 65311, 65319, 65327, 65335, 65343, 65350,
    65358, 65365, 65372, 65379, 65386, 65393, 65400, 65406, 65412, 65418, 65424, 65430, 65436, 65441, 65446, 65452,
    65457, 65461, 65466, 65471, 65475, 65479, 65483, 65487, 65491, 65495, 65498, 65501, 65505, 65508, 65511, 65513,
    65516, 65518, 65520, 65522, 65524, 65526, 65528, 65529, 65531, 65532, 65533, 65534, 65534, 65535, 65535, 65535,
};

/* sub_080009E4 (code_080009A0.c): the Q16 sine/cosine pair the
 * burst uses as 9E4(angle) / 9E4(angle + 0x4000). Literal port. */
static int32_t wo_9e4(int32_t param_0) {
    param_0 += 0x80 >> 4;               /* + 8 */
    param_0 &= 0xFFFF;
    if (param_0 & 0x8000) {
        param_0 -= 0x8000;
        if (param_0 & 0x4000) {
            if (param_0 & (int32_t)0xFFFFBFF0)
                return -(int32_t)wo_sine_table[(0x800F - param_0) >> 4];
            return -0x10000;
        }
        return -(int32_t)wo_sine_table[param_0 >> 4];
    }
    if (param_0 & 0x4000) {
        if (param_0 & (int32_t)0xFFFFBFF0)
            return (int32_t)wo_sine_table[(0x800F - param_0) >> 4];
        return 0x10000;
    }
    return (int32_t)wo_sine_table[param_0 >> 4];
}

/* Sqrt (BIOS svc 8): floor sqrt, u16 result. */
static uint16_t wo_sqrt(uint32_t num) {
    uint32_t r = (uint32_t)sqrt((double)num);
    while (r * r > num) --r;
    while ((r + 1) * (r + 1) <= num) ++r;
    return (uint16_t)r;
}

/* ArcTan2 (BIOS svc 0xA): angle of vector (x, y) in 0..0xFFFF,
 * 0 = +X, 0x4000 = +Y (y-down clockwise). The BIOS resolves the
 * angle through its own table; this quantizes atan2 to the same
 * 1/65536-turn units — documented approximation, at most a
 * 1-LSB difference. (0,0) returns 0 like the BIOS degenerate. */
static uint16_t wo_atan2(int32_t x, int32_t y) {
    if (x == 0 && y == 0)
        return 0;
    double a = atan2((double)y, (double)x);
    if (a < 0)
        a += 6.28318530717958647692;
    return (uint16_t)(a * (65536.0 / 6.28318530717958647692));
}

/* sub_08068CDC (code_08060B98.s:16448-16497) reduced to the
 * sub_0806E43C call shape (r3 = 0, stack args 0): r5 = step, the
 * clamp keeps it (lower bound 0), then
 *   +0x40 += (9E4(angle + 0x4000) >> 8) * (r5 >> 8)
 *   +0x44 += (9E4(angle)        >> 8) * (r5 >> 8)
 * and r5 is returned. Ported with the native branch structure
 * so the clamp semantics stay literal for any caller. */
static int32_t wo_68cdc(AosWo63AEC *p, uint32_t angle,
                        int32_t step) {
    int32_t r3 = 0;                     /* arg3 */
    int32_t r4 = step;                  /* arg2 */
    int32_t r5 = step;                  /* + stack-product (0) */
    /* cmp r3,r4; ble CFE / cmp r5,r3; bgt D06 / CFE: cmp r3,r4;
     * bge D08 / cmp r5,r3; bge D08 / D06: r5 = r3 / D08: */
    if (r3 <= r4) {
        if (!(r3 >= r4) && !(r5 >= r3))
            r5 = r3;
    } else {
        if (r5 > r3)
            r5 = r3;
        else if (!(r5 >= r3))
            r5 = r3;
    }
    int32_t step8 = r5 >> 8;            /* asrs r4, r5, #8 */
    p->field_40 += (wo_9e4((int32_t)angle + 0x4000) >> 8) * step8;
    p->field_44 += (wo_9e4((int32_t)angle) >> 8) * step8;
    return r5;
}

/* sub_0806E43C(entity, 0x780000, 0x500000, 3)
 * (code_08060B98.s:28044-28133): distance from the +0x40/+0x44
 * 16.16 position to screen (0x78, 0x50), ArcTan2 of the +0x42/
 * +0x46 (high-word) delta, then sub_08068CDC with step =
 * distance << 16 >> 3. Returns that step — zero only when the
 * integer pixel distance is 0, which is the caller's delete
 * condition. */
static int32_t wo_6e43c(AosWo63AEC *p) {
    int32_t dx = p->field_40 - 0x780000;
    int32_t dy = p->field_44 - 0x500000;
    int32_t px = dx >> 16;              /* asrs #0x10 */
    int32_t py = dy >> 16;
    uint32_t d2 = (uint32_t)(px * px + py * py);
    uint16_t dist = wo_sqrt(d2);
    /* r7 = dist << 16 (32-bit wrap), then lsrs by arg3 & 0xFF = 3. */
    uint32_t step = ((uint32_t)dist << 16) >> 3;
    /* +0x42/+0x46 aliases = the high words of field_40/44. */
    int32_t sx = (int16_t)(p->field_40 >> 16);
    int32_t sy = (int16_t)(p->field_44 >> 16);
    int32_t xdiff = (int16_t)(0x78 - sx);
    int32_t ydiff = (int16_t)(0x50 - sy);
    uint16_t angle = wo_atan2(xdiff, ydiff);
    wo_68cdc(p, angle, (int32_t)step);
    return (int32_t)step;               /* r4 = step, pre-clamp */
}

void aos_wo_burst_63aec(AosWo63AEC *pool, uint8_t *count,
                        uint8_t cap, int16_t x, int16_t y,
                        uint8_t parent, const AosWo63AECIn *in,
                        AosWo63AECBurstOut *out) {
    uint32_t rng = in->rng;
    memset(out, 0, sizeof *out);
    /* sl = 0..0x13 over the 20 stack records; r8 advances 8
     * bytes per record. A NULL sub_08062B60(1) skips the whole
     * record before any RNG draw. */
    for (unsigned i = 0; i < AOS_WO63AEC_MAX; ++i) {
        if ((*count >= cap) || (in->alloc_fail & (1u << i)))
            continue;                   /* _08063C18: next record */
        const AosWo63AECRec *rec = &wo_63aec_table_118b4c[i];
        AosWo63AEC *p = &pool[(*count)++];
        memset(p, 0, sizeof *p);
        /* sub_08062B60(1) prologue writes that survive: +0x58 =
         * (sub_0803E654 ret & 0x1F) | (~0x20 & 0), +0x59 |= 0x10,
         * +0x5C = 0x11 (overwritten below with the same value). */
        p->field_58 = in->flags58_init & 0x1F;
        p->field_59 = 0x10;
        /* update = sub_08063944 (implicit in the port). */
        p->field_11 = parent;           /* caller +0x10 */
        p->field_17 = (uint8_t)i;
        p->field_20 = x;                /* strh +0x20/+0x22 */
        p->field_22 = y;
        p->field_28 = rec->f2;          /* +0x28 = rec u8 @+2 */
        p->field_29 = (uint8_t)(rec->f3 >> 1); /* +0x29 */
        /* Two dead sub_080009E4(rec.f0 / rec.f0 + 0x4000) calls
         * precede the update store natively — pure, no effect. */
        /* +0x24 = rec.f0 - ((RNG & 0xFFF) + 0x280), u16 wrap. */
        p->field_24 = (uint16_t)(rec->f0 -
            (uint16_t)((wo_rng(&rng) & 0xFFFu) + 0x280u));
        p->field_38 = 0;
        p->field_26 = 1;
        /* +0x42/+0x46 = +0x20/+0x22 then += the trig offsets —
         * high-word writes on field_40/44 (low words stay 0). */
        int32_t ox = (wo_9e4((int32_t)p->field_24 + 0x4000) *
                      p->field_28) >> 13;
        int32_t oy = (wo_9e4((int32_t)p->field_24) *
                      p->field_29) >> 13;
        uint32_t hx = (uint16_t)p->field_20;
        uint32_t hy = (uint16_t)p->field_22;
        hx = (uint16_t)(hx + (uint16_t)ox);
        hy = (uint16_t)(hy + (uint16_t)oy);
        p->field_40 = (int32_t)((uint32_t)(uint16_t)hx << 16);
        p->field_44 = (int32_t)((uint32_t)(uint16_t)hy << 16);
        p->field_2a = 0;
        p->field_48 = wo_9e4((int32_t)p->field_24 + 0x4000) >> 8;
        p->field_4c = wo_9e4((int32_t)p->field_24) >> 8;
        p->field_1c = 0x40000;
        p->field_26 = (uint16_t)(rec->f4 << 1);
        /* +0xD = sl - 0x80 - sl/5 (C trunc division, sl >= 0). */
        p->field_d = (uint8_t)((int32_t)i - 0x80 - (int32_t)i / 5);
        p->field_5c = 0x11;
        /* +0x65 = RNG % 3 + 0xA — second and last draw. */
        p->field_65 = (uint8_t)((wo_rng(&rng) % 3) + 0xA);
        out->spawned++;
    }
    out->rng = rng;
}

void aos_wo_63aec_update(AosWo63AEC *p,
                         const AosWo63AECUpdateIn *in,
                         AosWo63AECUpdateOut *out) {
    memset(out, 0, sizeof *out);
    if (p->field_a == 0) {
        /* _08063968: two sub_080009E4 calls whose results the
         * native code never stores — pure, skipped. */
        /* +0x42 = +0x20 / +0x46 = +0x22 (high-word writes; the
         * low words of field_40/44 keep their accumulated
         * sub-offset). */
        uint32_t hx = (uint16_t)p->field_20;
        uint32_t hy = (uint16_t)p->field_22;
        /* +0x42 += (9E4(+0x24+0x4000) * +0x28) >> 13 — added to
         * the u16 high word with wrap. */
        int32_t ox = (wo_9e4((int32_t)p->field_24 + 0x4000) *
                      p->field_28) >> 13;
        int32_t oy = (wo_9e4((int32_t)p->field_24) *
                      p->field_29) >> 13;
        hx = (uint16_t)(hx + (uint16_t)ox);
        hy = (uint16_t)(hy + (uint16_t)oy);
        p->field_40 = (int32_t)(((uint32_t)p->field_40 & 0xFFFFu) |
                                ((uint32_t)(uint16_t)hx << 16));
        p->field_44 = (int32_t)(((uint32_t)p->field_44 & 0xFFFFu) |
                                ((uint32_t)(uint16_t)hy << 16));
        p->field_48 = wo_9e4((int32_t)p->field_24 + 0x4000) >> 8;
        p->field_4c = wo_9e4((int32_t)p->field_24) >> 8;
        /* +0x40 += +0x48 * (s16)+0x2A ; +0x44 likewise (s32 wrap
         * through int32_t like the native ldr/str). */
        p->field_40 += p->field_48 * (int32_t)(int16_t)p->field_2a;
        p->field_44 += p->field_4c * (int32_t)(int16_t)p->field_2a;
        /* +0xD = old + 0xFF (u8 -1); when the OLD byte is even
         * (bit0 clear) +0x2A++. */
        uint8_t old_d = p->field_d;
        p->field_d = (uint8_t)(old_d + 0xFF);
        if ((old_d & 1) == 0)
            p->field_2a = (int16_t)(p->field_2a + 1);
        p->field_24 = (uint16_t)(p->field_24 + 0x20);
        if (in->parent_flag3c_bit2) {
            /* The entityData[+0x11].unk_3C bit-1 branch:
             * damped velocity, +0xA = 1, +0xD =
             * +0x17 * (6 - (+0x17 >> 2)) (u8 product). */
            p->field_48 = wo_9e4((int32_t)p->field_24 + 0x4000) >> 2;
            p->field_4c = wo_9e4((int32_t)p->field_24) >> 2;
            p->field_a = 1;
            p->field_d = (uint8_t)((uint32_t)p->field_17 *
                                   (uint32_t)(6 - (p->field_17 >> 2)));
        }
    } else if (p->field_a == 1) {
        uint8_t nd = (uint8_t)(p->field_d - 1);
        p->field_d = nd;
        if (nd == 0xFF) {
            p->field_a = 2;             /* old +0xA + 1 */
            p->field_d = 0;
        } else {
            p->field_40 += p->field_48;
            p->field_44 += p->field_4c;
        }
    } else {
        /* +0xA >= 2: sub_0806E43C(entity, 0x780000, 0x500000, 3). */
        int32_t r = wo_6e43c(p);
        if (r != 0) {
            if (p->field_1c != 0)
                p->field_1c += (int32_t)0xFFFFF000; /* -0x1000 */
        } else {
            out->e6bc_call = true;      /* sub_0803E6BC(entity,
                                         * +0x58 & 1) */
            p->field_59 = (uint8_t)(p->field_59 | 8);
        }
    }
    out->affine_3e058 = true;           /* sub_0803E058 tail —
                                         * every call */
    out->done = (p->field_59 & 8) != 0;
}
