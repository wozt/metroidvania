/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_object20_slots.h"
#include <string.h>

/* Object20 slot-variant parity port — see aos_object20_slots.h
 * for the param0 -> native routine map. Native sources:
 * third_party/cvaos/asm/code/code_08050A3C.s (sub_0805BCB4,
 * sub_0805BE64, sub_0805BF24, sub_0805BF48, sub_0805CB6C,
 * sub_0805CB70 and the shared Object20Create/Update shells)
 * and code_08060B98.s (sub_08060B98..sub_08061994). */

/* The ten-word table sub_08060F98 (param0-15 update15) memcpy's
 * from ROM 0x081188E4 onto its stack (asm/code_08060B98.s
 * _08060FE4): cases 5/6 index it with the entity +0x14 byte
 * (valid 0..9) as the hit-test divisor — tbl14 raw for case 6,
 * tbl14*3/2 for case 5. Values decoded from the ROM image;
 * same committed-table convention as
 * aos_guard_pick_table_118c85. */
const int32_t aos_object20_slots_table_1188e4[10] = {
    0x80, 0x80, 0x20, 0x20, 0x40, 0x40,
    0x80, 0x60, 0x40, 0x30,
};

static void out_reset(AosObject20SlotsOut *out) {
    memset(out, 0, sizeof *out);
    out->window_text = -1;
    out->b878 = -1;
    out->jingle = -1;
    out->release_arg = -1;
    out->rec_5b754 = -1;
    out->setplayerinput = -1;
    out->c5a8 = -1;
    out->sfx_13eec = -1;
    out->call_13cf0 = -1;
    out->call_00ea8 = -1;
    for (unsigned i = 0; i < 4; ++i)
        out->anim_a[i] = -1;
}

static void anim3(AosObject20SlotsOut *out, int a, int b, int c) {
    if (out->anim_count >= 4) return;   /* bounded report list */
    unsigned i = out->anim_count++;
    out->anim_a[i] = a;
    out->anim_b[i] = b;
    out->anim_c[i] = c;
}

/* param0 15 update helpers: bounded report lists mirroring the
 * native call order. */
static void playsong_report(AosObject20SlotsOut *out, int arg) {
    if (out->playsong_count >= 4) return;
    out->playsong[out->playsong_count++] = arg;
}

static void fade_report(AosObject20SlotsOut *out, int fn,
                        int a0, int a1, int a2, int a3,
                        int a4, int a5) {
    if (out->fade_count >= 8) return;
    unsigned i = out->fade_count++;
    out->fade_fn[i] = fn;
    out->fade_args[i][0] = a0;
    out->fade_args[i][1] = a1;
    out->fade_args[i][2] = a2;
    out->fade_args[i][3] = a3;
    out->fade_args[i][4] = a4;
    out->fade_args[i][5] = a5;
}

static void player5a_store(AosObject20SlotsOut *out, uint8_t val) {
    if (out->player5a_count >= 4) return;
    out->player5a_vals[out->player5a_count++] = val;
}

/* __modsi3: signed modulo (result takes the dividend's sign,
 * matching C %). The ROM would crash on a zero divisor; modelled
 * as a nonzero result so the fade block simply does not fire —
 * documented approximation, the table words are nonzero in
 * practice. */
static int32_t slots_modsi3(int32_t a, int32_t b) {
    if (b == 0)
        return 1;               /* always nonzero: no fire */
    return a % b;
}

/* sub_0805B754(e, idx) over the 0x081186DC record table: on a
 * live sub_0806E0D0 probe it writes +0x5C = 0x11 and +0x59 |= 2
 * and returns 1; on a dead probe it EntityDeletes and returns
 * 0. `keep_5b754` is the injected probe result; the record
 * index is reported through out->rec_5b754. Returns the ROM
 * result. */
static int helper5b754(AosObject20Slots *s, int idx,
                       const AosObject20SlotsIn *in,
                       AosObject20SlotsOut *out) {
    out->rec_5b754 = idx;
    if (in->keep_5b754) {
        s->anim5c = 0x11;               /* +0x5C = 0x11 */
        s->flags59 |= 0x2;              /* +0x59 |= 2 */
        return 1;
    }
    s->deleted = true;                  /* EntityDelete */
    return 0;
}

/* ------------------------------------------------------------------
 * param0 0 — create sub_0805BF24.
 * +0x42C |= 1, sub_08067934(0xC). No entity state touched.
 * ------------------------------------------------------------------ */
static void create0(AosObject20Slots *s, const AosObject20SlotsIn *in,
                    AosObject20SlotsOut *out) {
    (void)s; (void)in;
    out->set_42c |= 1;
    out->window_text = 0xC;                /* sub_08067934(0xC) */
}

/* param0 1 — create sub_0805BF48: bx lr (empty variant; the
 * shared gates/tail still run). */
static void create1(AosObject20Slots *s, const AosObject20SlotsIn *in,
                    AosObject20SlotsOut *out) {
    (void)s; (void)in; (void)out;
}

/* param0 2 — create sub_0805CB6C: bx lr (empty variant). */
static void create2(AosObject20Slots *s, const AosObject20SlotsIn *in,
                    AosObject20SlotsOut *out) {
    (void)s; (void)in; (void)out;
}

/* ------------------------------------------------------------------
 * param0 11 — create sub_08060B98 (record-4 sprite spawn).
 * +0x42C bit 0x10000 set marks +0x59 |= 8 (the shared tail
 * deletes); otherwise sub_0805B754(e,4) gates the spawn — a
 * dead probe EntityDeletes. Success: F2C8(e,0,0,1), F17C,
 * +0x58 = (+0x58 & ~0x41) | (player +0x58 & 0x40), +0x42C |= 1.
 * ------------------------------------------------------------------ */
