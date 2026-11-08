/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_LEVELUP_H
#define AOS_LEVELUP_H

#include <stdbool.h>
#include <stdint.h>

/* sub_080455BC / sub_08045454: the level-up display. A dedicated entity
 * (slots 0x16-0x19, update EntityUpdateNothing, drawFunc sub_08045454)
 * shows still frame 0x48 of tileset 0x081C15F4 on OBJ row 6 (palette
 * bank 2 through sUnk_084F0DD8) at the player's x, y - 0x20, spinning
 * and scaling in for the first 0x10 frames, then holding for the
 * record's 0x3C-frame duration. Frame counter +0xD lives in the draw
 * pass; unk_A074_0 freezes every entity update from counter 2 until
 * the entity releases its OAM slot. PlaySong(0xBC) fires at creation
 * (character index 0). On odd counters <= 0x27 sub_08045270 bursts a
 * storm sparkle (sub_080451F4 art, palette row 6) from the player. */

enum { AOS_LEVELUP_DURATION = 0x3C };

typedef struct {
    uint8_t frame;      /* entity +0xD */
    uint8_t env_frame;  /* the +0xD value the running draw uses */
    bool active;
} AosLevelUp;

/* sub_080009E4, injected so the module stays ROM-free in tests. */
typedef int32_t (*AosLevelUpSin)(uint32_t angle);

void aos_levelup_start(AosLevelUp *display);

/* One native draw pass: snapshot the envelope frame, advance +0xD
 * (only while <= duration, like the native increment guard), and
 * report whether this pass bursts a sub_08045270 sparkle. When the
 * counter passes the duration the display ends. */
bool aos_levelup_tick(AosLevelUp *display);

/* unk_A074_0: entity updates are suspended while the counter sits in
 * [2, duration]. */
bool aos_levelup_frozen(const AosLevelUp *display);

/* The affine arguments of the frame's sub_0803E058 call, in Q16.16:
 * angle() is the rotation in turns (0x10000 per turn), scale() the
 * uniform scale (0x10000 = unit, the sub_0803E86C identity). */
int32_t aos_levelup_angle(uint8_t frame, AosLevelUpSin sine);
int32_t aos_levelup_scale(uint8_t frame, AosLevelUpSin sine);

/* sub_08045270's launch: the random byte becomes angle (byte << 8),
 * velocity = sine * 24 >> 4 (truncating) on (cos, sin), acceleration
 * = -velocity >> 6 with the same rounding, all Q16.16. out = vx, vy,
 * ax, ay. */
void aos_levelup_sparkle_velocity(uint8_t random_byte, AosLevelUpSin sine,
                                  int32_t out[4]);

#endif /* AOS_LEVELUP_H */
