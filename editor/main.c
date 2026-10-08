/* SPDX-License-Identifier: GPL-3.0-only */
/* GTK4 project-graph editor. Original ROM maps/tilesets are NOT imported yet. */
#include "core/world_graph.h"
#include "core/tilemap.h"
#include "core/tile_paint.h"
#include "native_workspace.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    FusionWorldGraph graph;
    FusionTilemap tilemaps[2];
    GtkWidget *tile_canvas;
    GtkWidget *asset_actor;
    GtkWidget *asset_action;
    GtkWidget *asset_frame;
    GtkWidget *asset_picture;
    GtkWidget *asset_status;
    GtkWidget *tile_status;
    GtkWidget *native_details;
    GtkWidget *native_preview;
    int selected_world, selected_layer, selected_brush;
    int selected_tool, last_col, last_row;
    bool stroke_active, stroke_modified, show_grid;
    double tile_zoom;
    FusionTilemap undo[2][16], redo[2][16], stroke_before;
    unsigned undo_count[2], redo_count[2];
    GtkWidget *tile_brush_selector, *tile_tool_selector, *tile_zoom_selector;
    GtkWidget *inspector_label;
    GtkWidget *tile_page, *graph_page, *native_page, *asset_page;
    NativeWorkspace *native_workspace;
    char selected_native_area[32];
    unsigned selected_native_index;
    GtkWidget *left_dock, *center_dock, *right_dock, *subtitle;
    int responsive_mode, small_focus, seen_width;
    const char *path;
    GtkWidget *canvas;
    GtkWidget *status;
    GtkWidget *toggle;
    int dragging;
    int start_x, start_y;
} Editor;

static void apply_responsive(Editor *ed);

static void draw(GtkDrawingArea *area, cairo_t *cr, int width, int height,
                 gpointer user_data)
{
    Editor *ed = user_data;
    unsigned i;
    (void)area;
    cairo_set_source_rgb(cr, .075, .085, .125);
    cairo_paint(cr);
    cairo_set_line_width(cr, 2.5);
    for (i = 0; i < ed->graph.link_count; ++i) {
        FusionGraphLink *link = &ed->graph.links[i];
        FusionGraphRoom *from = &ed->graph.rooms[link->from];
        FusionGraphRoom *to = &ed->graph.rooms[link->to];
        if (link->enabled) cairo_set_source_rgb(cr, .45, .95, .68);
        else cairo_set_source_rgb(cr, .48, .48, .52);
        cairo_move_to(cr, from->graph_x, from->graph_y);
        cairo_line_to(cr, to->graph_x, to->graph_y);
        cairo_stroke(cr);
    }
    for (i = 0; i < ed->graph.room_count; ++i) {
        FusionGraphRoom *room = &ed->graph.rooms[i];
        double x = room->graph_x - 85.0, y = room->graph_y - 25.0;
        if (room->world == WORLD_METROID)
            cairo_set_source_rgb(cr, .14, .37, .38);
        else cairo_set_source_rgb(cr, .39, .18, .32);
        cairo_rectangle(cr, x, y, 170, 50);
        cairo_fill_preserve(cr);
        cairo_set_source_rgb(cr, .86, .88, .91);
        cairo_stroke(cr);
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_move_to(cr, x + 10, y + 20);
        cairo_show_text(cr, room->world == WORLD_METROID
                             ? "METROID / DEMO SAVE" : "ARIA / DEMO SAVE");
        cairo_move_to(cr, x + 10, y + 39);
        cairo_show_text(cr, room->save_room ? "SAVE ROOM" : "NORMAL ROOM");
    }
    (void)width;
    (void)height;
}

static void begin_drag(GtkGestureDrag *gesture, double x, double y, gpointer data)
{
    Editor *ed = data;
    unsigned i;
    (void)gesture;
    ed->dragging = -1;
    for (i = 0; i < ed->graph.room_count; ++i) {
        FusionGraphRoom *r = &ed->graph.rooms[i];
        if (x > r->graph_x - 85.0 && x < r->graph_x + 85.0 &&
            y > r->graph_y - 25.0 && y < r->graph_y + 25.0) {
            ed->dragging = (int)i;
            ed->start_x = r->graph_x;
            ed->start_y = r->graph_y;
            break;
        }
    }
}

static void drag_update(GtkGestureDrag *gesture, double dx, double dy, gpointer data)
{
    Editor *ed = data;
    FusionGraphRoom *r;
    (void)gesture;
    if (ed->dragging < 0) return;
    r = &ed->graph.rooms[ed->dragging];
    r->graph_x = ed->start_x + (int)dx;
    r->graph_y = ed->start_y + (int)dy;
    gtk_widget_queue_draw(ed->canvas);
    gtk_label_set_text(GTK_LABEL(ed->status), "Unsaved graph changes");
}

static void end_drag(GtkGestureDrag *gesture, double dx, double dy, gpointer data)
{
    Editor *ed = data;
    (void)gesture;
    (void)dx;
    (void)dy;
    ed->dragging = -1;
}

static void toggled(GtkCheckButton *button, gpointer data)
{
    Editor *ed = data;
    if (!ed->graph.link_count) return;
    ed->graph.links[0].enabled = gtk_check_button_get_active(button);
    gtk_widget_queue_draw(ed->canvas);
    gtk_label_set_text(GTK_LABEL(ed->status), "Unsaved graph changes");
}

static void save_clicked(GtkButton *button, gpointer data)
{
    Editor *ed = data;
    char error[160] = {0};
    (void)button;
    if (fusion_graph_save(&ed->graph, ed->path, error, sizeof(error)))
        gtk_label_set_text(GTK_LABEL(ed->status), "Saved project graph");
    else gtk_label_set_text(GTK_LABEL(ed->status), error);
}

/* Layer painter deliberately edits the sample PC files, not ROM graphics. */
static const char *const tile_paths[2] = {
    "data/maps/metroid_demo_save.mvroom", "data/maps/aria_demo_save.mvroom"
};
static const char *const demo_ids[2] = {
    "metroid:demo:save_01", "aria:demo:save_01"
};

