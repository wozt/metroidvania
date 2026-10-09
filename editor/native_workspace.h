#ifndef FUSION_NATIVE_WORKSPACE_H
#define FUSION_NATIVE_WORKSPACE_H
#include <gtk/gtk.h>
typedef struct NativeWorkspace NativeWorkspace;
NativeWorkspace *native_workspace_new(void);
void native_workspace_free(NativeWorkspace *w);
void native_workspace_build(NativeWorkspace *w, GtkWidget *center, GtkWidget *right);
/* Weak pointer: global map owns its GTK widget and must outlive no picker. */
void native_workspace_set_world_atlas(NativeWorkspace *w, GtkWidget *map_page);
void native_workspace_import_async(NativeWorkspace *w, const char *area, unsigned room);
void native_workspace_import_aria_async(NativeWorkspace *w, unsigned area, unsigned room);
#ifdef FUSION_NATIVE_WORKSPACE_TESTING
guint native_workspace_test_load_native_doors(NativeWorkspace *w,
                                             guint index, const char *path);
guint native_workspace_test_document_count(const NativeWorkspace *w);
gboolean native_workspace_test_add_document(NativeWorkspace *w, const char *identity);
gboolean native_workspace_test_close_document(NativeWorkspace *w, guint index);
gboolean native_workspace_test_activate_close(NativeWorkspace *w, guint index);
gboolean native_workspace_test_choose_close(NativeWorkspace *w, guint index, guint choice);
gboolean native_workspace_test_prepare_modified(NativeWorkspace *w, guint index,
                                                 const char *override_path);
void native_workspace_test_activate_save(NativeWorkspace *w, guint index);
void native_workspace_test_set_unsaved(NativeWorkspace *w, guint index, gboolean unsaved);
gboolean native_workspace_test_move_document(NativeWorkspace *w, guint index,
                                              GtkWidget *center, GtkWidget *right);
#endif
#endif
