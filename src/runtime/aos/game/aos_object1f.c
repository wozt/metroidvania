#include "aos_object1f.h"

void aos_object1f_create(AosObject1F *o, bool sprite_ok,
                         bool *deleted) {
    *o = (AosObject1F){0};
    *deleted = false;
    o->sound = -1;
    o->display_4a = -1;
    o->player_flags_5a = -1;
    if (!sprite_ok) {
        o->deleted = true;
        *deleted = true;
        return;
    }
    /* sub_08055BE0 x8 + sub_0805607C x2 children. */
    o->child_count = 10;
}

void aos_object1f_update(AosObject1F *o, bool gate_open) {
    o->sound = -1;
    o->input_lock = false;
    o->global_42c_bit0 = false;
    o->display_4a = -1;
    o->player_flags_5a = -1;
    o->player_flag20 = false;
    o->seq_end = false;

    if (o->state == 0) {
        if (gate_open) {
            o->sound = 0;               /* sub_0800C5A8(0) */
            o->player_flags_5a = 2;     /* (+0x5A & ~7) | 2 */
            o->display_4a = 0x10;
            o->frames_left = 0x80;
            o->state = 1;
        }
        return;
    }
    if (o->state != 1) return;

    /* State 1: gEwramData + 0x42C |= 1, input locked. */
    o->global_42c_bit0 = true;
    o->input_lock = true;

    uint8_t d = (uint8_t)o->timer;
    if ((int8_t)d >= 0) {
        o->timer = (int8_t)(d + 1);
    } else {
        /* Negative path: player +0x58 bit 0x20, +0x5A & ~7;
         * the +0x4A = 0x808 store is overwritten by the
         * common write below, as in the ROM. */
        o->player_flag20 = true;
        o->player_flags_5a = 0;
    }
    /* Common: +0x4A = ((+0xD >> 3) << 8) | (0x10 - (+0xD >> 3)). */
    unsigned shift = d >> 3;
    o->display_4a = (int)(((shift << 8) | (0x10 - shift)) & 0xFFFF);

    /* +0x14-- ; old == 0 -> sub_08010978 + state 2 (the ROM
     * still stores the wrapped 0xFFFF). */
    if (o->frames_left == 0) {
        o->frames_left = 0xFFFF;
        o->seq_end = true;
        o->state = 2;
    } else {
        o->frames_left--;
    }
}

