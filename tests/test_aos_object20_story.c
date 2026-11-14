/* Unit tests for src/runtime/aos/game/aos_object20_story.c —
 * the Object20 story-variant parity port. Create dispatch
 * follows the native 0x08526658 indices (param0 9, 10, 20,
 * 21, 22, 23, 31); update dispatch covers param0 9, 10, 21,
 * 22, 23, 24, 29. */
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
    in.wo_field15 = -1;         /* +0x15 keep: sub_08064518 unported */
    return in;
}

/* Injected sub_080020A0 floor probes for the param0-31
 * grandchild/debris updates. */
static int32_t probe_zero(int32_t x, int32_t y, void *ctx) {
    (void)x; (void)y; (void)ctx;
    return 0;
}
static int32_t probe_one(int32_t x, int32_t y, void *ctx) {
    (void)x; (void)y; (void)ctx;
    return 1;
}
static int32_t probe_mixed(int32_t x, int32_t y, void *ctx) {
    (void)x; (void)y; (void)ctx;
    return 1;
}

/* update30 state 1's sub_080628AC whiteout writes the gEwramData
 * +4/+6 counters back through Out; the runtime keeps them as
 * globals, so the test threads them into the next call's In. */
static void update30_whiteout(AosObject20Story *s,
                              AosObject20StoryIn *in,
                              AosObject20StoryOut *out) {
    aos_object20_story_update(s, 30, in, out);
    if (out->wo_unk4 >= 0)
        in->ewram_unk4 = (uint16_t)out->wo_unk4;
    if (out->wo_unk6 >= 0)
        in->ewram_unk6 = (uint16_t)out->wo_unk6;
}

