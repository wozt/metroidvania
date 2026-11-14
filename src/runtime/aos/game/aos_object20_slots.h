/* SPDX-License-Identifier: GPL-3.0-only */
/* Object20 "slot" create/update table entries from the SPECIAL
 * id-32 dispatch tables at 0x08526658 (create) / 0x085266D8
 * (update) — the param0 values not owned by the opening, dirs,
 * story, shop or npc_talk modules:
 *
 *   param0  create (0x08526658)   update (0x085266D8)
 *   0       sub_0805BF24           sub_0805BCB4
 *   1       sub_0805BF48 (bx lr)   sub_0805BE64
 *   2       sub_0805CB6C (bx lr)   sub_0805CB70 (bx lr)
 *   11      sub_08060B98           sub_08060C20
 *   12      sub_08060D48           sub_08060DC4
 *   13      sub_08061874           sub_08061890
 *   14      sub_080618C8           sub_080618E4
 *   15      sub_08060F24           sub_08060F98
 *   16      sub_08061668           sub_080616F8
 *   17      sub_08061924 (bx lr)   sub_08061928
 *   18      sub_08061978           sub_08061994
 *
 * The shared Object20Create/Object20Update prologue
 * (0x0805B42C/0x0805B510) is folded into the entry points, the
 * same convention aos_object20_story uses. The create dispatch
 * follows the native 0x08526658 table indices; callers pass the
 * entity param0 unchanged.
 *
 * Integration status: consumed by the unified Object20
 * dispatcher (aos_object20) once the parent wires the
 * AOS_OBJECT20_FAMILY_SLOTS routing — this module is standalone
 * until then. Engine effects are reported through the Out
 * struct; approximations are documented per-routine.
 * classified partial in data/native_parity/annotations.tsv (no
 * emulator comparison yet). */

#ifndef AOS_OBJECT20_SLOTS_H
#define AOS_OBJECT20_SLOTS_H

#include <stdbool.h>
#include <stdint.h>

/* Per-entity state, mirroring the native +0x3E byte, the
 * +0xA/+0xB/+0xC/+0xD state bytes, the +0x42/+0x46 screen
 * words, the +0x40/+0x44 position words, the +0x48/+0x50
 * velocity/scroll words, +0x58/+0x59/+0x5C/+0x6C/+0x6D/+0x6E
 * flag and animation bytes used by this entry set. */
typedef struct {
    uint8_t state;      /* +0xA */
    uint8_t sub_b;      /* +0xB */
    uint8_t sub_c;      /* +0xC */
    uint8_t timer_d;    /* +0xD */
    uint8_t field3e;    /* +0x3E (param0 16 create |= 4) */
    uint8_t field6c;    /* +0x6C (param0 0 update case 3 |= 2
                         * when +0x6E == 1) */
    uint8_t field6e;    /* +0x6E (param0 0 update case 3 pick) */
    uint8_t flags58;    /* +0x58 */
    uint8_t flags59;    /* +0x59 */
    uint8_t anim5c;     /* +0x5C */
    uint8_t anim6d;     /* +0x6D */
    int16_t scr42;      /* +0x42 screen X */
    int16_t scr46;      /* +0x46 screen Y */
    int32_t pos40;      /* +0x40 fine X */
    int32_t pos44;      /* +0x44 fine Y */
    int32_t vel48;      /* +0x48 */
    int32_t vel4c;      /* +0x4C (param0 15 update case 9:
                         * +0x44 += +0x4C) */
    int32_t vel50;      /* +0x50 (param0 0 update case 2/3) */
    uint8_t field11;    /* +0x11 (param0 15 update case 0:
                         * record-table index, stride 0x84) */
    uint8_t field14;    /* +0x14 (param0 15 update cases 5/6:
                         * 0x081188E4 table index) */
    bool deleted;       /* EntityDelete requested */
} AosObject20Slots;

