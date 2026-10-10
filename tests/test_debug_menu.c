/* SPDX-License-Identifier: GPL-3.0-only */
/* Tests for the shared F1 debug menu model. */
#include "debug_menu.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { A = 0x001, B = 0x002, RIGHT = 0x010, LEFT = 0x020, UP = 0x040, DOWN = 0x080,
       R = 0x100, L = 0x200 };

int main(void) {
    static DebugMenu menu;
    bool hitboxes = false;
    int hp = 300, kind = 0;
    static const char *const kinds[] = {"bat", "zombie", "blue_crow"};
    debug_menu_clear(&menu);
    assert(debug_menu_toggle(&menu, "Hitboxes", &hitboxes, 1));
    assert(debug_menu_value(&menu, "HP", &hp, 1, 320, 1, NULL, 2));
    assert(debug_menu_value(&menu, "Enemy", &kind, 0, 2, 1, kinds, 3));
    assert(debug_menu_action(&menu, "Spawn", 4));

    /* Closed: input is ignored. */
    assert(debug_menu_input(&menu, A) == 0 && !hitboxes);
    menu.open = true;
    /* A toggles the flag in place. */
    assert(debug_menu_input(&menu, A) == 1 && hitboxes);
    /* Down, then Right / L / R change the value within its bounds. */
    assert(debug_menu_input(&menu, DOWN) == 0 && menu.cursor == 1);
    assert(debug_menu_input(&menu, RIGHT) == 2 && hp == 301);
    assert(debug_menu_input(&menu, R) == 2 && hp == 311);
    assert(debug_menu_input(&menu, R) == 2 && hp == 320);
    assert(debug_menu_input(&menu, R) == 0 && hp == 320);
    assert(debug_menu_input(&menu, L) == 2 && hp == 310);
    char line[64];
    debug_menu_line(&menu, 1, line, sizeof line);
    assert(strstr(line, "> HP") == line && strstr(line, "310"));
    /* Named values. */
    assert(debug_menu_input(&menu, DOWN | RIGHT) == 3 && kind == 1);
    debug_menu_line(&menu, 2, line, sizeof line);
    assert(strstr(line, "zombie"));
    /* Actions return their id; Up wraps from the first item to the last. */
    menu.cursor = 0;
    assert(debug_menu_input(&menu, UP) == 0 && menu.cursor == 3);
    assert(debug_menu_input(&menu, A) == 4);
    assert(debug_menu_input(&menu, LEFT) == 0);
    debug_menu_line(&menu, 0, line, sizeof line);
    assert(strstr(line, "on") && line[0] == ' ');
    /* B closes it; clearing keeps it closed or open. */
    assert(debug_menu_input(&menu, B) == 0 && !menu.open);
    debug_menu_clear(&menu);
    assert(menu.count == 0 && !menu.open);
    for (int i = 0; i < DEBUG_MENU_MAX_ITEMS; ++i) assert(debug_menu_action(&menu, "x", i + 1));
    assert(!debug_menu_action(&menu, "overflow", 99));
    puts("debug_menu: ok");
    return 0;
}
