/* SPDX-License-Identifier: GPL-3.0-only */
#include "core/roundtrip_guard.h"

#include <string.h>

bool fusion_roundtrip_exclusive(const GbaRuntime *active,
                                const GbaRuntime *suspended)
{
    return active && suspended && active != suspended &&
           active->active && !suspended->active;
}

bool fusion_roundtrip_snapshot_equal(const GbaRuntimeSnapshot *a,
                                     const GbaRuntimeSnapshot *b)
{
    return a && b && a->data && b->data && a->size &&
           a->size == b->size && !memcmp(a->data, b->data, a->size);
}

/* Compare named, decoded fields instead of memcmp on a padded C struct. */
bool fusion_roundtrip_aria_view_equal(const AriaStateView *a,
                                      const AriaStateView *b)
{
    return a && b && a->game_mode == b->game_mode &&
           a->game_mode_stage == b->game_mode_stage &&
           a->in_game_phase == b->in_game_phase &&
           a->in_game_phase_stage == b->in_game_phase_stage &&
           a->player_control_enabled == b->player_control_enabled &&
           a->area == b->area &&
           a->room == b->room &&
           a->staged_camera_x == b->staged_camera_x &&
           a->staged_camera_y == b->staged_camera_y &&
           a->staged_player_x == b->staged_player_x &&
           a->staged_player_y == b->staged_player_y &&
           a->staged_room_pointer == b->staged_room_pointer &&
           a->staged_arrival_plausible == b->staged_arrival_plausible &&
           a->player_entity_address == b->player_entity_address &&
           a->player_entity_valid == b->player_entity_valid &&
           a->camera_x_fixed == b->camera_x_fixed &&
           a->camera_y_fixed == b->camera_y_fixed &&
           a->x_position_fixed == b->x_position_fixed &&
           a->y_position_fixed == b->y_position_fixed &&
           a->x_velocity_fixed == b->x_velocity_fixed &&
           a->y_velocity_fixed == b->y_velocity_fixed &&
           a->animation_flags == b->animation_flags &&
           a->animation_id == b->animation_id &&
           a->animation_frame == b->animation_frame &&
           a->animation_counter == b->animation_counter &&
           a->current_character == b->current_character &&
           a->current_level == b->current_level &&
           a->current_hp == b->current_hp &&
           a->current_mp == b->current_mp &&
           a->max_hp == b->max_hp &&
           a->max_mp == b->max_mp &&
           a->equipped_weapon == b->equipped_weapon &&
           a->equipped_red_soul == b->equipped_red_soul &&
           a->equipped_blue_soul == b->equipped_blue_soul &&
           a->equipped_yellow_soul == b->equipped_yellow_soul &&
           a->equipped_armor == b->equipped_armor &&
           a->equipped_accessory == b->equipped_accessory &&
           a->current_experience == b->current_experience &&
           a->current_gold == b->current_gold &&
           a->gameplay_active == b->gameplay_active &&
           a->values_plausible == b->values_plausible &&
           a->gameplay_state_ready == b->gameplay_state_ready;
}

/* FNV-1a across every pixel, all channels, in a consistent byte order. */
bool fusion_roundtrip_frame_hash(const GbaFrameView *frame, uint64_t *out)
{
    uint64_t hash = UINT64_C(1469598103934665603);
    uint32_t x, y;
    if (!frame || !out || !frame->rgba32 ||
        frame->width != GBA_FRAME_WIDTH ||
        frame->height != GBA_FRAME_HEIGHT ||
        frame->stride_pixels < frame->width)
        return false;
    for (y = 0; y < frame->height; ++y) {
        for (x = 0; x < frame->width; ++x) {
            uint32_t pixel = frame->rgba32[(size_t)y * frame->stride_pixels + x];
            unsigned b;
            for (b = 0; b < 4; ++b) {
                hash ^= (pixel >> (8u * b)) & 0xffu;
                hash *= UINT64_C(1099511628211);
            }
        }
    }
    *out = hash;
    return true;
}
