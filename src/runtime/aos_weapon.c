/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow weapon entity; comments name the cvaos routine. */
#include "aos_weapon.h"

static void delete_weapon(AosWeaponEntity *weapon, AosSoma *soma) {
    weapon->active = false;
    soma->weapon_active = false;    /* gEwramData + 0x1311C = NULL */
}

/* sub_080221CC: case 0 (creation) falls through to the follow and step. */
void aos_weapon_update(AosWeaponEntity *weapon, AosSoma *soma, const AosWeaponFrames *frames) {
    if (!weapon->active) {
        if (!soma->weapon_active || !(soma->flags & AOS_FLAG_ATTACKING) || !frames) return;
        weapon->active = true;
        weapon->facing_left = soma->facing_left;
        weapon->y_offset = (soma->flags & AOS_FLAG_CROUCH) ? 13 : 0;
        aos_anim_start(&weapon->anim, frames->anims, 0, false);
    }
    if (!(soma->flags & AOS_FLAG_ATTACKING)) {
        delete_weapon(weapon, soma);
        return;
    }
    if (aos_anim_step(&weapon->anim, frames->anims) == 3) delete_weapon(weapon, soma);
}

bool aos_weapon_hitbox(const AosWeaponEntity *weapon, const AosSoma *soma,
                       const AosWeaponFrames *frames, int *x, int *y, int *w, int *h) {
    if (!weapon->active || !frames || !frames->hitboxes) return false;
    const AosHitbox *box = &frames->hitboxes[weapon->anim.frame];
    if (!box->active) return false;
    int origin_x = soma->x >> 16, origin_y = (soma->y >> 16) + weapon->y_offset;
    *x = weapon->facing_left ? origin_x - box->x - box->width : origin_x + box->x;
    *y = origin_y + box->y;
    *w = box->width;
    *h = box->height;
    return true;
}
