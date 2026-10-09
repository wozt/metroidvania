#ifndef FUSION_WORLD_ATLAS_EDITOR_H
#define FUSION_WORLD_ATLAS_EDITOR_H
#include <gtk/gtk.h>
#include "native_workspace.h"

/* Shared non-persistent room picking. G_MAXUINT means cancelled. */
typedef void (*WorldAtlasRoomPicked)(guint world, guint area, guint room,
                                      gpointer userdata);
gboolean world_atlas_begin_room_pick(GtkWidget *page, guint preferred_world,
                                      WorldAtlasRoomPicked callback, gpointer userdata);
void world_atlas_abandon_room_pick(GtkWidget *page, gpointer userdata);
/* One native minimap-cell browser for both worlds, with private previews. */
GtkWidget *world_atlas_build(GtkWidget *center, NativeWorkspace *workspace,
                              GtkWidget *world_indicator);
#endif
