/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free tests for the Aria of Sorrow Soma motion rules. */
#include "aos_soma.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static AosSoma grounded(void) {
    return (AosSoma){.x = AOS_FIXED(100), .y = AOS_FIXED(200)};
}

/* Runs a held-jump arc until Soma falls back to the start height. */
static int jump_arc(uint16_t held_frames, int32_t *peak) {
    AosSoma soma = grounded();
    int32_t start = soma.y;
    assert(aos_soma_jump(&soma, 0, AOS_KEY_JUMP));
    *peak = soma.y;
    for (int frame = 0; frame < 600; ++frame) {
        aos_soma_integrate(&soma);
        if (soma.y < *peak) *peak = soma.y;
        if (soma.y >= start && frame > 0) return frame;
        aos_soma_air(&soma, NULL, frame < held_frames ? AOS_KEY_JUMP : 0);
    }
    return -1;
}

enum { W = 64, H = 96 };
static uint8_t cells[W * H];
static const AosCollision layer = {2, 3, W, H, cells};

static void fill(int x0, int y0, int x1, int y1, uint8_t value) {
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) cells[y * W + x] = value;
}

static AosLanding frame(AosSoma *soma, uint16_t held, uint16_t pressed) {
    return aos_soma_update(soma, &layer, held, pressed);
}

static AosSoma at(int x, int y, uint32_t flags) {
    return (AosSoma){.x = AOS_FIXED(x), .y = AOS_FIXED(y), .flags = flags};
}

