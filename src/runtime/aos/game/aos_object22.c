#include "aos_object22.h"

/* ROM data dumped from the (USA) image:
 *   0x0852649C u16 stop bounds, indexed by s8 + 0x20 (1-5):
 *     {64,192,352,512,672,832}
 *   0x085264BC ptr[5] into the 0x085264A8 stream, bases
 *     {0,3,6,10,14}; record = [count, dir/mask bytes...]
 *   0x085264A8 stream (first 19 bytes):
 *     02 20 10 | 02 40 80 | 03 10 80 40 | 03 40 40 80 |
 *     04 40 10 80 20
 *   0x08118628 u8 lifetimes {60,54,46,38,30} indexed by
 *     stop - 1. */
static const uint16_t k_stop_bound[AOS_OBJ22_STOP_COUNT] = {
    64, 192, 352, 512, 672, 832};
static const uint8_t k_record_base[5] = {0, 3, 6, 10, 14};
static const uint8_t k_stream[] = {
    0x02,0x20,0x10, 0x02,0x40,0x80, 0x03,0x10,0x80,0x40,
    0x03,0x40,0x40,0x80, 0x04,0x40,0x10,0x80,0x20};
static const uint8_t k_lifetime[5] = {60, 54, 46, 38, 30};

static uint8_t stream_at(const AosObject22 *o, int stop,
                         int index) {
    (void)o;
    int p = k_record_base[stop - 1] + index;
    return p < (int)sizeof k_stream ? k_stream[p] : 0;
}

/* sub_08056F50 dir code -> (vx, vy) 16.16 pair. */
static void shot_velocity(int dir, int32_t *vx, int32_t *vy) {
    switch (dir) {
    case 0x40: *vx = 0;          *vy = 0xD6 << 15; break; /* 0x6B0000 */
    case 0x10: *vx = 0xC0 << 11; *vy = 0xE4 << 15; break; /* 0x60000/0x720000 */
    case 0x80: *vx = 0;          *vy = 0xF2 << 15; break; /* 0x790000 */
    case 0x20: *vx = (int32_t)0xFFFA0000; *vy = 0xE4 << 15; break;
    default:   break; /* unmatched codes keep the caller's
                       * carry-over velocities, as the ROM
                       * leaves r6/r8 untouched */
    }
}

void aos_object22_create(AosObject22 *o, int cam_byte,
                         uint16_t room_x, uint16_t room_y,
                         int32_t room_ox, int32_t room_oy,
                         bool sprite_ok, bool *deleted) {
    *o = (AosObject22){0};
    *deleted = false;
    o->cam_byte = (uint8_t)cam_byte;
    o->accel_y = 0x2800;                /* + 0x54 */
    o->song = -1;
    o->room_x6 = room_x;
    o->room_yA = room_y;
    o->room_ox = room_ox;
    o->room_oy = room_oy;
    if (!sprite_ok) {
        o->deleted = true;
        *deleted = true;
        return;
    }
    /* + 0x42 = 0x40 - A094[6] ; + 0x46 = 0x9D - A094[0xA] */
    o->bound_px = (int16_t)(0x40 - room_x);
    o->anchor_py = (int16_t)(0x9D - room_y);
}

