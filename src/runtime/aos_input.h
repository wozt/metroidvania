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

/* Active-high GBA key bits shared by every native menu/controller. */
enum {
    AOS_KEY_A = 1u << 0,
    AOS_KEY_B = 1u << 1,
    AOS_KEY_SELECT = 1u << 2,
    AOS_KEY_START = 1u << 3,
    AOS_KEY_RIGHT = 1u << 4,
    AOS_KEY_LEFT = 1u << 5,
    AOS_KEY_UP = 1u << 6,
    AOS_KEY_DOWN = 1u << 7,
    AOS_KEY_R = 1u << 8,
    AOS_KEY_L = 1u << 9
};

/* UpdateInput: host supplies the GBA's active-high ten-bit key mask. */
void aos_input_update(AosInput *state, uint16_t held);
/* SetPlayerInput: lower nonzero priority number wins. */
bool aos_input_set_player(AosInput *state, int32_t priority, uint16_t held);
/* SoftResetCheck. Called before UpdateInput, as in the original main loop.
 * The caller owns the single-use native inhibit flag and any actual reset.
 * True means that the original routine requests a GBA soft reset. */
bool aos_input_soft_reset_check(const AosInput *state, bool *inhibit_once);
#endif
