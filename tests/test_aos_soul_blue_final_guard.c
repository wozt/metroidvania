/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>

#include "../src/runtime/aos/blue_souls/aos_soul_blue_final_guard.h"

static uint32_t test_random(void) { return 0; }

static AosStats stats;
static AosBlueSoulRecord blue[AOS_BLUE_SOUL_RECORDS];
static AosGuardian guardian;
static uint32_t soma_flags;
static AosSoma soma;
static AosFinalGuardGuardian guard;

static bool step(void) {
    return aos_final_guard_guardian_update(&guard, &guardian, &stats, blue,
                                           AOS_KEY_R, 0, &soma_flags, &soma,
                                           test_random);
}

int main(void) {
    blue[7] = (AosBlueSoulRecord){0x0802FC5D, 60, 1, 0, 0};
    stats = (AosStats){.blue_soul = 7, .mp = 300, .max_mp = 300};
    guardian = (AosGuardian){.soul = 7, .state = 1};
    soma_flags = 0x100000;
    /* Grounded (bit 20), nothing else of the mask: idle state 0. */
    soma = (AosSoma){
        .x = 100 << 16, .y = 200 << 16,
        .flags = 0x100000 | 0x40000,
        .state = 0, .facing_left = true,
    };
    guard = (AosFinalGuardGuardian){0};

    /* Init: Soma's state is forced to 0xA, his 0x35 sequence is
     * requested, + 0x10 bits 0x400/0x1000000 clear, song 0x1A7 and
     * phase 1. Only 0x400 can be set here — bit 24 is inside the
     * init mask and would refuse the cast. */
    soma.flags |= 0x400;
    assert(step());
    assert(guard.phase == 1 && guard.song_1a7);
    assert(soma.state == 0xA && soma.anim_request == 0x35);
    assert(!(soma.flags & 0x400) && (soma.flags & 0x40000));

    /* Phase 1 only advances. */
    assert(step());
    assert(guard.phase == 2);

    /* Phase 2: the mask re-check passes — abilities get 0x4000,
     * aux_bits 0x820, and the guardian rides Soma's pixel position
     * with y + 4 and his facing. */
    soma.x = (120 << 16) + 0x1234; /* the fraction is dropped */
    soma.y = 80 << 16;
    soma.facing_left = false;
    assert(step());
    assert(guard.phase == 2 && (soma.abilities & 0x4000) &&
           guard.aux_bits == 0x820);
    assert(guard.x == 120 << 16 && guard.y == (84 << 16) &&
           !guard.facing_left);

    /* Leaving the ground (airborne bit) deletes the guardian. */
    soma.flags |= 0x2;
    assert(!step());
    assert(guard.dead);

    /* Init refused while Soma is not idle: state != 0 deletes. */
    guard = (AosFinalGuardGuardian){0};
    soma.flags = 0x100000;
    soma.state = 3;
    assert(!step());
    assert(guard.dead);

    /* Upkeep: the held soul ends when R is released -> delete. */
    guard = (AosFinalGuardGuardian){0};
    soma.state = 0;
    assert(step()); /* init, held R */
    assert(step()); /* phase 2 */
    assert(aos_final_guard_guardian_update(&guard, &guardian, &stats, blue,
                                           0, 0, &soma_flags, &soma,
                                           test_random) == false);
    assert(guard.dead);

    puts("test_aos_soul_blue_final_guard: ok");
    return 0;
}
