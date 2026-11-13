#include "../src/runtime/aos/game/aos_object1f.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

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
    assert((int8_t)o.timer == -1);
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

    /* sub_08055748 think machine. */
    memset(&o, 0, sizeof o);
    o.state = 0;
    /* case 0, flag set -> flash, +0x14 = 0x74, -> 5. */
    aos_object1f_think(&o, true, false, false, false, false);
    assert(o.flash_2000 && o.frames_left == 0x74 && o.state == 5);
    /* case 5 -> 6, player +0x5A &= ~0x79 window. */
    aos_object1f_think(&o, false, false, false, false, false);
    assert(o.state == 6 && o.player_5a_clr79);
    /* case 6 counts to 0x40 then advances to 7. */
    o.timer = 0x3F;
    aos_object1f_think(&o, false, false, false, false, false);
    assert(o.state == 6);          /* old 0x3F is not > 0x3F */
    aos_object1f_think(&o, false, false, false, false, false);
    assert(o.state == 7);
    /* case 7 idle; case 8 counts +0x14 down every 3rd. */
    o.state = 8; o.timer = 0; o.frames_left = 0x30;
    aos_object1f_think(&o, false, false, false, false, false);
    assert(o.flags_a074_2 && o.frames_left == 0x2F);
    /* 0x24 decrements to 0x23 -> immediate loop to state 0. */
    o.timer = 0; o.frames_left = 0x24;
    aos_object1f_think(&o, false, false, false, false, false);
    assert(o.state == 0 && o.frames_left == 0x24);

    /* case 1 substate 0 -> probe gate. */
    memset(&o, 0, sizeof o);
    o.state = 1; o.substate = 0;
    aos_object1f_think(&o, true, true, false, false, false);
    assert(o.substate == 1 && o.timer == 3);
    /* substate 1, think_done -> song 0xF3, -> 2. */
    aos_object1f_think(&o, false, false, true, false, false);
    assert(o.think_song == 0xF3 && o.state == 2 && o.substate == 0);
    /* case 2 s8 -1 -> back to 0. */
    aos_object1f_think(&o, false, false, false, true, false);
    assert(o.state == 0);
    /* case 0 without flag -> 1 with A074 bit. */
    aos_object1f_think(&o, false, false, false, false, false);
    assert(o.state == 1 && o.flags_a074_2);

    puts("aos_object1f: ok");
    return 0;
}
