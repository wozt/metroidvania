/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SOUL_BLUE_SKY_FISH_H
#define AOS_SOUL_BLUE_SKY_FISH_H
#include <stdbool.h>
#include <stdint.h>

#include "../souls/aos_soul.h"

/* SkillSkyFishUse (blue soul 21, toggled, 60 MP per second): the fish
 * that rides Soma's shoulder. Every update copies his position and
 * facing (+ 0x42/+ 0x46, + 0x58 bit 6) and sets + 0x59 bit 2. While
 * the guardian's state is 2 or 3 the upkeep sub_0802DF2C runs and a
 * source + 0x10 & 0x1A2 ends it into state 4. State 0 loads the
 * record and plays 0x99 (refused when the source flags carry 0x82);
 * state 1 spawns two sub_08045B44 sparkles a frame for 0x10 frames of
 * + 0x0D, then state 2 with + 0x30 = 1, gEwramData + 0x131B8 bit 0x800
 * and song 0x1A8; state 2 keeps + 0x1325C bit 0, plays 0x95 every 0x3C
 * frames of + 0x32 and ends into state 4 past 0x12B; state 4 becomes
 * state 5 with + 0x0D = 0xF0 when + 0x30 was set (song 0x1A2 when the
 * source flags carry 0x20) and deletes through sub_0802DD84 once the
 * timer drains, trailing a sparkle every eighth frame. The tail adds
 * + 0x32 / 2 to gEwramData + 0x131F2 and + 0x32 to + 0x131F8 — exposed
 * as accumulators; their readers are not ported. */
enum { AOS_SKY_FISH_CRUISE_FRAMES = 0x12C };

typedef struct {
    int32_t x, y;           /* + 0x40, + 0x44: Soma's position each frame */
    uint8_t state;          /* the guardian's + 0x0A: 0..5 */
    uint8_t timer;          /* + 0x0D: 0x10 intro, 0xF0 linger */
    uint16_t frame;         /* + 0x32: state-2 frame counter */
    bool entered;           /* + 0x30: reached state 2 */
    bool facing_left;       /* + 0x58 bit 6: copied from Soma */
    bool refused;           /* state 0 saw source + 0x10 & 0x82 */
    bool fx;                /* this update spawned sparkles */
    bool song_99, song_1a8, song_95, song_1a2;
    uint32_t altitude;      /* gEwramData + 0x131F2 accumulation */
    uint32_t distance;      /* gEwramData + 0x131F8 accumulation */
} AosSkyFishGuardian;

/* One update after Soma's; `soma_flags` is the source + 0x10 word.
 * Returns false once the entity is deleted (state 5's timer drained). */
bool aos_sky_fish_guardian_update(AosSkyFishGuardian *fish, AosGuardian *guardian,
                                  AosStats *stats,
                                  const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                                  uint16_t held, uint16_t pressed,
                                  uint32_t *soma_flags, int32_t soma_x, int32_t soma_y,
                                  bool soma_facing_left, uint32_t (*random)(void));
#endif
