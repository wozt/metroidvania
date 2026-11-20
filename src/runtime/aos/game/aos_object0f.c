/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_object0f.h"

/* sub_0802D5EC(n): (rng & (n - 1)) - (n >> 1) — a centred jitter. */
static int32_t jitter(uint32_t (*rng)(void), uint32_t n) {
    return (int32_t)((*rng)() & (n - 1)) - (int32_t)(n >> 1);
}

void aos_object0f_create(AosObject0f *o, int param0, int param1) {
    *o = (AosObject0f){0};
    o->palette_row = (uint8_t)(param0 >> 8);
    if (!o->palette_row) o->palette_row = 7;
    o->param0 = (uint8_t)(param0 & 0xFF);
    o->param1 = param1;
    o->song = -1;
    /* + 0x5C = 0x14, + 0x58 &= 0xBF, animation 0xE, attack-box preset
     * 0x080CF8FA, + 0x72 bit 0x02 and the sub_08050F08 frame callback
     * are native setup the runtime approximates through the exported
     * special_15 art. */
}

void aos_object0f_update(AosObject0f *o, bool near, bool hit,
                         int32_t fx, int32_t fy,
                         uint32_t (*rng)(void), bool mp_full_soma) {
    o->fx = fx;
    o->fy = fy;
    ++o->anim_tick;         /* sub_08050F08 frame step, once per update */
    o->song = -1;
    o->particle = false;
    o->burst = false;
    /* sub_08045F9C's drop is a per-frame output like the song: it
     * latches only on the accepted hit frame, so the caller must not
     * see it again on the following breaking frames. */
    o->drop_kind = AOS_OBJECT0F_DROP_NONE;
    o->drop_big = false;
    o->drop_variant = 0;
    o->drop_item = 0;
    if (!near) {
        /* Probe miss: + 0x58 bit 0x20 and the update ends. */
        o->flags58 |= 0x20;
        return;
    }
    if (o->state == 0) {
        o->flags58 &= ~0x21;
        if (hit) {
            /* sub_08045F9C(entity, 0, + 0x30, + 0x32): the attack-box
             * match already passed, so the drop dispatch runs on this
             * frame — the pickup spawns the moment the lamp is hit.
             * Selector = + 0x30's low byte (param0 & 0xFF), param =
             * + 0x32 (param1). gEwramData + 0x13266 reads 0 in the
             * port, so the selector-forcing gate never fires and case
             * 0's money branch needs only mp_full_soma. */
            uint8_t selector = o->param0;
            int param = o->param1;
            switch (selector) {
            case 0:
                if (mp_full_soma) {
                    o->drop_kind = AOS_OBJECT0F_DROP_MONEY;
                    o->drop_variant = (uint8_t)(((*rng)() & 1) + param);
                } else {
                    o->drop_kind = AOS_OBJECT0F_DROP_HEART;
                    o->drop_big = param != 0;
                }
                break;
            case 1:
                o->drop_kind = AOS_OBJECT0F_DROP_MONEY;
                o->drop_variant = (uint8_t)param;
                break;
            case 2:
                o->drop_kind = AOS_OBJECT0F_DROP_ITEM;
                o->drop_item = param;
                break;
            default:
                break;
            }
            o->song = 0xA2;
            o->flags58 |= 0x20;
            o->timer = 0x28;
            o->state = 1;
            o->burst = true;    /* break flourish, one shot */
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
