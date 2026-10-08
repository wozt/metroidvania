/* SPDX-License-Identifier: GPL-3.0-only */
/* GTK4 workspace for decoded native rooms and locally extracted assets. */
#include "native_workspace.h"
#include "world_atlas.h"

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    NativeWorkspace *native_workspace;
    GtkWidget *native_page;
    GtkWidget *native_details;
    GtkWidget *native_preview;
    GtkWidget *asset_page;
    GtkWidget *asset_actor;
    GtkWidget *asset_action;
    GtkWidget *asset_frame;
    GtkWidget *asset_picture;
    GtkWidget *asset_status;
    GtkWidget *world_map_page;
    GtkWidget *left_dock;
    GtkWidget *center_dock;
    GtkWidget *right_dock;
    GtkWidget *subtitle;
    char selected_native_area[32];
    unsigned selected_native_index;
    int responsive_mode;
    int small_focus;
    int seen_width;
} Editor;

static void apply_responsive(Editor *editor);

static const char *const asset_actor_names[] = {"Samus", "Soma", NULL};
static const char *const asset_animation_names[] = {
    "idle", "run", "jump", "attack", "run_start", "run_stop", NULL
};
static const unsigned asset_frame_counts[2][6] = {
    {4, 10, 8, 3, 0, 0},
    {4, 17, 12, 11, 3, 9},
};

static void asset_update_preview(Editor *editor)
{
    guint actor = gtk_drop_down_get_selected(GTK_DROP_DOWN(editor->asset_actor));
    guint action = gtk_drop_down_get_selected(GTK_DROP_DOWN(editor->asset_action));
    unsigned count;
    unsigned frame;
    char path[256];
    char message[360];

    if (actor >= 2 || action >= 6) return;
    count = asset_frame_counts[actor][action];
    if (!count) {
        gtk_picture_set_paintable(GTK_PICTURE(editor->asset_picture), NULL);
        gtk_label_set_text(GTK_LABEL(editor->asset_status),
                           "Animation unavailable for this character");
        return;
    }
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(editor->asset_frame), 0, count - 1);
    frame = (unsigned)gtk_spin_button_get_value_as_int(
        GTK_SPIN_BUTTON(editor->asset_frame));
    if (frame >= count) {
        frame = count - 1;
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(editor->asset_frame), frame);
    }
    snprintf(path, sizeof(path), "assets/extracted/sprites/%s/%s_%u.bmp",
             actor == 0 ? "samus" : "soma", asset_animation_names[action], frame);
    if (g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
        gtk_picture_set_filename(GTK_PICTURE(editor->asset_picture), path);
        snprintf(message, sizeof(message),
                 "Local ROM sprite: %s | frame %u/%u", path, frame + 1, count);
    } else {
        gtk_picture_set_paintable(GTK_PICTURE(editor->asset_picture), NULL);
        snprintf(message, sizeof(message),
                 "Sprite not extracted: %s | run the local asset importer", path);
    }
    gtk_label_set_text(GTK_LABEL(editor->asset_status), message);
}

static void asset_selection_changed(GObject *object, GParamSpec *spec,
                                    gpointer userdata)
{
    (void)object;
    (void)spec;
    asset_update_preview(userdata);
}

static void asset_frame_changed(GtkSpinButton *button, gpointer userdata)
{
    (void)button;
    asset_update_preview(userdata);
}

static void asset_refresh_clicked(GtkButton *button, gpointer userdata)
{
    (void)button;
    asset_update_preview(userdata);
}

