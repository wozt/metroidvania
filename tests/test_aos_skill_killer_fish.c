/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free deterministic tests for SkillKillerFishUse. */
#include "aos_skill_killer_fish.h"
#include "aos_audio.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t random_values[] = {0x12345, 0x54321};
static size_t random_index;

static uint32_t fixed_random(void) {
    return random_values[random_index++ % 2];
}

static AosCollision collision(uint8_t cells[64 * 64]) {
    return (AosCollision){2, 2, 64, 64, cells, NULL, false};
}

static void creates_and_falls_outside_water(void) {
    uint8_t cells[64 * 64] = {0};
    AosCollision layer = collision(cells);
    aos_audio_clear();
    AosKillerFishSoul fish = aos_killer_fish_soul_create(100 << 16, 120 << 16, false);
    AosKillerFishEffect effect;
    assert(fish.x == 120 << 16 && fish.y == 108 << 16);
    assert(fish.vx == 0x28000 && fish.facing_left);
    assert(aos_killer_fish_soul_update(&fish, &layer, NULL, 0, false,
                                       fixed_random, 0, 0, 4000, 4000, &effect));
    assert(fish.state == AOS_KILLER_FISH_APPEARING);
    assert(fish.x == 120 << 16 && !effect.spawn);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x19C) == 1);

    assert(aos_killer_fish_soul_update(&fish, &layer, NULL, 0, true,
                                       fixed_random, 0, 0, 4000, 4000, &effect));
    assert(fish.state == AOS_KILLER_FISH_AIRBORNE && fish.vertical_flip);
    assert(fish.vx == 0x14000 && fish.vy == -0x38000 && fish.ay == 0x8000);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x1AD) == 1);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0xDB) == 1);

    int32_t x = fish.x, y = fish.y;
    random_index = 0;
    for (int frame = 0; frame < 3; ++frame)
        assert(aos_killer_fish_soul_update(&fish, &layer, NULL, 0, false,
                                           fixed_random, 0, 0, 4000, 4000, &effect));
    assert(fish.x == x + 3 * 0x14000);
    assert(fish.y == y - 0x30000 - 0x28000 - 0x20000);
    assert(effect.spawn);
    assert(effect.x == fish.x + 0x12345 - 0x40000);
    assert(effect.y == fish.y + 0x54321 - 0x40000);
}

static void swims_toward_nearest_target(void) {
    uint8_t cells[64 * 64];
    memset(cells, 8, sizeof cells);
    AosCollision layer = collision(cells);
    aos_audio_clear();
    AosKillerFishSoul fish = aos_killer_fish_soul_create(100 << 16, 120 << 16, true);
    AosKillerFishTarget targets[] = {
        {true, 20 << 16, 30 << 16},
        {true, 40 << 16, 116 << 16},
        {false, 30 << 16, 100 << 16},
    };
    AosKillerFishEffect effect;
    assert(fish.x == 80 << 16 && fish.vx == -0x28000 && !fish.facing_left);
    assert(aos_killer_fish_soul_update(&fish, &layer, targets, 3, true,
                                       NULL, 0, 0, 4000, 4000, &effect));
    assert(fish.state == AOS_KILLER_FISH_SWIMMING);
    assert(fish.vx == -0x5000 && fish.vy == 0x1000);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0xDA) == 1);
    int32_t x = fish.x, y = fish.y;
    assert(aos_killer_fish_soul_update(&fish, &layer, targets, 3, false,
                                       NULL, 0, 0, 4000, 4000, &effect));
    assert(fish.x == x - 0x5000 && fish.y == y + 0x1000);
    assert(fish.state == AOS_KILLER_FISH_SWIMMING && !effect.spawn);
}

static void ends_on_wall_and_after_margin(void) {
    uint8_t cells[64 * 64];
    memset(cells, 8, sizeof cells);
    AosCollision layer = collision(cells);
    AosKillerFishSoul fish = aos_killer_fish_soul_create(100 << 16, 120 << 16, false);
    AosKillerFishEffect effect;
    assert(aos_killer_fish_soul_update(&fish, &layer, NULL, 0, true,
                                       NULL, 0, 0, 4000, 4000, &effect));
    int wall_x = ((fish.x + fish.vx) >> 16) + 8;
    int wall_y = fish.y >> 16;
    cells[(wall_y >> 3) * 64 + (wall_x >> 3)] = 3;
    assert(aos_killer_fish_soul_update(&fish, &layer, NULL, 0, false,
                                       NULL, 0, 0, 4000, 4000, &effect));
    assert(fish.state == AOS_KILLER_FISH_ENDING && fish.hidden && effect.spawn);
    assert(!aos_killer_fish_soul_update(&fish, &layer, NULL, 0, false,
                                        NULL, 0, 0, 4000, 4000, &effect));

    fish = aos_killer_fish_soul_create(100 << 16, 120 << 16, false);
    assert(aos_killer_fish_soul_update(&fish, &layer, NULL, 0, false,
                                       NULL, 1000, 1000, 16, 16, &effect));
    assert(fish.state == AOS_KILLER_FISH_ENDING && fish.hidden);
    assert(!aos_killer_fish_soul_update(&fish, &layer, NULL, 0, false,
                                        NULL, 0, 0, 16, 16, &effect));
}

int main(void) {
    creates_and_falls_outside_water();
    swims_toward_nearest_target();
    ends_on_wall_and_after_margin();
    puts("aos_skill_killer_fish: OK (ROM-free source-derived water and air paths)");
    return 0;
}
