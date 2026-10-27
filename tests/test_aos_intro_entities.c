/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_intro_entities.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    AosIntroEntities group;
    aos_intro_entities_init(&group);
    for (unsigned index = 0; index < AOS_INTRO_ENTITY_COUNT; ++index)
        assert(group.entities[index].alive);

    aos_intro_entities_update(&group, 1, 0x1600000, 0, 0x0C90000);
    assert(group.entities[AOS_INTRO_ENTITY_MAIN].x == 0x780000);
    assert(group.entities[AOS_INTRO_ENTITY_SCROLL_FOLLOWER].x == 0x780000);
    assert(group.entities[AOS_INTRO_ENTITY_SCROLL_FOLLOWER].y == 0x9E0000);
    assert(!group.entities[AOS_INTRO_ENTITY_AFFINE_RISE].visible);
    assert(group.entities[AOS_INTRO_ENTITY_ORBIT].affine);
    assert(group.entities[AOS_INTRO_ENTITY_PANEL_LEFT].double_draw &&
           group.entities[AOS_INTRO_ENTITY_PANEL_LEFT].x == 0xF7F000);
    assert(group.entities[AOS_INTRO_ENTITY_PANEL_RIGHT].x == -0x7F000);
    assert(group.alpha_override_valid && group.alpha_override == 0x060A);

    /* The main actor advances BG0 every five frames and reaches the first
     * 12-pixel text-script boundary on frame 40. */
    aos_intro_entities_init(&group);
    for (unsigned frame = 1; frame <= 40; ++frame) {
        aos_intro_entities_update(&group, 3, 0x1600000, 0, 0x0C90000);
        assert(group.text_step_requested == (frame == 40));
    }
    assert(group.bg0_vertical == -0x9C && group.text_draw_column == 0 &&
           group.text_column == 1);

    /* The follower is removed under 32 pixels of BG1 scroll, but retains
     * the native final relative-position update of its frame. */
    AosIntroEntity *follower =
        &group.entities[AOS_INTRO_ENTITY_SCROLL_FOLLOWER];
    int32_t follower_y = follower->y;
    aos_intro_entities_update(&group, 3, 0x1FFFFF, -0x1C00, 0x100000);
    assert(!follower->alive && follower->y == follower_y + 0x1C00);

    /* The affine-rise actor appears at BG1 <= 30, eases its velocity by
     * 79/80 and consumes all 256 scale steps before deletion. */
    aos_intro_entities_init(&group);
    AosIntroEntity *rise = &group.entities[AOS_INTRO_ENTITY_AFFINE_RISE];
    aos_intro_entities_update(&group, 3, 0x1E0000, -0x1C00, 0x100000);
    assert(rise->visible && rise->state == 2 && group.affine_sound_requested);
    assert(rise->vy == -88985 && rise->scale == 0x1FE00);
    for (unsigned frame = 1; frame < 256; ++frame)
        aos_intro_entities_update(&group, 3, 0x1E0000, 0, 0x100000);
    assert(rise->alive && rise->scale == 0);
    aos_intro_entities_update(&group, 3, 0x1E0000, 0, 0x100000);
    assert(!rise->alive);

    /* Both panels enter their 288-frame exit when BG1 passes 32. The left
     * panel owns the native 11..16 alpha override during that countdown. */
    aos_intro_entities_init(&group);
    AosIntroEntity *left = &group.entities[AOS_INTRO_ENTITY_PANEL_LEFT];
    aos_intro_entities_update(&group, 3, 0x1FFFFF, 0, 0);
    assert(left->state == 2 && left->timer == 0x120);
    aos_intro_entities_update(&group, 3, 0x1FFFFF, 0, 0);
    assert(left->timer == 0x11F && group.alpha_override == 0x050B);
    for (unsigned frame = 1; frame < 288; ++frame)
        aos_intro_entities_update(&group, 3, 0x1FFFFF, 0, 0);
    assert(left->alive && left->timer == 0);
    aos_intro_entities_update(&group, 3, 0x1FFFFF, 0, 0);
    assert(!left->alive);

    aos_intro_entities_clear(&group);
    assert(group.graphics_release_requested);
    for (unsigned index = 0; index < AOS_INTRO_ENTITY_COUNT; ++index)
        assert(!group.entities[index].alive);

    puts("Aria introduction entity tests passed");
    return 0;
}
