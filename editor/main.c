/* SPDX-License-Identifier: GPL-3.0-only */
/* GTK4 project-graph editor. Original ROM maps/tilesets are NOT imported yet. */
#include "core/world_graph.h"
#include "core/tilemap.h"
#include <gtk/gtk.h>
#include <stdio.h>

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
    int selected_world, selected_layer, selected_brush;
    double tile_drag_x, tile_drag_y;
    const char *path;
    GtkWidget *canvas;
    GtkWidget *status;
    GtkWidget *toggle;
    int dragging;
    int start_x, start_y;
} Editor;

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

static void tile_draw(GtkDrawingArea *area, cairo_t *cr, int width, int height,
                      gpointer userdata)
{
    Editor *ed = userdata;
    FusionTilemap *map = &ed->tilemaps[ed->selected_world];
    unsigned x, y, layer;
    (void)area; (void)width; (void)height;
    cairo_set_source_rgb(cr, .11, .12, .15);
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
                cairo_rectangle(cr, x * FUSION_TILE_SIZE, y * FUSION_TILE_SIZE,
                                FUSION_TILE_SIZE, FUSION_TILE_SIZE);
                cairo_fill(cr);
            }
    }
    cairo_set_source_rgba(cr, 1, 1, 1, .16);
    cairo_set_line_width(cr, .65);
    for (x = 0; x <= FUSION_TILE_COLS; ++x) {
        cairo_move_to(cr, x * FUSION_TILE_SIZE + .5, 0);
        cairo_line_to(cr, x * FUSION_TILE_SIZE + .5,
                      FUSION_TILE_ROWS * FUSION_TILE_SIZE);
    }
    for (y = 0; y <= FUSION_TILE_ROWS; ++y) {
        cairo_move_to(cr, 0, y * FUSION_TILE_SIZE + .5);
        cairo_line_to(cr, FUSION_TILE_COLS * FUSION_TILE_SIZE,
                      y * FUSION_TILE_SIZE + .5);
    }
    cairo_stroke(cr);
}

static void tile_paint(Editor *ed, double x, double y)
{
    int col = (int)(x / FUSION_TILE_SIZE);
    int row = (int)(y / FUSION_TILE_SIZE);
    uint8_t *cell;
    if (x < 0 || y < 0 || col < 0 || row < 0 ||
        col >= (int)FUSION_TILE_COLS || row >= (int)FUSION_TILE_ROWS)
        return;
    cell = &ed->tilemaps[ed->selected_world]
                .tiles[ed->selected_layer][row][col];
    if (*cell == (uint8_t)ed->selected_brush) return;
    *cell = (uint8_t)ed->selected_brush;
    gtk_widget_queue_draw(ed->tile_canvas);
    gtk_label_set_text(GTK_LABEL(ed->tile_status), "Unsaved tilemap changes");
}

static void tile_click(GtkGestureClick *gesture, int n_press,
                       double x, double y, gpointer userdata)
{
    (void)gesture; (void)n_press;
    tile_paint(userdata, x, y);
}

static void tile_drag_begin(GtkGestureDrag *gesture, double x, double y,
                            gpointer userdata)
{
    Editor *ed = userdata;
    (void)gesture;
    ed->tile_drag_x = x;
    ed->tile_drag_y = y;
    tile_paint(ed, x, y);
}

static void tile_drag_update(GtkGestureDrag *gesture, double dx, double dy,
                             gpointer userdata)
{
    Editor *ed = userdata;
    (void)gesture;
    tile_paint(ed, ed->tile_drag_x + dx, ed->tile_drag_y + dy);
}

static void tile_world_changed(GObject *object, GParamSpec *pspec, gpointer userdata)
{
    Editor *ed = userdata;
    guint value = gtk_drop_down_get_selected(GTK_DROP_DOWN(object));
    (void)pspec;
    if (value > 1) return;
    ed->selected_world = value;
    gtk_widget_queue_draw(ed->tile_canvas);
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
}