static void create11(AosObject20Slots *s, const AosObject20SlotsIn *in,
                     AosObject20SlotsOut *out) {
    if (in->flag42c & 0x10000u) {        /* 0x80 << 9 */
        s->flags59 |= 0x8;
        return;
    }
    if (helper5b754(s, 4, in, out) == 0) {
        /* The ROM calls EntityDelete again here — already
         * reported through `deleted`. */
        return;
    }
    anim3(out, 0, 0, 1);                /* sub_0803F2C8(e,0,0,1) */
    out->anim_step_count++;             /* sub_0803F17C */
    /* +0x58 = (+0x58 & ~0x41) | (player +0x58 & 0x40). */
    s->flags58 = (uint8_t)((s->flags58 & ~0x41u) |
                           (in->player_flags58 & 0x40u));
    out->set_42c |= 1;
}

/* ------------------------------------------------------------------
 * param0 12 — create sub_08060D48 (record-1 sprite spawn).
 * sub_0805B754(e,1) gates; success parks the entity at screen
 * X 0xFFF0 / Y 0xA0 - A094[0xA] with +0x48 = 0x4000 and
 * +0x58 |= 0x40. +0x42C |= 1 runs on BOTH paths (the ROM
 * reaches _08060D9E after EntityDelete too).
 * ------------------------------------------------------------------ */
static void create12(AosObject20Slots *s, const AosObject20SlotsIn *in,
                     AosObject20SlotsOut *out) {
    if (helper5b754(s, 1, in, out) != 0) {
        anim3(out, 1, 0, 1);            /* sub_0803F2C8(e,1,0,1) */
        out->anim_step_count++;
        s->scr42 = (int16_t)0xFFF0;
        s->scr46 = (int16_t)(0xA0 - in->a094_a);
        s->vel48 = 0x4000;              /* 0x80 << 7 */
        s->flags58 |= 0x40;
    }
    out->set_42c |= 1;                  /* _08060D9E tail */
}

/* param0 13 — create sub_08061874: +0x42C |= 1 only. */
static void create13(AosObject20Slots *s, const AosObject20SlotsIn *in,
                     AosObject20SlotsOut *out) {
    (void)s; (void)in;
    out->set_42c |= 1;
}

/* param0 14 — create sub_080618C8: +0x42C |= 1 only. */
static void create14(AosObject20Slots *s, const AosObject20SlotsIn *in,
                     AosObject20SlotsOut *out) {
    (void)s; (void)in;
    out->set_42c |= 1;
}

/* ------------------------------------------------------------------
 * param0 15 — create sub_08060F24 (record-1 sprite spawn).
 * sub_0805B754(e,1) gates; success mirrors the room origin:
 * +0x42 = -A094[6], +0x46 = 0xA0 - A094[0xA], +0x58 |= 0x60,
 * +0x42C |= 1.
 * ------------------------------------------------------------------ */
static void create15(AosObject20Slots *s, const AosObject20SlotsIn *in,
                     AosObject20SlotsOut *out) {
    if (helper5b754(s, 1, in, out) == 0)
        return;
    anim3(out, 0, 0, 1);                /* sub_0803F2C8(e,0,0,1) */
    out->anim_step_count++;
    s->scr42 = (int16_t)(0u - in->a094_6);   /* rsbs r0, r0, #0 */
    s->scr46 = (int16_t)(0xA0 - in->a094_a);
    s->flags58 |= 0x60;                 /* 0x20 | 0x40 */
    out->set_42c |= 1;
}

/* ------------------------------------------------------------------
 * param0 16 — create sub_08061668 (record-1 sprite spawn).
 * +0x42C bit 0x10000 set marks +0x59 |= 8; otherwise
 * sub_0805B754(e,1) gates. Success: F2C8(e,4,0,1), F17C,
 * +0x42 = 0x50 - A094[6], +0x46 = 0xC0 - A094[0xA],
 * +0x58 |= 0x40 and the +0x3E byte |= 4.
 * ------------------------------------------------------------------ */
static void create16(AosObject20Slots *s, const AosObject20SlotsIn *in,
                     AosObject20SlotsOut *out) {
    if (in->flag42c & 0x10000u) {
        s->flags59 |= 0x8;
        return;
    }
    if (helper5b754(s, 1, in, out) == 0)
        return;
    anim3(out, 4, 0, 1);                /* sub_0803F2C8(e,4,0,1) */
    out->anim_step_count++;
    s->scr42 = (int16_t)(0x50 - in->a094_6);
    s->scr46 = (int16_t)(0xC0 - in->a094_a);
    s->flags58 |= 0x40;
    s->field3e |= 4;                    /* +0x3E |= 4 */
}

/* param0 17 — create sub_08061924: bx lr (empty variant). */
static void create17(AosObject20Slots *s, const AosObject20SlotsIn *in,
                     AosObject20SlotsOut *out) {
    (void)s; (void)in; (void)out;
}

/* param0 18 — create sub_08061978: +0x42C |= 1 only. */
static void create18(AosObject20Slots *s, const AosObject20SlotsIn *in,
                     AosObject20SlotsOut *out) {
    (void)s; (void)in;
    out->set_42c |= 1;
}

/* ------------------------------------------------------------------
 * param0 0 — update sub_0805BCB4.
 * Returns r6 (0, 0x200 or 0x10). No sub_0803F17C tail.
 * ------------------------------------------------------------------ */