void aos_object22_update(AosObject22 *o, bool linked_self,
                         uint16_t hit_flags) {
    o->song = -1;
    o->deactivate = false;
    o->set_global_348 = false;
    o->spawn_projectile = false;
    o->spawn_carrier = false;

    if (o->flags23 & 1) {               /* + 0x23 bit 0 */
        o->deactivate = true;           /* sub_08021278(1) */
    }

    switch (o->state) {
    case 0:
        /* Anchor + 0x46 by the linked check; while linked ==
         * self count + 0xD to 0x3B then arm state 1. */
        o->anchor_py = (int16_t)((linked_self ? 0x9E : 0x9D) -
                                 o->room_yA);
        if (linked_self) {
            uint8_t old = o->timer;
            o->timer = (uint8_t)(old + 1);
            if (old > 0x3B) {
                o->state = 1;
                o->stop = 1;            /* + 0x20 s8 */
                o->vel_x = 0x40000;     /* + 0x48 */
                o->timer = 0;
            }
        } else {
            o->timer = 0;
        }
        break;

    case 1:
        o->pos_x += o->vel_x;           /* sub_0806D3C0 */
        if (!linked_self) {
            o->aux_1c = (uint8_t)(o->aux_1c + 1);
            if ((uint8_t)(o->aux_1c - 1) > 0x3B) {
                /* Timeout -> finale arm (state 4). */
                o->substate = 0;
                o->state = 4;
                o->timer = 0x14;
                break;
            }
        } else {
            o->anchor_py = (int16_t)(0x9E - o->room_yA);
            o->aux_1c = 0;
            /* Bound check: (+ 0x42 + room x) >= stop bound. */
            int probe = o->bound_px + o->room_x6;
            if (probe >= k_stop_bound[o->stop]) {
                o->bound_px = (int16_t)(k_stop_bound[o->stop] -
                                        o->room_x6);
                o->vel_x = 0;
                o->state = 2;
                o->substate = 0;
                o->timer = 0;
            }
        }
        break;

    case 2:
        if (!linked_self) {
            o->anchor_py = (int16_t)(0x9D - o->room_yA);
            o->aux_1c = (uint8_t)(o->aux_1c + 1);
            if ((uint8_t)(o->aux_1c - 1) > 0x3B) {
                o->substate = 0;
                o->state = 4;
                o->timer = 0x14;
                break;
            }
        } else {
            o->anchor_py = (int16_t)(0x9E - o->room_yA);
            o->aux_1c = 0;
        }
        if (o->substate == 0) {
            uint8_t old = o->timer;
            o->timer = (uint8_t)(old + 1);
            if (old > 0x1D) {
                o->substate = 1;
                o->timer = 0;
            }
        } else if (o->substate == 1) {
            /* Fire one volley shot: dir byte record[volley+1]. */
            int dir = stream_at(o, o->stop, o->volley + 1);
            int32_t vx = 0, vy = 0;
            /* The ROM adds carry-over r6/r8 for unmatched
             * codes; the port starts from zero and documents
             * the divergence. */
            shot_velocity(dir, &vx, &vy);
            int32_t bx = ((int32_t)k_stop_bound[o->stop]) << 16;
            o->shot_x = vx + bx - o->room_ox;
            o->shot_y = vy - o->room_oy;
            o->shot_life = k_lifetime[o->stop - 1];
            o->shot_dir = dir;
            o->spawn_projectile = true;
            o->substate = 2;
            o->timer = 0;
            o->volley_flag = 0;         /* + 0x22 */
            o->song = 0x15B;
        } else {
            /* Substate 2: lifetime count + hit masks. */
            o->timer = (uint8_t)(o->timer + 1);
            if (o->volley_flag == 0) {
                uint8_t mask = stream_at(o, o->stop, o->volley);
                if ((hit_flags & mask) != 0) {
                    o->volley_flag = 1;
                    o->song = 0x15A;
                } else if (hit_flags & 0xF0) {
                    /* The ROM stores the stale r3 (the record
                     * lifetime byte) here; approximated. */
                    o->volley_flag = o->shot_life;
                    o->timer = o->shot_life;
                    o->song = 0x15A;
                }
            }
            if (o->timer >= (uint8_t)(o->shot_life + 0x1E)) {
                if (o->volley_flag == 1) {
                    o->timer = 0;
                    o->volley = (int8_t)(o->volley + 1);
                    uint8_t count = stream_at(o, o->stop, 0);
                    if (o->volley < (int8_t)count) {
                        o->substate = 1;    /* next volley */
                    } else {
                        o->volley = 0;
                        o->substate = 0;
                        o->state = 3;
                    }
                } else {
                    o->substate = 0;
                    o->state = 4;
                    o->timer = 0x14;
                }
            }
        }
        break;

    case 3:
        o->anchor_py = (int16_t)((linked_self ? 0x9E : 0x9D) -
                                 o->room_yA);
        {
            uint8_t old = o->timer;
            o->timer = (uint8_t)(old + 1);
            if (old != 0) {
                o->stop = (int8_t)(o->stop + 1);
                if (o->stop > 5) {
                    o->state = 10;
                    o->timer = 0;
                } else {
                    o->timer = 0;
                    o->vel_x = 0x40000;
                    o->state = 1;
                }
            }
        }
        break;

    case 4:
        if (o->substate == 0) {
            o->substate = 1;
            o->vel_x = 0;
            o->aux_50 = 0;
            o->spawn_carrier = true;    /* sub_08056D6C */
            o->song = 0x15C;
        }
        if (--o->timer == 0) {          /* signed u8 expiry */
            o->flags23 |= 1;
            o->timer = 0x28;
            o->state = 5;
        }
        break;

    case 5:
        /* sub_0806A224 heavy physics approximated by the
         * shared gravity step; sub_0806D518 phase table
         * skipped (no per-frame phase input). */
        o->vel_y += o->accel_y;
        if (o->vel_y > 0x70000) o->vel_y = 0x70000;
        if (o->vel_y < -0x70000) o->vel_y = -0x70000;
        o->pos_y += o->vel_y;
        o->timer = (uint8_t)(o->timer - 1);
        if ((int8_t)o->timer <= 0) {
            o->flags59 |= 8;
        }
        break;

    case 10:
        o->set_global_348 = true;       /* + 0x348 bit 15 */
        o->state = 11;
        break;

    default:
        break;                          /* 6-9, 11: empty */
    }
}
