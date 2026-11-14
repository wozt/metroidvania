/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_object20_story.h"
#include <string.h>

static void out_reset(AosObject20StoryOut *out) {
    memset(out, 0, sizeof *out);
    out->window_text = -1;
    out->song = -1;
    out->b878 = -1;
    out->cf0 = -1;
    out->d60 = -1;
    out->e18 = -1;
    out->eec = -1;
    out->jingle = -1;
    out->ev_121e0 = -1;
    out->ed24_a = -1;
    out->c5a8 = -1;
    out->setplayerinput = -1;
    out->call_127f0_off = -1;
    out->call_1d028_a = -1;
    out->call_3d004 = -1;
    out->call_3d3a0 = -1;
    out->f6c = -1;
    out->release_arg = -1;
    out->wo_disp4c = -1;
    out->wo_disp48 = -1;
    out->wo_unk4 = -1;
    out->wo_unk6 = -1;
    out->wo_rng = -1;
    for (unsigned i = 0; i < 4; ++i) {
        out->anim_a[i] = -1;
        out->child_anim_a[i] = -1;
        out->spawn_type[i] = -1;
    }
}

static void anim3(AosObject20StoryOut *out, int a, int b, int c) {
    if (out->anim_count >= 4) return;   /* bounded report list */
    unsigned i = out->anim_count++;
    out->anim_a[i] = a;
    out->anim_b[i] = b;
    out->anim_c[i] = c;
}

static void child_anim3(AosObject20StoryOut *out, int a, int b, int c) {
    if (out->child_anim_count >= 4) return;
    unsigned i = out->child_anim_count++;
    out->child_anim_a[i] = a;
    out->child_anim_b[i] = b;
    out->child_anim_c[i] = c;
}

static void spawn1(AosObject20StoryOut *out, int type) {
    if (out->spawn_count >= 4) return;
    out->spawn_type[out->spawn_count++] = type;
}

/* The ROM's facing-bit idiom: bit6 of the source flags58 is
 * extracted ((f << 0x19) >> 0x1F), then
 * (f & ~0x41) | ((1 - bit6) & 1) << 6 — an inverted mirror. */
static uint8_t facing_invert(uint8_t flags58) {
    unsigned bit6 = (flags58 & 0x40u) ? 1u : 0u;
    return (uint8_t)((flags58 & ~0x41u) | (((1u - bit6) & 1u) << 6));
}

/* ------------------------------------------------------------------
 * param0 9 — create sub_0805FFDC.
 * Gate: sub_08012078(0x26) set OR player +0x42 <= 0x7F marks
 * +0x59 |= 8 (the shared tail deletes). Otherwise builds the
 * guide pair: self +0x14 child (type 5) and +0x18 follower
 * (type 1) with the 0xFF - A094[6] / own +0x46 placement.
 * ------------------------------------------------------------------ */
static void create9(AosObject20Story *s, const AosObject20StoryIn *in,
                    AosObject20StoryOut *out) {
    if (in->flag12078_26 != 0 || in->player_scr42 <= 0x7F) {
        s->flags59 |= 0x8;
        s->deleted = true;      /* shared tail EntityDelete */
        return;
    }
    out->player58_or_40 = true;             /* player +0x58 |= 0x40 */
    if (!in->keep_5b754) {
        s->deleted = true;
        return;
    }
    anim3(out, 3, 0, 1);                    /* sub_0803F2C8(e,3,0,1) */
    out->anim_step_count++;                 /* sub_0803F17C */
    s->scr42 = (int16_t)(0x48 - in->a094_6);
    s->scr46 = (int16_t)(0xA0 - in->a094_a);
    s->flags58 |= 0x40;
    s->anim5c = (uint8_t)(in->player_anim5c - 1);
    spawn1(out, 5);                         /* sub_0805B5E8(5) */
    s->child14 = in->spawn14_ok;
    if (!s->child14) {
        s->deleted = true;
        return;
    }
    child_anim3(out, 5, 0, 0);              /* sub_0803F2C8(child,5,0,0) */
    out->anim_step_count++;
    /* child +0x42 = own +0x42 + 0x13; child +0x46 = own +0x46;
     * child +0x48 = 0xFFFE8000; child +0x58 = inverted facing;
     * child +0x5C = own +0x5C + 1; child +0x59 |= 0x10. The
     * child record fields live on the caller side; the port
     * tracks the flag byte writes in child14_*. */
    s->child14_flags58 = facing_invert(s->flags58);
    s->child14_anim5c = (uint8_t)(s->anim5c + 1);
    s->child14_flags59 |= 0x10;
    /* Persist the child's screen placement and sprite record for
     * the draw pass: child +0x42 = own +0x42 + 0x13,
     * child +0x46 = own +0x46 (documented above); the sub_0805B5E8
     * index 5 is the private-table record. child +0x6D = 5 from
     * the sub_0803F2C8(child,5,0,0) pick. */
    s->child14_scr42 = (int16_t)(s->scr42 + 0x13);
    s->child14_scr46 = s->scr46;
    s->child14_record = 5;
    s->child14_anim6d = 5;
    out->call_3e654 = true;                 /* sub_0803E654(1) */
    if (in->rng_3e654 < 0) {
        s->deleted = true;
        return;
    }
    s->scr1c = 0x10000;                     /* 0x80 << 9 */
    s->scr20 = 0x10000;
    /* child +0x58 = (child +0x58 & ~0x20) | (rng & 0x1F). */
    s->child14_flags58 = (uint8_t)((s->child14_flags58 & ~0x20u) |
                                   (in->rng_3e654 & 0x1F));
    out->call_3e058 = true;
    out->e058_a = s->scr1c;
    out->e058_b = s->scr20;
    out->e058_flags = in->flag12078_26;     /* saved flag26 word */
    out->e058_arg = 0;
    out->e058_low5 = (int)(s->child14_flags58 & 0x1F);
    spawn1(out, 1);                         /* sub_0805B5E8(1) */
    s->link18 = in->spawn18_ok;
    if (!s->link18) {
        s->deleted = true;                  /* ROM deletes +0x14 then self */
        return;
    }
    child_anim3(out, 0, 0, 1);              /* sub_0803F2C8(link,0,0,1) */
    out->anim_step_count++;
    /* link +0x42 = 0xFF - A094[6]; link +0x46 = own +0x46;
     * link +0x48 = 0xFFFFA000; link +0x58 &= ~0x41. */
    s->link18_scr42 = (int16_t)(0xFF - in->a094_6);
    s->link18_scr46 = s->scr46;
    s->link18_vel48 = (int32_t)0xFFFFA000;
    s->link18_flags58 &= ~0x41u;
    s->link18_anim6d = 0;           /* F2C8(link,0,0,1) pick;
                                     * the +0x18 spawn index is
                                     * always record 1. */
    out->set_42c |= 1;
}

/* ------------------------------------------------------------------
 * param0 9 — update sub_08060248.
 * State +0xA 0 opens window 0x14; 1 polls then runs the
 * +0xB 0-6 cinematic substates; 3 waits on the player; 0x80
 * exits. Returns r7 (0x20 player-clamp, 0x80 finale flags).
 * ------------------------------------------------------------------ */
