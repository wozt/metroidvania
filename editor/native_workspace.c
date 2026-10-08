/* SPDX-License-Identifier: GPL-3.0-only */
/* Multi-document native MZM editor. ROM imports stay read-only; every tab is
 * backed by a private in-memory map and an ignored on-disk override. */
#include "native_workspace.h"
#include "core/native_map.h"
#include "core/native_selection.h"
#include <cairo.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { TOOL_PENCIL, TOOL_ERASER, TOOL_FILL, TOOL_PICK, TOOL_SELECT, TOOL_PAN, TOOL_COUNT };
#define HISTORY_LIMIT 16u

typedef struct {
    GtkWidget *popover;
    guint timer;
} DelayedTooltip;

struct NativeWorkspace {
    /* The instance returned by native_workspace_new() manages documents. */
    GPtrArray *documents;
    GtkNotebook *center, *right;
    struct NativeWorkspace *owner; /* non-NULL on a document */
    char *identity;

    GtkWidget *page, *palette_page, *canvas, *palette, *status;
    GtkWidget *layer, *brush, *zoom, *scroller, *grid_button;
    GtkWidget *tools[TOOL_COUNT];
    GtkWidget *tab_title;
    NativeMap *map, *stroke_before;
    NativeMap **undo, **redo;
    unsigned undo_count, redo_count;
    gboolean ready, busy, unsaved, drawing, changed, grid_visible;
    gboolean has_selection, selecting, moving, panning;
    unsigned layer_id, brush_id, tool_id;
    int start_x, start_y, last_x, last_y;
    int sel_x0, sel_y0, sel_x1, sel_y1, preview_dx, preview_dy;
    double scale, pointer_x, pointer_y, pan_horizontal, pan_vertical;
    char *override_path;
    cairo_surface_t *atlas;
    unsigned char *atlas_pixels;
};

static void message(NativeWorkspace *doc, const char *text)
{
    if (doc->status) gtk_label_set_text(GTK_LABEL(doc->status), text);
}

static void discard_atlas(NativeWorkspace *doc)
{
    if (doc->atlas) cairo_surface_destroy(doc->atlas);
    doc->atlas = NULL;
    g_free(doc->atlas_pixels);
    doc->atlas_pixels = NULL;
}

static gboolean load_atlas(NativeWorkspace *doc, const char *filename)
{
    GError *error = NULL;
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(filename, &error);
    if (!pixbuf) {
        if (error) g_error_free(error);
        return FALSE;
    }
    int width = gdk_pixbuf_get_width(pixbuf);
    int height = gdk_pixbuf_get_height(pixbuf);
    int stride = gdk_pixbuf_get_rowstride(pixbuf);
    int channels = gdk_pixbuf_get_n_channels(pixbuf);
    if (width < 16 || width > 1024 || height < 16 || height > 4096 || channels < 3) {
        g_object_unref(pixbuf);
        return FALSE;
    }
    int cairo_stride = cairo_format_stride_for_width(CAIRO_FORMAT_RGB24, width);
    unsigned char *pixels = g_malloc0((size_t)cairo_stride * (size_t)height);
    const guchar *source = gdk_pixbuf_read_pixels(pixbuf);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const guchar *color = source + (size_t)y * (size_t)stride + (size_t)x * (size_t)channels;
            uint32_t rgba = 0xff000000u | ((uint32_t)color[0] << 16) |
                            ((uint32_t)color[1] << 8) | color[2];
            memcpy(pixels + (size_t)y * (size_t)cairo_stride + (size_t)x * 4, &rgba, 4);
        }
    }
    g_object_unref(pixbuf);
    cairo_surface_t *surface = cairo_image_surface_create_for_data(
        pixels, CAIRO_FORMAT_RGB24, width, height, cairo_stride);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        g_free(pixels);
        return FALSE;
    }
    discard_atlas(doc);
    doc->atlas = surface;
    doc->atlas_pixels = pixels;
    return TRUE;
}

static void stamp(NativeWorkspace *doc, cairo_t *cr, unsigned id,
                  double x, double y, double scale)
{
    if (!doc->ready || !doc->atlas || id >= doc->map->tile_count) return;
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, scale, scale);
    cairo_rectangle(cr, 0, 0, 16, 16);
    cairo_clip(cr);
    cairo_set_source_surface(cr, doc->atlas, -(int)(id % 16) * 16,
                             -(int)(id / 16) * 16);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
    cairo_paint(cr);
    cairo_restore(cr);
}

static int low(int a, int b) { return a < b ? a : b; }
static int high(int a, int b) { return a > b ? a : b; }

static gboolean inside_selection(const NativeWorkspace *doc, int x, int y)
{
    return doc->has_selection && x >= low(doc->sel_x0, doc->sel_x1) &&
           x <= high(doc->sel_x0, doc->sel_x1) &&
           y >= low(doc->sel_y0, doc->sel_y1) && y <= high(doc->sel_y0, doc->sel_y1);
}

