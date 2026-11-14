#ifndef AOS_OBJECT1D_H
#define AOS_OBJECT1D_H
#include <stdbool.h>
#include <stdint.h>

/* Object1DCreate (0x080554F0), dispatch index 29 (0x1D).
 * Object1DUpdate is already covered by aos.objects.special_sprites
 * (the shared sub_0803F17C animation step only), so this module
 * ports the create-time variant resolution.
 *
 * Create picks one of two sprite sets by param0 (+ 0x30):
 *  - param0 == 0: palette descriptor 0x082052F0 through
 *    sub_0803AFB8 (bank-0 sub_0803B800 fallback), animation
 *    table 0x08119E14 via sub_0803C7B4(., 0, 1, 0xC) and frames
 *    0x08257BD8 through sub_0803B924 — twelve entries.
 *  - param0 != 0: descriptor 0x082052E8, table 0x0820C384,
 *    frames 0x08257964 — ten entries.
 * It then sets + 0x5C = 0x11 and sums the signed bytes at
 * offset + 4 of the + 0x18 record array (stride 0x10, count
 * from + 0x1C) reached through gEwramData + 0x60 -> + 0x28 —
 * decoded as the current room node's transition table: each
 * 16-byte entry is {target room pointer, s8 source screen X,
 * s8 source screen Y, s16 X adjust, u16 unused, u16 load X,
 * u16 load Y, u16 unused} (the same layout
 * src/assets/aos/world.c reads as the ROM transition table),
 * so byte + 4 is the transition's source screen X and the
 * sum is the total of screen_x over the room's exits — into r3:
 *  - param0 == 0, + 0x65 = 0xB: sum 1 shifts + 0x42 by -0xD0,
 *    sets + 0x14 = 1 and clears + 0x58 bits 0/6; sum -1 sets
 *    + 0x14 = -1 and + 0x58 bit 6; sum 0 deletes the entity.
 *  - param0 != 0, + 0x65 = 0x14: sum 1 sets + 0x42 = 0x10,
 *    + 0x46 = 0x70, + 0x14 = 1 and clears + 0x58 bits
 *    0/4/5/7; sum -1 sets + 0x42 = 0xE0, + 0x46 = 0x70,
 *    + 0x14 = 0 and + 0x58 bit 6; sum 0 deletes.
 * Any other sum keeps the entity fields untouched. The tail
 * fires four sub_08002200 display placements at + 0x42/+ 0x46
 * plus the A094 origin, shifted by -0x10 when + 0x58 bit 6 is
 * clear.
 *
 * Approximations: `sum` is an input; the runtime feeds it the
 * sum of the room transitions' source screen X (the decoded
 * native table). The sub_0803AFB8 palette-table lookup, the
 * sub_0803C7B4/sub_0803B924 sprite attach and the
 * sub_08002200 placements stay under special_sprites / output
 * counts; there is no emulator comparison. */
typedef struct {
    int16_t param0;         /* +0x30: sprite-set selector */
    int8_t facing;          /* +0x14: 1 / -1 / 0 variant */
    uint8_t flags58;        /* +0x58 */
    uint8_t anim;           /* +0x5C: 0x11 */
    uint8_t sel;            /* +0x65: 0xB or 0x14 */
    int16_t off_x;          /* +0x42: placement origin x */
    int16_t off_y;          /* +0x46: placement origin y */
    bool deleted;           /* sum == 0 self-delete */
    /* Per-create outputs. */
    unsigned placements;    /* sub_08002200 x4 */
    int frame_count;        /* 0xC or 0xA sprite entries */
} AosObject1D;

/* Object1DCreate. `pos_x`/`pos_y` seed + 0x42/+ 0x46 before
 * the sum-driven overrides and `sum` is the signed room-record
 * checksum; the sub_0803AFB8 palette lookup falls back to
 * sub_0803B800 bank 0 so there is no fail-delete. */
void aos_object1d_create(AosObject1D *o, int16_t param0,
                         int32_t pos_x, int32_t pos_y, int sum,
                         bool *deleted);

#endif
