/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_red_dryad.h"

#include "../../aos/core/aos_audio.h"

enum { AOS_DRYAD_LIMIT = 0x40000 };

AosDryad aos_dryad_create(int32_t soma_x, int32_t soma_y, bool facing_left) {
    return (AosDryad){
        .x = (int32_t)((uint32_t)(int16_t)((soma_x >> 16) + (facing_left ? -0x14 : 0x14)) << 16),
        .y = (int32_t)((uint32_t)(int16_t)((soma_y >> 16) - 0x1A) << 16),
        .vx = facing_left ? -0x10000 : 0x10000,
        .vy = -0x20000,
        .ay = 0x2000,
        .facing_left = facing_left};
}

/* sub_0806D3D8: speed += acceleration, capped at the limit on the
 * acceleration's side, then the position moves. */
static void move_axis(int32_t *position, int32_t *speed, int32_t acceleration) {
    *speed += acceleration;
    if (acceleration >= 0) {
        if (*speed > AOS_DRYAD_LIMIT) *speed = AOS_DRYAD_LIMIT;
    } else if (*speed < -AOS_DRYAD_LIMIT) {
        *speed = -AOS_DRYAD_LIMIT;
    }
    *position += *speed;
}

bool aos_dryad_update(AosDryad *dryad) {
    /* The tail runs sub_0806D128(e, 4) in every state: the margin or
     * the + 0x59 bit-4 mark enters state 4, whose case deletes. */
    if (dryad->state == 4) return false;
    if (dryad->outside) {
        dryad->state = 4;
        return true;
    }
    if (dryad->state == 0) {
        /* sub_08042A54(1, 0x14), box preset 5 plus 0x0202FFFF and song
         * 0xE5 ran at creation; the still frame 0 (+ 0x65 = 0) needs no
         * further setup. */
        dryad->state = 1;
        return true;
    }
    if (dryad->state == 1) {
        move_axis(&dryad->x, &dryad->vx, 0);
        move_axis(&dryad->y, &dryad->vy, dryad->ay);
        if (dryad->planted) {
            /* sub_0803F2C8(e, 0, 0, 0) restarts sequence 0; the strike
             * callback already set + 0x0D = 0x78 and the 0x1014F8F6
             * box. */
            dryad->state = 2;
        }
        return true;
    }
    if (dryad->state == 2) {
        /* child +0x24 (the host's + 0x34 ran out) deletes; otherwise
         * the first planted frame plays 0xD5 and grants the +8 bonus,
         * the counter counts to 0x14 and the host's + 0x72 bit 2 ends
         * it; only then does the animation-end check run. */
        if (dryad->host_gone) {
            dryad->state = 4;
            return true;
        }
        if (dryad->counter == 0) {
            aos_audio_play_song(0xD5);
            dryad->bonus = true;    /* sub_08021FEC(8): +8 and popup */
        }
        ++dryad->counter;
        if (dryad->counter > 0x14 || dryad->host_flag) {
            dryad->state = 4;
            return true;
        }
        if (dryad->anim_done) {
            dryad->timer = 0x10;
            dryad->state = 3;
        }
        return true;
    }
    if (dryad->state == 3) {
        dryad->blink = true;        /* + 0x0F = 1 */
        if (dryad->timer == 0) {
            dryad->state = 4;
            return true;
        }
        --dryad->timer;
        return true;
    }
    return true;
}
