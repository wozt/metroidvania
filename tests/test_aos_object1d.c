#include "../src/runtime/aos/game/aos_object1d.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    AosObject1D o;
    bool deleted;

    /* param0 == 0, sum == 0 -> self-delete. */
    aos_object1d_create(&o, 0, 0x40, 0x50, 0, &deleted);
    assert(deleted && o.deleted);

    /* param0 == 0, sum == 1 -> -0xD0 x shift, facing 1,
     * flags58 bits 0/6 clear, sel 0xB, 12 frames. */
    aos_object1d_create(&o, 0, 0x100, 0x200, 1, &deleted);
    assert(!deleted && !o.deleted);
    assert(o.sel == 0xB && o.frame_count == 0xC);
    assert(o.off_x == 0x100 - 0xD0 && o.off_y == 0x200);
    assert(o.facing == 1 && !(o.flags58 & 0x41));
    assert(o.anim == 0x11 && o.placements == 4);

    /* param0 == 0, sum == -1 -> facing -1, flags58 bit 6. */
    aos_object1d_create(&o, 0, 0x100, 0x200, -1, &deleted);
    assert(o.facing == -1 && (o.flags58 & 0x40));
    assert(o.off_x == 0x100 && o.sel == 0xB);

    /* param0 == 0, other sum -> fields untouched. */
    aos_object1d_create(&o, 0, 0x100, 0x200, 5, &deleted);
    assert(!deleted && o.facing == 0 && o.off_x == 0x100);

    /* param0 != 0, sum == 1 -> absolute 0x10/0x70, facing 1,
     * flags58 bits 0/4/5/7 clear, sel 0x14, 10 frames. */
    aos_object1d_create(&o, 1, 0x100, 0x200, 1, &deleted);
    assert(!deleted && o.sel == 0x14 && o.frame_count == 0xA);
    assert(o.off_x == 0x10 && o.off_y == 0x70);
    assert(o.facing == 1 && !(o.flags58 & 0xB1));

    /* param0 != 0, sum == -1 -> 0xE0/0x70, facing 0, bit 6. */
    aos_object1d_create(&o, 1, 0, 0, -1, &deleted);
    assert(o.off_x == 0xE0 && o.off_y == 0x70);
    assert(o.facing == 0 && (o.flags58 & 0x40));

    /* param0 != 0, sum == 0 -> delete. */
    aos_object1d_create(&o, 2, 0, 0, 0, &deleted);
    assert(deleted && o.deleted);

    puts("aos_object1d: ok");
    return 0;
}
