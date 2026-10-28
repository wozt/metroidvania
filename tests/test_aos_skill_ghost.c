/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>

#include "../src/runtime/aos_skill_ghost.h"

static uint32_t rolls[] = {0, 0xA0, 0x80, 0x40};
static unsigned draw;
static uint32_t scripted(void) { return rolls[draw++ % 4]; }

int main(void) {
    AosGhostSoul first = aos_ghost_soul_create(0, 100 << 16, 80 << 16, 0);
    AosGhostSoul second = aos_ghost_soul_create(1, 100 << 16, 80 << 16, 0);
    assert(first.angle == (int16_t)-0x7800 && second.angle == (int16_t)-0x1800);
    assert(first.x != 100 << 16 && first.y != 80 << 16);

    AosSkillTarget targets[4] = {
        {true, 240 << 16, 80 << 16, 10},
        {false, 0, 0, 0},
        {true, 110 << 16, 80 << 16, 10},
        {true, 180 << 16, 80 << 16, 10},
    };
    aos_ghost_soul_launch(&first, targets);
    assert(first.state == 2 && first.target == 2 && first.timer == AOS_GHOST_TIMER);
    int16_t angle = first.angle;
    int32_t x = first.x, y = first.y;
    aos_ghost_soul_update(&first, &targets[first.target], scripted);
    assert(first.angle != angle && first.speed == 0x2000 && first.timer == 0x3F);
    assert(first.x != x || first.y != y);

    for (int i = 0; i < 0x3F; ++i)
        aos_ghost_soul_update(&first, &targets[first.target], scripted);
    assert(first.state == 4 && first.speed == 0x40000);
    x = first.x;
    aos_ghost_soul_update(&first, NULL, scripted);
    assert(first.x != x && first.state == 4);

    AosGhostSoul wandering = aos_ghost_soul_create(0, 100 << 16, 80 << 16, 0);
    AosSkillTarget none[4] = {{0}};
    aos_ghost_soul_launch(&wandering, none);
    assert(wandering.state == 3 && wandering.target == -1);
    wandering.x = 100 << 16;
    wandering.y = 80 << 16;
    wandering.angle = 0;
    draw = 0;
    aos_ghost_soul_update(&wandering, NULL, scripted);
    /* Random X is drawn before Y: target (0, 160) turns down-left. */
    assert(draw == 2 && wandering.angle == 0x300 && wandering.timer == 0x3F);
    wandering.struck = true;
    wandering.state = 2;
    wandering.timer = 8;
    aos_ghost_soul_update(&wandering, &targets[0], scripted);
    assert(wandering.state == 4);

    assert(!aos_ghost_soul_outside(&second, 0, 0, 16, 16));
    assert(aos_ghost_soul_outside(&second, 1000, 0, 16, 16));
    puts("aos skill ghost tests passed");
    return 0;
}
