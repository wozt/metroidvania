#include "aos_object1c.h"

#include <string.h>

/* sub_08068AD4(entity, x, y, x_limit, y_limit): unsigned
 * (entity + 0x42 - x) <= x_limit and (entity + 0x46 - y) <=
 * y_limit on the player entity's room-pixel position. The native
 * call is (player, 0x68, 0x80, 0x20, 0x37) — the same rectangle
 * aos_save_room_player_inside models. */
static bool player_in_rect(const AosObject1CIn *in) {
    return (unsigned)(in->player_x - 0x68) <= 0x20u &&
           (unsigned)(in->player_y - 0x80) <= 0x37u;
}

static void row_push(AosObject1CRow *rows, unsigned *count, uint32_t desc,
                     unsigned src, unsigned n, unsigned dst) {
    if (*count >= AOS_O1C_MAX_ROWS) return;
    rows[*count].desc = desc;
    rows[*count].src = (uint8_t)src;
    rows[*count].count = (uint8_t)n;
    rows[*count].dst = (uint8_t)dst;
    ++*count;
}

/* `palettes`/`obj_palettes` count the stored rows (capped at
 * AOS_O1C_MAX_ROWS; the native never exceeds two copies a frame). */
static void pal_push(AosObject1COut *out, uint32_t desc, unsigned src,
                     unsigned n, unsigned dst) {
    row_push(out->pal_rows, &out->palettes, desc, src, n, dst);
}

static void obj_pal_push(AosObject1COut *out, uint32_t desc, unsigned src,
                         unsigned n, unsigned dst) {
    row_push(out->obj_rows, &out->obj_palettes, desc, src, n, dst);
}

static void disp_push(AosObject1COut *out, unsigned reg, int value) {
    if (out->disp_writes >= AOS_O1C_MAX_ROWS) return;
    out->disp_reg[out->disp_writes] = (uint8_t)reg;
    out->disp_val[out->disp_writes] = (int16_t)value;
    ++out->disp_writes;
}

bool aos_object1c_create(AosObject1C *o, const AosObject1CCreateIn *in,
                          AosObject1CCreateOut *out) {
    *o = (AosObject1C){0};
    *out = (AosObject1CCreateOut){0};

    /* sub_0800EB04 (undecoded prologue) and sub_080412DC(0x06005000):
     * the text-canvas pointer. */
    out->canvas_set = true;

    /* sub_0803AFB8(0x082052E8) -> sub_0803B800(., 0) fallback;
     * still negative deletes the entity. */
    if (in->palette_index < 0) {
        o->deleted = true;
        out->deleted = true;
        return false;
    }
    o->palette = (int16_t)in->palette_index;    /* strh -> + 0x18 */

    /* sub_0803C7B4(0x0820C384, 0, 4, 0xA): static OBJ palette copy,
     * reported through the create path only as a constant — the
     * update Out rows carry per-frame copies. */
    (void)AOS_O1C_DESC_MAIN;

    o->anim_1a = 0xA;                            /* strh 0xA -> + 0x1A */

    /* sub_0803E654(1): lowest free bit of the + 0x34 mask between
     * + 0x2E and + 0x30, stored raw at + 0x17 (-1 -> 0xFF, which
     * makes the per-frame sub_0803E058 skip its > 0x1F slot) and
     * merged into + 0x58 bits 0-4 ((old & 0xE0) | (slot & 0x1F)). */
    o->affine_slot = (uint8_t)in->affine_slot;
    o->flags58 = (uint8_t)((o->flags58 & 0xE0) | (o->affine_slot & 0x1F));

    /* + 0x42 = 0x78, + 0x46 = 0x50 then the sub_0806AF98 floor
     * snap; `ground_y` carries the post-snap value. */
    o->pos_x = 0x78;
    o->pos_y = in->ground_y;
    /* +0x40/+0x44 16.16 position: EntityCreateInRange seeds it from
     * the spawn record; approximated as the screen anchor << 16. */
    o->pos40 = (int32_t)o->pos_x << 16;
    o->pos44 = (int32_t)o->pos_y << 16;
    o->anim = 0x11;                              /* + 0x5C */

    /* Entity + 0x30..+ 0x33 seed from the two s16 record params;
     * the native reuses them as child->parent sync bytes. */
    o->sync30 = (uint8_t)in->param0;
    o->sync31 = (uint8_t)(in->param0 >> 8);
    o->sync32 = (uint8_t)in->param1;
    o->sync33 = (uint8_t)(in->param1 >> 8);

    /* EntityCreateInRange(0x1B, 0x48, EntityUpdateNothing): the
     * marker child. On success + 0x35 = child + 0x10, child + 0x11
     * = parent + 0x10, child + 0x58 |= 0x20, child + 0x5C = 3,
     * child update = sub_080544E8, frames 0x08257964. */
    if (in->marker_spawned) {
        o->frame_rec = in->frame_record;
        out->marker_child = true;
    }

    /* sub_08054E38: two type-0x1B sparkle children at most
     * (sub_08055070 update, + 0x5C = 0x13/0x14, + 0xD = 4). */
    out->sparkle_children = in->sparkle_spawned > 2 ? 2 : in->sparkle_spawned;

    /* Instantiate the children the native spawns unconditionally:
     * the type-0x1B marker (draw sub_080544E8) and the live
     * sub_08054E38 records {word0 0, +0x1A+2, 0x13} then {word0 4,
     * +0x1A+1, 0x14} — table entries 1/2 are dead natively (the
     * r8-1 <= 1 skip). Spawn-time engine effects (sub_0803B924,
     * sub_0803F2C8) are not reported through CreateOut. */
    {
        static const struct {
            uint16_t frame;
            int16_t rec_off;
            uint8_t anim5c;
        } sparkle_recs[2] = {
            {0, 2, 0x13}, {4, 1, 0x14},
        };
        AosObject1CChildEnv env = {
            .palette = o->palette,
            .anim_1a = o->anim_1a,
            .pos40 = o->pos40,
            .pos44 = o->pos44,
            .affine_slot = o->affine_slot,
            .owner = in->entity_index,
        };
        unsigned n = in->sparkle_spawned > 2 ? 2
                                             : (unsigned)in->sparkle_spawned;
        if (in->marker_spawned)
            aos_object1c_marker_spawn(o, &env, NULL);
        for (unsigned i = 0; i < n; ++i)
            aos_object1c_sparkle_spawn(o, &env, sparkle_recs[i].frame,
                                       (int16_t)(o->anim_1a +
                                                 sparkle_recs[i].rec_off),
                                       sparkle_recs[i].anim5c, NULL);
    }
    return true;
}

