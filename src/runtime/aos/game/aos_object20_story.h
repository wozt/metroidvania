/* SPDX-License-Identifier: GPL-3.0-only */
/* Object20 story/cinematic variants from the SPECIAL id-32
 * dispatch tables at 0x08526658 (create) / 0x085266D8 (update):
 * param0 9  create sub_0805FFDC / update sub_08060248
 * param0 10 create sub_080606C8 / update sub_080607E4
 * param0 20 create sub_08062040 (create only; its update is
 *           the shop buy routine in aos_shop)
 * param0 21 create sub_08062068 / update sub_08062074
 * param0 22 create sub_08062244 / update sub_080620E8
 * param0 23 create sub_08062260 / update sub_080623F0
 * param0 24            / update sub_08062664
 *           (create sub_080657B4 lives in aos_npc_talk)
 * param0 19 create sub_08061A00 (create only; the update is
 *           the shop sell routine in aos_shop — the dispatcher
 *           mirrors the shared state bytes into v.shop)
 * param0 29 create sub_0805BF4C / update sub_0805C08C
 * param0 30 create sub_0805B888 / update sub_0805B8F4
 * param0 31 create sub_0805C49C / update sub_0805C628
 * These are the Chaotic Realm / Floating Garden story beats and
 * the area_11_room_023 boss-room sequences. The shared
 * Object20Create/Object20Update prologue (0x0805B42C/0x0805B510)
 * is folded into the entry points. The create dispatch follows
 * the native 0x08526658 table indices; callers pass the entity
 * param0 unchanged. */
/* Integration status: consumed by the unified Object20
 * dispatcher (aos_object20), which is wired into the id-32
 * SPECIAL branch of update_entities in aria_runtime.c. Engine
 * effects are reported through the Out struct; approximations
 * are documented per-routine. classified partial in
 * data/native_parity/annotations.tsv (no emulator comparison
 * yet). */

#ifndef AOS_OBJECT20_STORY_H
#define AOS_OBJECT20_STORY_H

#include <stdbool.h>
#include <stdint.h>

/* Per-entity state, mirroring the native +0xA/+0xB/+0xC/+0xD
 * state bytes, the +0x13 pick, the +0x14/+0x18 entity links,
 * the +0x1C/+0x20 scroll pair, the +0x40/+0x44/+0x48/+0x4C
 * position/velocity words, the +0x42/+0x46 screen words,
 * +0x5C/+0x65/+0x6D animation selectors and the +0x58/+0x59
 * flag bytes. */
typedef struct {
    uint8_t state;      /* +0xA */
    uint8_t sub_b;      /* +0xB */
    uint8_t sub_c;      /* +0xC */
    uint8_t timer_d;    /* +0xD */
    uint8_t field13;    /* +0x13 (param0 24 window pick) */
    uint8_t field65;    /* +0x65 (param0 9 text-done helper) */
    uint8_t anim6d;     /* +0x6D */
    uint8_t flags58;    /* +0x58 */
    uint8_t flags59;    /* +0x59 */
    uint8_t anim5c;     /* +0x5C */
    uint8_t field6e;    /* +0x6E (param0 31 upd st2 song pick) */
    uint16_t field18;   /* +0x18 hword scroll counter (param0 30;
                         * param0 9 uses +0x18 as the link pointer
                         * through link18 instead) */
    int32_t pos40;      /* +0x40 fine X */
    int32_t pos44;      /* +0x44 fine Y */
    int32_t vel48;      /* +0x48 */
    int32_t vel4c;      /* +0x4C */
    int32_t scr1c;      /* +0x1C (param0 9 scroll pair) */
    int32_t scr20;      /* +0x20 */
    int16_t scr42;      /* +0x42 screen X */
    int16_t scr46;      /* +0x46 screen Y */
    /* +0x14 child entity (param0 9/24/29): nonzero = spawned. */
    bool child14;
    uint8_t child14_flags58;    /* child +0x58 */
    uint8_t child14_flags59;    /* child +0x59 */
    uint8_t child14_anim5c;     /* child +0x5C */
    uint8_t child14_anim6d;     /* child +0x6D */
    /* +0x18 linked follower (param0 9): nonzero = spawned. */
    bool link18;
    uint8_t link18_state_a;     /* link +0xA */
    uint8_t link18_flags58;     /* link +0x58 */
    uint8_t link18_anim6d;      /* link +0x6D */
    int32_t link18_pos40;       /* link +0x40 */
    int32_t link18_vel48;       /* link +0x48 */
    int16_t link18_scr42;       /* link +0x42 */
    int16_t link18_scr46;       /* link +0x46 */
    /* param0 29/31's +0x14 child runs sub_0805C39C; its own
     * +0xA/+0xB/+0xC/+0xD/+0x18/+0x1A/+0x59/+0x65 live here. */
    struct {
        uint8_t state;      /* child +0xA */
        uint8_t sub_b;      /* child +0xB (param0 31 upd st4) */
        uint8_t ctr_c;      /* child +0xC */
        uint8_t timer_d;    /* child +0xD */
        uint16_t field18;   /* child +0x18 */
        int16_t field1a;    /* child +0x1A table index */
        uint8_t flags59;    /* child +0x59 */
        uint8_t anim65;     /* child +0x65 */
    } child29;
    bool deleted;       /* EntityDelete requested */
} AosObject20Story;

