/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_red_student_witch.h"

AosStudentWitch aos_student_witch_create(int32_t soma_x, int32_t soma_y,
                                         bool soma_facing_left) {
    AosStudentWitch witch = {.state = 0};
    witch.body.x = (int32_t)((uint32_t)(int16_t)((soma_x >> 16) +
                                                   (soma_facing_left ? -8 : 8)) << 16);
    witch.body.y = (int32_t)((uint32_t)(int16_t)((soma_y >> 16) - 0x1A) << 16);
    witch.body.mirrored = soma_facing_left;
    return witch;
}

bool aos_student_witch_update(AosStudentWitch *witch, const AosCollision *layer,
                              const AosProbes *probes, bool outside, bool *song) {
    if (song) *song = false;
    /* State 3's dispatch deletes the entity. */
    if (witch->state == 3) return false;
    if (witch->state == 0) {
        witch->state = 1;
    } else if (witch->state == 1) {
        int result = aos_enemy_probe_walk(&witch->body, layer, probes,
                                          -0x20000, 8);
        if ((result & 8) && !witch->contacted) {
            witch->contacted = 1;
            if (song) *song = true;
        }
        if (result & 3) {
            witch->state = 2;
            witch->linger = 0x10;
        }
    } else if (witch->state == 2) {
        if (witch->linger == 0) {
            witch->state = 3;
        } else {
            --witch->linger;
        }
    }
    /* The margin probe forces state 3 (+ 0x58 bit 5, not ported). */
    if (outside) witch->state = 3;
    return true;
}
