/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos/game/aos_object1c.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static AosObject1CCreateIn create_in(void) {
    AosObject1CCreateIn in;
    memset(&in, 0, sizeof in);
    in.affine_slot = 3;
    in.palette_index = 7;
    in.ground_y = 0x50;
    in.marker_spawned = true;
    in.frame_record = 9;
    in.sparkle_spawned = 2;
    return in;
}

static uint32_t test_rng(void *ctx) {
    uint32_t *s = (uint32_t *)ctx;
    *s = *s * 1103515245u + 12345u;
    return *s;
}

static AosObject1CIn upd_in(void) {
    AosObject1CIn in;
    memset(&in, 0, sizeof in);
    in.player_x = 0x78;
    in.player_y = 0xA0;
    in.save_slot = 1;
    in.save_result = 0;
    return in;
}

int main(void) {
    AosObject1C o;
    AosObject1CCreateIn ci;
    AosObject1CCreateOut co;
    AosObject1CIn ui;
    AosObject1COut uo;

    /* ---- Create ---- */
    ci = create_in();
    assert(aos_object1c_create(&o, &ci, &co));
    assert(!co.deleted && co.marker_child && co.canvas_set);
    assert(co.sparkle_children == 2);
    assert(o.palette == 7 && o.anim_1a == 0xA && o.anim == 0x11);
    assert(o.affine_slot == 3 && o.flags58 == 3);
    assert(o.pos_x == 0x78 && o.pos_y == 0x50);
    assert(o.frame_rec == 9 && o.state == 0 && o.timer == 0);
    assert(o.sync30 == 0 && o.sync31 == 0 && o.sync32 == 0 && o.sync33 == 0);

    /* Palette failure deletes; affine slot masks to 5 bits; params
     * seed the + 0x30..+ 0x33 sync bytes; unspawned marker keeps
     * + 0x35 at zero. */
    ci = create_in();
    ci.palette_index = -1;
    assert(!aos_object1c_create(&o, &ci, &co));
    assert(co.deleted && o.deleted);

    ci = create_in();
    ci.affine_slot = -1;                 /* & 0x1F -> 0x1F */
    ci.param0 = 0x1234;                  /* sync30=0x34 sync31=0x12 */
    ci.param1 = 0x5678;                  /* sync32=0x78 sync33=0x56 */
    ci.marker_spawned = false;
    ci.sparkle_spawned = 5;              /* capped at 2 */
    ci.ground_y = (int16_t)(0x50 - 8);   /* snapped floor */
    assert(aos_object1c_create(&o, &ci, &co));
    assert(!co.marker_child && co.sparkle_children == 2);
    assert(o.affine_slot == 0xFF && o.flags58 == 0x1F);
    assert(o.sync30 == 0x34 && o.sync31 == 0x12);
    assert(o.sync32 == 0x78 && o.sync33 == 0x56);
    assert(o.frame_rec == 0 && o.pos_y == 0x48);

    /* ---- State 0: idle prompt ---- */
    aos_object1c_create(&o, (ci = create_in(), &ci), &co);
    ui = upd_in();
    /* No Up: clears + 0x42C bit 1, no prompt. */
    aos_object1c_update(&o, &ui, &uo);
    assert(uo.flags42c_clear == 0x2 && uo.flags42c_set == 0);
    assert(o.prompt == 0 && o.state == 0);
    /* Up held but player outside the 0x68/0x80/0x20/0x37 rect. */
    ui.buttons_16 = 0x40;
    ui.player_x = 0x90;                  /* > 0x88 */
    aos_object1c_update(&o, &ui, &uo);
    assert(o.prompt == 0);
    /* Up + inside -> prompt latches, + 0xD snapshots old + 0xB. */
    ui.player_x = 0x78;
    aos_object1c_update(&o, &ui, &uo);
    assert(o.prompt == 1 && o.timer == 1); /* timer: 0 then tail ++ */
    /* While latched the gate decides: busy keeps waiting, idle at
     * + 0xD == 3 cancels. */
    ui.player_busy = true;
    ui.buttons_16 = 0;                   /* Up may be released */
    aos_object1c_update(&o, &ui, &uo);   /* timer 2 */
    assert(o.prompt == 1);
    aos_object1c_update(&o, &ui, &uo);   /* timer 3 */
    assert(o.prompt == 1);
    aos_object1c_update(&o, &ui, &uo);   /* timer 4: + 0xD was 3 */
    /* + 0xD reached 3 two frames ago; cancel fires when the check
     * sees timer == 3, i.e. this update. */
    assert(o.prompt == 0);

    /* Idle gate opens the save screen. */
    aos_object1c_create(&o, (ci = create_in(), &ci), &co);
    ui = upd_in();
    ui.buttons_16 = 0x40;
    aos_object1c_update(&o, &ui, &uo);   /* prompt = 1 */
    ui.buttons_16 = 0;
    ui.player_busy = false;
    aos_object1c_update(&o, &ui, &uo);   /* opens */
    assert(o.state == 1 && o.prompt == 0 && o.save_sel == 1);
    assert(uo.song == 0xF3 && uo.save_setup && uo.fade_a && uo.fade_b &&
           uo.room_sync && uo.aux_post);
    assert(uo.flags42c_set == 0x1);
    assert(uo.save_mode_clear == 0x3);

    /* ---- State 1 -> 2 ---- */
    aos_object1c_update(&o, &ui, &uo);
    assert(o.state == 2);
    assert(uo.obj_palettes == 1 && uo.obj_rows[0].desc == AOS_O1C_DESC_MAIN);
    assert(uo.obj_rows[0].src == 4 && uo.obj_rows[0].count == 1 &&
           uo.obj_rows[0].dst == 0xE);
    assert((o.frame_flags & 0x21) == 0);

    /* ---- State 2: cursor moves ---- */
    ui.buttons_14 = 0x10;                /* Right */
    aos_object1c_update(&o, &ui, &uo);
    assert(o.save_sel == 0 && uo.song == 0xF1);
    aos_object1c_update(&o, &ui, &uo);   /* held again: no change */
    assert(uo.song != 0xF1 || o.save_sel != 0);
    ui.buttons_14 = 0x20;                /* Left */
    aos_object1c_update(&o, &ui, &uo);
    assert(o.save_sel == 1 && uo.song == 0xF1);
    ui.buttons_14 = 0;

    /* Decline: B held (buttons_16 & 3) without the A edge -> 0xF0,
     * state 3. */
    ui.buttons_16 = 0x2;
    aos_object1c_update(&o, &ui, &uo);
    assert(o.state == 3 && uo.song == 0xF0);
    /* State 3 restores and returns to idle. */
    ui.buttons_16 = 0;
    aos_object1c_update(&o, &ui, &uo);
    assert(o.state == 0 && uo.save_mode_set == 0x2 &&
           uo.obj_palettes == 1 && uo.text_resets == 1);
    assert(o.frame_flags & 0x20);

    /* Commit success: A edge + sel != 0 + A|B held. */
    aos_object1c_create(&o, (ci = create_in(), &ci), &co);
    o.state = 2; o.save_sel = 1;
    ui = upd_in();
    ui.buttons_16 = 0x1;                 /* A held */
    ui.buttons_14 = 0x1;                 /* A edge */
    ui.save_result = 0;
    aos_object1c_update(&o, &ui, &uo);
    assert(uo.save_write && uo.save_block_refresh && uo.save_committed);
    assert(!uo.save_rejected);
    assert(o.state == 0x14 && (o.flags24 & 2) && o.timer == 1);
    assert(uo.song == 0xF3 && uo.save_aux && uo.text_resets == 1);
    assert(o.frame_flags & 0x20);

    /* Commit failure: nonzero SRAM result -> state 0, flash. */
    aos_object1c_create(&o, (ci = create_in(), &ci), &co);
    o.state = 2; o.save_sel = 1;
    ui.buttons_16 = 0x1;
    ui.buttons_14 = 0x1;
    ui.save_result = -1;
    aos_object1c_update(&o, &ui, &uo);
    assert(uo.save_write && uo.save_rejected && !uo.save_committed);
    assert(o.state == 0 && uo.song == 0xEF);
    assert(uo.palettes == 1 && uo.pal_rows[0].desc == AOS_O1C_DESC_FLASH);
    assert(uo.pal_rows[0].src == 0 && uo.pal_rows[0].dst == 0xD);
    assert(uo.text_mode == 2 && uo.save_mode_set == 0x2);

    /* A edge with sel == 0 declines instead of committing. */
    aos_object1c_create(&o, (ci = create_in(), &ci), &co);
    o.state = 2; o.save_sel = 0;
    ui.buttons_16 = 0x1;
    ui.buttons_14 = 0x1;
    ui.save_result = 0;
    aos_object1c_update(&o, &ui, &uo);
    assert(!uo.save_write && o.state == 3 && uo.song == 0xF0);

    /* ---- States 10/20 beam gate ---- */
    aos_object1c_create(&o, (ci = create_in(), &ci), &co);
    o.state = 10; o.sync31 = 0;
    ui = upd_in();
    aos_object1c_update(&o, &ui, &uo);
    assert(o.state == 10 && uo.beam_children == 0 && uo.song == -1);
    o.sync31 = 1;
    aos_object1c_update(&o, &ui, &uo);
    assert(o.state == 11 && uo.beam_children == 8 && uo.song == 0xF6);
    assert(uo.palettes == 1 && uo.pal_rows[0].src == 0xC &&
           uo.pal_rows[0].dst == 2);

    /* State 20 shares the gate and lands in 21. */
    aos_object1c_create(&o, (ci = create_in(), &ci), &co);
    o.state = 20; o.sync31 = 1; o.sync32 = 0; o.sync33 = 0;
    aos_object1c_update(&o, &ui, &uo);
    assert(o.state == 21 && o.prompt == 0 && o.timer == 1);
    /* First state-21 frame seeds the substate and ramps. */
    aos_object1c_update(&o, &ui, &uo);
    assert(o.prompt == 1 && o.angle == 0x800);

    /* ---- State 11 ramp + advance ---- */
    aos_object1c_create(&o, (ci = create_in(), &ci), &co);
    o.state = 11; o.angle = 0x100;
    aos_object1c_update(&o, &ui, &uo);      /* sync 0/0: += 0x800 */
    assert(o.angle == 0x100 + 0x800);
    o.angle = 0xFFFF;
    aos_object1c_update(&o, &ui, &uo);
    assert(o.angle == 0x10000);             /* clamped */
    o.sync33 = 1;                           /* decay branch */
    o.angle = 0x100;
    aos_object1c_update(&o, &ui, &uo);
    assert(o.angle == 0x100 - 0xCC);
    o.angle = 0x50;
    aos_object1c_update(&o, &ui, &uo);
    assert(o.angle == 0);                   /* floored at 0 */
    o.sync33 = 0; o.sync32 = 1;             /* advance */
    aos_object1c_update(&o, &ui, &uo);
    assert(o.state == 12 && uo.disp_writes == 2);
    assert(uo.disp_reg[0] == 0x50 && uo.disp_val[0] == 0x3FBF);
    assert(uo.disp_reg[1] == 0x54 && uo.disp_val[1] == 0);

    /* ---- State 12 sweep + mark ---- */
    o.timer = 0x10;
    aos_object1c_update(&o, &ui, &uo);
    assert(o.state == 12 && uo.disp_writes == 1 &&
           uo.disp_val[0] == 0x10 >> 2);
    assert(!(o.flags58 & 0x20));
    o.timer = 0x40;
    aos_object1c_update(&o, &ui, &uo);
    assert(o.state == 13 && (o.flags58 & 0x20) && o.timer == 1);

    /* ---- State 13 fade -> 99 ---- */
    o.timer = 0x10;
    aos_object1c_update(&o, &ui, &uo);
    assert(o.state == 99 && uo.flags42c_clear == 0x2);

    /* ---- State 99 dormant ---- */
    aos_object1c_update(&o, &ui, &uo);
    assert(o.state == 99 && uo.obj_palettes == 1 &&
           uo.obj_rows[0].desc == AOS_O1C_DESC_SAVE);
    assert(o.flags59 & 0x8 && uo.save_mode_set == 0x2 &&
           uo.text_resets == 1);

    /* ---- State 21 substate machine ---- */
    aos_object1c_create(&o, (ci = create_in(), &ci), &co);
    o.state = 21; o.sync32 = 0; o.sync33 = 0;
    /* First update seeds prompt = 1 and ramps; timer becomes 1. */
    aos_object1c_update(&o, &ui, &uo);
    assert(o.prompt == 1);
    /* Drive timer to the substate-1 edge (0xE pre-increment). */
    o.timer = 0xE;
    aos_object1c_update(&o, &ui, &uo);
    /* timer == 0xE: reset pair, prompt -> 2, then substate-2 body
     * runs with timer 0 -> disp 0x54 = 0. */
    assert(o.prompt == 2 && uo.disp_writes == 3);
    assert(uo.disp_reg[0] == 0x50 && uo.disp_val[0] == 0x3FBF);
    assert(uo.disp_reg[1] == 0x54 && uo.disp_val[1] == 0);
    assert(uo.disp_reg[2] == 0x54 && uo.disp_val[2] == 0);
    /* Substate 2: sweep halved, advance past 0x1F. */
    o.timer = 0x20;
    aos_object1c_update(&o, &ui, &uo);
    assert(o.prompt == 3 && uo.disp_writes >= 2 &&
           uo.disp_val[uo.disp_writes - 2] == 0x10);
    /* Substate 3: fade 0x10 - (timer >> 3), advance past 0x3F. */
    o.timer = 0x40;
    aos_object1c_update(&o, &ui, &uo);
    assert(o.prompt == 4);
    /* Substate 4 waits for sync32 == 1. */
    aos_object1c_update(&o, &ui, &uo);
    assert(o.state == 21 && o.prompt == 4);
    o.sync32 = 1;
    aos_object1c_update(&o, &ui, &uo);
    assert(o.state == 99 && uo.flags42c_clear == 0x2 &&
           uo.disp_val[uo.disp_writes - 1] == 0);

    /* State 21 with sync32 != 0 skips the + 0x1C ramp but still
     * runs the substate machine: prompt 4 + sync32 == 1 completes. */
    aos_object1c_create(&o, (ci = create_in(), &ci), &co);
    o.state = 21; o.sync32 = 1; o.sync33 = 0; o.angle = 0x100;
    o.prompt = 4;
    aos_object1c_update(&o, &ui, &uo);
    assert(o.angle == 0x100);              /* no ramp */
    assert(o.state == 99 && o.prompt == 0);

    /* ---- Dead states and > 0x63: tail only ---- */
    aos_object1c_create(&o, (ci = create_in(), &ci), &co);
    o.state = 50;
    uo = (AosObject1COut){0};
    aos_object1c_update(&o, &ui, &uo);
    assert(o.state == 50 && uo.song == -1 && uo.forced_keys == 0x40);
    assert(o.timer == 1);
    o.state = 0x64;
    aos_object1c_update(&o, &ui, &uo);
    assert(o.state == 0x64 && uo.forced_keys == 0x40);
    o.state = 0;
    aos_object1c_update(&o, &ui, &uo);
    assert(uo.forced_keys == -1);          /* state 0: no lock */

    /* ---- Tail: timer cadence and the + 0x31-gated pulse ---- */
    aos_object1c_create(&o, (ci = create_in(), &ci), &co);
    o.sync31 = 0; o.pulse = 0;
    /* timer starts 0; each update increments then tests % 3.
     * Pulses on timer 3, 6, ... with alternating src rows. */
    for (int i = 0; i < 3; ++i) aos_object1c_update(&o, &ui, &uo);
    assert(o.timer == 3);
    /* The third update produced the first pulse: pulse toggled
     * 0 -> 1, src row 0xA. */
    /* Re-run deterministically. */
    aos_object1c_create(&o, (ci = create_in(), &ci), &co);
    o.sync31 = 0;
    aos_object1c_update(&o, &ui, &uo);     /* timer 1 */
    aos_object1c_update(&o, &ui, &uo);     /* timer 2 */
    aos_object1c_update(&o, &ui, &uo);     /* timer 3 -> pulse */
    assert(o.pulse == 1);
    /* Find the pulse row in the last Out. */
    assert(uo.palettes == 1 && uo.pal_rows[0].src == 0xA &&
           uo.pal_rows[0].dst == 2);
    aos_object1c_update(&o, &ui, &uo);     /* timer 4 */
    aos_object1c_update(&o, &ui, &uo);     /* timer 5 */
    aos_object1c_update(&o, &ui, &uo);     /* timer 6 -> pulse 1->0 */
    assert(o.pulse == 0);
    assert(uo.palettes == 1 && uo.pal_rows[0].src == 2);
    /* + 0x31 == 1 suppresses the pulse. */
    o.sync31 = 1;
    for (int i = 0; i < 3; ++i) aos_object1c_update(&o, &ui, &uo);
    assert(uo.palettes == 0 && o.pulse == 0);

    /* Forced input follows state != 0 through the whole machine. */
    aos_object1c_create(&o, (ci = create_in(), &ci), &co);
    o.state = 2;
    aos_object1c_update(&o, &ui, &uo);
    assert(uo.forced_keys == 0x40);

    /* ==================== child entities ==================== */
    AosObject1CChildrenIn cin;
    AosObject1CChildrenOut cout;
    memset(&cin, 0, sizeof cin);

    /* Create auto-spawns the marker + two live sparkle records. */
    aos_object1c_create(&o, (ci = create_in(), &ci), &co);
    assert(o.child_count == 3);
    assert(o.children[0].kind == AOS_O1C_CHILD_MARKER &&
           o.children[0].active);
    assert(o.children[0].flags58 & 0x20 && o.children[0].anim5c == 3);
    assert(o.children[1].kind == AOS_O1C_CHILD_SPARKLE &&
           o.children[2].kind == AOS_O1C_CHILD_SPARKLE);
    assert(o.children[1].timer == 4 && o.children[1].anim5c == 0x13 &&
           o.children[1].anim6d == 0);
    assert(o.children[2].anim6d == 4 && o.children[2].anim5c == 0x14);
    assert(o.children[1].pos40 == o.pos40 &&
           o.children[1].pos44 == o.pos44);
    assert(o.pos40 == (int32_t)0x78 << 16 &&
           o.pos44 == (int32_t)0x50 << 16);

    /* Marker drawFunc: three commits; +0x16 picks the variant.
     * The two sparkles add one sub_0803B9D0 commit each. */
    aos_object1c_children_draw(&o, &cout);
    assert(cout.commits == 5);
    assert(cout.commit[0].x == 0x48 && cout.commit[0].y == 0x26 &&
           cout.commit[0].frame == 0x15);
    assert(cout.commit[1].x == 0x39 && cout.commit[1].y == 0x3E &&
           cout.commit[1].frame == 0x17);   /* save_sel == 0 */
    assert(cout.commit[2].x == 0x99 && cout.commit[2].frame == 0x18);
    o.save_sel = 1;
    aos_object1c_children_draw(&o, &cout);
    assert(cout.commit[1].frame == 0x16 && cout.commit[2].frame == 0x19);
    o.save_sel = 0;

    /* Sparkle update: gated on parent +0x24 bit 1, then the frame
     * sets and the sync-byte writes. */
    aos_object1c_children_update(&o, &cin, &cout);
    assert(o.children[1].state == 0 && o.children[2].state == 0);
    o.flags24 = 2;                          /* save committed */
    aos_object1c_children_update(&o, &cin, &cout);
    assert(o.children[1].state == 1 && o.children[2].state == 1);
    aos_object1c_children_update(&o, &cin, &cout);
    /* state 1 -> 2 with sub_0803F2C8(1) / (5) frame sets. */
    assert(o.children[1].state == 2 && o.children[2].state == 2);
    assert(cout.frame_sets == 2);
    assert(cout.frame_set[0].frame == 1 && cout.frame_set[1].frame == 5);
    assert(o.children[1].anim6d == 1 && o.children[2].anim6d == 5);
    assert(o.children[1].anim6e == 0);
    /* +0x6E == 2 at frame 1/5 syncs parent +0x31 (beam enable). */
    o.children[1].anim6e = 2;
    o.children[2].anim6e = 0;
    aos_object1c_children_update(&o, &cin, &cout);
    assert(o.sync31 == 1);
    /* Expiry: +0x6E back to 0, +0x59 bit 0 set, +0xD counts 4 -> 0. */
    o.children[1].anim6e = 0;
    o.children[1].flags59 |= 1;
    o.sync30 = 0;
    for (int i = 0; i < 4; ++i)
        aos_object1c_children_update(&o, &cin, &cout);
    assert(o.sync30 == 1 && (o.children[1].flags59 & 8));
    /* Sparkle draw is the single sub_0803B9D0 commit. */
    aos_object1c_children_draw(&o, &cout);
    assert(cout.commits == 3 + 2);          /* marker 3 + 2 sparkles */

    /* ---- beams: state 10/20 auto-spawn ---- */
    aos_object1c_create(&o, (ci = create_in(), &ci), &co);
    o.state = 10; o.sync31 = 1;
    ui = upd_in();
    aos_object1c_update(&o, &ui, &uo);
    assert(o.state == 11 && uo.beam_children == 8);
    assert(o.child_count == 3 + 8);
    assert(uo.children.frame_inits == 8 && uo.children.frame_sets == 8);
    for (int i = 0; i < 8; ++i) {
        AosObject1CChild *b = &o.children[3 + i];
        assert(b->kind == AOS_O1C_CHILD_BEAM && b->active);
        assert(b->field20 == i && b->state == 0 && b->timer == 2);
        assert(b->field24 == 0x80000 && b->anim5c == 0x11);
        assert(b->flags59 == 0x10 && b->flags58 == 3);
    }

    /* Beam state 0 -> 1: white-box the lifetime to the edge. */
    AosObject1CChild *b = &o.children[3];
    b->field24 = 0x1000000;                 /* already past 0xFFFFFF */
    memset(&cin, 0, sizeof cin);            /* trig NULL -> 0 */
    aos_object1c_children_update(&o, &cin, &cout);
    assert(b->state == 1 && o.sync33 == 1);
    assert(b->anim5c == 9 && b->field28 == 0 && b->timer == 3);
    assert(b->pos44 == 0x500000 && b->pos40 == 0x760000);

    /* Beam state 1: focus delta + countdown -> parent +0x32. */
    cin.focus_x = 0x78; cin.focus_y = 0xA0;
    b->field24 = 0x30000;                   /* -0x30000 -> 0: expire */
    aos_object1c_children_update(&o, &cin, &cout);
    assert(o.sync32 == 1 && (b->flags59 & 8));
    /* pos40 = 0x780000 + (0x78<<16 - 0x780000) = 0x780000;
     * pos44 = 0x500000 + (0xA0<<16 - 0x500000) + 0x200000. */
    assert(b->pos40 == 0x780000 && b->pos44 == 0xC00000);
    /* The state-1 tail tick (timer 4 & 3 == 0) spawned a spark. */
    assert(cout.spark_attempts >= 1 && cout.sparks_spawned >= 1);

    /* Deterministic rng: sub_08055414 spin velocities. */
    {
        static uint32_t rng_state;
        /* test_rng outputs for seed 0: lcg(0), then lcg of that. */
        uint32_t ra = 12345u;
        uint32_t rb = ra * 1103515245u + 12345u;
        rng_state = 0;
        cin.rng = test_rng;
        cin.rng_ctx = &rng_state;
        int didx = aos_object1c_spark_spawn(&o, 3, 0x100000, 0x200000,
                                            &cin, &cout);
        assert(didx >= 0);
        AosObject1CChild *d = &o.children[didx];
        assert(d->pos40 == 0x100000 && d->pos44 == 0x200000);
        assert(d->field1c == ((int32_t)(ra & 0x1F) - 0x10) << 16);
        assert(d->field20 == ((int32_t)(rb & 0x1F) - 0x10) << 16);
        assert(d->acc50 == b->acc50 && d->acc54 == b->acc54);
        assert(d->anim5c == (uint8_t)(b->anim5c + 1));
        assert(d->flags58 == (b->flags58 & 7));
        cin.rng = NULL;
    }

    /* Spark lifecycle on an isolated child set (the live beams keep
     * spawning sparks, so a fresh parent keeps the Out counts
     * exact). */
    aos_object1c_create(&o, (ci = create_in(), &ci), &co);
    {
        AosObject1CChildEnv env = {
            .palette = o.palette, .anim_1a = o.anim_1a,
            .pos40 = o.pos40, .pos44 = o.pos44,
            .affine_slot = o.affine_slot, .owner = -1,
        };
        int bidx = aos_object1c_beam_spawn(&o, &env, 0, NULL);
        assert(bidx == 3);
        int sidx = aos_object1c_spark_spawn(&o, (unsigned)bidx,
                                            0x100000, 0x200000,
                                            &cin, &cout);
        assert(sidx >= 0);
        /* Silence the beam so only the spark produces effects. */
        aos_object1c_child_delete(&o, (unsigned)bidx);
        AosObject1CChild *s = &o.children[sidx];
        assert(s->kind == AOS_O1C_CHILD_SPARK && s->active);
        assert(s->owner == 3 && s->anim5c == 0x12 && s->field18 == 0);
        /* rng NULL -> spin velocities at the -16 extreme. */
        assert(s->field1c == (int32_t)(-0x10u << 16) &&
               s->field20 == (int32_t)(-0x10u << 16));
        /* state 0 -> 1: palette resolve + frame init + frame set 3. */
        aos_object1c_children_update(&o, &cin, &cout);
        assert(s->state == 1 && cout.pal_resolves == 1 &&
               cout.pal_index == o.palette);
        assert(cout.frame_inits == 1 && cout.frame_sets == 1 &&
               cout.frame_set[0].frame == 3);
        assert(s->vel4c == (int32_t)0xFFFF0000 && s->timer == 0xC);
        /* state 1 drift: +0x44 += +0x4C each frame. */
        int32_t y0 = s->pos44;
        aos_object1c_children_update(&o, &cin, &cout);
        assert(s->pos44 == y0 + s->vel4c && s->vel4c == (int32_t)0xFFFF0000);
        for (int i = 0; i < 11; ++i)
            aos_object1c_children_update(&o, &cin, &cout);
        assert(s->state == 2);
        for (int i = 0; i < 9; ++i)
            aos_object1c_children_update(&o, &cin, &cout);
        assert(s->flags59 & 8);
        /* Spark drawFunc sub_080554BC: two commits once state >= 1;
         * total = marker 3 + 2 sparkles + spark 2. */
        aos_object1c_children_draw(&o, &cout);
        assert(cout.commits == 7);
    }

    /* ---- fade child (sub_08054574) ---- */
    aos_object1c_create(&o, (ci = create_in(), &ci), &co);
    int fidx = aos_object1c_fade_spawn(&o, NULL);
    assert(fidx >= 0 && o.children[fidx].kind == AOS_O1C_CHILD_FADE);
    AosObject1CChild *f = &o.children[fidx];
    aos_object1c_children_update(&o, &cin, &cout);
    assert(f->substate == 1 && cout.obj_palettes == 1);
    assert(cout.obj_rows[0].desc == 0x0820C384 &&
           cout.obj_rows[0].src == 5 && cout.obj_rows[0].dst == 0xE);
    for (int i = 0; i < 8; ++i)
        aos_object1c_children_update(&o, &cin, &cout);
    assert(f->substate == 2);
    aos_object1c_children_update(&o, &cin, &cout);
    assert(cout.fade_finished == 1);
    /* State-1 (fade-out) variant. */
    f->state = 1; f->substate = 0; f->field14 = 0;
    aos_object1c_children_update(&o, &cin, &cout);
    assert(f->substate == 1 && f->field14 == 0x10000);
    for (int i = 0; i < 8; ++i)
        aos_object1c_children_update(&o, &cin, &cout);
    assert(f->substate == 2 && cout.obj_palettes == 1);
    assert(cout.obj_rows[0].desc == 0x082099FC &&
           cout.obj_rows[0].src == 0xE && cout.obj_rows[0].dst == 0xE);
    aos_object1c_children_update(&o, &cin, &cout);
    assert(cout.fade_finished == 1 && (f->flags58 & 0x20));

    /* Slot reuse after delete. */
    aos_object1c_child_delete(&o, (unsigned)fidx);
    assert(!o.children[fidx].active);
    int nidx = aos_object1c_fade_spawn(&o, NULL);
    assert(nidx == fidx);

    /* ==================== animation advance ====================
     * sub_0803F17C -> step chain over an injected sequence table
     * shaped like 0x08257964's type-1 sequences: contiguous flat
     * +0x65 frames, per-sub-frame tick durations. */
    {
        static const AosObject1CAnimTick seq0_ticks[4] = {
            {0, 6}, {1, 6}, {2, 6}, {3, 6},       /* anim 0 */
        };
        static const AosObject1CAnimTick seq1_ticks[4] = {
            {4, 8}, {5, 8}, {6, 8}, {7, 8},       /* anim 1 */
        };
        static const AosObject1CAnimTick seq2_ticks[2] = {
            {8, 4}, {9, 4},                       /* anim 2 */
        };
        static const AosObject1CAnimTick seq3_ticks[2] = {
            {10, 4}, {11, 4},                     /* anim 3 */
        };
        static const AosObject1CAnimTick seq4_ticks[4] = {
            {12, 6}, {13, 6}, {14, 6}, {15, 6},   /* anim 4 */
        };
        static const AosObject1CAnimTick seq5_ticks[4] = {
            {16, 8}, {17, 8}, {18, 8}, {19, 8},   /* anim 5 */
        };
        static const AosObject1CAnimTick seq6_ticks[1] = {{20, 1}};
        static const AosObject1CAnimTick seq7_ticks[1] = {{21, 1}};
        static const AosObject1CAnimTick seq8_ticks[1] = {{22, 1}};
        static const AosObject1CAnimTick seq9_ticks[1] = {{23, 1}};
        static const AosObject1CAnimTick seq10_ticks[1] = {{24, 1}};
        static const AosObject1CAnimTick seq11_ticks[1] = {{25, 1}};
        static const AosObject1CAnimSeq anims[12] = {
            {seq0_ticks, 4, 1}, {seq1_ticks, 4, 1},
            {seq2_ticks, 2, 1}, {seq3_ticks, 2, 1},
            {seq4_ticks, 4, 1}, {seq5_ticks, 4, 1},
            {seq6_ticks, 1, 1}, {seq7_ticks, 1, 1},
            {seq8_ticks, 1, 1}, {seq9_ticks, 1, 1},
            {seq10_ticks, 1, 1}, {seq11_ticks, 1, 1},
        };
        memset(&cin, 0, sizeof cin);
        cin.anims = anims;
        cin.anim_count = 12;

        /* Fresh create: two sparkles (anim6d 0 and 4) carry the
         * spawn-time anim_boot; the marker never advances. */
        aos_object1c_create(&o, (ci = create_in(), &ci), &co);
        assert(o.children[1].anim_boot == 1 &&
               o.children[2].anim_boot == 1);
        assert(o.children[1].flags6c == 0x9 &&
               o.children[1].mode5b == 0);
        /* +0x5A nibble: sparkle params 0xC/0xB -> banks 2/1. */
        assert(((o.children[1].field5a >> 3) & 0xF) == 0xC);
        assert(((o.children[2].field5a >> 3) & 0xF) == 0xB);

        /* First update: per sparkle the spawn-time boot advance
         * (+0x65 = entry0 frame, +0x6F = 1) then the updateFunc tail
         * advance (+0x6F = 2); state 0 with flags24 clear does
         * nothing between them. Sparkle 2's anim4 entry0 frame is
         * 12. Marker never advances -> 4 advances/steps total. */
        aos_object1c_children_update(&o, &cin, &cout);
        assert(!o.children[1].anim_boot);
        assert(o.children[1].anim65 == 0 && o.children[1].anim6e == 0 &&
               o.children[1].anim6f == 2);
        assert(o.children[2].anim65 == 12 && o.children[2].anim6e == 0 &&
               o.children[2].anim6f == 2);
        assert(cout.anim_advances == 4 && cout.anim_steps == 4);

        /* Subsequent updates: one tail advance per sparkle (state 0
         * is stable while flags24 bit 1 stays clear). */
        for (int i = 0; i < 8; ++i)
            aos_object1c_children_update(&o, &cin, &cout);
        /* sparkle commits carry the flat +0x65 (anim0 frames 0-3,
         * anim4 frames 12-15). */
        aos_object1c_children_draw(&o, &cout);
        assert(cout.commit[3].frame == o.children[1].anim65);
        assert(cout.commit[4].frame == o.children[2].anim65);

        /* Loop wrap: anim0 (4 subs * 6 ticks, one advance per
         * update now) wraps inside 40 updates; accumulate the
         * per-update counter. */
        unsigned loops = 0;
        for (int i = 0; i < 40; ++i) {
            aos_object1c_children_update(&o, &cin, &cout);
            loops += cout.anim_looped;
        }
        assert(loops > 0);
        assert(o.children[1].anim6e < 4 && o.children[1].anim6f < 6);
        assert(o.children[1].anim65 <= 3);

        /* Once (non-loop) sequence: clear the loop bit on an
         * isolated sparkle — +0x6E pins at count-1, +0x6C bit 2
         * latches, later advances early-out but still rewrite
         * +0x65. */
        {
            AosObject1CChildEnv env = {
                .palette = o.palette, .anim_1a = o.anim_1a,
                .pos40 = o.pos40, .pos44 = o.pos44,
                .affine_slot = o.affine_slot, .owner = -1,
            };
            int sidx = aos_object1c_sparkle_spawn(&o, &env, 0, 0xC,
                                                  0x13, NULL);
            assert(sidx >= 0);
            AosObject1CChild *s = &o.children[sidx];
            s->anim_boot = 0;               /* skip the spawn step */
            s->flags6c = 0x8;               /* bit3 seq, no loop */
            /* State 0 never leaves (flags24 bit 1 clear) -> exactly
             * one tail advance per update; 4 subs * 6 ticks = 24
             * advances to end. */
            for (int i = 0; i < 30; ++i)
                aos_object1c_children_update(&o, &cin, &cout);
            assert(s->flags6c & 4);
            assert(s->anim6e == 3 && s->anim6f == 0);
            assert(s->anim65 == 3);         /* last entry frame */
            uint8_t f6e = s->anim6e, f6f = s->anim6f;
            aos_object1c_children_update(&o, &cin, &cout);
            /* Ended early-out: +0x65 rewritten, counters frozen. */
            assert(s->anim6e == f6e && s->anim6f == f6f);
            assert(s->anim65 == 3);
        }

        /* Pause (+0x6C bit 1): step writes +0x65 then returns 1
         * without ticking. */
        {
            AosObject1CChild *s = &o.children[o.child_count - 1];
            /* reuse the once-ended child: clear ended, set pause */
            s->flags6c = (uint8_t)((s->flags6c & ~4u) | 2u);
            s->anim6e = 0; s->anim6f = 0;
            uint8_t old65 = s->anim65;
            aos_object1c_children_update(&o, &cin, &cout);
            assert(s->anim6f == 0 && s->anim6e == 0);
            assert(s->anim65 == 0);         /* entry0 frame rewritten */
            (void)old65;
        }

        /* +0x6C bit-3 clear: the frame-set +8 descriptor path is
         * unported — advance is a no-op. */
        {
            AosObject1CChild *s = &o.children[o.child_count - 1];
            s->flags6c = 0x1;               /* loop only, no seq bit */
            s->anim65 = 0x55;
            aos_object1c_children_update(&o, &cin, &cout);
            assert(s->anim65 == 0x55);
        }

        /* +0x5B modes: 2/3 count as unported; mode 1 runs the
         * palette tail when frame_pal is supplied. */
        {
            AosObject1CChild *s = &o.children[o.child_count - 1];
            s->flags6c = 0x9; s->mode5b = 2;
            s->anim6e = 0; s->anim6f = 0; s->anim6d = 2;
            aos_object1c_children_update(&o, &cin, &cout);
            assert(cout.anim_modes >= 1);
        }
        {
            /* mode 1 + frame_pal: seq2 entries frames 8,9. */
            static uint8_t pals[26];
            memset(pals, 0, sizeof pals);
            pals[8] = 0x11; pals[9] = 0x22;
            AosObject1CChild *s = &o.children[o.child_count - 1];
            s->flags6c = 0x9; s->mode5b = 1;
            s->anim6d = 2;                  /* seq2: frames 8,9 t4 */
            s->anim6e = 0; s->anim6f = 3;   /* one tick from advance */
            s->anim65 = 0; s->pal65 = -1;
            cin.frame_pal = pals;
            cin.frame_pal_count = 26;
            aos_object1c_children_update(&o, &cin, &cout);
            /* One tail advance (state 0): +0x65 0 -> 8 fires the
             * palette tail once. */
            assert(s->pal65 == 0x11);
            assert(cout.pal_changes == 1);
            assert(cout.pal_change[0].child == (uint8_t)(o.child_count - 1));
            assert(cout.pal_change[0].frame == 8);
            assert(cout.pal_change[0].pal == 0x11);
            cin.frame_pal = NULL;
            cin.frame_pal_count = 0;
        }

        /* Beam + spark children animate too: beam anim6d 2 loops
         * frames 8/9; spark state-0 frame set picks anim 3. */
        aos_object1c_create(&o, (ci = create_in(), &ci), &co);
        {
            AosObject1CChildEnv env = {
                .palette = o.palette, .anim_1a = o.anim_1a,
                .pos40 = o.pos40, .pos44 = o.pos44,
                .affine_slot = o.affine_slot, .owner = -1,
            };
            int bidx = aos_object1c_beam_spawn(&o, &env, 0, NULL);
            assert(bidx >= 0);
            AosObject1CChild *b = &o.children[bidx];
            assert(b->anim6d == 2 && b->flags6c == 0x9);
            assert(((b->field5a >> 3) & 0xF) == 0xD);   /* bank 3 */
            for (int i = 0; i < 10; ++i)
                aos_object1c_children_update(&o, &cin, &cout);
            assert(b->anim65 == 8 || b->anim65 == 9);
            /* Spark: state 0 -> 1 frame set anim 3 (frames 10/11). */
            int sidx = aos_object1c_spark_spawn(&o, (unsigned)bidx,
                                                0, 0, &cin, &cout);
            assert(sidx >= 0);
            aos_object1c_child_delete(&o, (unsigned)bidx); /* silence */
            for (int i = 0; i < 3; ++i)
                aos_object1c_children_update(&o, &cin, &cout);
            AosObject1CChild *s = &o.children[sidx];
            assert(s->state >= 1 && s->anim6d == 3);
            for (int i = 0; i < 12; ++i)
                aos_object1c_children_update(&o, &cin, &cout);
            assert(s->anim65 == 10 || s->anim65 == 11);
        }
    }

    puts("aos_object1c: ok");
    return 0;
}
