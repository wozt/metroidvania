/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SOUL_RED_DRYAD_H
#define AOS_SOUL_RED_DRYAD_H
#include <stdbool.h>
#include <stdint.h>

/* SkillDryadUse's seed: it arcs out of Soma with vx -+0x10000 and
 * vy -0x20000 under 0x2000 gravity (sub_0806D3D8, 8 px/frame cap),
 * plants on the first enemy its {-1,-1,2,2} box touches (the
 * sub_0802D9BC strike callback spawns the sub_080239E4 child on
 * slots 0x49-0xCF and grows the box to 0x1014F8F6) and then watches
 * the host for up to 0x14 frames. */
typedef struct {
    int32_t x, y;           /* + 0x40, + 0x44 (16.16) */
    int32_t vx, vy;         /* + 0x48, + 0x4C */
    int32_t ay;             /* + 0x54: 0x2000 */
    bool facing_left;       /* + 0x58 bit 6 */
    uint8_t state;          /* + 0x0A: 0 init, 1 flying, 2 planted,
                             * 3 lingering, 4 deleted */
    uint8_t timer;          /* + 0x0D: 0x78 after the strike, 0x10 in
                             * state 3 */
    int16_t counter;        /* + 0x26: planted frames, reset by each
                             * further strike (sub_0802DA08) */
    bool planted;           /* + 0x20: the child exists */
    bool host_gone;         /* the child's + 0x24: the host's + 0x34
                             * (hp) ran out — injected each update */
    bool host_flag;         /* the host's + 0x72 bit 2 — injected */
    bool anim_done;         /* sub_0803F17C == 3 — injected (the
                             * entity is a still frame; the runtime
                             * leaves it clear, documented) */
    bool outside;           /* sub_0806D128(e, 4) / + 0x59 bit 4 */
    bool song_d5;           /* this update requested song 0xD5 */
    bool bonus;             /* sub_08021FEC(8): + 0x1327A += 8 and the
                             * sub_08042A7C(8, 1) popup — once, at the
                             * first planted update */
    bool blink;             /* + 0x0F in state 3 */
} AosDryad;

/* The seed as sub_08031EA4(source, 0x14, -0x1A) leaves it, already
 * integrated by the caller or left at state 0 for the first update. */
AosDryad aos_dryad_create(int32_t soma_x, int32_t soma_y, bool facing_left);
/* One update; false once the entity is deleted (state 4). */
bool aos_dryad_update(AosDryad *dryad);
#endif
