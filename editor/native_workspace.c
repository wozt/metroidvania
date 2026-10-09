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
enum { OVERLAY_COLLISION, OVERLAY_ENEMIES, OVERLAY_ITEMS, OVERLAY_OBJECTS,
       OVERLAY_DOORS, OVERLAY_EVENTS, OVERLAY_TRIGGERS, OVERLAY_OTHER,
       OVERLAY_COUNT };
#define HISTORY_LIMIT 16u

typedef struct {
    guint kind, index;
    int x, y;
    guint width, height;
    char variant[48], native_type[96], label[384], details[320];
    gboolean project_owned; /* Original native records remain read-only. */
} RoomAnnotation;

struct NativeWorkspace {
    /* The instance returned by native_workspace_new() manages documents. */
    GPtrArray *documents;
    GtkNotebook *center, *right;
    struct NativeWorkspace *owner; /* non-NULL on a document */
    guint references;
    GCancellable *import_cancellable;
    GSubprocess *import_process;
    gboolean closing;
    gboolean close_pending;
    gboolean close_choice_pending;
    GtkWidget *close_dialog, *close_info;
    GtkWidget *close_buttons[3]; /* Cancel, Discard, Save */
    gboolean pointer_over_canvas;
    int hover_col, hover_row;
    gboolean pick_to_pencil;
    char *identity;

    GtkWidget *page, *palette_page, *canvas, *palette, *status;
    GtkWidget *annotations_list, *annotations_status;
    GtkWidget *annotation_popup;
    GtkWidget *layer, *brush, *zoom, *scroller, *grid_button;
    GtkWidget *tools[TOOL_COUNT];
    GtkWidget *tab_title, *close_button, *save_button;
    NativeMap *map, *stroke_before;
    NativeMap **undo, **redo;
    unsigned undo_count, redo_count;
    gboolean ready, busy, unsaved, drawing, changed, grid_visible;
    gboolean has_selection, selecting, moving, panning;
    unsigned layer_id, brush_id, tool_id;
    int start_x, start_y, last_x, last_y;
    int sel_x0, sel_y0, sel_x1, sel_y1, preview_dx, preview_dy;
    double scale, pointer_x, pointer_y, pan_horizontal, pan_vertical;
    double pending_pan_horizontal, pending_pan_vertical;
    guint pan_tick;
    char *override_path;
    char *annotations_path, *project_area;
    guint project_room, project_move_id, project_drag_id;
    guint project_drag_array_index;
    int project_drag_origin_x, project_drag_origin_y;
    gboolean project_aria, project_move_pending, project_click_consumed;
    gboolean dragging_project;
    GtkWidget *overlay_buttons[OVERLAY_COUNT];
    cairo_surface_t *atlas;
    unsigned char *atlas_pixels;
    cairo_surface_t *background;
    unsigned char *background_pixels;
    gboolean background_visible;
    cairo_surface_t *collision;
    unsigned char *collision_pixels;
    gboolean overlays[OVERLAY_COUNT];
    GArray *annotations;
    guint selected_annotation;
    gboolean annotation_selected;
};

static NativeWorkspace *document_ref(NativeWorkspace *doc)
{
    g_return_val_if_fail(doc != NULL, NULL);
    g_return_val_if_fail(doc->owner != NULL, NULL);
    ++doc->references;
    return doc;
}

static void history_clear(NativeMap **stack, unsigned *count);
static void document_unref(NativeWorkspace *doc);
static void focus_page(GtkWidget *page);
static void annotation_popup_close(NativeWorkspace *doc);
static void annotation_list_context_pressed(GtkGestureClick *gesture, gint presses,
                                            double x, double y, gpointer userdata);

/* GTK owns these widgets. A widget's lifetime holds a document ref. Clear
 * the borrowed pointer BEFORE releasing that ref so the final widget can
 * safely release the final document, including detached/external removals. */
typedef struct {
    NativeWorkspace *doc;
    GtkWidget **slot;
} DocumentWidgetWatch;

static void document_widget_gone(gpointer data)
{
    DocumentWidgetWatch *watch = data;
    *watch->slot = NULL;
    document_unref(watch->doc);
    g_free(watch);
}

static void document_track_widget(NativeWorkspace *doc, GtkWidget **slot,
                                  GtkWidget *widget)
{
    DocumentWidgetWatch *watch = g_new(DocumentWidgetWatch, 1);
    watch->doc = document_ref(doc);
    watch->slot = slot;
    *slot = widget;
    g_object_set_data_full(G_OBJECT(widget), "native-workspace-document",
                           watch, document_widget_gone);
}

static void message(NativeWorkspace *doc, const char *text)
{
    if (!doc->closing && doc->status) gtk_label_set_text(GTK_LABEL(doc->status), text);
}

static void discard_atlas(NativeWorkspace *doc)
{
    if (doc->atlas) cairo_surface_destroy(doc->atlas);
    doc->atlas = NULL;
    g_free(doc->atlas_pixels);
    doc->atlas_pixels = NULL;
}

/* Optional private BG3 preview, never an authored/ROM-modifying layer. */
static void discard_background(NativeWorkspace *doc)
{
    if (doc->background) cairo_surface_destroy(doc->background);
    doc->background = NULL;
    g_free(doc->background_pixels);
    doc->background_pixels = NULL;
}

static void discard_collision(NativeWorkspace *doc)
{
    if (doc->collision) cairo_surface_destroy(doc->collision);
    doc->collision = NULL;
    g_free(doc->collision_pixels);
    doc->collision_pixels = NULL;
}

static void document_destroy(NativeWorkspace *doc)
{
    if (!doc) return;
    annotation_popup_close(doc);
    if (doc->pan_tick && doc->canvas)
        gtk_widget_remove_tick_callback(doc->canvas, doc->pan_tick);
    g_clear_object(&doc->import_cancellable);
    g_clear_object(&doc->import_process);
    history_clear(doc->undo, &doc->undo_count);
    history_clear(doc->redo, &doc->redo_count);
    discard_atlas(doc);
    discard_background(doc);
    discard_collision(doc);
    if (doc->annotations) g_array_free(doc->annotations, TRUE);
    free(doc->undo);
    free(doc->redo);
    free(doc->map);
    free(doc->stroke_before);
    g_free(doc->override_path);
    g_free(doc->annotations_path);
    g_free(doc->project_area);
    g_free(doc->identity);
    free(doc);
}

static void document_unref(NativeWorkspace *doc)
{
    if (!doc) return;
    g_return_if_fail(doc->references > 0);
    if (--doc->references == 0) document_destroy(doc);
}

static gboolean load_preview_surface(const char *filename, cairo_surface_t **target,
                                     unsigned char **target_pixels)
{
    GError *error = NULL;
    GdkPixbuf *pix = gdk_pixbuf_new_from_file(filename, &error);
    if (!pix) {
        if (error) g_error_free(error);
        return FALSE;
    }
    int w = gdk_pixbuf_get_width(pix), h = gdk_pixbuf_get_height(pix);
    int channels = gdk_pixbuf_get_n_channels(pix);
    if (w < 8 || h < 8 || w > 2048 || h > 2048 || channels < 3) {
        g_object_unref(pix);
        return FALSE;
    }
    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_RGB24, w);
    unsigned char *pixels = g_malloc0((size_t)stride * (size_t)h);
    const guchar *src = gdk_pixbuf_read_pixels(pix);
    int ps = gdk_pixbuf_get_rowstride(pix);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const guchar *q = src + (size_t)y * (size_t)ps + (size_t)x * (size_t)channels;
            uint32_t rgb = 0xff000000u | ((uint32_t)q[0] << 16) |
                           ((uint32_t)q[1] << 8) | q[2];
            memcpy(pixels + (size_t)y * (size_t)stride + (size_t)x * 4, &rgb, 4);
        }
    }
    g_object_unref(pix);
    cairo_surface_t *surface = cairo_image_surface_create_for_data(
        pixels, CAIRO_FORMAT_RGB24, w, h, stride);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        g_free(pixels);
        return FALSE;
    }
    *target = surface;
    *target_pixels = pixels;
    return TRUE;
}

static void load_background(NativeWorkspace *doc, const char *filename)
{
    discard_background(doc);
    load_preview_surface(filename, &doc->background, &doc->background_pixels);
}

static void load_collision(NativeWorkspace *doc, const char *filename)
{
    discard_collision(doc);
    load_preview_surface(filename, &doc->collision, &doc->collision_pixels);
}

static gboolean parse_unsigned_field(const char *text, guint *value)
{
    char extra;
    return text && sscanf(text, "%u%c", value, &extra) == 1;
}

static gboolean parse_signed_field(const char *text, int *value)
{
    char extra;
    return text && sscanf(text, "%d%c", value, &extra) == 1;
}

static void annotation_list_clear(NativeWorkspace *doc)
{
    if (!doc->annotations_list) return;
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(doc->annotations_list)))
        gtk_list_box_remove(GTK_LIST_BOX(doc->annotations_list), child);
}

static void load_annotations(NativeWorkspace *doc, const char *filename)
{
    gchar *contents = NULL;
    g_array_set_size(doc->annotations, 0);
    doc->annotation_selected = FALSE;
    annotation_list_clear(doc);
    if (!g_file_get_contents(filename, &contents, NULL, NULL)) {
        if (doc->annotations_status)
            gtk_label_set_text(GTK_LABEL(doc->annotations_status),
                               "No decoded native room objects for this room.");
        return;
    }
    gchar **lines = g_strsplit(contents, "\n", -1);
    guint counts[OVERLAY_COUNT] = {0};
    for (guint line = 0; lines[line] && line < 4096; ++line) {
        if (!lines[line][0] || lines[line][0] == '#') continue;
        gchar **fields = g_strsplit(lines[line], "|", 11);
        RoomAnnotation item = {0};
        guint source_index, width, height;
        int x, y;
        guint count = g_strv_length(fields);
        if (count != 10 || !parse_unsigned_field(fields[1], &source_index) ||
            !parse_signed_field(fields[2], &x) || !parse_signed_field(fields[3], &y) ||
            !parse_unsigned_field(fields[4], &width) ||
            !parse_unsigned_field(fields[5], &height) ||
            x < -64 || y < -64 || x > 32768 || y > 32768 ||
            !width || width > 4096 || !height || height > 4096 ||
            strlen(fields[6]) >= sizeof(item.variant) ||
            strlen(fields[7]) >= sizeof(item.native_type) ||
            strlen(fields[8]) >= sizeof(item.label) ||
            strlen(fields[9]) >= sizeof(item.details)) {
            g_strfreev(fields);
            continue;
        }
        if (!strcmp(fields[0], "ENEMY")) item.kind = OVERLAY_ENEMIES;
        else if (!strcmp(fields[0], "ITEM")) item.kind = OVERLAY_ITEMS;
        else if (!strcmp(fields[0], "OBJECT")) item.kind = OVERLAY_OBJECTS;
        else if (!strcmp(fields[0], "DOOR")) item.kind = OVERLAY_DOORS;
        else if (!strcmp(fields[0], "EVENT")) item.kind = OVERLAY_EVENTS;
        else if (!strcmp(fields[0], "TRIGGER")) item.kind = OVERLAY_TRIGGERS;
        else if (!strcmp(fields[0], "OTHER") || !strcmp(fields[0], "ENTITY"))
            item.kind = OVERLAY_OTHER; /* legacy files stay readable */
        else { g_strfreev(fields); continue; }
        item.index = source_index; item.x = x; item.y = y;
        item.width = width; item.height = height;
        g_strlcpy(item.variant, fields[6], sizeof(item.variant));
        g_strlcpy(item.native_type, fields[7], sizeof(item.native_type));
        g_strlcpy(item.label, fields[8], sizeof(item.label));
        g_strlcpy(item.details, fields[9], sizeof(item.details));
        g_array_append_val(doc->annotations, item);
        ++counts[item.kind];

        if (doc->annotations_list) {
            const char *kind = item.kind == OVERLAY_ENEMIES ? "ENEMY" :
                               item.kind == OVERLAY_ITEMS ? "ITEM" :
                               item.kind == OVERLAY_OBJECTS ? "OBJECT" :
                               item.kind == OVERLAY_DOORS ? "DOOR" :
                               item.kind == OVERLAY_EVENTS ? "EVENT" :
                               item.kind == OVERLAY_TRIGGERS ? "TRIGGER" : "OTHER";
            gchar *summary = g_strdup_printf(
                "%s %u — %s\n(%d,%d) %ux%u | %s\n%s",
                kind, item.index, item.label, item.x, item.y,
                item.width, item.height, item.variant, item.details);
            GtkWidget *label = gtk_label_new(summary);
            gtk_label_set_xalign(GTK_LABEL(label), 0);
            gtk_label_set_wrap(GTK_LABEL(label), TRUE);
            gtk_label_set_selectable(GTK_LABEL(label), TRUE);
            gtk_widget_set_margin_start(label, 8);
            gtk_widget_set_margin_end(label, 8);
            gtk_widget_set_margin_top(label, 6);
            gtk_widget_set_margin_bottom(label, 6);
            g_object_set_data(G_OBJECT(label), "mv-annotation-index",
                              GUINT_TO_POINTER(doc->annotations->len));
            GtkGesture *context = gtk_gesture_click_new();
            gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(context),
                                          GDK_BUTTON_SECONDARY);
            gtk_widget_add_controller(label, GTK_EVENT_CONTROLLER(context));
            g_signal_connect(context, "pressed",
                             G_CALLBACK(annotation_list_context_pressed), doc);
            gtk_list_box_append(GTK_LIST_BOX(doc->annotations_list), label);
            g_free(summary);
        }
        g_strfreev(fields);
    }
    if (doc->annotations_status) {
        gchar *summary = g_strdup_printf(
            "%u enemies, %u items, %u world objects, %u doors, "
            "%u events, %u triggers, %u other/unknown. Native source is read-only.",
            counts[OVERLAY_ENEMIES], counts[OVERLAY_ITEMS],
            counts[OVERLAY_OBJECTS], counts[OVERLAY_DOORS],
            counts[OVERLAY_EVENTS], counts[OVERLAY_TRIGGERS],
            counts[OVERLAY_OTHER]);
        gtk_label_set_text(GTK_LABEL(doc->annotations_status), summary);
        g_free(summary);
    }
    g_strfreev(lines);
    g_free(contents);
}

