/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow shared effects (cvaos code_08060B98, code_08040A38 and
 * code_080211F0). No SDL. */
#ifndef AOS_EFFECT_H
#define AOS_EFFECT_H

#include <stdbool.h>
#include <stdint.h>

#include "aos_anim.h"
#include "aos_collision.h"

/* Shared effects drawn from the common frames 0x0820ED60, each with its
 * own animation set (one animation, index 0):
 * - the death flame sub_0806E524 (animation 0, OBJ row 7: Effect/flame),
 *   rising 0.5 per frame after its first one;
 * - the effect sub_0806B5DC of type 1 (sub_0806E664(x, y, 1); table
 *   0x08526D0C: animation 13, OBJ row 8: Effect/blood), still, priority 3;
 * both deleted when their animation ends;
 * - the particles of sub_08068F78 (`particle`), created by sub_0806D894
 *   (animation 5, OBJ row 6: Effect/smoke), sub_0806D930 (here animation 5,
 *   row 5: Effect/dark_smoke) and sub_0806D644 (animation 0, row 7: the
 *   flame art): + 0x18 bit 0 / 1 keeps 7/8 of vx / vy each frame, then
 *   sub_0806D3D8(e, 8.0) moves them, bit 2 blinks them with 0x08118CE0
 *   over 40 frames; deleted on the update after their animation ends;
 * - the drifting effects of sub_08045CEC(x, y, animation, row) updated by
 *   sub_08045C9C (`drift`): x += vx, vx += ax, y += vy, vy += ay, then the
 *   animation step, deleted when it ends; priority 0x0D. Used with
 *   animation 1, row 8 (Effect/fire), 16, row 6 (Effect/spark), 4, row 6
 *   (Effect/dust), 7, row 8 (Effect/impact, a class 4 shot on a wall) and
 *   1, row 7 (Effect/ember, the dead warrior's death);
 * - the shell casing sub_080228F8 (`casing`): sprite frame 0x35 with row 8
 *   (Effect/casing, a looping one-frame animation), see aos_effect_update;
 * - the burst pieces of sub_08045A98 updated by sub_08045E80 (`burst`):
 *   hidden while + 0x0D counts down, then moved by sub_0806D430 /
 *   sub_0806D460 (8.0 clamps) and animated, deleted when the animation
 *   ends. Burst list 0 (0x08525498) uses animation 0 row 7 (the flame
 *   art), 4 and 5 row 6 (dust, smoke) and the burst arts 1 row 7
 *   (Effect/burst_1), 12 row 6 (Effect/burst_12) and 15 row 7
 *   (Effect/burst_15). */
enum { AOS_EFFECT_FLAME = 0, AOS_EFFECT_BLOOD = 1, AOS_EFFECT_SMOKE = 2,
       AOS_EFFECT_DARK_SMOKE = 3, AOS_EFFECT_FIRE = 4, AOS_EFFECT_SPARK = 5,
       AOS_EFFECT_DUST = 6, AOS_EFFECT_IMPACT = 7, AOS_EFFECT_CASING = 8,
       AOS_EFFECT_EMBER = 9, AOS_EFFECT_BURST_1 = 10, AOS_EFFECT_BURST_12 = 11,
       AOS_EFFECT_BURST_15 = 12, AOS_EFFECT_KINDS = 13 };

typedef struct {
    uint8_t kind;           /* AOS_EFFECT_*: the art */
    int32_t x, y;           /* + 0x40, + 0x44 */
    bool mirrored;          /* + 0x58 bit 0x40, at random */
    bool started;           /* + 0x0A: 0 on the first update */
    bool particle;          /* sub_08068F78 */
    bool drift;             /* sub_08045C9C */
    bool casing;            /* sub_080228F8 */
    bool follow;            /* sub_080C9198: a spark held on the player */
    bool burst;             /* sub_08045E80 */
    uint8_t state;          /* + 0x0A of a casing: 1 falling, 2 bounced */
    int32_t ax, ay;         /* + 0x50, + 0x54 of a drifting effect */
    uint8_t flags;          /* + 0x18 of a particle */
    int32_t vx, vy;         /* + 0x48, + 0x4C of a particle */
    uint8_t timer;          /* + 0x0D: blink counter of a particle */
    bool hidden;            /* + 0x58 bit 0x20 */
    bool cycle_done;        /* + 0x59 bit 0 */
    AosAnimState anim;
} AosEffect;


/* What a casing needs from the frame: the collision layer, the camera and
 * RandomNumberGenerator. */
typedef struct {
    const AosCollision *layer;
    int32_t cam_x, cam_y;
    uint32_t (*random)(void);
    int32_t player_x;       /* the player's + 0x40 */
    int16_t player_top;     /* sub_0801CFB0: his y - 0x22 (- 0x16 crouching) */
} AosEffectWorld;

/* sub_0806E524 / sub_0806B5DC / sub_08068F78 / sub_08045C9C /
 * sub_080228F8 for one frame with the effect kind's animation set and the
 * blink pattern 0x08118CE0 (NULL: never hidden); false once the effect is
 * deleted. A particle's first call also performs its creation (animation
 * start and step), which the game does in the creating entity's frame.
 * `world` may be NULL except for a casing or a follower.
 *
 * A follower (sub_080C9198) plays its animation once from its first
 * update (a random flip); then each frame it turns over on even timers, sits
 * at the player's x and sub_0801CFB0 + 8 + timer / 2, and is deleted once
 * its animation has ended.
 *
 * A casing's first update deletes it inside a cell with bits 8 and 1
 * (sub_08002028), else sets ay = 0x3000; then (states 1 and 2) it moves with
 * sub_0806D430 / sub_0806D460 (8.0 clamps); in state 1 a solid cell 2
 * pixels below it (sub_080020A0) bounces it: vy = -(vy / 2), vx +=
 * sub_0802D5EC(0x4000), state 2 (sound 0x14F, then it falls through). It is
 * deleted outside sub_08068AD4(e, -16, -16, 0x110, 0xC0) on screen. */
bool aos_effect_update(AosEffect *effect, const AosAnimSet *anims, const uint8_t *blink,
                       const AosEffectWorld *world);

#endif /* AOS_EFFECT_H */