/* PATCH0040_TILE_TOOLS: project tilemaps only; authentic ROM previews read-only. */
static void refresh_inspector(Editor *ed)
{
    char summary[400];
    if (!ed->inspector_label) return;
    snprintf(summary, sizeof(summary),
             "World: %s\nRoom: %s\nLayer: %s\nTool: %s\nBrush ID: %d\n"
             "Zoom: %.0f%%\n\nThese are PC sample tiles (0..4).\n"
             "Original ROM layers are view-only.\n\n"
             "Drag tabs between docks. Drag separators to resize.",
             ed->selected_world ? "Aria / Castlevania" : "Zero Mission / Metroid",
             ed->tilemaps[ed->selected_world].room_id,
             (const char *const[]){"Background", "Terrain (visual)", "Foreground"}[ed->selected_layer],
             (const char *const[]){"Pencil", "Eraser", "Fill", "Eyedropper"}[ed->selected_tool],
             ed->selected_brush, ed->tile_zoom * 100.0);
    gtk_label_set_text(GTK_LABEL(ed->inspector_label), summary);
}

static int tile_cell_pixels(const Editor *ed)
{
    return (int)(FUSION_TILE_SIZE * ed->tile_zoom);
}

static void tile_draw(GtkDrawingArea *area, cairo_t *cr, int width, int height,
                      gpointer userdata)
{
    Editor *ed = userdata;
    FusionTilemap *map = &ed->tilemaps[ed->selected_world];
    const int cell = tile_cell_pixels(ed);
    unsigned x, y, layer;
    (void)area; (void)width; (void)height;
    cairo_set_source_rgb(cr, .105, .112, .14);
    cairo_paint(cr);
    for (layer = 0; layer < FUSION_TILE_LAYERS; ++layer) {
        if ((int)layer > ed->selected_layer) break;
        for (y = 0; y < FUSION_TILE_ROWS; ++y)
            for (x = 0; x < FUSION_TILE_COLS; ++x) {
                uint8_t rgba[4];
                unsigned tile = map->tiles[layer][y][x];
                if (!tile || !fusion_tile_color(map->world, layer, tile, rgba))
                    continue;
                cairo_set_source_rgb(cr, rgba[0] / 255.0,
                                     rgba[1] / 255.0, rgba[2] / 255.0);
                cairo_rectangle(cr, x * cell, y * cell, cell, cell);
                cairo_fill(cr);
            }
    }
    if (!ed->show_grid) return;
    cairo_set_source_rgba(cr, 1, 1, 1, .19);
    cairo_set_line_width(cr, .75);
    for (x = 0; x <= FUSION_TILE_COLS; ++x) {
        cairo_move_to(cr, x * cell + .5, 0);
        cairo_line_to(cr, x * cell + .5, FUSION_TILE_ROWS * cell);
    }
    for (y = 0; y <= FUSION_TILE_ROWS; ++y) {
        cairo_move_to(cr, 0, y * cell + .5);
        cairo_line_to(cr, FUSION_TILE_COLS * cell, y * cell + .5);
    }
    cairo_stroke(cr);
}

static void history_push(Editor *ed)
{
    unsigned world = (unsigned)ed->selected_world;
    if (ed->undo_count[world] == 16) {
        memmove(&ed->undo[world][0], &ed->undo[world][1],
                15 * sizeof(FusionTilemap));
        ed->undo_count[world]--;
    }
    ed->undo[world][ed->undo_count[world]++] = ed->stroke_before;
    ed->redo_count[world] = 0;
}

static void history_restore(Editor *ed, bool is_redo)
{
    unsigned w = (unsigned)ed->selected_world;
    unsigned *count = is_redo ? &ed->redo_count[w] : &ed->undo_count[w];
    unsigned *other = is_redo ? &ed->undo_count[w] : &ed->redo_count[w];
    FusionTilemap *stack = is_redo ? ed->redo[w] : ed->undo[w];
    FusionTilemap *opposite = is_redo ? ed->undo[w] : ed->redo[w];
    if (!*count || ed->stroke_active) return;
    if (*other == 16) {
        memmove(&opposite[0], &opposite[1], 15 * sizeof(FusionTilemap));
        --*other;
    }
    opposite[(*other)++] = ed->tilemaps[w];
    ed->tilemaps[w] = stack[--*count];
    gtk_widget_queue_draw(ed->tile_canvas);
    gtk_label_set_text(GTK_LABEL(ed->tile_status), "Unsaved tilemap changes (history)");
}

static void undo_clicked(GtkButton *button, gpointer userdata)
{ (void)button; history_restore(userdata, false); }
static void redo_clicked(GtkButton *button, gpointer userdata)
{ (void)button; history_restore(userdata, true); }

static bool point_to_tile(const Editor *ed, double x, double y,
                          int *out_x, int *out_y)
{
    const int cell = tile_cell_pixels(ed);
    if (x < 0 || y < 0 || cell < 1 ||
        x >= FUSION_TILE_COLS * cell || y >= FUSION_TILE_ROWS * cell)
        return false;
    *out_x = (int)(x / cell);
    *out_y = (int)(y / cell);
    return true;
}

static void tile_apply_at(Editor *ed, int x, int y, bool continuing)
{
    unsigned picked = 0;
    FusionTilemap *map = &ed->tilemaps[ed->selected_world];
    FusionPaintTool tool = (FusionPaintTool)ed->selected_tool;
    bool changed;
    if (continuing && ed->selected_tool >= FUSION_PAINT_FILL) return;
    if (continuing && ed->last_col >= 0)
        changed = fusion_tile_stroke(map, (unsigned)ed->selected_layer,
                                     ed->last_col, ed->last_row, x, y,
                                     tool, (unsigned)ed->selected_brush);
    else changed = fusion_tile_paint(map, (unsigned)ed->selected_layer,
                                     x, y, tool, (unsigned)ed->selected_brush, &picked);
    if (tool == FUSION_PAINT_PICK) {
        ed->selected_brush = (int)picked;
        gtk_drop_down_set_selected(GTK_DROP_DOWN(ed->tile_brush_selector), picked);
    }
    if (changed) {
        ed->stroke_modified = true;
        gtk_widget_queue_draw(ed->tile_canvas);
        gtk_label_set_text(GTK_LABEL(ed->tile_status), "Unsaved tilemap changes");
    }
    ed->last_col = x;
    ed->last_row = y;
    refresh_inspector(ed);
}

