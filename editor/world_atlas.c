/* SPDX-License-Identifier: GPL-3.0-only */
/* Original minimap cell grid for both worlds. No invented room footprints. */
#include "world_atlas.h"
#include <stdio.h>
#include <string.h>

typedef struct { guint area, room, x, y, save, warp, provenance; } MapCell;
typedef struct {
    GArray *cells;
    NativeWorkspace *workspace;
    GtkWidget *page, *grid, *details, *status, *world_select, *area_select, *zoom;
    GtkWidget *selected_cell;
    GtkWidget *world_badge;
    guint world, area;
    guint selection;
    gboolean selected, busy, pending_generation, changing_world;
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
        MapCell c={0}; char excess; guint a,r,x,y,s,v,provenance=0;
        if (!split[i][0] || split[i][0]=='#') continue;
        int fields=sscanf(split[i],"%u|%u|%u|%u|%u|%u|%u%c",
                          &a,&r,&x,&y,&s,&v,&provenance,&excess);
        if (fields != 7 && fields != 6) continue; /* also read legacy 6-field indexes */
        if (fields == 6) provenance = w->world ? 2u : 0u;
        if (a>=area_count(w) || r>=1000 || x>=128 || y>=128 || s>1 || v>1 ||
            provenance>2 || (w->world && provenance!=2) ||
            (!w->world && provenance==2)) continue;
        c.area=a;c.room=r;c.x=x;c.y=y;c.save=s;c.warp=v;
        c.provenance=provenance;
        g_array_append_val(w->cells,c);
    }
    g_strfreev(split);g_free(contents);
    return w->cells->len>0;
}