static void selection_outline(NativeWorkspace *doc, cairo_t *cr)
{
    int x0, y0, x1, y1;
    double cell = 16.0 * doc->scale;
    if (!doc->has_selection) return;
    x0 = low(doc->sel_x0, doc->sel_x1) + doc->preview_dx;
    y0 = low(doc->sel_y0, doc->sel_y1) + doc->preview_dy;
    x1 = high(doc->sel_x0, doc->sel_x1) + doc->preview_dx;
    y1 = high(doc->sel_y0, doc->sel_y1) + doc->preview_dy;
    cairo_save(cr);
    cairo_rectangle(cr, x0 * cell + 0.5, y0 * cell + 0.5,
                    (x1 - x0 + 1) * cell - 1, (y1 - y0 + 1) * cell - 1);
    cairo_set_source_rgba(cr, 0.97, 0.78, 0.18, 0.18);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 1, 0.88, 0.34);
    cairo_set_line_width(cr, 2);
    const double dash[] = {5.0, 4.0};
    cairo_set_dash(cr, dash, 2, 0);
    cairo_stroke(cr);
    cairo_restore(cr);
}

static void draw_room(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    (void)area; (void)width; (void)height;
    cairo_set_source_rgb(cr, 0.11, 0.13, 0.16);
    cairo_paint(cr);
    if (!doc->ready) return;
    unsigned layer = doc->layer_id;
    unsigned columns = doc->map->width[layer];
    unsigned rows = doc->map->height[layer];
    double cell = 16.0 * doc->scale;
    for (unsigned y = 0; y < rows; ++y) {
        for (unsigned x = 0; x < columns; ++x) {
            unsigned id = doc->map->blocks[layer][y * columns + x];
            stamp(doc, cr, id, x * cell, y * cell, doc->scale);
            if (id >= doc->map->tile_count) {
                cairo_set_source_rgba(cr, 1, 0.1, 0.25, 0.45);
                cairo_rectangle(cr, x * cell, y * cell, cell, cell);
                cairo_fill(cr);
            }
        }
    }
    if (doc->grid_visible) {
        cairo_set_source_rgba(cr, 1, 1, 1, 0.21);
        cairo_set_line_width(cr, 0.75);
        for (unsigned x = 0; x <= columns; ++x) {
            cairo_move_to(cr, x * cell + 0.5, 0);
            cairo_line_to(cr, x * cell + 0.5, rows * cell);
        }
        for (unsigned y = 0; y <= rows; ++y) {
            cairo_move_to(cr, 0, y * cell + 0.5);
            cairo_line_to(cr, columns * cell, y * cell + 0.5);
        }
        cairo_stroke(cr);
    }
    selection_outline(doc, cr);
}

static void draw_palette(GtkDrawingArea *area, cairo_t *cr, int width, int height,
                         gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    (void)area; (void)width; (void)height;
    cairo_set_source_rgb(cr, 0.09, 0.10, 0.12);
    cairo_paint(cr);
    if (!doc->ready) return;
    for (unsigned id = 0; id < doc->map->tile_count; ++id)
        stamp(doc, cr, id, (id % 16) * 24, (id / 16) * 24, 1.5);
    cairo_set_source_rgb(cr, 1, 0.8, 0.24);
    cairo_set_line_width(cr, 2);
    cairo_rectangle(cr, (doc->brush_id % 16) * 24 + 1,
                    (doc->brush_id / 16) * 24 + 1, 22, 22);
    cairo_stroke(cr);
}

static void update_title(NativeWorkspace *doc)
{
    if (doc->tab_title) {
        gchar *name = g_strdup_printf("%s%s", doc->identity ? doc->identity : "Room",
                                      doc->unsaved ? " *" : "");
        gtk_label_set_text(GTK_LABEL(doc->tab_title), name);
        g_free(name);
    }
}

static void mark_changed(NativeWorkspace *doc)
{
    doc->changed = TRUE;
    doc->unsaved = TRUE;
    update_title(doc);
    gtk_widget_queue_draw(doc->canvas);
    message(doc, "Modifications non enregistrées — Ctrl+S ou bouton Enregistrer");
}

static void history_clear(NativeMap **stack, unsigned *count)
{
    for (unsigned i = 0; i < *count; ++i) free(stack[i]);
    *count = 0;
}

static void history_push(NativeMap **stack, unsigned *count, const NativeMap *state)
{
    NativeMap *copy = malloc(sizeof(*copy));
    if (!copy) return;
    *copy = *state;
    if (*count >= HISTORY_LIMIT) {
        free(stack[0]);
        memmove(stack, stack + 1, (HISTORY_LIMIT - 1) * sizeof(*stack));
        *count = HISTORY_LIMIT - 1;
    }
    stack[(*count)++] = copy;
}

static void history_step(NativeWorkspace *doc, gboolean redo)
{
    NativeMap **source = redo ? doc->redo : doc->undo;
    NativeMap **target = redo ? doc->undo : doc->redo;
    unsigned *source_count = redo ? &doc->redo_count : &doc->undo_count;
    unsigned *target_count = redo ? &doc->undo_count : &doc->redo_count;
    if (!doc->ready || doc->drawing || !*source_count) return;
    history_push(target, target_count, doc->map);
    NativeMap *snapshot = source[--(*source_count)];
    *doc->map = *snapshot;
    free(snapshot);
    mark_changed(doc);
}

static void begin_edit(NativeWorkspace *doc)
{
    *doc->stroke_before = *doc->map;
    doc->changed = FALSE;
}