static void tile_click(GtkGestureClick *gesture, int n_press,
                       double x, double y, gpointer userdata)
{
    Editor *ed = userdata;
    int col, row;
    (void)gesture; (void)n_press;
    if (!point_to_tile(ed, x, y, &col, &row)) return;
    ed->stroke_active = true;
    ed->stroke_modified = false;
    ed->last_col = -1;
    ed->last_row = -1;
    ed->stroke_before = ed->tilemaps[ed->selected_world];
    tile_apply_at(ed, col, row, false);
}

static void tile_motion(GtkEventControllerMotion *motion, double x, double y,
                        gpointer userdata)
{
    Editor *ed = userdata;
    int col, row;
    (void)motion;
    if (!ed->stroke_active || ed->selected_tool >= FUSION_PAINT_FILL ||
        !point_to_tile(ed, x, y, &col, &row)) return;
    if (col != ed->last_col || row != ed->last_row)
        tile_apply_at(ed, col, row, true);
}

static void tile_release(GtkGestureClick *gesture, int n_press,
                         double x, double y, gpointer userdata)
{
    Editor *ed = userdata;
    unsigned w = (unsigned)ed->selected_world;
    (void)gesture; (void)n_press; (void)x; (void)y;
    if (!ed->stroke_active) return;
    (void)w;
    if (ed->stroke_modified) history_push(ed);
    ed->stroke_active = false;
}

static void tile_grid_toggled(GtkCheckButton *button, gpointer userdata)
{
    Editor *ed = userdata;
    ed->show_grid = gtk_check_button_get_active(button);
    gtk_widget_queue_draw(ed->tile_canvas);
}

static void tile_zoom_changed(GtkSpinButton *spin, gpointer userdata)
{
    Editor *ed = userdata;
    int cell;
    ed->tile_zoom = gtk_spin_button_get_value(spin) / 100.0;
    cell = tile_cell_pixels(ed);
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(ed->tile_canvas),
                                       FUSION_TILE_COLS * cell);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(ed->tile_canvas),
                                        FUSION_TILE_ROWS * cell);
    gtk_widget_set_size_request(ed->tile_canvas,
                                FUSION_TILE_COLS * cell, FUSION_TILE_ROWS * cell);
    gtk_widget_queue_draw(ed->tile_canvas);
    refresh_inspector(ed);
}

static void tile_tool_changed(GObject *object, GParamSpec *pspec,
                              gpointer userdata)
{
    Editor *ed = userdata;
    guint value = gtk_drop_down_get_selected(GTK_DROP_DOWN(object));
    (void)pspec;
    if (value <= FUSION_PAINT_PICK) ed->selected_tool = (int)value;
    refresh_inspector(ed);
}

static void tile_world_changed(GObject *object, GParamSpec *pspec, gpointer userdata)
{
    Editor *ed = userdata;
    guint value = gtk_drop_down_get_selected(GTK_DROP_DOWN(object));
    (void)pspec;
    if (value > 1) return;
    ed->selected_world = value;
    gtk_widget_queue_draw(ed->tile_canvas);
    refresh_inspector(ed);
    gtk_label_set_text(GTK_LABEL(ed->tile_status), demo_ids[value]);
}

static void tile_layer_changed(GObject *object, GParamSpec *pspec, gpointer userdata)
{
    Editor *ed = userdata;
    guint value = gtk_drop_down_get_selected(GTK_DROP_DOWN(object));
    (void)pspec;
    if (value >= FUSION_TILE_LAYERS) return;
    ed->selected_layer = value;
    gtk_widget_queue_draw(ed->tile_canvas);
    refresh_inspector(ed);
}

static void tile_brush_changed(GObject *object, GParamSpec *pspec, gpointer userdata)
{
    Editor *ed = userdata;
    guint value = gtk_drop_down_get_selected(GTK_DROP_DOWN(object));
    (void)pspec;
    if (value < FUSION_TILE_TYPES)
        ed->selected_brush = value;
    refresh_inspector(ed);
}

static void tile_save_clicked(GtkButton *button, gpointer userdata)
{
    Editor *ed = userdata;
    char error[160] = {0};
    (void)button;
    if (fusion_tilemap_save(&ed->tilemaps[ed->selected_world],
                            tile_paths[ed->selected_world],
                            error, sizeof(error)))
        gtk_label_set_text(GTK_LABEL(ed->tile_status),
                           "Saved room tilemap (reload the game to see changes)");
    else
        gtk_label_set_text(GTK_LABEL(ed->tile_status), error);
}

static void toolbar_add(GtkWidget *flow, GtkWidget *child)
{ gtk_flow_box_insert(GTK_FLOW_BOX(flow), child, -1); }

