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
    guint area, room, index, x, y;
    guint target_room, target_door; /* G_MAXUINT = unverified connection. */
    char type[96];
} NativeMapDoor;
typedef struct {
    guint area, room, door, target_world, target_area, target_room, target_door;
    guint reciprocal;
    char state[24];
} ProjectMapEdge0116;

typedef struct {
    GArray *cells;
    NativeWorkspace *workspace;
    GtkWidget *page, *grid, *map_background, *details, *status;
    GtkWidget *world_select, *area_select, *zoom;
    GtkWidget *doors_toggle, *door_expander, *door_list;
    GtkWidget *connections_toggle, *connections_all, *connection_list;
    GtkWidget *connection_expander, *connection_canvas;
    GArray *connection_edges; /* verified, SAVED project overrides only */
    GtkWidget *pick_bar;
    WorldAtlasRoomPicked pick_callback;
    gpointer pick_data;
    GtkWidget *selected_cell, *scroller, *popup;
    double drag_hstart, drag_vstart, pending_h, pending_v;
    guint pan_tick;
    GtkWidget *world_badge;
    guint world, area;
    guint selection;
    gboolean selected, busy, pending_generation, changing_world, prefetching;
    guint next_preview_area;
    guint size, grid_columns, grid_rows;
} WorldGrid;
static const char *const worlds[]={"Zero Mission", "Aria of Sorrow", NULL};
static const char *const mzm_areas[]={"Brinstar", "Kraid", "Norfair", "Ridley", "Tourian", "Crateria", "Chozodia", NULL};
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
    gchar *argv[] = {"python3", "scripts/editor_cli.py",
                     "--command=placement-list", "--world",
                     w->world ? "aria" : "zero_mission", "--format=tsv", NULL};
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

/* One rectangle per consecutive room footprint, just like the Aria atlas.
 * MZM 0/1 are both verified room anchors/ownership; unowned native cells
 * and different project drafts must never be silently fused. */
static gboolean same_atlas_group(const WorldGrid *w, const MapCell *a, const MapCell *b)
{
    if (!a || !b || a->area != b->area || a->room != b->room || a->room == 999)
        return FALSE;
    if (a->provenance == 3 || b->provenance == 3) return FALSE;
    if (a->provenance == 4 || b->provenance == 4)
        return a->provenance == 4 && b->provenance == 4 &&
               strcmp(a->draft_id, b->draft_id) == 0;
    if (!w->world)
        return a->provenance <= 1 && b->provenance <= 1;
    return a->provenance == b->provenance;
}

static void selected_room_doors_refresh(WorldGrid *w);
static void connections_refresh_0116(WorldGrid *w);
static void world_atlas_pick_finish_0106(WorldGrid *w, gboolean selected,
                                          guint area, guint room);

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
    selected_room_doors_refresh(w);
    connections_refresh_0116(w);
    if (w->pick_callback) {
        /* Only original cells with a verified native room are selectable.
         * A double click must NOT open a room during target picking. */
        world_atlas_pick_finish_0106(w, TRUE, c.area, c.room);
        return;
    }
    if (presses >= 2) selected_open(w);
}
static gboolean has_image(const WorldGrid *w,const MapCell *c,char *dest,size_t n)
{
    if(w->world)
        snprintf(dest,n,"assets/extracted/rooms/aria/previews/area_%02u_room_%03u_composite.bmp",c->area,c->room);
    else
        snprintf(dest,n,"assets/extracted/world_overview/mzm_cells/area_%02u_room_%03u_x_%02u_y_%02u.bmp",
                 c->area,c->room,c->x,c->y);
    return g_file_test(dest,G_FILE_TEST_IS_REGULAR);
}

static GArray *load_mzm_doors(void)
{
    GArray *doors = g_array_new(FALSE, FALSE, sizeof(NativeMapDoor));
    gchar *contents = NULL;
    if (!g_file_get_contents("assets/extracted/world_overview/mzm_doors.tsv",
                             &contents, NULL, NULL)) return doors;
    gchar **lines = g_strsplit(contents, "\n", -1);
    for (guint i = 0; lines[i] && i < 2048; ++i) {
        if (!*lines[i] || *lines[i] == '#') continue;
        gchar **fields = g_strsplit(lines[i], "|", 9);
        guint n = g_strv_length(fields);
        NativeMapDoor door = {0};
        door.target_room = G_MAXUINT;
        door.target_door = G_MAXUINT;
        if ((n == 6 || n == 8) &&
            parse_uint(fields[0], &door.area) && door.area < 7 &&
            parse_uint(fields[1], &door.room) && door.room < 1000 &&
            parse_uint(fields[2], &door.index) && door.index < 4096 &&
            parse_uint(fields[3], &door.x) && door.x < 32 &&
            parse_uint(fields[4], &door.y) && door.y < 32 &&
            strlen(fields[5]) < sizeof(door.type) &&
            (n != 8 ||
             ((strcmp(fields[6], "-") == 0 && strcmp(fields[7], "-") == 0) ||
              (parse_uint(fields[6], &door.target_room) && door.target_room < 1000 &&
               parse_uint(fields[7], &door.target_door) && door.target_door < 4096)))) {
            g_strlcpy(door.type, fields[5], sizeof(door.type));
            g_array_append_val(doors, door);
        }
        g_strfreev(fields);
    }
    g_strfreev(lines);
    g_free(contents);
    return doors;
}