static void build_assets_tab(Editor *editor, GtkWidget *dock)
{
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *refresh = gtk_button_new_with_label("Refresh local sprites");
    GtkWidget *scroll = gtk_scrolled_window_new();
    GtkWidget *notice = gtk_label_new(
        "Private ROM-derived preview. Extracted files remain ignored by Git.");

    editor->asset_actor = gtk_drop_down_new_from_strings(asset_actor_names);
    editor->asset_action = gtk_drop_down_new_from_strings(asset_animation_names);
    editor->asset_frame = gtk_spin_button_new_with_range(0, 16, 1);
    editor->asset_picture = gtk_picture_new();
    editor->asset_status = gtk_label_new(
        "Load local sprites with: python3 scripts/import_game_assets.py --scope sprites");
    editor->asset_page = root;

    gtk_widget_set_margin_start(root, 12);
    gtk_widget_set_margin_end(root, 12);
    gtk_widget_set_margin_top(root, 12);
    gtk_label_set_wrap(GTK_LABEL(notice), TRUE);
    gtk_label_set_wrap(GTK_LABEL(editor->asset_status), TRUE);
    gtk_picture_set_can_shrink(GTK_PICTURE(editor->asset_picture), TRUE);
    gtk_widget_set_size_request(editor->asset_picture, 180, 230);
    gtk_widget_set_hexpand(editor->asset_picture, TRUE);
    gtk_widget_set_vexpand(editor->asset_picture, TRUE);

    gtk_box_append(GTK_BOX(root), gtk_label_new("Character"));
    gtk_box_append(GTK_BOX(root), editor->asset_actor);
    gtk_box_append(GTK_BOX(root), gtk_label_new("Animation"));
    gtk_box_append(GTK_BOX(root), editor->asset_action);
    gtk_box_append(GTK_BOX(root), gtk_label_new("Frame"));
    gtk_box_append(GTK_BOX(root), editor->asset_frame);
    gtk_box_append(GTK_BOX(root), refresh);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), editor->asset_picture);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_box_append(GTK_BOX(root), scroll);
    gtk_box_append(GTK_BOX(root), editor->asset_status);
    gtk_box_append(GTK_BOX(root), notice);

    g_signal_connect(editor->asset_actor, "notify::selected",
                     G_CALLBACK(asset_selection_changed), editor);
    g_signal_connect(editor->asset_action, "notify::selected",
                     G_CALLBACK(asset_selection_changed), editor);
    g_signal_connect(editor->asset_frame, "value-changed",
                     G_CALLBACK(asset_frame_changed), editor);
    g_signal_connect(refresh, "clicked", G_CALLBACK(asset_refresh_clicked), editor);
    gtk_notebook_append_page(GTK_NOTEBOOK(dock), root, gtk_label_new("ROM visuals"));
    asset_update_preview(editor);
}

static void open_selected_native_room(Editor *editor)
{
    if (!editor->native_workspace || !editor->selected_native_area[0]) return;
    editor->small_focus = 0;
    apply_responsive(editor);
    native_workspace_import_async(editor->native_workspace,
                                  editor->selected_native_area,
                                  editor->selected_native_index);
}

static void native_row_selected(GtkListBox *list, GtkListBoxRow *row,
                                gpointer userdata)
{
    Editor *editor = userdata;
    const char *details;
    const char *area;
    const char *preview;
    (void)list;

    if (!row || !editor->native_details) return;
    details = g_object_get_data(G_OBJECT(row), "native-room-details");
    area = g_object_get_data(G_OBJECT(row), "native-area");
    preview = g_object_get_data(G_OBJECT(row), "native-room-preview");
    if (details) gtk_label_set_text(GTK_LABEL(editor->native_details), details);
    if (area) {
        snprintf(editor->selected_native_area,
                 sizeof(editor->selected_native_area), "%s", area);
        editor->selected_native_index = GPOINTER_TO_UINT(
            g_object_get_data(G_OBJECT(row), "native-index"));
    }
    if (preview && g_file_test(preview, G_FILE_TEST_IS_REGULAR))
        gtk_picture_set_filename(GTK_PICTURE(editor->native_preview), preview);
    else
        gtk_picture_set_paintable(GTK_PICTURE(editor->native_preview), NULL);
}

