/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_object0d.h"

void aos_object0d_create(AosObject0d *o, int param0, int param1,
                         bool *deleted) {
    *o = (AosObject0d){0};
    o->param0 = (uint8_t)param0;
    o->param1 = (uint8_t)param1;
    o->song = -1;
    /* sub_0806E0D0(0x081CBE74, 0x08209F54, 0x0821017C) + the
     * sub_0803E654 variant pick: covered by the exported special_13
     * art; the native failure path deletes the entity. */
    *deleted = false;
}

/* The shared re-arm: PlaySong 0x112 (0x89 << 1), + 0x1C = -+0x100 by
 * the x compare, + 0x20 = 0x100. */
static void rearm(AosObject0d *o, bool soma_left) {
    o->song = 0x112;
    o->swing = soma_left ? -0x100 : 0x100;
    o->speed = 0x100;
}

void aos_object0d_update(AosObject0d *o, int16_t screen_y, bool focused,
                         bool attacked, bool soma_left) {
    o->song = -1;
    o->registered = true;   /* sub_08021278(entity, 0) every update */
    if (screen_y > 0xA8) o->flags58 |= 0x20;
    else o->flags58 &= ~0x21;

    /* + 0xB focus bookkeeping: init once while focused, re-arm when
     * the focus leaves. */
    if (!o->unkB) {
        if (focused) {
            rearm(o, soma_left);
            o->unkB = true;
        }
    } else if (!focused) {
        o->unkB = false;
    }

    /* The attack path re-arms without touching + 0xB. */
    if (attacked) rearm(o, soma_left);

    /* The swing integration. */
    if (o->angle > 0) {
        o->swing -= 0x10;
        o->angle += o->swing;
        if (o->angle <= 0 && o->speed) o->speed -= 0x20;
    } else if (o->angle < 0) {
        o->swing += 0x10;
        o->angle += o->swing;
        if (o->angle >= 0 && o->speed) o->speed -= 0x20;
    } else {
        o->angle = o->swing;
    }
    if (o->swing > o->speed) o->swing = o->speed;
    else if (o->swing < -o->speed) o->swing = -o->speed;
    if (o->angle > 0x1000) o->angle = 0x1000;
    else if (o->angle < -0x1000) o->angle = -0x1000;
}
