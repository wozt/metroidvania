#include "../src/runtime/aos/game/aos_object10.h"
#include <assert.h>
#include <stdio.h>

static int owned_calls;
static bool owned_all(void *ctx, uint16_t item) {
    (void)ctx;
    owned_calls++;
    return item == 0x5B; /* only the first record's item owned */
}

int main(void) {
    AosObject10 o;

    /* Create: progress 0 covers every limit; item 0x5B is owned
     * so the scan falls to record 1 (0x5A). */
    owned_calls = 0;
    aos_object10_create(&o, 0, 0, 10, 20, owned_all, NULL);
    assert(o.state == 0 && o.item_id == 0x5A);
    assert(o.flags58 == 0x20 && o.flags59 == 0);
    assert(owned_calls == 2); /* record 0 owned -> record 1 free */

    /* Progress beyond all limits but the last still covered:
     * 0x5000 > 0x4650 skips record 1, <= 0x5460 keeps record 2. */
    aos_object10_create(&o, 0x5000, 2, 0, 0, NULL, NULL);
    assert(o.item_id == 0x45 && o.statue_state == 2);

    /* Everything covered/owned -> fallback item 1. */
    aos_object10_create(&o, 0x6000, 3, 0, 0, NULL, NULL);
    /* 0x6000 > 0x5460 skips all three records -> item 1. */
    assert(o.item_id == 1 && o.statue_state == 3);

    /* State 0 waits for the trigger; firing latches 0x80. */
    aos_object10_update(&o, false);
    assert(o.state == 0 && o.timer == 0);
    aos_object10_update(&o, true);
    assert(o.state == 1 && o.timer == 0x80);
    assert(o.flags58 == 0); /* 0x20's bit 5 is among the cleared */
    aos_object10_update(&o, false);
    assert(o.input_locked); /* SetPlayerInput runs in state 1 */

    /* State 1 counts down; the save sequence fires when + 0xD
     * reads zero (0x7F decrements left after the lock check,
     * then one more update). */
    for (int i = 0; i < 0x7F; ++i)
        aos_object10_update(&o, false);
    assert(o.state == 1 && o.timer == 0 && !o.save_committed);
    aos_object10_update(&o, false);
    assert(o.state == 2 && o.save_committed && o.stats_refreshed &&
           o.respawn_updated && o.grants == 1 && o.input_locked);

    /* State 2 marks + 0x64/+ 0x65 and enters inert state 3;
     * the lock releases one frame later when state 3 runs
     * without SetPlayerInput. */
    aos_object10_update(&o, false);
    assert(o.state == 3 && o.input_locked);
    aos_object10_update(&o, false);
    assert(!o.input_locked);
    aos_object10_update(&o, true);
    assert(o.state == 3 && o.grants == 1);

    puts("aos_object10: ok");
    return 0;
}
