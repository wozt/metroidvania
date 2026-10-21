/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow weapon entity; comments name the cvaos routine. */
#include "aos_weapon.h"

static void delete_weapon(AosWeaponEntity *weapon, AosSoma *soma) {
    weapon->active = false;
    soma->weapon_active = false;    /* gEwramData + 0x1311C = NULL */
}

/* 0x080E13B8: class 1 lifetimes by variant. */
static const uint8_t swing_lifetimes[3] = {26, 21, 21};

/* sub_080224BC case 0: the swing parameters. */
static void start_swing(AosWeaponEntity *weapon, const AosSoma *soma) {
    bool first = soma->weapon.variant == 0;
    weapon->swing_frames = first ? 0x10 : 0xC;
    weapon->divisor = first ? 5 : 4;
    weapon->angle = (int32_t)0xFFFFD000;
    weapon->angle_step = first ? 0x980 : 0xCC0;
    weapon->lifetime = swing_lifetimes[soma->weapon.variant < 3 ? soma->weapon.variant : 0];
    if (weapon->facing_left) {
        weapon->angle = -weapon->angle;
        weapon->angle_step = -weapon->angle_step;
    }
    /* + 0x4E: -25, or -13 with Soma's 0x400. */
    weapon->y_offset = (soma->flags & AOS_FLAG_CROUCH) ? -13 : -25;
    weapon->timer = 0;
}

/* sub_080224BC case 1: the position, the angle, the step and the lifetime. */
static void swing(AosWeaponEntity *weapon, AosSoma *soma, const AosWeaponFrames *frames) {
    weapon->timer++;
    int dx = weapon->timer / weapon->divisor + 1;
    if (dx > 3) dx = 3;
    weapon->x_offset = weapon->facing_left ? -dx : dx;
    if (weapon->timer < weapon->swing_frames) weapon->angle += weapon->angle_step;
    aos_anim_step(&weapon->anim, frames->anims);
    if (weapon->timer == weapon->lifetime) delete_weapon(weapon, soma);
}

/* sub_080221CC / sub_080224BC: case 0 (creation) falls through to the
 * follow and step. */
void aos_weapon_update(AosWeaponEntity *weapon, AosSoma *soma, const AosWeaponFrames *frames) {
    if (!weapon->active) {
        if (!soma->weapon_active || !(soma->flags & AOS_FLAG_ATTACKING) || !frames) return;
        *weapon = (AosWeaponEntity){.active = true, .facing_left = soma->facing_left,
                                    .weapon_class = soma->weapon.weapon_class};
        weapon->y_offset = (soma->flags & AOS_FLAG_CROUCH) ? 13 : 0;
        if (weapon->weapon_class == 1) start_swing(weapon, soma);
        aos_anim_start(&weapon->anim, frames->anims, 0, false);
    }
    if (!(soma->flags & AOS_FLAG_ATTACKING)) {
        delete_weapon(weapon, soma);
        return;
    }
    if (weapon->weapon_class == 1) {
        swing(weapon, soma, frames);
        return;
    }
    if (aos_anim_step(&weapon->anim, frames->anims) == 3) delete_weapon(weapon, soma);
}

bool aos_weapon_hitbox(const AosWeaponEntity *weapon, const AosSoma *soma,
                       const AosWeaponFrames *frames, int *x, int *y, int *w, int *h) {
    if (!weapon->active || !frames || !frames->hitboxes) return false;
    const AosHitbox *box = &frames->hitboxes[weapon->anim.frame];
    if (!box->active) return false;
    int origin_x = (soma->x >> 16) + weapon->x_offset;
    int origin_y = (soma->y >> 16) + weapon->y_offset;
    *x = weapon->facing_left ? origin_x - box->x - box->width : origin_x + box->x;
    *y = origin_y + box->y;
    *w = box->width;
    *h = box->height;
    return true;
}
