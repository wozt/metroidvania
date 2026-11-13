#include "../src/runtime/aos/game/aos_npc_talk.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int step_calls;
static void step_count(void *c) { (void)c; step_calls++; }
static bool spawn_ok(void) { return true; }

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

    puts("aos_npc_talk: ok");
    return 0;
}

/* appended coverage */
