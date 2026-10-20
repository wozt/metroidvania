/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_MENU_PREVIEW_H
#define AOS_MENU_PREVIEW_H
#include <stdint.h>
#include <stdbool.h>
typedef enum { AOS_PREVIEW_TITLE, AOS_PREVIEW_MENU, AOS_PREVIEW_NOTICE } AosPreviewScreen;
typedef struct { AosPreviewScreen screen; unsigned selected; unsigned notice_kind; bool show_background; unsigned repeat_delay; } AosMenuPreview;
void aos_menu_preview_init(AosMenuPreview *state);
void aos_menu_preview_press(AosMenuPreview *state, uint16_t pressed);
void aos_menu_preview_tick(AosMenuPreview *state, uint16_t held, uint16_t edges);
#endif
