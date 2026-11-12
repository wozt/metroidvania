#include "aos_object2a.h"
#include <stddef.h>

/* 0x08526510: four 16-byte sprite records indexed by param1
 * (+ 0x32). +0 tiles, +4 palette source, +8 frames, +0xC the
 * sub_0803F2C8 preset argument, +0xD the sub_08068264 palette
 * bank and +0xE the u16 wait timer (stored into + 0xD as u8).
 * Entries 4+ are not sprite records; sub_08057774's loads fail
 * and Object2ACreate deletes the entity, so only 0-3 are valid. */
static const struct Object2ARecord {
    uint32_t tiles, palette, frames;
    uint8_t preset, bank;
    uint16_t wait;
} records[4] = {
    {0x081CBEDC, 0x0820A1C4, 0x0821154C, 0, 6, 2},
    {0x081CBEE4, 0x0820A1C4, 0x082115F8, 2, 6, 2},
    {0x081CBEF4, 0x0820A308, 0x082118D8, 0, 0, 2},
    {0x081CBF04, 0x0820A350, 0x08211D44, 0, 0, 2},
};

/* 0x08118640: the five-word record copied to the stack at create;
 * sub_080428B4/sub_08042848 receive the word at param1 * 4. */
static const uint32_t attack_records[5] = {
    0x0C2000F0, 0x0C2000F0, 0x0C20F8F0, 0x0C20F8F0, 0x00780030,
};

void aos_object2a_create(AosObject2A *o, int16_t param0, int16_t param1,
                         int32_t spawn_x, int32_t spawn_y,
                         int32_t scroll_x, int32_t scroll_y) {
    const struct Object2ARecord *rec = NULL;
    o->valid = param1 >= 0 && param1 < (int16_t)(sizeof records / sizeof records[0]);
    if (o->valid) {
        rec = &records[param1];
        o->variant = (uint8_t)param1;
        o->preset = rec->preset;
        o->pal_bank = rec->bank;
        o->wait = (uint8_t)rec->wait;
        o->hitbox_word = attack_records[param1 & 7];
    } else {
        o->variant = 0;
        o->preset = 0;
        o->pal_bank = 0;
        o->wait = 0;
        o->hitbox_word = 0;
    }
    /* + 0x10/+ 0x14 are the spawn position in the bgInfo[1] scroll
     * frame; + 0x18 keeps the spawn screen x as target B and
     * + 0x1C = param0 << 16 is target A. + 0x20 records whether
     * target B sits right of target A. */
    o->x_acc = spawn_x + scroll_x;
    o->y_acc = spawn_y + scroll_y;
    o->target_b = o->x_acc;
    o->target_a = (int32_t)param0 << 16;
    o->dir = o->target_b > o->target_a ? 1 : 0;
    o->vel = 0;
    o->state = 0;
    o->timer = 0;
    o->out_x = spawn_x;
    o->out_y = spawn_y;
    o->attacks = 0;
    o->focus_calls = 0;
}

/* The shared approach leg of states 0 (+ 0x1C) and 2 (+ 0x18):
 * integrate + 0x48, then either arrive (state + 1, + 0x48 = 0,
 * + 0xD reloaded from the record) or adjust the velocity — a
 * 0x800/frame ramp outside 0xFFFFF, a distance-proportional
 * creep inside it. Returns true on arrival. */
static bool approach(AosObject2A *o, int32_t target) {
    int32_t old = o->x_acc;
    int32_t diff = target - old;
    int32_t adist = diff < 0 ? -diff : diff;
    o->x_acc = old + o->vel;
    if (!o->dir) {
        if (old >= target) goto arrive;
        if (adist > 0xFFFFF) {
            if (o->vel <= 0xFFFF) o->vel += 0x800;
        } else {
            /* (px + 1) * 0x1000: always positive, creeps across. */
            o->vel = ((adist >> 16) << 12) + 0x1000;
        }
    } else {
        if (old <= target) goto arrive;
        if (adist > 0xFFFFF) {
            if (o->vel > (int32_t)0xFFFF0000) o->vel -= 0x800;
        } else {
            int32_t v = -((adist >> 16) << 16);
            if (v < 0) v += 0xF;
            o->vel = (v >> 4) + (int32_t)0xFFFFF000;
        }
    }
    return false;
arrive:
    o->state = (uint8_t)(o->state + 1);
    o->vel = 0;
    o->timer = o->wait;
    return true;
}

void aos_object2a_update(AosObject2A *o, int32_t scroll_x,
                         int32_t scroll_y, bool focus, bool attack) {
    if (!o->valid) return;
    switch (o->state) {
    case 0:
        (void)approach(o, o->target_a);
        break;
    case 1:
        /* + 0xD counts down as a signed byte; the transition fires
         * when the stored value is no longer positive. */
        o->timer = (uint8_t)(o->timer - 1);
        if ((int8_t)o->timer <= 0) {
            o->state = 2;
            o->dir ^= 1;
        }
        break;
    case 2:
        (void)approach(o, o->target_b);
        break;
    case 3:
        o->timer = (uint8_t)(o->timer - 1);
        if ((int8_t)o->timer <= 0) {
            o->state = 0;
            o->dir ^= 1;
        }
        break;
    }
    /* Epilogue: the world draw position is the scroll-space
     * accumulator minus the current bgInfo[1] scroll (+ 0x24/+ 0x28
     * mirror it for the sub_080577DC draw swap). */
    o->out_x = o->x_acc - scroll_x;
    o->out_y = o->y_acc - scroll_y;
    if (focus) o->focus_calls++;
    if (attack) o->attacks++;
}
