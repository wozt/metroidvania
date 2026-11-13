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
