/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_TITLE_INPUT_H
#define AOS_TITLE_INPUT_H
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int32_t idle_timer;
    bool timed_out;
    bool leaving;
} AosTitleInput;

typedef enum {
    AOS_TITLE_STAY = 0,
    AOS_TITLE_EXIT = 1,
    AOS_TITLE_SUBMENU = 2,
    AOS_TITLE_BACK = 3
} AosTitleAction;

/* Stage 7 -> 8 sets the timeout to 0x258 (600 frames). */
void aos_title_input_enter(AosTitleInput *state);
/* Stage-8 controls, evaluated once per native 60 Hz frame.
 * The caller supplies the title entity's actual selection state and
 * whether the pending selection resolves to the Konami logo. */
AosTitleAction aos_title_input_tick(AosTitleInput *state, uint16_t new_input,
                                    uint16_t repeat_input, bool logo_selected,
                                    bool submenu_open, bool exit_blocked);
#endif
