/* SPDX-License-Identifier: GPL-3.0-only */
/* Both browsers share one widget contract without reading proprietary assets. */
#include "room_browser.h"
#include <gtk/gtk.h>
#include <glib/gstdio.h>

static void test_shared_browser_shells(void)
{
    GtkWidget *center = gtk_notebook_new();
    GtkWidget *right = gtk_notebook_new();
    NativeWorkspace *ws = native_workspace_new();
    native_workspace_build(ws, center, right);
    g_test_message("0058: constructing Zero rooms browser");
    GtkWidget *zero = room_browser_build(center, ws, ROOM_WORLD_ZERO);
    g_test_message("0058: constructing Aria rooms browser");
    GtkWidget *aria = room_browser_build(center, ws, ROOM_WORLD_ARIA);
    g_test_message("0058: both room browsers constructed");
    g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(center)), ==, 2);
    g_assert_cmpstr(gtk_label_get_text(GTK_LABEL(
        gtk_notebook_get_tab_label(GTK_NOTEBOOK(center), zero))), ==, "Zero rooms");
    g_assert_cmpstr(gtk_label_get_text(GTK_LABEL(
        gtk_notebook_get_tab_label(GTK_NOTEBOOK(center), aria))), ==, "Aria rooms");
    g_assert_cmpuint(GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(zero), "mv-world-mode")), ==, 1);
    g_assert_cmpuint(GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(aria), "mv-world-mode")), ==, 2);
    g_assert_true(GTK_IS_LIST_BOX(g_object_get_data(G_OBJECT(zero), "mv-room-browser-list")));
    g_assert_true(GTK_IS_LIST_BOX(g_object_get_data(G_OBJECT(aria), "mv-room-browser-list")));
    /* The menu must be hosted by a GTK widget that supports popovers. */
    GtkWidget *pages[] = {zero, aria};
    for (guint i = 0; i < G_N_ELEMENTS(pages); ++i) {
        GtkWidget *list = g_object_get_data(G_OBJECT(pages[i]), "mv-room-browser-list");
        GtkWidget *menu = g_object_get_data(G_OBJECT(pages[i]), "mv-room-browser-menu-button");
        GtkWidget *popover = g_object_get_data(G_OBJECT(pages[i]), "mv-create-popover");
        g_assert_true(GTK_IS_MENU_BUTTON(menu));
        g_assert_true(GTK_IS_POPOVER(popover));
        g_assert_true(gtk_menu_button_get_popover(GTK_MENU_BUTTON(menu)) == GTK_POPOVER(popover));
        g_assert_false(gtk_widget_get_parent(popover) == list);
        /* GTK4 consumes the GListModel passed to gtk_drop_down_new().
         * Both dropdowns must retain valid models across construction,
         * filtering and page teardown (even with no local ROM data). */
        GtkWidget *toolbar = gtk_widget_get_first_child(pages[i]);
        g_assert_nonnull(toolbar);
        GtkWidget *area_filter = gtk_widget_get_next_sibling(
            gtk_widget_get_first_child(toolbar));
        g_assert_true(GTK_IS_DROP_DOWN(area_filter));
        GListModel *model = gtk_drop_down_get_model(GTK_DROP_DOWN(area_filter));
        g_assert_true(G_IS_LIST_MODEL(model));
        g_assert_cmpuint(g_list_model_get_n_items(model), ==, i == 0 ? 8u : 13u);
        gtk_drop_down_set_selected(GTK_DROP_DOWN(area_filter), 1);
        gtk_drop_down_set_selected(GTK_DROP_DOWN(area_filter), 0);
    }
    /* Exercise filtering and selection during teardown even without
     * local proprietary catalogs. GTK must not use stale toolbar widgets. */
    for (guint i = 0; i < G_N_ELEMENTS(pages); ++i) {
        GtkListBox *list = GTK_LIST_BOX(g_object_get_data(
            G_OBJECT(pages[i]), "mv-room-browser-list"));
        GtkWidget *row = gtk_list_box_row_new();
        gtk_list_box_append(list, row);
        gtk_list_box_select_row(list, GTK_LIST_BOX_ROW(row));
        g_assert_true(gtk_list_box_get_selected_row(list) == GTK_LIST_BOX_ROW(row));
    }
    g_test_message("0061: removing selected Aria rooms page");
    gtk_notebook_remove_page(GTK_NOTEBOOK(center), 1);
    g_test_message("0061: removing selected Zero rooms page");
    gtk_notebook_remove_page(GTK_NOTEBOOK(center), 0);
    g_test_message("0061: freeing shared workspace");
    native_workspace_free(ws);
    g_test_message("0058: shared shells complete");
}

