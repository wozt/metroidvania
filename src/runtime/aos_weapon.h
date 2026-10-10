/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow weapon entity of classes 0, 2 and 3.
 *
 * Port of the lifetime and placement of cvaos sub_080221CC: created with
 * the attack (sub_080230A8), it plays its animation once at Soma's position
 * (13 pixels lower when he was crouched), facing as he did when it was
 * created, and is deleted when Soma's attack flag 0x20 clears or its
 * animation ends. Its hitbox comes from the current frame record. Damage
 * (sub_0802346C), the sounds and classes 1, 4 and 5 are not ported. No SDL. */
#ifndef AOS_WEAPON_H
#define AOS_WEAPON_H

#include <stdbool.h>
#include <stdint.h>

#include "aos_anim.h"
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
    int y_offset;
    AosAnimState anim;
} AosWeaponEntity;

/* Runs after aos_soma_update, like the entity update loop. */
void aos_weapon_update(AosWeaponEntity *weapon, AosSoma *soma, const AosWeaponFrames *frames);
/* Active hitbox in room pixels; false when the frame has none. */
bool aos_weapon_hitbox(const AosWeaponEntity *weapon, const AosSoma *soma,
                       const AosWeaponFrames *frames, int *x, int *y, int *w, int *h);

#endif /* AOS_WEAPON_H */
