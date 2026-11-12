/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_object02.h"

#include "../core/aos_soma.h"

/* sub_08068AD4: (u16)(ex - ax) <= w && (u16)(ey - ay) <= h on screen
 * pixels; the caller passes room pixels minus the camera. */
static bool probe(int32_t ex, int32_t ey, int32_t ax, int32_t ay,
                  uint16_t w, uint16_t h) {
    return (uint16_t)(ex - ax) <= w && (uint16_t)(ey - ay) <= h;
}

/* The three-block column sub_08002200/sub_08002248 write at the
 * entity anchor: room pixels (the screen position plus the + 0xA094
 * camera origin), at y - 8, y - 0x18 and y - 0x28. */
static void blocks(AosObject02 *o, AosCollision *layer, int32_t ex, int32_t ey,
                   bool solid) {
    (void)o;
    for (int dy = 8; dy <= 0x28; dy += 0x10)
        aos_collision_set_block(layer, ex, ey - dy, solid);
}

static void reset_outputs(AosObject02 *o) {
    o->song = -1;
    o->forced = 0;
    o->force_zero = false;
    o->zero_soma_vx = false;
    o->sparkle = false;
    o->sparkle_x = o->sparkle_y = 0;
}

bool aos_object02_create(AosObject02 *o, int param0, int param1,
                         uint16_t special_37e, bool near,
                         int32_t screen_x, int32_t soma_screen_x,
                         int32_t soma_vx) {
    reset_outputs(o);
    *o = (AosObject02){
        .state = 0, .timer = 0, .counter = 0,
        .param0 = (uint8_t)param0, .param1 = (uint8_t)param1,
        /* + 0x1A: the entity screen x > 0x78 picks the walk side. */
        .facing_right = screen_x > 0x78, .anim = 6, .flags58 = 0,
        .deleted = false, .holding = false, .scene_active = true,
        .song = -1,
    };
    /* param0 <= 1 objects are one-shot: the + 0x37E event bit deletes
     * them when the scene already ran. */
    if (param0 <= 1 && ((special_37e >> param1) & 1)) {
        o->deleted = true;
        o->scene_active = false;
        return false;
    }
    /* sub_0804DF3C: + 0x1B = 6, + 0x5C = 0x16, + 0x65 = 0x3F,
     * gUnk_03002CB0 |= 0x8000 — the art row and palette bookkeeping
     * live in the exported special_objects art instead. */
    if (param1 == 7) {
        o->anim = 9;
        /* + 0x5A = (+ 0x5A & ~0x78) | 0x48 — a draw-time variant mask,
         * approximated by the anim index. */
    }
    if (near) {
        /* Soma is inside the trigger zone: the scene starts locked —
         * + 0x14 = 0x30, state 0xA, SetPlayerInput(1, 0), then Soma's
         * + 0x48/+ 0x50 zero when he walks away from the screen centre
         * (x <= 0x77 moving left, x > 0x77 moving right). */
        o->timer = 0x30;
        o->state = 0xA;
        o->force_zero = true;
        o->zero_soma_vx = (soma_screen_x <= 0x77) ? (soma_vx < 0)
                                                : (soma_vx > 0);
    } else {
        /* Outside: seal the column of three blocks and pick the entry
         * state — param0 1 -> 0x14, param0 3 -> 0x63, else state 0. */
        o->timer = 0;
        o->state = param0 == 1 ? 0x14 : param0 == 3 ? 0x63 : 0;
    }
    return true;
}

void aos_object02_update(AosObject02 *o, AosCollision *layer,
                         int32_t cam_x, int32_t cam_y,
                         int32_t ex, int32_t ey,
                         int32_t soma_screen_x, int32_t soma_screen_y,
                         uint16_t special_37e, uint32_t (*rng)(void)) {
    (void)cam_x;
    (void)cam_y;
    reset_outputs(o);
    int32_t sx = ex - cam_x, sy = ey - cam_y;
    /* Every update: + 0x58 bit 0x20 tracks the screen window
     * (u16)(ex + 0x20) <= 0x130 && (u16)ey <= 0xD0. */
    o->flags58 = (o->flags58 & ~0x20) |
                 (probe(sx, sy, -0x20, 0, 0x130, 0xD0) ? 0 : 0x20);
    switch (o->state) {
    case 0: {
        /* The sparkle variant (param0 == 7) every fourth + 0xD tick. */
        if (o->param0 == 7 && o->timer == 0) {
            o->counter = (uint8_t)((o->counter + 1) & 3);
            if (o->counter == 0) {
                /* sub_0802D5EC(0x80000) on x and (rng % 0x30) - 0x30 on
                 * y, then sub_08045CEC(x, y, 0x10, 9): approximated by
                 * one room-pixel drift request. */
                o->sparkle = true;
                o->sparkle_x = ex + (int32_t)(rng() % 0x30) - 0x18;
                o->sparkle_y = ey + (int32_t)(rng() % 0x30) - 0x30;
            }
        }
        /* Horizontal proximity: (u16)(ex - soma_x + 0x28) <= 0x4F. */
        if ((uint16_t)(sx - soma_screen_x + 0x28) <= 0x4F) {
            blocks(o, layer, ex, ey, false);
            if (o->timer == 0 &&
                probe(sx, sy, -0x10, 0, 0x110, 0xC0))
                o->song = 0x113;
            o->timer += 2;
            if (o->timer > 0x2F) o->timer = 0x30;
        } else if (o->timer <= 2) {
            /* The release edge: timer 1-2 re-seals the column; timer 0
             * leaves it open. */
            if (o->timer != 0) blocks(o, layer, ex, ey, true);
            o->timer = 0;
        } else if (o->timer == 0x30) {
            if (probe(sx, sy, -0x10, 0, 0x110, 0xC0)) o->song = 0x113;
            o->timer -= 2;
        } else {
            o->timer -= 2;
        }
        break;
    }
    case 2:
        /* Walk Soma toward the object: screen x > 0x78 forces LEFT
         * (0x20), else RIGHT (0x10). */
        o->forced = o->facing_right ? 0x20 : 0x10;
        o->holding = true;
        break;
    case 10:
        o->forced = o->facing_right ? 0x20 : 0x10;
        o->holding = true;
        /* Wait until Soma leaves the 0x38 x 0x22 zone around
         * (soma x - 0x1C, soma y), then reseal and advance with the
         * song. Near keeps state 10. */
        if (!probe(sx, sy, soma_screen_x - 0x1C, soma_screen_y, 0x38, 0x22)) {
            blocks(o, layer, ex, ey, true);
            o->song = 0x113;
            o->state++;
        }
        break;
    case 11:
        o->force_zero = true;
        o->holding = true;
        if (--o->timer == 0) {
            /* + 0x42C &= ~0x101 and gUnk_03002CB0 &= 0x7FFF: the scene
             * releases the player. */
            o->holding = false;
            o->scene_active = false;
            o->state = o->param0 <= 1 ? 0x14 : 0x63;
        }
        break;
    case 20:
        /* Waits for the + 0x37E event bit; the sub_08013F6C(0x1040)
         * accent is a sound effect, not modelled. */
        if (((special_37e >> o->param1) & 1) == 0) break;
        o->song = 0x113;
        o->state++;
        break;
    case 21:
        if (++o->timer > 0x2F) {
            blocks(o, layer, ex, ey, false);
            o->state++;
        }
        break;
    default:
        break;
    }
}
