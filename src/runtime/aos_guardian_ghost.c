/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_guardian_ghost.h"

#include "aos_audio.h"
#include "aos_enemy.h"

bool aos_ghost_guardian_update(AosGhostGuardian *ghost, AosGuardian *guardian, AosStats *stats,
                               const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                               uint16_t held, uint16_t pressed, uint32_t *soma_flags,
                               int32_t soma_x, int32_t soma_y, bool soma_facing_left,
                               int cam_x, int cam_y) {
    if (guardian->state < 3 && aos_guardian_upkeep(guardian, stats, records, held, pressed)) {
        guardian->state = 3;
        ghost->attacking = false;
    }
    switch (guardian->state) {
    case 0:
        ghost->giant = records[guardian->soul].argument != 0;
        ghost->scale = 0;
        ghost->attacking = false;
        aos_audio_play_song(ghost->giant ? 0x8D : 0x97);
        guardian->state = 1;
        /* fallthrough */
    case 1:
        if (ghost->scale <= 0xFFFF) {
            ghost->scale += 0x2000;
        } else {
            guardian->state = 2;
            ghost->attacking = true;
        }
        break;
    case 3:
        if (ghost->scale <= 0) {
            aos_guardian_end(guardian, soma_flags);
            return false;
        }
        ghost->scale -= 0x2000;
        break;
    default: break;
    }
    ghost->facing_left = soma_facing_left;
    ghost->x = soma_x;
    ghost->y = soma_y;
    /* sub_0802F7E8: each spirit (sound 0x98 when made) flies until beyond
     * screen margin 2 (16 x 16), then frees its slot. Its type-5 sparkle
     * trail (sub_08045B44(x, y, 0, 0, 5, 6)) is not ported. */
    for (int k = 0; k < AOS_GHOST_SHOTS; ++k) {
        AosGhostShot *s = &ghost->shots[k];
        if (!s->alive) continue;
        if (s->state == 2) {
            s->alive = false;
            continue;
        }
        if (s->state == 0) {
            aos_audio_play_song(0x98);
            s->state = 1;
        }
        s->x += s->vx;
        s->y += s->vy;
        int sx = (s->x >> 16) - cam_x, sy = (s->y >> 16) - cam_y;
        if (sx < -16 || sx > 16 + 0xF0 || sy < -16 || sy > 16 + 0xA0) s->state = 2;
    }
    return true;
}

void aos_ghost_guardian_box(const AosGhostGuardian *ghost, int8_t box[4]) {
    if (ghost->giant) {
        box[0] = -24, box[1] = -40, box[2] = 48, box[3] = 40;
    } else {
        box[0] = 16, box[1] = -32, box[2] = 16, box[3] = 32;
    }
}

void aos_ghost_guardian_touch(AosGhostGuardian *ghost, int cx, int cy, uint32_t (*random)(void)) {
    AosGhostShot *s = NULL;
    for (int k = 0; k < AOS_GHOST_SHOTS && !s; ++k)
        if (!ghost->shots[k].alive) s = &ghost->shots[k];
    if (!s) return;
    int dx = (int16_t)(cx - (ghost->x >> 16));
    int dy = (int16_t)(cy - ((ghost->y >> 16) - 0x10) + (int32_t)(random() & 3) - 2);
    uint16_t angle = aos_arctan2((int16_t)dx, (int16_t)dy);
    *s = (AosGhostShot){.alive = true, .x = (int32_t)((uint32_t)cx << 16),
                        .y = (int32_t)((uint32_t)cy << 16),
                        .vx = aos_sine((uint32_t)angle + 0x4000) * 4,
                        .vy = aos_sine(angle) * 4};
}
