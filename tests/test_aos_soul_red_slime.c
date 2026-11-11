/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free deterministic tests for SkillSlimeUse and sub_0802C9E8. */
#include "aos/red_souls/aos_soul_red_slime.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

enum { W = 64, H = 32 };
static uint8_t cells[W * H];
static const AosCollision layer = {2, 1, W, H, cells, NULL, false};

int main(void) {
    memset(cells, 0, sizeof cells);
    /* Facing right: angle = (draw & 0x3FF) + 0x1000. */
    AosSlime s = aos_slime_create(100 << 16, 150 << 16, false, 0x200);
    assert(s.body.x == 112 << 16 && s.body.y == (150 - 0x1A) << 16);
    assert(!s.body.mirrored);
    assert(s.angle == 0x1200 && s.speed == 0x300000 && s.life == 0x100);
    assert(s.state == 0);
    /* Mirrored: angle = -0x8000 - angle (as a halfword). */
    AosSlime m = aos_slime_create(100 << 16, 150 << 16, true, 0x200);
    assert(m.body.x == 88 << 16 && m.body.mirrored);
    assert(m.angle == (uint16_t)(-0x8000 - 0x1200));
    /* First update enters state 1; velocities follow the sine fan. */
    bool variant = true;
    assert(aos_slime_update(&s, &layer, &variant));
    assert(s.state == 1 && !variant);
    int32_t vx = s.body.vx, vy = s.body.vy;
    assert(vx != 0 || vy != 0);
    /* The velocity recomputes from the same angle (constant direction)
     * and life counts 0x100 down; the variant swap fires at 0x20. */
    int frames = 0;
    variant = false;
    while (aos_slime_update(&s, &layer, &variant) && frames < 0x200) {
        if (variant) {
            assert(s.life == 0x20 && s.swapped);
            variant = false;
        }
        ++frames;
    }
    assert(!aos_slime_update(&s, &layer, &variant) == false || s.state == 2);
    /* Trail child: nine-entry ring, oldest entry first. */
    AosSlimeTrail trail = {0};
    for (int i = 0; i < 12; ++i)
        aos_slime_trail_push(&trail, (100 + i) << 16, 50 << 16);
    assert(trail.filled);
    int32_t tx = 0, ty = 0;
    aos_slime_trail_pos(&trail, &tx, &ty);
    assert(tx == (103 << 16) && ty == (50 << 16));
    assert(aos_slime_trail_update(&trail, true));
    assert(!aos_slime_trail_update(&trail, false));
    puts("aos_soul_red_slime: ok");
    return 0;
}
