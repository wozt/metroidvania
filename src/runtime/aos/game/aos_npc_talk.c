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

void aos_npc_talk_659a4(AosNpcSpawn *s, bool gate_12078,
                        bool keep_5b754, uint16_t cam_x, uint16_t cam_y,
                        int32_t self_x, int32_t player_x,
                        uint8_t player_anim5c) {
    if (gate_12078) {
        s->flags59 |= 0x8;
        return;
    }
    if (!keep_5b754) {
        s->deleted = true;
        return;
    }
    s->anim_stepped = true;         /* sub_0803F17C */
    s->field42 = (int16_t)(0x60 - cam_x);
    s->field46 = (int16_t)(0xA0 - cam_y);
    s->flags58 = (uint8_t)((s->flags58 & ~0x40u) |
                           (self_x < player_x ? 0x40u : 0u));
    s->anim5c = (uint8_t)(player_anim5c - 1);
    s->flag42c |= 1;
}
uint32_t aos_npc_talk_65a5c(AosNpcGuide *g, bool text_done,
                            int16_t player_x_px, int32_t player_x,
                            uint8_t player_flags58, bool *player_bit6,
                            bool *flag_417) {
    g->flag_r7 = 0;
    g->window_open = g->wrote_417 = g->text_poll = g->state_80 =
        g->song_1040 = g->flag_3b7_set = g->flag_3b7_clear =
        g->player_x_clamped = g->player_facing_set = g->release =
        false;
    if (g->state_a == 0) {
        /* sub_08013CF0(0xD) + sub_08067934(0x28) window. */
        g->window_open = true;
        *flag_417 = true;               /* gEwramData + 0x417 = 1 */
        g->wrote_417 = true;
        g->state_a = 1;
    }
    if (g->state_a == 1) {
        g->text_poll = true;            /* sub_08067A04 */
        if (text_done) {
            g->state_a = 0x80;
            g->state_80 = true;
        }
        if (g->ctr_b == 0) {
            if (g->ctr_c == 0) {
                *flag_417 = true; g->flag_3b7_set = true;
                g->ctr_c = 1;
                g->timer_d = 0x30;
                goto case_c1_a;         /* ROM fallthrough */
            }
            if (g->ctr_c == 1) {
            case_c1_a:
                g->timer_d = (uint8_t)(g->timer_d - 1);
                if (g->timer_d != 0xFF) goto tail;
                g->ctr_c = 2;           /* _08065BB0 */
            } else if (g->ctr_c == 2) {
                if (player_x_px <= 0xA0) {
                    g->player_x_clamped = true;
                    g->ctr_c = 3;       /* _08065BB0 */
                } else {
                    g->flag_r7 |= 0x20;
                }
            } else if (g->ctr_c == 3) {
                *flag_417 = false; g->flag_3b7_clear = true;
                goto tail;              /* no +0xC advance */
            }
        } else if (g->ctr_b == 1) {
            if (g->ctr_c == 0) {
                *flag_417 = true; g->flag_3b7_set = true;
                g->vx = 0x8000;
                /* own +0x58 = (own & ~0x41) | (player & 0x40) */
                g->flags58 = (uint8_t)((g->flags58 & ~0x41u) |
                                       (player_flags58 & 0x40));
                if (g->flags6d) {
                    /* sub_0803F2C8(e, 0, 0, 1) — engine-side. */
                }
                g->ctr_c = 1;
                goto case_c1_b;         /* ROM fallthrough */
            }
            if (g->ctr_c == 1) {
            case_c1_b:
                if ((int16_t)(g->x >> 16) > 0xFF) {
                    g->song_1040 = true;    /* sub_0805B878(0x1040) */
                    *flag_417 = false; g->flag_3b7_clear = true;
                    g->ctr_c = 2;           /* _08065BB0 */
                } else {
                    g->x += g->vx;
                    if (g->x <= player_x) {
                        *player_bit6 = true;    /* +0x58 |= 0x40 */
                    } else {
                        *player_bit6 = false;   /* +0x58 &= ~0x41 */
                    }
                    g->player_facing_set = true;
                }
            }
        }
    } else if (g->state_a == 0x80) {
        /* sub_08013960(gEwramData + 0x60), sub_08012048(0x2E),
         * sub_0805B66C(e, 1) — engine-side calls. */
        g->release = true;
    }
tail:
    /* state != 0x80 runs the sub_0803F17C animation step —
     * engine-side, not tracked here. */
    return g->flag_r7;
}

unsigned aos_npc_talk_65c2c(int16_t x_px, uint16_t flags_37e,
                            void (*step)(void *), void *ctx,
                            int16_t *targets) {
    unsigned count = 1;
    if (flags_37e & 0x10) count = 2;
    if (flags_37e & 0x02) count++;
    int16_t original = x_px;
    for (unsigned i = 0; i < count; ++i) {
        int16_t target = i == 0 ? (int16_t)(x_px - 0x10)
                        : i == 1 ? (int16_t)(x_px + 0x10)
                        : i == 2 ? (int16_t)(x_px - 8)
                                 : (int16_t)(x_px + 8);
        if (targets) targets[i] = target;
        if (step) step(ctx);
    }
    (void)original; /* +0x42 restored by the ROM tail */
    return count;
}
