/* SPDX-License-Identifier: GPL-3.0-only */
/* Read-only Aria room preview. Never writes ROM data or editable overrides. */
#include "aria_browser.h"
#include "native_workspace.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    NativeWorkspace *workspace;
    GtkWidget *page;
    GtkWidget *status;
    GtkWidget *details;
    GtkWidget *picture;
    GtkWidget *render_button;
    GtkWidget *mode;
    unsigned area;
    unsigned room;
    gboolean selected;
    gboolean busy;
} AriaBrowser;

static void display_preview(AriaBrowser *browser)
{
    char filename[320];
    unsigned mode = gtk_drop_down_get_selected(GTK_DROP_DOWN(browser->mode));
    const char *suffixes[] = {
        "composite.bmp", "bg1.bmp", "bg2.bmp", "bg3.bmp", "collision.bmp"
    };
    if (!browser->selected || mode >= G_N_ELEMENTS(suffixes)) return;
    snprintf(filename, sizeof(filename),
             "assets/extracted/rooms/aria/previews/area_%02u_room_%03u_%s",
             browser->area, browser->room, suffixes[mode]);
    if (g_file_test(filename, G_FILE_TEST_IS_REGULAR)) {
        gtk_picture_set_filename(GTK_PICTURE(browser->picture), filename);
    } else {
        gtk_picture_set_paintable(GTK_PICTURE(browser->picture), NULL);
    }
}

static void mode_changed(GObject *object, GParamSpec *parameter, gpointer userdata)
{
    (void)object;
    (void)parameter;
    display_preview(userdata);
}

static void room_selected(GtkListBox *list, GtkListBoxRow *row, gpointer userdata)
{
    AriaBrowser *browser = userdata;
    (void)list;
    if (!row) {
        browser->selected = FALSE;
        gtk_picture_set_paintable(GTK_PICTURE(browser->picture), NULL);
        return;
    }
    browser->area = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(row), "aria-area"));
    browser->room = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(row), "aria-room"));
    browser->selected = TRUE;
    const char *details = g_object_get_data(G_OBJECT(row), "aria-details");
    if (details) gtk_label_set_text(GTK_LABEL(browser->details), details);
    if (!browser->busy) {
        gtk_label_set_text(GTK_LABEL(browser->status),
                           "Click Render to decode this room from your verified local ROM.");
    }
    display_preview(browser);
}

static void render_complete(GObject *object, GAsyncResult *result, gpointer userdata)
{
    AriaBrowser *browser = userdata;
    GSubprocess *process = G_SUBPROCESS(object);
    GError *error = NULL;
    gchar *output = NULL, *diagnostic = NULL;
    gboolean ok = g_subprocess_communicate_utf8_finish(
        process, result, &output, &diagnostic, &error);
    unsigned area = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(process), "aria-area"));
    unsigned room = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(process), "aria-room"));
    browser->busy = FALSE;
    gtk_widget_set_sensitive(browser->render_button, TRUE);
    if (ok && g_subprocess_get_successful(process)) {
        if (browser->selected && browser->area == area && browser->room == room) {
            display_preview(browser);
            gtk_label_set_text(GTK_LABEL(browser->status),
                "Native ROM-derived preview generated. Diagnostic, not a fully reconstructed room.");
        } else {
            gtk_label_set_text(GTK_LABEL(browser->status),
                "Preview generated for the previous room; select it again to inspect.");
        }
    } else {
        const char *message = error ? error->message : diagnostic;
        gchar *description = g_strdup_printf("Aria decoder: %.500s",
            message && message[0] ? message : "No supported background layer / decoder failed");
        gtk_label_set_text(GTK_LABEL(browser->status), description);
        g_free(description);
    }
    if (error) g_error_free(error);
    g_free(output);
    g_free(diagnostic);
    /* The extra page reference keeps browser state alive through async work. */
    g_object_unref(browser->page);
}