/* Native MZM connections are read-only. Do not turn event/area exits
 * into fictional destinations. Open only targets verified in the door table. */
static void map_door_target_open(GtkButton *button, gpointer data)
{
    WorldGrid *w = data;
    guint a = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(button), "mv-target-area"));
    guint r = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(button), "mv-target-room"));
    if (w->world || a == 0 || a > 7 || r == 0 || r > 1000) return;
    native_workspace_import_async(w->workspace, mzm_areas[a - 1], r - 1);
}

static void selected_room_doors_refresh(WorldGrid *w)
{
    if (!w->door_list || !w->door_expander) return;
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(w->door_list)))
        gtk_box_remove(GTK_BOX(w->door_list), child);
    if (w->world || !w->selected || w->selection >= w->cells->len) {
        gtk_expander_set_expanded(GTK_EXPANDER(w->door_expander), FALSE);
        return;
    }
    const MapCell *room = &g_array_index(w->cells, MapCell, w->selection);
    if (room->room >= 998 || room->area >= 7 || room->provenance >= 3) {
        gtk_expander_set_expanded(GTK_EXPANDER(w->door_expander), FALSE);
        return;
    }
    GArray *doors = load_mzm_doors();
    guint found = 0;
    for (guint i = 0; i < doors->len; ++i) {
        const NativeMapDoor *door = &g_array_index(doors, NativeMapDoor, i);
        if (door->area != room->area || door->room != room->room) continue;
        GtkWidget *line = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gchar *description = door->target_room == G_MAXUINT ?
            g_strdup_printf("D%u at (%u,%u) - destination unknown", door->index,
                            door->x, door->y) :
            g_strdup_printf("D%u at (%u,%u) -> room %03u / D%u", door->index,
                            door->x, door->y, door->target_room, door->target_door);
        GtkWidget *label = gtk_label_new(description);
        gtk_label_set_xalign(GTK_LABEL(label), 0);
        gtk_widget_set_hexpand(label, TRUE);
        gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
        gtk_widget_set_tooltip_text(label, door->type);
        gtk_box_append(GTK_BOX(line), label);
        if (door->target_room != G_MAXUINT) {
            GtkWidget *open = gtk_button_new_with_label("Open target");
            g_object_set_data(G_OBJECT(open), "mv-target-area",
                              GUINT_TO_POINTER(door->area + 1));
            g_object_set_data(G_OBJECT(open), "mv-target-room",
                              GUINT_TO_POINTER(door->target_room + 1));
            g_signal_connect(open, "clicked", G_CALLBACK(map_door_target_open), w);
            gtk_box_append(GTK_BOX(line), open);
        }
        gtk_box_append(GTK_BOX(w->door_list), line);
        g_free(description);
        ++found;
    }
    if (!found) gtk_box_append(GTK_BOX(w->door_list),
                              gtk_label_new("No native doors in this room."));
    gchar *heading = g_strdup_printf("Native doors / %s / room %03u (%u)",
                                     mzm_areas[room->area], room->room, found);
    gtk_expander_set_label(GTK_EXPANDER(w->door_expander), heading);
    gtk_expander_set_expanded(GTK_EXPANDER(w->door_expander), TRUE);
    g_free(heading);
    g_array_free(doors, TRUE);
}

