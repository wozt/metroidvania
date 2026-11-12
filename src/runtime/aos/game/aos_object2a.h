#ifndef AOS_OBJECT2A_H
#define AOS_OBJECT2A_H
#include <stdbool.h>
#include <stdint.h>

/* Object2ACreate (0x08057438) / Object2AUpdate (0x08057744),
 * dispatch index 42 (0x2A): a screen-space horizontal oscillator.
 * Create loads the 0x08526510[param1] sprite record (tiles,
 * palette slot sub_08068264(palette, bank), frames — exported as
 * special_42_v00-v03), presets sub_0804277C(8, ...) plus the
 * 0x08118640[param1] attack/hurt record through sub_080428B4 /
 * sub_08042848, runs sub_0803F2C8(record preset, 0, 1), sets
 * + 0x5C = 0x12, + 0xA = 0, + 0x59 &= ~3 and + 0x72 |= 6.
 *
 * Positions live in the bgInfo[1] scroll frame (gEwramData
 * + 0xA094, xPos/yPos 16.16): + 0x10/+ 0x14 start at the entity
 * position plus the scroll, + 0x18 keeps that spawn screen x as
 * target B and + 0x1C = param0 << 16 is target A. + 0x20 records
 * whether target B sits right of target A. The epilogue writes
 * the world draw position + 0x40/+ 0x44 = accumulator minus the
 * current scroll, so the sprite stays screen-locked while the
 * camera moves.
 *
 * sub_08057538 (run while + 0xA == 0) is a four-state ping-pong:
 * state 0 accelerates + 0x48 toward target A (+ 0x1C) and state 2
 * toward target B (+ 0x18). Far from the target (> 0xFFFFF) the
 * velocity ramps by 0x800 a frame against a ~1.0 px/frame cap;
 * inside it the velocity becomes proportional to the remaining
 * distance ((px + 1) * 0x1000 in the + 0x20 == 0 direction,
 * -(px + 1) * 0x1000 mirrored). Reaching the target enters the
 * paired wait state (1 after 0, 3 after 2), zeroes + 0x48, reloads
 * the u8 + 0xD timer from the record's + 0xE halfword and toggles
 * + 0x20 on the way out, so the sprite oscillates forever.
 * sub_0802138C fires when the entity occupies the + 0x13190 focus
 * slot. */
typedef struct {
    int32_t x_acc, y_acc;   /* +0x10/+0x14: scroll-space position */
    int32_t target_a;       /* +0x1C: param0 << 16 screen x */
    int32_t target_b;       /* +0x18: spawn screen x */
    int32_t vel;            /* +0x48: horizontal velocity */
    int32_t out_x, out_y;   /* +0x40/+0x44: world draw position */
    uint8_t state;          /* +0xB: 0/2 approach, 1/3 wait */
    uint8_t timer;          /* +0xD: u8 wait counter */
    uint8_t wait;           /* record +0xE halfword, truncated u8 */
    uint8_t dir;            /* +0x20: target-B-right-of-A flag */
    uint8_t variant;        /* +0x32 param1: 0x08526510 record index */
    uint8_t preset;         /* record +0xC: sub_0803F2C8 argument */
    uint8_t pal_bank;       /* record +0xD: sub_08068264 bank */
    uint32_t hitbox_word;   /* 0x08118640[param1] attack record */
    bool valid;             /* record index in range (load would pass) */
    unsigned attacks;       /* sub_080421AC slot reads approximated */
    unsigned focus_calls;   /* sub_0802138C focus registrations */
} AosObject2A;

/* `spawn_x`/`spawn_y` are the entity's 16.16 world position,
 * `scroll_x`/`scroll_y` the bgInfo[1] xPos/yPos at create;
 * `param0` is the s16 + 0x30 target, `param1` the s16 + 0x32
 * record index. */
void aos_object2a_create(AosObject2A *o, int16_t param0, int16_t param1,
                         int32_t spawn_x, int32_t spawn_y,
                         int32_t scroll_x, int32_t scroll_y);

/* `scroll_x`/`scroll_y` are the current bgInfo[1] positions;
 * `focus` approximates the + 0x13190 slot match and `attack` the
 * sub_080421AC armed-slot read. */
void aos_object2a_update(AosObject2A *o, int32_t scroll_x,
                         int32_t scroll_y, bool focus, bool attack);
#endif
