/* SPDX-License-Identifier: GPL-3.0-only */
#include "native_workspace.h"
#include <gtk/gtk.h>
#include <glib/gstdio.h>

static NativeWorkspace *new_workspace(GtkWidget **center_out, GtkWidget **right_out)
{
    GtkWidget *center = gtk_notebook_new();
    GtkWidget *right = gtk_notebook_new();
    NativeWorkspace *workspace = native_workspace_new();
    g_assert_nonnull(workspace);
    native_workspace_build(workspace, center, right);
    *center_out = center;
    *right_out = right;
    return workspace;
}

static void test_close_and_reopen(void)
{
    GtkWidget *center, *right;
    NativeWorkspace *workspace = new_workspace(&center, &right);

    g_assert_true(native_workspace_test_add_document(workspace, "Brinstar 022"));
    g_assert_cmpuint(native_workspace_test_document_count(workspace), ==, 1);
    g_assert_true(native_workspace_test_activate_close(workspace, 0));
    g_assert_cmpuint(native_workspace_test_document_count(workspace), ==, 0);
    g_assert_true(native_workspace_test_add_document(workspace, "Brinstar 022"));
    g_assert_true(native_workspace_test_close_document(workspace, 0));

    native_workspace_free(workspace);
}

static void test_close_order_and_unsaved_guard(void)
{
    GtkWidget *center, *right;
    NativeWorkspace *workspace = new_workspace(&center, &right);
    const char *names[] = {
        "Brinstar 022", "Brinstar 027", "Brinstar 032",
        "Brinstar 033", "Brinstar 034"
    };
    for (guint i = 0; i < G_N_ELEMENTS(names); ++i)
        g_assert_true(native_workspace_test_add_document(workspace, names[i]));

    native_workspace_test_set_unsaved(workspace, 2, TRUE);
    g_assert_false(native_workspace_test_close_document(workspace, 2));
    native_workspace_test_set_unsaved(workspace, 2, FALSE);
    g_assert_true(native_workspace_test_close_document(workspace, 2));
    g_assert_true(native_workspace_test_close_document(workspace, 0));
    g_assert_true(native_workspace_test_close_document(workspace, 2));
    g_assert_true(native_workspace_test_close_document(workspace, 1));
    g_assert_true(native_workspace_test_close_document(workspace, 0));
    g_assert_cmpuint(native_workspace_test_document_count(workspace), ==, 0);

    native_workspace_free(workspace);
}

static void test_move_then_close_and_shutdown(void)
{
    GtkWidget *center, *right;
    NativeWorkspace *workspace = new_workspace(&center, &right);
    GtkWidget *other_center = gtk_notebook_new();
    GtkWidget *other_right = gtk_notebook_new();
    GtkWidget *detached_layout = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *detached_window = gtk_window_new();
    gtk_box_append(GTK_BOX(detached_layout), other_center);
    gtk_box_append(GTK_BOX(detached_layout), other_right);
    gtk_window_set_child(GTK_WINDOW(detached_window), detached_layout);

    g_assert_true(native_workspace_test_add_document(workspace, "Brinstar 022"));
    g_assert_true(native_workspace_test_add_document(workspace, "Brinstar 027"));
    g_assert_true(native_workspace_test_add_document(workspace, "Brinstar 032"));
    g_assert_true(native_workspace_test_add_document(workspace, "Brinstar 033"));
    g_assert_true(native_workspace_test_move_document(workspace, 0,
                                                       other_center, other_right));
    g_assert_true(native_workspace_test_close_document(workspace, 0));
    g_assert_cmpuint(native_workspace_test_document_count(workspace), ==, 3);

    /* Shutdown must detach every document from all notebook owners. */
    native_workspace_free(workspace);
    g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(center)), ==, 0);
    g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(right)), ==, 0);
    g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(other_center)), ==, 0);
    g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(other_right)), ==, 0);
    gtk_window_destroy(GTK_WINDOW(detached_window));
}

static void test_save_modified_then_close(void)
{
    GtkWidget *center, *right;
    NativeWorkspace *workspace = new_workspace(&center, &right);
    gchar *directory = g_dir_make_tmp("fusion-native-workspace-XXXXXX", NULL);
    g_assert_nonnull(directory);
    gchar *path = g_build_filename(directory, "saved.mvnative", NULL);

    g_assert_true(native_workspace_test_add_document(workspace, "Brinstar 022"));
    g_assert_true(native_workspace_test_prepare_modified(workspace, 0, path));
    g_assert_false(native_workspace_test_close_document(workspace, 0));
    native_workspace_test_activate_save(workspace, 0);
    g_assert_true(g_file_test(path, G_FILE_TEST_IS_REGULAR));
    g_assert_true(native_workspace_test_close_document(workspace, 0));

    native_workspace_free(workspace);
    g_assert_cmpint(g_remove(path), ==, 0);
    g_assert_cmpint(g_rmdir(directory), ==, 0);
    g_free(path);
    g_free(directory);
}

/* Closing an edited document must offer Save, Discard and Cancel,
 * never silently refuse to close or silently throw away changes. */
