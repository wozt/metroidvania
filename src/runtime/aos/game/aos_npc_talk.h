/* SPDX-License-Identifier: GPL-3.0-only */
/* Object20 generic talk-state callbacks from
 * code_08060B98.s: the + 0x42C bit-1 arm, the sub_08067934
 * window open, the sub_08067A04 text poll and the
 * sub_0805B66C/sub_08067898 exits. */
/* Integration status: faithful native-routine port kept as a
 * parity reference. It is unit-tested but NOT linked into
 * fusion_aria_runtime; classified partial in
 * data/native_parity/annotations.tsv until a real gameplay
 * path consumes it. */

#ifndef AOS_NPC_TALK_H
#define AOS_NPC_TALK_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t state;      /* +0xA */
    uint8_t flags59;    /* +0x59 */
    /* outputs, reset each call */
    bool arm_42c_1;         /* gEwramData + 0x42C bit 0 set */
    int window_text;        /* sub_08067934 argument, -1 none */
    bool window_close;      /* sub_08067898 call */
    bool entity_release;    /* sub_0805B66C(1) call */
} AosNpcTalk;

/* sub_08061928: text 0x17 — state 0 arms and opens, state 1
 * polls the text machine and releases the entity. */
void aos_npc_talk_17_update(AosNpcTalk *t, bool text_done);

/* sub_08061994: text 0x18 — state 0 arms and opens, state 1
 * polls then closes the window (+0x59 bit 3); state 0x80
 * releases the entity directly. */
void aos_npc_talk_18_update(AosNpcTalk *t, bool text_done);

/* sub_08061978: unconditional +0x42C bit-0 arm (the ROM tail
 * returns the modified flag word). */
bool aos_npc_talk_arm_flag(void);


/* sub_08061FDC: sprite-variant gate — sub_0806E0D0 picks the
 * 0x0811892C/0x08252748/0x0820C028 triple; a nonzero variant
 * sets + 0x5C = 0x11 and keeps the entity, else it deletes.
 * Returns the ROM's 1/0. */
int aos_npc_talk_sprite_gate(uint8_t *anim5c, bool *deleted,
                             bool sprite_variant);

/* sub_08062020: loads frames 0x0811892C and releases the
 * entity through sub_0805B66C(1). */
void aos_npc_talk_release_frames(void);

/* sub_08062040: + 0x42C bit-0 arm plus the sub_0803D270 /
 * sub_0803CED4 fade pair. */
void aos_npc_talk_arm_fade(void);

/* sub_08062068: + 0x59 |= 8. */
void aos_npc_talk_mark_done(uint8_t *flags59);

/* sub_08062074: sub_0805B878(0x1020), sound 0x25 through
 * sub_08013EEC and sub_0805B66C(1); returns 0. */
int aos_npc_talk_sound_release(void);

/* sub_08062098: the + 0xC/+ 0xD counter helper — first call
 * writes gEwramData + 0x417 = 1 and stores arg into + 0xD,
 * later calls count + 0xD down and advance + 0xC on wrap;
 * returns whether the step ran. `wrote_417` reports the
 * gEwramData write. */
bool aos_npc_talk_counter_tick(uint8_t *ctr_c, uint8_t *ctr_d,
                               uint8_t arg, bool *wrote_417);

/* sub_080657B4: without gEwramData + 0x42C bit 0x4000 the entity
 * is marked + 0x59 bit 3; otherwise + 0x42C gains bits 0x20 and
 * 0x10000, sub_0800EF6C runs and PlaySong(0x1000) plays. */
void aos_npc_talk_657b4(uint8_t *flags59, uint32_t *flag42c,
                        bool *called_0ef6c, uint16_t *song);

/* sub_08065FE4: reads the entity at gEwramData + 0x13110 —
 * returns 1 when its + 0xA byte is nonzero or |s16 + 0x48| exceeds
 * 0x8000, else the + 0x10 & 0x30010000 mask. */
uint32_t aos_npc_talk_65fe4(uint8_t state_a, int32_t field48,
                            uint32_t field10);