static void native_open_clicked(GtkButton *button, gpointer userdata)
{
    (void)button;
    open_selected_native_room(userdata);
}

static void native_row_activated(GtkListBox *list, GtkListBoxRow *row,
                                 gpointer userdata)
{
    native_row_selected(list, row, userdata);
    open_selected_native_room(userdata);
}

static void build_native_rooms_tab(Editor *editor, GtkWidget *dock)
{
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *scroll = gtk_scrolled_window_new();
    GtkWidget *list = gtk_list_box_new();
    GtkWidget *open = gtk_button_new_with_label("Open room in a new tab");
    GtkWidget *preview = gtk_picture_new();
    gchar *contents = NULL;
    gchar **lines;
    gsize length = 0;
    size_t index;

    editor->native_page = page;
    editor->native_preview = preview;
    editor->native_details = gtk_label_new(
        "Choose a room to inspect its source descriptors.");
    gtk_widget_set_margin_start(page, 12);
    gtk_widget_set_margin_end(page, 12);
    gtk_widget_set_margin_top(page, 12);
    gtk_box_append(GTK_BOX(page), gtk_label_new(
        "Zero Mission native room browser | double-click to edit BG1/BG2 privately"));
    gtk_box_append(GTK_BOX(page), open);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), list);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_box_append(GTK_BOX(page), scroll);
    gtk_widget_set_size_request(preview, 200, 160);
    gtk_picture_set_can_shrink(GTK_PICTURE(preview), TRUE);
    gtk_box_append(GTK_BOX(page), preview);
    gtk_label_set_wrap(GTK_LABEL(editor->native_details), TRUE);
    gtk_label_set_xalign(GTK_LABEL(editor->native_details), 0.0f);
    gtk_box_append(GTK_BOX(page), editor->native_details);
    g_signal_connect(open, "clicked", G_CALLBACK(native_open_clicked), editor);

    if (!g_file_get_contents("assets/extracted/rooms/metroid/rooms.tsv",
                             &contents, &length, NULL)) {
        gtk_label_set_text(GTK_LABEL(editor->native_details),
            "Room catalog unavailable. Run the local asset importer first.");
        gtk_notebook_append_page(GTK_NOTEBOOK(dock), page,
                                 gtk_label_new("Native rooms"));
        return;
    }

    lines = g_strsplit(contents, "\n", -1);
    for (index = 0; lines[index]; ++index) {
        gchar **parts;
        GtkWidget *label;
        GtkWidget *row;
        gchar *name;
        gchar *details;
        gchar *lower_area;
        gchar *filename;
        unsigned room;

        if (!lines[index][0] || lines[index][0] == '#') continue;
        parts = g_strsplit(lines[index], "|", -1);
        if (g_strv_length(parts) != 10) {
            g_strfreev(parts);
            continue;
        }
        room = (unsigned)g_ascii_strtoull(parts[1], NULL, 10);
        name = g_strdup_printf("%s / room %s    tileset %s    %s",
                               parts[0], parts[1], parts[2], parts[3]);
        details = g_strdup_printf(
            "Area: %s  room: %s    source tileset: %s\n"
            "Music: %s\nBG1: %s\nBG2: %s\nClipdata: %s\n"
            "Default spriteset: %s\nWorld map position: %s,%s",
            parts[0], parts[1], parts[2], parts[3], parts[4], parts[5],
            parts[6], parts[7], parts[8], parts[9]);
        lower_area = g_ascii_strdown(parts[0], -1);
        filename = g_strdup_printf(
            "assets/extracted/rooms/metroid/previews/%s_%03u_bg1.bmp",
            lower_area, room);
        g_free(lower_area);

        label = gtk_label_new(name);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        row = gtk_list_box_row_new();
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        g_object_set_data_full(G_OBJECT(row), "native-room-details", details, g_free);
        g_object_set_data_full(G_OBJECT(row), "native-area",
                               g_strdup(parts[0]), g_free);
        g_object_set_data(G_OBJECT(row), "native-index", GUINT_TO_POINTER(room));
        g_object_set_data_full(G_OBJECT(row), "native-room-preview", filename, g_free);
        gtk_list_box_append(GTK_LIST_BOX(list), row);
        g_free(name);
        g_strfreev(parts);
    }
    g_strfreev(lines);
    g_free(contents);
    g_signal_connect(list, "row-selected", G_CALLBACK(native_row_selected), editor);
    g_signal_connect(list, "row-activated", G_CALLBACK(native_row_activated), editor);
    gtk_notebook_append_page(GTK_NOTEBOOK(dock), page, gtk_label_new("Native rooms"));
}

