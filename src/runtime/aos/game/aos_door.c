/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow wooden door; comments name the cvaos routine. */
#include "aos_door.h"

#include "../math/aos_math.h"

#include <math.h>

/* sub_08068AD4: (u16)(entity - point) within the extent on both axes. */
static bool near_point(const AosDoor *door, int32_t x, int32_t y, int w, int h) {
    return (uint16_t)(door->x - x) <= w && (uint16_t)(door->y - y) <= h;
}

/* The door's three blocks at y - 8, y - 24, y - 40. */
static void set_blocks(const AosDoor *door, AosCollision *layer, bool solid) {
    for (int dy = 8; dy <= 40; dy += 16)
        aos_collision_set_block(layer, door->x, door->y - dy, solid);
}

static void force(AosForcedInput *input, uint16_t held) {
    input->active = true;
    input->held = held;
}

/* Walking out of the door into the room, or through it. */
static uint16_t away_from_door(const AosDoor *door) {
    return door->facing_left ? AOS_KEY_LEFT : AOS_KEY_RIGHT;
}

static uint16_t through_door(const AosDoor *door) {
    return door->facing_left ? AOS_KEY_RIGHT : AOS_KEY_LEFT;
}

/* Object00Create and sub_0804D8F0's facing. */
void aos_door_create(AosDoor *door, AosCollision *layer, int32_t x, int32_t y,
                     const AosSoma *soma, int32_t camera_x, AosForcedInput *input) {
    *door = (AosDoor){.x = x, .y = y, .facing_left = x - camera_x > 0x78};
    if (near_point(door, (soma->x >> 16) - 0x14, soma->y >> 16, 0x28, 0x28)) {
        /* Soma enters through this door: it starts open. */
        door->swing = 0x4000;
        door->state = 10;
        door->holding = true;
        force(input, away_from_door(door));
    } else {
        set_blocks(door, layer, true);
    }
}

/* Object00Update (sound ids: 0x115 opening, 0x114 closing). sub_0803C150
 * advances every registered palette script once per frame; the door's
 * instance was registered at spawn (sub_0804D8F0's sub_0806E0D0 call),
 * so its phase counts the door's own update frames, not the run's
 * frame counter. */
int aos_door_update(AosDoor *door, AosCollision *layer, const AosSoma *soma,
                    AosForcedInput *input, int32_t camera_x, int32_t camera_y) {
    ++door->palette_age;
    /* sub_08068AD4(e, -32, 0, 0x130, 0xD0) on the screen position. */
    door->hidden = !((uint16_t)(door->x - camera_x + 0x20) <= 0x130 &&
                     (uint16_t)(door->y - camera_y) <= 0xD0);
    int height = door->state <= 9 ? 2 : 0x28;
    bool touching = near_point(door, (soma->x >> 16) - 0x14, soma->y >> 16, 0x28, height);
    switch (door->state) {
    case 0:
        if (!touching || door->facing_left == soma->facing_left ||
            (soma->abilities & 0x200u) || (soma->flags & AOS_FLAG_BACKDASH))
            return 0;
        force(input, 0);
        door->holding = true;
        door->state = 1;
        return 0x115;
    case 1:
        force(input, 0);
        door->swing += 0x100;
        if (door->swing > 0x3FFF) {
            set_blocks(door, layer, false);
            door->state = 2;
        }
        return 0;
    case 2:
        force(input, through_door(door));
        return 0;
    case 10:
        force(input, away_from_door(door));
        if (touching) return 0;
        door->state = 11;
        return 0x114;
    case 11:
        force(input, 0);
        door->swing -= 0x1A0;
        if (door->swing > 0) return 0;
        door->swing = 0;
        set_blocks(door, layer, true);
        door->holding = false;
        door->state = 0;
        return 0;
    default:
        return 0;
    }
}

/* sub_0803C150's entry walk: the phase index whose tick span contains
 * `age` frames, looping over the script's phases. */
int aos_door_palette_phase(uint32_t age, const uint8_t *ticks, int count) {
    unsigned total = 0;
    for (int k = 0; k < count; ++k) total += ticks[k];
    if (!total || count <= 0) return 0;
    uint32_t t = age % total;
    int k = 0;
    while (k + 1 < count && t >= ticks[k]) t -= ticks[k++];
    return k;
}

/* sub_0804D794: the swing angle feeds sub_0803E058's affine matrix —
 * horizontal scale is the cosine (sine of swing + 0x4000) and the +0x40
 * screen-x adjust is sine(swing) * 32 - 1 pixels, negated on one facing
 * (the +0x58 bit 6 the drawFunc toggles). */
float aos_door_swing_scale(const AosDoor *door) {
    if (door->swing <= 0 || door->swing >= 0x4000) return 1.0f;
    return (float)aos_sine((uint32_t)door->swing + 0x4000) / 65536.0f;
}

float aos_door_swing_shift(const AosDoor *door) {
    if (door->swing <= 0 || door->swing >= 0x4000) return 0.0f;
    float shift = (float)aos_sine((uint32_t)door->swing) * 32.0f / 65536.0f - 1.0f;
    return door->facing_left ? -shift : shift;
}

bool aos_door_swing_bands(const AosDoor *door, float *top, float *bottom) {
    if (door->swing <= 0 || door->swing >= 0x4000) return false;
    *top = (float)aos_sine((uint32_t)door->swing + 0x4000) / 65536.0f;
    *bottom = (float)aos_sine((uint32_t)door->swing) / 65536.0f;
    return true;
}
