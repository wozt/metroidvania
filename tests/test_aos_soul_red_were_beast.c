/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos/red_souls/aos_soul_red_were_beast.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    /* State 0 enters state 1 on the first update and stays while the
     * animation runs and the source check passes. */
    AosWereBeast beast = aos_were_beast_create();
    assert(beast.state == 0);
    assert(aos_were_beast_update(&beast, false, false));
    assert(beast.state == 1);
    for (int i = 0; i < 30; ++i)
        assert(aos_were_beast_update(&beast, false, false));
    assert(beast.state == 1);

    /* The animation end (sub_0803F17C == 3) moves it to state 2, and the
     * next update deletes the entity. */
    assert(aos_were_beast_update(&beast, true, false));
    assert(beast.state == 2);
    assert(!aos_were_beast_update(&beast, false, false));

    /* The (source + 0x10 & 0x180) != 0x100 check ends it the same way. */
    AosWereBeast gone = aos_were_beast_create();
    assert(aos_were_beast_update(&gone, false, false));
    assert(aos_were_beast_update(&gone, false, true));
    assert(gone.state == 2);
    assert(!aos_were_beast_update(&gone, false, false));

    puts("aos_soul_red_were_beast: ok");
    return 0;
}
