/* SPDX-License-Identifier: GPL-3.0-only */
/* Source-level contracts of the GTK editor, the runtime and the story data:
 * the structural guarantees the former Python contract tests checked by
 * reading the sources (patches 0086-0169). */
#define _XOPEN_SOURCE 700
#include "core/json.h"
#include "core/toml.h"

#include <ctype.h>
#include <ftw.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef FUSION_SOURCE_DIR
#error "FUSION_SOURCE_DIR must identify the repository source directory"
#endif

static int failures = 0;

static void fail(const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    fputs("FAIL: ", stderr);
    vfprintf(stderr, format, arguments);
    fputc('\n', stderr);
    va_end(arguments);
    ++failures;
}

static char *text_of(const char *relative) {
    char path[4096];
    snprintf(path, sizeof path, "%s/%s", FUSION_SOURCE_DIR, relative);
    FILE *file = fopen(path, "rb");
    if (!file) {
        fail("cannot read %s", relative);
        return calloc(1, 1);
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    char *text = malloc((size_t)size + 1);
    size_t read = text ? fread(text, 1, (size_t)size, file) : 0;
    fclose(file);
    if (!text) exit(2);
    text[read] = '\0';
    return text;
}

static void has(const char *text, const char *needle, const char *where) {
    if (!strstr(text, needle)) fail("%s: missing \"%s\"", where, needle);
}

static void lacks(const char *text, const char *needle, const char *where) {
    if (strstr(text, needle)) fail("%s: unexpected \"%s\"", where, needle);
}

static size_t count(const char *text, const char *needle) {
    size_t found = 0, length = strlen(needle);
    for (const char *at = strstr(text, needle); at; at = strstr(at + length, needle)) ++found;
    return found;
}

/* The offset of ``needle`` at or after ``from``, or -1 (a failure). */
static long position(const char *text, const char *needle, long from, const char *where) {
    const char *at = strstr(text + from, needle);
    if (!at) {
        fail("%s: missing \"%s\"", where, needle);
        return -1;
    }
    return (long)(at - text);
}

/* text.split(start, 1)[1].split(end, 1)[0], as a new string. */
static char *section(const char *text, const char *start, const char *end, const char *where) {
    const char *from = strstr(text, start);
    if (!from) {
        fail("%s: missing section start \"%s\"", where, start);
        return calloc(1, 1);
    }
    from += strlen(start);
    const char *to = end ? strstr(from, end) : NULL;
    size_t length = to ? (size_t)(to - from) : strlen(from);
    if (end && !to) fail("%s: missing section end \"%s\"", where, end);
    char *copy = malloc(length + 1);
    if (!copy) exit(2);
    memcpy(copy, from, length);
    copy[length] = '\0';
    return copy;
}

static void less(long left, long right, const char *what) {
    if (left < 0 || right < 0 || left >= right) fail("%s: order", what);
}

static void test_runtime_contracts(void) {
    char *runtime = text_of("src/runtime/room_runtime.c");
    has(runtime, "if (kind == 'N' && (code == 16 || code == 17 || code == 18)) {", "0140");
    has(runtime, "verified full-solid cells (Clipdata 16)", "0140");
    lacks(runtime, "if (kind == 'N' && code == 5)", "0140");
    has(runtime, "mzm_samus_graphics_key(samus,equipment,weapon_highlighted", "0169");
    char *samus = text_of("src/runtime/mzm_samus.c");
    has(samus, "table = \"ScrewAttacking\";", "0169");
    has(samus, "selector = eq->items & MZM_ITEM_SPACE_JUMP ? \"TRUE\" : \"FALSE\";", "0169");
    char *cmake = text_of("CMakeLists.txt");
    has(cmake, "add_test(NAME mzm_samus_controller", "0169");
    /* 0106b: both GTK targets link the real world atlas picker. */
    static const char *const targets[] = {"fusion_native_workspace_gtk_tests",
                                          "fusion_room_browser_gtk_tests"};
    for (size_t index = 0; index < 2; ++index) {
        char start[128];
        snprintf(start, sizeof start, "add_executable(%s", targets[index]);
        char *part = section(cmake, start, "\n        )", targets[index]);
        has(part, "editor/native_workspace.c", targets[index]);
        has(part, "editor/world_atlas.c", targets[index]);
        free(part);
    }
    free(cmake);
    free(samus);
    free(runtime);
}

/* The game never selects bitmap display modes 3-5. */
static bool modes[6];

static int scan_mode(const char *path, const struct stat *status, int type, struct FTW *walk) {
    (void)status;
    (void)walk;
    size_t length = strlen(path);
    if (type != FTW_F || length < 2 || strcmp(path + length - 2, ".c")) return 0;
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    char line[4096];
    while (fgets(line, sizeof line, file)) {
        for (char *at = strstr(line, "DCNT_MODE_"); at; at = strstr(at + 1, "DCNT_MODE_")) {
            char digit = at[10];
            bool before = at == line || !(isalnum((unsigned char)at[-1]) || at[-1] == '_');
            bool after = !(isalnum((unsigned char)at[11]) || at[11] == '_');
            if (before && after && digit >= '0' && digit <= '5') modes[digit - '0'] = true;
        }
    }
    fclose(file);
    return 0;
}

static void test_display_modes(void) {
    char root[4096];
    snprintf(root, sizeof root, "%s/third_party/mzm/src", FUSION_SOURCE_DIR);
    if (nftw(root, scan_mode, 16, FTW_PHYS) != 0) fail("cannot scan %s", root);
    for (int mode = 3; mode <= 5; ++mode)
        if (modes[mode]) fail("Zero Mission selects bitmap mode %d", mode);
}

static void test_world_atlas_contracts(void) {
    char *atlas = text_of("editor/world_atlas.c");
    static const char *const navigation[] = {
        "map_drag_begin", "map_drag_update", "map_context_pressed", "map_context_enable",
        "GDK_BUTTON_SECONDARY", "gtk_popover_popup", "Copy cell coordinates", "mv-span-width",
        "mv-span-height", "map_creator_build(center)", "mv-create-room-action",
        "placement of new drafts", "Reuse the", "existing authoring page",
        "gboolean world_atlas_begin_room_pick(", "void world_atlas_abandon_room_pick("};
    for (size_t index = 0; index < sizeof navigation / sizeof *navigation; ++index)
        has(atlas, navigation[index], "world atlas navigation");
    char *grid = section(atlas, "static void grid_rebuild(WorldGrid *w)\n{",
                         "\nstatic void begin_generation(WorldGrid *w);", "grid_rebuild");
    /* 0095: one rectangle partition, screen mosaics, independent unowned cells. */
    lacks(grid, "w->world && c->provenance != 3", "0095");
    if (count(grid, "same_atlas_group(w, c, other)") != 2) fail("0095: same_atlas_group count");
    has(atlas, "static gboolean same_atlas_group(", "0095");
    has(atlas, "a->provenance <= 1 && b->provenance <= 1", "0095");
    has(atlas, "a->room == 999", "0095");
    static const char *const mosaic[] = {
        "GtkWidget *mosaic = gtk_grid_new();", "gtk_grid_attach(GTK_GRID(mosaic), tile,",
        "gtk_box_append(GTK_BOX(cell_content), mosaic);",
        "gtk_picture_set_content_fit(GTK_PICTURE(picture), GTK_CONTENT_FIT_FILL);",
        "has_image(w, part, tile_path, sizeof(tile_path))",
        "map_cell_door_badge(tile, native_doors, part);", "c->provenance != 3 && c->room != 999",
        "GtkWidget *grid_extent = gtk_box_new"};
    for (size_t index = 0; index < sizeof mosaic / sizeof *mosaic; ++index)
        has(grid, mosaic[index], "0095 grid");
    lacks(atlas, "mv-join-left", "0095");
    has(atlas, "a->provenance == 3 || b->provenance == 3", "0095");
    lacks(grid, "GtkWidget *empty = gtk_box_new", "0117");
    /* The pan uses the stationary scroller over one empty background. */
    static const char *const pan[] = {
        "gtk_widget_add_controller(scroller, GTK_EVENT_CONTROLLER(pan));",
        "gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller),pan_surface);",
        "gtk_widget_add_tick_callback(w->scroller, map_pan_frame, w, NULL)",
        "gtk_overlay_set_child(GTK_OVERLAY(map_overlay), w->map_background);",
        "gtk_overlay_add_overlay(GTK_OVERLAY(map_overlay), grid);",
        "gtk_box_append(GTK_BOX(canvas_row),map_overlay);",
        "gtk_widget_set_can_target(w->connection_canvas, FALSE);", "map_background_draw_0117",
        "GtkWidget *bar=gtk_flow_box_new();"};
    for (size_t index = 0; index < sizeof pan / sizeof *pan; ++index)
        has(atlas, pan[index], "atlas pan");
    lacks(atlas, "gtk_widget_add_controller(grid, GTK_EVENT_CONTROLLER(pan));", "atlas pan");
    lacks(atlas, "gtk_widget_add_controller(pan_surface, GTK_EVENT_CONTROLLER(pan));", "atlas pan");
    lacks(atlas, "gtk_widget_set_size_request(right_space,720,-1);", "atlas pan");
    lacks(atlas, "gtk_widget_set_size_request(bottom_space,-1,520);", "atlas pan");
    /* 0106: the one-shot room picker. */
    char *clicked = section(atlas, "static void grid_clicked(", "static gboolean has_image(",
                            "grid_clicked");
    has(clicked, "c.provenance == 3 || c.room == 999", "0106");
    has(clicked, "c.provenance == 4", "0106");
    has(clicked, "world_atlas_pick_finish_0106(w, TRUE, c.area, c.room);", "0106");
    less(position(clicked, "world_atlas_pick_finish_0106(w, TRUE", 0, "0106"),
         position(clicked, "if (presses >= 2) selected_open(w);", 0, "0106"), "0106 pick first");
    static const char *const picker[] = {
        "w->pick_callback = NULL;", "w->pick_data = NULL;",
        "world_atlas_abandon_room_pick(GtkWidget *page, gpointer userdata)",
        "world_atlas_pick_cancel_clicked_0106", "\"Cancel picking\"",
        "if (w->pick_callback)\n        world_atlas_pick_finish_0106", "w->pick_data != userdata",
        "gtk_notebook_set_current_page(GTK_NOTEBOOK(ancestor)"};
    for (size_t index = 0; index < sizeof picker / sizeof *picker; ++index)
        has(atlas, picker[index], "0106 picker");
    free(clicked);
    free(grid);
    free(atlas);
}

static void test_main_window_contracts(void) {
    char *main_c = text_of("editor/main.c");
    static const char *const docks[] = {
        "static GtkWidget *dock_viewport(GtkWidget *dock)",
        "gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), dock);",
        "gtk_widget_set_overflow(scroll, GTK_OVERFLOW_HIDDEN);",
        "editor->left_viewport = dock_viewport(left);",
        "gtk_widget_set_visible(editor->left_viewport, left);",
        "editor->center_viewport = dock_viewport(center);",
        "gtk_widget_set_visible(editor->center_viewport, center);",
        "editor->right_viewport = dock_viewport(right);",
        "gtk_widget_set_visible(editor->right_viewport, right);",
        "gtk_paned_set_shrink_start_child(GTK_PANED(outer_split), TRUE);",
        "gtk_paned_set_shrink_end_child(GTK_PANED(outer_split), TRUE);",
        "gtk_paned_set_shrink_start_child(GTK_PANED(inner_split), TRUE);",
        "gtk_paned_set_shrink_end_child(GTK_PANED(inner_split), TRUE);",
        "g_signal_connect(outer_split, \"notify::position\",",
        "g_signal_connect(inner_split, \"notify::position\",",
        "gtk_widget_get_width(editor->inner_split)",
        "if (remaining > 0 && remaining < threshold) right = FALSE;",
        "editor->updating_responsive = TRUE;", "editor->updating_responsive = FALSE;",
        "build_editor_workbench(editor, application);",
        "editor->editing_dock, editor->palette_dock);",
        "\"metroidvania-ephemeral-document-docks\"",
        "build_palette_workbench(editor, application, right);",
        /* 0087: the vertical workspace navigation. */
        "PATCH_0087_VERTICAL_WORKSPACE_NAV",
        "gtk_notebook_set_show_tabs(GTK_NOTEBOOK(center), FALSE);",
        "GtkWidget *left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);",
        "GtkWidget *navigation = gtk_list_box_new();",
        "gtk_list_box_append(GTK_LIST_BOX(list), row);",
        "gtk_notebook_get_tab_label_text(center, page)", "gtk_label_new(\"Open editors\")",
        "g_object_set_data(G_OBJECT(row), \"mv-workspace-page\", page);",
        "G_CALLBACK(navigation_row_selected)", "navigation_select_for_page(editor, page);",
        "gtk_notebook_page_num(center, page)", "gtk_notebook_set_current_page(center, target);",
        "gtk_notebook_page_num(GTK_NOTEBOOK(editor->center_dock),", "editor->editing_page);",
        "gtk_notebook_set_current_page(GTK_NOTEBOOK(editor->center_dock), workbench);",
        "build_inspector(right);", "native_workspace_build(editor->native_workspace,",
        /* 0086: the source workspace dock. */
        "PATCH_0086_SOURCE_WORKSPACE_DOCK", "make_responsive_button(editor, \"Sources\", 1)",
        "g_signal_connect(list, \"row-selected\", G_CALLBACK(navigation_row_selected)",
        "editor_show_world(editor, editor->native_page);",
        "gtk_notebook_append_page(GTK_NOTEBOOK(editor->center_dock), page,",
        "G_CALLBACK(editor_document_added)", "editor->small_focus = 0;",
        "editor->object_page = object_catalog_build(center);",
        "native_workspace_set_world_atlas(editor->native_workspace, editor->world_map_page)"};
    for (size_t index = 0; index < sizeof docks / sizeof *docks; ++index)
        has(main_c, docks[index], "main window");
    static const char *const removed[] = {
        "gtk_paned_set_start_child(GTK_PANED(outer_split), left);",
        "g_signal_connect(left, \"switch-page\",", "build_assets_tab(", "\"ROM visuals\"",
        "asset_update_preview(", "static void build_explorer(", "static void explorer_button(",
        "build_explorer(editor, left);", "room_browser_build(left,"};
    for (size_t index = 0; index < sizeof removed / sizeof *removed; ++index)
        lacks(main_c, removed[index], "main window");
    const char *zero = "room_browser_build(center, editor->native_workspace, ROOM_WORLD_ZERO)";
    const char *aria = "room_browser_build(center, editor->native_workspace, ROOM_WORLD_ARIA)";
    long zero_at = position(main_c, zero, 0, "browsers"), aria_at = position(main_c, aria, 0, "browsers");
    less(zero_at, aria_at, "zero before aria");
    less(aria_at, position(main_c, "world_atlas_build(center,", 0, "atlas"), "aria before atlas");
    long workbench = aria_at >= 0 ? position(main_c, "build_editor_workbench(editor, application);",
                                             aria_at, "workbench") : -1;
    less(position(main_c, "story_workspace_build(center,", 0, "story"), workbench,
         "story before editors");
    if (zero_at >= 0 && workbench > zero_at) {
        long length = position(main_c, "build_editor_workbench(editor, application);", zero_at,
                               "workbench") - zero_at;
        char *part = malloc((size_t)length + 1);
        memcpy(part, main_c + zero_at, (size_t)length);
        part[length] = '\0';
        char *before_aria = strstr(part, aria);
        if (before_aria) {
            *before_aria = '\0';
            lacks(part, "world_atlas_build", "browsers adjacent");
            *before_aria = 'r';
        }
        has(part, "world_atlas_build(center,", "sources");
        has(part, "object_catalog_build(center)", "sources");
        has(part, "story_workspace_build(center,", "sources");
        free(part);
    }
    free(main_c);
}

static void test_native_workspace_contracts(void) {
    char *source = text_of("editor/native_workspace.c");
    /* 0113b: semantic toolbar names. */
    has(source, "PATCH_0113B_SEMANTIC_TOOLBAR_NAMES", "0113b");
    has(source, "\"mv-room-toggle-name\", (gpointer)overlay_labels[i]", "0113b");
    has(source, "\"mv-room-toggle-name\", (gpointer)\"Animate hatches\"", "0113b");
    char *gtk_test = text_of("tests/test_native_workspace_gtk.c");
    char *find = section(gtk_test, "static GtkWidget *find_room_toggle(",
                         "static void test_native_door_bitflags_imported(", "find_room_toggle");
    has(find, "g_object_get_data(G_OBJECT(root), \"mv-room-toggle-name\")", "0113b");
    has(find, "g_object_get_data(G_OBJECT(root), \"mv-room-tool-name\")", "0113b");
    has(find, "gtk_button_get_label(GTK_BUTTON(root))", "0113b");
    has(gtk_test, "native-doors-default-visible", "0113b");
    has(gtk_test, "native-door-bitflags-imported", "0113b");
    has(gtk_test, "hatch-preview-lifetime", "0113b");
    has(gtk_test, "/native-workspace/shared-tile-project-undo-redo", "0112");
    has(gtk_test, "native_workspace_test_shared_history(workspace, 0)", "0112");
    free(find);
    free(gtk_test);
    /* 0111 and 0112: one environment owner and pre-command snapshots. */
    char *command = section(source, "static gboolean project_command(",
                            "static guint project_collision_type(", "project_command");
    long start = position(command, "gchar **environment = g_get_environ();", 0, "0111");
    long end = start >= 0 ? position(command, "gboolean success = launched", start, "0111") : -1;
    if (start >= 0 && end > start) {
        char *block = malloc((size_t)(end - start) + 1);
        memcpy(block, command + start, (size_t)(end - start));
        block[end - start] = '\0';
        has(block, "environment = g_environ_setenv(environment, \"MV_EDITOR_ROOM_STAGE\",", "0111");
        has(block, "g_spawn_sync(NULL, (gchar **)args->pdata, environment,", "0111");
        if (count(block, "g_strfreev(environment);") != 1) fail("0111: one g_strfreev");
        less(position(block, "g_spawn_sync(", 0, "0111"),
             position(block, "g_strfreev(environment);", 0, "0111"), "0111 free after spawn");
        lacks(block, "gchar **staged = g_environ_setenv", "0111");
        free(block);
    }
    has(command, "room_history_capture_0112(doc, doc->map)", "0112");
    has(command, "if (success && mutation)", "0112");
    has(command, "room_history_push_0112(doc->undo", "0112");
    has(command, "history_clear(doc->redo", "0112");
    free(command);
    static const char *const history[] = {
        "NativeMap *map;", "GBytes *stage_json, *stage_ini", "room_history_restore_0112(",
        "room_history_equal_0112(", "room_history_mark_saved_0112(doc);",
        "history_step(doc, FALSE);", "history_step(doc, TRUE);"};
    for (size_t index = 0; index < sizeof history / sizeof *history; ++index)
        has(source, history[index], "0112");
    lacks(source, "NativeMap **undo, **redo", "0112");
    /* 0103: popovers anchored to the fixed scroller. */
    char *place = section(source, "static void room_context_popover_place_0103(",
                          "static void project_context_empty(", "0103");
    has(place, "gtk_widget_compute_point(relative, doc->scroller", "0103");
    has(place, "anchor = doc->scroller;", "0103");
    has(place, "gtk_widget_set_parent(popover, anchor);", "0103");
    has(place, "gtk_popover_set_pointing_to", "0103");
    free(place);
    if (count(source, "room_context_popover_place_0103(doc, popover,") != 2)
        fail("0103: two popover anchors");
    has(source, "room_context_popover_place_0103(doc, popover, canvas, x, y)", "0103");
    has(source, "room_context_popover_place_0103(doc, popover, relative, x, y)", "0103");
    /* 0102: the shared project door form. */
    char *form = section(source, "/* PATCH_0102_SHARED_PROJECT_DOOR_PROPERTIES:",
                         "static void annotation_window_open(", "0102");
    static const char *const actions[] = {"door-update", "door-link", "transition-update",
                                          "transition-list", "transition-delete"};
    for (size_t index = 0; index < 5; ++index) {
        char quoted[64], compared[96];
        snprintf(quoted, sizeof quoted, "\"%s\"", actions[index]);
        snprintf(compared, sizeof compared, "!strcmp(action, \"%s\")", actions[index]);
        has(form, quoted, "0102");
        has(source, compared, "0102");
    }
    has(form, "!item->project_owned || item->kind != OVERLAY_DOORS", "0102");
    has(source, "if (editor && item->project_owned && item->kind == OVERLAY_DOORS)", "0102");
    static const char *const fields[] = {"Target world", "Target area", "Target room ID",
                                         "Target project door ID", "Spawn X", "Spawn Y",
                                         "Save door properties", "Save destination", "Unlink",
                                         "project_door_fetch_link_0102(form)"};
    for (size_t index = 0; index < sizeof fields / sizeof *fields; ++index)
        has(form, fields[index], "0102");
    free(form);
    char *lower = strdup(source);
    for (char *c = lower; *c; ++c) *c = (char)tolower((unsigned char)*c);
    has(lower, "original rom", "0102");
    free(lower);
    /* 0106: the map pick callback never saves. */
    long picked = position(source, "static void project_door_map_picked_0106(", 0, "0106");
    long pick = picked >= 0 ? position(source, "static void project_door_pick_map_0106(", picked,
                                       "0106") : -1;
    if (picked >= 0 && pick > picked) {
        char *body = strndup(source + picked, (size_t)(pick - picked));
        lacks(body, "project_command(", "0106 picked");
        lacks(body, "project_reload(", "0106 picked");
        has(body, "gtk_spin_button_set_value(GTK_SPIN_BUTTON(form->target_door), 0)", "0106");
        has(body, "gtk_spin_button_set_value(GTK_SPIN_BUTTON(form->spawn_x), 0)", "0106");
        long finish = position(source, "\n}\n", pick, "0106");
        char *button = strndup(source + pick, (size_t)(finish + 3 - pick));
        has(button, "world_atlas_begin_room_pick(", "0106 button");
        lacks(button, "project_command(", "0106 button");
        lacks(button, "project_reload(", "0106 button");
        free(button);
        free(body);
    }
    has(source, "\"Pick room on global map\"", "0106");
    has(source, "world_atlas_abandon_room_pick(form->atlas_page, form);", "0106");
    has(source, "g_object_add_weak_pointer(G_OBJECT(form->atlas_page)", "0106");
    has(source, "g_object_remove_weak_pointer(G_OBJECT(form->return_page)", "0106");
    /* Notebook focus and the 0086 legend. */
    char *focus = section(source, "static void focus_page(GtkWidget *page)",
                          "static gboolean key_pressed(", "focus_page");
    has(focus, "gtk_widget_is_ancestor(page, candidate)", "focus");
    has(focus, "gtk_notebook_get_nth_page(notebook, (gint)i)", "focus");
    has(focus, "gtk_notebook_set_current_page(notebook, (gint)i);", "focus");
    free(focus);
    static const char *const legend[] = {
        "PATCH_0086_COMPACT_ROOM_LEGEND", "gtk_widget_set_halign(legend, GTK_ALIGN_START);",
        "gtk_widget_set_hexpand(legend, FALSE);",
        "gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(legend), FALSE);",
        "gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(color), 11);",
        "gtk_box_append(GTK_BOX(page), room_color_legend());", "room_legend_draw,",
        "if (item->kind >= OVERLAY_COUNT || !doc->overlays[item->kind] ||\n"
        "            item->native_overridden) continue;",
        "room_draw_sprite(doc, cr, item, x, y, width, height);"};
    for (size_t index = 0; index < sizeof legend / sizeof *legend; ++index)
        has(source, legend[index], "0086 legend");
    /* The object overlays, event regions, record menus and entity editor. */
    static const char *const records[] = {
        "\"Walls\", \"Enemies\", \"Items\", \"Objects\", \"Doors\", \"Events\", \"Triggers\", "
        "\"Other\"",
        "gtk_label_new(\"Room data\")", "load_annotations(doc, annotations_path)",
        "Show project trigger regions; native trigger decoding is not available yet",
        "\"event-list\"", "\"event-validate\"", "\"event-create\"", "\"event-update\"",
        "\"event-delete\"", "\"Create project event region here...\"",
        "\"Create project trigger region here...\"", "\"Apply event changes\"",
        "\"References: timeline:<id>, cutscene:<id>, entity:<id>, event:<id>, \"",
        "\"Add condition\"", "project_conditions_json", "project_condition_modes",
        "project_condition_types", "project_event_move",
        "const guint step = doc->project_aria ? 8u : 16u;", "annotation_canvas_context_pressed",
        "annotation_list_context_pressed", "\"Inspect full record\"", "\"Locate in Room data\"",
        "\"Open %s editor…\"", "\"Apply project override\"",
        "gtk_widget_set_sensitive(apply, FALSE);", "command = \"entity-update\"",
        "project_command(doc, \"update\", options, NULL)", "\"Apply entity changes\"",
        "\"Placement grid: 8px (Aria)\"", "\"Placement grid: 16px (Zero Mission)\"",
        "gtk_spin_button_set_snap_to_ticks", "item->index, item->native_type, item->label"};
    for (size_t index = 0; index < sizeof records / sizeof *records; ++index)
        has(source, records[index], "room records");
    lacks(source, "gtk_widget_set_sensitive(overlay_buttons[OVERLAY_TRIGGERS], FALSE);",
          "room records");
    free(source);
    char *catalog = text_of("editor/object_catalog.c");
    static const char *const objects[] = {"Zero Mission objects", "Aria of Sorrow objects",
                                          "image-missing-symbolic",
                                          "gtk_widget_set_sensitive(create, FALSE);",
                                          "gtk_widget_set_sensitive(clone, FALSE);"};
    for (size_t index = 0; index < sizeof objects / sizeof *objects; ++index)
        has(catalog, objects[index], "object catalog");
    free(catalog);
}

/* data/story/timeline.toml: schema, unique flags, satisfiable acyclic order. */
static const FusionJson *member(const FusionJson *object, const char *key) {
    const FusionJson *value = fusion_json_member(object, key);
    if (!value) fail("timeline: missing %s", key);
    return value;
}

static int producer_of(const FusionJson *events, const char *flag) {
    for (size_t index = 0; index < events->count; ++index) {
        const FusionJson *sets = fusion_json_member(&events->items[index], "sets");
        for (size_t entry = 0; sets && entry < sets->count; ++entry)
            if (!strcmp(sets->items[entry].string, flag)) return (int)index;
    }
    return -1;
}

static bool visit(const FusionJson *events, int event, unsigned char *state) {
    if (state[event] == 1) {
        fail("timeline: cycle through %s",
             fusion_json_member(&events->items[event], "id")->string);
        return false;
    }
    if (state[event] == 2) return true;
    state[event] = 1;
    const FusionJson *requires = fusion_json_member(&events->items[event], "requires");
    for (size_t index = 0; requires && index < requires->count; ++index) {
        int producer = producer_of(events, requires->items[index].string);
        if (producer >= 0 && !visit(events, producer, state)) return false;
    }
    state[event] = 2;
    return true;
}

static void test_story_timeline(void) {
    char *text = text_of("data/story/timeline.toml");
    FusionJson data;
    char error[256];
    if (!fusion_toml_parse(text, strlen(text), &data, error, sizeof error)) {
        fail("timeline: %s", error);
        free(text);
        return;
    }
    free(text);
    if (strcmp(member(&data, "schema")->string, "metroid-vania-timeline"))
        fail("timeline: schema");
    if (member(&data, "version")->integer != 1) fail("timeline: version");
    const FusionJson *events = member(&data, "events");
    static const char *const required[] = {
        "id", "title", "act", "track", "world", "source_room", "character", "event_type",
        "canonical", "original", "min_order", "max_order", "requires", "sets", "rewards",
        "cutscene", "status"};
    for (size_t index = 0; index < events->count; ++index) {
        const FusionJson *event = &events->items[index];
        for (size_t key = 0; key < sizeof required / sizeof *required; ++key) member(event, required[key]);
        const char *id = fusion_json_member(event, "id")->string;
        if (fusion_json_member(event, "min_order")->integer >
            fusion_json_member(event, "max_order")->integer)
            fail("timeline: %s order", id);
        const char *track = fusion_json_member(event, "track")->string;
        if (strcmp(track, "metroid") && strcmp(track, "castlevania") && strcmp(track, "shared"))
            fail("timeline: %s track", id);
        const char *status = fusion_json_member(event, "status")->string;
        static const char *const statuses[] = {"planned", "data", "prototype", "verified",
                                               "integrated"};
        bool known = false;
        for (size_t other = 0; other < 5; ++other) known |= !strcmp(status, statuses[other]);
        if (!known) fail("timeline: %s status", id);
        for (size_t other = index + 1; other < events->count; ++other)
            if (!strcmp(id, fusion_json_member(&events->items[other], "id")->string))
                fail("timeline: duplicate id %s", id);
        const FusionJson *sets = fusion_json_member(event, "sets");
        for (size_t flag = 0; flag < sets->count; ++flag)
            if (producer_of(events, sets->items[flag].string) != (int)index)
                fail("timeline: flag %s set twice", sets->items[flag].string);
        const FusionJson *requires = fusion_json_member(event, "requires");
        for (size_t flag = 0; flag < requires->count; ++flag) {
            int producer = producer_of(events, requires->items[flag].string);
            if (producer < 0) {
                fail("timeline: %s requires an unproduced %s", id, requires->items[flag].string);
                continue;
            }
            if (fusion_json_member(&events->items[producer], "min_order")->integer >
                fusion_json_member(event, "max_order")->integer)
                fail("timeline: %s -> %s order",
                     fusion_json_member(&events->items[producer], "id")->string, id);
        }
    }
    unsigned char *state = calloc(events->count + 1, 1);
    for (size_t index = 0; index < events->count; ++index)
        if (!visit(events, (int)index, state)) break;
    free(state);
    static const char *const finals[] = {
        "mzm.mother_brain", "mzm.chozodia_route", "mzm.mecha_ridley", "cv.graham_branch",
        "cv.julius", "cv.chaos", "mzm.samus_secret_final", "cv.soma_secret_final",
        "shared.credits"};
    for (size_t index = 0; index < sizeof finals / sizeof *finals; ++index) {
        bool found = false;
        for (size_t event = 0; event < events->count; ++event)
            found |= !strcmp(fusion_json_member(&events->items[event], "id")->string, finals[index]);
        if (!found) fail("timeline: missing %s", finals[index]);
    }
    fusion_json_free(&data);
}

int main(void) {
    test_runtime_contracts();
    test_display_modes();
    test_world_atlas_contracts();
    test_main_window_contracts();
    test_native_workspace_contracts();
    test_story_timeline();
    if (failures) {
        fprintf(stderr, "%d source contract failure(s)\n", failures);
        return 1;
    }
    puts("source contracts: ok");
    return 0;
}
