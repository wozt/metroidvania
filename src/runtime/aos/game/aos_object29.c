#include "aos_object29.h"

/* Record table 0x085264D0 (stride 16) dumped from the (USA)
 * image: every record repeats every four entries and all
 * carry dwell = 2 at + 0xE; the sprite triples resolve to the
 * exported special_42_v00-v03 variants. The four collision
 * boxes at 0x08118630 are 0x0C2000F0/0x0C2000F0/
 * 0x0C20F8F0/0x0C20F8F0 selected by param1. */
#define AOS_OBJ29_DWELL 2

static int32_t approach(int32_t pos, int32_t target, int dir,
                        int32_t *vel) {
    int32_t dist = target - pos;
    if (dist < 0) dist = -dist;
    if ((dir == 0 && pos >= target) ||
        (dir == 1 && pos <= target))
        return 1;                       /* arrived */
    int32_t px = dist >> 16;
    if (dist > 0xFFFFF) {
        /* Cruise: accelerate by +/-0x800, capped at the ROM's
         * asymmetric limits (+0xFFFF / -0x10000). */
        if (dir == 0) {
            if (*vel <= 0xFFFF) *vel += 0x800;
        } else {
            if (*vel > (int32_t)0xFFFF0000) *vel -= 0x800;
        }
    } else {
        /* Near: velocity is SET to the damped step
         * -(dist_px * 0x1000 + 0x1000) .. +(...). */
        int32_t step = px * 0x1000 + 0x1000;
        *vel = dir == 0 ? step : -step;
    }
    return 0;
}

void aos_object29_create(AosObject29 *o, int16_t param0,
                         int16_t param1, int32_t room_x,
                         int32_t room_y, int32_t ent_x,
                         int32_t ent_y, bool sprite_ok,
                         bool *deleted) {
    *o = (AosObject29){0};
    *deleted = false;
    o->param0 = param0;
    o->param1 = param1;
    if (!sprite_ok) {
        o->deleted = true;
        *deleted = true;
        return;
    }
    o->threshold = (int32_t)param0 << 16;   /* + 0x1C */
    o->bottom = ent_y + room_y;             /* + 0x18 */
    o->pos_x = ent_x + room_x;              /* + 0x10 */
    o->pos_y = ent_y + room_y;              /* + 0x14 */
    o->scr_x = o->pos_x - room_x;           /* + 0x24 */
    o->scr_y = o->pos_y - room_y;           /* + 0x28 */
    o->world_x = ent_x;                     /* + 0x40 */
    o->world_y = ent_y;                     /* + 0x44 */
    o->dir = o->bottom > o->threshold ? 1 : 0;  /* + 0x20 */
}

void aos_object29_update(AosObject29 *o, int32_t room_x,
                         int32_t room_y) {
    switch (o->state) {
    case 0:
        if (approach(o->pos_y, o->threshold, o->dir, &o->vel_y)) {
            o->state = 1;
            o->vel_y = 0;
            o->timer = AOS_OBJ29_DWELL;
        } else {
            o->pos_y += o->vel_y;
        }
        break;
    case 1:
        if ((int8_t)--o->timer <= 0) {
            o->state = 2;
            o->dir ^= 1;
        }
        break;
    case 2:
        if (approach(o->pos_y, o->bottom, o->dir, &o->vel_y)) {
            o->state = 3;
            o->vel_y = 0;
            o->timer = AOS_OBJ29_DWELL;
        } else {
            o->pos_y += o->vel_y;
        }
        break;
    case 3:
        if ((int8_t)--o->timer <= 0) {
            o->state = 0;
            o->dir ^= 1;
        }
        break;
    }
    /* Tail: + 0x40/+ 0x44 follow + 0x24/+ 0x28 — the platform
     * carries riders through the world-position mirror. */
    o->scr_x = o->pos_x - room_x;
    o->scr_y = o->pos_y - room_y;
    o->world_x = o->scr_x + room_x;
    o->world_y = o->scr_y + room_y;
}