/* Injected native reads: the results of the probes the ROM
 * would take from gEwramData, the player entity, the room
 * origin words and the record-table lookups. */
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
    bool text_done;         /* sub_08067A04() != 0 */
    /* sub_0805B754(e, idx) keep result over the 0x081186DC
     * record table. The arg differs per create: idx 4 for
     * param0 11, idx 1 for param0 12/15/16 — one bool per call
     * is enough because each create runs exactly one variant.
     * On success the native helper also writes +0x5C = 0x11
     * and +0x59 |= 2; on failure it EntityDeletes and returns
     * 0 (the variant may call EntityDelete a second time —
     * reported once through `deleted`). */
    bool keep_5b754;
    uint16_t a094_6;        /* gEwramData + 0xA094 + 6 room
                             * origin X */
    uint16_t a094_a;        /* gEwramData + 0xA094 + 0xA room
                             * origin Y */
    int32_t player_scr42;   /* player entity +0x42 s16 */
    int32_t player_pos40;   /* player entity +0x40 */
    int32_t player_w4c;     /* player entity +0x4C s32 (the
                             * param0-16 update clamps it at 0) */
    uint8_t player_flags58; /* player entity +0x58 */
    int ret_roomx;          /* GetEntityRoomXPositionInteger(
                             * player) — param0 1/11 updates */
    bool probe_68ad4;       /* sub_08068AD4 rect probe result —
                             * param0 16 update (the ROM call is
                             * sub_08068AD4(e, +0x42-0x20,
                             * +0x46-0x20, 0x40, 0x40)) */
    int16_t s16_4be;        /* gEwramData + 0x4BE s16 — param0 16
                             * update pause word */
    /* param0 15 update (sub_08060F98) probes — native callees
     * with no ported equivalent are injected, never substituted. */
    uint32_t probe_212c8;   /* sub_080212C8() result — case 0
                             * tests & 0x1E (input busy) */
    int probe_61824;        /* sub_08061824(0xAA,0x3C,0x21)
                             * screen-rect result — case 0 tests
                             * the low byte, case 2 tests == 1 */
    int32_t tbl14;          /* word at 0x081188E4 + 4*+0x14 — the
                             * ten-word table the ROM copies to the
                             * stack; cases 5/6 index it with the
                             * entity +0x14 byte (valid 0..9) */
    int32_t rec_524;        /* s32 at gEwramData + (+0x11)*0x84
                             * + 0x524 — case 0 record X, compared
                             * against the player +0x40 */
    uint8_t rec_53c;        /* byte at gEwramData + (+0x11)*0x84
                             * + 0x53C — case 0 record flags */
    uint8_t player_byte5a;  /* player entity +0x5A — cases 4-7,
                             * 9 and the 0x80 exit rewrite it */
    uint32_t probe_6e140;   /* sub_0806E140(e,1,0x200000,0x10000,1)
                             * result — case 9 tests & 4 */
} AosObject20SlotsIn;

/* Outputs reset at the top of every call; each names a native
 * call or memory effect the ROM would perform. */
