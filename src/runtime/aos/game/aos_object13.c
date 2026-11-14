#include "aos_object13.h"
#include "../math/aos_math.h"

void aos_object13_create(AosObject13 *o, int param0, int param1,
                         int32_t amplitude, uint16_t phase,
                         uint8_t state) {
    o->param0 = (int16_t)param0;
    o->param1 = (int16_t)param1;
    o->amplitude = amplitude;
    o->phase = phase;
    o->state = state;
    o->death = 0;
    o->event_fired = false;
    o->deleted = false;
    o->sway = 0;
}

void aos_object13_update(AosObject13 *o, bool event_set) {
    o->event_fired = false;
    if (o->state == 0xFF) {
        /* + 0xD counts 0,1,2 then the third update deletes. */
        if (o->death <= 1) {
            o->death++;
        } else {
            o->deleted = true;
        }
        return;
    }
    if (o->amplitude > 0x8C << 6)
        o->amplitude = 0x8C << 6;    /* + 0x50 cap 0x2300 */

    if (o->param1 != 0 && !event_set) {
        int32_t threshold = o->param0 == 0 ? 0x84 << 6 : 0x90 << 5;
        /* (u32)(phase - 0xAC01) <= 0x7FE == phase in [0xAC01,0xB3FF]. */
        uint32_t window = (uint32_t)(uint16_t)(o->phase - 0xAC01);
        if (o->amplitude > threshold && window <= 0x7FE) {
            o->phase = (uint16_t)(o->phase - 0x40);
            o->amplitude -= 0x100;
            o->event_fired = true;
        }
    }

    if (o->amplitude > 0) {
        o->amplitude -= 4;
        o->phase = (uint16_t)(o->phase + 0x200);
    } else {
        o->amplitude = 0;
        o->phase = 0;
    }
    int64_t sway = (int64_t)aos_sine(o->phase) * o->amplitude;
    if (sway < 0) sway += 0xFFFF;
    o->sway = (int32_t)(sway >> 16);
}

void aos_object13_strike(AosObject13 *o, bool player_left) {
    if (o->state == 0xFF)
        return;             /* the head deletes itself on +0xA == 0xFF */
    /* sl (r6/sl in sub_0805170C): 1 while (u32)(phase - 0x4001) <=
     * 0x7FFE, i.e. phase in [0x4001, 0xBFFF]; else 2 while the
     * amplitude lives; else 0 (settled). */
    if ((uint32_t)o->phase - 0x4001u <= 0x7FFE) {
        /* _08051998/_080519C8: a left-side strike pumps +0x300
         * (_080519F8); a right-side strike falls through to
         * _08051A0C and brakes -0x60. */
        o->amplitude += player_left ? 0x300 : -0x60;
        return;
    }
    if (o->amplitude != 0) {
        /* sl == 2 (_0805195C): only a right-side strike with the
         * head's +0x58 bit 6 set pumps; bit 6 is unported (clear),
         * so every strike takes the _08051A0C -0x60 brake. */
        o->amplitude -= 0x60;
        return;
    }
    /* sl == 0 (_08051928-_080519F8): a settled strike always pumps
     * +0x300; from the left it also rewinds the phase to 0x8000. */
    if (player_left)
        o->phase = 0x8000;
    o->amplitude += 0x300;
}
