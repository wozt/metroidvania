/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_object06.h"

/* sub_08068AD4: (u16)(ex - ax) <= w && (u16)(ey - ay) <= h on screen
 * pixels; the caller passes room pixels minus the camera. */
static bool probe(int32_t ex, int32_t ey, int32_t ax, int32_t ay,
                  uint16_t w, uint16_t h) {
    return (uint16_t)(ex - ax) <= w && (uint16_t)(ey - ay) <= h;
}

/* sub_0802D5EC(n): (rng & (n - 1)) - (n >> 1) — a centred jitter. */
static int32_t jitter(uint32_t (*rng)(void), uint32_t n) {
    return (int32_t)((*rng)() & (n - 1)) - (int32_t)(n >> 1);
}

/* The three-block column sub_08002200/sub_08002248 write at the
 * entity anchor plus the gEwramData + 0xA094 camera origin, at
 * y - 8, y - 0x18 and y - 0x28 (room pixels here). */
static void blocks(AosCollision *layer, int32_t ex, int32_t ey,
                   bool solid) {
    for (int dy = 8; dy <= 0x28; dy += 0x10)
        aos_collision_set_block(layer, ex, ey - dy, solid);
}

static void reset_outputs(AosObject06 *o) {
    o->song = -1;
    o->forced = 0;
    o->force_zero = false;
    o->triggered = false;
    o->fx_drift = o->fx_burst = false;
    o->fx_x = o->fx_y = 0;
}

bool aos_object06_create(AosObject06 *o, AosCollision *layer,
                         int param0, int param1, uint16_t special_37e,
                         int32_t screen_x, int32_t screen_y,
                         int32_t soma_screen_x, int32_t soma_screen_y,
                         int32_t ex, int32_t ey) {
    (void)screen_y;
    reset_outputs(o);
    *o = (AosObject06){
        .state = 0, .timer = 0, .counter = 0,
        .param0 = (uint8_t)param0, .param1 = (uint8_t)param1,
        /* + 0x1A and + 0x58 bit 0x40 both come from screen x > 0x78. */
        .facing_right = screen_x > 0x78,
        .flags58 = screen_x > 0x78 ? 0x40 : 0,
        .deleted = false, .scene_active = true, .song = -1,
    };
    /* sub_0804DF3C: + 0x1B = 6, + 0x5C = 0x16, + 0x58 &= 0xBF (bit 6
     * then rewritten above), + 0x65 = 0x3F and the sprite attach —
     * approximated by the exported special_objects art. */
    bool near = probe(screen_x, screen_y, soma_screen_x - 0x1C,
                      soma_screen_y, 0x38, 0x22) ||
                ((special_37e & 4) != 0);
    if (near) {
        /* + 0x14 = 0x30, state 0xA, SetPlayerInput(1, 0). */
        o->timer = 0x30;
        o->state = 0xA;
        o->force_zero = true;
    } else {
        blocks(layer, ex, ey, true);
        o->state = param0 == 1 ? 0x14 : param0 == 3 ? 0x63 : 0;
    }
    return true;
}

void aos_object06_update(AosObject06 *o, AosCollision *layer,
                         int32_t cam_x, int32_t cam_y,
                         int32_t ex, int32_t ey,
                         int32_t soma_screen_x, int32_t soma_screen_y,
                         uint16_t soma_flags10, uint16_t special_37e,
                         bool cinematic, uint32_t (*rng)(void)) {
    (void)cam_x;
    (void)cam_y;
    (void)soma_flags10;
    reset_outputs(o);
    int32_t sx = ex - cam_x, sy = ey - cam_y;
    o->flags58 = (o->flags58 & ~0x20) |
                 (probe(sx, sy, -0x20, 0, 0x130, 0xD0) ? 0 : 0x20);
    /* + 0x58 bit 0x40 picks the forced direction: set -> 0x20, clear
     * -> 0x10 (the same mapping the door facing uses). */
    uint16_t dir = (o->flags58 & 0x40) ? 0x20 : 0x10;
    switch (o->state) {
    case 0: {
        if (special_37e & 1) {
            o->state = 2;
            break;
        }
        /* sub_0803D408()/sub_0803D270()/sub_0803CED4() fade bookkeeping
         * skipped — the + 0x12C70/+ 0x123E4 display state is not
         * modelled; the fades fire only on the trigger below. */
        o->counter++;
        /* Entity fixed-point position +0x40/+0x44 is approximated by
         * the room anchor (16.16). */
        /* Drift particle on odd + 0xD frames: x = + 0x40 +
         * jitter(0x80000), y = + 0x44 + (rng % 0x30 - 0x30) << 16. */
        int64_t dx = ((int64_t)ex << 16) + jitter(rng, 0x80000);
        int64_t dy = ((int64_t)ey << 16) +
                     (((int64_t)((int32_t)(*rng)() % 0x30) - 0x30) << 16);
        if (o->counter & 1) {
            /* sub_08045CEC(x, y, 0x10, 5). */
            o->fx_drift = true;
            o->fx_x = (int32_t)(dx >> 16);
            o->fx_y = (int32_t)(dy >> 16);
        }
        /* Burst every frame: sub_08045B44(x + jitter(0x40000) + 0x400,
         * y + (rng % 0x30 - 0x30) << 16, jitter(0x400) + 0x1000,
         * jitter(0x400), 5, 7). */
        int64_t bx = ((int64_t)ex << 16) + jitter(rng, 0x40000) + 0x400;
        int64_t by = ((int64_t)ey << 16) +
                     (((int64_t)((int32_t)(*rng)() % 0x30) - 0x30) << 16);
        o->fx_burst = true;
        o->fx_x = (int32_t)(bx >> 16);
        o->fx_y = (int32_t)(by >> 16);
        if (soma_flags10 & 0x80) break;
        if (!probe(sx, sy, soma_screen_x - 0x1C, soma_screen_y,
                   0x38, 0x22))
            break;
        /* The trigger: PlaySong(0x111), the two fades, the
         * + 0x131D4..+ 0x131DC snapshot writes and sub_08021248 —
         * only `triggered` is surfaced; the global writes are not
         * modelled. */
        o->song = 0x111;
        o->triggered = true;
        break;
    }
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
        if (o->timer == 0) blocks(layer, ex, ey, false);
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
