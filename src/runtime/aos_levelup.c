/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_levelup.h"

void aos_levelup_start(AosLevelUp *display) {
    display->frame = 0;
    display->env_frame = 0;
    display->active = true;
}

bool aos_levelup_tick(AosLevelUp *display) {
    if (!display->active) return false;
    display->env_frame = display->frame;
    if (display->frame <= AOS_LEVELUP_DURATION) display->frame++;
    /* Odd post-increment counters up to 0x27 burst one sparkle. */
    bool sparkle = (display->frame & 1) && display->frame <= 0x27;
    if (display->frame > AOS_LEVELUP_DURATION) display->active = false;
    return sparkle;
}

bool aos_levelup_frozen(const AosLevelUp *display) {
    return display->active && display->frame >= 2 &&
           display->frame <= AOS_LEVELUP_DURATION;
}

int32_t aos_levelup_angle(uint8_t frame, AosLevelUpSin sine) {
    if (frame > 0xF) return 0x10000;
    return 0x10000 - sine((uint32_t)frame * 0x400 + 0x4000);
}

int32_t aos_levelup_scale(uint8_t frame, AosLevelUpSin sine) {
    if (frame <= 7) return 0x2000;
    if (frame <= 0xF) return 0x10000 - sine((uint32_t)frame * 0x800);
    return 0x10000;
}

/* sub_08045270: vel = (sine(angle) * 24) >> 4 truncating toward zero;
 * acc = -vel >> 6 with the native +0x3F floor correction. Both use the
 * GBA pattern "if negative add 2^bits - 1, then arithmetic shift",
 * which truncates toward zero. */
static int32_t trunc_shift(int64_t value, unsigned bits) {
    if (value < 0) value += (int64_t)((1u << bits) - 1);
    return (int32_t)(value >> bits);
}

void aos_levelup_sparkle_velocity(uint8_t random_byte, AosLevelUpSin sine,
                                  int32_t out[4]) {
    uint32_t angle = (uint32_t)random_byte << 8;
    int64_t sx = sine(angle + 0x4000);   /* cos of the launch angle */
    int64_t sy = sine(angle);
    int32_t vx = trunc_shift(sx * 24, 4);
    int32_t vy = trunc_shift(sy * 24, 4);
    out[0] = vx;
    out[1] = vy;
    out[2] = trunc_shift(-(int64_t)vx, 6);
    out[3] = trunc_shift(-(int64_t)vy, 6);
}
