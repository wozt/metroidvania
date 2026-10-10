/* SPDX-License-Identifier: GPL-3.0-only */
#include "debug_menu.h"

#include <stdio.h>
#include <string.h>

/* GBA KEYINPUT bits (gba_input.h), repeated to keep this module SDL-free. */
enum {
    KEY_A = 0x001, KEY_B = 0x002, KEY_RIGHT = 0x010, KEY_LEFT = 0x020,
    KEY_UP = 0x040, KEY_DOWN = 0x080, KEY_R = 0x100, KEY_L = 0x200,
};

void debug_menu_clear(DebugMenu *menu) {
    bool open = menu->open;
    memset(menu, 0, sizeof *menu);
    menu->open = open;
}

static DebugItem *add(DebugMenu *menu, DebugItemKind kind, const char *label, int id) {
    if (menu->count >= DEBUG_MENU_MAX_ITEMS) return NULL;
    DebugItem *item = &menu->items[menu->count++];
    *item = (DebugItem){.kind = kind, .label = label, .id = id};
    return item;
}

bool debug_menu_action(DebugMenu *menu, const char *label, int id) {
    return add(menu, DEBUG_ITEM_ACTION, label, id) != NULL;
}

bool debug_menu_toggle(DebugMenu *menu, const char *label, bool *flag, int id) {
    DebugItem *item = add(menu, DEBUG_ITEM_TOGGLE, label, id);
    if (item) item->flag = flag;
    return item != NULL;
}

bool debug_menu_value(DebugMenu *menu, const char *label, int *value, int min, int max,
                      int step, const char *const *names, int id) {
    DebugItem *item = add(menu, DEBUG_ITEM_VALUE, label, id);
    if (!item) return false;
    item->value = value;
    item->min = min;
    item->max = max;
    item->step = step > 0 ? step : 1;
    item->names = names;
    return true;
}

static int change(DebugItem *item, int direction, int multiplier) {
    if (item->kind == DEBUG_ITEM_TOGGLE) {
        *item->flag = !*item->flag;
        return item->id;
    }
    if (item->kind != DEBUG_ITEM_VALUE) return 0;
    long next = (long)*item->value + (long)direction * item->step * multiplier;
    if (next < item->min) next = item->min;
    if (next > item->max) next = item->max;
    if (next == *item->value) return 0;
    *item->value = (int)next;
    return item->id;
}

int debug_menu_input(DebugMenu *menu, uint16_t pressed) {
    if (!menu->open || menu->count == 0) return 0;
    if (pressed & KEY_B) {
        menu->open = false;
        return 0;
    }
    if (pressed & KEY_UP) menu->cursor = (menu->cursor + menu->count - 1) % menu->count;
    if (pressed & KEY_DOWN) menu->cursor = (menu->cursor + 1) % menu->count;
    DebugItem *item = &menu->items[menu->cursor];
    if (pressed & KEY_A) {
        if (item->kind == DEBUG_ITEM_ACTION) return item->id;
        if (item->kind == DEBUG_ITEM_TOGGLE) return change(item, 1, 1);
    }
    if (pressed & KEY_RIGHT) return change(item, 1, 1);
    if (pressed & KEY_LEFT) return change(item, -1, 1);
    if (pressed & KEY_R) return change(item, 1, 10);
    if (pressed & KEY_L) return change(item, -1, 10);
    return 0;
}

void debug_menu_line(const DebugMenu *menu, int index, char *out, size_t size) {
    if (index < 0 || index >= menu->count) {
        if (size) out[0] = '\0';
        return;
    }
    const DebugItem *item = &menu->items[index];
    const char *mark = index == menu->cursor ? ">" : " ";
    switch (item->kind) {
    case DEBUG_ITEM_TOGGLE:
        snprintf(out, size, "%s %-18s %s", mark, item->label, *item->flag ? "on" : "off");
        break;
    case DEBUG_ITEM_VALUE:
        if (item->names)
            snprintf(out, size, "%s %-18s %s", mark, item->label,
                     item->names[*item->value - item->min]);
        else
            snprintf(out, size, "%s %-18s %d", mark, item->label, *item->value);
        break;
    default:
        snprintf(out, size, "%s %s", mark, item->label);
        break;
    }
}
