#ifndef FUSION_WORLD_ATLAS_EDITOR_H
#define FUSION_WORLD_ATLAS_EDITOR_H
#include <gtk/gtk.h>
#include "native_workspace.h"
/* One native minimap-cell browser for both worlds, with private previews. */
GtkWidget *world_atlas_build(GtkWidget *center, NativeWorkspace *workspace,
                              GtkWidget *world_indicator);
#endif