static void finish_edit(NativeWorkspace *doc)
{
    if (doc->changed) {
        history_push(doc->undo, &doc->undo_count, doc->stroke_before);
        history_clear(doc->redo, &doc->redo_count);
    }
    doc->changed = FALSE;
}

static void resize_room(NativeWorkspace *doc)
{
    if (!doc->ready || !doc->canvas) return;
    unsigned width = doc->map->width[doc->layer_id];
    unsigned height = doc->map->height[doc->layer_id];
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(doc->canvas),
                                       (int)(width * 16 * doc->scale));
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(doc->canvas),
                                        (int)(height * 16 * doc->scale));
    gtk_widget_queue_draw(doc->canvas);
}

static void brush_changed(GtkSpinButton *spin, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    doc->brush_id = (unsigned)gtk_spin_button_get_value_as_int(spin);
    gtk_widget_queue_draw(doc->palette);
}

static void layer_changed(GObject *object, GParamSpec *pspec, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    (void)pspec;
    if (doc->drawing) return;
    doc->layer_id = gtk_drop_down_get_selected(GTK_DROP_DOWN(object));
    doc->has_selection = FALSE;
    doc->preview_dx = doc->preview_dy = 0;
    resize_room(doc);
}

static void zoom_changed(GtkSpinButton *spin, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    if (doc->drawing) return;
    doc->scale = gtk_spin_button_get_value(spin) / 100.0;
    resize_room(doc);
}

static gboolean get_cell(NativeWorkspace *doc, double x, double y, int *cx, int *cy)
{
    if (!doc->ready || x < 0 || y < 0) return FALSE;
    *cx = (int)(x / (16.0 * doc->scale));
    *cy = (int)(y / (16.0 * doc->scale));
    return *cx >= 0 && *cy >= 0 &&
           (unsigned)*cx < doc->map->width[doc->layer_id] &&
           (unsigned)*cy < doc->map->height[doc->layer_id];
}

static void paint_cell(NativeWorkspace *doc, int x, int y)
{
    unsigned picked = doc->brush_id;
    if (native_map_edit(doc->map, doc->layer_id, x, y,
                        doc->tool_id, doc->brush_id, &picked))
        mark_changed(doc);
    if (doc->tool_id == TOOL_PICK && picked < doc->map->tile_count)
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(doc->brush), picked);
}

static void paint_line(NativeWorkspace *doc, int x, int y)
{
    if (doc->last_x < 0) {
        paint_cell(doc, x, y);
    } else {
        int x0 = doc->last_x, y0 = doc->last_y;
        int dx = abs(x - x0), dy = -abs(y - y0);
        int sx = x0 < x ? 1 : -1, sy = y0 < y ? 1 : -1;
        int error = dx + dy;
        for (;;) {
            paint_cell(doc, x0, y0);
            if (x0 == x && y0 == y) break;
            int step = error * 2;
            if (step >= dy) { error += dy; x0 += sx; }
            if (step <= dx) { error += dx; y0 += sy; }
        }
    }
    doc->last_x = x;
    doc->last_y = y;
}

static void gesture_begin(GtkGestureDrag *gesture, double x, double y, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    (void)gesture;
    if (!doc->ready || doc->drawing) return;
    int cx = 0, cy = 0;
    if (doc->tool_id != TOOL_PAN && !get_cell(doc, x, y, &cx, &cy)) return;
    doc->drawing = TRUE;
    doc->start_x = cx;
    doc->start_y = cy;
    doc->last_x = doc->last_y = -1;
    doc->pointer_x = x;
    doc->pointer_y = y;
    doc->selecting = doc->moving = doc->panning = FALSE;
    if (doc->tool_id == TOOL_PAN) {
        GtkAdjustment *h = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(doc->scroller));
        GtkAdjustment *v = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(doc->scroller));
        doc->pan_horizontal = gtk_adjustment_get_value(h);
        doc->pan_vertical = gtk_adjustment_get_value(v);
        doc->panning = TRUE;
    } else if (doc->tool_id == TOOL_SELECT) {
        if (inside_selection(doc, cx, cy)) {
            doc->moving = TRUE;
            begin_edit(doc);
        } else {
            doc->has_selection = TRUE;
            doc->selecting = TRUE;
            doc->sel_x0 = doc->sel_x1 = cx;
            doc->sel_y0 = doc->sel_y1 = cy;
            gtk_widget_queue_draw(doc->canvas);
        }
    } else {
        begin_edit(doc);
        paint_cell(doc, cx, cy);
        doc->last_x = cx;
        doc->last_y = cy;
    }
}