static GtkWidget *new_dock(GtkApplication *application);

static GtkNotebook *create_floating_dock(GtkNotebook *notebook, GtkWidget *page,
                                         gpointer userdata)
{
    GtkApplication *application = GTK_APPLICATION(userdata);
    GtkWidget *window = gtk_application_window_new(application);
    GtkWidget *dock = new_dock(application);
    (void)notebook;
    (void)page;
    gtk_window_set_title(GTK_WINDOW(window), "Metroid Vania - Detached tools");
    gtk_window_set_default_size(GTK_WINDOW(window), 780, 520);
    gtk_window_set_child(GTK_WINDOW(window), dock);
    gtk_window_present(GTK_WINDOW(window));
    return GTK_NOTEBOOK(dock);
}

static void dock_page_added(GtkNotebook *notebook, GtkWidget *child,
                            guint index, gpointer userdata)
{
    (void)index;
    (void)userdata;
    gtk_notebook_set_tab_reorderable(notebook, child, TRUE);
    gtk_notebook_set_tab_detachable(notebook, child, TRUE);
}

static GtkWidget *new_dock(GtkApplication *application)
{
    GtkWidget *dock = gtk_notebook_new();
    gtk_notebook_set_group_name(GTK_NOTEBOOK(dock), "metroidvania-native-docks");
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(dock), TRUE);
    gtk_widget_set_hexpand(dock, TRUE);
    gtk_widget_set_vexpand(dock, TRUE);
    g_signal_connect(dock, "page-added", G_CALLBACK(dock_page_added), NULL);
    g_signal_connect(dock, "create-window",
                     G_CALLBACK(create_floating_dock), application);
    return dock;
}

static void focus_dock_page(GtkWidget *page)
{
    GtkWidget *parent = page ? gtk_widget_get_parent(page) : NULL;
    if (parent && GTK_IS_NOTEBOOK(parent)) {
        GtkRoot *root;
        gtk_notebook_set_current_page(GTK_NOTEBOOK(parent),
                                      gtk_notebook_page_num(GTK_NOTEBOOK(parent), page));
        root = gtk_widget_get_root(parent);
        if (GTK_IS_WINDOW(root)) gtk_window_present(GTK_WINDOW(root));
    }
}

static void explorer_action(GtkButton *button, gpointer userdata)
{
    Editor *editor = userdata;
    const char *action = g_object_get_data(G_OBJECT(button), "editor-action");
    if (!action) return;
    if (strcmp(action, "native") == 0) focus_dock_page(editor->native_page);
    else if (strcmp(action, "world-map") == 0)
        focus_dock_page(editor->world_map_page);
    else if (strcmp(action, "assets") == 0) focus_dock_page(editor->asset_page);
    if (editor->responsive_mode == 0) {
        editor->small_focus = 0;
        apply_responsive(editor);
    }
}

static void explorer_button(GtkWidget *parent, Editor *editor,
                            const char *label, const char *action)
{
    GtkWidget *button = gtk_button_new_with_label(label);
    g_object_set_data(G_OBJECT(button), "editor-action", (gpointer)action);
    g_signal_connect(button, "clicked", G_CALLBACK(explorer_action), editor);
    gtk_box_append(GTK_BOX(parent), button);
}

