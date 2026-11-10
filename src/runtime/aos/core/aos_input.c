/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_input.h"

/* Native UpdateInput, src/main.c: the caller samples hardware once per frame.
 * The host supplies KEYINPUT ^ KEY_MASK after SDL mapping and filtering. */
void aos_input_update(AosInput *state, uint16_t held) {
    held &= 0x03ffu;
    state->new_input = (uint16_t)(held & ~state->held_input);
    state->held_input = held;
    if (state->player_input_priority == 0) {
        state->player_held_input = held;
        state->player_new_input = state->new_input;
    } else {
        state->player_input_priority = 0;
    }
    state->repeated_input = 0;
    if (state->new_input != 0) {
        state->repeated_input_timer = 32;
        state->repeated_input = held;
        return;
    }
    state->repeated_input_timer = (uint16_t)(state->repeated_input_timer - 1u);
    if (state->repeated_input_timer == 0) {
        state->repeated_input_timer = 6;
        state->repeated_input = state->held_input;
    }
}

/* Native SetPlayerInput, src/code_080009A0.c. In sub_080106DC, a
 * room-transition state machine uses SetPlayerInput(1, 0) to suppress Soma's
 * controls; the visual fade and camera movement are NOT implemented here. */
bool aos_input_set_player(AosInput *state, int32_t priority, uint16_t held) {
    if (state->player_input_priority == 0 || priority < state->player_input_priority) {
        state->player_input_priority = priority;
        state->player_new_input = (uint16_t)(~state->player_held_input & held);
        state->player_held_input = held;
        return true;
    }
    return false;
}

/* Native SoftResetCheck, main.c, called before UpdateInput. The original
 * requests a soft reset only when the A+B+Select+Start chord is currently
 * held AND this frame has a new key edge (but not all four newly pressed).
 * Its single-use inhibit flag is cleared on the first call. */
bool aos_input_soft_reset_check(const AosInput *state, bool *inhibit_once) {
    const uint16_t chord = 0x000fu;
    if (*inhibit_once) {
        *inhibit_once = false;
        return false;
    }
    if ((state->new_input & chord) == chord) return false;
    if (state->new_input == 0) return false;
    return (state->held_input & chord) == chord;
}