static void gesture_update(GtkGestureDrag *gesture, double dx, double dy, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    (void)gesture;
    if (!doc->ready || !doc->drawing) return;
    if (doc->panning) {
        GtkAdjustment *h = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(doc->scroller));
        GtkAdjustment *v = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(doc->scroller));
        gtk_adjustment_set_value(h, doc->pan_horizontal - dx);
        gtk_adjustment_set_value(v, doc->pan_vertical - dy);
        return;
    }
    int cx = 0, cy = 0;
    if (!get_cell(doc, doc->pointer_x + dx, doc->pointer_y + dy, &cx, &cy)) return;
    if (doc->selecting) {
        doc->sel_x1 = cx;
        doc->sel_y1 = cy;
        gtk_widget_queue_draw(doc->canvas);
    } else if (doc->moving) {
        int shift_x = cx - doc->start_x;
        int shift_y = cy - doc->start_y;
        int w = (int)doc->map->width[doc->layer_id];
        int h = (int)doc->map->height[doc->layer_id];
        if (low(doc->sel_x0, doc->sel_x1) + shift_x < 0 ||
            high(doc->sel_x0, doc->sel_x1) + shift_x >= w ||
            low(doc->sel_y0, doc->sel_y1) + shift_y < 0 ||
            high(doc->sel_y0, doc->sel_y1) + shift_y >= h) return;
        doc->preview_dx = shift_x;
        doc->preview_dy = shift_y;
        gtk_widget_queue_draw(doc->canvas);
    } else if (doc->tool_id == TOOL_PENCIL || doc->tool_id == TOOL_ERASER) {
        if (cx != doc->last_x || cy != doc->last_y) paint_line(doc, cx, cy);
    }
}

static void gesture_end(GtkGestureDrag *gesture, double dx, double dy, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    (void)gesture; (void)dx; (void)dy;
    if (!doc->drawing) return;
    if (doc->moving && (doc->preview_dx || doc->preview_dy)) {
        if (native_selection_move(doc->map, doc->layer_id,
                                  low(doc->sel_x0, doc->sel_x1),
                                  low(doc->sel_y0, doc->sel_y1),
                                  high(doc->sel_x0, doc->sel_x1),
                                  high(doc->sel_y0, doc->sel_y1),
                                  doc->preview_dx, doc->preview_dy)) {
            doc->sel_x0 += doc->preview_dx;
            doc->sel_x1 += doc->preview_dx;
            doc->sel_y0 += doc->preview_dy;
            doc->sel_y1 += doc->preview_dy;
            mark_changed(doc);
        }
    }
    if (doc->tool_id != TOOL_SELECT && doc->tool_id != TOOL_PAN) finish_edit(doc);
    if (doc->moving) finish_edit(doc);
    doc->preview_dx = doc->preview_dy = 0;
    doc->selecting = doc->moving = doc->panning = FALSE;
    doc->drawing = FALSE;
    gtk_widget_queue_draw(doc->canvas);
}

/* GestureClick covers single-click paint / fill; GestureDrag covers continuous
 * strokes and selection. Group them so the drag is not cancelled on motion. */
static void canvas_click_down(GtkGestureClick *click, int n_press,
                              double x, double y, gpointer userdata)
{
    (void)click; (void)n_press;
    gesture_begin(NULL, x, y, userdata);
}

static void canvas_click_up(GtkGestureClick *click, int n_press,
                            double x, double y, gpointer userdata)
{
    (void)click; (void)n_press; (void)x; (void)y;
    gesture_end(NULL, 0, 0, userdata);
}

static void palette_click(GtkGestureClick *gesture, int n_press, double x, double y,
                          gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    (void)gesture; (void)n_press;
    if (!doc->ready || x < 0 || y < 0) return;
    unsigned column = (unsigned)(x / 24), row = (unsigned)(y / 24);
    unsigned id = row * 16 + column;
    if (column < 16 && id < doc->map->tile_count)
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(doc->brush), id);
}

static gboolean tooltip_timeout(gpointer userdata)
{
    DelayedTooltip *tip = userdata;
    tip->timer = 0;
    gtk_popover_popup(GTK_POPOVER(tip->popover));
    return G_SOURCE_REMOVE;
}

static void tooltip_enter(GtkEventControllerMotion *motion, double x, double y,
                          gpointer userdata)
{
    DelayedTooltip *tip = userdata;
    (void)motion; (void)x; (void)y;
    if (tip->timer) g_source_remove(tip->timer);
    tip->timer = g_timeout_add(1500, tooltip_timeout, tip);
}

static void tooltip_leave(GtkEventControllerMotion *motion, gpointer userdata)
{
    DelayedTooltip *tip = userdata;
    (void)motion;
    if (tip->timer) g_source_remove(tip->timer);
    tip->timer = 0;
    gtk_popover_popdown(GTK_POPOVER(tip->popover));
}

static void tooltip_cleanup(gpointer userdata)
{
    DelayedTooltip *tip = userdata;
    if (tip->timer) g_source_remove(tip->timer);
    g_free(tip);
}

static void delayed_tip(GtkWidget *widget, const char *description)
{
    DelayedTooltip *tip = g_new0(DelayedTooltip, 1);
    GtkWidget *popover = gtk_popover_new();
    GtkWidget *label = gtk_label_new(description);
    tip->popover = popover;
    gtk_widget_set_margin_start(label, 8);
    gtk_widget_set_margin_end(label, 8);
    gtk_widget_set_margin_top(label, 4);
    gtk_widget_set_margin_bottom(label, 4);
    gtk_popover_set_child(GTK_POPOVER(popover), label);
    gtk_popover_set_autohide(GTK_POPOVER(popover), FALSE);
    gtk_widget_set_parent(popover, widget);
    GtkEventController *motion = gtk_event_controller_motion_new();
    gtk_widget_add_controller(widget, motion);
    g_signal_connect(motion, "enter", G_CALLBACK(tooltip_enter), tip);
    g_signal_connect(motion, "leave", G_CALLBACK(tooltip_leave), tip);
    g_object_set_data_full(G_OBJECT(widget), "native-delayed-tooltip", tip, tooltip_cleanup);
    gtk_accessible_update_property(GTK_ACCESSIBLE(widget),
                                   GTK_ACCESSIBLE_PROPERTY_LABEL, description, -1);
}

