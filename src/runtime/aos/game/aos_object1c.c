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
