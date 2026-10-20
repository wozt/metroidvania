/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_input.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    AosInput in = {0};
    aos_input_update(&in, 0x11);
    assert(in.held_input == 0x11 && in.new_input == 0x11);
    assert(in.player_held_input == 0x11 && in.player_new_input == 0x11);
    assert(in.repeated_input == 0x11 && in.repeated_input_timer == 32);
    aos_input_update(&in, 0x11);
    assert(in.new_input == 0 && in.player_new_input == 0);
    assert(in.repeated_input == 0 && in.repeated_input_timer == 31);
    for (int i = 0; i < 31; ++i) aos_input_update(&in, 0x11);
    assert(in.repeated_input == 0x11 && in.repeated_input_timer == 6);
    aos_input_update(&in, 0);
    assert(in.held_input == 0 && in.new_input == 0);
    assert(in.player_held_input == 0 && in.player_new_input == 0);
    aos_input_update(&in, 0x01);
    assert(in.new_input == 0x01 && in.repeated_input_timer == 32);

    /* SetPlayerInput: priorities compare numerically, lower wins. */
    assert(aos_input_set_player(&in, 5, 0x12));
    assert(in.player_held_input == 0x12 && in.player_new_input == 0x12);
    assert(!aos_input_set_player(&in, 5, 0x02));
    assert(in.player_held_input == 0x12);
    assert(aos_input_set_player(&in, 1, 0x02));
    assert(in.player_new_input == 0 && in.player_held_input == 0x02);

    /* Native sub_080106DC invokes SetPlayerInput(1, 0) on transitions.
     * Here that call clears player controls while keeping raw host input. */
    assert(aos_input_set_player(&in, 0, 0));
    assert(aos_input_set_player(&in, 1, 0));
    aos_input_update(&in, 0x20);
    assert(in.held_input == 0x20 && in.new_input == 0x20);
    assert(in.player_held_input == 0 && in.player_new_input == 0);
    assert(in.player_input_priority == 0);
    aos_input_update(&in, 0x20);
    assert(in.player_held_input == 0x20 && in.player_new_input == 0);
    aos_input_update(&in, 0);
    aos_input_update(&in, 0x20);
    assert(in.player_new_input == 0x20);
    /* The original main loop calls SoftResetCheck before UpdateInput.
     * A four-key chord pressed simultaneously must not reset. A delayed
     * final chord key does reset; repeated held frames have no new edge. */
    const uint16_t chord = 0x000fu;
    bool inhibit = false;
    AosInput reset = {0};
    aos_input_update(&reset, chord);
    assert(!aos_input_soft_reset_check(&reset, &inhibit));
    aos_input_update(&reset, chord);
    assert(!aos_input_soft_reset_check(&reset, &inhibit));
    aos_input_update(&reset, 0);
    aos_input_update(&reset, 0x0007);
    assert(!aos_input_soft_reset_check(&reset, &inhibit));
    aos_input_update(&reset, chord);
    assert(aos_input_soft_reset_check(&reset, &inhibit));
    inhibit = true;
    assert(!aos_input_soft_reset_check(&reset, &inhibit));
    assert(!inhibit);
    assert(aos_input_soft_reset_check(&reset, &inhibit));
    aos_input_update(&reset, chord);
    assert(!aos_input_soft_reset_check(&reset, &inhibit));
    /* Another key's rising edge also triggers a held reset chord. */
    aos_input_update(&reset, chord | 0x0020);
    assert(aos_input_soft_reset_check(&reset, &inhibit));
    puts("Aria native input tests passed");
    return 0;
}
