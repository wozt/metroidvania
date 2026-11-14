#include "../src/runtime/aos/game/aos_object20_dirs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const uint8_t table_852682c[] = { 0x05, 0x03, 0xFF, 0x07 };

static AosObject20DirsIn mk_in(void) {
    AosObject20DirsIn in;
    memset(&in, 0, sizeof in);
    in.flag1327a = 1;               /* shared create gate pass */
    in.keep_5b754 = true;
    in.player_scr42 = 0x100;
    in.a094_6 = 0;
    in.a094_a = 0;
    in.a094_4 = 0;
    in.table_852682c = table_852682c;
    return in;
}

static void mk(AosObject20Dirs *s, AosObject20DirsIn *in,
               AosObject20DirsOut *out, uint8_t param0) {
    memset(s, 0, sizeof *s);
    int32_t saved = in->player_scr42;
    if (param0 == 4) in->player_scr42 = 0x40; /* create4 gate <= 0x80 */
    bool ok = aos_object20_dirs_create(s, param0, in, out);
    in->player_scr42 = saved;
    assert(ok);
}

int main(void) {
    AosObject20Dirs s;
    AosObject20DirsIn in;
    AosObject20DirsOut out;

    /* ---- shared create gates ---- */
    in = mk_in();
    in.flag1327a = 0;
    in.flag42c = 0;                 /* no 0x200000 -> delete */
    memset(&s, 0, sizeof s);
    assert(!aos_object20_dirs_create(&s, 4, &in, &out));
    assert(s.deleted);

    in = mk_in();
    in.byte8e = 0x80;               /* +0x8E bit7 -> delete */
    memset(&s, 0, sizeof s);
    assert(!aos_object20_dirs_create(&s, 4, &in, &out));

    in = mk_in();
    in.byte13266 = 1;               /* set without +0x42C 0x8000 */
    memset(&s, 0, sizeof s);
    assert(!aos_object20_dirs_create(&s, 4, &in, &out));

    in = mk_in();
    in.byte13266 = 1;
    in.flag42c = 0x8000;            /* gate passes now */
    in.player_scr42 = 0x40;         /* <= 0x80 survives create4 */
    memset(&s, 0, sizeof s);
    assert(aos_object20_dirs_create(&s, 4, &in, &out));

    /* unported param0 reports and survives the tail */
    in = mk_in();
    memset(&s, 0, sizeof s);
    assert(aos_object20_dirs_create(&s, 3, &in, &out));
    assert(out.unported && !s.deleted);

    /* ---- param0 4 create ---- */
    in = mk_in(); in.player_scr42 = 0x40;  /* <= 0x80 survives */
    mk(&s, &in, &out, 4);
    assert(!s.deleted);
    assert(s.scr42 == 0xA0 && s.scr46 == 0xC0);
    assert(s.vel48 == 0xC000);
    assert((s.flags58 & 0x41) == 0);
    assert(out.anim_count == 1 && out.anim_a[0] == 0 &&
           out.anim_b[0] == 0 && out.anim_c[0] == 1);
    assert(out.keep_arg == 5 && out.anim_step_count == 1);
    assert(out.set_42c == (0x200u | 1u));
    assert(out.call_0eb04 && out.call_235c8 && out.call_2dff0 &&
           out.call_6666c && out.c5a8 == 0);
    assert(out.setplayerinput == 1 && out.setplayerinput_arg == 0);

    /* param0 4 mark paths */
    in = mk_in(); in.flag12078_21 = 1;
    memset(&s, 0, sizeof s);
    assert(!aos_object20_dirs_create(&s, 4, &in, &out));
    assert(s.flags59 & 8);
    in = mk_in(); in.player_scr42 = 0x81;  /* > 0x80 -> mark */
    memset(&s, 0, sizeof s);
    assert(!aos_object20_dirs_create(&s, 4, &in, &out));
    in = mk_in(); in.keep_5b754 = false; in.player_scr42 = 0x40;
    memset(&s, 0, sizeof s);
    assert(!aos_object20_dirs_create(&s, 4, &in, &out));
    assert(out.keep_arg == 5);

    /* ---- param0 5 create ---- */
    in = mk_in();
    mk(&s, &in, &out, 5);
    assert(s.scr42 == 0xA0 && s.scr46 == 0xA0);
    assert((s.flags58 & 0x41) == 0);
    assert(out.keep_arg == 2);
    in = mk_in(); in.flag12078_22 = 1;
    memset(&s, 0, sizeof s);
    assert(aos_object20_dirs_create(&s, 5, &in, &out));
    assert(out.release_arg == 1);       /* sub_0805B66C(e,1) */

    /* ---- param0 6 create ---- */
    in = mk_in();
    mk(&s, &in, &out, 6);
    assert(s.scr42 == 0x50 && s.scr46 == 0xC0);
    assert(s.flags58 & 0x40);
    assert(out.unk7864_or2);
    assert(out.keep_arg == 6);

    /* ---- param0 7 create ---- */
    in = mk_in();
    mk(&s, &in, &out, 7);
    assert(s.scr42 == 0xBE && s.scr46 == 0xA0);
    assert(s.flags58 & 0x40);
    assert(out.unk7864_or2 && out.dma_setup_786c);
    assert(out.call_6af98 && out.player58_or_40);
    assert(out.keep_arg == 5);
    in = mk_in(); in.player_scr42 = 0x1F; /* <= 0x1F -> mark */
    memset(&s, 0, sizeof s);
    assert(!aos_object20_dirs_create(&s, 7, &in, &out));

    /* ---- param0 8 create ---- */
    in = mk_in();
    mk(&s, &in, &out, 8);
    assert(s.scr42 == 0x50 && s.scr46 == 0xA0);
    assert(s.flags58 & 0x40);
    assert(out.keep_arg == 2);

    /* ================= updates ================= */

    /* ---- param0 4 update: state 0 -> window 0xF ---- */
    mk(&s, &in, &out, 4);
    s.state = 0; s.sub_b = 0; s.timer_d = 0;
    in = mk_in(); in.flag42c = 0;
    int ret = aos_object20_dirs_update(&s, 4, &in, &out);
    assert(ret == 0);
    assert(out.window_text == 0xF && out.b878 == 0x1040);
    assert(out.write_417 && out.write_417_val == 1);
    assert(s.state == 0 && s.sub_b == 1 && s.timer_d == 0x1F);
    /* 0x1F more decrements wraps -> state 1 */
    for (int i = 0; i < 0x20; ++i)
        aos_object20_dirs_update(&s, 4, &in, &out);
    assert(s.state == 1 && s.sub_b == 0 && s.sub_c == 0);

    /* state 1, sub_b 0, sub_c 0 -> 1 with 0x30 timer (decremented
     * same call to 0x2F) */
    aos_object20_dirs_update(&s, 4, &in, &out);
    assert(out.window_poll);
    assert(s.sub_c == 1 && s.timer_d == 0x2F);
    /* run the timer out: 0x2F more calls wraps to sub_c 2 */
    for (int i = 0; i < 0x30; ++i)
        aos_object20_dirs_update(&s, 4, &in, &out);
    assert(s.sub_c == 2);
    /* sub_c 2 with player px 0x100 > 0x56 -> clamp + advance */
    aos_object20_dirs_update(&s, 4, &in, &out);
    assert(out.player58_andnot_41 && out.player_scr42_val == 0x57);
    assert(s.sub_c == 3);
    /* sub_c 3 -> cf0, phase reset to sub_b 1 */
    aos_object20_dirs_update(&s, 4, &in, &out);
    assert(out.cf0 == 0x16 && s.sub_b == 1 && s.sub_c == 0);
    /* sub_b 1 sub_c 0 -> index14 = 0, timer 0xFF, sub_c 1 */
    aos_object20_dirs_update(&s, 4, &in, &out);
    assert(s.index14 == 0 && s.timer_d == 0xFF && s.sub_c == 1);
    /* force the wrap: timer_d 0 -> 0xFF, table[0] = 5 anim */
    s.timer_d = 0;
    aos_object20_dirs_update(&s, 4, &in, &out);
    assert(out.anim_count == 1 && out.anim_a[0] == 5);
    assert(s.index14 == 1 && s.timer_d == 0xFF);
    /* table[1] = 3, then table[2] = 0xFF resets index14 */
    s.timer_d = 0;
    aos_object20_dirs_update(&s, 4, &in, &out);
    assert(out.anim_a[0] == 3 && s.index14 == 2);
    s.timer_d = 0;
    aos_object20_dirs_update(&s, 4, &in, &out);
    assert(out.anim_a[0] == 0 && s.index14 == 0);
    /* sub_b 2 sub_c 0 -> sound, facing, anim, falls into the
     * move check: scr42 0xA0 <= 0xFF -> pos40 += vel48 */
    s.sub_b = 2; s.sub_c = 0; s.vel48 = 0x1000; s.pos40 = 0x5000;
    s.scr42 = 0xA0; s.anim6d = 1;             /* anim6d==1 skips F2C8 */
    aos_object20_dirs_update(&s, 4, &in, &out);
    assert(out.b878 == 0x1040 && out.write_417_val == 1);
    assert(s.flags58 & 0x40);
    assert(s.sub_c == 1 && s.pos40 == 0x6000);
    /* scr42 > 0xFF -> timer 0x10 + advance instead of moving */
    s.scr42 = 0x200; s.pos40 = 0x7000;
    aos_object20_dirs_update(&s, 4, &in, &out);
    assert(s.timer_d == 0x10 && s.sub_c == 2 && s.pos40 == 0x7000);
    /* timer wrap -> 417 = 0, +0x414 write, seq18 = 4 */
    s.timer_d = 0;
    aos_object20_dirs_update(&s, 4, &in, &out);
    assert(out.write_417_val == 0);
    assert(out.write_414 && out.write_414_val == 5);
    assert(s.seq18 == 4 && s.sub_c == 3);
    /* state 0x80 -> release path, no anim step */
    s.state = 0x80;
    aos_object20_dirs_update(&s, 4, &in, &out);
    assert(out.call_13960 && out.jingle == 0x21 &&
           out.release_arg == 1);
    assert(out.anim_step_count == 0);

    /* ---- param0 5 update ---- */
    mk(&s, &in, &out, 5);
    s.state = 1; s.sub_b = 0; s.sub_c = 0;
    aos_object20_dirs_update(&s, 5, &in, &out);
    assert(s.sub_c == 1 && s.timer_d == 0x2F);
    for (int i = 0; i < 0x30; ++i)
        aos_object20_dirs_update(&s, 5, &in, &out);
    assert(s.sub_c == 2);
    /* player px 0x100 > 0x4F -> clamp 0x50 + advance */
    aos_object20_dirs_update(&s, 5, &in, &out);
    assert(out.player_scr42_val == 0x50 && s.sub_c == 3);
    /* sub_c 3 -> cf0 + 417, counters NOT reset */
    aos_object20_dirs_update(&s, 5, &in, &out);
    assert(out.cf0 == 0x16 && s.sub_b == 0 && s.sub_c == 3);
    /* sub_b 1: facing mirror from PLAYER bit6, vel48 = 0x8000,
     * falls into the move check (scr42 0xA0 <= 0xFF -> move) */
    in.player_flags58 = 0x40;
    s.sub_b = 1; s.sub_c = 0; s.vel48 = 0; s.pos40 = 0x9000;
    s.scr42 = 0xA0;
    aos_object20_dirs_update(&s, 5, &in, &out);
    assert(out.b878 == 0x1040 && s.vel48 == 0x8000);
    assert((s.flags58 & 0x40) == 0);        /* player bit6 set ->
                                             * own bit6 cleared */
    assert(s.pos40 == 0x11000 && s.sub_c == 1);
    /* scr42 > 0xFF -> advance without moving */
    s.scr42 = 0x200; s.pos40 = 0xA000;
    aos_object20_dirs_update(&s, 5, &in, &out);
    assert(s.sub_c == 2 && s.pos40 == 0xA000);
    /* sub_c 2 -> 417 = 0 + +0x414, advance to 3 */
    aos_object20_dirs_update(&s, 5, &in, &out);
    assert(out.write_417_val == 0 && out.write_414_val == 5);
    assert(s.sub_c == 3);
    s.state = 0x80;
    aos_object20_dirs_update(&s, 5, &in, &out);
    assert(out.jingle == 0x22 && out.release_arg == 1);

    /* ---- param0 6 update ---- */
    mk(&s, &in, &out, 6);
    /* state 0 init falls into state 1 same call */
    s.state = 0;
    ret = aos_object20_dirs_update(&s, 6, &in, &out);
    assert(out.dma_setup && out.window_text == 0x11);
    assert(out.write_417_val == 1 && s.state == 1);
    /* sub_b 0 sub_c 0: double step to 2 + escort block.
     * a094_4 = 0 -> write; px 0x100 > lim 0xAE -> ret 0x20. */
    assert(ret == 0x20);
    assert(out.write_a094_4 && out.write_a094_4_val == 0);
    assert(out.write_427_val == 1);
    assert(s.sub_c == 2);
    /* px <= lim -> clamp to lim, cf0 + 417 = 0, sub_c 3 */
    in.player_scr42 = 0x50;
    aos_object20_dirs_update(&s, 6, &in, &out);
    assert(out.player_scr42_val == 0xAE);
    assert(out.cf0 == 0x16 && out.write_417_val == 0);
    assert(s.sub_c == 3);
    /* a094_4 > 0 -> ed24 instead of the write (re-enter the
     * block via sub_c 2) */
    in.a094_4 = 0x1234; in.player_scr42 = 0x40;
    s.sub_c = 2;
    aos_object20_dirs_update(&s, 6, &in, &out);
    assert(out.ed24_a == 1 && out.ed24_b == (int32_t)0xFFFE8000);
    /* sub_b 1: facing clear + anim, falls into probe tail —
     * scr42 0 > -0x10 -> probe (ret&4==0 -> y move), px!=0x78 */
    s.sub_b = 1; s.sub_c = 0; s.vel4c = 0x800; s.pos44 = 0x1000;
    s.scr42 = 0; s.anim6d = 1;
    in.probe_6e140 = 0;
    aos_object20_dirs_update(&s, 6, &in, &out);
    assert(out.call_6e140 && s.pos44 == 0x1800);
    assert(out.ed24_a == 1 && out.ed24_b == 0x10000);
    assert(s.sub_c == 1);
    /* px == 0x78 -> 417 = 0 + 414 + advance */
    in.player_scr42 = 0x78;
    s.scr42 = -0x20;                    /* <= -0x10 skips probe */
    aos_object20_dirs_update(&s, 6, &in, &out);
    assert(!out.call_6e140);
    assert(out.write_417_val == 0 && out.write_414_val == 5);
    assert(s.sub_c == 2);
    s.state = 0x80;
    aos_object20_dirs_update(&s, 6, &in, &out);
    assert(out.jingle == 0x23 && out.release_arg == 1 &&
           out.write_427_val == 0);

    /* ---- param0 7 update ---- */
    mk(&s, &in, &out, 7);
    s.state = 0;
    aos_object20_dirs_update(&s, 7, &in, &out);
    assert(out.player_vel_cleared);
    assert(out.window_text == 0x12);
    /* state-0 fallthrough: sub_b0 sub_c0 -> double step + block;
     * a094_6 = 0 <= 0x94 -> cf0, 417 = 0, sub_c 3 same call */
    assert(out.cf0 == 0x16 && out.write_417_val == 0);
    assert(s.sub_c == 3 && out.write_427_val == 1);
    /* sub_b 1 sub_c 0: facing invert own bit6, anim 8, falls
     * into probe tail — scr42 -0x20 <= -0x10 -> advance+417=0 */
    s.sub_b = 1; s.sub_c = 0; s.anim6d = 8; s.scr42 = -0x20;
    s.flags58 = 0x40;
    aos_object20_dirs_update(&s, 7, &in, &out);
    assert((s.flags58 & 0x40) == 0);        /* bit6 inverted off */
    assert(s.sub_c == 2 && out.write_417_val == 0);
    /* probe path: scr42 0 > -0x10, probe&4 set -> no y move */
    s.sub_c = 1; s.scr42 = 0; s.pos44 = 0x2000; s.vel4c = 0x400;
    in.probe_6e140 = 4;
    aos_object20_dirs_update(&s, 7, &in, &out);
    assert(out.call_6e140 && s.pos44 == 0x2000 && s.sub_c == 1);
    /* sub_b 2: ed24 0x8000 + sound, px 0x78 -> finish */
    s.sub_b = 2; s.sub_c = 0;
    in.player_scr42 = 0x78;
    aos_object20_dirs_update(&s, 7, &in, &out);
    assert(out.ed24_b == 0x8000 && out.b878 == 0x1040);
    assert(out.write_417_val == 0 && out.write_414_val == 5);
    assert(s.sub_c == 1);
    s.state = 0x80;
    aos_object20_dirs_update(&s, 7, &in, &out);
    assert(out.jingle == 0x24 && out.release_arg == 1 &&
           out.write_427_val == 0);

    /* ---- param0 8 update ---- */
    in = mk_in();                      /* player_scr42 back to 0x100 */
    mk(&s, &in, &out, 8);
    s.state = 0; s.sub_b = 0; s.timer_d = 0;
    aos_object20_dirs_update(&s, 8, &in, &out);
    assert(out.window_text == 0x13 && s.timer_d == 0x1F);
    for (int i = 0; i < 0x20; ++i)
        aos_object20_dirs_update(&s, 8, &in, &out);
    assert(s.state == 1 && s.sub_b == 0 && s.sub_c == 0);
    /* sub_c 0 -> 417, C 1, timer 0x30->0x2F (fallthrough) */
    aos_object20_dirs_update(&s, 8, &in, &out);
    assert(s.sub_c == 1 && s.timer_d == 0x2F);
    for (int i = 0; i < 0x30; ++i)
        aos_object20_dirs_update(&s, 8, &in, &out);
    assert(s.sub_c == 2);
    /* px 0x100 > 0xA0 -> ret 0x20 */
    ret = aos_object20_dirs_update(&s, 8, &in, &out);
    assert(ret == 0x20 && s.sub_c == 2);
    /* px <= 0xA0 -> clamp player, cf0, advance */
    in.player_scr42 = 0x80;
    aos_object20_dirs_update(&s, 8, &in, &out);
    assert(out.player_scr42_val == 0xA0 && out.cf0 == 0x16);
    assert(s.sub_c == 3);
    /* sub_b 1 sub_c 0 -> PlaySong(0x1B7) once */
    s.sub_b = 1; s.sub_c = 0;
    aos_object20_dirs_update(&s, 8, &in, &out);
    assert(out.song == 0x1B7 && s.sub_c == 1);
    aos_object20_dirs_update(&s, 8, &in, &out);
    assert(out.song == -1);                 /* c==1 waits */
    /* sub_b 2 sub_c 0: vel48 = -0x8000, anim, falls into move —
     * scr42 0 >= -8 -> pos40 += vel48 */
    s.sub_b = 2; s.sub_c = 0; s.vel48 = 0; s.pos40 = 0x4000;
    s.scr42 = 0; s.anim6d = 1;
    aos_object20_dirs_update(&s, 8, &in, &out);
    assert(s.vel48 == (int32_t)0xFFFF8000);
    assert(s.pos40 == 0x4000 - 0x8000 && s.sub_c == 1);
    /* scr42 < -8 -> 417 = 0 + 414 + advance */
    s.scr42 = -0x10;
    aos_object20_dirs_update(&s, 8, &in, &out);
    assert(out.write_417_val == 0 && out.write_414_val == 5);
    assert(s.sub_c == 2);
    /* state 0x80 -> release, NO anim step */
    s.state = 0x80;
    aos_object20_dirs_update(&s, 8, &in, &out);
    assert(out.jingle == 0x25 && out.release_arg == 1);
    assert(out.anim_step_count == 0);

    /* ---- shared update prologue ---- */
    memset(&s, 0, sizeof s); s.state = 0x80;   /* s8 < 0 */
    in = mk_in(); in.byte41b = 1; in.flag42c = 0;
    aos_object20_dirs_update(&s, 4, &in, &out);
    assert(out.call_67804);
    /* state >= 0 + word16&8 + word60&2 + !0x10000 -> +0x414 */
    memset(&s, 0, sizeof s); s.state = 1;
    in.word16 = 8; in.word60 = 2; in.byte414 = 0x30;
    aos_object20_dirs_update(&s, 4, &in, &out);
    assert(out.write_414 && out.write_414_val == 0x25); /* (0x30&~0x10)|5 */
    /* +0x42C bit0 -> SetPlayerInput(1, ret) + c5a8(0) */
    in.flag42c = 1;
    aos_object20_dirs_update(&s, 4, &in, &out);
    assert(out.setplayerinput == 1 && out.c5a8 == 0);
    in.flag42c = 0;
    aos_object20_dirs_update(&s, 4, &in, &out);
    assert(out.c5a8 == 1 && out.setplayerinput == -1);

    /* ---- sub_0805F1CC sequencer ---- */
    memset(&s, 0, sizeof s);
    in = mk_in();
    assert(aos_object20_dirs_seq18_update(&s, &in, &out) == 0);
    assert(out.song == 0x1010 && s.timer19 == 0xA0 && s.seq18 == 1);
    s.timer19 = 0;
    aos_object20_dirs_seq18_update(&s, &in, &out);
    assert(out.cf0 == 0x16 && s.seq18 == 2);
    /* seq18 2 is a no-op wait state */
    aos_object20_dirs_seq18_update(&s, &in, &out);
    assert(s.seq18 == 2 && out.song == -1);
    s.seq18 = 3;
    aos_object20_dirs_seq18_update(&s, &in, &out);
    assert(out.song == 0x1010 && s.seq18 == 4 && s.timer19 == 0xA0);
    s.timer19 = 0;
    aos_object20_dirs_seq18_update(&s, &in, &out);
    assert(out.call_13960 && s.seq18 == 5);
    assert(aos_object20_dirs_seq18_update(&s, &in, &out) == 1);
    s.seq18 = 6;                            /* > 5 -> return 0 */
    assert(aos_object20_dirs_seq18_update(&s, &in, &out) == 0);

    puts("aos_object20_dirs: ok");
    return 0;
}