void aos_object1c_update(AosObject1C *o, const AosObject1CIn *in,
                          AosObject1COut *out) {
    *out = (AosObject1COut){0};
    out->song = -1;
    out->forced_keys = -1;
    out->text_mode = -1;

    /* Jump table _08054778 on + 0xA; values above 0x63 and the dead
     * entries fall through to the shared tail. */
    if (o->state <= 0x63) {
        switch (o->state) {
        case 0:
            if (o->prompt == 0) {
                if ((in->buttons_16 & 0x40) && player_in_rect(in)) {
                    /* sub_08068AD4 probe passed: + 0xB = old + 1,
                     * + 0xD = old + 0xB (0). */
                    o->timer = o->prompt;
                    o->prompt = (uint8_t)(o->prompt + 1);
                } else {
                    out->flags42c_clear |= 0x2;
                }
            } else {
                /* _0805495C: the sub_08021304 busy gate. Idle opens
                 * the save screen; busy cancels at + 0xD == 3. */
                if (!in->player_busy) {
                    out->song = 0xF3;
                    out->fade_a = true;         /* sub_0803D270 */
                    out->fade_b = true;         /* sub_0803CED4 */
                    out->room_sync = true;      /* sub_080235C8 */
                    out->flags42c_set |= 0x1;
                    out->save_setup = true;     /* sub_08054280 */
                    out->aux_post = true;       /* sub_080213BC */
                    out->save_mode_clear |= 0x3;/* + 0xA074 &= ~3 */
                    o->save_sel = 1;            /* + 0x16 = 1 */
                    o->prompt = 0;              /* + 0xB = 0 */
                    o->state = (uint8_t)(o->state + 1);
                } else if (o->timer == 3) {
                    o->prompt = 0;
                }
            }
            break;

        case 1:
            /* _080549C2: frame-record palette swap
             * sub_0803C7B4(0x0820C384, 4, 1, 0xE) and + 0x58 bits
             * 0/5 cleared on the + 0x35 record. */
            obj_pal_push(out, AOS_O1C_DESC_MAIN, 4, 1, 0xE);
            o->frame_flags = (uint8_t)(o->frame_flags & ~0x21u);
            o->state = (uint8_t)(o->state + 1);
            break;

        case 2:
            if (in->buttons_16 & 0x3) {
                if ((in->buttons_14 & 0x1) && o->save_sel != 0) {
                    /* Commit: + 0x60 save-block refresh then
                     * SaveData_SaveSlotToSram(+ 0x428 slot). */
                    out->save_block_refresh = true;
                    out->save_write = true;
                    if (in->save_result == 0) {
                        out->save_aux = true;   /* sub_0800EF58 */
                        o->timer = 0;
                        o->state = 0x14;        /* direct store */
                        o->flags24 = (uint16_t)(o->flags24 | 0x2);
                        out->song = 0xF3;
                        obj_pal_push(out, AOS_O1C_DESC_SAVE, 5, 1, 0xE);
                        o->frame_flags |= 0x20;
                        ++out->text_resets;     /* sub_08040FE0 */
                        out->save_committed = true;
                    } else {
                        out->song = 0xEF;
                        obj_pal_push(out, AOS_O1C_DESC_SAVE, 5, 1, 0xE);
                        o->frame_flags |= 0x20;
                        ++out->text_resets;
                        pal_push(out, AOS_O1C_DESC_FLASH, 0, 2, 0xD);
                        out->text_mode = 2;     /* sub_08041304(2) */
                        out->save_mode_set |= 0x2;/* _08054B44 */
                        o->timer = 0;
                        o->state = 0;
                        out->save_rejected = true;
                    }
                } else {
                    out->song = 0xF0;
                    o->state = (uint8_t)(o->state + 1); /* -> 3 */
                }
            } else {
                /* Cursor: + 0x14 bits 0x10/0x20 with song 0xF1 on
                 * an actual change only. */
                if (in->buttons_14 & 0x10) {
                    if (o->save_sel != 0) {
                        out->song = 0xF1;
                        o->save_sel = 0;
                    }
                } else if (in->buttons_14 & 0x20) {
                    if (o->save_sel == 0) {
                        out->song = 0xF1;
                        o->save_sel = 1;
                    }
                }
            }
            break;

        case 3:
            /* _08054B16: restore the save palette, reset the text
             * window, then the _08054B44 exit tail. */
            obj_pal_push(out, AOS_O1C_DESC_SAVE, 5, 1, 0xE);
            o->frame_flags |= 0x20;
            ++out->text_resets;
            out->save_mode_set |= 0x2;
            o->timer = 0;
            o->state = 0;
            break;

        case 10:
        case 20:
            /* _08054C24: gated on + 0x31 == 1 (set by the sparkle
             * children). Spawns the sub_08054F74 beam children,
             * copies 0x08119C10 rows 0xC-0xD to row 2, advances and
             * plays 0xF6. */
            if (o->sync31 == 1) {
                out->beam_children = 8; /* requests; slots may fail */
                /* Instantiate the eight sub_08054F74 beams (index
                 * byte 0-7); spawn effects -> out->children. */
                AosObject1CChildEnv env = {
                    .palette = o->palette,
                    .anim_1a = o->anim_1a,
                    .pos40 = o->pos40,
                    .pos44 = o->pos44,
                    .affine_slot = o->affine_slot,
                    .owner = -1,
                };
                for (uint8_t i = 0; i < 8; ++i)
                    aos_object1c_beam_spawn(o, &env, i, &out->children);
                pal_push(out, AOS_O1C_DESC_PULSE, 0xC, 2, 2);
                o->state = (uint8_t)(o->state + 1);
                out->song = 0xF6;
            }
            break;

        case 11:
            /* _08054B68: + 0x1C ramp by the sync bytes; + 0x32 != 0
             * resets the display pair and advances to 12. */
            if (o->sync32 != 0) {
                disp_push(out, 0x50, 0x3FBF);
                disp_push(out, 0x54, 0);
                o->timer = 0;
                o->state = (uint8_t)(o->state + 1);
            } else if (o->sync33 != 0) {
                o->angle -= 0xCC;
                if (o->angle <= 0)
                    o->angle = 0;
            } else {
                o->angle += 0x800;
                if (o->angle > 0xFFFF)
                    o->angle = 0x10000;
            }
            break;

        case 12:
            /* _08054BC4: reg 0x54 sweep; past 0x3F frames mark + 0x58
             * bit 0x20 and advance. */
            disp_push(out, 0x54, o->timer >> 2);
            if (o->timer > 0x3F) {
                o->flags58 |= 0x20;
                o->timer = 0;
                o->state = (uint8_t)(o->state + 1);
            }
            break;

        case 13:
            /* _08054BF0: reg 0x54 fade; past 0xF frames clear + 0x42C
             * bit 1 and enter the dormant state 99. */
            disp_push(out, 0x54, 0x10 - o->timer);
            if (o->timer > 0xF) {
                out->flags42c_clear |= 0x2;
                o->state = 0x63;
            }
            break;

        case 21: {
            /* _08054C54: same + 0x1C ramp as state 11 plus the + 0xB
             * substate machine (_08054CB4) driving the display
             * writes; substate 4 completes at + 0x32 == 1. */
            if (o->sync32 == 0) {
                if (o->sync33 == 0) {
                    o->angle += 0x800;
                    if (o->angle > 0xFFFF)
                        o->angle = 0x10000;
                    if (o->prompt == 0) {
                        o->prompt = 1;
                        o->timer = 0;
                    }
                } else {
                    o->angle -= 0xCC;
                    if (o->angle <= 0)
                        o->angle = 0;
                }
            }
            switch (o->prompt) {
            case 1:
                if (o->timer == 0xE) {
                    disp_push(out, 0x50, 0x3FBF);
                    disp_push(out, 0x54, 0);
                    o->timer = 0;
                    o->prompt = (uint8_t)(o->prompt + 1);
                }
                /* fall through */
            case 2:
                disp_push(out, 0x54, o->timer >> 1);
                if (o->timer > 0x1F) {
                    disp_push(out, 0x54, 0x10);
                    o->prompt = (uint8_t)(o->prompt + 1);
                    o->timer = 0;
                }
                break;
            case 3:
                disp_push(out, 0x54, 0x10 - (o->timer >> 3));
                if (o->timer > 0x3F)
                    o->prompt = (uint8_t)(o->prompt + 1);
                break;
            case 4:
                if (o->sync32 == 1) {
                    out->flags42c_clear |= 0x2;
                    disp_push(out, 0x54, 0);
                    o->timer = 0;
                    o->prompt = 0;
                    o->state = 0x63;    /* _08054D5C */
                }
                break;
            default:
                break;
            }
            break;
        }

        case 99:
            /* _08054D68: dormant re-assert every frame. */
            obj_pal_push(out, AOS_O1C_DESC_SAVE, 5, 1, 0xE);
            o->frame_flags |= 0x20;
            ++out->text_resets;
            o->flags59 |= 0x8;
            out->save_mode_set |= 0x2;
            break;

        default:
            /* Dead entries 4-9, 14-19, 22-98: tail only. */
            break;
        }
    }

    /* Shared tail _08054DB0. */
    if (o->state != 0)
        out->forced_keys = 0x40;    /* SetPlayerInput(1, 0x40) */

    /* sub_0803E058(0, + 0x1C, 0, {0, + 0x17}): the affine-slot
     * registration runs every update; the slot and value are the
     * persistent + 0x17/+ 0x1C fields. */

    o->timer = (uint8_t)(o->timer + 1);
    if (o->timer % 3 == 0 && o->sync31 != 1) {
        /* + 0x34 = 1 - old; src row 2 when old was 1, 0xA
         * otherwise. */
        uint8_t old = o->pulse;
        o->pulse = (uint8_t)(1 - old);
        pal_push(out, AOS_O1C_DESC_PULSE, old == 1 ? 2 : 0xA, 2, 2);
    }
}