/* Injected native reads: the results of the probes the ROM
 * would take from gEwramData, the player entity, the room
 * origin words and the flag bytes. */
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
    /* Variant probes. */
    int flag12078_26;       /* sub_08012078(0x26) — param0 9 create */
    int flag12078_27;       /* sub_08012078(0x27) — param0 10 create */
    int flag12078_29;       /* sub_08012078(0x29) — param0 19 create */
    int flag12078_2b;       /* sub_08012078(0x2B) — param0 22 upd st0 */
    uint16_t flags37e;      /* gEwramData + 0x37E — param0 10 create,
                             * param0 29 upd st0 */
    uint16_t unk2cb0;       /* gUnk_03002CB0 — param0 23/24/29 */
    int32_t player_scr42;   /* player entity +0x42 s16 */
    int32_t player_pos40;   /* player entity +0x40 */
    int32_t player_pos44;   /* player entity +0x44 */
    uint8_t player_anim5c;  /* player entity +0x5C */
    uint8_t player_flags58; /* player entity +0x58 */
    uint16_t a094_6;        /* gEwramData + 0xA094 + 6 room origin X */
    uint16_t a094_a;        /* gEwramData + 0xA094 + 0xA room origin Y */
    bool keep_5b754;        /* sub_0805B754(e, arg) keep result */
    bool spawn14_ok;        /* sub_0805B5E8 -> +0x14 non-NULL */
    bool spawn18_ok;        /* sub_0805B5E8 -> +0x18 non-NULL */
    int rng_3e654;          /* sub_0803E654(1) result — param0 9 */
    bool fade_done;         /* sub_0803D408() != 0 — param0 23/24/29 */
    int32_t rng_32df0;      /* sub_08032DF0() — param0 24 upd st1 */
    bool text_done;         /* sub_08067A04() != 0 */
    /* param0 29 child29 sub_0805C328 support (0x085267B8
     * records): `c328_ok` is its return value. */
    bool c328_ok;
    /* param0 19 create sub_08061FDC support: the raw
     * sub_0806E0D0 result — nonzero low byte keeps the entity
     * (+0x5C = 0x11), zero deletes it. */
    int32_t ret_6e0d0;
    /* param0 30 update probes: sub_080628AC's return (nonzero
     * completes the whiteout hand-off), sub_0803D06C's return
     * (nonzero skips the gated block), sub_080009E4's return for
     * the +0x18 scroll word and the two RandomNumberGenerator
     * draws feeding the sub_08045B44 particle position. */
    int ret_628ac;
    int ret_3d06c;
    int32_t ret_9e4;
    uint32_t rng_p1, rng_p2;
    /* param0 31 update sub_0806E140 floor probe result (bit 2
     * set skips the +0x44 += +0x4C slide). */
    uint32_t ret_6e140;
} AosObject20StoryIn;

/* Outputs reset at the top of every call; each names a native
 * call or memory effect the ROM would perform. */
