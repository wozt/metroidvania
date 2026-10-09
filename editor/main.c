/* SPDX-License-Identifier: GPL-3.0-only */
/* GTK4 workspace for decoded native rooms and locally extracted assets. */
#include "native_workspace.h"
#include "world_atlas.h"
#include "story_workspace.h"
#include "room_browser.h"
#include "object_catalog.h"

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    NativeWorkspace *native_workspace;
    GtkWidget *native_page;
    GtkWidget *world_map_page;
    GtkWidget *object_page;
    GtkWidget *editing_page; /* Permanent top-level entry for opened documents. */
    GtkWidget *editing_dock; /* Secondary, closable native room tabs. */
    GtkWidget *palette_page, *palette_dock; /* Secondary room palettes. */
    GtkWidget *events_page, *cutscenes_page, *world_badge;
    GtkWidget *aria_page;
    GtkWidget *left_dock;
    GtkWidget *center_dock;
    GtkWidget *right_dock;
    GtkWidget *navigation_list; /* Vertical selector of center workspaces. */
    GtkWidget *outer_split, *inner_split;
    GtkWidget *left_viewport, *center_viewport, *right_viewport;
    gboolean updating_responsive;
    GtkWidget *subtitle;
    int responsive_mode;
    int small_focus;
    int seen_width;
} Editor;

static void apply_responsive(Editor *editor);

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

/* PATCH_0087_VERTICAL_WORKSPACE_NAV: left is a compact navigator,
 * NOT a second notebook containing full-size browser pages. Each page is
 * owned by the central notebook; its original tab label supplies the title.
 * This also discovers Map creation, which the world atlas inserts itself. */
static void navigation_select_for_page(Editor *editor, GtkWidget *page)
{
    if (!editor->navigation_list || !page) return;
    for (GtkWidget *child = gtk_widget_get_first_child(editor->navigation_list);
         child; child = gtk_widget_get_next_sibling(child)) {
        if (!GTK_IS_LIST_BOX_ROW(child)) continue;
        if (g_object_get_data(G_OBJECT(child), "mv-workspace-page") == page) {
            GtkListBoxRow *current = gtk_list_box_get_selected_row(
                GTK_LIST_BOX(editor->navigation_list));
            if (current != GTK_LIST_BOX_ROW(child))
                gtk_list_box_select_row(GTK_LIST_BOX(editor->navigation_list),
                                        GTK_LIST_BOX_ROW(child));
            return;
        }
    }
}

static void navigation_row_selected(GtkListBox *list, GtkListBoxRow *row,
                                    gpointer userdata)
{
    Editor *editor = userdata;
    (void)list;
    if (!row) return;
    GtkWidget *page = g_object_get_data(G_OBJECT(row), "mv-workspace-page");
    if (!page) return;
    GtkNotebook *center = GTK_NOTEBOOK(editor->center_dock);
    gint target = gtk_notebook_page_num(center, page);
    if (target < 0) return; /* Never navigate to a detached/destroyed page. */
    if (gtk_notebook_get_current_page(center) != target)
        gtk_notebook_set_current_page(center, target);
    if (editor->responsive_mode == 0) {
        editor->small_focus = 0; /* Narrow layout shows selected content. */
        apply_responsive(editor);
    }
}

static void build_workspace_navigation(Editor *editor)
{
    GtkNotebook *center = GTK_NOTEBOOK(editor->center_dock);
    GtkWidget *list = editor->navigation_list;
    gint count = gtk_notebook_get_n_pages(center);
    if (count < 2) g_error("Missing GTK center workspace pages");
    for (gint i = 0; i < count; ++i) {
        GtkWidget *page = gtk_notebook_get_nth_page(center, i);
        const char *title = gtk_notebook_get_tab_label_text(center, page);
        if (!title || !*title) g_error("Unnamed GTK workspace page");
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label = gtk_label_new(title);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_widget_set_margin_start(label, 12);
        gtk_widget_set_margin_end(label, 8);
        gtk_widget_set_margin_top(label, 7);
        gtk_widget_set_margin_bottom(label, 7);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        g_object_set_data(G_OBJECT(row), "mv-workspace-page", page);
        gtk_list_box_append(GTK_LIST_BOX(list), row);
    }
    g_signal_connect(list, "row-selected", G_CALLBACK(navigation_row_selected), editor);
    gtk_list_box_select_row(GTK_LIST_BOX(list),
                            gtk_list_box_get_row_at_index(GTK_LIST_BOX(list), 0));
}

