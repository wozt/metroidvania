#ifndef AOS_OBJECT36_H
#define AOS_OBJECT36_H
#include <stdbool.h>
#include <stdint.h>

/* Object36Create (0x0805A6C8) / Object36Update (0x0805A678),
 * dispatch index 54 (0x36): the metal gate and button. Create
 * ORs + 0x72 bits 1-2 and + 0x58 bit 0x20, reads the entity
 * + 0x30 s16 param as a 4-byte record index into the dumped
 * 0x081186A4 table (9 records of {s16 a, s16 b}): + 0x18 =
 * a / 2, + 0x1A = b and + 0x1C = 0x200 - a * 2. It then calls
 * sub_08022088(ent, 0x0811868C), which stores the entity in
 * the singleton slot gEwramData + 0x1316C and the callback
 * table pointer at + 0x10 — the gate registers itself as the
 * active interactive object (returns 0, ignored, when the
 * slot is busy).
 *
 * Update builds a 0x14-byte BgAffineSet parameter block on
 * layer 0x1A: {0x8000, 0x8000, + 0x42, + 0x46, + 0x1C,
 * + 0x1C, low16(+ 0x14)} then advances the 32-bit + 0x14
 * accumulator by the s16 + 0x1A — the gate's BG scroll
 * position and speed, both drivable by the callback table
 * the button writes through the + 0x1316C slot. */
typedef struct {
    int32_t pos;            /* +0x14: BG scroll accumulator (low16 used) */
    int16_t speed;          /* +0x1A: per-frame scroll step */
    uint16_t scale;         /* +0x18: record a / 2 */
    uint16_t extent;        /* +0x1C: 0x200 - record a * 2 */
    uint16_t anchor_x;      /* +0x42: affine block texture x */
    uint16_t anchor_y;      /* +0x46: affine block texture y */
    bool registered;        /* sub_08022088 stored the + 0x1316C slot */
} AosObject36;

/* `param0` is the entity + 0x30 s16 record index (0-8);
 * out-of-range params clamp like a defensive index (the
 * native code does not bounds-check). `slot_busy` reports the
 * sub_08022088 busy-slot failure. */
void aos_object36_create(AosObject36 *o, int16_t param0,
                          bool slot_busy);
/* Fills `block` with the 9 halfwords BgAffineSet receives
 * (two u32 0x8000 fields first) and advances pos by speed,
 * matching the native update order (block first, then +=). */
void aos_object36_update(AosObject36 *o, uint16_t block[9]);
#endif
