/* SPDX-License-Identifier: GPL-3.0-only */
/* SkillBuerUse and its flames sub_0802FDE4 (see aos_soul.h). */
#include "../../aos_audio.h"
#include "../../aos/math/aos_math.h"
#include "../souls/aos_soul.h"

bool aos_buer_guardian_update(AosBuerGuardian *buer, AosGuardian *guardian, AosStats *stats,
                              const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                              uint16_t held, uint16_t pressed, uint32_t *soma_flags,
                              int32_t soma_x, int32_t soma_y, bool soma_facing_left) {
    if (guardian->state < 2 && aos_guardian_upkeep(guardian, stats, records, held, pressed))
        guardian->state = 2;
    if (guardian->state == 2) {
        if (buer->radius <= 0) {
            aos_guardian_end(guardian, soma_flags);
            return false;
        }
        buer->radius -= 2;
    } else {
        if (guardian->state == 0) {
            buer->height = (int32_t)0xFFF00000;
            int16_t speed = soma_facing_left ? -0x200 : 0x200;
            buer->flames[0] = (AosBuerFlame){.angle = 0, .speed = speed};
            buer->flames[1] = (AosBuerFlame){.angle = 0x8000, .speed = speed};
            aos_audio_play_song(0x1A5);
            aos_audio_play_song(0x92);
            guardian->state = 1;
        }
        if (buer->radius <= 0x1B) buer->radius++;
        buer->x = (int32_t)((uint32_t)(uint16_t)soma_x << 16) | (buer->x & 0xFFFF);
        buer->y = (int32_t)((uint32_t)(uint16_t)(soma_y + (buer->height >> 16)) << 16) |
                  (buer->y & 0xFFFF);
        if (*soma_flags & 0x400u) {
            if (buer->height < (int32_t)0xFFF80000) buer->height += 0x4000;
        } else if (buer->height > (int32_t)0xFFEC0000) {
            buer->height -= 0x4000;
        }
    }
    /* sub_0802FDE4: each flame at the parent's radius, then turned. */
    for (int k = 0; k < AOS_BUER_FLAMES; ++k) {
        AosBuerFlame *flame = &buer->flames[k];
        int32_t angle = (int16_t)flame->angle;
        flame->x = buer->x + aos_sine((uint32_t)(angle + 0x4000)) * buer->radius;
        flame->y = buer->y + aos_sine((uint32_t)angle) * buer->radius;
        flame->angle = (uint16_t)(flame->angle + flame->speed);
        flame->timer++;
    }
    return true;
}
