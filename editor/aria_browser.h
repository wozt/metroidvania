#ifndef FUSION_ARIA_BROWSER_H
#define FUSION_ARIA_BROWSER_H
#include <gtk/gtk.h>
#include "native_workspace.h"
/* Native Aria browser: diagnostics and editable project-owned workroom tabs. */
GtkWidget *aria_browser_build(GtkWidget *center, NativeWorkspace *workspace);
#endif
