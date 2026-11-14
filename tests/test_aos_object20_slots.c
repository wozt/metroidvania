/* Unit tests for src/runtime/aos/game/aos_object20_slots.c —
 * the Object20 slot-variant parity port. Create dispatch
 * follows the native 0x08526658 indices (param0 0, 1, 2, 11,
 * 12, 13, 14, 15, 16, 17, 18); update dispatch covers param0
 * 0, 1, 2, 11, 12, 13, 14, 15, 16, 17, 18. */
#include "../src/runtime/aos/game/aos_object20_slots.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static AosObject20SlotsIn base_in(void) {
    AosObject20SlotsIn in;
    memset(&in, 0, sizeof in);
    in.flag1327a = 1;           /* pass the shared create gate */
    in.keep_5b754 = true;
    in.a094_6 = 0x40;
    in.a094_a = 0x30;
    in.player_scr42 = 0x100;
    in.player_pos40 = 0;
    in.player_w4c = 0;
    in.player_flags58 = 0;
    in.ret_roomx = 0;
    return in;
}

int main(void) {
    AosObject20Slots s;
    AosObject20SlotsOut out;
    AosObject20SlotsIn in;

    /* ---- param0 0: create sub_0805BF24 (+0x42C|=1, window
     * 0xC); update sub_0805BCB4 state machine. ---- */
    memset(&s, 0, sizeof s);
    in = base_in();
    assert(aos_object20_slots_create(&s, 0, &in, &out));
    assert(out.set_42c & 1);
    assert(out.window_text == 0xC);
    assert(out.set_42c & 0x200);
    assert(out.call_6666c);

    /* update state 0: player +0x42 (0x100) clamps to 0x4F,
     * player +0x58 &= ~0x41, own +0x58 |= 0x40, window 0xC,
     * state -> 1, returns 0. */
    memset(&out, 0, sizeof out);
    assert(aos_object20_slots_update(&s, 0, &in, &out) == 0);
    assert(out.player_scr42_set && out.player_scr42_val == 0x4F);
    assert(out.player58_andnot_41);
    assert(s.flags58 & 0x40);
    assert(out.window_text == 0xC);
    assert(s.state == 1);

    /* update state 1, +0xB == 0: no poll (+0xB != 3 -> poll
     * runs; text_done false keeps state 1). case0 (+0xB==1)
     * does not apply; +0xB==0 falls through the jump table. */
    s.sub_b = 0;
    memset(&out, 0, sizeof out);
    assert(aos_object20_slots_update(&s, 0, &in, &out) == 0);
    assert(out.window_poll);
    assert(s.state == 1);

    /* +0xB == 1 with text done: +0x42C bit 1 cleared, state
     * forced to 0x80, case0 clears own +0x58 & ~0x41. */
    s.flags58 |= 0x41;
    s.sub_b = 1;
    in.text_done = true;
    memset(&out, 0, sizeof out);
    assert(aos_object20_slots_update(&s, 0, &in, &out) == 0);
    assert(out.clear_42c & 2);
    assert(s.state == 0x80);
    assert((s.flags58 & 0x41) == 0);

    /* state 0x80: sub_0805B66C(e,1). */
    memset(&out, 0, sizeof out);
    assert(aos_object20_slots_update(&s, 0, &in, &out) == 0);
    assert(out.release_arg == 1);

    /* +0xB == 3 skips the poll entirely (no window_poll). */
    memset(&s, 0, sizeof s);
    s.state = 1;
    s.sub_b = 3;
    s.flags59 = 0;              /* +0x59 bit0 clear -> no advance */
    in.text_done = true;
    memset(&out, 0, sizeof out);
    assert(aos_object20_slots_update(&s, 0, &in, &out) == 0);
    assert(!out.window_poll);
    assert(s.state == 1);
    assert(s.sub_b == 3);

    /* +0xB == 3 with +0x59 bit0: F2C8(3,2,0), +0xB -> 4,
     * +0x48 = 0xFFFE0000, +0x50 = 0x800. */
    s.flags59 = 1;
    memset(&out, 0, sizeof out);
    assert(aos_object20_slots_update(&s, 0, &in, &out) == 0);
    assert(out.anim_count == 1);
    assert(out.anim_a[0] == 3 && out.anim_b[0] == 2 &&
           out.anim_c[0] == 0);
    assert(s.sub_b == 4);
    assert(s.vel48 == (int32_t)0xFFFE0000);
    assert(s.vel50 == 0x800);

    /* +0xB == 4 case3: +0x6E == 1 -> +0x6C |= 2; position
     * integrates; vel48 += vel50 stays <= 0xFFFE8000 so no
     * return flag and vel48 <= 0xFFFFF000 so +0xB does not
     * advance... vel48 = 0xFFFE0000 + 0x800 = 0xFFFE0800 which
     * is <= 0xFFFFF000 -> +0xB stays 4. */
    in.text_done = false;
    s.field6e = 1;
    s.pos40 = 0;
    memset(&out, 0, sizeof out);
    int r = aos_object20_slots_update(&s, 0, &in, &out);
    assert(r == 0);
    assert(s.field6c & 2);
    assert(s.pos40 == (int32_t)0xFFFE0000);
    assert(s.vel48 == (int32_t)0xFFFE0800);
    assert(s.sub_b == 4);

    /* Drive vel48 positive past both thresholds: +0xB++ and
     * r6 = 0x200 when vel48 > 0xFFFE8000. */
    s.vel48 = 0x100;
    s.vel50 = 0x10;
    memset(&out, 0, sizeof out);
    r = aos_object20_slots_update(&s, 0, &in, &out);
    assert(r == 0x200);
    assert(s.sub_b == 5);

    /* +0xB == 5 case4: F2C8(2,2,0), no +0xB advance. */
    memset(&out, 0, sizeof out);
    r = aos_object20_slots_update(&s, 0, &in, &out);
    assert(r == 0);             /* r6 resets per call */
    assert(out.anim_a[0] == 2);
    assert(s.sub_b == 5);

    /* +0xB == 6 case5: pos40 += 0x40000, pos44 += -0x8000;
     * scr42 (0x40 default? set explicitly) > 0x100 -> +0xB = 7,
     * returns 0x10. */
    memset(&s, 0, sizeof s);
    s.state = 1;
    s.sub_b = 6;
    s.scr42 = 0x101;
    s.pos40 = 0;
    s.pos44 = 0;
    memset(&out, 0, sizeof out);
    r = aos_object20_slots_update(&s, 0, &in, &out);
    assert(r == 0x10);
    assert(s.pos40 == 0x40000);
    assert(s.pos44 == (int32_t)0xFFFF8000);
    assert(s.sub_b == 7);

    /* scr42 <= 0x100: +0xB stays 6, still returns 0x10. */
    s.sub_b = 6;
    s.scr42 = 0x100;
    memset(&out, 0, sizeof out);
    r = aos_object20_slots_update(&s, 0, &in, &out);
    assert(r == 0x10);
    assert(s.sub_b == 6);

    /* ---- param0 1: create sub_0805BF48 (bx lr — gates+tail
     * only); update sub_0805BE64 state chain. ---- */
    memset(&s, 0, sizeof s);
    in = base_in();
    assert(aos_object20_slots_create(&s, 1, &in, &out));
    assert(!(out.set_42c & 1));         /* variant adds nothing */
    assert(out.set_42c & 0x200);        /* shared tail still ran */

    /* update state 0, roomx in (0x80, 0x11F]: +0x42C|=1, two
     * probe calls, state -> 1. */
    in.ret_roomx = 0x100;
    memset(&out, 0, sizeof out);
    assert(aos_object20_slots_update(&s, 1, &in, &out) == 0);
    assert(out.roomx_calls == 2);
    assert(out.set_42c & 1);
    assert(s.state == 1);

    /* roomx <= 0x80: one call, no state change. */
    memset(&s, 0, sizeof s);
    in.ret_roomx = 0x80;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 1, &in, &out);
    assert(out.roomx_calls == 1);
    assert(s.state == 0);
    assert(!(out.set_42c & 1));

    /* state 1 -> window 0xC, state 2; state 2 poll done ->
     * release + state 3; state 3 falls through; force 0x80 ->
     * 2CB0 &= 0xFEFF -> 0x81 -> release + |= 0x100. */
    s.state = 1;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 1, &in, &out);
    assert(out.window_text == 0xC);
    assert(s.state == 2);
    in.text_done = true;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 1, &in, &out);
    assert(out.release_arg == 1);
    assert(s.state == 3);
    s.state = 0x80;
    in.text_done = false;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 1, &in, &out);
    assert(out.unk2cb0_feff);
    assert(s.state == 0x81);
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 1, &in, &out);
    assert(out.release_arg == 1);
    assert(out.unk2cb0_or100);

    /* ---- param0 2: both sides bx lr — shell only. ---- */
    memset(&s, 0, sizeof s);
    in = base_in();
    assert(aos_object20_slots_create(&s, 2, &in, &out));
    assert(!(out.set_42c & 1));
    memset(&s, 0, sizeof s);
    s.state = 5;                    /* arbitrary — variant noop */
    memset(&out, 0, sizeof out);
    assert(aos_object20_slots_update(&s, 2, &in, &out) == 0);
    assert(out.c5a8 == 1);           /* flag42c bit0 clear */

    /* ---- param0 11: create sub_08060B98 / update sub_08060C20.
     * ---- */
    /* create gate: +0x42C bit 0x10000 -> +0x59 |= 8 delete. */
    memset(&s, 0, sizeof s);
    in = base_in();
    in.flag42c = 0x10000;
    in.flag1327a = 0;               /* gate via 0x42C bit?
                                     * 0x10000 is not 0x200000 —
                                     * use flag1327a path off:
                                     * set flag1327a = 0 and
                                     * flag42c 0x200000|0x10000 */
    in.flag42c = 0x210000; /* 0x200000 gate | 0x10000 variant */
    assert(!aos_object20_slots_create(&s, 11, &in, &out));
    assert(s.flags59 & 0x8);
    assert(out.deleted);

    /* create success: record idx 4, +0x5C = 0x11, +0x59 |= 2,
     * F2C8(0,0,1), facing mirror from player +0x58 bit 0x40. */
    memset(&s, 0, sizeof s);
    in = base_in();
    in.player_flags58 = 0x40;
    s.flags58 = 0x41;               /* pre-seeded entity state */
    assert(aos_object20_slots_create(&s, 11, &in, &out));
    assert(out.rec_5b754 == 4);
    assert(s.anim5c == 0x11);
    assert(s.flags59 & 2);
    assert(out.anim_a[0] == 0 && out.anim_b[0] == 0 &&
           out.anim_c[0] == 1);
    assert(out.anim_step_count == 1);
    assert(s.flags58 == 0x40);      /* (0x41 & ~0x41) | 0x40 */
    assert(out.set_42c & 1);

    /* create with dead probe -> deleted. */
    memset(&s, 0, sizeof s);
    in = base_in();
    in.keep_5b754 = false;
    assert(!aos_object20_slots_create(&s, 11, &in, &out));
    assert(out.deleted);
    assert(out.rec_5b754 == 4);

    /* update state 0: b878 0x1040, window 0x16, +0x417 = 1,
     * +0xD = 0x90, +0x42C |= 1, state 1, F17C runs. */
    memset(&s, 0, sizeof s);
    in = base_in();
    memset(&out, 0, sizeof out);
    assert(aos_object20_slots_update(&s, 11, &in, &out) == 0);
    assert(out.b878 == 0x1040);
    assert(out.window_text == 0x16);
    assert(out.write_417 && out.write_417_val == 1);
    assert(s.timer_d == 0x90);
    assert(s.state == 1);
    assert(out.anim_step_count == 1);
    assert(out.set_42c & 1);

    /* update state 1 +0xB==0: +0xD countdown; wrap -> +0xB++,
     * +0x417 = 0. */
    s.timer_d = 1;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 11, &in, &out);
    assert(s.timer_d == 0);
    assert(s.sub_b == 0);
    s.timer_d = 0;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 11, &in, &out);
    assert(s.timer_d == 0xFF);
    assert(s.sub_b == 1);
    assert(out.write_417 && out.write_417_val == 0);

    /* +0xB==2: +0x417 = 1 then roomx probe; > 0xB6 returns
     * 0x20 without advancing. */
    s.sub_b = 2;
    in.ret_roomx = 0xC0;
    memset(&out, 0, sizeof out);
    int r11 = aos_object20_slots_update(&s, 11, &in, &out);
    assert(r11 == 0x20);
    assert(s.sub_b == 2);
    in.ret_roomx = 0xB6;
    memset(&out, 0, sizeof out);
    r11 = aos_object20_slots_update(&s, 11, &in, &out);
    assert(r11 == 0);
    assert(s.sub_b == 3);

    /* +0xB==3: +0x417 = 0, +0xB -> 4. */
    s.sub_b = 3;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 11, &in, &out);
    assert(out.write_417 && out.write_417_val == 0);
    assert(s.sub_b == 4);

    /* poll done -> state 0x80; next call jingle 0x28 +
     * release(1), no F17C. */
    in.text_done = true;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 11, &in, &out);
    assert(s.state == 0x80);
    in.text_done = false;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 11, &in, &out);
    assert(out.jingle == 0x28);
    assert(out.release_arg == 1);
    assert(out.anim_step_count == 0);

    /* ---- param0 12: create sub_08060D48 / update sub_08060DC4.
     * ---- */
    memset(&s, 0, sizeof s);
    in = base_in();
    in.a094_a = 0x30;
    assert(aos_object20_slots_create(&s, 12, &in, &out));
    assert(out.rec_5b754 == 1);
    assert(out.anim_a[0] == 1 && out.anim_b[0] == 0 &&
           out.anim_c[0] == 1);
    assert(s.scr42 == (int16_t)0xFFF0);
    assert(s.scr46 == (int16_t)(0xA0 - 0x30));
    assert(s.vel48 == 0x4000);
    assert(s.flags58 & 0x40);
    assert(out.set_42c & 1);

    /* create with dead probe: still +0x42C |= 1, entity
     * deleted, no anim/placement. */
    memset(&s, 0, sizeof s);
    in = base_in();
    in.keep_5b754 = false;
    assert(!aos_object20_slots_create(&s, 12, &in, &out));
    assert(out.deleted);
    assert(out.set_42c & 1);
    assert(out.anim_count == 0);
    assert(s.scr42 == 0);

    /* update state 0: +0x42C|=1, window 0x19, state 1. */
    memset(&s, 0, sizeof s);
    in = base_in();
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 12, &in, &out);
    assert(out.set_42c & 1);
    assert(out.window_text == 0x19);
    assert(s.state == 1);

    /* state 1 +0xB==1, scr42 <= 7: +0xC wrap writes +0x417=1,
     * pos40 += vel48. */
    s.sub_b = 1;
    s.scr42 = 0;
    s.vel48 = 0x100;
    s.pos40 = 0;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 12, &in, &out);
    assert(s.sub_c == 1);
    assert(out.write_417 && out.write_417_val == 1);
    assert(s.pos40 == 0x100);
    assert(s.sub_b == 1);

    /* scr42 > 7 path: anim gate (+0x6D!=4 -> F2C8(4,0,1)),
     * player +0x58 |= 0x40, +0x417 = 0, +0xB -> 2, +0xC = 0. */
    s.scr42 = 8;
    s.anim6d = 0;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 12, &in, &out);
    assert(out.player58_or_40);
    assert(out.write_417 && out.write_417_val == 0);
    assert(s.sub_b == 2);
    assert(s.sub_c == 0);
    assert(out.anim_a[0] == 4);

    /* +0xB==4 +0xC wrap: +0xD = 0x40, +0x417 = 1, ret 0x40. */
    memset(&s, 0, sizeof s);
    s.state = 1;
    s.sub_b = 4;
    memset(&out, 0, sizeof out);
    int r12 = aos_object20_slots_update(&s, 12, &in, &out);
    assert(r12 == 0x40);
    assert(s.timer_d == 0x40);
    assert(out.write_417 && out.write_417_val == 1);

    /* +0xD counts down while +0xC != 1; at +0xD == 0 the next
     * call writes +0x417 = 0. */
    s.sub_c = 2;
    s.timer_d = 2;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 12, &in, &out);
    assert(s.timer_d == 1);
    assert(!out.write_417);
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 12, &in, &out);
    assert(s.timer_d == 0);
    assert(!out.write_417);
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 12, &in, &out);
    assert(out.write_417 && out.write_417_val == 0);

    /* poll done -> 0x80; window_close + +0x42C|=1, no F17C. */
    in.text_done = true;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 12, &in, &out);
    assert(s.state == 0x80);
    in.text_done = false;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 12, &in, &out);
    assert(out.window_close);
    assert(out.set_42c & 1);
    assert(out.anim_step_count == 0);

    /* ---- param0 13/14: +0x42C|=1 creates; window updates. ---- */
    for (unsigned p = 13; p <= 14; ++p) {
        memset(&s, 0, sizeof s);
        in = base_in();
        assert(aos_object20_slots_create(&s, (uint8_t)p, &in,
                                         &out));
        assert(out.set_42c & 1);
        memset(&s, 0, sizeof s);
        memset(&out, 0, sizeof out);
        aos_object20_slots_update(&s, (uint8_t)p, &in, &out);
        assert(out.window_text == (p == 13 ? 0x1A : 0x1B));
        assert(s.state == 1);
        in.text_done = true;
        memset(&out, 0, sizeof out);
        aos_object20_slots_update(&s, (uint8_t)p, &in, &out);
        if (p == 13) {
            assert(out.release_arg == 1);
        } else {
            assert(out.window_close);
            assert(s.flags59 & 0x8);
        }
    }

    /* ---- param0 15: create sub_08060F24 / update sub_08060F98.
     * ---- */
    memset(&s, 0, sizeof s);
    in = base_in();
    in.a094_6 = 0x40;
    in.a094_a = 0x30;
    assert(aos_object20_slots_create(&s, 15, &in, &out));
    assert(out.rec_5b754 == 1);
    assert(s.scr42 == (int16_t)(0u - 0x40));   /* -0x40 */
    assert(s.scr46 == (int16_t)(0xA0 - 0x30));
    assert(s.flags58 & 0x60);
    assert(out.set_42c & 1);

    /* update[15] sub_08060F98: state 0 opens window 0x1C,
     * writes +0x417 = 1 and advances; returns 0. */
    memset(&s, 0, sizeof s);
    in = base_in();
    memset(&out, 0, sizeof out);
    assert(aos_object20_slots_update(&s, 15, &in, &out) == 0);
    assert(!out.unported);
    assert(out.window_text == 0x1C);
    assert(out.write_417 && out.write_417_val == 1);
    assert(s.state == 1);
    assert(out.anim_step_count == 1);    /* +0xA >= 0 -> F17C */

    /* state 1, +0xB == 1 (native noop): poll runs, no flag,
     * F17C tail. */
    s.sub_b = 1;
    memset(&out, 0, sizeof out);
    assert(aos_object20_slots_update(&s, 15, &in, &out) == 0);
    assert(out.window_poll);
    assert(out.return_code == 0);
    assert(s.state == 1);

    /* text done forces +0xA = 0x80 but the +0xB case still runs;
     * with +0xB == 10 (noop) nothing else fires and the F17C
     * tail is skipped (state 0x80 is negative). */
    s.sub_b = 10;
    in.text_done = true;
    memset(&out, 0, sizeof out);
    assert(aos_object20_slots_update(&s, 15, &in, &out) == 0);
    assert(s.state == 0x80);
    assert(out.anim_step_count == 0);
    in.text_done = false;

    /* case 0 (_08061064): input busy (& 0x1E) -> no advance. */
    memset(&s, 0, sizeof s);
    s.state = 1;
    s.sub_b = 0;
    in.probe_212c8 = 0x1E;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 15, &in, &out);
    assert(s.sub_b == 0);
    assert(!out.rec_53c_write);

    /* input clear: record write — rec_524 < player_pos40 sets
     * bit 6, bits 0x41 cleared; probe_61824 low byte nonzero
     * -> +0xB = 2. */
    in.probe_212c8 = 0;
    in.rec_524 = 0;
    in.player_pos40 = 0x100;
    in.rec_53c = 0x43;
    in.probe_61824 = 1;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 15, &in, &out);
    assert(out.rec_53c_write);
    assert(out.rec_53c_val == 0x43);    /* (0x43 & ~0x40) | 0x40 */
    assert(out.probe61824_calls == 1);
    assert(s.sub_b == 2);

    /* rec_524 >= player_pos40 -> bit 6 clear; probe low byte 0
     * -> +0xB = 3 via _0806110C. */
    s.sub_b = 0;
    in.rec_524 = 0x200;
    in.probe_61824 = 0;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 15, &in, &out);
    assert(out.rec_53c_val == 0x03);    /* 0x43 & ~0x40 */
    assert(s.sub_b == 3);

    /* case 2 (_080610D0): probe low byte == 1 -> flag 0x10,
     * or 0x20 when player +0x42 > 0xAA; != 1 -> +0xB = 3. */
    s.sub_b = 2;
    in.player_scr42 = 0x50;
    in.probe_61824 = 1;
    memset(&out, 0, sizeof out);
    int r15 = aos_object20_slots_update(&s, 15, &in, &out);
    assert(r15 == 0x10);
    assert(s.sub_b == 2);
    in.player_scr42 = 0xBB;
    memset(&out, 0, sizeof out);
    r15 = aos_object20_slots_update(&s, 15, &in, &out);
    assert(r15 == 0x20);
    in.probe_61824 = 2;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 15, &in, &out);
    assert(s.sub_b == 3);

    /* case 3 (_08061112): first entry arms +0xD = 0x60, +0xC =
     * 1 and sub_08013EEC(0x16C); +0xD != 0 holds. With +0xD
     * externally driven to 0 the case writes +0x417 = 0,
     * +0xB++ and +0xC = +0xD (0). */
    s.sub_b = 3;
    s.sub_c = 0;
    s.timer_d = 0;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 15, &in, &out);
    assert(s.timer_d == 0x60);
    assert(s.sub_c == 1);
    assert(out.sfx_13eec == 0x16C);
    assert(s.sub_b == 3);               /* +0xD != 0: held */
    s.timer_d = 0;                      /* external countdown */
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 15, &in, &out);
    assert(out.write_417 && out.write_417_val == 0);
    assert(s.sub_b == 4);
    assert(s.sub_c == 0);

    /* case 4 (_08061144): +0xC == 0 init fires the fade pair +
     * PlaySong(0x170) once; every call decrements +0xD and
     * rewrites player +0x5A on old +0xD & 3, then
     * sub_0801D028(0x52,0,0,0). */
    memset(&s, 0, sizeof s);
    s.state = 1;
    s.sub_b = 4;
    s.timer_d = 4;                      /* old & 3 == 0 */
    in.player_byte5a = 0x79;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 15, &in, &out);
    assert(s.sub_c == 1);
    assert(s.timer_d == 3);
    assert(out.playsong_count == 1 && out.playsong[0] == 0x170);
    assert(out.fade_count == 2);
    assert(out.fade_fn[0] == 0 && out.fade_fn[1] == 1);
    assert(out.fade_args[0][3] == 0 && out.fade_args[0][4] == 0x10000 &&
           out.fade_args[0][5] == 0x20);
    assert(out.fade_args[1][3] == 0x4000 && out.fade_args[1][4] == 0x10000 &&
           out.fade_args[1][5] == 0x10);
    assert(out.player5a_count == 1);
    assert(out.player5a_vals[0] == 0x01);   /* 0x79 & 0x87 */
    assert(out.call_1d028);
    /* init does not refire; old +0xD & 3 != 0 sets bit 0x20. */
    s.timer_d = 3;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 15, &in, &out);
    assert(out.playsong_count == 0);
    assert(out.player5a_vals[0] == 0x21);   /* (0x79&0x87)|0x20 */

    /* case 5 (_080611D8): divisor = tbl14*3/2; a zero remainder
     * fires song + fades and advances +0x14 (wraps at 0xA). */
    memset(&s, 0, sizeof s);
    s.state = 1;
    s.sub_b = 5;
    s.timer_d = 1;                      /* new +0xD = 0 */
    s.field14 = 9;
    in.tbl14 = 4;                       /* div = 6; 0 % 6 == 0 */
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 15, &in, &out);
    assert(s.timer_d == 0);
    assert(s.field14 == 0);             /* 9 + 1 -> wrap */
    assert(out.playsong_count == 1);
    assert(out.call_1d028);
    /* nonzero remainder: no song/fades, +0x14 unchanged. */
    s.sub_b = 5;
    s.timer_d = 2;                      /* new +0xD = 1; 1 % 6 != 0 */
    s.field14 = 3;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 15, &in, &out);
    assert(s.field14 == 3);
    assert(out.playsong_count == 0);
    assert(out.fade_count == 0);

    /* case 6 (_08061244): divisor = tbl14 raw; old +0xD & 2
     * (not & 3) picks the +0x5A 0x20 bit. */
    memset(&s, 0, sizeof s);
    s.state = 1;
    s.sub_b = 6;
    s.timer_d = 2;                      /* old & 2 == 2, new = 1 */
    s.field14 = 0;
    in.tbl14 = 1;                       /* 1 % 1 == 0 -> hit */
    in.player_byte5a = 0xFF;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 15, &in, &out);
    assert(s.timer_d == 1);
    assert(s.field14 == 1);
    assert(out.player5a_vals[0] == 0xA7);   /* (0xFF&0x87)|0x20 */
    assert(out.playsong_count == 1);
    assert(out.call_1d028);

    /* case 7 (_080612FC): +0xC == 0 init — +0x417 = 1, +0xD =
     * 0x87, +0xC = 1, three PlaySong args and two fades, then
     * the +0xC==1 body runs the same call. */
    memset(&s, 0, sizeof s);
    s.state = 1;
    s.sub_b = 7;
    in.player_byte5a = 0;
    memset(&out, 0, sizeof out);
    int r7v = aos_object20_slots_update(&s, 15, &in, &out);
    assert(r7v == 0x80);                /* _08061486 */
    assert(out.write_417 && out.write_417_val == 1);
    assert(s.timer_d == 0x86);          /* 0x87 - 1 same call */
    assert(s.sub_c == 1);
    assert(out.playsong_count == 3);
    assert(out.playsong[0] == 0x816C && out.playsong[1] == 0x197 &&
           out.playsong[2] == 0x169);
    assert(out.fade_count == 2);
    assert(out.fade_args[0][3] == 0x10000 && out.fade_args[0][4] == 0x50 &&
           out.fade_args[0][5] == 0x7F);
    /* old +0xD 0x87 & 2 -> +0x5A = (0 & ~0x79) | 0x20 = 0x20. */
    assert(out.player5a_count == 1);
    assert(out.player5a_vals[0] == 0x20);

    /* +0xC == 1 steady state: +0xD 1 -> 0, no wrap; ret 0x80. */
    s.timer_d = 1;
    memset(&out, 0, sizeof out);
    r7v = aos_object20_slots_update(&s, 15, &in, &out);
    assert(r7v == 0x80);
    assert(s.timer_d == 0);
    assert(s.sub_c == 1);
    assert(out.player5a_count == 1);    /* 0 & 2 == 0 -> clear */
    assert(out.player5a_vals[0] == 0x00);

    /* +0xD == 0 -> wrap: sub_08000EA8(0x40), second fade pair,
     * +0xD = 0xFF, +0xC -> 2, final +0x5A clear (2 stores). */
    s.timer_d = 0;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 15, &in, &out);
    assert(out.call_00ea8 == 0x40);
    assert(out.fade_count == 2);
    assert(out.fade_args[0][5] == 0xFF);
    assert(s.timer_d == 0xFF);
    assert(s.sub_c == 2);
    assert(out.player5a_count == 2);
    assert(out.player5a_vals[1] == 0x00);

    /* +0xC == 2: +0xD == 0x20 arms sub_08013CF0(0x21); wrap ->
     * sub_0803D270 + sub_0803CED4, +0xC -> 3, +0xD = 0x40. */
    s.timer_d = 0x20;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 15, &in, &out);
    assert(out.call_13cf0 == 0x21);
    assert(s.timer_d == 0x1F);
    assert(!out.call_3d270);
    s.timer_d = 0;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 15, &in, &out);
    assert(out.call_3d270 && out.call_3ced4);
    assert(s.sub_c == 3);
    assert(s.timer_d == 0x40);

    /* +0xC == 3: wrap -> +0x5A clear, +0xB -> 8, +0xC = 0,
     * +0x417 = 0. */
    s.timer_d = 0;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 15, &in, &out);
    assert(s.sub_b == 8);
    assert(s.sub_c == 0);
    assert(out.write_417 && out.write_417_val == 0);
    assert(out.return_code == 0x80);

    /* case 8 (_0806149C): +0xC 0 -> 1, returns 0x80. */
    memset(&s, 0, sizeof s);
    s.state = 1;
    s.sub_b = 8;
    memset(&out, 0, sizeof out);
    r15 = aos_object20_slots_update(&s, 15, &in, &out);
    assert(r15 == 0x80);
    assert(s.sub_c == 1);
    s.sub_c = 5;                        /* != 0: stays */
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 15, &in, &out);
    assert(s.sub_c == 5);

    /* case 9 (_080614B0): +0xC == 0 init — player +0x58 |= 0x40,
     * +0x417 = 1, own +0x58 &= ~0x20, +0xC = 1, +0xD = 0xC0;
     * probe & 4 clear integrates +0x40 += +0x48,
     * +0x44 += +0x4C; signed +0x42 >= 0x50 parks and +0xB++. */
    memset(&s, 0, sizeof s);
    s.state = 1;
    s.sub_b = 9;
    s.flags58 = 0x60;
    s.scr42 = 0x60;
    s.scr46 = 0x10;
    s.pos40 = 1;
    s.pos44 = 2;
    s.vel48 = 0x100;
    s.vel4c = 0x40;
    in.probe_6e140 = 0;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 15, &in, &out);
    assert(out.player58_or_40);
    assert(s.timer_d == 0xC0);          /* init armed; the park
                                         * below resets +0xC */
    assert((s.flags58 & 0x20) == 0);
    assert(s.pos40 == 0x101);
    assert(s.pos44 == 0x42);
    assert(s.scr42 == 0x50);
    assert(s.scr46 == 0x11);
    assert(out.write_417 && out.write_417_val == 0);
    assert(s.sub_b == 10);
    assert(s.sub_c == 0);
    assert(out.anim_count == 1 && out.anim_a[0] == 1);

    /* probe & 4 set + scr42 < 0x50: F2C8(0,0,1) gate on +0x6D,
     * no integration, no advance. */
    memset(&s, 0, sizeof s);
    s.state = 1;
    s.sub_b = 9;
    s.sub_c = 1;
    s.scr42 = 0x40;
    s.anim6d = 3;
    in.probe_6e140 = 4;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 15, &in, &out);
    assert(out.anim_a[0] == 0 && out.anim_b[0] == 0 &&
           out.anim_c[0] == 1);
    assert(s.sub_b == 9);
    assert(s.pos40 == 0);

    /* case 11 (_08061588): +0xC gate; +0x6D != 4 -> F2C8(4,0,1). */
    memset(&s, 0, sizeof s);
    s.state = 1;
    s.sub_b = 11;
    s.anim6d = 0;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 15, &in, &out);
    assert(s.sub_c == 1);
    assert(out.anim_a[0] == 4);
    s.sub_c = 1;
    s.anim6d = 4;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 15, &in, &out);
    assert(out.anim_count == 0);

    /* state 0x80 exit (_080615AA): jingle 0x3A, CED4+D270,
     * b878 0x1040, +0x90 flag store, release(0), player +0x5A
     * clear, +0x42 = 0x50, +0x46 = 0xC0 - A094[0xA], +0x58 &=
     * ~0x20, +0x6D != 4 anim gate, F17C, PlaySong(0x816C),
     * sub_08000EA8(0x40), +0xA = 0x81, +0xC/+0xB = 0, ret 0. */
    memset(&s, 0, sizeof s);
    s.state = 0x80;
    s.flags58 = 0x60;
    s.anim6d = 0;
    in.a094_a = 0x30;
    in.player_byte5a = 0xFF;
    memset(&out, 0, sizeof out);
    assert(aos_object20_slots_update(&s, 15, &in, &out) == 0);
    assert(out.jingle == 0x3A);
    assert(out.call_3ced4 && out.call_3d270);
    assert(out.b878 == 0x1040);
    assert(out.write_90 && out.write_90_val == 0);
    assert(out.release_arg == 0);
    assert(out.player5a_vals[0] == 0x87);   /* 0xFF & 0x87 */
    assert(s.scr42 == 0x50);
    assert(s.scr46 == (int16_t)(0xC0 - 0x30));
    assert((s.flags58 & 0x20) == 0);
    assert(out.anim_a[0] == 4);
    assert(out.anim_step_count == 1);       /* explicit F17C only */
    assert(out.playsong_count == 1 && out.playsong[0] == 0x816C);
    assert(out.call_00ea8 == 0x40);
    assert(s.state == 0x81);
    assert(s.sub_b == 0 && s.sub_c == 0);

    /* state 0x81 (negative): fully inert, no F17C. */
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 15, &in, &out);
    assert(out.anim_step_count == 0);
    assert(out.return_code == 0);

    /* ---- param0 16: create sub_08061668 / update sub_080616F8.
     * ---- */
    /* create gate: +0x42C bit 0x10000 -> delete mark. */
    memset(&s, 0, sizeof s);
    in = base_in();
    in.flag1327a = 0;
    in.flag42c = 0x210000; /* 0x200000 gate | 0x10000 variant */
    assert(!aos_object20_slots_create(&s, 16, &in, &out));
    assert(s.flags59 & 0x8);

    /* create success: record 1, F2C8(4,0,1), placement
     * (0x50 - A094[6], 0xC0 - A094[0xA]), +0x58 |= 0x40,
     * +0x3E |= 4. */
    memset(&s, 0, sizeof s);
    in = base_in();
    assert(aos_object20_slots_create(&s, 16, &in, &out));
    assert(out.rec_5b754 == 1);
    assert(out.anim_a[0] == 4);
    assert(s.scr42 == (int16_t)(0x50 - 0x40));
    assert(s.scr46 == (int16_t)(0xC0 - 0x30));
    assert(s.flags58 & 0x40);
    assert(s.field3e & 4);

    /* update state 0 -> release(0), state 1, probe chain all
     * gates pass: player velocity writes, +0xD window pick,
     * facing mirror, +0x42C|=1, state -> 2. */
    in.probe_68ad4 = true;
    in.word16 = 0x42;
    in.s16_4be = 0;
    in.player_w4c = -5;
    s.pos40 = 0;
    in.player_pos40 = 0x1000;    /* own < player -> bit 0x40 */
    memset(&out, 0, sizeof out);
    assert(aos_object20_slots_update(&s, 16, &in, &out) == 0);
    assert(out.release_arg == 0);
    assert(out.player_w48_zero && out.player_w50_zero);
    assert(out.player_w4c_min0);
    assert(out.window_text == 0x1D);    /* old +0xD = 0 <= 3 */
    assert(s.timer_d == 1);
    assert(s.flags58 & 0x40);
    assert(out.set_42c & 1);
    assert(s.state == 2);

    /* old +0xD > 3 picks window 0x1E — drive state back. */
    memset(&s, 0, sizeof s);
    s.state = 1;
    s.timer_d = 4;
    s.pos40 = 0x2000;
    in.player_pos40 = 0;         /* own > player -> bit clear */
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 16, &in, &out);
    assert(out.window_text == 0x1E);
    assert((s.flags58 & 0x41) == 0);
    assert(s.state == 2);

    /* probe chain broken (probe_68ad4 false): state stays 1,
     * no writes. */
    memset(&s, 0, sizeof s);
    s.state = 1;
    in.probe_68ad4 = false;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 16, &in, &out);
    assert(s.state == 1);
    assert(!out.player_w48_zero);
    assert(!out.set_42c);

    /* state 2 poll done -> 0x80 + release(0). */
    in.probe_68ad4 = true;
    s.state = 2;
    in.text_done = true;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 16, &in, &out);
    assert(s.state == 0x80);
    assert(out.release_arg == 0);

    /* ---- param0 17: create bx lr; update window 0x17 +
     * release. ---- */
    memset(&s, 0, sizeof s);
    in = base_in();
    assert(aos_object20_slots_create(&s, 17, &in, &out));
    assert(!(out.set_42c & 1));
    memset(&s, 0, sizeof s);
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 17, &in, &out);
    assert(out.set_42c & 1);
    assert(out.window_text == 0x17);
    assert(s.state == 1);
    in.text_done = true;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 17, &in, &out);
    assert(out.release_arg == 1);

    /* ---- param0 18: create +0x42C|=1; update window 0x18,
     * done poll -> close + mark, 0x80 -> release. ---- */
    memset(&s, 0, sizeof s);
    in = base_in();
    assert(aos_object20_slots_create(&s, 18, &in, &out));
    assert(out.set_42c & 1);
    memset(&s, 0, sizeof s);
    in.text_done = false;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 18, &in, &out);
    assert(out.set_42c & 1);
    assert(out.window_text == 0x18);
    assert(s.state == 1);
    in.text_done = true;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 18, &in, &out);
    assert(out.window_close);
    assert(s.flags59 & 0x8);
    s.state = 0x80;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 18, &in, &out);
    assert(out.release_arg == 1);

    /* ---- shared-shell checks: create gate failure deletes;
     * update prologue transforms. ---- */
    memset(&s, 0, sizeof s);
    in = base_in();
    in.flag1327a = 0;
    in.flag42c = 0;                /* no 0x200000 -> gate fails */
    assert(!aos_object20_slots_create(&s, 0, &in, &out));
    assert(out.deleted);

    in = base_in();
    in.flag42c = 0x80;             /* update prologue: +0x7864|=2 */
    memset(&s, 0, sizeof s);
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 2, &in, &out);
    assert(out.unk7864_or2);
    /* +0x414 transform: word16&8, word60&2, !(42C&0x10000). */
    in.word16 = 8;
    in.word60 = 2;
    in.byte414 = 0x1F;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 2, &in, &out);
    assert(out.write_414);
    assert(out.write_414_val == ((0x1F & ~0x10u) | 5u));
    /* negative +0xA with byte41b -> sub_08067804(-3). */
    s.state = 0x80;
    in.byte41b = 1;
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 2, &in, &out);
    assert(out.call_67804);

    /* unknown param0 reports unported on both sides. */
    memset(&s, 0, sizeof s);
    in = base_in();
    assert(aos_object20_slots_create(&s, 99, &in, &out));
    assert(out.unported);
    memset(&out, 0, sizeof out);
    aos_object20_slots_update(&s, 99, &in, &out);
    assert(out.unported);

    /* The committed 0x081188E4 table the runtime fill binds into
     * in->tbl14 for update15 cases 5/6: ten words decoded from
     * the ROM image, indexed by entity +0x14 (0..9). */
    {
        static const int32_t want[10] = {
            0x80, 0x80, 0x20, 0x20, 0x40, 0x40,
            0x80, 0x60, 0x40, 0x30,
        };
        assert(memcmp(aos_object20_slots_table_1188e4, want,
                      sizeof want) == 0);
    }

    puts("aos_object20_slots tests passed");
    return 0;
}
