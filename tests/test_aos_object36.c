#include "../src/runtime/aos/game/aos_object36.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    AosObject36 o;
    uint16_t block[9];

    /* param0 = 4 selects record {128, 120}: scale 64,
     * speed 120, extent 0x200 - 256 = 0x100. */
    aos_object36_create(&o, 4, false);
    assert(o.registered);
    assert(o.scale == 64 && o.speed == 120 && o.extent == 0x100);
    assert(o.pos == 0);

    memset(block, 0xAA, sizeof block);
    aos_object36_update(&o, block);
    /* The block mirrors the native stack image and pos advanced
     * by speed after the block was filled. */
    assert(block[0] == 0x8000 && block[1] == 0);
    assert(block[2] == 0x8000 && block[3] == 0);
    assert(block[6] == 0x100 && block[7] == 0x100);
    assert(block[8] == 0);
    assert(o.pos == 120);

    aos_object36_update(&o, block);
    assert(block[8] == 120 && o.pos == 240);

    /* param0 = 3 selects {96, -90}: negative speed walks pos
     * backward and the low halfword wraps like the native. */
    AosObject36 g;
    aos_object36_create(&g, 3, false);
    assert(g.scale == 48 && g.speed == -90 && g.extent == 0x140);
    aos_object36_update(&g, block);
    assert(g.pos == -90);

    /* A busy + 0x1316C slot reports unregistered. */
    AosObject36 b;
    aos_object36_create(&b, 0, true);
    assert(!b.registered);

    puts("aos_object36: ok");
    return 0;
}