/* ==================== child entities ====================
 * Ports of the Object1C child routines in the same asm region:
 * sub_080544E8 (marker drawFunc), sub_08055070 (sparkle update),
 * sub_0805514C (beam update) with its sub_080553A0/sub_08055414
 * spark-spawn helpers and the type-0x49 spark's sub_080552C4
 * update / sub_080554BC drawFunc, and sub_08054574 (fade, no caller
 * in the dump). Engine calls are reported through
 * AosObject1CChildrenOut or injected through AosObject1CChildrenIn.
 * ------------------------------------------------------------------ */

/* int32 muls wrap mod 2^32 like the native. */
static int32_t child_mul32(int32_t a, int32_t b) {
    return (int32_t)((uint32_t)a * (uint32_t)b);
}

/* Adjust the high word of a 16.16 position the way the native
 * ldrh/strh on +0x42/+0x46 does (mod 0x10000, low word kept). */
static int32_t child_adj_hi16(int32_t pos, int delta) {
    uint32_t hi = ((uint32_t)pos >> 16) & 0xFFFFu;
    hi = (hi + (uint32_t)delta) & 0xFFFFu;
    return (pos & 0xFFFF) | ((int32_t)hi << 16);
}

static int child_alloc(AosObject1C *o) {
    /* Reuse the first inactive slot, else append while below the
     * bound (EntityCreateInRange slot exhaustion -> -1). */
    for (unsigned i = 0; i < o->child_count; ++i)
        if (!o->children[i].active)
            return (int)i;
    if (o->child_count >= AOS_O1C_MAX_CHILDREN)
        return -1;
    return (int)o->child_count++;
}

