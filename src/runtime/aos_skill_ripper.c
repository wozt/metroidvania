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
                            bool outside, const AosRipperTarget *target) {
    if (!soul || soul->state == AOS_RIPPER_REMOVED) return false;
    if (!soul->started) {
        /* Source state 0: after successful art and palette initialization. */
        soul->started = true;
        aos_audio_play_song(0xE6);
    }
    if (soul->state == AOS_RIPPER_FLYING && soul->attached) {
        /* Skill_EvilButcher_Ripper_Use state 1 with sub_080239E4 alive:
         * variant 0 ends immediately; variant 1 follows the struck enemy
         * until its post-incremented helper age passes 20. */
        if (outside || !target || !target->active || target->defeated || soul->variant == 0 ||
            soul->attached_age++ > 20) {
            soul->state = AOS_RIPPER_REMOVED;
            return false;
        }
        soul->x = add32(target->x, soul->target_dx);
        soul->y = add32(target->y, soul->target_dy);
        if (soul->timer == 0) {
            soul->state = AOS_RIPPER_FADING;
            soul->timer = 15;
        } else {
            --soul->timer;
        }
        return true;
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

void aos_ripper_soul_attach(AosRipperSoul *soul, int32_t target_x,
                            int32_t target_y) {
    if (!soul || soul->state != AOS_RIPPER_FLYING || soul->attached) return;
    soul->attached = true;
    soul->attached_age = 0;
    soul->timer = 0x78;
    soul->target_dx = (int32_t)((uint32_t)soul->x - (uint32_t)target_x);
    soul->target_dy = (int32_t)((uint32_t)soul->y - (uint32_t)target_y);
}
