/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SOUL_RED_MANDRAGORA_H
#define AOS_SOUL_RED_MANDRAGORA_H
#include <stdbool.h>
#include <stdint.h>

/* SkillMandragoraUse's stationary plant: state 1 emits a leaf particle
 * every fourth frame of + 0x0D through sub_08045CEC at a sine-offset
 * position around + 0x40/+ 0x44; the + 0x6C bit-4 hit flag starts the
 * 0x20 + 8 frame death sequence (states 2 and 3). The leaf entity's
 * fields (+ 0x50/+ 0x54 velocities, + 0x58 flip bits) and the
 * sub_0803D18C/sub_0803D270/sub_0803CED4 palette effects are not
 * ported. */
typedef struct {
    uint8_t state;      /* + 0x0A: 0 init, 1 idle, 2 death, 3 delete */
    uint8_t timer;      /* + 0x0D: emission parity, then death counters */
    uint8_t flag_f;     /* + 0x0F: set when state 2 hands over to state 3 */
    bool leaf;          /* a leaf emission happened this update */
    int32_t leaf_x;     /* emitted leaf position (16.16, entity space) */
    int32_t leaf_y;
} AosMandragora;

/* State 0 loads the entry-0x12C record, sequence 0, + 0x5C = 0xF,
 * places it through sub_08031EA4(entity, source, 0x10, -0x1A), presets
 * attack box 5 plus the 0x7F7FC0C0 box, plays song 0xDF, sets
 * + 0x72 |= 4, + 0x59 |= 2, + 0x5A bits 0-2 = 2 and enters state 1. */
AosMandragora aos_mandragora_create(void);
/* One update. `rng` supplies the native RandomNumberGenerator draw for
 * the leaf angle (the two flip-bit draws collapse into it);
 * `hit`/`outside` stand for the + 0x6C bit-4 flag and the
 * sub_0806D128(e, 4) margin probe. Returns false once deleted. */
bool aos_mandragora_update(AosMandragora *mandragora, int32_t rng,
                           int32_t entity_x, int32_t entity_y,
                           bool hit, bool outside);
#endif
