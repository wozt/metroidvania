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
    out->release_arg = -1;
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
 * param0 21 — create sub_08062040 / update sub_08062074.
 * ------------------------------------------------------------------ */
static void create21(AosObject20Story *s, const AosObject20StoryIn *in,
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
 * param0 22 — create sub_08062068: unconditional +0x59 |= 8, so
 * the shared create tail always deletes the entity.
 * ------------------------------------------------------------------ */
static void create22(AosObject20Story *s, const AosObject20StoryIn *in,
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
 * param0 23 — create sub_08062244 / update sub_080623F0.
 * ------------------------------------------------------------------ */
static void create23(AosObject20Story *s, const AosObject20StoryIn *in,
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
 * param0 24 — create sub_08062260 / update sub_08062664.
 * ------------------------------------------------------------------ */
static void create24(AosObject20Story *s, const AosObject20StoryIn *in,
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
     * the child record writes stay engine-side. */
    child_anim3(out, 1, 0, 1);
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
 * param0 29 — create sub_0805C49C / update sub_0805C08C.
 * ------------------------------------------------------------------ */
static void create29(AosObject20Story *s, const AosObject20StoryIn *in,
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
     * engine-side; child29 state is initialized below. */
    child_anim3(out, 0, 0, 1);
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
 * sub_0805C39C — the param0-29 +0x14 child's update.
 * ------------------------------------------------------------------ */
void aos_object20_story_child29_update(AosObject20Story *s,
                                       const AosObject20StoryIn *in,
                                       AosObject20StoryOut *out) {
    (void)in;
    uint8_t st = s->child29.state;
    if (st == 0) {
        out->anim_step_count++;             /* sub_0803F17C */
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
            s->child29.state = 2;
            s->child29.ctr_c = 0;
            /* the ROM also clears +0xB — always 0 here. */
        } else {
            out->anim_step_count++;         /* sub_0803F17C */
        }
        return;
    }
    if (st == 2) {
        if (s->child29.ctr_c == 0) {
            /* Spawn the type-0xB grandchild at own +0x42 - 0xB /
             * +0x46 - 0x10 with the 0xFFFF6000 velocity pair —
             * approximated: the spawn is reported and the
             * placement/velocity writes stay engine-side. */
            spawn1(out, 0xB);
            s->child29.anim65 = 2;          /* +0x65 = state(2) */
            out->ent65_set = true;
            out->ent65_val = 2;
            s->child29.ctr_c = 1;
            s->child29.timer_d = 0x20;
        }
        s->child29.timer_d = (uint8_t)(s->child29.timer_d - 1);
        if (s->child29.timer_d == 0xFF) {
            s->child29.state = 3;
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
        if (!in->c328_ok) {                 /* sub_0805C328 == 0 */
            s->child29.state = 0x80;
            s->child29.ctr_c = 0;
        }
        return;
    }
    /* states >= 4 (incl. 0x80) return without work. */
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
        gate_pass = (in->flag42c & 0x20000000u) != 0;
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
        case 21: create21(s, in, out); break;
        case 22: create22(s, in, out); break;
        case 23: create23(s, in, out); break;
        case 24: create24(s, in, out); break;
        case 29: create29(s, in, out); break;
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
    default: break;
    }
    if (ret == 0 && out->return_code != 0)
        ret = out->return_code;
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
