/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_object0f.h"

/* sub_0802D5EC(n): (rng & (n - 1)) - (n >> 1) — a centred jitter. */
static int32_t jitter(uint32_t (*rng)(void), uint32_t n) {
    return (int32_t)((*rng)() & (n - 1)) - (int32_t)(n >> 1);
}

void aos_object0f_create(AosObject0f *o, int param0) {
    *o = (AosObject0f){0};
    o->palette_row = (uint8_t)(param0 >> 8);
    if (!o->palette_row) o->palette_row = 7;
    o->param0 = (uint8_t)(param0 & 0xFF);
    o->song = -1;
    /* + 0x5C = 0x14, + 0x58 &= 0xBF, animation 0xE, attack-box preset
     * 0x080CF8FA and the sub_08050F08 frame callback are native setup
     * the runtime approximates through the exported special_15 art. */
}

void aos_object0f_update(AosObject0f *o, bool near, bool hit,
                         int32_t fx, int32_t fy,
                         uint32_t (*rng)(void)) {
    o->fx = fx;
    o->fy = fy;
    o->song = -1;
    o->particle = false;
    if (!near) {
        /* Probe miss: + 0x58 bit 0x20 and the update ends. */
        o->flags58 |= 0x20;
        return;
    }
    if (o->state == 0) {
        o->flags58 &= ~0x21;
        if (hit) {
            o->song = 0xA2;
            o->flags58 |= 0x20;
            o->timer = 0x28;
            o->state = 1;
        }
        return;
    }
    if (o->state == 1) {
        if (o->timer > 0x1E && !(o->timer & 3)) {
            /* sub_08045CEC drift: position jittered by
             * sub_0802D5EC(0x80000) on both axes, palette from the
             * + 0x5A draw byte (unmodelled, kept 0). */
            o->particle = true;
            o->particle_x = fx + jitter(rng, 0x80000);
            o->particle_y = fy + jitter(rng, 0x80000);
            /* ( + 0x5A << 0x19 ) >> 0x1C = bits 3-6 of the draw byte. */
            o->particle_palette = (o->unk5a >> 3) & 0xF;
        }
        o->timer = (uint8_t)(o->timer - 1);
        if (o->timer == 0xFF) o->finished = true;   /* + 0x59 bit 3 */
    }
}