static void build_explorer(Editor *editor, GtkWidget *dock)
{
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 9);
    GtkWidget *hint = gtk_label_new(
        "Native project workspace\n\n"
        "Drag tabs between columns or detach them.\n\n"
        "ROM data and edited overrides remain local.");
    gtk_widget_set_margin_start(root, 12);
    gtk_widget_set_margin_end(root, 12);
    gtk_widget_set_margin_top(root, 12);
    gtk_box_append(GTK_BOX(root), gtk_label_new("PROJECT / METROID VANIA"));
    gtk_box_append(GTK_BOX(root), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    explorer_button(root, editor, "Zero Mission native rooms", "native");
    explorer_button(root, editor, "Zero Mission world map", "world-map");
    explorer_button(root, editor, "Local ROM visuals", "assets");
    gtk_box_append(GTK_BOX(root), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
    gtk_box_append(GTK_BOX(root), hint);
    gtk_notebook_append_page(GTK_NOTEBOOK(dock), root, gtk_label_new("Explorer"));
}

static void build_inspector(GtkWidget *dock)
{
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *text = gtk_label_new(
        "Active native scope\n\n"
        "Zero Mission: BG1/BG2 metatile editing and world atlas.\n\n"
        "Aria: save-room directory metadata only. Native room rendering and editing are pending.\n\n"
        "Boss, entity, collision, music and cutscene editing are pending.");
    gtk_widget_set_margin_start(root, 14);
    gtk_widget_set_margin_end(root, 14);
    gtk_widget_set_margin_top(root, 14);
    gtk_label_set_wrap(GTK_LABEL(text), TRUE);
    gtk_label_set_xalign(GTK_LABEL(text), 0.0f);
    gtk_box_append(GTK_BOX(root), gtk_label_new("PROJECT STATUS"));
    gtk_box_append(GTK_BOX(root), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(root), text);
    gtk_notebook_append_page(GTK_NOTEBOOK(dock), root, gtk_label_new("Inspector"));
}

static void apply_responsive(Editor *editor)
{
    gboolean left = TRUE;
    gboolean center = TRUE;
    gboolean right = TRUE;
    if (editor->responsive_mode == 0) {
        left = editor->small_focus == 1;
        center = editor->small_focus == 0;
        right = editor->small_focus == 2;
    } else if (editor->responsive_mode == 1) {
        left = editor->small_focus != 2;
        right = editor->small_focus == 2;
    }
    gtk_widget_set_visible(editor->left_dock, left);
    gtk_widget_set_visible(editor->center_dock, center);
    gtk_widget_set_visible(editor->right_dock, right);
    gtk_widget_set_visible(editor->subtitle, editor->responsive_mode == 2);
}

static gboolean responsive_idle(gpointer userdata)
{
    Editor *editor = userdata;
    int mode = editor->seen_width < 920 ? 0 : editor->seen_width < 1450 ? 1 : 2;
    if (editor->responsive_mode != mode) {
        editor->responsive_mode = mode;
        editor->small_focus = 0;
    }
    apply_responsive(editor);
    return G_SOURCE_REMOVE;
}

static void responsive_sensor(GtkDrawingArea *area, cairo_t *cr,
                              int width, int height, gpointer userdata)
{
    Editor *editor = userdata;
    GtkRoot *root = gtk_widget_get_root(GTK_WIDGET(area));
    int current_width;
    (void)cr;
    (void)width;
    (void)height;
    if (!GTK_IS_WINDOW(root)) return;
    current_width = gtk_widget_get_width(GTK_WIDGET(root));
    if (current_width > 0 && current_width != editor->seen_width) {
        editor->seen_width = current_width;
        g_idle_add(responsive_idle, editor);
    }
}

static void responsive_button(GtkButton *button, gpointer userdata)
{
    Editor *editor = userdata;
    int requested = GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(button), "focus"));
    editor->small_focus = editor->small_focus == requested ? 0 : requested;
    apply_responsive(editor);
}

