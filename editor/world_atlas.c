/* SPDX-License-Identifier: GPL-3.0-only */
/* Original minimap cell grid for both worlds. No invented room footprints. */
#include "world_atlas.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    guint area, room, x, y, save, warp, provenance, tile;
    char ownership[192], draft_id[96];
} MapCell;
typedef struct {
    GArray *cells;
    NativeWorkspace *workspace;
    GtkWidget *page, *grid, *details, *status, *world_select, *area_select, *zoom;
    GtkWidget *selected_cell, *scroller, *popup;
    double drag_hstart, drag_vstart, pending_h, pending_v;
    guint pan_tick;
    GtkWidget *world_badge;
    guint world, area;
    guint selection;
    gboolean selected, busy, pending_generation, changing_world, prefetching;
    guint next_preview_area;
    guint size;
} WorldGrid;
static const char *const worlds[]={"Zero Mission", "Aria of Sorrow", NULL};
static const char *const mzm_areas[]={"Brinstar", "Kraid", "Norfair", "Ridley", "Tourian", "Crateria", "Chozodia", NULL};
static const char *const mzm_slugs[]={"brinstar","kraid","norfair","ridley","tourian","crateria","chozodia"};
static const char *const aria_areas[]={"Castle Corridor", "Chapel", "Study", "Dance Hall", "Inner Quarters", "Floating Garden", "Clock Tower", "Underground", "The Arena", "Top Floor", "Chaotic Realm entrance", "Chaotic Realm boss", NULL};
static const char *world_code(const WorldGrid *w) { return w->world ? "aria" : "mzm"; }
static guint area_count(const WorldGrid *w) { return w->world ? 12u : 7u; }
static const char *area_name(const WorldGrid *w, guint a)
{ return w->world ? aria_areas[a] : mzm_areas[a]; }

static gboolean parse_uint(const char *text, guint *value)
{
    char extra;
    return text && sscanf(text, "%u%c", value, &extra) == 1;
}

static gboolean reload_rows(WorldGrid *w)
{
    char filename[192], *lines, **split;
    gchar *contents=NULL;
    snprintf(filename,sizeof(filename),"assets/extracted/world_overview/%s.tsv",world_code(w));
    if (!g_file_get_contents(filename,&contents,NULL,NULL)) {
        g_array_set_size(w->cells, 0);
        return FALSE;
    }
    lines=contents; split=g_strsplit(lines,"\n",-1);
    g_array_set_size(w->cells,0);
    for(guint i=0;split[i];++i){
        MapCell c={0}; guint a,r,x,y,s,v,provenance=0,tile=0;
        if (!split[i][0] || split[i][0]=='#') continue;
        char **fields = g_strsplit(split[i], "|", 10);
        guint count = g_strv_length(fields);
        if (count < 6 || count > 9 || !parse_uint(fields[0], &a) ||
            !parse_uint(fields[1], &r) || !parse_uint(fields[2], &x) ||
            !parse_uint(fields[3], &y) || !parse_uint(fields[4], &s) ||
            !parse_uint(fields[5], &v) ||
            (count >= 7 && !parse_uint(fields[6], &provenance)) ||
            (count >= 8 && !parse_uint(fields[7], &tile))) {
            g_strfreev(fields);
            continue;
        }
        /* Old private indexes remain compatible with this source-only change. */
        if (count == 6) provenance = w->world ? 2u : 0u;
        if (a>=area_count(w) || r>=1000 || x>=128 || y>=128 || s>1 || v>1 ||
            provenance>3 || (w->world && provenance!=2) ||
            (!w->world && provenance==2) ||
            (count == 9 && strlen(fields[8]) >= sizeof(c.ownership))) {
            g_strfreev(fields);
            continue;
        }
        c.area=a;c.room=r;c.x=x;c.y=y;c.save=s;c.warp=v;
        c.provenance=provenance;c.tile=tile;
        if (count == 9 && strcmp(fields[8], "-"))
            g_strlcpy(c.ownership, fields[8], sizeof(c.ownership));
        g_array_append_val(w->cells,c);
        g_strfreev(fields);
    }
    g_strfreev(split);g_free(contents);
    /* Project-authored placements are independent of original ROM occupancy.
     * Listing is bounded and validated by the versioned Python placement API. */
    gchar *out = NULL, *err = NULL;
    GError *spawn_error = NULL;
    gint exit_status = -1;
    gchar *argv[] = {"python3", "-m", "scripts.map_placements", "list", "--world",
                     w->world ? "aria" : "zero_mission", "--format", "tsv", NULL};
    gboolean launched = g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
                                     NULL, NULL, &out, &err, &exit_status, &spawn_error);
    if (launched && g_spawn_check_wait_status(exit_status, NULL) && out) {
        char **records = g_strsplit(out, "\n", -1);
        for (guint i = 0; records[i] && i < 4096; ++i) {
            if (!*records[i]) continue;
            char **fields = g_strsplit(records[i], "\t", 9);
            if (g_strv_length(fields) == 8) {
                guint a, x, y, width, height;
                if (sscanf(fields[3], "%u", &a) == 1 &&
                    sscanf(fields[4], "%u", &x) == 1 &&
                    sscanf(fields[5], "%u", &y) == 1 &&
                    sscanf(fields[6], "%u", &width) == 1 &&
                    sscanf(fields[7], "%u", &height) == 1 &&
                    strlen(fields[0]) < 96 && a < area_count(w) &&
                    width >= 1 && width <= 8 && height >= 1 && height <= 8 &&
                    x + width <= (w->world ? 64u : 32u) &&
                    y + height <= (w->world ? 35u : 32u)) {
                    for (guint yy = y; yy < y + height; ++yy)
                        for (guint xx = x; xx < x + width; ++xx) {
                            MapCell c = {0};
                            c.area = a; c.room = 998; c.x = xx; c.y = yy;
                            c.provenance = 4; /* Private project draft only. */
                            g_strlcpy(c.draft_id, fields[0], sizeof(c.draft_id));
                            g_array_append_val(w->cells, c);
                        }
                }
            }
            g_strfreev(fields);
        }
        g_strfreev(records);
    } else if (launched && err && *err) {
        g_warning("Private draft placement listing failed: %.300s", err);
    }
    g_clear_error(&spawn_error);
    g_free(out); g_free(err);
    return w->cells->len>0;
}

