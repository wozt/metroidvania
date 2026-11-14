/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_object20.h"

#include <string.h>

/* Unified Object20 dispatcher — see aos_object20.h for the
 * param0 -> native routine map and the known story-create
 * divergence. The shared Object20Create gates/tail and the
 * Object20Update prologue/tail are reproduced here for the
 * families whose ported modules do not fold them in
 * (npc_talk, shop, unported); dirs/story delegate whole calls
 * to their own folded copies. */

static void out_reset(AosObject20Out *out) {
    memset(out, 0, sizeof *out);
    out->setplayerinput = -1;
    out->setplayerinput_arg = 0;
    out->c5a8 = -1;
    out->song = -1;
    out->npc_ret = -1;
}

static uint8_t family_of(int16_t param0) {
    if (param0 >= 4 && param0 <= 8) return AOS_OBJECT20_FAMILY_DIRS;
    if (param0 == 9 || param0 == 10 ||
        (param0 >= 21 && param0 <= 24) ||
        (param0 >= 29 && param0 <= 31))
        return AOS_OBJECT20_FAMILY_STORY;
    if (param0 == 19 || param0 == 20) return AOS_OBJECT20_FAMILY_SHOP;
    if (param0 == 25) return AOS_OBJECT20_FAMILY_GUIDE;
    if (param0 == 26) return AOS_OBJECT20_FAMILY_LOOP;
    if (param0 == 27) return AOS_OBJECT20_FAMILY_GUARD;
    if (param0 == 28) return AOS_OBJECT20_FAMILY_TALK_LOOP;
    if (param0 == 3) return AOS_OBJECT20_FAMILY_OPENING;
    return AOS_OBJECT20_FAMILY_INERT;
}

/* Copy the shared context block into the dirs/story In
 * structs; their duplicate shared fields are ignored. */
static AosObject20DirsIn dirs_in(const AosObject20In *in) {
    AosObject20DirsIn d = in->dirs;
    d.flag1327a = in->flag1327a;
    d.flag42c = in->flag42c;
    d.byte8e = in->byte8e;
    d.byte13266 = in->byte13266;
    d.word16 = in->word16;
    d.word60 = in->word60;
    d.byte41b = in->byte41b;
    d.byte414 = in->byte414;
    d.text_done = in->text_done;
    d.keep_5b754 = in->keep_5b754;
    d.a094_4 = in->a094_4;
    d.a094_6 = in->a094_6;
    d.a094_a = in->a094_a;
    d.player_scr42 = in->player_scr42;
    d.player_flags58 = in->player_flags58;
    return d;
}

static AosObject20StoryIn story_in(const AosObject20In *in) {
    AosObject20StoryIn s = in->story;
    s.flag1327a = in->flag1327a;
    s.flag42c = in->flag42c;
    s.byte8e = in->byte8e;
    s.byte13266 = in->byte13266;
    s.word16 = in->word16;
    s.word60 = in->word60;
    s.byte41b = in->byte41b;
    s.byte414 = in->byte414;
    s.text_done = in->text_done;
    s.keep_5b754 = in->keep_5b754;
    s.a094_6 = in->a094_6;
    s.a094_a = in->a094_a;
    s.player_scr42 = in->player_scr42;
    s.player_pos40 = in->player_pos40;
    s.player_pos44 = in->player_pos44;
    s.player_anim5c = in->player_anim5c;
    s.player_flags58 = in->player_flags58;
    return s;
}

/* Shared Object20Create gates (0x0805B42C): +0x1327A > 0 or
 * +0x42C bit 0x200000 (0x80 << 0xE) passes; then +0x8E bit 0x80
 * fails, and +0x13266 != 0 requires +0x42C bit 0x8000. NOTE:
 * aos_object20_story tests 0x20000000 here — the ROM literal is
 * 0x0805B44A `movs r1,#0x80; lsls r1,#0xE` = 0x00200000; this
 * dispatcher uses the ROM value (possible story-module bug,
 * reported). */
