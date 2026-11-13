/* SPDX-License-Identifier: GPL-3.0-only */
/* Object20 shop state callbacks from code_08060B98.s:
 * sub_08061ABC (the sell/give sequence) and sub_08061D10
 * (the buy/menu sequence). Both drive the + 0xA state byte,
 * the + 0xB/+ 0xC menu substates, the + 0xD countdown and the
 * text-window polls; native globals are injected as inputs and
 * every engine effect is reported as an output request. */
#ifndef AOS_SHOP_H
#define AOS_SHOP_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t state;      /* +0xA */
    uint8_t menu;       /* +0xB */
    uint8_t cursor;     /* +0xC */
    uint8_t timer;      /* +0xD */
    int32_t vel48;      /* +0x48 slide velocity */
    uint8_t flag6d;     /* +0x6D animation gate */
    uint8_t slot11;     /* +0x11 shop record index */
    int32_t pos40;      /* +0x40 fine X, only the +0x48 slide writes it */
} AosShop;

/* Per-call injected inputs: the results of the native probes the
 * ROM would read from gEwramData, the player entity, the room
 * records and the input state. */
typedef struct {
    bool text_done;         /* sub_08067A04() != 0 */
    bool input_busy;        /* sub_080212C8() & 0x1E (sub_08061D10) */
    int16_t probe;          /* player +0x42 + A094[6] */
    int16_t a094_6;         /* A094[6] alone (> 0x70 branch) */
    int16_t a094_0;         /* A094[0] (room-width record) */
    int8_t dir;             /* sub_0805B814(ent +0x42, 0x3C, 0x64) */
    bool ent_left_of_player;/* ent +0x42 < player +0x42 (+0xB == 2) */
    int32_t room_q3;        /* (GetEntityRoomXPositionInteger e, +7 if
                               negative) >> 3 */
    bool record_left;       /* record +0x40 < player +0x40 (side bit) */
} AosShopInput;

/* Outputs reset at the top of every call; they name the native
 * calls/effects the ROM would perform. */
typedef struct {
    int window_text;        /* sub_08067934 argument, -1 none */
    bool window_poll;       /* sub_08067A04 called */
    bool sound_2b;          /* sub_08013D60(0x2B) held sound */
    bool release_sound_2b;  /* sub_08013E18(0x2B) */
    int b878;               /* sub_0805B878 argument, -1 none */
    int jingle;             /* sub_08012048 argument, -1 none */
    int song;               /* sub_08013CF0 argument, -1 none */
    uint32_t set_42c;       /* bits OR-ed into gEwramData + 0x42C */
    uint32_t clear_42c;     /* bits AND-NOT-ed out of + 0x42C */
    bool write_417;         /* gEwramData + 0x417 byte write */
    uint8_t write_417_val;
    bool write_427;         /* gEwramData + 0x427 byte write */
    uint8_t write_427_val;
    bool transaction;       /* sub_0800ED24(1, arg2, 0) */
    uint32_t transaction_arg2;
    int anim_a, anim_b, anim_c; /* sub_0803F2C8(e, a, b, c), -1 none */
    bool anim_step;         /* sub_0803F17C(e) */
    bool release;           /* sub_0805B66C(1) */
    bool frames_release;    /* sub_08062020 frames + release */
    /* Shop-record side bit (record +0x53C in the menu init,
     * record +0x58 and player +0x58 in the confirm state). */
    bool side_record_53c;
    bool side_record_58;
    bool side_player_58;
    uint8_t andnot_player58;    /* player +0x58 &= ~mask */
    uint8_t andnot_ent58;       /* entity +0x58 &= ~mask */
    int return_code;        /* the ROM's r0 result */
} AosShopOut;

/* sub_08061ABC: returns the ROM result (0, or 0x20 while the
 * state-1 proximity gate holds). */
int aos_shop_sell_update(AosShop *s, const AosShopInput *in, AosShopOut *out);

/* sub_08061D10: returns the ROM result (0, 0x10 or 0x20). */
int aos_shop_buy_update(AosShop *s, const AosShopInput *in, AosShopOut *out);

#endif