/* sub_080659A4: Object20 follower spawn — the sub_08012078(0x2E)
 * gate marks +0x59 bit 3, the sub_0805B754(e, 3) failure deletes
 * the entity; on success it steps the animation, parks +0x42/+0x46
 * at (0x60 - cam_x, 0xA0 - cam_y) from the gEwramData + 0xA094
 * words, sets +0x58 bit 6 from the player x compare, copies
 * player +0x5C - 1 and arms gEwramData + 0x42C bit 0. */
typedef struct {
    uint8_t flags59;    /* +0x59 in/out */
    uint8_t flags58;    /* +0x58 in/out */
    uint8_t anim5c;     /* +0x5C out */
    int16_t field42;    /* +0x42 out */
    int16_t field46;    /* +0x46 out */
    uint32_t flag42c;   /* gEwramData + 0x42C out (|= 1) */
    bool deleted;       /* EntityDelete path */
    bool anim_stepped;  /* sub_0803F17C ran */
} AosNpcSpawn;

void aos_npc_talk_659a4(AosNpcSpawn *s, bool gate_12078,
                        bool keep_5b754, uint16_t cam_x, uint16_t cam_y,
                        int32_t self_x, int32_t player_x,
                        uint8_t player_anim5c);

/* sub_08065A5C: the guide follower state machine — dispatch on
 * +0xA (0 opens window 0x28 and sets gEwramData + 0x417, 1 polls
 * sub_08067A04 then runs the +0xB/+0xC script, 0x80 closes through
 * sub_08013960/sub_08012048/sub_0805B66C); returns the ROM's r7
 * flag word (0x20 when the +0xC==2 x-clamp branch fires). The
 * gEwramData + 0x60 + 0x3B7 byte and the player entity fields are
 * injected. */
typedef struct {
    uint8_t state_a;    /* +0xA */
    uint8_t ctr_b;      /* +0xB */
    uint8_t ctr_c;      /* +0xC */
    uint8_t timer_d;    /* +0xD */
    uint8_t flags58;   /* +0x58 */
    uint8_t flags6d;    /* +0x6D */
    int32_t x;          /* +0x40 16.16 */
    int32_t vx;         /* +0x48 */
    uint32_t flag_r7;   /* return value */
    /* outputs */
    bool window_open;       /* sub_08067934(0x28) */
    bool wrote_417;         /* gEwramData + 0x417 = 1 */
    bool text_poll;         /* sub_08067A04 ran */
    bool state_80;          /* +0xA became 0x80 */
    bool song_1040;         /* sub_0805B878(0x1040) */
    bool flag_3b7_set;      /* gEwramData + 0x60 + 0x3B7 = 1 */
    bool flag_3b7_clear;    /* gEwramData + 0x60 + 0x3B7 = 0 */
    bool player_x_clamped;  /* player +0x42 forced to 0xA0 */
    bool player_facing_set; /* player +0x58 bit 6 updates */
    bool release;           /* sub_0805B66C(1) on state 0x80 */
} AosNpcGuide;

uint32_t aos_npc_talk_65a5c(AosNpcGuide *g, bool text_done,
                            int16_t player_x_px, int32_t player_x,
                            uint8_t player_flags58, bool *player_bit6,
                            bool *flag_417);

/* sub_08065C2C: sidestep writer — count = 1, forced to 2 by
 * gEwramData + 0x37E bit 0x10, +1 by bit 0x02; for each step it
 * writes the s16 pixel x at +0x42 (x-16, x+16, x-8, then x+8)
 * and calls sub_0803AC40(e), restoring the original +0x42 after.
 * `step` is the injected sub_0803AC40. Returns the count. */
unsigned aos_npc_talk_65c2c(int16_t x_px, uint16_t flags_37e,
                            void (*step)(void *ctx), void *ctx,
                            int16_t *targets);