static void render_selected(GtkButton *button, gpointer userdata)
{
    AriaBrowser *browser = userdata;
    (void)button;
    if (!browser->selected || browser->busy) return;
    char area_text[16], room_text[16];
    snprintf(area_text, sizeof(area_text), "%u", browser->area);
    snprintf(room_text, sizeof(room_text), "%u", browser->room);
    GError *error = NULL;
    GSubprocess *process = g_subprocess_new(
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE,
        &error, "python3", "-m", "scripts.aos_room_render",
        "--area", area_text, "--room", room_text, NULL);
    if (!process) {
        gtk_label_set_text(GTK_LABEL(browser->status),
                           error ? error->message : "Unable to start Aria renderer");
        if (error) g_error_free(error);
        return;
    }
    browser->busy = TRUE;
    gtk_widget_set_sensitive(browser->render_button, FALSE);
    gtk_label_set_text(GTK_LABEL(browser->status), "Decoding original Aria room data...");
    g_object_set_data(G_OBJECT(process), "aria-area", GUINT_TO_POINTER(browser->area));
    g_object_set_data(G_OBJECT(process), "aria-room", GUINT_TO_POINTER(browser->room));
    g_object_ref(browser->page);
    g_subprocess_communicate_utf8_async(process, NULL, NULL, render_complete, browser);
    g_object_unref(process);
}

static void edit_selected(GtkButton *button, gpointer userdata)
{
    AriaBrowser *browser = userdata;
    (void)button;
    if (!browser->selected || !browser->workspace) return;
    native_workspace_import_aria_async(browser->workspace,
                                       browser->area, browser->room);
}

static void room_activated(GtkListBox *list, GtkListBoxRow *row, gpointer userdata)
{
    room_selected(list, row, userdata);
    edit_selected(NULL, userdata);
}

static void populate_rooms(GtkWidget *list, AriaBrowser *browser)
{
    gchar *contents = NULL;
    if (!g_file_get_contents("assets/extracted/rooms/aria/rooms.tsv",
                             &contents, NULL, NULL)) {
        gtk_label_set_text(GTK_LABEL(browser->status),
            "Aria index missing. Run: python3 scripts/import_aos_world.py");
        return;
    }
    gchar **lines = g_strsplit(contents, "\n", -1);
    for (gsize i = 0; lines[i]; ++i) {
        if (!lines[i][0] || lines[i][0] == '#') continue;
        gchar **fields = g_strsplit(lines[i], "|", -1);
        if (g_strv_length(fields) != 9) {
            g_strfreev(fields);
            continue;
        }
        gchar *end_area = NULL, *end_room = NULL;
        guint64 area = g_ascii_strtoull(fields[0], &end_area, 10);
        guint64 room = g_ascii_strtoull(fields[1], &end_room, 10);
        if (!end_area || *end_area || !end_room || *end_room || area >= 12 || room >= 1000) {
            g_strfreev(fields);
            continue;
        }
        gchar *label_text = g_strdup_printf(
            "%02u:%03u  %s%s%s", (unsigned)area, (unsigned)room, fields[2],
            strcmp(fields[7], "1") == 0 ? "  [SAVE]" : "",
            strcmp(fields[8], "1") == 0 ? "  [BOSS]" : "");
        gchar *details = g_strdup_printf(
            "Aria / %s | engine room %02u:%03u | map size %sx%s screens\n"
            "Entities: %s | transitions: %s | save: %s | boss: %s\n"
            "Original graphics are previewed locally; edits are not enabled.",
            fields[2], (unsigned)area, (unsigned)room, fields[3], fields[4],
            fields[5], fields[6], fields[7], fields[8]);
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label = gtk_label_new(label_text);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        g_object_set_data(G_OBJECT(row), "aria-area", GUINT_TO_POINTER((guint)area));
        g_object_set_data(G_OBJECT(row), "aria-room", GUINT_TO_POINTER((guint)room));
        g_object_set_data_full(G_OBJECT(row), "aria-details", details, g_free);
        gtk_list_box_append(GTK_LIST_BOX(list), row);
        g_free(label_text);
        g_strfreev(fields);
    }
    g_strfreev(lines);
    g_free(contents);
}

