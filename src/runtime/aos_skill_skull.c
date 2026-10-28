/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_skill_skull.h"

#include "aos_audio.h"

AosSkull aos_skull_create(bool soma_facing_left) {
    return (AosSkull){.dx = (int16_t)(soma_facing_left ? -0x14 : 0x14), .dy = -0x10,
                      .facing_left = soma_facing_left, .state = 1};
}

bool aos_skull_update(AosSkull *skull, int32_t soma_x, int32_t soma_y, uint8_t frame,
                      uint8_t tick) {
    if (skull->state == 2) {
        aos_audio_release(AOS_SKULL_BEAM_SOUND);
        return false;
    }
    /* Only the integer halves are written (strh to + 0x42, + 0x46). */
    skull->x = (int32_t)((uint32_t)(int16_t)((soma_x >> 16) + skull->dx) << 16);
    skull->y = (int32_t)((uint32_t)(int16_t)((soma_y >> 16) + skull->dy) << 16);
    if (frame == 10 && tick == 0) aos_audio_hold(AOS_SKULL_BEAM_SOUND);
    return true;
}

void aos_skull_end(AosSkull *skull) { skull->state = 2; }