static void map_cell_door_badge(GtkWidget *cell, const GArray *doors, const MapCell *c)
{
    guint count = 0, first = 0;
    GString *tip = g_string_new("Native Zero Mission doors: ");
    for (guint i = 0; i < doors->len; ++i) {
        const NativeMapDoor *door = &g_array_index(doors, NativeMapDoor, i);
        if (door->area != c->area || door->x != c->x || door->y != c->y) continue;
        if (!count) first = door->index;
        if (count) g_string_append(tip, "; ");
        if (door->target_room == G_MAXUINT)
            g_string_append_printf(tip, "D%u (room %u; target unknown; %s)",
                                   door->index, door->room, door->type);
        else
            g_string_append_printf(tip, "D%u (room %u -> room %u / D%u; %s)",
                                   door->index, door->room, door->target_room,
                                   door->target_door, door->type);
        ++count;
    }
    if (count) {
        gchar *title = count == 1 ? g_strdup_printf("D%u", first) :
                        g_strdup_printf("D×%u", count);
        GtkWidget *badge = gtk_label_new(title);
        gtk_widget_add_css_class(badge, "mv-native-door-badge");
        gtk_widget_set_halign(badge, GTK_ALIGN_END);
        gtk_widget_set_valign(badge, GTK_ALIGN_START);
        gtk_widget_set_tooltip_text(badge, tip->str);
        gtk_overlay_add_overlay(GTK_OVERLAY(cell), badge);
        g_free(title);
    }
    g_string_free(tip, TRUE);
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

static void map_background_draw_0117(GtkDrawingArea *area, cairo_t *cr,
                                     int width, int height, gpointer userdata)
{
    WorldGrid *w = userdata;
    (void)area; (void)width; (void)height;
    cairo_set_source_rgb(cr, 0.063, 0.098, 0.137);
    cairo_paint(cr);
    cairo_set_source_rgb(cr, 0.16, 0.216, 0.263);
    cairo_set_line_width(cr, 1.0);
    for (guint x = 0; x <= w->grid_columns; ++x) {
        cairo_move_to(cr, x * w->size + 0.5, 0);
        cairo_line_to(cr, x * w->size + 0.5, w->grid_rows * w->size);
    }
    for (guint y = 0; y <= w->grid_rows; ++y) {
        cairo_move_to(cr, 0, y * w->size + 0.5);
        cairo_line_to(cr, w->grid_columns * w->size, y * w->size + 0.5);
    }
    cairo_stroke(cr);
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

/* PATCH_0116_CONNECTION_VISUALIZATION: saved project metadata ONLY.
 * No native door guesses and no writes to ROMs, staging, or overrides. */
static gboolean connection_center_0116(const WorldGrid *w, guint area, guint room,
                                       double *x, double *y)
{
    double sumx = 0, sumy = 0;
    guint count = 0;
    for (guint i = 0; i < w->cells->len; ++i) {
        const MapCell *c = &g_array_index(w->cells, MapCell, i);
        if (c->area != area || c->room != room || c->room >= 998 ||
            c->provenance >= 3 || (w->area != G_MAXUINT && c->area != w->area))
            continue;
        sumx += c->x + 0.5;
        sumy += c->y + 0.5;
        ++count;
    }
    if (!count) return FALSE;
    *x = sumx / count * w->size;
    *y = sumy / count * w->size;
    return TRUE;
}

static void connection_draw_0116(GtkDrawingArea *area, cairo_t *cr,
                                 int width, int height, gpointer userdata)
{
    (void)area; (void)width; (void)height;
    WorldGrid *w = userdata;
    if (!w->connection_edges || !gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(w->connections_toggle))) return;
    gboolean show_all = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->connections_all));
    guint selected_area = G_MAXUINT, selected_room = G_MAXUINT;
    if (w->selected && w->selection < w->cells->len) {
        const MapCell *c = &g_array_index(w->cells, MapCell, w->selection);
        selected_area = c->area;
        selected_room = c->room;
    }
    for (guint i = 0; i < w->connection_edges->len; ++i) {
        const ProjectMapEdge0116 *edge = &g_array_index(
            w->connection_edges, ProjectMapEdge0116, i);
        if (!show_all && (edge->area != selected_area || edge->room != selected_room))
            continue;
        double x1, y1, x2, y2;
        if (!connection_center_0116(w, edge->area, edge->room, &x1, &y1)) continue;
        gboolean valid = strcmp(edge->state, "invalid") != 0 &&
                          strcmp(edge->state, "missing") != 0;
        gboolean same_map = edge->target_world == w->world &&
            (w->area == G_MAXUINT || edge->target_area == w->area);
        gboolean has_target = valid && same_map &&
            connection_center_0116(w, edge->target_area, edge->target_room, &x2, &y2);
        cairo_save(cr);
        if (!valid) cairo_set_source_rgb(cr, 0.96, 0.35, 0.38);
        else if (edge->target_world != w->world) cairo_set_source_rgb(cr, 0.96, 0.71, 0.34);
        else if (edge->reciprocal) cairo_set_source_rgb(cr, 0.35, 0.83, 0.57);
        else cairo_set_source_rgb(cr, 0.94, 0.71, 0.38);
        cairo_set_line_width(cr, 2.5);
        if (!edge->reciprocal || !valid) {
            const double dash[] = {5.0, 4.0};
            cairo_set_dash(cr, dash, 2, 0);
        }
        if (has_target && (x1 != x2 || y1 != y2)) {
            cairo_move_to(cr, x1, y1);
            cairo_curve_to(cr, x1, (y1 + y2) / 2, x2, (y1 + y2) / 2, x2, y2);
            cairo_stroke(cr);
            cairo_arc(cr, x2, y2, 3.5, 0, 2 * G_PI);
            cairo_fill(cr);
        } else {
            /* Interworld/cross-area destinations have no shared map geometry.
             * Mark only their source and show the real target in the list. */
            cairo_arc(cr, x1, y1, 7.0, 0, 2 * G_PI);
            cairo_stroke(cr);
        }
        cairo_arc(cr, x1, y1, 3.5, 0, 2 * G_PI);
        cairo_fill(cr);
        cairo_restore(cr);
    }
}

static void connection_go_to_0116(GtkButton *button, gpointer userdata)
{
    WorldGrid *w = userdata;
    guint world = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(button), "mv-conn-world"));
    guint area = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(button), "mv-conn-area"));
    guint room = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(button), "mv-conn-room"));
    if (world > 1 || area >= (world ? 12u : 7u) || room > 999) return;
    gtk_drop_down_set_selected(GTK_DROP_DOWN(w->world_select), world);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(w->area_select), world ? area + 1 : area);
    /* world_changed/area_changed synchronously rebuild the native grid. */
    for (guint i = 0; i < w->cells->len; ++i) {
        const MapCell *c = &g_array_index(w->cells, MapCell, i);
        if (c->area == area && c->room == room && c->provenance < 3) {
            w->selection = i;
            w->selected = TRUE;
            grid_rebuild(w);
            selected_room_doors_refresh(w);
            connections_refresh_0116(w);
            gchar *label = g_strdup_printf("Destination: %s / %s / room %03u",
                worlds[w->world], area_name(w, area), room);
            gtk_label_set_text(GTK_LABEL(w->details), label);
            g_free(label);
            return;
        }
    }
    gtk_label_set_text(GTK_LABEL(w->details),
        "Destination not found in verified original map cells.");
}

