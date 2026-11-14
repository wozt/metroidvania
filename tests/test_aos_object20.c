/* Unit tests for src/runtime/aos/game/aos_object20.c — the
 * unified Object20 (SPECIAL id 32) param0 dispatcher over the
 * dirs/story/npc_talk/shop variant modules. */
#include "../src/runtime/aos/game/aos_object20.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static AosObject20In mk_in(void) {
    AosObject20In in;
    memset(&in, 0, sizeof in);
    in.flag1327a = 1;               /* shared create gate pass */
    in.flag42c = 0x2000;            /* story create24 gate bit */
    in.keep_5b754 = true;
    in.text_done = false;
    in.player_scr42 = 0x100;        /* > 0x80 / > 0x7F gates */
    in.player_pos40 = 0x1000;
    in.player_anim5c = 3;
    in.a094_6 = 0x40;
    in.a094_a = 0x30;
    /* dirs probes: all clear so creates survive */
    in.dirs.table_852682c = NULL;
    /* story probes */
    in.story.spawn14_ok = true;
    in.story.spawn18_ok = true;
    in.story.fade_done = false;
    in.story.c328_ok = true;
    /* npc probes */
    in.npc.gate_4be = 0;
    in.npc.cam_x = 0x20;
    in.npc.cam_y = 0x10;
    in.npc.ent_pos40 = 0x2000;
    in.npc.rng = 0;
    in.npc.rng_bit = 1;
    return in;
}

static bool create_ok(AosObject20 *o, int16_t param0,
                      const AosObject20In *in, AosObject20Out *out) {
    return aos_object20_create(o, param0, in, out);
}

