/* SPDX-License-Identifier: GPL-3.0-only */
/* Shared read-only catalog of native object definitions from both engines. */
#include "object_catalog.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    GtkWidget *page, *zero_list, *aria_list, *status, *details;
} ObjectCatalog;

static void clear_list(GtkWidget *list)
{
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(list)))
        gtk_list_box_remove(GTK_LIST_BOX(list), child);
}

static void selected(GtkListBox *box, GtkListBoxRow *row, gpointer userdata)
{
    ObjectCatalog *catalog = userdata;
    (void)box;
    const char *details = row ? g_object_get_data(G_OBJECT(row), "mv-object-details") : NULL;
    gtk_label_set_text(GTK_LABEL(catalog->details), details ? details :
        "Select a native object definition to inspect its decoded fields.");
}

static void append_object(GtkWidget *list, gchar **fields)
{
    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *layout = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *icon = gtk_image_new_from_icon_name("image-missing-symbolic");
    gchar *summary = g_strdup_printf("%s\n%s · native %s", fields[3], fields[4], fields[1]);
    GtkWidget *label = gtk_label_new(summary);
    gchar *details = g_strdup_printf(
        "%s\nWorld: %s\nNative identity: %s\nCategory: %s\n%s\n\n"
        "Sprite preview: not decoded; the diagnostic icon is intentional.\n"
        "Editing: unavailable until this engine type has a validated encoder.",
        fields[3], fields[0], fields[2], fields[4], fields[5]);
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_widget_set_margin_start(layout, 6);
    gtk_widget_set_margin_end(layout, 6);
    gtk_widget_set_margin_top(layout, 5);
    gtk_widget_set_margin_bottom(layout, 5);
    gtk_box_append(GTK_BOX(layout), icon);
    gtk_box_append(GTK_BOX(layout), label);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), layout);
    g_object_set_data_full(G_OBJECT(row), "mv-object-details", details, g_free);
    gtk_list_box_append(GTK_LIST_BOX(list), row);
    g_free(summary);
}

static void refresh(GtkButton *button, gpointer userdata)
{
    ObjectCatalog *catalog = userdata;
    (void)button;
    gchar *out = NULL, *err = NULL;
    GError *error = NULL;
    gint status = -1;
    gchar *argv[] = {"python3", "scripts/editor_cli.py",
                     "--command=list-assets", "--format=tsv", NULL};
    gboolean launched = g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
                                     NULL, NULL, &out, &err, &status, &error);
    gboolean succeeded = launched && g_spawn_check_wait_status(status, NULL);
    clear_list(catalog->zero_list);
    clear_list(catalog->aria_list);
    guint zero = 0, aria = 0;
    if (succeeded && out) {
        gchar **lines = g_strsplit(out, "\n", -1);
        for (guint i = 0; lines[i] && i < 4096; ++i) {
            if (!lines[i][0]) continue;
            gchar **fields = g_strsplit(lines[i], "\t", 9);
            if (g_strv_length(fields) == 8) {
                if (!strcmp(fields[0], "Zero Mission")) {
                    append_object(catalog->zero_list, fields); ++zero;
                } else if (!strcmp(fields[0], "Aria of Sorrow")) {
                    append_object(catalog->aria_list, fields); ++aria;
                }
            }
            g_strfreev(fields);
        }
        g_strfreev(lines);
    }
    gchar *message = NULL;
    if (error) message = g_strdup(error->message);
    else if (!succeeded)
        message = g_strdup(err && *err ? err : "Unable to decode native object catalogs");
    else message = g_strdup_printf(
        "%u Zero Mission and %u Aria native types. Original definitions are read-only.",
        zero, aria);
    gtk_label_set_text(GTK_LABEL(catalog->status), message);
    g_free(message); g_free(out); g_free(err); g_clear_error(&error);
}

static GtkWidget *catalog_column(const char *title, GtkWidget **list_out,
                                 ObjectCatalog *catalog)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *heading = gtk_label_new(title);
    GtkWidget *scroll = gtk_scrolled_window_new();
    GtkWidget *list = gtk_list_box_new();
    gtk_widget_add_css_class(heading, "title-4");
    gtk_label_set_xalign(GTK_LABEL(heading), 0);
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_SINGLE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), list);
    gtk_widget_set_hexpand(scroll, TRUE);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_box_append(GTK_BOX(box), heading);
    gtk_box_append(GTK_BOX(box), scroll);
    g_signal_connect(list, "row-selected", G_CALLBACK(selected), catalog);
    *list_out = list;
    return box;
}

static void catalog_free(gpointer data) { g_free(data); }

GtkWidget *object_catalog_build(GtkWidget *center)
{
    ObjectCatalog *catalog = g_new0(ObjectCatalog, 1);
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *refresh_button = gtk_button_new_with_label("Refresh native catalog");
    GtkWidget *create = gtk_button_new_with_label("Create project object");
    GtkWidget *clone = gtk_button_new_with_label("Clone selected for editing");
    GtkWidget *columns = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *zero = catalog_column("Zero Mission objects", &catalog->zero_list, catalog);
    GtkWidget *aria = catalog_column("Aria of Sorrow objects", &catalog->aria_list, catalog);
    catalog->status = gtk_label_new("Loading native object definitions...");
    catalog->details = gtk_label_new(
        "Select a native object definition to inspect its decoded fields.");
    catalog->page = page;
    gtk_widget_set_sensitive(create, FALSE);
    gtk_widget_set_sensitive(clone, FALSE);
    gtk_widget_set_tooltip_text(create,
        "Disabled until the project-object schema and target-engine encoder are validated.");
    gtk_widget_set_tooltip_text(clone,
        "Native definitions stay read-only until a validated override encoder exists.");
    gtk_box_append(GTK_BOX(toolbar), refresh_button);
    gtk_box_append(GTK_BOX(toolbar), create);
    gtk_box_append(GTK_BOX(toolbar), clone);
    gtk_paned_set_start_child(GTK_PANED(columns), zero);
    gtk_paned_set_end_child(GTK_PANED(columns), aria);
    gtk_paned_set_position(GTK_PANED(columns), 470);
    gtk_paned_set_resize_start_child(GTK_PANED(columns), TRUE);
    gtk_paned_set_resize_end_child(GTK_PANED(columns), TRUE);
    gtk_widget_set_vexpand(columns, TRUE);
    gtk_label_set_xalign(GTK_LABEL(catalog->status), 0);
    gtk_label_set_wrap(GTK_LABEL(catalog->status), TRUE);
    gtk_label_set_selectable(GTK_LABEL(catalog->status), TRUE);
    gtk_label_set_xalign(GTK_LABEL(catalog->details), 0);
    gtk_label_set_wrap(GTK_LABEL(catalog->details), TRUE);
    gtk_label_set_selectable(GTK_LABEL(catalog->details), TRUE);
    gtk_widget_set_margin_start(page, 10);
    gtk_widget_set_margin_end(page, 10);
    gtk_widget_set_margin_top(page, 8);
    gtk_box_append(GTK_BOX(page), toolbar);
    gtk_box_append(GTK_BOX(page), catalog->status);
    gtk_box_append(GTK_BOX(page), columns);
    gtk_box_append(GTK_BOX(page), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(page), catalog->details);
    g_object_set_data_full(G_OBJECT(page), "mv-object-catalog", catalog, catalog_free);
    gtk_notebook_append_page(GTK_NOTEBOOK(center), page, gtk_label_new("Object catalog"));
    g_signal_connect(refresh_button, "clicked", G_CALLBACK(refresh), catalog);
    refresh(NULL, catalog);
    return page;
}
