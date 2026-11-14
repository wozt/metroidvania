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

unsigned aos_npc_talk_65c2c_child(AosNpcLoop *g, uint16_t flags_37e,
                                 void (*step)(void *ctx), void *ctx,
                                 int16_t *targets) {
    if (!g->child18)
        return 0;
    return aos_npc_talk_65c2c(g->child18_scr42, flags_37e,
                              step, ctx, targets);
}

void aos_npc_talk_65cd8(AosNpcCompanion *c, bool flag_12078_20,
                        bool flag_12078_2e, bool keep_5b754,
                        int rng_bit, uint8_t player_anim5c,
                        bool (*spawn_child)(void)) {
    if (!flag_12078_20 || !flag_12078_2e || !keep_5b754) {
        c->flags59 |= 0x8;
        c->marked = true;
        return;
    }
    c->variant_a = (rng_bit & 1) == 0;  /* sub_0803F2C8(1|2, 0, 1) */
    c->flags58 |= 0x40;
    c->anim5c = (uint8_t)(player_anim5c + 1);
    if (spawn_child && spawn_child()) {
        c->child = 1;                   /* +0x18 non-NULL */
        c->child_anim = (uint8_t)(c->anim5c + 1);
        c->child_x = c->spawn_x;
        c->child_y = c->spawn_y;
        /* child +4 = sub_08065C2C, +0 = EntityUpdateNothing —
         * engine-side wiring. */
    }
}

void aos_npc_talk_65cd8_commit(const AosNpcCompanion *c,
                               AosNpcLoop *g) {
    g->child18 = c->child != 0;
    if (!g->child18)
        return;
    g->child18_scr42 = c->child_x;
    g->child18_scr46 = c->child_y;
    g->child18_anim5c = c->child_anim;
    /* The create's RNG&1 pick ran sub_0803F2C8(1|2,0,1) on the
     * child: variant_a selects sequence 1, otherwise 2. */
    g->child18_anim = c->variant_a ? 1 : 2;
}
void aos_npc_talk_65db8(AosNpcLoop *g, uint32_t flags,
                        bool text_done, int32_t player_x) {
    g->cleared_42c_200 = g->froze_player = g->window_text =
        g->armed_42c_0 = g->facing_mirror = g->f2c8_3 =
        g->window_close = g->f2c8_2 = g->release_0 =
        g->sound_1041 = g->song_26 = g->wrote_254cc =
        g->sfx_26 = g->ev_1e = false;
    (void)player_x;
    if (g->state == 0) {
        g->cleared_42c_200 = true;      /* +0x42C &= ~0x200 */
        g->state = 1;
        g->ctr_b = g->ctr_c = 0;
    }
    if (g->state == 1) {
        if (!(flags & 1)) return;               /* probe miss */
        if (!(flags & 2)) return;               /* +0x16 & 0x42 == 0 */
        if (!(flags & 4)) return;               /* +0x4BE != 0 */
        g->froze_player = true;
        /* Text priority chain: the sub_0805B864 result selects
         * the 0x32 pair, else flag26/25/2E pick pairs
         * (31/30/2F); a set selector with a clear partner shows
         * the alternate text and arms the partner through
         * sub_08012048. Default stays 0x2D. */
        g->text_id = 0x2D;
        if (flags & 8) {                        /* sub_0805B864 != 0 */
            if (!(flags & 0x10)) {              /* 0x32 clear */
                g->text_id = 0x2C;
            }
        } else if (flags & 0x20) {              /* 0x26 set */
            if (!(flags & 0x40)) g->text_id = 0x2B;   /* 0x31 clear */
        } else if (flags & 0x80) {              /* 0x25 set */
            if (!(flags & 0x100)) g->text_id = 0x2A;  /* 0x30 clear */
        } else if (flags & 0x200) {             /* 0x2E set */
            if (!(flags & 0x400)) g->text_id = 0x29;  /* 0x2F clear */
        }
        g->armed_42c_0 = true;      /* +0x42C |= 1 */
        g->window_text = true;      /* sub_08067934(+0x14) */
        g->facing_mirror = true;    /* +0x58 bit 6 vs player x */
        g->state = 2;
        g->ctr_b = g->ctr_c = 0;
        g->sfx_26 = true;           /* sub_08013E18(0x26) */
        g->ev_1e = true;            /* sub_08013CF0(0x1E) */
        if (g->flags6d == 2) g->f2c8_3 = true;
    } else if (g->state == 2) {
        if (!text_done) return;
        if (g->flags6d == 3) g->f2c8_2 = true;
        g->window_close = true;     /* sub_08067898 */
        g->state = 3;
    } else if (g->state == 3) {
        g->release_0 = true;        /* sub_0805B66C(0) */
        g->sound_1041 = true;       /* sub_0805B878(0x1041) */
        g->wrote_254cc = true;      /* +0x254CC pair writes */
        g->song_26 = true;          /* PlaySong(0x26) */
        g->text_id = 0x2D;
        g->state = 1;               /* loops */
    }
    /* tail: sub_0803F17C animation step every call. */
}

