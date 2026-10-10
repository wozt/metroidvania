/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow wooden door; comments name the cvaos routine. */
#include "aos_door.h"

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
        force(input, away_from_door(door));
    } else {
        set_blocks(door, layer, true);
    }
}

/* Object00Update (sound ids: 0x115 opening, 0x114 closing). */
int aos_door_update(AosDoor *door, AosCollision *layer, const AosSoma *soma,
                    AosForcedInput *input) {
    int height = door->state <= 9 ? 2 : 0x28;
    bool touching = near_point(door, (soma->x >> 16) - 0x14, soma->y >> 16, 0x28, height);
    switch (door->state) {
    case 0:
        if (!touching || door->facing_left == soma->facing_left ||
            (soma->abilities & 0x200u) || (soma->flags & AOS_FLAG_BACKDASH))
            return 0;
        force(input, 0);
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
        door->state = 0;
        return 0;
    default:
        return 0;
    }
}
