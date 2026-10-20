/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria input state, independent of SDL host polling. */
#ifndef AOS_INPUT_H
#define AOS_INPUT_H
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t held_input;
    uint16_t new_input;
    uint16_t player_held_input;
    uint16_t player_new_input;
    uint16_t repeated_input;
    uint16_t repeated_input_timer;
    int32_t player_input_priority;
} AosInput;

/* UpdateInput: host supplies the GBA's active-high ten-bit key mask. */
void aos_input_update(AosInput *state, uint16_t held);
/* SetPlayerInput: lower nonzero priority number wins. */
bool aos_input_set_player(AosInput *state, int32_t priority, uint16_t held);
#endif
