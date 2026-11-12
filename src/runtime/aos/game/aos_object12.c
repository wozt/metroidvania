#include "aos_object12.h"

/* 0x085263A8: four 24-byte records — flags@0x10, callback@0x14.
 * Record 0 runs sub_0805133C, records 2/3 run sub_080514D8. */
static const uint8_t rec_flags[4] = {0x00, 0x08, 0x04, 0x06};

void aos_object12_create(AosObject12 *o, int param0, int param1,
                         bool *deleted) {
    (void)param1;
    int rec = param0 < 0 ? 0 : (param0 > 3 ? 3 : param0);
    *deleted = false;
    o->variant = (uint8_t)rec;
    o->state = 0;
    o->timer = 0;
    o->seq = 0xFF;
    o->flags = rec_flags[rec];
    o->palette_step = 0;
    o->animated = !(o->flags & 4);
}

static void record0_update(AosObject12 *o, bool hit, uint32_t (*rng)(void)) {
    /* sub_0805133C: states 0-4, sequences 0/1/2 via sub_0803F0E4. */
    switch (o->state) {
    case 0:
        o->seq = 0;
        o->timer = (uint8_t)((rng() & 0x7F) + 8);
        o->state = 1;
        break;
    case 1:
        if (o->timer == 0) {
            o->seq = 1;
            o->state = 2;
        } else {
            o->timer--;
        }
        break;
    case 2:
        if (hit) {
            o->timer = (uint8_t)((rng() & 0x7F) + 0x10);
            o->state = 3;
        }
        break;
    case 3:
        if (o->timer == 0) {
            o->seq = 2;
            o->state = 4;
        } else {
            o->timer--;
        }
        break;
    case 4:
        if (hit) o->state = 0;
        break;
    }
}

static void drift_update(AosObject12 *o, bool param1_is_one) {
    /* sub_080514D8: state 0 arms the sub_0805153C stepper once the
     * + 0x32 == 1 gate holds (camera deltas folded into flags); the
     * stepper then cycles the + 0x5A palette bits each update —
     * approximated by palette_step. */
    if (o->state == 0) {
        o->state = 1;
    }
    if (param1_is_one)
        o->palette_step = (uint8_t)((o->palette_step + 1) & 3);
}

void aos_object12_update(AosObject12 *o, bool near, bool hit,
                         bool param1_is_one, uint32_t (*rng)(void)) {
    if (o->flags & 8) {
        if (!near) return;    /* probe miss skips callback and anim */
    }
    if (o->variant == 0) {
        record0_update(o, hit, rng);
    } else if (o->flags & 4) {
        drift_update(o, param1_is_one);
    }
    /* !(flags & 4) records run the shared sub_0803F17C step, which
     * the runtime's animation loop performs for every sprite. */
}
