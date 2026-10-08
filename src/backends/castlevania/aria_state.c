/* SPDX-License-Identifier: GPL-3.0-only */
#include "aria/state.h"

#include <stdio.h>
#include <string.h>

enum {
    ARIA_MODE_ADDRESS = 0x02000010,
    ARIA_GAMEPLAY_ADDRESS = 0x02000064,
    ARIA_LOCATION_ADDRESS = 0x0200009e,
    ARIA_STAGED_ARRIVAL_ADDRESS = 0x02000334,
    ARIA_STAGED_ROOM_POINTER_ADDRESS = 0x020003cc,
    ARIA_CONTROL_FLAGS_ADDRESS = 0x0200a074,
    ARIA_CAMERA_ADDRESS = 0x0200a098,
    ARIA_PLAYER_POINTER_ADDRESS = 0x02013110,
    ARIA_PROGRESSION_ADDRESS = 0x02013266,
    ARIA_ENTITY_ARRAY_ADDRESS = 0x020004e4,
    ARIA_ENTITY_SIZE = 0x84,
    ARIA_ENTITY_COUNT = 0xe0,
    ARIA_GAME_MODE_IN_GAME = 4,
    ARIA_GAME_MODE_COUNT = 21,
    ARIA_IN_GAME_PHASE_COUNT = 15,
    ARIA_AREA_COUNT = 16,
    ARIA_ROOM_COUNT = 64,
    ARIA_CHARACTER_COUNT = 2,
    ARIA_MAX_LEVEL = 99,
};

static uint16_t read_u16(const uint8_t *bytes, size_t offset)
{
    return (uint16_t)(bytes[offset] | ((uint16_t)bytes[offset + 1] << 8));
}

static int16_t read_s16(const uint8_t *bytes, size_t offset)
{
    return (int16_t)read_u16(bytes, offset);
}

static uint32_t read_u32(const uint8_t *bytes, size_t offset)
{
    return (uint32_t)bytes[offset] |
           ((uint32_t)bytes[offset + 1] << 8) |
           ((uint32_t)bytes[offset + 2] << 16) |
           ((uint32_t)bytes[offset + 3] << 24);
}

static int32_t read_s32(const uint8_t *bytes, size_t offset)
{
    return (int32_t)read_u32(bytes, offset);
}

static bool player_entity_address_valid(uint32_t address)
{
    uint32_t offset;
    uint32_t array_size = ARIA_ENTITY_SIZE * ARIA_ENTITY_COUNT;
    if (address < ARIA_ENTITY_ARRAY_ADDRESS ||
        address >= ARIA_ENTITY_ARRAY_ADDRESS + array_size)
        return false;
    offset = address - ARIA_ENTITY_ARRAY_ADDRESS;
    return offset % ARIA_ENTITY_SIZE == 0;
}

static bool room_pointer_plausible(uint32_t address)
{
    return address >= UINT32_C(0x08000000) &&
           address < UINT32_C(0x08800000) &&
           address % sizeof(uint32_t) == 0;
}

static void set_error(char *error, size_t error_size, const char *message)
{
    if (error && error_size) snprintf(error, error_size, "%s", message);
}

bool aria_state_read(const GbaRuntime *runtime, AriaStateView *out,
                     char *error, size_t error_size)
{
    AriaStateBytes bytes;
    uint32_t player_address;

    memset(&bytes, 0, sizeof(bytes));
    if (!runtime || !out ||
        !gba_runtime_read_memory(runtime, ARIA_MODE_ADDRESS,
                                 bytes.mode, sizeof(bytes.mode)) ||
        !gba_runtime_read_memory(runtime, ARIA_GAMEPLAY_ADDRESS,
                                 bytes.gameplay, sizeof(bytes.gameplay)) ||
        !gba_runtime_read_memory(runtime, ARIA_LOCATION_ADDRESS,
                                 bytes.location, sizeof(bytes.location)) ||
        !gba_runtime_read_memory(runtime, ARIA_STAGED_ARRIVAL_ADDRESS,
                                 bytes.staged_arrival,
                                 sizeof(bytes.staged_arrival)) ||
        !gba_runtime_read_memory(runtime, ARIA_STAGED_ROOM_POINTER_ADDRESS,
                                 bytes.staged_room_pointer,
                                 sizeof(bytes.staged_room_pointer)) ||
        !gba_runtime_read_memory(runtime, ARIA_CONTROL_FLAGS_ADDRESS,
                                 &bytes.control_flags,
                                 sizeof(bytes.control_flags)) ||
        !gba_runtime_read_memory(runtime, ARIA_CAMERA_ADDRESS,
                                 bytes.camera, sizeof(bytes.camera)) ||
        !gba_runtime_read_memory(runtime, ARIA_PLAYER_POINTER_ADDRESS,
                                 bytes.player_pointer,
                                 sizeof(bytes.player_pointer)) ||
        !gba_runtime_read_memory(runtime, ARIA_PROGRESSION_ADDRESS,
                                 bytes.progression, sizeof(bytes.progression))) {
        set_error(error, error_size, "cannot read the verified Aria EWRAM ranges");
        return false;
    }
    player_address = read_u32(bytes.player_pointer, 0);
    if (player_entity_address_valid(player_address) &&
        !gba_runtime_read_memory(runtime, player_address,
                                 bytes.player_entity,
                                 sizeof(bytes.player_entity))) {
        set_error(error, error_size, "cannot read the verified Aria player entity");
        return false;
    }
    if (!aria_state_decode(&bytes, out)) {
        set_error(error, error_size, "cannot decode the verified Aria state layout");
        return false;
    }
    set_error(error, error_size, "");
    return true;
}

