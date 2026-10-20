/* SPDX-License-Identifier: GPL-3.0-only */
/* Diagnostic navigation, not original Aria GameModeMainMenuUpdate. */
#include "aos_menu_preview.h"
enum { KEY_A=1, KEY_B=2, KEY_START=8, KEY_UP=64, KEY_DOWN=128 };
void aos_menu_preview_init(AosMenuPreview *s) {
    s->screen = AOS_PREVIEW_TITLE; s->selected = 0; s->notice_kind = 0;
}
void aos_menu_preview_press(AosMenuPreview *s, uint16_t keys) {
    if (s->screen == AOS_PREVIEW_TITLE) {
        if (keys & (KEY_A | KEY_START)) { s->screen = AOS_PREVIEW_MENU; s->selected = 0; }
    } else if (s->screen == AOS_PREVIEW_MENU) {
        if (keys & KEY_B) s->screen = AOS_PREVIEW_TITLE;
        else if (keys & KEY_UP) s->selected = (s->selected + 3u) % 4u;
        else if (keys & KEY_DOWN) s->selected = (s->selected + 1u) % 4u;
        else if (keys & (KEY_A | KEY_START)) {
            if (s->selected == 3) s->screen = AOS_PREVIEW_TITLE;
            else { s->notice_kind = s->selected; s->screen = AOS_PREVIEW_NOTICE; }
        }
    } else if (keys & (KEY_A | KEY_B | KEY_START)) s->screen = AOS_PREVIEW_MENU;
}