static void build_tile_tab(Editor *ed, GtkWidget *tabs)
{
    static const char *const worlds[] = {"Metroid / demo save", "Aria / demo save", NULL};
    static const char *const layers[] = {"Background", "Terrain (visual)", "Foreground", NULL};
    static const char *const brushes[] = {
        "0 - Empty", "1 - Dark", "2 - Midtone", "3 - Light", "4 - Accent", NULL
    };
    static const char *const tools[] = {
        "Pencil", "Eraser", "Flood fill", "Eyedropper", NULL
    };
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *toolbar = gtk_flow_box_new();
    GtkWidget *second = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(toolbar), GTK_SELECTION_NONE);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(second), GTK_SELECTION_NONE);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(toolbar), 12);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(second), 12);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(toolbar), 6);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(second), 6);
    GtkWidget *world = gtk_drop_down_new_from_strings(worlds);
    GtkWidget *layer = gtk_drop_down_new_from_strings(layers);
    GtkWidget *brush = gtk_drop_down_new_from_strings(brushes);
    GtkWidget *tool = gtk_drop_down_new_from_strings(tools);
    GtkWidget *zoom = gtk_spin_button_new_with_range(50, 200, 25);
    GtkWidget *grid = gtk_check_button_new_with_label("Grid");
    GtkWidget *undo = gtk_button_new_with_label("Undo");
    GtkWidget *redo = gtk_button_new_with_label("Redo");
    GtkWidget *save = gtk_button_new_with_label("Save tilemap");
    GtkWidget *scroll = gtk_scrolled_window_new();
    GtkGesture *click = gtk_gesture_click_new();
    GtkEventController *motion = gtk_event_controller_motion_new();
    ed->tile_brush_selector = brush;
    ed->tile_tool_selector = tool;
    ed->tile_zoom_selector = zoom;
    ed->tile_zoom = 1.0;
    ed->show_grid = true;
    gtk_check_button_set_active(GTK_CHECK_BUTTON(grid), TRUE);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(zoom), 100);
    gtk_box_append(GTK_BOX(root), toolbar);
    gtk_box_append(GTK_BOX(root), second);
    toolbar_add(toolbar, gtk_label_new("WORLD"));
    toolbar_add(toolbar, world);
    toolbar_add(toolbar, gtk_label_new("LAYER"));
    toolbar_add(toolbar, layer);
    toolbar_add(toolbar, gtk_label_new("TOOL"));
    toolbar_add(toolbar, tool);
    toolbar_add(toolbar, gtk_label_new("BRUSH"));
    toolbar_add(toolbar, brush);
    toolbar_add(second, gtk_label_new("ZOOM %"));
    toolbar_add(second, zoom);
    toolbar_add(second, grid);
    toolbar_add(second, undo);
    toolbar_add(second, redo);
    toolbar_add(second, save);
    ed->tile_canvas = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(ed->tile_canvas),
                                       FUSION_TILE_COLS * FUSION_TILE_SIZE);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(ed->tile_canvas),
                                        FUSION_TILE_ROWS * FUSION_TILE_SIZE);
    gtk_widget_set_size_request(ed->tile_canvas,
                                FUSION_TILE_COLS * FUSION_TILE_SIZE,
                                FUSION_TILE_ROWS * FUSION_TILE_SIZE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(ed->tile_canvas),
                                   tile_draw, ed, NULL);
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), GDK_BUTTON_PRIMARY);
    gtk_widget_add_controller(ed->tile_canvas, GTK_EVENT_CONTROLLER(click));
    gtk_widget_add_controller(ed->tile_canvas, motion);
    g_signal_connect(click, "pressed", G_CALLBACK(tile_click), ed);
    g_signal_connect(click, "released", G_CALLBACK(tile_release), ed);
    g_signal_connect(motion, "motion", G_CALLBACK(tile_motion), ed);
    g_signal_connect(world, "notify::selected", G_CALLBACK(tile_world_changed), ed);
    g_signal_connect(layer, "notify::selected", G_CALLBACK(tile_layer_changed), ed);
    g_signal_connect(brush, "notify::selected", G_CALLBACK(tile_brush_changed), ed);
    g_signal_connect(tool, "notify::selected", G_CALLBACK(tile_tool_changed), ed);
    g_signal_connect(zoom, "value-changed", G_CALLBACK(tile_zoom_changed), ed);
    g_signal_connect(grid, "toggled", G_CALLBACK(tile_grid_toggled), ed);
    g_signal_connect(save, "clicked", G_CALLBACK(tile_save_clicked), ed);
    g_signal_connect(undo, "clicked", G_CALLBACK(undo_clicked), ed);
    g_signal_connect(redo, "clicked", G_CALLBACK(redo_clicked), ed);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), ed->tile_canvas);
    gtk_widget_set_hexpand(scroll, TRUE);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_box_append(GTK_BOX(root), scroll);
    ed->tile_status = gtk_label_new(
        "PC demo tiles (0..4) — authentic ROM tilesets are read-only until imported for editing");
    gtk_label_set_xalign(GTK_LABEL(ed->tile_status), 0.0f);
    gtk_box_append(GTK_BOX(root), ed->tile_status);
    gtk_notebook_append_page(GTK_NOTEBOOK(tabs), root, gtk_label_new("Tile painter"));
    ed->tile_page = root;
    g_object_set_data(G_OBJECT(root), "world-selector", world);
}

/* Preview of local, ROM-derived animation BMPs; never writes proprietary data. */
static const char *const asset_actor_names[] = {
    "Samus", "Soma", "Metroid native reference", "Aria native reference", NULL
};
static const char *const asset_animation_names[] = {
    "idle", "run", "jump", "attack", "run_start", "run_stop", NULL
};
static const unsigned asset_frame_counts[2][6] = {
    {4, 10, 8, 3, 0, 0}, {4, 17, 12, 11, 3, 9}
};

static void asset_update_preview(Editor *ed)
{
    guint actor = gtk_drop_down_get_selected(GTK_DROP_DOWN(ed->asset_actor));
    guint action = gtk_drop_down_get_selected(GTK_DROP_DOWN(ed->asset_action));
    unsigned count, frame;
    char path[256];
    char message[360];
    if (actor >= 4) return;
    if (actor >= 2) {
        const char *reference = actor == 2
            ? "captures/arrival-preview/mzm-after.bmp"
            : "captures/arrival-preview/aria-after.bmp";
        if (g_file_test(reference, G_FILE_TEST_IS_REGULAR)) {
            gtk_picture_set_filename(GTK_PICTURE(ed->asset_picture), reference);
            snprintf(message, sizeof(message),
                     "REAL engine framebuffer: %s | reference frame, NOT editable map data",
                     reference);
        } else {
            gtk_picture_set_paintable(GTK_PICTURE(ed->asset_picture), NULL);
            snprintf(message, sizeof(message),
                     "No native room capture: run ./build/fusion_dev --authentic-arrival-preview");
        }
        gtk_label_set_text(GTK_LABEL(ed->asset_status), message);
        return;
    }
    if (action >= 6) return;
    count = asset_frame_counts[actor][action];
    if (!count) {
        gtk_picture_set_paintable(GTK_PICTURE(ed->asset_picture), NULL);
        gtk_label_set_text(GTK_LABEL(ed->asset_status),
                           "Animation not available for this character");
        return;
    }
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(ed->asset_frame), 0, count - 1);
    frame = (unsigned)gtk_spin_button_get_value_as_int(
        GTK_SPIN_BUTTON(ed->asset_frame));
    snprintf(path, sizeof(path), "assets/extracted/sprites/%s/%s_%u.bmp",
             actor == 0 ? "samus" : "soma", asset_animation_names[action], frame);
    if (g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
        gtk_picture_set_filename(GTK_PICTURE(ed->asset_picture), path);
        snprintf(message, sizeof(message),
                 "REAL ROM sprite: %s | frame %u/%u | source asset, not a dummy rectangle",
                 path, frame + 1, count);
    } else {
        gtk_picture_set_paintable(GTK_PICTURE(ed->asset_picture), NULL);
        snprintf(message, sizeof(message),
                 "Sprite not extracted: %s | run python3 scripts/import_game_assets.py --scope sprites",
                 path);
    }
    gtk_label_set_text(GTK_LABEL(ed->asset_status), message);
}