static GtkWidget *icon_button(const char *icon, const char *description)
{
    GtkWidget *button = gtk_button_new_from_icon_name(icon);
    gtk_widget_add_css_class(button, "flat");
    gtk_widget_set_size_request(button, 34, 34);
    delayed_tip(button, description);
    return button;
}

static GtkWidget *icon_toggle(const char *icon, const char *description)
{
    GtkWidget *button = gtk_toggle_button_new();
    gtk_button_set_child(GTK_BUTTON(button), gtk_image_new_from_icon_name(icon));
    gtk_widget_add_css_class(button, "flat");
    gtk_widget_set_size_request(button, 34, 34);
    delayed_tip(button, description);
    return button;
}

static void tool_toggled(GtkToggleButton *button, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    if (doc->drawing) return;
    if (!gtk_toggle_button_get_active(button)) {
        if (doc->tools[doc->tool_id] == GTK_WIDGET(button))
            gtk_toggle_button_set_active(button, TRUE);
        return;
    }
    for (unsigned i = 0; i < TOOL_COUNT; ++i) {
        if (doc->tools[i] == GTK_WIDGET(button)) {
            doc->tool_id = i;
        } else {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(doc->tools[i]), FALSE);
        }
    }
}

static void grid_toggled(GtkToggleButton *button, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    doc->grid_visible = gtk_toggle_button_get_active(button);
    if (doc->canvas) gtk_widget_queue_draw(doc->canvas);
}

static void undo_clicked(GtkButton *button, gpointer userdata)
{ (void)button; history_step(userdata, FALSE); }

static void redo_clicked(GtkButton *button, gpointer userdata)
{ (void)button; history_step(userdata, TRUE); }

static void save_clicked(GtkButton *button, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    char error[160] = {0};
    (void)button;
    if (!doc->ready || !doc->override_path || doc->drawing) return;
    if (native_map_save(doc->map, doc->override_path, error, sizeof(error))) {
        doc->unsaved = FALSE;
        update_title(doc);
        message(doc, "Override sauvegardé. La ROM et l'import initial sont intacts.");
    } else {
        message(doc, error);
    }
}

static void close_clicked(GtkButton *button, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    (void)button;
    if (doc->busy || doc->drawing) return;
    if (doc->unsaved) {
        message(doc, "Modifications non sauvegardées : enregistre avant de fermer l'onglet.");
        return;
    }
    NativeWorkspace *manager = doc->owner;
    GtkWidget *parent = gtk_widget_get_parent(doc->page);
    if (GTK_IS_NOTEBOOK(parent)) {
        gtk_notebook_remove_page(GTK_NOTEBOOK(parent),
                                 gtk_notebook_page_num(GTK_NOTEBOOK(parent), doc->page));
    }
    parent = gtk_widget_get_parent(doc->palette_page);
    if (GTK_IS_NOTEBOOK(parent)) {
        gtk_notebook_remove_page(GTK_NOTEBOOK(parent),
                                 gtk_notebook_page_num(GTK_NOTEBOOK(parent), doc->palette_page));
    }
    for (guint i = 0; i < manager->documents->len; ++i) {
        if (g_ptr_array_index(manager->documents, i) == doc) {
            g_ptr_array_remove_index(manager->documents, i);
            break;
        }
    }
    history_clear(doc->undo, &doc->undo_count);
    history_clear(doc->redo, &doc->redo_count);
    discard_atlas(doc);
    free(doc->undo);
    free(doc->redo);
    free(doc->map);
    free(doc->stroke_before);
    g_free(doc->override_path);
    g_free(doc->identity);
    free(doc);
}

static void focus_page(GtkWidget *page)
{
    GtkWidget *parent = gtk_widget_get_parent(page);
    if (GTK_IS_NOTEBOOK(parent)) {
        GtkNotebook *notebook = GTK_NOTEBOOK(parent);
        gtk_notebook_set_current_page(notebook, gtk_notebook_page_num(notebook, page));
        GtkRoot *root = gtk_widget_get_root(parent);
        if (GTK_IS_WINDOW(root)) gtk_window_present(GTK_WINDOW(root));
    }
}