static void selected_open(WorldGrid *w)
{
    if (!w->selected || w->selection>=w->cells->len)return;
    MapCell c=g_array_index(w->cells,MapCell,w->selection);
    if (c.provenance == 3 || c.room == 999) return; /* No native room owner. */
    if(w->world)native_workspace_import_aria_async(w->workspace,c.area,c.room);
    else native_workspace_import_async(w->workspace,mzm_areas[c.area],c.room);
}
static void selected_open_click(GtkButton *b,gpointer data)
{ (void)b;selected_open(data); }
static guint room_cells_in_area(const WorldGrid *w, guint area, guint room)
{
    guint n = 0;
    for (guint i = 0; i < w->cells->len; ++i) {
        const MapCell *c = &g_array_index(w->cells, MapCell, i);
        if (c->area == area && c->room == room) ++n;
    }
    return n;
}

static void grid_clicked(GtkGestureClick *g, gint presses, double x, double y, gpointer data)
{
    WorldGrid *w = data;
    (void)x; (void)y;
    GtkWidget *widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(g));
    guint n = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(widget), "mv-grid-index"));
    if (!n || n > w->cells->len) return;
    w->selection = n - 1;
    MapCell c = g_array_index(w->cells, MapCell, w->selection);
    if (c.provenance == 4) {
        w->selected = FALSE;
        gchar *info = g_strdup_printf("PRIVATE DRAFT %s at (%u,%u) | "
            "Select Map creation to move it. Not playable or ROM-backed.",
            c.draft_id, c.x, c.y);
        gtk_label_set_text(GTK_LABEL(w->details), info);
        g_free(info);
        return;
    }
    if (c.provenance == 3 || c.room == 999) {
        w->selected = FALSE;
        gchar *info = g_strdup_printf(
            "Original MZM minimap tile (%u,%u), native code 0x%04x. "
            "Room ownership unknown (%s). Open an original room through a verified anchor.",
            c.x, c.y, c.tile, c.ownership[0] ? c.ownership : "unassigned");
        gtk_label_set_text(GTK_LABEL(w->details), info);
        g_free(info);
        return;
    }
    w->selected = TRUE;
    /* Selection applies to every segment of the same native room, including
     * irregular and disconnected Aria shapes. Never erase source cell gaps. */
    w->selected_cell = widget;
    for (GtkWidget *child = gtk_widget_get_first_child(w->grid); child;
         child = gtk_widget_get_next_sibling(child)) {
        guint index = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(child), "mv-grid-index"));
        if (!index || index > w->cells->len) continue;
        MapCell other = g_array_index(w->cells, MapCell, index - 1);
        if (other.area == c.area && other.room == c.room)
            gtk_widget_add_css_class(child, "mv-selected");
        else
            gtk_widget_remove_css_class(child, "mv-selected");
    }
    const char *source = c.provenance == 2 ? "original minimap cells" :
                         c.provenance == 1 ? "native room geometry/minimap intersection" :
                         "original map anchor ONLY (extent unavailable)";
    gchar *message = g_strdup_printf(
        "%s / %s / room %03u | %u mapped case(s), clicked (%u,%u) "
        "%s %s | %s%s%s",
        worlds[w->world], area_name(w, c.area), c.room,
        room_cells_in_area(w, c.area, c.room), c.x, c.y,
        c.save ? "[SAVE]" : "", c.warp ? "[WARP]" : "", source,
        c.ownership[0] ? " | progression variants: " : "", c.ownership);
    gtk_label_set_text(GTK_LABEL(w->details), message);
    g_free(message);
    if (presses >= 2) selected_open(w);
}
static gboolean has_image(const WorldGrid *w,const MapCell *c,char *dest,size_t n)
{
    if(w->world)snprintf(dest,n,"assets/extracted/rooms/aria/previews/area_%02u_room_%03u_composite.bmp",c->area,c->room);
    else snprintf(dest,n,"assets/extracted/rooms/metroid/previews/%s_%03u_bg1.bmp",mzm_slugs[c->area],c->room);
    return g_file_test(dest,G_FILE_TEST_IS_REGULAR);
}
/* Mouse navigation is on the grid so panning works over occupied cells.
 * The adjustment values are clamped within the real scrolled map content. */
static void map_drag_begin(GtkGestureDrag *drag, double x, double y, gpointer data)
{
    WorldGrid *w = data;
    (void)drag; (void)x; (void)y;
    GtkAdjustment *h = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(w->scroller));
    GtkAdjustment *v = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(w->scroller));
    w->drag_hstart = gtk_adjustment_get_value(h);
    w->drag_vstart = gtk_adjustment_get_value(v);
}

static gboolean map_pan_frame(GtkWidget *widget, GdkFrameClock *clock, gpointer data)
{
    WorldGrid *w = data;
    (void)widget; (void)clock;
    GtkAdjustment *h = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(w->scroller));
    GtkAdjustment *v = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(w->scroller));
    gtk_adjustment_set_value(h, w->pending_h);
    gtk_adjustment_set_value(v, w->pending_v);
    w->pan_tick = 0;
    return G_SOURCE_REMOVE;
}

static void map_drag_update(GtkGestureDrag *drag, double dx, double dy, gpointer data)
{
    WorldGrid *w = data;
    (void)drag;
    GtkAdjustment *h = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(w->scroller));
    GtkAdjustment *v = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(w->scroller));
    w->pending_h = CLAMP(w->drag_hstart - dx,
        gtk_adjustment_get_lower(h),
        MAX(gtk_adjustment_get_lower(h),
            gtk_adjustment_get_upper(h) - gtk_adjustment_get_page_size(h)));
    w->pending_v = CLAMP(w->drag_vstart - dy,
        gtk_adjustment_get_lower(v),
        MAX(gtk_adjustment_get_lower(v),
            gtk_adjustment_get_upper(v) - gtk_adjustment_get_page_size(v)));
    if (!w->pan_tick)
        w->pan_tick = gtk_widget_add_tick_callback(w->scroller, map_pan_frame, w, NULL);
}

static gboolean map_scroll(GtkEventControllerScroll *controller, double dx,
                           double dy, gpointer data)
{
    WorldGrid *w = data;
    GdkModifierType state = gtk_event_controller_get_current_event_state(
        GTK_EVENT_CONTROLLER(controller));
    (void)dx;
    if (!(state & GDK_CONTROL_MASK) || dy == 0) return FALSE;
    double value = gtk_spin_button_get_value(GTK_SPIN_BUTTON(w->zoom));
    double step = gtk_adjustment_get_step_increment(
        gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(w->zoom)));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->zoom), value + (dy < 0 ? step : -step));
    return TRUE;
}

