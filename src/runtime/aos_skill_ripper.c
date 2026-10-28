/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_skill_ripper.h"

#include "aos_audio.h"

/* Preserve GBA-style 32-bit wrapping without signed-overflow UB. */
static int32_t add32(int32_t a, int32_t b) {
    return (int32_t)((uint32_t)a + (uint32_t)b);
}

AosRipperSoul aos_ripper_soul_create(int32_t soma_x, int32_t soma_y,
                                    bool facing_left, uint8_t variant, bool rebound) {
    AosRipperSoul soul = {0};
    if (variant > 1) {
        soul.state = AOS_RIPPER_REMOVED;
        return soul;
    }
    soul.x = (int32_t)((uint32_t)(int16_t)(soma_x >> 16) << 16);
    soul.y = (int32_t)((uint32_t)(int16_t)((soma_y >> 16) - 26) << 16);
    soul.facing_left = facing_left;
    soul.variant = variant;
    soul.rebound = rebound;
    soul.vx = facing_left ? -0x60000 : 0x60000;
    soul.state = AOS_RIPPER_FLYING;
    return soul;
}

bool aos_ripper_soul_update(AosRipperSoul *soul, int16_t wall_delta_pixels,
                            bool outside) {
    if (!soul || soul->state == AOS_RIPPER_REMOVED) return false;
    if (!soul->started) {
        /* Source state 0: after successful art and palette initialization. */
        soul->started = true;
        aos_audio_play_song(0xE6);
    }
    if (soul->state == AOS_RIPPER_FLYING) {
        soul->x = add32(soul->x, soul->vx);
        if (wall_delta_pixels != 0) {
            soul->x = add32(soul->x,
                            (int32_t)((uint32_t)(int32_t)wall_delta_pixels << 16));
            soul->timer = AOS_RIPPER_END_FRAMES;
            /* Native transition stores these even for the fading variant. */
            soul->vx = soul->facing_left ? 0x18000 : -0x18000;
            soul->vy = -0x20000;
            soul->ay = 0x2000;
            if (soul->rebound) {
                soul->state = AOS_RIPPER_REBOUNDING;
                aos_audio_play_song(0x138);  /* _08023D1E, PlaySong */
            } else {
                soul->state = AOS_RIPPER_FADING;
            }
        }
        if (outside) soul->state = AOS_RIPPER_REMOVED;
        return soul->state != AOS_RIPPER_REMOVED;
    }
    if (soul->state == AOS_RIPPER_REBOUNDING) {
        /* _08023D80: fixed-point motion and affine turn. */
        soul->x = add32(soul->x, soul->vx);
        soul->y = add32(soul->y, soul->vy);
        soul->vy = add32(soul->vy, soul->ay);
        soul->angle = (int16_t)((uint16_t)soul->angle +
                                (soul->facing_left ? 1u : 0xFFFFu));
    }
    if (soul->state == AOS_RIPPER_REBOUNDING || soul->state == AOS_RIPPER_FADING) {
        /* _08023DD6 runs in both states, including immediately after a
         * rebounding update; the zero timer enters state 4. */
        soul->blink = soul->timer > 10 ? 1 : 2;
        if (soul->timer == 0) soul->state = AOS_RIPPER_REMOVED;
        else --soul->timer;
        return soul->state != AOS_RIPPER_REMOVED;
    }
    soul->state = AOS_RIPPER_REMOVED;
    return false;
}
