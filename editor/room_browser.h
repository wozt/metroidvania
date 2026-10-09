#ifndef FUSION_ROOM_BROWSER_H
#define FUSION_ROOM_BROWSER_H
#include <gtk/gtk.h>
#include "native_workspace.h"

typedef enum {
    ROOM_WORLD_ZERO = 0,
    ROOM_WORLD_ARIA = 1
} RoomWorld;

/* One browser, distinct verified source catalogs and native import adapters. */
GtkWidget *room_browser_build(GtkWidget *center, NativeWorkspace *workspace,
                              RoomWorld world);
#ifdef FUSION_ROOM_BROWSER_TESTING
void room_browser_test_replace_drafts(GtkWidget *page, const gchar *tsv);
#endif
#endif
