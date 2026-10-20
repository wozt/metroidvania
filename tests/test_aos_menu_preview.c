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
    return 0;
}