static void collision_tests(void) {
    /* Solid ground: cell rows 20+ (top surface at pixel 160). */
    fill(0, 20, W - 1, H - 1, 0x03);

    /* A short fall lands with the feet on pixel 159. */
    AosSoma soma = at(100, 120, AOS_FLAG_AIRBORNE);
    AosLanding landing = AOS_LANDING_NONE;
    for (int i = 0; i < 60 && landing == AOS_LANDING_NONE; ++i)
        landing = frame(&soma, 0, 0);
    assert(landing == AOS_LANDING_NORMAL);
    assert(soma.y == AOS_FIXED(159) && soma.vy == 0 && soma.state == 0);
    assert((soma.flags & AOS_FLAG_GROUNDED) && !(soma.flags & AOS_FLAG_AIRBORNE));
    assert(!(soma.flags & AOS_FLAG_PLATFORM_ONLY));
    /* Standing still keeps the contact and the height. */
    for (int i = 0; i < 10; ++i) assert(frame(&soma, 0, 0) == AOS_LANDING_NONE);
    assert(soma.y == AOS_FIXED(159) && (soma.flags & AOS_FLAG_GROUNDED));

    /* Falling faster than 6.25 pixels per frame is a hard landing. */
    fill(0, 20, W - 1, 79, 0x00);
    fill(0, 80, W - 1, H - 1, 0x03);
    soma = at(100, 40, AOS_FLAG_AIRBORNE);
    landing = AOS_LANDING_NONE;
    for (int i = 0; i < 200 && landing == AOS_LANDING_NONE; ++i)
        landing = frame(&soma, 0, 0);
    assert(landing == AOS_LANDING_HARD && soma.state == 4);
    assert(soma.y == AOS_FIXED(639) && (soma.flags & AOS_FLAG_HARD_LANDING));
    fill(0, 20, W - 1, 79, 0x03);

    /* A wall at cells x 30+ stops the body 8 pixels before it and turns
     * the velocity into a quarter bounce. The push is in whole pixels and
     * keeps the subpixel fraction, as the game adds push << 16. */
    fill(30, 0, W - 1, 19, 0x03);
    soma = at(220, 159, AOS_FLAG_GROUNDED);
    for (int i = 0; i < 30; ++i) frame(&soma, AOS_KEY_RIGHT, 0);
    assert((soma.x >> 16) == 231 && (soma.flags & AOS_FLAG_WALL));
    soma.x = AOS_FIXED(231);
    soma.vx = AOS_WALK_SPEED;
    aos_soma_integrate(&soma);
    aos_soma_collide(&soma, &layer);
    assert(soma.x == AOS_FIXED(231.5) && soma.vx == -AOS_WALK_SPEED / 4);
    /* With flag 0x80 the wall stops Soma instead. */
    soma.flags |= AOS_FLAG_STOP_AT_WALL;
    soma.vx = AOS_WALK_SPEED;
    aos_soma_integrate(&soma);
    aos_soma_collide(&soma, &layer);
    assert((soma.x >> 16) == 231 && soma.vx == 0);
    fill(30, 0, W - 1, 19, 0x00);

    /* Ceiling at cell row 12 (bottom at pixel 103): a jump from the floor
     * bumps the head and turns back down at +0.0625. */
    fill(0, 12, W - 1, 12, 0x03);
    soma = at(100, 159, AOS_FLAG_GROUNDED);
    frame(&soma, AOS_KEY_JUMP, AOS_KEY_JUMP);
    /* The state routine applies the first gravity in the jump frame. */
    assert(soma.vy == AOS_JUMP_VELOCITY + 0x2000 + 0x1A00);
    int32_t top = soma.y;
    bool bumped = false;
    for (int i = 0; i < 20; ++i) {
        frame(&soma, AOS_KEY_JUMP, 0);
        if (soma.y < top) top = soma.y;
        if (soma.vy > 0 && soma.vy < 0x4000 && !bumped) bumped = true;
    }
    assert(bumped && (top >> 16) - 32 >= 103 - 8);
    fill(0, 12, W - 1, 12, 0x00);

    /* One-way platform 0x01 on cell row 15 (top at pixel 120): a jump rises
     * through it, the fall lands on it. */
    fill(8, 15, 20, 15, 0x01);
    soma = at(100, 159, AOS_FLAG_GROUNDED);
    frame(&soma, AOS_KEY_JUMP, AOS_KEY_JUMP);
    landing = AOS_LANDING_NONE;
    for (int i = 0; i < 120 && landing == AOS_LANDING_NONE; ++i)
        landing = frame(&soma, AOS_KEY_JUMP, 0);
    assert(landing == AOS_LANDING_NORMAL && soma.y == AOS_FIXED(119));
    assert(soma.flags & AOS_FLAG_PLATFORM_ONLY);
    /* Walking off its right edge (cell 20 ends at pixel 167) falls. */
    for (int i = 0; i < 60 && (soma.flags & AOS_FLAG_GROUNDED); ++i)
        frame(&soma, AOS_KEY_RIGHT, 0);
    assert(!(soma.flags & AOS_FLAG_GROUNDED) && (soma.flags & AOS_FLAG_AIRBORNE));
    assert((soma.x >> 16) - 5 > 167);
    landing = AOS_LANDING_NONE;
    for (int i = 0; i < 60 && landing == AOS_LANDING_NONE; ++i)
        landing = frame(&soma, 0, 0);
    assert(landing == AOS_LANDING_NORMAL && soma.y == AOS_FIXED(159));
    fill(8, 15, 20, 15, 0x00);

    /* Walking down a 45 degree slope keeps the ground contact: cells
     * 0x41 descend to the right (the height grows downward with x),
     * stacked one per row. */
    fill(0, 20, W - 1, H - 1, 0x00);
    for (int i = 0; i < 8; ++i) {
        cells[(12 + i) * W + 10 + i] = 0x41;
        fill(0, 12 + i, 9 + i, 12 + i, 0x03);
    }
    fill(0, 20, W - 1, H - 1, 0x03);
    soma = at(70, 95, AOS_FLAG_AIRBORNE);
    landing = AOS_LANDING_NONE;
    for (int i = 0; i < 30 && landing == AOS_LANDING_NONE; ++i)
        landing = frame(&soma, 0, 0);
    assert(landing == AOS_LANDING_NORMAL);
    int air = 0;
    for (int i = 0; i < 70; ++i) {
        frame(&soma, AOS_KEY_RIGHT, 0);
        if (!(soma.flags & AOS_FLAG_GROUNDED)) ++air;
        /* On the slope the feet follow its surface pixel by pixel. */
        if (soma.x >= AOS_FIXED(85) && soma.x < AOS_FIXED(140))
            assert((soma.y >> 16) - (soma.x >> 16) >= 14 &&
                   (soma.y >> 16) - (soma.x >> 16) <= 16);
    }
    assert(air == 0 && soma.x > AOS_FIXED(160) && soma.y == AOS_FIXED(159));
    /* Back up the slope to the left: slope step 1 slows vx to
     * 1.5 / 24 * 16 = 1.0 pixel per frame. */
    int uphill = 0;
    for (int i = 0; i < 100; ++i) {
        int32_t before = soma.x;
        frame(&soma, AOS_KEY_LEFT, 0);
        assert(soma.flags & AOS_FLAG_GROUNDED);
        if (soma.x - before == -AOS_FIXED(1)) ++uphill;
    }
    assert(uphill > 40 && soma.y == AOS_FIXED(95));
    memset(cells, 0, sizeof(cells));
}

