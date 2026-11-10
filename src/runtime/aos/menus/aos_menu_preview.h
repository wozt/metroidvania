/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_MENU_PREVIEW_H
#define AOS_MENU_PREVIEW_H
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    AOS_PREVIEW_TITLE,
    AOS_PREVIEW_MENU,
    AOS_PREVIEW_FILE_SELECT,
    AOS_PREVIEW_OVERWRITE,
    AOS_PREVIEW_OPTIONS,
} AosPreviewScreen;

typedef enum {
    AOS_PREVIEW_NEW_GAME,
    AOS_PREVIEW_CONTINUE,
} AosPreviewFileMode;

typedef enum {
    AOS_PREVIEW_ACTION_NONE,
    AOS_PREVIEW_ACTION_CREATE,
    AOS_PREVIEW_ACTION_LOAD,
} AosPreviewAction;

typedef struct {
    AosPreviewScreen screen;
    unsigned selected;
    unsigned slot_selected;
    AosPreviewFileMode file_mode;
    AosPreviewAction action;
    bool slot_present[3];
    bool confirm_yes;
    bool show_background;
    unsigned repeat_delay;
} AosMenuPreview;

void aos_menu_preview_init(AosMenuPreview *state);
void aos_menu_preview_set_slot(AosMenuPreview *state, unsigned slot, bool present);
void aos_menu_preview_press(AosMenuPreview *state, uint16_t pressed);
void aos_menu_preview_tick(AosMenuPreview *state, uint16_t held, uint16_t edges);
AosPreviewAction aos_menu_preview_take_action(AosMenuPreview *state, unsigned *slot);
#endif