static int update9(AosObject20Story *s, const AosObject20StoryIn *in,
                   AosObject20StoryOut *out) {
    int r7 = 0;
    uint8_t st = s->state;
    if (st == 0) {
        out->song = 0x1B8;                  /* PlaySong(0xdc << 1) */
        out->eec = 0x28;                    /* sub_08013EEC(0x28) */
        out->window_text = 0x14;            /* sub_08067934(0x14) */
        out->write_417 = true;
        out->write_417_val = 0;             /* +0x417 = 0 */
        s->state = 1;
        st = 1;                             /* ROM fallthrough */
    }
    if (st == 1) {
        out->window_poll = true;            /* sub_08067A04 */
        if (in->text_done) {
            aos_object20_story_601d4(s, in->player_anim5c, out);
            s->state = 0x80;
            out->release_arg = 0;           /* sub_0805B66C(e, 0) */
            out->jingle = 0x26;             /* sub_08012048(0x26) */
            return r7;                      /* skips sub_0803F17C */
        }
        /* +0xB jump table, cases 0-7 (0 and 7 skip to the
         * follower sync). The +0xC "cases" are sequential code
         * blocks: entering at +0xC == k falls through the
         * remaining steps until an explicit exit branch, so a
         * single call can advance several steps. */
        switch (s->sub_b) {
        case 1:
            /* _08060310 chain: c0 -> c1 -> c2 -> exit. */
            if (s->sub_c <= 4) {
                if (s->sub_c == 0) {
                    out->write_417 = true;  /* +0x60+0x3B7 == +0x417 */
                    out->write_417_val = 1;
                    s->sub_c = 1;
                }
                if (s->sub_c == 1) {
                    if (s->child14) {
                        child_anim3(out, 6, 0, 0);   /* child,6,0,0 */
                        s->child14_anim6d = 6;  /* child +0x6D */
                        /* child +0x58 = facing_invert(child +0x58). */
                        s->child14_flags58 =
                            facing_invert(s->child14_flags58);
                    }
                    anim3(out, 5, 0, 1);    /* self,5,0,1 */
                    s->sub_c = 2;
                }
                if (s->sub_c == 2) {
                    if (s->child14_flags59 & 1) {
                        if (s->child14_anim6d != 7) {
                            /* approximated: sub_0803F2C8(child,7,0,1)
                             * targets the +0x14 entity. */
                            child_anim3(out, 7, 0, 1);
                            s->child14_anim6d = 7;
                        }
                        s->sub_c = 3;
                        s->timer_d = 8;
                    }
                    /* not ready: exits without advancing */
                } else if (s->sub_c == 3) {
                    s->timer_d = (uint8_t)(s->timer_d - 1);
                    if (s->timer_d == 0xFF)
                        s->sub_c = 4;
                } else if (s->sub_c == 4) {
                    out->write_417 = true;  /* +0x3B7 = 0 via +0x417 */
                    out->write_417_val = 0;
                }
            }
            break;
        case 2:
            /* _080603D4 chain: c0 -> c1 -> _08060496 scroll. */
            if (s->sub_c == 0) {
                out->write_417 = true;
                out->write_417_val = 1;
                s->sub_c = 1;
                s->timer_d = 0x10;
                if (s->anim6d != 4)
                    anim3(out, 4, 0, 0);
            }
            if (s->sub_c == 1) {
                s->timer_d = (uint8_t)(s->timer_d - 1);
                if (s->timer_d == 0xFF) {
                    s->sub_c = 2;
                    s->timer_d = 0;
                }
            } else if (s->sub_c == 2) {
                if (s->timer_d == 0x24) {
                    out->song = 0x156;       /* PlaySong(0xab << 1) */
                } else if (s->timer_d == 0) {
                    out->song = 0x155;       /* PlaySong(0x155) */
                }
                s->timer_d++;
                /* player +0x40 vs own +0x40 + 0x200000 clamp. */
                if (in->player_pos40 > s->pos40 + 0x200000) {
                    r7 |= 0x20;
                } else {
                    out->player_pos40_set = true;
                    out->player_pos40_val = s->pos40 + 0x200000;
                    s->sub_c++;
                }
            } else if (s->sub_c == 3) {
                r7 |= 0x80;
                out->write_417 = true;      /* +0x3B7 = 0 */
                out->write_417_val = 0;
            }
            /* _08060496: +0x1C/+0x20 scroll pair decay +
             * sub_0803E058, or child +0x58 |= 0x20 when
             * +0x1C == 0 — runs for every +0xC of +0xB == 2. */
            if (s->scr1c != 0) {
                s->scr1c += (int32_t)0xFFFFF800;   /* -= 0x800 */
                s->scr20 += 0x400;
                out->call_3e058 = true;
                out->e058_a = 0x10000;
                out->e058_b = 0x10000;
                out->e058_flags = 0;
                out->e058_arg = 0;
                out->e058_low5 = (int)(s->child14_flags58 & 0x1F);
            } else if (s->child14) {
                s->child14_flags58 |= 0x20; /* child +0x58 |= 0x20 */
            }
            break;
        case 3:
            /* _080604DE chain: c0 -> c1 -> _080605F8; every path
             * sets r7 |= 0x80. */
            if (s->sub_c == 0) {
                out->write_417 = true;
                out->write_417_val = 1;
                s->sub_c = 1;
                s->timer_d = 0x30;
            }
            if (s->sub_c == 1) {
                s->timer_d = (uint8_t)(s->timer_d - 1);
                if (s->timer_d == 0xFF) {
                    s->sub_c = 2;
                    s->timer_d = 0x60;
                    if (s->anim6d != 7)
                        anim3(out, 7, 0, 0);
                }
            } else if (s->sub_c == 2) {
                /* own +0x59 bit 0 -> +0x3B7 = 0 */
                if (s->flags59 & 1) {
                    out->write_417 = true;
                    out->write_417_val = 0;
                }
            }
            r7 |= 0x80;                     /* _080605F8 */
            break;
        case 4:
            /* _08060554 chain: c0 -> c1 -> _080605F8; every path
             * sets r7 |= 0x80. */
            if (s->sub_c == 0) {
                out->write_417 = true;
                out->write_417_val = 1;
                s->timer_d = 0x30;
                s->sub_c = 1;
                if (s->anim6d != 8)
                    anim3(out, 8, 0, 0);
            }
            if (s->sub_c == 1) {
                s->timer_d = (uint8_t)(s->timer_d - 1);
                if (s->timer_d == 0xFF)
                    s->sub_c++;
            } else if (s->sub_c == 2) {
                out->write_417 = true;      /* +0x3B7 = 0 */
                out->write_417_val = 0;
            }
            r7 |= 0x80;                     /* _080605F8 */
            break;
        case 5:
            /* _080605BC chain: c0 -> c1 -> _080605F8; every path
             * sets r7 |= 0x80. */
            if (s->sub_c == 0) {
                out->write_417 = true;
                out->write_417_val = 1;
                s->sub_c = 1;
                s->timer_d = 0x30;
            }
            if (s->sub_c == 1) {
                if (s->timer_d == 0) {
                    out->write_417 = true;
                    out->write_417_val = 0;
                    if (s->link18)
                        s->link18_state_a = 1;  /* link +0xA = 1 */
                } else {
                    s->timer_d--;
                }
            }
            r7 |= 0x80;                     /* _080605F8 */
            break;
        case 6:
            out->player58_andnot_41 = true;    /* player +0x58 &= ~0x41 */
            s->anim5c = (uint8_t)(in->player_anim5c + 1);
            /* _08060624 -> follower sync; r7 is NOT modified. */
            break;
        default:    /* cases 0 and 7 fall to the follower sync */
            break;
        }
        /* _08060626: the +0x18 follower catch-up — runs for
         * every +0xB case while the link entity's +0xA is set. */
        if (s->link18 && s->link18_state_a != 0) {
            int32_t thresh = s->pos40 + 0x600000;
            if (s->link18_pos40 < thresh) {
                s->link18_pos40 = thresh;
                if (s->link18_anim6d != 1) {
                    /* approximated: sub_0803F2C8(link,1,0,1)
                     * targets the +0x18 entity. */
                    child_anim3(out, 1, 0, 1);
                    s->link18_anim6d = 1;
                }
                s->link18_state_a = 0;
            } else {
                s->link18_pos40 += s->link18_vel48;
            }
        }
    } else if (st == 3) {
        if (s->sub_c == 0) {
            out->player58_or_40 = true;
            s->sub_c = 1;
        }
        if (s->sub_c == 1) {
            if (in->player_scr42 <= 2) {
                s->sub_c++;
            } else {
                r7 |= 0x20;
            }
        }
    } else if (st == 0x80) {
        out->return_code = r7;
        return r7;                          /* skips sub_0803F17C */
    }
    out->anim_step_count++;                 /* sub_0803F17C tail */
    out->return_code = r7;
    return r7;
}

/* ------------------------------------------------------------------
 * sub_080601D4 — param0-9 text-done helper (also exported).
 * ------------------------------------------------------------------ */
void aos_object20_story_601d4(AosObject20Story *s,
                              uint8_t player_anim5c,
                              AosObject20StoryOut *out) {
    s->flags58 |= 0x40;                     /* +0x58 |= 0x40 */
    out->player_anim5c_inc = true;          /* player +0x5C read */
    s->anim5c = (uint8_t)(player_anim5c + 1);
    s->field65 = 0x12;                      /* +0x65 = 0x12 */
    out->ent65_set = true;
    out->ent65_val = 0x12;
    if (s->child14)
        s->child14_flags59 |= 0x8;          /* child +0x59 |= 8 */
    if (s->link18) {
        s->link18_pos40 = s->pos40 + 0x600000;
        child_anim3(out, 1, 0, 1);          /* approximated:
            sub_0803F2C8(link,1,0,1) targets +0x18. */
        s->link18_anim6d = 1;
    }
    s->flags58 |= 0x40;
}

/* ------------------------------------------------------------------
 * param0 10 — create sub_080606C8.
 * Gates: sub_08012078(0x27), +0x37E bit 0x40, player +0x42 <=
 * 0x80. Places self at (0x118 - A094[6], 0xC0 - A094[10]) and
 * mirrors facing against the player.
 * ------------------------------------------------------------------ */
static void create10(AosObject20Story *s, const AosObject20StoryIn *in,
                     AosObject20StoryOut *out) {
    if (in->flag12078_27 != 0 ||
        (in->flags37e & 0x40) == 0 ||
        in->player_scr42 > 0x80) {
        s->flags59 |= 0x8;
        s->deleted = true;
        return;
    }
    out->unk7864_or2 = true;
    out->dma_setup_786c = true;             /* 0x040000D4 chain */
    if (!in->keep_5b754) {
        s->deleted = true;
        return;
    }
    anim3(out, 4, 1, 1);                    /* sub_0803F2C8(e,4,1,1) */
    out->anim_step_count++;
    s->scr42 = (int16_t)(0x118 - in->a094_6);
    s->scr46 = (int16_t)(0xC0 - in->a094_a);
    /* +0x58 = (+0x58 & ~0x41) | ((e+0x40 < player+0x40) << 6). */
    unsigned bit = s->pos40 < in->player_pos40 ? 0x40u : 0u;
    s->flags58 = (uint8_t)((s->flags58 & ~0x41u) | bit);
    out->player58_andnot_41 = true;         /* player +0x58 &= ~0x41 */
}

/* ------------------------------------------------------------------
 * param0 10 — update sub_080607E4.
 * States: 0 proximity arm, 1 window 0x15 open, 2 poll + the
 * +0xB/+0xC two-level cinematic, 0x80 release. Returns r2
 * (bit 0x10 from the b0/c2 late branch).
 * ------------------------------------------------------------------ */
static int update10(AosObject20Story *s, const AosObject20StoryIn *in,
                    AosObject20StoryOut *out) {
    int r2 = 0;
    uint8_t st = s->state;
    if (st == 0) {
        if (in->player_scr42 > 0x60) {
            out->unk7864_or2 = true;
            out->dma_setup_786c = true;
            out->set_42c |= 0x200;
            out->call_0eb04 = true;
            out->call_235c8 = true;
            out->call_2dff0 = true;
            out->fade_out = true;          /* 3D270 + 3CED4 */
            s->state = 1;
            s->sub_b = 0;
            s->sub_c = 0;
        } else {
            out->clear_42c |= 0x200;
        }
        out->anim_step_count++;
        out->return_code = r2;
        return r2;
    }
    if (st == 1) {
        out->set_42c |= 1;                  /* +0x42C |= state(1) */
        out->window_text = 0x15;
        out->write_417 = true;
        out->write_417_val = 1;
        s->state = 2;
        s->sub_b = 0;
        s->sub_c = 0;
        st = 2;                             /* ROM fallthrough */
    }
    if (st == 2) {
        out->window_poll = true;
        if (in->text_done)
            s->state = 0x80;
        switch (s->sub_b) {
        case 0:
            /* _08060920 chain: c0 -> c1; c2/c3 are terminal. */
            if (s->sub_c <= 4) {
                if (s->sub_c == 0) {
                    out->write_417 = true;  /* +0x3B7 = 1 == +0x417 */
                    out->write_417_val = 1;
                    s->sub_c = 1;
                    s->timer_d = 0x30;
                }
                if (s->sub_c == 1) {
                    s->timer_d = (uint8_t)(s->timer_d - 1);
                    if (s->timer_d == 0xFF) {
                        out->b878 = 0x1040; /* sub_0805B878(0x1040) */
                        s->sub_c++;         /* _08060B34 */
                    }
                } else if (s->sub_c == 2) {
                    int32_t limit = 0xE0 - (int32_t)in->a094_6;
                    if (in->player_scr42 > limit) {
                        out->player_scr42_set = true;
                        out->player_scr42_val = limit;
                        out->player58_andnot_41 = true;
                        out->write_427 = true;
                        out->write_427_val = 1;
                        s->sub_c++;
                    } else {
                        r2 |= 0x10;
                    }
                } else if (s->sub_c == 3) {
                    if ((int32_t)in->a094_6 <= 0x89) {
                        out->ed24_a = 1;
                        out->ed24_b = 0x8000;    /* 0x80 << 8 */
                    } else {
                        out->cf0 = 0x16;         /* sub_08013CF0(0x16) */
                        out->write_417 = true;
                        out->write_417_val = 0;
                        s->sub_c++;
                    }
                }
                /* +0xC == 4 (and > 4, guarded out) is a no-op. */
            }
            break;
        case 1:
            if (s->sub_c <= 3) {
                switch (s->sub_c) {
                case 0:
                    out->write_417 = true;  /* +0x3B7 = +0xB(1) */
                    out->write_417_val = 1;
                    if (s->anim6d != 0xF)
                        anim3(out, 0xF, 1, 0);
                    s->sub_c++;
                    break;
                case 1:
                    if (s->flags59 & 1) {
                        s->timer_d = 0x40;
                        if (s->anim6d != 0)
                            anim3(out, 0, 1, 1);
                    }
                    break;
                case 2:
                    s->timer_d = (uint8_t)(s->timer_d - 1);
                    if (s->timer_d == 0xFF)
                        s->sub_c++;
                    break;
                case 3:
                    out->write_417 = true;  /* +0x3B7 = 0 */
                    out->write_417_val = 0;
                    break;
                default:
                    break;
                }
            }
            break;
        case 2:
            /* _08060A88 chain: c0 -> c1; +0xC > 1 exits. */
            if (s->sub_c == 0) {
                out->b878 = 0x1040;
                out->write_417 = true;
                out->write_417_val = 1;
                s->vel48 = 0x18000;         /* 0xC0 << 9 */
                s->flags58 |= 0x40;
                if (s->anim6d != 1)
                    anim3(out, 1, 1, 1);
                s->timer_d = 0x10;
                s->sub_c = 1;
            }
            if (s->sub_c == 1) {
                if ((int16_t)s->scr42 <= 0x1FF)
                    s->pos40 += s->vel48;
                if (s->timer_d == 0) {
                    out->ed24_a = 1;
                    out->ed24_b = (int32_t)0xFFFF8000;
                    if (in->player_scr42 == 0x78) {
                        out->b878 = 0x1040;
                        out->write_417 = true;
                        out->write_417_val = 0;
                        /* approximated: the ROM writes
                         * gEwramData + 0x414 =
                         * (byte & ~0x10) | 5 — reported through
                         * write_414. */
                        out->write_414 = true;
                        out->write_414_val = 5;
                        s->sub_c++;         /* _08060B34 */
                    }
                    /* player +0x42 != 0x78 -> _08060B76 tail,
                     * +0xC is NOT advanced. */
                } else {
                    s->timer_d--;
                }
            }
            break;
        default:
            break;
        }
    } else if (st == 0x80) {
        out->call_13960 = true;             /* sub_08013960(+0x60) */
        out->write_427 = true;
        out->write_427_val = (uint8_t)r2;   /* stores sb (r2 flags) */
        out->release_arg = 1;               /* sub_0805B66C(e,1) */
        out->jingle = 0x27;                 /* sub_08012048(0x27) */
        out->return_code = r2;
        return r2;                          /* state 0x80 skips
                                               sub_0803F17C */
    }
    out->anim_step_count++;
    out->return_code = r2;
    return r2;
}