static void tile_brush_changed(GObject *object, GParamSpec *pspec, gpointer userdata)
{
    Editor *ed = userdata;
    guint value = gtk_drop_down_get_selected(GTK_DROP_DOWN(object));
    (void)pspec;
    if (value < FUSION_TILE_TYPES)
        ed->selected_brush = value;
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

static GtkWidget *tile_combo(const char *const labels[], size_t count)
{
    (void)count;
    return gtk_drop_down_new_from_strings(labels);
}

static void build_tile_tab(Editor *ed, GtkWidget *tabs)
{
    static const char *const worlds[] = {"Metroid / demo save", "Aria / demo save", NULL};
    static const char *const layers[] = {"Background", "Terrain (visual)", "Foreground", NULL};
    static const char *const brushes[] = {
        "0 - Erase", "1 - Dark", "2 - Midtone", "3 - Light", "4 - Accent", NULL
    };
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *world = tile_combo(worlds, 2);
    GtkWidget *layer = tile_combo(layers, 3);
    GtkWidget *brush = tile_combo(brushes, 5);
    GtkWidget *save = gtk_button_new_with_label("Save current tilemap");
    GtkWidget *scroll = gtk_scrolled_window_new();
    GtkGesture *drag = gtk_gesture_drag_new();
    GtkGesture *click = gtk_gesture_click_new();
    gtk_box_append(GTK_BOX(root), toolbar);
    gtk_box_append(GTK_BOX(toolbar), gtk_label_new("Room:"));
    gtk_box_append(GTK_BOX(toolbar), world);
    gtk_box_append(GTK_BOX(toolbar), gtk_label_new("Layer:"));
    gtk_box_append(GTK_BOX(toolbar), layer);
    gtk_box_append(GTK_BOX(toolbar), gtk_label_new("Brush:"));
    gtk_box_append(GTK_BOX(toolbar), brush);
    gtk_box_append(GTK_BOX(toolbar), save);
    ed->tile_canvas = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(ed->tile_canvas),
                                       (int)(FUSION_TILE_COLS * FUSION_TILE_SIZE));
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(ed->tile_canvas),
                                        (int)(FUSION_TILE_ROWS * FUSION_TILE_SIZE));
    gtk_widget_set_size_request(ed->tile_canvas,
                                (int)(FUSION_TILE_COLS * FUSION_TILE_SIZE),
                                (int)(FUSION_TILE_ROWS * FUSION_TILE_SIZE));
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(ed->tile_canvas),
                                   tile_draw, ed, NULL);
    gtk_widget_add_controller(ed->tile_canvas, GTK_EVENT_CONTROLLER(drag));
    gtk_widget_add_controller(ed->tile_canvas, GTK_EVENT_CONTROLLER(click));
    g_signal_connect(drag, "drag-begin", G_CALLBACK(tile_drag_begin), ed);
    g_signal_connect(drag, "drag-update", G_CALLBACK(tile_drag_update), ed);
    g_signal_connect(click, "pressed", G_CALLBACK(tile_click), ed);
    g_signal_connect(world, "notify::selected", G_CALLBACK(tile_world_changed), ed);
    g_signal_connect(layer, "notify::selected", G_CALLBACK(tile_layer_changed), ed);
    g_signal_connect(brush, "notify::selected", G_CALLBACK(tile_brush_changed), ed);
    g_signal_connect(save, "clicked", G_CALLBACK(tile_save_clicked), ed);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), ed->tile_canvas);
    gtk_widget_set_hexpand(scroll, TRUE);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_box_append(GTK_BOX(root), scroll);
    ed->tile_status = gtk_label_new("Paint local demo colors; physics still use original demo collision rectangles");
    gtk_box_append(GTK_BOX(root), ed->tile_status);
    gtk_notebook_append_page(GTK_NOTEBOOK(tabs), root, gtk_label_new("Tile painter"));
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
    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *refresh = gtk_button_new_with_label("Refresh local sprites");
    GtkWidget *scroll = gtk_scrolled_window_new();
    ed->asset_actor = gtk_drop_down_new_from_strings(asset_actor_names);
    ed->asset_action = gtk_drop_down_new_from_strings(asset_animation_names);
    ed->asset_frame = gtk_spin_button_new_with_range(0, 16, 1);
    ed->asset_picture = gtk_picture_new();
    ed->asset_status = gtk_label_new("Load ROM-extracted BMPs using the local importer");
    gtk_picture_set_can_shrink(GTK_PICTURE(ed->asset_picture), TRUE);
    gtk_widget_set_size_request(ed->asset_picture, 480, 370);
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

static void activate(GtkApplication *app, gpointer user_data)
{
    Editor *ed = user_data;
    GtkWidget *window = gtk_application_window_new(app);
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *title = gtk_label_new("METROID VANIA / SAVE-ROOM GRAPH (DEMO)");
    GtkWidget *save = gtk_button_new_with_label("Save graph");
    GtkWidget *tabs = gtk_notebook_new();
    GtkWidget *graph_tab = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkGesture *drag;
    gtk_window_set_title(GTK_WINDOW(window), "Metroid Vania Map Editor");
    gtk_window_set_default_size(GTK_WINDOW(window), 1140, 760);
    gtk_window_set_child(GTK_WINDOW(window), root);
    gtk_widget_set_margin_start(root, 14);
    gtk_widget_set_margin_end(root, 14);
    gtk_widget_set_margin_top(root, 14);
    gtk_widget_set_margin_bottom(root, 14);
    gtk_box_append(GTK_BOX(root), title);
    gtk_widget_set_vexpand(tabs, TRUE);
    gtk_box_append(GTK_BOX(root), tabs);
    gtk_notebook_append_page(GTK_NOTEBOOK(tabs), graph_tab,
                             gtk_label_new("World graph"));
    ed->canvas = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(ed->canvas), 760);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(ed->canvas), 350);
    gtk_widget_set_hexpand(ed->canvas, TRUE);
    gtk_widget_set_vexpand(ed->canvas, TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(ed->canvas), draw, ed, NULL);
    gtk_box_append(GTK_BOX(graph_tab), ed->canvas);
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
    ed->status = gtk_label_new("Drag room nodes to arrange; this is not a tilemap editor yet.");
    gtk_box_append(GTK_BOX(graph_tab), ed->status);
    build_tile_tab(ed, tabs);
    build_assets_tab(ed, tabs);
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
    return code;
}
