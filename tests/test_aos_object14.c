#include "../src/runtime/aos/game/aos_object14.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int probe_tile;    /* sub_08001E58 hit distance, 0 = clear */
static int probe_point;   /* sub_080020D8 hit */
static int probe_wall_a;  /* sub_080020A0 hit */
static int probe_wall_b;  /* sub_08001F3C hit */

static int obj14_probe(void *ctx, int kind, int32_t a, int32_t b,
                       int32_t c) {
    (void)ctx; (void)a; (void)b; (void)c;
    switch (kind) {
    case AOS_OBJ14_PROBE_TILE:  return probe_tile;
    case AOS_OBJ14_PROBE_POINT: return probe_point;
    case AOS_OBJ14_PROBE_WALL_A: return probe_wall_a;
    case AOS_OBJ14_PROBE_WALL_B: return probe_wall_b;
    default: return 0;
    }
}

static AosObject14Env env;

static void env_reset(void) {
    memset(&env, 0, sizeof(env));
    env.palette_slot = 3;
    env.probe = obj14_probe;
    env.player_x = 0x50000;
    env.player_y = 0x40000;
    env.room_ox = 0x10;
    env.room_oy = 0x20;
    probe_tile = probe_point = 0;
    probe_wall_a = probe_wall_b = 0;
}