static void map_popover_close(WorldGrid *w)
{
    if (!w->popup) return;
    GtkWidget *popup = w->popup;
    /* An unparented popover may live past this callback. Remove the weak
     * handle now, not later, so an old popup cannot be reused accidentally. */
    g_object_remove_weak_pointer(G_OBJECT(popup), (gpointer *)&w->popup);
    w->popup = NULL;
    gtk_popover_popdown(GTK_POPOVER(popup));
    gtk_widget_unparent(popup);
}

/* Map creation is project-owned: opening a form never edits a ROM and
 * never assigns a location to a room without a native authoring adapter. */
static void map_context_action(GtkButton *button, gpointer data)
{
    WorldGrid *w = data;
    guint action = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(button), "mv-map-action"));
    guint cell_x = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(button), "mv-cell-x"));
    guint cell_y = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(button), "mv-cell-y"));
    if (cell_x) --cell_x;
    if (cell_y) --cell_y;
    if (action == 1) {
        selected_open(w);
    } else if (action == 2) {
        gchar *coordinates = g_strdup_printf("%s, map cell (%u,%u)",
            worlds[w->world], cell_x, cell_y);
        GdkDisplay *display = gdk_display_get_default();
        if (display) gdk_clipboard_set_text(gdk_display_get_clipboard(display), coordinates);
        g_free(coordinates);
    } else if (action == 3) {
        GtkWidget *notebook = gtk_widget_get_ancestor(w->page, GTK_TYPE_NOTEBOOK);
        if (GTK_IS_NOTEBOOK(notebook)) {
            guint total = gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));
            for (guint i = 0; i < total; ++i) {
                GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), (gint)i);
                GtkWidget *hint = g_object_get_data(G_OBJECT(page), "mv-map-creator-hint");
                if (!hint) continue;
                gchar *info = g_strdup_printf(
                    "Selected %s cell (%u,%u). Choose an existing private draft "
                    "below to save its placement or create a new draft first.",
                    worlds[w->world], cell_x, cell_y);
                gtk_label_set_text(GTK_LABEL(hint), info);
                GtkWidget *xspin = g_object_get_data(G_OBJECT(page), "mv-placement-x");
                GtkWidget *yspin = g_object_get_data(G_OBJECT(page), "mv-placement-y");
                if (GTK_IS_SPIN_BUTTON(xspin))
                    gtk_spin_button_set_value(GTK_SPIN_BUTTON(xspin), cell_x);
                if (GTK_IS_SPIN_BUTTON(yspin))
                    gtk_spin_button_set_value(GTK_SPIN_BUTTON(yspin), cell_y);
                g_free(info);
                gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), (gint)i);
                break;
            }
        }
    }
    if (w->popup) gtk_popover_popdown(GTK_POPOVER(w->popup));
}

static void map_context_add_button(GtkWidget *list, WorldGrid *w, const char *title,
                                   guint action, guint x, guint y)
{
    GtkWidget *button = gtk_button_new_with_label(title);
    g_object_set_data(G_OBJECT(button), "mv-map-action", GUINT_TO_POINTER(action));
    g_object_set_data(G_OBJECT(button), "mv-cell-x", GUINT_TO_POINTER(x + 1));
    g_object_set_data(G_OBJECT(button), "mv-cell-y", GUINT_TO_POINTER(y + 1));
    g_signal_connect(button, "clicked", G_CALLBACK(map_context_action), w);
    gtk_box_append(GTK_BOX(list), button);
}

static void map_context_pressed(GtkGestureClick *gesture, gint n,
                                double px, double py, gpointer data)
{
    WorldGrid *w = data;
    GtkWidget *cell = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
    (void)n;
    map_popover_close(w);
    guint entry = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(cell), "mv-grid-index"));
    guint x = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(cell), "mv-cell-x"));
    guint y = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(cell), "mv-cell-y"));
    if (x) --x;
    if (y) --y;
    /* Spanning Aria rooms contain several minimap cases. Use the actual
     * pointer position within the displayed rectangle, not its top-left. */
    guint columns = MAX(1u, GPOINTER_TO_UINT(g_object_get_data(
        G_OBJECT(cell), "mv-span-width")));
    guint rows = MAX(1u, GPOINTER_TO_UINT(g_object_get_data(
        G_OBJECT(cell), "mv-span-height")));
    guint width = MAX(1, gtk_widget_get_width(cell));
    guint height = MAX(1, gtk_widget_get_height(cell));
    x += MIN(columns - 1, (guint)(MAX(0.0, px) * columns / width));
    y += MIN(rows - 1, (guint)(MAX(0.0, py) * rows / height));
    gboolean room = FALSE;
    if (entry && entry <= w->cells->len) {
        MapCell marker = g_array_index(w->cells, MapCell, entry - 1);
        room = marker.provenance != 3 && marker.provenance != 4 && marker.room != 999;
        if (room) {
            w->selected = TRUE;
            w->selection = entry - 1;
        }
    }
    GtkWidget *popover = gtk_popover_new();
    GtkWidget *actions = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_start(actions, 9);
    gtk_widget_set_margin_end(actions, 9);
    gtk_widget_set_margin_top(actions, 9);
    gtk_widget_set_margin_bottom(actions, 9);
    gtk_popover_set_child(GTK_POPOVER(popover), actions);
    if (room) map_context_add_button(actions, w, "Open room editor", 1, x, y);
    map_context_add_button(actions, w, "Create project map...", 3, x, y);
    map_context_add_button(actions, w, "Copy cell coordinates", 2, x, y);
    gtk_widget_set_parent(popover, cell);
    GdkRectangle pointer = {(int)px, (int)py, 1, 1};
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &pointer);
    w->popup = popover;
    g_object_add_weak_pointer(G_OBJECT(popover), (gpointer *)&w->popup);
    gtk_popover_popup(GTK_POPOVER(popover));
}

static void map_context_enable(GtkWidget *cell, WorldGrid *w, guint x, guint y,
                               guint columns, guint rows)
{
    g_object_set_data(G_OBJECT(cell), "mv-cell-x", GUINT_TO_POINTER(x + 1));
    g_object_set_data(G_OBJECT(cell), "mv-cell-y", GUINT_TO_POINTER(y + 1));
    g_object_set_data(G_OBJECT(cell), "mv-span-width", GUINT_TO_POINTER(columns));
    g_object_set_data(G_OBJECT(cell), "mv-span-height", GUINT_TO_POINTER(rows));
    GtkGesture *right = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(right), GDK_BUTTON_SECONDARY);
    gtk_widget_add_controller(cell, GTK_EVENT_CONTROLLER(right));
    g_signal_connect(right, "pressed", G_CALLBACK(map_context_pressed), w);
}

/* Dedicated map-creation workspace shares the existing validated form with
 * both room browsers. Spatial placement gets its own future schema/adapter. */