/* PATCH_0086_SOURCE_WORKSPACE_DOCK: permanent browsing tools live in the source dock. */
static void build_inspector(GtkWidget *dock)
{
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *text = gtk_label_new(
        "Active native scope\n\n"
        "Zero Mission: BG1/BG2 metatile editing and world atlas.\n\n"
        "Aria: same native document editor; original graphics remain partially decoded.\n\n"
        "Native entities and collision are inspectable. Their authoring, plus "
        "boss, music and cutscene engine adapters, remains pending.");
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
        /* Hide the optional inspector when source browsing plus the
         * central document canvas no longer fit comfortably. */
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
    navigation_select_for_page(editor, page);
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

/* A room selected from the source column should bring its document
 * into view even in the narrow one-column responsive layout. */
static void editor_document_added(GtkNotebook *tabs, GtkWidget *page,
                                  guint index, gpointer userdata)
{
    Editor *editor = userdata;
    (void)tabs; (void)index;
    /* Opening a room must show Open editors, not the old room browser. */
    gint workbench = gtk_notebook_page_num(GTK_NOTEBOOK(editor->center_dock),
                                          editor->editing_page);
    if (workbench >= 0)
        gtk_notebook_set_current_page(GTK_NOTEBOOK(editor->center_dock), workbench);
    if (editor->responsive_mode == 0) {
        editor->small_focus = 0;
        apply_responsive(editor);
    }
    editor_show_world(editor, page);
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
    g_signal_connect(documents, "page-added", G_CALLBACK(editor_document_added), editor);
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
    GtkWidget *left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *navigation = gtk_list_box_new();
    GtkWidget *center = new_dock(application);
    GtkWidget *right = new_dock(application);
    GtkWidget *outer_split = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *inner_split = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *sensor = gtk_drawing_area_new();
    editor->world_badge = gtk_label_new("● METROID: ZERO MISSION");
    gtk_widget_add_css_class(editor->world_badge, "title-4");

    editor->left_dock = left;
    editor->navigation_list = navigation;
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(navigation), GTK_SELECTION_SINGLE);
    gtk_widget_add_css_class(navigation, "navigation-sidebar");
    gtk_widget_set_margin_start(navigation, 4);
    gtk_widget_set_margin_end(navigation, 4);
    GtkWidget *navigation_title = gtk_label_new("WORKSPACES");
    gtk_widget_add_css_class(navigation_title, "dim-label");
    gtk_label_set_xalign(GTK_LABEL(navigation_title), 0.0f);
    gtk_widget_set_margin_start(navigation_title, 16);
    gtk_widget_set_margin_top(navigation_title, 9);
    gtk_box_append(GTK_BOX(left), navigation_title);
    gtk_box_append(GTK_BOX(left), navigation);
    editor->center_dock = center;
    editor->right_dock = right;
    editor->outer_split = outer_split;
    editor->inner_split = inner_split;
    editor->subtitle = gtk_label_new("Workspaces · Active editor · Room tools");
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
    gtk_box_append(GTK_BOX(header), make_responsive_button(editor, "Sources", 1));
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
    /* Browsing pages reside in the left source dock, with documents in the
     * center and inspectors on the right. All three remain independently
     * clipped and user-resizable. */
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
    gtk_paned_set_position(GTK_PANED(outer_split), 225);
    gtk_paned_set_position(GTK_PANED(inner_split), 840);
    g_signal_connect(outer_split, "notify::position",
                     G_CALLBACK(splitter_position_changed), editor);
    g_signal_connect(inner_split, "notify::position",
                     G_CALLBACK(splitter_position_changed), editor);

    /* Every permanent page stays in ONE central notebook. Hide its
     * horizontal tabs: the left GtkListBox becomes the sole workspace menu. */
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(center), FALSE);
    editor->native_page = room_browser_build(center, editor->native_workspace, ROOM_WORLD_ZERO);
    editor->aria_page = room_browser_build(center, editor->native_workspace, ROOM_WORLD_ARIA);
    editor->world_map_page = world_atlas_build(center, editor->native_workspace, editor->world_badge);
    editor->object_page = object_catalog_build(center);
    story_workspace_build(center, &editor->events_page, &editor->cutscenes_page);
    build_editor_workbench(editor, application);
    build_palette_workbench(editor, application, right);
    native_workspace_build(editor->native_workspace,
                           editor->editing_dock, editor->palette_dock);
    build_inspector(right);
    /* Stable sidebar tabs first; transient palette tools remain nested last. */
    gtk_notebook_reorder_child(GTK_NOTEBOOK(right), editor->palette_page, -1);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(right), 0);
    /* One left vertical selector, one central page at a time. */
    g_signal_connect(center, "switch-page", G_CALLBACK(center_page_changed), editor);
    build_workspace_navigation(editor);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(center), 0);
    editor_show_world(editor, editor->native_page);
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