static gboolean key_pressed(GtkEventControllerKey *controller, guint keyval,
                            guint keycode, GdkModifierType modifiers, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    (void)controller; (void)keycode;
    if (modifiers & GDK_CONTROL_MASK) {
        switch (gdk_keyval_to_lower(keyval)) {
        case GDK_KEY_s: save_clicked(NULL, doc); return TRUE;
        case GDK_KEY_z: history_step(doc, (modifiers & GDK_SHIFT_MASK) != 0); return TRUE;
        case GDK_KEY_y: history_step(doc, TRUE); return TRUE;
        default: break;
        }
        return FALSE;
    }
    switch (gdk_keyval_to_lower(keyval)) {
    case GDK_KEY_g:
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(doc->grid_button),
            !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(doc->grid_button)));
        return TRUE;
    case GDK_KEY_b: doc->tool_id = TOOL_PENCIL; break;
    case GDK_KEY_e: doc->tool_id = TOOL_ERASER; break;
    case GDK_KEY_f: doc->tool_id = TOOL_FILL; break;
    case GDK_KEY_i: doc->tool_id = TOOL_PICK; break;
    case GDK_KEY_v: doc->tool_id = TOOL_SELECT; break;
    case GDK_KEY_h: doc->tool_id = TOOL_PAN; break;
    default: return FALSE;
    }
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(doc->tools[doc->tool_id]), TRUE);
    return TRUE;
}

static void document_build(NativeWorkspace *doc)
{
    static const char *const layers[] = {"BG1", "BG2", NULL};
    static const char *const icons[TOOL_COUNT] = {
        "document-edit-symbolic", "edit-clear-symbolic", "color-fill-symbolic",
        "color-select-symbolic", "edit-select-all-symbolic", "transform-move-symbolic"
    };
    static const char *const names[TOOL_COUNT] = {
        "Crayon (dessiner)", "Gomme (effacer)", "Pot de peinture (remplissage)",
        "Pipette (prélever un motif)", "Sélection rectangulaire (glisser pour déplacer)",
        "Main (déplacer la vue)"
    };
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *tools = gtk_flow_box_new();
    GtkWidget *scroll = gtk_scrolled_window_new();
    GtkWidget *palette_scroll = gtk_scrolled_window_new();
    GtkWidget *palette = gtk_drawing_area_new();
    GtkWidget *undo = icon_button("edit-undo-symbolic", "Annuler");
    GtkWidget *redo = icon_button("edit-redo-symbolic", "Rétablir");
    GtkWidget *save = icon_button("document-save-symbolic", "Enregistrer l'override (Ctrl+S)");
    GtkWidget *grid = icon_toggle("view-grid-symbolic", "Afficher / masquer la grille (G)");
    GtkWidget *tab_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *close = icon_button("window-close-symbolic", "Fermer cet onglet");
    doc->tab_title = gtk_label_new(doc->identity);
    gtk_box_append(GTK_BOX(tab_box), doc->tab_title);
    gtk_box_append(GTK_BOX(tab_box), close);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(tools), GTK_SELECTION_NONE);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(tools), 20);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(tools), 3);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(tools), 3);
    gtk_widget_add_css_class(tools, "toolbar");
    for (unsigned i = 0; i < TOOL_COUNT; ++i) {
        doc->tools[i] = icon_toggle(icons[i], names[i]);
        gtk_flow_box_insert(GTK_FLOW_BOX(tools), doc->tools[i], -1);
        g_signal_connect(doc->tools[i], "toggled", G_CALLBACK(tool_toggled), doc);
    }
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(doc->tools[TOOL_PENCIL]), TRUE);
    doc->grid_button = grid;
    doc->grid_visible = TRUE;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(grid), TRUE);
    gtk_flow_box_insert(GTK_FLOW_BOX(tools), gtk_separator_new(GTK_ORIENTATION_VERTICAL), -1);
    gtk_flow_box_insert(GTK_FLOW_BOX(tools), grid, -1);
    gtk_flow_box_insert(GTK_FLOW_BOX(tools), undo, -1);
    gtk_flow_box_insert(GTK_FLOW_BOX(tools), redo, -1);
    gtk_flow_box_insert(GTK_FLOW_BOX(tools), save, -1);
    doc->layer = gtk_drop_down_new_from_strings(layers);
    doc->brush = gtk_spin_button_new_with_range(0, 1023, 1);
    doc->zoom = gtk_spin_button_new_with_range(50, 400, 25);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(doc->zoom), 200);
    GtkWidget *values = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(values), gtk_label_new("Calque"));
    gtk_box_append(GTK_BOX(values), doc->layer);
    gtk_box_append(GTK_BOX(values), gtk_label_new("Metatile"));
    gtk_box_append(GTK_BOX(values), doc->brush);
    gtk_box_append(GTK_BOX(values), gtk_label_new("Zoom %"));
    gtk_box_append(GTK_BOX(values), doc->zoom);
    gtk_widget_set_hexpand(values, FALSE);
    gtk_box_append(GTK_BOX(page), tools);
    gtk_box_append(GTK_BOX(page), values);
    doc->canvas = gtk_drawing_area_new();
    doc->scroller = scroll;
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(doc->canvas), 320);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(doc->canvas), 240);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(doc->canvas), draw_room, doc, NULL);
    GtkGesture *drag = gtk_gesture_drag_new();
    GtkGesture *click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(drag), GDK_BUTTON_PRIMARY);
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), GDK_BUTTON_PRIMARY);
    gtk_gesture_group(drag, click);
    gtk_widget_add_controller(doc->canvas, GTK_EVENT_CONTROLLER(drag));
    gtk_widget_add_controller(doc->canvas, GTK_EVENT_CONTROLLER(click));
    g_signal_connect(drag, "drag-begin", G_CALLBACK(gesture_begin), doc);
    g_signal_connect(drag, "drag-update", G_CALLBACK(gesture_update), doc);
    g_signal_connect(drag, "drag-end", G_CALLBACK(gesture_end), doc);
    g_signal_connect(click, "pressed", G_CALLBACK(canvas_click_down), doc);
    g_signal_connect(click, "released", G_CALLBACK(canvas_click_up), doc);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), doc->canvas);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(scroll, TRUE);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_box_append(GTK_BOX(page), scroll);
    doc->status = gtk_label_new("Importation en cours…");
    gtk_label_set_xalign(GTK_LABEL(doc->status), 0);
    gtk_label_set_wrap(GTK_LABEL(doc->status), TRUE);
    gtk_box_append(GTK_BOX(page), doc->status);
    GtkEventController *keys = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(keys, GTK_PHASE_CAPTURE);
    gtk_widget_add_controller(page, keys);
    g_signal_connect(keys, "key-pressed", G_CALLBACK(key_pressed), doc);
    gtk_widget_set_focusable(page, TRUE);
    doc->page = page;
    gtk_notebook_append_page(doc->owner->center, page, tab_box);
    gtk_notebook_set_tab_reorderable(doc->owner->center, page, TRUE);
    gtk_notebook_set_tab_detachable(doc->owner->center, page, TRUE);
    gtk_widget_set_size_request(palette, 384, 200);
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(palette), 384);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(palette), 500);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(palette), draw_palette, doc, NULL);
    GtkGesture *pick = gtk_gesture_click_new();
    gtk_widget_add_controller(palette, GTK_EVENT_CONTROLLER(pick));
    g_signal_connect(pick, "pressed", G_CALLBACK(palette_click), doc);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(palette_scroll), palette);
    gtk_widget_set_hexpand(palette_scroll, TRUE);
    gtk_widget_set_vexpand(palette_scroll, TRUE);
    doc->palette = palette;
    doc->palette_page = palette_scroll;
    GtkWidget *palette_label = gtk_label_new("Metatiles natifs");
    gtk_notebook_append_page(doc->owner->right, palette_scroll, palette_label);
    gtk_notebook_set_tab_reorderable(doc->owner->right, palette_scroll, TRUE);
    gtk_notebook_set_tab_detachable(doc->owner->right, palette_scroll, TRUE);
    g_signal_connect(doc->layer, "notify::selected", G_CALLBACK(layer_changed), doc);
    g_signal_connect(doc->brush, "value-changed", G_CALLBACK(brush_changed), doc);
    g_signal_connect(doc->zoom, "value-changed", G_CALLBACK(zoom_changed), doc);
    g_signal_connect(grid, "toggled", G_CALLBACK(grid_toggled), doc);
    g_signal_connect(undo, "clicked", G_CALLBACK(undo_clicked), doc);
    g_signal_connect(redo, "clicked", G_CALLBACK(redo_clicked), doc);
    g_signal_connect(save, "clicked", G_CALLBACK(save_clicked), doc);
    g_signal_connect(close, "clicked", G_CALLBACK(close_clicked), doc);
    focus_page(page);
    gtk_widget_grab_focus(page);
    focus_page(palette_scroll);
}

