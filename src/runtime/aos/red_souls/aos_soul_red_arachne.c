/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_red_arachne.h"

enum { AOS_ARACHNE_SHARD_GRAVITY = 0x4000 };

AosArachneSoul aos_arachne_create(int32_t soma_x, int32_t soma_y,
                                  bool facing_left, uint8_t parameter,
                                  bool releases, int32_t vx, int32_t vy,
                                  int32_t ay) {
    return (AosArachneSoul){
        .x = soma_x + (int32_t)(facing_left ? -8 : 8) * 0x10000,
        .y = soma_y - 0x1A * 0x10000,
        .vx = facing_left ? -vx : vx,
        .vy = vy,
        .ay = ay,
        .parameter = parameter,
        .releases = releases,
        .facing_left = facing_left};
}

/* sub_0806D3D8: speed += acceleration, capped at the limit only on
 * the acceleration's side (a zero acceleration caps the positive
 * side alone), then the position moves. */
static void move_axis(int32_t *position, int32_t *speed, int32_t acceleration) {
    *speed += acceleration;
    if (acceleration >= 0) {
        if (*speed > AOS_ARACHNE_LIMIT) *speed = AOS_ARACHNE_LIMIT;
    } else if (*speed < -AOS_ARACHNE_LIMIT) {
        *speed = -AOS_ARACHNE_LIMIT;
    }
    *position += *speed;
}

bool aos_arachne_update(AosArachneSoul *soul, bool outside, bool anim_done) {
    /* The tail runs sub_0806D128(e, 2) in every state: the margin or
     * the + 0x59 bit-4 mark deletes immediately. */
    if (outside) return false;
    if (soul->state == 0) {
        /* State 0's init (resource load, sub_0803F2C8 sequence, box
         * preset 5 + sub_0802DB84, song, count) ran at creation; the
         * native falls into state 1's movement on the same frame. */
        soul->state = 1;
    }
    if (soul->state == 1) {
        move_axis(&soul->x, &soul->vx, 0);
        move_axis(&soul->y, &soul->vy, soul->ay);
        if (soul->planted) {
            if (soul->releases) {
                /* Mudman: PlaySong 0xD2, three shards, sequence 2. */
                soul->released = true;
                soul->state = 3;
            } else {
                /* Arachne: linger 0x10 frames, blink, +0x64 = 5. */
                soul->timer = 0x10;
                soul->blink = true;
                soul->frame5 = true;
                soul->state = 2;
            }
        }
        return true;
    }
    if (soul->state == 2) {
        /* The child's +0x24 or an exhausted +0x0D deletes. */
        if (soul->host_gone || soul->timer == 0) return false;
        --soul->timer;
        return true;
    }
    if (soul->state == 3) {
        /* The child's +0x24 or the sequence-2 animation end deletes. */
        if (soul->host_gone || anim_done) return false;
        return true;
    }
    return true;
}

AosArachneShard aos_arachne_shard_create(int32_t x, int32_t y,
                                         int32_t parent_vx,
                                         uint32_t draw_frame,
                                         uint32_t draw_vx,
                                         uint32_t draw_vy) {
    /* -+0x48/2 with the native floor rounding (vx is even here). */
    int32_t base = -parent_vx / 2;
    return (AosArachneShard){
        .x = x,
        .y = y,
        .vx = base + (int32_t)(((draw_vx & 0xFF) - 0x80) << 9),
        .vy = (int32_t)((draw_vy & 0x7FFF) - 0x8000),
        .frame = (uint8_t)((draw_frame & 1) + 10),
        .alive = true};
}

bool aos_arachne_shard_update(AosArachneShard *shard, bool outside) {
    if (!shard->alive) return false;
    /* +0x40 += +0x48, +0x48 += +0x50 (0); +0x44 += +0x4C,
     * +0x4C += +0x54. */
    shard->x += shard->vx;
    shard->y += shard->vy;
    shard->vy += AOS_ARACHNE_SHARD_GRAVITY;
    if (outside) {
        shard->alive = false;
        return false;
    }
    return true;
}
