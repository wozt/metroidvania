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

    /* Stage 6 changes the alpha every eight ticks and enters stage 7
     * one call after the countdown reaches zero, as in the decompilation. */
    AosTitleStages transition = {.stage=6, .other_timer=0x88};
    for (int i = 0; i < 0x88; ++i) {
        aos_title_stage6(&transition);
        assert(transition.stage == 6);
    }
    assert(transition.other_timer == 0 && transition.alpha == 0);
    aos_title_stage6(&transition);
    assert(transition.stage == 7 && transition.show_bg1);
    assert((transition.other_flags & 4) && transition.alpha == 16);
    assert(transition.affine_angle == (0x89 * 0x40));

    transition.reveal_motion = 0x400;
    aos_title_stage7(&transition);
    assert(transition.reveal_motion < 0x400 && transition.stage == 7);
    transition.reveal_motion = 0x37f;
    aos_title_stage7(&transition);
    assert(transition.other_flags & 2);
    transition.reveal_motion = 0xc0;
    aos_title_stage7(&transition);
    assert(transition.reveal_motion == 0);
    aos_title_stage7(&transition);
    assert(transition.stage == 8 && transition.show_bg0);
    assert(transition.idle_timer == 600 && transition.palette_event);
    puts("Aria title stages 4-7 passed");
    return 0;
}
