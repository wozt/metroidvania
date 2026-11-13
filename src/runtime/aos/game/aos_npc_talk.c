/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_npc_talk.h"

static void talk_reset(AosNpcTalk *t) {
    t->arm_42c_1 = false;
    t->window_text = -1;
    t->window_close = false;
    t->entity_release = false;
}

void aos_npc_talk_17_update(AosNpcTalk *t, bool text_done) {
    talk_reset(t);
    if (t->state == 0) {
        t->arm_42c_1 = true;
        t->window_text = 0x17;
        t->state = 1;
    } else if (t->state == 1) {
        if (text_done)
            t->entity_release = true;   /* sub_0805B66C(1) */
    }
}

void aos_npc_talk_18_update(AosNpcTalk *t, bool text_done) {
    talk_reset(t);
    if (t->state == 0) {
        t->arm_42c_1 = true;
        t->window_text = 0x18;
        t->state = 1;
    } else if (t->state == 1) {
        if (text_done) {
            t->window_close = true;     /* sub_08067898 */
            t->flags59 |= 0x8;
        }
    } else if (t->state == 0x80) {
        t->entity_release = true;
    }
}

bool aos_npc_talk_arm_flag(void) {
    /* +0x42C |= 1; the ROM returns the new word (nonzero). */
    return true;
}