static void connections_refresh_0116(WorldGrid *w)
{
    if (!w->connection_edges || !w->connection_list) return;
    g_array_set_size(w->connection_edges, 0);
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(w->connection_list)))
        gtk_box_remove(GTK_BOX(w->connection_list), child);
    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->connections_toggle))) {
        gtk_widget_queue_draw(w->connection_canvas);
        return;
    }
    gboolean all = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->connections_all));
    if (!all && (!w->selected || w->selection >= w->cells->len)) {
        gtk_box_append(GTK_BOX(w->connection_list),
                       gtk_label_new("Select a native room to inspect its project links."));
        gtk_widget_queue_draw(w->connection_canvas);
        return;
    }
    guint selected_area = G_MAXUINT, selected_room = G_MAXUINT;
    if (w->selected && w->selection < w->cells->len) {
        const MapCell *c = &g_array_index(w->cells, MapCell, w->selection);
        selected_area = c->area;
        selected_room = c->room;
    }
    gchar *world_arg = g_strdup_printf("--world=%s", w->world ? "aria" : "zero_mission");
    /* Aria's "All areas" view still needs an area for one-room lookup. */
    guint requested_area = (w->area == G_MAXUINT && !all) ? selected_area : w->area;
    gchar *area_arg = requested_area == G_MAXUINT ? NULL :
        g_strdup_printf("--area=%u", requested_area);
    gchar *room_arg = (!all && selected_room <= 999) ?
        g_strdup_printf("--room=%u", selected_room) : NULL;
    gchar *args[9];
    guint argc = 0;
    args[argc++] = "python3";
    args[argc++] = "scripts/editor_cli.py";
    args[argc++] = "--command=connection-list";
    args[argc++] = world_arg;
    if (area_arg) args[argc++] = area_arg;
    if (room_arg) args[argc++] = room_arg;
    args[argc++] = "--format=tsv";
    args[argc] = NULL;
    gchar *out = NULL, *err = NULL;
    GError *error = NULL;
    gint code = -1;
    gboolean launched = g_spawn_sync(NULL, args, NULL, G_SPAWN_SEARCH_PATH,
                                     NULL, NULL, &out, &err, &code, &error);
    g_free(world_arg); g_free(area_arg); g_free(room_arg);
    if (!launched || !g_spawn_check_wait_status(code, NULL)) {
        gtk_box_append(GTK_BOX(w->connection_list),
            gtk_label_new("Connection index unavailable; check saved project data."));
        g_free(out); g_free(err); g_clear_error(&error);
        gtk_widget_queue_draw(w->connection_canvas);
        return;
    }
    gchar **lines = g_strsplit(out ? out : "", "\n", -1);
    guint shown = 0;
    for (guint i = 0; lines[i] && i < 8192; ++i) {
        if (!*lines[i]) continue;
        gchar **fields = g_strsplit(lines[i], "\t", 11);
        guint source_room, source_door, target_room, target_door, reverse;
        if (g_strv_length(fields) != 10 ||
            !parse_uint(fields[2], &source_room) || source_room > 999 ||
            !parse_uint(fields[3], &source_door) || !source_door ||
            !parse_uint(fields[6], &target_room) || target_room > 999 ||
            !parse_uint(fields[7], &target_door) ||
            !parse_uint(fields[9], &reverse) || reverse > 1 ||
            strlen(fields[8]) >= sizeof(((ProjectMapEdge0116 *)0)->state)) {
            g_strfreev(fields);
            continue;
        }
        guint src_area = G_MAXUINT, target_area = G_MAXUINT, target_world = w->world;
        for (guint area = 0; area < area_count(w); ++area)
            if (!g_strcmp0(fields[1], w->world ? NULL : mzm_areas[area]) ||
                (w->world && parse_uint(fields[1], &src_area) && src_area == area)) {
                src_area = area;
                break;
            }
        if (src_area >= area_count(w)) { g_strfreev(fields); continue; }
        if (!g_strcmp0(fields[4], "aria")) target_world = 1;
        else if (!g_strcmp0(fields[4], "mzm")) target_world = 0;
        else if (g_strcmp0(fields[4], "-")) { g_strfreev(fields); continue; }
        if (g_strcmp0(fields[5], "-")) {
            if (target_world == 1) {
                if (!parse_uint(fields[5], &target_area) || target_area >= 12)
                    target_area = G_MAXUINT;
            } else for (guint a = 0; a < 7; ++a)
                if (!g_strcmp0(fields[5], mzm_areas[a])) { target_area = a; break; }
        }
        ProjectMapEdge0116 edge = {0};
        edge.area = src_area; edge.room = source_room; edge.door = source_door;
        edge.target_world = target_world; edge.target_area = target_area;
        edge.target_room = target_room; edge.target_door = target_door;
        edge.reciprocal = reverse;
        g_strlcpy(edge.state, fields[8], sizeof(edge.state));
        g_array_append_val(w->connection_edges, edge);
        const char *status = !strcmp(edge.state, "reciprocal") ? "Reciprocal" :
            !strcmp(edge.state, "interworld") ? "Interworld" :
            !strcmp(edge.state, "simple") ? "One-way" :
            !strcmp(edge.state, "invalid") ? "Invalid target" : "No destination";
        gchar *label = target_area == G_MAXUINT ?
            g_strdup_printf("Room %03u D%u — %s", source_room, source_door, status) :
            g_strdup_printf("Room %03u D%u → %s / %s %03u D%u — %s%s",
                source_room, source_door, worlds[target_world],
                target_world ? aria_areas[target_area] : mzm_areas[target_area],
                target_room, target_door, status,
                reverse && target_world != w->world ? " (reciprocal)" : "");
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
        GtkWidget *text = gtk_label_new(label);
        gtk_label_set_xalign(GTK_LABEL(text), 0);
        gtk_label_set_ellipsize(GTK_LABEL(text), PANGO_ELLIPSIZE_END);
        gtk_widget_set_hexpand(text, TRUE);
        gtk_widget_set_tooltip_text(text, label);
        gtk_box_append(GTK_BOX(row), text);
        if (target_area != G_MAXUINT && strcmp(edge.state, "invalid") &&
            strcmp(edge.state, "missing")) {
            GtkWidget *go = gtk_button_new_from_icon_name("go-next-symbolic");
            gtk_widget_set_tooltip_text(go, "Select destination on its original map");
            g_object_set_data(G_OBJECT(go), "mv-conn-world", GUINT_TO_POINTER(target_world));
            g_object_set_data(G_OBJECT(go), "mv-conn-area", GUINT_TO_POINTER(target_area));
            g_object_set_data(G_OBJECT(go), "mv-conn-room", GUINT_TO_POINTER(target_room));
            g_signal_connect(go, "clicked", G_CALLBACK(connection_go_to_0116), w);
            gtk_box_append(GTK_BOX(row), go);
        }
        gtk_box_append(GTK_BOX(w->connection_list), row);
        ++shown;
        g_free(label);
        g_strfreev(fields);
    }
    g_strfreev(lines);
    g_free(out); g_free(err); g_clear_error(&error);
    if (!shown) gtk_box_append(GTK_BOX(w->connection_list),
        gtk_label_new("No saved project doors in this scope."));
    gtk_widget_queue_draw(w->connection_canvas);
}

