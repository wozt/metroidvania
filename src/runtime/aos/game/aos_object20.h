/* SPDX-License-Identifier: GPL-3.0-only */
/* Unified Object20 (SPECIAL entity id 32, native object 0x20)
 * dispatcher. Object20Create (0x0805B42C) / Object20Update
 * (0x0805B510) dispatch on the entity +0x30 s16 through the
 * 32-entry pointer tables at 0x08526658 (create) and
 * 0x085266D8 (update — the second half of the same 64-word
 * table). This module routes each param0 value to the already
 * ported variant routine:
 *
 *   param0  create (0x08526658)   update (0x085266D8)  owner
 *   3       sub_0805CE28           sub_0805CED0          opening
 *           scene — handled by the bespoke opening path
 *           (aos_opening_event), NOT here.
 *   4-8     sub_0805F120..5FD65    sub_0805F268..5FDF1   aos_object20_dirs
 *   9,10    sub_0805FFDC/606C8     sub_08060248/607E4    aos_object20_story
 *   19      sub_08061A00           sub_08061ABC (sell)  story create /
 *           shop update (the shared state bytes are mirrored
 *           into v.shop — see aos_object20_create)
 *   20      sub_08062040           sub_08061D10 (buy)   aos_shop
 *   21      sub_08062068           sub_08062074          aos_object20_story
 *   22      sub_08062244           sub_080620E8          aos_object20_story
 *   23      sub_08062260           sub_080623F0          aos_object20_story
 *   24      sub_080657B4           sub_08062664          story update /
 *           npc_talk create helper
 *   25      sub_080659A4           sub_08065A5C          aos_npc_talk
 *   26      sub_08065CD8           sub_08065DB8          aos_npc_talk
 *   27      sub_08066024           sub_08066094          aos_npc_talk
 *   28      sub_08066828           sub_080666D4          aos_npc_talk
 *   29      sub_0805BF4C           sub_0805C08C          aos_object20_story
 *   30      sub_0805B888           sub_0805B8F4          aos_object20_story
 *   31      sub_0805C49C           sub_0805C628          aos_object20_story
 *   other   (table reads past its end natively)         inert,
 *           reported
 *
 * The story module's create dispatch was corrected to the
 * 0x08526658 table indices (it previously ran native create
 * [N-1] for its case N on param0 21-24 and create[31] for case
 * 29); callers now pass the entity param0 unchanged.
 *
 * Integration status: wired into aria_runtime (the id-32
 * dispatch case and the gEwramData/player bridge landed with
 * object20_fill_in/object20_apply_out); classified partial in
 * data/native_parity/annotations.tsv — the param0-30
 * sub_080628AC whiteout sequence and the param0-19/30/31
 * engine probes are still injected, not bridged. */

#ifndef AOS_OBJECT20_H
#define AOS_OBJECT20_H

#include <stdbool.h>
#include <stdint.h>

#include "aos_object20_dirs.h"
#include "aos_object20_slots.h"
#include "aos_object20_story.h"
#include "aos_npc_talk.h"
#include "aos_shop.h"

/* Resolved variant family for the entity's param0. */
enum {
    AOS_OBJECT20_FAMILY_OPENING = 0, /* param0 3: bespoke path */
    AOS_OBJECT20_FAMILY_DIRS,        /* param0 4-8 */
    AOS_OBJECT20_FAMILY_STORY,       /* param0 9,10,21-24,29-31
                                      * (+19 create via story) */
    AOS_OBJECT20_FAMILY_SHOP,        /* param0 19 (sell), 20 (buy) */
    AOS_OBJECT20_FAMILY_GUIDE,       /* param0 25 */
    AOS_OBJECT20_FAMILY_LOOP,        /* param0 26 */
    AOS_OBJECT20_FAMILY_GUARD,       /* param0 27 */
    AOS_OBJECT20_FAMILY_TALK_LOOP,   /* param0 28 */
    AOS_OBJECT20_FAMILY_SLOTS,       /* param0 0-2, 11-18 */
    AOS_OBJECT20_FAMILY_INERT        /* param0 other */
};

/* npc_talk-family injected probes that no variant In struct
 * already carries. The flag bytes the NPC routines consume are
 * sub_08012078/event-flag reads; `flag12078` packs them as a
 * bitmask (bit i = sub_08012078(i) result, i up to 0x3F). */
