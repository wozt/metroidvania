/* SPDX-License-Identifier: GPL-3.0-only */
/* The F1 debug menu shared by both runtimes (no SDL).
 *
 * Items point at the engine's real state: a toggle flips a bool, a value
 * edits an int within bounds, and an action returns its id to the runtime,
 * which performs it (teleport, spawn, frame step...). Navigation uses GBA
 * key edges: Up/Down select (wrapping), Left/Right change a value by its
 * step (L/R by ten steps), A toggles or runs, B closes. Runtimes list only
 * what their engine really implements. */
#ifndef DEBUG_MENU_H
#define DEBUG_MENU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DEBUG_MENU_MAX_ITEMS 48

typedef enum {
    DEBUG_ITEM_ACTION,
    DEBUG_ITEM_TOGGLE,
    DEBUG_ITEM_VALUE,
} DebugItemKind;

typedef struct {
    DebugItemKind kind;
    const char *label;
    int id;                 /* returned when run or changed (0: never) */
    bool *flag;             /* DEBUG_ITEM_TOGGLE */
    int *value;             /* DEBUG_ITEM_VALUE */
    int min, max, step;
    /* Optional names of values (value - min indexes them). */
    const char *const *names;
} DebugItem;

typedef struct {
    DebugItem items[DEBUG_MENU_MAX_ITEMS];
    int count;
    int cursor;
    bool open;
} DebugMenu;

void debug_menu_clear(DebugMenu *menu);
/* Each returns false when the menu is full. */
bool debug_menu_action(DebugMenu *menu, const char *label, int id);
bool debug_menu_toggle(DebugMenu *menu, const char *label, bool *flag, int id);
bool debug_menu_value(DebugMenu *menu, const char *label, int *value, int min, int max,
                      int step, const char *const *names, int id);
/* One frame of input (GBA key edges). Returns the id of the item that was
 * run, toggled or changed, or 0. B closes the menu. */
int debug_menu_input(DebugMenu *menu, uint16_t pressed);
/* "label  value" for item `index`, with a cursor mark. */
void debug_menu_line(const DebugMenu *menu, int index, char *out, size_t size);

#endif /* DEBUG_MENU_H */