static const char *annotation_kind_name(guint kind)
{
    switch (kind) {
    case OVERLAY_ENEMIES: return "Enemy";
    case OVERLAY_ITEMS: return "Item / pickup";
    case OVERLAY_OBJECTS: return "World object";
    case OVERLAY_DOORS: return "Door / transition";
    case OVERLAY_OTHER: return "Other / unidentified";
    case OVERLAY_EVENTS: return "Event";
    case OVERLAY_TRIGGERS: return "Trigger";
    default: return "Native record";
    }
}

static RoomAnnotation *annotation_from_widget(NativeWorkspace *doc, GtkWidget *widget,
                                              guint *array_index)
{
    guint encoded = GPOINTER_TO_UINT(
        g_object_get_data(G_OBJECT(widget), "mv-annotation-index"));
    if (!encoded || !doc->annotations || encoded > doc->annotations->len) return NULL;
    if (array_index) *array_index = encoded - 1;
    return &g_array_index(doc->annotations, RoomAnnotation, encoded - 1);
}

/* PATCH_0079_PROJECT_ROOM_ENTITIES */
/* Project entities are versioned private authoring markers. All original ROM
 * annotations remain read-only. The Python validator owns disk persistence. */
static gboolean project_command(NativeWorkspace *doc, const char *action,
                                const char *const *options, gchar **output)
{
    if (!doc->ready || !doc->project_area || doc->closing) return FALSE;
    gchar room[16], width[16], height[16];
    snprintf(room, sizeof(room), "%u", doc->project_room);
    snprintf(width, sizeof(width), "%u", doc->map->width[0] * 16);
    snprintf(height, sizeof(height), "%u", doc->map->height[0] * 16);
    GPtrArray *args = g_ptr_array_new();
    const char *const base[] = {
        "python3", "-m", "scripts.project_room_entities",
        "--world", doc->project_aria ? "aria" : "mzm",
        "--area", doc->project_area, "--room", room,
        "--width", width, "--height", height, action, NULL
    };
    for (guint i = 0; base[i]; ++i) g_ptr_array_add(args, (gpointer)base[i]);
    if (options)
        for (guint i = 0; options[i]; ++i) g_ptr_array_add(args, (gpointer)options[i]);
    g_ptr_array_add(args, NULL);
    gchar *out = NULL, *err = NULL;
    GError *error = NULL;
    gint status = -1;
    gboolean launched = g_spawn_sync(NULL, (gchar **)args->pdata, NULL,
                                     G_SPAWN_SEARCH_PATH, NULL, NULL,
                                     &out, &err, &status, &error);
    gboolean success = launched && g_spawn_check_wait_status(status, NULL);
    if (!success) {
        gchar *diagnostic = g_strdup_printf("Project entities: %.480s",
            error ? error->message : err && *err ? err : "project validator rejected change");
        message(doc, diagnostic);
        g_free(diagnostic);
    }
    if (output) *output = success ? out : NULL;
    if (!success || !output) g_free(out);
    g_free(err);
    g_clear_error(&error);
    g_ptr_array_free(args, TRUE);
    return success;
}

static void project_load(NativeWorkspace *doc)
{
    gchar *output = NULL;
    if (!project_command(doc, "list", NULL, &output)) return;
    guint projects = 0;
    gchar **lines = g_strsplit(output ? output : "", "\n", -1);
    for (guint i = 0; lines[i] && i < 512; ++i) {
        if (!lines[i][0]) continue;
        gchar **fields = g_strsplit(lines[i], "\t", 7);
        guint id = 0;
        int x = 0, y = 0;
        RoomAnnotation item = {0};
        if (g_strv_length(fields) == 6 &&
            parse_unsigned_field(fields[0], &id) && id &&
            parse_signed_field(fields[2], &x) &&
            parse_signed_field(fields[3], &y) &&
            x >= 0 && y >= 0 &&
            strlen(fields[4]) < sizeof(item.label) &&
            strlen(fields[5]) < sizeof(item.native_type)) {
            item.kind = !strcmp(fields[1], "ENEMY") ? OVERLAY_ENEMIES :
                        !strcmp(fields[1], "ITEM") ? OVERLAY_ITEMS :
                        !strcmp(fields[1], "OBJECT") ? OVERLAY_OBJECTS : OVERLAY_COUNT;
            if (item.kind < OVERLAY_COUNT) {
                item.project_owned = TRUE;
                item.index = id;
                item.x = x;
                item.y = y;
                item.width = item.height = 16;
                g_strlcpy(item.variant, "project", sizeof(item.variant));
                g_strlcpy(item.native_type, fields[5], sizeof(item.native_type));
                g_strlcpy(item.label, fields[4], sizeof(item.label));
                g_strlcpy(item.details,
                    "Private project entity (not exportable to ROM). Use Select to drag.",
                    sizeof(item.details));
                g_array_append_val(doc->annotations, item);
                GtkWidget *row = gtk_label_new(NULL);
                gchar *summary = g_strdup_printf(
                    "[PROJECT] %s #%u — %s\n(%d,%d) | %s | not engine-ready",
                    fields[1], id, item.label, x, y, item.native_type);
                gtk_label_set_text(GTK_LABEL(row), summary);
                gtk_label_set_xalign(GTK_LABEL(row), 0);
                gtk_label_set_wrap(GTK_LABEL(row), TRUE);
                gtk_label_set_selectable(GTK_LABEL(row), TRUE);
                gtk_widget_set_margin_start(row, 8);
                gtk_widget_set_margin_bottom(row, 6);
                g_object_set_data(G_OBJECT(row), "mv-annotation-index",
                    GUINT_TO_POINTER(doc->annotations->len));
                GtkGesture *context = gtk_gesture_click_new();
                gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(context),
                                              GDK_BUTTON_SECONDARY);
                gtk_widget_add_controller(row, GTK_EVENT_CONTROLLER(context));
                g_signal_connect(context, "pressed",
                                 G_CALLBACK(annotation_list_context_pressed), doc);
                gtk_list_box_append(GTK_LIST_BOX(doc->annotations_list), row);
                if (doc->overlay_buttons[item.kind])
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
                        doc->overlay_buttons[item.kind]), TRUE);
                g_free(summary);
                ++projects;
            }
        }
        g_strfreev(fields);
    }
    if (doc->annotations_status && projects) {
        const char *previous = gtk_label_get_text(GTK_LABEL(doc->annotations_status));
        gchar *status = g_strdup_printf(
            "%s  + %u private project entity/ies (saved separately; not playable).",
            previous, projects);
        gtk_label_set_text(GTK_LABEL(doc->annotations_status), status);
        g_free(status);
    }
    g_strfreev(lines);
    g_free(output);
}

static void project_reload(NativeWorkspace *doc)
{
    if (!doc->annotations_path) return;
    load_annotations(doc, doc->annotations_path);
    project_load(doc);
    doc->annotation_selected = FALSE;
    if (doc->canvas) gtk_widget_queue_draw(doc->canvas);
}

static gboolean project_move(NativeWorkspace *doc, guint id, int x, int y)
{
    gchar sid[16], sx[16], sy[16];
    snprintf(sid, sizeof(sid), "%u", id);
    snprintf(sx, sizeof(sx), "%d", x);
    snprintf(sy, sizeof(sy), "%d", y);
    const char *const options[] = {"--id", sid, "--x", sx, "--y", sy, NULL};
    gboolean ok = project_command(doc, "move", options, NULL);
    project_reload(doc);
    if (ok) message(doc, "Project entity moved and saved. Native room data unchanged.");
    return ok;
}

/* PATCH_0080_NATIVE_CATALOG_BINDING: keep native records read-only. */
typedef struct {
    NativeWorkspace *doc;
    GtkWidget *window, *name, *native_type, *catalog_description;
    GtkWidget *item_id, *param0, *param1, *flags, *selected_icon;
    GPtrArray *catalog_names;
    GArray *catalog_item_ids; /* index -> exact Aria item ID; -1 = no item ID */
    GPtrArray *catalog_ids; /* selected dropdown index -> validated native token */
    int x, y;
    guint kind, editing_id;
} ProjectCreation;

static void project_creation_destroy(gpointer data)
{
    ProjectCreation *form = data;
    if (form->catalog_ids) g_ptr_array_free(form->catalog_ids, TRUE);
    if (form->catalog_item_ids) g_array_free(form->catalog_item_ids, TRUE);
    if (form->catalog_names) g_ptr_array_free(form->catalog_names, TRUE);
    document_unref(form->doc);
    g_free(form);
}

static gboolean project_close_idle(gpointer data)
{
    GtkWidget *window = GTK_WIDGET(data);
    gtk_window_destroy(GTK_WINDOW(window));
    return G_SOURCE_REMOVE;
}

static void project_creation_submit(GtkButton *button, gpointer userdata)
{
    ProjectCreation *form = userdata;
    NativeWorkspace *doc = form->doc;
    (void)button;
    if (doc->closing || !doc->ready) return;
    const char *label = gtk_editable_get_text(GTK_EDITABLE(form->name));
    guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(form->native_type));
    if (!form->catalog_ids || selected >= form->catalog_ids->len) return;
    const char *native = g_ptr_array_index(form->catalog_ids, selected);
    gboolean with_item = form->doc->project_aria && form->kind == OVERLAY_ITEMS &&
                         form->item_id && gtk_widget_get_sensitive(form->item_id);
    gchar siditem[12], spa[12], spb[12], sflags[12];
    snprintf(siditem, sizeof(siditem), "%d", with_item ?
             gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(form->item_id)) : 0);
    snprintf(spa, sizeof(spa), "%d", with_item ?
             gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(form->param0)) : 0);
    snprintf(spb, sizeof(spb), "%d", with_item ?
             gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(form->param1)) : 0);
    snprintf(sflags, sizeof(sflags), "%d", with_item ?
             gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(form->flags)) : 0);
    const char *const item_options[] = {"--item-id", siditem, "--parameter-0", spa,
                                        "--parameter-1", spb, "--flags", sflags};
    gboolean success = FALSE;
    if (form->editing_id) {
        gchar id[16];
        snprintf(id, sizeof(id), "%u", form->editing_id);
        const char *const options[] = {"--id", id, "--native-type", native,
                                        with_item ? item_options[0] : NULL,
                                        with_item ? item_options[1] : NULL,
                                        with_item ? item_options[2] : NULL,
                                        with_item ? item_options[3] : NULL,
                                        with_item ? item_options[4] : NULL,
                                        with_item ? item_options[5] : NULL,
                                        with_item ? item_options[6] : NULL,
                                        with_item ? item_options[7] : NULL, NULL};
        success = project_command(doc, "assign", options, NULL);
    } else {
        const char *kind = form->kind == OVERLAY_ENEMIES ? "ENEMY" :
                           form->kind == OVERLAY_ITEMS ? "ITEM" : "OBJECT";
        gchar sx[16], sy[16];
        snprintf(sx, sizeof(sx), "%d", form->x);
        snprintf(sy, sizeof(sy), "%d", form->y);
        const char *const options[] = {"--kind", kind, "--x", sx, "--y", sy,
                                       "--label", label, "--native-type", native,
                                       with_item ? item_options[0] : NULL,
                                       with_item ? item_options[1] : NULL,
                                       with_item ? item_options[2] : NULL,
                                       with_item ? item_options[3] : NULL,
                                       with_item ? item_options[4] : NULL,
                                       with_item ? item_options[5] : NULL,
                                       with_item ? item_options[6] : NULL,
                                       with_item ? item_options[7] : NULL, NULL};
        success = project_command(doc, "create", options, NULL);
    }
    if (!success) return;
    project_reload(doc);
    message(doc, "Project native reference saved (not yet exportable to ROM).");
    /* Destroy after GtkButton dispatch to preserve GTK active-state accounting. */
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, project_close_idle,
                    g_object_ref(form->window), g_object_unref);
}



/* PATCH_0081_ARIA_PICKUP_THUMBNAILS
 * GTK4 list-item factories: real PNG previews when source-verified local
 * decoded graphics are present, explicit missing-image glyph otherwise. */
static gchar *project_icon_path(ProjectCreation *form, const char *token)
{
    const char *world = form->doc->project_aria ? "aria" : "mzm";
    if (!token || !*token || strlen(token) > 64) return NULL;
    for (const char *p = token; *p; ++p)
        if (!g_ascii_isalnum(*p) && *p != ':' && *p != '_' && *p != '-') return NULL;
    return g_strdup_printf("assets/extracted/sprite_previews/%s/%s.png", world, token);
}

static void project_icon_assign(GtkWidget *image, ProjectCreation *form, const char *token)
{
    gchar *path = project_icon_path(form, token);
    if (path && g_file_test(path, G_FILE_TEST_IS_REGULAR))
        gtk_image_set_from_file(GTK_IMAGE(image), path);
    else gtk_image_set_from_icon_name(GTK_IMAGE(image), "applications-graphics-symbolic");
    g_free(path);
    gtk_image_set_pixel_size(GTK_IMAGE(image), 28);
}

/* PATCH_0082_ARIA_NAMED_ITEMS: bind exact item icons and text identities. */
static void project_row_icon_assign(GtkWidget *image, ProjectCreation *form,
                                    const char *token, gint item_id)
{
    if (form->doc->project_aria && form->kind == OVERLAY_ITEMS &&
        token && item_id >= 0 && item_id <= 255) {
        const char *subtype = strrchr(token, ':');
        if (subtype && strlen(subtype + 1) == 2) {
            char *end = NULL;
            guint number = (guint)g_ascii_strtoull(subtype + 1, &end, 16);
            if (end && !*end && number >= 2 && number <= 4) {
                gchar *path = g_strdup_printf(
                    "assets/extracted/sprite_previews/aria/items/%02X_%03d.png",
                    number, item_id);
                if (g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
                    gtk_image_set_from_file(GTK_IMAGE(image), path);
                    gtk_image_set_pixel_size(GTK_IMAGE(image), 28);
                    g_free(path);
                    return;
                }
                g_free(path);
            }
        }
    }
    project_icon_assign(image, form, token);
}

