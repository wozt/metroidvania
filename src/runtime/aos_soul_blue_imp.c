/* SPDX-License-Identifier: GPL-3.0-only */
/* SkillImpUse (see aos_soul_blue_imp.h). */
#include "aos_soul_blue_imp.h"

#include "aos_audio.h"
#include "aos_math.h"

/* Sqrt (BIOS call 0x08): the integer square root, rounded down. */
static uint32_t bios_sqrt(uint32_t x) {
    uint32_t r = 0;
    for (uint32_t bit = 1u << 30; bit; bit >>= 2) {
        if (x >= r + bit) {
            x -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
    }
    return r;
}

/* A 16.16 value's whole part rounded toward zero, as the halfword reads of
 * sub_0806E43C compute it. */
static int32_t whole_toward_zero(int32_t v) { return v < 0 ? -((-v) >> 16) : v >> 16; }

/* sub_0806D3D8 with no acceleration: vx and vy capped at `cap` (the
 * accelerations being 0, only the positive side is clamped). */
static void imp_move(AosImpGuardian *imp, int32_t cap) {
    if (imp->vx > cap) imp->vx = cap;
    if (imp->vy > cap) imp->vy = cap;
    imp->x += imp->vx;
    imp->y += imp->vy;
}

/* sub_0806E43C(e, tx, ty, shift): a step of 1 / 2^shift of the distance
 * toward the point (sub_08068CDC), vx and vy the motion; returns the step. */
static uint32_t imp_seek(AosImpGuardian *imp, int32_t tx, int32_t ty, int shift) {
    int32_t ox = imp->x, oy = imp->y;
    int32_t dx = (ox - tx) >> 16, dy = (oy - ty) >> 16;
    uint32_t distance = bios_sqrt((uint32_t)(dx * dx + dy * dy)) << 16;
    int16_t ax = (int16_t)(whole_toward_zero(tx) - (int16_t)(imp->x >> 16));
    int16_t ay = (int16_t)(whole_toward_zero(ty) - (int16_t)(imp->y >> 16));
    uint16_t angle = aos_arctan2(ax, ay);
    uint32_t step = distance >> shift;
    int32_t s = (int32_t)step >> 8;
    imp->x += s * (aos_sine((uint32_t)angle + 0x4000u) >> 8);
    imp->y += s * (aos_sine(angle) >> 8);
    imp->vx = imp->x - ox;
    imp->vy = imp->y - oy;
    return step;
}

/* sub_0806E3F8: the whole-pixel distance to a point, 16.16. */
static int32_t imp_distance(const AosImpGuardian *imp, int32_t tx, int32_t ty) {
    int32_t dx = tx - imp->x, dy = ty - imp->y;
    dx = dx < 0 ? -((-dx) >> 16) : dx >> 16;
    dy = dy < 0 ? -((-dy) >> 16) : dy >> 16;
    return (int32_t)(bios_sqrt((uint32_t)(dx * dx + dy * dy)) << 16);
}

bool aos_imp_guardian_update(AosImpGuardian *imp, AosGuardian *guardian, AosStats *stats,
                             const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                             uint16_t held, uint16_t pressed, uint32_t *soma_flags,
                             int32_t soma_x, int32_t soma_y, bool soma_facing_left,
                             const AosSkillTarget targets[AOS_IMP_TARGETS],
                             uint32_t (*random)(void)) {
    /* sub_0806CD38 / sub_0806CD78: Soma (gEwramData + 0x13168), 40 above. */
    int32_t px = soma_x, py = soma_y - 0x280000;
    if (guardian->state < AOS_IMP_LEAVING &&
        aos_guardian_upkeep(guardian, stats, records, held, pressed)) {
        guardian->state = AOS_IMP_LEAVING;
        imp->blinking = true;
        imp->timer = 0x10;
    }
    switch (guardian->state) {
    case 0:
        *imp = (AosImpGuardian){
            .mirrored = soma_facing_left, .target = -1,
            .x = (int32_t)((uint32_t)(uint16_t)(soma_x >> 16) << 16),
            .y = (int32_t)((uint32_t)(uint16_t)((soma_y >> 16) - 0x10) << 16),
            .vy = (int32_t)0xFFFFC000};
        guardian->state = AOS_IMP_ENTERING;
        aos_audio_play_song(0x19E);
        /* fall through */
    case AOS_IMP_ENTERING:
        imp_move(imp, 0x80000);
        if (imp->timer == 0) guardian->state = AOS_IMP_FOLLOWING;
        else imp->timer--;
        break;
    case AOS_IMP_FOLLOWING: {
        if ((random() & 0xFF) == 0) {
            guardian->state = AOS_IMP_SEARCHING;
            break;
        }
        uint32_t step = imp_seek(imp, px, py, 5);
        imp->mirrored = imp->vx >= 0;
        if (step <= 0x7FFF) guardian->state = AOS_IMP_HOVERING;
        break;
    }
    case AOS_IMP_HOVERING:
        if ((random() & 0xFF) <= 1) {
            guardian->state = AOS_IMP_SEARCHING;
            break;
        }
        imp->timer++;
        imp->vx = aos_sine((uint32_t)imp->timer << 8) / 4;
        imp->vy = aos_sine(((uint32_t)imp->timer << 9) + 0xC000u) / 4;
        imp_move(imp, 0x20000);
        if (imp_distance(imp, px, py) > 0x300000) guardian->state = AOS_IMP_FOLLOWING;
        break;
    case AOS_IMP_SEARCHING: {
        uint32_t best = 0xFFFFFFFFu;
        imp->target = -1;
        for (int k = 0; k < AOS_IMP_TARGETS; ++k) {
            if (!targets[k].present) continue;
            int32_t dx = (int16_t)(targets[k].x >> 16) - (int16_t)(imp->x >> 16);
            int32_t dy = (int16_t)(targets[k].y >> 16) - (int16_t)(imp->y >> 16);
            uint32_t d = (uint32_t)(dx * dx + dy * dy);
            if (best > d) {
                best = d;
                imp->target = k;
            }
        }
        if (imp->target < 0) {
            guardian->state = AOS_IMP_FOLLOWING;
            imp->timer = 0x80;
            break;
        }
        imp->target_dy = -(int32_t)(((random() & 7) + 0x10) << 16);
        guardian->state = AOS_IMP_DIVING;
    }
        /* fall through */
    case AOS_IMP_DIVING: {
        imp->timer--;
        const AosSkillTarget *target =
            imp->target >= 0 ? &targets[imp->target] : NULL;
        if (!target || !target->present || target->hp <= 0 || imp->timer == 0) {
            guardian->state = AOS_IMP_FOLLOWING;
            break;
        }
        uint32_t step = imp_seek(imp, target->x, target->y + imp->target_dy, 4);
        imp->mirrored = imp->vx >= 0;
        if (step <= 0x3FFF) guardian->state = AOS_IMP_FOLLOWING;
        break;
    }
    case AOS_IMP_LEAVING:
        if (imp->timer == 0) {
            aos_guardian_end(guardian, soma_flags);
            return false;
        }
        imp->timer--;
        break;
    default:
        break;
    }
    return true;
}
