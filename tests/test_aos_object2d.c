#include "../src/runtime/aos/game/aos_object2d.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    AosObject2D o;
    aos_object2d_create(&o);
    assert(o.phase == 0xA && o.preset == 2 && o.anim == 1);
    assert(o.attack == 0 && o.flags == 0);

    /* Dormant: outside the window nothing changes but anim. */
    aos_object2d_update(&o, 0x600000, 0);
    assert(o.phase == 0xA && o.anim == 2 && o.attack == 0);

    /* Dormant preset swaps need + 0x59 bit 0. */
    o.flags = 1; o.marks = 0;
    aos_object2d_update(&o, 0x600000, 0);
    assert(o.preset == 2);              /* gate closed */
    o.marks = 1;
    aos_object2d_update(&o, 0x600000, 0);
    assert(o.preset == 2);              /* flags 1 -> preset 2 */
    o.flags = 0;
    aos_object2d_update(&o, 0x600000, 0);
    assert(o.preset == 1);              /* flags 0 -> preset 1 */

    /* Inside the window the dormant state enters 0xC. */
    aos_object2d_update(&o, 0x100, 0x100);
    assert(o.phase == 0xC);

    /* Active: attack step runs while inside, presets swap. */
    o.flags = 2;
    aos_object2d_update(&o, 0x100, 0x100);
    assert(o.attack == 1 && o.preset == 4);

    /* Leaving the window returns to dormant. */
    aos_object2d_update(&o, 0x600000, 0);
    assert(o.phase == 0xA && o.attack == 1);

    /* Y window is wider (0x5FFFFF). */
    o.phase = 0xC;
    aos_object2d_update(&o, 0x100, 0x5F0000);
    assert(o.phase == 0xC && o.attack == 2);
    aos_object2d_update(&o, 0x100, 0x610000);
    assert(o.phase == 0xA);

    puts("aos_object2d: ok");
    return 0;
}
