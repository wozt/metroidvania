#include "../src/runtime/aos/game/aos_object0e.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    AosObject0e o;
    bool deleted;

    /* Create: param0 selects the config record and its resource. */
    aos_object0e_create(&o, 0, 0, &deleted);
    assert(!deleted);
    assert(o.variant == 0);
    assert(o.state == 0 && o.timer == 0 && !o.finished);
    assert(o.song == -1);

    aos_object0e_create(&o, 7, 3, &deleted);
    assert(!deleted);
    assert(o.variant == 7);          /* 0x085262BC[7].b0 = 7 */

    aos_object0e_create(&o, 13, 5, &deleted);
    assert(!deleted);
    assert(o.variant == 13);

    /* Out-of-range param0 clamps to the last config record. */
    aos_object0e_create(&o, 200, 0, &deleted);
    assert(!deleted);
    assert(o.variant == 13);

    /* Probe miss: +0x58 bit 0x20, update skipped. */
    aos_object0e_create(&o, 0, 0, &deleted);
    o.flags58 = 0;
    aos_object0e_update(&o, false, true, 0x10000, 0x20000);
    assert(o.flags58 & 0x20);
    assert(o.state == 0);
    assert(o.fx == 0x10000 && o.fy == 0x20000);

    /* State 0 clears bits 0 and 5 while near. */
    o.flags58 = 0xFF;
    aos_object0e_update(&o, true, false, 0, 0);
    assert(!(o.flags58 & 0x21));
    assert(o.state == 0 && o.song == -1);

    /* Hit: content children, song 0xA2, timer 0x28, state 1. */
    o.flags58 = 0;
    aos_object0e_update(&o, true, true, 0x10000, 0x20000);
    assert(o.state == 1);
    assert(o.song == 0xA2);
    assert(o.timer == 0x28);
    assert(o.content_fx == 3);       /* cfg[0] byte 3 */
    assert(o.flags58 & 0x20);

    /* Variant 13 releases 5 children. */
    aos_object0e_create(&o, 13, 0, &deleted);
    aos_object0e_update(&o, true, true, 0, 0);
    assert(o.content_fx == 5);

    /* State 1: gore burst only on every 4th frame while timer > 0x1E. */
    aos_object0e_create(&o, 0, 0, &deleted);
    aos_object0e_update(&o, true, true, 0, 0);
    assert(o.timer == 0x28);
    int gore = 0;
    for (int i = 0; i < 0x40 && !o.finished; i++) {
        int before = o.gore_fx;
        aos_object0e_update(&o, true, false, 0, 0);
        gore += o.gore_fx - before;
    }
    /* Timers checked before each decrement: 0x28,0x27,...,0x01 then
     * 0x00 -> 0xFF wrap ends it. Bursts at 0x28,0x24,0x20 -> 3x4. */
    assert(gore == 12);
    assert(o.finished);              /* u8 wrap marks +0x59 bit 3 */

    /* No further updates matter once finished; state stays 1. */
    assert(o.state == 1);

    puts("aos_object0e: ok");
    return 0;
}
