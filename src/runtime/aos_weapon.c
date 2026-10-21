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

/* sub_08022A54 case 0: the shot starts 8.0 ahead (+ 0x50 added once) and
 * sub_08031EA4 places a casing (sub_080228F8) 8 pixels ahead and 26 above
 * Soma (+ the crouch offset), flung back at -1.0 + (r & 0x7FFF) and up at
 * -1.25 - (r & 0x7FFF); the draws are made only when it is created. */
static void start_shot(AosWeaponEntity *weapon, const AosSoma *soma, uint32_t (*random)(void)) {
    weapon->shot_ax = weapon->facing_left ? -0x80000 : 0x80000;
    weapon->shot_x = weapon->shot_ax;
    int crouch = (soma->flags & AOS_FLAG_CROUCH) ? 12 : 0;
    weapon->shot_y = (int32_t)((uint32_t)crouch << 16);
    weapon->y_offset = crouch;
    if (!random) return;
    AosEffect *casing = &weapon->effects[weapon->effect_count++];
    int dx = weapon->facing_left ? -8 : 8;
    *casing = (AosEffect){.kind = AOS_EFFECT_CASING, .casing = true,
                          .mirrored = weapon->facing_left,
                          .x = (int32_t)((uint32_t)((soma->x >> 16) + dx) << 16),
                          .y = (int32_t)((uint32_t)((soma->y >> 16) + crouch - 0x1A) << 16)};
    casing->vx = (int32_t)(random() & 0x7FFF) - 0x10000;
    casing->vy = (int32_t)0xFFFEC000 - (int32_t)(random() & 0x7FFF);
    if (casing->mirrored) casing->vx = -casing->vx;
}

/* sub_08022A54 case 1: the shot follows Soma at its offset, its box on at
 * the third frame, accelerating after the third; until it hits a wall
 * (sub_080020D8 moving right, sub_0800210C left) it leaves an impact
 * there (sub_08045CEC(x + push, y, 7, 8); sound 0x14E) and its box goes
 * off. Deleted when the animation ends. */
static void shot(AosWeaponEntity *weapon, AosSoma *soma, const AosWeaponFrames *frames,
                 const AosCollision *layer) {
    weapon->timer++;
    if (!weapon->shot_stopped && weapon->timer == 3) weapon->shot_box = true;
    if (weapon->timer > 3) weapon->shot_x += weapon->shot_ax;
    int32_t x = soma->x + weapon->shot_x, y = soma->y + weapon->shot_y - 0x1C0000;
    if (!weapon->shot_stopped) {
        int push = 0;
        if (layer)
            push = weapon->shot_ax > 0 ? aos_wall_push_left(layer, x >> 16, y >> 16)
                                       : aos_wall_push_right(layer, x >> 16, y >> 16);
        if (push && weapon->effect_count < 2) {
            weapon->effects[weapon->effect_count++] = (AosEffect){
                .kind = AOS_EFFECT_IMPACT, .drift = true,
                .x = x + (int32_t)((uint32_t)push << 16), .y = y};
            weapon->shot_stopped = true;
        }
    } else {
        weapon->shot_box = false;
    }
    if (aos_anim_step(&weapon->anim, frames->anims) == 3) delete_weapon(weapon, soma);
}

/* sub_080221CC / sub_080224BC / sub_08022A54: case 0 (creation) falls
 * through to the follow and step. */
void aos_weapon_update(AosWeaponEntity *weapon, AosSoma *soma, const AosWeaponFrames *frames,
                       const AosCollision *layer, uint32_t (*random)(void)) {
    weapon->effect_count = 0;
    if (!weapon->active) {
        if (!soma->weapon_active || !(soma->flags & AOS_FLAG_ATTACKING) || !frames) return;
        *weapon = (AosWeaponEntity){.active = true, .facing_left = soma->facing_left,
                                    .weapon_class = soma->weapon.weapon_class};
        weapon->y_offset = (soma->flags & AOS_FLAG_CROUCH) ? 13 : 0;
        if (weapon->weapon_class == 1) start_swing(weapon, soma);
        if (weapon->weapon_class == 4) start_shot(weapon, soma, random);
        aos_anim_start(&weapon->anim, frames->anims, 0, false);
    }
    /* sub_08022DEC (class 5) keeps following without the 0x20 check. */
    if (!(soma->flags & AOS_FLAG_ATTACKING) && weapon->weapon_class != 5) {
        delete_weapon(weapon, soma);
        return;
    }
    if (weapon->weapon_class == 1) {
        swing(weapon, soma, frames);
        return;
    }
    if (weapon->weapon_class == 4) {
        shot(weapon, soma, frames, layer);
        return;
    }
    if (aos_anim_step(&weapon->anim, frames->anims) == 3) delete_weapon(weapon, soma);
}

bool aos_weapon_hitbox(const AosWeaponEntity *weapon, const AosSoma *soma,
                       const AosWeaponFrames *frames, int *x, int *y, int *w, int *h) {
    if (!weapon->active) return false;
    if (weapon->weapon_class == 4) {
        /* The bytes of 0x0810FCF8 copied as the box: (-8, -4, 16, 8). */
        if (!weapon->shot_box) return false;
        int origin_x = (soma->x + weapon->shot_x) >> 16;
        int origin_y = (soma->y + weapon->shot_y - 0x1C0000) >> 16;
        *x = weapon->facing_left ? origin_x + 8 - 16 : origin_x - 8;
        *y = origin_y - 4;
        *w = 16;
        *h = 8;
        return true;
    }
    if (!frames || !frames->hitboxes) return false;
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