typedef struct {
    uint64_t flag12078;     /* bit i = sub_08012078(i) != 0 */
    bool probe_inside;      /* sub_08068AD4 proximity probe hit */
    int16_t gate_4be;       /* gEwramData + 0x4BE s16 pause word */
    bool cond_5b864;        /* sub_0805B864 result */
    uint16_t flags37e;      /* gEwramData + 0x37E — param0-26
                             * companion child + param0-27
                             * pick ctx */
    uint32_t rng_pick;      /* RNG draw for the param0-27
                             * flag-0x26 &1 pick */
    uint32_t rng_queue;     /* RNG draw for the param0-27
                             * queue %9 fallback */
    uint8_t picked_text;    /* DEPRECATED: the param0-27 +0x14
                             * pick now runs the ported
                             * sub_08066308 picker through
                             * guard_pick; kept for callers
                             * that still fill it. */
    uint32_t rng;           /* RandomNumberGenerator (param0 28) */
    uint32_t rng_bit;       /* RNG&1 pick (param0 26 create) */
    bool (*spawn_child)(void);  /* sub_0805B5E8 child-spawn probe
                                 * for the param0-26 create; NULL
                                 * reports no child. */
    uint16_t cam_x, cam_y;  /* camera words for the param0-25
                             * follower spawn (+0x42/+0x46 park) */
    int32_t ent_pos40;      /* entity +0x40 fine X at create
                             * (param0-25 facing compare) */
} AosObject20NpcIn;

/* Per-entity state. Exactly one union member is live, selected
 * by `family`/`param0`; create zeroes the whole union before
 * delegating, matching the ROM's freshly allocated entity. */
typedef struct {
    int16_t param0;         /* entity +0x30 s16 variant selector */
    uint8_t family;         /* AOS_OBJECT20_FAMILY_* */
    bool deleted;           /* EntityDelete requested */
    int return_code;        /* last update's ROM r0 result */
    /* +0x42/+0x46 screen words, +0x58/+0x59/+0x5C shared by
     * the npc_talk/shop/unported variants whose ported state
     * structs do not carry them (dirs/story keep their own
     * copies inside their structs). */
    int16_t scr42, scr46;
    uint8_t flags58, flags59, anim5c;
    /* gEwramData +0x138F4 guard-sequence cursor block — a
     * global in the ROM; kept here as a single-consumer
     * approximation for the param0-28 text pick. */
    AosGuardSeq guard_seq;
    /* gEwramData +0x344 event bitfield + +0x1325C+0x18C cursor
     * for the ported param0-27 text pickers (sub_08066308);
     * persistent across calls like the ROM globals. */
    AosGuardPickCtx guard_pick;
    /* param0-25 guide in/out bytes the native code keeps in
     * gEwramData/player state across calls (sub_08065A5C's
     * player bit-6 and the +0x417 byte). */
    bool guide_player_bit6;
    bool guide_flag_417;
    union {
        AosObject20Dirs dirs;       /* param0 4-8 */
        AosObject20Story story;     /* param0 9,10,21-24,29-31
                                     * (+19 create only) */
        AosNpcGuide guide;          /* param0 25 */
        AosNpcLoop npc_loop;        /* param0 26 */
        AosNpcGuard guard;          /* param0 27 */
        AosNpcTalkLoop talk_loop;   /* param0 28 */
        AosShop shop;               /* param0 19/20 */
        AosObject20Slots slots;     /* param0 0-2, 11-18 */
    } v;
} AosObject20;

/* Injectable context. The shared block is the single source for
 * the Object20Create gates and Object20Update prologue reads;
 * the dispatcher copies it into the dirs/story sub-structs
 * (whose duplicate fields are ignored) before delegating. Only
 * the sub-struct for the entity's family is read. */
