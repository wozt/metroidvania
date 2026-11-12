/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_object01.h"

/* sUnk_080E1DA8 @ 0x080E1DA8: count 2, records at 0x080E1D90 /
 * 0x080E1D98 — {u8 count, u16 xy[count]} read from the ROM. */
typedef struct { uint8_t count; int16_t xy[6]; } AosObject01Record;
static const AosObject01Record records[2] = {
    {1, {0xE0, 0xBF}},
    {3, {0x80, 0xC0, 0x80, 0xA0, 0x100, 0xC0}},
};

static bool slot_held;    /* gEwramData + 0x1316C != NULL */

void aos_object01_reset(void) { slot_held = false; }

bool aos_object01_claim_slot(void) {
    if (slot_held) return false;
    slot_held = true;
    return true;
}

void aos_object01_release_slot(void) { slot_held = false; }

void aos_object01_create(AosObject01 *o, int16_t param0) {
    *o = (AosObject01){.param0 = param0};
    /* sub_08022088 claims the slot; temp_r4 <= 1 bounds the record
     * index (negative or larger param0 fails like the native cmp). */
    if (param0 < 0 || param0 > 1 || !aos_object01_claim_slot()) return;
    o->claimed = true;
    const AosObject01Record *rec = &records[param0];
    o->child_count = rec->count;
    for (int i = 0; i < rec->count; ++i) {
        /* child + 0x526/+ 0x52A = coord - BG1 origin: screen pixels.
         * Room pixels keep the same value (world = screen + camera). */
        o->child_x[i] = rec->xy[i * 2];
        o->child_y[i] = rec->xy[i * 2 + 1];
    }
}

void aos_object01_update(AosObject01 *o) {
    if (o->claimed) return;         /* unk_53D_2 set: update is inert */
    /* sub_08033C18: mark the (empty) children, unk_53D_3 = 1 on the
     * parent, then sub_080220B4 releases the slot unconditionally. */
    o->deleted = true;
    aos_object01_release_slot();
}
