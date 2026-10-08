#ifndef FUSION_NATIVE_WORKSPACE_H
#define FUSION_NATIVE_WORKSPACE_H
#include <gtk/gtk.h>
typedef struct NativeWorkspace NativeWorkspace;
NativeWorkspace *native_workspace_new(void);
void native_workspace_free(NativeWorkspace *w);
void native_workspace_build(NativeWorkspace *w, GtkWidget *center, GtkWidget *right);
void native_workspace_import_async(NativeWorkspace *w, const char *area, unsigned room);
#endif