uint32_t aos_npc_talk_66094(AosNpcGuard *g, uint32_t flags,
                            bool text_done, int32_t player_x,
                            AosGuardSeq *seq, AosGuardPickCtx *pick) {
    g->cleared_42c_200 = g->sfx_26_entry = g->froze_player =
        g->armed_42c_200 = g->facing_mirror = g->window_text =
        g->ev_1d = g->sound_1040 = g->cleared_18 = g->song_1b5 =
        g->wrote_254cc = g->song_26 = g->sound_1041 =
        g->release_0 = false;
    g->ret = 0;
    if (g->state == 0) {
        g->cleared_42c_200 = true;      /* +0x42C &= ~0x200 */
        g->sfx_26_entry = true;         /* sub_08013EEC(0x26) */
        g->state = 1;
        g->ctr_b = g->ctr_c = 0;
    }
    if (g->state == 1) {
        if ((flags & 7) != 7) return g->ret; /* probe+mask+pause */
        g->armed_42c_200 = true;            /* +0x42C |= 0x200 */
        g->froze_player = true;
        /* facing: +0x58 = (own & ~0x41) | (self_x >= player_x ?
         * 0x40 : 0) — the ROM's inverted compare. */
        g->flags58 = (uint8_t)((g->flags58 & ~0x41u) |
                               (g->x >= player_x ? 0x40u : 0u));
        g->facing_mirror = true;
        g->state = 2;
        g->ctr_b = g->ctr_c = 0;
        g->window_text = true;              /* sub_08067934(+0x14) */
        g->ev_1d = true;                    /* sub_08013CF0(0x1D) */
        g->sound_1040 = true;               /* sub_0805B878(0x1040) */
        if (g->field18) {
            /* sub_080213BC + PlaySong(0x1B6) + sub_08066308 —
             * the +0x18 variant pick. */
            g->song_1b5 = true;
            if (pick && seq) {
                uint16_t t14 = 0;
                aos_npc_talk_66308(seq, pick, &t14);
                g->text_id = (uint8_t)t14;
            }
        } else if (pick && seq) {
            if (flags & 8) {                /* sub_0805B864 */
                aos_guard_seq_reset(seq);   /* sub_0806666C */
                g->text_id = 0x44;
            } else {
                /* sub_0806668C: +0x14 = 0x36, sequence index
                 * from flags 0x2E/0x26. */
                uint8_t t8 = 0;
                aos_guard_seq_pick(seq,
                                   (uint8_t)((pick->unk33c >> 0x2E) & 1),
                                   (uint8_t)((pick->unk33c >> 0x26) & 1),
                                   &t8);
                g->text_id = t8;
            }
        }
    } else if (g->state == 2) {
        if (!text_done) return g->ret;
        if (g->field18) {
            g->cleared_18 = true;
            g->field18 = 0;
        }
        g->song_1b5 = true;         /* PlaySong(0x1B5) */
        g->wrote_254cc = true;      /* +0x254CC pair writes */
        g->song_26 = true;          /* PlaySong(0x26) */
        g->sound_1041 = true;       /* sub_0805B878(0x1041) */
        if (flags & 8) {            /* sub_0805B864 -> advance */
            g->state = 3;
        } else {
            g->release_0 = true;    /* sub_0805B66C(0) */
            g->state = 1;
        }
        g->ctr_b = g->ctr_c = 0;
    } else if (g->state == 3) {
        if (g->ctr_b == 0) {
            g->timer_d = 0x10;
            g->ctr_b = 1;
            g->ctr_c = 0;
            g->ret = 0x10;
            return g->ret;
        }
        if (g->ctr_b == 1) {
            /* facing mirror (same inverted compare) */
            g->flags58 = (uint8_t)((g->flags58 & ~0x41u) |
                                   (g->x >= player_x ? 0x40u : 0u));
            g->facing_mirror = true;
            g->timer_d = 0x10;
            g->ret = 0x10;
            /* wait while self_x + 0x300000 >= player_x */
            if (g->x + 0x300000 >= player_x) return g->ret;
            g->timer_d = 0;
            g->ctr_b = 2;
            g->ctr_c = 0;
            return g->ret;
        }
        g->release_0 = true;        /* sub_0805B66C(0) */
        g->state = 1;
        g->ctr_b = g->ctr_c = 0;
    }
    return g->ret;
}

