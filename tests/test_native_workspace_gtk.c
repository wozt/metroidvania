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

/* Find real category toggle widgets; this does not inspect source text. */
static GtkWidget *find_room_toggle(GtkWidget *root, const char *label)
{
    /* PATCH_0113B_SEMANTIC_TOOLBAR_NAMES: icon-only GTK4 toggles no longer
     * expose a GtkButton text label. Prefer their stable semantic role,
     * keeping the former text-label lookup for old room controls. */
    if (GTK_IS_TOGGLE_BUTTON(root) &&
        (g_strcmp0(g_object_get_data(G_OBJECT(root), "mv-room-toggle-name"),
                   label) == 0 ||
         g_strcmp0(g_object_get_data(G_OBJECT(root), "mv-room-tool-name"),
                   label) == 0 ||
         g_strcmp0(gtk_button_get_label(GTK_BUTTON(root)), label) == 0))
        return root;
    for (GtkWidget *child = gtk_widget_get_first_child(root); child;
         child = gtk_widget_get_next_sibling(child)) {
        GtkWidget *found = find_room_toggle(child, label);
        if (found) return found;
    }
    return NULL;
}

/* A literal native OR bitflag previously introduced an 11th pipe-separated
 * token. The native room importer then dropped the entire door record. */
static void test_native_door_bitflags_imported(void)
{
    GtkWidget *center, *right;
    NativeWorkspace *workspace = new_workspace(&center, &right);
    g_assert_true(native_workspace_test_add_document(workspace, "Brinstar 010"));
    GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(center), 0);
    g_assert_nonnull(page);
    GtkWidget *toggle = find_room_toggle(page, "Doors");
    g_assert_nonnull(toggle);
    g_assert_true(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle)));

    gchar *directory = g_dir_make_tmp("mv-native-doors-0097-XXXXXX", NULL);
    g_assert_nonnull(directory);
    gchar *filename = g_build_filename(directory, "annotations.tsv", NULL);
    const gchar *rows =
        "# kind|index|x|y|width|height|variant|native_type|label|details\n"
        "DOOR|26|32|96|16|64|native|26|Door 26|native_door=26; "
        "type=DOOR_TYPE_CLOSED_HATCH | DOOR_TYPE_NORMAL; destination_door=22\n"
        "DOOR|22|496|80|16|64|native|22|Door 22|native_door=22; "
        "type=DOOR_TYPE_CLOSED_HATCH | DOOR_TYPE_NORMAL; destination_door=26\n";
    g_assert_true(g_file_set_contents(filename, rows, -1, NULL));
    g_assert_cmpuint(native_workspace_test_load_native_doors(
        workspace, 0, filename), ==, 2);
    g_assert_cmpint(g_remove(filename), ==, 0);
    g_assert_cmpint(g_rmdir(directory), ==, 0);
    g_free(filename);
    g_free(directory);
    native_workspace_free(workspace);
}

static void test_native_doors_default_visible(void)
{
    GtkWidget *center, *right;
    NativeWorkspace *workspace = new_workspace(&center, &right);
    const char *rooms[] = {"Brinstar 022", "Aria 00:010"};
    for (guint i = 0; i < G_N_ELEMENTS(rooms); ++i) {
        g_assert_true(native_workspace_test_add_document(workspace, rooms[i]));
        GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(center), (gint)i);
        g_assert_nonnull(page);
        GtkWidget *doors = find_room_toggle(page, "Doors");
        g_assert_nonnull(doors);
        g_assert_true(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(doors)));
        const char *const labels[] = {"Enemies", "Items", "Objects", "Events"};
        for (guint j = 0; j < G_N_ELEMENTS(labels); ++j) {
            const char *label = labels[j];
            GtkWidget *toggle = find_room_toggle(page, label);
            g_assert_nonnull(toggle);
            g_assert_true(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle)));
        }
        GtkWidget *triggers = find_room_toggle(page, "Triggers");
        g_assert_nonnull(triggers);
        g_assert_true(gtk_widget_get_sensitive(triggers));
        g_assert_false(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(triggers)));
        /* User remains able to hide and restore the door overlay. */
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(doors), FALSE);
        g_assert_false(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(doors)));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(doors), TRUE);
        g_assert_true(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(doors)));
    }
    native_workspace_free(workspace);
}