static void map_creator_launch(GtkButton *button, gpointer userdata)
{
    GtkWidget *center = GTK_WIDGET(userdata);
    guint target = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(button), "mv-target-world"));
    guint n = gtk_notebook_get_n_pages(GTK_NOTEBOOK(center));
    for (guint i = 0; i < n; ++i) {
        GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(center), (gint)i);
        GtkWidget *create = g_object_get_data(G_OBJECT(page), "mv-create-room-action");
        guint world = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(page), "mv-world-mode"));
        if (!create || world != target) continue;
        gtk_notebook_set_current_page(GTK_NOTEBOOK(center), (gint)i);
        g_signal_emit_by_name(create, "clicked");
        return;
    }
    /* A room browser may be detached in another dock: no stale callback. */
    GtkWidget *tab = gtk_notebook_get_nth_page(GTK_NOTEBOOK(center),
        gtk_notebook_get_current_page(GTK_NOTEBOOK(center)));
    GtkWidget *hint = tab ? g_object_get_data(G_OBJECT(tab), "mv-map-creator-hint") : NULL;
    if (GTK_IS_LABEL(hint)) gtk_label_set_text(GTK_LABEL(hint),
        "Open the matching Zero rooms or Aria rooms browser first, then retry.");
}

static void grid_rebuild(WorldGrid *w);

static void placement_refresh(GtkButton *button, gpointer userdata)
{
    (void)button;
    GtkWidget *page = GTK_WIDGET(userdata);
    GtkWidget *choice = g_object_get_data(G_OBJECT(page), "mv-placement-choice");
    GtkWidget *status = g_object_get_data(G_OBJECT(page), "mv-placement-status");
    gchar *out = NULL, *err = NULL;
    GError *error = NULL;
    gint code = -1;
    gchar *args[] = {"python3", "-m", "scripts.authored_rooms", "list",
                     "--format", "tsv", NULL};
    gboolean ok = g_spawn_sync(NULL, args, NULL, G_SPAWN_SEARCH_PATH,
                               NULL, NULL, &out, &err, &code, &error);
    GtkStringList *names = gtk_string_list_new(NULL);
    if (ok && g_spawn_check_wait_status(code, NULL) && out) {
        gchar **rows = g_strsplit(out, "\n", -1);
        for (guint i = 0; rows[i] && i < 4096; ++i) {
            if (!*rows[i]) continue;
            gchar **parts = g_strsplit(rows[i], "\t", 5);
            if (g_strv_length(parts) == 4 && strlen(parts[0]) < 96)
                gtk_string_list_append(names, parts[0]);
            g_strfreev(parts);
        }
        g_strfreev(rows);
    }
    guint count = g_list_model_get_n_items(G_LIST_MODEL(names));
    gtk_drop_down_set_model(GTK_DROP_DOWN(choice), G_LIST_MODEL(names));
    if (count) gtk_drop_down_set_selected(GTK_DROP_DOWN(choice), 0);
    gtk_label_set_text(GTK_LABEL(status), error ? error->message :
        (ok && g_spawn_check_wait_status(code, NULL) ?
            (count ? "Select a draft, then place it on an empty original map cell."
                   : "No drafts yet. Create one using the buttons above, then refresh.") :
            (err && *err ? err : "Unable to list private drafts")));
    g_object_unref(names);
    g_clear_error(&error); g_free(out); g_free(err);
}

static void placement_save(GtkButton *button, gpointer userdata)
{
    (void)button;
    GtkWidget *page = GTK_WIDGET(userdata);
    GtkWidget *choice = g_object_get_data(G_OBJECT(page), "mv-placement-choice");
    GtkWidget *status = g_object_get_data(G_OBJECT(page), "mv-placement-status");
    GtkWidget *xspin = g_object_get_data(G_OBJECT(page), "mv-placement-x");
    GtkWidget *yspin = g_object_get_data(G_OBJECT(page), "mv-placement-y");
    GObject *item = gtk_drop_down_get_selected_item(GTK_DROP_DOWN(choice));
    if (!GTK_IS_STRING_OBJECT(item)) {
        gtk_label_set_text(GTK_LABEL(status), "No authored draft selected. Refresh the list.");
        return;
    }
    const char *id = gtk_string_object_get_string(GTK_STRING_OBJECT(item));
    gchar *x = g_strdup_printf("%d", gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(xspin)));
    gchar *y = g_strdup_printf("%d", gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(yspin)));
    gchar *args[] = {"python3", "-m", "scripts.map_placements", "place",
                     "--id", (gchar *)id, "--x", x, "--y", y, NULL};
    gchar *out = NULL, *err = NULL;
    GError *error = NULL;
    gint code = -1;
    gboolean ok = g_spawn_sync(NULL, args, NULL, G_SPAWN_SEARCH_PATH,
                               NULL, NULL, &out, &err, &code, &error);
    gboolean saved = ok && g_spawn_check_wait_status(code, NULL);
    gtk_label_set_text(GTK_LABEL(status), error ? error->message :
        (saved ? "Project placement saved privately. Original ROM untouched." :
                 (err && *err ? err : "Private placement failed")));
    if (saved) {
        GtkWidget *center = gtk_widget_get_ancestor(page, GTK_TYPE_NOTEBOOK);
        if (GTK_IS_NOTEBOOK(center)) {
            gint n = gtk_notebook_get_n_pages(GTK_NOTEBOOK(center));
            for (gint i = 0; i < n; ++i) {
                GtkWidget *candidate = gtk_notebook_get_nth_page(GTK_NOTEBOOK(center), i);
                WorldGrid *w = g_object_get_data(G_OBJECT(candidate), "mv-world-grid");
                if (w) {
                    reload_rows(w);
                    grid_rebuild(w);
                    break;
                }
            }
        }
    }
    g_free(x); g_free(y); g_free(out); g_free(err);
    g_clear_error(&error);
}