static void test_unsaved_close_dialog(void)
{
    GtkWidget *center, *right;
    NativeWorkspace *workspace = new_workspace(&center, &right);
    gchar *directory = g_dir_make_tmp("fusion-native-confirm-XXXXXX", NULL);
    g_assert_nonnull(directory);
    gchar *path = g_build_filename(directory, "aria_room.mvnative", NULL);
    g_assert_true(native_workspace_test_add_document(workspace, "Aria 00:010"));
    g_assert_true(native_workspace_test_prepare_modified(workspace, 0, path));

    /* Cancel keeps the document open and modified. */
    g_assert_true(native_workspace_test_activate_close(workspace, 0));
    g_assert_cmpuint(native_workspace_test_document_count(workspace), ==, 1);
    g_assert_true(native_workspace_test_choose_close(workspace, 0, 0));
    g_assert_cmpuint(native_workspace_test_document_count(workspace), ==, 1);

    /* Save and close persists the local override, then closes. */
    g_assert_true(native_workspace_test_activate_close(workspace, 0));
    g_assert_true(native_workspace_test_choose_close(workspace, 0, 2));
    g_assert_true(g_file_test(path, G_FILE_TEST_IS_REGULAR));
    g_assert_cmpuint(native_workspace_test_document_count(workspace), ==, 0);

    /* Discard does not overwrite the existing saved file. */
    g_assert_true(native_workspace_test_add_document(workspace, "Brinstar 033"));
    g_assert_true(native_workspace_test_prepare_modified(workspace, 0, path));
    g_assert_true(native_workspace_test_activate_close(workspace, 0));
    g_assert_true(native_workspace_test_choose_close(workspace, 0, 1));
    g_assert_cmpuint(native_workspace_test_document_count(workspace), ==, 0);

    native_workspace_free(workspace);
    g_assert_cmpint(g_remove(path), ==, 0);
    g_assert_cmpint(g_rmdir(directory), ==, 0);
    g_free(path);
    g_free(directory);
}


/* An external GTK notebook removal must not leave dangling page pointers. */
static void test_external_page_removal_then_shutdown(void)
{
    GtkWidget *center, *right;
    NativeWorkspace *workspace = new_workspace(&center, &right);
    g_assert_true(native_workspace_test_add_document(workspace, "Brinstar 022"));
    gtk_notebook_remove_page(GTK_NOTEBOOK(center), 0);
    gtk_notebook_remove_page(GTK_NOTEBOOK(right), 0);
    native_workspace_free(workspace);
}

/* Repeated click closes and reopen must never retain dead GtkLabel pointers. */
static void test_repeated_close_and_reopen(void)
{
    GtkWidget *center, *right;
    NativeWorkspace *workspace = new_workspace(&center, &right);
    for (unsigned i = 0; i < 24; ++i) {
        g_assert_true(native_workspace_test_add_document(workspace, "Brinstar 022"));
        g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(center)), ==, 1);
        g_assert_true(native_workspace_test_activate_close(workspace, 0));
        g_assert_cmpuint(native_workspace_test_document_count(workspace), ==, 0);
        g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(center)), ==, 0);
        g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(right)), ==, 0);
    }
    native_workspace_free(workspace);
}

/* Cancel an open modal by destroying its parent document/manager. */
static void test_shutdown_with_unsaved_dialog(void)
{
    GtkWidget *center, *right;
    NativeWorkspace *workspace = new_workspace(&center, &right);
    native_workspace_test_add_document(workspace, "Aria 00:010");
    native_workspace_test_set_unsaved(workspace, 0, TRUE);
    g_assert_true(native_workspace_test_activate_close(workspace, 0));
    g_assert_cmpuint(native_workspace_test_document_count(workspace), ==, 1);
    native_workspace_free(workspace);
    g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(center)), ==, 0);
    g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(right)), ==, 0);
    while (g_main_context_pending(NULL)) g_main_context_iteration(NULL, FALSE);
}

static gboolean stop_loop(gpointer userdata)
{
    g_main_loop_quit(userdata);
    return G_SOURCE_REMOVE;
}

static void test_close_during_import(void)
{
    GtkWidget *center, *right;
    NativeWorkspace *workspace = new_workspace(&center, &right);
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    native_workspace_import_async(workspace, "Brinstar", 22);
    g_assert_cmpuint(native_workspace_test_document_count(workspace), ==, 1);
    g_assert_true(native_workspace_test_close_document(workspace, 0));
    g_assert_cmpuint(native_workspace_test_document_count(workspace), ==, 0);

    /* Let the cancelled GSubprocess callback release its final document ref. */
    g_timeout_add(1000, stop_loop, loop);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);
    native_workspace_free(workspace);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    /* GTK_A11Y=test is GTK's documented backend for automated tests.
     * The Xvfb session need not connect to the user's AT-SPI bus. */
    g_setenv("GTK_A11Y", "test", TRUE);
    gtk_init();
    g_test_add_func("/native-workspace/close-and-reopen", test_close_and_reopen);
    g_test_add_func("/native-workspace/close-order-and-unsaved-guard",
                    test_close_order_and_unsaved_guard);
    g_test_add_func("/native-workspace/move-then-close-and-shutdown",
                    test_move_then_close_and_shutdown);
    g_test_add_func("/native-workspace/save-modified-then-close",
                    test_save_modified_then_close);
    g_test_add_func("/native-workspace/close-during-import",
                    test_close_during_import);
    g_test_add_func("/native-workspace/unsaved-close-dialog",
                    test_unsaved_close_dialog);
    g_test_add_func("/native-workspace/external-page-removal",
                    test_external_page_removal_then_shutdown);
    g_test_add_func("/native-workspace/repeated-close-reopen",
                    test_repeated_close_and_reopen);
    g_test_add_func("/native-workspace/shutdown-with-modal",
                    test_shutdown_with_unsaved_dialog);
    return g_test_run();
}
