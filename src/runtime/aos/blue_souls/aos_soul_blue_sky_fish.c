/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_blue_sky_fish.h"

bool aos_sky_fish_guardian_update(AosSkyFishGuardian *fish, AosGuardian *guardian,
                                  AosStats *stats,
                                  const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                                  uint16_t held, uint16_t pressed,
                                  uint32_t *soma_flags, int32_t soma_x, int32_t soma_y,
                                  bool soma_facing_left, uint32_t (*random)(void)) {
    (void)random;
    fish->fx = false;
    fish->song_99 = fish->song_1a8 = fish->song_95 = fish->song_1a2 = false;
    /* + 0x59 bit 2 is set every update by the caller's bookkeeping. */
    /* The upkeep window: the guardian's state 2 or 3 (its + 0x0A - 1
     * in {1, 2}) runs sub_0802DF2C; a source + 0x10 & 0x1A2 also ends
     * it — both land in state 4. */
    if (fish->state >= 2 && fish->state <= 3 &&
        (aos_guardian_upkeep(guardian, stats, records, held, pressed) ||
         (*soma_flags & 0x1A2u)))
        fish->state = 4;
    /* + 0x42/+ 0x46 and + 0x58 bit 6 copy Soma every frame. */
    fish->x = soma_x;
    fish->y = soma_y;
    fish->facing_left = soma_facing_left;
    switch (fish->state) {
    case 0:
        if (*soma_flags & 0x82u) {
            /* _0802F4F6: sub_0802DD84 then EntityDelete. */
            fish->refused = true;
            aos_guardian_end(guardian, soma_flags);
            return false;
        }
        fish->state = 1;
        fish->timer = 0x10;
        fish->song_99 = true;
        break;
    case 1:
        /* Two sub_08045B44 sparkles a frame at random +-16 px offsets
         * around the fish (y - 0x28 above Soma's feet in the native;
         * the position copy already rides Soma). */
        fish->fx = true;
        if (fish->timer == 0) {
            fish->state = 2;
            fish->entered = true;
            fish->song_1a8 = true;
        } else {
            fish->timer--;
        }
        break;
    case 2:
        if (fish->frame % 0x3C == 0) fish->song_95 = true;
        if (fish->frame > 0x12B) {
            fish->state = 4;
        } else {
            fish->frame++;
        }
        break;
    case 3:
        break;
    case 4:
        fish->state = 5;
        fish->timer = 0;
        if (fish->entered) {
            fish->timer = 0xF0;
            if (*soma_flags & 0x20u) fish->song_1a2 = true;
        }
        break;
    case 5:
        if (fish->timer == 0) {
            aos_guardian_end(guardian, soma_flags);
            return false;
        }
        if ((fish->timer & 7) == 0) fish->fx = true;
        fish->timer--;
        break;
    }
    /* The tail: + 0x32 / 2 into gEwramData + 0x131F2 and + 0x32 into
     * + 0x131F8 (their gameplay readers are not ported), then the
     * sub_0803F17C animation step is the caller's. */
    fish->altitude += fish->frame >> 1;
    fish->distance += fish->frame;
    return true;
}