static void fx_frame_set(AosObject1CChild *c, unsigned child_idx,
                         uint8_t frame, AosObject1CChildrenOut *out) {
    /* sub_0803F2C8(child, frame, 0, 1): on success +0x6D = frame,
     * +0x6E = +0x6F = 0, +0x5B = 0, +0x6C = 1 | 8, +0x59 &= ~2.
     * The native fails silently on a missing frame record; the
     * port has no frame table and assumes success (documented).
     * +0x5B/+0x6C/+0x6F are engine-owned and not modeled. */
    c->anim6d = frame;
    c->anim6e = 0;
    c->flags59 = (uint8_t)(c->flags59 & ~0x2u);
    if (out != NULL && out->frame_sets < AOS_O1C_CHILD_MAX_FX) {
        out->frame_set[out->frame_sets].child = (uint8_t)child_idx;
        out->frame_set[out->frame_sets].frame = frame;
        ++out->frame_sets;
    }
}

static void fx_frame_init(AosObject1CChildrenOut *out) {
    /* sub_0803B924(child, 0x08257964, palette, param): frame-record
     * init — reported only. */
    if (out != NULL)
        ++out->frame_inits;
}

static void commit_push(AosObject1CChildrenOut *out, unsigned child_idx,
                        int32_t x, int32_t y, uint8_t frame) {
    /* sub_0803AC40 sprite-position commit; its visibility result
     * (+0x58 bit 5 early-out, +0xE/+0xF counter, bounds check) is
     * not modeled. */
    if (out->commits >= AOS_O1C_CHILD_MAX_COMMITS)
        return;
    AosObject1CCommit *cm = &out->commit[out->commits++];
    cm->child = (uint8_t)child_idx;
    cm->x = (int16_t)x;
    cm->y = (int16_t)y;
    cm->frame = frame;
}

static void obj_row_push(AosObject1CChildrenOut *out, uint32_t desc,
                         unsigned src, unsigned n, unsigned dst) {
    if (out->obj_palettes >= AOS_O1C_MAX_ROWS)
        return;
    AosObject1CRow *r = &out->obj_rows[out->obj_palettes++];
    r->desc = desc;
    r->src = (uint8_t)src;
    r->count = (uint8_t)n;
    r->dst = (uint8_t)dst;
}

/* ---- spawn helpers ---- */

