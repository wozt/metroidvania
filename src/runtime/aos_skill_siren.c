/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_skill_siren.h"

#include "aos_enemy.h"

AosSirenSoul aos_siren_soul_create(int32_t soma_x, int32_t soma_y, bool soma_facing_left,
                                   uint32_t variant_random, uint32_t phase_random) {
    uint8_t variant = (uint8_t)(variant_random & 1);
    return (AosSirenSoul){
        .x = (int32_t)((uint32_t)(int16_t)((soma_x >> 16) + (soma_facing_left ? -8 : 8))
                       << 16),
        .y = (int32_t)((uint32_t)(int16_t)((soma_y >> 16) - 0x1A) << 16),
        .vx = soma_facing_left ? -0x40000 : 0x40000,
        .phase = (uint16_t)phase_random,
        .state = 1,
        .frame = (uint8_t)(AOS_SIREN_FRAME_FIRST + variant),
        .song = (uint16_t)(AOS_SIREN_SOUND_FIRST + variant),
    };
}

bool aos_siren_soul_update(AosSirenSoul *note, int cam_x, int cam_y, int mx, int my) {
    if (note->state == 2) return false;
    note->vy = aos_sine(note->phase);
    note->phase = (uint16_t)(note->phase + 0x400);
    note->x += note->vx;
    note->y += note->vy;
    int sx = (note->x >> 16) - cam_x, sy = (note->y >> 16) - cam_y;
    if (note->struck || sx < -mx || sx > mx + 0xF0 || sy < -my || sy > my + 0xA0)
        note->state = 2;
    return true;
}