static bool create_gate_pass(const AosObject20In *in) {
    bool pass;
    if (in->flag1327a > 0) {
        pass = true;
    } else {
        pass = (in->flag42c & 0x200000u) != 0;
    }
    if (pass) {
        if (in->byte8e & 0x80) {
            pass = false;
        } else if (in->byte13266 != 0 &&
                   (in->flag42c & 0x8000u) == 0) {
            pass = false;
        }
    }
    return pass;
}

/* Shared Object20Create tail (0x0805B4B8): the +0x59 bit-3
 * EntityDelete check runs BEFORE the tail; a deleted entity
 * returns false with no tail effects. The tail ORs +0x42C
 * 0x200, runs the three engine init calls, sub_0800C5A8(0),
 * the +0x41C self-link, the conditional SetPlayerInput(1,0)
 * (the ROM re-reads +0x42C after the OR — bit 0 is unaffected)
 * and sub_0806666C. */
static bool shell_create_tail(AosObject20 *o, const AosObject20In *in,
                              AosObject20Out *out) {
    if (o->flags59 & 0x8) {
        o->deleted = true;
        out->deleted = true;
        return false;
    }
    out->set_42c |= 0x200;
    out->call_0eb04 = true;
    out->call_235c8 = true;
    out->call_2dff0 = true;
    out->c5a8 = 0;
    out->call_6666c = true;
    if ((in->flag42c | out->set_42c) & 1) {
        out->setplayerinput = 1;
        out->setplayerinput_arg = 0;
    }
    return true;
}

/* Run the shared create shell for a family that has no folded
 * create port: gates, then `variant` (NULL = unported, reported
 * and the entity kept inert), then the flags59/delete check and
 * the shared tail. */
static bool shell_create(AosObject20 *o, const AosObject20In *in,
                         AosObject20Out *out, bool unported) {
    if (!create_gate_pass(in)) {
        o->deleted = true;
        out->deleted = true;
        return false;
    }
    if (unported)
        out->unported = true;
    return shell_create_tail(o, in, out);
}

/* param0-24 create: native create[24] = sub_080657B4, ported
 * as aos_npc_talk_657b4 — without +0x42C bit 0x4000 the entity
 * is marked +0x59 bit 3, otherwise +0x42C |= 0x20|0x10000,
 * sub_0800EF6C runs and PlaySong(0x1000) plays. The helper
 * mutates the global +0x42C word, so a copy is passed and the
 * delta reported. */
static bool create24_657b4(AosObject20 *o, const AosObject20In *in,
                           AosObject20Out *out) {
    if (!create_gate_pass(in)) {
        o->deleted = true;
        out->deleted = true;
        return false;
    }
    uint32_t f42c = in->flag42c;
    bool called_ef6c = false;
    uint16_t song = 0;
    aos_npc_talk_657b4(&o->flags59, &f42c, &called_ef6c, &song);
    out->set_42c |= f42c & ~in->flag42c;
    out->clear_42c |= in->flag42c & ~f42c;
    out->call_ef6c = called_ef6c;
    if (song != 0)
        out->song = song;
    return shell_create_tail(o, in, out);
}

static bool flag12078(const AosObject20In *in, unsigned bit) {
    return (in->npc.flag12078 & (1ull << bit)) != 0;
}

/* param0-25 create: native create[25] = sub_080659A4, the
 * guide/follower spawn (aos_npc_talk_659a4). */
static bool create25_659a4(AosObject20 *o, const AosObject20In *in,
                           AosObject20Out *out) {
    if (!create_gate_pass(in)) {
        o->deleted = true;
        out->deleted = true;
        return false;
    }
    AosNpcSpawn s;
    memset(&s, 0, sizeof s);
    aos_npc_talk_659a4(&s, flag12078(in, 0x2E), in->keep_5b754,
                       in->npc.cam_x, in->npc.cam_y,
                       in->npc.ent_pos40, in->player_pos40,
                       in->player_anim5c);
    o->v.guide.flags58 = s.flags58;
    o->flags59 = s.flags59;
    o->anim5c = s.anim5c;
    o->scr42 = s.field42;
    o->scr46 = s.field46;
    if (s.deleted)
        out->npc_deleted = true;
    if (s.flag42c & 1)
        out->set_42c |= 1;
    if (s.anim_stepped)
        out->npc_anim_steps++;
    return shell_create_tail(o, in, out);
}

