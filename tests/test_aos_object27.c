#include "../src/runtime/aos/game/aos_object27.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    AosObject27 o;

    /* Create presets: phase 0, done mark, variant kept. */
    aos_object27_create(&o, 0x5A);
    assert(o.phase == 0 && o.variant == 0x5A && o.done);
    assert(o.spin == 0 && o.timer == 0 && o.attack_armed);

    /* State 0 waits for the target-far gate. */
    aos_object27_update(&o, false);
    assert(o.phase == 0 && o.spin == 0);
    aos_object27_update(&o, true);
    assert(o.phase == 1 && o.spin == 0xC000);
    assert(o.angle == 0xC000);

    /* State 1 ramps + 0x48 by 0x800; the frame that finds it past
     * 0xFFFF clamps to 0x40000 and enters state 2. */
    for (int i = 0; i < 64 && o.phase == 1; ++i)
        aos_object27_update(&o, false);
    assert(o.phase == 2 && o.spin == 0x40000);
    assert(o.timer == 0x1E && o.preset_refreshed && !o.attack_armed);

    /* State 2 counts + 0xD down; the old <= 1 enters state 3. */
    aos_object27_update(&o, false);
    assert(o.phase == 2 && o.timer == 0x1D);
    o.timer = 2;
    aos_object27_update(&o, false);
    assert(o.phase == 2 && o.timer == 1);
    aos_object27_update(&o, false);         /* old == 1 -> wrap */
    assert(o.phase == 3 && o.timer == 0 && o.attack_armed);

    /* State 3 bleeds + 0x48 by 0x800 to 0xC000 then resets. */
    while (o.phase == 3 && o.spin > 0xC000)
        aos_object27_update(&o, false);
    assert(o.spin == 0xC000);
    aos_object27_update(&o, false);
    assert(o.phase == 0 && o.done);

    puts("aos_object27: ok");
    return 0;
}
