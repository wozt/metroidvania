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
    puts("Round-trip guards passed.");
    return 0;
}