/* param0-26 create: native create[26] = sub_08065CD8, the
 * companion spawn (aos_npc_talk_65cd8). */
static bool create26_65cd8(AosObject20 *o, const AosObject20In *in,
                           AosObject20Out *out) {
    if (!create_gate_pass(in)) {
        o->deleted = true;
        out->deleted = true;
        return false;
    }
    AosNpcCompanion c;
    memset(&c, 0, sizeof c);
    aos_npc_talk_65cd8(&c, flag12078(in, 0x20), flag12078(in, 0x2E),
                       in->keep_5b754, in->npc.rng_bit,
                       in->player_anim5c, in->npc.spawn_child);
    o->v.npc_loop.flags58 = c.flags58;
    o->flags59 = c.flags59;
    o->anim5c = c.anim5c;
    o->scr42 = c.spawn_x;
    o->scr46 = c.spawn_y;
    /* API gap: the companion's +0x18 child entity and its
     * sub_08065C2C update vector have no field in AosNpcLoop;
     * c.child/c.child_* are reported nowhere persistent. */
    if (c.marked)
        out->npc_marked = true;
    return shell_create_tail(o, in, out);
}

/* param0-27 create: native create[27] = sub_08066024, the
 * follower spawn gate (aos_npc_talk_66024). */
static bool create27_66024(AosObject20 *o, const AosObject20In *in,
                           AosObject20Out *out) {
    if (!create_gate_pass(in)) {
        o->deleted = true;
        out->deleted = true;
        return false;
    }
    AosNpcSpawnGate g;
    memset(&g, 0, sizeof g);
    uint32_t ret = aos_npc_talk_66024(&g, flag12078(in, 0x20),
                                      in->keep_5b754,
                                      in->player_anim5c);
    o->v.guard.flags58 = g.flags58;
    o->flags59 = g.flags59;
    o->v.guard.field18 = g.field18;
    o->anim5c = g.anim5c;
    out->npc_ret = (int)ret;
    return shell_create_tail(o, in, out);
}

/* param0-28 create: native create[28] = sub_08066828, the
 * repeating-NPC spawn gate (aos_npc_talk_66828). */
static bool create28_66828(AosObject20 *o, const AosObject20In *in,
                           AosObject20Out *out) {
    if (!create_gate_pass(in)) {
        o->deleted = true;
        out->deleted = true;
        return false;
    }
    AosNpcSpawnGate2 g;
    memset(&g, 0, sizeof g);
    aos_npc_talk_66828(&g, flag12078(in, 0x20), flag12078(in, 0x26),
                       in->keep_5b754);
    o->v.talk_loop.flags58 = g.flags58;
    o->flags59 = g.flags59;
    if (g.marked_8)
        out->npc_marked = true;
    if (g.deleted)
        out->npc_deleted = true;
    if (g.anim_step)
        out->npc_anim_steps++;
    return shell_create_tail(o, in, out);
}