int aos_object1c_marker_spawn(AosObject1C *o, const AosObject1CChildEnv *env,
                              AosObject1CChildrenOut *out) {
    /* Object1CCreate tail: EntityCreateInRange(0x1B, 0x48) then
     * +0x58 |= 0x20, +0x5C = 3, child +0x11 = parent +0x10,
     * parent +0x35 = child +0x10 (the caller's frame_record),
     * update EntityUpdateNothing / draw sub_080544E8,
     * sub_0803B924(., 0x08257964, parent +0x18, 0xE). */
    int idx = child_alloc(o);
    if (idx < 0)
        return -1;
    AosObject1CChild *c = &o->children[idx];
    memset(c, 0, sizeof *c);
    c->kind = AOS_O1C_CHILD_MARKER;
    c->active = 1;
    c->flags58 |= 0x20;
    c->anim5c = 3;
    c->owner = env->owner;
    fx_frame_init(out);
    return idx;
}

int aos_object1c_sparkle_spawn(AosObject1C *o, const AosObject1CChildEnv *env,
                               uint16_t frame, int16_t rec_param,
                               uint8_t anim5c,
                               AosObject1CChildrenOut *out) {
    /* sub_08054E38 per-attempt tail: +0x40/+0x44 = parent +0x40/+0x44,
     * frame 2 -> +0x46 -= 0x9F & +0x42 -= 0x30, frame 7 -> +0x46 -=
     * 0x9F & +0x42 += 0x30 (high-word writes), +0x59 |= 2,
     * +0x4 = sub_0803B9D0, sub_0803B924, sub_0803F2C8(frame, 0, 1),
     * +0x0 = sub_08055070, +0x1C = parent, +0xD = 4, +0x5C = rec. */
    int idx = child_alloc(o);
    if (idx < 0)
        return -1;
    AosObject1CChild *c = &o->children[idx];
    memset(c, 0, sizeof *c);
    c->kind = AOS_O1C_CHILD_SPARKLE;
    c->active = 1;
    c->pos40 = env->pos40;
    c->pos44 = env->pos44;
    if (frame == 2 || frame == 7)
        c->pos44 = child_adj_hi16(c->pos44, -0x9F);
    if (frame == 2)
        c->pos40 = child_adj_hi16(c->pos40, -0x30);
    else if (frame == 7)
        c->pos40 = child_adj_hi16(c->pos40, 0x30);
    c->flags59 |= 2;
    c->owner = -1;
    fx_frame_init(out);
    fx_frame_set(c, (unsigned)idx, (uint8_t)frame, out);
    c->timer = 4;               /* +0xD = 4 */
    c->anim5c = anim5c;
    (void)rec_param;            /* consumed by sub_0803B924 */
    return idx;
}

int aos_object1c_beam_spawn(AosObject1C *o, const AosObject1CChildEnv *env,
                            uint8_t index, AosObject1CChildrenOut *out) {
    /* sub_08054F74 per-iteration tail: +0x40/+0x44 = parent
     * +0x40/+0x44, +0x59 |= 2, +0x4 = sub_0803B9D0, sub_0803B924(.,
     * 0x08257964, parent +0x18, parent +0x1A + 3),
     * sub_0803F2C8(2, 0, 1), +0x5C = 0x11, +0x0 = sub_0805514C,
     * +0x1C = parent, +0x20 = index, +0x24 = 0x80000, +0x28 =
     * +0x2C = 0, +0xA = 0, +0xD = 2, +0x58 = (old & 0xE0) |
     * (parent +0x17 & 0x1F), +0x59 |= 0x10. */
    int idx = child_alloc(o);
    if (idx < 0)
        return -1;
    AosObject1CChild *c = &o->children[idx];
    memset(c, 0, sizeof *c);
    c->kind = AOS_O1C_CHILD_BEAM;
    c->active = 1;
    c->pos40 = env->pos40;
    c->pos44 = env->pos44;
    c->flags59 |= 2;
    c->owner = -1;
    fx_frame_init(out);
    fx_frame_set(c, (unsigned)idx, 2, out);
    c->anim5c = 0x11;
    c->field20 = index;         /* +0x20 low byte = beam index */
    c->field24 = 0x80000;
    c->field28 = 0;
    c->field2c = 0;
    c->state = 0;
    c->timer = 2;
    c->flags58 = (uint8_t)((c->flags58 & 0xE0) | (env->affine_slot & 0x1F));
    c->flags59 |= 0x10;
    return idx;
}