static void map_creator_build(GtkWidget *center)
{
    /* Global maps can be opened repeatedly in the same dock. Reuse the
     * existing authoring page instead of appending duplicate tabs. */
    guint total = gtk_notebook_get_n_pages(GTK_NOTEBOOK(center));
    for (guint i = 0; i < total; ++i) {
        GtkWidget *existing = gtk_notebook_get_nth_page(GTK_NOTEBOOK(center), (gint)i);
        if (g_object_get_data(G_OBJECT(existing), "mv-map-creator-hint")) return;
    }
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *title = gtk_label_new("Map creation / project-authored rooms");
    gtk_widget_add_css_class(title, "title-3");
    gtk_label_set_xalign(GTK_LABEL(title), 0);
    GtkWidget *help = gtk_label_new(
        "Create private room drafts for either game using the shared authoring "
        "form. Original global map cells stay read-only. Room collision, "
        "graphics and native export are not implemented. The placement of new drafts "
        "is stored separately from ROM data and does not make a room playable.");
    gtk_label_set_wrap(GTK_LABEL(help), TRUE);
    gtk_label_set_selectable(GTK_LABEL(help), TRUE);
    gtk_label_set_xalign(GTK_LABEL(help), 0);
    GtkWidget *hint = gtk_label_new(
        "Right-click any global map case to send its coordinates here. "
        "These coordinates can now be saved to the private project placement layer.");
    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_label_set_selectable(GTK_LABEL(hint), TRUE);
    gtk_label_set_xalign(GTK_LABEL(hint), 0);
    GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    const char *names[] = {"Create Zero Mission room...", "Create Aria room..."};
    for (guint i = 0; i < 2; ++i) {
        GtkWidget *button = gtk_button_new_with_label(names[i]);
        g_object_set_data(G_OBJECT(button), "mv-target-world", GUINT_TO_POINTER(i + 1));
        g_signal_connect(button, "clicked", G_CALLBACK(map_creator_launch), center);
        gtk_box_append(GTK_BOX(buttons), button);
    }
    gtk_widget_set_margin_start(page, 18);
    gtk_widget_set_margin_end(page, 18);
    gtk_widget_set_margin_top(page, 14);
    gtk_widget_set_margin_bottom(page, 14);
    gtk_box_append(GTK_BOX(page), title);
    gtk_box_append(GTK_BOX(page), help);
    gtk_box_append(GTK_BOX(page), hint);
    gtk_box_append(GTK_BOX(page), buttons);
    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *choice_label = gtk_label_new("Place existing draft on global map");
    GtkWidget *draft_choice = gtk_drop_down_new(NULL, NULL);
    GtkWidget *coordinate_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *xspin = gtk_spin_button_new_with_range(0, 63, 1);
    GtkWidget *yspin = gtk_spin_button_new_with_range(0, 34, 1);
    GtkWidget *refresh = gtk_button_new_with_label("Refresh private drafts");
    GtkWidget *save = gtk_button_new_with_label("Save project placement");
    GtkWidget *placement_status = gtk_label_new("Loading private project drafts...");
    gtk_label_set_wrap(GTK_LABEL(placement_status), TRUE);
    gtk_label_set_selectable(GTK_LABEL(placement_status), TRUE);
    gtk_label_set_xalign(GTK_LABEL(placement_status), 0);
    gtk_label_set_xalign(GTK_LABEL(choice_label), 0);
    gtk_box_append(GTK_BOX(coordinate_row), gtk_label_new("X:"));
    gtk_box_append(GTK_BOX(coordinate_row), xspin);
    gtk_box_append(GTK_BOX(coordinate_row), gtk_label_new("Y:"));
    gtk_box_append(GTK_BOX(coordinate_row), yspin);
    gtk_box_append(GTK_BOX(coordinate_row), save);
    gtk_box_append(GTK_BOX(page), separator);
    gtk_box_append(GTK_BOX(page), choice_label);
    gtk_box_append(GTK_BOX(page), draft_choice);
    gtk_box_append(GTK_BOX(page), refresh);
    gtk_box_append(GTK_BOX(page), coordinate_row);
    gtk_box_append(GTK_BOX(page), placement_status);
    g_object_set_data(G_OBJECT(page), "mv-placement-choice", draft_choice);
    g_object_set_data(G_OBJECT(page), "mv-placement-x", xspin);
    g_object_set_data(G_OBJECT(page), "mv-placement-y", yspin);
    g_object_set_data(G_OBJECT(page), "mv-placement-status", placement_status);
    g_signal_connect(refresh, "clicked", G_CALLBACK(placement_refresh), page);
    g_signal_connect(save, "clicked", G_CALLBACK(placement_save), page);
    g_object_set_data(G_OBJECT(page), "mv-map-creator-hint", hint);
    gtk_notebook_append_page(GTK_NOTEBOOK(center), page, gtk_label_new("Map creation"));
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(center), page, TRUE);
    gtk_notebook_set_tab_detachable(GTK_NOTEBOOK(center), page, TRUE);
    placement_refresh(NULL, page);
}

