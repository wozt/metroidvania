/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_button_config.h"

static const uint16_t buttons[4] = {AOS_KEY_A, AOS_KEY_B, AOS_KEY_R, AOS_KEY_L};

void aos_button_config_default(AosButtonConfig *config) {
    if (!config) return;
    /* 0x0852550C, in attack, jump, ability, guardian field order. */
    static const uint8_t defaults[AOS_BUTTON_ACTIONS] = {1, 0, 3, 2};
    for (int action = 0; action < AOS_BUTTON_ACTIONS; ++action)
        config->action[action] = buttons[defaults[action]];
}

uint8_t aos_button_config_index(uint16_t button) {
    for (uint8_t index = 0; index < 4; ++index)
        if (buttons[index] == button) return index;
    return 0xFF;
}

uint16_t aos_button_config_button(uint8_t index) {
    return index < 4 ? buttons[index] : 0;
}

bool aos_button_config_valid(const AosButtonConfig *config) {
    if (!config) return false;
    uint8_t used = 0;
    for (int action = 0; action < AOS_BUTTON_ACTIONS; ++action) {
        uint8_t index = aos_button_config_index(config->action[action]);
        if (index >= 4 || (used & (1u << index))) return false;
        used |= (uint8_t)(1u << index);
    }
    return used == 0x0F;
}

const char *aos_button_config_button_name(uint8_t index) {
    static const char *const names[4] = {"A", "B", "R", "L"};
    return index < 4 ? names[index] : "?";
}

const char *aos_button_config_action_name(uint8_t action) {
    static const char *const names[AOS_BUTTON_ACTIONS] = {
        "ATTACK", "JUMP", "ABILITY", "GUARDIAN",
    };
    return action < AOS_BUTTON_ACTIONS ? names[action] : "UNKNOWN";
}

uint16_t aos_button_config_remap(const AosButtonConfig *config, uint16_t physical) {
    AosButtonConfig fallback;
    if (!aos_button_config_valid(config)) {
        aos_button_config_default(&fallback);
        config = &fallback;
    }
    uint16_t semantic = physical & (uint16_t)~(AOS_KEY_A | AOS_KEY_B | AOS_KEY_R | AOS_KEY_L);
    if (physical & config->action[AOS_BUTTON_ATTACK]) semantic |= AOS_KEY_B;
    if (physical & config->action[AOS_BUTTON_JUMP]) semantic |= AOS_KEY_A;
    if (physical & config->action[AOS_BUTTON_ABILITY]) semantic |= AOS_KEY_L;
    if (physical & config->action[AOS_BUTTON_GUARDIAN]) semantic |= AOS_KEY_R;
    return semantic;
}
