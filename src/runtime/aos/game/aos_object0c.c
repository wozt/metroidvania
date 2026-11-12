/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_object0c.h"

#include "aos_object01.h"

/* The 0x085261FC[param0] record lists read from the ROM: three lists,
 * each a run of 0x10-byte records closed by a type-3 terminator. */
typedef struct {
    uint16_t type;      /* +0x00: callback index, 3 terminates */
    uint16_t pad;
    uint32_t param;     /* +0x04 */
    int16_t x, y;       /* +0x08/+0x0A: window origin, room pixels */
    uint16_t w, h;      /* +0x0C/+0x0E: window size */
} AosObject0CRec;

static const AosObject0CRec list0[] = {
    {2, 0, 0x00000000u, 344, 1088, 48, 144},
    {1, 0, 0x00000000u, 376, 160, 64, 928},
    {3, 0, 0x00000000u, 0, 0, 0, 0},
};
static const AosObject0CRec list1[] = {
    {1, 0, 0x00000000u, 88, 400, 40, 272},
    {0, 0, 0x00008000u, 512, 400, 96, 96},
    {1, 0, 0x00000000u, 512, 448, 64, 128},
    {1, 0, 0x00000000u, 672, 592, 48, 96},
    {1, 0, 0x00000000u, 720, 528, 48, 144},
    {1, 0, 0x00000000u, 1152, 400, 48, 224},
    {3, 0, 0x00000000u, 0, 0, 0, 0},
};
static const AosObject0CRec list2[] = {
    {1, 0, 0x00000000u, 88, 752, 40, 112},
    {3, 0, 0x00000000u, 0, 0, 0, 0},
};
static const AosObject0CRec *const lists[3] = {list0, list1, list2};

void aos_object0c_create(AosObject0c *o, int param0) {
    *o = (AosObject0c){0};
    o->param0 = (uint8_t)param0;
    o->claimed = aos_object01_claim_slot();
    o->flags59 = 4;             /* + 0x59 bit 2, set unconditionally */
}

static void callback_type0(AosObject0c *o, const AosObject0CRec *rec,
                           bool soma_flag30) {
    /* sub_08050694: gated on Soma + 0x10 bit 30; entity + 0x48 gets the
     * record param word. */
    if (soma_flag30) o->value48 = rec->param;
}

static void callback_type1(AosObject0c *o, bool soma_flag30) {
    /* sub_080506C0: gated on the same bit; sets Soma + 0x10 |= 0x4000000
     * (AOS_FLAG_HEAVY). Reported through `heavy_set` for the runtime to
     * apply on its Soma flags. */
    if (soma_flag30) o->heavy_set = true;
}

static void callback_type2(AosObject0c *o, const AosObject0CRec *rec,
                           bool gate, int32_t soma_fx, int32_t soma_fy,
                           uint8_t soma_1e) {
    /* sub_08050510: gate = + 0x13260 bit 0x200 OR Soma + 0x10 bit 30. */
    if (!gate) return;
    if (soma_1e) {
        o->soma_1e_forced = true;
        return;
    }
    o->snapshot_armed = true;   /* + 0x131D6 = 1, + 0x131D4 = 3 */
    o->snapshot_x = soma_fx + 0x80000;
    o->snapshot_y = soma_fy;
    /* sub_08021248 writes + 0x131E0/+ 0x131E4 while + 0x131D4 == 3,
     * which the arming above just made true. */
    if (rec->param == 0) {
        o->pan_x = -0x30000;
        o->pan_y = -0x30000;
        o->value48 = (uint32_t)-0x30000;
    } else {
        o->pan_x = 0x30000;
        o->pan_y = -0x30000;
        o->value48 = 0x30000;
    }
}

void aos_object0c_update(AosObject0c *o, uint16_t soma_x, uint16_t soma_y,
                         bool soma_flag30, bool global_13260_200,
                         int32_t soma_fx, int32_t soma_fy, uint8_t soma_1e) {
    o->value48 = 0;         /* + 0x48 cleared at the top of the update */
    int index = o->param0 < 3 ? o->param0 : 0;
    /* The native indexes 0x085261FC by the s16 param0 unchecked; values
     * outside 0-2 would read adjacent data, clamped here to the table. */
    const AosObject0CRec *rec = lists[index];
    for (; rec->type != 3; ++rec) {
        if ((uint16_t)soma_x < (uint16_t)rec->x ||
            (uint16_t)soma_x > (uint16_t)(rec->x + rec->w) ||
            (uint16_t)soma_y < (uint16_t)rec->y ||
            (uint16_t)soma_y > (uint16_t)(rec->y + rec->h))
            continue;
        switch (rec->type) {
        case 0: callback_type0(o, rec, soma_flag30); break;
        case 1: callback_type1(o, soma_flag30); break;
        case 2: callback_type2(o, rec, global_13260_200 || soma_flag30,
                               soma_fx, soma_fy, soma_1e); break;
        default: break;
        }
    }
    if (!(o->flags59 & 4)) {
        o->flags59 |= 8;
        aos_object01_release_slot();
        o->claimed = false;
    }
}
