/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>

#include "../src/runtime/aos/blue_souls/aos_soul_blue_alastor.h"

static uint32_t next_rand = 0x82;
static uint32_t test_random(void) { return next_rand; }

static AosStats stats;
static AosBlueSoulRecord blue[AOS_BLUE_SOUL_RECORDS];
static AosGuardian guardian;
static uint32_t soma_flags;
static AosAlastorGuardian ala;
static AosSkillTarget targets[AOS_ALASTOR_TARGETS];

static bool step(void) {
    return aos_alastor_guardian_update(&ala, &guardian, &stats, blue,
                                       AOS_KEY_R, 0, &soma_flags,
                                       100 << 16, 200 << 16,
                                       100 << 16, 160 << 16,
                                       true, targets, test_random);
}

int main(void) {
    blue[23] = (AosBlueSoulRecord){0x080314AD, 3, 2, 0, 0};
    stats = (AosStats){.blue_soul = 23, .mp = 80, .max_mp = 80};
    guardian = (AosGuardian){.soul = 23, .state = 1};
    soma_flags = 0x100000;
    ala = (AosAlastorGuardian){0};
    targets[0] = (AosSkillTarget){true, 150 << 16, 100 << 16, 10};

    /* Cast: the source's pixel position copies with y - 0x10, the
     * spin takes Soma's facing sign then inverts the sprite bit,
     * song 0x19E fires, and state 1 begins. */
    assert(step());
    assert(ala.state == 1 && ala.song_19e && ala.x == 100 << 16 &&
           ala.y == (200 << 16) - (0x10 << 16) && ala.vel == -0x100 &&
           !ala.facing_left);

    /* State 1: + 0x2C += + 0x2E each frame while + 0x28 ramps by
     * 0x1000; past 0xFFFF -> state 2. Sixteen adds reach 0x10000,
     * the seventeenth update transitions. */
    int16_t angle0 = ala.angle;
    for (int i = 0; i < 16; ++i) {
        assert(step());
        assert(ala.state == 1 && ala.scale == (uint32_t)(i + 1) * 0x1000);
    }
    assert(ala.angle == (int16_t)(angle0 - 0x100 * 16));
    assert(step());
    assert(ala.state == 2 && ala.scale == 0x10000);

    /* State 2: (rng & 0x7F) != 0 homes once (zeroing + 0x48/+ 0x4C),
     * counts + 0xD and lands in state 3 with the sin wobble. */
    next_rand = 0x82;
    assert(step());
    assert(ala.state == 3 && ala.tick == 1 && ala.vx == 0 && ala.vy == 0 &&
           ala.attacking);

    /* State 3: the aim is 24 px away (< 48), so it keeps wobbling —
     * + 0x4C rewritten each frame, position drifts by it. */
    int32_t y0 = ala.y;
    next_rand = 0x82;
    assert(step());
    assert(ala.state == 3 && ala.tick == 2 && ala.y != y0);

    /* A low roll (rng & 0x7F <= 1) enters state 4: the nearest homing
     * slot is picked and + 0x34 randomized. */
    next_rand = 1;
    assert(step());
    assert(ala.state == 4);
    next_rand = 5;
    assert(step());
    assert(ala.state == 5 && ala.target == 0 && ala.offset == -(5 << 16) &&
           ala.attacking);

    /* State 5 is a single frame natively: + 0x72 bit 1 clears, + 0x2C
     * gains + 0x2E * 4 and it falls back to state 2. */
    int16_t before = ala.angle;
    next_rand = 0x82;
    assert(step());
    assert(ala.state == 2 && !ala.attacking &&
           ala.angle == (int16_t)(before + (ala.vel << 2)));

    /* State 4 with no living target returns to state 2 and reloads
     * + 0xD = 0x80. */
    targets[0].hp = 0;
    next_rand = 0x82;
    assert(step()); /* state 2 -> 3 */
    assert(step()); /* state 3 */
    next_rand = 1;
    assert(step()); /* state 3 -> 4 */
    assert(step()); /* state 4: no target */
    assert(ala.state == 2 && ala.tick == 0x80 && ala.target == -1);
    targets[0].hp = 10;

    /* State 2 -> 3. */
    next_rand = 0x82;
    assert(step());
    assert(ala.state == 3);

    /* Upkeep: the toggled soul ends on a second R press — while the
     * state is below 6 it enters state 6, whose shrink-out begins on
     * the same update. */
    assert(aos_alastor_guardian_update(&ala, &guardian, &stats, blue,
                                     AOS_KEY_R, AOS_KEY_R, &soma_flags,
                                     100 << 16, 200 << 16,
                                     100 << 16, 160 << 16,
                                     true, targets, test_random));
    assert(ala.state == 6);

    /* State 6: + 0x2C += + 0x2E << 4 and + 0x28 drains by 0x1000. */
    uint32_t scale0 = ala.scale;
    int16_t a0 = ala.angle;
    assert(step());
    assert(ala.state == 6 && ala.scale == scale0 - 0x1000 &&
           ala.angle == (int16_t)(a0 + (ala.vel << 4)));

    /* The drain reaches zero -> sub_0802DD84 and the entity is
     * deleted on the following update: step() returns false. */
    for (int i = 0; i < 64 && step() && !ala.dead; ++i) {
    }
    assert(ala.dead);

    puts("test_aos_soul_blue_alastor: ok");
    return 0;
}