static int spark_spawn_core(AosObject1C *o, unsigned beam_idx,
                            int32_t x, int32_t y,
                            const AosObject1CChildrenIn *in,
                            AosObject1CChildrenOut *out) {
    /* sub_08055414: EntityCreateInRange(0x49, 0xCF, sub_080552C4);
     * slot failure just skips. +0x40/+0x44 = args, +0x50/+0x54 =
     * beam +0x50/+0x54, +0x18 = 0, +0x1C/+0x20 = ((rand & 0x1F) -
     * 0x10) << 16, +0x5A bits 3-6 = nibble arg, +0x34 = beam,
     * +0x5C = beam +0x5C + 1, +0x58 = (old & ~0x20) | (beam +0x58 &
     * 7). */
    if (out != NULL)
        ++out->spark_attempts;
    int idx = child_alloc(o);
    if (idx < 0)
        return -1;
    AosObject1CChild *beam = &o->children[beam_idx];
    AosObject1CChild *c = &o->children[idx];
    uint32_t ra = in->rng ? in->rng(in->rng_ctx) : 0;
    uint32_t rb = in->rng ? in->rng(in->rng_ctx) : 0;
    memset(c, 0, sizeof *c);
    c->kind = AOS_O1C_CHILD_SPARK;
    c->active = 1;
    c->pos40 = x;
    c->pos44 = y;
    c->acc50 = beam->acc50;
    c->acc54 = beam->acc54;
    c->field18 = 0;
    /* Native: ((rand & 0x1F) << 16) + 0xFFF00000. */
    c->field1c = ((int32_t)(ra & 0x1Fu) - 0x10) << 16;
    c->field20 = ((int32_t)(rb & 0x1Fu) - 0x10) << 16;
    c->field5a = (uint8_t)(((beam->field5a >> 3) & 0xF) << 3);
    c->owner = (int)beam_idx;
    c->anim5c = (uint8_t)(beam->anim5c + 1);
    c->flags58 = (uint8_t)((c->flags58 & ~0x20u) | (beam->flags58 & 7));
    if (out != NULL)
        ++out->sparks_spawned;
    return idx;
}

int aos_object1c_spark_spawn(AosObject1C *o, unsigned beam_idx,
                             int32_t x, int32_t y,
                             const AosObject1CChildrenIn *in,
                             AosObject1CChildrenOut *out) {
    return spark_spawn_core(o, beam_idx, x, y, in, out);
}

int aos_object1c_fade_spawn(AosObject1C *o, AosObject1CChildrenOut *out) {
    /* No native spawn site in the dump; allocate a zeroed state-0
     * substate-0 child for sub_08054574. */
    (void)out;
    int idx = child_alloc(o);
    if (idx < 0)
        return -1;
    AosObject1CChild *c = &o->children[idx];
    memset(c, 0, sizeof *c);
    c->kind = AOS_O1C_CHILD_FADE;
    c->active = 1;
    c->owner = -1;
    return idx;
}

void aos_object1c_child_delete(AosObject1C *o, unsigned idx) {
    if (idx < o->child_count)
        o->children[idx].active = 0;
}

/* ---- updateFunc routines ---- */

/* sub_08055070 sparkle update. `parent` is child +0x1C = o. */
static void child_sparkle_update(AosObject1C *o, AosObject1CChild *c,
                                 unsigned idx,
                                 AosObject1CChildrenOut *out) {
    switch (c->state) {
    case 0:
        /* _0805508C: parent +0x24 bit 1 (save committed) starts the
         * animation. */
        if (o->flags24 & 0x2)
            c->state = 1;
        break;
    case 1:
        /* _0805509C: +0x6D 0 -> frame 1, 4 -> frame 5 via
         * sub_0803F2C8, then state 2. */
        if (c->anim6d == 0)
            fx_frame_set(c, idx, 1, out);
        else if (c->anim6d == 4)
            fx_frame_set(c, idx, 5, out);
        c->state = 2;
        break;
    case 2:
        /* _080550CE: +0x6D 2/7 wait on parent +0x30; 1/5 either sync
         * parent +0x31 at +0x6E == 2 or run the +0xD expiry that
         * writes parent +0x30 and marks +0x59 bit 3. */
        if (c->anim6d == 2 || c->anim6d == 7) {
            if (o->sync30 == 1)
                c->flags59 |= 8;
        } else if (c->anim6d == 1 || c->anim6d == 5) {
            if (c->anim6e == 2) {
                o->sync31 = 1;
            } else if (c->flags59 & 1) {
                c->timer = (uint8_t)(c->timer - 1);
                if (c->timer == 0) {
                    c->flags59 |= 8;
                    o->sync30 = 1;
                }
            }
        }
        break;
    default:
        break;
    }
    /* Tail _08055140: sub_0803F17C animation advance. */
    ++out->anim_advances;
}

/* sub_080553A0 + sub_08055414 invoked from the beam tail with
 * params (0x10, 0x10): spark offset = ((rand & 0xF) - 8) << 16. */
static void beam_spark_attempt(AosObject1C *o, unsigned beam_idx,
                               const AosObject1CChildrenIn *in,
                               AosObject1CChildrenOut *out) {
    AosObject1CChild *b = &o->children[beam_idx];
    uint32_t r1 = in->rng ? in->rng(in->rng_ctx) : 0;
    uint32_t r2 = in->rng ? in->rng(in->rng_ctx) : 0;
    /* Native: offset = (rand & (param - 1)) - (param >> 1), << 16;
     * param is 0x10 for both axes -> (rand & 0xF) - 8. */
    int32_t x = b->pos40 + (int32_t)(((int32_t)(r1 & 0xFu) - 8) << 16);
    int32_t y = b->pos44 + (int32_t)(((int32_t)(r2 & 0xFu) - 8) << 16);
    spark_spawn_core(o, beam_idx, x, y, in, out);
}

