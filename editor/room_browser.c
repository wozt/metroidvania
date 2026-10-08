/* SPDX-License-Identifier: GPL-3.0-only */
/* Shared structural room browser. Never writes ROM data or inferred geometry. */
#include "room_browser.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    NativeWorkspace *workspace;
    RoomWorld world;
    GtkWidget *page, *list, *status, *details, *picture;
    GtkWidget *render_button, *mode, *area_filter, *context_menu_button;
    gboolean selected, busy;
    guint area, room;
} RoomBrowser;

/* A notebook page owns RoomBrowser, but GTK destroys children in tree
 * order. Keep controls alive until the page's finalizer has finished all
 * child teardown callbacks (particularly GtkListBox filtering/selection). */
static void room_browser_state_free(gpointer userdata)
{
    RoomBrowser *browser = userdata;
    /* Disconnect callbacks before releasing any referenced GTK child. */
    g_signal_handlers_disconnect_by_data(browser->list, browser);
    gtk_list_box_set_filter_func(GTK_LIST_BOX(browser->list), NULL, NULL, NULL);
    g_signal_handlers_disconnect_by_data(browser->mode, browser);
    g_signal_handlers_disconnect_by_data(browser->area_filter, browser);
    g_signal_handlers_disconnect_by_data(browser->render_button, browser);

    g_object_unref(browser->list);
    g_object_unref(browser->mode);
    g_object_unref(browser->area_filter);
    g_object_unref(browser->render_button);
    g_object_unref(browser->status);
    g_object_unref(browser->details);
    g_object_unref(browser->picture);
    g_object_unref(browser->context_menu_button);
    g_free(browser);
}

static const char *const zero_areas[] = {
    "Brinstar", "Kraid", "Norfair", "Ridley", "Tourian", "Crateria", "Chozodia", NULL
};
static const char *const aria_areas[] = {
    "Castle Corridor", "Chapel", "Study", "Dance Hall", "Inner Quarters",
    "Floating Garden", "Clock Tower", "Underground", "The Arena", "Top Floor",
    "Chaotic Realm entrance", "Chaotic Realm boss", NULL
};
static const char *const zero_modes[] = {"BG1", "BG2", NULL};
static const char *const aria_modes[] = {
    "Composite", "BG1", "BG2", "BG3", "Collision (diagnostic)", NULL
};
static const char *const aria_suffixes[] = {
    "composite", "bg1", "bg2", "bg3", "collision"
};

static const char *const *area_names(RoomWorld world)
{
    return world == ROOM_WORLD_ARIA ? aria_areas : zero_areas;
}

static void display_preview(RoomBrowser *browser)
{
    char filename[320];
    guint mode = gtk_drop_down_get_selected(GTK_DROP_DOWN(browser->mode));
    if (!browser->selected) {
        gtk_picture_set_paintable(GTK_PICTURE(browser->picture), NULL);
        return;
    }
    if (browser->world == ROOM_WORLD_ARIA) {
        if (mode >= G_N_ELEMENTS(aria_suffixes)) return;
        snprintf(filename, sizeof(filename),
                 "assets/extracted/rooms/aria/previews/area_%02u_room_%03u_%s.bmp",
                 browser->area, browser->room, aria_suffixes[mode]);
    } else {
        if (mode >= G_N_ELEMENTS(zero_modes) - 1) return;
        gchar *area_name = g_ascii_strdown(zero_areas[browser->area], -1);
        gchar *layer_name = g_ascii_strdown(zero_modes[mode], -1);
        snprintf(filename, sizeof(filename),
                 "assets/extracted/rooms/metroid/previews/%s_%03u_%s.bmp",
                 area_name, browser->room, layer_name);
        g_free(area_name);
        g_free(layer_name);
    }
    if (g_file_test(filename, G_FILE_TEST_IS_REGULAR))
        gtk_picture_set_filename(GTK_PICTURE(browser->picture), filename);
    else
        gtk_picture_set_paintable(GTK_PICTURE(browser->picture), NULL);
}

static void mode_changed(GObject *object, GParamSpec *pspec, gpointer userdata)
{
    (void)object; (void)pspec;
    display_preview(userdata);
}

