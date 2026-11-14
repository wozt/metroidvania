#include "../src/runtime/aos/game/aos_object14.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Injected engine queries: per-kind results the test flips. */
static int tile_res, point_res, wall_a_res, wall_b_res;
static int probe_calls[4];

static int obj14_probe(void *ctx, int kind, int32_t a, int32_t b,
                       int32_t c) {
    (void)ctx;
    if (kind >= 0 && kind < 4) probe_calls[kind]++;
    switch (kind) {
    case AOS_OBJ14_PROBE_TILE: return tile_res;
    case AOS_OBJ14_PROBE_POINT: return point_res;
    case AOS_OBJ14_PROBE_WALL_A: return wall_a_res;
    case AOS_OBJ14_PROBE_WALL_B: return wall_b_res;
    default: return 0;
    }
    (void)a; (void)b; (void)c;
}

static AosObject14Env env;

static void env_reset(void) {
    memset(&env, 0, sizeof(env));
    env.probe = obj14_probe;
    env.room_ox = 0x10;
    env.room_oy = 0x20;
    env.player_x = 0x50000;
    env.player_y = 0x40000;
    env.palette_slot = 3;
    tile_res = point_res = wall_a_res = wall_b_res = 0;
    memset(probe_calls, 0, sizeof(probe_calls));
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
    assert(o.song == -1 && o.song_stop == -1);

    /* --- variant 0: state 0 init falls into state 1 --- */
    env.palette_slot = 5;
    aos_object14_update(&o, &env);
    assert(!o.deleted);
    /* slot & 0x1F into + 0x58, + 0x59 bit 4, + 0x5C = 0xC,
     * + 0x72 = 6, + 0x48 = 0x8000, hurt/preset/callback. */
    assert((o.flags58 & 0x3F) == 5);
    assert(o.flags59 & 0x14);
    assert(o.aux_5c == 0xC && o.flags72 == 6);
    assert(o.cb_installs == 1 && o.hurt_calls == 1);
    /* The state-0 preset 0x103000E8 is overwritten the same
     * frame by the tail: dist 0x40000 -> dpx 4, angle 0 ->
     * range 0xA, sine(0x4000)=0x10000, sine(0)=0 ->
     * 0x14000000 + 0xA<<17 + (-0xA+4)&0xFF = 0x141400FA. */
    assert(o.preset_arg == 0x141400FA);
    assert(o.vel_x == 0x8000);
    /* room pos = pos >> 16 = (1, 2). */
    assert(o.home_x == 1 && o.home_y == 2);
    assert(o.room_x == 1 && o.room_y == 2);
    assert(o.state == 1);
    /* State 1 same frame: + 0x65 = 0, + 0x24 += 0x400. */
    assert(o.sel_65 == 0 && o.f24 == 0x400);
    /* Tail ran: affine (angle, + 0x24 word) + preset. */
    assert(o.affine_calls == 1 && o.affine_a == 0 &&
           o.affine_b == 0x400);
    assert(o.preset_arg != 0x103000E8);

    /* + 0x24 ramps 0x400/frame; > 0xFFFF fires when the new
     * value reaches 0x10000: 63 more updates (0x400 + 63*0x400
     * = 0x10000). */
    for (int i = 0; i < 62; ++i)
        aos_object14_update(&o, &env);
    assert(o.state == 1 && o.f24 == 0xFC00);
    aos_object14_update(&o, &env);
    assert(o.state == 2 && o.sel_65 == 2 && o.f24 == 0x10000);

    /* State 2: probe miss -> state 3 with + 0x54 = 0x2000. */
    tile_res = 0;
    aos_object14_update(&o, &env);
    assert(o.state == 3 && o.acc_y == 0x2000);
    /* sub_08051BF8 registered channel 7 and integrated. */
    assert(o.register_arg == 7);
    assert(probe_calls[AOS_OBJ14_PROBE_TILE] >= 1);

    /* State 3: probe hit with vel_y > 0x40000 -> state 4. */
    o.vel_y = 0x50000;
    tile_res = 4;
    aos_object14_update(&o, &env);
    assert(o.state == 4);
    /* + 0x44 bumped by (hit + yoff) << 16 (vel_y != 0 -> yoff 0
     * only when + 0x4C == 0; here vel_y != 0 so yoff = + 0x4E =
     * 0): pos_y += 4 << 16 on top of the vel_y integrate. */
    assert(o.pos_y != 0x20000);

    /* State 3 bounce path: hit but vel_y <= 0x40000 -> state 2
     * with velocities cleared. */
    o.state = 3;
    o.vel_y = 0x1000;
    tile_res = 4;
    aos_object14_update(&o, &env);
    assert(o.state == 2 && o.vel_y == 0 && o.acc_y == 0);

    /* State 2 probe hit -> stays state 2. */
    aos_object14_update(&o, &env);
    assert(o.state == 2);

    /* State 4: screen probe hit -> children + song + reset. */
    o.state = 4;
    o.scr_x = 0x40; o.scr_y = 0x30;   /* inside (-0x10,0,0x110,0xB0) */
    unsigned children_before = o.child_count;
    aos_object14_update(&o, &env);
    assert(o.child_count == children_before + 6);
    assert(o.song == 0xAC);
    assert(o.state == 1);
    assert(o.f24 == 0 && o.angle == 0 && o.vel_y == 0 && o.acc_y == 0);
    /* + 0x42/+ 0x46 re-anchored to home - A094. */
    assert(o.scr_x == (int16_t)(1 - 0x10) &&
           o.scr_y == (int16_t)(2 - 0x20));

    /* State 4 probe miss: no children/song, still resets. */
    o.state = 4;
    o.scr_x = -100; o.scr_y = -100;
    children_before = o.child_count;
    aos_object14_update(&o, &env);
    assert(o.child_count == children_before);
    assert(o.song == -1 && o.state == 1);

    /* + 0x4BE gate: states <= 1 frozen, state > 1 -> channel 0
     * register and no machine/tail. */
    env.gate_4be = 1;
    o.state = 1;
    unsigned aff_before = o.affine_calls;
    unsigned rc_before = o.register_calls;
    aos_object14_update(&o, &env);
    assert(o.state == 1 && o.register_calls == rc_before);
    o.state = 3;
    aos_object14_update(&o, &env);
    assert(o.register_arg == 0 && o.register_calls == rc_before + 1);
    assert(o.affine_calls == aff_before);   /* tail skipped */
    env.gate_4be = 0;

    /* Palette allocator full -> EntityDelete, no tail. */
    env.palette_slot = -1;
    o.state = 0;
    assert(!aos_object14_update(&o, &env));
    assert(o.deleted);

    /* --- variant 1 --- */
    aos_object14_create(&o, 1, 0x10000, 0x20000, true, &deleted);
    env_reset();
    env.palette_slot = 2;
    aos_object14_update(&o, &env);
    assert(!o.deleted && o.state == 1);
    assert(o.sel_65 == 2);
    assert(o.preset_arg == 0x102EFEE8);
    assert(o.claim_calls == 1);
    assert((o.flags58 & 0x3F) == 2 && (o.flags59 & 0x14));
    assert(o.home_x == 1 && o.home_y == 2);
    /* State 1 same frame: collide14 = 0 -> vel_x = 0 branch,
     * angle 0 -> stays state 1; tail cleared + 0x14 and ran the
     * identity affine (angle, 0x10000). */
    assert(o.flags14 == 0);
    assert(o.affine_a == 0 && o.affine_b == 0x10000);
    assert(o.song == -1 && o.song_stop == -1);

    /* Contact bit -> song_hold 0x136, counter++, vel_x = 0x4000. */
    env.collide14 = 1;
    aos_object14_update(&o, &env);
    assert(o.song == 0x136 && o.cnt_c == 1 && o.vel_x == 0x4000);
    /* Contact clear with vel_x != 0 -> song_stop, vel_x = 0. */
    env.collide14 = 0;
    aos_object14_update(&o, &env);
    assert(o.song_stop == 0x136 && o.vel_x == 0);

    /* angle != 0 after the probe -> state 2, timer 8, angle
     * cleared, song_hold again. */
    o.angle = 0x100;
    aos_object14_update(&o, &env);
    assert(o.state == 2 && o.timer_d == 8 && o.angle == 0);
    assert(o.song == 0x136);

    /* State 2: timer counts 8 -> release_calls on the wrap;
     * vel_x ramps by angle/2 = 0 (angle is 0). */
    for (int i = 0; i < 8; ++i)
        aos_object14_update(&o, &env);
    assert(o.release_calls == 1 && o.timer_d == 0);
    assert(o.state == 2);

    /* angle < 0 after the probe -> state 3 with the release
     * song stop and the + 0x54/+ 0x4C writes. */
    o.angle = -0x40;
    aos_object14_update(&o, &env);
    assert(o.state == 3);
    assert(o.song_stop == 0x136);
    assert(o.acc_y == 0x400);

    /* State 3 point-probe hit -> children + song + palette
     * free + delete. */
    point_res = 1;
    bool alive = aos_object14_update(&o, &env);
    assert(!alive && o.deleted);
    assert(o.child_count == 6 && o.song == 0xAC);
    assert(o.palette_frees == 1 && o.palette_free_arg == (o.flags58 & 7));

    /* State 3 point-probe miss -> survives, aim tail runs. */
    aos_object14_create(&o, 1, 0x10000, 0x20000, true, &deleted);
    env_reset();
    env.palette_slot = 2;
    o.state = 3;
    o.vel_x = 0x1000;
    point_res = 0;
    assert(aos_object14_update(&o, &env));
    assert(!o.deleted && o.state == 3);
    /* Aim tail for state > 1 uses the 0x14140000 base. */
    assert(o.preset_arg >= 0x14140000 &&
           o.preset_arg <= 0x1414FFFF);

    /* Palette failure on variant 1 -> delete. */
    env.palette_slot = -1;
    o.state = 0;
    assert(!aos_object14_update(&o, &env));
    assert(o.deleted);

    /* --- variant 2 --- */
    aos_object14_create(&o, 2, 0x10000, 0x20000, true, &deleted);
    env_reset();
    env.rng_a = 1;      /* bit0 -> + 0x24 = 0x8000 */
    env.rng_b = 1;      /* & 0x3F = 1 -> + 0x26 = 0x41 */
    aos_object14_update(&o, &env);
    assert(!o.deleted && o.state == 1);
    assert(o.flags59 == 2);
    assert((o.flags58 & 0x3F) == 0);   /* no palette write */
    assert(o.aux_5c == 0xC && o.sel_65 == 2);
    assert(o.preset_arg == 0x103000E8 && o.flags72 == 6);
    /* + 0x24 = 0x8000 + 0x41 = 0x8041; vel_x = sine/3. */
    assert((o.f24 & 0xFFFF) == 0x8041);
    assert(o.f26 == 0x41);
    assert(o.pos_x != 0x10000);        /* integrated */

    /* RNG bit0 clear leaves + 0x24 at 0. */
    aos_object14_create(&o, 2, 0x10000, 0x20000, true, &deleted);
    env_reset();
    env.rng_a = 0x40;   /* bit0 clear; & 0x3F = 0 -> speed 0x40 */
    env.rng_b = 0x40;
    aos_object14_update(&o, &env);
    assert((o.f24 & 0xFFFF) == 0x40);

    /* Head probe: inside the (-0x80,-0x40,0x1F0,0x120) box
     * with linked_self -> channel 3 + player snap. */
    o.scr_x = 0x80; o.scr_y = 0x60;
    env.linked_self = true;
    int32_t pos_before = o.pos_x;   /* snap precedes state-1 integrate */
    aos_object14_update(&o, &env);
    assert(o.register_arg == 3);
    assert(o.player_snap);
    assert(o.player_snap_x == (int16_t)pos_before);

    /* Head probe miss -> no register/snap. */
    o.scr_x = -200; o.scr_y = -200;
    env.linked_self = true;
    unsigned rc2_before = o.register_calls;
    aos_object14_update(&o, &env);
    assert(o.register_calls == rc2_before && !o.player_snap);

    /* State 2 wall bounce: WALL_A hit + WALL_B miss halves
     * vel_x, drag 79/80, |vel| <= 0x7FF -> state 1. */
    o.state = 2;
    o.vel_x = 0x1000;
    o.scr_x = 0x80; o.scr_y = 0x60;
    wall_a_res = 1; wall_b_res = 0;
    aos_object14_update(&o, &env);
    /* -0x800 after the flip, * 79 / 80 = -2022 -> state 1. */
    assert(o.state == 1);
    wall_a_res = 0;

    /* Passable wall (WALL_B hit) does not halve: vel stays
     * above the restart threshold -> state 2 kept. */
    o.state = 2;
    o.vel_x = 0x40000;
    wall_a_res = 1; wall_b_res = 1;
    aos_object14_update(&o, &env);
    assert(o.state == 2);
    wall_a_res = wall_b_res = 0;

    /* Out-of-range param0: no handler, no crash. */
    aos_object14_create(&o, 7, 0, 0, true, &deleted);
    env_reset();
    assert(aos_object14_update(&o, &env));
    assert(!o.deleted && o.state == 0);

    /* --- child callback (sub_080527E4) --- */
    aos_object14_create(&o, 0, 0x10000, 0x20000, true, &deleted);
    env_reset();
    env.palette_slot = 1;
    aos_object14_update(&o, &env);   /* installs cb, home (1,2) */
    assert(o.cb_installs == 1);
    o.room_x = 0x50; o.room_y = 0x70;
    aos_object14_child_update(&o, 0x10, 0x20);
    assert(o.scr_x == (int16_t)(0x50 - 0x10));
    assert(o.scr_y == (int16_t)(0x70 - 0x20));
    assert(o.draw_calls == 1);
    /* room pos refreshed from pos >> 16 = (1, 2). */
    assert(o.room_x == 1 && o.room_y == 2);

    puts("aos_object14: ok");
    return 0;
}