static void project_row_setup(GtkSignalListItemFactory *factory,
                              GtkListItem *item, gpointer userdata)
{
    (void)factory; (void)userdata;
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *preview = gtk_image_new_from_icon_name("applications-graphics-symbolic");
    GtkWidget *label = gtk_label_new("");
    gtk_image_set_pixel_size(GTK_IMAGE(preview), 28);
    gtk_widget_set_size_request(preview, 32, 32);
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_box_append(GTK_BOX(row), preview);
    gtk_box_append(GTK_BOX(row), label);
    gtk_list_item_set_child(item, row);
}

static void project_row_bind(GtkSignalListItemFactory *factory,
                             GtkListItem *item, gpointer userdata)
{
    (void)factory;
    ProjectCreation *form = userdata;
    GtkWidget *row = gtk_list_item_get_child(item);
    if (!row) return;
    GtkWidget *icon = gtk_widget_get_first_child(row);
    GtkWidget *label = icon ? gtk_widget_get_next_sibling(icon) : NULL;
    guint index = gtk_list_item_get_position(item);
    GtkStringObject *entry = GTK_STRING_OBJECT(gtk_list_item_get_item(item));
    if (GTK_IS_LABEL(label) && GTK_IS_STRING_OBJECT(entry))
        gtk_label_set_text(GTK_LABEL(label), gtk_string_object_get_string(entry));
    if (GTK_IS_IMAGE(icon) && form->catalog_ids && index < form->catalog_ids->len) {
        gint item_id = form->catalog_item_ids && index < form->catalog_item_ids->len ?
            g_array_index(form->catalog_item_ids, gint, index) : -1;
        project_row_icon_assign(icon, form,
            g_ptr_array_index(form->catalog_ids, index), item_id);
    }
}

static void project_item_fields_update(ProjectCreation *form, const char *token)
{
    if (!form->item_id) return;
    gboolean eligible = FALSE;
    guint max_id = 0;
    if (form->doc->project_aria && form->kind == OVERLAY_ITEMS && token) {
        const char *sub = strrchr(token, ':');
        if (sub && strlen(sub + 1) == 2 &&
            (g_str_has_prefix(token, "pickup:") ||
             g_str_has_prefix(token, "hard-mode-pickup:") ||
             g_str_has_prefix(token, "all-souls-reward:"))) {
            char *end = NULL;
            guint type = (guint)g_ascii_strtoull(sub + 1, &end, 16);
            static const guint limits[] = {0,255,31,58,44,55,24,35,5};
            if (end && !*end && type < G_N_ELEMENTS(limits)) {
                max_id = limits[type];
                eligible = TRUE;
            }
        }
    }
    gtk_widget_set_sensitive(form->item_id, eligible);
    gtk_widget_set_sensitive(form->param0, eligible);
    gtk_widget_set_sensitive(form->param1, eligible);
    gtk_widget_set_sensitive(form->flags, eligible);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(form->item_id), 0, max_id);
}


static void project_specific_icon_refresh(GtkSpinButton *spin, gpointer userdata)
{
    (void)spin;
    ProjectCreation *form = userdata;
    if (!form->selected_icon || !form->item_id || !form->catalog_ids) return;
    guint selection = gtk_drop_down_get_selected(GTK_DROP_DOWN(form->native_type));
    if (selection >= form->catalog_ids->len) return;
    const char *token = g_ptr_array_index(form->catalog_ids, selection);
    if (form->doc->project_aria && form->kind == OVERLAY_ITEMS &&
        gtk_widget_get_sensitive(form->item_id) && token) {
        const char *sub = strrchr(token, ':');
        if (sub && strlen(sub + 1) == 2) {
            char *end = NULL;
            guint category = (guint)g_ascii_strtoull(sub + 1, &end, 16);
            if (end && !*end && category >= 2 && category <= 4) {
                gchar *path = g_strdup_printf(
                    "assets/extracted/sprite_previews/aria/items/%02X_%03u.png",
                    category, (guint)gtk_spin_button_get_value_as_int(
                        GTK_SPIN_BUTTON(form->item_id)));
                if (g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
                    gtk_image_set_from_file(GTK_IMAGE(form->selected_icon), path);
                    g_free(path);
                    return;
                }
                g_free(path);
            }
        }
    }
    project_icon_assign(form->selected_icon, form, token);
}

static void project_catalog_changed(GObject *object, GParamSpec *pspec,
                                    gpointer userdata)
{
    ProjectCreation *form = userdata;
    (void)pspec;
    guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(object));
    if (!selected || !form->catalog_ids || selected >= form->catalog_ids->len) {
        gtk_label_set_text(GTK_LABEL(form->catalog_description),
            "Unassigned project marker. No native behavior linked.");
        if (form->selected_icon)
            project_icon_assign(form->selected_icon, form, "unassigned");
        project_item_fields_update(form, NULL);
        return;
    }
    const char *token = g_ptr_array_index(form->catalog_ids, selected);
    project_item_fields_update(form, token);
    if (form->item_id && form->catalog_item_ids && selected < form->catalog_item_ids->len) {
        gint item_id = g_array_index(form->catalog_item_ids, gint, selected);
        if (item_id >= 0 && gtk_widget_get_sensitive(form->item_id))
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(form->item_id), item_id);
    }
    if (form->selected_icon) {
        gint item_id = form->catalog_item_ids && selected < form->catalog_item_ids->len ?
            g_array_index(form->catalog_item_ids, gint, selected) : -1;
        project_row_icon_assign(form->selected_icon, form, token, item_id);
    }
    project_specific_icon_refresh(NULL, form);
    if (!form->editing_id && form->catalog_names && selected < form->catalog_names->len)
        gtk_editable_set_text(GTK_EDITABLE(form->name),
                              g_ptr_array_index(form->catalog_names, selected));
    gchar *message = g_strdup_printf(
        "Native identity: %s. Sprite/icon is source-derived only when a private PNG is available; otherwise missing-image is shown.", token);
    gtk_label_set_text(GTK_LABEL(form->catalog_description), message);
    g_free(message);
}

static void project_creation_open(NativeWorkspace *doc, int x, int y, guint kind,
                                  guint editing_id, const char *current_native)
{
    if (!doc->ready || doc->closing) return;
    ProjectCreation *form = g_new0(ProjectCreation, 1);
    form->doc = document_ref(doc);
    form->x = x;
    form->y = y;
    form->kind = kind;
    form->editing_id = editing_id;
    form->catalog_ids = g_ptr_array_new_with_free_func(g_free);
    form->catalog_names = g_ptr_array_new_with_free_func(g_free);
    form->catalog_item_ids = g_array_new(FALSE, FALSE, sizeof(gint));
    GtkWidget *window = gtk_window_new();
    form->window = window;
    GtkRoot *root = doc->page ? gtk_widget_get_root(doc->page) : NULL;
    if (GTK_IS_WINDOW(root)) gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(root));
    gtk_window_set_title(GTK_WINDOW(window), editing_id ?
        "Assign validated native type" : "Create private project entity");
    gtk_window_set_default_size(GTK_WINDOW(window), 510, 360);
    GtkWidget *layout = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *notice = gtk_label_new(
        "Choose a native definition from this game's verified catalog. "
        "This links metadata only; no executable enemy or item is exported to the ROM.");
    GtkWidget *position = gtk_label_new(NULL);
    gchar *pos = g_strdup_printf("Position: %d, %d (16px cell)", x, y);
    gtk_label_set_text(GTK_LABEL(position), pos);
    g_free(pos);
    gtk_label_set_wrap(GTK_LABEL(notice), TRUE);
    gtk_label_set_xalign(GTK_LABEL(notice), 0);
    gtk_label_set_xalign(GTK_LABEL(position), 0);
    form->name = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(form->name), "Project entity label");
    gtk_editable_set_text(GTK_EDITABLE(form->name),
        kind == OVERLAY_ENEMIES ? "New enemy" :
        kind == OVERLAY_ITEMS ? "New item" : "New object");
    gtk_widget_set_sensitive(form->name, editing_id == 0);
    const char *role = kind == OVERLAY_ENEMIES ? "ENEMY" :
                       kind == OVERLAY_ITEMS ? "ITEM" : "OBJECT";
    /* Existing project item needs both subtype and item ID to select one row. */
    guint initial_item_id = 0;
    if (editing_id && doc->project_aria && kind == OVERLAY_ITEMS) {
        gchar sid[16];
        snprintf(sid, sizeof(sid), "%u", editing_id);
        const char *const item_options[] = {"--id", sid, NULL};
        gchar *item_output = NULL;
        if (project_command(doc, "item-settings", item_options, &item_output) && item_output)
            (void)sscanf(item_output, "%u", &initial_item_id);
        g_free(item_output);
    }
    const char *const options[] = {"--kind", role, NULL};
    gchar *output = NULL;
    gboolean catalog_available = project_command(doc, "catalog", options, &output);
    GPtrArray *labels = g_ptr_array_new_with_free_func(g_free);
    g_ptr_array_add(labels, g_strdup("Unassigned (project marker only)"));
    g_ptr_array_add(form->catalog_ids, g_strdup("unassigned"));
    g_ptr_array_add(form->catalog_names, g_strdup(""));
    gint no_item_id = -1;
    g_array_append_val(form->catalog_item_ids, no_item_id);
    guint initial = 0;
    if (catalog_available && output) {
        gchar **lines = g_strsplit(output, "\n", -1);
        for (guint i = 0; lines[i] && i < 2048; ++i) {
            if (!lines[i][0] || form->catalog_ids->len >= 1600) continue;
            gchar **fields = g_strsplit(lines[i], "\t", 5);
            guint nfields = g_strv_length(fields);
            if ((nfields == 3 || nfields == 4) && strlen(fields[0]) <= 64 &&
                strlen(fields[1]) <= 80 && strlen(fields[2]) <= 160) {
                gchar *display = g_strdup_printf("%s — %s (%s)",
                                                 fields[1], fields[0], fields[2]);
                g_ptr_array_add(labels, display);
                g_ptr_array_add(form->catalog_ids, g_strdup(fields[0]));
                g_ptr_array_add(form->catalog_names, g_strdup(fields[1]));
                gint row_item_id = -1;
                if (nfields == 4 && fields[3][0]) {
                    char *end = NULL;
                    gint64 value = g_ascii_strtoll(fields[3], &end, 10);
                    if (end && !*end && value >= 0 && value <= 255)
                        row_item_id = (gint)value;
                }
                g_array_append_val(form->catalog_item_ids, row_item_id);
                if (current_native && !strcmp(current_native, fields[0]) &&
                    (!doc->project_aria || kind != OVERLAY_ITEMS ||
                     row_item_id < 0 || (guint)row_item_id == initial_item_id))
                    initial = form->catalog_ids->len - 1;
            }
            g_strfreev(fields);
        }
        g_strfreev(lines);
    }
    g_free(output);
    g_ptr_array_add(labels, NULL);
    form->native_type = gtk_drop_down_new_from_strings(
        (const char * const *)labels->pdata);
    GtkListItemFactory *popup_factory = gtk_signal_list_item_factory_new();
    GtkListItemFactory *button_factory = gtk_signal_list_item_factory_new();
    g_signal_connect(popup_factory, "setup", G_CALLBACK(project_row_setup), form);
    g_signal_connect(popup_factory, "bind", G_CALLBACK(project_row_bind), form);
    g_signal_connect(button_factory, "setup", G_CALLBACK(project_row_setup), form);
    g_signal_connect(button_factory, "bind", G_CALLBACK(project_row_bind), form);
    gtk_drop_down_set_list_factory(GTK_DROP_DOWN(form->native_type), popup_factory);
    gtk_drop_down_set_factory(GTK_DROP_DOWN(form->native_type), button_factory);
    g_object_unref(popup_factory);
    g_object_unref(button_factory);
    g_ptr_array_free(labels, TRUE);
    form->catalog_description = gtk_label_new(catalog_available ?
        "Select a named native item. Actual 16x16 icons appear when extracted from your verified ROM." :
        "Catalog unavailable: unassigned marker is still supported.");
    gtk_label_set_wrap(GTK_LABEL(form->catalog_description), TRUE);
    gtk_label_set_xalign(GTK_LABEL(form->catalog_description), 0);
    GtkWidget *save = gtk_button_new_with_label(editing_id ?
        "Assign and save native type" : "Create and save project entity");
    gtk_widget_set_margin_start(layout, 16);
    gtk_widget_set_margin_end(layout, 16);
    gtk_widget_set_margin_top(layout, 16);
    gtk_widget_set_margin_bottom(layout, 16);
    gtk_box_append(GTK_BOX(layout), notice);
    gtk_box_append(GTK_BOX(layout), position);
    if (!editing_id) {
        gtk_box_append(GTK_BOX(layout), gtk_label_new("Project label"));
        gtk_box_append(GTK_BOX(layout), form->name);
    }
    gtk_box_append(GTK_BOX(layout), gtk_label_new("Native definition (read-only catalog)"));
    gtk_box_append(GTK_BOX(layout), form->native_type);
    form->selected_icon = gtk_image_new_from_icon_name("applications-graphics-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(form->selected_icon), 32);
    gtk_box_append(GTK_BOX(layout), form->selected_icon);
    if (doc->project_aria && kind == OVERLAY_ITEMS) {
        GtkWidget *settings = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(settings), 5);
        gtk_grid_set_column_spacing(GTK_GRID(settings), 8);
        const char *const field_names[] = {
            "Specific item/soul ID", "Native parameter 0 (u16)",
            "Native parameter 1 (u16)", "Native flags (u8)"};
        GtkWidget **fields[] = {&form->item_id, &form->param0,
                                &form->param1, &form->flags};
        const int limits[] = {255, 65535, 65535, 255};
        for (guint i = 0; i < G_N_ELEMENTS(fields); ++i) {
            GtkWidget *label = gtk_label_new(field_names[i]);
            gtk_label_set_xalign(GTK_LABEL(label), 0);
            *fields[i] = gtk_spin_button_new_with_range(0, limits[i], 1);
            gtk_grid_attach(GTK_GRID(settings), label, 0, (int)i, 1, 1);
            gtk_grid_attach(GTK_GRID(settings), *fields[i], 1, (int)i, 1, 1);
        }
        GtkWidget *hint = gtk_label_new(
            "Aria families: Money, Consumable, Weapon, Armor/Accessory, "
            "Red/Blue/Yellow/Ability Soul; Normal, Hard Mode, All Souls. "
            "Parameters and flags are project metadata only (no ROM encoder).");
        gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
        gtk_label_set_xalign(GTK_LABEL(hint), 0);
        gtk_box_append(GTK_BOX(layout), hint);
        gtk_box_append(GTK_BOX(layout), settings);
        if (editing_id) {
            gchar id_text[12];
            snprintf(id_text, sizeof(id_text), "%u", editing_id);
            const char *const opts[] = {"--id", id_text, NULL};
            gchar *settings_out = NULL;
            if (project_command(doc, "item-settings", opts, &settings_out) &&
                settings_out) {
                guint id, a, b, flags;
                if (sscanf(settings_out, "%u	%u	%u	%u", &id, &a, &b, &flags) == 4) {
                    gtk_spin_button_set_value(GTK_SPIN_BUTTON(form->item_id), id);
                    gtk_spin_button_set_value(GTK_SPIN_BUTTON(form->param0), a);
                    gtk_spin_button_set_value(GTK_SPIN_BUTTON(form->param1), b);
                    gtk_spin_button_set_value(GTK_SPIN_BUTTON(form->flags), flags);
                }
                g_free(settings_out);
            }
        }
        g_signal_connect(form->item_id, "value-changed",
                         G_CALLBACK(project_specific_icon_refresh), form);
    }
    gtk_box_append(GTK_BOX(layout), form->catalog_description);
    gtk_box_append(GTK_BOX(layout), save);
    gtk_window_set_child(GTK_WINDOW(window), layout);
    g_object_set_data_full(G_OBJECT(window), "mv-project-create-form", form,
                           project_creation_destroy);
    g_signal_connect(form->native_type, "notify::selected",
                     G_CALLBACK(project_catalog_changed), form);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(form->native_type), initial);
    project_catalog_changed(G_OBJECT(form->native_type), NULL, form);
    g_signal_connect(save, "clicked", G_CALLBACK(project_creation_submit), form);
    gtk_window_present(GTK_WINDOW(window));
}