static int update0(AosObject20Slots *s, const AosObject20SlotsIn *in,
                   AosObject20SlotsOut *out) {
    int r6 = 0;
    uint8_t st = s->state;
    if (st == 0) {
        /* Clamp the player +0x42 s16 to <= 0x4F, clear the
         * player +0x58 0x41 bits, set own +0x58 bit 0x40 and
         * open window 0xC. */
        if (in->player_scr42 > 0x4F) {
            out->player_scr42_set = true;
            out->player_scr42_val = 0x4F;
        }
        out->player58_andnot_41 = true;
        s->flags58 |= 0x40;
        out->window_text = 0xC;            /* sub_08067934(0xC) */
        s->state = 1;
        return r6;
    }
    if (st == 1) {
        /* The text poll runs only while +0xB != 3; a done poll
         * clears +0x42C bit 1 and advances +0xA to 2. */
        if (s->sub_b != 3) {
            out->window_poll = true;        /* sub_08067A04 */
            if (in->text_done) {
                out->clear_42c |= 2;
                s->state = 2;
            }
        }
        uint8_t b = s->sub_b;
        if (b == 1)
            s->state = 0x80;                /* _0805BD4A */
        /* Jump table on +0xB - 1, cases 0-6 (+0xB 1-7); +0xB 0
         * and > 7 fall to the end. */
        switch (b) {
        case 1:                             /* _0805BD84 */
            s->flags58 &= ~0x41u;
            break;
        case 2:                             /* _0805BD94 */
            anim3(out, 2, 2, 0);            /* F2C8(e,2,2,0) */
            s->sub_b++;                     /* _0805BE08 */
            break;
        case 3:                             /* _0805BDA2 */
            if (s->flags59 & 1) {
                anim3(out, 3, 2, 0);        /* F2C8(e,3,2,0) */
                s->sub_b++;
                s->vel48 = (int32_t)0xFFFE0000;
                s->vel50 = 0x800;           /* 0x80 << 4 */
            }
            break;
        case 4: {                           /* _0805BDD4 */
            if (s->field6e == 1)
                s->field6c |= 2;            /* +0x6C |= 2 */
            s->pos40 += s->vel48;
            s->vel48 += s->vel50;
            if (s->vel48 > (int32_t)0xFFFE8000)
                r6 = 0x200;                 /* 0x80 << 2 */
            if (s->vel48 > (int32_t)0xFFFFF000)
                s->sub_b++;                 /* _0805BE08 */
            break;
        }
        case 5:                             /* _0805BE18 */
            anim3(out, 2, 2, 0);            /* F2C8(e,2,2,0) */
            break;
        case 6: {                           /* _0805BE26 */
            s->pos40 += 0x40000;            /* 0x80 << 0xB */
            s->pos44 += (int32_t)0xFFFF8000;
            if (s->scr42 > 0x100) {
                /* The ROM stores r2 + 1 where r2 still holds
                 * the +0xB byte loaded at _0805BD3E (= 6). */
                s->sub_b = (uint8_t)(b + 1);
            }
            r6 = 0x10;
            break;
        }
        default:
            break;
        }
        return r6;
    }
    if (st == 0x80) {
        out->release_arg = 1;               /* sub_0805B66C(e,1) */
    }
    return r6;
}

/* ------------------------------------------------------------------
 * param0 1 — update sub_0805BE64. Returns 0; no F17C tail.
 * ------------------------------------------------------------------ */
static int update1(AosObject20Slots *s, const AosObject20SlotsIn *in,
                   AosObject20SlotsOut *out) {
    uint8_t st = s->state;
    if (st == 0) {
        /* GetEntityRoomXPositionInteger(player) twice: the
         * second call only runs while the first reads > 0x80;
         * a result in (0x80, 0x11F] arms +0x42C bit 0 and
         * advances +0xA. */
        out->roomx_calls = 1;
        if (in->ret_roomx > 0x80) {
            out->roomx_calls = 2;
            if (in->ret_roomx <= 0x11F) {
                out->set_42c |= 1;
                s->state = 1;
            }
        }
        return 0;
    }
    if (st == 1) {
        out->window_text = 0xC;            /* sub_08067934(0xC) */
        s->state = 2;
        return 0;
    }
    if (st == 2) {
        out->window_poll = true;
        if (in->text_done) {
            out->release_arg = 1;           /* sub_0805B66C(e,1) */
            s->state = 3;
        }
        return 0;
    }
    if (st == 0x80) {
        out->unk2cb0_feff = true;           /* &= 0xFEFF */
        s->state = 0x81;
        return 0;
    }
    if (st == 0x81) {
        out->release_arg = 1;
        out->unk2cb0_or100 = true;          /* |= 0x100 */
    }
    return 0;
}

/* param0 2 — update sub_0805CB70: bx lr (empty variant). */
static int update2(AosObject20Slots *s, const AosObject20SlotsIn *in,
                   AosObject20SlotsOut *out) {
    (void)s; (void)in; (void)out;
    return 0;
}

/* ------------------------------------------------------------------
 * param0 11 — update sub_08060C20.
 * +0x42C |= 1 every call; returns r8 (0 or 0x20). The +0xD
 * countdown feeds the +0xB==0 window-0x417 clear; the +0xB==2
 * case polls the player room X (> 0xB6 returns 0x20).
 * ------------------------------------------------------------------ */
