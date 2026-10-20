/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_title_stages.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    AosTitleStages s = {.stage=4, .motion=0x4000, .x=0x980000, .y=0x700000};
    aos_title_stage4(&s);
    assert(s.motion == 0x2000 && s.x == 0x97e000 && s.y == 0x6fe000);
    assert(s.affine_scale == 0x108 && s.stage == 4);
    aos_title_stage4(&s);
    assert(s.motion == 0 && s.stage == 4);
    int32_t x = s.x, y = s.y;
    aos_title_stage4(&s);
    assert(s.x == x && s.y == y);
    s.flags = 2;
    s.stage_timer = 5;
    aos_title_stage4(&s);
    assert(s.stage == 5 && s.stage_timer == 0);
    assert(s.progress == 1 && s.affine_scale == 0x118);
    assert(s.entity_53e_1 == 2);
    for (int i = 1; i < 64; ++i) {
        aos_title_stage5(&s);
        assert(s.stage == 5);
    }
    assert(s.progress == 64 && s.stage == 5);
    aos_title_stage5(&s);
    assert(s.stage == 6 && s.other_timer == 0x88);
    assert(s.alpha == 16 && (s.other_flags & 1));
    assert(s.affine_scale == 0x118 - 64);
    aos_title_stage5(&s);
    assert(s.stage == 6 && s.other_timer == 0x88);
    puts("Aria title stages 4-5 passed");
    return 0;
}
