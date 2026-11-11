/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SOUL_RED_ARACHNE_H
#define AOS_SOUL_RED_ARACHNE_H

#include <stdbool.h>
#include <stdint.h>

/* SkillArachneUse, 0x08028EC0, shared by red souls 16 (Arachne,
 * parameter 6) and 21 (Mudman, parameter 7) through the throw table
 * 0x084F12C8. ROM-free state machine; graphics, palette allocation,
 * the frame-box lookup and collision probing stay with the runtime.
 *
 * State 0 places the entity through sub_08031EA4(source, 8, -0x1A)
 * with the record's vx (negated facing left), vy and +0x54 gravity,
 * attack box preset 5 plus the sub_0802DB84 hit callback, song from
 * record +0x10 and the +0x131BE count, then state 1.
 *
 * State 1 runs sub_0806D3D8(entity, 0x80000) — +0x48 += +0x50,
 * +0x4C += +0x54 capped on the acceleration's side, then the position
 * — and sub_0803F17C's animation advance with the +0x64/+0x65 frame
 * box lookup. Once the strike's sub_080239E4 child exists (+0x20):
 *   - record flags & 0x100 clear (Arachne): state 2, +0x0D = 0x10,
 *     +0x0F = 1 and +0x64 = 5.
 *   - record flags & 0x100 set (Mudman): PlaySong 0xD2, three
 *     sub_0802D880 shard children and sub_0803F2C8 sequence 2, then
 *     state 3.
 *
 * State 2 deletes when the child's +0x24 runs out or +0x0D counts to
 * zero. State 3 deletes on the child's +0x24 or the animation end
 * (sub_0803F17C == 3). Every state's tail runs sub_0806D128(e, 2) and
 * the +0x59 bit-4 mark into EntityDelete. */

enum { AOS_ARACHNE_LIMIT = 0x80000 };

typedef struct {
    int32_t x, y;               /* +0x40, +0x44, 16.16 room pixels */
    int32_t vx, vy, ay;         /* +0x48, +0x4C, +0x54 (+0x50 stays 0) */
    uint8_t state;              /* +0x0A: 0 init, 1 flying, 2 lingering,
                                 * 3 releasing */
    uint8_t timer;              /* +0x0D: the state-2 linger */
    bool blink;                 /* +0x0F: set entering state 2 */
    bool frame5;                /* +0x64 = 5, set entering state 2 */
    uint8_t parameter;          /* +0x1E: throw record index (6/7) */
    bool releases;              /* record +0x12 & 0x100: Mudman only */
    bool facing_left;           /* +0x58 bit 6 */
    bool started;               /* state 0 ran once */
    bool planted;               /* +0x20: the sub_0802DB84 child exists */
    bool host_gone;             /* the child's +0x24: host +0x34 ran out */
    bool released;              /* out-event: state 3 entered once */
} AosArachneSoul;

/* sub_08031EA4(entity, source, 8, -0x1A): x offset follows the source
 * facing, vx is the record's speed negated when mirrored. vx/vy/ay are
 * the record's +0x00/+0x04/+0x08 fields (vy already signed). */
AosArachneSoul aos_arachne_create(int32_t soma_x, int32_t soma_y,
                                  bool facing_left, uint8_t parameter,
                                  bool releases, int32_t vx, int32_t vy,
                                  int32_t ay);

/* Advance one frame; false once EntityDelete ran. outside is the
 * sub_0806D128(e, 2) margin result, anim_done the sub_0803F17C == 3
 * animation end (only read in state 3). */
bool aos_arachne_update(AosArachneSoul *soul, bool outside, bool anim_done);

/* A sub_0802D880 shard child (entity type 0x49/0xCF on the resource-17
 * art): +0x48 vx, +0x4C vy, +0x54 = 0x4000 gravity, no x acceleration.
 * It deletes when the parent's count drains (the runtime's orphan
 * check) or the sub_08068AD4 window probe leaves the view. */
typedef struct {
    int32_t x, y;               /* +0x40, +0x44 */
    int32_t vx, vy;             /* +0x48, +0x4C (+0x54 gravity below) */
    uint8_t frame;              /* +0x65: 10 or 11 */
    bool alive;
} AosArachneShard;

/* The three draws the release consumes, in order: frame pick
 * (rng & 1) + 10, horizontal drift -(parent +0x48)/2 +
 * ((rng & 0xFF) - 0x80) << 9 and vertical drift (rng & 0x7FFF) - 0x8000
 * (the native 0xFFFF8000 | rng). The ±4 px spawn offsets the native
 * writes to +0x42/+0x46 are overwritten by the parent position copy,
 * so the shard spawns on the parent exactly. */
AosArachneShard aos_arachne_shard_create(int32_t x, int32_t y,
                                         int32_t parent_vx,
                                         uint32_t draw_frame,
                                         uint32_t draw_vx,
                                         uint32_t draw_vy);

/* sub_0802D880's integration: position += velocity, velocity.y +=
 * 0x4000, then the window probe; false once deleted. outside is the
 * caller's sub_08068AD4 approximation. */
bool aos_arachne_shard_update(AosArachneShard *shard, bool outside);

#endif