static gboolean project_popover_close_idle(gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    if (!doc->closing) annotation_popup_close(doc);
    return G_SOURCE_REMOVE;
}

static void project_popover_defer_close(NativeWorkspace *doc)
{
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, project_popover_close_idle,
                    document_ref(doc), (GDestroyNotify)document_unref);
}

static void project_create_clicked(GtkButton *button, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    int x = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "mv-project-x")) - 1;
    int y = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "mv-project-y")) - 1;
    guint kind = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(button), "mv-project-kind"));
    project_popover_defer_close(doc);
    project_creation_open(doc, x, y, kind, 0, NULL);
}

static void project_context_add(GtkWidget *layout, NativeWorkspace *doc,
                                const char *label, int x, int y, guint kind)
{
    GtkWidget *button = gtk_button_new_with_label(label);
    g_object_set_data(G_OBJECT(button), "mv-project-x", GINT_TO_POINTER(x + 1));
    g_object_set_data(G_OBJECT(button), "mv-project-y", GINT_TO_POINTER(y + 1));
    g_object_set_data(G_OBJECT(button), "mv-project-kind", GUINT_TO_POINTER(kind));
    g_signal_connect(button, "clicked", G_CALLBACK(project_create_clicked), doc);
    gtk_box_append(GTK_BOX(layout), button);
}

static void project_context_empty(NativeWorkspace *doc, GtkWidget *canvas,
                                  double x, double y)
{
    int cx = (int)(x / (16.0 * doc->scale));
    int cy = (int)(y / (16.0 * doc->scale));
    if (cx < 0 || cy < 0 || (guint)cx >= doc->map->width[0] ||
        (guint)cy >= doc->map->height[0]) return;
    int px = cx * 16, py = cy * 16;
    annotation_popup_close(doc);
    GtkWidget *popover = gtk_popover_new();
    GtkWidget *layout = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    project_context_add(layout, doc, "Create project enemy here...", px, py, OVERLAY_ENEMIES);
    project_context_add(layout, doc, "Create project item here...", px, py, OVERLAY_ITEMS);
    project_context_add(layout, doc, "Create project object here...", px, py, OVERLAY_OBJECTS);
    gtk_popover_set_child(GTK_POPOVER(popover), layout);
    gtk_widget_set_parent(popover, canvas);
    GdkRectangle pointer = {(int)x, (int)y, 1, 1};
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &pointer);
    doc->annotation_popup = popover;
    g_object_add_weak_pointer(G_OBJECT(popover), (gpointer *)&doc->annotation_popup);
    gtk_popover_popup(GTK_POPOVER(popover));
}

static void project_move_start_clicked(GtkButton *button, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    RoomAnnotation *item = annotation_from_widget(doc, GTK_WIDGET(button), NULL);
    if (item && item->project_owned) {
        doc->project_move_pending = TRUE;
        doc->project_move_id = item->index;
        message(doc, "Project entity: click the destination tile (Esc to cancel).");
    }
    project_popover_defer_close(doc);
}

static void project_assign_native_clicked(GtkButton *button, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    RoomAnnotation *item = annotation_from_widget(doc, GTK_WIDGET(button), NULL);
    if (!item || !item->project_owned || item->kind < OVERLAY_ENEMIES ||
        item->kind > OVERLAY_OBJECTS) return;
    /* Read identity before returning to the main loop: popup closes after click. */
    project_creation_open(doc, item->x, item->y, item->kind,
                          item->index, item->native_type);
    project_popover_defer_close(doc);
}

typedef struct {
    NativeWorkspace *doc;
    guint id;
} ProjectDelete;

static gboolean project_delete_idle(gpointer userdata)
{
    ProjectDelete *request = userdata;
    NativeWorkspace *doc = request->doc;
    if (!doc->closing && doc->ready) {
        gchar id[16];
        snprintf(id, sizeof(id), "%u", request->id);
        const char *const options[] = {"--id", id, NULL};
        annotation_popup_close(doc);
        if (project_command(doc, "delete", options, NULL)) {
            project_reload(doc);
            message(doc, "Project entity deleted; original ROM annotations unchanged.");
        }
    }
    document_unref(doc);
    g_free(request);
    return G_SOURCE_REMOVE;
}

static void project_delete_clicked(GtkButton *button, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    RoomAnnotation *item = annotation_from_widget(doc, GTK_WIDGET(button), NULL);
    if (!item || !item->project_owned) return;
    ProjectDelete *request = g_new0(ProjectDelete, 1);
    request->doc = document_ref(doc);
    request->id = item->index;
    /* Do not destroy the right-click GtkPopover during GtkButton::clicked. */
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, project_delete_idle, request, NULL);
}

static gint project_hit(NativeWorkspace *doc, double x, double y)
{
    if (!doc->annotations) return -1;
    for (guint cursor = doc->annotations->len; cursor > 0; --cursor) {
        guint index = cursor - 1;
        const RoomAnnotation *item = &g_array_index(doc->annotations, RoomAnnotation, index);
        if (!item->project_owned || !doc->overlays[item->kind]) continue;
        double px = item->x * doc->scale, py = item->y * doc->scale;
        if (x >= px && x < px + 16 * doc->scale &&
            y >= py && y < py + 16 * doc->scale) return (gint)index;
    }
    return -1;
}

static void annotation_popup_close(NativeWorkspace *doc)
{
    if (!doc || !doc->annotation_popup) return;
    GtkWidget *popup = doc->annotation_popup;
    g_object_remove_weak_pointer(G_OBJECT(popup),
                                 (gpointer *)&doc->annotation_popup);
    doc->annotation_popup = NULL;
    gtk_popover_popdown(GTK_POPOVER(popup));
    if (gtk_widget_get_parent(popup)) gtk_widget_unparent(popup);
}

static void annotation_window_add_field(GtkGrid *grid, gint row,
                                        const char *name, const char *value)
{
    GtkWidget *name_label = gtk_label_new(name);
    GtkWidget *entry = gtk_entry_new();
    gtk_label_set_xalign(GTK_LABEL(name_label), 1);
    gtk_editable_set_text(GTK_EDITABLE(entry), value ? value : "");
    gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
    gtk_widget_set_hexpand(entry, TRUE);
    gtk_grid_attach(grid, name_label, 0, row, 1, 1);
    gtk_grid_attach(grid, entry, 1, row, 1, 1);
}

static void annotation_window_open(NativeWorkspace *doc,
                                   const RoomAnnotation *item, gboolean editor)
{
    if (!doc || doc->closing || !item) return;
    const char *kind = annotation_kind_name(item->kind);
    gchar *title = g_strdup_printf("%s %s — %s",
                                   kind, editor ? "editor" : "information", item->label);
    GtkWidget *window = gtk_window_new();
    GtkRoot *root = doc->page ? gtk_widget_get_root(doc->page) : NULL;
    if (GTK_IS_WINDOW(root))
        gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(root));
    gtk_window_set_title(GTK_WINDOW(window), title);
    gtk_window_set_default_size(GTK_WINDOW(window), 560, 440);

    GtkWidget *layout = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *heading = gtk_label_new(title);
    GtkWidget *notice = gtk_label_new(editor
        ? "Native source record. Fields are read-only until this type has a validated project encoder."
        : "Decoded native source information. No ROM or project data is modified.");
    GtkWidget *grid = gtk_grid_new();
    GtkWidget *details_heading = gtk_label_new("Decoded details");
    GtkWidget *details = gtk_label_new(item->details);
    GtkWidget *actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *close = gtk_button_new_with_label("Close");
    gtk_widget_add_css_class(heading, "title-2");
    gtk_label_set_xalign(GTK_LABEL(heading), 0);
    gtk_label_set_xalign(GTK_LABEL(notice), 0);
    gtk_label_set_wrap(GTK_LABEL(notice), TRUE);
    gtk_label_set_xalign(GTK_LABEL(details_heading), 0);
    gtk_widget_add_css_class(details_heading, "heading");
    gtk_label_set_xalign(GTK_LABEL(details), 0);
    gtk_label_set_wrap(GTK_LABEL(details), TRUE);
    gtk_label_set_selectable(GTK_LABEL(details), TRUE);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);

    gchar *source_index = g_strdup_printf("%u", item->index);
    gchar *geometry = g_strdup_printf("x=%d, y=%d, width=%u, height=%u",
                                      item->x, item->y, item->width, item->height);
    annotation_window_add_field(GTK_GRID(grid), 0, "Kind", kind);
    annotation_window_add_field(GTK_GRID(grid), 1, "Label", item->label);
    annotation_window_add_field(GTK_GRID(grid), 2, "Native identity", item->native_type);
    annotation_window_add_field(GTK_GRID(grid), 3, "Source index", source_index);
    annotation_window_add_field(GTK_GRID(grid), 4, "Variant", item->variant);
    annotation_window_add_field(GTK_GRID(grid), 5, "Geometry", geometry);
    g_free(source_index);
    g_free(geometry);

    if (editor) {
        GtkWidget *apply = gtk_button_new_with_label("Apply project override");
        gtk_widget_set_sensitive(apply, FALSE);
        gtk_widget_set_tooltip_text(apply,
            "Unavailable until a validated project schema and engine encoder exist.");
        gtk_box_append(GTK_BOX(actions), apply);
    }
    gtk_widget_set_hexpand(actions, TRUE);
    gtk_widget_set_halign(close, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(actions), close);
    gtk_widget_set_margin_start(layout, 18);
    gtk_widget_set_margin_end(layout, 18);
    gtk_widget_set_margin_top(layout, 18);
    gtk_widget_set_margin_bottom(layout, 18);
    gtk_box_append(GTK_BOX(layout), heading);
    gtk_box_append(GTK_BOX(layout), notice);
    gtk_box_append(GTK_BOX(layout), grid);
    gtk_box_append(GTK_BOX(layout), details_heading);
    gtk_box_append(GTK_BOX(layout), details);
    gtk_box_append(GTK_BOX(layout), actions);
    gtk_window_set_child(GTK_WINDOW(window), layout);
    g_signal_connect_swapped(close, "clicked", G_CALLBACK(gtk_window_destroy), window);
    g_object_set_data_full(G_OBJECT(window), "native-annotation-document",
                           document_ref(doc), (GDestroyNotify)document_unref);
    gtk_window_present(GTK_WINDOW(window));
    g_free(title);
}

static void annotation_inspect_clicked(GtkButton *button, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    RoomAnnotation *item = annotation_from_widget(doc, GTK_WIDGET(button), NULL);
    if (item) annotation_window_open(doc, item, FALSE);
    annotation_popup_close(doc);
}

static void annotation_editor_clicked(GtkButton *button, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    RoomAnnotation *item = annotation_from_widget(doc, GTK_WIDGET(button), NULL);
    if (item) annotation_window_open(doc, item, TRUE);
    annotation_popup_close(doc);
}

static void annotation_locate_clicked(GtkButton *button, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    guint index = 0;
    RoomAnnotation *item = annotation_from_widget(doc, GTK_WIDGET(button), &index);
    if (!item || !doc->annotations_list) return;
    GtkWidget *row = gtk_widget_get_first_child(doc->annotations_list);
    for (guint i = 0; row && i < index; ++i)
        row = gtk_widget_get_next_sibling(row);
    if (GTK_IS_LIST_BOX_ROW(row))
        gtk_list_box_select_row(GTK_LIST_BOX(doc->annotations_list),
                                GTK_LIST_BOX_ROW(row));
    if (GTK_IS_NOTEBOOK(doc->palette_page))
        gtk_notebook_set_current_page(GTK_NOTEBOOK(doc->palette_page), 1);
    focus_page(doc->palette_page);
    gchar *status = g_strdup_printf("Selected %s %u — %s in Room data.",
                                    annotation_kind_name(item->kind),
                                    item->index, item->label);
    message(doc, status);
    g_free(status);
    annotation_popup_close(doc);
}

static GtkWidget *annotation_menu_button(NativeWorkspace *doc, const char *label,
                                         guint array_index, GCallback callback)
{
    GtkWidget *button = gtk_button_new_with_label(label);
    g_object_set_data(G_OBJECT(button), "mv-annotation-index",
                      GUINT_TO_POINTER(array_index + 1));
    g_signal_connect(button, "clicked", callback, doc);
    return button;
}

