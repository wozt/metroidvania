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
                        bool up_pressed, bool probe_ok,
                        bool think_done, bool s8_3f4_neg,
                        bool words_differ) {
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
    o->event_39 = false;

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
            /* inputData.newInput & 0x40 (Up edge) + the probe —
             * the ROM reads + 0x16 here, not the + 0x42C word. */
            if (up_pressed && probe_ok) {
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
            o->event_39 = true;
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

static void child_outputs_reset(AosObject1FChild *c) {
    c->sound = -1;
    c->affine_ops = 0;
    c->anim_sets = 0;
    c->anim_id = 0;
    c->flags_42c_clr41 = false;
    c->record_inc = false;
}

/* The period table shared by the ring decay loops: index <= 3
 * -> 0x1E, <= 5 -> 0x14, else 0xA (sub_08055CB4/sub_08055DF8
 * cases 4); state 8 uses 0 for <= 5 and 5 above. */
static int ring_period(uint8_t idx, bool state8) {
    if (state8) return idx > 5 ? 5 : 0;
    if (idx <= 3) return 0x1E;
    return idx <= 5 ? 0x14 : 0xA;
}

/* The shared decay body: +0x14 -= +0x1A, +0x1A += +0x18, +0xD++
 * and +0x20 += +0x22 once the old timer passes the period; the
 * affine call runs every frame in the dispatcher cases. */
static void ring_decay(AosObject1FChild *c, int period,
                       bool affine) {
    c->life -= c->vel1a;
    c->vel1a += c->vel18;
    uint8_t old = c->timer_d;
    c->timer_d = (uint8_t)(old + 1);
    if (old > period)
        c->acc20 += c->acc22;
    if (affine)
        c->affine_ops++;
}

void aos_object1f_child_ring_init(AosObject1FChild *c,
                                  bool flag42c_40) {
    child_outputs_reset(c);
    c->pos42 = 0x78;
    c->pos46 = 0x48;
    c->state = 1;
    /* +0xD = aux61 * 5 (+0xF when aux61 != 0), read at +0x61. */
    c->timer_d = (uint8_t)(c->aux61 * 5 + (c->aux61 ? 0xF : 0));
    c->life = 0xFFFF;
    c->vel1a = 0xA;
    c->vel18 = 0x3C;
    c->acc20 = 0;
    c->acc22 = 0x800;
    c->affine_ops++;
    c->flags59 &= 0x7F;
    if (flag42c_40)
        c->timer_d = 0;
    /* The gated half runs only while +0x42C bit 0x40 is set;
     * the ROM loops inside the same call. */
    if (!flag42c_40)
        goto rise;
    while (c->life > 0) {
        ring_decay(c, ring_period(c->idx65, false), false);
        if (c->life <= 0)
            break;
    }
rise:
    c->state = 6;
    c->acc22 = -c->acc22;
    c->vel1a = -c->vel1a;
    c->vel18 = -c->vel18;
    c->timer_d = (uint8_t)((9 - c->idx65) * 5 +
                           (c->idx65 ? 0 : 0xF));
    c->affine_ops++;
    c->flags59 |= 0x10;
}

void aos_object1f_child_ring_update(AosObject1FChild *c,
                                    uint8_t record_state,
                                    uint32_t rng, bool flag42c_40) {
    child_outputs_reset(c);
    switch (c->state) {
    case 0:
        aos_object1f_child_ring_init(c, flag42c_40);
        break;
    case 1:
        if (record_state == 3)
            c->state++;
        break;
    case 2:
        if ((uint8_t)(c->timer_d - 1) == 0xFF) {
            c->flags59 |= 0x10;
            c->anchor_x = c->pos42;
            c->anchor_y = c->pos46;
            c->state++;
            c->timer_d = 0;
        } else {
            c->timer_d--;
        }
        break;
    case 3:
        if (c->idx65 == 0) {
            if (c->timer_d & 1) {
                int32_t j = (int32_t)(rng % 3) - 1;
                c->pos42 = (int16_t)(c->pos42 - j);
                c->pos46 = (int16_t)(c->pos46 - j);
            } else {
                c->pos42 = (int16_t)c->anchor_x;
                c->pos46 = (int16_t)c->anchor_y;
            }
        }
        if (c->timer_d++ > 0x50) {
            c->pos42 = (int16_t)c->anchor_x;
            c->pos46 = (int16_t)c->anchor_y;
            c->timer_d = 0;
            c->state++;
        }
        break;
    case 4:
        c->flags59 |= 0x80;
        ring_decay(c, ring_period(c->idx65, false), true);
        if (c->life <= 0)
            c->state++;
        break;
    case 5:
        c->flags58 |= 0x20;
        break;
    case 6:
        if (record_state == 5)
            c->state++;
        break;
    case 7:
        if ((uint8_t)(c->timer_d - 1) == 0xFF) {
            c->flags59 |= 0x10;
            c->flags58 &= (uint8_t)~0x21;
            c->timer_d = 0;
        } else {
            c->timer_d--;
        }
        break;
    case 8:
        c->flags59 |= 0x80;
        ring_decay(c, ring_period(c->idx65, true), true);
        if ((int16_t)c->acc20 == 0) {
            c->state = 0;
            c->record_inc = true;
            c->flags_42c_clr41 = true;
        }
        break;
    }
}

void aos_object1f_child_flank_update(AosObject1FChild *c,
                                     uint8_t record_state,
                                     bool probe_ok, bool poll_done) {
    static const uint8_t anim_table[12] =
        {4, 7, 5, 6, 5, 7, 4, 7, 6, 5, 6, 7}; /* 0x081185F8 */
    child_outputs_reset(c);
    if (c->state == 0) {
        if (probe_ok && poll_done) {
            c->anim_idx = 0;                /* +0x12 = poll result */
            c->state = 1;
        }
        return;
    }
    if (c->state != 1)
        return;
    if (!probe_ok && record_state <= 2) {
        /* Sprite reload (sub_0803C7B4 triple) then back to 0. */
        c->anim_sets++;
        c->state = 0;
        return;
    }
    c->anim_sets++;
    c->anim_id = anim_table[c->anim_idx & 0xB];
    uint8_t old = c->timer_d;
    c->timer_d = (uint8_t)(old + 1);
    if (old % 8 == 0) {                       /* __modsi3(old,8) */
        int8_t next = (int8_t)(c->anim_idx + 1);
        c->anim_idx = next > 0xB ? 0 : next;
    }
}

void aos_object1f_child_free_update(AosObject1FChild *c,
                                    uint8_t record_state,
                                    uint32_t rng) {
    child_outputs_reset(c);
    switch (c->state) {
    case 0:
        if (record_state != 0) {
            c->timer_d = (uint8_t)((rng & 0xF) + 8);
            c->state++;
        }
        break;
    case 1:
        if (c->timer_d != 0) {
            c->timer_d--;
            c->fine44 += -0x10000;          /* -1 px per tick */
        } else {
            c->timer_d = (uint8_t)((rng & 0xF) + 0x10);
            c->state++;
        }
        break;
    case 2:
        c->fine40 += 0x400;
        if ((uint8_t)(c->timer_d - 1) == 0) {
            c->vel48 = -(c->fine40 >> 6);
            c->vel4c = (int32_t)((int64_t)(c->fine40 - 0xA00000) >> 7);
            c->state++;
        } else {
            c->timer_d--;
        }
        break;
    case 3: {
        /* Spring: +0x48/+0x4C chase zero with the +0x14 phase
         * limiting acceleration; positions integrate and the
         * +0x1C s32 lifetime burns 0x200 a tick into state
         * 0x63. */
        int32_t lim = 0x30000 - ((int32_t)c->life << 9);
        if (lim < 0) lim = 0;
        if (c->pos42 <= 0x7F) {
            c->vel48 += 0x4000;
            if (c->vel48 > lim) c->vel48 = lim;
        } else {
            c->vel48 -= 0x4000;
            if (c->vel48 < -lim) c->vel48 = -lim;
        }
        if (c->pos46 <= 0x4F) {
            c->vel4c += 0x3000;
            if (c->vel4c > lim) c->vel4c = lim;
        } else {
            c->vel4c -= 0x3000;
            if (c->vel4c < -lim) c->vel4c = -lim;
        }
        c->life = (c->life + 1) & 0xFFFF;  /* +0x14 strh */
        c->fine40 += c->vel48;
        c->fine44 += c->vel4c;
        c->anchor_x -= 0x200;               /* +0x1C s32 low half */
        if (c->anchor_x == 0) {
            c->state = 0x63;
        }
        break;
    }
    case 0xA:
        c->fine40 += 0x400;
        c->timer_d++;
        if (c->timer_d > 0x2F)
            c->state++;
        break;
    case 0xB:
        c->vel4c += 0x6000;
        c->fine44 += c->vel4c;
        if (c->pos46 > 0x83) {
            if (c->aux37 == 0)
                c->sound = 1;               /* sub_0800C5A8(1) */
            c->pos46 = 0x84;
            c->state = 0;
        }
        break;
    case 0x63:
        c->flags59 |= 0x8;
        /* sub_0803E6BC(+0x17) then b _080564F0 — this case
         * skips the common sub_0803E058 tail. */
        c->affine_ops++;
        return;
    default:
        break;
    }
    /* Common tail: sub_0803E058(+0x1C, +0x17, 0). */
    c->affine_ops++;
}
