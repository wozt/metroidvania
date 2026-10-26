/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_title_input.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    AosTitleInput s = {0};
    aos_title_input_enter(&s);
    assert(s.idle_timer == 600 && !s.timed_out && !s.leaving);
    assert(aos_title_input_tick(&s, 0, 0, false, false, false) == AOS_TITLE_STAY);
    assert(s.idle_timer == 599);
    assert(aos_title_input_tick(&s, 0, 128, false, false, false) == AOS_TITLE_STAY);
    assert(s.idle_timer == 599);
    assert(aos_title_input_tick(&s, 8, 0, true, false, false) == AOS_TITLE_SUBMENU);
    assert(!s.leaving);
    assert(aos_title_input_tick(&s, 2, 0, false, true, false) == AOS_TITLE_BACK);
    assert(aos_title_input_tick(&s, 8, 0, false, false, true) == AOS_TITLE_STAY);
    assert(aos_title_input_tick(&s, 1, 0, false, false, false) == AOS_TITLE_EXIT);
    assert(s.leaving);
    for (unsigned frame = 0; frame < 32; ++frame) {
        assert(!aos_title_exit_tick(&s));
        assert(s.brightness == frame / 2);
    }
    assert(aos_title_exit_tick(&s) && s.brightness == 16);
    aos_title_input_enter(&s);
    for (int i = 0; i < 599; ++i)
        assert(aos_title_input_tick(&s, 0, 0, false, false, false) == AOS_TITLE_STAY);
    assert(aos_title_input_tick(&s, 0, 0, false, false, false) == AOS_TITLE_EXIT);
    assert(s.timed_out);
    puts("Aria title input tests passed");
    return 0;
}
