/* SPDX-License-Identifier: GPL-3.0-only */
#include "core/roundtrip_guard.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    GbaRuntime metroid = {0};
    GbaRuntime aria = {0};
    uint8_t first[] = {1, 2, 3, 4};
    uint8_t copy[] = {1, 2, 3, 4};
    uint8_t changed[] = {1, 2, 3, 5};
    GbaRuntimeSnapshot original = {first, sizeof(first)};
    GbaRuntimeSnapshot matching = {copy, sizeof(copy)};
    GbaRuntimeSnapshot mismatch = {changed, sizeof(changed)};
    GbaRuntimeSnapshot short_state = {copy, sizeof(copy) - 1};
    GbaRuntimeSnapshot empty = {0};

    assert(!fusion_roundtrip_exclusive(NULL, &aria));
    assert(!fusion_roundtrip_exclusive(&metroid, NULL));
    assert(!fusion_roundtrip_exclusive(&metroid, &metroid));
    assert(!fusion_roundtrip_exclusive(&metroid, &aria));
    metroid.active = true;
    assert(fusion_roundtrip_exclusive(&metroid, &aria));
    aria.active = true;
    assert(!fusion_roundtrip_exclusive(&metroid, &aria));
    metroid.active = false;
    assert(fusion_roundtrip_exclusive(&aria, &metroid));
    aria.active = false;
    assert(!fusion_roundtrip_exclusive(&aria, &metroid));

    assert(fusion_roundtrip_snapshot_equal(&original, &matching));
    assert(!fusion_roundtrip_snapshot_equal(&original, &mismatch));
    assert(!fusion_roundtrip_snapshot_equal(&original, &short_state));
    assert(!fusion_roundtrip_snapshot_equal(&original, &empty));
    assert(!fusion_roundtrip_snapshot_equal(NULL, &matching));
    {
        static uint32_t pixels[GBA_FRAME_WIDTH * GBA_FRAME_HEIGHT] = {0};
        GbaFrameView frame = {pixels, GBA_FRAME_WIDTH, GBA_FRAME_HEIGHT,
                              GBA_FRAME_WIDTH};
        uint64_t hash_before = 0;
        uint64_t hash_after = 0;
        AriaStateView before = {0};
        AriaStateView after = {0};
        before.game_mode = after.game_mode = 4;
        before.in_game_phase = after.in_game_phase = 1;
        before.gameplay_state_ready = after.gameplay_state_ready = true;
        before.current_hp = after.current_hp = 320;
        before.max_hp = after.max_hp = 320;
        before.player_entity_address = after.player_entity_address =
            UINT32_C(0x020004e4);
        assert(fusion_roundtrip_aria_view_equal(&before, &after));
        after.current_hp--;
        assert(!fusion_roundtrip_aria_view_equal(&before, &after));
        after.current_hp++;
        after.player_control_enabled = true;
        assert(!fusion_roundtrip_aria_view_equal(&before, &after));
        assert(!fusion_roundtrip_aria_view_equal(NULL, &after));
        assert(fusion_roundtrip_frame_hash(&frame, &hash_before));
        pixels[0] = 1;
        assert(fusion_roundtrip_frame_hash(&frame, &hash_after));
        assert(hash_after != hash_before);
        frame.stride_pixels = GBA_FRAME_WIDTH - 1;
        assert(!fusion_roundtrip_frame_hash(&frame, &hash_after));
        assert(!fusion_roundtrip_frame_hash(NULL, &hash_after));
        frame.stride_pixels = GBA_FRAME_WIDTH;
        assert(!fusion_roundtrip_frame_hash(&frame, NULL));
    }
    assert(fusion_roundtrip_switch_action(&metroid, &aria, false) ==
           FUSION_ROUNDTRIP_SWITCH_REJECTED);
    metroid.active = true;
    assert(fusion_roundtrip_switch_action(&metroid, &aria, false) ==
           FUSION_ROUNDTRIP_SWITCH_ARRIVAL);
    assert(fusion_roundtrip_switch_action(&metroid, &aria, true) ==
           FUSION_ROUNDTRIP_SWITCH_RESUME);
    aria.active = true;
    assert(fusion_roundtrip_switch_action(&metroid, &aria, true) ==
           FUSION_ROUNDTRIP_SWITCH_REJECTED);
    metroid.active = false;
    assert(fusion_roundtrip_switch_action(&aria, &metroid, true) ==
           FUSION_ROUNDTRIP_SWITCH_RESUME);
    assert(fusion_roundtrip_switch_action(&aria, &aria, false) ==
           FUSION_ROUNDTRIP_SWITCH_REJECTED);
    aria.active = false;
    puts("Round-trip guards passed.");
    return 0;
}