static int update11(AosObject20Slots *s, const AosObject20SlotsIn *in,
                    AosObject20SlotsOut *out) {
    int r8 = 0;
    out->set_42c |= 1;                      /* _08060C54 head */
    uint8_t st = s->state;
    if (st == 0) {
        out->b878 = 0x1040;                 /* sub_0805B878(
                                             * 0x82 << 5) */
        out->window_text = 0x16;            /* sub_08067934(0x16) */
        out->write_417 = true;              /* +0x417 = 1 */
        out->write_417_val = 1;
        s->state = 1;
        s->timer_d = 0x90;
    } else if (st == 1) {
        out->window_poll = true;
        if (in->text_done) {
            s->state = 0x80;
        } else if (s->sub_b <= 4) {
            switch (s->sub_b) {
            case 0:                         /* _08060CBC */
                s->timer_d = (uint8_t)(s->timer_d - 1);
                if (s->timer_d == 0xFF) {   /* old +0xD was 0 */
                    s->sub_b++;
                    out->write_417 = true;  /* +0x417 = 0 */
                    out->write_417_val = 0;
                }
                break;
            case 1:                         /* _08060D2E: noop */
                break;
            case 2:                         /* _08060CE0 */
                out->write_417 = true;
                out->write_417_val = 1;
                out->roomx_calls++;
                if (in->ret_roomx > 0xB6) {
                    r8 = 0x20;
                } else {
                    s->sub_b++;             /* _08060D14 */
                }
                break;
            case 3:                         /* _08060D0C */
                out->write_417 = true;
                out->write_417_val = 0;
                s->sub_b++;                 /* _08060D14 */
                break;
            default:                        /* case 4: noop */
                break;
            }
        }
    } else if (st == 0x80) {
        out->jingle = 0x28;                 /* sub_08012048(0x28) */
        out->release_arg = 1;               /* sub_0805B66C(e,1) */
    }
    /* _08060D2E tail: sub_0803F17C unless +0xA == 0x80. */
    if (s->state != 0x80)
        out->anim_step_count++;
    out->return_code = r8;
    return r8;
}

/* ------------------------------------------------------------------
 * param0 12 — update sub_08060DC4.
 * Returns r6 (0 or 0x40). +0xB==1 runs the +0x6D-gated approach
 * and the +0x42 > 7 / player +0x58 write block; +0xB==4 runs
 * the +0xC/+0xD 0x40 hold driving +0x417.
 * ------------------------------------------------------------------ */
static int update12(AosObject20Slots *s, const AosObject20SlotsIn *in,
                    AosObject20SlotsOut *out) {
    int r6 = 0;
    uint8_t st = s->state;
    if (st == 0) {
        out->set_42c |= 1;
        out->window_text = 0x19;            /* sub_08067934(0x19) */
        s->state = 1;
    } else if (st == 1) {
        out->window_poll = true;
        if (in->text_done) {
            s->state = 0x80;
        } else {
            uint8_t b = s->sub_b;
            if (b == 1) {
                /* _08060E16: +0xC wraps to 1 once — anim gate
                 * (+0x6D != 0 -> F2C8(e,0,0,1)) and +0x417 =
                 * +0xB. */
                s->sub_c = (uint8_t)(s->sub_c + 1);
                if (s->sub_c == 1) {
                    if (s->anim6d != 0)
                        anim3(out, 0, 0, 1);
                    out->write_417 = true;  /* = +0xB (1) */
                    out->write_417_val = b;
                }
                if (s->scr42 > 7) {
                    /* _08060E68: anim gate (+0x6D != 4 ->
                     * F2C8(e,4,0,1)), player +0x58 |= 0x40,
                     * +0x417 = 0, +0xB++, +0xC = 0. */
                    if (s->anim6d != 4)
                        anim3(out, 4, 0, 1);
                    out->player58_or_40 = true;
                    out->write_417 = true;
                    out->write_417_val = 0;
                    s->sub_b++;
                    s->sub_c = 0;
                } else {
                    s->pos40 += s->vel48;   /* _08060E4A tail */
                }
            } else if (b >= 3) {
                r6 = 0x40;
                if (b == 4) {
                    /* _08060EB4: +0xC wrap arms +0xD = 0x40 and
                     * writes +0x417 = 1; otherwise +0xD counts
                     * down and a zero +0xD writes +0x417 = 0. */
                    s->sub_c = (uint8_t)(s->sub_c + 1);
                    if (s->sub_c == 1) {
                        s->timer_d = 0x40;
                        out->write_417 = true;
                        out->write_417_val = 1;
                    } else if (s->timer_d != 0) {
                        s->timer_d = (uint8_t)(s->timer_d - 1);
                    } else {
                        out->write_417 = true;
                        out->write_417_val = 0;
                    }
                }
            }
            /* +0xB 0/2/3 (and > 4) fall to the end. */
        }
    } else if (st == 0x80) {
        out->window_close = true;              /* sub_08067898 */
        out->set_42c |= 1;
    }
    if (s->state != 0x80)
        out->anim_step_count++;
    out->return_code = r6;
    return r6;
}

/* param0 13 — update sub_08061890: window 0x1A, then release. */
static int update13(AosObject20Slots *s, const AosObject20SlotsIn *in,
                    AosObject20SlotsOut *out) {
    uint8_t st = s->state;
    if (st == 0) {
        out->window_text = 0x1A;            /* sub_08067934(0x1A) */
        s->state = 1;
    } else if (st == 1) {
        out->window_poll = true;
        if (in->text_done)
            out->release_arg = 1;           /* sub_0805B66C(e,1) */
    }
    out->return_code = 0;
    return 0;
}

/* param0 14 — update sub_080618E4: window 0x1B; a done poll
 * closes the window and marks +0x59 |= 8. */
static int update14(AosObject20Slots *s, const AosObject20SlotsIn *in,
                    AosObject20SlotsOut *out) {
    uint8_t st = s->state;
    if (st == 0) {
        out->window_text = 0x1B;            /* sub_08067934(0x1B) */
        s->state = 1;
    } else if (st == 1) {
        out->window_poll = true;
        if (in->text_done) {
            out->window_close = true;       /* sub_08067898 */
            s->flags59 |= 0x8;
        }
    }
    out->return_code = 0;
    return 0;
}