/* ------------------------------------------------------------------
 * param0 20 — create sub_08062040 (native create[20]; the
 * param0-20 update is the shop buy routine, not this module).
 * ------------------------------------------------------------------ */
static void create20(AosObject20Story *s, const AosObject20StoryIn *in,
                     AosObject20StoryOut *out) {
    (void)s; (void)in;
    out->set_42c |= 1;
    out->fade_out = true;                  /* 3D270 + 3CED4 */
}

static int update21(AosObject20Story *s, const AosObject20StoryIn *in,
                    AosObject20StoryOut *out) {
    (void)s; (void)in;
    out->b878 = 0x1040;                    /* sub_0805B878(0x82 << 5) */
    out->eec = 0x25;                        /* sub_08013EEC(0x25) */
    out->release_arg = 1;                   /* sub_0805B66C(e,1) */
    out->return_code = 0;
    return 0;
}

/* ------------------------------------------------------------------
 * param0 21 — create sub_08062068: unconditional +0x59 |= 8,
 * so the shared create tail always deletes the entity.
 * ------------------------------------------------------------------ */
static void create21(AosObject20Story *s, const AosObject20StoryIn *in,
                     AosObject20StoryOut *out) {
    (void)in; (void)out;
    s->flags59 |= 0x8;
    s->deleted = true;
}

/* sub_08062098 counter helper — identical to the ported
 * aos_npc_talk_counter_tick; reimplemented locally to keep
 * this module self-contained. Returns 1 while stepping. */