static void state_tests(void) {
    fill(0, 20, W - 1, H - 1, 0x03);

    /* Down crouches: no steering, friction 0.15625 brakes the walk. */
    AosSoma soma = at(100, 159, AOS_FLAG_GROUNDED);
    for (int i = 0; i < 4; ++i) frame(&soma, AOS_KEY_RIGHT, 0);
    assert(soma.vx == AOS_WALK_SPEED);
    frame(&soma, AOS_KEY_RIGHT | AOS_KEY_DOWN, 0);
    assert(soma.flags & AOS_FLAG_CROUCH);
    frame(&soma, AOS_KEY_RIGHT | AOS_KEY_DOWN, 0);
    assert(soma.vx == AOS_WALK_SPEED - 0x2800);
    for (int i = 0; i < 20; ++i) frame(&soma, AOS_KEY_RIGHT | AOS_KEY_DOWN, 0);
    assert(soma.vx == 0 && (soma.flags & AOS_FLAG_CROUCH));
    frame(&soma, 0, 0);
    assert(!(soma.flags & AOS_FLAG_CROUCH));

    /* A ceiling at y - 20 keeps Soma crouched and y - 33 blocks jumps. */
    fill(10, 16, 14, 17, 0x03);           /* bottom at pixel 143 */
    soma = at(100, 159, AOS_FLAG_GROUNDED);
    frame(&soma, AOS_KEY_JUMP, AOS_KEY_JUMP);
    assert(soma.flags & AOS_FLAG_LOW_CEILING);
    assert((soma.flags & AOS_FLAG_CROUCH) && (soma.flags & AOS_FLAG_GROUNDED));
    assert(soma.vy == 0 && soma.y == AOS_FIXED(159));
    fill(10, 16, 14, 17, 0x00);

    /* Down + jump on a one-way platform drops through it. */
    fill(8, 15, 20, 15, 0x01);
    soma = at(100, 119, AOS_FLAG_GROUNDED | AOS_FLAG_PLATFORM_ONLY);
    frame(&soma, 0, 0);
    assert((soma.flags & AOS_FLAG_GROUNDED) && (soma.flags & AOS_FLAG_PLATFORM_ONLY));
    frame(&soma, AOS_KEY_DOWN | AOS_KEY_JUMP, AOS_KEY_JUMP);
    assert(soma.drop_timer == 16 && (soma.flags & AOS_FLAG_AIRBORNE));
    assert(soma.vy > 0);
    AosLanding landing = AOS_LANDING_NONE;
    for (int i = 0; i < 60 && landing == AOS_LANDING_NONE; ++i)
        landing = frame(&soma, AOS_KEY_DOWN, 0);
    assert(landing == AOS_LANDING_NORMAL && soma.y == AOS_FIXED(159));
    fill(8, 15, 20, 15, 0x00);

    /* Hard landing: state 4 brakes by 0.1875 and keeps Soma crouched until
     * the animation player reports the end of the landing animation. */
    soma = at(100, 120, AOS_FLAG_AIRBORNE);
    soma.vy = AOS_FIXED(7);
    soma.vx = AOS_FIXED(1);
    landing = AOS_LANDING_NONE;
    for (int i = 0; i < 20 && landing == AOS_LANDING_NONE; ++i)
        landing = frame(&soma, 0, 0);
    assert(landing == AOS_LANDING_HARD && soma.state == 4);
    for (int i = 0; i < 30; ++i) frame(&soma, AOS_KEY_RIGHT, AOS_KEY_JUMP);
    assert(soma.state == 4 && soma.vx == 0 && (soma.flags & AOS_FLAG_CROUCH));
    assert(soma.y == AOS_FIXED(159));
    soma.flags |= AOS_FLAG_ANIM_DONE;
    frame(&soma, 0, 0);
    assert(soma.state == 0 && !(soma.flags & AOS_FLAG_HARD_LANDING));
    frame(&soma, 0, 0);
    assert(!(soma.flags & AOS_FLAG_CROUCH));
    memset(cells, 0, sizeof(cells));
}

