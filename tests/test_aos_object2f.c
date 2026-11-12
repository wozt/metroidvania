#include "../src/runtime/aos/game/aos_object2f.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    AosObject2F o;

    /* + 0x37E bit 2 set deletes the entity like the native create. */
    aos_object2f_create(&o, 4, 0);
    assert(o.deleted);

    aos_object2f_create(&o, 0, 1);
    assert(!o.deleted);
    assert(o.anchor_x == 0x78 && o.anchor_y == 0x48);
    assert(o.anim == 1 && o.stopped == 0 && o.mark == 0x20);

    /* The + 0x42C bit-0x1000 trigger latches + 0xB and clears the
     * + 0x58 bit-0x20 mark; the animation keeps stepping. */
    aos_object2f_update(&o, true);
    assert(o.stopped == 1 && o.mark == 0);
    assert(o.anim == 2);
    aos_object2f_update(&o, true);
    assert(o.anim == 3);

    /* param0 outside 0-2 falls back to variant 0 like a sane index. */
    AosObject2F p;
    aos_object2f_create(&p, 0, 7);
    assert(p.anchor_x == 0x30 && p.anchor_y == 0x38);

    puts("aos_object2f: ok");
    return 0;
}