static void annotation_context_show(NativeWorkspace *doc, GtkWidget *relative,
                                    guint array_index, double x, double y)
{
    if (doc->closing || !doc->annotations || array_index >= doc->annotations->len) return;
    annotation_popup_close(doc);
    const RoomAnnotation *item = &g_array_index(
        doc->annotations, RoomAnnotation, array_index);
    doc->annotation_selected = TRUE;
    doc->selected_annotation = array_index;
    if (doc->canvas) gtk_widget_queue_draw(doc->canvas);

    GtkWidget *popover = gtk_popover_new();
    GtkWidget *layout = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gchar *heading_text = g_strdup_printf("%s %u — %s",
        annotation_kind_name(item->kind), item->index, item->label);
    gchar *summary_text = g_strdup_printf(
        "Native: %s\nVariant: %s\nPosition: (%d,%d), size %ux%u\n%s",
        item->native_type, item->variant, item->x, item->y,
        item->width, item->height, item->details);
    GtkWidget *heading = gtk_label_new(heading_text);
    GtkWidget *summary = gtk_label_new(summary_text);
    gtk_widget_add_css_class(heading, "heading");
    gtk_label_set_xalign(GTK_LABEL(heading), 0);
    gtk_label_set_xalign(GTK_LABEL(summary), 0);
    gtk_label_set_wrap(GTK_LABEL(summary), TRUE);
    gtk_widget_set_size_request(summary, 360, -1);
    gtk_box_append(GTK_BOX(layout), heading);
    gtk_box_append(GTK_BOX(layout), summary);
    gtk_box_append(GTK_BOX(layout), annotation_menu_button(
        doc, "Inspect full record", array_index, G_CALLBACK(annotation_inspect_clicked)));
    gchar *editor_label = g_strdup_printf("Open %s editor…",
                                           annotation_kind_name(item->kind));
    gtk_box_append(GTK_BOX(layout), annotation_menu_button(
        doc, editor_label, array_index, G_CALLBACK(annotation_editor_clicked)));
    gtk_box_append(GTK_BOX(layout), annotation_menu_button(
        doc, "Locate in Room data", array_index, G_CALLBACK(annotation_locate_clicked)));
    if (item->project_owned) {
        gtk_box_append(GTK_BOX(layout), annotation_menu_button(
            doc, "Choose native enemy / item / object type…", array_index,
            G_CALLBACK(project_assign_native_clicked)));
        gtk_box_append(GTK_BOX(layout), annotation_menu_button(
            doc, "Move project entity (click destination)", array_index,
            G_CALLBACK(project_move_start_clicked)));
        gtk_box_append(GTK_BOX(layout), annotation_menu_button(
            doc, "Delete project entity", array_index,
            G_CALLBACK(project_delete_clicked)));
    }
    g_free(editor_label);
    g_free(heading_text);
    g_free(summary_text);
    gtk_widget_set_margin_start(layout, 10);
    gtk_widget_set_margin_end(layout, 10);
    gtk_widget_set_margin_top(layout, 10);
    gtk_widget_set_margin_bottom(layout, 10);
    gtk_popover_set_child(GTK_POPOVER(popover), layout);
    gtk_widget_set_parent(popover, relative);
    GdkRectangle point = {(int)x, (int)y, 1, 1};
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &point);
    doc->annotation_popup = popover;
    g_object_add_weak_pointer(G_OBJECT(popover),
                              (gpointer *)&doc->annotation_popup);
    gtk_popover_popup(GTK_POPOVER(popover));
}

static void annotation_list_context_pressed(GtkGestureClick *gesture, gint presses,
                                            double x, double y, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    GtkWidget *widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
    guint encoded = GPOINTER_TO_UINT(
        g_object_get_data(G_OBJECT(widget), "mv-annotation-index"));
    (void)presses;
    if (encoded) annotation_context_show(doc, widget, encoded - 1, x, y);
}

static void annotation_canvas_context_pressed(GtkGestureClick *gesture, gint presses,
                                              double x, double y, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    (void)presses;
    if (doc->closing || !doc->ready || !doc->annotations) return;
    for (guint cursor = doc->annotations->len; cursor > 0; --cursor) {
        guint index = cursor - 1;
        const RoomAnnotation *item = &g_array_index(
            doc->annotations, RoomAnnotation, index);
        if (item->kind >= OVERLAY_COUNT || !doc->overlays[item->kind]) continue;
        double left = item->x * doc->scale;
        double top = item->y * doc->scale;
        double width = MAX(4.0, item->width * doc->scale);
        double height = MAX(4.0, item->height * doc->scale);
        if (x < left || y < top || x > left + width || y > top + height) continue;
        GtkWidget *canvas = gtk_event_controller_get_widget(
            GTK_EVENT_CONTROLLER(gesture));
        annotation_context_show(doc, canvas, index, x, y);
        gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
        return;
    }
    /* On empty canvas space, create a real saved project entity. */
    project_context_empty(doc, gtk_event_controller_get_widget(
        GTK_EVENT_CONTROLLER(gesture)), x, y);
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
            uint32_t alpha = channels == 4 ? color[3] : 255u;
            /* Cairo ARGB32 requires premultiplied components. */
            uint32_t red = ((uint32_t)color[0] * alpha + 127u) / 255u;
            uint32_t green = ((uint32_t)color[1] * alpha + 127u) / 255u;
            uint32_t blue = ((uint32_t)color[2] * alpha + 127u) / 255u;
            uint32_t rgba = (alpha << 24) | (red << 16) | (green << 8) | blue;
            memcpy(pixels + (size_t)y * (size_t)cairo_stride + (size_t)x * 4, &rgba, 4);
        }
    }
    g_object_unref(pixbuf);
    cairo_surface_t *surface = cairo_image_surface_create_for_data(
        pixels, channels == 4 ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24,
        width, height, cairo_stride);
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

static void draw_annotations(NativeWorkspace *doc, cairo_t *cr)
{
    if (!doc->annotations) return;
    for (guint i = 0; i < doc->annotations->len; ++i) {
        const RoomAnnotation *item = &g_array_index(doc->annotations, RoomAnnotation, i);
        if (item->kind >= OVERLAY_COUNT || !doc->overlays[item->kind]) continue;
        double x = item->x * doc->scale;
        double y = item->y * doc->scale;
        double width = item->width * doc->scale;
        double height = item->height * doc->scale;
        double red = 0.2, green = 0.9, blue = 0.45;
        if (item->kind == OVERLAY_ENEMIES) { red = 1.0; green = 0.3; blue = 0.3; }
        else if (item->kind == OVERLAY_ITEMS) { red = 1.0; green = 0.82; blue = 0.18; }
        else if (item->kind == OVERLAY_OTHER) { red = 0.65; green = 0.65; blue = 0.65; }
        else if (item->kind == OVERLAY_DOORS) { red = 0.72; green = 0.35; blue = 1.0; }
        else if (item->kind == OVERLAY_EVENTS) { red = 1.0; green = 0.65; blue = 0.12; }
        else if (item->kind == OVERLAY_TRIGGERS) { red = 0.2; green = 0.8; blue = 1.0; }
        cairo_save(cr);
        cairo_rectangle(cr, x + 1, y + 1, MAX(4.0, width - 2), MAX(4.0, height - 2));
        cairo_set_source_rgba(cr, red, green, blue, 0.28);
        cairo_fill_preserve(cr);
        cairo_set_source_rgba(cr, red, green, blue, 0.98);
        cairo_set_line_width(cr, 2.0);
        if (item->kind == OVERLAY_EVENTS || item->kind == OVERLAY_TRIGGERS) {
            const double dash[] = {6.0, 4.0};
            cairo_set_dash(cr, dash, 2, 0);
        }
        cairo_stroke(cr);
        if (item->project_owned) {
            const double dash[] = {3.0, 2.0};
            cairo_set_dash(cr, dash, 2, 0);
            cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
            cairo_rectangle(cr, x + 2, y + 2, MAX(2.0, width - 4), MAX(2.0, height - 4));
            cairo_stroke(cr);
            cairo_set_dash(cr, NULL, 0, 0);
        }
        if (doc->annotation_selected && doc->selected_annotation == i) {
            cairo_rectangle(cr, x - 1, y - 1, MAX(6.0, width + 2),
                            MAX(6.0, height + 2));
            cairo_set_source_rgb(cr, 1.0, 0.84, 0.22);
            cairo_set_line_width(cr, 3.0);
            cairo_set_dash(cr, NULL, 0, 0);
            cairo_stroke(cr);
        }
        char id[24];
        snprintf(id, sizeof(id), "%s%u",
                 item->kind == OVERLAY_ENEMIES ? "N" :
                 item->kind == OVERLAY_ITEMS ? "I" :
                 item->kind == OVERLAY_OBJECTS ? "O" :
                 item->kind == OVERLAY_DOORS ? "D" :
                 item->kind == OVERLAY_EVENTS ? "E" :
                 item->kind == OVERLAY_TRIGGERS ? "T" : "?", item->index);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 10.0);
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_move_to(cr, x + 3, y + 12);
        cairo_show_text(cr, id);
        cairo_restore(cr);
    }
}

static void draw_room(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    if (doc->closing) return;
    (void)area; (void)width; (void)height;
    cairo_set_source_rgb(cr, 0.11, 0.13, 0.16);
    cairo_paint(cr);
    if (!doc->ready) return;
    unsigned layer = doc->layer_id;
    unsigned columns = doc->map->width[layer];
    unsigned rows = doc->map->height[layer];
    double cell = 16.0 * doc->scale;
    if (doc->background && doc->background_visible) {
        cairo_save(cr);
        cairo_scale(cr, doc->scale, doc->scale);
        cairo_set_source_surface(cr, doc->background, 0, 0);
        cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
        cairo_rectangle(cr, 0, 0, columns * 16, rows * 16);
        cairo_fill(cr);
        cairo_restore(cr);
    }
    /* When editing BG1, display the actual decoded BG2 blocks behind it. */
    if (doc->background_visible && layer == 0) {
        unsigned bw = doc->map->width[1], bh = doc->map->height[1];
        for (unsigned y = 0; y < bh; ++y) {
            for (unsigned x = 0; x < bw; ++x) {
                unsigned id = doc->map->blocks[1][y * bw + x];
                if (id && id < doc->map->tile_count)
                    stamp(doc, cr, id, x * cell, y * cell, doc->scale);
            }
        }
    }
    for (unsigned y = 0; y < rows; ++y) {
        for (unsigned x = 0; x < columns; ++x) {
            unsigned id = doc->map->blocks[layer][y * columns + x];
            /* Transparent/empty metatile zero leaves BG2 or BG3 visible. */
            if (id != 0 || !doc->background_visible)
                stamp(doc, cr, id, x * cell, y * cell, doc->scale);
            if (id >= doc->map->tile_count) {
                cairo_set_source_rgba(cr, 1, 0.1, 0.25, 0.45);
                cairo_rectangle(cr, x * cell, y * cell, cell, cell);
                cairo_fill(cr);
            }
        }
    }
    if (doc->collision && doc->overlays[OVERLAY_COLLISION]) {
        cairo_save(cr);
        cairo_scale(cr, doc->scale, doc->scale);
        cairo_rectangle(cr, 0, 0, columns * 16, rows * 16);
        cairo_clip(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_SCREEN);
        cairo_set_source_surface(cr, doc->collision, 0, 0);
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
        cairo_paint_with_alpha(cr, 0.58);
        cairo_restore(cr);
    }
    draw_annotations(doc, cr);
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
    /* Display the selected, original 16x16 tile under the pointer without
     * changing the map until the user presses the primary mouse button. */
    if (doc->pointer_over_canvas && doc->tool_id == TOOL_PENCIL &&
        doc->brush_id < doc->map->tile_count) {
        double x = doc->hover_col * cell;
        double y = doc->hover_row * cell;
        cairo_save(cr);
        cairo_push_group(cr);
        stamp(doc, cr, doc->brush_id, x, y, doc->scale);
        cairo_pop_group_to_source(cr);
        cairo_paint_with_alpha(cr, 0.68);
        cairo_set_source_rgba(cr, 1.0, 0.85, 0.3, 0.95);
        cairo_set_line_width(cr, 1.5);
        cairo_rectangle(cr, x + 0.75, y + 0.75, cell - 1.5, cell - 1.5);
        cairo_stroke(cr);
        cairo_restore(cr);
    }
}

static void draw_palette(GtkDrawingArea *area, cairo_t *cr, int width, int height,
                         gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    if (doc->closing) return;
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
    if (!doc->closing && doc->tab_title) {
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
    message(doc, "Unsaved changes - press Ctrl+S or use the Save button");
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
    if (doc->closing) return;
    doc->brush_id = (unsigned)gtk_spin_button_get_value_as_int(spin);
    if (doc->palette) gtk_widget_queue_draw(doc->palette);
    if (doc->canvas) gtk_widget_queue_draw(doc->canvas);
    if (doc->ready && !doc->drawing && doc->tools[TOOL_PENCIL] &&
        doc->tool_id != TOOL_PENCIL)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(doc->tools[TOOL_PENCIL]), TRUE);
}

static void layer_changed(GObject *object, GParamSpec *pspec, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    if (doc->closing) return;
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
    if (doc->closing) return;
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

static gboolean pan_frame(GtkWidget *widget, GdkFrameClock *clock, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    (void)widget; (void)clock;
    if (!doc->closing && doc->scroller) {
        GtkAdjustment *h = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(doc->scroller));
        GtkAdjustment *v = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(doc->scroller));
        gtk_adjustment_set_value(h, doc->pending_pan_horizontal);
        gtk_adjustment_set_value(v, doc->pending_pan_vertical);
    }
    doc->pan_tick = 0;
    return G_SOURCE_REMOVE;
}