static void asset_dropdown_changed(GObject *object, GParamSpec *pspec,
                                   gpointer userdata)
{
    (void)object;
    (void)pspec;
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

static void build_assets_tab(Editor *ed, GtkWidget *tabs)
{
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *refresh = gtk_button_new_with_label("Refresh local sprites");
    GtkWidget *scroll = gtk_scrolled_window_new();
    ed->asset_actor = gtk_drop_down_new_from_strings(asset_actor_names);
    ed->asset_action = gtk_drop_down_new_from_strings(asset_animation_names);
    ed->asset_frame = gtk_spin_button_new_with_range(0, 16, 1);
    ed->asset_picture = gtk_picture_new();
    ed->asset_status = gtk_label_new("Load ROM-extracted BMPs using the local importer");
    gtk_picture_set_can_shrink(GTK_PICTURE(ed->asset_picture), TRUE);
    gtk_widget_set_size_request(ed->asset_picture, 180, 230);
    gtk_widget_set_hexpand(ed->asset_picture, TRUE);
    gtk_widget_set_vexpand(ed->asset_picture, TRUE);
    gtk_box_append(GTK_BOX(bar), gtk_label_new("Character:"));
    gtk_box_append(GTK_BOX(bar), ed->asset_actor);
    gtk_box_append(GTK_BOX(bar), gtk_label_new("Animation:"));
    gtk_box_append(GTK_BOX(bar), ed->asset_action);
    gtk_box_append(GTK_BOX(bar), gtk_label_new("Frame:"));
    gtk_box_append(GTK_BOX(bar), ed->asset_frame);
    gtk_box_append(GTK_BOX(bar), refresh);
    gtk_box_append(GTK_BOX(root), bar);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), ed->asset_picture);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_box_append(GTK_BOX(root), scroll);
    gtk_box_append(GTK_BOX(root), ed->asset_status);
    gtk_box_append(GTK_BOX(root), gtk_label_new(
        "Extracted animation viewer: room, enemy, item, music and cutscene decoding still pending"));
    g_signal_connect(ed->asset_actor, "notify::selected",
                     G_CALLBACK(asset_dropdown_changed), ed);
    g_signal_connect(ed->asset_action, "notify::selected",
                     G_CALLBACK(asset_dropdown_changed), ed);
    g_signal_connect(ed->asset_frame, "value-changed",
                     G_CALLBACK(asset_frame_changed), ed);
    g_signal_connect(refresh, "clicked", G_CALLBACK(asset_refresh_clicked), ed);
    gtk_notebook_append_page(GTK_NOTEBOOK(tabs), root, gtk_label_new("ROM visuals"));
    asset_update_preview(ed);
}

/* Read-only browser of original Zero Mission RoomEntryRom descriptors.
 * Graphics have NOT been decoded from the ROM; do not fake a rendered room. */
static void native_row_selected(GtkListBox *list, GtkListBoxRow *row, gpointer userdata)
{
    Editor *ed = userdata;
    const char *details;
    (void)list;
    if (!row || !ed->native_details) return;
    details = g_object_get_data(G_OBJECT(row), "native-room-details");
    if (details) gtk_label_set_text(GTK_LABEL(ed->native_details), details);
    const char *area = g_object_get_data(G_OBJECT(row), "native-area");
    if (area) {
        snprintf(ed->selected_native_area, sizeof(ed->selected_native_area), "%s", area);
        ed->selected_native_index = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(row), "native-index"));
    }
    if (ed->native_preview) {
        const char *preview_path = g_object_get_data(G_OBJECT(row), "native-room-preview");
        if (preview_path && g_file_test(preview_path, G_FILE_TEST_IS_REGULAR))
            gtk_picture_set_filename(GTK_PICTURE(ed->native_preview), preview_path);
        else gtk_picture_set_paintable(GTK_PICTURE(ed->native_preview), NULL);
    }
}

