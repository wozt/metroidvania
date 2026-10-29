/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free deterministic tests for SkillGladiatorUse. */
#include "aos_skill_gladiator.h"
#include "aos_audio.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { W = 64, H = 32 };
static uint8_t cells[W * H];
static const AosCollision layer = {2, 1, W, H, cells, NULL, false};
/* 0x084F13C8: one wall probe, ceiling -14, floor 14, half width 12. */
static const AosProbes probes = {1, -14, 14, 12, {0}};
static uint32_t draws;
static uint32_t scripted(void) { return draws++ * 0x11111; }

static void flight_and_landing(void) {
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    aos_audio_clear();
    AosGladiatorSoul soul = aos_gladiator_soul_create(100 << 16, 146 << 16, false);
    assert(soul.body.x == 108 << 16 && soul.body.y == 120 << 16);
    assert(soul.stored_vx == -0x20000 && soul.body.vy == -0x28000 &&
           soul.body.ay == 0x2000 && !soul.facing_left);
    AosGladiatorDust dust;
    assert(aos_gladiator_soul_update(&soul, &layer, &probes, scripted,
                                      0, 0, 32, 32, &dust));
    assert(!dust.spawn && soul.state == AOS_GLADIATOR_MOVING);
    assert(soul.body.x == 110 << 16 && soul.body.vy == -0x26000);
    assert(soul.stored_vx == -0x1FE00 && soul.angle == 0x07F8);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0xC5) == 1);
    bool landed = false;
    for (int i = 0; i < 80 && soul.state == AOS_GLADIATOR_MOVING; ++i) {
        int before = aos_audio_requested(AOS_AUDIO_START_SONG, 0x15F);
        assert(aos_gladiator_soul_update(&soul, &layer, &probes, scripted,
                                         0, 0, 32, 32, &dust));
        if (aos_audio_requested(AOS_AUDIO_START_SONG, 0x15F) > before) landed = true;
    }
    assert(landed);
}

static void mirror_wall_and_end(void) {
    memset(cells, 0, sizeof cells);
    AosGladiatorSoul left = aos_gladiator_soul_create(100 << 16, 146 << 16, true);
    assert(left.body.x == 92 << 16 && left.stored_vx == 0x20000 && left.facing_left);
    AosGladiatorDust dust;
    assert(aos_gladiator_soul_update(&left, &layer, &probes, scripted,
                                      0, 0, 32, 32, &dust));
    assert(left.body.x == 90 << 16 && left.angle == (uint16_t)-0x07F8);

    for (int y = 0; y < H; ++y) cells[y * W + 16] = 0x03;
    AosGladiatorSoul wall = aos_gladiator_soul_create(100 << 16, 146 << 16, false);
    aos_audio_clear();
    bool bounced = false;
    for (int i = 0; i < 20 && !bounced; ++i) {
        assert(aos_gladiator_soul_update(&wall, &layer, &probes, scripted,
                                         0, 0, 32, 32, &dust));
        bounced = wall.stored_vx > 0;
    }
    assert(bounced && aos_audio_requested(AOS_AUDIO_START_SONG, 0x15F) == 1);

    AosGladiatorSoul ending = aos_gladiator_soul_create(100 << 16, 146 << 16, false);
    ending.state = AOS_GLADIATOR_MOVING;
    ending.stored_vx = -0x100;
    assert(aos_gladiator_soul_update(&ending, NULL, NULL, scripted,
                                      0, 0, 32, 32, &dust));
    assert(ending.state == AOS_GLADIATOR_ENDING && ending.timer == 0x10 &&
           ending.blinking);
    draws = 0;
    int particles = 0, alive = 0;
    do {
        bool keep = aos_gladiator_soul_update(&ending, NULL, NULL, scripted,
                                               0, 0, 32, 32, &dust);
        particles += dust.spawn;
        alive += keep;
        if (!keep) break;
    } while (1);
    assert(alive == 16 && particles == 5 && draws == 10);
    assert(!aos_gladiator_soul_update(NULL, NULL, NULL, scripted,
                                      0, 0, 0, 0, &dust));
}

int main(void) {
    flight_and_landing();
    mirror_wall_and_end();
    puts("aos_skill_gladiator: OK (ROM-free source-derived rolling soul)");
    return 0;
}