static void open_room(NativeWorkspace *doc, const char *area, unsigned number)
{
    char area_lower[32], base[320], over[320], atlas[420], error[180] = {0};
    size_t length = strlen(area);
    if (length >= sizeof(area_lower)) return;
    for (size_t i = 0; i < length; ++i) area_lower[i] = (char)g_ascii_tolower(area[i]);
    area_lower[length] = '\0';
    snprintf(base, sizeof(base),
             "assets/extracted/rooms/metroid/workrooms/%s_%03u.mvnative",
             area_lower, number);
    snprintf(over, sizeof(over),
             "assets/extracted/overrides/metroid/%s_%03u.mvnative",
             area_lower, number);
    NativeMap *temporary = calloc(1, sizeof(*temporary));
    if (!temporary) { message(doc, "Mémoire insuffisante"); return; }
    const char *source = g_file_test(over, G_FILE_TEST_IS_REGULAR) ? over : base;
    if (!native_map_load(temporary, source, error, sizeof(error))) {
        message(doc, error);
        free(temporary);
        return;
    }
    snprintf(atlas, sizeof(atlas), "assets/extracted/%s", temporary->atlas);
    if (!load_atlas(doc, atlas)) {
        message(doc, "Impossible de charger l'atlas authentique de cette salle");
        free(temporary);
        return;
    }
    *doc->map = *temporary;
    free(temporary);
    history_clear(doc->undo, &doc->undo_count);
    history_clear(doc->redo, &doc->redo_count);
    g_free(doc->override_path);
    doc->override_path = g_strdup(over);
    char *dir = g_path_get_dirname(over);
    g_mkdir_with_parents(dir, 0700);
    g_free(dir);
    doc->ready = TRUE;
    doc->unsaved = FALSE;
    doc->brush_id = doc->layer_id = 0;
    doc->has_selection = FALSE;
    doc->preview_dx = doc->preview_dy = 0;
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(doc->brush), 0,
                              doc->map->tile_count - 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(doc->brush), 0);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(doc->layer), 0);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(doc->palette),
                                        (int)((doc->map->tile_count + 15) / 16 * 24));
    resize_room(doc);
    gtk_widget_queue_draw(doc->palette);
    update_title(doc);
    gchar *summary = g_strdup_printf("%s — %u metatiles d'origine, BG1/BG2. %s",
                                     doc->map->room_id, doc->map->tile_count,
                                     strcmp(source, over) == 0 ?
                                     "Override précédent restauré." : "Copie privée créée.");
    message(doc, summary);
    g_free(summary);
    focus_page(doc->page);
    focus_page(doc->palette_page);
}