static void paint_cell(NativeWorkspace *doc, int x, int y)
{
    unsigned picked = doc->brush_id;
    if (native_map_edit(doc->map, doc->layer_id, x, y,
                        doc->tool_id, doc->brush_id, &picked))
        mark_changed(doc);
    if (doc->tool_id == TOOL_PICK && picked < doc->map->tile_count) {
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(doc->brush), picked);
        /* A picker click first finishes its gesture; then it becomes a stamp. */
        doc->pick_to_pencil = TRUE;
    }
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
    if (doc->closing) return;
    (void)gesture;
    if (!doc->ready || doc->drawing || doc->project_click_consumed) return;
    /* Select-and-drag works only for project-owned markers. */
    if (doc->tool_id == TOOL_SELECT) {
        gint marker = project_hit(doc, x, y);
        if (marker >= 0) {
            RoomAnnotation *item = &g_array_index(doc->annotations, RoomAnnotation, (guint)marker);
            doc->dragging_project = TRUE;
            doc->drawing = TRUE;
            doc->project_drag_id = item->index;
            doc->project_drag_array_index = (guint)marker;
            doc->project_drag_origin_x = item->x;
            doc->project_drag_origin_y = item->y;
            doc->annotation_selected = TRUE;
            doc->selected_annotation = (guint)marker;
            gtk_widget_queue_draw(doc->canvas);
            return;
        }
    }
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
    if (doc->closing) return;
    (void)gesture;
    if (!doc->ready || !doc->drawing) return;
    if (doc->dragging_project) {
        RoomAnnotation *item = &g_array_index(doc->annotations, RoomAnnotation,
                                               doc->project_drag_array_index);
        int x = (int)((doc->project_drag_origin_x + dx / doc->scale) / 16.0) * 16;
        int y = (int)((doc->project_drag_origin_y + dy / doc->scale) / 16.0) * 16;
        item->x = CLAMP(x, 0, (int)doc->map->width[0] * 16 - 16);
        item->y = CLAMP(y, 0, (int)doc->map->height[0] * 16 - 16);
        gtk_widget_queue_draw(doc->canvas);
        return;
    }
    if (doc->panning) {
        doc->pending_pan_horizontal = doc->pan_horizontal - dx;
        doc->pending_pan_vertical = doc->pan_vertical - dy;
        if (!doc->pan_tick)
            doc->pan_tick = gtk_widget_add_tick_callback(doc->canvas, pan_frame, doc, NULL);
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
    if (doc->closing) return;
    (void)gesture; (void)dx; (void)dy;
    if (!doc->drawing) return;
    if (doc->dragging_project) {
        RoomAnnotation *item = &g_array_index(doc->annotations, RoomAnnotation,
                                               doc->project_drag_array_index);
        int x = item->x, y = item->y;
        guint id = doc->project_drag_id;
        doc->dragging_project = FALSE;
        doc->drawing = FALSE;
        if (x != doc->project_drag_origin_x || y != doc->project_drag_origin_y)
            project_move(doc, id, x, y);
        else gtk_widget_queue_draw(doc->canvas);
        return;
    }
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
    if (doc->pick_to_pencil) {
        doc->pick_to_pencil = FALSE;
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(doc->tools[TOOL_PENCIL]), TRUE);
    }
    gtk_widget_queue_draw(doc->canvas);
}

/* GestureClick covers single-click paint / fill; GestureDrag covers continuous
 * strokes and selection. Group them so the drag is not cancelled on motion. */
static void canvas_click_down(GtkGestureClick *click, int n_press,
                              double x, double y, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    (void)n_press;
    if (doc->project_move_pending && doc->ready) {
        int cx = (int)(x / (16.0 * doc->scale));
        int cy = (int)(y / (16.0 * doc->scale));
        if (cx < 0 || cy < 0 || (guint)cx >= doc->map->width[0] ||
            (guint)cy >= doc->map->height[0]) return;
        guint id = doc->project_move_id;
        doc->project_move_pending = FALSE;
        doc->project_click_consumed = TRUE;
        project_move(doc, id, cx * 16, cy * 16);
        gtk_gesture_set_state(GTK_GESTURE(click), GTK_EVENT_SEQUENCE_CLAIMED);
        return;
    }
    gesture_begin(NULL, x, y, userdata);
}

static void canvas_click_up(GtkGestureClick *click, int n_press,
                            double x, double y, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    (void)click; (void)n_press; (void)x; (void)y;
    if (doc->project_click_consumed) {
        doc->project_click_consumed = FALSE;
        return;
    }
    gesture_end(NULL, 0, 0, userdata);
}

static void palette_click(GtkGestureClick *gesture, int n_press, double x, double y,
                          gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    if (doc->closing) return;
    (void)gesture; (void)n_press;
    if (!doc->ready || x < 0 || y < 0) return;
    unsigned column = (unsigned)(x / 24), row = (unsigned)(y / 24);
    unsigned id = row * 16 + column;
    if (column < 16 && id < doc->map->tile_count) {
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(doc->brush), id);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(doc->tools[TOOL_PENCIL]), TRUE);
    }
}

static void canvas_hover(GtkEventControllerMotion *controller, double x, double y,
                         gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    if (doc->closing) return;
    int col = 0, row = 0;
    (void)controller;
    gboolean valid = get_cell(doc, x, y, &col, &row);
    if (valid != doc->pointer_over_canvas ||
        (valid && (doc->hover_col != col || doc->hover_row != row))) {
        doc->pointer_over_canvas = valid;
        if (valid) { doc->hover_col = col; doc->hover_row = row; }
        gtk_widget_queue_draw(doc->canvas);
    }
}

static void canvas_leave(GtkEventControllerMotion *controller, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    if (doc->closing) return;
    (void)controller;
    if (doc->pointer_over_canvas) {
        doc->pointer_over_canvas = FALSE;
        gtk_widget_queue_draw(doc->canvas);
    }
}

static gboolean canvas_scroll(GtkEventControllerScroll *controller, double dx,
                              double dy, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    GdkModifierType state = gtk_event_controller_get_current_event_state(
        GTK_EVENT_CONTROLLER(controller));
    (void)dx;
    if (doc->closing || !(state & GDK_CONTROL_MASK) || dy == 0) return FALSE;
    double value = gtk_spin_button_get_value(GTK_SPIN_BUTTON(doc->zoom));
    GtkAdjustment *adjustment = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(doc->zoom));
    double step = gtk_adjustment_get_step_increment(adjustment);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(doc->zoom), value + (dy < 0 ? step : -step));
    return TRUE;
}

/* GTK owns the tooltip lifecycle; no manual GtkPopover/timer callbacks.
 * Preserve the requested delay when this GTK build exposes the setting. */
static void delayed_tip(GtkWidget *widget, const char *description)
{
    gtk_widget_set_tooltip_text(widget, description);
    GtkSettings *settings = gtk_settings_get_default();
    if (settings && g_object_class_find_property(G_OBJECT_GET_CLASS(settings),
                                                   "gtk-tooltip-timeout"))
        g_object_set(settings, "gtk-tooltip-timeout", 1500, NULL);
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
    if (doc->closing) return;
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
    if (doc->canvas) gtk_widget_queue_draw(doc->canvas);
}

static void background_toggled(GtkToggleButton *button, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    if (doc->closing) return;
    doc->background_visible = gtk_toggle_button_get_active(button);
    if (doc->canvas) gtk_widget_queue_draw(doc->canvas);
}

static void overlay_toggled(GtkToggleButton *button, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    if (doc->closing) return;
    guint overlay = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(button), "overlay-kind"));
    if (overlay >= OVERLAY_COUNT) return;
    doc->overlays[overlay] = gtk_toggle_button_get_active(button);
    if (doc->canvas) gtk_widget_queue_draw(doc->canvas);
}

static void grid_toggled(GtkToggleButton *button, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    if (doc->closing) return;
    doc->grid_visible = gtk_toggle_button_get_active(button);
    if (doc->canvas) gtk_widget_queue_draw(doc->canvas);
}

static void undo_clicked(GtkButton *button, gpointer userdata)
{ (void)button; history_step(userdata, FALSE); }

static void redo_clicked(GtkButton *button, gpointer userdata)
{ (void)button; history_step(userdata, TRUE); }

static gboolean save_override(NativeWorkspace *doc)
{
    char error[160] = {0};
    if (doc->closing || !doc->ready || !doc->override_path || doc->drawing) return FALSE;
    if (!native_map_save(doc->map, doc->override_path, error, sizeof(error))) {
        message(doc, error);
        if (doc->close_info) gtk_label_set_text(GTK_LABEL(doc->close_info), error);
        return FALSE;
    }
    doc->unsaved = FALSE;
    update_title(doc);
    message(doc, "Override saved. The ROM and initial import remain unchanged.");
    return TRUE;
}

static void save_clicked(GtkButton *button, gpointer userdata)
{
    (void)button;
    (void)save_override(userdata);
}

static void remove_notebook_page(GtkWidget **page_slot)
{
    GtkWidget *page = *page_slot;
    if (!page) return;
    *page_slot = NULL;
    /* In GTK4 the page is not guaranteed to be a direct notebook child. A
     * palette page can itself be a nested GtkNotebook, so the nearest widget
     * of that type is not necessarily the notebook that owns the page. */
    GtkNotebook *owner = NULL;
    for (GtkWidget *parent = gtk_widget_get_parent(page); parent;
         parent = gtk_widget_get_parent(parent)) {
        if (GTK_IS_NOTEBOOK(parent) &&
            gtk_notebook_page_num(GTK_NOTEBOOK(parent), page) >= 0) {
            owner = GTK_NOTEBOOK(parent);
            break;
        }
    }
    if (!owner) return;

    /* Keep the page alive until gtk_notebook_remove_page() has completed all
     * synchronous widget teardown and signal emission. */
    g_object_ref(page);
    int index = gtk_notebook_page_num(owner, page);
    if (index >= 0) gtk_notebook_remove_page(owner, index);
    g_object_unref(page);
}

static gboolean close_document(NativeWorkspace *doc)
{
    NativeWorkspace *manager;
    if (!doc || doc->closing) return FALSE;
    if (doc->unsaved) {
        message(doc, "Unsaved changes: save before closing this tab.");
        return FALSE;
    }

    doc->closing = TRUE;
    /* A pending confirmation dialog owns a document reference. Releasing it
     * before tearing down the room prevents a stale modal owning dead widgets. */
    if (doc->close_dialog) {
        GtkWidget *dialog = doc->close_dialog;
        doc->close_dialog = NULL;
        doc->close_info = NULL;
        for (unsigned i = 0; i < 3; ++i) doc->close_buttons[i] = NULL;
        gtk_window_destroy(GTK_WINDOW(dialog));
    }
    manager = doc->owner;
    if (doc->import_cancellable) g_cancellable_cancel(doc->import_cancellable);
    if (doc->import_process) g_subprocess_force_exit(doc->import_process);
    remove_notebook_page(&doc->page);
    remove_notebook_page(&doc->palette_page);
    for (guint i = 0; manager && manager->documents &&
                            i < manager->documents->len; ++i) {
        if (g_ptr_array_index(manager->documents, i) == doc) {
            g_ptr_array_remove_index(manager->documents, i);
            return TRUE;
        }
    }
    return FALSE;
}

/* Never destroy the GtkButton (and its notebook tab) from its own
 * "clicked" signal emission. Close at idle, after GTK finishes dispatch. */
static gboolean close_document_idle(gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    doc->close_pending = FALSE;
    if (!doc->closing && doc->owner) close_document(doc);
    return G_SOURCE_REMOVE;
}

/* Keep the document alive for the entire modal dialog lifetime. Never
 * destroy the dialog from a GtkButton::clicked handler: schedule the response
 * to run after GTK has finished processing that click. */
static void clear_close_dialog_refs(NativeWorkspace *doc)
{
    doc->close_dialog = NULL;
    doc->close_info = NULL;
    for (unsigned i = 0; i < 3; ++i) doc->close_buttons[i] = NULL;
}

/* GTK4 has GtkWindow::close-request, not the old GtkWidget::destroy signal. */
static gboolean confirm_dialog_close_request(GtkWindow *window, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    if (doc->close_dialog == GTK_WIDGET(window)) clear_close_dialog_refs(doc);
    return FALSE; /* GTK performs the default window close. */
}

typedef struct {
    NativeWorkspace *doc;
    int action;
} CloseChoice;

static gboolean confirm_choice_idle(gpointer userdata)
{
    CloseChoice *choice = userdata;
    NativeWorkspace *doc = choice->doc;
    doc->close_choice_pending = FALSE;
    if (!doc->closing && doc->owner && doc->close_dialog) {
        gboolean should_close = FALSE;
        if (choice->action == 0) {
            /* Cancel: retain all edits. */
        } else if (choice->action == 1) {
            /* Discard: do not write any override to disk. */
            doc->unsaved = FALSE;
            update_title(doc);
            should_close = TRUE;
        } else if (choice->action == 2) {
            should_close = save_override(doc);
        }
        if (choice->action != 2 || should_close) {
            GtkWidget *dialog = doc->close_dialog;
            clear_close_dialog_refs(doc);
            gtk_window_destroy(GTK_WINDOW(dialog));
        }
        if (should_close && !doc->closing && !doc->close_pending) {
            doc->close_pending = TRUE;
            g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, close_document_idle,
                            document_ref(doc), (GDestroyNotify)document_unref);
        }
    }
    document_unref(doc);
    g_free(choice);
    return G_SOURCE_REMOVE;
}

static void confirm_button_clicked(GtkButton *button, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    if (!doc || doc->closing || !doc->owner || doc->close_choice_pending) return;
    CloseChoice *choice = g_new0(CloseChoice, 1);
    choice->doc = document_ref(doc);
    choice->action = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "close-choice"));
    doc->close_choice_pending = TRUE;
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, confirm_choice_idle, choice, NULL);
}

