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

    puts("aos_object1c: ok");
    return 0;
}
