#include "aria/state.h"

#include <assert.h>
#include <stdio.h>

static void write_u16(uint8_t *bytes, size_t offset, uint16_t value)
{
    bytes[offset] = (uint8_t)value;
    bytes[offset + 1] = (uint8_t)(value >> 8);
}

static void write_u32(uint8_t *bytes, size_t offset, uint32_t value)
{
    bytes[offset] = (uint8_t)value;
    bytes[offset + 1] = (uint8_t)(value >> 8);
    bytes[offset + 2] = (uint8_t)(value >> 16);
    bytes[offset + 3] = (uint8_t)(value >> 24);
}

int main(void)
{
    AriaStateBytes bytes = {0};
    AriaStateView view;
    GbaRuntime runtime = {0};
    char error[128];

    assert(!aria_state_decode(NULL, &view));
    assert(!aria_state_decode(&bytes, NULL));
    assert(!aria_state_read(&runtime, &view, error, sizeof(error)));
    assert(error[0]);

    bytes.mode[0] = 4;
    bytes.mode[1] = 0;
    bytes.gameplay[0] = 1;
    bytes.gameplay[1] = 0;
    bytes.control_flags = 1u << 1;
    bytes.location[0] = 11;
    bytes.location[1] = 63;
    write_u16(bytes.staged_arrival, 0, 0x20);
    write_u16(bytes.staged_arrival, 2, 0x200);
    write_u16(bytes.staged_arrival, 4, 0x78);
    write_u16(bytes.staged_arrival, 6, 0x8d);
    write_u32(bytes.staged_room_pointer, 0, 0x0850ef9c);
    write_u32(bytes.camera, 0, 0x00100000);
    write_u32(bytes.camera, 4, 0x00200000);
    write_u32(bytes.player_pointer, 0, 0x020005ec);
    bytes.progression[0] = 0;
    bytes.progression[2] = 9;
    bytes.progression[3] = 10;
    bytes.progression[4] = 11;
    bytes.progression[5] = 12;
    bytes.progression[6] = 13;
    bytes.progression[7] = 14;
    bytes.progression[0x13] = 42;
    write_u16(bytes.progression, 0x14, 333);
    write_u16(bytes.progression, 0x16, 77);
    write_u16(bytes.progression, 0x18, 500);
    write_u16(bytes.progression, 0x1a, 250);
    write_u32(bytes.progression, 0x26, 123456);
    write_u32(bytes.progression, 0x2a, 654321);
    write_u32(bytes.player_entity, 0x40, 0x00640000);
    write_u32(bytes.player_entity, 0x44, 0x00c80000);
    write_u32(bytes.player_entity, 0x48, (uint32_t)-0x18000);
    write_u32(bytes.player_entity, 0x4c, 0x20000);
    bytes.player_entity[0x6c] = 9;
    bytes.player_entity[0x6d] = 12;
    bytes.player_entity[0x6e] = 3;
    bytes.player_entity[0x6f] = 4;

    assert(aria_state_decode(&bytes, &view));
    assert(view.gameplay_active && view.player_entity_valid);
    assert(view.values_plausible && view.gameplay_state_ready);
    assert(view.game_mode == 4 && view.game_mode_stage == 0);
    assert(view.in_game_phase == 1 && view.in_game_phase_stage == 0);
    assert(view.player_control_enabled);
    assert(view.area == 11 && view.room == 63);
    assert(view.staged_camera_x == 0x20 && view.staged_camera_y == 0x200);
    assert(view.staged_player_x == 0x78 && view.staged_player_y == 0x8d);
    assert(view.staged_room_pointer == UINT32_C(0x0850ef9c));
    assert(view.staged_arrival_plausible);
    assert(view.player_entity_address == UINT32_C(0x020005ec));
    assert(view.x_position_fixed == 0x00740000);
    assert(view.y_position_fixed == 0x00e80000);
    assert(view.x_velocity_fixed == -0x18000);
    assert(view.y_velocity_fixed == 0x20000);
    assert(view.animation_flags == 9 && view.animation_id == 12);
    assert(view.animation_frame == 3 && view.animation_counter == 4);
    assert(view.current_character == 0 && view.current_level == 42);
    assert(view.current_hp == 333 && view.max_hp == 500);
    assert(view.current_mp == 77 && view.max_mp == 250);
    assert(view.equipped_weapon == 9 && view.equipped_red_soul == 10);
    assert(view.equipped_blue_soul == 11 && view.equipped_yellow_soul == 12);
    assert(view.equipped_armor == 13 && view.equipped_accessory == 14);
    assert(view.current_experience == 123456 && view.current_gold == 654321);

    write_u32(bytes.staged_room_pointer, 0, 0x02000000);
    assert(aria_state_decode(&bytes, &view));
    assert(!view.staged_arrival_plausible);
    assert(view.values_plausible && view.gameplay_state_ready);
    write_u32(bytes.staged_room_pointer, 0, 0x0850ef9c);

    bytes.control_flags = 0;
    assert(aria_state_decode(&bytes, &view));
    assert(view.values_plausible && !view.gameplay_state_ready);
    bytes.control_flags = 1u << 1;
    bytes.gameplay[0] = 0;
    assert(aria_state_decode(&bytes, &view));
    assert(view.values_plausible && !view.gameplay_state_ready);
    bytes.gameplay[0] = 1;

    bytes.player_entity[0x6d] = UINT8_MAX;
    assert(aria_state_decode(&bytes, &view));
    assert(view.values_plausible && !view.gameplay_state_ready);
    bytes.player_entity[0x6d] = 12;
    write_u32(bytes.player_entity, 0x40, 0xfff00000);
    write_u32(bytes.player_entity, 0x44, 0xffe00000);
    assert(aria_state_decode(&bytes, &view));
    assert(view.x_position_fixed == 0 && view.y_position_fixed == 0);
    assert(view.values_plausible && !view.gameplay_state_ready);
    write_u32(bytes.player_entity, 0x40, 0x00640000);
    write_u32(bytes.player_entity, 0x44, 0x00c80000);

    write_u32(bytes.player_pointer, 0, 0x020005ed);
    assert(aria_state_decode(&bytes, &view));
    assert(!view.player_entity_valid && !view.values_plausible);
    assert(!view.gameplay_state_ready);
    write_u32(bytes.player_pointer, 0, 0x020005ec);
    write_u16(bytes.progression, 0x14, 501);
    assert(aria_state_decode(&bytes, &view));
    assert(view.player_entity_valid && !view.values_plausible);
    assert(!view.gameplay_state_ready);

    bytes.mode[0] = 2;
    write_u32(bytes.player_pointer, 0, 0);
    assert(aria_state_decode(&bytes, &view));
    assert(!view.gameplay_active && view.values_plausible);
    assert(!view.gameplay_state_ready);

    puts("Aria state decoder tests passed.");
    return 0;
}
