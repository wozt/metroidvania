#include "aos_object37.h"

void aos_object37_create(AosObject37 *o, int32_t pos_x,
                         int32_t pos_y, bool player_near_x,
                         bool rng_ok, uint8_t rng_bits,
                         bool sprite_ok, bool *deleted) {
    *o = (AosObject37){0};
    *deleted = false;
    o->pos_x = pos_x;
    o->pos_y = pos_y;
    o->anim = 9;
    o->sel = 1;
    o->song = -1;
    if (!sprite_ok || !rng_ok) {
        o->deleted = true;
        *deleted = true;
        return;
    }
    /* +0x58 bit 5 = RNG & 0x1F (bits 0/6 stay clear). */
    o->flag_58 = (uint8_t)((rng_bits & 0x1F) << 5);
    if (player_near_x) {
        /* NEAR: +0x18 = 0x3000, +0x1C = 0. */
        o->fade = 0x3000;
    } else {
        /* FAR: +0x1C = 1, five sub_08002200 placements, then
         * the sub_0805AC50 child. */
        o->flag_1c = 1;
        o->place_out = 5;
        o->child_count = 1;
    }
}

void aos_object37_update(AosObject37 *o, bool player_far_x,
                         bool player_flag40) {
    o->song = -1;
    o->place_out = 0;
    o->place_in = 0;

    if (o->flag_a != 0) return;

    if (o->state == 0) {
        /* +0x18 -= 0x100 clamped at 0. */
        o->fade -= 0x100;
        if (o->fade < 0) o->fade = 0;
        if (player_far_x && player_flag40) {
            o->state = 1;
            o->song = 0x113;
        }
    } else {
        /* +0x18 += 0x100 clamped at 0x3000. */
        o->fade += 0x100;
        if (o->fade > 0x3000) o->fade = 0x3000;
        if (o->flag_1c) {
            o->flag_1c = 0;
            o->place_in = 5;                /* sub_08002248 x5 */
        }
        if (player_far_x && !player_flag40) {
            o->state = 0;
            o->flag_1c = 1;
            o->place_out = 5;               /* sub_08002200 x5 */
            o->song = 0x113;
        }
    }
}
