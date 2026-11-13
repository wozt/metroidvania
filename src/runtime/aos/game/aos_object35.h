#ifndef AOS_OBJECT35_H
#define AOS_OBJECT35_H
#include <stdbool.h>
#include <stdint.h>

/* Object35Create (0x0805A18C) / Object35Update (0x0805A590) /
 * sub_0805A30C / sub_0805A50C / sub_0805A618, dispatch index
 * 53 (0x35).
 *
 * Create loads the sub_0805A618 sprite — tiles 0x081CBF1C,
 * palette sub_08068264(0x0820A3B4, 0), frames 0x08211FB0,
 * drawFunc sub_0803B9D0 — deletes on load failure. With
 * param0 (+ 0x30) == 0 it installs the sub_0804277C(8) hurt
 * slot and the 0x0C10E8F8 collision box, sets + 0x5C = 0x12,
 * + 0x65 = 2 and fires the sub_0805A50C type-0x49 child (same
 * art, + 0x5C = 0x11, + 0x65 = 3, + 0x20 = parent link,
 * inert bx-lr update); with param0 != 0 it skips collision,
 * sets + 0x65 = 1 and starts directly in state 0xA. Both set
 * + 0x72 bits 1-2, + 0xA = 0, + 0x59 bit 1 and clear + 0x58
 * bits 0/6. With param0 == 0 the + 0xB machine starts at 0 —
 * or 2 with a + 0x60000 (6 px) + 0x44 lift when the global
 * bitfield word at gEwramData + 0x344 already has bit
 * (1 << (param1 & 0x1F)) of word (param1 >> 5) set.
 *
 * Update runs sub_08021278(1) only when param0 == 0, then
 * sub_0805A30C while + 0xA == 0. The + 0xB machine:
 *  - state 0 waits for the + 0x13190 linked entity to be
 *    self; + 0x18 counts to 8 then -> 1 with the global bit
 *    set and PlaySong 0x133. Any other link resets + 0x18.
 *  - state 1 integrates + 0x44 += + 0x4C (sub_0806D3CC) at
 *    + 0x4C = 0x10000 and counts + 0xD past 4 -> 2.
 *  - states 2-9 are empty — reached only through external
 *    + 0xB writes.
 *  - state 10 (the param0 != 0 entry) waits for the global
 *    bit, then -> 11 with + 0x4C = 0x8000, PlaySong 0x113.
 *  - state 11 integrates, sets gUnk_03002CB0 bit 15, marks
 *    + 0x1C and every eighth frame (+ 0xD & 7 == 0) fires a
 *    sub_0806D9CC particle with RNG velocities; + 0xD past
 *    0x5F -> 12 with three sub_08002248 placements.
 *  - state 12 clears gUnk_03002CB0 bit 15.
 *
 * Approximations: the sub_0806D9CC particles collapse to
 * counted sparkle effects, the sub_08002200/sub_08002248
 * display placements and the gUnk_03002CB0 flash bit become
 * outputs, the + 0x344 bitfield and + 0x13190 link are
 * inputs, the collision box stays under special_sprites and
 * there is no emulator comparison. */
typedef struct {
    uint8_t state;          /* +0xB: 0-12 */
    uint8_t flag_a;         /* +0xA: gates the machine */
    uint8_t timer;          /* +0xD */
    uint32_t counter;       /* +0x18: linked wait count */
    int32_t vel_y;          /* +0x4C: 16.16 */
    int32_t flag_1c;        /* +0x1C */
    int32_t world_x;        /* +0x10 */
    int32_t world_y;        /* +0x14 */
    int32_t pos_x;          /* +0x40 */
    int32_t pos_y;          /* +0x44: integrated by vel_y */
    int16_t param0;         /* +0x30: 0 actor / !=0 effect */
    int16_t param1;         /* +0x32: global bit index */
    uint8_t anim;           /* +0x5C: 0x12 */
    uint8_t sel;            /* +0x65: 2 or 1 */
    uint8_t flags59;        /* +0x59 */
    uint8_t flags58;        /* +0x58 */
    bool deleted;           /* sprite load failure */
    /* Per-frame outputs, reset by update. */
    int song;               /* PlaySong id, -1 none */
    bool deactivate;        /* sub_08021278 request */
    bool flash;             /* gUnk_03002CB0 bit 15 window */
    unsigned particle_count;/* sub_0806D9CC emissions */
    unsigned child_count;   /* create-time sub_0805A50C child */
} AosObject35;

/* Object35Create. `param0`/`param1` are the + 0x30/+ 0x32
 * selectors, `pos_x`/`pos_y` the entity + 0x40/+ 0x44 and
 * `global_bit`/`global_bit_preset` the + 0x344 bitfield read
 * (preset = already set at create). `sprite_ok` is the
 * sub_0805A618 load result. */
void aos_object35_create(AosObject35 *o, int16_t param0,
                         int16_t param1, int32_t pos_x,
                         int32_t pos_y, bool global_bit,
                         bool global_bit_preset, bool sprite_ok,
                         bool *deleted);

/* Object35Update while + 0xA == 0. `linked_self` is the
 * + 0x13190 == entity check; `global_bit` re-reads the
 * + 0x344 bitfield bit each frame. */
void aos_object35_update(AosObject35 *o, bool linked_self,
                         bool global_bit);

#endif
