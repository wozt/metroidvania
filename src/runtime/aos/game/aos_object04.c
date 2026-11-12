/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_object04.h"

#include "../core/aos_soma.h"

/* sub_08068AD4: (u16)(ex - ax) <= w && (u16)(ey - ay) <= h on screen
 * pixels; the caller passes room pixels minus the camera. */
static bool probe(int32_t ex, int32_t ey, int32_t ax, int32_t ay,
                  uint16_t w, uint16_t h) {
    return (uint16_t)(ex - ax) <= w && (uint16_t)(ey - ay) <= h;
}

/* The three-block column sub_08002200/sub_08002248 write at the
 * entity anchor plus the gEwramData + 0xA094 camera origin, at
 * y - 8, y - 0x18 and y - 0x28 (room pixels here). */
static void blocks(AosObject04 *o, AosCollision *layer, int32_t ex, int32_t ey,
                   bool solid) {
    (void)o;
    for (int dy = 8; dy <= 0x28; dy += 0x10)
        aos_collision_set_block(layer, ex, ey - dy, solid);
}

/* The directional approach probe of Object04Create/Object04Update
 * case 10: + 0x1A set wants soma_x >= ex - 0x18, clear wants
 * soma_x <= ex + 0x18 (screen pixels). */
static bool near_side(int facing_right, int32_t ex, int32_t soma_x) {
    return facing_right ? soma_x >= ex - 0x18 : soma_x <= ex + 0x18;
}

static void reset_outputs(AosObject04 *o) {
    o->song = -1;
    o->forced = 0;
    o->force_zero = false;
    o->zero_soma_vx = false;
    o->sparkle = false;
    o->sparkle_x = o->sparkle_y = 0;
}

bool aos_object04_create(AosObject04 *o, int param0, int param1,
                         uint16_t special_37e, int32_t screen_x,
                         int32_t soma_screen_x, int32_t soma_vx) {
    reset_outputs(o);
    *o = (AosObject04){
        .state = 0, .timer = 0, .counter = 0,
        .param0 = (uint8_t)param0, .param1 = (uint8_t)param1,
        /* + 0x1A: the shared sub_0804DF3C init sets it from screen x. */
        .facing_right = screen_x > 0x78, .anim = 6, .flags58 = 0,
        .deleted = false, .holding = false, .scene_active = true,
        .song = -1,
    };
    if (param0 <= 1 && ((special_37e >> param1) & 1)) {
        o->deleted = true;
        o->scene_active = false;
        return false;
    }
    if (near_side(o->facing_right, screen_x, soma_screen_x)) {
        /* Locked-scene start: state 0xA, timer 0x30,
         * SetPlayerInput(1, 0), sub_080235C8 bookkeeping skipped, and
         * Soma's + 0x48/+ 0x50 cleared when he walks away from the
         * screen centre (x <= 0x77 moving left, x > 0x77 moving right). */
        o->timer = 0x30;
        o->state = 0xA;
        o->force_zero = true;
        o->zero_soma_vx = (soma_screen_x <= 0x77) ? (soma_vx < 0)
                                                : (soma_vx > 0);
    } else {
        o->timer = 0;
        o->state = param0 == 1 ? 0x14 : param0 == 3 ? 0x63 : 0;
    }
    return true;
}

void aos_object04_update(AosObject04 *o, AosCollision *layer,
                         int32_t cam_x, int32_t cam_y,
                         int32_t ex, int32_t ey,
                         int32_t soma_screen_x, int32_t soma_screen_y,
                         uint16_t special_37e, uint32_t (*rng)(void)) {
    (void)cam_x;
    (void)cam_y;
    (void)soma_screen_y;
    (void)rng;
    reset_outputs(o);
    int32_t sx = ex - cam_x, sy = ey - cam_y;
    o->flags58 = (o->flags58 & ~0x20) |
                 (probe(sx, sy, -0x20, 0, 0x130, 0xD0) ? 0 : 0x20);
    switch (o->state) {
    case 0: {
        /* Identical block cycle to Object02's state 0. */
        if ((uint16_t)(sx - soma_screen_x + 0x28) <= 0x4F) {
            blocks(o, layer, ex, ey, false);
            if (o->timer == 0 &&
                probe(sx, sy, -0x10, 0, 0x110, 0xC0))
                o->song = 0x113;
            o->timer += 2;
            if (o->timer > 0x2F) o->timer = 0x30;
        } else if (o->timer <= 2) {
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
        /* + 0x1A ? RIGHT (0x10) : LEFT (0x20) — opposite of case 10,
         * and no + 0x42C hold bits. */
        o->forced = o->facing_right ? 0x10 : 0x20;
        break;
    case 10:
        /* + 0x1A ? LEFT : RIGHT, then the directional probe: while
         * Soma stays on the approach side the state holds; leaving it
         * reseals the column, plays 0x113 and advances. */
        o->forced = o->facing_right ? 0x20 : 0x10;
        if (!near_side(o->facing_right, sx, soma_screen_x)) {
            blocks(o, layer, ex, ey, true);
            o->song = 0x113;
            o->state++;
        }
        break;
    case 11:
        o->force_zero = true;
        if (--o->timer == 0) {
            o->scene_active = false;
            o->state = o->param0 <= 1 ? 0x14 : 0x63;
        }
        break;
    case 20:
        if (((special_37e >> o->param1) & 1) == 0) break;
        /* sub_08013F6C(0x1040) accent skipped while + 0x42C bit
         * 0x40000000 is set; the sound itself is not modelled. */
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