GtkWidget *aria_browser_build(GtkWidget *center, NativeWorkspace *workspace)
{
    static const char *const modes[] = {
        "Composite", "BG1", "BG2", "BG3", "Collision (diagnostic)", NULL
    };
    AriaBrowser *browser = g_new0(AriaBrowser, 1);
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 7);
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *layout = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *list_scroll = gtk_scrolled_window_new();
    GtkWidget *image_scroll = gtk_scrolled_window_new();
    GtkWidget *list = gtk_list_box_new();
    browser->page = page;
    browser->workspace = workspace;
    browser->mode = gtk_drop_down_new_from_strings(modes);
    browser->picture = gtk_picture_new();
    browser->status = gtk_label_new("Load the native Aria catalog to browse rooms.");
    browser->details = gtk_label_new("Choose a room to inspect its ROM-derived metadata.");
    browser->render_button = gtk_button_new_with_label("Render selected room");
    GtkWidget *edit_button = gtk_button_new_from_icon_name("document-edit-symbolic");
    gtk_widget_set_tooltip_text(edit_button, "Edit selected Aria room using native tile tools");
    gtk_widget_set_tooltip_text(browser->render_button,
        "Decode original background layers with the verified local USA ROM");
    gtk_widget_set_margin_start(page, 8);
    gtk_widget_set_margin_end(page, 8);
    gtk_widget_set_margin_top(page, 8);
    gtk_widget_set_margin_bottom(page, 8);
    gtk_box_append(GTK_BOX(toolbar), gtk_label_new("Layer:"));
    gtk_box_append(GTK_BOX(toolbar), browser->mode);
    gtk_box_append(GTK_BOX(toolbar), browser->render_button);
    gtk_box_append(GTK_BOX(toolbar), edit_button);
    gtk_box_append(GTK_BOX(page), toolbar);
    gtk_box_append(GTK_BOX(page), browser->details);
    gtk_label_set_wrap(GTK_LABEL(browser->details), TRUE);
    gtk_label_set_xalign(GTK_LABEL(browser->details), 0.0f);
    gtk_widget_set_vexpand(layout, TRUE);
    gtk_widget_set_hexpand(layout, TRUE);
    gtk_box_append(GTK_BOX(page), layout);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(list_scroll), list);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(list_scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(list_scroll, 190, -1);
    gtk_paned_set_start_child(GTK_PANED(layout), list_scroll);
    gtk_paned_set_end_child(GTK_PANED(layout), image_scroll);
    gtk_paned_set_position(GTK_PANED(layout), 330);
    gtk_picture_set_can_shrink(GTK_PICTURE(browser->picture), TRUE);
    gtk_widget_set_hexpand(browser->picture, TRUE);
    gtk_widget_set_vexpand(browser->picture, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(image_scroll), browser->picture);
    gtk_box_append(GTK_BOX(page), browser->status);
    gtk_label_set_wrap(GTK_LABEL(browser->status), TRUE);
    gtk_label_set_xalign(GTK_LABEL(browser->status), 0.0f);
    populate_rooms(list, browser);
    g_signal_connect(list, "row-selected", G_CALLBACK(room_selected), browser);
    g_signal_connect(list, "row-activated", G_CALLBACK(room_activated), browser);
    g_signal_connect(browser->mode, "notify::selected", G_CALLBACK(mode_changed), browser);
    g_signal_connect(browser->render_button, "clicked", G_CALLBACK(render_selected), browser);
    g_signal_connect(edit_button, "clicked", G_CALLBACK(edit_selected), browser);
    g_object_set_data_full(G_OBJECT(page), "aria-browser-state", browser, g_free);
    gtk_notebook_append_page(GTK_NOTEBOOK(center), page, gtk_label_new("Aria / Native rooms"));
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(center), page, TRUE);
    gtk_notebook_set_tab_detachable(GTK_NOTEBOOK(center), page, TRUE);
    return page;
}