int main(void) {
    AosObject20Story s;
    AosObject20StoryOut out;
    AosObject20StoryIn in;

    /* ---- param0 20: trivial create (sub_08062040) + the
     * param0-21 fixed update (sub_08062074). ---- */
    memset(&s, 0, sizeof s);
    in = base_in();
    assert(aos_object20_story_create(&s, 20, &in, &out));
    assert(out.set_42c & 1);
    assert(out.fade_out);
    assert(out.call_6666c);
    assert(out.set_42c & 0x200);
    memset(&out, 0, sizeof out);
    assert(aos_object20_story_update(&s, 21, &in, &out) == 0);
    assert(out.b878 == 0x1040);
    assert(out.eec == 0x25);
    assert(out.release_arg == 1);

    /* ---- param0 21: create always deletes (+0x59 |= 8,
     * sub_08062068); the param0-22 update chain follows. ---- */
    memset(&s, 0, sizeof s);
    in = base_in();
    assert(!aos_object20_story_create(&s, 21, &in, &out));
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

    /* ---- param0 22: create arms +0x42C bit 0 (sub_08062244);
     * the param0-23 update chain follows. ---- */
    memset(&s, 0, sizeof s);
    in = base_in();
    assert(aos_object20_story_create(&s, 22, &in, &out));
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

    /* ---- param0 23: create gate + child spawn (sub_08062260);
     * the param0-24 update chain follows. create(24) itself is
     * sub_080657B4 in aos_npc_talk, not this module. ---- */
    memset(&s, 0, sizeof s);
    in = base_in();
    in.flag42c = 0x2000;        /* pass the +0x42C bit-0x2000 gate */
    assert(aos_object20_story_create(&s, 23, &in, &out));
    assert(s.child14);
    assert(out.spawn_count == 1 && out.spawn_type[0] == 1);
    assert(out.cf0 == 0x19);
    assert(out.set_42c & (1 | 0x20 | 0x10000));
    /* gate failure deletes. */
    memset(&s, 0, sizeof s);
    in = base_in();
    in.flag42c = 0;
    assert(!aos_object20_story_create(&s, 23, &in, &out));
    /* param0 24 create is not this module's routine; param0 29
     * create (sub_0805BF4C) is — with flag42c = 0 its 0x8000
     * gate fails and the entity is marked for deletion. */
    memset(&s, 0, sizeof s);
    in = base_in();
    assert(!aos_object20_story_create(&s, 24, &in, &out));
    memset(&s, 0, sizeof s);
    in = base_in();
    assert(!aos_object20_story_create(&s, 29, &in, &out));
    assert(s.flags59 & 0x8);
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

    /* ---- param0 31: create (sub_0805C49C) + the param0-29
     * update chain (sub_0805C08C). ---- */
    memset(&s, 0, sizeof s);
    in = base_in();
    assert(aos_object20_story_create(&s, 31, &in, &out));
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
    assert(!aos_object20_story_create(&s, 31, &in, &out));
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

    /* ---- param0 19: create sub_08061A00 (the shop sell
     * spawn gate; update[19] is the shop routine). ---- */
    memset(&s, 0, sizeof s);
    in = base_in();
    in.flag12078_29 = 0;
    in.ret_6e0d0 = 1;               /* sub_0806E0D0 low byte live */
    assert(aos_object20_story_create(&s, 19, &in, &out));
    assert(s.scr42 == (int16_t)(0x150 - 0x40));
    assert(s.vel48 == (int32_t)0xFFFF0000);
    assert(s.anim5c == 0x11);
    assert((s.flags58 & 0x20) != 0);
    assert(out.set_42c & 1);
    /* flag29 set marks +0x59 |= 8 -> delete. */
    memset(&s, 0, sizeof s);
    in = base_in();
    in.flag12078_29 = 1;
    in.ret_6e0d0 = 1;
    assert(!aos_object20_story_create(&s, 19, &in, &out));
    assert(s.flags59 & 0x8);
    /* player x gate: 0x40 + 0 <= 0x7F fails. */
    memset(&s, 0, sizeof s);
    in = base_in();
    in.a094_6 = 0;
    in.player_scr42 = 0x40;
    in.ret_6e0d0 = 1;
    assert(!aos_object20_story_create(&s, 19, &in, &out));
    /* dead sub_0806E0D0 probe: release(1) + delete. */
    memset(&s, 0, sizeof s);
    in = base_in();
    in.ret_6e0d0 = 0;
    assert(!aos_object20_story_create(&s, 19, &in, &out));
    assert(out.release_arg == 1);

    /* ---- param0 29: create sub_0805BF4C. ---- */
    memset(&s, 0, sizeof s);
    in = base_in();
    in.flag42c = 0x8000;            /* +0x42C bit-0x8000 gate */
    assert(aos_object20_story_create(&s, 29, &in, &out));
    assert(s.scr42 == (int16_t)(0x40 - 0x40));
    assert(s.scr46 == (int16_t)(0xBF - 0x30));
    assert(s.flags58 & 0x40);
    assert(out.player58_or_20);
    assert(out.player_scr42_val == (int32_t)s.scr42 + 0x18);
    assert(out.player_pos44_val == s.scr46);
    assert(out.fade_3d18c && out.fade_3cdf0);
    assert(out.dma_setup && out.unk2cb0_feff);
    assert(out.set_42c & 1);
    /* keep_5b754 false marks +0x59 |= 8 -> delete. */
    memset(&s, 0, sizeof s);
    in = base_in();
    in.flag42c = 0x8000;
    in.keep_5b754 = false;
    assert(!aos_object20_story_create(&s, 29, &in, &out));
    assert(s.flags59 & 0x8);

    /* ---- param0 30: create sub_0805B888. ---- */
    memset(&s, 0, sizeof s);
    in = base_in();
    assert(aos_object20_story_create(&s, 30, &in, &out));
    assert(out.set_42c & 0x80021);
    assert(out.call_ef6c);
    assert(out.song == 0x1000);
    assert(out.player_scr42_val == 0x78);
    assert(out.player_pos44_val == 0x50);
    assert(out.player_w4a_zero && out.player_w4e_zero);
    assert(out.player58_or_20);

    /* ---- param0 30: update sub_0805B8F4. ---- */
    memset(&s, 0, sizeof s);
    in = base_in();
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 30, &in, &out);
    /* state 0 -> 1 (the ROM's literal shared advance); the
     * tail re-reads +0xA == 1 and parks the player. */
    assert(s.state == 1);
    assert(out.player_scr42_val == 0x78);
    assert(out.player_w48_zero && out.player_w4c_zero);
    assert(out.call_1d028_a == 0x33);
    assert(out.set_42c & 0x20);
    assert(out.anim_step_count == 0);   /* no F17C in this routine */
    /* state 1 runs the ported sub_080628AC whiteout. Case 0
     * re-pins the cinematic counters and wraps +0xD into case
     * 1; +0xD entered state 1 at 7 (state 0's arm), so eight
     * calls. */
    for (int i = 0; i < 8; ++i) {
        memset(&out, 0, sizeof out);
        update30_whiteout(&s, &in, &out);
        assert(s.state == 1);
        assert(out.call_628ac);
        assert(out.unk2cb0_zero && out.wo_disp_init);
        assert(out.wo_unk4 == 0 && out.wo_unk6 == 1);
    }
    assert(s.sub_b == 1);
    /* case 1: the one-time layer/palette setup. */
    memset(&out, 0, sizeof out);
    update30_whiteout(&s, &in, &out);
    assert(s.sub_b == 2);
    assert(out.unk2cb0_1e01 && out.wo_disp_stage);
    assert(out.call_3dd14 && out.call_3fd60 && out.call_3fd9c &&
           out.call_3c918 && out.call_3f8a8);
    assert(out.wo_unk6 == 2);
    /* case 2 ramps ewram_unk4 every eighth frame; the display
     * mirror 0x10 - (unk_4 >> 1) hits 0 at unk_4 == 0x20. */
    int guard = 0;
    while (s.sub_b == 2 && guard++ < 400) {
        memset(&out, 0, sizeof out);
        update30_whiteout(&s, &in, &out);
        assert(out.call_64518 && out.call_65800);
        assert(out.wo_disp4c >= 0);
    }
    assert(s.sub_b == 3);
    assert(out.wo_disp48 == 0x3F40);
    assert(in.ewram_unk4 == 0);
    /* case 3 gates on +0x15 == 0xA (the unported sub_08064518
     * sub-state, injected through wo_field15). */
    for (int i = 0; i < 4; ++i) {
        memset(&out, 0, sizeof out);
        update30_whiteout(&s, &in, &out);
        assert(s.sub_b == 3);
        assert(out.wo_unk4 < 0);    /* +4 untouched while gated */
    }
    in.wo_field15 = 0xA;
    guard = 0;
    while (s.sub_b == 3 && guard++ < 100) {
        memset(&out, 0, sizeof out);
        update30_whiteout(&s, &in, &out);
    }
    assert(s.sub_b == 4);
    /* case 4 ramps to old +4 > 0x80: the fade pair fires. */
    guard = 0;
    while (s.sub_b == 4 && guard++ < 200) {
        memset(&out, 0, sizeof out);
        update30_whiteout(&s, &in, &out);
    }
    assert(s.sub_b == 5);
    assert(out.fade_out && out.fade_3d18c && out.fade_3cdf0);
    /* case 5 ramps to old +4 > 0x40: the DMA chain fires, then
     * case 6 returns 1 and update30 completes the hand-off. */
    guard = 0;
    while (s.sub_b == 5 && guard++ < 100) {
        memset(&out, 0, sizeof out);
        update30_whiteout(&s, &in, &out);
    }
    assert(s.sub_b == 6);
    assert(out.dma_setup_wo);
    memset(&out, 0, sizeof out);
    update30_whiteout(&s, &in, &out);
    assert(s.state == 2);
    assert(out.player58_andnot_21);
    assert(out.cf0 == 0x20);
    assert(out.call_3c7b4);
    /* the completion call's tail already sees +0xA == 2: the
     * scroll branch runs and +0x18 -> 0x200; the whiteout left
     * +0xD at 0xF9 so the particle edge does not fire yet. */
    assert(s.field18 == 0x200);
    assert(out.call_9e4 && !out.call_45b44);
    /* preset +0xD small so the shared tail's particle edge is
     * exercised as the injected-probe run did. */
    s.timer_d = 6;
    /* state 2 +0xB chain: 2CB0 clear -> DMA + window 0x25 ->
     * fade -> 3D06C gate; +0x18 keeps scrolling and the shared
     * tail keeps decrementing +0xD, so the particle edge
     * (old +0xD & 0xF0 == 0) keeps firing while it counts
     * 6..0 down — matching the ROM's burst behaviour. */
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 30, &in, &out);
    assert(out.unk2cb0_f1ff);
    assert(s.sub_b == 1);
    assert(out.call_45b44);
    assert(s.field18 == 0x400);
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 30, &in, &out);
    assert(out.dma_setup && out.window_text == 0x25);
    assert(s.sub_b == 2 && s.timer_d == 0xFF); /* tail -- */
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 30, &in, &out);
    assert(out.fade_3cdf0);
    assert(s.sub_b == 3);
    assert(s.field18 == 0x800);
    /* +0xB == 3 with sub_0803D06C clear advances state. */
    in.ret_3d06c = 0;
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 30, &in, &out);
    assert(s.state == 3 && s.sub_b == 0);
    assert(s.field18 == 0xA00);
    /* state 3: poll not done, +0xB == 0 -> tail only. */
    in.text_done = false;
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 30, &in, &out);
    assert(out.window_poll);
    assert(s.state == 3);
    /* +0xB == 1 arms the +0xD 0x40 hold; the shared tail then
     * decrements it once more this same call. */
    s.sub_b = 1;
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 30, &in, &out);
    assert(out.write_417 && out.write_417_val == 1);
    assert(s.timer_d == 0x3E);
    s.timer_d = 0;
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 30, &in, &out);
    assert(out.write_417 && out.write_417_val == 0);
    assert(s.sub_b == 2);
    /* text done advances +0xA. */
    in.text_done = true;
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 30, &in, &out);
    assert(s.state == 4);
    /* state 4 +0xB == 0 arms the fade + f6c. */
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 30, &in, &out);
    assert(out.fade_3cdf0);
    assert(out.f6c == 0x1040);
    assert(s.sub_b == 1 && s.sub_c == 0);
    /* +0xD counts down through the shared tail (0x20 -> 0x1F
     * this call). */
    assert(s.timer_d == 0x1F);
    /* +0xB == 1 with 3D06C clear runs the event block. */
    in.ret_3d06c = 0;
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 30, &in, &out);
    assert(out.call_3d004 == 0xE);
    assert(out.call_3d3a0 == 0);
    assert(out.write_a074);
    assert(out.set_42c & 0x804000);
    assert(out.call_11f44 && out.call_11ad0);
    assert(out.disp48_zero);
    assert(s.sub_b == 2);
    /* the +0xB >= 2 hold fires once +0xD and +0xC both read 0
     * (+0xC was cleared by the event block; +0xD counts down
     * through the shared tail). */
    s.timer_d = 0;
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 30, &in, &out);
    assert(out.f6c == 0x1040);
    assert(s.sub_c == 1);

    /* sub_080628AC direct: case 6 returns 1, +0xB > 6 returns 0
     * with only the +6 tail increment (u16 wrap). Direct calls
     * bypass out_reset, so the -1 sentinels are set by hand. */
    memset(&s, 0, sizeof s);
    s.sub_b = 6;
    in = base_in();
    in.ewram_unk6 = 0xFFFE;
    memset(&out, 0, sizeof out);
    out.wo_disp4c = out.wo_disp48 = out.wo_unk4 = out.wo_unk6 = -1;
    assert(aos_object20_whiteout_update(&s, &in, &out) == 1);
    assert(out.call_628ac);
    assert(out.wo_unk6 == 0xFFFF);
    in.ewram_unk6 = 0xFFFF;
    memset(&out, 0, sizeof out);
    out.wo_disp4c = out.wo_disp48 = out.wo_unk4 = out.wo_unk6 = -1;
    assert(aos_object20_whiteout_update(&s, &in, &out) == 1);
    assert(out.wo_unk6 == 0);
    s.sub_b = 7;
    memset(&out, 0, sizeof out);
    out.wo_disp4c = out.wo_disp48 = out.wo_unk4 = out.wo_unk6 = -1;
    assert(aos_object20_whiteout_update(&s, &in, &out) == 0);
    assert(out.wo_unk6 == 0);
    assert(out.wo_unk4 < 0);

    /* ---- param0 31: update sub_0805C628. ---- */
    memset(&s, 0, sizeof s);
    in = base_in();
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 31, &in, &out);
    /* state 0: +0xD armed 0x80 then counted to 0x7F. */
    assert(s.state == 0 && s.timer_d == 0x7F && s.sub_c == 1);
    assert(out.anim_step_count == 1);
    /* wrap -> fades + state 1. */
    s.timer_d = 0;
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 31, &in, &out);
    assert(s.state == 1);
    assert(out.fade_3d18c && out.fade_3cdf0);
    /* state 1: cf0 once, fade gate clear -> advance. */
    in.fade_done = false;
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 31, &in, &out);
    assert(out.cf0 == 0x21);
    assert(s.state == 2);
    assert(out.fade_out && out.dma_setup);
    assert(out.unk2cb0_or100);
    /* state 2 +0xB == 0 hold -> player park on wrap (skip the
     * +0xC == 0 init by presetting the armed counter). */
    memset(&s, 0, sizeof s);
    s.state = 2; s.sub_c = 1; s.timer_d = 0;
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 31, &in, &out);
    assert(out.player_scr42_val == 0xD0 - 0x40);
    assert(out.player_pos44_val == 0xBF - 0x30);
    assert(s.sub_b == 1);
    /* +0xB == 2 floor probe: bit2 clear slides +0x44, set
     * does not. */
    s.sub_b = 2; s.sub_c = 1; s.scr42 = 0x100;
    s.pos44 = 0; s.vel4c = 0x10;
    in.ret_6e140 = 0;
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 31, &in, &out);
    assert(s.pos44 == 0x10);
    in.ret_6e140 = 4;
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 31, &in, &out);
    assert(s.pos44 == 0x10);
    /* +0xB == 3 +0x59 release jumps +0xB by 2. */
    s.sub_b = 3; s.flags59 = 1; s.anim6d = 0;
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 31, &in, &out);
    assert(s.sub_b == 5);
    /* +0xB == 4 song picks + +0xB by 1 on the +0x59 release. */
    s.sub_b = 4; s.field6e = 4;
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 31, &in, &out);
    assert(out.song == 0x1DF);
    assert(s.sub_b == 5);
    s.sub_b = 4; s.field6e = 8; s.flags59 = 0;
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 31, &in, &out);
    assert(out.song == 0xFC);
    assert(s.sub_b == 4);
    /* states 3-6: poll, child hand-off, f6c, finale. */
    memset(&s, 0, sizeof s);
    s.state = 3;
    in.text_done = true;
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 31, &in, &out);
    assert(s.state == 4);
    /* state 4 wrap: preset the armed counter at 0. */
    s.sub_c = 1; s.timer_d = 0;
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 31, &in, &out);
    assert(s.state == 5);
    assert(s.child29.state == 1);
    assert(s.child29.sub_b == 0 && s.child29.ctr_c == 0);
    s.child29.state = 0x80;
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 31, &in, &out);
    assert(out.f6c == 0x1040);
    memset(&s, 0, sizeof s);
    s.state = 6;
    in.fade_done = false;
    memset(&out, 0, sizeof out);
    aos_object20_story_update(&s, 31, &in, &out);
    assert(s.state == 0x80);
    assert(out.jingle == 0x2D);
    assert(out.unk2cb0_feff);
    assert(out.window_close);
    assert(out.write_64_0a);

    /* ---- child29 update (sub_0805C39C). ---- */
    memset(&s, 0, sizeof s);
    s.child29.state = 0;
    memset(&out, 0, sizeof out);
    in = base_in();
    aos_object20_story_child29_update(&s, &in, &out);
    assert(out.child_anim_step_count == 1);
    /* state 1: clears +0x59 bit 1 once, waits for bit 0 —
     * anim step while waiting, advance when bit 0 set. */
    s.child29.state = 1;
    s.child29.flags59 = 0x2;
    memset(&out, 0, sizeof out);
    aos_object20_story_child29_update(&s, &in, &out);
    assert((s.child29.flags59 & 2) == 0);
    assert(s.child29.state == 1);
    assert(out.child_anim_step_count == 1); /* bit 0 still clear */
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

    /* state 3 with the injected 0x085267B8 table: the
     * sub_0805C328 counter/threshold/index state machine.
     * Records: {u32 arg, u16 arg, u16 threshold} x4. */
    static const uint8_t c328_tab[4 * 8] = {
        1, 0, 0, 0, 0x10, 0, 2, 0,   /* rec0 thresh 2 */
        2, 0, 0, 0, 0x11, 0, 1, 0,   /* rec1 thresh 1 */
        3, 0, 0, 0, 0x12, 0, 0, 0,   /* rec2 thresh 0 */
        4, 0, 0, 0, 0x13, 0, 0, 0,   /* rec3 thresh 0 */
    };
    memset(&s, 0, sizeof s);
    s.child29.state = 3;
    in = base_in();
    in.c328_table_085267b8 = c328_tab;
    /* below threshold: +0x18 counts, no effects, stays 3. */
    memset(&out, 0, sizeof out);
    aos_object20_story_child29_update(&s, &in, &out);
    assert(s.child29.state == 3 && s.child29.field18 == 1 &&
           s.child29.field1a == 0);
    assert(!out.c328_c7b4 && !out.c328_cf34);
    aos_object20_story_child29_update(&s, &in, &out);
    aos_object20_story_child29_update(&s, &in, &out);
    assert(s.child29.field18 == 3);
    /* 4th call: old 3 > thresh 2 -> reset, C7B4 + CF34 on
     * rec1, +0x1A -> 1. */
    memset(&out, 0, sizeof out);
    aos_object20_story_child29_update(&s, &in, &out);
    assert(s.child29.state == 3 && s.child29.field18 == 0 &&
           s.child29.field1a == 1);
    assert(out.c328_c7b4 && out.c328_cf34);
    /* rec1 thresh 1: two waits then a crossing. */
    aos_object20_story_child29_update(&s, &in, &out);
    aos_object20_story_child29_update(&s, &in, &out);
    assert(s.child29.field1a == 1 && s.child29.field18 == 2);
    memset(&out, 0, sizeof out);
    aos_object20_story_child29_update(&s, &in, &out);
    assert(s.child29.field1a == 2 && out.c328_cf34);
    /* rec2 thresh 0: one wait then crossing -> rec3. */
    aos_object20_story_child29_update(&s, &in, &out);
    memset(&out, 0, sizeof out);
    aos_object20_story_child29_update(&s, &in, &out);
    assert(s.child29.field1a == 3 && out.c328_cf34);
    /* rec3 thresh 0: one wait then crossing wraps +0x1A ->
     * sub_0805C328 returns 0 -> child parks at 0x80. */
    aos_object20_story_child29_update(&s, &in, &out);
    memset(&out, 0, sizeof out);
    aos_object20_story_child29_update(&s, &in, &out);
    assert(s.child29.state == 0x80 && s.child29.field1a == 0 &&
           s.child29.sub_b == 0 && s.child29.ctr_c == 0);
    assert(out.c328_c7b4 && !out.c328_cf34);

    /* ---- shared create gates. ---- */
    memset(&s, 0, sizeof s);
    in = base_in();
    in.flag1327a = 0;
    in.flag42c = 0;            /* no 0x00200000, no +0x1327A -> del */
    assert(!aos_object20_story_create(&s, 20, &in, &out));
    /* +0x8E bit 0x80 kills even a passing gate. */
    in = base_in();
    in.byte8e = 0x80;
    assert(!aos_object20_story_create(&s, 20, &in, &out));
    /* +0x13266 set without +0x42C 0x8000 kills. */
    in = base_in();
    in.byte13266 = 1;
    assert(!aos_object20_story_create(&s, 20, &in, &out));
    in.flag42c = 0x8001;    /* 0x8000 passes the +0x13266 gate,
                             * bit 0 arms the SetPlayerInput tail */
    assert(aos_object20_story_create(&s, 20, &in, &out));
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

    /* ---- children_update: the per-child update vectors.
     * param0 9's +0x14/+0x18 children keep the sub_0805B748
     * anim-step update; param0 23's escort child the same;
     * param0 31 runs child29 (sub_0805C39C) + the grandchild
     * chain (sub_0805C284/sub_0805CB08). ---- */
    memset(&s, 0, sizeof s);
    s.child14 = true; s.link18 = true;
    in = base_in();
    memset(&out, 0, sizeof out);
    aos_object20_story_children_update(&s, 9, &in, &out);
    assert(out.child_anim_step_count == 2);
    s.link18 = false;
    memset(&out, 0, sizeof out);
    aos_object20_story_children_update(&s, 9, &in, &out);
    assert(out.child_anim_step_count == 1);
    /* param0 23 escort child: one anim step. */
    memset(&s, 0, sizeof s);
    s.child14 = true;
    memset(&out, 0, sizeof out);
    aos_object20_story_children_update(&s, 23, &in, &out);
    assert(out.child_anim_step_count == 1);
    /* param0 29 has no child: nothing runs. */
    memset(&s, 0, sizeof s);
    s.child14 = true;
    memset(&out, 0, sizeof out);
    aos_object20_story_children_update(&s, 29, &in, &out);
    assert(out.child_anim_step_count == 0 &&
           out.anim_step_count == 0);
    /* param0 31 child29 state 0: the child's own anim step. */
    memset(&s, 0, sizeof s);
    s.child14 = true;
    s.child29.state = 0;
    memset(&out, 0, sizeof out);
    aos_object20_story_children_update(&s, 31, &in, &out);
    assert(out.child_anim_step_count == 1);

    /* child29 state 2 spawns the sub_0805C284 grandchild:
     * +0x42/+0x46 = the child's screen words - 0xB/-0x10,
     * +0x48 = 0xFFFF6000, +0x4C = 0x2000, +0x50 = 0x100,
     * +0x54 = 0x1000, +0x65 = 3. */
    memset(&s, 0, sizeof s);
    s.child14 = true;
    s.child14_scr42 = 0x70;
    s.child14_scr46 = 0x60;
    s.child29.state = 2;
    s.child29.ctr_c = 0;
    in = base_in();
    in.spawn14_ok = true;
    memset(&out, 0, sizeof out);
    aos_object20_story_child29_update(&s, &in, &out);
    assert(s.gc.spawned);
    assert(s.gc.state == 0 && s.gc.anim65 == 3);
    assert(s.gc.scr42 == (int16_t)(0x70 - 0xB));
    assert(s.gc.scr46 == (int16_t)(0x60 - 0x10));
    assert(s.gc.vel48 == (int32_t)0xFFFF6000);
    assert(s.gc.vel4c == 0x2000);
    assert(s.gc.acc50 == 0x100 && s.gc.acc54 == 0x1000);
    /* failed spawn: gc stays inert but the child still arms. */
    memset(&s, 0, sizeof s);
    s.child29.state = 2; s.child29.ctr_c = 0;
    in.spawn14_ok = false;
    memset(&out, 0, sizeof out);
    aos_object20_story_child29_update(&s, &in, &out);
    assert(!s.gc.spawned && s.child29.timer_d == 0x1F);

    /* gc_update state 0, probe miss: velocity integration
     * with the +0x50/+0x54 acceleration and the positive
     * +0x48 -> 0 clamp (the probe result on this path). */
    memset(&s, 0, sizeof s);
    s.gc.spawned = true;
    s.gc.state = 0;
    s.gc.scr42 = 0x40; s.gc.scr46 = 0x40;
    s.gc.vel48 = 0x10; s.gc.vel4c = 0x20;
    s.gc.acc50 = 0x100; s.gc.acc54 = 0x1000;
    in = base_in();
    in.probe_020a0 = probe_zero;
    memset(&out, 0, sizeof out);
    aos_object20_story_gc_update(&s, &in, &out);
    assert(s.gc.pos40 == 0x10 && s.gc.pos44 == 0x20);
    /* vel48 = 0x10 + 0x100 = 0x110 > 0 -> clamped to the
     * probe result (0) on the same frame; vel4c keeps its
     * acceleration. */
    assert(s.gc.vel48 == 0 && s.gc.vel4c == 0x1020);
    memset(&out, 0, sizeof out);
    aos_object20_story_gc_update(&s, &in, &out);
    assert(s.gc.vel48 == 0);
    assert(s.gc.state == 0 && out.song == 0);   /* no PlaySong */

    /* gc_update state 0, probe hit: the sub_0805C218 burst +
     * PlaySong(0xAA) + land at state 1. */
    static const int8_t debris_tab[12 * 8] = {
        /* rec0: +0x65=3, dx=1, dy=-2, vx=1<<12, vy=-(2<<12), d=0x20 */
        3, 1, -2, 1, 2, 0x20, 0, 0,
        /* rec1..11: distinct fields to verify indexing */
        4, -3, 5, -1, 3, 0x10, 0, 0,
        5, 0, 0, 2, -4, 0x08, 0, 0,
        6, 7, -7, 4, 1, 0x04, 0, 0,
        7, -8, 9, -2, -2, 0x02, 0, 0,
        8, 10, -10, 1, 1, 0x01, 0, 0,
        9, -11, 11, -3, -3, 0x00, 0, 0,
        10, 12, -12, 2, 2, 0x30, 0, 0,
        11, -13, 13, -4, -4, 0x28, 0, 0,
        12, 14, -14, 3, 3, 0x18, 0, 0,
        13, -15, 15, -5, -5, 0x08, 0, 0,
        14, 16, -16, 5, 5, 0x38, 0, 0,
    };
    memset(&s, 0, sizeof s);
    s.gc.spawned = true;
    s.gc.state = 0;
    s.gc.scr42 = 0x50; s.gc.scr46 = 0x50;
    in = base_in();
    in.probe_020a0 = probe_one;
    in.spawn14_ok = true;
    in.debris_table_08526758 = debris_tab;
    memset(&out, 0, sizeof out);
    aos_object20_story_gc_update(&s, &in, &out);
    assert(s.gc.state == 1 && s.gc.sub_b == 0 && s.gc.ctr_c == 0);
    assert(out.song == 0xAA);
    assert(out.debris_spawn_count == 12);
    assert(!out.debris_table_missing);
    /* rec0: scr = (0x50+1, 0x50-2-2) — the burst y arg is
     * +0x46 - 2 before the record offsets. */
    assert(s.gc.debris[0].active);
    assert(s.gc.debris[0].scr42 == (int16_t)(0x50 + 1));
    assert(s.gc.debris[0].scr46 == (int16_t)(0x50 - 2 - 2));
    assert(s.gc.debris[0].vel48 == (int32_t)(1 << 12));
    assert(s.gc.debris[0].vel4c == (int32_t)(-(2 << 12)));
    assert(s.gc.debris[0].acc54 == 0x2000);
    assert(s.gc.debris[0].timer_d == 0x20);
    assert(s.gc.debris[0].anim65 == 3);
    /* rec1: s8 fields sign-extend. */
    assert(s.gc.debris[1].scr42 == (int16_t)(0x50 - 3));
    assert(s.gc.debris[1].scr46 == (int16_t)(0x50 - 2 + 5));
    assert(s.gc.debris[1].vel48 == -(int32_t)(1 << 12));
    assert(s.gc.debris[1].vel4c == -(int32_t)(3 << 12));
    /* NULL table: burst still reports the 12 spawns. */
    memset(&s, 0, sizeof s);
    s.gc.spawned = true;
    in.debris_table_08526758 = NULL;
    memset(&out, 0, sizeof out);
    aos_object20_story_gc_update(&s, &in, &out);
    assert(out.debris_spawn_count == 12);
    assert(out.debris_table_missing);
    assert(!s.gc.debris[0].active);

    /* landed state: +0x65 = 8 arms once through +0xC; the
     * debris keep updating (probe hit marks +0x59 |= 8,
     * a miss integrates). */
    memset(&s, 0, sizeof s);
    s.gc.spawned = true;
    s.gc.state = 1;
    s.gc.scr42 = 0x40; s.gc.scr46 = 0x40;
    s.gc.debris[0].active = true;
    s.gc.debris[0].scr42 = 0x40; s.gc.debris[0].scr46 = 0x40;
    s.gc.debris[0].vel48 = 0x10; s.gc.debris[0].vel4c = 0x20;
    s.gc.debris[0].acc54 = 0x100;
    in = base_in();
    in.probe_020a0 = probe_mixed;  /* gc hit? state 1 skips
                                    * the probe; debris see
                                    * their own args */
    memset(&out, 0, sizeof out);
    aos_object20_story_gc_update(&s, &in, &out);
    assert(s.gc.anim65 == 8 && s.gc.ctr_c == 1);
    /* probe_mixed returns nonzero -> debris marked done. */
    assert(s.gc.debris[0].flags59 & 8);
    /* miss path: clear the mark, probe returns 0. */
    s.gc.debris[0].flags59 = 0;
    in.probe_020a0 = probe_zero;
    memset(&out, 0, sizeof out);
    aos_object20_story_gc_update(&s, &in, &out);
    assert(!(s.gc.debris[0].flags59 & 8));
    assert(s.gc.debris[0].pos40 == 0x10);
    assert(s.gc.debris[0].pos44 == 0x20);
    assert(s.gc.debris[0].vel4c == 0x120);
    assert(s.gc.debris[0].vel48 == 0x10);   /* +0x48 untouched */

    /* children_update param0 31 runs child29 + gc together. */
    memset(&s, 0, sizeof s);
    s.child14 = true;
    s.child29.state = 0;
    s.gc.spawned = true; s.gc.state = 1; s.gc.ctr_c = 1;
    in = base_in();
    memset(&out, 0, sizeof out);
    aos_object20_story_children_update(&s, 31, &in, &out);
    assert(out.child_anim_step_count == 1);       /* child29 st0 */
    assert(s.gc.anim65 == 8 || s.gc.ctr_c == 1);

    puts("aos_object20_story: all tests passed");
    return 0;
}
