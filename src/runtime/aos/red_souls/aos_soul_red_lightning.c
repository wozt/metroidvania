/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_red_lightning.h"

/* sub_08031EA4(e, soma, 0x10, -0x1C). */
static void place(AosLightning *lightning, int32_t soma_x, int32_t soma_y, bool left) {
    lightning->x = (int32_t)((uint32_t)(int16_t)((soma_x >> 16) + (left ? -0x10 : 0x10)) << 16);
    lightning->y = (int32_t)((uint32_t)(int16_t)((soma_y >> 16) - 0x1C) << 16);
}

AosLightning aos_lightning_create(int32_t soma_x, int32_t soma_y, bool soma_facing_left,
                                  bool vflip) {
    AosLightning lightning = {.facing_left = soma_facing_left, .vflip = vflip, .reach = 0x10};
    place(&lightning, soma_x, soma_y, soma_facing_left);
    return lightning;
}

void aos_lightning_update(AosLightning *lightning, uint8_t frame, uint32_t soma_flags,
                          int32_t soma_x, int32_t soma_y, bool soma_facing_left) {
    if (frame > 3) {
        if ((int8_t)lightning->reach >= 0) lightning->reach = (uint8_t)(lightning->reach + 8);
    } else if (soma_flags & 0x100u) {
        place(lightning, soma_x, soma_y, soma_facing_left);
    }
    if (!lightning->palette_reloaded && frame == 8) lightning->palette_reloaded = true;
}

AosBox aos_lightning_box(const AosLightning *lightning) {
    return (AosBox){0, -16, lightning->reach, 32};
}
