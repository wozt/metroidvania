/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_object20_dirs.h"
#include <string.h>

static void out_reset(AosObject20DirsOut *out) {
    memset(out, 0, sizeof *out);
    out->window_text = -1;
    out->song = -1;
    out->b878 = -1;
    out->cf0 = -1;
    out->release_arg = -1;
    out->jingle = -1;
    out->ed24_a = -1;
    out->c5a8 = -1;
    out->setplayerinput = -1;
    out->keep_arg = -1;
}

static void anim3(AosObject20DirsOut *out, int a, int b, int c) {
    if (out->anim_count >= 4) return;   /* bounded report list */
    unsigned i = out->anim_count++;
    out->anim_a[i] = a;
    out->anim_b[i] = b;
    out->anim_c[i] = c;
}

/* gEwramData + 0x417 byte write (aliases + 0x60 + 0x3B7); last
 * write wins, matching the final native state. */
static void w417(AosObject20DirsOut *out, uint8_t v) {
    out->write_417 = true;
    out->write_417_val = v;
}

static void w427(AosObject20DirsOut *out, uint8_t v) {
    out->write_427 = true;
    out->write_427_val = v;
}

static void w414(AosObject20DirsOut *out, uint8_t in414) {
    out->write_414 = true;
    out->write_414_val = (uint8_t)((in414 & ~0x10u) | 5u);
}

/* The ROM's facing-bit idiom: bit6 of the source flags58 is
 * extracted ((f << 0x19) >> 0x1F), then
 * (f & ~0x41) | ((1 - bit6) & 1) << 6 — an inverted mirror. */
static unsigned facing_bit6(uint8_t flags58) {
    return (flags58 & 0x40u) ? 1u : 0u;
}

/* ------------------------------------------------------------------
 * param0 4 — create sub_0805F120.
 * Gate: sub_08012078(0x21) set OR player +0x42 > 0x80 marks
 * +0x59 |= 8. Otherwise sub_0805B754(e,5), F2C8(0,0,1), the
 * 0xA0/0xC0 A094 placement with vel48 = 0xC000.
 * ------------------------------------------------------------------ */
static void create4(AosObject20Dirs *s, const AosObject20DirsIn *in,
                    AosObject20DirsOut *out) {
    /* _0805F13C: flag12078(0x21) set OR player +0x42 > 0x80
     * (cmp #0x80, ble survives). */
    if (in->flag12078_21 != 0 || in->player_scr42 > 0x80) {
        s->flags59 |= 0x8;
        return;
    }
    if (!in->keep_5b754) {
        out->keep_arg = 5;
        s->deleted = true;      /* EntityDelete */
        return;
    }
    out->keep_arg = 5;
    anim3(out, 0, 0, 1);                    /* sub_0803F2C8(e,0,0,1) */
    out->anim_step_count++;                 /* sub_0803F17C */
    s->scr42 = (int16_t)(0xA0 - in->a094_6);
    s->scr46 = (int16_t)(0xC0 - in->a094_a);
    s->vel48 = 0xC000;                      /* 0xC0 << 8 */
    s->flags58 &= ~0x41u;
    out->set_42c |= 1;
}

/* ------------------------------------------------------------------
 * param0 4 — update sub_0805F268.
 * States: 0 opens window 0xF after a 0x20-frame delay; 1 polls
 * then runs the +0xB 0-2 sub-state script; 0x80 releases.
 * Returns r8 (0x10 from the +0xB==0,+0xC==2 late branch).
 * ------------------------------------------------------------------ */
