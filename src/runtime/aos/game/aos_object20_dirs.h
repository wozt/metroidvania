/* SPDX-License-Identifier: GPL-3.0-only */
/* Object20 door/gate variants from the SPECIAL id-32 dispatch
 * tables at 0x08526658 (create) / 0x085266D8 (update):
 * param0 4  create sub_0805F120 / update sub_0805F268
 * param0 5  create sub_0805F4B9 / update sub_0805F541
 * param0 6  create sub_0805F72D / update sub_0805F7C9
 * param0 7  create sub_0805FA4D / update sub_0805FB3D
 * param0 8  create sub_0805FD65 / update sub_0805FDF1
 * plus the adjacent +0x18 sequencer sub_0805F1CC that the
 * param0-7 update arms with seq18 = 4. The shared
 * Object20Create/Object20Update prologue (0x0805B42C/0x0805B510)
 * is folded into the entry points. */
/* Integration status: faithful native-routine port kept as a
 * parity reference. It is unit-tested but NOT linked into
 * fusion_aria_runtime; classified partial in
 * data/native_parity/annotations.tsv until a real gameplay
 * path consumes it. */

#ifndef AOS_OBJECT20_DIRS_H
#define AOS_OBJECT20_DIRS_H

#include <stdbool.h>
#include <stdint.h>

/* Per-entity state, mirroring the native +0xA/+0xB/+0xC/+0xD
 * state bytes, the +0x12 flag byte, the +0x14 animation-table
 * index, the +0x18/+0x19 sequencer pair, the +0x40/+0x44/+0x48/
 * +0x4C position/velocity words, the +0x42/+0x46 screen words
 * and the +0x58/+0x59/+0x6D flag bytes. */
typedef struct {
    uint8_t state;      /* +0xA */
    uint8_t sub_b;      /* +0xB */
    uint8_t sub_c;      /* +0xC */
    uint8_t timer_d;    /* +0xD */
    uint8_t flags12;    /* +0x12 (param0 7 create |= 0x40) */
    uint8_t index14;    /* +0x14 (param0 4 anim-table index) */
    uint8_t seq18;      /* +0x18 (param0 7 sets 4; sub_0805F1CC) */
    uint8_t timer19;    /* +0x19 (sub_0805F1CC countdown) */
    uint8_t anim6d;     /* +0x6D */
    uint8_t flags58;    /* +0x58 */
    uint8_t flags59;    /* +0x59 */
    int32_t pos40;      /* +0x40 fine X */
    int32_t pos44;      /* +0x44 fine Y */
    int32_t vel48;      /* +0x48 */
    int32_t vel4c;      /* +0x4C */
    int16_t scr42;      /* +0x42 screen X */
    int16_t scr46;      /* +0x46 screen Y */
    bool deleted;       /* EntityDelete requested */
} AosObject20Dirs;

/* Injected native reads: the results of the probes the ROM
 * would take from gEwramData, the player entity, the
 * 0xA094 origin words and the flag bytes. */
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
    /* Variant create probes sub_08012078(0x21..0x25). */
    int flag12078_21;       /* param0 4 create */
    int flag12078_22;       /* param0 5 create */
    int flag12078_23;       /* param0 6 create */
    int flag12078_24;       /* param0 7 create */
    int flag12078_25;       /* param0 8 create */
    /* gEwramData + 0xA094 block: +4 scroll word (param0 6
     * update, signed), +6/+0xA room origin words (creates use
     * ldrh, the param0 6/7 updates ldrsh — cast at use). */
    int32_t a094_4;
    uint16_t a094_6;
    uint16_t a094_a;
    /* Player entity (+0x13110) fields. scr42 is read and
     * written through the Out report; flags58 bit 6 feeds the
     * param0 5 facing mirror. */
    int32_t player_scr42;   /* player +0x42 s16 */
    uint8_t player_flags58; /* player +0x58 */
    /* Injected results. */
    bool keep_5b754;        /* sub_0805B754(e, arg) keep result */
    bool text_done;         /* sub_08067A04() != 0 */
    int probe_6e140;        /* sub_0806E140(...) return — bit 0x4
                             * gates the param0 6/7 y-moves */
    /* ROM byte table at 0x0852682C indexed by +0x14 in the
     * param0-4 update (sub_0805F39C). Not committed; the caller
     * supplies the dumped bytes. NULL reads as 0xFF (the ROM's
     * table-end sentinel), which resets +0x14 to 0. */
    const uint8_t *table_852682c;
} AosObject20DirsIn;

