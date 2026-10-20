/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_menu_preview.h"
#include <assert.h>
int main(void) {
    AosMenuPreview m = {0}; aos_menu_preview_init(&m);
    aos_menu_preview_press(&m, 128); assert(m.screen == AOS_PREVIEW_TITLE);
    aos_menu_preview_press(&m, 8); assert(m.screen == AOS_PREVIEW_MENU);
    aos_menu_preview_press(&m, 64); assert(m.selected == 3);
    aos_menu_preview_press(&m, 128); assert(m.selected == 0);
    aos_menu_preview_press(&m, 128); assert(m.selected == 1);
    aos_menu_preview_press(&m, 1);
    assert(m.screen == AOS_PREVIEW_NOTICE && m.notice_kind == 1);
    aos_menu_preview_press(&m, 2); assert(m.screen == AOS_PREVIEW_MENU);
    aos_menu_preview_press(&m, 2); assert(m.screen == AOS_PREVIEW_TITLE);
    aos_menu_preview_press(&m, 8);
    m.selected = 2;
    aos_menu_preview_press(&m, 1);
    assert(m.screen == AOS_PREVIEW_NOTICE && m.notice_kind == 2);
    assert(m.show_background);
    aos_menu_preview_press(&m, 1);
    assert(!m.show_background);
    aos_menu_preview_press(&m, 1);
    assert(m.show_background);
    aos_menu_preview_press(&m, 2);
    assert(m.screen == AOS_PREVIEW_MENU);
    m.selected = 0;
    aos_menu_preview_tick(&m, 128, 128);
    assert(m.selected == 1);
    for (int i = 0; i < 31; ++i) aos_menu_preview_tick(&m, 128, 0);
    assert(m.selected == 1);
    aos_menu_preview_tick(&m, 128, 0);
    assert(m.selected == 2);
    aos_menu_preview_tick(&m, 0, 0);
    assert(m.repeat_delay == 0);
    return 0;
}