void aos_object1f_think(AosObject1F *o, bool flags_42c_40,
                        bool probe_ok, bool think_done,
                        bool s8_3f4_neg, bool words_differ) {
    o->think_song = -1;
    o->flash_2000 = false;
    o->flags_42c_40 = false;
    o->flags_42c_20000 = false;
    o->flags_42c_clr41 = false;
    o->flags_a074_2 = false;
    o->flags_a074_clr3 = false;
    o->player_5a_20 = false;
    o->player_5a_clr79 = false;
    o->misc_ops = false;

    switch (o->state) {
    case 0:
        if (flags_42c_40) {
            /* gUnk_03002CB0 |= 0x2000, +0x14 = 0x74, display
             * window regs, player +0x46 = 0x60, -> 5. */
            o->flash_2000 = true;
            o->frames_left = 0x74;
            o->state = 5;
        } else {
            /* gUnk_03002CB0 &= ~0x2000, +0x42C &= ~1,
             * A074 |= 2, -> 1. */
            o->flash_2000 = false;
            o->flags_42c_clr41 = true;   /* &= ~1 window */
            o->flags_a074_2 = true;
            o->state = 1;
        }
        o->timer = 0;
        break;
    case 1:
        if (o->substate == 0) {
            if (flags_42c_40 && probe_ok) {
                /* sub_0800C5A8(0), +0xB = 1, +0xD = 3. */
                o->sound = 0;
                o->substate = 1;
                o->timer = 3;
            }
        } else if (o->substate == 1) {
            if (think_done) {
                /* PlaySong 0xF3, palette/fade helpers,
                 * +0x42C |= 0x20000, A074 &= ~3, +0xD = +0xB
                 * = 0, -> 2, sub_080115D4. */
                o->think_song = 0xF3;
                o->flags_42c_20000 = true;
                o->flags_a074_clr3 = true;
                o->misc_ops = true;
                o->timer = 0;
                o->substate = 0;
                o->state = 2;
            } else if ((uint8_t)(o->timer - 1) == 0xFF) {
                /* +0xD wrapped to 0xFF: sub_0800C5A8(0),
                 * +0xB--. */
                o->sound = 0;
                o->substate--;
            } else {
                o->timer--;
            }
        }
        break;
    case 2:
        if (s8_3f4_neg) {
            o->state = 0;
            o->substate = 0;
        } else {
            /* Both compare outcomes advance; a word mismatch
             * also plays 0x1010 and 0xF9. */
            if (!words_differ) {
                o->state++;
            } else {
                o->think_song = 0xF9;      /* 0x1010 queued too */
                o->misc_ops = true;
                o->substate = 0;
                o->state++;
            }
        }
        break;
    case 3: {
        uint8_t d = o->timer;
        if ((d & 3) != 0) {
            o->player_5a_20 = true;         /* |= 0x20 window */
        } else {
            o->player_5a_clr79 = true;      /* &= ~0x79 window */
        }
        o->timer = (uint8_t)(d + 1);
        if (d > 0x80) {
            /* BG fades + palette ops, +0xD = 0 -> 4. */
            o->misc_ops = true;
            o->timer = 0;
            o->state++;
        }
        break;
    }
    case 4:
        if (o->timer > 0x3E) {
            /* +0x42C |= 0x40, sub_08012048(0x39),
             * sub_080109B8, VRAM DMA setup, player pos copy,
             * sub_0800ECA0, sub_0801083C. */
            o->flags_42c_40 = true;
            o->misc_ops = true;
        }
        o->timer++;
        break;
    case 5:
        /* Palette/fade helpers, fade to black, +0xD = 0,
         * -> 6, player +0x5A &= ~0x79. */
        o->misc_ops = true;
        o->player_5a_clr79 = true;
        o->timer = 0;
        o->state++;
        break;
    case 6:
        if (o->timer > 0x3F) {
            /* Palette helpers, -> 7, +0xD = 0,
             * +0x42C &= ~0x41, sub_08013C5C. */
            o->misc_ops = true;
            o->flags_42c_clr41 = true;
            o->timer = 0;
            o->state++;
        } else {
            o->timer++;
        }
        break;
    case 7:
        break;                                 /* empty */
    case 8:
        /* A074 |= 2, +0xD++; every third frame (old % 3 ==
         * 0) counts +0x14 down; <= 0x23 resets to 0x24 and
         * loops to state 0. */
        o->flags_a074_2 = true;
        uint8_t d = o->timer;
        o->timer = (uint8_t)(d + 1);
        if (d % 3 == 0) {
            /* s16 decrement like the ROM: 0 - 1 wraps to
             * -1 which is <= 0x23 and resets. */
            int16_t left = (int16_t)o->frames_left - 1;
            o->frames_left = (uint16_t)left;
            if (left <= 0x23) {
                o->frames_left = 0x24;
                o->timer = 0;
                o->state = 0;
            }
        }
        break;
    }

    /* Shared tail. */
    if ((uint8_t)(o->state - 3) <= 2) {
        /* States 3-5: A074 &= ~3, sub_0801D028(0x20, 1, 0,
         * -0x2000). */
        o->flags_a074_clr3 = true;
        o->misc_ops = true;
    }
    if ((uint8_t)(o->state - 2) <= 5) {
        /* States 2-7: SetPlayerInput(1, 0). */
        o->input_lock = true;
    }
    if (o->state > 4) {
        /* display +0x38 = 0x5898, +0x3C = +0x14 | 0x2400 —
         * exposed through frames_left; no runtime consumer. */
    }
}
