/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>

#include "../src/runtime/aos/game/aos_object01.h"

int main(void) {
    AosObject01 o;

    /* param0 0: one child at (0xE0, 0xBF). */
    aos_object01_reset();
    aos_object01_create(&o, 0);
    assert(o.claimed && !o.deleted && o.child_count == 1);
    assert(o.child_x[0] == 0xE0 && o.child_y[0] == 0xBF);
    aos_object01_update(&o);
    assert(o.claimed && !o.deleted);   /* claimed path is inert */

    /* The slot stays held: a second object fails and self-deletes on
     * its first update, releasing the slot unconditionally. */
    AosObject01 p;
    aos_object01_create(&p, 1);
    assert(!p.claimed && !p.deleted && p.child_count == 0);
    aos_object01_update(&p);
    assert(p.deleted);

    /* param0 1 now claims: three children at the table positions. */
    aos_object01_create(&p, 1);
    assert(p.claimed && p.child_count == 3);
    assert(p.child_x[0] == 0x80 && p.child_y[0] == 0xC0);
    assert(p.child_x[1] == 0x80 && p.child_y[1] == 0xA0);
    assert(p.child_x[2] == 0x100 && p.child_y[2] == 0xC0);

    /* param0 > 1 or negative never claims. */
    aos_object01_reset();
    aos_object01_create(&o, 2);
    assert(!o.claimed);
    aos_object01_update(&o);
    assert(o.deleted);
    aos_object01_create(&o, -1);
    assert(!o.claimed);

    printf("aos_object01: ok\n");
    return 0;
}