static void connections_changed_0116(GtkToggleButton *button, gpointer userdata)
{ (void)button; connections_refresh_0116(userdata); }

static void placement_refresh(GtkButton *button, gpointer userdata)
{
    (void)button;
    GtkWidget *page = GTK_WIDGET(userdata);
    GtkWidget *choice = g_object_get_data(G_OBJECT(page), "mv-placement-choice");
    GtkWidget *status = g_object_get_data(G_OBJECT(page), "mv-placement-status");
    gchar *out = NULL, *err = NULL;
    GError *error = NULL;
    gint code = -1;
    gchar *args[] = {"python3", "scripts/editor_cli.py", "--command=room-list",
                     "--source=draft", "--format=tsv", NULL};
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
    gchar *args[] = {"python3", "scripts/editor_cli.py", "--command=room-place",
                     "--room", (gchar *)id, "--x", x, "--y", y,
                     "--format=text", NULL};
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
    w->grid_columns = stride;
    w->grid_rows = ymax + 1;
    int canvas_width = (int)(w->grid_columns * w->size);
    int canvas_height = (int)(w->grid_rows * w->size);
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(w->map_background),
                                       canvas_width);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(w->map_background),
                                        canvas_height);
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(w->connection_canvas),
                                       canvas_width);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(w->connection_canvas),
                                        canvas_height);
    gtk_widget_set_size_request(w->grid, canvas_width, canvas_height);
    g_object_set_data(G_OBJECT(w->map_background), "mv-span-width",
                      GUINT_TO_POINTER(w->grid_columns));
    g_object_set_data(G_OBJECT(w->map_background), "mv-span-height",
                      GUINT_TO_POINTER(w->grid_rows));
    /* One transparent span gives GtkGrid its exact coordinate geometry. The
     * old implementation allocated one GtkBox for every empty case. */
    GtkWidget *grid_extent = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_can_target(grid_extent, FALSE);
    gtk_widget_set_opacity(grid_extent, 0.0);
    gtk_grid_attach(GTK_GRID(w->grid), grid_extent, 0, 0,
                    (int)w->grid_columns, (int)w->grid_rows);
    guint *lookup = g_new0(guint, count);
    gboolean *covered = g_new0(gboolean, count);
    guint overlapping = 0, groups = 0, thumbs = 0;
    GArray *native_doors = w->world ? NULL : load_mzm_doors();
    gboolean show_doors = native_doors &&
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->doors_toggle));
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
                covered[pos] = TRUE;
                continue;
            }
            const MapCell *c = &g_array_index(w->cells, MapCell, entry - 1);
            guint width = 1, height = 1;
            for (guint xx = x + 1; c->provenance != 3 && c->room != 999 && xx <= xmax; ++xx) {
                guint spot = y * stride + xx, next = lookup[spot];
                if (covered[spot] || !next) break;
                const MapCell *other = &g_array_index(w->cells, MapCell, next - 1);
                if (!same_atlas_group(w, c, other)) break;
                ++width;
            }
            for (guint yy = y + 1; c->provenance != 3 && c->room != 999 && yy <= ymax; ++yy) {
                gboolean full = TRUE;
                for (guint xx = x; xx < x + width; ++xx) {
                    guint spot = yy * stride + xx, next = lookup[spot];
                    if (covered[spot] || !next) { full = FALSE; break; }
                    const MapCell *other = &g_array_index(w->cells, MapCell, next - 1);
                    if (!same_atlas_group(w, c, other)) { full = FALSE; break; }
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
            GtkWidget *cell = gtk_overlay_new();
            GtkWidget *cell_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
            gtk_overlay_set_child(GTK_OVERLAY(cell), cell_content);
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
            if (!w->world && c->provenance <= 1 && c->room != 999) {
                /* MZM room mosaic: one actual GTK child for the whole
                 * rectangular footprint, not bordered children per case.
                 * Each 240x160 native screen retains its exact atlas slot. */
                GtkWidget *mosaic = gtk_grid_new();
                gtk_grid_set_column_homogeneous(GTK_GRID(mosaic), TRUE);
                gtk_grid_set_row_homogeneous(GTK_GRID(mosaic), TRUE);
                gtk_grid_set_column_spacing(GTK_GRID(mosaic), 0);
                gtk_grid_set_row_spacing(GTK_GRID(mosaic), 0);
                gtk_widget_set_hexpand(mosaic, TRUE);
                gtk_widget_set_vexpand(mosaic, TRUE);
                for (guint local_y = 0; local_y < height; ++local_y) {
                    for (guint local_x = 0; local_x < width; ++local_x) {
                        guint tile_entry = lookup[(y + local_y) * stride + x + local_x];
                        const MapCell *part = &g_array_index(w->cells, MapCell, tile_entry - 1);
                        GtkWidget *tile = gtk_overlay_new();
                        gtk_widget_set_hexpand(tile, TRUE);
                        gtk_widget_set_vexpand(tile, TRUE);
                        char tile_path[256];
                        if (part->provenance == 1 && has_image(w, part, tile_path, sizeof(tile_path))) {
                            GtkWidget *picture = gtk_picture_new_for_filename(tile_path);
                            gtk_picture_set_can_shrink(GTK_PICTURE(picture), TRUE);
                            gtk_widget_set_size_request(picture,
                                (int)w->size - 4, (int)w->size - 4);
                            /* This is a single map case, not a room-wide image. */
                            gtk_picture_set_content_fit(GTK_PICTURE(picture), GTK_CONTENT_FIT_FILL);
                            gtk_overlay_set_child(GTK_OVERLAY(tile), picture);
                            ++thumbs;
                        } else {
                            /* Unknown original graphics stay blank, never stretched. */
                            GtkWidget *label = gtk_label_new(width == 1 && height == 1 ?
                                "room" : "");
                            gtk_widget_set_halign(label, GTK_ALIGN_CENTER);
                            gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
                            gtk_overlay_set_child(GTK_OVERLAY(tile), label);
                        }
                        if (show_doors) map_cell_door_badge(tile, native_doors, part);
                        gtk_grid_attach(GTK_GRID(mosaic), tile,
                                        (int)local_x, (int)local_y, 1, 1);
                    }
                }
                gtk_box_append(GTK_BOX(cell_content), mosaic);
            } else if (c->provenance != 0 && c->provenance != 3 && c->provenance != 4 &&
                       room_cells == width * height &&
                       has_image(w, c, path, sizeof(path))) {
                GtkWidget *picture = gtk_picture_new_for_filename(path);
                gtk_picture_set_can_shrink(GTK_PICTURE(picture), TRUE);
                gtk_picture_set_content_fit(GTK_PICTURE(picture), GTK_CONTENT_FIT_CONTAIN);
                gtk_widget_set_size_request(picture, (int)(w->size * width) - 4,
                                             (int)(w->size * height) - 4);
                gtk_widget_set_hexpand(picture, TRUE);
                gtk_widget_set_vexpand(picture, TRUE);
                gtk_box_append(GTK_BOX(cell_content), picture);
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
                gtk_box_append(GTK_BOX(cell_content), label);
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
            if (show_doors && (w->world || c->provenance > 1))
                map_cell_door_badge(cell, native_doors, c);
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
    if (native_doors) g_array_free(native_doors, TRUE);
    gtk_widget_queue_draw(w->map_background);
    if (w->connection_canvas) gtk_widget_queue_draw(w->connection_canvas);
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
            guint remaining = 0, generated = 0;
            const char *more = out ? strstr(out, "remaining=") : NULL;
            const char *made = out ? strstr(out, "generated ") : NULL;
            if (more) sscanf(more, "remaining=%u", &remaining);
            if (made) sscanf(made, "generated %u", &generated);
            /* Budget zero was the indexing pass: DO NOT skip the first area.
             * Continue while we make progress, but avoid an endless retry if
             * local ROM resources are missing or a decoder is unsupported. */
            if (generated_budget && (!remaining || generated == 0))
                ++w->next_preview_area;
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
    guint budget = g_file_test(index_path, G_FILE_TEST_IS_REGULAR) ? 24u : 0u;
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
    gtk_widget_set_visible(w->doors_toggle, w->world == 0);
    gtk_widget_set_visible(w->door_expander, w->world == 0);
    w->prefetching=TRUE;
    w->next_preview_area=0;
    w->selected=FALSE;
    selected_room_doors_refresh(w);
    connections_refresh_0116(w);
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
    selected_room_doors_refresh(w);
    connections_refresh_0116(w);
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
static void doors_changed(GtkToggleButton *button, gpointer data)
{ (void)button; WorldGrid *w = data; if (w->cells->len) grid_rebuild(w); }
static void generate_clicked(GtkButton *b,gpointer data)
{
    WorldGrid *w=data;(void)b;
    w->prefetching=TRUE;
    w->next_preview_area=0;
    begin_generation(w);
}
/* PATCH_0106_GLOBAL_MAP_DESTINATION_PICKER
 * One pending picker at a time; no permanent state nor mutation of source maps.
 * The caller must abandon the callback when its window is destroyed. */
static void world_atlas_pick_finish_0106(WorldGrid *w, gboolean selected,
                                          guint area, guint room)
{
    WorldAtlasRoomPicked callback = w->pick_callback;
    gpointer context = w->pick_data;
    w->pick_callback = NULL;
    w->pick_data = NULL;
    if (w->pick_bar) gtk_widget_set_visible(w->pick_bar, FALSE);
    if (callback)
        callback(selected ? w->world : G_MAXUINT,
                 selected ? area : G_MAXUINT,
                 selected ? room : G_MAXUINT, context);
}

static void world_atlas_pick_cancel_clicked_0106(GtkButton *button, gpointer userdata)
{
    (void)button;
    WorldGrid *w = userdata;
    world_atlas_pick_finish_0106(w, FALSE, G_MAXUINT, G_MAXUINT);
}

/* Switching to the map also activates its left-side workspace navigation,
 * because that navigation already follows the central notebook selection. */
gboolean world_atlas_begin_room_pick(GtkWidget *page, guint preferred_world,
                                      WorldAtlasRoomPicked callback, gpointer userdata)
{
    if (!GTK_IS_WIDGET(page) || preferred_world > 1 || !callback || !userdata)
        return FALSE;
    WorldGrid *w = g_object_get_data(G_OBJECT(page), "mv-world-grid");
    GtkWidget *ancestor = gtk_widget_get_ancestor(page, GTK_TYPE_NOTEBOOK);
    if (!w || w->pick_callback || !GTK_IS_NOTEBOOK(ancestor) ||
        gtk_notebook_page_num(GTK_NOTEBOOK(ancestor), page) < 0)
        return FALSE;
    w->pick_callback = callback;
    w->pick_data = userdata;
    gtk_widget_set_visible(w->pick_bar, TRUE);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(w->world_select), preferred_world);
    gtk_label_set_text(GTK_LABEL(w->details),
        "Choose an original room on the global map. Unknown/unowned cells and drafts cannot be targets.");
    gtk_notebook_set_current_page(GTK_NOTEBOOK(ancestor),
                                  gtk_notebook_page_num(GTK_NOTEBOOK(ancestor), page));
    return TRUE;
}

void world_atlas_abandon_room_pick(GtkWidget *page, gpointer userdata)
{
    if (!GTK_IS_WIDGET(page) || !userdata) return;
    WorldGrid *w = g_object_get_data(G_OBJECT(page), "mv-world-grid");
    if (!w || !w->pick_callback || w->pick_data != userdata) return;
    /* Silent discard: userdata may be a form already being finalized. */
    w->pick_callback = NULL;
    w->pick_data = NULL;
    if (w->pick_bar) gtk_widget_set_visible(w->pick_bar, FALSE);
}

static void world_free(gpointer data)
{
    WorldGrid *w = data;
    /* A detached/closed map cannot leave the source form hidden. */
    if (w->pick_callback)
        world_atlas_pick_finish_0106(w, FALSE, G_MAXUINT, G_MAXUINT);
    if (w->pan_tick && w->scroller)
        gtk_widget_remove_tick_callback(w->scroller, w->pan_tick);
    if (w->popup)
        g_object_remove_weak_pointer(G_OBJECT(w->popup), (gpointer *)&w->popup);
    g_array_free(w->cells, TRUE);
    if (w->connection_edges) g_array_free(w->connection_edges, TRUE);
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
        ".mv-project-draft{background:#425052;border:2px dashed #f4cc58;}"
        ".mv-native-door-badge{background:#4d266c;color:#fff;border:1px solid #ca91f8;"
        "font-size:9px;font-weight:bold;border-radius:3px;padding:0 2px;}"};
    GtkCssProvider *provider=gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider,css);
    GdkDisplay *display=gdk_display_get_default();
    if(display)gtk_style_context_add_provider_for_display(display,GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
    WorldGrid *w=g_new0(WorldGrid,1);
    w->workspace=workspace;w->world_badge=indicator;w->cells=g_array_new(FALSE,FALSE,sizeof(MapCell));
    w->size=28;
    w->connection_edges = g_array_new(FALSE, FALSE, sizeof(ProjectMapEdge0116));
    w->prefetching=TRUE;
    GtkWidget *root=gtk_box_new(GTK_ORIENTATION_VERTICAL,6);
    GtkWidget *bar=gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(bar), GTK_SELECTION_NONE);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(bar), FALSE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(bar), 1);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(bar), 9);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(bar), 6);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(bar), 4);
    gtk_widget_add_css_class(bar, "toolbar");
    GtkWidget *scroller=gtk_scrolled_window_new();
    GtkWidget *grid=gtk_grid_new();
    GtkWidget *map_overlay = gtk_overlay_new();
    w->map_background = gtk_drawing_area_new();
    w->connection_canvas = gtk_drawing_area_new();
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(w->map_background),
                                   map_background_draw_0117, w, NULL);
    gtk_overlay_set_child(GTK_OVERLAY(map_overlay), w->map_background);
    gtk_widget_set_can_target(grid, FALSE);
    gtk_overlay_add_overlay(GTK_OVERLAY(map_overlay), grid);
    gtk_widget_set_halign(map_overlay, GTK_ALIGN_START);
    gtk_widget_set_valign(map_overlay, GTK_ALIGN_START);
    gtk_widget_set_can_target(w->connection_canvas, FALSE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(w->connection_canvas),
                                   connection_draw_0116, w, NULL);
    gtk_overlay_add_overlay(GTK_OVERLAY(map_overlay), w->connection_canvas);
    /* The scrolling surface is exactly the map. Artificial right/bottom
     * padding used to create large empty scroll ranges and accidental drift. */
    GtkWidget *pan_surface=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
    GtkWidget *canvas_row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
    gtk_widget_set_halign(grid,GTK_ALIGN_START);
    gtk_widget_set_valign(grid,GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(canvas_row),map_overlay);
    gtk_box_append(GTK_BOX(pan_surface),canvas_row);
    gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
    GtkWidget *generate=gtk_button_new_with_label("Generate more original previews");
    GtkWidget *open=gtk_button_new_with_label("Open selected room");
    w->pick_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *pick_text = gtk_label_new(
        "PICK DOOR DESTINATION: click a verified room, or cancel.");
    GtkWidget *pick_cancel = gtk_button_new_with_label("Cancel picking");
    gtk_box_append(GTK_BOX(w->pick_bar), pick_text);
    gtk_box_append(GTK_BOX(w->pick_bar), pick_cancel);
    gtk_widget_set_visible(w->pick_bar, FALSE);
    g_signal_connect(pick_cancel, "clicked",
                     G_CALLBACK(world_atlas_pick_cancel_clicked_0106), w);
    /* Keep global-map door IDs optional. Native door overlays belong
     * primarily in the individual room editor, not over the map mosaic. */
    w->connections_toggle = gtk_toggle_button_new_with_label("Show connections");
    w->connections_all = gtk_toggle_button_new_with_label("All links in current map");
    gtk_widget_set_tooltip_text(w->connections_all,
        "Selected room by default; all saved project links when enabled.");
    w->doors_toggle = gtk_toggle_button_new_with_label("Show global door IDs");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w->doors_toggle), FALSE);
    w->page=root;w->grid=grid;w->scroller=scroller;
    map_context_enable(w->map_background, w, 0, 0, 1, 1);
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
    w->connection_expander = gtk_expander_new("Project connections (saved overrides)");
    w->connection_list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    GtkWidget *connection_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(connection_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(connection_scroll, -1, 125);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(connection_scroll), w->connection_list);
    gtk_expander_set_child(GTK_EXPANDER(w->connection_expander), connection_scroll);
    w->door_expander = gtk_expander_new("Native doors / select a Zero Mission room");
    GtkWidget *door_scroller = gtk_scrolled_window_new();
    w->door_list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(w->door_list, 8);
    gtk_widget_set_margin_end(w->door_list, 8);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(door_scroller), w->door_list);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(door_scroller),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(door_scroller, -1, 120);
    gtk_expander_set_child(GTK_EXPANDER(w->door_expander), door_scroller);
    w->world_select=gtk_drop_down_new_from_strings(worlds);
    w->area_select=gtk_drop_down_new_from_strings(mzm_areas);
    w->zoom=gtk_spin_button_new_with_range(22,64,6);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->zoom),28);
    GtkWidget *world_group = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *area_group = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *zoom_group = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_append(GTK_BOX(world_group),gtk_label_new("WORLD:"));
    gtk_box_append(GTK_BOX(world_group),w->world_select);
    gtk_box_append(GTK_BOX(area_group),gtk_label_new("Area:"));
    gtk_box_append(GTK_BOX(area_group),w->area_select);
    gtk_box_append(GTK_BOX(zoom_group),gtk_label_new("Case size:"));
    gtk_box_append(GTK_BOX(zoom_group),w->zoom);
    gtk_flow_box_append(GTK_FLOW_BOX(bar),world_group);
    gtk_flow_box_append(GTK_FLOW_BOX(bar),area_group);
    gtk_flow_box_append(GTK_FLOW_BOX(bar),zoom_group);
    gtk_flow_box_append(GTK_FLOW_BOX(bar),generate);
    gtk_flow_box_append(GTK_FLOW_BOX(bar),open);
    gtk_flow_box_append(GTK_FLOW_BOX(bar),w->doors_toggle);
    gtk_flow_box_append(GTK_FLOW_BOX(bar),w->connections_toggle);
    gtk_flow_box_append(GTK_FLOW_BOX(bar),w->connections_all);
    gtk_box_append(GTK_BOX(root),bar);
    gtk_box_append(GTK_BOX(root),w->pick_bar);
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
    gtk_box_append(GTK_BOX(root),w->connection_expander);
    gtk_box_append(GTK_BOX(root),w->door_expander);
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
    g_signal_connect(w->doors_toggle,"toggled",G_CALLBACK(doors_changed),w);
    g_signal_connect(w->connections_toggle,"toggled", G_CALLBACK(connections_changed_0116),w);
    g_signal_connect(w->connections_all,"toggled", G_CALLBACK(connections_changed_0116),w);
    g_signal_connect(generate,"clicked",G_CALLBACK(generate_clicked),w);
    g_signal_connect(open,"clicked",G_CALLBACK(selected_open_click),w);
    if(reload_rows(w))grid_rebuild(w);
    begin_generation(w);
    return root;
}
