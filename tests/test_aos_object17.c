#include "../src/runtime/aos/game/aos_object17.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    AosObject17 o;

    /* Create: player at y=100 picks band 0 (0x80+0x20=0xA0 >= 100). */
    aos_object17_create(&o, 100, 0, 0);
    assert(o.variant == 0 && !o.deleted);
    assert(o.pos_x == 0x78 && o.pos_y == 0x80);
    assert(o.state == 0 && o.child_count == 19);
    assert((o.flags59 & 0x04) && !(o.flags59 & 0x08));

    /* player_y=500 -> band 2 (0x180+0x20=0x1A0 < 500, 0x280+0x20 >= 500). */
    aos_object17_create(&o, 500, 0, 0);
    assert(o.variant == 2 && o.pos_y == 0x280);

    /* player_y=700 -> no band matches, variant stays 0. */
    aos_object17_create(&o, 700, 0, 0);
    assert(o.variant == 0);

    /* Room offsets fold into the position (u16 wrap). */
    aos_object17_create(&o, 100, 0x10, 0x20);
    assert(o.pos_x == 0x68 && o.pos_y == 0x60);

    /* State 0: probe miss keeps dwell at 0. */
    aos_object17_create(&o, 100, 0, 0);
    for (int i = 0; i < 10; ++i)
        aos_object17_update(&o, false, false, 0x80, 0, 0);
    assert(o.state == 0 && o.dwell == 0);

    /* Dwell needs flags & 0xC0: near but no direction -> no count. */
    for (int i = 0; i < 10; ++i)
        aos_object17_update(&o, false, true, 0x00, 0, 0);
    assert(o.dwell == 0);

    /* Gate blocked resets the dwell even with direction held. */
    aos_object17_update(&o, false, true, 0x80, 0, 0);
    aos_object17_update(&o, true, true, 0x80, 0, 0);
    assert(o.dwell == 0);

    /* Three dwell frames with 0x80: variant 0 -> 1, state 1,
     * timer 0x20, dir 0, input locked, speed cleared. */
    aos_object17_update(&o, false, true, 0x80, 0, 0);
    aos_object17_update(&o, false, true, 0x80, 0, 0);
    aos_object17_update(&o, false, true, 0x80, 0, 0);
    assert(o.state == 1 && o.variant == 1 && o.dir == 0);
    assert(o.timer == 0x20 && o.locked && o.speed == 0);

    /* State 1 counts 0x20 frames then plays song 0x116 -> state 2. */
    int steps = 0;
    while (o.state == 1 && steps < 0x30) {
        aos_object17_update(&o, false, false, 0, 0, 0);
        steps++;
    }
    assert(o.state == 2 && o.song == 0x116);

    /* State 2 ramps +0x4C by 0x1000/frame (cap check on old value)
     * and advances +0x44 by the OLD speed each frame. */
    aos_object17_update(&o, false, false, 0, 0, 0);
    assert(o.pos == 0 && o.speed == 0x1000);   /* pos += 0, v += 0x1000 */
    aos_object17_update(&o, false, false, 0, 0, 0);
    assert(o.pos == 0x1000 && o.speed == 0x2000);
    assert(o.locked);

    /* Force the pan to arrive: pos44 near target 0x180<<16. */
    o.pos = (0x180 << 16) - 0x3000;
    o.speed = 0x2000;
    /* pos probe = pos + speed + room_y16 -> 0x180<<16 + 0x1000 ->
     * posi 0x180 >= target -> finish. */
    aos_object17_update(&o, false, false, 0, 0, 0);
    assert(o.state == 3 && o.timer == 0x40);
    assert(o.song == 0x117 && o.song_stop == 0x116 && o.fade);
    assert(o.pos == (0x180 << 16) && o.speed == 0);

    /* State 3 counts down without locking, then state 0. */
    steps = 0;
    while (o.state == 3 && steps < 0x50) {
        aos_object17_update(&o, false, false, 0, 0, 0);
        steps++;
    }
    assert(o.state == 0);

    /* Pan-up path: dwell with 0x40 at variant 1 -> variant 0,
     * dir 1, timer 0x40. */
    o.state = 0; o.dwell = 0; o.variant = 1;
    aos_object17_update(&o, false, true, 0x40, 0, 0);
    aos_object17_update(&o, false, true, 0x40, 0, 0);
    aos_object17_update(&o, false, true, 0x40, 0, 0);
    assert(o.state == 1 && o.variant == 0 && o.dir == 1);
    assert(o.timer == 0x40);

    /* State 2 dir=1: speed ramps negative, arrives when
     * posi <= target (0x80). */
    o.state = 2; o.timer = 0; o.song = -1;
    o.pos = (0x80 << 16) + 0x4000;
    o.speed = -0x2000;
    /* pos += speed -> 0x80<<16+0x2000; speed -> -0x3000;
     * probe pos = pos+speed = 0x80<<16 - 0x1000 -> posi 0x7F <= 0x80. */
    aos_object17_update(&o, false, false, 0, 0, 0);
    assert(o.state == 3 && o.song == 0x117);

    /* +0x59 bit-2 clear: slot cleared, machine skipped. */
    aos_object17_create(&o, 100, 0, 0);
    o.flags59 = 0x00;
    aos_object17_update(&o, false, true, 0x80, 0, 0);
    assert(o.slot_clear && (o.flags59 & 0x08) && o.state == 0);

    puts("aos_object17: ok");
    return 0;
}
