/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission hatches; comments name the pinned decompilation's routines. */
#include "mzm_hatches.h"

#include <stddef.h>

enum {
    TILEMAP_FLAG = 1 << 10,         /* CLIPDATA_TILEMAP_FLAG */
    /* The opening frames left and right of the door
     * (CLIPDATA_TILEMAP_FLAG | 0x11, | 0x16). */
    OPENING_LEFT = TILEMAP_FLAG | 0x11,
    OPENING_RIGHT = TILEMAP_FLAG | 0x16,
    DAMAGE_SUPER_MISSILE = 1 << 3,  /* CAA_DAMAGE_TYPE_SUPER_MISSILE */
};

static void set_column(MzmHatch *hatch, const MzmHatchEnv *env, uint16_t value) {
    for (int i = 0; i < MZM_HATCH_VERTICAL_SIZE; ++i)
        env->set_block(env->context, (uint16_t)(value + 16 * i), hatch->y + i, hatch->x);
}

void mzm_hatch_update_animation(MzmHatch *hatch, const MzmHatchEnv *env) {
    uint32_t value = hatch->facing_right ? OPENING_RIGHT : OPENING_LEFT;
    int frame = hatch->frame - 1;
    if (hatch->state == MZM_HATCH_CLOSING) {
        frame = 2 - frame;
        if (frame < 0) {
            frame = 0;
            value = (uint32_t)(hatch->facing_right + env->tilemap_values[hatch->type]);
        } else if (hatch->type != MZM_HATCH_NONE) {
            frame += 0x40;
        }
    }
    if (hatch->type == MZM_HATCH_NONE) frame += 0x80;
    set_column(hatch, env, (uint16_t)(value + (uint32_t)frame));
}

void mzm_hatch_start_lock_animation(MzmHatch *hatch, const MzmHatchEnv *env, uint8_t state) {
    hatch->state = state;
    hatch->frame = 0;
    mzm_hatch_update_animation(hatch, env);
}

void mzm_hatch_override(MzmHatch *hatch, const MzmHatchEnv *env, uint8_t type) {
    hatch->type = type;
    set_column(hatch, env, (uint16_t)(env->tilemap_values[type] + hatch->facing_right));
    hatch->state = MZM_HATCH_CLOSED;
    hatch->frame = 0;
    hatch->adc = 0;
}

int mzm_hatch_check_opening(MzmHatch *hatches, int count, const MzmHatchEnv *env, int x, int y,
                            unsigned damage, MzmHatch **opened) {
    int action = MZM_HATCH_NOT_OPENING;
    if (opened) *opened = NULL;
    for (int i = 0; i < count; ++i) {
        MzmHatch *h = &hatches[i];
        if (!h->exists || h->state != MZM_HATCH_CLOSED) continue;
        action = MZM_HATCH_NOT_OPENING;
        if (h->x == x && h->y <= y && y <= h->y + (MZM_HATCH_VERTICAL_SIZE - 1) &&
            env->weakness[h->type] & damage) {
            h->hits++;
            action = MZM_HATCH_ACTION_OPENING;
            if (h->type == MZM_HATCH_LOCKED) {
                if (!env->unlocking) action = MZM_HATCH_ACTION_LOCKED;
            } else if (h->type == MZM_HATCH_LOCKED_NAVIGATION) {
                if (!env->navigation_unlocking) action = MZM_HATCH_ACTION_LOCKED;
            } else if (h->type == MZM_HATCH_MISSILE && damage & DAMAGE_SUPER_MISSILE) {
                h->hits = env->health[MZM_HATCH_MISSILE];
            }
        }
        if (action == MZM_HATCH_NOT_OPENING) continue;
        if (action == MZM_HATCH_ACTION_OPENING) {
            if (h->hits >= env->health[h->type]) {
                h->flashing_timer = 0;
                h->state = MZM_HATCH_OPENING;
                if (opened) *opened = h;
            } else {
                h->flashing_timer = 1;
            }
        } else {
            h->hits = 0;
        }
        h->hit_timer = 0;
        break;
    }
    return action;
}

/* ConnectionHatchFlashingAnimation */
static void flash(MzmHatch *hatch, const MzmHatchEnv *env) {
    if (hatch->hit_timer == 0) {
        hatch->hit_timer = 4;           /* CONVERT_SECONDS(1.f / 15) */
    } else {
        hatch->hit_timer--;
        return;
    }
    uint16_t value = hatch->flashing_timer & 1 ? env->tilemap_values[MZM_HATCH_NONE]
                                               : env->tilemap_values[hatch->type];
    if (hatch->facing_right) value++;
    for (int i = 0; i < MZM_HATCH_VERTICAL_SIZE; ++i)
        env->set_block(env->context, (uint16_t)(value + 16 * i), hatch->y + i, hatch->x);
    if (hatch->flashing_timer >= 4) hatch->flashing_timer = 0;
    else hatch->flashing_timer++;
}

void mzm_hatches_update(MzmHatch *hatches, int count, const MzmHatchEnv *env) {
    for (int i = 0; i < count; ++i) {
        MzmHatch *h = &hatches[i];
        if (!h->exists) continue;
        if (h->state != MZM_HATCH_OPENING && h->state != MZM_HATCH_CLOSING) {
            if (h->flashing_timer && h->type == MZM_HATCH_MISSILE) flash(h, env);
            continue;
        }
        if (h->frame == 0) {
            h->adc = 0;
            h->frame++;
        } else if (h->frame == 7) {
            if (h->state == MZM_HATCH_OPENING) {
                h->frame = 4;
                mzm_hatch_update_animation(h, env);
                h->state = MZM_HATCH_OPENED;
                h->frame = 0;
            }
        } else {
            if (h->adc < env->durations[h->frame]) {
                h->adc++;
                continue;
            }
            h->adc = 0;
            mzm_hatch_update_animation(h, env);
            h->frame++;
            if (h->frame == 5) {
                h->state = h->state == MZM_HATCH_OPENING ? MZM_HATCH_OPENED : MZM_HATCH_CLOSED;
                h->frame = 0;
            }
        }
    }
}