static void grid_rebuild(WorldGrid *w)
{
    GtkWidget *child;
    map_popover_close(w);
    w->selected_cell = NULL;
    while ((child = gtk_widget_get_first_child(w->grid)))
        gtk_grid_remove(GTK_GRID(w->grid), child);
    guint xmax = (w->world && w->area == G_MAXUINT) ? 63u : 0u;
    guint ymax = (w->world && w->area == G_MAXUINT) ? 34u : 0u;
    guint total = 0, anchor_only = 0;
    for (guint i = 0; i < w->cells->len; ++i) {
        const MapCell *c = &g_array_index(w->cells, MapCell, i);
        if (w->area != G_MAXUINT && c->area != w->area) continue;
        xmax = MAX(xmax, c->x);
        ymax = MAX(ymax, c->y);
        ++total;
        if (c->provenance == 0) ++anchor_only;
    }
    if (!total) {
        gtk_label_set_text(GTK_LABEL(w->status),
            "No original map cells available. Import the local ROM first.");
        return;
    }
    if (xmax > 90 || ymax > 90) {
        gtk_label_set_text(GTK_LABEL(w->status),
            "Original map bounds exceed safe preview limits.");
        return;
    }
    guint stride = xmax + 1;
    guint count = stride * (ymax + 1);
    guint *lookup = g_new0(guint, count);
    gboolean *covered = g_new0(gboolean, count);
    guint overlapping = 0, groups = 0, thumbs = 0;
    for (guint i = 0; i < w->cells->len; ++i) {
        const MapCell *c = &g_array_index(w->cells, MapCell, i);
        if (w->area != G_MAXUINT && c->area != w->area) continue;
        guint pos = c->y * stride + c->x;
        if (lookup[pos]) {
            ++overlapping; /* Never silently fabricate stacked room ownership. */
            continue;
        }
        lookup[pos] = i + 1;
    }
    /* Partition each room's occupied cells into exact, non-overlapping
     * rectangles. A large 2x3 room becomes ONE GtkGrid child spanning 2x3,
     * while L/T-shaped rooms are split without filling nonexistent cells. */
    for (guint y = 0; y <= ymax; ++y) {
        for (guint x = 0; x <= xmax; ++x) {
            guint pos = y * stride + x;
            if (covered[pos]) continue;
            guint entry = lookup[pos];
            if (!entry) {
                GtkWidget *empty = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
                gtk_widget_add_css_class(empty, "mv-map-cell");
                gtk_widget_set_size_request(empty, (int)w->size, (int)w->size);
                map_context_enable(empty, w, x, y, 1, 1);
                gtk_grid_attach(GTK_GRID(w->grid), empty, (int)x, (int)y, 1, 1);
                covered[pos] = TRUE;
                continue;
            }
            const MapCell *c = &g_array_index(w->cells, MapCell, entry - 1);
            guint width = 1, height = 1;
            for (guint xx = x + 1; c->provenance != 3 && xx <= xmax; ++xx) {
                guint spot = y * stride + xx, next = lookup[spot];
                if (covered[spot] || !next) break;
                const MapCell *other = &g_array_index(w->cells, MapCell, next - 1);
                if (other->room != c->room || other->area != c->area ||
                    other->provenance != c->provenance ||
                    (c->provenance == 4 && strcmp(c->draft_id, other->draft_id) != 0)) break;
                ++width;
            }
            for (guint yy = y + 1; c->provenance != 3 && yy <= ymax; ++yy) {
                gboolean full = TRUE;
                for (guint xx = x; xx < x + width; ++xx) {
                    guint spot = yy * stride + xx, next = lookup[spot];
                    if (covered[spot] || !next) { full = FALSE; break; }
                    const MapCell *other = &g_array_index(w->cells, MapCell, next - 1);
                    if (other->room != c->room || other->area != c->area ||
                        other->provenance != c->provenance ||
                        (c->provenance == 4 && strcmp(c->draft_id, other->draft_id) != 0)) { full = FALSE; break; }
                }
                if (!full) break;
                ++height;
            }
            gboolean save = FALSE, warp = FALSE;
            for (guint yy = y; yy < y + height; ++yy)
                for (guint xx = x; xx < x + width; ++xx) {
                    guint spot = yy * stride + xx;
                    const MapCell *part = &g_array_index(w->cells, MapCell, lookup[spot] - 1);
                    save |= part->save != 0;
                    warp |= part->warp != 0;
                    covered[spot] = TRUE;
                }
            GtkWidget *cell = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
            gtk_widget_add_css_class(cell, "mv-map-cell");
            gtk_widget_add_css_class(cell, "mv-occupied");
            gtk_widget_add_css_class(cell, "mv-room-footprint");
            if (c->provenance == 0) gtk_widget_add_css_class(cell, "mv-anchor-only");
            if (c->provenance == 3) gtk_widget_add_css_class(cell, "mv-native-minimap");
            if (c->provenance == 4) gtk_widget_add_css_class(cell, "mv-project-draft");
            if (save) gtk_widget_add_css_class(cell, "mv-save");
            if (warp) gtk_widget_add_css_class(cell, "mv-warp");
            if (w->selected && w->selection < w->cells->len) {
                const MapCell *selected = &g_array_index(w->cells, MapCell, w->selection);
                if (selected->area == c->area && selected->room == c->room) {
                    gtk_widget_add_css_class(cell, "mv-selected");
                    w->selected_cell = cell;
                }
            }
            gtk_widget_set_size_request(cell, (int)(w->size * width),
                                        (int)(w->size * height));
            guint room_cells = room_cells_in_area(w, c->area, c->room);
            /* A single full-room preview is meaningful only when the exact
             * occupied rectangle is complete. No duplicated whole-room images
             * on disconnected segments or unverified MZM anchor markers. */
            char path[256];
            if (c->provenance != 0 && c->provenance != 3 && c->provenance != 4 &&
                room_cells == width * height &&
                has_image(w, c, path, sizeof(path))) {
                GtkWidget *picture = gtk_picture_new_for_filename(path);
                gtk_picture_set_can_shrink(GTK_PICTURE(picture), TRUE);
                gtk_widget_set_size_request(picture, (int)(w->size * width) - 4,
                                             (int)(w->size * height) - 4);
                gtk_box_append(GTK_BOX(cell), picture);
                ++thumbs;
            } else {
                gchar *name = c->provenance == 4 ? g_strdup("DRAFT") :
                    c->provenance == 3 ? g_strdup("") :
                    g_strdup_printf("%u%s", c->room,
                        room_cells > width * height ? " …" : "");
                GtkWidget *label = gtk_label_new(name);
                gtk_widget_set_halign(label, GTK_ALIGN_CENTER);
                gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
                gtk_widget_set_vexpand(label, TRUE);
                gtk_box_append(GTK_BOX(cell), label);
                g_free(name);
            }
            gchar *tip = c->provenance == 4 ? g_strdup_printf(
                "PROJECT DRAFT: %s (not playable or ROM-backed)", c->draft_id) :
                g_strdup_printf("Room %u: %u verified case(s); %ux%u displayed segment; %s%s%s",
                c->room, room_cells, width, height,
                c->provenance == 2 ? "original Aria map" :
                c->provenance == 3 ? "original MZM minimap tile (room unknown)" :
                c->provenance == 1 ? "native MZM scroll/Clipdata and minimap intersection" : "MZM anchor only",
                c->ownership[0] ? "; ownership: " : "", c->ownership);
            gtk_widget_set_tooltip_text(cell, tip);
            g_free(tip);
            g_object_set_data(G_OBJECT(cell), "mv-grid-index", GUINT_TO_POINTER(entry));
            map_context_enable(cell, w, x, y, width, height);
            GtkGesture *click = gtk_gesture_click_new();
            gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), GDK_BUTTON_PRIMARY);
            gtk_widget_add_controller(cell, GTK_EVENT_CONTROLLER(click));
            g_signal_connect(click, "pressed", G_CALLBACK(grid_clicked), w);
            gtk_grid_attach(GTK_GRID(w->grid), cell, (int)x, (int)y,
                            (int)width, (int)height);
            ++groups;
        }
    }
    g_free(covered);
    g_free(lookup);
    gchar *message = g_strdup_printf(
        "%s / %s | %u occupied case(s), %u room segments, %u previews; "
        "%u anchor-only case(s), %u overlapping source coordinates. "
        "Multi-case rooms retain their actual positions. Double-click to edit.",
        worlds[w->world], w->area == G_MAXUINT ? "All castle areas" :
        area_name(w, w->area), total, groups, thumbs,
        anchor_only, overlapping);
    gtk_label_set_text(GTK_LABEL(w->status), message);
    g_free(message);
}
static void begin_generation(WorldGrid *w);