/* ------------------------------------------------------------------
 * param0 15 — update sub_08060F98 (0x08060F98-0x08061664, ~840 asm
 * lines): the arena/boss intro, a 12-case +0xB machine.
 *
 * State +0xA: 0 opens window 0x1C, writes +0x417 = 1 and advances;
 * 1 polls sub_08067A04 (a done poll forces +0xA = 0x80 but the
 * +0xB case still runs that call) then dispatches +0xB 0..0xB —
 * cases 1 and 10 are native no-ops; 0x80 is the exit sequence.
 * Every path returns the per-call flag word [sp,#0x30] (case 2
 * stores 0x10/0x20, cases 7/8 OR 0x80) and runs sub_0803F17C
 * while (int8)+0xA >= 0.
 *
 * Native callees without a ported equivalent are injected probes
 * or reported through Out — nothing is silently substituted:
 *   sub_080212C8()              -> in->probe_212c8 (& 0x1E)
 *   sub_08061824(0xAA,0x3C,0x21)-> in->probe_61824, calls counted
 *   0x081188E4[+0x14]           -> in->tbl14 (the ten-word stack
 *                                copy; cases 5/6 index it)
 *   record +0x524/+0x53C        -> in->rec_524/in->rec_53c for the
 *                                +0x11-selected 0x84-stride record,
 *                                the store reported via
 *                                rec_53c_write/rec_53c_val
 *   player +0x5A                -> in->player_byte5a, each store
 *                                listed in player5a_vals
 *   sub_0806E140(e,1,0x200000,
 *     0x10000,1)                -> in->probe_6e140 (& 4)
 *   sub_08013EEC(0x16C)         -> out->sfx_13eec
 *   sub_08013CF0(0x21)          -> out->call_13cf0
 *   PlaySong                    -> out->playsong[]
 *   sub_0803D18C/sub_0803CDF0   -> out->fade_* (fn 0/1, 6 args)
 *   sub_08000EA8(0x40)          -> out->call_00ea8
 *   sub_0801D028(0x52,0,0,0)    -> out->call_1d028
 *   sub_0803D270/sub_0803CED4   -> out->call_3d270/call_3ced4
 *   gEwramData + 0x90 u16       -> out->write_90/write_90_val
 *
 * Case 3 arms +0xD = 0x60 through sub_08013EEC and completes only
 * when +0xD reads 0; the routine itself never decrements +0xD, so
 * the countdown owner is external and the port stays literal.
 * ------------------------------------------------------------------ */
