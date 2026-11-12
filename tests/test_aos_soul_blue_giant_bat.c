/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>

#include "../src/runtime/aos/blue_souls/aos_soul_blue_giant_bat.h"

static uint32_t next_rand = 0x40;
static uint32_t test_random(void) { return next_rand; }

static AosStats stats;
static AosBlueSoulRecord blue[AOS_BLUE_SOUL_RECORDS];
static AosGuardian guardian;
static uint32_t soma_flags;
static AosSoma soma;
static AosGiantBatGuardian bat;

static bool step(void) {
    return aos_giant_bat_guardian_update(&bat, &guardian, &stats, blue,
                                         AOS_KEY_R, 0, &soma_flags, &soma,
                                         test_random);
}

int main(void) {
    blue[2] = (AosBlueSoulRecord){0x0802EB85, 30, 2, 0, 0};
    stats = (AosStats){.blue_soul = 2, .mp = 300, .max_mp = 300};
    guardian = (AosGuardian){.soul = 2, .state = 1};
    soma_flags = 0x100000;
    soma = (AosSoma){
        .x = 100 << 16, .y = 200 << 16,
        .vx = 0x80000, .vy = -0x90000,          /* over the +-0x40000 clamp */
        .friction = 0x1234, .gravity_mod = 0x5678,
        /* bit 0 survives the 0x010001A0 delete mask and is cleared by
         * the init's 0x21 mask; bit 18 is untouched. */
        .flags = 0x40001, .transform_step = 7,
    };
    bat = (AosGiantBatGuardian){0};

    /* First update: velocities clamp to +-4 px/frame, + 0x50/+ 0x54
     * zero, + 0x10 bits 0x21 clear, + 0xB zero, song 0x8F, + 0xD = 0x10
     * and one sparkle already spawns (the + 0xD block runs after the
     * init on the same update). */
    assert(step());
    assert(bat.cast && bat.song_8f && bat.timer == 0xF && bat.fx);
    assert(soma.vx == 0x40000 && soma.vy == -0x40000 && soma.friction == 0 &&
           soma.gravity_mod == 0 && !(soma.flags & 0x21) &&
           soma.transform_step == 0 && (soma.abilities & 0x100));

    /* The sparkle orbits Soma: angle = (rng & 0xFF) << 8 with a1 =
     * angle + 0x4000 and a2 = angle; offsets are sin * 4. */
    next_rand = 0x40; /* angle 0x4000: s1 = sin(0x8000) = 0? check below */
    int32_t sx = soma.x, sy = soma.y;
    assert(step());
    assert(bat.timer == 0xE && bat.fx);
    /* angle = 0x40 << 8 = 0x4000; a1 = 0x8000 -> sin = 0 (16.16);
     * a2 = 0x4000 -> sin = 0x10000 -> fx_y = y - 0x10px + 4px. */
    assert(bat.fx_x == sx && bat.fx_y == sy - (0x10 << 16) + (4 << 16));
    assert(bat.fx_vx == 0 && bat.fx_vy == 0x20000);

    /* Sixteen frames total: the cast plus the checked frame plus
     * fourteen more, then the fx stops once + 0xD reaches zero. */
    for (int i = 0; i < 14; ++i) assert(step() && bat.fx);
    assert(bat.timer == 0);
    assert(step() && !bat.fx);

    /* The + 0x10 mask 0x010001A0 deletes: bit 5 set -> gone. */
    soma.flags |= 0x20;
    assert(!step());
    assert(bat.dead);

    /* Upkeep: a toggled soul ends on a second R press -> delete. */
    bat = (AosGiantBatGuardian){0};
    soma.flags = 0;
    guardian.state = 1;
    assert(step());
    assert(aos_giant_bat_guardian_update(&bat, &guardian, &stats, blue,
                                         AOS_KEY_R, AOS_KEY_R, &soma_flags,
                                         &soma, test_random) == false);
    assert(bat.dead);

    puts("test_aos_soul_blue_giant_bat: ok");
    return 0;
}