static void imported(GObject *object, GAsyncResult *result, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    GSubprocess *proc = G_SUBPROCESS(object);
    GError *error = NULL;
    gchar *out = NULL, *err = NULL;
    gboolean succeeded = g_subprocess_communicate_utf8_finish(
        proc, result, &out, &err, &error);
    doc->busy = FALSE;
    if (succeeded && g_subprocess_get_successful(proc)) {
        const char *area = g_object_get_data(G_OBJECT(proc), "native-area");
        unsigned number = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(proc), "native-room"));
        open_room(doc, area, number);
    } else {
        message(doc, error ? error->message : err ? err : "Échec de l'importation");
    }
    if (error) g_error_free(error);
    g_free(out);
    g_free(err);
}

static void start_import(NativeWorkspace *doc, const char *area, unsigned number)
{
    char room_text[16];
    snprintf(room_text, sizeof(room_text), "%u", number);
    GError *error = NULL;
    GSubprocess *proc = g_subprocess_new(
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE,
        &error, "python3", "-m", "scripts.mzm_native_workspace",
        "--area", area, "--room", room_text, NULL);
    if (!proc) {
        message(doc, error ? error->message : "Python importer unavailable");
        if (error) g_error_free(error);
        return;
    }
    doc->busy = TRUE;
    message(doc, "Importation des données vérifiées de la ROM…");
    g_object_set_data_full(G_OBJECT(proc), "native-area", g_strdup(area), g_free);
    g_object_set_data(G_OBJECT(proc), "native-room", GUINT_TO_POINTER(number));
    g_subprocess_communicate_utf8_async(proc, NULL, NULL, imported, doc);
    g_object_unref(proc);
}

NativeWorkspace *native_workspace_new(void)
{
    NativeWorkspace *manager = calloc(1, sizeof(*manager));
    if (!manager) return NULL;
    manager->documents = g_ptr_array_new();
    return manager;
}

void native_workspace_build(NativeWorkspace *manager, GtkWidget *center, GtkWidget *right)
{
    if (!manager || !GTK_IS_NOTEBOOK(center) || !GTK_IS_NOTEBOOK(right)) return;
    manager->center = GTK_NOTEBOOK(center);
    manager->right = GTK_NOTEBOOK(right);
}

void native_workspace_import_async(NativeWorkspace *manager, const char *area, unsigned number)
{
    if (!manager || !manager->documents || !manager->center || !manager->right ||
        !area || !area[0] || strlen(area) > 23 || number > 999) return;
    for (const char *p = area; *p; ++p)
        if (!g_ascii_isalnum(*p)) return;
    char *identity = g_strdup_printf("%s %03u", area, number);
    for (guint i = 0; i < manager->documents->len; ++i) {
        NativeWorkspace *doc = g_ptr_array_index(manager->documents, i);
        if (g_ascii_strcasecmp(doc->identity, identity) == 0) {
            focus_page(doc->page);
            focus_page(doc->palette_page);
            if (!doc->busy && !doc->ready) start_import(doc, area, number);
            g_free(identity);
            return;
        }
    }
    NativeWorkspace *doc = calloc(1, sizeof(*doc));
    if (!doc) { g_free(identity); return; }
    doc->owner = manager;
    doc->identity = identity;
    doc->map = calloc(1, sizeof(NativeMap));
    doc->stroke_before = calloc(1, sizeof(NativeMap));
    doc->undo = calloc(HISTORY_LIMIT, sizeof(*doc->undo));
    doc->redo = calloc(HISTORY_LIMIT, sizeof(*doc->redo));
    if (!doc->map || !doc->stroke_before || !doc->undo || !doc->redo) {
        free(doc->map); free(doc->stroke_before); free(doc->undo); free(doc->redo);
        g_free(doc->identity); free(doc); return;
    }
    doc->scale = 2.0;
    doc->last_x = doc->last_y = -1;
    g_ptr_array_add(manager->documents, doc);
    document_build(doc);
    start_import(doc, area, number);
}

void native_workspace_free(NativeWorkspace *manager)
{
    if (!manager) return;
    if (manager->documents) {
        for (guint i = 0; i < manager->documents->len; ++i) {
            NativeWorkspace *doc = g_ptr_array_index(manager->documents, i);
            history_clear(doc->undo, &doc->undo_count);
            history_clear(doc->redo, &doc->redo_count);
            discard_atlas(doc);
            free(doc->undo); free(doc->redo);
            free(doc->map); free(doc->stroke_before);
            g_free(doc->override_path); g_free(doc->identity);
            free(doc);
        }
        g_ptr_array_free(manager->documents, TRUE);
    }
    free(manager);
}