static int update15(AosObject20Slots *s, const AosObject20SlotsIn *in,
                    AosObject20SlotsOut *out) {
    /* [sp,#0x30]: the flag word returned in r0; the 0x80 exit also
     * stores it as a u16 at gEwramData + 0x90. */
    uint32_t sp30 = 0;
    /* player entity +0x5A; the native mask is -0x79 = ~0x78,
     * i.e. keep 0x87, optionally OR 0x20. */
    uint8_t p5a = in->player_byte5a;
    uint8_t st = s->state;
    if (st == 0) {
        /* _08060FF4: sub_08067934(0x1C), +0x417 = 1, +0xA -> 1. */
        out->window_text = 0x1C;
        out->write_417 = true;
        out->write_417_val = 1;
        s->state = 1;
    } else if (st == 1) {
        /* _08061010: the poll; done forces +0xA = 0x80 and the
         * +0xB jump below still runs this call. */
        out->window_poll = true;
        if (in->text_done)
            s->state = 0x80;
        uint8_t b = s->sub_b;
        if (b <= 0xB) {
            switch (b) {
            case 0: {               /* _08061064 */
                if (in->probe_212c8 & 0x1E)
                    break;          /* input busy: no advance */
                /* Record selected by +0x11: base =
                 * gEwramData + idx*0x84; bit 6 of the +0x53C
                 * byte = record s32 at +0x524 < player +0x40,
                 * bit 0x40 rewritten from the compare. */
                unsigned bit6 = in->rec_524 < in->player_pos40
                                     ? 0x40u : 0u;
                uint8_t nv =
                    (uint8_t)((in->rec_53c & ~0x40u) | bit6);
                out->rec_53c_write = true;
                out->rec_53c_val = nv;
                out->probe61824_calls++;
                if ((in->probe_61824 & 0xFF) != 0) {
                    s->sub_b = 2;
                } else {
                    s->sub_b = 3;   /* _0806110C */
                }
                break;
            }
            case 1:                 /* _0806163C: native noop */
                break;
            case 2: {               /* _080610D0 */
                out->probe61824_calls++;
                /* sign-extended low byte must equal 1 */
                if ((int8_t)(in->probe_61824 & 0xFF) == 1) {
                    sp30 = 0x10;
                    if (in->player_scr42 > 0xAA)
                        sp30 = 0x20;
                } else {
                    s->sub_b = 3;   /* _0806110C */
                }
                break;
            }
            case 3: {               /* _08061112 */
                if (s->sub_c == 0) {
                    s->timer_d = 0x60;
                    s->sub_c = 1;
                    out->sfx_13eec = 0x16C; /* sub_08013EEC(
                                             * 0xB6 << 1) */
                }
                if (s->timer_d == 0) {
                    out->write_417 = true;  /* +0x417 = 0 */
                    out->write_417_val = 0;
                    s->sub_b++;
                    s->sub_c = s->timer_d;  /* = 0 */
                }
                break;
            }
            case 4: {               /* _08061144 */
                if (s->sub_c == 0) {
                    fade_report(out, 0, 0x1F, 0x1F, 0x1F, 0,
                                0x10000, 0x20);
                    fade_report(out, 1, 0x1F, 0x1F, 0x1F,
                                0x4000, 0x10000, 0x10);
                    playsong_report(out, 0x170);
                    s->sub_c = 1;
                }
                uint8_t old_d = s->timer_d;
                s->timer_d = (uint8_t)(s->timer_d - 1);
                if (old_d & 3) {
                    p5a = (uint8_t)((p5a & 0x87u) | 0x20u);
                } else {
                    p5a = (uint8_t)(p5a & 0x87u);
                }
                player5a_store(out, p5a);
                out->call_1d028 = true; /* sub_0801D028(0x52,0,0,0) */
                break;
            }
            case 5: {               /* _080611D8 */
                uint8_t old_d = s->timer_d;
                s->timer_d = (uint8_t)(s->timer_d - 1);
                if (old_d & 3) {
                    p5a = (uint8_t)((p5a & 0x87u) | 0x20u);
                } else {
                    p5a = (uint8_t)(p5a & 0x87u);
                }
                player5a_store(out, p5a);
                /* divisor = tbl14[+0x14] * 3 / 2 truncated;
                 * hit only when (+0xD new) % divisor == 0. */
                int32_t div = (in->tbl14 * 3) / 2;
                if (slots_modsi3(s->timer_d, div) == 0) {
                    playsong_report(out, 0x170);
                    fade_report(out, 0, 0x1F, 0x1F, 0x1F, 0,
                                0x10000, 0x20);
                    fade_report(out, 1, 0x1F, 0x1F, 0x1F,
                                0x4000, 0x10000, 0x10);
                    s->field14 = (uint8_t)(s->field14 + 1);
                    if (s->field14 == 0xA)
                        s->field14 = 0; /* strb r5 (= 0) */
                }
                out->call_1d028 = true;
                break;
            }
            case 6: {               /* _08061244 */
                uint8_t old_d = s->timer_d;
                s->timer_d = (uint8_t)(s->timer_d - 1);
                if (old_d & 2) {
                    p5a = (uint8_t)((p5a & 0x87u) | 0x20u);
                } else {
                    p5a = (uint8_t)(p5a & 0x87u);
                }
                player5a_store(out, p5a);
                /* same hit block, divisor = tbl14[+0x14] raw */
                if (slots_modsi3(s->timer_d, in->tbl14) == 0) {
                    playsong_report(out, 0x170);
                    fade_report(out, 0, 0x1F, 0x1F, 0x1F, 0,
                                0x10000, 0x20);
                    fade_report(out, 1, 0x1F, 0x1F, 0x1F,
                                0x4000, 0x10000, 0x10);
                    s->field14 = (uint8_t)(s->field14 + 1);
                    if (s->field14 == 0xA)
                        s->field14 = 0;
                }
                out->call_1d028 = true;
                break;
            }
            case 7: {               /* _080612FC */
                if (s->sub_c == 0) {
                    /* _0806131A init: +0x417 = 1, +0xD = 0x87,
                     * +0xC = 1, three songs and the first fade
                     * pair, then falls into the +0xC==1 body. */
                    out->write_417 = true;
                    out->write_417_val = 1;
                    s->timer_d = 0x87;  /* (0xFF >> 1) + 8 */
                    s->sub_c = 1;
                    playsong_report(out, 0x816C);
                    playsong_report(out, 0x197);
                    playsong_report(out, 0x169);
                    fade_report(out, 0, 0x1F, 0x1F, 0x1F,
                                0x10000, 0x50, 0x7F);
                    fade_report(out, 1, 0x1F, 0x1F, 0x1F,
                                0x10000, 0x50, 0x7F);
                }
                uint8_t c = s->sub_c;
                if (c == 1) {
                    /* _08061368: +0x5A write on old +0xD & 2,
                     * then +0xD counts down; a wrap (new ==
                     * 0xFF) fires sub_08000EA8(0x40), the second
                     * fade pair, +0xC++ and a final +0x5A clear. */
                    uint8_t old_d = s->timer_d;
                    if (old_d & 2) {
                        p5a = (uint8_t)((p5a & 0x87u) | 0x20u);
                    } else {
                        p5a = (uint8_t)(p5a & 0x87u);
                    }
                    player5a_store(out, p5a);
                    s->timer_d = (uint8_t)(s->timer_d - 1);
                    if (s->timer_d == 0xFF) {
                        out->call_00ea8 = 0x40;
                        fade_report(out, 0, 0x1F, 0x1F, 0x1F, 0,
                                    0x10000, 0xFF);
                        fade_report(out, 1, 0x1F, 0x1F, 0x1F, 0,
                                    0x10000, 0xFF);
                        s->timer_d = 0xFF;
                        s->sub_c++;
                        p5a = (uint8_t)(p5a & 0x87u);
                        player5a_store(out, p5a);
                    }
                } else if (c == 2) {
                    /* _08061424: +0xD == 0x20 arms
                     * sub_08013CF0(0x21); wrap -> sub_0803D270 +
                     * sub_0803CED4 (native order), +0xC++,
                     * +0xD = 0x40. */
                    if (s->timer_d == 0x20)
                        out->call_13cf0 = 0x21;
                    s->timer_d = (uint8_t)(s->timer_d - 1);
                    if (s->timer_d == 0xFF) {
                        out->call_3d270 = true;
                        out->call_3ced4 = true;
                        s->sub_c++;
                        s->timer_d = 0x40;
                    }
                } else if (c == 3) {
                    /* _08061452: wrap -> +0x5A clear, +0xB++,
                     * +0xC = 0, +0x417 = 0. */
                    s->timer_d = (uint8_t)(s->timer_d - 1);
                    if (s->timer_d == 0xFF) {
                        p5a = (uint8_t)(p5a & 0x87u);
                        player5a_store(out, p5a);
                        s->sub_b++;
                        s->sub_c = 0;
                        out->write_417 = true;
                        out->write_417_val = 0;
                    }
                }
                /* _08061486: +0xC > 3 also lands here. */
                sp30 |= 0x80;
                break;
            }
            case 8:                 /* _0806149C */
                if (s->sub_c == 0)
                    s->sub_c = 1;
                sp30 |= 0x80;           /* _080614A6 */
                break;
            case 9: {               /* _080614B0 */
                if (s->sub_c == 0) {
                    /* _080614BC init: player +0x58 |= 0x40,
                     * +0x417 = 1, own +0x58 &= ~0x20, +0xC = 1,
                     * +0xD = 0xC0 — then falls into the probe. */
                    out->player58_or_40 = true;
                    out->write_417 = true;
                    out->write_417_val = 1;
                    s->flags58 = (uint8_t)(s->flags58 & ~0x20u);
                    s->sub_c = 1;
                    s->timer_d = 0xC0;
                }
                /* _080614EE: sub_0806E140(e,1,0x200000,0x10000,1);
                 * result & 4 clear integrates the entity. */
                if ((in->probe_6e140 & 4) == 0) {
                    s->pos40 += s->vel48;
                    s->pos44 += s->vel4c;
                } else if (s->anim6d != 0) {
                    anim3(out, 0, 0, 1);    /* F2C8(e,0,0,1) */
                }
                /* _08061542: signed +0x42 >= 0x50 parks the entity
                 * and advances +0xB. */
                if (s->scr42 >= 0x50) {
                    s->scr42 = 0x50;
                    if (s->anim6d != 1)
                        anim3(out, 1, 0, 1);/* F2C8(e,1,0,1) */
                    s->scr46 = (int16_t)((uint16_t)s->scr46 + 1);
                    out->write_417 = true;  /* +0x417 = 0 */
                    out->write_417_val = 0;
                    s->sub_b++;
                    s->sub_c = 0;
                }
                break;
            }
            case 10:                /* _0806163C: native noop */
                break;
            case 11:                /* _08061588 */
                if (s->sub_c == 0) {
                    s->sub_c = 1;
                    if (s->anim6d != 4)
                        anim3(out, 4, 0, 1);/* F2C8(e,4,0,1) */
                }
                break;
            default:
                break;
            }
        }
    } else if (st == 0x80) {
        /* _080615AA exit sequence: jingle 0x3A, sub_0803CED4,
         * sub_0803D270, sub_0805B878(0x1040), the flag-word store
         * to gEwramData + 0x90, release(0), the player +0x5A clear,
         * the +0x42/+0x46 park, +0x58 &= ~0x20, the +0x6D!=4 anim
         * gate, F17C, PlaySong(0x816C), sub_08000EA8(0x40) and the
         * +0xA/+0xC/+0xB resets. */
        out->jingle = 0x3A;                 /* sub_08012048(0x3A) */
        out->call_3ced4 = true;
        out->call_3d270 = true;
        out->b878 = 0x1040;                 /* sub_0805B878(
                                             * 0x82 << 5) */
        out->write_90 = true;
        out->write_90_val = (uint16_t)sp30;
        out->release_arg = 0;               /* sub_0805B66C(e,0) */
        p5a = (uint8_t)(p5a & 0x87u);
        player5a_store(out, p5a);
        s->scr42 = 0x50;                    /* strh r7 (0x50) */
        s->scr46 = (int16_t)(0xC0 - in->a094_a);
        s->flags58 = (uint8_t)(s->flags58 & ~0x20u);
        if (s->anim6d != 4)
            anim3(out, 4, 0, 1);            /* F2C8(e,4,0,1) */
        out->anim_step_count++;             /* sub_0803F17C */
        playsong_report(out, 0x816C);
        out->call_00ea8 = 0x40;
        s->state = 0x81;
        s->sub_c = 0;
        s->sub_b = 0;
    }
    /* _0806163C: sub_0803F17C while (int8)+0xA >= 0; the ROM
     * returns the flag word. */
    if ((int8_t)s->state >= 0)
        out->anim_step_count++;
    out->return_code = (int)sp30;
    return (int)sp30;
}