static void prompt_unsaved_close(NativeWorkspace *doc)
{
    if (doc->close_dialog) {
        gtk_window_present(GTK_WINDOW(doc->close_dialog));
        return;
    }
    GtkWidget *dialog = gtk_window_new();
    GtkRoot *root = gtk_widget_get_root(doc->page);
    if (GTK_IS_WINDOW(root))
        gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(root));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_title(GTK_WINDOW(dialog), "Unsaved room changes");
    gtk_window_set_default_size(GTK_WINDOW(dialog), 420, -1);
    GtkWidget *layout = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gchar *title = g_strdup_printf("Close %s?", doc->identity ? doc->identity : "room");
    GtkWidget *heading = gtk_label_new(title);
    g_free(title);
    gtk_widget_add_css_class(heading, "title-3");
    gtk_label_set_xalign(GTK_LABEL(heading), 0);
    doc->close_info = gtk_label_new(
        "This room contains unsaved changes. Save, discard, or cancel closing.");
    gtk_label_set_wrap(GTK_LABEL(doc->close_info), TRUE);
    gtk_label_set_selectable(GTK_LABEL(doc->close_info), TRUE);
    gtk_label_set_xalign(GTK_LABEL(doc->close_info), 0);
    gtk_widget_set_margin_start(layout, 18);
    gtk_widget_set_margin_end(layout, 18);
    gtk_widget_set_margin_top(layout, 18);
    gtk_widget_set_margin_bottom(layout, 18);
    gtk_box_append(GTK_BOX(layout), heading);
    gtk_box_append(GTK_BOX(layout), doc->close_info);
    const char *labels[] = {"Cancel", "Discard changes", "Save and close"};
    for (int i = 0; i < 3; ++i) {
        GtkWidget *button = gtk_button_new_with_label(labels[i]);
        doc->close_buttons[i] = button;
        g_object_set_data(G_OBJECT(button), "close-choice", GINT_TO_POINTER(i));
        g_signal_connect(button, "clicked", G_CALLBACK(confirm_button_clicked), doc);
        if (i == 2) gtk_widget_add_css_class(button, "suggested-action");
        if (i == 1) gtk_widget_add_css_class(button, "destructive-action");
        gtk_box_append(GTK_BOX(buttons), button);
    }
    gtk_box_append(GTK_BOX(layout), buttons);
    gtk_window_set_child(GTK_WINDOW(dialog), layout);
    doc->close_dialog = dialog;
    /* The window owns a strong document ref until its GObject is finalized. */
    g_object_set_data_full(G_OBJECT(dialog), "native-room-close-doc",
                           document_ref(doc), (GDestroyNotify)document_unref);
    g_signal_connect(dialog, "close-request",
                     G_CALLBACK(confirm_dialog_close_request), doc);
    gtk_window_present(GTK_WINDOW(dialog));
}

static void close_clicked(GtkButton *button, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    (void)button;
    if (!doc || doc->closing || doc->close_pending || doc->close_choice_pending) return;
    if (doc->unsaved) {
        prompt_unsaved_close(doc);
        return;
    }
    doc->close_pending = TRUE;
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, close_document_idle,
                    document_ref(doc), (GDestroyNotify)document_unref);
}

static void focus_page(GtkWidget *page)
{
    if (!page) return;
    /* GTK4 GtkNotebook pages may sit below internal container widgets.
     * The direct parent of a notebook page need not be the GtkNotebook.
     * Instead, for each ancestor notebook, select the actual registered
     * page containing our document, including the outer Open editors page.
     * This also handles nested palette notebooks and detached windows. */
    GtkWidget *last_notebook = NULL;
    for (GtkWidget *ancestor = gtk_widget_get_parent(page); ancestor;
         ancestor = gtk_widget_get_parent(ancestor)) {
        if (!GTK_IS_NOTEBOOK(ancestor)) continue;
        GtkNotebook *notebook = GTK_NOTEBOOK(ancestor);
        guint count = gtk_notebook_get_n_pages(notebook);
        for (guint i = 0; i < count; ++i) {
            GtkWidget *candidate = gtk_notebook_get_nth_page(notebook, (gint)i);
            if (candidate != page &&
                !gtk_widget_is_ancestor(page, candidate)) continue;
            gtk_notebook_set_current_page(notebook, (gint)i);
            last_notebook = ancestor;
            break;
        }
    }
    if (last_notebook) {
        GtkRoot *root = gtk_widget_get_root(last_notebook);
        if (GTK_IS_WINDOW(root)) gtk_window_present(GTK_WINDOW(root));
    }
}

static gboolean key_pressed(GtkEventControllerKey *controller, guint keyval,
                            guint keycode, GdkModifierType modifiers, gpointer userdata)
{
    NativeWorkspace *doc = userdata;
    if (doc->closing) return FALSE;
    if (keyval == GDK_KEY_Escape && doc->project_move_pending) {
        doc->project_move_pending = FALSE;
        message(doc, "Project move canceled.");
        return TRUE;
    }
    (void)controller; (void)keycode;
    if (modifiers & GDK_CONTROL_MASK) {
        switch (gdk_keyval_to_lower(keyval)) {
        case GDK_KEY_s: save_clicked(NULL, doc); return TRUE;
        case GDK_KEY_w: close_clicked(NULL, doc); return TRUE;
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
        "Pencil (draw)", "Eraser", "Fill bucket",
        "Eyedropper (pick a metatile)", "Rectangle selection (drag to move)",
        "Hand (pan the view)"
    };
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *tools = gtk_flow_box_new();
    GtkWidget *scroll = gtk_scrolled_window_new();
    GtkWidget *palette_scroll = gtk_scrolled_window_new();
    GtkWidget *palette_tabs = gtk_notebook_new();
    GtkWidget *data_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *data_scroll = gtk_scrolled_window_new();
    GtkWidget *data_list = gtk_list_box_new();
    GtkWidget *palette = gtk_drawing_area_new();
    GtkWidget *undo = icon_button("edit-undo-symbolic", "Undo");
    GtkWidget *redo = icon_button("edit-redo-symbolic", "Redo");
    GtkWidget *save = icon_button("document-save-symbolic", "Save override (Ctrl+S)");
    doc->save_button = save;
    GtkWidget *grid = icon_toggle("view-grid-symbolic", "Show or hide the grid (G)");
    GtkWidget *background = icon_toggle("image-x-generic-symbolic", "Show BG2 / BG3 (experimental background preview)");
    static const char *const overlay_labels[OVERLAY_COUNT] = {
        "Walls", "Enemies", "Items", "Objects", "Doors", "Events", "Triggers", "Other"
    };
    static const char *const overlay_tips[OVERLAY_COUNT] = {
        "Show original native collision / wall data",
        "Show decoded enemy placements, including conditional variants",
        "Show native pickups, upgrades and items",
        "Show native world objects and props",
        "Show native door sprites, door tables and room transitions",
        "Show native events (not conditional enemy/item spawn variants)",
        "Trigger region decoder is not available yet",
        "Show unknown or unclassified native entity records"
    };
    GtkWidget *overlay_buttons[OVERLAY_COUNT];
    GtkWidget *tab_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *close = icon_button("window-close-symbolic", "Close this tab");
    doc->close_button = close;
    document_track_widget(doc, &doc->tab_title, gtk_label_new(doc->identity));
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
    doc->background_visible = TRUE;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(background), TRUE);
    gtk_flow_box_insert(GTK_FLOW_BOX(tools), background, -1);
    for (guint i = 0; i < OVERLAY_COUNT; ++i) {
        overlay_buttons[i] = gtk_toggle_button_new_with_label(overlay_labels[i]);
        doc->overlay_buttons[i] = overlay_buttons[i];
        gtk_widget_add_css_class(overlay_buttons[i], "flat");
        delayed_tip(overlay_buttons[i], overlay_tips[i]);
        g_object_set_data(G_OBJECT(overlay_buttons[i]), "overlay-kind", GUINT_TO_POINTER(i));
        gtk_flow_box_insert(GTK_FLOW_BOX(tools), overlay_buttons[i], -1);
        g_signal_connect(overlay_buttons[i], "toggled", G_CALLBACK(overlay_toggled), doc);
    }
    /* Trigger structures are not decoded for either engine yet. */
    gtk_widget_set_sensitive(overlay_buttons[OVERLAY_TRIGGERS], FALSE);
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
    gtk_box_append(GTK_BOX(values), gtk_label_new("Layer"));
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
    GtkGesture *annotation_context = gtk_gesture_click_new();
    GtkEventController *hover = gtk_event_controller_motion_new();
    GtkEventController *wheel = gtk_event_controller_scroll_new(
        GTK_EVENT_CONTROLLER_SCROLL_VERTICAL | GTK_EVENT_CONTROLLER_SCROLL_DISCRETE);
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(drag), GDK_BUTTON_PRIMARY);
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), GDK_BUTTON_PRIMARY);
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(annotation_context),
                                  GDK_BUTTON_SECONDARY);
    gtk_gesture_group(drag, click);
    gtk_widget_add_controller(doc->canvas, GTK_EVENT_CONTROLLER(drag));
    gtk_widget_add_controller(doc->canvas, GTK_EVENT_CONTROLLER(click));
    gtk_widget_add_controller(doc->canvas, GTK_EVENT_CONTROLLER(annotation_context));
    gtk_widget_add_controller(doc->canvas, hover);
    gtk_event_controller_set_propagation_phase(wheel, GTK_PHASE_CAPTURE);
    gtk_widget_add_controller(scroll, wheel);
    g_signal_connect(hover, "motion", G_CALLBACK(canvas_hover), doc);
    g_signal_connect(hover, "leave", G_CALLBACK(canvas_leave), doc);
    g_signal_connect(drag, "drag-begin", G_CALLBACK(gesture_begin), doc);
    g_signal_connect(drag, "drag-update", G_CALLBACK(gesture_update), doc);
    g_signal_connect(drag, "drag-end", G_CALLBACK(gesture_end), doc);
    g_signal_connect(click, "pressed", G_CALLBACK(canvas_click_down), doc);
    g_signal_connect(click, "released", G_CALLBACK(canvas_click_up), doc);
    g_signal_connect(annotation_context, "pressed",
                     G_CALLBACK(annotation_canvas_context_pressed), doc);
    g_signal_connect(wheel, "scroll", G_CALLBACK(canvas_scroll), doc);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), doc->canvas);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(scroll, TRUE);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_box_append(GTK_BOX(page), scroll);
    document_track_widget(doc, &doc->status,
                          gtk_label_new("Import in progress..."));
    gtk_label_set_xalign(GTK_LABEL(doc->status), 0);
    gtk_label_set_wrap(GTK_LABEL(doc->status), TRUE);
    gtk_label_set_selectable(GTK_LABEL(doc->status), TRUE);
    gtk_box_append(GTK_BOX(page), doc->status);
    GtkEventController *keys = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(keys, GTK_PHASE_CAPTURE);
    gtk_widget_add_controller(page, keys);
    g_signal_connect(keys, "key-pressed", G_CALLBACK(key_pressed), doc);
    gtk_widget_set_focusable(page, TRUE);
    document_track_widget(doc, &doc->page, page);
    g_object_set_data(G_OBJECT(page), "mv-world-mode",
        GINT_TO_POINTER(g_str_has_prefix(doc->identity, "Aria ") ? 2 : 1));
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
    doc->annotations_list = data_list;
    doc->annotations_status = gtk_label_new("Native room data imports with the room.");
    gtk_label_set_xalign(GTK_LABEL(doc->annotations_status), 0);
    gtk_label_set_wrap(GTK_LABEL(doc->annotations_status), TRUE);
    gtk_label_set_selectable(GTK_LABEL(doc->annotations_status), TRUE);
    gtk_widget_set_margin_start(doc->annotations_status, 8);
    gtk_widget_set_margin_end(doc->annotations_status, 8);
    gtk_widget_set_margin_top(doc->annotations_status, 8);
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(data_list), GTK_SELECTION_SINGLE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(data_scroll), data_list);
    gtk_widget_set_vexpand(data_scroll, TRUE);
    gtk_box_append(GTK_BOX(data_page), doc->annotations_status);
    gtk_box_append(GTK_BOX(data_page), data_scroll);
    gtk_notebook_append_page(GTK_NOTEBOOK(palette_tabs), palette_scroll,
                             gtk_label_new("Metatiles"));
    gtk_notebook_append_page(GTK_NOTEBOOK(palette_tabs), data_page,
                             gtk_label_new("Room data"));
    document_track_widget(doc, &doc->palette_page, palette_tabs);
    GtkWidget *palette_label = gtk_label_new(doc->identity);
    gtk_notebook_append_page(doc->owner->right, palette_tabs, palette_label);
    gtk_notebook_set_tab_reorderable(doc->owner->right, palette_tabs, TRUE);
    gtk_notebook_set_tab_detachable(doc->owner->right, palette_tabs, TRUE);
    g_signal_connect(doc->layer, "notify::selected", G_CALLBACK(layer_changed), doc);
    g_signal_connect(doc->brush, "value-changed", G_CALLBACK(brush_changed), doc);
    g_signal_connect(doc->zoom, "value-changed", G_CALLBACK(zoom_changed), doc);
    g_signal_connect(grid, "toggled", G_CALLBACK(grid_toggled), doc);
    g_signal_connect(background, "toggled", G_CALLBACK(background_toggled), doc);
    g_signal_connect(undo, "clicked", G_CALLBACK(undo_clicked), doc);
    g_signal_connect(redo, "clicked", G_CALLBACK(redo_clicked), doc);
    g_signal_connect(save, "clicked", G_CALLBACK(save_clicked), doc);
    g_signal_connect(close, "clicked", G_CALLBACK(close_clicked), doc);
    focus_page(page);
    gtk_widget_grab_focus(page);
    focus_page(palette_tabs);
}

