#include "aos_object1b.h"

/* 0x081185C8: the three tile frames the mod-3 index selects. */
static const uint8_t frames[3] = {0x18, 0x1D, 0x1E};

void aos_object1b_create(AosObject1b *o) {
    o->state = 0;
    o->toggle = 0;
    o->angle = 0;
    o->frame = 0xFF;
    o->song = -1;
}

void aos_object1b_update(AosObject1b *o, bool focused, int32_t *clock) {
    o->song = -1;
    if (o->state == 0 && focused) {
        o->toggle ^= 1;
        o->song = 0x133;
        o->state = 1;
    } else if (o->state == 1 && *clock <= 0x13FFF) {
        *clock += 0x80 << 5;    /* + 0x1000 */
    }
    o->angle += *clock;
    int32_t index = o->angle >> 0x13;
    index %= 3;
    if (index < 0) index = -index;
    uint8_t frame = frames[index];
    if (o->frame != frame) {
        o->frame = frame;       /* sub_0803C918 tile push */
    }
}
