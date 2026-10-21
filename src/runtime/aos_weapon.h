/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow weapon entity of classes 0 to 3.
 *
 * Port of the lifetime and placement of cvaos sub_080221CC (classes 0, 2
 * and 3): created with the attack (sub_080230A8), it plays its animation
 * once at Soma's position (13 pixels lower when he was crouched), facing as
 * he did when it was created, and is deleted when Soma's attack flag 0x20
 * clears or its animation ends. Class 1 (sub_080224BC) swings: it sits 25
 * pixels above Soma (13 when crouched) and 1 to 3 pixels ahead, turns its
 * affine angle from -0x3000 by 0x980 (variant 0, 15 frames) or 0xCC0 (11
 * frames), mirrored when facing left, and lives 26 or 21 frames
 * (0x080E13B8[variant]) regardless of its animation. The hitbox comes from
 * the current frame record, unrotated. Class 4 (sub_08022A54) fires: its
 * sprite plays at Soma's position (12 pixels lower when crouched) while
 * the entity itself, the shot, moves from Soma + 8.0, 28 pixels above,
 * accelerating by 8.0 per frame from its fourth frame, with the box
 * (-8, -4, 16, 8) of sub_08042884(e, 0x0810FCF8) live from its third frame
 * until it meets a wall (an impact effect there); a shell casing is
 * ejected at creation. Damage (sub_0802346C / sub_080234B0), the sounds and
 * class 5 are not ported. No SDL. */
#ifndef AOS_WEAPON_H
#define AOS_WEAPON_H

#include <stdbool.h>
#include <stdint.h>

#include "aos_anim.h"
#include "aos_collision.h"
#include "aos_effect.h"
#include "aos_soma.h"

typedef struct {
    int8_t x, y;
    uint8_t width, height;
    bool active;
} AosHitbox;

typedef struct {
    const AosAnimSet *anims;    /* one animation: the weapon's */
    const AosHitbox *hitboxes;  /* one per animation frame */
} AosWeaponFrames;

typedef struct {
    bool active;
    bool facing_left;
    int x_offset, y_offset;     /* from Soma's position, pixels */
    uint8_t weapon_class;
    uint8_t timer;              /* + 0x0D (class 1) */
    uint8_t swing_frames;       /* + 0x0B: the angle turns while timer < it */
    uint8_t divisor;            /* + 0x0C: x offset = min(timer / it + 1, 3) */
    uint8_t lifetime;           /* + 0x19 */
    int32_t angle, angle_step;  /* + 0x1C, + 0x20: affine angle, 0x10000 a turn */
    /* Class 4: the shot's offset from Soma (+ 0x48, + 0x4C, 16.16), its
     * acceleration (+ 0x50), + 0x1C (a wall was hit) and its attack box. */
    int32_t shot_x, shot_y, shot_ax;
    bool shot_stopped, shot_box;
    /* Effects created by this update (impact, casing). */
    uint8_t effect_count;
    AosEffect effects[2];
    AosAnimState anim;
} AosWeaponEntity;

/* Runs after aos_soma_update, like the entity update loop; the layer and
 * random() (RandomNumberGenerator) serve class 4 and may be NULL for the
 * other classes. */
void aos_weapon_update(AosWeaponEntity *weapon, AosSoma *soma, const AosWeaponFrames *frames,
                       const AosCollision *layer, uint32_t (*random)(void));
/* Active hitbox in room pixels; false when the frame has none. */
bool aos_weapon_hitbox(const AosWeaponEntity *weapon, const AosSoma *soma,
                       const AosWeaponFrames *frames, int *x, int *y, int *w, int *h);

#endif /* AOS_WEAPON_H */