static int update4(AosObject20Dirs *s, const AosObject20DirsIn *in,
                   AosObject20DirsOut *out) {
    int r8 = 0;
    int32_t px = in->player_scr42;  /* player +0x42, local mirror */
    uint8_t st = s->state;
    if (st == 0) {
        if (s->sub_b == 0) {
            out->b878 = 0x1040;             /* sub_0805B878(0x82 << 5) */
            out->window_text = 0xF;         /* sub_08067934(0xF) */
            w417(out, 1);
            s->timer_d = 0x20;
            s->sub_b = 1;
        }
        s->timer_d = (uint8_t)(s->timer_d - 1);
        if (s->timer_d == 0xFF) {
            s->state = (uint8_t)(s->state + 1);
            s->sub_c = 0;
            s->sub_b = 0;
        }
    } else if (st == 1) {
        out->window_poll = true;            /* sub_08067A04 */
        if (in->text_done)
            s->state = 0x80;
        switch (s->sub_b) {
        case 0:
            switch (s->sub_c) {
            case 0:
                s->sub_c = 1;
                s->timer_d = 0x30;
                /* ROM _0805F31C -> _0805F322. */
                /* fall through */
            case 1:
                s->timer_d = (uint8_t)(s->timer_d - 1);
                if (s->timer_d != 0xFF) break;
                s->sub_c = (uint8_t)(s->sub_c + 1);   /* _0805F438 */
                break;
            case 2:
                if (px > 0x56) {
                    out->player58_andnot_41 = true;
                    px = 0x57;
                    out->player_scr42_set = true;
                    out->player_scr42_val = px;
                    s->sub_c = (uint8_t)(s->sub_c + 1); /* _0805F438 */
                } else {
                    r8 = 0x10;
                }
                break;
            case 3:
                out->cf0 = 0x16;            /* sub_08013CF0(0x16) */
                w417(out, (uint8_t)r8);
                s->sub_b = 1;               /* r5 = state == 1 */
                s->sub_c = 0;
                break;
            default:
                break;
            }
            break;
        case 1:
            switch (s->sub_c) {
            case 0:
                s->index14 = (uint8_t)r8;   /* +0x14 = r8 (0) */
                s->timer_d = 0xFF;
                s->sub_c = (uint8_t)(s->sub_c + 1);
                break;
            case 1:
                s->timer_d = (uint8_t)(s->timer_d - 1);
                if (s->timer_d != 0xFF) break;
                {
                    /* _0805F39C: walk the 0x0852682C byte table —
                     * index14 increments first, the OLD index
                     * picks the anim arg; 0xFF resets to 0. */
                    unsigned idx = s->index14;
                    s->index14 = (uint8_t)(idx + 1);
                    uint8_t v = 0xFF;
                    if (in->table_852682c)
                        v = in->table_852682c[idx];
                    if (v == 0xFF) {
                        v = 0;
                        s->index14 = 0;
                    }
                    anim3(out, v, 0, 0);    /* sub_0803F2C8(e,v,0,0) */
                    s->timer_d = 0xFF;      /* |= 0xFF */
                }
                break;
            default:
                break;
            }
            break;
        case 2:
            switch (s->sub_c) {
            case 0:
                out->b878 = 0x1040;
                w417(out, 1);               /* r5 = state == 1 */
                s->flags58 |= 0x40;
                if (s->anim6d != 1)
                    anim3(out, 1, 0, 1);    /* sub_0803F2C8(e,1,0,1) */
                s->sub_c = (uint8_t)(s->sub_c + 1);
                /* ROM _0805F422 -> _0805F428. */
                /* fall through */
            case 1:
                if ((int16_t)s->scr42 <= 0xFF) {
                    s->pos40 += s->vel48;
                } else {
                    s->timer_d = 0x10;
                    s->sub_c = (uint8_t)(s->sub_c + 1);
                }
                break;
            case 2:
                s->timer_d = (uint8_t)(s->timer_d - 1);
                if (s->timer_d != 0xFF) break;
                w417(out, (uint8_t)r8);
                w414(out, in->byte414);
                s->sub_c = (uint8_t)(s->sub_c + 1);
                s->seq18 = 4;               /* +0x18 = 4 */
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
    } else if (st == 0x80) {
        out->call_13960 = true;             /* sub_08013960(+0x60) */
        out->jingle = 0x21;                 /* sub_08012048(0x21) */
        out->release_arg = 1;               /* sub_0805B66C(e,1) */
    }
    if (s->state != 0x80)
        out->anim_step_count++;             /* sub_0803F17C tail */
    out->return_code = r8;
    return r8;
}

/* ------------------------------------------------------------------
 * param0 5 — create sub_0805F4B9.
 * Gate: sub_08012078(0x22) set releases through sub_0805B66C(1);
 * otherwise sub_0805B754(e,2) and the 0xA0/0xA0 placement.
 * ------------------------------------------------------------------ */
static void create5(AosObject20Dirs *s, const AosObject20DirsIn *in,
                    AosObject20DirsOut *out) {
    if (in->flag12078_22 != 0) {
        out->release_arg = 1;              /* sub_0805B66C(e,1) */
        return;
    }
    if (!in->keep_5b754) {
        out->keep_arg = 2;
        s->deleted = true;
        return;
    }
    out->keep_arg = 2;
    anim3(out, 0, 0, 1);
    out->anim_step_count++;
    s->scr42 = (int16_t)(0xA0 - in->a094_6);
    s->scr46 = (int16_t)(0xA0 - in->a094_a);
    s->flags58 &= ~0x41u;
    out->set_42c |= 1;
}

/* ------------------------------------------------------------------
 * param0 5 — update sub_0805F541.
 * Same skeleton as param0 4 with window 0x10, different clamp
 * constants and no +0xB/+0xC reset on the +0xB==0,+0xC==3 exit.
 * Returns sb (0x10 late branch, 0x20 never set here).
 * ------------------------------------------------------------------ */
static int update5(AosObject20Dirs *s, const AosObject20DirsIn *in,
                   AosObject20DirsOut *out) {
    int sb = 0;
    int32_t px = in->player_scr42;
    uint8_t st = s->state;
    if (st == 0) {
        if (s->sub_b == 0) {
            out->b878 = 0x1040;
            out->window_text = 0x10;
            w417(out, 1);
            s->timer_d = 0x20;
            s->sub_b = 1;
        }
        s->timer_d = (uint8_t)(s->timer_d - 1);
        if (s->timer_d == 0xFF) {
            s->state = (uint8_t)(s->state + 1);
            s->sub_c = 0;
            s->sub_b = 0;
        }
    } else if (st == 1) {
        out->window_poll = true;
        if (in->text_done)
            s->state = 0x80;
        switch (s->sub_b) {
        case 0:
            switch (s->sub_c) {
            case 0:
                s->sub_c = 1;
                s->timer_d = 0x30;
                /* ROM _0805F5EE -> _0805F5F4. */
                /* fall through */
            case 1:
                s->timer_d = (uint8_t)(s->timer_d - 1);
                if (s->timer_d != 0xFF) break;
                s->sub_c = (uint8_t)(s->sub_c + 1);   /* _0805F6EE */
                break;
            case 2:
                if (px > 0x4F) {
                    px = 0x50;
                    out->player_scr42_set = true;
                    out->player_scr42_val = px;
                    s->sub_c = (uint8_t)(s->sub_c + 1); /* _0805F6EE */
                } else {
                    sb = 0x10;
                }
                break;
            case 3:
                out->cf0 = 0x16;
                w417(out, (uint8_t)sb);
                /* _0805F62E: NO +0xB/+0xC reset — the phase
                 * counters stay for the external driver. */
                break;
            default:
                break;
            }
            break;
        case 1:
            switch (s->sub_c) {
            case 0:
                out->b878 = 0x1040;
                w417(out, 1);               /* r6 = sub_b == 1 */
                s->vel48 = 0x8000;          /* 0x80 << 8 */
                /* facing mirror sourced from the PLAYER's
                 * +0x58 bit 6: (own & ~0x41) | ((1-bit6)<<6). */
                s->flags58 = (uint8_t)((s->flags58 & ~0x41u) |
                    ((((1u - facing_bit6(in->player_flags58)) & 1u)
                      << 6)));
                if (s->anim6d != 1)
                    anim3(out, 1, 0, 1);
                s->sub_c = (uint8_t)(s->sub_c + 1);
                /* ROM _0805F6B0 -> _0805F6B6. */
                /* fall through */
            case 1:
                if ((int16_t)s->scr42 > 0xFF) {
                    s->sub_c = (uint8_t)(s->sub_c + 1); /* _0805F6EE */
                } else {
                    s->pos40 += s->vel48;
                }
                break;
            case 2:
                w417(out, (uint8_t)sb);
                w414(out, in->byte414);
                s->sub_c = (uint8_t)(s->sub_c + 1);     /* _0805F6EE */
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
    } else if (st == 0x80) {
        out->call_13960 = true;
        out->jingle = 0x22;
        out->release_arg = 1;
    }
    if (s->state != 0x80)
        out->anim_step_count++;
    out->return_code = sb;
    return sb;
}

/* ------------------------------------------------------------------
 * param0 6 — create sub_0805F72D.
 * Gate: sub_08012078(0x23) set marks +0x59 |= 8; otherwise
 * sub_0805B754(e,6), the 0x50/0xC0 placement, +0x58 |= 0x40,
 * +0x42C |= 1 and gEwramData + 0x7864 |= 2.
 * ------------------------------------------------------------------ */
static void create6(AosObject20Dirs *s, const AosObject20DirsIn *in,
                    AosObject20DirsOut *out) {
    if (in->flag12078_23 != 0) {
        s->flags59 |= 0x8;
        return;
    }
    if (!in->keep_5b754) {
        out->keep_arg = 6;
        s->deleted = true;
        return;
    }
    out->keep_arg = 6;
    anim3(out, 0, 0, 1);
    out->anim_step_count++;
    s->scr42 = (int16_t)(0x50 - in->a094_6);
    s->scr46 = (int16_t)(0xC0 - in->a094_a);
    s->flags58 |= 0x40;
    out->set_42c |= 1;
    out->unk7864_or2 = true;
}

/* ------------------------------------------------------------------
 * param0 6 — update sub_0805F7C9.
 * States: 0 DMA setup + window 0x11 (falls into state 1 the same
 * call); 1 polls then runs the +0xB 0-1 escort script; 0x80
 * releases. Returns r8 (0x20 from the scroll-clamp branch).
 * ------------------------------------------------------------------ */
static int update6(AosObject20Dirs *s, const AosObject20DirsIn *in,
                   AosObject20DirsOut *out) {
    int r8 = 0;
    int32_t px = in->player_scr42;
    /* +0xA094 +4 scroll word — read twice per frame by the ROM;
     * modelled as a local so the conditional zero write is
     * visible to the second read (sub_0800ED24's effect on the
     * word is approximated as nil). */
    int32_t a4 = in->a094_4;
    uint8_t st = s->state;
    if (st == 0) {
        /* approximated: the 0x040000D4 -> 0x06004000 /
         * 0x85000400 queue writes are reported as dma_setup. */
        out->dma_setup = true;
        out->b878 = 0x1040;
        out->window_text = 0x11;
        w417(out, 1);
        s->state = (uint8_t)(s->state + 1);
        st = 1;                             /* ROM fallthrough */
    }
    if (st == 1) {
        out->window_poll = true;
        if (in->text_done)
            s->state = 0x80;
        switch (s->sub_b) {
        case 0:
            switch (s->sub_c) {
            case 0:
                w417(out, 1);               /* +0x417 = 1 */
                s->sub_c = (uint8_t)(s->sub_c + 1);
                w427(out, 1);               /* +0x427 = 1 */
                /* ROM _0805F86E -> _0805F886 -> _0805F88C. */
                /* fall through */
            case 1:
                s->sub_c = (uint8_t)(s->sub_c + 1);
                /* _0805F88C: every remaining +0xC path runs the
                 * scroll/escort block. */
                if (a4 > 0) {
                    out->ed24_a = 1;        /* sub_0800ED24(1,..,0) */
                    out->ed24_b = (int32_t)0xFFFE8000;
                } else {
                    a4 = 0;
                    out->write_a094_4 = true;
                    out->write_a094_4_val = 0;
                }
                {
                    int32_t lim = 0xAE - (int32_t)(int16_t)in->a094_6;
                    if (px > lim) {
                        r8 |= 0x20;
                    } else {
                        px = lim;
                        out->player_scr42_set = true;
                        out->player_scr42_val = px;
                    }
                }
                if (a4 == 0 && px == 0xAE - (int32_t)(int16_t)in->a094_6) {
                    out->cf0 = 0x16;        /* sub_08013CF0(0x16) */
                    w417(out, (uint8_t)a4); /* +0x417 = a4 (0) */
                    s->sub_c = (uint8_t)(s->sub_c + 1); /* _0805F9FA */
                }
                break;
            case 2:
                /* _0805F88C directly — same block, no step. */
                if (a4 > 0) {
                    out->ed24_a = 1;
                    out->ed24_b = (int32_t)0xFFFE8000;
                } else {
                    a4 = 0;
                    out->write_a094_4 = true;
                    out->write_a094_4_val = 0;
                }
                {
                    int32_t lim = 0xAE - (int32_t)(int16_t)in->a094_6;
                    if (px > lim) {
                        r8 |= 0x20;
                    } else {
                        px = lim;
                        out->player_scr42_set = true;
                        out->player_scr42_val = px;
                    }
                }
                if (a4 == 0 && px == 0xAE - (int32_t)(int16_t)in->a094_6) {
                    out->cf0 = 0x16;
                    w417(out, (uint8_t)a4);
                    s->sub_c = (uint8_t)(s->sub_c + 1);
                }
                break;
            default:
                break;
            }
            break;
        case 1:
            switch (s->sub_c) {
            case 0:
                out->b878 = 0x1040;
                w417(out, 1);               /* r4 = sub_b == 1 */
                s->flags58 &= ~0x41u;
                if (s->anim6d != 1)
                    anim3(out, 1, 0, 1);
                s->sub_c = (uint8_t)(s->sub_c + 1);
                /* ROM _0805F982 -> _0805F988. */
                /* fall through */
            case 1: {
                int16_t spx = s->scr42;
                if (spx > -0x10) {
                    out->call_6e140 = true; /* sub_0806E140(e,1,
                                             * 0x10000,0x200000,
                                             * 0x20000,stk 1) */
                    if ((in->probe_6e140 & 4) == 0)
                        s->pos44 += s->vel4c;
                }
                /* _0805F9BE tail — every +0xC==1 path reaches it. */
                out->ed24_a = 1;
                out->ed24_b = 0x10000;
                if (px == 0x78) {
                    w417(out, 0);
                    w414(out, in->byte414);
                    s->sub_c = (uint8_t)(s->sub_c + 1); /* _0805F9FA */
                }
                break;
            }
            default:
                break;
            }
            break;
        default:
            break;
        }
    } else if (st == 0x80) {
        out->call_13960 = true;
        w427(out, (uint8_t)r8);
        out->jingle = 0x23;
        out->release_arg = 1;
    }
    if (s->state != 0x80)
        out->anim_step_count++;
    out->return_code = r8;
    return r8;
}

/* ------------------------------------------------------------------
 * param0 7 — create sub_0805FA4D.
 * Gate: sub_08012078(0x24) set OR player +0x42 <= 0x1F marks
 * +0x59 |= 8. Otherwise +0x7864 |= 2, the +0x786C DMA variant,
 * sub_0805B754(e,5), the 0xBE/0xA0 placement, +0x58 |= 0x40,
 * sub_0806AF98 and player +0x58 |= 0x40.
 * ------------------------------------------------------------------ */
static void create7(AosObject20Dirs *s, const AosObject20DirsIn *in,
                    AosObject20DirsOut *out) {
    if (in->flag12078_24 != 0 || in->player_scr42 <= 0x1F) {
        s->flags59 |= 0x8;
        return;
    }
    out->unk7864_or2 = true;
    /* approximated: the 0x040000D4 -> +0x786C / 0x85000280
     * queue writes are reported as dma_setup_786c. */
    out->dma_setup_786c = true;
    if (!in->keep_5b754) {
        out->keep_arg = 5;
        s->deleted = true;
        return;
    }
    out->keep_arg = 5;
    anim3(out, 0, 0, 1);
    out->anim_step_count++;
    s->scr42 = (int16_t)(0xBE - in->a094_6);
    s->scr46 = (int16_t)(0xA0 - in->a094_a);
    /* The ROM reaches +0x58 through e+0x46 + 0x12 and ORs 0x40 —
     * the flags12 field is unused by these variants. */
    s->flags58 |= 0x40;
    out->call_6af98 = true;                /* sub_0806AF98(e) */
    out->player58_or_40 = true;            /* player +0x58 |= 0x40 */
    out->set_42c |= 1;
}

/* ------------------------------------------------------------------
 * param0 7 — update sub_0805FB3D.
 * States: 0 freezes the player velocity pair and opens window
 * 0x12 (falls into state 1); 1 polls then runs the +0xB 0-2
 * script; 0x80 releases. Always returns 0.
 * ------------------------------------------------------------------ */
static int update7(AosObject20Dirs *s, const AosObject20DirsIn *in,
                   AosObject20DirsOut *out) {
    int r7 = 0;
    int32_t px = in->player_scr42;
    uint8_t st = s->state;
    if (st == 0) {
        out->player_vel_cleared = true;     /* player +0x48/+0x4C = 0 */
        out->b878 = 0x1040;
        out->window_text = 0x12;
        w417(out, 1);
        s->state = (uint8_t)(s->state + 1);
        st = 1;                             /* ROM fallthrough */
    }
    if (st == 1) {
        out->window_poll = true;
        if (in->text_done)
            s->state = 0x80;
        switch (s->sub_b) {
        case 0:
            if (s->sub_c == 0) {
                w417(out, 1);
                s->sub_c = (uint8_t)(s->sub_c + 1);
                s->sub_c = (uint8_t)(s->sub_c + 1);   /* _0805FBE0 */
            } else if (s->sub_c == 1) {
                s->sub_c = (uint8_t)(s->sub_c + 1);
            } else if (s->sub_c != 2) {
                break;
            }
            /* _0805FBE6: scroll gate + escape, runs for the
             * +0xC==0/1 entry paths and every +0xC==2 frame. */
            w427(out, 1);
            if ((int16_t)in->a094_6 > 0x94) {
                out->ed24_a = 1;
                out->ed24_b = (int32_t)0xFFFF8000;
            } else {
                out->cf0 = 0x16;
                w417(out, 0);
                s->sub_c = (uint8_t)(s->sub_c + 1);   /* _0805FD16 */
            }
            break;
        case 1:
            switch (s->sub_c) {
            case 0:
                w417(out, 1);               /* r1 = sub_b == 1 */
                /* facing invert sourced from OWN +0x58 bit 6. */
                s->flags58 = (uint8_t)((s->flags58 & ~0x41u) |
                    ((((1u - facing_bit6(s->flags58)) & 1u) << 6)));
                if (s->anim6d != 8)
                    anim3(out, 8, 0, 1);
                s->sub_c = (uint8_t)(s->sub_c + 1);
                /* ROM _0805FC7A -> _0805FC80. */
                /* fall through */
            case 1: {
                int16_t spx = s->scr42;
                if (spx <= -0x10) {
                    s->sub_c = (uint8_t)(s->sub_c + 1); /* _0805FCBC */
                    w417(out, 0);
                } else {
                    out->call_6e140 = true;
                    if ((in->probe_6e140 & 4) == 0)
                        s->pos44 += s->vel4c;
                }
                break;
            }
            default:
                break;
            }
            break;
        case 2:
            w417(out, 1);
            out->ed24_a = 1;
            out->ed24_b = 0x8000;               /* 0x80 << 8 */
            out->b878 = 0x1040;
            if (px == 0x78) {
                w417(out, 0);
                w414(out, in->byte414);
                s->sub_c = (uint8_t)(s->sub_c + 1); /* _0805FD16 */
            }
            break;
        default:
            break;
        }
    } else if (st == 0x80) {
        w427(out, 0);
        out->call_13960 = true;
        out->jingle = 0x24;
        out->release_arg = 1;
    }
    if (s->state != 0x80)
        out->anim_step_count++;
    out->return_code = r7;
    return r7;
}

/* ------------------------------------------------------------------
 * param0 8 — create sub_0805FD65.
 * Gate: sub_08012078(0x25) set marks +0x59 |= 8; otherwise
 * sub_0805B754(e,2), the 0x50/0xA0 placement, +0x58 |= 0x40,
 * +0x42C |= 1.
 * ------------------------------------------------------------------ */
static void create8(AosObject20Dirs *s, const AosObject20DirsIn *in,
                    AosObject20DirsOut *out) {
    if (in->flag12078_25 != 0) {
        s->flags59 |= 0x8;
        return;
    }
    if (!in->keep_5b754) {
        out->keep_arg = 2;
        s->deleted = true;
        return;
    }
    out->keep_arg = 2;
    anim3(out, 0, 0, 1);
    out->anim_step_count++;
    s->scr42 = (int16_t)(0x50 - in->a094_6);
    s->scr46 = (int16_t)(0xA0 - in->a094_a);
    s->flags58 |= 0x40;
    out->set_42c |= 1;
}

/* ------------------------------------------------------------------
 * param0 8 — update sub_0805FDF1.
 * States: 0 window 0x13 after a 0x20-frame delay; 1 polls then
 * runs the +0xB 0-2 script; 0x80 releases WITHOUT the
 * sub_0803F17C tail. Returns sb (0x20/0x10-class codes).
 * ------------------------------------------------------------------ */
static int update8(AosObject20Dirs *s, const AosObject20DirsIn *in,
                   AosObject20DirsOut *out) {
    int sb = 0;
    int32_t px = in->player_scr42;
    uint8_t st = s->state;
    if (st == 0) {
        if (s->sub_b == 0) {
            out->b878 = 0x1040;
            out->window_text = 0x13;
            w417(out, 1);
            s->timer_d = 0x20;
            s->sub_b = 1;
        }
        s->timer_d = (uint8_t)(s->timer_d - 1);
        if (s->timer_d == 0xFF) {
            s->state = (uint8_t)(s->state + 1);
            s->sub_c = 0;
            s->sub_b = 0;
        }
    } else if (st == 1) {
        out->window_poll = true;
        if (in->text_done)
            s->state = 0x80;
        switch (s->sub_b) {
        case 0:
            switch (s->sub_c) {
            case 0:
                w417(out, 1);               /* r5 = sub_b == 1 */
                s->sub_c = (uint8_t)(s->sub_c + 1);
                s->timer_d = 0x30;
                /* ROM _0805FEA4 -> _0805FEB4. */
                /* fall through */
            case 1:
                s->timer_d = (uint8_t)(s->timer_d - 1);
                if (s->timer_d != 0xFF) break;
                s->sub_c = (uint8_t)(s->sub_c + 1);   /* _0805FF92 */
                break;
            case 2:
                if (px <= 0xA0) {
                    out->cf0 = 0x16;
                    w417(out, (uint8_t)sb);
                    px = 0xA0;
                    out->player_scr42_set = true;
                    out->player_scr42_val = px;
                    s->sub_c = (uint8_t)(s->sub_c + 1); /* _0805FF92 */
                } else {
                    sb = 0x20;
                }
                break;
            default:
                break;
            }
            break;
        case 1:
            if (s->sub_c == 0) {
                out->song = 0x1B7;              /* PlaySong(0x1B7) */
                s->sub_c = (uint8_t)(s->sub_c + 1); /* _0805FF92 */
            }
            break;
        case 2:
            switch (s->sub_c) {
            case 0:
                out->b878 = 0x1040;
                w417(out, 1);               /* r5 = sub_b == 1 */
                s->vel48 = (int32_t)0xFFFF8000;    /* -0x8000 */
                s->flags58 &= ~0x41u;
                if (s->anim6d != 1)
                    anim3(out, 1, 0, 1);
                s->sub_c = (uint8_t)(s->sub_c + 1);
                /* ROM _0805FF62 -> _0805FF68. */
                /* fall through */
            case 1:
                if ((int16_t)s->scr42 >= -8) {
                    s->pos40 += s->vel48;
                } else {
                    w417(out, 0);
                    w414(out, in->byte414);
                    s->sub_c = (uint8_t)(s->sub_c + 1); /* _0805FF92 */
                }
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
    } else if (st == 0x80) {
        out->call_13960 = true;
        out->jingle = 0x25;
        out->release_arg = 1;
        out->return_code = sb;
        return sb;                          /* skips sub_0803F17C */
    }
    out->anim_step_count++;
    out->return_code = sb;
    return sb;
}

/* ------------------------------------------------------------------
 * sub_0805F1CC — the +0x18 sequencer between the param0-4 create
 * and update in the ROM. Dispatches on the +0x18 byte:
 * 0/3 PlaySong(0x1010) + 0xA0 timer, 1/4 countdown to
 * sub_08013CF0(0x16)/sub_08013960, 5 returns 1.
 * ------------------------------------------------------------------ */
int aos_object20_dirs_seq18_update(AosObject20Dirs *s,
                                   const AosObject20DirsIn *in,
                                   AosObject20DirsOut *out) {
    (void)in;
    out_reset(out);
    int r5 = 0;
    uint8_t st = s->seq18;
    if (st > 5) {               /* ROM jump-table guard: >5 exits */
        out->return_code = 0;
        return 0;
    }
    switch (st) {
    case 0:
        out->song = 0x1010;                     /* PlaySong(0x1010) */
        s->timer19 = 0xA0;
        s->seq18 = (uint8_t)(s->seq18 + 1);
        break;
    case 1:
        s->timer19 = (uint8_t)(s->timer19 - 1);
        if (s->timer19 == 0xFF) {
            out->cf0 = 0x16;
            s->seq18 = (uint8_t)(s->seq18 + 1);
        }
        break;
    case 2:
        break;
    case 3:
        out->song = 0x1010;
        s->timer19 = 0xA0;
        s->seq18 = (uint8_t)(s->seq18 + 1);
        break;
    case 4:
        s->timer19 = (uint8_t)(s->timer19 - 1);
        if (s->timer19 == 0xFF) {
            out->call_13960 = true;
            s->seq18 = (uint8_t)(s->seq18 + 1);
        }
        break;
    case 5:
        r5 = 1;
        break;
    }
    out->return_code = r5;
    return r5;
}

/* ------------------------------------------------------------------
 * Dispatchers.
 * ------------------------------------------------------------------ */
bool aos_object20_dirs_create(AosObject20Dirs *s, uint8_t param0,
                              const AosObject20DirsIn *in,
                              AosObject20DirsOut *out) {
    out_reset(out);
    s->deleted = false;
    /* Shared Object20Create gates (0x0805B42C). */
    bool gate_pass;
    if (in->flag1327a > 0) {
        gate_pass = true;
    } else {
        gate_pass = (in->flag42c & 0x200000u) != 0;  /* 0x80 << 0xE */
    }
    if (gate_pass) {
        if (in->byte8e & 0x80) {
            gate_pass = false;
        } else if (in->byte13266 != 0 &&
                   (in->flag42c & 0x8000u) == 0) {
            gate_pass = false;
        }
    }
    if (gate_pass) {
        switch (param0) {
        case 4:  create4(s, in, out); break;
        case 5:  create5(s, in, out); break;
        case 6:  create6(s, in, out); break;
        case 7:  create7(s, in, out); break;
        case 8:  create8(s, in, out); break;
        default:
            /* The real 0x08526658 entries for other param0 values
             * belong to sibling variant modules; nothing ran. */
            out->unported = true;
            break;
        }
        if (s->flags59 & 0x8)
            s->deleted = true;
    } else {
        s->deleted = true;
    }
    if (s->deleted)
        return false;
    /* Shared tail: +0x42C |= 0x200, engine init calls,
     * gEwramData + 0x41C = entity, conditional
     * SetPlayerInput(1, 0), sub_0806666C. The ROM re-reads the
     * flag word after the variant ran, so the bit-0 test sees
     * the OR-ed value. */
    out->set_42c |= 0x200;
    out->call_0eb04 = true;
    out->call_235c8 = true;
    out->call_2dff0 = true;
    out->c5a8 = 0;
    out->call_6666c = true;
    if ((in->flag42c | out->set_42c) & 1) {
        out->setplayerinput = 1;
        out->setplayerinput_arg = 0;
    }
    return true;
}

int aos_object20_dirs_update(AosObject20Dirs *s, uint8_t param0,
                             const AosObject20DirsIn *in,
                             AosObject20DirsOut *out) {
    out_reset(out);
    /* Shared Object20Update prologue (0x0805B510). */
    if (in->flag42c & 0x80)
        out->unk7864_or2 = true;
    if ((int8_t)s->state < 0) {            /* ldrsb +0xA < 0 */
        if (in->byte41b != 0)
            out->call_67804 = true;         /* sub_08067804(-3) */
    } else {
        if ((in->word16 & 8) && (in->word60 & 2) &&
            !(in->flag42c & 0x10000)) {
            /* +0x414 = (byte & ~0x10) | 5 — a variant's later
             * w414 reports the same idempotent transform. */
            w414(out, in->byte414);
        }
    }
    int ret = 0;
    switch (param0) {
    case 4:  ret = update4(s, in, out); break;
    case 5:  ret = update5(s, in, out); break;
    case 6:  ret = update6(s, in, out); break;
    case 7:  ret = update7(s, in, out); break;
    case 8:  ret = update8(s, in, out); break;
    default:
        out->unported = true;
        break;
    }
    /* Tail: +0x42C bit 0 selects SetPlayerInput(1, ret) +
     * sub_0800C5A8(0), else sub_0800C5A8(1). */
    if (in->flag42c & 1) {
        out->setplayerinput = 1;
        out->setplayerinput_arg = ret;
        out->c5a8 = 0;
    } else {
        out->c5a8 = 1;
    }
    return ret;
}
