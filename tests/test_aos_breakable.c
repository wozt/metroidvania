/* SPDX-License-Identifier: GPL-3.0-only */
/* Unit coverage for the kind-3 breakable port: sub_08046080 create,
 * sub_08045EE0's probe/state machine and sub_08045F9C case-0 drop
 * latching (heart vs money gate). */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "aos/game/aos_breakable.h"

int main(void) {
    AosBreakable b;

    /* sub_08046080: param0/param1 land on + 0x30/+ 0x32, state 0. */
    aos_breakable_create(&b, 5, 3);
    assert(b.param0 == 5 && b.param1 == 3);
    assert(b.state == 0 && b.song == -1 && !b.finished);

    /* Probe miss: + 0x58 bit 0x20, no state change. */
    aos_breakable_update(&b, false, true, false, true);
    assert(b.flags58 & 0x20);
    assert(b.state == 0 && b.song == -1);

    /* Hit with non-full MP: sub_08043C10 heart, big = param1 != 0. */
    aos_breakable_update(&b, true, true, false, false);
    assert(b.song == 0xA2 && b.state == 1 && b.timer == 0x28);
    assert(b.drop_kind == AOS_BREAKABLE_DROP_HEART && b.drop_big);
    assert(b.flags58 & 0x20); /* bit 5 re-set by the successful hit */

    /* State 1: gore every fourth frame while timer > 0x1E, then the
     * u8 timer wraps to 0xFF and + 0x59 bit 3 (finished) sets. */
    int gore = 0;
    for (int step = 0; step < 0x29 && !b.finished; ++step) {
        aos_breakable_update(&b, true, false, false, false);
        gore += b.gore_fx;
    }
    assert(b.finished);
    /* timers 0x28..0x1F emit on &3==0: 0x28,0x24,0x20 -> 3 bursts */
    assert(gore == 3);

    /* Second prop, param1 == 0: the heart is the small variant. */
    aos_breakable_create(&b, 0, 0);
    aos_breakable_update(&b, true, true, false, false);
    assert(b.drop_kind == AOS_BREAKABLE_DROP_HEART && !b.drop_big);

    /* Full MP on Soma: sub_08043F70 money, variant needs the
     * (RandomNumberGenerator & 1) + param1 roll. */
    aos_breakable_create(&b, 2, 1);
    aos_breakable_update(&b, true, true, false, true);
    assert(b.drop_kind == AOS_BREAKABLE_DROP_MONEY && b.drop_roll);

    /* No hit: nothing latches. */
    aos_breakable_create(&b, 1, 1);
    aos_breakable_update(&b, true, false, false, true);
    assert(b.drop_kind == AOS_BREAKABLE_DROP_NONE && b.state == 0);

    puts("aos_breakable: ok");
    return 0;
}
