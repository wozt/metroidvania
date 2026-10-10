/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free tests for the Aria of Sorrow Soma motion rules. */
#include "aos_soma.h"

#include <assert.h>
#include <stdio.h>

static AosSoma grounded(void) {
    return (AosSoma){.x = AOS_FIXED(100), .y = AOS_FIXED(200)};
}

/* Runs a held-jump arc until Soma falls back to the start height. */
static int jump_arc(uint16_t held_frames, int32_t *peak) {
    AosSoma soma = grounded();
    int32_t start = soma.y;
    assert(aos_soma_jump(&soma, AOS_KEY_JUMP));
    *peak = soma.y;
    for (int frame = 0; frame < 600; ++frame) {
        aos_soma_integrate(&soma);
        if (soma.y < *peak) *peak = soma.y;
        if (soma.y >= start && frame > 0) return frame;
        aos_soma_air(&soma, frame < held_frames ? AOS_KEY_JUMP : 0, false);
    }
    return -1;
}

int main(void) {
    /* Integration: extra_vx applies once and the fall speed caps at 8. */
    AosSoma soma = grounded();
    soma.vx = AOS_FIXED(1);
    soma.extra_vx = AOS_FIXED(2);
    soma.vy = AOS_FIXED(12);
    aos_soma_integrate(&soma);
    assert(soma.x == AOS_FIXED(103) && soma.extra_vx == 0);
    assert(soma.vy == AOS_FALL_CAP && soma.y == AOS_FIXED(208));

    /* Steering: left wins over right, then friction 0.25 brakes to zero. */
    soma = grounded();
    aos_soma_steer(&soma, AOS_KEY_LEFT | AOS_KEY_RIGHT, AOS_WALK_SPEED);
    assert(soma.vx == -AOS_WALK_SPEED && soma.facing_left);
    aos_soma_steer(&soma, AOS_KEY_RIGHT, AOS_WALK_SPEED);
    assert(soma.vx == AOS_WALK_SPEED && !soma.facing_left);
    int frames = 0;
    while (soma.vx != 0) {
        aos_soma_steer(&soma, 0, AOS_WALK_SPEED);
        ++frames;
    }
    /* Landing exactly on zero keeps the friction until the next frame. */
    assert(frames == 6 && soma.friction == -AOS_FRICTION && !soma.facing_left);
    aos_soma_steer(&soma, 0, AOS_WALK_SPEED);
    assert(soma.vx == 0 && soma.friction == 0);
    soma.vx = (int32_t)0xFFFFA000;              /* -0.375: overshoot stops */
    aos_soma_steer(&soma, 0, AOS_WALK_SPEED);
    assert(soma.vx == -0x2000 && soma.friction == AOS_FRICTION);
    aos_soma_steer(&soma, 0, AOS_WALK_SPEED);
    assert(soma.vx == 0 && soma.friction == 0);

    /* Jump: impulse, flags, coyote window of three airborne frames. */
    soma = grounded();
    soma.flags = 0x400u;
    assert(!aos_soma_jump(&soma, 0));
    assert(aos_soma_jump(&soma, AOS_KEY_JUMP));
    assert(soma.vy == AOS_JUMP_VELOCITY && soma.air_frames == 16);
    assert(soma.flags == AOS_FLAG_AIRBORNE);
    soma.air_frames = 3;
    assert(aos_soma_jump(&soma, AOS_KEY_JUMP));
    soma.air_frames = 4;
    assert(!aos_soma_jump(&soma, AOS_KEY_JUMP));
    soma = grounded();
    soma.flags = AOS_FLAG_HEAVY;
    assert(aos_soma_jump(&soma, AOS_KEY_JUMP) && soma.vy == AOS_SLOWED_JUMP_VELOCITY);

    /* Leaving a ledge: vy 0, gravity_mod -0.0625, then the first gravity. */
    soma = grounded();
    soma.vy = AOS_FIXED(3);
    aos_soma_air(&soma, 0, false);
    assert(soma.flags & AOS_FLAG_AIRBORNE);
    /* 0 + 0.125 + 0.1015625 then -0.0625 gravity_mod, which grows by 1/64. */
    assert(soma.vy == 0x2000 + 0x1A00 - 0x1000);
    assert(soma.gravity_mod == -0x1000 + 0x400 && soma.air_frames == 1);

    /* First airborne frame after a jump: only the gravity terms. */
    soma = grounded();
    aos_soma_jump(&soma, AOS_KEY_JUMP);
    aos_soma_air(&soma, AOS_KEY_JUMP, false);
    assert(soma.vy == AOS_JUMP_VELOCITY + 0x2000 + 0x1A00);
    assert(soma.air_frames == 16 && soma.gravity_mod == 0);

    /* Releasing jump while rising fast clamps vy to -0.25. */
    soma = grounded();
    aos_soma_jump(&soma, AOS_KEY_JUMP);
    aos_soma_air(&soma, 0, false);
    assert(soma.vy == (int32_t)0xFFFFC000 + 0x2000 + 0x1A00);
    assert(soma.gravity_mod == (int32_t)0xFFFFE000);

    /* Apex float: holding jump near vy 0 lowers gravity_mod by 1/32 a
     * frame (net 1/64 after gravity) until the -0.125 clamp. */
    soma = grounded();
    soma.flags = AOS_FLAG_AIRBORNE;
    soma.vy = 0x1000;
    for (int i = 0; i < 10; ++i) {
        soma.vy = 0x1000;
        aos_soma_air(&soma, AOS_KEY_JUMP, false);
    }
    assert(soma.gravity_mod == (int32_t)0xFFFFE000 + 0x400);

    /* Slow fall subtracts 0.15625 above 0.15625 before gravity. */
    soma = grounded();
    soma.flags = AOS_FLAG_AIRBORNE;
    soma.vy = AOS_FIXED(2);
    aos_soma_air(&soma, 0, true);
    assert(soma.vy == AOS_FIXED(2) - 0x2800 + 0x1A00);

    /* Heavy flag: +0.375 gravity and gravity_mod reset (r4 = 0). */
    soma = grounded();
    soma.flags = AOS_FLAG_AIRBORNE | AOS_FLAG_HEAVY;
    soma.vy = AOS_FIXED(1);
    soma.gravity_mod = 0x800;
    aos_soma_gravity(&soma);
    assert(soma.vy == AOS_FIXED(1) + 0x6000 + 0x1A00);
    assert(soma.gravity_mod == 0x400);

    /* A full held jump rises higher and lasts longer than a tapped one. */
    int32_t held_peak, tap_peak;
    int held = jump_arc(600, &held_peak);
    int tap = jump_arc(1, &tap_peak);
    assert(held > tap && tap > 0);
    assert(held_peak < tap_peak);
    printf("aos_soma: held jump %d frames, %.2f px; tapped %d frames, %.2f px\n",
           held, (AOS_FIXED(200) - held_peak) / 65536.0, tap,
           (AOS_FIXED(200) - tap_peak) / 65536.0);
    return 0;
}
