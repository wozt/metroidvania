/* Unit tests for src/runtime/aos/game/aos_object20_story.c —
 * the Object20 story-variant parity port (param0 9, 10, 21,
 * 22, 23, 24, 29). */
#include "../src/runtime/aos/game/aos_object20_story.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static AosObject20StoryIn base_in(void) {
    AosObject20StoryIn in;
    memset(&in, 0, sizeof in);
    in.flag1327a = 1;           /* pass the shared create gate */
    in.byte8e = 0;
    in.byte13266 = 0;
    in.keep_5b754 = true;
    in.spawn14_ok = true;
    in.spawn18_ok = true;
    in.a094_6 = 0x40;
    in.a094_a = 0x30;
    in.player_scr42 = 0x100;
    in.player_pos40 = 0;
    in.player_pos44 = 0;
    in.player_anim5c = 3;
    in.fade_done = false;
    in.c328_ok = true;
    return in;
}

int main(void) {
    AosObject20Story s;
    AosObject20StoryOut out;
    AosObject20StoryIn in;

    /* ---- param0 21: trivial create + fixed update. ---- */
    memset(&s, 0, sizeof s);
    in = base_in();
    assert(aos_object20_story_create(&s, 21, &in, &out));
    assert(out.set_42c & 1);
    assert(out.fade_out);
    assert(out.call_6666c);
    assert(out.set_42c & 0x200);
    memset(&out, 0, sizeof out);
    assert(aos_object20_story_update(&s, 21, &in, &out) == 0);
    assert(out.b878 == 0x1040);
    assert(out.eec == 0x25);
    assert(out.release_arg == 1);

    /* ---- param0 22: create always deletes (+0x59 |= 8). ---- */
    memset(&s, 0, sizeof s);
    in = base_in();
    assert(!aos_object20_story_create(&s, 22, &in, &out));
    assert(s.flags59 & 0x8);
    /* update state 0: flag clear -> arm +0x42C, state 1. */
    memset(&s, 0, sizeof s);
    s.state = 0;
    memset(&out, 0, sizeof out);
    in = base_in();
    in.flag12078_2b = 0;
    assert(aos_object20_story_update(&s, 22, &in, &out) == 0);
    assert(out.eec == 0x2D);
    assert(out.set_42c & 1);
    assert(s.state == 1);
    /* state 1: window 0x22 opens, +0x417 = 1, state -> 2. */
    memset(&out, 0, sizeof out);
    assert(aos_object20_story_update(&s, 22, &in, &out) == 0);
    assert(out.window_text == 0x22);
    /* state 1->2 is a single transition (_08062158 returns);
     * the +0xB == 0 counter starts on the next call. */
    assert(s.state == 2 && s.sub_b == 0 && s.sub_c == 0);
    memset(&out, 0, sizeof out);
    in.text_done = false;
    assert(aos_object20_story_update(&s, 22, &in, &out) == 0);
    assert(out.window_poll);
    assert(s.sub_b == 0 && s.sub_c == 1 && s.timer_d == 0x40);
    /* text done: release + jingle + song + +0x254CC. */
    memset(&out, 0, sizeof out);
    in.text_done = true;
    assert(aos_object20_story_update(&s, 22, &in, &out) == 0);
    assert(s.state == 0x80);
    assert(out.jingle == 0x2B);
    assert(out.release_arg == 1);
    assert(out.b878 == 0x1041);
    assert(out.write_254cc && out.write_254cc_val == 0x2D);
    assert(out.song == 0x2D);

    /* ---- param0 23: create arms +0x42C bit 0. ---- */
    memset(&s, 0, sizeof s);
    in = base_in();
    assert(aos_object20_story_create(&s, 23, &in, &out));
    assert(out.set_42c & 1);
    /* update: +0x42C |= 1 and |= 0x20 every call; state 0 arms
     * +0xD = 0x80 on the first call then counts down — the wrap
     * opens window 0x23 and advances to state 1. */
    memset(&s, 0, sizeof s);
    memset(&out, 0, sizeof out);
    in = base_in();
    for (int i = 0; i < 0x80; ++i)
        aos_object20_story_update(&s, 23, &in, &out);
    assert(s.state == 0 && s.timer_d == 0);
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 23, &in, &out);
    assert(out.window_text == 0x23);
    assert(s.state == 1);
    /* state 1 poll done -> fades, jingle 0x2C, b878 0x1040. */
    memset(&out, 0, sizeof out);
    in.text_done = true;
    aos_object20_story_update(&s, 23, &in, &out);
    assert(s.state == 2);
    /* state 2 waits on sub_0803D408; clear -> state 0x80. */
    s.state = 2;
    in.text_done = false;
    in.fade_done = false;
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 23, &in, &out);
    assert(s.state == 0x80);
    /* state 0x80: window close, +0x427-ish writes, +0x64 = 0xA. */
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 23, &in, &out);
    assert(out.window_close);
    assert(out.write_64_0a);
    assert(out.call_127f0_off == 0x428);

    /* ---- param0 24: create gate + child spawn. ---- */
    memset(&s, 0, sizeof s);
    in = base_in();
    in.flag42c = 0x2000;        /* pass the +0x42C bit-0x2000 gate */
    assert(aos_object20_story_create(&s, 24, &in, &out));
    assert(s.child14);
    assert(out.spawn_count == 1 && out.spawn_type[0] == 1);
    assert(out.cf0 == 0x19);
    assert(out.set_42c & (1 | 0x20 | 0x10000));
    /* gate failure deletes. */
    memset(&s, 0, sizeof s);
    in = base_in();
    in.flag42c = 0;
    assert(!aos_object20_story_create(&s, 24, &in, &out));
    /* update state 0: fade in, then sub_0803D408 -> helper. */
    memset(&s, 0, sizeof s);
    s.state = 0; s.sub_b = 1;
    in = base_in();
    in.keep_5b754 = true;
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 24, &in, &out);
    assert(s.state == 1);
    assert(s.timer_d == 0xFF);
    assert(out.song == 0x18);
    assert(out.call_1d028_a == 0x51);
    /* state 1: +0xD wraps -> window pick by sub_08032DF0. */
    s.timer_d = 0;          /* decrement wraps to 0xFF */
    in.rng_32df0 = 0x400;       /* > 0x31F -> text 0x26, +0x13 = 1 */
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 24, &in, &out);
    assert(s.state == 2);
    assert(out.window_text == 0x26);
    assert(s.field13 == 1);
    /* state 2 poll not done, +0xB == 1: player +0x58 |= 0x40. */
    in.text_done = false;
    s.sub_b = 1;
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 24, &in, &out);
    assert(out.player58_or_40);
    /* poll done -> fades, jingle 0x2D path waits on 3D408. */
    in.text_done = true;
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 24, &in, &out);
    assert(s.state == 3);
    assert(out.b878 == 0x1040);
    assert(out.unk2cb0_feff);
    /* state 3 fade clear -> window close + jingle + +0x64. */
    in.fade_done = false;
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 24, &in, &out);
    assert(out.window_close);
    assert(out.jingle == 0x2D);
    assert(out.write_64_0a);

    /* ---- param0 9: create builds the guide pair. ---- */
    memset(&s, 0, sizeof s);
    in = base_in();
    in.flag12078_26 = 0;
    in.player_scr42 = 0x80;     /* > 0x7F passes the x gate */
    in.rng_3e654 = 3;
    assert(aos_object20_story_create(&s, 9, &in, &out));
    assert(s.child14 && s.link18);
    assert(s.scr42 == (int16_t)(0x48 - 0x40));
    assert(s.scr46 == (int16_t)(0xA0 - 0x30));
    assert(out.spawn_count == 2);
    assert(out.spawn_type[0] == 5 && out.spawn_type[1] == 1);
    assert(out.call_3e058 && out.call_3e654);
    assert(out.set_42c & 1);
    /* create gate: flag26 set or player x <= 0x7F deletes. */
    memset(&s, 0, sizeof s);
    in = base_in();
    in.flag12078_26 = 1;
    in.player_scr42 = 0x80;
    assert(!aos_object20_story_create(&s, 9, &in, &out));
    assert(s.flags59 & 0x8);
    /* update state 0: song + window 0x14. */
    memset(&s, 0, sizeof s);
    in = base_in();
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 9, &in, &out);
    assert(s.state == 1);
    assert(out.song == 0x1B8);
    assert(out.eec == 0x28);
    assert(out.window_text == 0x14);
    /* state 1 poll done -> 601d4 + release(0) + jingle 0x26. */
    in.text_done = true;
    s.child14 = true; s.link18 = true;
    memset(&out, 0, sizeof out);
    assert(aos_object20_story_update(&s, 9, &in, &out) == 0);
    assert(s.state == 0x80);
    assert(out.release_arg == 0);
    assert(out.jingle == 0x26);
    assert(s.field65 == 0x12);
    /* state 0x80 returns without anim step. */
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 9, &in, &out);
    assert(out.anim_step_count == 0);
    /* state 1 live, +0xB == 6: player facing clear + anim5c. */
    memset(&s, 0, sizeof s);
    s.state = 1; s.sub_b = 6;
    in = base_in();
    in.text_done = false;
    in.player_anim5c = 4;
    memset(&out, 0, sizeof out);
    int r = aos_object20_story_update(&s, 9, &in, &out);
    assert(r == 0);             /* _080605FE leaves r7 unchanged */
    assert(out.player58_andnot_41);
    assert(s.anim5c == 5);
    /* state 3 proximity: player x > 2 -> r7 0x20. */
    memset(&s, 0, sizeof s);
    s.state = 3; s.sub_c = 1;
    in.player_scr42 = 0x10;
    memset(&out, 0, sizeof out);
    r = aos_object20_story_update(&s, 9, &in, &out);
    assert(r == 0x20);

    /* ---- param0 10: create gates. ---- */
    memset(&s, 0, sizeof s);
    in = base_in();
    in.flag12078_27 = 0;
    in.flags37e = 0x40;
    in.player_scr42 = 0x40;     /* <= 0x80 passes */
    in.player_pos40 = 0x1000;   /* self pos40 0 < player -> bit6 */
    assert(aos_object20_story_create(&s, 10, &in, &out));
    assert(s.scr42 == (int16_t)(0x118 - 0x40));
    assert(s.flags58 & 0x40);
    assert(out.player58_andnot_41);
    /* flag27 set deletes. */
    memset(&s, 0, sizeof s);
    in = base_in();
    in.flag12078_27 = 1;
    in.flags37e = 0x40;
    in.player_scr42 = 0x40;
    assert(!aos_object20_story_create(&s, 10, &in, &out));
    /* update state 0 proximity: player x > 0x60 arms +0x42C 0x200. */
    memset(&s, 0, sizeof s);
    in = base_in();
    in.player_scr42 = 0x70;
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 10, &in, &out);
    assert(s.state == 1);
    assert(out.set_42c & 0x200);
    assert(out.fade_out);
    /* state 1 -> window 0x15 + +0x417 = 1, falls into state 2. */
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 10, &in, &out);
    assert(out.window_text == 0x15);
    assert(s.state == 2);
    /* state 1->2 falls through the c0->c1 chain: +0xC == 1,
     * +0xD armed 0x30 then counted down to 0x2F. */
    assert(s.sub_c == 1 && s.timer_d == 0x2F);
    /* next poll: +0xC == 1 counts +0xD down. */
    memset(&out, 0, sizeof out);
    in.text_done = false;
    aos_object20_story_update(&s, 10, &in, &out);
    assert(s.sub_c == 1 && s.timer_d == 0x2E);
    /* timer wrap -> b878 0x1040, +0xC = 2. */
    s.timer_d = 0;
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 10, &in, &out);
    assert(out.b878 == 0x1040);
    assert(s.sub_c == 2);
    /* +0xC == 2 player clamp: player x > 0xE0 - a094_6. */
    in.a094_6 = 0x40;       /* limit = 0xA0 */
    in.player_scr42 = 0xA1;
    memset(&out, 0, sizeof out);
    int r10 = aos_object20_story_update(&s, 10, &in, &out);
    assert(out.player_scr42_set);
    assert(out.player_scr42_val == 0xA0);
    assert(s.sub_c == 3);
    /* +0xC == 3 a094_6 > 0x89 -> sub_08013CF0(0x16) + +0x417 = 0. */
    in.a094_6 = 0x8A;
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 10, &in, &out);
    assert(out.cf0 == 0x16);
    assert(out.write_417 && out.write_417_val == 0);
    /* state 0x80 release path. */
    s.state = 0x80;
    memset(&out, 0, sizeof out);
    r10 = aos_object20_story_update(&s, 10, &in, &out);
    assert(out.release_arg == 1);
    assert(out.jingle == 0x27);
    assert(out.call_13960);
    (void)r10;

    /* ---- param0 29: create + update chain. ---- */
    memset(&s, 0, sizeof s);
    in = base_in();
    assert(aos_object20_story_create(&s, 29, &in, &out));
    assert(s.state == 1);
    assert(s.child14);
    assert(out.spawn_count == 1 && out.spawn_type[0] == 0xB);
    assert(out.player58_or_20);
    assert(out.player_scr42_val == 0xA0 - 0x40);
    assert(out.set_42c & (1 | 0x10000));
    /* create failure: keep_5b754 false marks +0x59 |= 8. */
    memset(&s, 0, sizeof s);
    in = base_in();
    in.keep_5b754 = false;
    assert(!aos_object20_story_create(&s, 29, &in, &out));
    /* update state 1: +0xD wraps -> fades + jingle + state 2. */
    memset(&s, 0, sizeof s);
    s.state = 1; s.timer_d = 0;
    in = base_in();
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 29, &in, &out);
    assert(s.state == 2);
    assert(out.jingle == 0x2D);
    assert(out.unk2cb0_feff);
    /* state 2 fade clear -> +0x37E clear, event, close, +0x64. */
    in.fade_done = false;
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 29, &in, &out);
    assert(s.state == 0x80);
    assert(out.flags37e_clear1);
    assert(out.ev_121e0 == 0x28);
    assert(out.window_close);
    assert(out.write_64_0a);
    /* state 0 path: sub_08013EEC(0x25) once, fade gate. */
    memset(&s, 0, sizeof s);
    s.state = 0;
    in = base_in();
    in.fade_done = false;   /* 3D408 == 0 -> advance sub_b */
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 29, &in, &out);
    assert(out.eec == 0x25);
    assert(s.sub_b == 1 && s.sub_c == 0);
    /* next call: DMA + unk2cb0 |= 0x100 + state 1. */
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 29, &in, &out);
    assert(out.dma_setup);
    assert(out.unk2cb0_or100);
    assert(s.state == 1);
    /* fade_done set -> &= 0xFEFF instead of advancing. */
    memset(&s, 0, sizeof s);
    s.state = 0;
    in.fade_done = true;
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 29, &in, &out);
    assert(out.unk2cb0_feff);
    assert(s.sub_b == 0 && s.sub_c == 1);

    /* ---- child29 update (sub_0805C39C). ---- */
    memset(&s, 0, sizeof s);
    s.child29.state = 0;
    memset(&out, 0, sizeof out);
    in = base_in();
    aos_object20_story_child29_update(&s, &in, &out);
    assert(out.anim_step_count == 1);
    /* state 1: clears +0x59 bit 1 once, waits for bit 0 —
     * anim step while waiting, advance when bit 0 set. */
    s.child29.state = 1;
    s.child29.flags59 = 0x2;
    memset(&out, 0, sizeof out);
    aos_object20_story_child29_update(&s, &in, &out);
    assert((s.child29.flags59 & 2) == 0);
    assert(s.child29.state == 1);
    assert(out.anim_step_count == 1);   /* bit 0 still clear */
    s.child29.flags59 = 0x1;
    aos_object20_story_child29_update(&s, &in, &out);
    assert(s.child29.state == 2);
    /* state 2 first call: spawn type 0xB + timer 0x20. */
    memset(&s, 0, sizeof s);
    s.child29.state = 2;
    memset(&out, 0, sizeof out);
    aos_object20_story_child29_update(&s, &in, &out);
    assert(out.spawn_count == 1 && out.spawn_type[0] == 0xB);
    assert(s.child29.timer_d == 0x1F);   /* armed 0x20 then -- */
    /* state 3: c328_ok false -> state 0x80. */
    s.child29.state = 3;
    s.child29.ctr_c = 1;
    in.c328_ok = false;
    aos_object20_story_child29_update(&s, &in, &out);
    assert(s.child29.state == 0x80);

    /* ---- shared create gates. ---- */
    memset(&s, 0, sizeof s);
    in = base_in();
    in.flag1327a = 0;
    in.flag42c = 0;            /* no 0x20000000, no +0x1327A -> del */
    assert(!aos_object20_story_create(&s, 21, &in, &out));
    /* +0x8E bit 0x80 kills even a passing gate. */
    in = base_in();
    in.byte8e = 0x80;
    assert(!aos_object20_story_create(&s, 21, &in, &out));
    /* +0x13266 set without +0x42C 0x8000 kills. */
    in = base_in();
    in.byte13266 = 1;
    assert(!aos_object20_story_create(&s, 21, &in, &out));
    in.flag42c = 0x8001;    /* 0x8000 passes the +0x13266 gate,
                             * bit 0 arms the SetPlayerInput tail */
    assert(aos_object20_story_create(&s, 21, &in, &out));
    /* shared tail: flag42c bit0 -> SetPlayerInput(1,0). */
    assert(out.setplayerinput == 1 && out.setplayerinput_arg == 0);

    /* ---- shared update prologue. ---- */
    memset(&s, 0, sizeof s);
    s.state = 0x80;            /* negative +0xA */
    in = base_in();
    in.byte41b = 1;
    in.flag42c = 0x80;         /* +0x42C bit 0x80 -> +0x7864 |= 2 */
    in.word16 = 8; in.word60 = 2; in.byte414 = 0x1F;
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 21, &in, &out);
    assert(out.unk7864_or2);
    assert(out.call_67804);
    /* non-negative state: +0x414 rewrite when bits align. */
    s.state = 0;
    in.flag42c = 0;            /* bit0 clear -> c5a8(1) */
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 23, &in, &out);
    assert(out.unk414_write);
    assert(out.unk414_val == ((0x1F & ~0x10u) | 5u));
    assert(out.c5a8 == 1);

    puts("aos_object20_story: all tests passed");
    return 0;
}
