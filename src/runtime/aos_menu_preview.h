/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_MENU_PREVIEW_H
#define AOS_MENU_PREVIEW_H
#include <stdint.h>
typedef enum { AOS_PREVIEW_TITLE, AOS_PREVIEW_MENU, AOS_PREVIEW_NOTICE } AosPreviewScreen;
typedef struct { AosPreviewScreen screen; unsigned selected; unsigned notice_kind; } AosMenuPreview;
void aos_menu_preview_init(AosMenuPreview *state);
void aos_menu_preview_press(AosMenuPreview *state, uint16_t pressed);
#endif