bool aos_object20_create(AosObject20 *o, int16_t param0,
                         const AosObject20In *in,
                         AosObject20Out *out) {
    out_reset(out);
    memset(o, 0, sizeof *o);   /* fresh entity, like the ROM */
    o->param0 = param0;
    o->family = family_of(param0);

    switch (o->family) {
    case AOS_OBJECT20_FAMILY_OPENING:
        /* param0 3: native create sub_0805CE28 / update
         * sub_0805CED0 belong to the bespoke opening-scene path
         * (aos_opening_event); the host keeps that logic. */
        out->handled_externally = true;
        return true;
    case AOS_OBJECT20_FAMILY_DIRS: {
        AosObject20DirsIn din = dirs_in(in);
        bool ok = aos_object20_dirs_create(&o->v.dirs,
                                           (uint8_t)param0, &din,
                                           &out->dirs);
        o->deleted = !ok || o->v.dirs.deleted;
        out->deleted = o->deleted;
        o->return_code = out->dirs.return_code;
        out->return_code = o->return_code;
        return ok;
    }
    case AOS_OBJECT20_FAMILY_STORY: {
        if (param0 == 24) {
            /* Native create[24] = sub_080657B4 lives in the
             * npc_talk module, not the story module. */
            bool ok = create24_657b4(o, in, out);
            o->deleted = !ok;
            out->deleted = o->deleted;
            return ok;
        }
        /* param0 9/10/21/22/23/29/30/31 route to the story
         * module with the native table index unchanged (its
         * create dispatch now matches 0x08526658); 24 was
         * handled above. */
        AosObject20StoryIn sin = story_in(in);
        bool ok = aos_object20_story_create(&o->v.story,
                                            (uint8_t)param0, &sin,
                                            &out->story);
        o->deleted = !ok || o->v.story.deleted;
        out->deleted = o->deleted;
        o->return_code = out->story.return_code;
        out->return_code = o->return_code;
        return ok;
    }
    case AOS_OBJECT20_FAMILY_SHOP:
        if (param0 == 20) {
            /* Native create[20] = sub_08062040: +0x42C |= 1
             * plus the sub_0803D270/sub_0803CED4 fade pair
             * (aos_npc_talk_arm_fade documents the same stub). */
            if (!create_gate_pass(in)) {
                o->deleted = true;
                out->deleted = true;
                return false;
            }
            out->set_42c |= 1;
            out->fade_out = true;
            return shell_create_tail(o, in, out);
        }
        /* param0 19: native create[19] = sub_08061A00 (the shop
         * sell spawn gate) is ported in the story module — the
         * Object20 create-table owner — while update[19]
         * stays the shop sell routine. The create runs on
         * v.story through the shared shell, then the state
         * bytes both layouts share (+0xA/+0xB/+0xC/+0xD,
         * +0x40/+0x48, +0x6D) are mirrored into v.shop and the
         * flat +0x42/+0x46/+0x58/+0x59/+0x5C fields so the
         * shop update sees the entity the ROM left behind. */
        {
            AosObject20StoryIn sin = story_in(in);
            bool ok = aos_object20_story_create(&o->v.story, 19,
                                                &sin, &out->story);
            o->v.shop.state  = o->v.story.state;
            o->v.shop.menu   = o->v.story.sub_b;
            o->v.shop.cursor = o->v.story.sub_c;
            o->v.shop.timer  = o->v.story.timer_d;
            o->v.shop.pos40  = o->v.story.pos40;
            o->v.shop.vel48  = o->v.story.vel48;
            o->v.shop.flag6d = o->v.story.anim6d;
            o->scr42   = o->v.story.scr42;
            o->scr46   = o->v.story.scr46;
            o->flags58 = o->v.story.flags58;
            o->flags59 = o->v.story.flags59;
            o->anim5c  = o->v.story.anim5c;
            o->deleted = !ok || o->v.story.deleted;
            out->deleted = o->deleted;
            o->return_code = out->story.return_code;
            out->return_code = o->return_code;
            return ok;
        }
    case AOS_OBJECT20_FAMILY_GUIDE:
        {
            bool ok = create25_659a4(o, in, out);
            o->deleted = !ok;
            out->deleted = o->deleted;
            return ok;
        }
    case AOS_OBJECT20_FAMILY_LOOP:
        {
            bool ok = create26_65cd8(o, in, out);
            o->deleted = !ok;
            out->deleted = o->deleted;
            return ok;
        }
    case AOS_OBJECT20_FAMILY_GUARD:
        {
            bool ok = create27_66024(o, in, out);
            o->deleted = !ok;
            out->deleted = o->deleted;
            return ok;
        }
    case AOS_OBJECT20_FAMILY_TALK_LOOP:
        {
            bool ok = create28_66828(o, in, out);
            o->deleted = !ok;
            out->deleted = o->deleted;
            return ok;
        }
    case AOS_OBJECT20_FAMILY_INERT:
    default:
        /* Every in-table param0 now has a ported create and/or
         * update; values past the 32-entry tables read garbage
         * natively. The shared shell runs and the gap is
         * reported; the entity stays inert. */
        {
            bool ok = shell_create(o, in, out, true);
            o->deleted = !ok;
            out->deleted = o->deleted;
            return ok;
        }
    }
}

