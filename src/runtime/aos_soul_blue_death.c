/* SPDX-License-Identifier: GPL-3.0-only */
/* SkillDeathUse (see aos_soul_blue_death.h). */
#include "aos_soul_blue_death.h"

#include "aos_audio.h"
#include "aos_math.h"

/* sub_0802D5EC(n): (random & (n - 1)) - n / 2. */
static int32_t random_centred(uint32_t (*random)(void), uint32_t n) {
    return (int32_t)(random() & (n - 1)) - (int32_t)(n >> 1);
}

static int32_t whole_toward_zero(int32_t v) { return v < 0 ? -((-v) >> 16) : v >> 16; }

/* sub_0802D408(e, tx, ty, heading, rate): one turn step toward the target
 * along the shorter way. */
static uint16_t turn_toward(const AosDeathSickle *s, int32_t tx, int32_t ty, int16_t heading,
                            int rate) {
    int16_t dx = (int16_t)(whole_toward_zero(tx) - (int16_t)(s->x >> 16));
    int16_t dy = (int16_t)(whole_toward_zero(ty) - (int16_t)(s->y >> 16));
    int32_t difference = (int32_t)aos_arctan2(dx, dy) - heading;
    bool up = difference > 0 ? difference <= 0x7FFF : difference < -0x8000;
    return (uint16_t)(heading + (up ? rate : -rate));
}

/* The heading's velocity at the speed, then sub_0806D3D8(e, 8.0) without
 * acceleration (only the positive side is clamped). */
static void sickle_move(AosDeathSickle *s) {
    s->vx = (aos_sine((uint32_t)s->heading + 0x4000u) >> 8) * (s->speed >> 8);
    s->vy = (aos_sine(s->heading) >> 8) * (s->speed >> 8);
    if (s->vx > 0x80000) s->vx = 0x80000;
    if (s->vy > 0x80000) s->vy = 0x80000;
    s->x += s->vx;
    s->y += s->vy;
}

static void sickle_update(AosDeathSickle *s, const AosSkillTarget targets[AOS_DEATH_TARGETS],
                          const AosAnimSet *anims, int cam_x, int cam_y, int margin_x,
                          int margin_y, uint32_t (*random)(void)) {
    switch (s->state) {
    case 0:
        aos_anim_start(&s->anim, anims, 0, false);
        s->angle = s->heading = (uint16_t)random();
        s->spin = (int16_t)0xF700;
        s->mirrored = random() & 1;
        if (s->mirrored) s->spin = (int16_t)-s->spin;
        aos_audio_play_song(0x96);
        s->state = 1;
        /* fall through */
    case 1:
        if (s->scale <= 0xFFFF) s->scale += 0x2000;
        s->angle = (uint16_t)(s->angle + s->spin);
        if (aos_anim_step(&s->anim, anims) != 3) break;
        {
            uint32_t best = 0xFFFFFFFFu;
            s->target = -1;
            for (int k = 0; k < AOS_DEATH_TARGETS; ++k) {
                if (!targets[k].present) continue;
                int32_t dx = (int16_t)(targets[k].x >> 16) - (int16_t)(s->x >> 16);
                int32_t dy = (int16_t)(targets[k].y >> 16) - (int16_t)(s->y >> 16);
                uint32_t d = (uint32_t)(dx * dx + dy * dy);
                if (best > d) {
                    best = d;
                    s->target = k;
                }
            }
        }
        s->state = s->target >= 0 ? 2 : 3;
        s->attacking = true;
        s->timer = 0x40;
        break;
    case 2: {
        if (s->speed <= 0x1FFFF) s->speed += 0x2000;
        const AosSkillTarget *t = &targets[s->target];
        s->heading = turn_toward(s, t->x, t->y, (int16_t)s->heading, 0x555);
        s->angle = (uint16_t)(s->angle + s->spin);
        sickle_move(s);
        s->timer--;
        if (!t->present || t->hp <= 0 || s->timer == 0) s->state = 3;
        break;
    }
    case 3: {
        if (s->speed <= 0x1FFFF) s->speed += 0x2000;
        int16_t curve = (int16_t)((s->spin < 0 ? s->spin + 0xF : s->spin) >> 4);
        s->heading = (uint16_t)(s->heading + curve);
        s->angle = (uint16_t)(s->angle + s->spin);
        sickle_move(s);
        break;
    }
    default:
        s->alive = false;
        return;
    }
    /* sub_0806D128(e, 1): beyond the screen widened by margin 1. */
    int sx = (s->x >> 16) - cam_x, sy = (s->y >> 16) - cam_y;
    if (sx < -margin_x || sx > margin_x + 0xF0 || sy < -margin_y || sy > margin_y + 0xA0)
        s->state = 4;
}

bool aos_death_guardian_update(AosDeathGuardian *death, AosGuardian *guardian, AosStats *stats,
                               const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                               uint16_t held, uint16_t pressed, uint32_t *soma_flags,
                               int32_t soma_x, int32_t soma_y, bool soma_facing_left,
                               const AosSkillTarget targets[AOS_DEATH_TARGETS],
                               const AosAnimSet *anims, int cam_x, int cam_y, int margin_x,
                               int margin_y, uint32_t (*random)(void)) {
    if (aos_guardian_upkeep(guardian, stats, records, held, pressed)) {
        for (int k = 0; k < AOS_DEATH_SICKLES; ++k) death->sickles[k].alive = false;
        aos_guardian_end(guardian, soma_flags);
        return false;
    }
    if (guardian->state == 0) {
        death->timer = 8;
        guardian->state = 1;
    }
    death->facing_left = soma_facing_left;
    death->x = soma_x;
    death->y = soma_y - 0x20;
    if (--death->timer == 0) {
        for (int k = 0; k < AOS_DEATH_SICKLES; ++k) {
            if (death->sickles[k].alive) continue;
            int32_t x = death->x + random_centred(random, 0x40);
            int32_t y = death->y + random_centred(random, 0x40);
            death->sickles[k] = (AosDeathSickle){.alive = true, .target = -1,
                                                 .x = (int32_t)((uint32_t)x << 16),
                                                 .y = (int32_t)((uint32_t)y << 16)};
            break;
        }
        death->timer = 0x38;
    }
    for (int k = 0; k < AOS_DEATH_SICKLES; ++k)
        if (death->sickles[k].alive)
            sickle_update(&death->sickles[k], targets, anims, cam_x, cam_y, margin_x, margin_y,
                          random);
    return true;
}
