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
