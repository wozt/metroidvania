/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free deterministic tests for SkillAltairUse. */
#include "aos/red_souls/aos_soul_red_altair.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    AosAltair a = aos_altair_create();
    assert(a.state == 0 && a.timer == 0 && a.phase == 0);
    bool song = true;
    /* State 0 enters state 1 without needing a hit. */
    assert(aos_altair_update(&a, false, false, &song));
    assert(a.state == 1 && !song);
    /* States 1->2->3 only advance on the + 0x6C bit-4 hit flag. */
    for (int i = 0; i < 4; ++i)
        assert(aos_altair_update(&a, false, false, &song));
    assert(a.state == 1);
    assert(aos_altair_update(&a, true, false, &song));
    assert(a.state == 2);
    /* State 2 counts + 0x0D and stays until hit again. */
    for (int i = 0; i < 4; ++i)
        assert(aos_altair_update(&a, false, false, &song));
    assert(a.state == 2 && a.timer == 4);
    assert(aos_altair_update(&a, true, false, &song));
    assert(a.state == 3);
    /* State 3: + 0x6E == 2 requests song 0xEE once. */
    assert(aos_altair_update(&a, false, false, &song));
    assert(!song);
    assert(aos_altair_update(&a, false, false, &song));
    assert(song && a.phase == 2);
    /* The margin probe deletes from any state. */
    AosAltair m = aos_altair_create();
    aos_altair_update(&m, false, false, &song);
    assert(!aos_altair_update(&m, false, true, &song));
    puts("aos_soul_red_altair: ok");
    return 0;
}
