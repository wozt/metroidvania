#ifndef FUSION_ARIA_STATE_H
#define FUSION_ARIA_STATE_H

#include "gba/runtime.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ARIA_MODE_DATA_SIZE 2u
#define ARIA_GAMEPLAY_DATA_SIZE 2u
#define ARIA_LOCATION_DATA_SIZE 2u
#define ARIA_CAMERA_DATA_SIZE 8u
#define ARIA_PLAYER_POINTER_SIZE 4u
#define ARIA_PROGRESSION_DATA_SIZE 46u
#define ARIA_PLAYER_ENTITY_SIZE 112u

typedef struct {
    uint8_t mode[ARIA_MODE_DATA_SIZE];
    uint8_t gameplay[ARIA_GAMEPLAY_DATA_SIZE];
    uint8_t control_flags;
    uint8_t location[ARIA_LOCATION_DATA_SIZE];
    uint8_t camera[ARIA_CAMERA_DATA_SIZE];
    uint8_t player_pointer[ARIA_PLAYER_POINTER_SIZE];
    uint8_t progression[ARIA_PROGRESSION_DATA_SIZE];
    uint8_t player_entity[ARIA_PLAYER_ENTITY_SIZE];
} AriaStateBytes;

typedef struct AriaStateView {
    uint8_t game_mode;
    uint8_t game_mode_stage;
    uint8_t in_game_phase;
    uint8_t in_game_phase_stage;
    bool player_control_enabled;
    uint8_t area;
    uint8_t room;
    uint32_t player_entity_address;
    uint32_t x_position_fixed;
    uint32_t y_position_fixed;
    int32_t x_velocity_fixed;
    int32_t y_velocity_fixed;
    uint8_t animation_flags;
    uint8_t animation_id;
    uint8_t animation_frame;
    uint8_t animation_counter;
    uint8_t current_character;
    uint8_t current_level;
    int16_t current_hp;
    int16_t current_mp;
    uint16_t max_hp;
    uint16_t max_mp;
    uint8_t equipped_weapon;
    uint8_t equipped_red_soul;
    uint8_t equipped_blue_soul;
    uint8_t equipped_yellow_soul;
    uint8_t equipped_armor;
    uint8_t equipped_accessory;
    uint32_t current_experience;
    uint32_t current_gold;
    bool gameplay_active;
    bool player_entity_valid;
    bool values_plausible;
    bool gameplay_state_ready;
} AriaStateView;

bool aria_state_decode(const AriaStateBytes *bytes, AriaStateView *out);
bool aria_state_read(const GbaRuntime *runtime, AriaStateView *out,
                     char *error, size_t error_size);

#endif
