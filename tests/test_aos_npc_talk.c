#include "../src/runtime/aos/game/aos_npc_talk.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int step_calls;
static void step_count(void *c) { (void)c; step_calls++; }
static bool spawn_ok(void) { return true; }

static uint16_t null_xform(uint16_t v) { return v; }
#define NULL_XFORM null_xform

int main(void) {
    AosNpcTalk t;

    memset(&t, 0, sizeof t);
    aos_npc_talk_17_update(&t, false);
    assert(t.arm_42c_1 && t.window_text == 0x17 && t.state == 1);
    aos_npc_talk_17_update(&t, false);
    assert(!t.entity_release);
    aos_npc_talk_17_update(&t, true);
    assert(t.entity_release);

    memset(&t, 0, sizeof t);
    aos_npc_talk_18_update(&t, false);
    assert(t.window_text == 0x18 && t.state == 1);
    aos_npc_talk_18_update(&t, true);
    assert(t.window_close && (t.flags59 & 0x8));
    t.state = 0x80;
    aos_npc_talk_18_update(&t, false);
    assert(t.entity_release);

    assert(aos_npc_talk_arm_flag());

    /* misc Object20 callbacks. */
    uint8_t a5c = 0; bool del = false;
    assert(aos_npc_talk_sprite_gate(&a5c, &del, true) == 1 &&
           a5c == 0x11 && !del);
    a5c = 0; del = false;
    assert(aos_npc_talk_sprite_gate(&a5c, &del, false) == 0 &&
           del);
    aos_npc_talk_release_frames();
    aos_npc_talk_arm_fade();
    uint8_t f59 = 0;
    aos_npc_talk_mark_done(&f59);
    assert(f59 & 0x8);
    assert(aos_npc_talk_sound_release() == 0);
    uint8_t cc = 0, cd = 0; bool w417;
    assert(aos_npc_talk_counter_tick(&cc, &cd, 5, &w417) &&
           w417 && cd == 5 && cc == 1);
    assert(aos_npc_talk_counter_tick(&cc, &cd, 0, &w417) &&
           !w417 && cd == 4);
    cd = 0;
    assert(aos_npc_talk_counter_tick(&cc, &cd, 0, &w417) && cc == 2);
    assert(!aos_npc_talk_counter_tick(&cc, &cd, 0, &w417));

    /* sub_080657B4: flag gate then mark or arm+song. */
    uint8_t f59b = 0; uint32_t fl = 0; bool c0 = false; uint16_t song = 0;
    aos_npc_talk_657b4(&f59b, &fl, &c0, &song);
    assert((f59b & 0x8) && fl == 0 && !c0 && song == 0);
    f59b = 0; fl = 0x4000; c0 = false; song = 0;
    aos_npc_talk_657b4(&f59b, &fl, &c0, &song);
    assert(!(f59b & 0x8) && (fl & 0x10020u) == 0x10020u &&
           c0 && song == 0x1000);

    /* sub_08065FE4: entity-at-0x13110 predicate. */
    assert(aos_npc_talk_65fe4(1, 0, 0) == 1);
    assert(aos_npc_talk_65fe4(0, 0x8001, 0) == 1);
    assert(aos_npc_talk_65fe4(0, -0x8000, 0) == 0);
    assert(aos_npc_talk_65fe4(0, 0, 0x30010000u) == 0x30010000u);
    assert(aos_npc_talk_65fe4(0, 0, 0x1000u) == 0);

    /* sub_080659A4: follower spawn gates and fields. */
    AosNpcSpawn sp;
    memset(&sp, 0, sizeof sp);
    aos_npc_talk_659a4(&sp, true, true, 0, 0, 0, 0, 0);
    assert((sp.flags59 & 0x8) && !sp.anim_stepped && !sp.deleted);
    memset(&sp, 0, sizeof sp);
    aos_npc_talk_659a4(&sp, false, false, 0, 0, 0, 0, 0);
    assert(sp.deleted && !sp.anim_stepped);
    memset(&sp, 0, sizeof sp);
    sp.flags58 = 0xFF;
    aos_npc_talk_659a4(&sp, false, true, 0x20, 0x30, 100, 200, 5);
    assert(sp.anim_stepped && sp.field42 == 0x40 &&
           sp.field46 == 0x70 && (sp.flags58 & 0x40) &&
           sp.anim5c == 4 && sp.flag42c == 1);
    memset(&sp, 0, sizeof sp);
    aos_npc_talk_659a4(&sp, false, true, 0, 0, 300, 200, 0);
    assert(!(sp.flags58 & 0x40));

    /* sub_08065A5C: guide follower state machine. */
    AosNpcGuide gd;
    bool bit6 = false, f417 = false;
    memset(&gd, 0, sizeof gd);
    uint32_t r = aos_npc_talk_65a5c(&gd, false, 0, 0, 0, &bit6, &f417);
    assert(gd.window_open && gd.wrote_417 && gd.state_a == 1 &&
           f417 && gd.text_poll);
    /* state 0 falls into state 1 in the same call: script A
     * case 0 -> 1 with the ROM fallthrough decrement. */
    assert(gd.ctr_c == 1 && gd.timer_d == 0x2F && gd.flag_3b7_set);
    /* count down to the wrap: 0x30 more calls (0x2F -> 0 -> 0xFF). */
    for (int i = 0; i < 0x30; ++i)
        aos_npc_talk_65a5c(&gd, false, 0, 0, 0, &bit6, &f417);
    assert(gd.ctr_c == 2);
    /* case 2 with player x <= 0xA0 clamps and advances. */
    aos_npc_talk_65a5c(&gd, false, 0xA0, 0, 0, &bit6, &f417);
    assert(gd.player_x_clamped && gd.ctr_c == 3);
    aos_npc_talk_65a5c(&gd, false, 0, 0, 0, &bit6, &f417);
    assert(gd.flag_3b7_clear && gd.ctr_c == 3);
    /* player x > 0xA0 returns flag 0x20 without advancing. */
    memset(&gd, 0, sizeof gd); gd.state_a = 1; gd.ctr_b = 0;
    gd.ctr_c = 2;
    r = aos_npc_talk_65a5c(&gd, false, 0xA1, 0, 0, &bit6, &f417);
    assert(r == 0x20 && gd.ctr_c == 2);
    /* script B: case 0 arms vx and falls into case 1. */
    memset(&gd, 0, sizeof gd); gd.state_a = 1; gd.ctr_b = 1;
    gd.flags58 = 0xFF;
    aos_npc_talk_65a5c(&gd, false, 0, 0x10000, 0x40, &bit6, &f417);
    assert(gd.vx == 0x8000 && gd.ctr_c == 1);
    assert((gd.flags58 & 0x41) == 0x40);
    /* x int part <= 0xFF: x += 0x8000, facing mirror vs player. */
    gd.x = 0x10000; /* 1.0 px */
    aos_npc_talk_65a5c(&gd, false, 0, 0x20000, 0, &bit6, &f417);
    assert(gd.x == 0x18000 && bit6);
    /* text done -> state 0x80 -> release next call. */
    memset(&gd, 0, sizeof gd); gd.state_a = 1;
    aos_npc_talk_65a5c(&gd, true, 0, 0, 0, &bit6, &f417);
    assert(gd.state_a == 0x80 && gd.state_80);
    aos_npc_talk_65a5c(&gd, false, 0, 0, 0, &bit6, &f417);
    assert(gd.release);

    /* sub_08065C2C: sidestep count and targets. */
    int16_t tg[4];
    step_calls = 0;
    assert(aos_npc_talk_65c2c(100, 0, step_count, NULL, tg) == 1);
    assert(step_calls == 1 && tg[0] == 84);
    step_calls = 0;
    assert(aos_npc_talk_65c2c(100, 0x02, step_count, NULL, tg) == 2);
    assert(step_calls == 2 && tg[0] == 84 && tg[1] == 116);
    step_calls = 0;
    assert(aos_npc_talk_65c2c(100, 0x10, step_count, NULL, tg) == 2);
    assert(tg[0] == 84 && tg[1] == 116);
    step_calls = 0;
    assert(aos_npc_talk_65c2c(100, 0x12, step_count, NULL, tg) == 3);
    assert(step_calls == 3 && tg[2] == 92);

    /* sub_08065CD8: companion spawn gates and child copy. */
    AosNpcCompanion cp;
    memset(&cp, 0, sizeof cp);
    aos_npc_talk_65cd8(&cp, false, true, true, 0, 3, NULL);
    assert(cp.marked && (cp.flags59 & 0x8));
    memset(&cp, 0, sizeof cp);
    aos_npc_talk_65cd8(&cp, true, false, true, 0, 3, NULL);
    assert(cp.marked);
    memset(&cp, 0, sizeof cp);
    aos_npc_talk_65cd8(&cp, true, true, false, 0, 3, NULL);
    assert(cp.marked);
    memset(&cp, 0, sizeof cp);
    cp.spawn_x = 11; cp.spawn_y = 22;
    aos_npc_talk_65cd8(&cp, true, true, true, 1, 3, NULL);
    assert(!cp.marked && !cp.variant_a && (cp.flags58 & 0x40) &&
           cp.anim5c == 4 && cp.child == 0);
    memset(&cp, 0, sizeof cp);
    cp.spawn_x = 11; cp.spawn_y = 22;
    aos_npc_talk_65cd8(&cp, true, true, true, 0, 3, spawn_ok);
    assert(cp.variant_a && cp.child == 1 && cp.child_anim == 5 &&
           cp.child_x == 11 && cp.child_y == 22);

    /* sub_08065DB8: looping NPC interaction. */
    AosNpcLoop lp;
    memset(&lp, 0, sizeof lp);
    /* state 0 clears +0x42C bit 0x200 then falls into state 1;
     * a probe miss stops before the freeze. */
    aos_npc_talk_65db8(&lp, 0, false, 0);
    assert(lp.cleared_42c_200 && lp.state == 1 && !lp.froze_player);
    /* gates: need probe + mask + pause-clear. */
    aos_npc_talk_65db8(&lp, 1, false, 0);
    assert(!lp.froze_player);
    aos_npc_talk_65db8(&lp, 3, false, 0);
    assert(!lp.froze_player);
    aos_npc_talk_65db8(&lp, 7, false, 0);
    assert(lp.froze_player && lp.armed_42c_0 && lp.window_text &&
           lp.state == 2 && lp.text_id == 0x2D && lp.sfx_26 &&
           lp.ev_1e);
    /* text poll pending keeps state 2. */
    aos_npc_talk_65db8(&lp, 7, false, 0);
    assert(lp.state == 2 && !lp.window_close);
    /* poll done -> close, state 3. */
    aos_npc_talk_65db8(&lp, 7, true, 0);
    assert(lp.window_close && lp.state == 3);
    /* state 3 releases, sounds, loops to 1. */
    aos_npc_talk_65db8(&lp, 0, false, 0);
    assert(lp.release_0 && lp.sound_1041 && lp.song_26 &&
           lp.wrote_254cc && lp.state == 1 && lp.text_id == 0x2D);
    /* text picks: b864 + 0x32 clear -> 0x2C. */
    memset(&lp, 0, sizeof lp); lp.state = 1;
    aos_npc_talk_65db8(&lp, 7 | 8, false, 0);
    assert(lp.text_id == 0x2C);
    /* 0x26 set + 0x31 clear -> 0x2B. */
    memset(&lp, 0, sizeof lp); lp.state = 1;
    aos_npc_talk_65db8(&lp, 7 | 0x20, false, 0);
    assert(lp.text_id == 0x2B);
    /* 0x26+0x31 set -> stays 0x2D (elif consumed). */
    memset(&lp, 0, sizeof lp); lp.state = 1;
    aos_npc_talk_65db8(&lp, 7 | 0x20 | 0x40, false, 0);
    assert(lp.text_id == 0x2D);
    /* 0x25 set + 0x30 clear -> 0x2A; 0x2E set + 0x2F clear -> 0x29. */
    memset(&lp, 0, sizeof lp); lp.state = 1;
    aos_npc_talk_65db8(&lp, 7 | 0x80, false, 0);
    assert(lp.text_id == 0x2A);
    memset(&lp, 0, sizeof lp); lp.state = 1;
    aos_npc_talk_65db8(&lp, 7 | 0x200, false, 0);
    assert(lp.text_id == 0x29);
    /* +0x6D variants: 2 -> f2c8_3 in state1, 3 -> f2c8_2 in state2. */
    memset(&lp, 0, sizeof lp); lp.state = 1; lp.flags6d = 2;
    aos_npc_talk_65db8(&lp, 7, false, 0);
    assert(lp.f2c8_3 && lp.state == 2);
    lp.flags6d = 3;
    aos_npc_talk_65db8(&lp, 7, true, 0);
    assert(lp.f2c8_2);

    /* sub_08066094: guard NPC sequence. */
    AosNpcGuard gd2;
    memset(&gd2, 0, sizeof gd2);
    uint32_t rr = aos_npc_talk_66094(&gd2, 0, 0, false, 0);
    assert(gd2.cleared_42c_200 && gd2.sfx_26_entry &&
           gd2.state == 1 && rr == 0);
    /* gates block state 1. */
    rr = aos_npc_talk_66094(&gd2, 1, 0, false, 0);
    assert(!gd2.froze_player && gd2.state == 1);
    /* full gates: freeze, facing (inverted), window, state 2. */
    gd2.x = 0x100000; /* 16 px */
    rr = aos_npc_talk_66094(&gd2, 7, 0x44, false, 0x200000);
    assert(gd2.armed_42c_200 && gd2.froze_player &&
           gd2.window_text && gd2.text_id == 0x44 &&
           gd2.state == 2 && gd2.sound_1040 && gd2.ev_1d);
    /* self_x < player_x -> bit6 clear (inverted compare). */
    assert((gd2.flags58 & 0x40) == 0);
    /* poll pending keeps state 2; done -> songs + release loop. */
    rr = aos_npc_talk_66094(&gd2, 7, 0, false, 0);
    assert(gd2.state == 2 && !gd2.song_1b5);
    rr = aos_npc_talk_66094(&gd2, 7, 0, true, 0);
    assert(gd2.song_1b5 && gd2.wrote_254cc && gd2.song_26 &&
           gd2.sound_1041 && gd2.release_0 && gd2.state == 1);
    /* b864 set on exit -> state 3 escort substates. */
    memset(&gd2, 0, sizeof gd2); gd2.state = 2;
    rr = aos_npc_talk_66094(&gd2, 15, 0, true, 0);
    assert(gd2.state == 3);
    rr = aos_npc_talk_66094(&gd2, 0, 0, false, 0);
    assert(gd2.ctr_b == 1 && gd2.timer_d == 0x10 && rr == 0x10);
    /* player far ahead (self + 0x300000 < player_x) advances. */
    gd2.x = 0;
    rr = aos_npc_talk_66094(&gd2, 0, 0, false, 0x400000);
    assert(gd2.ctr_b == 2 && rr == 0x10);
    rr = aos_npc_talk_66094(&gd2, 0, 0, false, 0);
    assert(gd2.release_0 && gd2.state == 1);
    /* +0x18 variant: song on entry, cleared on exit. */
    memset(&gd2, 0, sizeof gd2); gd2.state = 1; gd2.field18 = 1;
    aos_npc_talk_66094(&gd2, 7, 0x30, false, 0);
    assert(gd2.song_1b5);
    aos_npc_talk_66094(&gd2, 7, 0, true, 0);
    assert(gd2.cleared_18 && gd2.field18 == 0);

    /* sub_08066024 spawn gate. */
    AosNpcSpawnGate sg;
    memset(&sg, 0, sizeof sg);
    uint32_t gr = aos_npc_talk_66024(&sg, false, true, 0);
    assert((sg.flags59 & 8) && gr == 0);
    memset(&sg, 0, sizeof sg);
    gr = aos_npc_talk_66024(&sg, true, false, 0);
    assert((sg.flags59 & 8) && gr == 0 && !sg.variant);
    memset(&sg, 0, sizeof sg); sg.flags58 = 0x41; sg.anim5c = 7;
    gr = aos_npc_talk_66024(&sg, true, true, 9);
    assert(gr == 1 && sg.variant && sg.anim_step &&
           (sg.flags58 & 0x41) == 0 && sg.anim5c == 10 &&
           sg.field18 == 1);

    /* Guard sequence cursor: init/reset/pick/tick. */
    AosGuardSeq gs;
    aos_guard_seq_init(&gs, 0x10, 2, 0x55);
    assert(gs.mode == 0x10 && gs.counter == 0 &&
           gs.index == 2 && gs.fallback == 0x55);
    aos_guard_seq_reset(&gs);
    assert(gs.mode == 0 && gs.counter == 0xFF);
    uint8_t txt = 0;
    aos_guard_seq_pick(&gs, true, true, &txt);
    assert(txt == 0x36 && gs.mode == 0x36 && gs.index == 3);
    aos_guard_seq_pick(&gs, false, true, &txt);
    assert(gs.index == 1);
    aos_guard_seq_pick(&gs, true, false, &txt);
    assert(gs.index == 2);
    /* tick: counter 0 picks rec[3 + rng % rec[2]], counter 1 rec[1]. */
    static const uint16_t recA[] = {0, 0x77, 4, 10, 20, 30, 40};
    static const uint16_t recB[] = {0, 0x88, 2, 5, 6};
    static const uint16_t *tbl[] = {recA, recB, recA, recB, recA, recB, recA, recB};
    aos_guard_seq_init(&gs, 0, 0, 0);
    uint16_t out = 0;
    uint32_t tr = aos_guard_seq_tick(&gs, tbl, 7, NULL_XFORM, &out);
    assert(tr == 1 && gs.counter == 1);
    /* rng 7 % 4 = 3 -> rec[3+3] = 40 -> xform passthrough */
    assert(out == 40);
    tr = aos_guard_seq_tick(&gs, tbl, 0, NULL_XFORM, &out);
    assert(tr == 1 && gs.counter == 2 && out == 0x77);
    tr = aos_guard_seq_tick(&gs, tbl, 0, NULL_XFORM, &out);
    assert(tr == 0);    /* counter > 1: idle */
    /* index 4/6 index the pick array with the +0x138FA
     * fallback (rec[3 + fallback]). */
    aos_guard_seq_init(&gs, 0, 6, 1);
    tr = aos_guard_seq_tick(&gs, tbl, 0, NULL_XFORM, &out);
    assert(tr == 1 && out == 20);    /* recA[3 + 1] */

    puts("aos_npc_talk: ok");
    return 0;
}

/* appended coverage */