static void room_selected(GtkListBox *list, GtkListBoxRow *row, gpointer userdata)
{
    RoomBrowser *browser = userdata;
    (void)list;
    if (!row) {
        browser->selected = FALSE;
        gtk_picture_set_paintable(GTK_PICTURE(browser->picture), NULL);
        gtk_label_set_text(GTK_LABEL(browser->details), "Select a verified original room.");
        return;
    }
    browser->area = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(row), "mv-area"));
    browser->room = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(row), "mv-room"));
    browser->selected = TRUE;
    const char *details = g_object_get_data(G_OBJECT(row), "mv-details");
    gtk_label_set_text(GTK_LABEL(browser->details), details ? details : "Native room");
    if (!browser->busy)
        gtk_label_set_text(GTK_LABEL(browser->status),
            "Open to edit the project-owned native room, or Render for a source preview.");
    display_preview(browser);
}

static void edit_selected(GtkButton *button, gpointer userdata)
{
    RoomBrowser *browser = userdata;
    (void)button;
    if (!browser->selected || !browser->workspace) return;
    if (browser->world == ROOM_WORLD_ARIA)
        native_workspace_import_aria_async(browser->workspace,
                                           browser->area, browser->room);
    else
        native_workspace_import_async(browser->workspace,
                                      zero_areas[browser->area], browser->room);
}

static void room_activated(GtkListBox *list, GtkListBoxRow *row, gpointer userdata)
{
    room_selected(list, row, userdata);
    edit_selected(NULL, userdata);
}

static void render_complete(GObject *object, GAsyncResult *result, gpointer userdata)
{
    RoomBrowser *browser = userdata;
    GSubprocess *process = G_SUBPROCESS(object);
    GError *error = NULL;
    gchar *output = NULL, *diagnostic = NULL;
    gboolean ok = g_subprocess_communicate_utf8_finish(
        process, result, &output, &diagnostic, &error);
    guint area = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(process), "mv-area"));
    guint room = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(process), "mv-room"));
    browser->busy = FALSE;
    gtk_widget_set_sensitive(browser->render_button, TRUE);
    if (ok && g_subprocess_get_successful(process)) {
        if (browser->selected && browser->area == area && browser->room == room) {
            display_preview(browser);
            gtk_label_set_text(GTK_LABEL(browser->status),
                "Original room preview generated. Incomplete layers remain diagnostic.");
        } else {
            gtk_label_set_text(GTK_LABEL(browser->status),
                "Previous room rendered; select it again to inspect the preview.");
        }
    } else {
        const char *reason = error ? error->message : diagnostic;
        gchar *msg = g_strdup_printf("Native renderer: %.500s",
                                    reason && *reason ? reason : "Failed without diagnostic");
        gtk_label_set_text(GTK_LABEL(browser->status), msg);
        g_free(msg);
    }
    if (error) g_error_free(error);
    g_free(output);
    g_free(diagnostic);
    g_object_unref(browser->page); /* Strong ref retained until async completion. */
}

static void render_selected(GtkButton *button, gpointer userdata)
{
    RoomBrowser *browser = userdata;
    (void)button;
    if (!browser->selected || browser->busy) return;
    char area[16], room[16];
    snprintf(area, sizeof(area), "%u", browser->area);
    snprintf(room, sizeof(room), "%u", browser->room);
    const char *module = browser->world == ROOM_WORLD_ARIA ?
        "scripts.aos_room_render" : "scripts.mzm_room_render";
    const char *area_arg = browser->world == ROOM_WORLD_ARIA ?
        area : zero_areas[browser->area];
    GError *error = NULL;
    GSubprocess *proc = g_subprocess_new(
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE,
        &error, "python3", "-m", module,
        "--area", area_arg, "--room", room, NULL);
    if (!proc) {
        gtk_label_set_text(GTK_LABEL(browser->status),
                           error ? error->message : "Native renderer unavailable");
        g_clear_error(&error);
        return;
    }
    browser->busy = TRUE;
    gtk_widget_set_sensitive(browser->render_button, FALSE);
    gtk_label_set_text(GTK_LABEL(browser->status),
                       "Decoding original graphics from private local sources...");
    g_object_set_data(G_OBJECT(proc), "mv-area", GUINT_TO_POINTER(browser->area));
    g_object_set_data(G_OBJECT(proc), "mv-room", GUINT_TO_POINTER(browser->room));
    g_object_ref(browser->page);
    g_subprocess_communicate_utf8_async(proc, NULL, NULL,
        render_complete, browser);
    g_object_unref(proc);
}

