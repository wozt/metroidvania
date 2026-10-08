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
        g_assert_true(gtk_menu_button_get_popover(GTK_MENU_BUTTON(menu)) == GTK_WIDGET(popover));
        g_assert_false(gtk_widget_get_parent(popover) == list);
    }
    g_test_message("0058: removing Aria rooms page");
    gtk_notebook_remove_page(GTK_NOTEBOOK(center), 1);
    g_test_message("0058: removing Zero rooms page");
    gtk_notebook_remove_page(GTK_NOTEBOOK(center), 0);
    g_test_message("0058: freeing shared workspace");
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

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_setenv("GTK_A11Y", "test", TRUE);
    gtk_init();
    g_test_add_func("/room-browser/shared-shells", test_shared_browser_shells);
    g_test_add_func("/room-browser/repeated-lifecycle", test_repeated_browser_lifecycle);
    return g_test_run();
}
