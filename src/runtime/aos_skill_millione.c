/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_skill_millione.h"

#include "aos_audio.h"

AosMillioneSoul aos_millione_soul_create(void) {
    return (AosMillioneSoul){0};
}

bool aos_millione_soul_update(AosMillioneSoul *soul, int32_t soma_x,
                              int32_t soma_y, bool soma_facing_left,
                              AosMillioneSpawn *spawn) {
    if (spawn) *spawn = (AosMillioneSpawn){0};
    if (!soul || soul->state == AOS_MILLIONE_REMOVED) return false;
    if (soul->state == AOS_MILLIONE_ENDING) {
        soul->state = AOS_MILLIONE_REMOVED;
        return false;
    }
    if (!soul->started) {
        soul->started = true;
        soul->state = AOS_MILLIONE_ACTIVE;
        aos_audio_play_song(0xC9);
    }

    int lead = 20 + soul->offset;
    soul->facing_left = soma_facing_left;
    soul->x = (int32_t)((uint32_t)(int16_t)((soma_x >> 16) +
                                             (soma_facing_left ? -lead : lead))
                        << 16);
    soul->y = (int32_t)((uint32_t)(int16_t)((soma_y >> 16) - 20) << 16);
    soul->offset++;
    soul->interval++;
    if (soul->interval > 1) {
        soul->interval = 0;
        if (spawn)
            *spawn = (AosMillioneSpawn){soul->x, soul->y, soul->facing_left, true};
        soul->emitted++;
        if (soul->emitted > AOS_MILLIONE_CHILDREN - 1)
            soul->state = AOS_MILLIONE_ENDING;
    }
    return true;
}