static void test_hatch_preview_lifetime(void)
{
    GtkWidget *center, *right;
    NativeWorkspace *workspace = new_workspace(&center, &right);
    g_assert_true(native_workspace_test_add_document(workspace, "Brinstar 010"));
    GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(center), 0);
    GtkWidget *animate = find_room_toggle(page, "Animate hatches");
    g_assert_nonnull(animate);
    g_assert_true(gtk_widget_get_visible(animate));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(animate), TRUE);
    g_assert_true(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(animate)));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(animate), FALSE);
    g_assert_false(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(animate)));
    /* Free the document while its timeout is active: no dangling callback. */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(animate), TRUE);
    native_workspace_free(workspace);
}

static void test_collision_tools_available_in_both_modes(void)
{
    GtkWidget *center, *right;
    NativeWorkspace *workspace = new_workspace(&center, &right);
    g_assert_true(native_workspace_test_add_document(workspace, "Brinstar 010"));
    g_assert_true(native_workspace_test_add_document(workspace, "Aria 03:010"));
    const char *const names[] = {
        "Wall (W): paint solid collision; native counterpart verified in both games",
        "Platform (P): paint one-way collision; native counterpart verified in both games",
        "Hazard (D): paint damage collision; native family verified in both games",
        "Slope up (R): paint a floor rising to the right; native family verified in both games",
        "Slope down (T): paint a floor falling to the right; native family verified in both games",
        "Water (U): paint water collision; native counterpart verified in both games",
        "Air (A): erase with an explicit passable override; native counterpart verified in both games",
    };
    for (gint page_index = 0; page_index < 2; ++page_index) {
        GtkWidget *page = gtk_notebook_get_nth_page(
            GTK_NOTEBOOK(center), page_index);
        GtkWidget *previous = find_room_toggle(page, "Pencil (draw)");
        g_assert_nonnull(previous);
        for (guint i = 0; i < G_N_ELEMENTS(names); ++i) {
            GtkWidget *tool = find_room_toggle(page, names[i]);
            g_assert_nonnull(tool);
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tool), TRUE);
            g_assert_true(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(tool)));
            g_assert_false(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(previous)));
            previous = tool;
        }
    }
    native_workspace_free(workspace);
}

static void test_close_and_reopen(void)
{
    GtkWidget *center, *right;
    NativeWorkspace *workspace = new_workspace(&center, &right);

    g_assert_true(native_workspace_test_add_document(workspace, "Brinstar 022"));
    g_assert_cmpuint(native_workspace_test_document_count(workspace), ==, 1);
    g_assert_true(native_workspace_test_activate_close(workspace, 0));
    g_assert_cmpuint(native_workspace_test_document_count(workspace), ==, 0);
    g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(center)), ==, 0);
    g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(right)), ==, 0);
    g_assert_true(native_workspace_test_add_document(workspace, "Brinstar 022"));
    g_assert_true(native_workspace_test_close_document(workspace, 0));
    g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(center)), ==, 0);
    g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(right)), ==, 0);

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
    g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(center)), ==, 0);
    g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(right)), ==, 0);

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
    g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(center)), ==, 3);
    g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(right)), ==, 3);
    g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(other_center)), ==, 1);
    g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(other_right)), ==, 1);
    g_assert_true(native_workspace_test_close_document(workspace, 0));
    g_assert_cmpuint(native_workspace_test_document_count(workspace), ==, 3);
    g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(other_center)), ==, 0);
    g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(other_right)), ==, 0);

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

/* Permanent notebook above a nested, closable editor notebook.
 * An edited room must activate both layers, and closing must not remove any
 * of the permanent navigation tabs. No proprietary room data is required. */