static void open_selected_native_room(Editor *ed)
{
    if (ed->native_workspace && ed->selected_native_area[0])
        native_workspace_import_async(ed->native_workspace,
                                      ed->selected_native_area, ed->selected_native_index);
}
static void native_open_clicked(GtkButton *button, gpointer userdata)
{ (void)button; open_selected_native_room(userdata); }
static void native_row_activated(GtkListBox *list, GtkListBoxRow *row, gpointer userdata)
{
    Editor *ed = userdata;
    native_row_selected(list, row, ed);
    open_selected_native_room(ed);
}
static void build_native_rooms_tab(Editor *ed, GtkWidget *tabs)
{
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *scroll = gtk_scrolled_window_new();
    GtkWidget *list = gtk_list_box_new();
    GtkWidget *open = gtk_button_new_with_label("Open selected original room in Native tile painter");
    GtkWidget *preview = gtk_picture_new();
    gchar *contents = NULL;
    gchar **lines;
    gsize length = 0;
    size_t i;
    gtk_box_append(GTK_BOX(page), gtk_label_new(
        "Zero Mission source room browser | double-click to edit BG1/BG2 privately"));
    gtk_box_append(GTK_BOX(page), open);
    g_signal_connect(open, "clicked", G_CALLBACK(native_open_clicked), ed);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), list);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_box_append(GTK_BOX(page), scroll);
    gtk_widget_set_size_request(preview, 200, 160);
    gtk_picture_set_can_shrink(GTK_PICTURE(preview), TRUE);
    gtk_box_append(GTK_BOX(page), preview);
    ed->native_preview = preview;
    ed->native_page = page;
    ed->native_details = gtk_label_new("Choose a room to inspect its source pointers and music ID.");
    gtk_label_set_wrap(GTK_LABEL(ed->native_details), TRUE);
    gtk_label_set_xalign(GTK_LABEL(ed->native_details), 0.0f);
    gtk_box_append(GTK_BOX(page), ed->native_details);
    if (!g_file_get_contents("assets/extracted/rooms/metroid/rooms.tsv",
                             &contents, &length, NULL)) {
        gtk_label_set_text(GTK_LABEL(ed->native_details),
            "Room catalog unavailable. Run: python3 scripts/import_game_assets.py --scope all");
        gtk_notebook_append_page(GTK_NOTEBOOK(tabs), page,
                                 gtk_label_new("Native rooms"));
        return;
    }
    lines = g_strsplit(contents, "\n", -1);
    for (i = 0; lines[i]; ++i) {
        gchar **parts;
        GtkWidget *label;
        GtkWidget *row;
        gchar *name;
        gchar *details;
        if (!lines[i][0] || lines[i][0] == '#') continue;
        parts = g_strsplit(lines[i], "|", -1);
        if (g_strv_length(parts) != 10) {
            g_strfreev(parts);
            continue;
        }
        name = g_strdup_printf("%s / room %s    tileset %s    %s",
                               parts[0], parts[1], parts[2], parts[3]);
        details = g_strdup_printf(
            "Area: %s  room: %s    source tile set: %s\n"
            "Native music symbol: %s\nBG1: %s\nBG2: %s\n"
            "Clipdata: %s\nDefault spriteset: %s\n"
            "World map position: %s,%s\n"
            "These are original source references, not editable native map pixels.",
            parts[0], parts[1], parts[2], parts[3], parts[4], parts[5],
            parts[6], parts[7], parts[8], parts[9]);
        label = gtk_label_new(name);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        row = gtk_list_box_row_new();
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        g_object_set_data_full(G_OBJECT(row), "native-room-details",
                               details, g_free);
        g_object_set_data_full(G_OBJECT(row), "native-area", g_strdup(parts[0]), g_free);
        g_object_set_data(G_OBJECT(row), "native-index",
                          GUINT_TO_POINTER((guint)g_ascii_strtoull(parts[1], NULL, 10)));
        {
            gchar *filename = g_strdup_printf(
                "assets/extracted/rooms/metroid/previews/%s_%03u_bg1.bmp",
                g_ascii_strdown(parts[0], -1), (unsigned)g_ascii_strtoull(parts[1], NULL, 10));
            g_object_set_data_full(G_OBJECT(row), "native-room-preview", filename, g_free);
        }
        gtk_list_box_append(GTK_LIST_BOX(list), row);
        g_free(name);
        g_strfreev(parts);
    }
    g_strfreev(lines);
    g_free(contents);
    g_signal_connect(list, "row-selected", G_CALLBACK(native_row_selected), ed);
    g_signal_connect(list, "row-activated", G_CALLBACK(native_row_activated), ed);
    gtk_notebook_append_page(GTK_NOTEBOOK(tabs), page,
                             gtk_label_new("Native rooms"));
}

/* PATCH0040_DOCKS: notebook groups allow drag/drop across split regions. */
static GtkWidget *new_dock(GtkApplication *app);

static GtkNotebook *create_floating_dock(GtkNotebook *notebook, GtkWidget *page,
                                         gpointer userdata)
{
    GtkApplication *app = GTK_APPLICATION(userdata);
    GtkWidget *window = gtk_application_window_new(app);
    GtkWidget *floating = new_dock(app);
    (void)notebook; (void)page;
    gtk_window_set_title(GTK_WINDOW(window), "Metroid Vania - Detached tools");
    gtk_window_set_default_size(GTK_WINDOW(window), 780, 520);
    gtk_window_set_child(GTK_WINDOW(window), floating);
    gtk_window_present(GTK_WINDOW(window));
    return GTK_NOTEBOOK(floating);
}

static void dock_page_added(GtkNotebook *notebook, GtkWidget *child,
                            guint index, gpointer userdata)
{
    (void)index; (void)userdata;
    gtk_notebook_set_tab_reorderable(notebook, child, TRUE);
    gtk_notebook_set_tab_detachable(notebook, child, TRUE);
}

static GtkWidget *new_dock(GtkApplication *app)
{
    GtkWidget *dock = gtk_notebook_new();
    gtk_notebook_set_group_name(GTK_NOTEBOOK(dock), "metroidvania-editor-docks");
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(dock), TRUE);
    gtk_widget_set_hexpand(dock, TRUE);
    gtk_widget_set_vexpand(dock, TRUE);
    g_signal_connect(dock, "page-added", G_CALLBACK(dock_page_added), NULL);
    g_signal_connect(dock, "create-window", G_CALLBACK(create_floating_dock), app);
    return dock;
}

static void focus_dock_page(GtkWidget *page)
{
    GtkWidget *parent = page ? gtk_widget_get_parent(page) : NULL;
    if (parent && GTK_IS_NOTEBOOK(parent)) {
        gtk_notebook_set_current_page(GTK_NOTEBOOK(parent),
                                      gtk_notebook_page_num(GTK_NOTEBOOK(parent), page));
        GtkRoot *root = gtk_widget_get_root(parent);
        if (GTK_IS_WINDOW(root)) gtk_window_present(GTK_WINDOW(root));
    }
}

static void explorer_action(GtkButton *button, gpointer userdata)
{
    Editor *ed = userdata;
    const char *action = g_object_get_data(G_OBJECT(button), "editor-action");
    if (!action) return;
    if (strcmp(action, "graph") == 0) focus_dock_page(ed->graph_page);
    else if (strcmp(action, "native") == 0) focus_dock_page(ed->native_page);
    else if (strcmp(action, "assets") == 0) focus_dock_page(ed->asset_page);
    else if (strcmp(action, "metroid") == 0 || strcmp(action, "aria") == 0) {
        ed->selected_world = strcmp(action, "aria") == 0;
        /* Keep the dropdown and data model in sync. */
        gtk_drop_down_set_selected(GTK_DROP_DOWN(
            g_object_get_data(G_OBJECT(ed->tile_page), "world-selector")),
            (guint)ed->selected_world);
        focus_dock_page(ed->tile_page);
    }
    refresh_inspector(ed);
    /* A narrow explorer navigation must reveal the center dock. */
    if (ed->responsive_mode == 0) {
        ed->small_focus = 0;
        apply_responsive(ed);
    }
}