uint32_t aos_npc_talk_66024(AosNpcSpawnGate *g, bool flag20,
                            bool keep, uint8_t player_5c) {
    g->kept = g->variant = g->anim_step = g->facing_cleared = false;
    g->ret = 0;
    if (!(flag20 && keep)) {
        g->flags59 |= 8;            /* +0x59 |= 8 */
        return g->ret;
    }
    g->kept = true;
    g->variant = true;              /* sub_0803F2C8(e,0,0,1) */
    g->anim_step = true;            /* sub_0803F17C(e) */
    g->flags58 &= ~0x41u;           /* +0x58 &= ~0x41 */
    g->facing_cleared = true;
    g->anim5c = (uint8_t)(player_5c + 1);   /* +0x5C = player + 1 */
    g->field18 = 1;
    g->ret = 1;
    return g->ret;
}

void aos_guard_seq_init(AosGuardSeq *g, uint16_t mode,
                        uint16_t index, uint16_t fallback) {
    /* sub_08066648 */
    g->mode = mode;
    g->counter = 0;
    g->index = index;
    g->fallback = fallback;
}

void aos_guard_seq_reset(AosGuardSeq *g) {
    /* sub_0806666C */
    g->mode = 0;
    g->counter = 0xFF;
    g->index = 0;
    g->fallback = 0;
}

uint8_t aos_guard_seq_pick(AosGuardSeq *g, bool flag2e,
                           bool flag26, uint8_t *text_id) {
    /* sub_0806668C: entity +0x14 = 0x36, mode/index/fallback reset,
     * index = flag2e ? (flag26 ? 3 : 2) : 1. */
    *text_id = 0x36;
    g->mode = 0x36;
    g->counter = 0;
    g->fallback = 0;
    g->index = (uint16_t)(flag2e ? (flag26 ? 3 : 2) : 1);
    return *text_id;
}

uint32_t aos_guard_seq_tick(AosGuardSeq *g,
                            const uint16_t *const *table,
                            uint32_t rng, AosGuardXform xform,
                            uint16_t *out) {
    /* sub_080665B4 */
    if (g->counter > 1) return 0;
    const uint16_t *rec = table[g->index];
    uint16_t v;
    if (g->counter == 0) {
        /* modes 4/6 index the pick array with the +0x138FA
         * fallback; other modes use rng % rec[2]. */
        uint16_t pick = (g->index == 4 || g->index == 6)
            ? g->fallback
            : (uint16_t)(rng % rec[2]);
        v = rec[3 + pick];
    } else {
        v = rec[1];
    }
    *out = xform(v);                 /* sub_08041434 -> +0x40C */
    g->counter = (uint16_t)(g->counter + 1);
    return 1;
}

