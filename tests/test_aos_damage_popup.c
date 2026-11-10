/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos/core/aos_damage_popup.h"

#include <assert.h>
#include <stdio.h>

static void value_tests(void) {
    AosDamagePopup popup = aos_damage_popup_create(100 << 16, 80 << 16, 123, 0);
    assert(popup.active && !popup.guard && popup.x == 100 << 16 && popup.y == 80 << 16);
    for (int i = 0; i < 8; ++i) assert(popup.glyphs[i] == AOS_DAMAGE_POPUP_BLANK);
    assert(popup.glyphs[8] == 1 && popup.glyphs[9] == 2 && popup.glyphs[10] == 3);

    popup = aos_damage_popup_create(0, 0, -42, 0);
    assert(popup.glyphs[0] == AOS_DAMAGE_POPUP_MINUS);
    assert(popup.glyphs[9] == 4 && popup.glyphs[10] == 2);

    popup = aos_damage_popup_create(0, 20 << 16, 7, 1);
    assert(popup.y == 12 << 16 && popup.glyphs[0] == AOS_DAMAGE_POPUP_PLUS &&
           popup.glyphs[10] == 7);

    popup = aos_damage_popup_create(0, 0, 0, 0);
    assert(popup.guard);
    for (int i = 0; i < AOS_DAMAGE_POPUP_DIGITS; ++i)
        assert(popup.glyphs[i] == AOS_DAMAGE_POPUP_BLANK);
}

static void lifetime_tests(void) {
    AosDamagePopup popup = aos_damage_popup_create(0, 10 << 16, 12, 0);
    for (int frame = 0; frame < 21; ++frame) {
        assert(aos_damage_popup_update(&popup));
        assert(!popup.hidden);
    }
    assert(popup.age == 21 && popup.y == (10 << 16) - 21 * 0x8000);
    assert(aos_damage_popup_update(&popup) && popup.hidden);
    assert(aos_damage_popup_update(&popup) && !popup.hidden);
    while (popup.active) aos_damage_popup_update(&popup);
    assert(popup.age == 28);

    popup = aos_damage_popup_create(0, 0, 1, 2);
    while (popup.active) aos_damage_popup_update(&popup);
    assert(popup.age == 40 && popup.y == -8 * 0x10000 - 40 * 0x8000);

    popup = aos_damage_popup_create(0, 20 << 16, 1, 3);
    assert(aos_damage_popup_update(&popup) && popup.age == 0 && popup.y == 20 << 16);
}

int main(void) {
    value_tests();
    lifetime_tests();
    puts("aos_damage_popup: ok");
    return 0;
}
