/* SPDX-License-Identifier: GPL-3.0-only */
/* Object20 generic talk-state callbacks from
 * code_08060B98.s: the + 0x42C bit-1 arm, the sub_08067934
 * window open, the sub_08067A04 text poll and the
 * sub_0805B66C/sub_08067898 exits. */
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

#endif

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
