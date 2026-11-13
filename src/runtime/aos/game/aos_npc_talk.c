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

int aos_npc_talk_sprite_gate(uint8_t *anim5c, bool *deleted,
                             bool sprite_variant) {
    if (sprite_variant) {
        *anim5c = 0x11;
        return 1;
    }
    *deleted = true;
    return 0;
}

void aos_npc_talk_release_frames(void) {
    /* sub_0803AFB8(0x0811892C) + sub_0803B980 + sub_0805B66C(1)
     * — reported through the caller's release path. */
}

void aos_npc_talk_arm_fade(void) {
    /* +0x42C |= 1 then sub_0803D270/sub_0803CED4. */
}

void aos_npc_talk_mark_done(uint8_t *flags59) {
    *flags59 |= 0x8;
}

int aos_npc_talk_sound_release(void) {
    /* sub_0805B878(0x1020), sub_08013EEC(0x25), sub_0805B66C(1). */
    return 0;
}

bool aos_npc_talk_counter_tick(uint8_t *ctr_c, uint8_t *ctr_d,
                               uint8_t arg, bool *wrote_417) {
    *wrote_417 = false;
    if (*ctr_c == 0) {
        *wrote_417 = true;      /* gEwramData + 0x417 = 1 */
        *ctr_d = arg;
        (*ctr_c)++;
        return true;
    }
    if (*ctr_c == 1) {
        *ctr_d = (uint8_t)(*ctr_d - 1);
        if (*ctr_d == 0xFF)
            (*ctr_c)++;
        return true;
    }
    return false;
}

void aos_npc_talk_657b4(uint8_t *flags59, uint32_t *flag42c,
                        bool *called_0ef6c, uint16_t *song) {
    if (!(*flag42c & 0x4000u)) {
        *flags59 |= 0x8;
        return;
    }
    *flag42c |= 0x20u | 0x10000u;
    *called_0ef6c = true;   /* sub_0800EF6C */
    *song = 0x1000;         /* PlaySong(0x80 << 5) */
}

uint32_t aos_npc_talk_65fe4(uint8_t state_a, int32_t field48,
                            uint32_t field10) {
    if (state_a != 0) return 1;
    int32_t mag = field48 < 0 ? -(int32_t)field48 : field48;
    if (mag > 0x8000) return 1;
    return field10 & 0x30010000u;
}
