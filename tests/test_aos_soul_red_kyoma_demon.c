/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free deterministic tests for SkillKyomaDemonUse. */
#include "aos/red_souls/aos_soul_red_kyoma_demon.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    AosKyomaDemon demon = aos_kyoma_demon_create();
    assert(demon.state == 0);
    /* First update enters state 1 with + 0x0D = 0x20. */
    assert(aos_kyoma_demon_update(&demon));
    assert(demon.state == 1 && demon.wobble == 0x20);
    /* State 1 lasts until + 0x0D reads 0, then state 2 arms + 0x24 = 0xB4. */
    int frames = 0;
    while (demon.state == 1) {
        assert(aos_kyoma_demon_update(&demon));
        ++frames;
    }
    assert(demon.state == 2 && demon.linger == 0xB4);
    assert(frames == 0x21);
    /* State 2 counts 0xB4 down to 0, then the entity deletes. */
    frames = 0;
    while (aos_kyoma_demon_update(&demon)) ++frames;
    assert(frames == 0xB4);
    assert(!aos_kyoma_demon_update(&demon) || demon.linger == 0);
    puts("aos_soul_red_kyoma_demon: ok");
    return 0;
}