/* sub_08065CD8: companion spawn — both sub_08012078(0x20) and
 * sub_08012078(0x2E) must be set and sub_0805B754(e, 3) must
 * keep the entity, else +0x59 bit 3. On success a RNG&1 pick
 * runs sub_0803F2C8(1|2, 0, 1), +0x58 bit 6 arms, +0x5C becomes
 * player +0x5C + 1, and the sub_0805B5E8(3) child copies
 * +0x42/+0x46/+0x5C + 1 with update sub_08065C2C. */
typedef struct {
    uint8_t flags58;    /* +0x58 */
    uint8_t flags59;    /* +0x59 */
    uint8_t anim5c;     /* +0x5C */
    int16_t spawn_x, spawn_y;   /* own +0x42/+0x46 */
    uint32_t child;     /* +0x18 entity, 0 none */
    uint8_t child_anim; /* child +0x5C */
    int16_t child_x, child_y; /* child +0x42/+0x46 */
    bool marked;        /* +0x59 bit 3 */
    bool variant_a;     /* RNG&1 == 0 -> sub_0803F2C8 arg 1 */
} AosNpcCompanion;

void aos_npc_talk_65cd8(AosNpcCompanion *c, bool flag_12078_20,
                        bool flag_12078_2e, bool keep_5b754,
                        int rng_bit, uint8_t player_anim5c,
                        bool (*spawn_child)(void));

/* sub_08065DB8: the looping NPC interaction — state 0 clears
 * +0x42C bit 0x200; state 1 gates on the sub_08068AD4 probe, the
 * gEwramData + 0x16 & 0x42 mask and the + 0x4BE pause word, freezes
 * the player's +0x48/+0x50/+0x4C, picks the +0x14 text through
 * the sub_08012078/sub_08012048 flag pairs (0x32/0x2C, 0x31/0x2B,
 * 0x30/0x2A, 0x2F/0x29, default 0x2D), arms +0x42C bit 0, opens
 * the window, mirrors facing and advances; state 2 polls
 * sub_08067A04 then closes; state 3 releases with sub_0805B66C(0),
 * plays sub_0805B878(0x1041) + PlaySong(0x26), writes the
 * gEwramData + 0x254CC pair and loops to state 1. */
typedef struct {
    uint8_t state;      /* +0xA */
    uint8_t ctr_b;      /* +0xB */
    uint8_t ctr_c;      /* +0xC */
    uint8_t text_id;    /* +0x14 */
    uint8_t flags58;    /* +0x58 */
    uint8_t flags6d;    /* +0x6D */
    int32_t x;          /* +0x40 for the facing compare */
    /* outputs */
    bool cleared_42c_200;   /* +0x42C &= ~0x200 */
    bool froze_player;      /* player +0x48/+0x50 = 0, +0x4C clamp */
    bool window_text;       /* sub_08067934(+0x14) */
    bool armed_42c_0;       /* +0x42C |= 1 */
    bool facing_mirror;     /* +0x58 bit 6 update */
    bool f2c8_3;            /* +0x6D == 2 -> sub_0803F2C8(3,0,1) */
    bool window_close;      /* sub_08067898 */
    bool f2c8_2;            /* +0x6D == 3 -> sub_0803F2C8(2,0,1) */
    bool release_0;         /* sub_0805B66C(0) */
    bool sound_1041;        /* sub_0805B878(0x1041) */
    bool song_26;           /* PlaySong(0x26) */
    bool wrote_254cc;       /* +0x254CC pair writes */
    bool sfx_26;            /* sub_08013E18(0x26) */
    bool ev_1e;             /* sub_08013CF0(0x1E) */
} AosNpcLoop;

/* `flags` packs the injected globals: bit0 probe_inside,
 * bit1 gEwramData+0x16 & 0x42, bit2 +0x4BE == 0, bit3 the
 * sub_0805B864 result, bits 4-10 the flag bytes 0x32, 0x26,
 * 0x31, 0x25, 0x30, 0x2E, 0x2F. `text_done` is the sub_08067A04
 * result; `player_x` the player +0x40. */