typedef struct {
    /* Shared Object20Create gates (0x0805B42C). */
    int32_t flag1327a;      /* s16 gEwramData + 0x1327A */
    uint32_t flag42c;       /* gEwramData + 0x42C on entry */
    uint8_t byte8e;         /* gEwramData + 0x8E */
    uint8_t byte13266;      /* gEwramData + 0x13266 */
    /* Shared Object20Update prologue reads. */
    uint16_t word16;        /* gEwramData + 0x16 */
    uint32_t word60;        /* gEwramData + 0x60 */
    uint8_t byte41b;        /* gEwramData + 0x41B */
    uint8_t byte414;        /* gEwramData + 0x414 */
    bool text_done;         /* sub_08067A04() != 0 */
    bool keep_5b754;        /* sub_0805B754(e, arg) keep result —
                             * arg-specific results collapse to
                             * this one bool, the same
                             * approximation the variant modules
                             * already make. */
    /* gEwramData + 0xA094 room/camera block. */
    int32_t a094_4;         /* +4 scroll word */
    uint16_t a094_6;        /* +6 room origin X */
    uint16_t a094_a;        /* +0xA room origin Y */
    uint16_t a094_0;        /* +0 room-width record (shop) */
    /* Player entity (gEwramData + 0x13110). */
    int32_t player_scr42;   /* +0x42 s16 / px probe */
    int32_t player_pos40;   /* +0x40 fine X */
    int32_t player_pos44;   /* +0x44 fine Y */
    uint8_t player_anim5c;  /* +0x5C */
    uint8_t player_flags58; /* +0x58 */
    /* Variant-specific probe blocks. */
    AosObject20DirsIn dirs;
    AosObject20StoryIn story;
    AosShopInput shop;
    AosObject20NpcIn npc;
    AosObject20SlotsIn slots;
} AosObject20In;

/* Outputs reset at the top of every call. The per-family variant
 * outs are written only for the entity's family; the flat
 * fields carry the shared Object20Create/Update shell effects
 * for the npc_talk/shop/unported families (the dirs/story outs
 * report their own copies of the same effects). */
typedef struct {
    AosObject20DirsOut dirs;
    AosObject20StoryOut story;
    AosShopOut shop;
    AosObject20SlotsOut slots;
    /* Shared-shell outputs (npc_talk/shop/unported families). */
    uint32_t set_42c;       /* bits OR-ed into gEwramData + 0x42C */
    uint32_t clear_42c;     /* bits AND-NOT-ed out of + 0x42C */
    bool unk7864_or2;       /* +0x7864 |= 2 (update prologue) */
    bool write_414;         /* +0x414 byte transform write */
    uint8_t write_414_val;
    bool call_67804;        /* sub_08067804(-3) — +0xA negative */
    int setplayerinput;     /* SetPlayerInput enable arg, -1 none */
    int setplayerinput_arg; /* SetPlayerInput second arg */
    int c5a8;               /* sub_0800C5A8 argument, -1 none */
    bool call_0eb04;
    bool call_235c8;
    bool call_2dff0;
    bool call_6666c;        /* sub_0806666C guard-seq reset */
    bool call_ef6c;         /* sub_0800EF6C (param0-24 create) */
    bool fade_out;          /* sub_0803D270 + sub_0803CED4 pair */
    int song;               /* PlaySong argument, -1 none */
    /* npc_talk create-helper reports. */
    bool npc_deleted;       /* helper requested EntityDelete */
    bool npc_marked;        /* +0x59 |= 8 mark from a spawn gate */
    int npc_ret;            /* spawn-gate return value, -1 none */
    unsigned npc_anim_steps;/* sub_0803F17C count from helpers */
    /* Dispatch metadata. */
    int return_code;        /* the ROM's r0 result */
    bool unported;          /* the param0's native routine is not
                             * ported — the shared shell ran but
                             * the variant body did not; the
                             * entity is kept inert. */
    bool handled_externally;/* param0 3: the bespoke opening-scene
                             * path owns create+update; nothing
                             * ran here. */
    bool deleted;           /* entity delete requested this call */
} AosObject20Out;

/* Object20Create dispatch. `param0` is the entity +0x30 s16.
 * Returns true when the entity survives (no EntityDelete). */
bool aos_object20_create(AosObject20 *o, int16_t param0,
                         const AosObject20In *in,
                         AosObject20Out *out);

/* Object20Update dispatch on the stored param0. Returns the
 * ROM's r0 result (0 for void variants). */
int aos_object20_update(AosObject20 *o, const AosObject20In *in,
                        AosObject20Out *out);

/* sub_08067A04's early-out gate on the room-dialogue poll
 * (asm/code_08060B98.s _08067A04): while gEwramData +0x417 != 0
 * and (+0x414 & 0xF) <= 4 the native routine returns 0 before
 * touching the display state machine, so the entity's
 * text_done read reports not-done that frame. The runtime keeps
 * the byte pair in session channels and evaluates this predicate
 * when forwarding a window_poll; the routine's own VRAM/register/
 * DMA work stays in the dialogue bridge — only the gate is
 * modelled here. */
bool aos_object20_text_poll_gate(uint8_t byte417, uint8_t byte414);

#endif