/* ------------------------------------------------------------------
 * param0 16 — update sub_080616F8. Returns 0; no F17C tail.
 * ------------------------------------------------------------------ */
static int update16(AosObject20Slots *s, const AosObject20SlotsIn *in,
                    AosObject20SlotsOut *out) {
    uint8_t st = s->state;
    if (st == 0) {
        out->release_arg = 0;               /* sub_0805B66C(e,0) */
        s->state = 1;
        /* falls through into the state-1 body, like the ROM */
    }
    if (st <= 1) {
        /* _08061734: the probe chain — sub_08068AD4 rect hit,
         * word16 & 0x42, +0x4BE s16 == 0 — then the player
         * entity velocity writes, the +0xD window pick
         * (old +0xD > 3 -> 0x1E else 0x1D), the facing mirror
         * and +0x42C |= 1. */
        if (in->probe_68ad4 && (in->word16 & 0x42) &&
            in->s16_4be == 0) {
            out->player_w48_zero = true;    /* player +0x48 = 0 */
            out->player_w50_zero = true;    /* player +0x50 = 0 */
            if (in->player_w4c < 0)
                out->player_w4c_min0 = true;
            uint8_t old_d = s->timer_d;
            s->timer_d = (uint8_t)(s->timer_d + 1);
            out->window_text = old_d > 3 ? 0x1E : 0x1D;
            unsigned bit = s->pos40 < in->player_pos40 ? 0x40u : 0u;
            s->flags58 = (uint8_t)((s->flags58 & ~0x41u) | bit);
            out->set_42c |= 1;
            s->state = (uint8_t)(s->state + 1); /* -> 2 */
        }
    } else if (st == 2) {
        out->window_poll = true;
        if (in->text_done) {
            s->state = 0x80;
            out->release_arg = 0;           /* sub_0805B66C(e,0) */
        }
    }
    out->return_code = 0;
    return 0;
}

