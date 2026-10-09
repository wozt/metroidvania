/* SPDX-License-Identifier: GPL-3.0-only */
/* GTK4 workspace for decoded native rooms and locally extracted assets. */
#include "native_workspace.h"
#include "world_atlas.h"
#include "story_workspace.h"
#include "room_browser.h"

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    NativeWorkspace *native_workspace;
    GtkWidget *native_page;
    GtkWidget *asset_page;
    GtkWidget *asset_actor;
    GtkWidget *asset_action;
    GtkWidget *asset_frame;
    GtkWidget *asset_picture;
    GtkWidget *asset_status;
    GtkWidget *world_map_page;
    GtkWidget *editing_page; /* Permanent top-level entry for opened documents. */
    GtkWidget *editing_dock; /* Secondary, closable native room tabs. */
    GtkWidget *palette_page, *palette_dock; /* Secondary room palettes. */
    GtkWidget *events_page, *cutscenes_page, *world_badge;
    GtkWidget *aria_page;
    GtkWidget *left_dock;
    GtkWidget *center_dock;
    GtkWidget *right_dock;
    GtkWidget *outer_split, *inner_split;
    GtkWidget *left_viewport, *center_viewport, *right_viewport;
    gboolean updating_responsive;
    GtkWidget *subtitle;
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
    gtk_label_set_selectable(GTK_LABEL(notice), TRUE);
    gtk_label_set_wrap(GTK_LABEL(editor->asset_status), TRUE);
    gtk_label_set_selectable(GTK_LABEL(editor->asset_status), TRUE);
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

static GtkWidget *new_dock(GtkApplication *application);

static GtkNotebook *create_floating_dock(GtkNotebook *notebook, GtkWidget *page,
                                         gpointer userdata)
{
    GtkApplication *application = GTK_APPLICATION(userdata);
    GtkWidget *window = gtk_application_window_new(application);
    GtkWidget *dock = new_dock(application);
    /* Keep permanent workspaces and temporary editors in separate DnD groups.
     * Floating editor windows inherit the source group, so detaching works. */
    const char *group = gtk_notebook_get_group_name(notebook);
    if (group) gtk_notebook_set_group_name(GTK_NOTEBOOK(dock), group);
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

/* A GtkNotebook's active page may request more width than is available.
 * Give the notebook a real, bounded viewport: large toolbars can scroll
 * within their pane instead of drawing over the adjacent editor. This does
 * not change the notebook's parent-child tab hierarchy or detach groups. */
static GtkWidget *dock_viewport(GtkWidget *dock)
{
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_propagate_natural_width(
        GTK_SCROLLED_WINDOW(scroll), FALSE);
    gtk_scrolled_window_set_min_content_width(GTK_SCROLLED_WINDOW(scroll), 0);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scroll), 0);
    gtk_widget_set_hexpand(scroll, TRUE);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_widget_set_overflow(scroll, GTK_OVERFLOW_HIDDEN);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), dock);
    return scroll;
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
    else if (strcmp(action, "events") == 0) focus_dock_page(editor->events_page);
    else if (strcmp(action, "cutscenes") == 0) focus_dock_page(editor->cutscenes_page);
    else if (strcmp(action, "aria") == 0) focus_dock_page(editor->aria_page);
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
    explorer_button(root, editor, "Zero rooms", "native");
    explorer_button(root, editor, "Global maps / both worlds", "world-map");
    explorer_button(root, editor, "Event orchestration", "events");
    explorer_button(root, editor, "Cutscene editor", "cutscenes");
    explorer_button(root, editor, "Aria rooms", "aria");
    explorer_button(root, editor, "Local ROM visuals", "assets");
    gtk_box_append(GTK_BOX(root), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_label_set_selectable(GTK_LABEL(hint), TRUE);
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
        "Aria: same native document editor; original graphics remain partially decoded.\n\n"
        "Boss, entity, collision, music and cutscene editing are pending.");
    gtk_widget_set_margin_start(root, 14);
    gtk_widget_set_margin_end(root, 14);
    gtk_widget_set_margin_top(root, 14);
    gtk_label_set_wrap(GTK_LABEL(text), TRUE);
    gtk_label_set_selectable(GTK_LABEL(text), TRUE);
    gtk_label_set_xalign(GTK_LABEL(text), 0.0f);
    gtk_box_append(GTK_BOX(root), gtk_label_new("PROJECT STATUS"));
    gtk_box_append(GTK_BOX(root), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(root), text);
    gtk_notebook_append_page(GTK_NOTEBOOK(dock), root, gtk_label_new("Inspector"));
}

/* Window breakpoints alone cannot describe the workspace after the user
 * drags the left splitter. Account for the remaining allocation as well.
 * Hysteresis avoids a visible right-dock oscillation near the threshold. */