static void test_nested_editor_tab_focus_and_close(void)
{
    GtkWidget *general = gtk_notebook_new();
    GtkWidget *editor_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *editors = gtk_notebook_new();
    GtkWidget *right = gtk_notebook_new();
    GtkWidget *right_general = gtk_notebook_new();
    GtkWidget *right_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *browser = gtk_label_new("Permanent Zero rooms browser");
    NativeWorkspace *workspace = native_workspace_new();
    gtk_box_append(GTK_BOX(editor_page), editors);
    gtk_box_append(GTK_BOX(right_page), right);
    gtk_notebook_append_page(GTK_NOTEBOOK(right_general),
                             gtk_label_new("Permanent ROM visuals"),
                             gtk_label_new("ROM visuals"));
    gtk_notebook_append_page(GTK_NOTEBOOK(right_general), right_page,
                             gtk_label_new("Room palettes"));
    gtk_notebook_append_page(GTK_NOTEBOOK(general), browser,
                             gtk_label_new("Zero rooms"));
    gtk_notebook_append_page(GTK_NOTEBOOK(general), editor_page,
                             gtk_label_new("Open editors"));
    native_workspace_build(workspace, editors, right);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(general), 0);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(right_general), 0);
    g_assert_true(native_workspace_test_add_document(workspace, "Brinstar 022"));
    /* Both outer notebooks must activate even though their page widgets
     * are separated from the nested document by GTK4 internal children. */
    g_assert_cmpint(gtk_notebook_get_current_page(GTK_NOTEBOOK(general)), ==, 1);
    g_assert_cmpint(gtk_notebook_get_current_page(GTK_NOTEBOOK(right_general)), ==, 1);
    g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(editors)), ==, 1);
    g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(general)), ==, 2);
    g_assert_true(native_workspace_test_activate_close(workspace, 0));
    g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(editors)), ==, 0);
    g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(general)), ==, 2);
    g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(right_general)), ==, 2);
    g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(right)), ==, 0);
    native_workspace_free(workspace);
    g_object_unref(g_object_ref_sink(general));
    g_object_unref(g_object_ref_sink(right_general));
}

static void test_shared_tile_and_project_history(void)
{
    GtkWidget *center, *right;
    NativeWorkspace *workspace = new_workspace(&center, &right);
    g_assert_true(native_workspace_test_add_document(workspace, "Brinstar 010"));
    g_assert_true(native_workspace_test_shared_history(workspace, 0));
    native_workspace_free(workspace);
}

static void test_collision_brush_interpolates_fast_motion(void)
{
    gint coordinates[8] = {0};
    g_assert_cmpuint(native_workspace_test_collision_line(
        0, 0, 3, 2, coordinates, 4), ==, 4);
    const gint expected[] = {0, 0, 1, 1, 2, 1, 3, 2};
    for (guint i = 0; i < G_N_ELEMENTS(expected); ++i)
        g_assert_cmpint(coordinates[i], ==, expected[i]);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    /* GTK_A11Y=test is GTK's documented backend for automated tests.
     * The Xvfb session need not connect to the user's AT-SPI bus. */
    g_setenv("GTK_A11Y", "test", TRUE);
    gtk_init();
    g_test_add_func("/native-workspace/nested-editor-tabs",
                    test_nested_editor_tab_focus_and_close);
    g_test_add_func("/native-workspace/native-doors-default-visible",
                    test_native_doors_default_visible);
    g_test_add_func("/native-workspace/hatch-preview-lifetime",
                    test_hatch_preview_lifetime);
    g_test_add_func("/native-workspace/collision-tools-both-modes",
                    test_collision_tools_available_in_both_modes);
    g_test_add_func("/native-workspace/native-door-bitflags-imported",
                    test_native_door_bitflags_imported);
    g_test_add_func("/native-workspace/close-and-reopen", test_close_and_reopen);
    g_test_add_func("/native-workspace/close-order-and-unsaved-guard",
                    test_close_order_and_unsaved_guard);
    g_test_add_func("/native-workspace/move-then-close-and-shutdown",
                    test_move_then_close_and_shutdown);
    g_test_add_func("/native-workspace/save-modified-then-close",
                    test_save_modified_then_close);
    g_test_add_func("/native-workspace/shared-tile-project-undo-redo",
                    test_shared_tile_and_project_history);
    g_test_add_func("/native-workspace/collision-brush-interpolation",
                    test_collision_brush_interpolates_fast_motion);
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
