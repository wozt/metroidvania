#ifndef FUSION_MZM_STATE_H
#define FUSION_MZM_STATE_H

#include "gba/runtime.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MZM_SAMUS_DATA_SIZE 32u
#define MZM_EQUIPMENT_SIZE 20u

typedef struct {
    uint8_t difficulty;
    uint8_t location[3];
    uint8_t mode[4];
    uint8_t samus[MZM_SAMUS_DATA_SIZE];
    uint8_t equipment[MZM_EQUIPMENT_SIZE];
} MzmStateBytes;

typedef struct MzmStateView {
    uint16_t game_mode;
    int16_t sub_game_mode;
    uint8_t difficulty;
    uint8_t area;
    uint8_t room;
    uint8_t last_door;
    uint8_t pose;
    uint8_t standing_status;
    uint8_t arm_cannon_direction;
    uint16_t direction;
    uint16_t x_subpixels;
    uint16_t y_subpixels;
    int16_t x_velocity;
    int16_t y_velocity;
    uint16_t max_energy;
    uint16_t current_energy;
    uint16_t max_missiles;
    uint16_t current_missiles;
    uint8_t max_super_missiles;
    uint8_t current_super_missiles;
    uint8_t max_power_bombs;
    uint8_t current_power_bombs;
    uint8_t beam_bomb_flags;
    uint8_t suit_misc_flags;
    bool gameplay_active;
    bool values_plausible;
    bool gameplay_state_ready;
} MzmStateView;

bool mzm_state_decode(const MzmStateBytes *bytes, MzmStateView *out);
bool mzm_state_read(const GbaRuntime *runtime, MzmStateView *out,
                    char *error, size_t error_size);

#endif