typedef struct {
    int window_text;        /* sub_08067934 argument, -1 none */
    bool window_poll;       /* sub_08067A04 called */
    bool window_close;      /* sub_08067898 called */
    int b878;               /* sub_0805B878 argument, -1 none */
    int jingle;             /* sub_08012048 argument, -1 none */
    int release_arg;        /* sub_0805B66C argument, -1 none */
    int rec_5b754;          /* sub_0805B754 record-table index,
                             * -1 none (4 for create 11, 1 for
                             * creates 12/15/16) */
    unsigned roomx_calls;   /* GetEntityRoomXPositionInteger
                             * invocations this call */
    uint32_t set_42c;       /* bits OR-ed into gEwramData + 0x42C */
    uint32_t clear_42c;     /* bits AND-NOT-ed out of + 0x42C */
    bool unk2cb0_feff;      /* gUnk_03002CB0 &= 0xFEFF —
                             * param0 1 update state 0x80 */
    bool unk2cb0_or100;     /* gUnk_03002CB0 |= 0x100 —
                             * param0 1 update state 0x81 */
    bool write_417;         /* gEwramData + 0x417 byte write (the
                             * +0x60 + 0x3B7 alias the param0-11/12
                             * updates use lands on the same
                             * address) */
    uint8_t write_417_val;
    bool write_414;         /* gEwramData + 0x414 byte write */
    uint8_t write_414_val;
    bool unk7864_or2;       /* gEwramData + 0x7864 |= 2 */
    bool call_67804;        /* sub_08067804(-3) — +0xA negative */
    int setplayerinput;     /* SetPlayerInput enable arg, -1 none */
    int setplayerinput_arg; /* SetPlayerInput second arg */
    int c5a8;               /* sub_0800C5A8 argument, -1 none */
    bool call_0eb04;
    bool call_235c8;
    bool call_2dff0;
    bool call_6666c;        /* sub_0806666C guard-seq reset */
    /* sub_0803F2C8 calls on the entity itself: up to 4 per
     * call. */
    unsigned anim_count;
    int anim_a[4], anim_b[4], anim_c[4];
    unsigned anim_step_count;   /* sub_0803F17C calls */
    /* Player-entity writes (gEwramData + 0x13110). */
    bool player_scr42_set;
    int32_t player_scr42_val;
    bool player58_or_40;        /* player +0x58 |= 0x40 —
                                 * param0 12 update +0xB==1 gate */
    bool player58_andnot_41;    /* player +0x58 &= ~0x41 —
                                 * param0 0 update state 0 */
    bool player_w48_zero;       /* player +0x48 word = 0 —
                                 * param0 16 update probe chain */
    bool player_w50_zero;       /* player +0x50 word = 0 */
    bool player_w4c_min0;       /* player +0x4C clamped to >= 0 */
    /* param0 15 update (sub_08060F98) reports. write_417_val is
     * the LAST +0x417 store when a call writes it twice (case 9
     * can). */
    unsigned probe61824_calls;  /* sub_08061824 invocations */
    unsigned playsong_count;    /* PlaySong args, call order */
    int32_t playsong[4];
    unsigned fade_count;        /* sub_0803D18C/sub_0803CDF0
                                 * calls, call order */
    int fade_fn[8];             /* 0 = sub_0803D18C, 1 = sub_0803CDF0 */
    int fade_args[8][6];        /* the six native args each */
    int sfx_13eec;              /* sub_08013EEC arg, -1 none
                                 * (case 3 arms 0x16C) */
    int call_13cf0;             /* sub_08013CF0 arg, -1 none
                                 * (case 7 +0xC==2 arms 0x21) */
    int call_00ea8;             /* sub_08000EA8 arg, -1 none
                                 * (always 0x40 here) */
    bool call_1d028;            /* sub_0801D028(0x52,0,0,0) —
                                 * cases 4/5/6 tail */
    bool call_3ced4;            /* sub_0803CED4 */
    bool call_3d270;            /* sub_0803D270 (native order is
                                 * CED4->D270 in the 0x80 exit,
                                 * D270->CED4 in case 7 +0xC==2) */
    bool rec_53c_write;         /* case-0 record +0x53C byte store */
    uint8_t rec_53c_val;
    unsigned player5a_count;    /* player +0x5A stores, in order */
    uint8_t player5a_vals[4];
    bool write_90;              /* gEwramData + 0x90 u16 store —
                                 * the 0x80 exit writes the flag
                                 * word there */
    uint16_t write_90_val;
    int return_code;            /* the ROM's r0/r6/r7/r8 result */
    bool unported;              /* the param0's native routine is
                                 * not a slots-table entry — the
                                 * shared shell ran but no variant
                                 * body did; the entity is kept
                                 * inert. */
    bool deleted;               /* entity delete requested this
                                 * call */
} AosObject20SlotsOut;

/* Shared Object20Create (0x0805B42C): the +0x1327A/+0x42C/
 * +0x8E/+0x13266 gates, the variant create dispatch and the
 * common tail (+0x42C |= 0x200, engine init calls, +0x41C
 * self-link, conditional SetPlayerInput(1,0), sub_0806666C).
 * Returns true when the entity survives (no EntityDelete).
 * `param0` is the native 0x08526658 table index (0,1,2,11-18). */
bool aos_object20_slots_create(AosObject20Slots *s,
                               uint8_t param0,
                               const AosObject20SlotsIn *in,
                               AosObject20SlotsOut *out);

/* Shared Object20Update (0x0805B510): the +0x7864/+0x414
 * prologue, the variant update dispatch and the
 * SetPlayerInput/sub_0800C5A8 tail. Returns the ROM result —
 * param0 15 returns the per-call flag word ([sp,#0x30]: 0,
 * 0x10/0x20 from case 2, |0x80 from cases 7/8). */
int aos_object20_slots_update(AosObject20Slots *s,
                              uint8_t param0,
                              const AosObject20SlotsIn *in,
                              AosObject20SlotsOut *out);

#endif