static void generator_finished(GObject *object, GAsyncResult *result, gpointer userdata)
{
    GtkWidget *page = GTK_WIDGET(userdata);
    WorldGrid *w = g_object_get_data(G_OBJECT(page), "mv-world-grid");
    GSubprocess *proc = G_SUBPROCESS(object);
    GError *error = NULL;
    gchar *out = NULL, *err = NULL;
    gboolean ok = g_subprocess_communicate_utf8_finish(proc, result, &out, &err, &error);
    guint generated_world = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(proc), "mv-world"));
    guint generated_budget = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(proc), "mv-budget"));
    gboolean success = ok && g_subprocess_get_successful(proc);
    if (w) {
        w->busy = FALSE;
        if (generated_world == w->world) {
            if (success) {
                if (reload_rows(w)) grid_rebuild(w);
                else gtk_label_set_text(GTK_LABEL(w->status),
                    "No verified cells for this world. Check the local catalog.");
            } else {
                gchar *diagnostic = g_strdup_printf(
                    "Map generation failed: %.2048s",
                    error ? error->message : (err && *err ? err : "Unknown index/decoder failure"));
                gtk_label_set_text(GTK_LABEL(w->status), diagnostic);
                g_free(diagnostic);
            }
        }
        if (w->pending_generation) {
            w->pending_generation = FALSE;
            begin_generation(w);
        } else if (success && generated_world == w->world && w->prefetching) {
            /* First index, then process a bounded preview batch per area.
             * Do not block the UI on the full world rendering. */
            if (generated_budget != 0) ++w->next_preview_area;
            if (w->next_preview_area < area_count(w)) begin_generation(w);
            else w->prefetching = FALSE;
        }
    }
    g_clear_error(&error);
    g_free(out);
    g_free(err);
    g_object_unref(page);
}