void aos_npc_talk_65db8(AosNpcLoop *g, uint32_t flags,
                        bool text_done, int32_t player_x);

/* sub_08066094: the guard NPC sequence — state 0 clears
 * +0x42C bit 0x200 and plays sub_08013EEC(0x26); state 1 gates on
 * the probe/+0x16 mask/+0x4BE pause, sets +0x42C bit 0x200,
 * freezes the player, picks the +0x14 text through the +0x18 /
 * sub_0805B864 branches (sub_08066308, sub_0806666C -> 0x44,
 * sub_0806668C — unported, text supplied by the caller), mirrors
 * facing, opens the window and plays sub_0805B878(0x1040);
 * state 2 polls, clears +0x18, plays the 0x1B5/0x26 songs and
 * either loops to state 1 (release) or advances to state 3 on
 * sub_0805B864; state 3 runs the +0xB escort sub-states and
 * returns 0x10 while waiting for the player 48 px ahead. */
typedef struct {
    uint8_t state;      /* +0xA */
    uint8_t ctr_b;      /* +0xB */
    uint8_t ctr_c;      /* +0xC */
    uint8_t timer_d;    /* +0xD */
    uint8_t field18;    /* +0x18 variant flag */
    uint8_t text_id;    /* +0x14 */
    uint8_t flags58;    /* +0x58 */
    uint8_t flags6d;    /* unused here, kept for parity */
    int32_t x;          /* +0x40 */
    /* outputs */
    bool cleared_42c_200;
    bool sfx_26_entry;      /* sub_08013EEC(0x26) on state 0 */
    bool froze_player;
    bool armed_42c_200;     /* +0x42C |= 0x200 */
    bool facing_mirror;
    bool window_text;
    bool ev_1d;             /* sub_08013CF0(0x1D) */
    bool sound_1040;        /* sub_0805B878(0x1040) */
    bool cleared_18;        /* +0x18 = 0 on state-2 exit */
    bool song_1b5;
    bool wrote_254cc;
    bool song_26;
    bool sound_1041;
    bool release_0;         /* sub_0805B66C(0) */
    uint32_t ret;           /* ROM return value (0 or 0x10) */
} AosNpcGuard;

/* `flags`: bit0 probe_inside, bit1 +0x16 & 0x42, bit2 +0x4BE == 0,
 * bit3 sub_0805B864 result. `picked_text` is the +0x14 chosen by
 * the unported pickers; `player_x` the player +0x40. */
uint32_t aos_npc_talk_66094(AosNpcGuard *g, uint32_t flags,
                            uint8_t picked_text, bool text_done,
                            int32_t player_x);

/* sub_08066024: Object20 spawn gate — when flag 0x20 is set and
 * sub_0805B754(e, 0) keeps the entity, the follower variant runs
 * (sub_0803F2C8(e,0,0,1), sub_0803F17C, +0x58 &= ~0x41,
 * +0x5C = player +0x5C + 1, +0x18 = 1, return 1); otherwise the
 * entity is marked +0x59 |= 8 and returns 0. */
typedef struct {
    uint8_t flags58;    /* +0x58 */
    uint8_t flags59;    /* +0x59 */
    uint8_t field18;    /* +0x18 */
    uint8_t anim5c;     /* +0x5C */
    /* outputs */
    bool kept;          /* sub_0805B754 nonzero */
    bool variant;       /* sub_0803F2C8 ran */
    bool anim_step;     /* sub_0803F17C ran */
    bool facing_cleared;/* +0x58 &= ~0x41 */
    uint32_t ret;
} AosNpcSpawnGate;

uint32_t aos_npc_talk_66024(AosNpcSpawnGate *g, bool flag20,
                            bool keep, uint8_t player_5c);

