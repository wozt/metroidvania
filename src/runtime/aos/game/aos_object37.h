#ifndef AOS_OBJECT37_H
#define AOS_OBJECT37_H
#include <stdbool.h>
#include <stdint.h>

/* Object37Create (0x0805A930) / Object37Update (0x0805AE00) /
 * sub_0805AA94 / sub_0805AC50 / sub_0805AD9C / sub_0805AE18,
 * dispatch index 55 (0x37): the proximity fade director.
 *
 * Create runs the sub_0805AE18 sprite attach — tiles
 * 0x08259130 bank 0, palette sub_08068264(0x08259138, 0),
 * frames 0x082591BC, drawFunc sub_0803B9D0 — deletes on load
 * failure or on a negative sub_0803E654(1) roll, sets
 * + 0x65 = 1, + 0x5C = 9, + 0x58 bits 0/6 cleared with bit 5
 * = RNG & 0x1F, + 0x72 bits 1-2, + 0xA = 0 and the affine
 * transform. It then probes the player entity
 * (gEwramData + 0x13110) on the + 0x40 x axis against
 * 0x2FFFFF (~48 px in 16.16):
 *  - NEAR (<=): + 0x18 = 0x3000 (full fade), + 0x1C = 0.
 *  - FAR (>): + 0x1C = 1, five sub_08002200 display
 *    placements, then the sub_0805AC50 type-0x49 child (same
 *    art, + 0x5C = 0xA, + 0x65 = 2, + 0x72 bits 2/4, own RNG
 *    delete roll, + 0x18 = -0x2000 and + 0xB = 1 when the
 *    player is near at its first update).
 *
 * Update runs sub_0805AA94 while + 0xA == 0 — a two-state
 * hysteresis on the same x distance and the player + 0x58
 * bit 0x40:
 *  - state 0 decays + 0x18 by 0x100 (clamped at 0); when the
 *    player is far AND flag 0x40 set -> state 1, PlaySong
 *    0x113.
 *  - state 1 grows + 0x18 by 0x100 (clamped at 0x3000); a
 *    pending + 0x1C runs five sub_08002248 placements; when
 *    the player is far AND flag 0x40 clear -> state 0,
 *    + 0x1C = 1, five sub_08002200 placements, PlaySong
 *    0x113.
 * The tail refreshes the sub_0803E058 affine transform from
 * + 0x58 each frame.
 *
 * Approximations: + 0x18 is exposed as the fade radius the
 * native consumer (likely a lighting mask) would read, the
 * sub_08002200/sub_08002248 display placements become output
 * counts, the player + 0x58 bit 0x40 is an input flag, the
 * child collapses to a counted spawn, the RNG rolls are
 * inputs and there is no emulator comparison. */
typedef struct {
    uint8_t state;          /* +0xB: 0 decay / 1 grow */
    uint8_t flag_a;         /* +0xA: gates the machine */
    uint8_t flag_58;        /* +0x58: RNG variant bits */
    uint8_t anim;           /* +0x5C: 9 */
    uint8_t sel;            /* +0x65: 1 */
    int32_t fade;           /* +0x18: 0..0x3000 */
    int32_t flag_1c;        /* +0x1C: placement pending */
    int32_t pos_x;          /* +0x40: screen x (distance axis) */
    int32_t pos_y;          /* +0x44 */
    bool deleted;           /* sprite or RNG delete */
    /* Per-frame outputs, reset by update. */
    int song;               /* PlaySong id, -1 none */
    unsigned place_out;     /* sub_08002200 placements */
    unsigned place_in;      /* sub_08002248 placements */
    unsigned child_count;   /* create-time sub_0805AC50 child */
} AosObject37;

/* Object37Create. `pos_x`/`pos_y` are the entity + 0x40/+ 0x44
 * screen position, `player_near_x` the |+ 0x40 delta| <=
 * 0x2FFFFF probe, `rng_ok` the sub_0803E654(1) sign and
 * `rng_bits` the RNG & 0x1F written to + 0x58 bit 5.
 * `sprite_ok` is the sub_0805AE18 load result. */
void aos_object37_create(AosObject37 *o, int32_t pos_x,
                         int32_t pos_y, bool player_near_x,
                         bool rng_ok, uint8_t rng_bits,
                         bool sprite_ok, bool *deleted);

/* Object37Update while + 0xA == 0. `player_far_x` is the
 * |+ 0x40 delta| > 0x2FFFFF probe and `player_flag40` the
 * player + 0x58 bit 0x40. */
void aos_object37_update(AosObject37 *o, bool player_far_x,
                         bool player_flag40);

#endif