static void apply_responsive(Editor *editor)
{
    if (editor->updating_responsive) return;
    editor->updating_responsive = TRUE;
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
    } else if (editor->inner_split) {
        int remaining = gtk_widget_get_width(editor->inner_split);
        /* Only show the optional inspector when enough space remains after
         * the Explorer dock. Always leave the center workbench accessible. */
        int threshold = gtk_widget_get_visible(editor->right_dock) ? 880 : 1020;
        if (remaining > 0 && remaining < threshold) right = FALSE;
    }
    gtk_widget_set_visible(editor->left_dock, left);
    gtk_widget_set_visible(editor->center_dock, center);
    gtk_widget_set_visible(editor->right_dock, right);
    /* Hide the containing viewport too: hiding only the notebook would leave
     * an empty pane occupying the same width. */
    gtk_widget_set_visible(editor->left_viewport, left);
    gtk_widget_set_visible(editor->center_viewport, center);
    gtk_widget_set_visible(editor->right_viewport, right);
    gtk_widget_set_visible(editor->subtitle, editor->responsive_mode == 2);
    editor->updating_responsive = FALSE;
}

/* The Paned position changes even when the window itself does not resize. */
static void splitter_position_changed(GObject *pane, GParamSpec *spec,
                                      gpointer userdata)
{
    (void)pane;
    (void)spec;
    apply_responsive(userdata);
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

/* General tabs are permanent; the nested notebook owns temporary room tabs.
 * The badge always describes the active document when Open editors is shown. */
static void editor_show_world(Editor *editor, GtkWidget *page)
{
    guint mode = page ? GPOINTER_TO_UINT(
        g_object_get_data(G_OBJECT(page), "mv-world-mode")) : 0u;
    if (page == editor->aria_page) mode = 2;
    else if (page == editor->native_page) mode = 1;
    gtk_label_set_text(GTK_LABEL(editor->world_badge),
        mode == 2 ? "● ARIA OF SORROW" :
        mode == 1 ? "● METROID: ZERO MISSION" : "◇ SHARED WORKSPACE");
}

static void center_page_changed(GtkNotebook *tabs, GtkWidget *page,
                                guint index, gpointer userdata)
{
    Editor *editor = userdata;
    (void)tabs; (void)index;
    if (page == editor->editing_page) {
        GtkNotebook *documents = GTK_NOTEBOOK(editor->editing_dock);
        gint current = gtk_notebook_get_current_page(documents);
        page = current < 0 ? NULL : gtk_notebook_get_nth_page(documents, current);
    }
    editor_show_world(editor, page);
}

static gboolean editor_workbench_is_active(Editor *editor)
{
    GtkNotebook *main_tabs = GTK_NOTEBOOK(editor->center_dock);
    gint selected = gtk_notebook_get_current_page(main_tabs);
    return selected >= 0 && gtk_notebook_get_nth_page(main_tabs, selected) ==
        editor->editing_page;
}

static void editor_document_changed(GtkNotebook *tabs, GtkWidget *page,
                                    guint index, gpointer userdata)
{
    Editor *editor = userdata;
    (void)tabs; (void)index;
    if (editor_workbench_is_active(editor)) editor_show_world(editor, page);
}

static void editor_document_removed(GtkNotebook *tabs, GtkWidget *page,
                                    guint index, gpointer userdata)
{
    Editor *editor = userdata;
    (void)page; (void)index;
    if (!gtk_notebook_get_n_pages(tabs) && editor_workbench_is_active(editor))
        editor_show_world(editor, NULL);
}

static void build_editor_workbench(Editor *editor, GtkApplication *application)
{
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *hint = gtk_label_new(
        "ROOM EDITORS / TEMPORARY TABS — open a room from a world browser or map. "
        "Close individual documents from their tab. Unsaved edits are confirmed.");
    GtkWidget *documents = new_dock(application);
    gtk_notebook_set_group_name(GTK_NOTEBOOK(documents),
                                "metroidvania-ephemeral-document-docks");
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_label_set_selectable(GTK_LABEL(hint), TRUE);
    gtk_widget_set_margin_start(hint, 8);
    gtk_widget_set_margin_top(hint, 6);
    gtk_widget_set_vexpand(documents, TRUE);
    gtk_box_append(GTK_BOX(page), hint);
    gtk_box_append(GTK_BOX(page), documents);
    editor->editing_page = page;
    editor->editing_dock = documents;
    g_object_set_data(G_OBJECT(page), "mv-permanent-workbench", GUINT_TO_POINTER(1));
    g_object_set_data(G_OBJECT(documents), "mv-ephemeral-document-tabs", GUINT_TO_POINTER(1));
    gtk_notebook_append_page(GTK_NOTEBOOK(editor->center_dock), page,
                             gtk_label_new("Open editors"));
    /* Nested tabs are the sole destination for native room documents. */
    g_signal_connect(documents, "switch-page", G_CALLBACK(editor_document_changed), editor);
    g_signal_connect(documents, "page-removed", G_CALLBACK(editor_document_removed), editor);
}

static void build_palette_workbench(Editor *editor, GtkApplication *application,
                                    GtkWidget *right)
{
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *palettes = new_dock(application);
    gtk_notebook_set_group_name(GTK_NOTEBOOK(palettes),
                                "metroidvania-ephemeral-palette-docks");
    GtkWidget *hint = gtk_label_new("ROOM TOOLS — palettes follow the active room editor.");
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
    gtk_widget_set_margin_start(hint, 8);
    gtk_widget_set_margin_top(hint, 6);
    gtk_widget_set_vexpand(palettes, TRUE);
    gtk_box_append(GTK_BOX(page), hint);
    gtk_box_append(GTK_BOX(page), palettes);
    editor->palette_page = page;
    editor->palette_dock = palettes;
    gtk_notebook_append_page(GTK_NOTEBOOK(right), page,
                             gtk_label_new("Room palettes"));
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
    editor->world_badge = gtk_label_new("● METROID: ZERO MISSION");
    gtk_widget_add_css_class(editor->world_badge, "title-4");

    editor->left_dock = left;
    editor->center_dock = center;
    editor->right_dock = right;
    editor->outer_split = outer_split;
    editor->inner_split = inner_split;
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
    gtk_box_append(GTK_BOX(header), editor->world_badge);
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
    /* Two resizable splitters with isolated viewport allocations: neither
     * Explorer nor a long center toolbar may paint on top of its neighbor. */
    editor->left_viewport = dock_viewport(left);
    editor->center_viewport = dock_viewport(center);
    editor->right_viewport = dock_viewport(right);
    gtk_paned_set_start_child(GTK_PANED(outer_split), editor->left_viewport);
    gtk_paned_set_end_child(GTK_PANED(outer_split), inner_split);
    gtk_paned_set_start_child(GTK_PANED(inner_split), editor->center_viewport);
    gtk_paned_set_end_child(GTK_PANED(inner_split), editor->right_viewport);
    gtk_paned_set_resize_start_child(GTK_PANED(outer_split), FALSE);
    gtk_paned_set_resize_end_child(GTK_PANED(outer_split), TRUE);
    gtk_paned_set_resize_start_child(GTK_PANED(inner_split), TRUE);
    gtk_paned_set_resize_end_child(GTK_PANED(inner_split), FALSE);
    gtk_paned_set_shrink_start_child(GTK_PANED(outer_split), TRUE);
    gtk_paned_set_shrink_end_child(GTK_PANED(outer_split), TRUE);
    gtk_paned_set_shrink_start_child(GTK_PANED(inner_split), TRUE);
    gtk_paned_set_shrink_end_child(GTK_PANED(inner_split), TRUE);
    gtk_paned_set_wide_handle(GTK_PANED(outer_split), TRUE);
    gtk_paned_set_wide_handle(GTK_PANED(inner_split), TRUE);
    gtk_paned_set_position(GTK_PANED(outer_split), 275);
    gtk_paned_set_position(GTK_PANED(inner_split), 980);
    g_signal_connect(outer_split, "notify::position",
                     G_CALLBACK(splitter_position_changed), editor);
    g_signal_connect(inner_split, "notify::position",
                     G_CALLBACK(splitter_position_changed), editor);

    /* The outer center notebook is exclusively for permanent workspaces.
     * NativeMap documents are routed into the secondary notebook below it. */
    editor->native_page = room_browser_build(center, editor->native_workspace, ROOM_WORLD_ZERO);
    editor->world_map_page = world_atlas_build(center, editor->native_workspace, editor->world_badge);
    story_workspace_build(center, &editor->events_page, &editor->cutscenes_page);
    editor->aria_page = room_browser_build(center, editor->native_workspace, ROOM_WORLD_ARIA);
    build_editor_workbench(editor, application);
    build_palette_workbench(editor, application, right);
    native_workspace_build(editor->native_workspace,
                           editor->editing_dock, editor->palette_dock);
    build_assets_tab(editor, right);
    build_inspector(right);
    /* Stable sidebar tabs first; transient palette tools remain nested last. */
    gtk_notebook_reorder_child(GTK_NOTEBOOK(right), editor->palette_page, -1);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(right), 0);
    build_explorer(editor, left);
    g_signal_connect(center, "switch-page", G_CALLBACK(center_page_changed), editor);
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