static gboolean filter_room(GtkListBoxRow *row, gpointer userdata)
{
    RoomBrowser *browser = userdata;
    guint wanted = gtk_drop_down_get_selected(GTK_DROP_DOWN(browser->area_filter));
    return wanted == 0 ||
        GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(row), "mv-area")) == wanted - 1;
}

static void area_changed(GObject *object, GParamSpec *pspec, gpointer userdata)
{
    RoomBrowser *browser = userdata;
    (void)object; (void)pspec;
    gtk_list_box_unselect_all(GTK_LIST_BOX(browser->list));
    gtk_list_box_invalidate_filter(GTK_LIST_BOX(browser->list));
}

static void append_room(RoomBrowser *browser, guint area, guint room,
                        const char *label, gchar *details)
{
    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *text = gtk_label_new(label);
    gtk_label_set_xalign(GTK_LABEL(text), 0.0f);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), text);
    g_object_set_data(G_OBJECT(row), "mv-area", GUINT_TO_POINTER(area));
    g_object_set_data(G_OBJECT(row), "mv-room", GUINT_TO_POINTER(room));
    g_object_set_data_full(G_OBJECT(row), "mv-details", details, g_free);
    gtk_list_box_append(GTK_LIST_BOX(browser->list), row);
}

static gboolean parse_uint(const char *src, guint bound, guint *result)
{
    if (!src || !*src) return FALSE;
    for (const char *p = src; *p; ++p)
        if (!g_ascii_isdigit(*p)) return FALSE;
    gchar *end = NULL;
    guint64 n = g_ascii_strtoull(src, &end, 10);
    if (!end || *end || n >= bound) return FALSE;
    *result = (guint)n;
    return TRUE;
}

static void populate_rooms(RoomBrowser *browser)
{
    const char *path = browser->world == ROOM_WORLD_ARIA ?
        "assets/extracted/rooms/aria/rooms.tsv" :
        "assets/extracted/rooms/metroid/rooms.tsv";
    gchar *contents = NULL;
    if (!g_file_get_contents(path, &contents, NULL, NULL)) {
        gtk_label_set_text(GTK_LABEL(browser->status),
            browser->world == ROOM_WORLD_ARIA ?
            "Aria catalog missing. Run python3 -m scripts.import_aos_world then python3 -m scripts.aos_room_index." :
            "Zero Mission catalog missing. Run python3 -m scripts.import_mzm_rooms.");
        return;
    }
    gchar **lines = g_strsplit(contents, "\n", -1);
    guint count = 0;
    for (guint i = 0; lines[i]; ++i) {
        if (!lines[i][0] || lines[i][0] == '#') continue;
        gchar **fields = g_strsplit(lines[i], "|", -1);
        guint n = g_strv_length(fields);
        guint area = G_MAXUINT, room = 0;
        if (browser->world == ROOM_WORLD_ARIA) {
            if (n == 9 && parse_uint(fields[0], 12, &area) &&
                parse_uint(fields[1], 1000, &room)) {
                gchar *label = g_strdup_printf("%02u:%03u  %s%s%s", area, room,
                    fields[2], strcmp(fields[7], "1") == 0 ? "  [SAVE]" : "",
                    strcmp(fields[8], "1") == 0 ? "  [BOSS]" : "");
                gchar *details = g_strdup_printf(
                    "Aria / %s | engine room %02u:%03u | %sx%s screens\n"
                    "Entities: %s | transitions: %s | save: %s | boss: %s\n"
                    "Original graphics remain diagnostic; authored edits stay private.",
                    fields[2], area, room, fields[3], fields[4], fields[5],
                    fields[6], fields[7], fields[8]);
                append_room(browser, area, room, label, details);
                g_free(label);
                ++count;
            }
        } else if (n == 10) {
            for (guint j = 0; zero_areas[j]; ++j) {
                if (g_strcmp0(fields[0], zero_areas[j]) == 0) { area = j; break; }
            }
            if (area != G_MAXUINT && parse_uint(fields[1], 1000, &room)) {
                gchar *label = g_strdup_printf("%s / room %03u  tileset %s  %s",
                                               fields[0], room, fields[2], fields[3]);
                gchar *details = g_strdup_printf(
                    "Zero Mission / %s / room %03u | tileset %s\n"
                    "Music: %s | BG1: %s | BG2: %s | clip: %s\n"
                    "Sprite set: %s | original minimap anchor: %s,%s",
                    fields[0], room, fields[2], fields[3], fields[4], fields[5],
                    fields[6], fields[7], fields[8], fields[9]);
                append_room(browser, area, room, label, details);
                g_free(label);
                ++count;
            }
        }
        g_strfreev(fields);
    }
    g_strfreev(lines);
    g_free(contents);
    gchar *message = g_strdup_printf("%u verified original room descriptors. Double-click to edit.", count);
    gtk_label_set_text(GTK_LABEL(browser->status), message);
    g_free(message);
}

