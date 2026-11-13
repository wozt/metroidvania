/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_shop.h"
#include <string.h>

static void out_reset(AosShopOut *out) {
    memset(out, 0, sizeof *out);
    out->window_text = -1;
    out->b878 = -1;
    out->jingle = -1;
    out->song = -1;
    out->anim_a = -1;
}

/* sub_08061ABC — the give/sell sequence. State +0xA: 0 arms the
 * held sound and falls into the state-1 proximity gate; 1 sets
 * +0x42C bit 0 and advances when player +0x42 + A094[6] <= 0xBF
 * (else returns 0x20); 2 opens text 0x1F; 3 polls the window and
 * runs the +0xB/+0xC purchase UI while it is up; 0x80 writes
 * +0x427 = 0 and releases through sub_08062020. */
int aos_shop_sell_update(AosShop *s, const AosShopInput *in, AosShopOut *out) {
    out_reset(out);
    int ret = 0;
    uint8_t st = s->state;
    if (st == 0) {
        out->b878 = 0x1040;                /* sub_0805B878(0x1040) */
        out->sound_2b = true;              /* sub_08013D60(0x2B) */
        s->state = 1;
        s->cursor = 0;
        s->menu = 0;
        st = 1;                            /* the ROM falls through */
    }
    if (st == 1) {
        out->set_42c |= 1;
        if (in->probe <= 0xBF) {
            s->state = (uint8_t)(s->state + 1);
            s->cursor = 0;
            s->menu = 0;
        } else {
            ret |= 0x20;
        }
        return ret;
    }
    if (st == 2) {
        out->window_text = 0x1F;           /* sub_08067934(0x1F) */
        s->state = 3;
        s->cursor = 0;
        s->menu = 0;
        return ret;
    }
    if (st == 3) {
        out->window_poll = true;           /* sub_08067A04() */
        if (in->text_done) {
            out->jingle = 0x29;            /* sub_08012048(0x29) */
            out->release_sound_2b = true;  /* sub_08013E18(0x2B) */
            out->clear_42c |= 0x00040000;
            out->song = 0x14;              /* sub_08013CF0(0x14) */
            s->state = 0x80;
            return ret;
        }
        if (s->menu == 1) {
            if (s->cursor == 0) {
                out->andnot_player58 = 0x41;
                out->andnot_ent58 = 0x21;
                out->write_417 = true;
                out->write_417_val = s->menu;   /* ROM stores +0xB (1) */
                out->write_427 = true;
                out->write_427_val = s->menu;
                s->cursor = 1;
            } else if (s->cursor == 1) {
                out->transaction = true;         /* sub_0800ED24(1, arg2, 0) */
                out->transaction_arg2 = in->a094_6 > 0x70 ? 0 : 0x40000;
                if (in->probe <= 0x11F) {
                    s->timer = 0x28;
                    s->cursor = 2;
                    if (s->flag6d != 0) {
                        out->anim_a = 0; out->anim_b = 1; out->anim_c = 1;
                    }
                } else if (in->probe <= 0x127) {
                    if (s->flag6d != 2) {
                        out->anim_a = 2; out->anim_b = 1; out->anim_c = 0;
                    }
                    s->vel48 = -0x8000;
                    s->pos40 += s->vel48;
                } else {
                    s->pos40 += s->vel48;
                }
            } else if (s->cursor == 2) {
                out->transaction = true;
                out->transaction_arg2 = 0;
                s->timer = (uint8_t)(s->timer - 1);
                if (s->timer == 0xFF) {           /* wrapped from 0 */
                    out->write_417 = true;
                    out->write_417_val = 0;
                    s->menu = 2;
                    s->cursor = 0;
                }
            }
        } else if (s->menu == 2) {
            out->transaction = true;
            out->transaction_arg2 = 0;
        }
        out->anim_step = true;               /* sub_0803F17C(e) tail */
        return ret;
    }
    if (s->state == 0x80) {
        out->write_427 = true;
        out->write_427_val = 0;
        out->frames_release = true;          /* sub_08062020 */
    }
    return ret;
}

/* sub_08061D10 — the buy/menu sequence. State +0xA: 0 opens text
 * 0x20 and falls into the poll; 1 polls — done arms +0xD = 0x40
 * and goes to 0x7F, live runs the +0xB d-pad menu; 0x7F counts
 * +0xD down then 0x80 releases through sub_0805B66C(1). The +0xB
 * states return 0x10/0x20 action codes. */
int aos_shop_buy_update(AosShop *s, const AosShopInput *in, AosShopOut *out) {
    out_reset(out);
    int ret = 0;
    uint8_t st = s->state;
    if (st == 0) {
        out->window_text = 0x20;           /* sub_08067934(0x20) */
        out->write_417 = true;
        out->write_417_val = 1;
        s->state = 1;
        st = 1;                            /* the ROM falls through */
    }
    if (st == 1) {
        out->window_poll = true;           /* sub_08067A04() */
        if (in->text_done) {
            s->timer = 0x40;
            s->state = 0x7F;
        } else {
            /* +0xB menu machine */
            if (s->menu == 0) {
                if (!in->input_busy) {
                    out->side_record_53c = true;   /* record +0x53C bit 6 */
                    if (in->dir == 0) {
                        s->menu = 4;
                    } else if (in->dir == -1) {
                        s->menu = 3;
                    } else if (in->dir == 1) {
                        s->menu = 2;
                    }
                }
            } else if (s->menu == 2) {
                if (in->dir == 1) {
                    ret = in->ent_left_of_player ? 0x20 : 0x10;
                } else {
                    s->menu = 4;
                }
            } else if (s->menu == 3) {
                out->side_record_53c = true;
                if (in->dir == -1) {
                    int32_t limit = in->a094_0 == 1 ? 0x1E : (int32_t)in->a094_0 << 5;
                    ret = in->room_q3 > (limit >> 1) ? 0x20 : 0x10;
                } else {
                    s->menu = 4;
                }
            } else if (s->menu == 4 && s->cursor == 0) {
                out->side_record_58 = true;       /* record +0x58 bit 6 */
                out->side_player_58 = true;       /* player +0x58 bit 6 */
                out->write_417 = true;
                out->write_417_val = s->cursor;   /* ROM stores +0xC (0) */
                s->cursor = 1;
            }
        }
        return ret;
    }
    if (st == 0x7F) {
        out->jingle = 0x2A;                  /* sub_08012048(0x2A) */
        out->set_42c |= 0x40000000;
        s->timer = (uint8_t)(s->timer - 1);
        if (s->timer == 0xFF)
            s->state = 0x80;
        return ret;
    }
    if (st == 0x80) {
        out->release = true;                 /* sub_0805B66C(1) */
    }
    return ret;
}