int main(void) {
    collision_tests();
    state_tests();
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
    assert(!aos_soma_jump(&soma, 0, 0));
    assert(aos_soma_jump(&soma, 0, AOS_KEY_JUMP));
    assert(soma.vy == AOS_JUMP_VELOCITY && soma.air_frames == 16);
    assert(soma.flags == AOS_FLAG_AIRBORNE);
    soma.air_frames = 3;
    assert(aos_soma_jump(&soma, 0, AOS_KEY_JUMP));
    soma.air_frames = 4;
    assert(!aos_soma_jump(&soma, 0, AOS_KEY_JUMP));
    soma = grounded();
    soma.flags = AOS_FLAG_HEAVY;
    assert(aos_soma_jump(&soma, 0, AOS_KEY_JUMP) && soma.vy == AOS_SLOWED_JUMP_VELOCITY);

    /* Leaving a ledge: vy 0, gravity_mod -0.0625, then the first gravity. */
    soma = grounded();
    soma.vy = AOS_FIXED(3);
    aos_soma_air(&soma, NULL, 0);
    assert(soma.flags & AOS_FLAG_AIRBORNE);
    /* 0 + 0.125 + 0.1015625 then -0.0625 gravity_mod, which grows by 1/64. */
    assert(soma.vy == 0x2000 + 0x1A00 - 0x1000);
    assert(soma.gravity_mod == -0x1000 + 0x400 && soma.air_frames == 1);

    /* First airborne frame after a jump: only the gravity terms. */
    soma = grounded();
    aos_soma_jump(&soma, 0, AOS_KEY_JUMP);
    aos_soma_air(&soma, NULL, AOS_KEY_JUMP);
    assert(soma.vy == AOS_JUMP_VELOCITY + 0x2000 + 0x1A00);
    assert(soma.air_frames == 16 && soma.gravity_mod == 0);

    /* Releasing jump while rising fast clamps vy to -0.25. */
    soma = grounded();
    aos_soma_jump(&soma, 0, AOS_KEY_JUMP);
    aos_soma_air(&soma, NULL, 0);
    assert(soma.vy == (int32_t)0xFFFFC000 + 0x2000 + 0x1A00);
    assert(soma.gravity_mod == (int32_t)0xFFFFE000);

    /* Apex float: holding jump near vy 0 lowers gravity_mod by 1/32 a
     * frame (net 1/64 after gravity) until the -0.125 clamp. */
    soma = grounded();
    soma.flags = AOS_FLAG_AIRBORNE;
    soma.vy = 0x1000;
    for (int i = 0; i < 10; ++i) {
        soma.vy = 0x1000;
        aos_soma_air(&soma, NULL, AOS_KEY_JUMP);
    }
    assert(soma.gravity_mod == (int32_t)0xFFFFE000 + 0x400);

    /* Slow fall subtracts 0.15625 above 0.15625 before gravity. */
    soma = grounded();
    soma.flags = AOS_FLAG_AIRBORNE;
    soma.vy = AOS_FIXED(2);
    soma.abilities = AOS_ABILITY_SLOW_FALL;
    aos_soma_air(&soma, NULL, 0);
    assert(soma.vy == AOS_FIXED(2) - 0x2800 + 0x1A00);

    /* Heavy flag: +0.375 gravity and gravity_mod reset (r4 = 0). */
    soma = grounded();
    soma.flags = AOS_FLAG_AIRBORNE | AOS_FLAG_HEAVY;
    soma.vy = AOS_FIXED(1);
    soma.gravity_mod = 0x800;
    aos_soma_gravity(&soma, NULL);
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