static void selected_open(WorldGrid *w)
{
    if (!w->selected || w->selection>=w->cells->len)return;
    MapCell c=g_array_index(w->cells,MapCell,w->selection);
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
    w->selected = TRUE;
    MapCell c = g_array_index(w->cells, MapCell, w->selection);
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
                         c.provenance == 1 ? "native clipdata bounds" :
                         "original map anchor ONLY (extent unavailable)";
    gchar *message = g_strdup_printf(
        "%s / %s / room %03u | %u mapped case(s), clicked (%u,%u) "
        "%s %s | %s",
        worlds[w->world], area_name(w, c.area), c.room,
        room_cells_in_area(w, c.area, c.room), c.x, c.y,
        c.save ? "[SAVE]" : "", c.warp ? "[WARP]" : "", source);
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
static void grid_rebuild(WorldGrid *w)
{
    GtkWidget *child;
    w->selected_cell = NULL;
    while ((child = gtk_widget_get_first_child(w->grid)))
        gtk_grid_remove(GTK_GRID(w->grid), child);
    guint xmax = 0, ymax = 0, total = 0, anchor_only = 0;
    for (guint i = 0; i < w->cells->len; ++i) {
        const MapCell *c = &g_array_index(w->cells, MapCell, i);
        if (c->area != w->area) continue;
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
        if (c->area != w->area) continue;
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
                gtk_grid_attach(GTK_GRID(w->grid), empty, (int)x, (int)y, 1, 1);
                covered[pos] = TRUE;
                continue;
            }
            const MapCell *c = &g_array_index(w->cells, MapCell, entry - 1);
            guint width = 1, height = 1;
            for (guint xx = x + 1; xx <= xmax; ++xx) {
                guint spot = y * stride + xx, next = lookup[spot];
                if (covered[spot] || !next) break;
                const MapCell *other = &g_array_index(w->cells, MapCell, next - 1);
                if (other->room != c->room || other->area != c->area ||
                    other->provenance != c->provenance) break;
                ++width;
            }
            for (guint yy = y + 1; yy <= ymax; ++yy) {
                gboolean full = TRUE;
                for (guint xx = x; xx < x + width; ++xx) {
                    guint spot = yy * stride + xx, next = lookup[spot];
                    if (covered[spot] || !next) { full = FALSE; break; }
                    const MapCell *other = &g_array_index(w->cells, MapCell, next - 1);
                    if (other->room != c->room || other->area != c->area ||
                        other->provenance != c->provenance) { full = FALSE; break; }
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
            if (c->provenance != 0 && room_cells == width * height &&
                has_image(w, c, path, sizeof(path))) {
                GtkWidget *picture = gtk_picture_new_for_filename(path);
                gtk_picture_set_can_shrink(GTK_PICTURE(picture), TRUE);
                gtk_widget_set_size_request(picture, (int)(w->size * width) - 4,
                                             (int)(w->size * height) - 4);
                gtk_box_append(GTK_BOX(cell), picture);
                ++thumbs;
            } else {
                gchar *name = g_strdup_printf("%u%s", c->room,
                        room_cells > width * height ? " …" : "");
                GtkWidget *label = gtk_label_new(name);
                gtk_widget_set_halign(label, GTK_ALIGN_CENTER);
                gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
                gtk_widget_set_vexpand(label, TRUE);
                gtk_box_append(GTK_BOX(cell), label);
                g_free(name);
            }
            gchar *tip = g_strdup_printf("Room %u: %u verified case(s); %ux%u displayed segment; %s",
                c->room, room_cells, width, height,
                c->provenance == 2 ? "original Aria map" :
                c->provenance == 1 ? "native MZM clip bounds" : "MZM anchor only");
            gtk_widget_set_tooltip_text(cell, tip);
            g_free(tip);
            g_object_set_data(G_OBJECT(cell), "mv-grid-index", GUINT_TO_POINTER(entry));
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
        worlds[w->world], area_name(w, w->area), total, groups, thumbs,
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
        } else if (success && generated_world == w->world && generated_budget == 0) {
            /* First present the verified cells, then render graphics separately. */
            begin_generation(w);
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
    gchar *area = g_strdup_printf("%u", w->area);
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
    for(guint i=0;i<area_count(w);++i)gtk_string_list_append(list,area_name(w,i));
    gtk_drop_down_set_model(GTK_DROP_DOWN(w->area_select),G_LIST_MODEL(list));
    g_object_unref(list);
    w->area=0;w->selected=FALSE;
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
    guint area=gtk_drop_down_get_selected(GTK_DROP_DOWN(object));
    if(area>=area_count(w))return;
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
{(void)b;begin_generation(data);}
static void world_free(gpointer data)
{WorldGrid *w=data;g_array_free(w->cells,TRUE);g_free(w);}
GtkWidget *world_atlas_build(GtkWidget *center,NativeWorkspace *workspace,GtkWidget *indicator)
{
    static const char css[]={
        ".mv-map-cell{background:#101923;border:1px solid #293743;}"
        ".mv-occupied{background:#345968;border:1px solid #6c9da6;}"
        ".mv-save{border:2px solid #71d0a3;}"
        ".mv-warp{border:2px solid #b7a1eb;}"
        ".mv-selected{border:3px solid #f4cc58;}"
        ".mv-room-footprint{border:2px solid #91b8c7;}"
        ".mv-anchor-only{border:2px dashed #d7a564;}"};
    GtkCssProvider *provider=gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider,css);
    GdkDisplay *display=gdk_display_get_default();
    if(display)gtk_style_context_add_provider_for_display(display,GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
    WorldGrid *w=g_new0(WorldGrid,1);
    w->workspace=workspace;w->world_badge=indicator;w->cells=g_array_new(FALSE,FALSE,sizeof(MapCell));
    w->size=36;
    GtkWidget *root=gtk_box_new(GTK_ORIENTATION_VERTICAL,6);
    GtkWidget *bar=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,6);
    GtkWidget *scroller=gtk_scrolled_window_new();
    GtkWidget *grid=gtk_grid_new();
    GtkWidget *generate=gtk_button_new_with_label("Generate more original previews");
    GtkWidget *open=gtk_button_new_with_label("Open selected room");
    w->page=root;w->grid=grid;
    w->status=gtk_label_new("Reading verified original minimap cases…");
    w->details=gtk_label_new("Double-click a case to edit it in the shared room editor.");
    w->world_select=gtk_drop_down_new_from_strings(worlds);
    w->area_select=gtk_drop_down_new_from_strings(mzm_areas);
    w->zoom=gtk_spin_button_new_with_range(22,64,6);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->zoom),36);
    gtk_box_append(GTK_BOX(bar),gtk_label_new("WORLD:"));
    gtk_box_append(GTK_BOX(bar),w->world_select);
    gtk_box_append(GTK_BOX(bar),gtk_label_new("Area:"));
    gtk_box_append(GTK_BOX(bar),w->area_select);
    gtk_box_append(GTK_BOX(bar),gtk_label_new("Case size:"));
    gtk_box_append(GTK_BOX(bar),w->zoom);
    gtk_box_append(GTK_BOX(bar),generate);
    gtk_box_append(GTK_BOX(bar),open);
    gtk_box_append(GTK_BOX(root),bar);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller),grid);
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
    g_signal_connect(w->world_select,"notify::selected",G_CALLBACK(world_changed),w);
    g_signal_connect(w->area_select,"notify::selected",G_CALLBACK(area_changed),w);
    g_signal_connect(w->zoom,"value-changed",G_CALLBACK(zoom_changed),w);
    g_signal_connect(generate,"clicked",G_CALLBACK(generate_clicked),w);
    g_signal_connect(open,"clicked",G_CALLBACK(selected_open_click),w);
    if(reload_rows(w))grid_rebuild(w);
    begin_generation(w);
    return root;
}
