/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free tests for the Aria of Sorrow wooden door. */
#include "aos/game/aos_door.h"

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
    return aos_door_update(door, &layer, soma, input, 0, 0);
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
    assert(door.state == 1 && sound == 0x115 && (soma.x >> 16) == 180 && door.holding);
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
    assert(aos_door_update(&door, &layer, &soma, &input, 0, 0) == 0 && door.state == 0);
    soma.facing_left = false;
    soma.flags |= AOS_FLAG_BACKDASH;
    assert(aos_door_update(&door, &layer, &soma, &input, 0, 0) == 0 && door.state == 0);
    soma.flags &= ~(uint32_t)AOS_FLAG_BACKDASH;
    assert(aos_door_update(&door, &layer, &soma, &input, 0, 0) == 0x115 && door.state == 1);

    /* Entering through a door: it starts open, walks Soma into the room,
     * then closes behind him (sound 0x114) and blocks again. */
    memset(blocks, 0, sizeof blocks);
    layer.blocks_active = false;
    soma = aos_soma_spawn(AOS_FIXED(205), AOS_FIXED(159), NULL);
    aos_door_create(&door, &layer, 200, 160, &soma, 0, &input);
    assert(door.state == 10 && door.swing == 0x4000 && input.active && door.holding &&
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
    assert(door.swing == 0 && aos_collision_cell(&layer, 200, 152) == 3 && !door.holding);

    /* Hidden outside the screen widened by 32 pixels on each side and 48
     * below (sub_08068AD4(e, -32, 0, 0x130, 0xD0)). */
    aos_door_update(&door, &layer, &soma, &input, 200 - 272, 0);
    assert(!door.hidden);
    aos_door_update(&door, &layer, &soma, &input, 200 - 273, 0);
    assert(door.hidden);
    aos_door_update(&door, &layer, &soma, &input, 232, 0);
    assert(!door.hidden);
    aos_door_update(&door, &layer, &soma, &input, 233, 0);
    assert(door.hidden);
    aos_door_update(&door, &layer, &soma, &input, 0, 160 - 0xD0);
    assert(!door.hidden);
    aos_door_update(&door, &layer, &soma, &input, 0, 161);
    assert(door.hidden);
    /* sub_0804D794's affine swing: identity at the endpoints, cosine
     * scale and 32*sin - 1 pixel shift in between. */
    AosDoor swing = {.swing = 0, .facing_left = false};
    assert(aos_door_swing_scale(&swing) == 1.0f &&
           aos_door_swing_shift(&swing) == 0.0f);
    swing.swing = 0x4000;
    assert(aos_door_swing_scale(&swing) == 1.0f &&
           aos_door_swing_shift(&swing) == 0.0f);
    swing.swing = 0x2000;   /* half of the quarter turn: cos ~ 0.707 */
    assert(aos_door_swing_scale(&swing) > 0.6f &&
           aos_door_swing_scale(&swing) < 0.8f);
    /* sin(45 deg) * 32 - 1 ~ 21.6 px, negated for the left-facing door. */
    assert(aos_door_swing_shift(&swing) > 20.0f &&
           aos_door_swing_shift(&swing) < 23.0f);
    swing.facing_left = true;
    assert(aos_door_swing_shift(&swing) < -20.0f &&
           aos_door_swing_shift(&swing) > -23.0f);
    /* sub_0804D794's two affine submits: the upper band takes
     * cos(swing), the lower sin(swing); both ~0.707 at half swing,
     * swapped toward the endpoints, and the helper refuses the plain
     * draw endpoints. */
    {
        float top = 0.0f, bottom = 0.0f;
        swing.swing = 0;
        assert(!aos_door_swing_bands(&swing, &top, &bottom));
        swing.swing = 0x2000;
        assert(aos_door_swing_bands(&swing, &top, &bottom));
        assert(top > 0.6f && top < 0.8f && bottom > 0.6f && bottom < 0.8f);
        swing.swing = 0x1000;   /* 22.5 deg: cos ~0.92, sin ~0.38 */
        assert(aos_door_swing_bands(&swing, &top, &bottom));
        assert(top > 0.85f && bottom > 0.3f && bottom < 0.45f);
        swing.swing = 0x3000;   /* 67.5 deg: cos ~0.38, sin ~0.92 */
        assert(aos_door_swing_bands(&swing, &top, &bottom));
        assert(bottom > 0.85f && top > 0.3f && top < 0.45f);
        swing.swing = 0x4000;
        assert(!aos_door_swing_bands(&swing, &top, &bottom));
    }

    /* sub_0803C150's palette script is a per-door instance registered at
     * spawn: palette_age counts the door's own update frames (hidden
     * frames included, like the native entry that ticks regardless of
     * the draw) and resets on create — it is not the run's frame
     * counter, so two doors updated different numbers of frames sit at
     * different phases. */
    memset(blocks, 0, sizeof blocks);
    layer.blocks_active = false;
    soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(159), NULL);
    AosDoor late;
    aos_door_create(&late, &layer, 600, 160, &soma, 0, &input);
    assert(late.palette_age == 0);
    for (int i = 0; i < 5; ++i)
        aos_door_update(&late, &layer, &soma, &input, 0, 0);
    assert(late.palette_age == 5);
    /* A hidden door keeps counting: the native entry ticks every frame. */
    aos_door_update(&late, &layer, &soma, &input, 5000, 0);
    assert(late.hidden && late.palette_age == 6);
    /* The phase walk loops over the script's per-phase ticks. */
    {
        const uint8_t ticks[3] = {7, 7, 7};
        for (uint32_t age = 0; age < 21; ++age)
            assert(aos_door_palette_phase(age, ticks, 3) == (int)(age / 7));
        assert(aos_door_palette_phase(21, ticks, 3) == 0);
        assert(aos_door_palette_phase(0, ticks, 0) == 0);
    }
    puts("aos_door: ok");
    return 0;
}