int main(void) {
    AosObject14 o;
    bool deleted;
    env_reset();

    /* --- create --- */
    aos_object14_create(&o, 0, 0x10000, 0x20000, false, &deleted);
    assert(deleted && o.deleted);

    aos_object14_create(&o, 0, 0x10000, 0x20000, true, &deleted);
    assert(!deleted && !o.deleted);
    /* + 0x58 = (+ 0x58 & ~0x41) | 0x20 ; + 0x59 |= 2. */
    assert(o.flags58 == 0x20 && o.flags59 == 2);
    assert(o.param0 == 0 && o.state == 0);

    /* --- variant 0: state 0 init falls into state 1 --- */
    env.palette_slot = 5;
    bool alive = aos_object14_update(&o, &env);
    assert(alive && !o.deleted);
    /* Palette slot 5 into + 0x58 low5 after the ~0x21 clear;
     * + 0x5C = 0xC, + 0x59 bit4, frame callback, home snapshot
     * from + 0x40/+ 0x44 >> 16, + 0x48 = 0x8000, hurt slot,
     * + 0x72 = 6, then state 1: + 0x65 = 0, + 0x24 = 0x400. */
    assert(o.flags58 == 5);
    assert(o.flags59 == 0x12);
    assert(o.aux_5c == 0xC && o.flags72 == 6);
    assert(o.cb_installs == 1 && o.hurt_calls == 1);
    assert(o.home_x == 1 && o.home_y == 2);
    assert(o.room_x == 1 && o.room_y == 2);
    assert(o.vel_x == 0x8000 && o.sel_65 == 0);
    assert(o.state == 1 && o.f24 == 0x400);
    /* Shared tail: affine counted, attack preset has the
     * 0x14000000 base (the init preset was overwritten). */
    assert(o.affine_calls == 1 && o.affine_a == 0x400 &&
           o.affine_b == 0);
    assert((o.preset_arg & 0xFF000000u) == 0x14000000u);

    /* Scale ramps 0x400/frame; the > 0xFFFF check fires at
     * 0x10000: 62 more frames reach 0xFC00, the next hits
     * 0x10000 -> state 2, + 0x65 = 2. */
    for (int i = 0; i < 62; ++i)
        aos_object14_update(&o, &env);
    assert(o.state == 1 && o.f24 == 0xFC00);
    aos_object14_update(&o, &env);
    assert(o.state == 2 && o.sel_65 == 2 && o.f24 == 0x10000);

    /* State 2: clear probes -> state 3, + 0x54 = 0x2000; the
     * helper registers channel 7. */
    unsigned regs = o.register_calls;
    aos_object14_update(&o, &env);
    assert(o.state == 3 && o.acc_y == 0x2000);
    assert(o.register_calls == regs + 1 && o.register_arg == 7);

    /* State 3: floor probe hit 4 with vel_y = 0x50000 — the
     * helper first integrates + 0x44 += + 0x4C, then bumps it
     * by (4 + yoff) << 16 with yoff = + 0x4E = 0 (vel_y != 0).
     * vel_y > 0x40000 -> state 4. */
    o.vel_y = 0x50000;
    probe_tile = 4;
    int32_t y_before = o.pos_y;
    aos_object14_update(&o, &env);
    assert(o.pos_y == y_before + 0x50000 + (4 << 16));
    assert(o.state == 4);
    probe_tile = 0;

    /* State 4: the (-0x10, 0, 0x110, 0xB0) box on + 0x42/+ 0x46.
     * A hit fires six children + PlaySong 0xAC, resets the
     * motion fields and re-anchors + 0x42/+ 0x46 to home -
     * A094, then -> state 1. */
    o.scr_x = 0x40; o.scr_y = 0x30;
    aos_object14_update(&o, &env);
    assert(o.child_count == 6 && o.song == 0xAC);
    assert(o.state == 1 && o.f24 == 0 && o.angle == 0 &&
           o.vel_y == 0 && o.acc_y == 0);
    assert(o.scr_x == (int16_t)(o.home_x - env.room_ox) &&
           o.scr_y == (int16_t)(o.home_y - env.room_oy));

    /* Probe-miss path: outside the box -> no children/song,
     * still resets to state 1. */
    o.state = 4;
    o.scr_x = -100; o.scr_y = -100;
    aos_object14_update(&o, &env);
    assert(o.child_count == 6 && o.song == -1);
    assert(o.state == 1);

    /* Gate: + 0x4BE nonzero freezes states 0-1 outright and
     * only registers channel 0 past them. */
    env.gate_4be = 1;
    regs = o.register_calls;
    aos_object14_update(&o, &env);           /* state 1 */
    assert(o.state == 1 && o.register_calls == regs);
    o.state = 3;
    aos_object14_update(&o, &env);
    assert(o.register_calls == regs + 1 && o.register_arg == 0);
    env.gate_4be = 0;

    /* Palette alloc failure at state 0 -> EntityDelete. */
    env.palette_slot = -1;
    o.state = 0;
    alive = aos_object14_update(&o, &env);
    assert(!alive && o.deleted);
    env.palette_slot = 3;

    /* --- variant 1 --- */
    aos_object14_create(&o, 1, 0x10000, 0x20000, true, &deleted);
    env_reset();
    env.palette_slot = 2;
    alive = aos_object14_update(&o, &env);
    assert(alive && !o.deleted);
    /* Init: slot 2, + 0x5C, + 0x59 bit4, + 0x65 = 2, claim,
     * hurt, 0x102EFEE8 preset, + 0x72 = 6 -> state 1. No
     * collide bits: vel_x stays 0, angle 0 -> stays state 1.
     * Tail: identity affine, no preset (state <= 1). */
    assert(o.state == 1 && o.sel_65 == 2);
    assert(o.flags58 == 2 && o.flags59 == 0x12);
    assert(o.aux_5c == 0xC && o.flags72 == 6);
    assert(o.cb_installs == 1 && o.claim_calls == 1 &&
           o.hurt_calls == 1);
    assert(o.affine_calls == 1 && o.affine_b == 0x10000);
    assert(o.preset_arg == 0x102EFEE8);   /* init preset kept:
                                           * tail skipped it */
    assert(o.song == -1 && o.song_stop == -1);

    /* collide14 bit0 with vel_x == 0: sub_08013D60(0x136) via
     * song, counter++, vel_x = 0x4000; tail clears + 0x14. */
    env.collide14 = 1;
    aos_object14_update(&o, &env);
    assert(o.song == 0x136 && o.cnt_c == 1 && o.vel_x == 0x4000);
    assert(o.flags14 == 0 && o.state == 1);
    /* Next frame ungrounded: vel_x != 0 -> song_stop, vel_x = 0. */
    env.collide14 = 0;
    aos_object14_update(&o, &env);
    assert(o.song_stop == 0x136 && o.vel_x == 0);

    /* angle != 0 after the probe -> state 2, timer 8. */
    o.angle = 0x100;
    aos_object14_update(&o, &env);
    assert(o.state == 2 && o.timer_d == 8 && o.angle == 0);

    /* State 2: timer counts 8 -> 0 releasing the slot once;
     * vel_x ramps by angle/2 = 0; angle stays 0 -> stays. */
    unsigned releases = o.release_calls;
    for (int i = 0; i < 8; ++i)
        aos_object14_update(&o, &env);
    assert(o.timer_d == 0 && o.release_calls == releases + 1);
    assert(o.state == 2);

    /* angle < 0 after the probe -> release to state 3 with
     * + 0x54 = 0x400 and vel_y -= 0x34000. */
    o.angle = -0x40;
    aos_object14_update(&o, &env);
    assert(o.state == 3 && o.song_stop == 0x136);
    assert(o.acc_y == 0x400);

    /* State 3 point-probe miss: alive, tail preset recorded. */
    probe_point = 0;
    alive = aos_object14_update(&o, &env);
    assert(alive && !o.deleted && o.state == 3);
    assert(o.preset_arg >= 0x14140000 &&
           o.preset_arg <= 0x1414FFFF);

    /* Hit: children + PlaySong 0xAC + palette free + delete. */
    probe_point = 1;
    alive = aos_object14_update(&o, &env);
    assert(!alive && o.deleted);
    assert(o.child_count == 6 && o.song == 0xAC);
    assert(o.palette_frees == 1 && o.palette_free_arg == 2);

    /* Palette failure on variant 1 -> delete. */
    aos_object14_create(&o, 1, 0x10000, 0x20000, true, &deleted);
    env_reset();
    env.palette_slot = -1;
    alive = aos_object14_update(&o, &env);
    assert(!alive && o.deleted);

    /* --- variant 2 --- */
    aos_object14_create(&o, 2, 0x10000, 0x20000, true, &deleted);
    env_reset();
    env.rng_a = 1;      /* bit0 -> angle preset 0x8000 */
    env.rng_b = 1;      /* & 0x3F + 0x40 -> speed 0x41 */
    alive = aos_object14_update(&o, &env);
    assert(alive && !o.deleted && o.state == 1);
    /* No palette roll on variant 2: + 0x58 keeps only the
     * ~0x21 clear, + 0x59 stays the create value 2. */
    assert(o.flags58 == 0 && o.flags59 == 2);
    assert(o.aux_5c == 0xC && o.sel_65 == 2);
    assert(o.cb_installs == 1 && o.hurt_calls == 1);
    assert(o.preset_arg == 0x103000E8);
    /* State 1 same frame: angle 0x8000 + speed 0x41 -> 0x8041;
     * + 0x40 += sine(0x8041)/3 = -402/3 = -134. */
    assert(o.f24 == 0x8041 && o.f26 == 0x41);
    assert(o.vel_x == -134);
    assert(o.pos_x == 0x10000 - 134);

    /* RNG bit0 clear leaves the angle at 0; speed (0 & 0x3F) +
     * 0x40 = 0x40 -> angle 0x40, sine 402/3 = 134. */
    aos_object14_create(&o, 2, 0x10000, 0x20000, true, &deleted);
    env_reset();
    env.rng_a = 0;
    env.rng_b = 0x40;
    aos_object14_update(&o, &env);
    assert(o.f24 == 0x40 && o.f26 == 0x40);
    assert(o.vel_x == 134 && o.pos_x == 0x10000 + 134);

    /* Head probe: inside the wide box + linked self ->
     * register channel 3 and the player + 0x40 snap. */
    o.scr_x = 0x80; o.scr_y = 0x60;
    env.linked_self = true;
    unsigned regs2 = o.register_calls;
    int32_t px_snap = o.pos_x;   /* snap precedes state-1 motion */
    aos_object14_update(&o, &env);
    assert(o.register_calls == regs2 + 1 && o.register_arg == 3);
    assert(o.player_snap && o.player_snap_x == (int16_t)px_snap);
    env.linked_self = false;

    /* State 2 wall bounce: WALL_A hit + WALL_B clear halves
     * + 0x48 (-0x1000 -> -0x800), the 79/80 drag lands -2022
     * and |v| <= 0x7FF restarts state 1. */
    o.state = 2;
    o.vel_x = 0x1000;
    probe_wall_a = 1;
    aos_object14_update(&o, &env);
    assert(o.vel_x == -2022 && o.state == 1);
    probe_wall_a = 0;

    /* Passable tile (WALL_B also hit) keeps the velocity and
     * stays in state 2. */
    o.state = 2;
    o.vel_x = 0x40000;
    probe_wall_a = 1; probe_wall_b = 1;
    aos_object14_update(&o, &env);
    assert(o.state == 2);
    probe_wall_a = probe_wall_b = 0;

    /* --- child callback (sub_080527E4) --- */
    aos_object14_create(&o, 0, 0, 0, true, &deleted);
    env_reset();
    env.palette_slot = 1;
    aos_object14_update(&o, &env);
    o.room_x = 0x50; o.room_y = 0x70;
    aos_object14_child_update(&o, 0x10, 0x20);
    assert(o.scr_x == (int16_t)(0x50 - 0x10) &&
           o.scr_y == (int16_t)(0x70 - 0x20));
    assert(o.draw_calls == 1);
    /* Room pos refreshes from + 0x40/+ 0x44 >> 16. */
    o.pos_x = 0x90000; o.pos_y = 0xA0000;
    aos_object14_child_update(&o, 0, 0);
    assert(o.room_x == 9 && o.room_y == 10);

    /* Out-of-range param0: no handler, no crash. */
    aos_object14_create(&o, 7, 0, 0, true, &deleted);
    env_reset();
    alive = aos_object14_update(&o, &env);
    assert(alive && !o.deleted && o.state == 0);

    puts("aos_object14: ok");
    return 0;
}