/* sub_08066308 support: the unk_33C bitfield helpers. */
static bool pk_12078(const AosGuardPickCtx *c, unsigned bit) {
    return ((c->unk33c >> bit) & 1) != 0;
}

/* sub_080120AC: returns the bit and sets it when clear. */
static bool pk_120ac(AosGuardPickCtx *c, unsigned bit) {
    bool v = pk_12078(c, bit);
    if (!v)
        c->unk33c |= 1ull << bit;
    return v;
}

/* One cursor step of the queue pick: slot = (cursor & 0xF) +
 * mode*8 into the 0x08118C85 table, byte -> sequence fallback,
 * cursor +1, reseated to mode<<4 when the low nibble hits 8. */
static void pk_pick_step(AosGuardPickCtx *ctx, AosGuardSeq *seq,
                         unsigned mode) {
    unsigned slot = (ctx->cursor_18c & 0xF) + mode * 8;
    seq->fallback = ctx->table_08118c85[slot];
    ctx->cursor_18c = (uint8_t)(ctx->cursor_18c + 1);
    if ((ctx->cursor_18c & 0xF) == 8)
        ctx->cursor_18c = (uint8_t)(mode << 4);
}

void aos_npc_talk_66308(AosGuardSeq *seq, AosGuardPickCtx *ctx,
                        uint16_t *text_id) {
    *text_id = 0;
    if (ctx->cond_5b864) {
        /* sub_080120AC(0x36): set keeps +0x14 at 0 (the queue
         * block still runs), clear picks 0x43. */
        if (!pk_120ac(ctx, 0x36))
            *text_id = 0x43;
    } else if (pk_12078(ctx, 0x3B) && ctx->unk132ae[0] &&
               ctx->unk132ae[1] && ctx->unk132ae[2]) {
        /* sub_08012048(0x3B) re-set, text 0x45 and the
         * +0x138F4 sequence programmed to mode 0x45/index 4/
         * fallback 8. */
        ctx->unk33c |= 1ull << 0x3B;
        *text_id = 0x45;
        aos_guard_seq_init(seq, 0x45, 4, 8);
    } else if (pk_12078(ctx, 0x26)) {
        if (!pk_120ac(ctx, 0x35)) {
            *text_id = 0x30;
        } else if (!(ctx->rng_pick & 1)) {
            *text_id = 0x31;
            aos_guard_seq_init(seq, 0x31, 0, 0);
        }
        /* 0x35 consumed + RNG&1 set: +0x14 stays 0 -> queue. */
    } else if (pk_12078(ctx, 0x2E)) {
        if (!pk_120ac(ctx, 0x34)) {
            *text_id = 0x2F;
        } else if (!pk_120ac(ctx, 0x37)) {
            *text_id = 0x53;
        }
        /* 0x37 consumed: +0x14 stays 0 -> queue. */
    } else if (pk_12078(ctx, 0x21)) {
        if (!pk_120ac(ctx, 0x37))
            *text_id = 0x53;
    } else if (pk_12078(ctx, 0x20)) {
        if (pk_120ac(ctx, 0x33))
            *text_id = 0x2E;
    }
    if (*text_id != 0)
        return;

    /* Queue pick: +0x14 = 0x45, sequence mode 0x45/counter 0/
     * index 4 (6 under sub_0805B864), fallback RNG%9. */
    *text_id = 0x45;
    seq->mode = 0x45;
    seq->counter = 0;
    seq->index = 4;
    seq->fallback = (uint16_t)(ctx->rng_queue % 9);
    unsigned mode = 0;                  /* r7 cleared on entry */
    if (ctx->cond_5b864) {
        mode = 5;
        seq->index = 6;
    } else if (pk_12078(ctx, 0x21)) {
        if (!(ctx->flags37e & 0x10)) {
            mode = 1;
        } else if (!pk_12078(ctx, 0x24)) {
            if (pk_12078(ctx, 0x3C)) {
                mode = pk_12078(ctx, 0x3D) ? 3 : 6;
            } else {
                mode = 2;
            }
        } else if (!(ctx->flags37e & 0x02)) {
            mode = 3;
        } else {
            mode = (ctx->flags37e & 0x40) ? 4 : 3;
        }
    }
    if (mode != (unsigned)(ctx->unk133e8 >> 4))
        ctx->cursor_18c = (uint8_t)(mode << 4);
    if (!ctx->table_08118c85)
        return;             /* documented degradation: the
                             * RNG%9 fallback stays */
    pk_pick_step(ctx, seq, mode);
    if (pk_12078(ctx, 0x39)) {
        while (seq->fallback == 7)
            pk_pick_step(ctx, seq, mode);
    }
    if (pk_12078(ctx, 0x3D)) {
        while (seq->fallback == 9)
            pk_pick_step(ctx, seq, mode);
    }
}