typedef struct {
    int window_text;        /* sub_08067934 argument, -1 none */
    bool window_poll;       /* sub_08067A04 called */
    bool window_close;      /* sub_08067898 called */
    int song;               /* PlaySong argument, -1 none */
    int b878;               /* sub_0805B878 argument, -1 none */
    int cf0;                /* sub_08013CF0 argument, -1 none */
    int d60;                /* sub_08013D60 argument, -1 none */
    int e18;                /* sub_08013E18 argument, -1 none */
    int eec;                /* sub_08013EEC argument, -1 none */
    int jingle;             /* sub_08012048 argument, -1 none */
    int ev_121e0;           /* sub_080121E0 argument, -1 none */
    int ed24_a;             /* sub_0800ED24 arg0, -1 none */
    int32_t ed24_b;         /* sub_0800ED24 arg1 */
    int c5a8;               /* sub_0800C5A8 argument, -1 none */
    int setplayerinput;     /* SetPlayerInput enable arg, -1 none */
    int setplayerinput_arg; /* SetPlayerInput second arg */
    uint32_t set_42c;       /* bits OR-ed into gEwramData + 0x42C */
    uint32_t clear_42c;     /* bits AND-NOT-ed out of + 0x42C */
    bool write_417;         /* gEwramData + 0x417 byte write — the
                             * +0x60 + 0x3B7 byte aliases this
                             * address, so the cinematic "3B7"
                             * writes land here too. */
    uint8_t write_417_val;
    bool write_427;         /* gEwramData + 0x427 byte write */
    uint8_t write_427_val;
    bool write_414;         /* gEwramData + 0x414 byte write */
    uint8_t write_414_val;
    bool write_254cc;       /* gEwramData + 0x254CC word write */
    uint16_t write_254cc_val;
    bool write_64_0a;       /* gEwramData + 0x64 = 0x0A */
    bool or_60_word;        /* gEwramData + 0x60 |= 1 (word) */
    bool flags37e_clear1;   /* gEwramData + 0x37E &= ~1 */
    bool unk7864_or2;       /* gEwramData + 0x7864 |= 2 */
    bool unk414_write;      /* gEwramData + 0x414 rewrite */
    uint8_t unk414_val;
    bool unk2cb0_feff;      /* gUnk_03002CB0 &= 0xFEFF */
    bool unk2cb0_or100;     /* gUnk_03002CB0 |= 0x100 */
    bool unk2cb0_f1ff;      /* gUnk_03002CB0 &= 0xF1FF —
                             * param0 30 update case 2 */
    bool write_a074;        /* gEwramData + 0xA074 byte &= ~3 —
                             * param0 30 update case 4 */
    bool disp48_zero;       /* gDisplayRegisters + 0x48 hword = 0 */
    bool call_ef6c;         /* sub_0800EF6C — param0 30 create */
    bool call_628ac;        /* sub_080628AC invoked — param0 30
                             * update case 1 (the whiteout
                             * sequence itself is unported; its
                             * result arrives via in->ret_628ac) */
    bool call_9e4;          /* sub_080009E4(scroll) invoked */
    bool call_3c7b4;        /* sub_0803C7B4(0x082099FC,5,1,0xE) */
    bool call_11f44;        /* sub_08011F44(0x08522C54,0,0,0) */
    bool call_11ad0;        /* sub_08011AD0(gEwramData + 0x60) */
    bool call_45b44;        /* sub_08045B44 particle spawn */
    int32_t p45b44_x;       /* sub_08045B44 arg0 (fine X) */
    int32_t p45b44_y;       /* sub_08045B44 arg1 (fine Y) */
    /* Engine-side helpers reported, not modelled. */
    bool call_0eb04;
    bool call_235c8;
    bool call_2dff0;
    bool call_67804;        /* sub_08067804(-3) — +0xA negative */
    bool fade_out;          /* sub_0803D270 + sub_0803CED4 pair */
    bool fade_3d18c;        /* sub_0803D18C call */
    bool fade_3cdf0;        /* sub_0803CDF0 call */
    bool call_13788;
    int call_127f0_off;     /* sub_080127F0 offset arg, -1 none */
    int call_127f0_val;
    bool call_137fc;
    bool call_13960;        /* sub_08013960(gEwramData + 0x60) */
    bool call_d154;
    bool call_d1f0;
    int call_1d028_a;       /* sub_0801D028 arg0, -1 none */
    int32_t call_1d028_r3;  /* sub_0801D028 4th arg (r3) — only
                             * meaningful when call_1d028_a is
                             * set; the param0-30 scroll tail is
                             * the only caller passing nonzero */
    int call_3d004;         /* sub_0803D004 argument, -1 none */
    int call_3d3a0;         /* sub_0803D3A0 argument, -1 none */
    int f6c;                /* sub_08013F6C argument, -1 none */
    int release_arg;        /* sub_0805B66C argument, -1 none */
    bool call_6666c;        /* sub_0806666C guard-seq reset */
    bool dma_setup;         /* 0x040000D4/0x06004000/... chain —
                             * approximated: reported as one flag */
    bool dma_setup_786c;    /* the +0x786C / 0x85000280 variant */
    /* sub_0803F2C8 calls on the entity itself: up to 4 per
     * update. */
    unsigned anim_count;
    int anim_a[4], anim_b[4], anim_c[4];
    /* sub_0803F2C8 calls on the +0x14/+0x18 child entities —
     * approximated: reported separately since the target is
     * not the entity itself. */
    unsigned child_anim_count;
    int child_anim_a[4], child_anim_b[4], child_anim_c[4];
    unsigned anim_step_count;   /* sub_0803F17C calls */
    /* sub_0805B5E8 spawn requests: up to 4 per call. */
    unsigned spawn_count;
    int spawn_type[4];
    /* sub_0803E058 scroll/fade call (param0 9). */
    bool call_3e058;
    int32_t e058_a, e058_b;
    int32_t e058_flags;     /* stack arg word (flag26 result) */
    int e058_arg;
    int e058_low5;          /* child +0x58 & 0x1F argument */
    bool call_3e654;        /* sub_0803E654(1) ran */
    /* Player-entity writes (gEwramData + 0x13110). */
    bool player58_or_40;
    bool player58_or_20;
    bool player58_andnot_41;
    bool player58_andnot_21; /* player +0x58 &= ~0x21 —
                              * param0 30 update case 1 */
    bool player_scr42_set;
    int32_t player_scr42_val;
    bool player_pos40_set;
    int32_t player_pos40_val;
    bool player_pos44_set;
    int32_t player_pos44_val;
    bool player_w48_zero;   /* player +0x48 word = 0 */
    bool player_w4c_zero;   /* player +0x4C word = 0 */
    bool player_w4a_zero;   /* player +0x4A hword = 0 */
    bool player_w4e_zero;   /* player +0x4E hword = 0 */
    bool player_anim5c_inc; /* player +0x5C read for +1 copies */
    /* Own-entity writes beyond the state struct. */
    bool ent65_set;         /* +0x65 byte write */
    uint8_t ent65_val;
    int return_code;        /* the ROM's r0/r7 result */
} AosObject20StoryOut;

