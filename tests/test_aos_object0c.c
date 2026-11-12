/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>

#include "../src/runtime/aos/game/aos_object0c.h"
#include "../src/runtime/aos/game/aos_object01.h"

static void create_claims_and_marks(void) {
    AosObject0c o;
    aos_object0c_create(&o, 1);
    assert(o.claimed);
    assert(o.flags59 == 4);
    assert(o.param0 == 1);
}

static void type0_writes_param_when_flag30(void) {
    AosObject0c o;
    aos_object0c_create(&o, 1);   /* list1: type-0 rec at (512,400,96,96) */
    /* Outside every window: nothing fires, +0x48 stays cleared. */
    aos_object0c_update(&o, 0, 0, true, false, 0, 0, 0);
    assert(o.value48 == 0);
    /* Inside the type-0 record with bit 30 set: +0x48 = 0x8000. */
    aos_object0c_update(&o, 560, 440, true, false, 0, 0, 0);
    assert(o.value48 == 0x8000);
    /* Same frame a type-1 record also contains the point? No: the
     * type-0 rect is disjoint; a neighbouring type-1 point sets the
     * heavy flag instead. */
    aos_object0c_update(&o, 100, 500, true, false, 0, 0, 0);
    assert(o.heavy_set);
    /* Bit 30 clear: the callback is gated off. */
    AosObject0c p;
    aos_object0c_create(&p, 1);
    aos_object0c_update(&p, 560, 440, false, false, 0, 0, 0);
    assert(p.value48 == 0);
    assert(!p.heavy_set);
}

static void type1_sets_heavy(void) {
    AosObject0c o;
    aos_object0c_create(&o, 1);
    /* list1 type-1 record (88,400,40,272): x in [88,128], y in [400,672]. */
    aos_object0c_update(&o, 100, 500, true, false, 0, 0, 0);
    assert(o.heavy_set);
    /* list2 type-1 record (88,752,40,112). */
    AosObject0c p;
    aos_object0c_create(&p, 2);
    aos_object0c_update(&p, 100, 800, true, false, 0, 0, 0);
    assert(p.heavy_set);
}

static void type2_arms_snapshot_and_pan(void) {
    AosObject0c o;
    aos_object0c_create(&o, 0);   /* list0 type-2 rec (344,1088,48,144) */
    /* Gate needs + 0x13260 bit 0x200 or Soma bit 30. */
    aos_object0c_update(&o, 360, 1100, false, false, 0x12345678, -0x10, 0);
    assert(!o.snapshot_armed);
    aos_object0c_update(&o, 360, 1100, false, true, 0x12345678, -0x10, 0);
    assert(o.snapshot_armed);
    assert(o.snapshot_x == 0x12345678 + 0x80000);
    assert(o.snapshot_y == (int32_t)-0x10);
    /* record param == 0: pan (-0x30000, -0x30000), value48 = 0xFFFD0000. */
    assert(o.pan_x == -0x30000 && o.pan_y == -0x30000);
    assert(o.value48 == 0xFFFD0000u);
    /* entity + 0x1E nonzero: forced to 1, no snapshot. */
    AosObject0c p;
    aos_object0c_create(&p, 0);
    aos_object0c_update(&p, 360, 1100, true, false, 0, 0, 7);
    assert(p.soma_1e_forced);
    assert(!p.snapshot_armed);
}

static void release_needs_bit2_clear(void) {
    /* Earlier tests left the shared + 0x1316C slot held. */
    aos_object01_release_slot();
    AosObject0c o;
    aos_object0c_create(&o, 1);
    aos_object0c_update(&o, 0, 0, true, false, 0, 0, 0);
    /* bit 2 stays set: no release, bit 3 never set. */
    assert(o.flags59 == 4);
    assert(o.claimed);
    /* Clearing bit 2 (an external + 0x59 write) releases next update. */
    o.flags59 &= ~4;
    aos_object0c_update(&o, 0, 0, true, false, 0, 0, 0);
    assert(o.flags59 & 8);
    assert(!o.claimed);
}

int main(void) {
    create_claims_and_marks();
    type0_writes_param_when_flag30();
    type1_sets_heavy();
    type2_arms_snapshot_and_pan();
    release_needs_bit2_clear();
    puts("aos_object0c: ok");
    return 0;
}