bool aria_state_decode(const AriaStateBytes *bytes, AriaStateView *out)
{
    AriaStateView view = {0};
    const uint8_t *progression;
    const uint8_t *entity;
    uint32_t entity_x;
    uint32_t entity_y;
    if (!bytes || !out) return false;
    progression = bytes->progression;
    entity = bytes->player_entity;

    view.game_mode = bytes->mode[0];
    view.game_mode_stage = bytes->mode[1];
    view.in_game_phase = bytes->gameplay[0];
    view.in_game_phase_stage = bytes->gameplay[1];
    view.player_control_enabled = (bytes->control_flags & (1u << 1)) != 0;
    view.area = bytes->location[0];
    view.room = bytes->location[1];
    view.staged_camera_x = read_u16(bytes->staged_arrival, 0);
    view.staged_camera_y = read_u16(bytes->staged_arrival, 2);
    view.staged_player_x = read_u16(bytes->staged_arrival, 4);
    view.staged_player_y = read_u16(bytes->staged_arrival, 6);
    view.staged_room_pointer = read_u32(bytes->staged_room_pointer, 0);
    view.staged_arrival_plausible =
        room_pointer_plausible(view.staged_room_pointer);
    view.player_entity_address = read_u32(bytes->player_pointer, 0);
    view.player_entity_valid =
        player_entity_address_valid(view.player_entity_address);
    if (view.player_entity_valid) {
        entity_x = read_u32(entity, 0x40);
        entity_y = read_u32(entity, 0x44);
        view.x_position_fixed = read_u32(bytes->camera, 0) + entity_x;
        view.y_position_fixed = read_u32(bytes->camera, 4) + entity_y;
        view.x_velocity_fixed = read_s32(entity, 0x48);
        view.y_velocity_fixed = read_s32(entity, 0x4c);
        view.animation_flags = entity[0x6c];
        view.animation_id = entity[0x6d];
        view.animation_frame = entity[0x6e];
        view.animation_counter = entity[0x6f];
    }
    view.current_character = progression[0];
    view.equipped_weapon = progression[2];
    view.equipped_red_soul = progression[3];
    view.equipped_blue_soul = progression[4];
    view.equipped_yellow_soul = progression[5];
    view.equipped_armor = progression[6];
    view.equipped_accessory = progression[7];
    view.current_level = progression[0x13];
    view.current_hp = read_s16(progression, 0x14);
    view.current_mp = read_s16(progression, 0x16);
    view.max_hp = read_u16(progression, 0x18);
    view.max_mp = read_u16(progression, 0x1a);
    view.current_experience = read_u32(progression, 0x26);
    view.current_gold = read_u32(progression, 0x2a);
    view.gameplay_active = view.game_mode == ARIA_GAME_MODE_IN_GAME;
    view.values_plausible = view.game_mode < ARIA_GAME_MODE_COUNT &&
                            (!view.gameplay_active ||
                             (view.area < ARIA_AREA_COUNT &&
                              view.room < ARIA_ROOM_COUNT &&
                              view.in_game_phase < ARIA_IN_GAME_PHASE_COUNT &&
                              view.player_entity_valid &&
                              view.current_character < ARIA_CHARACTER_COUNT &&
                              view.current_level > 0 &&
                              view.current_level <= ARIA_MAX_LEVEL &&
                              view.max_hp > 0 &&
                              view.max_mp > 0 &&
                              view.current_hp >= 0 &&
                              view.current_hp <= view.max_hp &&
                              view.current_mp >= 0 &&
                              view.current_mp <= view.max_mp));
    view.gameplay_state_ready = view.gameplay_active &&
                                view.values_plausible &&
                                view.in_game_phase == 1 &&
                                view.player_control_enabled &&
                                view.animation_id != UINT8_MAX &&
                                (view.x_position_fixed != 0 ||
                                 view.y_position_fixed != 0);
    *out = view;
    return true;
}
