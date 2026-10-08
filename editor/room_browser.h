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
#endif