/* Outputs reset at the top of every call; each names a native
 * call or memory effect the ROM would perform. Repeated writes
 * to the same gEwramData byte keep the last value, matching
 * the final native state. */
typedef struct {
    int window_text;        /* sub_08067934 argument, -1 none */
    bool window_poll;       /* sub_08067A04 called */
    int song;               /* PlaySong argument, -1 none */
    int b878;               /* sub_0805B878 argument, -1 none */
    int cf0;                /* sub_08013CF0 argument, -1 none */
    int release_arg;        /* sub_0805B66C argument, -1 none */
    int jingle;             /* sub_08012048 argument, -1 none */
    int ed24_a;             /* sub_0800ED24 arg0, -1 none */
    int32_t ed24_b;         /* sub_0800ED24 arg1 (arg2 is always 0) */
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
    bool write_a094_4;      /* gEwramData + 0xA094 + 4 word write */
    int32_t write_a094_4_val;
    bool unk7864_or2;       /* gEwramData + 0x7864 |= 2 */
    /* Engine-side helpers reported, not modelled. */
    bool call_0eb04;
    bool call_235c8;
    bool call_2dff0;
    bool call_67804;        /* sub_08067804(-3) — +0xA negative */
    bool call_13960;        /* sub_08013960(gEwramData + 0x60) */
    bool call_6666c;        /* sub_0806666C guard-seq reset */
    bool call_6af98;        /* sub_0806AF98(e) — param0 7 create */
    bool call_6e140;        /* sub_0806E140 collision probe ran */
    int keep_arg;           /* sub_0805B754 argument, -1 none */
    bool dma_setup;         /* 0x040000D4/0x06004000/0x85000400
                             * chain — approximated: reported as
                             * one flag */
    bool dma_setup_786c;    /* the +0x786C / 0x85000280 variant */
    /* sub_0803F2C8 calls on the entity itself: up to 4 per
     * call (arg1 is the injected table byte for the param0-4
     * +0xB==1,+0xC==1 pick). */
    unsigned anim_count;
    int anim_a[4], anim_b[4], anim_c[4];
    unsigned anim_step_count;   /* sub_0803F17C calls */
    /* Player-entity writes (gEwramData + 0x13110). */
    bool player58_or_40;        /* player +0x58 |= 0x40 */
    bool player58_andnot_41;    /* player +0x58 &= ~0x41 */
    bool player_scr42_set;      /* player +0x42 s16 write */
    int32_t player_scr42_val;
    bool player_vel_cleared;    /* player +0x48/+0x4C = 0 (p0 7 st0) */
    int return_code;            /* the ROM's r0/r2/r7/r8 result */
    bool unported;              /* param0 outside 4..8 — the real
                                 * table entries belong to other
                                 * variant modules; nothing ran */
} AosObject20DirsOut;

/* Shared Object20Create (0x0805B42C): the +0x1327A/+0x42C/
 * +0x8E/+0x13266 gates, the variant create dispatch and the
 * common tail (+0x42C |= 0x200, engine init calls, +0x41C
 * self-link, conditional SetPlayerInput(1,0), sub_0806666C).
 * Returns true when the entity survives (no EntityDelete). */
bool aos_object20_dirs_create(AosObject20Dirs *s, uint8_t param0,
                              const AosObject20DirsIn *in,
                              AosObject20DirsOut *out);

/* Shared Object20Update (0x0805B510): the +0x7864/+0x414
 * prologue, the variant update dispatch and the
 * SetPlayerInput/sub_0800C5A8 tail. Returns the ROM result. */
int aos_object20_dirs_update(AosObject20Dirs *s, uint8_t param0,
                             const AosObject20DirsIn *in,
                             AosObject20DirsOut *out);

/* sub_0805F1CC: the +0x18 sequencer sitting between the
 * param0-4 create and update in the ROM — dispatches on the
 * entity +0x18 byte (0/3 PlaySong(0x1010) + 0xA0 timer, 1/4
 * countdown to sub_08013CF0(0x16)/sub_08013960, 5 returns 1).
 * The param0-7 update writes seq18 = 4 when its escape
 * sequence finishes. Returns the ROM's r5 result. */
int aos_object20_dirs_seq18_update(AosObject20Dirs *s,
                                   const AosObject20DirsIn *in,
                                   AosObject20DirsOut *out);

#endif
