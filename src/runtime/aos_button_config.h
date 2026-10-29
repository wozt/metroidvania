/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria's four configurable player-action buttons. */
#ifndef AOS_BUTTON_CONFIG_H
#define AOS_BUTTON_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#include "aos_input.h"

enum {
    AOS_BUTTON_ATTACK,
    AOS_BUTTON_JUMP,
    AOS_BUTTON_ABILITY,
    AOS_BUTTON_GUARDIAN,
    AOS_BUTTON_ACTIONS,
};

typedef struct {
    /* Physical GBA masks in native field order: attack, jump, ability,
     * guardian (gEwramData + 0x13398 through + 0x1339E). */
    uint16_t action[AOS_BUTTON_ACTIONS];
} AosButtonConfig;

void aos_button_config_default(AosButtonConfig *config);
bool aos_button_config_valid(const AosButtonConfig *config);
uint8_t aos_button_config_index(uint16_t button);
uint16_t aos_button_config_button(uint8_t index);
const char *aos_button_config_button_name(uint8_t index);
const char *aos_button_config_action_name(uint8_t action);

/* Translate physical A/B/R/L into the fixed semantic bits consumed by the
 * native player reconstruction. Directions, Start and Select pass through. */
uint16_t aos_button_config_remap(const AosButtonConfig *config, uint16_t physical);

#endif /* AOS_BUTTON_CONFIG_H */