/* The gEwramData+0x138F4 guard-sequence cursor driven by
 * sub_080665B4: +0x138F4 mode, +0x138F6 counter, +0x138F8 table
 * index, +0x138FA fallback value. sub_08066648 initializes it,
 * sub_0806666C resets it (counter 0xFF), sub_0806668C selects the
 * mode from flags 0x2E/0x26 and sets text 0x36, and sub_080665B4
 * ticks it: while the counter is <= 1 it picks a word through the
 * 0x085269DC record table (counter 0 picks rec[3 + rng % rec[2]]
 * — or rec fallback for modes 4/6 — counter 1 picks rec[1]),
 * transforms it through sub_08041434 and stores it at
 * gEwramData+0x40C, then increments the counter and returns 1;
 * afterwards it returns 0. */
typedef struct {
    uint16_t mode;      /* +0x138F4 */
    uint16_t counter;   /* +0x138F6 */
    uint16_t index;     /* +0x138F8 */
    uint16_t fallback;  /* +0x138FA */
} AosGuardSeq;

/* Record layout at each table entry (words): [1] alternate pick,
 * [2] modulo count, [3..] pick array. */
typedef uint16_t (*AosGuardXform)(uint16_t v);

void aos_guard_seq_init(AosGuardSeq *g, uint16_t mode,
                        uint16_t index, uint16_t fallback);
void aos_guard_seq_reset(AosGuardSeq *g);
uint8_t aos_guard_seq_pick(AosGuardSeq *g, bool flag2e,
                           bool flag26, uint8_t *text_id);
uint32_t aos_guard_seq_tick(AosGuardSeq *g,
                            const uint16_t *const *table,
                            uint32_t rng, AosGuardXform xform,
                            uint16_t *out);

/* sub_080666d4: repeating NPC dialogue — state 0 clears +0x42C
 * bit 0x200; state 1 gates on probe/+0x16 mask/+0x4BE pause,
 * freezes the player, picks the +0x14 text through
 * sub_080667DC, sets +0x42C bit 0 and opens the window; state 2
 * polls, marks sub_08012048(0x38), releases with
 * sub_0805B66C(0) and loops to state 0. */
typedef struct {
    uint8_t state;      /* +0xA */
    uint8_t ctr_b;      /* +0xB */
    uint8_t ctr_c;      /* +0xC */
    uint32_t text_id;   /* +0x14 (word) */
    uint8_t flags58;    /* +0x58 unused here */
    /* outputs */
    bool cleared_42c_200;
    bool froze_player;
    bool armed_42c_1;
    bool window_text;
    bool mark_38;       /* sub_08012048(0x38) */
    bool release_0;     /* sub_0805B66C(0) */
} AosNpcTalkLoop;

/* `flags`: bit0 probe_inside, bit1 +0x16 & 0x42, bit2 +0x4BE == 0,
 * bit3 sub_08067A04 done. */
void aos_npc_talk_666d4(AosNpcTalkLoop *g, uint32_t flags,
                        bool cond864, uint32_t rng);

/* sub_080667DC: +0x14 text pick — 0x59 when sub_0805B864 and
 * (rng & 0xF) <= 3, else 0x54 which also programs the guard
 * sequence (+0x138F4 = 0x54, counter 0, index 5, fallback 0). */
void aos_npc_talk_667dc(AosNpcTalkLoop *g, bool cond864,
                        uint32_t rng, AosGuardSeq *seq);

/* sub_08066828: spawn gate — flags 0x20 AND 0x26 must both be
 * set or +0x59 |= 8; then sub_0805B754(e, 2) keep: failure runs
 * EntityDelete, success runs sub_0803F2C8(e,6,0,1),
 * sub_0803F17C and sets +0x58 bit 0x40. */
typedef struct {
    uint8_t flags58;    /* +0x58 */
    uint8_t flags59;    /* +0x59 */
    /* outputs */
    bool marked_8;      /* +0x59 |= 8 */
    bool deleted;       /* EntityDelete */
    bool variant;       /* sub_0803F2C8(6,0,1) */
    bool anim_step;     /* sub_0803F17C */
} AosNpcSpawnGate2;

void aos_npc_talk_66828(AosNpcSpawnGate2 *g, bool flag20,
                        bool flag26, bool keep);

#endif