static void explorer_button(GtkWidget *parent, Editor *ed,
                            const char *label, const char *action)
{
    GtkWidget *button = gtk_button_new_with_label(label);
    gtk_widget_set_halign(button, GTK_ALIGN_FILL);
    g_object_set_data(G_OBJECT(button), "editor-action", (gpointer)action);
    g_signal_connect(button, "clicked", G_CALLBACK(explorer_action), ed);
    gtk_box_append(GTK_BOX(parent), button);
}

static void build_explorer(Editor *ed, GtkWidget *dock)
{
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 9);
    GtkWidget *header = gtk_label_new("PROJECT  /  METROID VANIA");
    GtkWidget *hint = gtk_label_new(
        "Workspace navigation\n\n"
        "Drag tabs between columns.\n"
        "Pull a tab outside to float.\n"
        "Move dividers to resize.\n\n"
        "ROM data stays local.\n"
        "Real MZM room previews are read-only.");
    gtk_widget_set_margin_start(root, 12);
    gtk_widget_set_margin_end(root, 12);
    gtk_widget_set_margin_top(root, 12);
    gtk_label_set_xalign(GTK_LABEL(header), 0);
    gtk_label_set_xalign(GTK_LABEL(hint), 0);
    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_box_append(GTK_BOX(root), header);
    gtk_box_append(GTK_BOX(root), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    explorer_button(root, ed, "World graph", "graph");
    explorer_button(root, ed, "Metroid / editable demo room", "metroid");
    explorer_button(root, ed, "Aria / editable demo room", "aria");
    explorer_button(root, ed, "Zero Mission / native rooms", "native");
    explorer_button(root, ed, "Authentic ROM visuals", "assets");
    gtk_box_append(GTK_BOX(root), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(root), hint);
    gtk_notebook_append_page(GTK_NOTEBOOK(dock), root, gtk_label_new("Explorer"));
}

static void build_inspector(Editor *ed, GtkWidget *dock)
{
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    GtkWidget *title = gtk_label_new("TILE PROPERTIES");
    ed->inspector_label = gtk_label_new("");
    gtk_widget_set_margin_start(root, 16);
    gtk_widget_set_margin_end(root, 16);
    gtk_widget_set_margin_top(root, 16);
    gtk_label_set_xalign(GTK_LABEL(title), 0);
    gtk_label_set_xalign(GTK_LABEL(ed->inspector_label), 0);
    gtk_label_set_wrap(GTK_LABEL(ed->inspector_label), TRUE);
    gtk_box_append(GTK_BOX(root), title);
    gtk_box_append(GTK_BOX(root), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(root), ed->inspector_label);
    gtk_notebook_append_page(GTK_NOTEBOOK(dock), root, gtk_label_new("Inspector"));
    refresh_inspector(ed);
}


/* PATCH0041_RESPONSIVE: width-aware docks + explicit sidebar focus controls.
 * A tiny drawing area receives an allocation on each window resize. UI mutations
 * are scheduled via an idle callback, outside the drawing callback. */
static void apply_responsive(Editor *ed)
{
    gboolean left=TRUE,center=TRUE,right=TRUE;
    if(ed->responsive_mode==0){
        left=ed->small_focus==1;
        center=ed->small_focus==0;
        right=ed->small_focus==2;
    }else if(ed->responsive_mode==1){
        left=ed->small_focus!=2;
        right=ed->small_focus==2;
    }else{
        left=ed->small_focus!=1;
        right=ed->small_focus!=2;
    }
    gtk_widget_set_visible(ed->left_dock,left);
    gtk_widget_set_visible(ed->center_dock,center);
    gtk_widget_set_visible(ed->right_dock,right);
    gtk_widget_set_visible(ed->subtitle,ed->responsive_mode==2);
}
static gboolean responsive_idle(gpointer userdata)
{
    Editor *ed=userdata;
    int new_mode=ed->seen_width<920?0:ed->seen_width<1450?1:2;
    if(ed->responsive_mode!=new_mode){
        ed->responsive_mode=new_mode;
        ed->small_focus=0;
    }
    apply_responsive(ed);
    return G_SOURCE_REMOVE;
}
static void responsive_sensor(GtkDrawingArea *area,cairo_t *cr,int width,int height,gpointer userdata)
{
    Editor *ed=userdata;
    (void)area;(void)cr;(void)width;(void)height;
    GtkRoot *root=gtk_widget_get_root(GTK_WIDGET(area));
    if(!GTK_IS_WINDOW(root))return;
    int w=gtk_widget_get_width(GTK_WIDGET(root));
    if(w>0&&w!=ed->seen_width){ed->seen_width=w;g_idle_add(responsive_idle,ed);}
}
static void responsive_button(GtkButton *button,gpointer userdata)
{
    Editor *ed=userdata;
    int requested=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button),"focus"));
    if(ed->responsive_mode==0){
        ed->small_focus=(ed->small_focus==requested)?0:requested;
    }else if(ed->responsive_mode==1){
        ed->small_focus=(ed->small_focus==requested)?0:requested;
    }else if(requested==0){
        ed->small_focus=0;
    }else{
        ed->small_focus=(ed->small_focus==requested)?0:requested;
    }
    apply_responsive(ed);
}
static GtkWidget *make_responsive_button(Editor *ed,const char *text,int focus)
{
    GtkWidget *b=gtk_button_new_with_label(text);
    g_object_set_data(G_OBJECT(b),"focus",GINT_TO_POINTER(focus));
    g_signal_connect(b,"clicked",G_CALLBACK(responsive_button),ed);
    return b;
}