/* Shared Object20Update prologue (0x0805B532): +0x42C bit 0x80
 * ORs +0x7864 bit 2; a negative (s8) +0xA with +0x41B set calls
 * sub_08067804(-3); otherwise the +0x16&8 / +0x60&2 /
 * !(+0x42C&0x10000) gate rewrites +0x414. */
static void update_prologue(const AosObject20In *in,
                            AosObject20Out *out, int8_t state_a) {
    if (in->flag42c & 0x80)
        out->unk7864_or2 = true;
    if (state_a < 0) {
        if (in->byte41b != 0)
            out->call_67804 = true;
    } else {
        if ((in->word16 & 8) && (in->word60 & 2) &&
            !(in->flag42c & 0x10000)) {
            out->write_414 = true;
            out->write_414_val =
                (uint8_t)((in->byte414 & ~0x10u) | 5u);
        }
    }
}

/* Shared Object20Update tail (0x0805B5A2): +0x42C bit 0 selects
 * SetPlayerInput(1, ret) + sub_0800C5A8(0), else
 * sub_0800C5A8(1). */
static void update_tail(const AosObject20In *in, AosObject20Out *out,
                        int ret) {
    if (in->flag42c & 1) {
        out->setplayerinput = 1;
        out->setplayerinput_arg = ret;
        out->c5a8 = 0;
    } else {
        out->c5a8 = 1;
    }
}

/* sub_08065DB8's packed `flags` argument: bit0 probe_inside,
 * bit1 gEwramData+0x16 & 0x42, bit2 +0x4BE == 0, bit3
 * sub_0805B864, bits 4-10 the flag bytes 0x32, 0x26, 0x31,
 * 0x25, 0x30, 0x2E, 0x2F in that order. */
static uint32_t npc_flags_65db8(const AosObject20In *in) {
    uint32_t f = 0;
    static const unsigned order[7] =
        { 0x32, 0x26, 0x31, 0x25, 0x30, 0x2E, 0x2F };
    if (in->npc.probe_inside) f |= 1;
    if (in->word16 & 0x42) f |= 2;
    if (in->npc.gate_4be == 0) f |= 4;
    if (in->npc.cond_5b864) f |= 8;
    for (unsigned i = 0; i < 7; ++i)
        if (flag12078(in, order[i]))
            f |= 1u << (4 + i);
    return f;
}

/* sub_08066094's `flags`: bit0 probe_inside, bit1 +0x16 & 0x42,
 * bit2 +0x4BE == 0, bit3 sub_0805B864. */
static uint32_t npc_flags_66094(const AosObject20In *in) {
    uint32_t f = 0;
    if (in->npc.probe_inside) f |= 1;
    if (in->word16 & 0x42) f |= 2;
    if (in->npc.gate_4be == 0) f |= 4;
    if (in->npc.cond_5b864) f |= 8;
    return f;
}

/* sub_080666D4's `flags`: bit0 probe_inside, bit1 +0x16 & 0x42,
 * bit2 +0x4BE == 0, bit3 sub_08067A04 done. */
static uint32_t npc_flags_666d4(const AosObject20In *in) {
    uint32_t f = 0;
    if (in->npc.probe_inside) f |= 1;
    if (in->word16 & 0x42) f |= 2;
    if (in->npc.gate_4be == 0) f |= 4;
    if (in->text_done) f |= 8;
    return f;
}

static int update_shop(AosObject20 *o, const AosObject20In *in,
                       AosObject20Out *out) {
    update_prologue(in, out, (int8_t)o->v.shop.state);
    AosShopInput sin = in->shop;
    sin.text_done = in->text_done;   /* same sub_08067A04 probe */
    int ret;
    if (o->param0 == 19) {           /* update[19] = sub_08061ABC */
        ret = aos_shop_sell_update(&o->v.shop, &sin, &out->shop);
    } else {                         /* update[20] = sub_08061D10 */
        ret = aos_shop_buy_update(&o->v.shop, &sin, &out->shop);
    }
    update_tail(in, out, ret);
    o->return_code = ret;
    out->return_code = ret;
    return ret;
}