/* Repeat browser construction and teardown to exercise the GtkListBox
 * filter callbacks and GObject-owned popup destruction after tab removal. */
static void test_repeated_browser_lifecycle(void)
{
    for (guint iteration = 0; iteration < 12; ++iteration) {
        GtkWidget *center = gtk_notebook_new();
        GtkWidget *right = gtk_notebook_new();
        NativeWorkspace *workspace = native_workspace_new();
        native_workspace_build(workspace, center, right);
        g_test_message("0058: lifecycle iteration %u build Zero", iteration);
        room_browser_build(center, workspace, ROOM_WORLD_ZERO);
        g_test_message("0058: lifecycle iteration %u build Aria", iteration);
        room_browser_build(center, workspace, ROOM_WORLD_ARIA);
        g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(center)), ==, 2);
        gtk_notebook_remove_page(GTK_NOTEBOOK(center), 1);
        gtk_notebook_remove_page(GTK_NOTEBOOK(center), 0);
        g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(center)), ==, 0);
        native_workspace_free(workspace);
        g_object_unref(g_object_ref_sink(center));
        g_object_unref(g_object_ref_sink(right));
    }
}

/* Only inspect GTK controls: test must never write real private room drafts. */
static void test_create_room_dialogs(void)
{
    GtkWidget *center = gtk_notebook_new();
    GtkWidget *right = gtk_notebook_new();
    NativeWorkspace *ws = native_workspace_new();
    native_workspace_build(ws, center, right);
    GtkWidget *pages[2] = {
        room_browser_build(center, ws, ROOM_WORLD_ZERO),
        room_browser_build(center, ws, ROOM_WORLD_ARIA),
    };
    GListModel *windows = gtk_window_get_toplevels();
    for (guint world = 0; world < G_N_ELEMENTS(pages); ++world) {
        GtkWidget *page = pages[world];
        GtkWidget *action = g_object_get_data(G_OBJECT(page), "mv-create-room-action");
        g_assert_true(GTK_IS_BUTTON(action));
        g_assert_true(gtk_widget_get_sensitive(action));
        GtkWidget *toolbar = gtk_widget_get_first_child(page);
        GtkWidget *area_filter = gtk_widget_get_next_sibling(
            gtk_widget_get_first_child(toolbar));
        gtk_drop_down_set_selected(GTK_DROP_DOWN(area_filter), 2);
        guint before = g_list_model_get_n_items(windows);
        gtk_button_clicked(GTK_BUTTON(action));
        g_assert_cmpuint(g_list_model_get_n_items(windows), ==, before + 1);
        GtkWindow *dialog = NULL;
        for (guint n = 0; n < g_list_model_get_n_items(windows); ++n) {
            GtkWindow *candidate = GTK_WINDOW(g_list_model_get_item(windows, n));
            if (GPOINTER_TO_UINT(g_object_get_data(
                    G_OBJECT(candidate), "mv-room-draft-world")) == world + 1) {
                dialog = candidate;
                break;
            }
            g_object_unref(candidate);
        }
        g_assert_nonnull(dialog);
        GtkWidget *area = g_object_get_data(G_OBJECT(dialog), "mv-room-draft-area");
        g_assert_true(GTK_IS_DROP_DOWN(area));
        g_assert_cmpuint(gtk_drop_down_get_selected(GTK_DROP_DOWN(area)), ==, 1);
        gtk_window_destroy(dialog);
        g_object_unref(dialog);
    }
    gtk_notebook_remove_page(GTK_NOTEBOOK(center), 1);
    gtk_notebook_remove_page(GTK_NOTEBOOK(center), 0);
    native_workspace_free(ws);
    g_object_unref(g_object_ref_sink(center));
    g_object_unref(g_object_ref_sink(right));
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_setenv("GTK_A11Y", "test", TRUE);
    gtk_init();
    g_test_add_func("/room-browser/shared-shells", test_shared_browser_shells);
    g_test_add_func("/room-browser/repeated-lifecycle", test_repeated_browser_lifecycle);
    g_test_add_func("/room-browser/create-draft-form", test_create_room_dialogs);
    return g_test_run();
}
