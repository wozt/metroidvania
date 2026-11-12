#include "aos_object36.h"

/* 0x081186A4: nine 4-byte records {s16 a, s16 b} read through
 * the entity + 0x30 s16 param scaled by 4. */
static const int16_t gate_records[9][2] = {
    {128, 90}, {128, -60}, {128, 90},
    {96, -90}, {128, 120}, {128, 90},
    {128, -90}, {128, 90}, {96, -60},
};

void aos_object36_create(AosObject36 *o, int16_t param0,
                          bool slot_busy) {
    unsigned idx = param0 >= 0 && param0 < 9 ? (unsigned)param0 : 0;
    int16_t a = gate_records[idx][0];
    o->scale = (uint16_t)(a / 2);       /* trunc toward zero */
    o->speed = gate_records[idx][1];
    o->extent = (uint16_t)(0x200 - a * 2);
    o->pos = 0;
    o->anchor_x = 0;
    o->anchor_y = 0;
    o->registered = !slot_busy;         /* sub_08022088 result */
}

void aos_object36_update(AosObject36 *o, uint16_t block[9]) {
    /* The native 0x14-byte stack image is two u32 0x8000 fields
     * (halfwords {0x8000, 0} each in little-endian), then
     * + 0x42, + 0x46, + 0x1C twice and the low half of + 0x14;
     * the += on + 0x14 runs after BgAffineSet. */
    block[0] = 0x8000; block[1] = 0;
    block[2] = 0x8000; block[3] = 0;
    block[4] = o->anchor_x;
    block[5] = o->anchor_y;
    block[6] = o->extent;
    block[7] = o->extent;
    block[8] = (uint16_t)o->pos;
    o->pos += o->speed;
}
