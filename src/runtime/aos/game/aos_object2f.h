#ifndef AOS_OBJECT2F_H
#define AOS_OBJECT2F_H
#include <stdbool.h>
#include <stdint.h>

/* Object2FCreate (0x08058C1C) / Object2FUpdate (0x08058CD8),
 * dispatch index 47 (0x2F): the skull-eye flame jets. Create
 * deletes the entity when the + 0x37E bitfield bit 2 is set,
 * resolves the sprite through sub_08058CF8 (tiles 0x081CBEEC,
 * palette slot sub_08068264(0x0820A1C4, 9), frames 0x08211844,
 * drawFunc sub_0803B9D0 — exported as special_47), presets
 * sub_0803F2C8(6, 0, 1), steps the animation once, rolls
 * + 0x6F = rng % 6, sets + 0x5C = 0x1A, + 0x59 bit 1 clear,
 * + 0x58 bit 0x20 and + 0xA = 0. The + 0x30 s16 param indexes
 * the 0x08118650 table — x offsets {0x30, 0x78, 0xA8}, y
 * offsets {0x38, 0x48, 0x30} — written to + 0x42/+ 0x46.
 *
 * Update runs sub_08058D44 while + 0xA == 0 then the
 * sub_0803F17C step: while + 0xB == 0 and the + 0x42C bit
 * 0x1000 trigger (the keys<<4 word — R button) is set, + 0xB
 * latches and + 0x58 bit 0x20 clears. */
typedef struct {
    uint8_t anim;           /* sub_0803F17C step count */
    uint8_t stopped;        /* +0xB: the R-trigger latch */
    uint8_t mark;           /* +0x58 bit 0x20 (create-set, cleared on stop) */
    uint16_t anchor_x;      /* +0x42: table x offset by +0x30 param */
    uint16_t anchor_y;      /* +0x46: table y offset by +0x30 param */
    bool deleted;           /* EntityDelete ran (+ 0x37E bit 2) */
} AosObject2F;

/* `special_37e` is the gEwramData + 0x37E bitfield; bit 2 set
 * deletes the entity like the native create does.
 * `param0` is the entity + 0x30 s16 table index (0-2).
 * `r_pressed` is the + 0x42C bit-0x1000 test. */
void aos_object2f_create(AosObject2F *o, uint16_t special_37e,
                          int16_t param0);
void aos_object2f_update(AosObject2F *o, bool r_pressed);
#endif
