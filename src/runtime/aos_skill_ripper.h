/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SKILL_RIPPER_H
#define AOS_SKILL_RIPPER_H

#include <stdbool.h>
#include <stdint.h>

/* Skill_EvilButcher_Ripper_Use, 0x08023A70; ROM-free motion submodel.
 * The source's configuration table at 0x084F128C has two 16-byte entries.
 * A zero flag at record byte +15 rebounds; nonzero begins the fade.
 * Graphics, palette allocation and collision probing stay with the runtime;
 * the hit helper's target-relative position and lifetime are represented. */
enum {
    AOS_RIPPER_FLYING = 1,
    AOS_RIPPER_REBOUNDING = 2,
    AOS_RIPPER_FADING = 3,
    AOS_RIPPER_REMOVED = 4,
    AOS_RIPPER_END_FRAMES = 50
};

typedef struct {
    int32_t x, y;               /* +0x40, +0x44, 16.16 room pixels */
    int32_t vx, vy, ay;         /* +0x48, +0x4C, +0x54 */
    int16_t angle;              /* +0x24: adjusted during rebound */
    uint8_t state, timer, blink;/* +0x0A, +0x0D, +0x0F */
    uint8_t variant;            /* record parameter, 0 or 1 */
    uint8_t attached_age;       /* skill entity's +0x50A counter */
    bool rebound;               /* entry[15] == 0, sourced from ROM */
    bool facing_left;           /* +0x58 bit 6 */
    bool started;               /* source state 0 entered once */
    bool attached;              /* +0x20 points at sub_080239E4 */
    int32_t target_dx, target_dy; /* helper +0x50/+0x54 */
} AosRipperSoul;

typedef struct {
    int32_t x, y;
    bool active;
    bool defeated;
} AosRipperTarget;

/* Initial position uses Soma's integer halves and y - 26, as in native
 * state 0.  rebound is true only when configuration byte +15 is zero. */
AosRipperSoul aos_ripper_soul_create(int32_t soma_x, int32_t soma_y,
                                    bool facing_left, uint8_t variant, bool rebound);

/* Advance one frame, returning false when removed.  wall_delta_pixels is
 * the signed horizontal correction that the native sub_08002058 or
 * sub_0800207C returned (zero when no wall was found).  outside is the
 * result of the native sub_0806D128(..., 4) activity-window test.
 * These are supplied by the caller; this module does not synthesize probes.
 * The runtime also supplies the target identity and owns palette/affine
 * rendering. */
bool aos_ripper_soul_update(AosRipperSoul *soul, int16_t wall_delta_pixels,
                            bool outside, const AosRipperTarget *target);

/* sub_0802D9BC: the first hit creates sub_080239E4 and records the blade's
 * offset from that enemy. Further hits keep the original target. */
void aos_ripper_soul_attach(AosRipperSoul *soul, int32_t target_x,
                            int32_t target_y);

#endif
