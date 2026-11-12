#include "aos_object17.h"

/* 0x08118590: three s16 floor bands {0x80, 0x180, 0x280}. */
static const int16_t pan_levels[3] = {0x80, 0x180, 0x280};

void aos_object17_create(AosObject17 *o, int player_y,
                         uint16_t room_ox, uint16_t room_oy) {
    /* First band whose level + 0x20 is not below the player Y;
     * no match leaves the zeroed + 0xB. */
    o->variant = 0;
    for (int i = 0; i < 3; ++i) {
        if ((int16_t)(pan_levels[i] + 0x20) >= player_y) {
            o->variant = (uint8_t)i;
            break;
        }
    }
    o->pos_x = (uint16_t)(0x78 - room_ox);
    o->pos_y = (uint16_t)(pan_levels[o->variant] - room_oy);
    o->dir = 0;
    o->state = 0;
    o->dwell = 0;
    o->timer = 0;
    o->speed = 0;
    /* Entities are zeroed at spawn; + 0x44 gets its first write
     * in state 2, so the accumulator starts at 0 natively. The
     * port keeps 0 rather than guessing a spawn-side init. */
    o->pos = 0;
    o->flags59 = 0x04;          /* create sets + 0x59 bit 2 */
    o->child_count = 19;        /* 1 + 2 + 16 type-0x49 children */
    o->deleted = false;
    o->locked = false;
    o->song = -1;
    o->song_stop = -1;
    o->fade = false;
    o->slot_clear = false;
}

void aos_object17_update(AosObject17 *o, bool gate_blocked,
                         bool near, uint16_t flags,
                         int32_t room_y16, uint16_t room_oy) {
    o->locked = false;
    o->song = -1;
    o->song_stop = -1;
    o->fade = false;
    o->slot_clear = false;

    if (!(o->flags59 & 0x04)) {
        /* + 0x59 bit 2 clear: set bit 3, clear the + 0x1316C
         * singleton slot and skip the state machine. */
        o->flags59 |= 0x08;
        o->slot_clear = true;
        return;
    }

    switch (o->state) {
    case 0:
        if (gate_blocked || !near) {
            o->dwell = 0;
            break;
        }
        if (flags & 0xC0)
            o->dwell++;
        else
            o->dwell = 0;
        if (o->dwell <= 2)
            break;
        if ((flags & 0x80) && o->variant <= 1) {
            o->variant++;
            o->dir = 0;
            o->state = 1;
            o->timer = 0x20;
            o->locked = true;
        }
        if ((flags & 0x40) && o->variant != 0) {
            o->variant--;
            o->dir = 1;
            o->state = 1;
            o->timer = 0x40;
            o->locked = true;
        }
        o->speed = 0;
        break;
    case 1:
        o->locked = true;
        if (o->timer) {
            o->timer--;
        } else {
            o->song = 0x116;    /* sub_08013D60 */
            o->state = 2;
            o->speed = 0;
        }
        break;
    case 2: {
        o->locked = true;
        o->pos += o->speed;
        if (!o->dir) {
            if (o->speed <= 0x2FFFF)
                o->speed += 0x1000;
        } else {
            if (o->speed > (int32_t)0xFFFD0000)
                o->speed -= 0x1000;
        }
        int32_t pos = o->pos + o->speed + room_y16;
        if (pos < 0)
            pos = -(((-pos) >> 16) << 16);
        int16_t posi = (int16_t)((uint32_t)pos >> 16);
        int16_t target = pan_levels[o->variant];
        bool arrived = o->dir ? posi <= target : posi >= target;
        if (!arrived)
            break;
        o->song_stop = 0x116;   /* sub_08013E18 */
        o->song = 0x117;        /* PlaySong */
        o->fade = true;         /* sub_08011FC0(0, 0x10, 0, 7) */
        o->pos = ((int32_t)target - (int32_t)room_oy) << 16;
        o->speed = 0;
        o->state = 3;
        o->timer = 0x40;
        break;
    }
    case 3:
        if (o->timer) {
            o->timer--;
        } else {
            o->state = 0;
        }
        break;
    }
}
