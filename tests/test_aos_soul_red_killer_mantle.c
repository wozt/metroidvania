/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos/red_souls/aos_soul_red_killer_mantle.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    /* State 0 enters state 1 on the first update and stays while the
     * animation runs inside the margins. */
    AosKillerMantle mantle = aos_killer_mantle_create();
    assert(mantle.state == 0);
    assert(aos_killer_mantle_update(&mantle, false, false));
    assert(mantle.state == 1);
    for (int i = 0; i < 30; ++i)
        assert(aos_killer_mantle_update(&mantle, false, false));
    assert(mantle.state == 1);

    /* The animation end (sub_0803F17C == 3) moves it to state 2, and the
     * next update deletes the entity. */
    assert(aos_killer_mantle_update(&mantle, true, false));
    assert(mantle.state == 2);
    assert(!aos_killer_mantle_update(&mantle, false, false));

    /* The sub_0806D128(e, 4) margin probe ends it the same way. */
    AosKillerMantle outside = aos_killer_mantle_create();
    assert(aos_killer_mantle_update(&outside, false, false));
    assert(aos_killer_mantle_update(&outside, false, true));
    assert(outside.state == 2);
    assert(!aos_killer_mantle_update(&outside, false, false));

    puts("aos_soul_red_killer_mantle: ok");
    return 0;
}
