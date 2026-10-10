/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free tests for the Aria of Sorrow wooden door. */
#include "aos_door.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { W = 64, H = 32 };
static uint8_t cells[W * H];
static uint8_t blocks[(W / 2) * (H / 2)];
static AosCollision layer = {2, 1, W, H, cells, blocks, false};

/* One frame: forced input replaces the player's, then the door runs. */
static int step(AosSoma *soma, AosDoor *door, AosForcedInput *input, uint16_t user,
                uint16_t *previous) {
    uint16_t held = input->active ? input->held : user;
    input->active = false;
    aos_soma_update(soma, &layer, held, (uint16_t)(held & ~*previous));
    *previous = held;
    return aos_door_update(door, &layer, soma, input);
}

int main(void) {
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;  /* floor top 160 */
    AosForcedInput input = {0};
    uint16_t previous = 0;

    /* A closed door at x 200 blocks the three 16x16 blocks above y 160. */
    AosSoma soma = aos_soma_spawn(AOS_FIXED(150), AOS_FIXED(159), NULL);
    AosDoor door;
    aos_door_create(&door, &layer, 200, 160, &soma, 0, &input);
    assert(door.state == 0 && door.facing_left && !input.active);
    assert(aos_collision_cell(&layer, 200, 152) == 3 && aos_collision_cell(&layer, 200, 120) == 3);
    assert(aos_collision_cell(&layer, 200, 100) == 0);

    /* Walking right reaches the door, which then opens: input locked while
     * it swings for 64 frames, then Soma is walked through. */
    int sound = 0, frames = 0;
    while (door.state == 0 && frames < 100) {
        sound = step(&soma, &door, &input, AOS_KEY_RIGHT, &previous);
        ++frames;
    }
    /* Contact starts 20 pixels before the door, before the blocks stop him. */
    assert(door.state == 1 && sound == 0x115 && (soma.x >> 16) == 180);
    int opening = 0;
    while (door.state == 1) {
        int32_t x = soma.x;
        step(&soma, &door, &input, AOS_KEY_LEFT, &previous);
        assert(soma.x >= x - 0x4000);      /* the player cannot walk away */
        ++opening;
    }
    assert(opening == 64 && door.state == 2 && aos_collision_cell(&layer, 200, 152) == 0);
    for (int i = 0; i < 20; ++i) step(&soma, &door, &input, AOS_KEY_LEFT, &previous);
    assert((soma.x >> 16) > 200 && !soma.facing_left);

    /* Backdashing into a door does not open it, nor does facing away. */
    memset(blocks, 0, sizeof blocks);
    soma = aos_soma_spawn(AOS_FIXED(100), AOS_FIXED(159), NULL);
    aos_door_create(&door, &layer, 200, 160, &soma, 0, &input);
    soma.x = AOS_FIXED(183);
    soma.facing_left = true;
    assert(aos_door_update(&door, &layer, &soma, &input) == 0 && door.state == 0);
    soma.facing_left = false;
    soma.flags |= AOS_FLAG_BACKDASH;
    assert(aos_door_update(&door, &layer, &soma, &input) == 0 && door.state == 0);
    soma.flags &= ~(uint32_t)AOS_FLAG_BACKDASH;
    assert(aos_door_update(&door, &layer, &soma, &input) == 0x115 && door.state == 1);

    /* Entering through a door: it starts open, walks Soma into the room,
     * then closes behind him (sound 0x114) and blocks again. */
    memset(blocks, 0, sizeof blocks);
    layer.blocks_active = false;
    soma = aos_soma_spawn(AOS_FIXED(205), AOS_FIXED(159), NULL);
    aos_door_create(&door, &layer, 200, 160, &soma, 0, &input);
    assert(door.state == 10 && door.swing == 0x4000 && input.active &&
           input.held == AOS_KEY_LEFT);
    assert(aos_collision_cell(&layer, 200, 152) == 0);
    frames = 0;
    sound = 0;
    while (door.state == 10 && frames < 100) {
        sound = step(&soma, &door, &input, 0, &previous);
        ++frames;
    }
    assert(door.state == 11 && sound == 0x114 && (soma.x >> 16) < 180);
    while (door.state == 11) step(&soma, &door, &input, AOS_KEY_RIGHT, &previous);
    assert(door.swing == 0 && aos_collision_cell(&layer, 200, 152) == 3);
    puts("aos_door: ok");
    return 0;
}