/* sub_0805514C beam update. `parent` is child +0x1C = o. */
static void child_beam_update(AosObject1C *o, unsigned idx,
                              AosObject1CChild *c,
                              const AosObject1CChildrenIn *in,
                              AosObject1CChildrenOut *out) {
    uint32_t bi = c->field20 & 0xFFu;   /* +0x20 low byte = index */
    if (c->state == 0) {
        /* _08055164: orbit position from sub_080009E4(bi << 13) and
         * the +0x4000 quadrature, scaled by s16(+0x24) >> 16; then
         * the +0x24 angle/lifetime ramp driven by +0x28. */
        int32_t s1 = in->trig ? in->trig(bi << 13, in->trig_ctx) : 0;
        int32_t s2 = in->trig ? in->trig((bi << 13) + 0x4000u,
                                       in->trig_ctx) : 0;
        int32_t hi = c->field24 >> 16;  /* asrs */
        c->pos44 = child_mul32(s1, hi) + 0x500000;
        c->pos40 = child_mul32(s2, hi) + 0x760000;
        uint32_t hi28 = (uint32_t)c->field28 >> 16;   /* lsrs */
        int32_t ramp = (int32_t)(hi28 << 13);
        if ((uint32_t)c->field28 > 0x1DFFFFu)
            ramp = (int32_t)((hi28 * 3u) << 13);
        c->field24 += ramp;
        c->field28 += 0x10000;
        c->field2c += 0x800;
        if (c->field24 > 0xFFFFFF) {
            /* -> state 1: +0xD = 2, +0x5C = 9, +0x24 = 0x1000000,
             * parent +0x33 = 1, +0x28 = 0. */
            c->state = 1;
            c->timer = 2;
            c->anim5c = 9;
            c->field24 = 0x1000000;
            o->sync33 = 1;
            c->field28 = 0;
        }
    } else if (c->state == 1) {
        /* _08055200: same trig pair advanced by +0x28, scaled by
         * s16 +0x26, shifted by the sub_0806CDB8/sub_0806CDFC
         * focus-entity delta vs parent +0x40/+0x44; +0x24 counts
         * down and parent +0x32 = 1 on expiry. */
        int32_t s1 = in->trig ? in->trig((bi << 13) +
                                       (uint32_t)c->field28,
                                       in->trig_ctx) : 0;
        int32_t s2 = in->trig ? in->trig((bi << 13) + 0x4000u +
                                       (uint32_t)c->field28,
                                       in->trig_ctx) : 0;
        int32_t scale = c->field26;
        c->pos44 = child_mul32(s1, scale) + 0x500000;
        c->pos40 = child_mul32(s2, scale) + 0x780000;
        c->pos40 += ((int32_t)in->focus_x << 16) - o->pos40;
        c->pos44 += ((int32_t)in->focus_y << 16) - o->pos44 + 0x200000;
        c->field24 -= 0x30000;
        c->field28 += 0x200;
        if (c->field24 <= 0) {
            o->sync32 = (uint8_t)c->state;  /* = 1 */
            c->flags59 |= 8;
        }
    }
    /* Shared tail _08055294: +0xD++, sub_080553A0 spark spawn every
     * 4th tick, sub_0803F17C. */
    c->timer = (uint8_t)(c->timer + 1);
    if ((c->timer & 3) == 0)
        beam_spark_attempt(o, idx, in, out);
    ++out->anim_advances;
}

/* sub_080552C4 type-0x49 spark update; state 0 swaps the drawFunc
 * to sub_080554BC (implicit: kind SPARK draws it once state >= 1). */
static void child_spark_update(AosObject1C *o, AosObject1CChild *c,
                               unsigned idx,
                               AosObject1CChildrenOut *out) {
    switch (c->state) {
    case 0:
        /* _080552DE: sub_0803AFB8(0x082052E8) — no bank-0 fallback —
         * then sub_0803B924(child, 0x08257964, result,
         * (+0x5A >> 3) & 0xF), drawFunc = sub_080554BC,
         * sub_0803F2C8(3, 0, 1), +0xD = 0xC, +0x59 |= 2,
         * +0x4C = 0xFFFF0000, +0xA = 1. */
        ++out->pal_resolves;
        out->pal_index = o->palette;
        ++out->frame_inits;
        fx_frame_set(c, idx, 3, out);
        c->timer = 0xC;
        c->flags59 |= 2;
        c->vel4c = (int32_t)0xFFFF0000;
        c->state = 1;
        break;
    case 1:
        /* _08055334: integrate +0x40/+0x44 by +0x48/+0x4C, accel
         * +0x48/+0x4C by +0x50/+0x54; u8 +0xD countdown to state 2. */
        c->pos40 += c->vel48;
        c->pos44 += c->vel4c;
        c->vel48 += c->acc50;
        c->vel4c += c->acc54;
        c->timer = (uint8_t)(c->timer - 1);
        if (c->timer == 0) {
            c->flags59 |= 8;
            c->state = 2;
            c->timer = 0;
        }
        break;
    case 2:
        /* _08055372: +0x44 -= +0x4C; u8 +0xD past 8 -> delete mark. */
        c->pos44 -= c->vel4c;
        c->timer = (uint8_t)(c->timer + 1);
        if (c->timer > 8)
            c->flags59 |= 8;
        break;
    default:
        break;
    }
    /* Tail _08055394: sub_0803F17C. */
    ++out->anim_advances;
}

