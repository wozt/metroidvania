/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria save-room ambient children extracted from the
 * sub_08054E38/sub_08054F74 spawners and the sub_08055070/
 * sub_0805514C/sub_080552C4/sub_080554BC updates that
 * Object1CUpdate installs on its type-0x1B/0x49 children. */
/* Integration status: faithful native-routine port kept as a
 * parity reference. It is unit-tested but NOT linked into
 * fusion_aria_runtime; classified partial in
 * data/native_parity/annotations.tsv until a real gameplay
 * path consumes it. */

#ifndef AOS_SAVE_ROOM_FX_H
#define AOS_SAVE_ROOM_FX_H

#include <stdbool.h>
#include <stdint.h>

enum { AOS_SAVE_FX_ORBITERS = 8, AOS_SAVE_FX_TWinkles = 2 };

/* One type-0x1B ambience child. Fields follow the entity
 * offsets the ROM routines touch. */
typedef struct {
    uint8_t state;          /* +0xA */
    uint8_t timer_d;        /* +0xD */
    uint8_t aux6d;          /* +0x6D parent phase byte */
    uint8_t anim5c;         /* +0x5C sprite/anim arg */
    uint8_t flags59;        /* +0x59 */
    uint8_t flags58;        /* +0x58 */
    uint8_t kind;           /* which update family: 0 orbit A,
                             * 1 orbit B, 2 particle */
    int32_t pos40;          /* +0x40 fine x */
    int32_t pos44;          /* +0x44 fine y */
    int32_t vel48;          /* +0x48 */
    int32_t vel4c;          /* +0x4C */
    int32_t acc50;          /* +0x50 */
    int32_t acc54;          /* +0x54 */
    int32_t phase24;        /* +0x24 orbit angle/period */
    int32_t phase28;        /* +0x28 orbit radius/step */
    int32_t phase2c;        /* +0x2C */
    int32_t aux20;          /* +0x20 angle index byte */
    int32_t link1c;         /* +0x1C parent pointer token */
    /* outputs */
    unsigned sparkle_bursts;/* sub_080553A0 calls this frame */
    bool parent_mark32;     /* parent +0x32 = 7 request */
    bool parent_mark33;     /* parent +0x33 = 1 request */
} AosSaveRoomFx;

/* sub_08055070: first ambience child — waits on the parent
 * +0x24 bit 1, plays the +0x6D-picked animation and marks
 * +0x59 bit 3 when the parent's +0x30 record reads 1. */
void aos_save_fx_orbit_a_update(AosSaveRoomFx *c,
                                uint8_t parent_phase6d,
                                uint8_t parent_rec30);

/* sub_0805514C: second ambience child — the two-state orbit
 * with the sub_080009E4 sine/cosine lookup approximated by
 * `sin_cos` (radians -> [-256,255] like the ROM's 8.8 table),
 * the camera deltas `cam_dx`/`cam_dy` and the +0x24 period
 * decay; emits sparkle bursts every fourth tick. */
void aos_save_fx_orbit_b_update(AosSaveRoomFx *c,
                                int32_t cam_dx, int32_t cam_dy,
                                int32_t (*sin_cos)(int32_t angle));

/* sub_080552C4: sparkle particle — state 0 attaches the
 * 0x082052E8/0x08257964 triple and falls for 0xC ticks,
 * state 1 integrates position/velocity, state 2 drifts and
 * marks +0x59 bit 3. */
void aos_save_fx_particle_update(AosSaveRoomFx *c);

/* sub_080554BC: installed as the particle's + 4 callback —
 * oscillates +0x40/+0x44 by ±velocities around two
 * sub_0803AC40 animation calls. */
void aos_save_fx_particle_oscillate(AosSaveRoomFx *c);

/* sub_080553A0 + sub_08055414: scatter ONE sparkle particle
 * inside the +/- `spread` box around (x, y) — the ROM calls
 * this every fourth tick of orbit_b. Fills `out` with the
 * type-0x49 particle fields (velocities, palette nibble,
 * parent link); returns false when the entity table is
 * full (approximated as always true here). */
bool aos_save_fx_sparkle_spawn(AosSaveRoomFx *out,
                               int32_t x, int32_t y,
                               int32_t spread, uint32_t rng,
                               uint8_t palette_nib);

#endif