int main(void) {
    AosObject20 o;
    AosObject20In in;
    AosObject20Out out;
    static const int16_t all_params[] = {
        3, 4, 5, 6, 7, 8, 9, 10, 19, 20, 21, 22, 23, 24, 25, 26,
        27, 28, 29, 30, 31, 0, 1, 2, 11, 18, 32, 200, -1
    };

    /* ---- create dispatch: every param0 resolves a family. -- */
    for (unsigned i = 0; i < sizeof all_params / sizeof all_params[0];
         ++i) {
        int16_t p = all_params[i];
        memset(&o, 0, sizeof o);
        in = mk_in();
        /* npc flag bits for the spawn gates */
        in.npc.flag12078 = (1ull << 0x20) | (1ull << 0x26) |
                           (1ull << 0x2E);
        bool ok = create_ok(&o, p, &in, &out);
        assert(o.param0 == p);
        assert(o.family <= AOS_OBJECT20_FAMILY_INERT);
        (void)ok;
    }

    /* param0 3 reports handled-externally and survives without
     * running any variant body. */
    memset(&o, 0, sizeof o);
    in = mk_in();
    assert(create_ok(&o, 3, &in, &out));
    assert(o.family == AOS_OBJECT20_FAMILY_OPENING);
    assert(out.handled_externally);
    assert(!out.unported);
    assert(out.dirs.return_code == 0 && out.story.return_code == 0);
    /* update also reports handled-externally */
    assert(aos_object20_update(&o, &in, &out) == 0);
    assert(out.handled_externally);

    /* ---- dirs family (param0 4-8): create + update smoke. -- */
    memset(&o, 0, sizeof o);
    in = mk_in();
    in.npc.flag12078 = 0;   /* dirs probes live in in.dirs */
    in.player_scr42 = 0x40; /* create4 gate: <= 0x80 survives */
    assert(create_ok(&o, 4, &in, &out));
    assert(o.family == AOS_OBJECT20_FAMILY_DIRS);
    assert(!o.deleted);
    assert(out.dirs.set_42c & 0x200);        /* shared tail ran */
    assert(out.dirs.anim_step_count == 1);   /* create4 F17C */
    /* update4 state 0 opens window 0xF */
    assert(aos_object20_update(&o, &in, &out) >= 0);
    assert(out.dirs.window_text == 0xF);
    assert(out.dirs.b878 == 0x1040);

    /* param0 5-8 create dispatch (smoke: all survive) */
    for (int16_t p = 5; p <= 8; ++p) {
        memset(&o, 0, sizeof o);
        in = mk_in();
        assert(create_ok(&o, p, &in, &out));
        assert(o.family == AOS_OBJECT20_FAMILY_DIRS);
        assert(!o.deleted);
    }

    /* ---- story family. ---- */
    /* param0 9 create needs flag12078_26 clear, player > 0x7F,
     * keep + spawn probes. */
    memset(&o, 0, sizeof o);
    in = mk_in();
    in.player_scr42 = 0x100;
    assert(create_ok(&o, 9, &in, &out));
    assert(o.family == AOS_OBJECT20_FAMILY_STORY);
    assert(!o.deleted);
    /* param0 10 create gates: flag12078_27 clear, +0x37E bit
     * 0x40 set, player +0x42 <= 0x80. */
    memset(&o, 0, sizeof o);
    in = mk_in();
    in.story.flags37e = 0x40;
    in.player_scr42 = 0x40;
    assert(create_ok(&o, 10, &in, &out));
    assert(o.family == AOS_OBJECT20_FAMILY_STORY);
    assert(!o.deleted);

    /* param0 21: native create[21] = sub_08062068 marks
     * +0x59 |= 8 -> the shared tail deletes the entity. This
     * routed to the story module's case 21. */
    memset(&o, 0, sizeof o);
    in = mk_in();
    assert(!create_ok(&o, 21, &in, &out));
    assert(o.deleted);
    assert(out.deleted);

    /* param0 22: native create[22] = sub_08062244 (+0x42C |= 1)
     * via the story module's case 22; the entity survives. */
    memset(&o, 0, sizeof o);
    in = mk_in();
    assert(create_ok(&o, 22, &in, &out));
    assert(o.family == AOS_OBJECT20_FAMILY_STORY);
    assert(!o.deleted);
    assert(out.story.set_42c & 1);

    /* param0 23: native create[23] = sub_08062260 (the escort
     * create) via the story module's case 23; needs +0x42C
     * bit 0x2000. */
    memset(&o, 0, sizeof o);
    in = mk_in();
    in.flag42c = 0x2000;
    assert(create_ok(&o, 23, &in, &out));
    assert(!o.deleted);
    assert(out.story.cf0 == 0x19);

    /* param0 24: native create[24] = sub_080657B4 via
     * aos_npc_talk_657b4 — +0x42C bit 0x4000 arms the song
     * path. */
    memset(&o, 0, sizeof o);
    in = mk_in();
    in.flag42c = 0x4000;
    assert(create_ok(&o, 24, &in, &out));
    assert(o.family == AOS_OBJECT20_FAMILY_STORY);
    assert(!o.deleted);
    assert(out.call_ef6c);
    assert(out.song == 0x1000);
    assert(out.set_42c & 0x10020);
    /* without 0x4000 the helper marks +0x59 |= 8 -> delete */
    memset(&o, 0, sizeof o);
    in = mk_in();
    in.flag42c = 0;
    assert(!create_ok(&o, 24, &in, &out));
    assert(o.deleted);

    /* param0 29: native create[29] = sub_0805BF4C via the
     * story module's case 29 — needs +0x42C bit 0x8000;
     * update still routes to story update29 (sub_0805C08C). */
    memset(&o, 0, sizeof o);
    in = mk_in();
    in.flag42c = 0x2000 | 0x8000;
    assert(create_ok(&o, 29, &in, &out));
    assert(o.family == AOS_OBJECT20_FAMILY_STORY);
    assert(!out.unported);
    assert(!o.deleted);
    assert(out.story.fade_3d18c && out.story.fade_3cdf0);
    assert(out.story.dma_setup && out.story.unk2cb0_feff);
    assert(aos_object20_update(&o, &in, &out) == 0);
    assert(out.story.set_42c & 0x20);
    assert(out.story.anim_step_count == 1);
    /* without bit 0x8000 the create gate marks +0x59 |= 8. */
    memset(&o, 0, sizeof o);
    in = mk_in();
    assert(!create_ok(&o, 29, &in, &out));
    assert(o.deleted);

    /* ---- shop family: param0 19 sell / 20 buy. ---- */
    memset(&o, 0, sizeof o);
    in = mk_in();
    in.story.ret_6e0d0 = 1;         /* sub_0806E0D0 low byte live */
    assert(create_ok(&o, 19, &in, &out));
    assert(o.family == AOS_OBJECT20_FAMILY_SHOP);
    assert(!out.unported);          /* create sub_08061A00 ported */
    assert(!o.deleted);
    assert(out.story.set_42c & 1);
    assert(o.v.shop.vel48 == (int32_t)0xFFFF0000);
    assert(o.scr42 == (int16_t)(0x150 - 0x40));
    assert(o.anim5c == 0x11);
    in.shop.probe = 0;              /* player +0x42 + A094[6] */
    assert(aos_object20_update(&o, &in, &out) == 0);
    assert(out.shop.b878 == 0x1040);
    assert(out.shop.set_42c & 1);
    assert(o.v.shop.state == 2);

    memset(&o, 0, sizeof o);
    in = mk_in();
    assert(create_ok(&o, 20, &in, &out));
    assert(o.family == AOS_OBJECT20_FAMILY_SHOP);
    assert(!o.deleted);
    assert(out.set_42c & 1);        /* create sub_08062040 arm */
    assert(out.fade_out);
    assert(aos_object20_update(&o, &in, &out) == 0);
    assert(out.shop.window_text == 0x20);   /* buy opens text 0x20 */
    assert(o.v.shop.state == 1);

    /* ---- npc_talk family: creates + updates. ---- */
    /* param0 25: create sub_080659A4 needs flag 0x2E CLEAR. */
    memset(&o, 0, sizeof o);
    in = mk_in();
    in.npc.flag12078 = (1ull << 0x20);      /* 0x2E clear */
    in.npc.cam_x = 0x20;
    in.npc.cam_y = 0x10;
    in.npc.ent_pos40 = 0x2000;
    in.player_pos40 = 0x1000;
    in.player_anim5c = 3;
    assert(create_ok(&o, 25, &in, &out));
    assert(o.family == AOS_OBJECT20_FAMILY_GUIDE);
    assert(!o.deleted);
    assert(o.scr42 == (int16_t)(0x60 - 0x20));
    assert(o.scr46 == (int16_t)(0xA0 - 0x10));
    assert(o.anim5c == 2);          /* player +0x5C - 1 */
    /* update sub_08065A5C state 0 opens window 0x28 */
    assert(aos_object20_update(&o, &in, &out) == 0);
    assert(o.v.guide.state_a == 1);
    assert(o.v.guide.window_open);
    assert(o.guide_flag_417);

    /* param0 26: create sub_08065CD8 needs flags 0x20 + 0x2E. */
    memset(&o, 0, sizeof o);
    in = mk_in();
    in.npc.flag12078 = (1ull << 0x20) | (1ull << 0x2E);
    in.npc.rng_bit = 1;
    in.player_anim5c = 3;
    assert(create_ok(&o, 26, &in, &out));
    assert(o.family == AOS_OBJECT20_FAMILY_LOOP);
    assert(!o.deleted);
    assert(o.anim5c == 4);          /* player +0x5C + 1 */
    assert((o.v.npc_loop.flags58 & 0x40) != 0);
    /* update sub_08065DB8 state 0 -> 1 */
    assert(aos_object20_update(&o, &in, &out) == 0);
    assert(o.v.npc_loop.state == 1);
    assert(o.v.npc_loop.cleared_42c_200);

    /* param0 27: create sub_08066024 needs flag 0x20. */
    memset(&o, 0, sizeof o);
    in = mk_in();
    in.npc.flag12078 = (1ull << 0x20);
    in.player_anim5c = 3;
    assert(create_ok(&o, 27, &in, &out));
    assert(o.family == AOS_OBJECT20_FAMILY_GUARD);
    assert(!o.deleted);
    assert(o.v.guard.field18 == 1);
    assert(o.anim5c == 4);
    assert(out.npc_ret == 1);
    /* update sub_08066094 state 0 -> 1 */
    assert(aos_object20_update(&o, &in, &out) == 0);
    assert(o.v.guard.state == 1);
    assert(o.v.guard.cleared_42c_200);
    assert(o.v.guard.sfx_26_entry);

    /* param0 28: create sub_08066828 needs flags 0x20 + 0x26. */
    memset(&o, 0, sizeof o);
    in = mk_in();
    in.npc.flag12078 = (1ull << 0x20) | (1ull << 0x26);
    assert(create_ok(&o, 28, &in, &out));
    assert(o.family == AOS_OBJECT20_FAMILY_TALK_LOOP);
    assert(!o.deleted);
    assert((o.v.talk_loop.flags58 & 0x40) != 0);
    /* update sub_080666D4: state 0 -> 1, then the gated state-1
     * frame runs the composed sub_080667DC text pick. */
    assert(aos_object20_update(&o, &in, &out) == 0);
    assert(o.v.talk_loop.state == 1);
    assert(o.v.talk_loop.cleared_42c_200);
    in.npc.probe_inside = true;
    in.word16 = 0x42;
    in.npc.cond_5b864 = true;
    in.npc.rng = 0;             /* (rng & 0xF) <= 3 -> text 0x59 */
    assert(aos_object20_update(&o, &in, &out) == 0);
    assert(o.v.talk_loop.state == 2);
    assert(o.v.talk_loop.window_text);
    assert(o.v.talk_loop.text_id == 0x59);
    in.text_done = true;
    assert(aos_object20_update(&o, &in, &out) == 0);
    assert(o.v.talk_loop.state == 0);
    assert(o.v.talk_loop.mark_38);
    assert(o.v.talk_loop.release_0);

    /* param0 30/31: story family — creates and updates are
     * ported (sub_0805B888/5B8F4, sub_0805C49C/5C628). */
    memset(&o, 0, sizeof o);
    in = mk_in();
    assert(create_ok(&o, 30, &in, &out));
    assert(o.family == AOS_OBJECT20_FAMILY_STORY);
    assert(!out.unported);
    assert(!o.deleted);
    assert(out.story.set_42c & 0x80021);
    assert(out.story.song == 0x1000);
    assert(aos_object20_update(&o, &in, &out) == 0);
    assert(!out.unported);
    assert(out.story.player_scr42_val == 0x78);

    memset(&o, 0, sizeof o);
    in = mk_in();
    assert(create_ok(&o, 31, &in, &out));
    assert(o.family == AOS_OBJECT20_FAMILY_STORY);
    assert(!out.unported);
    assert(!o.deleted);
    assert(aos_object20_update(&o, &in, &out) == 0);
    assert(!out.unported);
    assert(out.story.anim_step_count == 1);

    /* ---- unported/out-of-table param0 values: shared shell +
     * report. Every in-table param0 (0-31) now resolves a family. */
    for (unsigned i = 0; i < sizeof all_params / sizeof all_params[0];
         ++i) {
        int16_t p = all_params[i];
        if (p >= 0 && p <= 31)
            continue;   /* covered families */
        memset(&o, 0, sizeof o);
        in = mk_in();
        assert(create_ok(&o, p, &in, &out));
        assert(o.family == AOS_OBJECT20_FAMILY_INERT);
        assert(out.unported);
        assert(!o.deleted);
        assert(out.set_42c & 0x200);        /* shared tail ran */
        assert(aos_object20_update(&o, &in, &out) == 0);
        assert(out.unported);
        assert(out.c5a8 == 1);              /* flag42c bit0 clear */
    }

    /* create gate failure deletes before any variant body. */
    memset(&o, 0, sizeof o);
    in = mk_in();
    in.flag1327a = 0;
    in.flag42c = 0;             /* no 0x200000 pass bit */
    assert(!create_ok(&o, 26, &in, &out));
    assert(o.deleted);
    assert(out.deleted);

    printf("aos_object20 tests passed\n");
    return 0;
}