void aos_npc_talk_667dc(AosNpcTalkLoop *g, bool cond864,
                        uint32_t rng, AosGuardSeq *seq) {
    /* sub_080667DC */
    g->text_id = 0;
    if (cond864 && (rng & 0xF) <= 3) {
        g->text_id = 0x59;
    }
    if (g->text_id == 0) {
        g->text_id = 0x54;
        if (seq) {
            seq->mode = 0x54;
            seq->counter = 0;
            seq->index = 5;
            seq->fallback = 0;
        }
    }
}

void aos_npc_talk_666d4(AosNpcTalkLoop *g, uint32_t flags,
                        bool cond864, uint32_t rng) {
    /* sub_080666d4 */
    g->cleared_42c_200 = g->froze_player = g->armed_42c_1 =
        g->window_text = g->mark_38 = g->release_0 = false;
    if (g->state == 0) {
        g->cleared_42c_200 = true;      /* +0x42C &= ~0x200 */
        g->state = 1;
        g->ctr_b = g->ctr_c = 0;
    }
    if (g->state == 1) {
        if ((flags & 7) != 7) return;   /* probe+mask+pause */
        g->froze_player = true;
        g->armed_42c_1 = true;          /* +0x42C |= 1 */
        /* the ROM calls sub_080667DC here to fill +0x14; the
         * port exposes that pick as aos_npc_talk_667dc so the
         * caller composes both (cond864/rng are its inputs). */
        g->window_text = true;          /* sub_08067934(+0x14) */
        g->state = 2;
        g->ctr_b = g->ctr_c = 0;
        (void)cond864; (void)rng;
    } else if (g->state == 2) {
        if (!(flags & 8)) return;       /* sub_08067A04 poll */
        g->mark_38 = true;              /* sub_08012048(0x38) */
        g->release_0 = true;            /* sub_0805B66C(0) */
        g->state = 0;
        g->ctr_b = g->ctr_c = 0;
    }
}

void aos_npc_talk_66828(AosNpcSpawnGate2 *g, bool flag20,
                        bool flag26, bool keep) {
    /* sub_08066828 */
    g->marked_8 = g->deleted = g->variant = g->anim_step = false;
    if (!flag20 || !flag26) {
        g->flags59 |= 8;
        g->marked_8 = true;
        return;
    }
    if (!keep) {
        g->deleted = true;              /* EntityDelete */
        return;
    }
    g->variant = true;                  /* sub_0803F2C8(e,6,0,1) */
    g->anim_step = true;                /* sub_0803F17C */
    g->flags58 |= 0x40;
}