/* Shared Object20Create (0x0805B42C): the +0x1327A/+0x42C/
 * +0x8E/+0x13266 gates, the variant create dispatch and the
 * common tail (+0x42C |= 0x200, engine init calls, +0x41C
 * self-link, conditional SetPlayerInput(1,0), sub_0806666C).
 * Returns true when the entity survives (no EntityDelete). */
bool aos_object20_story_create(AosObject20Story *s,
                               uint8_t param0,
                               const AosObject20StoryIn *in,
                               AosObject20StoryOut *out);

/* Shared Object20Update (0x0805B510): the +0x7864/+0x414
 * prologue, the variant update dispatch and the
 * SetPlayerInput/sub_0800C5A8 tail. Returns the ROM result. */
int aos_object20_story_update(AosObject20Story *s,
                              uint8_t param0,
                              const AosObject20StoryIn *in,
                              AosObject20StoryOut *out);

/* sub_0805C39C: the param0-29 +0x14 child's own update —
 * states 0/1 wait, state 2 spawns the type-0xB grandchild,
 * state 3 runs the sub_0805C328 record machine. `c328_ok` is
 * the injected sub_0805C328 result. */
void aos_object20_story_child29_update(AosObject20Story *s,
                                       const AosObject20StoryIn *in,
                                       AosObject20StoryOut *out);

/* sub_080601D4: the param0-9 text-done helper — +0x58 |= 0x40,
 * +0x5C = player +0x5C + 1, +0x65 = 0x12, child/link fixups. */
void aos_object20_story_601d4(AosObject20Story *s,
                              uint8_t player_anim5c,
                              AosObject20StoryOut *out);

#endif
