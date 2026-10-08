/* SPDX-License-Identifier: GPL-3.0-only */
#include "mzm/state.h"

#include <stdio.h>

enum {
    MZM_DIFFICULTY_ADDRESS = 0x0300002c,
    MZM_LOCATION_ADDRESS = 0x03000054,
    MZM_GAME_MODE_ADDRESS = 0x03000c70,
    MZM_SAMUS_DATA_ADDRESS = 0x030013d4,
    MZM_EQUIPMENT_ADDRESS = 0x03001530,
    MZM_GAME_MODE_INGAME = 4,
    MZM_GAME_MODE_DEMO = 11,
    MZM_GAME_MODE_COUNT = 17,
    MZM_SUB_GAME_MODE_DOOR_TRANSITION = 1,
    MZM_SUB_GAME_MODE_PLAYING = 2,
    MZM_SUB_GAME_MODE_LOADING_ROOM = 3,
    MZM_SUB_GAME_MODE_DYING = 5,
    MZM_SUB_GAME_MODE_NO_CLIP = 6,
    MZM_AREA_COUNT = 7,
    MZM_DIFFICULTY_COUNT = 3,
};

static uint16_t read_u16(const uint8_t *bytes, size_t offset)
{
    return (uint16_t)(bytes[offset] | ((uint16_t)bytes[offset + 1] << 8));
}

static int16_t read_s16(const uint8_t *bytes, size_t offset)
{
    return (int16_t)read_u16(bytes, offset);
}

static void set_error(char *error, size_t error_size, const char *message)
{
    if (error && error_size) snprintf(error, error_size, "%s", message);
}

bool mzm_state_read(const GbaRuntime *runtime, MzmStateView *out,
                    char *error, size_t error_size)
{
    MzmStateBytes bytes;

    if (!runtime || !out ||
        !gba_runtime_read_memory(runtime, MZM_DIFFICULTY_ADDRESS,
                                 &bytes.difficulty, sizeof(bytes.difficulty)) ||
        !gba_runtime_read_memory(runtime, MZM_LOCATION_ADDRESS,
                                 bytes.location, sizeof(bytes.location)) ||
        !gba_runtime_read_memory(runtime, MZM_GAME_MODE_ADDRESS,
                                 bytes.mode, sizeof(bytes.mode)) ||
        !gba_runtime_read_memory(runtime, MZM_SAMUS_DATA_ADDRESS,
                                 bytes.samus, sizeof(bytes.samus)) ||
        !gba_runtime_read_memory(runtime, MZM_EQUIPMENT_ADDRESS,
                                 bytes.equipment, sizeof(bytes.equipment))) {
        set_error(error, error_size, "cannot read the verified MZM WRAM ranges");
        return false;
    }
    if (!mzm_state_decode(&bytes, out)) {
        set_error(error, error_size, "cannot decode the verified MZM state layout");
        return false;
    }
    set_error(error, error_size, "");
    return true;
}

bool mzm_state_decode(const MzmStateBytes *bytes, MzmStateView *out)
{
    MzmStateView view = {0};
    bool active_submode;
    const uint8_t *location;
    const uint8_t *mode;
    const uint8_t *samus;
    const uint8_t *equipment;
    if (!bytes || !out) return false;
    location = bytes->location;
    mode = bytes->mode;
    samus = bytes->samus;
    equipment = bytes->equipment;

    view.game_mode = read_u16(mode, 0);
    view.sub_game_mode = read_s16(mode, 2);
    view.difficulty = bytes->difficulty;
    view.area = location[0];
    view.room = location[1];
    view.last_door = location[2];
    view.pose = samus[0];
    view.standing_status = samus[1];
    view.arm_cannon_direction = samus[2];
    view.direction = read_u16(samus, 14);
    view.x_subpixels = read_u16(samus, 18);
    view.y_subpixels = read_u16(samus, 20);
    view.x_velocity = read_s16(samus, 22);
    view.y_velocity = read_s16(samus, 24);
    view.max_energy = read_u16(equipment, 0);
    view.max_missiles = read_u16(equipment, 2);
    view.max_super_missiles = equipment[4];
    view.max_power_bombs = equipment[5];
    view.current_energy = read_u16(equipment, 6);
    view.current_missiles = read_u16(equipment, 8);
    view.current_super_missiles = equipment[10];
    view.current_power_bombs = equipment[11];
    view.beam_bomb_flags = equipment[12];
    view.suit_misc_flags = equipment[14];
    view.gameplay_active = view.game_mode == MZM_GAME_MODE_INGAME ||
                           view.game_mode == MZM_GAME_MODE_DEMO;
    view.values_plausible = view.game_mode < MZM_GAME_MODE_COUNT &&
                            view.difficulty < MZM_DIFFICULTY_COUNT &&
                            (!view.gameplay_active ||
                             (view.area < MZM_AREA_COUNT &&
                              view.max_energy > 0 &&
                              view.current_energy <= view.max_energy &&
                              view.current_missiles <= view.max_missiles &&
                              view.current_super_missiles <= view.max_super_missiles &&
                              view.current_power_bombs <= view.max_power_bombs));
    active_submode = view.sub_game_mode == MZM_SUB_GAME_MODE_DOOR_TRANSITION ||
                     view.sub_game_mode == MZM_SUB_GAME_MODE_PLAYING ||
                     view.sub_game_mode == MZM_SUB_GAME_MODE_LOADING_ROOM ||
                     view.sub_game_mode == MZM_SUB_GAME_MODE_DYING ||
                     view.sub_game_mode == MZM_SUB_GAME_MODE_NO_CLIP;
    view.gameplay_state_ready = view.gameplay_active &&
                                view.values_plausible &&
                                active_submode &&
                                (view.x_subpixels != 0 || view.y_subpixels != 0);
    *out = view;
    return true;
}
