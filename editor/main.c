/* SPDX-License-Identifier: GPL-3.0-only */
/* GTK4 project-graph editor. Original ROM maps/tilesets are NOT imported yet. */
#include "core/world_graph.h"
#include <gtk/gtk.h>
#include <stdio.h>

typedef struct {
    FusionWorldGraph graph;
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

static void activate(GtkApplication *app, gpointer user_data)
{
    Editor *ed = user_data;
    GtkWidget *window = gtk_application_window_new(app);
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *title = gtk_label_new("METROID VANIA / SAVE-ROOM GRAPH (DEMO)");
    GtkWidget *save = gtk_button_new_with_label("Save graph");
    GtkGesture *drag;
    gtk_window_set_title(GTK_WINDOW(window), "Metroid Vania Map Editor");
    gtk_window_set_default_size(GTK_WINDOW(window), 850, 520);
    gtk_window_set_child(GTK_WINDOW(window), root);
    gtk_widget_set_margin_start(root, 14);
    gtk_widget_set_margin_end(root, 14);
    gtk_widget_set_margin_top(root, 14);
    gtk_widget_set_margin_bottom(root, 14);
    gtk_box_append(GTK_BOX(root), title);
    ed->canvas = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(ed->canvas), 760);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(ed->canvas), 350);
    gtk_widget_set_hexpand(ed->canvas, TRUE);
    gtk_widget_set_vexpand(ed->canvas, TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(ed->canvas), draw, ed, NULL);
    gtk_box_append(GTK_BOX(root), ed->canvas);
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
    gtk_box_append(GTK_BOX(root), ed->toggle);
    gtk_box_append(GTK_BOX(root), save);
    g_signal_connect(save, "clicked", G_CALLBACK(save_clicked), ed);
    ed->status = gtk_label_new("Drag room nodes to arrange; this is not a tilemap editor yet.");
    gtk_box_append(GTK_BOX(root), ed->status);
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
    app = gtk_application_new("org.metroidvania.grapheditor",
                              G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), &editor);
    code = g_application_run(G_APPLICATION(app), 1, argv);
    g_object_unref(app);
    return code;
}