static int counter_tick(AosObject20Story *s, uint8_t arg,
                        AosObject20StoryOut *out) {
    if (s->sub_c == 0) {
        out->write_417 = true;
        out->write_417_val = 1;
        s->timer_d = arg;
        s->sub_c++;
        return 1;
    }
    if (s->sub_c == 1) {
        s->timer_d = (uint8_t)(s->timer_d - 1);
        if (s->timer_d == 0xFF)
            s->sub_c++;
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------
 * param0 22 — update sub_080620E8.
 * ------------------------------------------------------------------ */
static int update22(AosObject20Story *s, const AosObject20StoryIn *in,
                    AosObject20StoryOut *out) {
    int r7 = 0;
    uint8_t st = s->state;
    if (st == 0) {
        out->eec = 0x2D;                    /* sub_08013EEC(0x2D) */
        if (in->flag12078_2b != 0) {
            s->flags59 |= 0x8;
            out->return_code = r7;
            return r7;
        }
        out->set_42c |= 1;
        s->state = 1;
        s->sub_b = 0;
        s->sub_c = 0;
        out->return_code = r7;
        return r7;
    }
    if (st == 1) {
        out->call_d154 = true;              /* sub_0800D154 */
        out->window_text = 0x22;            /* sub_08067934(0x22) */
        out->write_417 = true;
        out->write_417_val = 1;
        s->state = 2;
        s->sub_b = 0;
        s->sub_c = 0;
        /* _08062158 returns directly — no state-2 fallthrough
         * and this routine has no sub_0803F17C tail. */
        out->return_code = r7;
        return r7;
    }
    if (st == 2) {
        out->window_poll = true;
        if (in->text_done) {
            s->state = 0x80;
            out->jingle = 0x2B;             /* sub_08012048(0x2B) */
            out->release_arg = 1;           /* sub_0805B66C(e,1) */
            out->call_d1f0 = true;          /* sub_0800D1F0 */
            out->b878 = 0x1041;             /* sub_0805B878(0x1041) */
            out->write_254cc = true;        /* +0x254CC = 0x2D word */
            out->write_254cc_val = 0x2D;
            out->song = 0x2D;               /* PlaySong(0x2D) */
            out->return_code = r7;
            return r7;                      /* skips sub_0803F17C */
        }
        switch (s->sub_b) {
        case 0:
            if (counter_tick(s, 0x40, out) == 0) {
                s->sub_b++;
                s->sub_c = 0;
            }
            break;
        case 1:
            if (in->player_scr42 > 0x9F) {
                r7 |= 0x20;
            } else {
                out->player_scr42_set = true;
                out->player_scr42_val = 0x9F;
                s->sub_b++;
                s->sub_c = 0;
            }
            break;
        case 2:
            if (counter_tick(s, 0x40, out) == 0) {
                out->write_417 = true;      /* +0x417 = 0 */
                out->write_417_val = 0;
            }
            break;
        case 3:
            if (counter_tick(s, 0x40, out) == 0) {
                out->e18 = 0x2D;            /* sub_08013E18(0x2D) */
                out->cf0 = 0x24;            /* sub_08013CF0(0x24) */
                out->write_417 = true;
                out->write_417_val = 0;
            }
            break;
        default:
            break;
        }
    }
    /* sub_080620E8 has no sub_0803F17C tail — every path
     * returns r7 through _08062234/_08062236. */
    out->return_code = r7;
    return r7;
}

/* ------------------------------------------------------------------
 * param0 22 — create sub_08062244 / update sub_080623F0.
 * ------------------------------------------------------------------ */
static void create22(AosObject20Story *s, const AosObject20StoryIn *in,
                     AosObject20StoryOut *out) {
    (void)s; (void)in;
    out->set_42c |= 1;
}

static int update23(AosObject20Story *s, const AosObject20StoryIn *in,
                    AosObject20StoryOut *out) {
    (void)in;
    out->set_42c |= 1;                      /* every call */
    out->c5a8 = 0;                          /* sub_0800C5A8(0) */
    uint8_t st = s->state;
    if (st == 0) {
        if (s->sub_b == 0) {
            out->dma_setup = true;          /* 0x040000D4 chain */
            out->set_42c |= 0x20 | 0x10000;
            s->timer_d = 0x80;
            s->sub_b = 1;
        }
        s->timer_d = (uint8_t)(s->timer_d - 1);
        if (s->timer_d == 0xFF) {
            out->window_text = 0x23;        /* sub_08067934(0x23) */
            s->state = 1;
            s->sub_c = 0;
            s->sub_b = 0;
        }
    } else if (st == 1) {
        out->window_poll = true;
        if (in->text_done) {
            s->state = 2;
            s->sub_c = 0;
            s->sub_b = 0;
            out->fade_3d18c = true;         /* sub_0803D18C(0,0,0,0x10000) */
            out->fade_3cdf0 = true;         /* sub_0803CDF0 same */
            out->unk2cb0_feff = true;       /* gUnk_03002CB0 &= 0xFEFF */
            out->jingle = 0x2C;             /* sub_08012048(0x2C) */
            out->b878 = 0x1040;             /* sub_0805B878(0x1040) */
        }
    } else if (st == 2) {
        if (!in->fade_done) {               /* sub_0803D408() == 0 */
            s->state = 0x80;
            s->sub_c = 0;
            s->sub_b = 0;
        }
    } else if (st == 0x80) {
        out->or_60_word = true;             /* gEwramData + 0x60 |= 1 */
        out->call_13788 = true;             /* sub_08013788 */
        out->call_127f0_off = 0x428;        /* +0x85 << 3 */
        out->call_127f0_val = 0;
        out->window_close = true;           /* sub_08067898 */
        out->write_64_0a = true;            /* +0x64 = 0x0A */
    }
    out->call_1d028_a = 0;                  /* sub_0801D028(0,0,0,0) */
    out->set_42c |= 0x20;
    out->return_code = 0;
    return 0;
}

/* sub_08062570 — the param0-24 state-0 +0xB helper: gate, place
 * the player escort pair and arm the flags. */
static void helper62570(AosObject20Story *s,
                        const AosObject20StoryIn *in,
                        AosObject20StoryOut *out) {
    if (!in->keep_5b754) {
        s->deleted = true;
        /* approximated: the ROM's EntityDelete is deferred to
         * the entity system; the parent update continues. */
        return;
    }
    anim3(out, 4, 0, 1);                    /* sub_0803F2C8(e,4,0,1) */
    out->anim_step_count++;
    s->scr42 = (int16_t)(0x40 - in->a094_6);
    s->scr46 = (int16_t)(0xC0 - in->a094_a);
    s->flags58 &= ~0x41u;
    /* player +0x42 = own +0x42 + 0x18; player +0x46 = own +0x46;
     * player +0x58 &= ~0x41. */
    out->player_scr42_set = true;
    out->player_scr42_val = (int32_t)s->scr42 + 0x18;
    out->player_pos44_set = true;
    out->player_pos44_val = s->scr46;
    out->player58_andnot_41 = true;
    out->set_42c |= 1 | 0x20;
    out->dma_setup = true;
    out->song = 0x18;                       /* PlaySong(0x18) */
}

/* ------------------------------------------------------------------
 * param0 23 — create sub_08062260 / update sub_08062664.
 * ------------------------------------------------------------------ */
static void create23(AosObject20Story *s, const AosObject20StoryIn *in,
                     AosObject20StoryOut *out) {
    if ((in->flag42c & 0x2000) == 0) {      /* +0x42C bit 0x2000 */
        s->flags59 |= 0x8;
        s->deleted = true;
        return;
    }
    if (!in->keep_5b754) {
        s->deleted = true;
        return;
    }
    anim3(out, 0, 0, 1);                    /* sub_0803F2C8(e,0,0,1) */
    out->anim_step_count++;
    s->scr42 = (int16_t)(0x40 - in->a094_6);
    s->scr46 = (int16_t)(0xC0 - in->a094_a);
    s->flags58 &= ~0x41u;
    /* player +0x42 = own +0x42 + 0x20; player +0x46 = own +0x46;
     * player +0x58 &= ~0x41. */
    out->player_scr42_set = true;
    out->player_scr42_val = (int32_t)s->scr42 + 0x20;
    out->player_pos44_set = true;
    out->player_pos44_val = s->scr46;
    out->player58_andnot_41 = true;
    spawn1(out, 1);                         /* sub_0805B5E8(1) */
    s->child14 = in->spawn14_ok;
    if (!s->child14) {
        s->deleted = true;
        return;
    }
    /* child +0x42 = own +0x42 + 0x60; child +0x46 = own +0x46 + 1;
     * child +0x58 &= ~0x41; F2C8(child,1,0,1) — approximated:
     * the child record writes stay engine-side. The screen
     * placement and the sub_0805B5E8(1) record are persisted
     * for the draw pass. */
    child_anim3(out, 1, 0, 1);
    s->child14_scr42 = (int16_t)(s->scr42 + 0x60);
    s->child14_scr46 = (int16_t)(s->scr46 + 1);
    s->child14_record = 1;
    s->child14_anim6d = 1;
    out->set_42c |= 1;
    out->dma_setup = true;
    out->fade_3d18c = true;                 /* 3D18C/3CDF0(0x1F..,0) */
    out->fade_3cdf0 = true;
    out->set_42c |= 0x20 | 0x10000;
    out->cf0 = 0x19;                        /* sub_08013CF0(0x19) */
}

static int update24(AosObject20Story *s, const AosObject20StoryIn *in,
                    AosObject20StoryOut *out) {
    uint8_t st = s->state;
    if (st == 0) {
        if (s->sub_b == 0) {
            if (s->sub_c == 0) {
                out->fade_3d18c = true;     /* (0x1F,0x1F,0x1F,0) */
                out->fade_3cdf0 = true;
                s->sub_c = 1;
            } else if (in->fade_done) {     /* sub_0803D408() != 0 */
                s->sub_b = 1;
                s->sub_c = 0;
            }
        } else {
            helper62570(s, in, out);
            /* approximated: the ROM continues the parent state
             * machine even when the helper deletes the entity. */
            s->state = 1;
            s->sub_c = 0;
            s->sub_b = 0;
            s->timer_d = 0xFF;
        }
        out->call_1d028_a = 0x51;           /* sub_0801D028(0x51,0,0,0) */
        /* state == 0 skips the sub_0803F17C tail anim step. */
        out->set_42c |= 0x20;
        out->return_code = 0;
        return 0;
    }
    if (st == 1) {
        s->timer_d = (uint8_t)(s->timer_d - 1);
        if (s->timer_d == 0xFF) {
            bool pick = in->rng_32df0 > 0x31F;
            out->window_text = pick ? 0x26 : 0x27;
            s->field13 = pick ? 1 : 0;
            s->state = 2;
            s->sub_c = 0;
            s->sub_b = 0;
        }
    } else if (st == 2) {
        out->window_poll = true;
        if (in->text_done) {
            s->state = 3;
            s->sub_c = 0;
            s->sub_b = 0;
            out->fade_3d18c = true;         /* (0,0,0,0x10000) */
            out->fade_3cdf0 = true;
            out->unk2cb0_feff = true;
            out->call_137fc = true;         /* sub_080137FC */
            out->b878 = 0x1040;
        } else {
            switch (s->sub_b) {
            case 0:
                out->call_1d028_a = 0x51;
                break;
            case 1:
                out->player58_or_40 = true;
                if (s->anim6d != 0)
                    anim3(out, 0, 0, 0);
                break;
            default:                        /* +0xB >= 2 -> _080627D4 */
                out->player58_andnot_41 = true;
                break;
            }
        }
    } else if (st == 3) {
        if (!in->fade_done) {               /* sub_0803D408() == 0 */
            out->window_close = true;       /* sub_08067898 */
            out->jingle = 0x2D;             /* sub_08012048(0x2D) */
            out->write_64_0a = true;
        } else if (s->field13 != 0) {
            /* player +0x42 - 0xC vs own +0x42 (s16). */
            if (in->player_scr42 - 0xC > (int32_t)s->scr42) {
                if (s->anim6d != 6)
                    anim3(out, 6, 0, 1);
                s->pos40 += 0x2000;         /* +0x40 += 0x2000 */
            } else {
                if (s->anim6d != 1)
                    anim3(out, 1, 0, 0);
            }
        }
    }
    if (s->state != 0)
        out->anim_step_count++;             /* sub_0803F17C tail */
    out->set_42c |= 0x20;
    out->return_code = 0;
    return 0;
}

/* ------------------------------------------------------------------
 * param0 31 — create sub_0805C49C (native create[31]; the
 * param0-29 update is sub_0805C08C, handled by update29).
 * ------------------------------------------------------------------ */
static void create31(AosObject20Story *s, const AosObject20StoryIn *in,
                     AosObject20StoryOut *out) {
    if (!in->keep_5b754) {
        s->flags59 |= 0x8;
        s->deleted = true;
        return;
    }
    anim3(out, 0, 0, 1);                    /* sub_0803F2C8(e,0,0,1) */
    out->anim_step_count++;
    s->scr42 = (int16_t)(0xFF - in->a094_6);
    s->scr46 = (int16_t)(0xBF - in->a094_a);
    s->flags58 &= ~0x41u;
    spawn1(out, 0xB);                       /* sub_0805B5E8(0xB) */
    s->child14 = in->spawn14_ok;
    if (!s->child14) {
        s->flags59 |= 0x8;                  /* _0805C506 marks; the
                                               shared tail deletes */
        s->deleted = true;
        return;
    }
    /* child +0x42 = 0x78 - A094[6]; child +0x46 = 0xB0 - A094[10];
     * F2C8(child,0,0,1); sub_0803F17C(parent — the ROM passes r7,
     * the parent entity); child +0 = sub_0805C39C. approximated:
     * the child screen placement and update-vector install stay
     * engine-side; child29 state is initialized below. The
     * placement and the sub_0805B5E8(0xB) record are persisted
     * on the shared child14 fields for the draw pass. */
    child_anim3(out, 0, 0, 1);
    s->child14_scr42 = (int16_t)(0x78 - in->a094_6);
    s->child14_scr46 = (int16_t)(0xB0 - in->a094_a);
    s->child14_record = 0xB;
    s->child14_anim6d = 0;
    out->anim_step_count++;
    memset(&s->child29, 0, sizeof s->child29);
    s->child29.state = 0;
    out->player58_or_20 = true;             /* player +0x58 |= 0x20 */
    out->player_scr42_set = true;
    out->player_scr42_val = 0xA0 - (int32_t)in->a094_6;
    out->player_pos44_set = true;
    out->player_pos44_val = 0xBF - (int32_t)in->a094_a;
    out->set_42c |= 1 | 0x10000;
    out->fade_3d18c = true;                 /* sub_0803D18C(0,0,0,0) */
    out->fade_3cdf0 = true;                 /* sub_0803CDF0(0,0,0,0) */
    out->unk2cb0_feff = true;
    s->state = 1;
    s->sub_c = 0;
    s->sub_b = 0;
    /* trailing 0x040000D4 -> 0x06004000/0x85000800 ->
     * 0x0600E000/0x81000400 DMA chain with source word 0 —
     * reported through dma_setup. */
    out->dma_setup = true;
}

static int update29(AosObject20Story *s, const AosObject20StoryIn *in,
                    AosObject20StoryOut *out) {
    uint8_t st = s->state;
    if (st == 0) {
        if (s->sub_b == 0) {
            if (s->sub_c == 0) {
                out->eec = 0x25;            /* sub_08013EEC(0x25) */
                s->sub_c = 1;
            }
            if (in->fade_done) {            /* sub_0803D408() != 0 */
                out->unk2cb0_feff = true;   /* gUnk_03002CB0 &= 0xFEFF */
            } else {
                s->sub_b = 1;
                s->sub_c = 0;
            }
        } else {
            out->dma_setup = true;
            out->unk2cb0_or100 = true;      /* |= 0x100 */
            s->timer_d = 0xFF;
            s->state = 1;
            s->sub_c = 0;
            s->sub_b = 0;
        }
    } else if (st == 1) {
        s->timer_d = (uint8_t)(s->timer_d - 1);
        if (s->timer_d == 0xFF) {
            out->fade_3d18c = true;         /* (0,0,0,0x10000) */
            out->fade_3cdf0 = true;
            out->unk2cb0_feff = true;
            out->jingle = 0x2D;             /* sub_08012048(0x2D) */
            s->state = 2;
            s->sub_c = 0;
            s->sub_b = 0;
        }
    } else if (st == 2) {
        if (!in->fade_done) {               /* sub_0803D408() == 0 */
            out->flags37e_clear1 = true;    /* +0x37E &= ~1 */
            out->ev_121e0 = 0x28;           /* sub_080121E0(0x28) */
            out->call_127f0_off = 0x428;
            out->call_127f0_val = 1;
            s->state = 0x80;
            out->window_close = true;       /* sub_08067898 */
            out->write_64_0a = true;
        }
    }
    out->set_42c |= 0x20;
    out->anim_step_count++;                 /* sub_0803F17C tail */
    out->return_code = 0;
    return 0;
}

/* ------------------------------------------------------------------
 * sub_0805C328 — the child29 state-3 record/effect helper.
 * 8-byte records at 0x085267B8 indexed by +0x1A (0..3, wraps):
 * each call bumps +0x18 and compares the OLD value against the
 * record's +6 hword; on exceed it resets +0x18, fires
 * sub_0803C7B4(record+0, record+4, entity+0x2C, 1), advances
 * +0x1A — wrapping past 3 returns 0 (the caller parks the
 * child at state 0x80) — and fires sub_0803CF34 on the NEW
 * record before returning 1. approximated: the record table
 * is injected (in->c328_table_085267b8); NULL keeps the
 * pre-extraction c328_ok return-value stand-in.
 * ------------------------------------------------------------------ */
static bool c328_step(AosObject20Story *s, const AosObject20StoryIn *in,
                      AosObject20StoryOut *out) {
    const uint8_t *t = in->c328_table_085267b8;
    if (!t)
        return in->c328_ok;
    int idx = (int16_t)s->child29.field1a;
    if (idx < 0 || idx > 3)
        idx = 0;
    const uint8_t *rec = t + (unsigned)idx * 8;
    uint16_t old18 = s->child29.field18;
    s->child29.field18 = (uint16_t)(old18 + 1);
    int32_t o = (int32_t)(int16_t)old18;        /* lsls/asrs */
    uint32_t thresh = (uint32_t)rec[6] |
                      ((uint32_t)rec[7] << 8);  /* ldrh +6 */
    if (o <= (int32_t)thresh)                   /* ble: keep waiting */
        return true;
    s->child29.field18 = 0;
    out->c328_c7b4 = true;
    int nidx = (int)s->child29.field1a + 1;
    s->child29.field1a = (int16_t)nidx;
    if (nidx > 3) {
        s->child29.field1a = 0;
        return false;                           /* +0x1A wrapped */
    }
    rec = t + (unsigned)nidx * 8;
    out->c328_cf34 = true;
    return true;
}

/* ------------------------------------------------------------------
 * sub_0805C39C — the param0-31 +0x14 child's update (create31's
 * sub_0805B5E8(0xB) spawn installs it as the child's +0).
 * ------------------------------------------------------------------ */
void aos_object20_story_child29_update(AosObject20Story *s,
                                       const AosObject20StoryIn *in,
                                       AosObject20StoryOut *out) {
    (void)in;
    uint8_t st = s->child29.state;
    if (st == 0) {
        out->child_anim_step_count++;       /* sub_0803F17C */
        return;
    }
    if (st == 1) {
        /* _0805C3BA: +0xC == 0 clears +0x59 bit 1 once, then the
         * state waits for +0x59 bit 0 — anim step while clear,
         * advance to state 2 when set. */
        if (s->child29.ctr_c == 0) {
            s->child29.flags59 &= ~0x2u;    /* +0x59 &= ~2 */
            s->child29.ctr_c = 1;
        }
        if (s->child29.flags59 & 1) {
            /* _0805C48E tail: +0xA = 2, +0xB = +0xC = 0. */
            s->child29.state = 2;
            s->child29.sub_b = 0;
            s->child29.ctr_c = 0;
        } else {
            out->child_anim_step_count++;    /* sub_0803F17C */
        }
        return;
    }
    if (st == 2) {
        if (s->child29.ctr_c == 0) {
            /* Spawn the type-0xB grandchild at own +0x42 - 0xB /
             * +0x46 - 0x10 with +0x48 = 0xFFFF6000, +0x4C =
             * 0x2000, +0x50 = 0x100, +0x54 = 0x1000, +0x65 = 3
             * and +0 = sub_0805C284; on a failed spawn the
             * native still writes the child's +0x65/+0xC/+0xD
             * below. The grandchild's own screen words are the
             * +0x14 child's +0x42/+0x46 — the child14_*
             * fields. approximated: the spawn is reported and
             * gc.spawned stands in for the sub_0805B5E8
             * pointer (in->spawn14_ok). */
            spawn1(out, 0xB);
            if (in->spawn14_ok) {
                s->gc.spawned = true;
                s->gc.state = 0;
                s->gc.sub_b = 0;
                s->gc.ctr_c = 0;
                s->gc.scr42 = (int16_t)(s->child14_scr42 - 0xB);
                s->gc.scr46 = (int16_t)(s->child14_scr46 - 0x10);
                s->gc.pos40 = 0;
                s->gc.pos44 = 0;
                s->gc.vel48 = (int32_t)0xFFFF6000;
                s->gc.vel4c = 0x2000;
                s->gc.acc50 = 0x100;
                s->gc.acc54 = 0x1000;
                s->gc.anim65 = 3;
            }
            s->child29.anim65 = 2;          /* +0x65 = state(2) */
            out->ent65_set = true;
            out->ent65_val = 2;
            s->child29.ctr_c = 1;
            s->child29.timer_d = 0x20;
        }
        s->child29.timer_d = (uint8_t)(s->child29.timer_d - 1);
        if (s->child29.timer_d == 0xFF) {
            /* _0805C48E tail: +0xA = 3, +0xB = +0xC = 0. */
            s->child29.state = 3;
            s->child29.sub_b = 0;
            s->child29.ctr_c = 0;
        }
        return;
    }
    if (st == 3) {
        if (s->child29.ctr_c == 0) {
            s->child29.field18 = 0;
            s->child29.field1a = 0;
            s->child29.ctr_c = 1;
        }
        if (!c328_step(s, in, out)) {        /* sub_0805C328 == 0 */
            /* _0805C48E tail: +0xA = 0x80, +0xB = +0xC = 0. */
            s->child29.state = 0x80;
            s->child29.sub_b = 0;
            s->child29.ctr_c = 0;
        }
        return;
    }
    /* states >= 4 (incl. 0x80) return without work. */
}

/* ------------------------------------------------------------------
 * sub_0805C218 — the param0-31 grandchild's floor-hit debris
 * burst: 12 sub_0805B5E8(0xB) entities from the 0x08526758
 * table (8-byte records), each with +0 = sub_0805CB08,
 * +0x54 = 0x2000 and the record's +0x65/+0x42/+0x46/+0x48/
 * +0x4C/+0xD fields. approximated: in->spawn14_ok stands in
 * for each spawn's non-NULL result and a NULL
 * in->debris_table_08526758 reports the burst without
 * populating the debris (the table is not committed).
 * ------------------------------------------------------------------ */
static void gc_burst(AosObject20Story *s, const AosObject20StoryIn *in,
                     AosObject20StoryOut *out) {
    int16_t x = s->gc.scr42;
    int16_t y = (int16_t)(s->gc.scr46 - 2);    /* _0805C1FE pair */
    if (!in->spawn14_ok)
        return;                     /* every spawn fails natively */
    out->debris_spawn_count += 12;
    if (!in->debris_table_08526758) {
        out->debris_table_missing = true;
        return;
    }
    const int8_t *t = in->debris_table_08526758;
    for (unsigned i = 0; i < 12; ++i, t += 8) {
        AosObject20GcDebris *d = &s->gc.debris[i];
        d->active = true;
        d->flags59 = 0;
        d->scr42 = (int16_t)(x + t[1]);
        d->scr46 = (int16_t)(y + t[2]);
        d->pos40 = 0;
        d->pos44 = 0;
        d->vel48 = (int32_t)((int32_t)t[3] << 12);
        d->vel4c = (int32_t)(-((int32_t)t[4] << 12));
        d->acc54 = 0x2000;
        d->timer_d = (uint8_t)t[5];
        d->anim65 = (uint8_t)t[0];
    }
}

/* ------------------------------------------------------------------
 * sub_0805CB08 — one debris entity's update: the sub_080020A0
 * floor probe on the +0x42/+0x46 screen words + A094 origin;
 * a hit marks +0x59 |= 8 (the engine delete mark), a miss
 * integrates +0x40/+0x44 by +0x48/+0x4C and +0x4C by +0x54.
 * ------------------------------------------------------------------ */
static void debris_update(AosObject20Story *s, unsigned i,
                          const AosObject20StoryIn *in,
                          AosObject20StoryOut *out) {
    (void)out;
    AosObject20GcDebris *d = &s->gc.debris[i];
    if (!d->active || (d->flags59 & 8))
        return;                     /* +0x59 bit 3 = deleted */
    int32_t x = (int32_t)d->scr42 + (int32_t)(int16_t)in->a094_6;
    int32_t y = (int32_t)d->scr46 + (int32_t)(int16_t)in->a094_a;
    int32_t hit = in->probe_020a0
        ? in->probe_020a0(x, y, in->probe_020a0_ctx) : 0;
    if (hit != 0) {
        d->flags59 |= 8;
    } else {
        d->pos40 += d->vel48;
        d->pos44 += d->vel4c;
        d->vel4c += d->acc54;
    }
}

/* ------------------------------------------------------------------
 * sub_0805C284 — the param0-31 grandchild's own update (+0).
 * ------------------------------------------------------------------ */
void aos_object20_story_gc_update(AosObject20Story *s,
                                  const AosObject20StoryIn *in,
                                  AosObject20StoryOut *out) {
    if (!s->gc.spawned)
        return;
    if (s->gc.state == 0) {
        /* _0805C2A8: probe world px = +0x42/+0x46 + A094
         * origin; nonzero = ground hit -> the sub_0805C218
         * burst + PlaySong(0xAA) + land at state 1. */
        int32_t x = (int32_t)s->gc.scr42 +
                    (int32_t)(int16_t)in->a094_6;
        int32_t y = (int32_t)s->gc.scr46 +
                    (int32_t)(int16_t)in->a094_a;
        int32_t hit = in->probe_020a0
            ? in->probe_020a0(x, y, in->probe_020a0_ctx) : 0;
        if (hit != 0) {
            gc_burst(s, in, out);
            out->song = 0xAA;               /* PlaySong(0xAA) */
            s->gc.state = 1;
            s->gc.sub_b = 0;                /* old +0xA */
            s->gc.ctr_c = 0;
        } else {
            /* _0805C2E8: integrate; +0x48 += +0x50, +0x4C +=
             * +0x54; a positive new +0x48 clamps to the probe
             * result (0 on this path). */
            s->gc.pos40 += s->gc.vel48;
            s->gc.pos44 += s->gc.vel4c;
            s->gc.vel48 += s->gc.acc50;
            s->gc.vel4c += s->gc.acc54;
            if (s->gc.vel48 > 0)
                s->gc.vel48 = hit;
        }
    } else {
        /* _0805C30E: any nonzero state arms +0x65 = 8 once
         * through +0xC — no integration, no anim step. */
        if (s->gc.ctr_c == 0) {
            s->gc.anim65 = 8;
            s->gc.ctr_c = 1;
        }
    }
    /* The debris are separate entities in the ROM; their
     * sub_0805CB08 updates run every frame once spawned. */
    for (unsigned i = 0; i < 12; ++i)
        debris_update(s, i, in, out);
}

/* ------------------------------------------------------------------
 * The child-entity update pass — see the header for the
 * per-param0 dispatch.
 * ------------------------------------------------------------------ */
void aos_object20_story_children_update(AosObject20Story *s,
                                        uint8_t param0,
                                        const AosObject20StoryIn *in,
                                        AosObject20StoryOut *out) {
    switch (param0) {
    case 9:
        /* +0x14 child (record 5) and +0x18 link (record 1):
         * both keep the sub_0805B748 update — a bare
         * sub_0803F17C animation step. update9 already wrote
         * their +0x58/+0x59/+0x6D picks and the link's +0x40
         * follower sync. */
        if (s->child14)
            out->child_anim_step_count++;
        if (s->link18)
            out->child_anim_step_count++;
        break;
    case 23:
        /* +0x14 escort child (record 1): same sub_0805B748
         * anim step; nothing else tracks it natively. */
        if (s->child14)
            out->child_anim_step_count++;
        break;
    case 31:
        if (s->child14)
            aos_object20_story_child29_update(s, in, out);
        aos_object20_story_gc_update(s, in, out);
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------------
 * param0 19 — create sub_08061A00 (native create[19]; the
 * param0-19 update is the shop sell routine sub_08061ABC in
 * aos_shop, not this module).
 * Gates: sub_08012078(0x29) clear AND player +0x42 + A094[6]
 * (both s16) > 0x7F, else +0x59 |= 8. sub_08061FDC's
 * sub_0806E0D0 probe then decides: a dead probe releases
 * (sub_0805B66C(1)) and deletes; a live one parks +0x42 at
 * 0x150 - A094[6], writes +0x48 = 0xFFFF0000 and mirrors
 * facing into +0x58.
 * ------------------------------------------------------------------ */

/* sub_08061FDC: on a live sub_0806E0D0 result (low result byte
 * nonzero) it sets +0x5C = 0x11 and returns 1; otherwise it
 * EntityDeletes and returns 0. The ROM data pointers
 * (0x08252748/0x0811892C/0x0820C028) are engine-side — only the
 * probe result is injected via in->ret_6e0d0. */
static int helper61fdc(AosObject20Story *s,
                       const AosObject20StoryIn *in) {
    if ((in->ret_6e0d0 & 0xFF) != 0) {
        s->anim5c = 0x11;               /* +0x5C = 0x11 */
        return 1;
    }
    s->deleted = true;                  /* EntityDelete */
    return 0;
}

static void create19(AosObject20Story *s, const AosObject20StoryIn *in,
                     AosObject20StoryOut *out) {
    if (in->flag12078_29 != 0 ||
        (int32_t)(int16_t)in->player_scr42 +
            (int32_t)(int16_t)in->a094_6 <= 0x7F) {
        s->flags59 |= 0x8;              /* shared tail deletes */
        return;
    }
    if (helper61fdc(s, in) == 0) {
        out->release_arg = 1;           /* sub_0805B66C(e, 1) */
        return;                         /* already deleted */
    }
    s->scr42 = (int16_t)(0x150 - in->a094_6);   /* 0xA8 << 1 */
    s->vel48 = (int32_t)0xFFFF0000;             /* +0x48 word */
    s->flags58 = (uint8_t)((s->flags58 & ~0x41u) | 0x20);
    anim3(out, 1, 1, 1);                /* sub_0803F2C8(e,1,1,1) */
    out->anim_step_count++;             /* sub_0803F17C */
    out->set_42c |= 1;
}

/* ------------------------------------------------------------------
 * param0 29 — create sub_0805BF4C (native create[29]; the
 * param0-29 update is sub_0805C08C, ported as update29).
 * Gate: +0x42C bit 0x8000 set AND sub_0805B754(e,7) keeps the
 * entity — either failing marks +0x59 |= 8. Success places the
 * entity at (0x40 - A094[6], 0xBF - A094[10]), sets +0x58 bit
 * 0x40, parks the player escort pair (+0x58 |= 0x20,
 * +0x42 = own +0x42 + 0x18, +0x46 = own +0x46), arms +0x42C
 * bit 0 and runs the fade/DMA/2CB0 clear tail.
 * ------------------------------------------------------------------ */
static void create29(AosObject20Story *s, const AosObject20StoryIn *in,
                     AosObject20StoryOut *out) {
    if ((in->flag42c & 0x8000u) == 0 || !in->keep_5b754) {
        s->flags59 |= 0x8;              /* shared tail deletes */
        return;
    }
    anim3(out, 0, 0, 1);                /* sub_0803F2C8(e,0,0,1) */
    out->anim_step_count++;
    s->scr42 = (int16_t)(0x40 - in->a094_6);
    s->scr46 = (int16_t)(0xBF - in->a094_a);
    s->flags58 |= 0x40;
    out->player58_or_20 = true;         /* player +0x58 |= 0x20 */
    out->player_scr42_set = true;       /* player +0x42 = own +0x18 */
    out->player_scr42_val = (int32_t)s->scr42 + 0x18;
    out->player_pos44_set = true;       /* player +0x46 = own +0x46 */
    out->player_pos44_val = s->scr46;
    out->set_42c |= 1;
    out->fade_3d18c = true;             /* sub_0803D18C(0x1F,0x1F,
                                         * 0x1F,0) pair below */
    out->fade_3cdf0 = true;
    out->dma_setup = true;              /* 0x040000D4 -> 0x06004000
                                         * /0x85000800 -> 0x0600E000
                                         * /0x81000400 chain */
    out->unk2cb0_feff = true;           /* gUnk_03002CB0 &= 0xFEFF */
}

/* ------------------------------------------------------------------
 * param0 30 — create sub_0805B888 (native create[30]; the
 * param0-30 update is sub_0805B8F4, ported below).
 * Unconditional: +0x42C |= 0x80021, sub_0800EF6C, PlaySong
 * (0x1000), then the player entity parks at screen (0x78, 0x50)
 * with its +0x4A/+0x4E words cleared and +0x58 bit 0x20 set.
 * ------------------------------------------------------------------ */
static void create30(AosObject20Story *s, const AosObject20StoryIn *in,
                     AosObject20StoryOut *out) {
    (void)s; (void)in;
    out->set_42c |= 0x80021;            /* 0x80<<0xC | 0x20 | 1 */
    out->call_ef6c = true;              /* sub_0800EF6C */
    out->song = 0x1000;                 /* PlaySong(0x80 << 5) */
    out->player_scr42_set = true;       /* player +0x42 = 0x78 */
    out->player_scr42_val = 0x78;
    out->player_pos44_set = true;       /* player +0x46 = 0x50 */
    out->player_pos44_val = 0x50;
    out->player_w4a_zero = true;        /* player +0x4A hword = 0 */
    out->player_w4e_zero = true;        /* player +0x4E hword = 0 */
    out->player58_or_20 = true;         /* player +0x58 |= 0x20 */
}

/* ------------------------------------------------------------------
 * sub_080628AC — the param0-30 whiteout/fade-to-white cinematic,
 * called every frame by update30 state 1 (_0805B958). Its state
 * lives on the entity: +0xB is the case index (0-6, values above
 * 6 fall to the tail), +0xC/+0xD the arm/frame counters shared
 * with update30, +0x15 the sub_08064518 sub-state the case-3 gate
 * waits for (== 0xA). The gEwramData +4/+6 halfwords
 * (EwramData.unk_4/unk_6) are the shared cinematic counters:
 * case 0 holds both at 0 every call, cases 2-5 ramp +4 and the
 * _08062B40 tail increments +6 unconditionally. Returns sb: 1
 * only from case 6.
 *
 * sub_08064518 (the ~1500-line whiteout-bar renderer — BgAffineSet
 * loops, PlaySong, palette DMA, its own +0x14/+0x15/+0x16/+0x18/
 * +0x1C..+0x24/+0x2C/+0x36/+0x3C sub-state machine) and
 * sub_08065800 (the 0x085268A8 tile pick through sub_0803C918,
 * already modelled as aos_seq_table_pick in aos_seq_particles —
 * the table itself is not committed) are engine-side: reported
 * through Out. The only effect the whiteout state machine reads
 * from 64518 is the +0x15 byte, injected per call through
 * in->wo_field15. sub_0806E0D0 is not on this path (it is the
 * param0-19 sprite-variant loader, already injected as
 * in->ret_6e0d0).
 * ------------------------------------------------------------------ */
static void whiteout_64518(AosObject20Story *s,
                           const AosObject20StoryIn *in,
                           AosObject20StoryOut *out) {
    out->call_64518 = true;
    out->call_65800 = true;     /* sub_08065800 follows every call */
    /* sub_08064518 ported in aos_object20_whiteout_render: the
     * +0x15 sub-state machine, RNG jitter, bar geometry, fades,
     * spawns and purges. s->wo_render carries the entity bytes
     * (+0x14/+0x15/+0x16/+0x18/+0x1C..+0x24/+0x2C/+0x34..+0x3C/
     * +0x42/+0x46); +0xD stays shared with sub_080628AC exactly
     * like the native byte. s->field15 mirrors field_15 for the
     * case-3 gate; in->wo_field15 remains a post-call override. */
    AosWoRenderIn win;
    memset(&win, 0, sizeof win);
    win.rng = in->wo_rng;
    win.ewram_7864 = in->wo_7864;
    win.field10 = in->field10;
    AosWoRenderOut wout;
    aos_wo_render_update(&s->wo_render, &win, &wout);
    s->field15 = s->wo_render.field_15;
    if (in->wo_field15 >= 0)
        s->field15 = (uint8_t)in->wo_field15;
    /* Forward the renderer's reports: the RNG state round-trips,
     * PlaySong lands on the shared song channel, and the bar
     * records/events are exposed for the host (no BG2-affine or
     * spawn-table consumer exists yet — documented gap). */
    out->wo_rng = (int64_t)(uint32_t)wout.rng;
    out->wo_bars_drawn = wout.bars_drawn;
    if (wout.bars_drawn)
        memcpy(out->wo_bars, wout.bars, sizeof out->wo_bars);
    out->wo_palette_copy = wout.palette_copy;
    out->wo_cycle_6447c = wout.cycle_6447c;
    out->wo_purge_547 = wout.purge_547;
    out->wo_purge_flagged = wout.purge_flagged;
    out->wo_burst_63aec = wout.burst_63aec;
    out->wo_burst_x = wout.burst_x;
    out->wo_burst_y = wout.burst_y;
    out->wo_spawn_count = wout.spawn_count;
    if (wout.spawn_count)
        memcpy(out->wo_spawns, wout.spawns,
               wout.spawn_count * sizeof(AosWoSpawn));
    out->wo_particle_count = wout.particle_count;
    if (wout.song >= 0)
        out->song = wout.song;
    if (wout.fade_pair) {
        out->fade_pair_wo = true;
        out->fade_pair_a3 = (int)wout.fade_a3;
        out->fade_pair_s0 = (int)wout.fade_s0;
        out->fade_pair_s1 = (int)wout.fade_s1;
    }
}

int aos_object20_whiteout_update(AosObject20Story *s,
                                 const AosObject20StoryIn *in,
                                 AosObject20StoryOut *out) {
    out->call_628ac = true;
    int result = 0;
    uint16_t unk4 = in->ewram_unk4;
    uint16_t unk6 = in->ewram_unk6;
    bool unk4_written = false;
    switch (s->sub_b) {
    case 0: /* _080628F0: display init, then the +0xD wrap arm. */
        if (s->sub_c == 0) {
            s->timer_d = 4;
            s->sub_c = 1;
        }
        out->unk2cb0_zero = true;    /* gUnk_03002CB0 hword = 0 */
        out->wo_disp_init = true;    /* gDisplayRegisters +8/+0xA/
                                      * +0xC/+0xE/+0x10/+0x12 = 0,
                                      * +0x48 = 0xFF, +0x4A = 0x1000,
                                      * +0x4C = 0x10 */
        unk4 = 0;
        unk6 = 0;
        unk4_written = true;
        s->timer_d = (uint8_t)(s->timer_d - 1);
        if (s->timer_d == 0xFF) {
            out->fade_out = true;    /* sub_0803D270 + sub_0803CED4 */
            s->sub_b = 1;
        }
        break;
    case 1: /* _0806295C: one-time layer/palette setup. */
        out->unk2cb0_1e01 = true;   /* gUnk_03002CB0 = 0x1E01 */
        out->wo_disp_stage = true;  /* disp +2 = 0x1D09, +4 = 0x5E80 */
        out->call_3dd14 = true;     /* sub_0803DD14(0x16,1) + (6,2) */
        out->call_3fd60 = true;
        out->call_3fd9c = true;     /* (0x08119890,0x6000000,0) +
                                     * (0x08119898,0x06008000,0) */
        out->call_3c918 = true;     /* (0x08119FDC,0,0x10,0) */
        out->call_3f8a8 = true;     /* (1,0x0815DE6C,0,0x30) +
                                     * (2,0x0815DDDC,0,0) */
        s->sub_b = 2;
        break;
    case 2: /* _080629E8: the +4 scroll ramp; display +0x4C mirrors
             * 0x10 - (unk_4 >> 1) and the ramp ends when that
             * halfword reads 0. */
        whiteout_64518(s, in, out);
        {
            uint32_t v = 0x10u - (uint32_t)(unk4 >> 1);
            out->wo_disp4c = (int16_t)v;    /* strh — low halfword */
            if ((uint16_t)v == 0) {
                out->wo_disp48 = (int16_t)(0xFDu << 6); /* 0x3F40 */
                unk4 = 0;
                unk4_written = true;
                s->sub_b = 3;
            } else if ((s->timer_d & 7) == 0) {
                unk4 = (uint16_t)(unk4 + 1);
                unk4_written = true;
            }
        }
        s->timer_d++;
        break;
    case 3: /* _08062A40: gated on +0x15 == 0xA, then the +4 ramp
             * to 0x33. */
        whiteout_64518(s, in, out);
        if (s->field15 == 0xA) {
            uint16_t old = unk4;
            unk4 = (uint16_t)(unk4 + 1);
            unk4_written = true;
            if (old > 0x32) {
                unk4 = 0;
                s->sub_b = 4;
            }
        }
        break;
    case 4: /* _08062A70: the +4 ramp to 0x81 ends in the fade
             * pair and the (0x1F,0x1F,0x1F,0x10000) fades. */
        whiteout_64518(s, in, out);
        {
            uint16_t old = unk4;
            unk4 = (uint16_t)(unk4 + 1);
            unk4_written = true;
            if (old > 0x80) {
                out->fade_out = true;        /* 3D270 + 3CED4 */
                out->fade_3d18c = true;      /* args 0x1F,0x1F,0x1F,
                                               * 0x80 << 9 */
                out->fade_3cdf0 = true;
                unk4 = 0;
                s->sub_b = 5;
            }
        }
        break;
    case 5: /* _08062AD0: the +4 ramp to 0x41 ends in the
             * 0x040000D4 register chain. */
        whiteout_64518(s, in, out);
        {
            uint16_t old = unk4;
            unk4 = (uint16_t)(unk4 + 1);
            unk4_written = true;
            if (old > 0x40) {
                out->dma_setup_wo = true;   /* -> 0x0600E800,
                    * 0x0600F000, 0x0600F800, 0x85000200 */
                s->sub_b = 6;
            }
        }
        break;
    case 6: /* _08062B3C */
        result = 1;
        break;
    default:
        break;                 /* +0xB > 6: tail only, sb stays 0 */
    }
    /* _08062B40 tail: the EwramData +6 halfword increments on
     * every call, whatever the case did. */
    unk6 = (uint16_t)(unk6 + 1);
    out->wo_unk6 = (int)unk6;
    if (unk4_written)
        out->wo_unk4 = (int)unk4;
    return result;
}

/* ------------------------------------------------------------------
 * param0 30 — update sub_0805B8F4 (native update[30]).
 * +0xA states 0-4 drive the arena-entry cinematic; every call
 * runs the shared tail (_0805BBB6): a freshly-read +0xA <= 1
 * parks the player entity and calls sub_0801D028(0x33,0,0),
 * +0xA >= 2 scrolls +0x18 through sub_080009E4/
 * sub_0801D028(0x33,0,0,r3) and edge-spawns the sub_08045B44
 * particle; all paths end +0x42C |= 0x20. Returns 0.
 * ------------------------------------------------------------------ */
static int update30(AosObject20Story *s, const AosObject20StoryIn *in,
                    AosObject20StoryOut *out) {
    uint8_t st = s->state;
    if (st <= 4) {
        switch (st) {
        case 0:
            /* _0805B92C: the ROM reaches _0805BA6C on BOTH the
             * timer-wrap and the running path, so +0xA advances
             * every call (twice on a wrap) — ported literally. */
            if (s->sub_c == 0) {
                s->timer_d = 8;
                s->sub_c = 1;
            }
            s->timer_d = (uint8_t)(s->timer_d - 1);
            if (s->timer_d == 0xFF) {
                s->state++;
                s->sub_c = 0;
                s->sub_b = 0;
            }
            s->state++;
            s->sub_c = 0;
            s->sub_b = 0;
            break;
        case 1:
            /* _0805B958: +0xC arms once, then sub_080628AC (the
             * whiteout sequence, ported above) gates the
             * completion block on its case-6 result. */
            if (s->sub_c == 0)
                s->sub_c = 1;
            if (aos_object20_whiteout_update(s, in, out) != 0) {
                s->field18 = 0;             /* +0x18 hword = 0 */
                out->player58_andnot_21 = true; /* &= ~0x21 */
                out->cf0 = 0x20;            /* sub_08013CF0(0x20) */
                s->state++;
                s->sub_c = 0;
                s->sub_b = 0;
                out->call_3c7b4 = true;     /* sub_0803C7B4(
                    * 0x082099FC,5,1,0xE) */
            }
            break;
        case 2:
            /* _0805B9B4: +0xB substeps — the 2CB0 flag clears,
             * the DMA fade chain, window 0x25 and the
             * sub_0803D06C gate. */
            switch (s->sub_b) {
            case 0:
                out->unk2cb0_f1ff = true;   /* &= 0xF1FF */
                s->sub_b = 1;
                s->sub_c = 0;
                break;
            case 1:
                out->dma_setup = true;      /* 0x040000D4 chain:
                    * src word 0xFFFF -> 0x06004000/0x85000800,
                    * src gEwramData+0x1261C -> 0x0600E000/
                    * 0x81000400 */
                out->window_text = 0x25;    /* sub_08067934(0x25) */
                s->sub_b = 2;
                s->sub_c = 0;
                s->timer_d = 0;
                break;
            case 2:
                out->fade_3cdf0 = true;     /* sub_0803CDF0(
                    * 0x1F,0x1F,0x1F,0) */
                s->sub_b = 3;
                s->sub_c = 0;
                break;
            default:
                if (in->ret_3d06c == 0) {
                    out->unk2cb0_f1ff = true;
                    s->state++;
                    s->sub_c = 0;
                    s->sub_b = 0;
                }
                break;
            }
            break;
        case 3:
            /* _0805BA80: window poll; a done poll advances +0xA
             * once. +0xB == 1 runs the +0xD 0x40 hold that
             * clears +0x417 and bumps +0xB. */
            out->window_poll = true;        /* sub_08067A04 */
            if (in->text_done) {
                s->state++;
                s->sub_c = 0;
                s->sub_b = 0;
            }
            if (s->sub_b == 1) {
                if (s->sub_c == 0) {
                    out->write_417 = true;  /* +0x417 = 1 */
                    out->write_417_val = 1;
                    s->timer_d = 0x40;
                    s->sub_c = 1;
                }
                s->timer_d = (uint8_t)(s->timer_d - 1);
                if (s->timer_d == 0xFF) {
                    out->write_417 = true;  /* +0x417 = 0 */
                    out->write_417_val = 0;
                    s->sub_b++;
                    s->sub_c = 0;
                }
            }
            break;
        case 4: {
            /* _0805BAE4: +0xB substeps — the fade arm, the
             * sub_0803D06C-gated event block and the +0xD/+0xC
             * 0x1040 sfx hold (_0805BB9C). */
            uint8_t b = s->sub_b;
            if (b == 0) {
                out->fade_3cdf0 = true;     /* sub_0803CDF0(
                    * 0x1F,0x1F,0x1F,0) */
                s->sub_b = 1;
                s->sub_c = 0;
                s->timer_d = 0x20;
                out->f6c = 0x1040;          /* sub_08013F6C(0x1040) */
                break;
            }
            if (b == 1 && in->ret_3d06c == 0) {
                out->call_3d004 = 0xE;      /* sub_0803D004(0xE) */
                out->call_3d3a0 = 0;       /* sub_0803D3A0(0) */
                out->write_a074 = true;     /* +0xA074 &= ~3 */
                out->set_42c |= 0x800000;   /* 0x80 << 0x10 */
                out->call_11f44 = true;     /* sub_08011F44(
                    * 0x08522C54,0,0,0) */
                out->call_11ad0 = true;     /* sub_08011AD0(
                    * gEwramData + 0x60) */
                out->disp48_zero = true;    /* gDisplayRegisters
                    * +0x48 hword = 0 */
                s->sub_b = 2;
                s->sub_c = 0;
                out->set_42c |= 0x4000;     /* 0x80 << 7 */
                break;
            }
            /* _0805BB9C: +0xB >= 2, or +0xB == 1 with
             * sub_0803D06C busy — +0xD == 0 && +0xC == 0 fires
             * sub_08013F6C(0x1040) once. */
            if (s->timer_d == 0 && s->sub_c == 0) {
                out->f6c = 0x1040;
                s->sub_c = 1;
            }
            break;
        }
        default:
            break;
        }
    }
    /* Shared tail (_0805BBB6): the ROM re-reads +0xA here, so
     * the branch uses the post-case state. */
    if (s->state <= 1) {
        /* _0805BC5C: park the player entity (+0x42 = 0x78,
         * +0x46 = 0x50, +0x48/+0x4C = 0) then
         * sub_0801D028(0x33,0,0) — r3 is leftover; reported 0. */
        out->player_scr42_set = true;
        out->player_scr42_val = 0x78;
        out->player_pos44_set = true;
        out->player_pos44_val = 0x50;
        out->player_w48_zero = true;
        out->player_w4c_zero = true;
        out->call_1d028_a = 0x33;
    } else {
        /* _0805BC88 prologue: scroll = sub_080009E4(+0x18 +
         * 0x4000); negative results get +3 before the >>2;
         * sub_0801D028(0x33,0,0,r3); +0x18 += 0x200; +0xD--.
         * The old +0xD high nibble clear edge spawns the
         * sub_08045B44 particle at
         * (player+0x40 - 0x100000 + (rng1 & 0x7FFFF) * 4,
         *  player+0x44 - 0xC0000 + (rng2 & 0x7FFFF) * 2)
         * with args (x, y, 0, 0xFFFFE000, 7, 9). */
        out->call_9e4 = true;
        int32_t v = in->ret_9e4;
        if (v < 0)
            v += 3;
        out->call_1d028_a = 0x33;
        out->call_1d028_r3 = v >> 2;        /* asrs */
        s->field18 = (uint16_t)(s->field18 + 0x200);
        uint8_t old_d = s->timer_d;
        s->timer_d = (uint8_t)(s->timer_d - 1);
        if ((old_d & 0xF0) == 0) {
            out->call_45b44 = true;
            out->p45b44_x = in->player_pos40 +
                (int32_t)0xFFF00000 +
                (int32_t)((in->rng_p1 & 0x7FFFFu) << 2);
            out->p45b44_y = in->player_pos44 +
                (int32_t)0xFFF40000 +
                (int32_t)((in->rng_p2 & 0x7FFFFu) << 1);
        }
    }
    out->set_42c |= 0x20;
    out->return_code = 0;
    return 0;
}

/* ------------------------------------------------------------------
 * param0 31 — update sub_0805C628 (native update[31]; the
 * param0-31 create is sub_0805C49C, ported as create31).
 * +0xA states 0-6: the fade/whiteout hand-off, the +0xB 0-5
 * escort substeps with the sub_0806E140 floor probe, the
 * window-0x5A hold and the +0x14 child hand-off. Every call
 * ends sub_0803F17C + +0x42C |= 0x20. Returns 0.
 * ------------------------------------------------------------------ */
static int update31(AosObject20Story *s, const AosObject20StoryIn *in,
                    AosObject20StoryOut *out) {
    uint8_t st = s->state;
    if (st <= 6) {
        switch (st) {
        case 0:
            /* _0805C660: +0xC arms +0xD = 0x80; on wrap the fade
             * pair runs (0,0,0,0), +0xD clears and +0xA
             * advances. */
            if (s->sub_c == 0) {
                s->timer_d = 0x80;
                s->sub_c = 1;
            }
            s->timer_d = (uint8_t)(s->timer_d - 1);
            if (s->timer_d == 0xFF) {
                out->fade_3d18c = true;
                out->fade_3cdf0 = true;
                s->timer_d = 0;
                s->state++;
                s->sub_c = 0;
                s->sub_b = 0;
            }
            break;
        case 1:
            /* _0805C6B4: sub_08013CF0(0x21) once, then the
             * sub_0803D408 gate — a clear result runs the
             * fade-out pair, the DMA chain, +0xD = 0xFF and
             * advances; a set result only clears 2CB0 bit
             * 0x100. */
            if (s->sub_c == 0) {
                out->cf0 = 0x21;         /* sub_08013CF0(0x21) */
                s->sub_c = 1;
            }
            if (in->fade_done) {        /* sub_0803D408() != 0 */
                out->unk2cb0_feff = true;
            } else {
                out->fade_out = true;   /* 3D270 + 3CED4 */
                out->dma_setup = true;  /* 0x040000D4 chain */
                out->unk2cb0_or100 = true;
                s->timer_d = 0xFF;
                s->state++;
                s->sub_c = 0;
                s->sub_b = 0;
            }
            break;
        case 2:
            /* _0805C748: +0xB substeps 0-5 — the camera parks,
             * the +0x6D anim gates, the sub_0806E140 floor
             * probe and the +0x59-bit0 release jumps. */
            if (s->sub_b > 5)
                break;
            switch (s->sub_b) {
            case 0:
                /* _0805C778: +0xD = 0x80 hold; on wrap the
                 * player parks at (0xD0 - A094[6],
                 * 0xBF - A094[10]) and +0xB advances. */
                if (s->sub_c == 0) {
                    s->timer_d = 0x80;
                    s->sub_c = 1;
                }
                s->timer_d = (uint8_t)(s->timer_d - 1);
                if (s->timer_d == 0xFF) {
                    out->player_scr42_set = true;
                    out->player_scr42_val =
                        0xD0 - (int32_t)in->a094_6;
                    out->player_pos44_set = true;
                    out->player_pos44_val =
                        0xBF - (int32_t)in->a094_a;
                    s->sub_b++;
                    s->sub_c = 0;
                }
                break;
            case 1:
                /* _0805C7D4: +0xD = 0x40 hold, then +0xB. */
                if (s->sub_c == 0) {
                    s->timer_d = 0x40;
                    s->sub_c = 1;
                }
                s->timer_d = (uint8_t)(s->timer_d - 1);
                if (s->timer_d == 0xFF) {
                    s->sub_b++;
                    s->sub_c = 0;
                }
                break;
            case 2:
                /* _0805C7FE: +0x6D != 1 anim gate; own +0x42 +
                 * A094[6] > 0xB8 runs sub_0806E140(e,0x10000,
                 * 0x200000,0x40000,1) — a clear bit-2 result
                 * slides +0x44 by +0x4C; otherwise the player
                 * is re-parked and +0xB advances. */
                if (s->sub_c == 0) {
                    if (s->anim6d != 1)
                        anim3(out, 1, 1, 1);
                    s->sub_c = 1;
                }
                if ((int32_t)s->scr42 +
                        (int32_t)(int16_t)in->a094_6 > 0xB8) {
                    if ((in->ret_6e140 & 4) == 0)
                        s->pos44 += s->vel4c;
                } else {
                    out->player_scr42_set = true;
                    out->player_scr42_val =
                        0xA0 - (int32_t)in->a094_6;
                    out->player_pos44_set = true;
                    out->player_pos44_val =
                        0xBF - (int32_t)in->a094_a;
                    s->sub_b++;
                    s->sub_c = 0;
                }
                break;
            case 3:
                /* _0805C898: +0x6D != 2 anim gate; own +0x42 +
                 * A094[6] > 0xB0 runs sub_0806E140(e,0x8000,
                 * 0x200000,0x40000,1); +0x59 bit0 releases
                 * +0xB by 2. */
                if (s->sub_c == 0) {
                    if (s->anim6d != 2)
                        anim3(out, 2, 1, 0);
                    s->sub_c = 1;
                }
                if ((int32_t)s->scr42 +
                        (int32_t)(int16_t)in->a094_6 > 0xB0 &&
                    (in->ret_6e140 & 4) == 0)
                    s->pos44 += s->vel4c;
                if (s->flags59 & 1) {
                    if (s->anim6d != 0)
                        anim3(out, 0, 1, 1);
                    s->sub_b += 2;
                    s->sub_c = 0;
                }
                break;
            case 4:
                /* _0805C930: +0x6D != 5 anim gate; same >0xB0
                 * probe; +0x6E selects songs 0x1DF/0xFC; +0x59
                 * bit0 releases +0xB by 1. */
                if (s->sub_c == 0) {
                    if (s->anim6d != 5)
                        anim3(out, 5, 1, 0);
                    s->sub_c = 1;
                }
                if ((int32_t)s->scr42 +
                        (int32_t)(int16_t)in->a094_6 > 0xB0 &&
                    (in->ret_6e140 & 4) == 0)
                    s->pos44 += s->vel4c;
                if (s->field6e == 4)
                    out->song = 0x1DF;
                if (s->field6e == 8)
                    out->song = 0xFC;
                if (s->flags59 & 1) {
                    if (s->anim6d != 0)
                        anim3(out, 0, 1, 1);
                    s->sub_b++;
                    s->sub_c = 0;
                }
                break;
            case 5:
                /* _0805C9EC: window 0x5A once, +0xD = 0x44 hold;
                 * on wrap +0xA advances. */
                if (s->sub_c == 0) {
                    out->window_text = 0x5A;/* sub_08067934(0x5A) */
                    s->timer_d = 0x44;
                    s->sub_c = 1;
                }
                s->timer_d = (uint8_t)(s->timer_d - 1);
                if (s->timer_d == 0xFF) {
                    s->state++;
                    s->sub_c = 0;
                    s->sub_b = 0;
                }
                break;
            default:
                break;
            }
            break;
        case 3:
            /* _0805CA18: window poll; done advances +0xA. */
            out->window_poll = true;        /* sub_08067A04 */
            if (in->text_done) {
                s->state++;
                s->sub_c = 0;
                s->sub_b = 0;
            }
            break;
        case 4:
            /* _0805CA22: +0xD = 0x68 hold; on wrap the +0x14
             * child's +0xA/+0xB/+0xC advance and this entity
             * follows. */
            if (s->sub_c == 0) {
                s->timer_d = 0x68;
                s->sub_c = 1;
            }
            s->timer_d = (uint8_t)(s->timer_d - 1);
            if (s->timer_d == 0xFF) {
                s->child29.state++;         /* child +0xA += 1 */
                s->child29.sub_b = 0;       /* child +0xB = 0 */
                s->child29.ctr_c = 0;       /* child +0xC = 0 */
                s->state++;
                s->sub_c = 0;
                s->sub_b = 0;
            }
            break;
        case 5:
            /* _0805CA5A: child +0xA == 0x80 fires
             * sub_08013F6C(0x1040) once per call. */
            if (s->child29.state == 0x80)
                out->f6c = 0x1040;
            break;
        case 6:
            /* _0805CA78: one-shot fade pair + jingle 0x2D +
             * 2CB0 &= 0xFEFF; then the sub_0803D408 gate parks
             * +0xA = 0x80, closes the window and sets +0x64. */
            if (s->sub_c == 0) {
                out->fade_3d18c = true;     /* (0,0,0,0x10000) */
                out->fade_3cdf0 = true;
                out->unk2cb0_feff = true;
                out->jingle = 0x2D;         /* sub_08012048(0x2D) */
                s->sub_c = 1;
                s->timer_d = 0x80;
            }
            if (!in->fade_done) {           /* 3D408() == 0 */
                s->state = 0x80;
                out->window_close = true;   /* sub_08067898 */
                out->write_64_0a = true;    /* +0x64 = 0x0A */
            }
            break;
        default:
            break;
        }
    }
    out->anim_step_count++;                 /* sub_0803F17C tail */
    out->set_42c |= 0x20;
    out->return_code = 0;
    return 0;
}

/* ------------------------------------------------------------------
 * Dispatchers.
 * ------------------------------------------------------------------ */
bool aos_object20_story_create(AosObject20Story *s, uint8_t param0,
                               const AosObject20StoryIn *in,
                               AosObject20StoryOut *out) {
    out_reset(out);
    s->deleted = false;
    /* Shared Object20Create gates (0x0805B42C). */
    bool gate_pass;
    if (in->flag1327a > 0) {
        gate_pass = true;
    } else {
        gate_pass = (in->flag42c & 0x00200000u) != 0; /* 0x80 << 0xE */
    }
    if (gate_pass) {
        if (in->byte8e & 0x80) {
            gate_pass = false;
        } else if (in->byte13266 != 0 &&
                   (in->flag42c & 0x8000u) == 0) {
            gate_pass = false;
        }
    }
    if (gate_pass) {
        switch (param0) {
        case 9:  create9(s, in, out); break;
        case 10: create10(s, in, out); break;
        case 19: create19(s, in, out); break;
        case 20: create20(s, in, out); break;
        case 21: create21(s, in, out); break;
        case 22: create22(s, in, out); break;
        case 23: create23(s, in, out); break;
        case 29: create29(s, in, out); break;
        case 30: create30(s, in, out); break;
        case 31: create31(s, in, out); break;
        /* create[24] = sub_080657B4 (aos_npc_talk) is not
         * this module's routine. */
        default: s->deleted = true; break;
        }
        if (s->flags59 & 0x8)
            s->deleted = true;
    } else {
        s->deleted = true;
    }
    if (s->deleted)
        return false;
    /* Shared tail: +0x42C |= 0x200, engine init calls,
     * gEwramData + 0x41C = entity, conditional
     * SetPlayerInput(1, 0), sub_0806666C. */
    out->set_42c |= 0x200;
    out->call_0eb04 = true;
    out->call_235c8 = true;
    out->call_2dff0 = true;
    out->c5a8 = 0;
    out->call_6666c = true;
    if (in->flag42c & 1) {
        out->setplayerinput = 1;
        out->setplayerinput_arg = 0;
    }
    return true;
}

int aos_object20_story_update(AosObject20Story *s, uint8_t param0,
                              const AosObject20StoryIn *in,
                              AosObject20StoryOut *out) {
    out_reset(out);
    /* Shared Object20Update prologue (0x0805B510). */
    if (in->flag42c & 0x80)
        out->unk7864_or2 = true;
    if ((int8_t)s->state < 0) {            /* ldrsb +0xA < 0 */
        if (in->byte41b != 0)
            out->call_67804 = true;         /* sub_08067804(-3) */
    } else {
        if ((in->word16 & 8) && (in->word60 & 2) &&
            !(in->flag42c & 0x10000)) {
            out->unk414_write = true;
            out->unk414_val = (uint8_t)((in->byte414 & ~0x10u) | 5u);
        }
    }
    int ret = 0;
    switch (param0) {
    case 9:  ret = update9(s, in, out); break;
    case 10: ret = update10(s, in, out); break;
    case 21: ret = update21(s, in, out); break;
    case 22: ret = update22(s, in, out); break;
    case 23: ret = update23(s, in, out); break;
    case 24: ret = update24(s, in, out); break;
    case 29: ret = update29(s, in, out); break;
    case 30: ret = update30(s, in, out); break;
    case 31: ret = update31(s, in, out); break;
    default: break;
    }
    if (ret == 0 && out->return_code != 0)
        ret = out->return_code;
    /* The +0x14/+0x18 children are separate entities natively
     * (the entity loop runs their own +0 update callbacks);
     * composed here so the aggregate advances them once per
     * parent frame. Order vs. the tail below is immaterial —
     * the tail only reports input routing. */
    aos_object20_story_children_update(s, param0, in, out);
    /* Tail: +0x42C bit 0 selects SetPlayerInput(1, ret) +
     * sub_0800C5A8(0), else sub_0800C5A8(1). */
    if (in->flag42c & 1) {
        out->setplayerinput = 1;
        out->setplayerinput_arg = ret;
        out->c5a8 = 0;
    } else {
        out->c5a8 = 1;
    }
    return ret;
}