static void activate(GtkApplication *app, gpointer user_data)
{
    Editor *ed = user_data;
    GtkWidget *window = gtk_application_window_new(app);
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    GtkWidget *title = gtk_label_new("METROID VANIA  /  PC WORLD EDITOR");
    GtkWidget *subtitle = gtk_label_new("Project graph  •  tile painter  •  native ROM browser");
    GtkWidget *left = new_dock(app);
    GtkWidget *center = new_dock(app);
    GtkWidget *right = new_dock(app);
    GtkWidget *split_left = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *split_right = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *graph_tab = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *save = gtk_button_new_with_label("Save graph");
    GtkGesture *drag;
    gtk_window_set_title(GTK_WINDOW(window), "Metroid Vania - PC World Editor");
    gtk_window_set_default_size(GTK_WINDOW(window), 1680, 980);
    ed->left_dock=left;ed->center_dock=center;ed->right_dock=right;
    ed->subtitle=subtitle;ed->responsive_mode=2;ed->small_focus=0;

    gtk_window_set_child(GTK_WINDOW(window), root);
    gtk_widget_set_margin_start(header, 14);
    gtk_widget_set_margin_end(header, 14);
    gtk_widget_set_margin_top(header, 8);
    gtk_widget_set_margin_bottom(header, 8);
    gtk_widget_set_hexpand(subtitle, TRUE);
    gtk_label_set_xalign(GTK_LABEL(subtitle), 1);
    gtk_box_append(GTK_BOX(header), title);
    gtk_box_append(GTK_BOX(header), make_responsive_button(ed,"Explorer",1));
    gtk_box_append(GTK_BOX(header), make_responsive_button(ed,"Canvas",0));
    gtk_box_append(GTK_BOX(header), make_responsive_button(ed,"Inspector",2));
    GtkWidget *sensor=gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(sensor),1);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(sensor),1);
    gtk_widget_set_hexpand(sensor,TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(sensor),responsive_sensor,ed,NULL);
    gtk_box_append(GTK_BOX(header),sensor);
    gtk_box_append(GTK_BOX(header), subtitle);
    gtk_box_append(GTK_BOX(root), header);
    gtk_box_append(GTK_BOX(root), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_widget_set_vexpand(split_left, TRUE);
    gtk_box_append(GTK_BOX(root), split_left);
    gtk_paned_set_start_child(GTK_PANED(split_left), left);
    gtk_paned_set_end_child(GTK_PANED(split_left), split_right);
    gtk_paned_set_start_child(GTK_PANED(split_right), center);
    gtk_paned_set_end_child(GTK_PANED(split_right), right);
    gtk_paned_set_shrink_start_child(GTK_PANED(split_left), TRUE);
    gtk_paned_set_shrink_end_child(GTK_PANED(split_left), TRUE);
    gtk_paned_set_shrink_start_child(GTK_PANED(split_right), TRUE);
    gtk_paned_set_shrink_end_child(GTK_PANED(split_right), TRUE);
    gtk_paned_set_position(GTK_PANED(split_left), 275);
    gtk_paned_set_position(GTK_PANED(split_right), 980);
    gtk_paned_set_resize_start_child(GTK_PANED(split_left), FALSE);
    gtk_paned_set_resize_end_child(GTK_PANED(split_left), TRUE);
    gtk_paned_set_resize_start_child(GTK_PANED(split_right), TRUE);
    gtk_paned_set_resize_end_child(GTK_PANED(split_right), FALSE);

    ed->canvas = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(ed->canvas), 950);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(ed->canvas), 570);
    gtk_widget_set_hexpand(ed->canvas, TRUE);
    gtk_widget_set_vexpand(ed->canvas, TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(ed->canvas), draw, ed, NULL);
    GtkWidget *graph_scroll=gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(graph_scroll),ed->canvas);
    gtk_widget_set_hexpand(graph_scroll,TRUE);
    gtk_widget_set_vexpand(graph_scroll,TRUE);
    gtk_box_append(GTK_BOX(graph_tab), graph_scroll);
    drag = gtk_gesture_drag_new();
    gtk_widget_add_controller(ed->canvas, GTK_EVENT_CONTROLLER(drag));
    g_signal_connect(drag, "drag-begin", G_CALLBACK(begin_drag), ed);
    g_signal_connect(drag, "drag-update", G_CALLBACK(drag_update), ed);
    g_signal_connect(drag, "drag-end", G_CALLBACK(end_drag), ed);
    ed->toggle = gtk_check_button_new_with_label(
        "Enable bidirectional demo save-room link");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(ed->toggle),
                                ed->graph.link_count && ed->graph.links[0].enabled);
    g_signal_connect(ed->toggle, "toggled", G_CALLBACK(toggled), ed);
    gtk_box_append(GTK_BOX(graph_tab), ed->toggle);
    gtk_box_append(GTK_BOX(graph_tab), save);
    g_signal_connect(save, "clicked", G_CALLBACK(save_clicked), ed);
    ed->status = gtk_label_new("Drag native graph nodes; edits affect only the PC project graph");
    gtk_box_append(GTK_BOX(graph_tab), ed->status);
    gtk_notebook_append_page(GTK_NOTEBOOK(center), graph_tab,
                             gtk_label_new("World graph"));
    ed->graph_page = graph_tab;
    build_tile_tab(ed, center);
    /* Explorer buttons use the tile page world selector. */
    build_native_rooms_tab(ed, center);
    build_inspector(ed, right);
    build_assets_tab(ed, right);
    ed->asset_page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(right), 1);
    native_workspace_build(ed->native_workspace, center, right);
    build_explorer(ed, left);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(center), 1);
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv)
{
    Editor editor = {0};
    GtkApplication *app;
    char error[160] = {0};
    int code;
    editor.path = argc > 1 ? argv[1] : "data/world_graph.mvg";
    editor.dragging = -1;
    editor.native_workspace=native_workspace_new();
    if(!editor.native_workspace){fputs("Native workspace allocation failed\n",stderr);return 1;}
    if (!fusion_graph_load(&editor.graph, editor.path, error, sizeof(error))) {
        fprintf(stderr, "Map editor: %s (%s)\n", error, editor.path);
        return 1;
    }
    if (!fusion_tilemap_load(&editor.tilemaps[0], tile_paths[0],
                             error, sizeof(error)) ||
        !fusion_tilemap_load(&editor.tilemaps[1], tile_paths[1],
                             error, sizeof(error)) ||
        strcmp(editor.tilemaps[0].room_id, demo_ids[0]) ||
        strcmp(editor.tilemaps[1].room_id, demo_ids[1])) {
        fprintf(stderr, "Map editor: tilemap load/identity failure: %s\n", error);
        return 1;
    }
    app = gtk_application_new("org.metroidvania.grapheditor",
                              G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), &editor);
    code = g_application_run(G_APPLICATION(app), 1, argv);
    g_object_unref(app);
    native_workspace_free(editor.native_workspace);
    return code;
}
