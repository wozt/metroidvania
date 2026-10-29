/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "aos_input.h"
#include "aos_soul_menu.h"

int main(void) {
    AosStats stats = {0};
    aos_souls_add(&stats, AOS_SOUL_RED, 1, 1);      /* Bat, native ID 2 */
    aos_souls_add(&stats, AOS_SOUL_RED, 13, 2);     /* Rock Armor, ID 14 */
    aos_souls_add(&stats, AOS_SOUL_BLUE, 3, 1);     /* Buer, ID 4 */
    aos_souls_add(&stats, AOS_SOUL_YELLOW, 5, 1);   /* Peeping Eye, ID 6 */
    aos_souls_add(&stats, AOS_SOUL_ABILITY, 2, 1);  /* Malphas */
    stats.red_soul = 2;

    AosSoulMenu menu = {0};
    assert(aos_soul_menu_update(&menu, &stats, AOS_KEY_START, AOS_KEY_START) ==
           AOS_SOUL_MENU_OPENED);
    assert(menu.open && menu.page == AOS_SOUL_MENU_PAGE_HUB && menu.hub_cursor == 0);
    assert(!strcmp(aos_pause_entry_name(0), "SOUL SET"));
    assert(!strcmp(aos_pause_entry_name(6), "ENEMY"));

    /* The hub wraps exactly seven native entries and exposes unavailable pages. */
    assert(aos_soul_menu_update(&menu, &stats, 0, AOS_KEY_UP) == AOS_SOUL_MENU_MOVED);
    assert(menu.hub_cursor == AOS_PAUSE_ENEMY);
    assert(aos_soul_menu_update(&menu, &stats, AOS_KEY_A, AOS_KEY_A) ==
           (AOS_SOUL_MENU_ACCEPTED | AOS_SOUL_MENU_UNAVAILABLE));
    assert(menu.page == AOS_SOUL_MENU_PAGE_UNAVAILABLE);
    assert(aos_soul_menu_update(&menu, &stats, AOS_KEY_B, AOS_KEY_B) == AOS_SOUL_MENU_BACK);
    assert(menu.page == AOS_SOUL_MENU_PAGE_HUB && menu.open);
    menu.hub_cursor = AOS_PAUSE_SOUL_SET;
    assert(aos_soul_menu_update(&menu, &stats, AOS_KEY_A, AOS_KEY_A) ==
           AOS_SOUL_MENU_ACCEPTED);
    assert(menu.page == AOS_SOUL_MENU_PAGE_SOUL_SET && menu.category == AOS_SOUL_MENU_RED &&
           menu.cursor[0] == 2);
    assert(!strcmp(aos_soul_menu_name(menu.category, menu.cursor[0]), "Bat"));
    assert(aos_soul_menu_quantity(&menu, &stats) == 1 && aos_soul_menu_enabled(&menu, &stats));

    /* Down skips every unowned red soul and reaches Rock Armor. */
    aos_soul_menu_update(&menu, &stats, 0, AOS_KEY_DOWN);
    assert(menu.cursor[0] == 14 && aos_soul_menu_quantity(&menu, &stats) == 2);
    assert(aos_soul_menu_update(&menu, &stats, AOS_KEY_A, AOS_KEY_A) ==
           AOS_SOUL_MENU_CHANGED);
    assert(stats.red_soul == 14 && aos_soul_menu_enabled(&menu, &stats));
    aos_soul_menu_update(&menu, &stats, 0, AOS_KEY_UP);
    assert(menu.cursor[0] == 2);
    aos_soul_menu_update(&menu, &stats, 0, AOS_KEY_UP);
    assert(menu.cursor[0] == 0 && !strcmp(aos_soul_menu_name(0, 0), "None"));
    assert(aos_soul_menu_update(&menu, &stats, AOS_KEY_A, AOS_KEY_A) ==
           AOS_SOUL_MENU_CHANGED && stats.red_soul == 0);

    aos_soul_menu_update(&menu, &stats, AOS_KEY_R, AOS_KEY_R);
    assert(menu.category == AOS_SOUL_MENU_BLUE && menu.cursor[1] == 0);
    aos_soul_menu_update(&menu, &stats, 0, AOS_KEY_DOWN);
    assert(menu.cursor[1] == 4);
    aos_soul_menu_update(&menu, &stats, AOS_KEY_A, AOS_KEY_A);
    assert(stats.blue_soul == 4);
    aos_soul_menu_update(&menu, &stats, AOS_KEY_R, AOS_KEY_R);
    assert(menu.category == AOS_SOUL_MENU_YELLOW && menu.cursor[2] == 0);
    aos_soul_menu_update(&menu, &stats, 0, AOS_KEY_DOWN);
    assert(menu.cursor[2] == 6);
    /* Ability is the hub's separate native entry, not a fourth Soul Set family. */
    assert(aos_soul_menu_update(&menu, &stats, AOS_KEY_B, AOS_KEY_B) == AOS_SOUL_MENU_BACK);
    menu.hub_cursor = AOS_PAUSE_ABILITY;
    assert(aos_soul_menu_update(&menu, &stats, AOS_KEY_A, AOS_KEY_A) ==
           AOS_SOUL_MENU_ACCEPTED);
    assert(menu.page == AOS_SOUL_MENU_PAGE_ABILITY && menu.category == AOS_SOUL_MENU_ABILITY &&
           menu.cursor[3] == 2);
    assert(!aos_soul_menu_enabled(&menu, &stats));
    assert(aos_soul_menu_update(&menu, &stats, AOS_KEY_A, AOS_KEY_A) ==
           AOS_SOUL_MENU_CHANGED && aos_soul_menu_enabled(&menu, &stats));

    assert(aos_soul_menu_update(&menu, &stats, AOS_KEY_B, AOS_KEY_B) == AOS_SOUL_MENU_BACK);
    assert(menu.open && menu.page == AOS_SOUL_MENU_PAGE_HUB);
    assert(aos_soul_menu_update(&menu, &stats, AOS_KEY_START, AOS_KEY_START) ==
           AOS_SOUL_MENU_CLOSED && !menu.open);
    puts("aos soul menu tests passed");
    return 0;
}