static int update_npc(AosObject20 *o, const AosObject20In *in,
                      AosObject20Out *out) {
    int ret = 0;
    switch (o->family) {
    case AOS_OBJECT20_FAMILY_GUIDE:
        /* update[25] = sub_08065A5C, returns the r7 flag word. */
        update_prologue(in, out, (int8_t)o->v.guide.state_a);
        ret = (int)aos_npc_talk_65a5c(&o->v.guide, in->text_done,
                                    (int16_t)in->player_scr42,
                                    in->player_pos40,
                                    in->player_flags58,
                                    &o->guide_player_bit6,
                                    &o->guide_flag_417);
        break;
    case AOS_OBJECT20_FAMILY_LOOP:
        /* update[26] = sub_08065DB8 (void). */
        update_prologue(in, out, (int8_t)o->v.npc_loop.state);
        aos_npc_talk_65db8(&o->v.npc_loop,
                           npc_flags_65db8(in), in->text_done,
                           in->player_pos40);
        ret = 0;
        break;
    case AOS_OBJECT20_FAMILY_GUARD:
        /* update[27] = sub_08066094, returns 0 or 0x10. */
        update_prologue(in, out, (int8_t)o->v.guard.state);
        ret = (int)aos_npc_talk_66094(&o->v.guard,
                                    npc_flags_66094(in),
                                    in->npc.picked_text,
                                    in->text_done,
                                    in->player_pos40);
        break;
    case AOS_OBJECT20_FAMILY_TALK_LOOP:
        /* update[28] = sub_080666D4; the ROM calls
         * sub_080667DC inside the state-1 gated branch to fill
         * +0x14, so the dispatcher composes the pick here. */
        update_prologue(in, out, (int8_t)o->v.talk_loop.state);
        if (o->v.talk_loop.state == 1 &&
            (npc_flags_666d4(in) & 7) == 7) {
            aos_npc_talk_667dc(&o->v.talk_loop,
                               in->npc.cond_5b864, in->npc.rng,
                               &o->guard_seq);
        }
        aos_npc_talk_666d4(&o->v.talk_loop, npc_flags_666d4(in),
                           in->npc.cond_5b864, in->npc.rng);
        ret = 0;
        break;
    default:
        break;
    }
    update_tail(in, out, ret);
    o->return_code = ret;
    out->return_code = ret;
    return ret;
}

int aos_object20_update(AosObject20 *o, const AosObject20In *in,
                        AosObject20Out *out) {
    out_reset(out);
    switch (o->family) {
    case AOS_OBJECT20_FAMILY_OPENING:
        out->handled_externally = true;
        return 0;
    case AOS_OBJECT20_FAMILY_DIRS: {
        AosObject20DirsIn din = dirs_in(in);
        int ret = aos_object20_dirs_update(&o->v.dirs,
                                           (uint8_t)o->param0,
                                           &din, &out->dirs);
        o->deleted = o->v.dirs.deleted;
        out->deleted = o->deleted;
        o->return_code = ret;
        out->return_code = ret;
        return ret;
    }
    case AOS_OBJECT20_FAMILY_STORY: {
        AosObject20StoryIn sin = story_in(in);
        int ret = aos_object20_story_update(&o->v.story,
                                            (uint8_t)o->param0,
                                            &sin, &out->story);
        o->deleted = o->v.story.deleted;
        out->deleted = o->deleted;
        o->return_code = ret;
        out->return_code = ret;
        return ret;
    }
    case AOS_OBJECT20_FAMILY_SHOP:
        return update_shop(o, in, out);
    case AOS_OBJECT20_FAMILY_GUIDE:
    case AOS_OBJECT20_FAMILY_LOOP:
    case AOS_OBJECT20_FAMILY_GUARD:
    case AOS_OBJECT20_FAMILY_TALK_LOOP:
        return update_npc(o, in, out);
    case AOS_OBJECT20_FAMILY_INERT:
    default:
        /* Unported variant body: the shared prologue/tail still
         * run (they are param-independent engine effects) and
         * the gap is reported. */
        update_prologue(in, out, 0);
        out->unported = true;
        update_tail(in, out, 0);
        o->return_code = 0;
        out->return_code = 0;
        return 0;
    }
}
