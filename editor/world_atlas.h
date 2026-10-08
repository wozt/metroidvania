#ifndef FUSION_WORLD_ATLAS_EDITOR_H
#define FUSION_WORLD_ATLAS_EDITOR_H
#include <gtk/gtk.h>
#include "native_workspace.h"
/* Adds a scrollable original-MZM minimap/door graph as a separate dock tab. */
GtkWidget *world_atlas_build(GtkWidget *center, NativeWorkspace *workspace);
#endif