/* sub_08054574 fade routine (no caller in the dump). Returns 1 in
 * substate 2; counted through fade_finished. */
static void child_fade_update(AosObject1CChild *c,
                              AosObject1CChildrenOut *out) {
    int done = 0;
    if (c->state == 0) {
        if (c->substate == 0) {
            /* _0805459C: +0x58 &= ~0x21, +0x14 = 0,
             * sub_0803C7B4(0x0820C384, 5, 1, 0xE), +0xB = 1. */
            c->flags58 &= (uint8_t)~0x21u;
            c->field14 = 0;
            obj_row_push(out, 0x0820C384, 5, 1, 0xE);
            c->substate = 1;
        } else if (c->substate == 1) {
            /* _080545C0: +0x14 += 0x2000; past 0xFFFF clamp to
             * 0x10000 and +0xB = 2. */
            c->field14 += 0x2000;
            if (c->field14 > 0xFFFF) {
                c->field14 = 0x10000;
                c->substate = 2;
            }
        } else if (c->substate == 2) {
            done = 1;               /* _08054640 */
        }
    } else if (c->state == 1) {
        if (c->substate == 0) {
            /* _080545EE: +0x58 &= ~0x21, +0x14 = 0x10000, +0xB = 1. */
            c->flags58 &= (uint8_t)~0x21u;
            c->field14 = 0x10000;
            c->substate = 1;
        } else if (c->substate == 1) {
            /* _0805460A: +0x14 -= 0x2000; at/below 0 -> +0x14 = 0,
             * +0xB = 2, sub_0803C7B4(0x082099FC, 0xE, 1, 0xE). */
            c->field14 -= 0x2000;
            if (c->field14 <= 0) {
                c->field14 = 0;
                c->substate = 2;
                obj_row_push(out, 0x082099FC, 0xE, 1, 0xE);
            }
        } else if (c->substate == 2) {
            /* _08054634: +0x58 |= 0x20 then done. */
            c->flags58 |= 0x20;
            done = 1;
        }
    }
    if (done)
        ++out->fade_finished;
}

void aos_object1c_children_update(AosObject1C *o,
                                  const AosObject1CChildrenIn *in,
                                  AosObject1CChildrenOut *out) {
    *out = (AosObject1CChildrenOut){0};
    out->pal_index = -1;
    for (unsigned i = 0; i < o->child_count; ++i) {
        AosObject1CChild *c = &o->children[i];
        if (!c->active)
            continue;
        switch (c->kind) {
        case AOS_O1C_CHILD_MARKER:
            break;              /* updateFunc = EntityUpdateNothing */
        case AOS_O1C_CHILD_SPARKLE:
            child_sparkle_update(o, c, i, out);
            break;
        case AOS_O1C_CHILD_BEAM:
            child_beam_update(o, i, c, in, out);
            break;
        case AOS_O1C_CHILD_SPARK:
            child_spark_update(o, c, i, out);
            break;
        case AOS_O1C_CHILD_FADE:
            child_fade_update(c, out);
            break;
        default:
            break;
        }
    }
}

/* ---- drawFunc routines ---- */

void aos_object1c_children_draw(AosObject1C *o,
                                AosObject1CChildrenOut *out) {
    *out = (AosObject1CChildrenOut){0};
    out->pal_index = -1;
    for (unsigned i = 0; i < o->child_count; ++i) {
        AosObject1CChild *c = &o->children[i];
        if (!c->active)
            continue;
        switch (c->kind) {
        case AOS_O1C_CHILD_MARKER:
            /* sub_080544E8: three sub_0803AC40 commits per frame;
             * +0x65 picks the YES/NO variant from parent +0x16
             * (o->save_sel). */
            {
                uint8_t sel = o->save_sel;
                commit_push(out, i, 0x48, 0x26, 0x15);
                commit_push(out, i, 0x39, 0x3E, sel ? 0x16 : 0x17);
                commit_push(out, i, 0x99, 0x3E, sel ? 0x19 : 0x18);
                c->pos42 = 0x99;
                c->pos46 = 0x3E;
                c->anim65 = sel ? 0x19 : 0x18;
            }
            break;
        case AOS_O1C_CHILD_SPARKLE:
        case AOS_O1C_CHILD_BEAM:
            /* sub_0803B9D0 drawFunc = sub_0803AC40 at the current
             * position (+0x42/+0x46 = +0x40/+0x44 high words). */
            commit_push(out, i, c->pos40 >> 16, c->pos44 >> 16,
                        c->anim65);
            break;
        case AOS_O1C_CHILD_SPARK:
            if (c->state >= 1) {
                /* sub_080554BC: commit, offset by +0x1C/+0x20,
                 * commit, restore. */
                commit_push(out, i, c->pos40 >> 16, c->pos44 >> 16,
                            c->anim65);
                c->pos40 += c->field1c;
                c->pos44 += c->field20;
                commit_push(out, i, c->pos40 >> 16, c->pos44 >> 16,
                            c->anim65);
                c->pos40 -= c->field1c;
                c->pos44 -= c->field20;
            }
            break;
        default:
            break;
        }
    }
}
