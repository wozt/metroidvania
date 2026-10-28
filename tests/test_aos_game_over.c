/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_game_over.h"

#include "aos_game_mode.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    /* sub_0803E8BC: 0x10000 over 0x20 frames steps 0x800; signs keep. */
    assert(aos_fixed_divide(0x10000, 0x20 << 16) == 0x800);
    assert(aos_fixed_divide(-0x10000, 0x40 << 16) == -0x400);
    assert(aos_fixed_divide(0x1234, 0x10000) == 0x1234 && aos_fixed_divide(5, 0) == 0);

    /* A fade reaches its target after exactly its frames. */
    AosPaletteFade fade;
    aos_fade_start(&fade, 0, 0, 0, 0x10000, 0, 0x40);
    for (int i = 0; i < 0x3F; ++i) aos_fade_tick(&fade);
    assert(fade.active && fade.weight == 0x10000 - 0x3F * 0x400);
    aos_fade_tick(&fade);
    assert(!fade.active && fade.weight == 0);

    /* sub_0800C8C8: BG black after 0x40 frames, GAME_OVER once the OBJ
     * fade (0x80 frames) has ended. */
    AosGameOver death;
    aos_death_start(&death);
    int frames = 0;
    while (!aos_death_update(&death) && frames < 1000) ++frames;
    assert(frames == 0x80 && death.bg.weight == 0 && death.obj.weight == 0);

    /* GameModeGameOverUpdate. */
    AosGameOver over = {0};
    AosInput idle = {0}, press = {0};
    press.new_input = 1;
    assert(aos_game_over_update(&over, &idle) == AOS_MODE_SAME && over.stage == 1 &&
           over.eva == 0 && over.evb == 16 && over.bg.weight == 0x800);
    frames = 1;
    while (over.stage == 1) {
        aos_game_over_update(&over, &idle);
        ++frames;
    }
    /* The BG fades in over 0x20 frames; stage 2 starts its 0x80 wait. */
    assert(frames == 0x21 && over.timer == 0x80 && over.bg.weight == 0x10000);
    for (int i = 0; i < 0x7F; ++i) aos_game_over_update(&over, &idle);
    assert(!over.sprite_shown && over.timer == 1);
    aos_game_over_update(&over, &idle);
    assert(over.sprite_shown && over.timer == 0);
    /* The sprite blends in over 0x20 frames (EVA 0 -> 16, EVB 16 -> 0). */
    aos_game_over_update(&over, &idle);
    assert(over.alpha == 0x1F && over.eva == 1 && over.evb == 15);
    for (int i = 0; i < 0x1F; ++i) aos_game_over_update(&over, &idle);
    assert(over.alpha == 0 && over.eva == 16 && over.evb == 0 && over.stage == 2);
    aos_game_over_update(&over, &idle);
    assert(over.stage == 3);
    /* It waits for a press, then fades to white and returns to the logo. */
    for (int i = 0; i < 50; ++i) assert(aos_game_over_update(&over, &idle) == AOS_MODE_SAME);
    assert(over.stage == 3);
    aos_game_over_update(&over, &press);
    assert(over.stage == 4 && over.bg.r == 0x1F && over.obj.active);
    int32_t mode = AOS_MODE_SAME;
    frames = 0;
    while (mode == AOS_MODE_SAME && frames < 100) {
        mode = aos_game_over_update(&over, &idle);
        ++frames;
    }
    assert(mode == AOS_MODE_KONAMI_LOGO && frames == 0x20 && !over.sprite_shown);
    puts("Aria game over tests passed");
    return 0;
}