static void show_context(GtkGestureClick *gesture, gint presses,
                         double x, double y, gpointer userdata)
{
    RoomBrowser *browser = userdata;
    (void)gesture; (void)presses; (void)x; (void)y;
    /* GtkMenuButton is a supported GtkPopover host in GTK4. Do not attach
     * native popovers to GtkListBox or to a GtkOverlay allocation slot. */
    if (browser->context_menu_button &&
        gtk_widget_get_root(browser->context_menu_button))
        gtk_menu_button_popup(GTK_MENU_BUTTON(browser->context_menu_button));
}

GtkWidget *room_browser_build(GtkWidget *center, NativeWorkspace *workspace,
                              RoomWorld world)
{
    RoomBrowser *browser = g_new0(RoomBrowser, 1);
    browser->world = world;
    browser->workspace = workspace;
    browser->page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    browser->list = gtk_list_box_new();
    browser->picture = gtk_picture_new();
    browser->details = gtk_label_new("Select a verified original room.");
    browser->status = gtk_label_new("Loading room catalog...");
    browser->render_button = gtk_button_new_with_label("Render selected room");
    browser->mode = gtk_drop_down_new_from_strings(
        world == ROOM_WORLD_ARIA ? aria_modes : zero_modes);
    GtkWidget *open = gtk_button_new_with_label("Open room in editor");
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *layout = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *left = gtk_scrolled_window_new();
    GtkWidget *right = gtk_scrolled_window_new();
    GtkWidget *menu_button = gtk_menu_button_new();
    browser->context_menu_button = menu_button;
    GtkStringList *areas = gtk_string_list_new(NULL);
    gtk_string_list_append(areas, "All areas");
    for (const char *const *a = area_names(world); *a; ++a)
        gtk_string_list_append(areas, *a);
    /* gtk_drop_down_new() consumes the model reference. Do not unref it. */
    browser->area_filter = gtk_drop_down_new(G_LIST_MODEL(areas), NULL);

    gtk_widget_set_margin_start(browser->page, 10);
    gtk_widget_set_margin_end(browser->page, 10);
    gtk_widget_set_margin_top(browser->page, 8);
    gtk_widget_set_margin_bottom(browser->page, 8);
    gtk_box_append(GTK_BOX(toolbar), gtk_label_new("Area:"));
    gtk_box_append(GTK_BOX(toolbar), browser->area_filter);
    gtk_box_append(GTK_BOX(toolbar), gtk_label_new("Layer:"));
    gtk_box_append(GTK_BOX(toolbar), browser->mode);
    gtk_box_append(GTK_BOX(toolbar), browser->render_button);
    gtk_box_append(GTK_BOX(toolbar), open);
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(menu_button), "view-more-symbolic");
    gtk_widget_set_tooltip_text(menu_button, "Room actions (also via right-click)");
    gtk_box_append(GTK_BOX(toolbar), menu_button);
    gtk_box_append(GTK_BOX(browser->page), toolbar);
    gtk_label_set_xalign(GTK_LABEL(browser->details), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(browser->details), TRUE);
    gtk_box_append(GTK_BOX(browser->page), browser->details);
    gtk_widget_set_hexpand(layout, TRUE);
    gtk_widget_set_vexpand(layout, TRUE);
    gtk_box_append(GTK_BOX(browser->page), layout);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(left), browser->list);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(left),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(left, 250, -1);
    gtk_paned_set_start_child(GTK_PANED(layout), left);
    gtk_paned_set_end_child(GTK_PANED(layout), right);
    gtk_paned_set_position(GTK_PANED(layout), 360);
    gtk_picture_set_can_shrink(GTK_PICTURE(browser->picture), TRUE);
    gtk_widget_set_hexpand(browser->picture, TRUE);
    gtk_widget_set_vexpand(browser->picture, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(right), browser->picture);
    gtk_label_set_xalign(GTK_LABEL(browser->status), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(browser->status), TRUE);
    gtk_box_append(GTK_BOX(browser->page), browser->status);

    /* Explicit future extension point, without pretending there is a runtime. */
    GtkWidget *popover = gtk_popover_new();
    GtkWidget *note = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *create = gtk_button_new_with_label("Create room...");
    GtkWidget *hint = gtk_label_new(
        "Unavailable: authored-room schema and native engine adapter are pending.");
    gtk_widget_set_sensitive(create, FALSE);
    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_widget_set_size_request(hint, 245, -1);
    gtk_box_append(GTK_BOX(note), create);
    gtk_box_append(GTK_BOX(note), hint);
    gtk_popover_set_child(GTK_POPOVER(popover), note);
    /* GtkMenuButton owns its popover and correctly manages its lifecycle. */
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(menu_button), popover);
    g_object_set_data(G_OBJECT(browser->page), "mv-create-popover", popover);
    g_object_set_data(G_OBJECT(browser->page), "mv-room-browser-menu-button", menu_button);
    GtkGesture *right_click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(right_click), GDK_BUTTON_SECONDARY);
    gtk_widget_add_controller(browser->list, GTK_EVENT_CONTROLLER(right_click));
    g_signal_connect(right_click, "pressed", G_CALLBACK(show_context), browser);

    gtk_list_box_set_filter_func(GTK_LIST_BOX(browser->list), filter_room, browser, NULL);
    populate_rooms(browser);
    g_signal_connect(browser->list, "row-selected", G_CALLBACK(room_selected), browser);
    g_signal_connect(browser->list, "row-activated", G_CALLBACK(room_activated), browser);
    g_signal_connect(browser->area_filter, "notify::selected",
                     G_CALLBACK(area_changed), browser);
    g_signal_connect(browser->mode, "notify::selected", G_CALLBACK(mode_changed), browser);
    g_signal_connect(browser->render_button, "clicked", G_CALLBACK(render_selected), browser);
    g_signal_connect(open, "clicked", G_CALLBACK(edit_selected), browser);

    /* The GTK container may dispose the toolbar before the filtered list.
     * Independent strong references prevent callbacks from seeing stale
     * GtkDropDown/GtkLabel/GtkPicture pointers during page destruction. */
    g_object_ref(browser->list);
    g_object_ref(browser->mode);
    g_object_ref(browser->area_filter);
    g_object_ref(browser->render_button);
    g_object_ref(browser->status);
    g_object_ref(browser->details);
    g_object_ref(browser->picture);
    g_object_ref(browser->context_menu_button);
    g_object_set_data_full(G_OBJECT(browser->page), "mv-room-browser-state",
                           browser, room_browser_state_free);
    g_object_set_data(G_OBJECT(browser->page), "mv-world-mode",
                      GUINT_TO_POINTER(world == ROOM_WORLD_ARIA ? 2 : 1));
    g_object_set_data(G_OBJECT(browser->page), "mv-room-browser-list", browser->list);
    gtk_notebook_append_page(GTK_NOTEBOOK(center), browser->page,
        gtk_label_new(world == ROOM_WORLD_ARIA ? "Aria rooms" : "Zero rooms"));
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(center), browser->page, TRUE);
    gtk_notebook_set_tab_detachable(GTK_NOTEBOOK(center), browser->page, TRUE);
    return browser->page;
}
