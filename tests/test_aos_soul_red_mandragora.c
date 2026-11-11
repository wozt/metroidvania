/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free deterministic tests for SkillMandragoraUse. */
#include "aos/red_souls/aos_soul_red_mandragora.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    AosMandragora m = aos_mandragora_create();
    assert(m.state == 0 && m.timer == 0 && !m.leaf);
    assert(aos_mandragora_update(&m, 0, 0x100000, 0x200000, false, false));
    assert(m.state == 1);
    /* State 1: + 0x0D counts up; a leaf emits every fourth frame. */
    int leaves = 0;
    for (int i = 0; i < 16; ++i) {
        assert(aos_mandragora_update(&m, i * 977, 0x100000, 0x200000,
                                     false, false));
        leaves += m.leaf;
    }
    assert(leaves == 4 && m.timer == 16);
    /* Leaf positions stay within the +/- 24 px sine offsets of the
     * entity origin passed in. */
    AosMandragora p = aos_mandragora_create();
    aos_mandragora_update(&p, 0, 0, 0, false, false);
    for (int i = 0; i < 8; ++i)
        aos_mandragora_update(&p, 0x4000 * i, 0x1000000, 0x2000000,
                              false, false);
    assert(p.leaf);
    int dx = (p.leaf_x >> 16) - 0x100, dy = (p.leaf_y >> 16) - 0x200;
    assert(dx >= -24 && dx <= 24 && dy >= -24 && dy <= 24);
    /* The + 0x6C bit-4 hit flag starts the 0x20 + 8 death sequence. */
    AosMandragora d = aos_mandragora_create();
    aos_mandragora_update(&d, 0, 0, 0, false, false);
    assert(aos_mandragora_update(&d, 0, 0, 0, true, false));
    assert(d.state == 2 && d.timer == 0x20);
    for (int i = 0; i < 0x21; ++i)
        assert(aos_mandragora_update(&d, 0, 0, 0, false, false));
    assert(d.state == 3 && d.timer == 8 && d.flag_f);
    for (int i = 0; i < 8; ++i)
        assert(aos_mandragora_update(&d, 0, 0, 0, false, false));
    assert(!aos_mandragora_update(&d, 0, 0, 0, false, false));
    /* The margin probe deletes from any state. */
    AosMandragora o = aos_mandragora_create();
    aos_mandragora_update(&o, 0, 0, 0, false, false);
    assert(!aos_mandragora_update(&o, 0, 0, 0, false, true));
    puts("aos_soul_red_mandragora: ok");
    return 0;
}
