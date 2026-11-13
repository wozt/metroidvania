#include "../src/runtime/aos/game/aos_object1f.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    AosObject1F o;
    bool deleted;

    aos_object1f_create(&o, false, &deleted);
    assert(deleted && o.deleted);

    /* Create counts the ten children. */
    aos_object1f_create(&o, true, &deleted);
    assert(!deleted && !o.deleted);
    assert(o.child_count == 10 && o.state == 0);

    /* State 0 waits for the bundled gate. */
    aos_object1f_update(&o, false);
    assert(o.state == 0 && o.sound == -1);
    aos_object1f_update(&o, true);
    assert(o.state == 1 && o.sound == 0);
    assert(o.player_flags_5a == 2 && o.display_4a == 0x10);
    assert(o.frames_left == 0x80);

    /* State 1: +0xD >= 0 path increments and writes the
     * blended +0x4A; +0x14 counts down from 0x80. */
    aos_object1f_update(&o, true);
    assert(o.global_42c_bit0 && o.input_lock);
    assert(o.timer == 1);
    /* shift = 1>>3 = 0 -> +0x4A = 0x10 */
    assert(o.display_4a == 0x10);
    assert(o.frames_left == 0x7F);

    /* Drive +0xD to 8 -> shift 1 -> +0x4A = 0x108 | 0xF =
     * 0x10F... (1<<8)|(0x10-1) = 0x10F. */
    o.timer = 8;
    aos_object1f_update(&o, true);
    assert(o.display_4a == 0x10F);

    /* Negative +0xD: player flag 0x20, +0x5A cleared, timer
     * stays, +0x4A still computed from the u8 read. */
    o.timer = -1;
    aos_object1f_update(&o, true);
    assert(o.player_flag20 && o.player_flags_5a == 0);
    assert(o.timer == -1);
    /* d u8 = 0xFF -> shift 0x1F -> +0x4A =
     * (0x1F<<8)|(0x10-0x1F) = 0xFFF1 after the strh. */
    assert(o.display_4a == (int)0xFFF1);

    /* +0x14 wrap: old 0 -> seq_end + state 2 + 0xFFFF. */
    o.timer = 0; o.frames_left = 0;
    aos_object1f_update(&o, true);
    assert(o.seq_end && o.state == 2 && o.frames_left == 0xFFFF);

    /* State 2 inert. */
    aos_object1f_update(&o, true);
    assert(o.state == 2 && o.display_4a == -1);

    puts("aos_object1f: ok");
    return 0;
}