static void begin_generation(WorldGrid *w)
{
    if (w->busy) { w->pending_generation = TRUE; return; }
    guint active_area = w->prefetching ? w->next_preview_area :
        (w->area == G_MAXUINT ? 0u : w->area);
    gchar *area = g_strdup_printf("%u", active_area);
    char index_path[128];
    snprintf(index_path, sizeof(index_path), "assets/extracted/world_overview/%s.tsv",
             world_code(w));
    /* Missing index: budget zero skips slow graphics and publishes cells first. */
    guint budget = g_file_test(index_path, G_FILE_TEST_IS_REGULAR) ? 6u : 0u;
    gchar *budget_arg = g_strdup_printf("%u", budget);
    GError *error = NULL;
    GSubprocess *proc = g_subprocess_new(
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE,
        &error, "python3", "-m", "scripts.world_overview",
        "--world", world_code(w), "--area", area, "--budget", budget_arg, NULL);
    g_free(area);
    g_free(budget_arg);
    if (!proc) {
        gtk_label_set_text(GTK_LABEL(w->status),
            error ? error->message : "Unable to start native map generator");
        g_clear_error(&error);
        return;
    }
    w->busy = TRUE;
    if (budget == 0)
        gtk_label_set_text(GTK_LABEL(w->status), "Indexing verified original map cells...");
    g_object_set_data(G_OBJECT(proc), "mv-world", GUINT_TO_POINTER(w->world));
    g_object_set_data(G_OBJECT(proc), "mv-budget", GUINT_TO_POINTER(budget));
    g_subprocess_communicate_utf8_async(proc, NULL, NULL,
                                        generator_finished, g_object_ref(w->page));
    g_object_unref(proc);
}
static void world_changed(GObject *object,GParamSpec *pspec,gpointer userdata)
{
    WorldGrid *w=userdata;(void)pspec;
    w->world=gtk_drop_down_get_selected(GTK_DROP_DOWN(object));
    if(w->world>1)return;
    w->changing_world=TRUE;
    GtkStringList *list=gtk_string_list_new(NULL);
    if (w->world) gtk_string_list_append(list, "All areas (original castle)");
    for(guint i=0;i<area_count(w);++i)gtk_string_list_append(list,area_name(w,i));
    gtk_drop_down_set_model(GTK_DROP_DOWN(w->area_select),G_LIST_MODEL(list));
    g_object_unref(list);
    w->area=w->world ? G_MAXUINT : 0u;
    w->prefetching=TRUE;
    w->next_preview_area=0;
    w->selected=FALSE;
    gtk_drop_down_set_selected(GTK_DROP_DOWN(w->area_select),0);
    w->changing_world=FALSE;
    g_object_set_data(G_OBJECT(w->page),"mv-world-mode",GINT_TO_POINTER(w->world?2:1));
    if(w->world_badge)gtk_label_set_text(GTK_LABEL(w->world_badge),
        w->world?"● ARIA OF SORROW":"● METROID: ZERO MISSION");
    if (reload_rows(w)) grid_rebuild(w);
    else {
        GtkWidget *child;
        w->selected_cell = NULL;
        while ((child = gtk_widget_get_first_child(w->grid)))
            gtk_grid_remove(GTK_GRID(w->grid), child);
        gtk_label_set_text(GTK_LABEL(w->status), "Generating original map cases…");
    }
    begin_generation(w);
}
static void area_changed(GObject *object,GParamSpec *pspec,gpointer userdata)
{
    WorldGrid *w=userdata;(void)pspec;
    if(w->changing_world)return;
    guint selected=gtk_drop_down_get_selected(GTK_DROP_DOWN(object));
    guint area = w->world ? (selected == 0 ? G_MAXUINT : selected - 1) : selected;
    if(area != G_MAXUINT && area>=area_count(w))return;
    w->area=area;w->selected=FALSE;
    if (reload_rows(w)) grid_rebuild(w);
    else {
        GtkWidget *child;
        w->selected_cell = NULL;
        while ((child = gtk_widget_get_first_child(w->grid)))
            gtk_grid_remove(GTK_GRID(w->grid), child);
        gtk_label_set_text(GTK_LABEL(w->status), "Generating original map cases…");
    }
    begin_generation(w);
}
static void zoom_changed(GtkSpinButton *spin,gpointer data)
{WorldGrid *w=data;w->size=(guint)gtk_spin_button_get_value_as_int(spin);grid_rebuild(w);}
static void generate_clicked(GtkButton *b,gpointer data)
{
    WorldGrid *w=data;(void)b;
    w->prefetching=TRUE;
    w->next_preview_area=0;
    begin_generation(w);
}
static void world_free(gpointer data)
{
    WorldGrid *w = data;
    if (w->pan_tick && w->scroller)
        gtk_widget_remove_tick_callback(w->scroller, w->pan_tick);
    if (w->popup)
        g_object_remove_weak_pointer(G_OBJECT(w->popup), (gpointer *)&w->popup);
    g_array_free(w->cells, TRUE);
    g_free(w);
}
GtkWidget *world_atlas_build(GtkWidget *center,NativeWorkspace *workspace,GtkWidget *indicator)
{
    static const char css[]={
        ".mv-map-cell{background:#101923;border:1px solid #293743;}"
        ".mv-occupied{background:#345968;border:1px solid #6c9da6;}"
        ".mv-save{border:2px solid #71d0a3;}"
        ".mv-warp{border:2px solid #b7a1eb;}"
        ".mv-selected{border:3px solid #f4cc58;}"
        ".mv-room-footprint{border:2px solid #91b8c7;}"
        ".mv-anchor-only{border:2px dashed #d7a564;}"
        ".mv-native-minimap{background:#4a7881;border:1px solid #7cacb6;}"
        ".mv-project-draft{background:#425052;border:2px dashed #f4cc58;}"};
    GtkCssProvider *provider=gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider,css);
    GdkDisplay *display=gdk_display_get_default();
    if(display)gtk_style_context_add_provider_for_display(display,GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
    WorldGrid *w=g_new0(WorldGrid,1);
    w->workspace=workspace;w->world_badge=indicator;w->cells=g_array_new(FALSE,FALSE,sizeof(MapCell));
    w->size=28;
    w->prefetching=TRUE;
    GtkWidget *root=gtk_box_new(GTK_ORIENTATION_VERTICAL,6);
    GtkWidget *bar=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,6);
    GtkWidget *scroller=gtk_scrolled_window_new();
    GtkWidget *grid=gtk_grid_new();
    /* A 32x32 Zero map can fit inside the viewport, leaving nothing to drag.
     * Padding belongs to a surrounding canvas, NOT to the native cell grid:
     * room coordinates and per-cell dimensions must remain unchanged. */
    GtkWidget *pan_surface=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
    GtkWidget *canvas_row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
    GtkWidget *right_space=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
    GtkWidget *bottom_space=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
    gtk_widget_set_size_request(right_space,720,-1);
    gtk_widget_set_size_request(bottom_space,-1,520);
    gtk_widget_set_halign(grid,GTK_ALIGN_START);
    gtk_widget_set_valign(grid,GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(canvas_row),grid);
    gtk_box_append(GTK_BOX(canvas_row),right_space);
    gtk_box_append(GTK_BOX(pan_surface),canvas_row);
    gtk_box_append(GTK_BOX(pan_surface),bottom_space);
    gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
    GtkWidget *generate=gtk_button_new_with_label("Generate more original previews");
    GtkWidget *open=gtk_button_new_with_label("Open selected room");
    w->page=root;w->grid=grid;w->scroller=scroller;
    GtkGesture *pan = gtk_gesture_drag_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(pan), GDK_BUTTON_PRIMARY);
    gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(pan), GTK_PHASE_CAPTURE);
    /* The scroller is stationary while its child moves, so drag offsets remain
     * stable. A gesture attached to the moving map caused feedback jitter. */
    gtk_widget_add_controller(scroller, GTK_EVENT_CONTROLLER(pan));
    g_object_set_data(G_OBJECT(root), "mv-map-pan-surface", pan_surface);
    g_signal_connect(pan, "drag-begin", G_CALLBACK(map_drag_begin), w);
    g_signal_connect(pan, "drag-update", G_CALLBACK(map_drag_update), w);
    GtkEventController *wheel = gtk_event_controller_scroll_new(
        GTK_EVENT_CONTROLLER_SCROLL_VERTICAL | GTK_EVENT_CONTROLLER_SCROLL_DISCRETE);
    gtk_event_controller_set_propagation_phase(wheel, GTK_PHASE_CAPTURE);
    gtk_widget_add_controller(scroller, wheel);
    g_signal_connect(wheel, "scroll", G_CALLBACK(map_scroll), w);
    w->status=gtk_label_new("Reading verified original minimap cases…");
    w->details=gtk_label_new("Double-click a case to edit it in the shared room editor.");
    w->world_select=gtk_drop_down_new_from_strings(worlds);
    w->area_select=gtk_drop_down_new_from_strings(mzm_areas);
    w->zoom=gtk_spin_button_new_with_range(22,64,6);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->zoom),28);
    gtk_box_append(GTK_BOX(bar),gtk_label_new("WORLD:"));
    gtk_box_append(GTK_BOX(bar),w->world_select);
    gtk_box_append(GTK_BOX(bar),gtk_label_new("Area:"));
    gtk_box_append(GTK_BOX(bar),w->area_select);
    gtk_box_append(GTK_BOX(bar),gtk_label_new("Case size:"));
    gtk_box_append(GTK_BOX(bar),w->zoom);
    gtk_box_append(GTK_BOX(bar),generate);
    gtk_box_append(GTK_BOX(bar),open);
    gtk_box_append(GTK_BOX(root),bar);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller),pan_surface);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(scroller,TRUE);gtk_widget_set_vexpand(scroller,TRUE);
    gtk_box_append(GTK_BOX(root),scroller);
    gtk_label_set_wrap(GTK_LABEL(w->details),TRUE);
    gtk_label_set_selectable(GTK_LABEL(w->details), TRUE);
    gtk_label_set_wrap(GTK_LABEL(w->status),TRUE);
    gtk_label_set_selectable(GTK_LABEL(w->status), TRUE);
    gtk_label_set_xalign(GTK_LABEL(w->details),0);
    gtk_label_set_xalign(GTK_LABEL(w->status),0);
    gtk_box_append(GTK_BOX(root),w->details);
    gtk_box_append(GTK_BOX(root),w->status);
    g_object_set_data_full(G_OBJECT(root),"mv-world-grid",w,world_free);
    g_object_set_data(G_OBJECT(root),"mv-world-mode",GINT_TO_POINTER(1));
    gtk_notebook_append_page(GTK_NOTEBOOK(center),root,gtk_label_new("Global maps"));
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(center),root,TRUE);
    gtk_notebook_set_tab_detachable(GTK_NOTEBOOK(center),root,TRUE);
    map_creator_build(center);
    g_signal_connect(w->world_select,"notify::selected",G_CALLBACK(world_changed),w);
    g_signal_connect(w->area_select,"notify::selected",G_CALLBACK(area_changed),w);
    g_signal_connect(w->zoom,"value-changed",G_CALLBACK(zoom_changed),w);
    g_signal_connect(generate,"clicked",G_CALLBACK(generate_clicked),w);
    g_signal_connect(open,"clicked",G_CALLBACK(selected_open_click),w);
    if(reload_rows(w))grid_rebuild(w);
    begin_generation(w);
    return root;
}