/* param0 17 — update sub_08061928: +0x42C |= 1 + window 0x17,
 * then release on a done poll. */
static int update17(AosObject20Slots *s, const AosObject20SlotsIn *in,
                    AosObject20SlotsOut *out) {
    uint8_t st = s->state;
    if (st == 0) {
        out->set_42c |= 1;
        out->window_text = 0x17;            /* sub_08067934(0x17) */
        s->state = 1;
    } else if (st == 1) {
        out->window_poll = true;
        if (in->text_done)
            out->release_arg = 1;           /* sub_0805B66C(e,1) */
    }
    out->return_code = 0;
    return 0;
}

/* param0 18 — update sub_08061994: +0x42C |= 1 + window 0x18;
 * a done poll closes the window and marks +0x59 |= 8; state
 * 0x80 releases. */
static int update18(AosObject20Slots *s, const AosObject20SlotsIn *in,
                    AosObject20SlotsOut *out) {
    uint8_t st = s->state;
    if (st == 0) {
        out->set_42c |= 1;
        out->window_text = 0x18;            /* sub_08067934(0x18) */
        s->state = 1;
    } else if (st == 1) {
        out->window_poll = true;
        if (in->text_done) {
            out->window_close = true;       /* sub_08067898 */
            s->flags59 |= 0x8;
        }
    } else if (st == 0x80) {
        out->release_arg = 1;               /* sub_0805B66C(e,1) */
    }
    out->return_code = 0;
    return 0;
}

/* ------------------------------------------------------------------
 * Dispatchers — the shared Object20Create gates/tail and the
 * Object20Update prologue/tail are folded in, matching
 * aos_object20_story.
 * ------------------------------------------------------------------ */
bool aos_object20_slots_create(AosObject20Slots *s, uint8_t param0,
                               const AosObject20SlotsIn *in,
                               AosObject20SlotsOut *out) {
    out_reset(out);
    s->deleted = false;
    /* Shared Object20Create gates (0x0805B42C). */
    bool gate_pass;
    if (in->flag1327a > 0) {
        gate_pass = true;
    } else {
        gate_pass = (in->flag42c & 0x00200000u) != 0; /* 0x80 << 0xE */
    }
    if (gate_pass) {
        if (in->byte8e & 0x80) {
            gate_pass = false;
        } else if (in->byte13266 != 0 &&
                   (in->flag42c & 0x8000u) == 0) {
            gate_pass = false;
        }
    }
    if (!gate_pass) {
        s->deleted = true;
        out->deleted = true;
        return false;
    }
    switch (param0) {
    case 0:  create0(s, in, out); break;
    case 1:  create1(s, in, out); break;
    case 2:  create2(s, in, out); break;
    case 11: create11(s, in, out); break;
    case 12: create12(s, in, out); break;
    case 13: create13(s, in, out); break;
    case 14: create14(s, in, out); break;
    case 15: create15(s, in, out); break;
    case 16: create16(s, in, out); break;
    case 17: create17(s, in, out); break;
    case 18: create18(s, in, out); break;
    default:
        /* Not a slots-table param0 — report and keep the entity
         * inert rather than deleting it. */
        out->unported = true;
        break;
    }
    if (s->flags59 & 0x8)
        s->deleted = true;
    if (s->deleted) {
        out->deleted = true;
        return false;
    }
    /* Shared tail: +0x42C |= 0x200, engine init calls,
     * gEwramData + 0x41C = entity, conditional
     * SetPlayerInput(1, 0), sub_0806666C. */
    out->set_42c |= 0x200;
    out->call_0eb04 = true;
    out->call_235c8 = true;
    out->call_2dff0 = true;
    out->c5a8 = 0;
    out->call_6666c = true;
    if (in->flag42c & 1) {
        out->setplayerinput = 1;
        out->setplayerinput_arg = 0;
    }
    return true;
}

int aos_object20_slots_update(AosObject20Slots *s, uint8_t param0,
                              const AosObject20SlotsIn *in,
                              AosObject20SlotsOut *out) {
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
            out->write_414 = true;
            out->write_414_val =
                (uint8_t)((in->byte414 & ~0x10u) | 5u);
        }
    }
    int ret = 0;
    switch (param0) {
    case 0:  ret = update0(s, in, out); break;
    case 1:  ret = update1(s, in, out); break;
    case 2:  ret = update2(s, in, out); break;
    case 11: ret = update11(s, in, out); break;
    case 12: ret = update12(s, in, out); break;
    case 13: ret = update13(s, in, out); break;
    case 14: ret = update14(s, in, out); break;
    case 15: ret = update15(s, in, out); break;
    case 16: ret = update16(s, in, out); break;
    case 17: ret = update17(s, in, out); break;
    case 18: ret = update18(s, in, out); break;
    default:
        out->unported = true;
        break;
    }
    if (ret == 0 && out->return_code != 0)
        ret = out->return_code;
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
