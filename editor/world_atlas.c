/* SPDX-License-Identifier: GPL-3.0-only */
/* Original MZM area minimap anchors and verified same-area door topology. */
#include "world_atlas.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    guint8 area, room, x, y, tileset, save_related;
    char music[96];
} AtlasRoom;
typedef struct {
    guint8 area, source, destination;
    guint16 source_door, destination_door;
} AtlasDoor;
typedef struct {
    GArray *rooms, *doors;
    NativeWorkspace *workspace; /* owned by GTK application, not this tab */
    GtkWidget *canvas, *details, *zoom;
    guint area;
    gint selected;
    double step;
} AtlasUI;
static const char *const area_names[] = {
    "Brinstar", "Kraid", "Norfair", "Ridley", "Tourian", "Crateria", "Chozodia", NULL
};
static gint area_index(const char *name)
{
    for (gint i = 0; area_names[i]; ++i)
        if (g_strcmp0(name, area_names[i]) == 0) return i;
    return -1;
}
static gint room_slot(const AtlasUI *u, guint area, guint room)
{
    for (guint i = 0; i < u->rooms->len; ++i) {
        const AtlasRoom *r = &g_array_index(u->rooms, AtlasRoom, i);
        if (r->area == area && r->room == room) return (gint)i;
    }
    return -1;
}
static void room_center(const AtlasUI *u, const AtlasRoom *r, double *x, double *y)
{
    *x = 44 + (r->x + 0.5) * u->step;
    *y = 44 + (r->y + 0.5) * u->step;
}
static gboolean atlas_load(AtlasUI *u)
{
    gchar *contents = NULL;
    if (!g_file_get_contents("assets/extracted/rooms/metroid/world_atlas.tsv",
                             &contents, NULL, NULL)) return FALSE;
    gchar **lines = g_strsplit(contents, "\n", -1);
    gboolean ok = TRUE;
    for (guint i = 0; lines[i] && ok; ++i) {
        if (!lines[i][0] || lines[i][0] == '#') continue;
        gchar **p = g_strsplit(lines[i], "|", -1);
        if (p[0] && strcmp(p[0], "R") == 0 && g_strv_length(p) == 8) {
            gint area = area_index(p[1]);
            unsigned room, x, y, tileset, save;
            if (area < 0 || sscanf(p[2], "%u", &room) != 1 ||
                sscanf(p[3], "%u", &x) != 1 || sscanf(p[4], "%u", &y) != 1 ||
                sscanf(p[5], "%u", &tileset) != 1 ||
                sscanf(p[7], "%u", &save) != 1 ||
                room > 255 || x > 127 || y > 127 || tileset > 255 || save > 1 ||
                strlen(p[6]) >= sizeof(((AtlasRoom *)0)->music)) {
                ok = FALSE;
            } else {
                AtlasRoom r = {.area = (guint8)area, .room = (guint8)room,
                               .x = (guint8)x, .y = (guint8)y,
                               .tileset = (guint8)tileset, .save_related = (guint8)save};
                snprintf(r.music, sizeof(r.music), "%s", p[6]);
                g_array_append_val(u->rooms, r);
            }
        } else if (p[0] && strcmp(p[0], "D") == 0 && g_strv_length(p) == 6) {
            gint area = area_index(p[1]);
            unsigned source, dest, sd, dd;
            if (area < 0 || sscanf(p[2], "%u", &source) != 1 ||
                sscanf(p[3], "%u", &dest) != 1 ||
                sscanf(p[4], "%u", &sd) != 1 || sscanf(p[5], "%u", &dd) != 1 ||
                source > 255 || dest > 255 || sd > 999 || dd > 999) ok = FALSE;
            else {
                AtlasDoor d = {.area = (guint8)area, .source = (guint8)source,
                               .destination = (guint8)dest,
                               .source_door = (guint16)sd,
                               .destination_door = (guint16)dd};
                g_array_append_val(u->doors, d);
            }
        } else ok = FALSE;
        g_strfreev(p);
    }
    g_strfreev(lines);
    g_free(contents);
    if (!ok) { g_array_set_size(u->rooms, 0); g_array_set_size(u->doors, 0); }
    return ok && u->rooms->len > 0;
}
static void atlas_dimensions(AtlasUI *u)
{
    guint maxx = 20, maxy = 14;
    for (guint i = 0; i < u->rooms->len; ++i) {
        const AtlasRoom *r = &g_array_index(u->rooms, AtlasRoom, i);
        if (r->area != u->area) continue;
        if (r->x > maxx) maxx = r->x;
        if (r->y > maxy) maxy = r->y;
    }
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(u->canvas),
                                       (int)((maxx + 2) * u->step + 88));
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(u->canvas),
                                        (int)((maxy + 2) * u->step + 88));
    gtk_widget_queue_draw(u->canvas);
}
static void atlas_draw(GtkDrawingArea *canvas, cairo_t *cr,
                       int width, int height, gpointer userdata)
{
    AtlasUI *u = userdata;
    (void)canvas;
    cairo_set_source_rgb(cr, 0.075, 0.09, 0.13);
    cairo_paint(cr);
    cairo_set_line_width(cr, 0.6);
    cairo_set_source_rgba(cr, 0.62, 0.72, 0.78, 0.11);
    for (double x = 44; x < width; x += u->step) {
        cairo_move_to(cr, x, 0); cairo_line_to(cr, x, height);
    }
    for (double y = 44; y < height; y += u->step) {
        cairo_move_to(cr, 0, y); cairo_line_to(cr, width, y);
    }
    cairo_stroke(cr);
    cairo_set_line_width(cr, 1.1);
    for (guint i = 0; i < u->doors->len; ++i) {
        const AtlasDoor *d = &g_array_index(u->doors, AtlasDoor, i);
        if (d->area != u->area) continue;
        gint ai = room_slot(u, d->area, d->source);
        gint bi = room_slot(u, d->area, d->destination);
        if (ai < 0 || bi < 0) continue;
        const AtlasRoom *a = &g_array_index(u->rooms, AtlasRoom, ai);
        const AtlasRoom *b = &g_array_index(u->rooms, AtlasRoom, bi);
        double ax, ay, bx, by;
        room_center(u, a, &ax, &ay); room_center(u, b, &bx, &by);
        cairo_set_source_rgba(cr, 0.38, 0.78, 0.83, 0.34);
        cairo_move_to(cr, ax, ay); cairo_line_to(cr, bx, by);
        cairo_stroke(cr);
    }
    for (guint i = 0; i < u->rooms->len; ++i) {
        const AtlasRoom *r = &g_array_index(u->rooms, AtlasRoom, i);
        if (r->area != u->area) continue;
        double x, y;
        room_center(u, r, &x, &y);
        if ((gint)i == u->selected) cairo_set_source_rgb(cr, 1.0, 0.78, 0.29);
        else if (r->save_related) cairo_set_source_rgb(cr, 0.39, 0.92, 0.66);
        else cairo_set_source_rgb(cr, 0.53, 0.73, 0.87);
        cairo_rectangle(cr, x - 8, y - 8, 16, 16);
        cairo_fill(cr);
        if (u->step >= 20.0) {
            char label[12];
            snprintf(label, sizeof(label), "%u", r->room);
            cairo_set_source_rgb(cr, 0.96, 0.96, 0.95);
            cairo_move_to(cr, x + 10, y + 4);
            cairo_show_text(cr, label);
        }
    }
}
static void atlas_select(AtlasUI *u, gint index)
{
    u->selected = index;
    if (index >= 0) {
        const AtlasRoom *r = &g_array_index(u->rooms, AtlasRoom, index);
        gchar *message = g_strdup_printf(
            "%s — salle %03u | minimap %u,%u | tileset %u | %s\n"
            "Clique une salle pour la sélectionner ; double-clique pour l'ouvrir. "
            "Les traits représentent uniquement les portes internes décodées.",
            area_names[r->area], r->room, r->x, r->y, r->tileset, r->music);
        gtk_label_set_text(GTK_LABEL(u->details), message);
        g_free(message);
    } else gtk_label_set_text(GTK_LABEL(u->details), "Choisis une salle sur la carte.");
    gtk_widget_queue_draw(u->canvas);
}
static void atlas_click(GtkGestureClick *gesture, gint presses,
                        double x, double y, gpointer userdata)
{
    AtlasUI *u = userdata;
    (void)gesture;
    gint best = -1;
    double bestd = 13.0 * 13.0;
    for (guint i = 0; i < u->rooms->len; ++i) {
        const AtlasRoom *r = &g_array_index(u->rooms, AtlasRoom, i);
        if (r->area != u->area) continue;
        double rx, ry;
        room_center(u, r, &rx, &ry);
        double dist = (rx-x)*(rx-x) + (ry-y)*(ry-y);
        if (dist < bestd) { best = (gint)i; bestd = dist; }
    }
    atlas_select(u, best);
    if (presses >= 2 && best >= 0) {
        const AtlasRoom *r = &g_array_index(u->rooms, AtlasRoom, best);
        native_workspace_import_async(u->workspace, area_names[r->area], r->room);
    }
}
static void atlas_open(GtkButton *button, gpointer userdata)
{
    AtlasUI *u = userdata;
    (void)button;
    if (u->selected < 0) return;
    const AtlasRoom *r = &g_array_index(u->rooms, AtlasRoom, u->selected);
    native_workspace_import_async(u->workspace, area_names[r->area], r->room);
}
static void area_changed(GObject *object, GParamSpec *pspec, gpointer userdata)
{
    AtlasUI *u = userdata;
    (void)pspec;
    guint val = gtk_drop_down_get_selected(GTK_DROP_DOWN(object));
    if (val >= 7) return;
    u->area = val;
    atlas_dimensions(u);
    atlas_select(u, -1);
}
static void zoom_changed(GtkSpinButton *spin, gpointer userdata)
{
    AtlasUI *u = userdata;
    u->step = gtk_spin_button_get_value(spin) * 0.32;
    atlas_dimensions(u);
}
static void atlas_free(gpointer userdata)
{
    AtlasUI *u = userdata;
    if (u->rooms) g_array_free(u->rooms, TRUE);
    if (u->doors) g_array_free(u->doors, TRUE);
    g_free(u);
}
GtkWidget *world_atlas_build(GtkWidget *center, NativeWorkspace *workspace)
{
    AtlasUI *u = g_new0(AtlasUI, 1);
    u->rooms = g_array_new(FALSE, FALSE, sizeof(AtlasRoom));
    u->doors = g_array_new(FALSE, FALSE, sizeof(AtlasDoor));
    u->workspace = workspace;
    u->selected = -1;
    u->step = 30.0;
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *region = gtk_drop_down_new_from_strings(area_names);
    GtkWidget *zoom = gtk_spin_button_new_with_range(40, 250, 10);
    GtkWidget *open = gtk_button_new_with_label("Open selected room");
    GtkWidget *scroller = gtk_scrolled_window_new();
    GtkWidget *canvas = gtk_drawing_area_new();
    u->canvas = canvas;
    u->details = gtk_label_new("");
    u->zoom = zoom;
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(zoom), 100);
    gtk_box_append(GTK_BOX(toolbar), gtk_label_new("Region:"));
    gtk_box_append(GTK_BOX(toolbar), region);
    gtk_box_append(GTK_BOX(toolbar), gtk_label_new("Zoom:"));
    gtk_box_append(GTK_BOX(toolbar), zoom);
    gtk_box_append(GTK_BOX(toolbar), open);
    gtk_box_append(GTK_BOX(root), toolbar);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(canvas), atlas_draw, u, NULL);
    GtkGesture *click = gtk_gesture_click_new();
    gtk_widget_add_controller(canvas, GTK_EVENT_CONTROLLER(click));
    g_signal_connect(click, "pressed", G_CALLBACK(atlas_click), u);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), canvas);
    gtk_widget_set_hexpand(scroller, TRUE);
    gtk_widget_set_vexpand(scroller, TRUE);
    gtk_box_append(GTK_BOX(root), scroller);
    gtk_label_set_wrap(GTK_LABEL(u->details), TRUE);
    gtk_label_set_xalign(GTK_LABEL(u->details), 0);
    gtk_box_append(GTK_BOX(root), u->details);
    if (!atlas_load(u)) {
        gtk_label_set_text(GTK_LABEL(u->details),
                           "Missing atlas: run python3 -m scripts.mzm_world_atlas");
    } else {
        atlas_dimensions(u);
        atlas_select(u, -1);
    }
    g_signal_connect(region, "notify::selected", G_CALLBACK(area_changed), u);
    g_signal_connect(zoom, "value-changed", G_CALLBACK(zoom_changed), u);
    g_signal_connect(open, "clicked", G_CALLBACK(atlas_open), u);
    g_object_set_data_full(G_OBJECT(root), "mzm-atlas", u, atlas_free);
    gtk_notebook_append_page(GTK_NOTEBOOK(center), root, gtk_label_new("World map / MZM"));
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(center), root, TRUE);
    gtk_notebook_set_tab_detachable(GTK_NOTEBOOK(center), root, TRUE);
    return root;
}