static void open_room(NativeWorkspace *doc, const char *area, unsigned number)
{
    char area_lower[32], base[320], over[320], atlas[420], error[180] = {0};
    gboolean aria = g_str_has_prefix(doc->identity, "Aria ");
    size_t length = strlen(area);
    if (length >= sizeof(area_lower)) return;
    for (size_t i = 0; i < length; ++i) area_lower[i] = (char)g_ascii_tolower(area[i]);
    area_lower[length] = '\0';
    if (aria) {
        snprintf(base, sizeof(base),
                 "assets/extracted/rooms/aria/workrooms/area_%02u_room_%03u.mvnative",
                 (unsigned)atoi(area), number);
        snprintf(over, sizeof(over),
                 "assets/extracted/overrides/aria/area_%02u_room_%03u.mvnative",
                 (unsigned)atoi(area), number);
    } else {
        snprintf(base, sizeof(base),
                 "assets/extracted/rooms/metroid/workrooms/%s_%03u.mvnative",
                 area_lower, number);
        snprintf(over, sizeof(over),
                 "assets/extracted/overrides/metroid/%s_%03u.mvnative",
                 area_lower, number);
    }
    NativeMap *temporary = calloc(1, sizeof(*temporary));
    if (!temporary) { message(doc, "Out of memory"); return; }
    const char *source = g_file_test(over, G_FILE_TEST_IS_REGULAR) ? over : base;
    if (!native_map_load(temporary, source, error, sizeof(error))) {
        message(doc, error);
        free(temporary);
        return;
    }
    snprintf(atlas, sizeof(atlas), "assets/extracted/%s", temporary->atlas);
    if (!load_atlas(doc, atlas)) {
        message(doc, "Unable to load this room's authentic atlas");
        free(temporary);
        return;
    }
    *doc->map = *temporary;
    free(temporary);
    char bgpath[420];
    if (aria)
        snprintf(bgpath, sizeof(bgpath),
                 "assets/extracted/rooms/aria/previews/area_%02u_room_%03u_bg3.bmp",
                 (unsigned)atoi(area), number);
    else
        snprintf(bgpath, sizeof(bgpath),
                 "assets/extracted/rooms/metroid/previews/%s_%03u_bg3.bmp",
                 area_lower, number);
    load_background(doc, bgpath);
    char collision_path[420];
    if (aria)
        snprintf(collision_path, sizeof(collision_path),
                 "assets/extracted/rooms/aria/previews/area_%02u_room_%03u_collision.bmp",
                 (unsigned)atoi(area), number);
    else
        snprintf(collision_path, sizeof(collision_path),
                 "assets/extracted/rooms/metroid/previews/%s_%03u_collision.bmp",
                 area_lower, number);
    load_collision(doc, collision_path);
    char annotations_path[420];
    if (aria)
        snprintf(annotations_path, sizeof(annotations_path),
                 "assets/extracted/rooms/aria/annotations/area_%02u_room_%03u.tsv",
                 (unsigned)atoi(area), number);
    else
        snprintf(annotations_path, sizeof(annotations_path),
                 "assets/extracted/rooms/metroid/annotations/%s_%03u.tsv",
                 area_lower, number);
    g_free(doc->annotations_path);
    doc->annotations_path = g_strdup(annotations_path);
    g_free(doc->project_area);
    doc->project_area = g_strdup(aria ? area :
        g_ascii_strcasecmp(area, "brinstar") == 0 ? "Brinstar" :
        g_ascii_strcasecmp(area, "kraid") == 0 ? "Kraid" :
        g_ascii_strcasecmp(area, "norfair") == 0 ? "Norfair" :
        g_ascii_strcasecmp(area, "ridley") == 0 ? "Ridley" :
        g_ascii_strcasecmp(area, "tourian") == 0 ? "Tourian" :
        g_ascii_strcasecmp(area, "crateria") == 0 ? "Crateria" : "Chozodia");
    doc->project_aria = aria;
    doc->project_room = number;
    load_annotations(doc, annotations_path);
    history_clear(doc->undo, &doc->undo_count);
    history_clear(doc->redo, &doc->redo_count);
    g_free(doc->override_path);
    doc->override_path = g_strdup(over);
    char *dir = g_path_get_dirname(over);
    g_mkdir_with_parents(dir, 0700);
    g_free(dir);
    doc->ready = TRUE;
    project_reload(doc);
    doc->unsaved = FALSE;
    doc->brush_id = doc->layer_id = 0;
    doc->has_selection = FALSE;
    doc->pointer_over_canvas = FALSE;
    doc->pick_to_pencil = FALSE;
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
    gchar *summary = g_strdup_printf("%s - %u original metatiles, BG1/BG2. %s",
                                     doc->map->room_id, doc->map->tile_count,
                                     strcmp(source, over) == 0 ?
                                     "Previous override restored." : "Private copy created.");
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
    g_clear_object(&doc->import_cancellable);
    g_clear_object(&doc->import_process);
    if (!doc->closing && doc->page && doc->palette_page &&
        succeeded && g_subprocess_get_successful(proc)) {
        const char *area = g_object_get_data(G_OBJECT(proc), "native-area");
        unsigned number = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(proc), "native-room"));
        open_room(doc, area, number);
    } else if (!doc->closing && doc->page) {
        message(doc, error ? error->message : err ? err : "Import failed");
    }
    if (error) g_error_free(error);
    g_free(out);
    g_free(err);
    document_unref(doc);
}

static void start_import(NativeWorkspace *doc, const char *area, unsigned number)
{
    char room_text[16];
    snprintf(room_text, sizeof(room_text), "%u", number);
    GError *error = NULL;
    const char *importer = g_str_has_prefix(doc->identity, "Aria ") ?
        "scripts.aos_native_workspace" : "scripts.mzm_native_workspace";
    GSubprocess *proc = g_subprocess_new(
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE,
        &error, "python3", "-m", importer,
        "--area", area, "--room", room_text, NULL);
    if (!proc) {
        message(doc, error ? error->message : "Python importer unavailable");
        if (error) g_error_free(error);
        return;
    }
    doc->busy = TRUE;
    message(doc, "Importing verified ROM data...");
    doc->import_cancellable = g_cancellable_new();
    doc->import_process = g_object_ref(proc);
    g_object_set_data_full(G_OBJECT(proc), "native-area", g_strdup(area), g_free);
    g_object_set_data(G_OBJECT(proc), "native-room", GUINT_TO_POINTER(number));
    g_subprocess_communicate_utf8_async(proc, NULL, doc->import_cancellable,
                                        imported, document_ref(doc));
    g_object_unref(proc);
}

NativeWorkspace *native_workspace_new(void)
{
    NativeWorkspace *manager = calloc(1, sizeof(*manager));
    if (!manager) return NULL;
    manager->documents = g_ptr_array_new_with_free_func((GDestroyNotify)document_unref);
    return manager;
}

void native_workspace_build(NativeWorkspace *manager, GtkWidget *center, GtkWidget *right)
{
    if (!manager || !GTK_IS_NOTEBOOK(center) || !GTK_IS_NOTEBOOK(right)) return;
    manager->center = GTK_NOTEBOOK(center);
    manager->right = GTK_NOTEBOOK(right);
}

static NativeWorkspace *create_document(NativeWorkspace *manager, char *identity)
{
    NativeWorkspace *doc = calloc(1, sizeof(*doc));
    if (!doc) {
        g_free(identity);
        return NULL;
    }
    doc->owner = manager;
    doc->references = 1;
    doc->identity = identity;
    doc->map = calloc(1, sizeof(NativeMap));
    doc->stroke_before = calloc(1, sizeof(NativeMap));
    doc->undo = calloc(HISTORY_LIMIT, sizeof(*doc->undo));
    doc->redo = calloc(HISTORY_LIMIT, sizeof(*doc->redo));
    doc->annotations = g_array_new(FALSE, FALSE, sizeof(RoomAnnotation));
    if (!doc->map || !doc->stroke_before || !doc->undo || !doc->redo ||
        !doc->annotations) {
        document_destroy(doc);
        return NULL;
    }
    doc->scale = 2.0;
    doc->last_x = doc->last_y = -1;
    g_ptr_array_add(manager->documents, doc);
    document_build(doc);
    return doc;
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
    NativeWorkspace *doc = create_document(manager, identity);
    if (doc) start_import(doc, area, number);
}

void native_workspace_import_aria_async(NativeWorkspace *manager, unsigned area,
                                        unsigned number)
{
    if (!manager || !manager->documents || !manager->center || !manager->right ||
        area >= 12 || number >= 1000) return;
    char *identity = g_strdup_printf("Aria %02u:%03u", area, number);
    for (guint i = 0; i < manager->documents->len; ++i) {
        NativeWorkspace *doc = g_ptr_array_index(manager->documents, i);
        if (g_ascii_strcasecmp(doc->identity, identity) == 0) {
            focus_page(doc->page);
            focus_page(doc->palette_page);
            if (!doc->busy && !doc->ready) {
                char text[16];
                snprintf(text, sizeof(text), "%u", area);
                start_import(doc, text, number);
            }
            g_free(identity);
            return;
        }
    }
    NativeWorkspace *doc = create_document(manager, identity);
    if (doc) {
        char text[16];
        snprintf(text, sizeof(text), "%u", area);
        start_import(doc, text, number);
    }
}

#ifdef FUSION_NATIVE_WORKSPACE_TESTING
guint native_workspace_test_document_count(const NativeWorkspace *manager)
{
    return manager && manager->documents ? manager->documents->len : 0;
}

gboolean native_workspace_test_add_document(NativeWorkspace *manager,
                                             const char *identity)
{
    if (!manager || !manager->documents || !manager->center || !manager->right ||
        !identity || !identity[0]) return FALSE;
    return create_document(manager, g_strdup(identity)) != NULL;
}

gboolean native_workspace_test_close_document(NativeWorkspace *manager, guint index)
{
    if (!manager || !manager->documents || index >= manager->documents->len) return FALSE;
    NativeWorkspace *doc = g_ptr_array_index(manager->documents, index);
    document_ref(doc);
    gboolean closed = close_document(doc);
    document_unref(doc);
    return closed;
}

gboolean native_workspace_test_activate_close(NativeWorkspace *manager, guint index)
{
    if (!manager || !manager->documents || index >= manager->documents->len) return FALSE;
    NativeWorkspace *doc = g_ptr_array_index(manager->documents, index);
    g_signal_emit_by_name(doc->close_button, "clicked");
    /* Run the idle close after the clicked emission, like the GTK event loop. */
    while (g_main_context_pending(NULL))
        g_main_context_iteration(NULL, FALSE);
    return TRUE;
}

gboolean native_workspace_test_choose_close(NativeWorkspace *manager, guint index,
                                            guint choice)
{
    if (!manager || !manager->documents || index >= manager->documents->len || choice >= 3)
        return FALSE;
    NativeWorkspace *doc = g_ptr_array_index(manager->documents, index);
    if (!doc->close_dialog || !doc->close_buttons[choice]) return FALSE;
    g_signal_emit_by_name(doc->close_buttons[choice], "clicked");
    while (g_main_context_pending(NULL)) g_main_context_iteration(NULL, FALSE);
    return TRUE;
}

gboolean native_workspace_test_prepare_modified(NativeWorkspace *manager, guint index,
                                                 const char *override_path)
{
    if (!manager || !manager->documents || index >= manager->documents->len ||
        !override_path || !override_path[0]) return FALSE;
    NativeWorkspace *doc = g_ptr_array_index(manager->documents, index);
    memset(doc->map, 0, sizeof(*doc->map));
    snprintf(doc->map->room_id, sizeof(doc->map->room_id), "mzm:test:001");
    snprintf(doc->map->atlas, sizeof(doc->map->atlas),
             "rooms/metroid/tilesets/test_atlas.bmp");
    doc->map->tile_count = 1;
    doc->map->width[0] = doc->map->height[0] = 1;
    doc->map->width[1] = doc->map->height[1] = 1;
    g_free(doc->override_path);
    doc->override_path = g_strdup(override_path);
    doc->ready = TRUE;
    doc->unsaved = TRUE;
    update_title(doc);
    return TRUE;
}

void native_workspace_test_activate_save(NativeWorkspace *manager, guint index)
{
    if (!manager || !manager->documents || index >= manager->documents->len) return;
    NativeWorkspace *doc = g_ptr_array_index(manager->documents, index);
    g_signal_emit_by_name(doc->save_button, "clicked");
}

void native_workspace_test_set_unsaved(NativeWorkspace *manager, guint index,
                                        gboolean unsaved)
{
    if (!manager || !manager->documents || index >= manager->documents->len) return;
    NativeWorkspace *doc = g_ptr_array_index(manager->documents, index);
    doc->unsaved = unsaved;
    update_title(doc);
}

static void move_page_for_test(GtkWidget *page, GtkNotebook *target)
{
    GtkNotebook *owner = NULL;
    for (GtkWidget *parent = gtk_widget_get_parent(page); parent;
         parent = gtk_widget_get_parent(parent)) {
        if (GTK_IS_NOTEBOOK(parent) &&
            gtk_notebook_page_num(GTK_NOTEBOOK(parent), page) >= 0) {
            owner = GTK_NOTEBOOK(parent);
            break;
        }
    }
    if (!owner || owner == target) return;
    GtkWidget *label = gtk_notebook_get_tab_label(owner, page);
    g_object_ref(page);
    if (label) g_object_ref(label);
    gtk_notebook_detach_tab(owner, page);
    gtk_notebook_append_page(target, page, label);
    if (label) g_object_unref(label);
    g_object_unref(page);
}

gboolean native_workspace_test_move_document(NativeWorkspace *manager, guint index,
                                              GtkWidget *center, GtkWidget *right)
{
    if (!manager || !manager->documents || index >= manager->documents->len ||
        !GTK_IS_NOTEBOOK(center) || !GTK_IS_NOTEBOOK(right)) return FALSE;
    NativeWorkspace *doc = g_ptr_array_index(manager->documents, index);
    move_page_for_test(doc->page, GTK_NOTEBOOK(center));
    move_page_for_test(doc->palette_page, GTK_NOTEBOOK(right));
    return TRUE;
}
#endif

void native_workspace_free(NativeWorkspace *manager)
{
    if (!manager) return;
    if (manager->documents) {
        for (guint i = 0; i < manager->documents->len; ++i) {
            NativeWorkspace *doc = g_ptr_array_index(manager->documents, i);
            doc->closing = TRUE;
            if (doc->close_dialog) {
                GtkWidget *dialog = doc->close_dialog;
                clear_close_dialog_refs(doc);
                gtk_window_destroy(GTK_WINDOW(dialog));
            }
            if (doc->import_cancellable) g_cancellable_cancel(doc->import_cancellable);
            if (doc->import_process) g_subprocess_force_exit(doc->import_process);
            /* Shut down GTK-owned callbacks before releasing manager ownership.
             * A notebook/page might already have been destroyed by the window;
             * document_track_widget() guarantees these slots become NULL. */
            remove_notebook_page(&doc->page);
            remove_notebook_page(&doc->palette_page);
            doc->owner = NULL;
        }
        g_ptr_array_free(manager->documents, TRUE);
    }
    free(manager);
}
