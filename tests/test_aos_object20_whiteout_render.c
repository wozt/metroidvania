/* Unit tests for src/runtime/aos/game/aos_object20_whiteout_render.c —
 * the sub_08064518 whiteout bar/tile renderer port. Covers the
 * +0x15 jump-table states 0-10, the 160-bar BgAffineSet geometry,
 * the state-2 jitter/schedule, the state-4/5 wipe-out exit and
 * the RNG round-trip through Out. */
#include "../src/runtime/aos/game/aos_object20_whiteout_render.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static AosWoRenderIn base_in(void) {
    AosWoRenderIn in;
    memset(&in, 0, sizeof in);
    in.rng = 0x12345678;         /* arbitrary LCG seed */
    in.ewram_7864 = 0;
    in.field10 = 0;
    return in;
}

static void step(AosWoRender *s, AosWoRenderIn *in, AosWoRenderOut *out) {
    aos_wo_render_update(s, in, out);
    in->rng = out->rng;          /* gEwramData+8 round-trips */
    in->ewram_7864 |= out->tail_7864_or1 ? 1 : 0;
}

int main(void) {
    AosWoRender s;
    AosWoRenderIn in;
    AosWoRenderOut out;

    /* ---- state 0: one-shot init then a 17-frame hold that ends
     * in PlaySong(0x166) and +0x15 += 2 -> 2. ---- */
    memset(&s, 0, sizeof s);
    s.field_15 = 0;
    in = base_in();
    step(&s, &in, &out);
    assert(out.hblank_setup && out.tail_7864_or1);
    assert(out.bars_drawn == AOS_WO_BAR_COUNT);
    assert(out.palette_copy);            /* sub_0803C7B4 once */
    assert(s.field_20 == 0xb8 && s.field_22 == 0x70);
    assert(s.field_1c == 0xb0 && s.field_1e == 0xb0);
    assert(s.scr_42 == 0xb8 && s.scr_46 == 0x70);
    assert(s.field_2c == 0x0e && s.field_24 == 0x0f);
    assert(s.field_16 == 1);
    /* bar i: x=+0x20, y=+0x22-i, bgx/bgy=+0x1C/+0x1E,
     * intensity = +0x14 << 8 = 0. */
    assert(out.bars[0].x == 0xb8 && out.bars[0].y == 0x70);
    assert(out.bars[159].y == (uint16_t)(0x70 - 159));
    assert(out.bars[0].bgx == 0xb0 && out.bars[0].bgy == 0xb0);
    assert(out.bars[0].intensity == 0);
    assert(out.song == -1);
    /* 16 more frames: +0x24 0xF -> -1 is 16 decrements; the 17th
     * call (this one already consumed the first) finishes. */
    for (int i = 0; i < 15; ++i) {
        step(&s, &in, &out);
        assert(out.bars_drawn == AOS_WO_BAR_COUNT);
        assert(out.song == -1);
    }
    step(&s, &in, &out);                  /* +0x24 hits -1 */
    assert(out.song == 0x166);            /* PlaySong(0xB3 << 1) */
    assert(s.field_15 == 2);
    assert(s.field_24 == 0 && s.field_16 == 0);

    /* ---- state 1: pure tail, no bars. ---- */
    memset(&s, 0, sizeof s);
    s.field_15 = 1;
    in = base_in();
    step(&s, &in, &out);
    assert(out.bars_drawn == 0);
    assert(out.tail_7864_or1);

    /* ---- state 2: init spawns + 0x29E-frame countdown. ---- */
    memset(&s, 0, sizeof s);
    s.field_15 = 2;
    in = base_in();
    step(&s, &in, &out);
    /* init loads +0x24 = 0x29E but the same call's tail
     * decrements it -> 0x29D. */
    assert(s.field_24 == 0x29d && s.field_16 == 1 && s.state_d == 1);
    assert(out.spawn_count == 2);
    assert(out.spawns[0].kind == AOS_WO_SPAWN_FX_63DF8);
    assert(out.spawns[1].kind == AOS_WO_SPAWN_FX_64198);
    /* FX_64198 child pos: s8(+0x26)-0x30 + +0x20, s8(+0x27)+8 +
     * +0x22 — computed BEFORE this frame's RNG block, so the
     * zeroed state gives (-0x30, +8) offsets off zero origins. */
    assert(out.spawns[1].scr_x == (int16_t)(0 - 0x30 + 0));
    assert(out.spawns[1].scr_y == (int16_t)(0 + 8 + 0));
    assert(out.bars_drawn == AOS_WO_BAR_COUNT);
    /* +0x24 == 0x29D is ODD -> jitter path active; the two RNG
     * draws must have moved the LCG state. */
    /* Run until +0x24 wraps to -1: 0x29F calls total, one done. */
    int frames = 1;
    while (s.field_15 == 2) {
        step(&s, &in, &out);
        ++frames;
        assert(frames < 1000);
    }
    assert(s.field_15 == 3);
    assert(s.field_24 == 0 && s.field_16 == 0 && s.state_d == 0);
    assert(frames == 0x29f);
    /* Spot-check a scheduled event: +0x24 == 0x40 spawns table
     * word 0; == 0x30/0x20 fire fades. Re-run state 2 watching
     * for the fade at +0x24 == 0x30 (frame index 0x29E-0x30). */
    memset(&s, 0, sizeof s);
    s.field_15 = 2;
    s.field_16 = 1;                  /* skip re-init spawns */
    s.field_24 = 0x31;
    in = base_in();
    step(&s, &in, &out);
    assert(!out.fade_pair);           /* 0x31 -> 0x30 after dec? no:
                                       * events test pre-dec value
                                       * 0x31, not 0x30 */
    step(&s, &in, &out);              /* now +0x24 == 0x30 */
    assert(out.fade_pair);
    assert(out.fade_s0 == 0x10000 && out.fade_s1 == 0x20);
    assert(s.field_24 == 0x2f);

    /* ---- state 3: +0x1C ramps on odd +0xD until 0xD0 -> +0x15=5. */
    memset(&s, 0, sizeof s);
    s.field_15 = 3;
    s.field_16 = 1;                    /* skip the one-shot init,
                                        * which would zero +0xD */
    s.field_1c = 0xcf;
    s.field_1e = 0x40;
    s.state_d = 1;                    /* odd -> increments this call */
    in = base_in();
    step(&s, &in, &out);
    assert(out.bars_drawn == AOS_WO_BAR_COUNT);
    assert(s.field_1c == 0xd0 && s.field_1e == 0x41);
    assert(s.field_15 == 3);           /* exits NEXT odd +0xD */
    assert(s.state_d == 2);            /* incremented after test */
    step(&s, &in, &out);               /* +0xD = 2: even, no ramp */
    assert(s.field_1c == 0xd0 && s.state_d == 3);
    step(&s, &in, &out);               /* +0xD = 3: odd, f1c==0xD0 */
    assert(s.field_15 == 5 && s.field_16 == 0 && s.field_18 == 0);

    /* ---- states 4/5: init once, then the wipe-out. Drive the
     * exit directly: even +0x24 with +0x14 already <= 0xC0. ---- */
    memset(&s, 0, sizeof s);
    s.field_15 = 5;
    s.field_16 = 1;
    s.field_24 = 0xa2;                /* > 0x9F, even */
    s.field_14 = 0xc0;                /* (u8)(0xC0-1)=0xBF <= 0xBF */
    s.field_1c = 0x100;
    s.field_1e = 8;
    in = base_in();
    step(&s, &in, &out);
    assert(out.bars_drawn == AOS_WO_BAR_COUNT);
    assert(!out.cycle_6447c);          /* exit frame skips 6447C */
    assert(s.field_15 == 7);           /* 5 + 2 */
    assert(s.field_3c & 2);
    assert(s.field_24 == 0 && s.field_18 == 0 && s.state_d == 0);
    /* odd +0x24 in the >0x9F region: no exit, +0x1C/+0x1E ramp. */
    memset(&s, 0, sizeof s);
    s.field_15 = 4;
    s.field_16 = 1;
    s.field_24 = 0xa1;
    s.field_14 = 0xff;
    s.field_1c = 0x100;
    s.field_1e = 8;
    in = base_in();
    step(&s, &in, &out);
    assert(s.field_15 == 4);
    assert(s.field_1c == 0x180 && s.field_1e == 4);
    assert(s.field_24 == 0xa2);
    assert(out.cycle_6447c);
    /* state4/5 init frame: spawn idx4 + fade(0xFF) + purge_547. */
    memset(&s, 0, sizeof s);
    s.field_15 = 4;
    in = base_in();
    step(&s, &in, &out);
    assert(s.field_16 == 1 && s.field_38 == 5);
    /* +0x14 init 0 is immediately re-decremented by the same
     * frame's +0x24 % 5 == 0 block (u8 wrap) -> 0xFF. */
    assert(s.field_14 == 0xff);
    assert(out.spawn_count == 1 &&
           out.spawns[0].kind == AOS_WO_SPAWN_TABLE_118B34 &&
           out.spawns[0].aux == 4);
    /* The committed 0x08118B34 table decodes aux 4 to the
     * sub_08063740 Thumb update pointer. */
    assert(out.spawns[0].fn_addr == 0x08063741);
    assert(out.fade_pair && out.fade_s1 == 0xff);
    assert(out.purge_547);

    /* State-2 scheduled spawn at +0x24 == 0x40: table word 0 ->
     * sub_08063420. */
    memset(&s, 0, sizeof s);
    s.field_15 = 2;
    s.field_16 = 1;
    s.field_24 = 0x40;
    in = base_in();
    step(&s, &in, &out);
    assert(out.spawn_count == 1 &&
           out.spawns[0].kind == AOS_WO_SPAWN_TABLE_118B34 &&
           out.spawns[0].aux == 0 &&
           out.spawns[0].fn_addr == 0x08063421);

    /* ---- sub_0806447C cycle: the committed 0x08526924 table.
     * State 4/5, odd +0x24 (no exit), +0x34 = 0 entry dwell 64:
     * +0x36 must exceed 64 (old > dwell) to fire. ---- */
    memset(&s, 0, sizeof s);
    s.field_15 = 4;
    s.field_16 = 1;
    s.field_24 = 0xa1;                /* odd, > 0x9F: no exit */
    s.field_14 = 0xff;
    s.field_1c = 0x100;
    s.field_1e = 8;
    s.field_2c = 0x0e;
    s.field_34 = 0;
    s.field_36 = 65;                  /* old 65 > dwell 64 -> fire */
    in = base_in();
    step(&s, &in, &out);
    assert(out.cycle_6447c && out.cycle.ran);
    assert(out.cycle.step);
    assert(out.cycle.index == 0 && out.cycle.next_index == 1);
    assert(out.cycle.data == 0x08119FDC && out.cycle.tile == 0 &&
           out.cycle.dwell == 64);
    /* next entry (index 1): tile 5, dwell 16; the sequence pair
     * arms on it with arg0 = entity +0x2C. */
    assert(out.cycle.next_data == 0x08119FDC &&
           out.cycle.next_tile == 5 && out.cycle.next_dwell == 16);
    assert(out.cycle.seq_arg0 == 0x0e);
    assert(out.cycle.copy_3c918 && out.cycle.copy_3c7b4);
    assert(out.cycle.seq_3d2d0 && out.cycle.seq_3cf34);
    assert(s.field_34 == 1 && s.field_36 == 0);
    /* Same frame next call: +0x36 restarted at 1, entry 1 dwell
     * 16 — old 0 <= 16, no step but the counter moves. */
    step(&s, &in, &out);
    assert(out.cycle.ran && !out.cycle.step);
    assert(s.field_36 == 1 && s.field_34 == 1);
    /* Push +0x36 past entry-1's dwell: old 17 > 16 fires with
     * index 1 -> next_index 2 (tile 6, dwell 16). */
    s.field_36 = 17;
    step(&s, &in, &out);
    assert(out.cycle.step && out.cycle.index == 1 &&
           out.cycle.next_index == 2 && out.cycle.tile == 5 &&
           out.cycle.next_tile == 6);
    /* +0x34 == 6 early-out: ran but +0x36 untouched, no step. */
    memset(&s, 0, sizeof s);
    s.field_15 = 4;
    s.field_16 = 1;
    s.field_24 = 0xa1;
    s.field_14 = 0xff;
    s.field_1c = 0x100;
    s.field_1e = 8;
    s.field_34 = 6;
    s.field_36 = 100;
    in = base_in();
    step(&s, &in, &out);
    assert(out.cycle.ran && !out.cycle.step);
    assert(s.field_34 == 6 && s.field_36 == 100);

    /* ---- states 6/7: +0x1C += 0x640/frame, +0xD wrap advances. -- */
    memset(&s, 0, sizeof s);
    s.field_15 = 6;
    s.field_1c = 0x1000;
    s.state_d = 0;                    /* 0 - 1 wraps to 0xFF */
    in = base_in();
    step(&s, &in, &out);
    assert(s.field_1c == 0x1640);
    assert(s.state_d == 0 && s.field_15 == 7); /* wrapped -> +1 */
    memset(&s, 0, sizeof s);
    s.field_15 = 7;
    s.field_1c = 0xff00;               /* <= 0xFFFE -> += 0x640,
                                        * strh wraps to 0x0540 */
    s.state_d = 1;                     /* 1 - 1 = 0: no wrap,
                                        * no advance */
    in = base_in();
    step(&s, &in, &out);
    assert(s.field_1c == 0x0540);
    assert(s.state_d == 0 && s.field_15 == 7);
    step(&s, &in, &out);                /* 0 - 1 = 0xFF -> advance */
    assert(s.field_15 == 8 && s.state_d == 0);

    /* ---- state 8: ramps +0x1E by 0x180 while +0xD == 0; clamps
     * at 0x4000 -> +0x15 = 9, +0xD = 0x10. ---- */
    memset(&s, 0, sizeof s);
    s.field_15 = 8;
    s.state_d = 0;
    s.field_1c = 0x3000;
    s.field_1e = 0x3f00;
    in = base_in();
    step(&s, &in, &out);
    assert(s.field_1c == 0x4000);      /* 0x3000+0x2000 sat */
    assert(s.field_1e == 0x4000);      /* 0x3F00+0x180 > 0x4000 */
    assert(s.field_15 == 9 && s.state_d == 0x10);
    /* state_d != 0 path: plain decrement, no ramp. */
    memset(&s, 0, sizeof s);
    s.field_15 = 8;
    s.state_d = 5;
    s.field_1e = 0x100;
    in = base_in();
    step(&s, &in, &out);
    assert(s.state_d == 4 && s.field_1e == 0x100);

    /* ---- state 9: +0xD wrap -> +0x15 = 0xA (the gate value). --- */
    memset(&s, 0, sizeof s);
    s.field_15 = 9;
    s.state_d = 1;                     /* 1-1=0: no wrap yet */
    in = base_in();
    step(&s, &in, &out);
    assert(s.state_d == 0 && s.field_15 == 9);
    assert(out.bars_drawn == 0);        /* state 9 draws no bars */
    step(&s, &in, &out);                /* 0-1=0xFF -> advance */
    assert(s.state_d == 0 && s.field_15 == 0x0a);

    /* ---- state 10: terminal hold, +0x1E = 0x4000/0x3000 by
     * old +0xD bit 2. ---- */
    memset(&s, 0, sizeof s);
    s.field_15 = 10;
    s.state_d = 4;                     /* bit2 set */
    in = base_in();
    step(&s, &in, &out);
    assert(out.bars_drawn == AOS_WO_BAR_COUNT);
    assert(s.field_1e == 0x4000 && s.state_d == 5);
    s.state_d = 1;
    step(&s, &in, &out);
    assert(s.field_1e == 0x3000 && s.state_d == 2);

    /* ---- +0x15 > 10: tail only. ---- */
    memset(&s, 0, sizeof s);
    s.field_15 = 11;
    in = base_in();
    step(&s, &in, &out);
    assert(out.bars_drawn == 0);
    assert(out.tail_7864_or1 && out.hblank_setup);

    /* ---- RNG determinism: same seed -> same jitter bytes. ---- */
    memset(&s, 0, sizeof s);
    s.field_15 = 2;
    s.field_16 = 1;
    s.field_24 = 0x29e;                /* odd -> mask1/base1 */
    in = base_in();
    step(&s, &in, &out);
    uint8_t jx1 = s.field_26, jy1 = s.field_27;
    memset(&s, 0, sizeof s);
    s.field_15 = 2;
    s.field_16 = 1;
    s.field_24 = 0x29e;
    in = base_in();
    step(&s, &in, &out);
    assert(s.field_26 == jx1 && s.field_27 == jy1);
    /* mask/base path: +0x24 <= 0xFF (even clears bytes, so use
     * odd value 0xFD) -> base 2 mask 3. */
    memset(&s, 0, sizeof s);
    s.field_15 = 2;
    s.field_16 = 1;
    s.field_24 = 0xfd;
    in = base_in();
    step(&s, &in, &out);
    /* field_26 = 2 - (rng&3) as u8: in {0xFF,0,1,2} */
    assert(s.field_26 == 0xff || s.field_26 <= 2);

    /* The committed 0x08118B34 table decodes aux 3 to the
     * sub_08063698 Thumb update pointer (ROM word 0x08063699). */
    {
        memset(&s, 0, sizeof s);
        s.field_15 = 2;
        s.field_16 = 1;
        s.field_24 = 0xc0;
        in = base_in();
        step(&s, &in, &out);
        assert(out.spawn_count == 1 &&
               out.spawns[0].aux == 3 &&
               out.spawns[0].fn_addr == 0x08063699);
    }

    /* ---- 0x08118B34 spawn-child updates (sub_08063420/3558/3600/
     * 3698/3740). ---- */
    {
        /* init: only TABLE_118B34 spawns with aux < 5 instantiate. */
        AosWoSpawn sp;
        memset(&sp, 0, sizeof sp);
        sp.kind = AOS_WO_SPAWN_FX_63DF8;
        sp.aux = 0; sp.scr_x = 10; sp.scr_y = 20;
        AosWoSpawnChild ch;
        assert(!aos_wo_spawn_child_init(&ch, &sp));
        sp.kind = AOS_WO_SPAWN_TABLE_118B34;
        sp.aux = 5;
        assert(!aos_wo_spawn_child_init(&ch, &sp));
        sp.aux = 2;
        assert(aos_wo_spawn_child_init(&ch, &sp));
        assert(ch.aux == 2 && ch.field_a == 0 && ch.field_c == 0 &&
               ch.scr_x == 10 && ch.scr_y == 20 && ch.field_59 == 0);

        AosWoSpawnChildIn cin;
        AosWoSpawnChildOut cout;
        memset(&cin, 0, sizeof cin);
        cin.rng = 0x12345678;

        /* aux 2 / sub_08063600: table 0x08118A94, 5 entries all
         * delay 0x20, fire = sub_08063168(s16(+0x42+dx),
         * s16(+0x46+dy)), no in-routine RNG. Entry 0 is {0,0,0x20}:
         * call 1 loads +0xD=0x20, +0xA=1; calls 2..0x21 count down
         * (0x20 -> 0xFF wrap advances); fire on the call after. */
        aos_wo_spawn_child_update(&ch, &cin, &cout);
        assert(cout.rng == cin.rng);        /* no draws yet */
        assert(ch.field_a == 1 && ch.field_d == 0x20);
        assert(!cout.done);
        int calls = 1;
        /* +0xD 0x20 -> 0 takes 0x21 calls; the wrap call sets
         * +0xA = 2; the next call fires. */
        while (ch.field_a != 2) {
            aos_wo_spawn_child_update(&ch, &cin, &cout);
            ++calls;
            assert(calls < 100);
            assert(!cout.burst_63168 && !cout.carrier_spawn &&
                   !cout.burst_632b0);
        }
        assert(ch.field_d == 0xff);
        aos_wo_spawn_child_update(&ch, &cin, &cout);
        assert(cout.burst_63168);
        assert(cout.burst_x == 10 && cout.burst_y == 20);
        assert(!cout.carrier_spawn && !cout.burst_632b0);
        assert(cout.rng == cin.rng);        /* aux 2 draws no RNG */
        assert(ch.field_a == 0 && ch.field_c == 1);
        assert(!cout.done);                 /* +0xC 1 != 5 */
        cin.rng = cout.rng;

        /* Last entry walk: aux-2 record 4 = {30,20,0x20}. Drive
         * the machine with +0xD = 0 so the countdown wraps to
         * 0xFF (and +0xA = 2) on the next call. */
        memset(&ch, 0, sizeof ch);
        ch.aux = 2; ch.scr_x = 10; ch.scr_y = 20;
        ch.field_c = 4; ch.field_a = 1; ch.field_d = 0;
        aos_wo_spawn_child_update(&ch, &cin, &cout); /* 0->0xFF,a=2 */
        assert(ch.field_a == 2);
        aos_wo_spawn_child_update(&ch, &cin, &cout); /* fire entry 4 */
        assert(cout.burst_63168);
        assert(cout.burst_x == (int16_t)(10 + 30) &&
               cout.burst_y == (int16_t)(20 + 20));
        assert(ch.field_c == 5 && (ch.field_59 & 8));
        assert(cout.done);
        /* Post-done calls: no events, done still reported. */
        aos_wo_spawn_child_update(&ch, &cin, &cout);
        assert(cout.done && !cout.burst_63168 &&
               !cout.carrier_spawn && !cout.burst_632b0);
        assert(cout.rng == cin.rng);

        /* aux 0 / sub_08063420: table 0x081189CC, 15 entries.
         * Even +0xA fire -> carrier spawn with variant RNG & 3.
         * Entry 0 = {0,0,0}: init d=0, one countdown call wraps,
         * next call fires. */
        memset(&ch, 0, sizeof ch);
        ch.aux = 0; ch.scr_x = 0xb8; ch.scr_y = 0x70;
        cin.rng = 0x12345678;
        uint32_t rng_before = cin.rng;
        aos_wo_spawn_child_update(&ch, &cin, &cout); /* +0xA 0->1 */
        assert(ch.field_a == 1 && ch.field_d == 0);
        aos_wo_spawn_child_update(&ch, &cin, &cout); /* wrap -> 2 */
        assert(ch.field_a == 2);
        uint32_t expect_rng = (rng_before >> 8) * 0x3243F6ADu +
                              0x1B0CB175u;
        aos_wo_spawn_child_update(&ch, &cin, &cout); /* fire */
        assert(cout.carrier_spawn);
        assert(cout.carrier_x == 0xb8 && cout.carrier_y == 0x70);
        assert(cout.carrier_variant == (uint8_t)(expect_rng & 3));
        assert(!cout.burst_63168 && !cout.burst_632b0);
        assert(cout.rng == expect_rng);      /* exactly one draw */
        cin.rng = cout.rng;
        assert(ch.field_c == 1 && ch.field_a == 0);

        /* Odd +0xA (unreachable via the machine but ported):
         * aux 0 -> sub_08063168 burst, NO rng draw. */
        ch.field_a = 3; ch.field_c = 0;
        aos_wo_spawn_child_update(&ch, &cin, &cout);
        assert(cout.burst_63168 && !cout.carrier_spawn);
        assert(cout.rng == cin.rng);
        assert(ch.field_c == 1 && ch.field_a == 0);

        /* aux 4 / sub_08063740 shares the aux-0 machine with
         * table 0x08118B0C (5 entries, delay 8). Last entry 4 =
         * {30,20,8} -> done after its fire. */
        memset(&ch, 0, sizeof ch);
        ch.aux = 4; ch.scr_x = 0; ch.scr_y = 0;
        ch.field_c = 4; ch.field_a = 2;      /* fire directly */
        cin.rng = 0x12345678;
        aos_wo_spawn_child_update(&ch, &cin, &cout);
        assert(cout.carrier_spawn);
        assert(cout.carrier_x == 30 && cout.carrier_y == 20);
        assert(ch.field_c == 5 && (ch.field_59 & 8) && cout.done);

        /* aux 1 / sub_08063558: table 0x08118A44, 10 entries,
         * delay 1, jittered sub_080632B0 fire (2 RNG draws). */
        memset(&ch, 0, sizeof ch);
        ch.aux = 1; ch.scr_x = 100; ch.scr_y = 50;
        ch.field_c = 0; ch.field_a = 2;      /* fire directly */
        cin.rng = 0x12345678;
        rng_before = cin.rng;
        aos_wo_spawn_child_update(&ch, &cin, &cout);
        assert(cout.burst_632b0 && !cout.burst_63168 &&
               !cout.carrier_spawn);
        /* entry 0 = {42,10}: base (142,60); x -= (r1&3)-2,
         * y -= (r2&3)-2 where r1/r2 are successive LCG outputs. */
        uint32_t r1 = (rng_before >> 8) * 0x3243F6ADu + 0x1B0CB175u;
        uint32_t r2 = (r1 >> 8) * 0x3243F6ADu + 0x1B0CB175u;
        int ex = (int16_t)(142 - ((int32_t)(r1 & 3) - 2));
        int ey = (int16_t)(60 - ((int32_t)(r2 & 3) - 2));
        assert(cout.burst_632b0_x == ex && cout.burst_632b0_y == ey);
        assert(cout.rng == r2);              /* exactly two draws */
        cin.rng = cout.rng;
        assert(ch.field_c == 1 && ch.field_a == 0);

        /* aux 3 / sub_08063698: same machine, table 0x08118ABC.
         * Entry 0 = {-45,8}: scr (200,100) -> base (155,108). */
        memset(&ch, 0, sizeof ch);
        ch.aux = 3; ch.scr_x = 200; ch.scr_y = 100;
        ch.field_c = 9; ch.field_a = 2;      /* last entry {-45,28} */
        cin.rng = 0x12345678;
        rng_before = cin.rng;
        aos_wo_spawn_child_update(&ch, &cin, &cout);
        assert(cout.burst_632b0);
        r1 = (rng_before >> 8) * 0x3243F6ADu + 0x1B0CB175u;
        r2 = (r1 >> 8) * 0x3243F6ADu + 0x1B0CB175u;
        /* entry 9 = {-45,28}: base (155,128) */
        ex = (int16_t)(155 - ((int32_t)(r1 & 3) - 2));
        ey = (int16_t)(128 - ((int32_t)(r2 & 3) - 2));
        assert(cout.burst_632b0_x == ex && cout.burst_632b0_y == ey);
        assert(ch.field_c == 10 && (ch.field_59 & 8) && cout.done);

        /* aux 1 entry-count tail: +0xC == 10 -> done. Covered by
         * aux 3 above (same count); aux-0 count is 15. */
        memset(&ch, 0, sizeof ch);
        ch.aux = 0; ch.field_c = 15;         /* past the table */
        ch.field_a = 1; ch.field_d = 5;
        cin.rng = 0x12345678;
        aos_wo_spawn_child_update(&ch, &cin, &cout);
        assert(!cout.done);                  /* bit3 not set yet */
        assert(!cout.carrier_spawn && !cout.burst_63168 &&
               !cout.burst_632b0);
        ch.field_59 |= 8;                    /* host-visible done */
        aos_wo_spawn_child_update(&ch, &cin, &cout);
        assert(cout.done);                   /* still reported */
        assert(cout.rng == cin.rng);
    }

    puts("test_aos_object20_whiteout_render: OK");
    return 0;
}