static GtkWidget *make_responsive_button(Editor *editor,
                                         const char *label, int focus)
{
    GtkWidget *button = gtk_button_new_with_label(label);
    g_object_set_data(G_OBJECT(button), "focus", GINT_TO_POINTER(focus));
    g_signal_connect(button, "clicked", G_CALLBACK(responsive_button), editor);
    return button;
}

static void activate(GtkApplication *application, gpointer userdata)
{
    Editor *editor = userdata;
    GtkWidget *window = gtk_application_window_new(application);
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *left = new_dock(application);
    GtkWidget *center = new_dock(application);
    GtkWidget *right = new_dock(application);
    GtkWidget *outer_split = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *inner_split = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *sensor = gtk_drawing_area_new();

    editor->left_dock = left;
    editor->center_dock = center;
    editor->right_dock = right;
    editor->subtitle = gtk_label_new("Native room and asset workspace");
    editor->responsive_mode = 2;
    editor->small_focus = 0;

    gtk_window_set_title(GTK_WINDOW(window), "Metroid Vania - Native Editor");
    gtk_window_set_default_size(GTK_WINDOW(window), 1680, 980);
    gtk_window_set_child(GTK_WINDOW(window), root);
    gtk_widget_set_margin_start(header, 14);
    gtk_widget_set_margin_end(header, 14);
    gtk_widget_set_margin_top(header, 8);
    gtk_widget_set_margin_bottom(header, 8);
    gtk_box_append(GTK_BOX(header), gtk_label_new("METROID VANIA / NATIVE EDITOR"));
    gtk_box_append(GTK_BOX(header), make_responsive_button(editor, "Explorer", 1));
    gtk_box_append(GTK_BOX(header), make_responsive_button(editor, "Canvas", 0));
    gtk_box_append(GTK_BOX(header), make_responsive_button(editor, "Inspector", 2));
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(sensor), 1);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(sensor), 1);
    gtk_widget_set_hexpand(sensor, TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(sensor),
                                   responsive_sensor, editor, NULL);
    gtk_box_append(GTK_BOX(header), sensor);
    gtk_box_append(GTK_BOX(header), editor->subtitle);
    gtk_box_append(GTK_BOX(root), header);
    gtk_box_append(GTK_BOX(root), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_widget_set_vexpand(outer_split, TRUE);
    gtk_box_append(GTK_BOX(root), outer_split);
    gtk_paned_set_start_child(GTK_PANED(outer_split), left);
    gtk_paned_set_end_child(GTK_PANED(outer_split), inner_split);
    gtk_paned_set_start_child(GTK_PANED(inner_split), center);
    gtk_paned_set_end_child(GTK_PANED(inner_split), right);
    gtk_paned_set_position(GTK_PANED(outer_split), 275);
    gtk_paned_set_position(GTK_PANED(inner_split), 980);

    build_native_rooms_tab(editor, center);
    native_workspace_build(editor->native_workspace, center, right);
    editor->world_map_page = world_atlas_build(center, editor->native_workspace);
    build_assets_tab(editor, right);
    build_inspector(right);
    build_explorer(editor, left);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(center), 0);
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv)
{
    Editor editor = {0};
    GtkApplication *application;
    int result;
    (void)argc;

    editor.native_workspace = native_workspace_new();
    if (!editor.native_workspace) {
        fputs("Native workspace allocation failed\n", stderr);
        return 1;
    }
    application = gtk_application_new("org.metroidvania.nativeeditor",
                                      G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(application, "activate", G_CALLBACK(activate), &editor);
    result = g_application_run(G_APPLICATION(application), 1, argv);
    g_object_unref(application);
    native_workspace_free(editor.native_workspace);
    return result;
}
