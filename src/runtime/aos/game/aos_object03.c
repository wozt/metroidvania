/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_object03.h"

/* sub_08068AD4: (u16)(ex - ax) <= w && (u16)(ey - ay) <= h on screen
 * pixels; the caller passes room pixels minus the camera. */
static bool probe(int32_t ex, int32_t ey, int32_t ax, int32_t ay,
                  uint16_t w, uint16_t h) {
    return (uint16_t)(ex - ax) <= w && (uint16_t)(ey - ay) <= h;
}

/* The three-block column sub_08002200/sub_08002248 write at the
 * entity anchor plus the gEwramData + 0xA094 camera origin, at
 * y - 8, y - 0x18 and y - 0x28 (room pixels here). */
static void blocks(AosCollision *layer, int32_t ex, int32_t ey,
                   bool solid) {
    for (int dy = 8; dy <= 0x28; dy += 0x10)
        aos_collision_set_block(layer, ex, ey - dy, solid);
}

/* The escort window compares are unsigned 32-bit: a negative delta
 * wraps huge and fails, so the window is effectively
 * 0 <= delta <= bound. */
static bool in_window(int32_t delta_plus, uint32_t bound) {
    return (uint32_t)delta_plus <= bound;
}

static void reset_outputs(AosObject03 *o) {
    o->song = -1;
    o->forced = 0;
    o->force_zero = false;
}

bool aos_object03_create(AosObject03 *o, AosCollision *layer,
                         int param0, int param1,
                         int32_t screen_x, int32_t ex, int32_t ey) {
    (void)param1;
    reset_outputs(o);
    *o = (AosObject03){
        .state = 0, .timer = 0, .counter = 0,
        .param0 = (uint8_t)param0,
        /* + 0x1A and + 0x58 bit 0x40 both come from screen x > 0x78. */
        .facing_right = screen_x > 0x78,
        .flags58 = screen_x > 0x78 ? 0x40 : 0,
        .deleted = false, .scene_active = true, .song = -1,
    };
    /* sub_0804DF3C init + winOut, then the column is ALWAYS sealed —
     * no probe, no one-shot, no SetPlayerInput. */
    blocks(layer, ex, ey, true);
    o->state = param0 == 1 ? 0x14 : param0 == 3 ? 0x63 : 0;
    return true;
}

void aos_object03_update(AosObject03 *o, AosCollision *layer,
                         int32_t cam_x, int32_t cam_y,
                         int32_t ex, int32_t ey,
                         int32_t soma_screen_x, int32_t soma_screen_y,
                         bool escort_busy, bool cinematic,
                         int32_t partner1_sx, int32_t partner2_sx) {
    (void)cam_x;
    (void)cam_y;
    (void)soma_screen_y;
    reset_outputs(o);
    int32_t sx = ex - cam_x;
    o->flags58 = (o->flags58 & ~0x20) |
                 (probe(sx, ey - cam_y, -0x20, 0, 0x130, 0xD0) ? 0 : 0x20);
    /* + 0x58 bit 0x40 picks the forced direction: set -> 0x20,
     * clear -> 0x10. */
    uint16_t dir = (o->flags58 & 0x40) ? 0x20 : 0x10;
    switch (o->state) {
    case 0:
        if (escort_busy) break;         /* + 0x13220 != 0: skip */
        if (!cinematic) {
            o->state = 2;
            break;
        }
        /* Soma window: (u32)(ex_sx - soma_sx + 0x18) <= 0x2F forces
         * the walk; (u32)(ex_sx - soma_sx + 0x13) <= 0x26 must also
         * hold or + 0xD resets and the state waits. */
        if (in_window(sx - soma_screen_x + 0x18, 0x2F))
            o->forced = dir;
        if (!in_window(sx - soma_screen_x + 0x13, 0x26)) {
            o->counter = 0;
            break;
        }
        /* Both partners abreast: partner1 inside its 0x26 window
         * (ex_sx - partner1_sx + 0x13) and partner2's screen x
         * exactly equal to partner1's. */
        if (!in_window(sx - partner1_sx + 0x13, 0x26)) break;
        if (partner2_sx != partner1_sx) break;
        o->counter = 4;
        o->forced = dir;
        o->state = 1;
        break;
    case 1:
        o->forced = dir;
        if (o->counter == 0) {
            o->state = 2;
        } else {
            o->counter--;
        }
        break;
    case 2:
        if (cinematic) o->forced = dir;
        if (o->timer == 0) {
            blocks(layer, ex, ey, false);
            o->song = 0x113;
        }
        if (++o->timer > 0x2F) {
            o->scene_active = false;    /* + 0x42C &= ~0x101 */
            if (cinematic) o->state++;
        }
        break;
    case 3:
        o->forced = dir;
        break;
    default:
        break;
    }
}
