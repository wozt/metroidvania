/* SPDX-License-Identifier: GPL-3.0-only */
/* Shared structural room browser. Never writes ROM data or inferred geometry. */
#include "room_browser.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    NativeWorkspace *workspace;
    RoomWorld world;
    GtkWidget *page, *list, *status, *details, *picture;
    GtkWidget *render_button, *open_button, *mode, *area_filter, *context_menu_button;
    guint draft_refresh_generation;
    gboolean selected, busy;
    guint area, room;
} RoomBrowser;

/* A notebook page owns RoomBrowser, but GTK destroys children in tree
 * order. Keep controls alive until the page's finalizer has finished all
 * child teardown callbacks (particularly GtkListBox filtering/selection). */
static void room_browser_state_free(gpointer userdata)
{
    RoomBrowser *browser = userdata;
    /* Disconnect callbacks before releasing any referenced GTK child. */
    g_signal_handlers_disconnect_by_data(browser->list, browser);
    gtk_list_box_set_filter_func(GTK_LIST_BOX(browser->list), NULL, NULL, NULL);
    g_signal_handlers_disconnect_by_data(browser->mode, browser);
    g_signal_handlers_disconnect_by_data(browser->area_filter, browser);
    g_signal_handlers_disconnect_by_data(browser->render_button, browser);

    g_object_unref(browser->list);
    g_object_unref(browser->mode);
    g_object_unref(browser->area_filter);
    g_object_unref(browser->render_button);
    g_object_unref(browser->open_button);
    g_object_unref(browser->status);
    g_object_unref(browser->details);
    g_object_unref(browser->picture);
    g_object_unref(browser->context_menu_button);
    g_free(browser);
}

static const char *const zero_areas[] = {
    "Brinstar", "Kraid", "Norfair", "Ridley", "Tourian", "Crateria", "Chozodia", NULL
};
static const char *const aria_areas[] = {
    "Castle Corridor", "Chapel", "Study", "Dance Hall", "Inner Quarters",
    "Floating Garden", "Clock Tower", "Underground", "The Arena", "Top Floor",
    "Chaotic Realm entrance", "Chaotic Realm boss", NULL
};
static const char *const zero_modes[] = {"BG1", "BG2", NULL};
static const char *const aria_modes[] = {
    "Composite", "BG1", "BG2", "BG3", "Collision (diagnostic)", NULL
};
static const char *const aria_suffixes[] = {
    "composite", "bg1", "bg2", "bg3", "collision"
};

static const char *const *area_names(RoomWorld world)
{
    return world == ROOM_WORLD_ARIA ? aria_areas : zero_areas;
}

static void display_preview(RoomBrowser *browser)
{
    char filename[320];
    guint mode = gtk_drop_down_get_selected(GTK_DROP_DOWN(browser->mode));
    if (!browser->selected) {
        gtk_picture_set_paintable(GTK_PICTURE(browser->picture), NULL);
        return;
    }
    if (browser->world == ROOM_WORLD_ARIA) {
        if (mode >= G_N_ELEMENTS(aria_suffixes)) return;
        snprintf(filename, sizeof(filename),
                 "assets/extracted/rooms/aria/previews/area_%02u_room_%03u_%s.bmp",
                 browser->area, browser->room, aria_suffixes[mode]);
    } else {
        if (mode >= G_N_ELEMENTS(zero_modes) - 1) return;
        gchar *area_name = g_ascii_strdown(zero_areas[browser->area], -1);
        gchar *layer_name = g_ascii_strdown(zero_modes[mode], -1);
        snprintf(filename, sizeof(filename),
                 "assets/extracted/rooms/metroid/previews/%s_%03u_%s.bmp",
                 area_name, browser->room, layer_name);
        g_free(area_name);
        g_free(layer_name);
    }
    if (g_file_test(filename, G_FILE_TEST_IS_REGULAR))
        gtk_picture_set_filename(GTK_PICTURE(browser->picture), filename);
    else
        gtk_picture_set_paintable(GTK_PICTURE(browser->picture), NULL);
}

static void mode_changed(GObject *object, GParamSpec *pspec, gpointer userdata)
{
    (void)object; (void)pspec;
    display_preview(userdata);
}

static void room_selected(GtkListBox *list, GtkListBoxRow *row, gpointer userdata)
{
    RoomBrowser *browser = userdata;
    (void)list;
    if (!row) {
        browser->selected = FALSE;
        gtk_widget_set_sensitive(browser->open_button, FALSE);
        if (!browser->busy) gtk_widget_set_sensitive(browser->render_button, FALSE);
        gtk_picture_set_paintable(GTK_PICTURE(browser->picture), NULL);
        gtk_label_set_text(GTK_LABEL(browser->details), "Select a verified original room or a private draft.");
        return;
    }
    if (g_object_get_data(G_OBJECT(row), "mv-authored-draft")) {
        /* Never send an authored draft to the source-ROM import/renderer path. */
        browser->selected = FALSE;
        gtk_widget_set_sensitive(browser->open_button, FALSE);
        if (!browser->busy) gtk_widget_set_sensitive(browser->render_button, FALSE);
        gtk_picture_set_paintable(GTK_PICTURE(browser->picture), NULL);
        const char *info = g_object_get_data(G_OBJECT(row), "mv-details");
        gtk_label_set_text(GTK_LABEL(browser->details), info ? info : "Private room draft");
        gtk_label_set_text(GTK_LABEL(browser->status),
            "Private draft selected (NOT playable). Source-room import and rendering are disabled.");
        return;
    }
    gtk_widget_set_sensitive(browser->open_button, TRUE);
    if (!browser->busy) gtk_widget_set_sensitive(browser->render_button, TRUE);
    browser->area = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(row), "mv-area"));
    browser->room = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(row), "mv-room"));
    browser->selected = TRUE;
    const char *details = g_object_get_data(G_OBJECT(row), "mv-details");
    gtk_label_set_text(GTK_LABEL(browser->details), details ? details : "Native room");
    if (!browser->busy)
        gtk_label_set_text(GTK_LABEL(browser->status),
            "Open to edit the project-owned native room, or Render for a source preview.");
    display_preview(browser);
}

static void edit_selected(GtkButton *button, gpointer userdata)
{
    RoomBrowser *browser = userdata;
    (void)button;
    GtkListBoxRow *row = gtk_list_box_get_selected_row(GTK_LIST_BOX(browser->list));
    if (!row || g_object_get_data(G_OBJECT(row), "mv-authored-draft") ||
        !browser->selected || !browser->workspace) return;
    if (browser->world == ROOM_WORLD_ARIA)
        native_workspace_import_aria_async(browser->workspace,
                                           browser->area, browser->room);
    else
        native_workspace_import_async(browser->workspace,
                                      zero_areas[browser->area], browser->room);
}

static void room_activated(GtkListBox *list, GtkListBoxRow *row, gpointer userdata)
{
    room_selected(list, row, userdata);
    edit_selected(NULL, userdata);
}

static void render_complete(GObject *object, GAsyncResult *result, gpointer userdata)
{
    RoomBrowser *browser = userdata;
    GSubprocess *process = G_SUBPROCESS(object);
    GError *error = NULL;
    gchar *output = NULL, *diagnostic = NULL;
    gboolean ok = g_subprocess_communicate_utf8_finish(
        process, result, &output, &diagnostic, &error);
    guint area = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(process), "mv-area"));
    guint room = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(process), "mv-room"));
    browser->busy = FALSE;
    gtk_widget_set_sensitive(browser->render_button, browser->selected);
    if (ok && g_subprocess_get_successful(process)) {
        if (browser->selected && browser->area == area && browser->room == room) {
            display_preview(browser);
            gtk_label_set_text(GTK_LABEL(browser->status),
                "Original room preview generated. Incomplete layers remain diagnostic.");
        } else {
            gtk_label_set_text(GTK_LABEL(browser->status),
                "Previous room rendered; select it again to inspect the preview.");
        }
    } else {
        const char *reason = error ? error->message : diagnostic;
        gchar *msg = g_strdup_printf("Native renderer: %.500s",
                                    reason && *reason ? reason : "Failed without diagnostic");
        gtk_label_set_text(GTK_LABEL(browser->status), msg);
        g_free(msg);
    }
    if (error) g_error_free(error);
    g_free(output);
    g_free(diagnostic);
    g_object_unref(browser->page); /* Strong ref retained until async completion. */
}

static void render_selected(GtkButton *button, gpointer userdata)
{
    RoomBrowser *browser = userdata;
    (void)button;
    GtkListBoxRow *row = gtk_list_box_get_selected_row(GTK_LIST_BOX(browser->list));
    if (!row || g_object_get_data(G_OBJECT(row), "mv-authored-draft") ||
        !browser->selected || browser->busy) return;
    char area[16], room[16];
    snprintf(area, sizeof(area), "%u", browser->area);
    snprintf(room, sizeof(room), "%u", browser->room);
    const char *module = browser->world == ROOM_WORLD_ARIA ?
        "scripts.aos_room_render" : "scripts.mzm_room_render";
    const char *area_arg = browser->world == ROOM_WORLD_ARIA ?
        area : zero_areas[browser->area];
    GError *error = NULL;
    GSubprocess *proc = g_subprocess_new(
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE,
        &error, "python3", "-m", module,
        "--area", area_arg, "--room", room, NULL);
    if (!proc) {
        gtk_label_set_text(GTK_LABEL(browser->status),
                           error ? error->message : "Native renderer unavailable");
        g_clear_error(&error);
        return;
    }
    browser->busy = TRUE;
    gtk_widget_set_sensitive(browser->render_button, FALSE);
    gtk_label_set_text(GTK_LABEL(browser->status),
                       "Decoding original graphics from private local sources...");
    g_object_set_data(G_OBJECT(proc), "mv-area", GUINT_TO_POINTER(browser->area));
    g_object_set_data(G_OBJECT(proc), "mv-room", GUINT_TO_POINTER(browser->room));
    g_object_ref(browser->page);
    g_subprocess_communicate_utf8_async(proc, NULL, NULL,
        render_complete, browser);
    g_object_unref(proc);
}

static gboolean filter_room(GtkListBoxRow *row, gpointer userdata)
{
    RoomBrowser *browser = userdata;
    guint wanted = gtk_drop_down_get_selected(GTK_DROP_DOWN(browser->area_filter));
    return wanted == 0 ||
        GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(row), "mv-area")) == wanted - 1;
}

static void area_changed(GObject *object, GParamSpec *pspec, gpointer userdata)
{
    RoomBrowser *browser = userdata;
    (void)object; (void)pspec;
    gtk_list_box_unselect_all(GTK_LIST_BOX(browser->list));
    gtk_list_box_invalidate_filter(GTK_LIST_BOX(browser->list));
}

static void append_room(RoomBrowser *browser, guint area, guint room,
                        const char *label, gchar *details)
{
    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *text = gtk_label_new(label);
    gtk_label_set_xalign(GTK_LABEL(text), 0.0f);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), text);
    g_object_set_data(G_OBJECT(row), "mv-area", GUINT_TO_POINTER(area));
    g_object_set_data(G_OBJECT(row), "mv-room", GUINT_TO_POINTER(room));
    g_object_set_data_full(G_OBJECT(row), "mv-details", details, g_free);
    gtk_list_box_append(GTK_LIST_BOX(browser->list), row);
}

static gboolean parse_uint(const char *src, guint bound, guint *result)
{
    if (!src || !*src) return FALSE;
    for (const char *p = src; *p; ++p)
        if (!g_ascii_isdigit(*p)) return FALSE;
    gchar *end = NULL;
    guint64 n = g_ascii_strtoull(src, &end, 10);
    if (!end || *end || n >= bound) return FALSE;
    *result = (guint)n;
    return TRUE;
}

static void populate_rooms(RoomBrowser *browser)
{
    const char *path = browser->world == ROOM_WORLD_ARIA ?
        "assets/extracted/rooms/aria/rooms.tsv" :
        "assets/extracted/rooms/metroid/rooms.tsv";
    gchar *contents = NULL;
    if (!g_file_get_contents(path, &contents, NULL, NULL)) {
        gtk_label_set_text(GTK_LABEL(browser->status),
            browser->world == ROOM_WORLD_ARIA ?
            "Aria catalog missing. Run python3 -m scripts.import_aos_world then python3 -m scripts.aos_room_index." :
            "Zero Mission catalog missing. Run python3 -m scripts.import_mzm_rooms.");
        return;
    }
    gchar **lines = g_strsplit(contents, "\n", -1);
    guint count = 0;
    for (guint i = 0; lines[i]; ++i) {
        if (!lines[i][0] || lines[i][0] == '#') continue;
        gchar **fields = g_strsplit(lines[i], "|", -1);
        guint n = g_strv_length(fields);
        guint area = G_MAXUINT, room = 0;
        if (browser->world == ROOM_WORLD_ARIA) {
            if (n == 9 && parse_uint(fields[0], 12, &area) &&
                parse_uint(fields[1], 1000, &room)) {
                gchar *label = g_strdup_printf("%02u:%03u  %s%s%s", area, room,
                    fields[2], strcmp(fields[7], "1") == 0 ? "  [SAVE]" : "",
                    strcmp(fields[8], "1") == 0 ? "  [BOSS]" : "");
                gchar *details = g_strdup_printf(
                    "Aria / %s | engine room %02u:%03u | %sx%s screens\n"
                    "Entities: %s | transitions: %s | save: %s | boss: %s\n"
                    "Original graphics remain diagnostic; authored edits stay private.",
                    fields[2], area, room, fields[3], fields[4], fields[5],
                    fields[6], fields[7], fields[8]);
                append_room(browser, area, room, label, details);
                g_free(label);
                ++count;
            }
        } else if (n == 10) {
            for (guint j = 0; zero_areas[j]; ++j) {
                if (g_strcmp0(fields[0], zero_areas[j]) == 0) { area = j; break; }
            }
            if (area != G_MAXUINT && parse_uint(fields[1], 1000, &room)) {
                gchar *label = g_strdup_printf("%s / room %03u  tileset %s  %s",
                                               fields[0], room, fields[2], fields[3]);
                gchar *details = g_strdup_printf(
                    "Zero Mission / %s / room %03u | tileset %s\n"
                    "Music: %s | BG1: %s | BG2: %s | clip: %s\n"
                    "Sprite set: %s | original minimap anchor: %s,%s",
                    fields[0], room, fields[2], fields[3], fields[4], fields[5],
                    fields[6], fields[7], fields[8], fields[9]);
                append_room(browser, area, room, label, details);
                g_free(label);
                ++count;
            }
        }
        g_strfreev(fields);
    }
    g_strfreev(lines);
    g_free(contents);
    gchar *message = g_strdup_printf("%u verified original room descriptors. Double-click to edit.", count);
    gtk_label_set_text(GTK_LABEL(browser->status), message);
    g_free(message);
}

/* One shared draft creation dialog. The Python schema remains authoritative.
 * No native gameplay, collision, layers or source-ROM editing is claimed. */
typedef struct {
    RoomWorld world;
    GtkWidget *window, *area, *slug, *name, *width, *height;
    GtkWidget *feedback, *submit, *cancel;
    GWeakRef page_ref;
} RoomDraftForm;

typedef struct {
    GWeakRef window_ref;
    GWeakRef page_ref;
    gchar *identity;
} RoomDraftJob;

static void room_draft_form_free(gpointer userdata)
{
    RoomDraftForm *form = userdata;
    g_weak_ref_clear(&form->page_ref);
    g_free(form);
}

static void room_draft_job_free(RoomDraftJob *job)
{
    g_weak_ref_clear(&job->window_ref);
    g_weak_ref_clear(&job->page_ref);
    g_free(job->identity);
    g_free(job);
}

static gboolean valid_draft_slug(const char *slug)
{
    size_t length = strlen(slug);
    if (length < 1 || length > 40 || slug[0] < 'a' || slug[0] > 'z')
        return FALSE;
    for (size_t i = 1; i < length; ++i) {
        char c = slug[i];
        if ((c < 'a' || c > 'z') && (c < '0' || c > '9') &&
            c != '-' && c != '_') return FALSE;
    }
    return TRUE;
}

static gboolean valid_draft_name(const char *name)
{
    if (!g_utf8_validate(name, -1, NULL)) return FALSE;
    glong count = g_utf8_strlen(name, -1);
    if (count < 1 || count > 80 || g_str_has_prefix(name, " ") ||
        g_str_has_suffix(name, " ")) return FALSE;
    for (const char *p = name; *p; p = g_utf8_next_char(p)) {
        gunichar c = g_utf8_get_char(p);
        if (c < 32 || c == 127 || c == '|') return FALSE;
    }
    return TRUE;
}

/* The Python draft schema is the only parser for private authored JSON.
 * GTK consumes a bounded, tab-separated listing with no control characters. */
typedef struct {
    GtkWidget *page; /* Strong reference until subprocess callback completes. */
    guint generation;
} RoomDraftListJob;

static gboolean draft_listing_slug_valid(const char *slug)
{
    return valid_draft_slug(slug);
}

static guint room_browser_replace_drafts(RoomBrowser *browser, const gchar *output)
{
    if (!output || strlen(output) > 1024 * 1024) return 0;
    GtkWidget *child = gtk_widget_get_first_child(browser->list);
    while (child) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        if (GTK_IS_LIST_BOX_ROW(child) &&
            g_object_get_data(G_OBJECT(child), "mv-authored-draft"))
            gtk_list_box_remove(GTK_LIST_BOX(browser->list), child);
        child = next;
    }
    const char *expected = browser->world == ROOM_WORLD_ARIA ? "aria" : "zero_mission";
    const guint area_count = browser->world == ROOM_WORLD_ARIA ? 12u : 7u;
    gchar **lines = g_strsplit(output, "\n", -1);
    guint count = 0;
    for (guint i = 0; lines[i] && i < 4096; ++i) {
        if (!*lines[i]) continue;
        gchar **fields = g_strsplit(lines[i], "\t", -1);
        if (g_strv_length(fields) == 4) {
            gchar **id = g_strsplit(fields[0], ":", -1);
            guint area = 0, width = 0, height = 0;
            if (g_strv_length(id) == 3 && g_strcmp0(id[0], expected) == 0 &&
                parse_uint(id[1], area_count, &area) &&
                draft_listing_slug_valid(id[2]) &&
                valid_draft_name(fields[1]) &&
                parse_uint(fields[2], 9u, &width) && width > 0 &&
                parse_uint(fields[3], 9u, &height) && height > 0) {
                GtkWidget *row = gtk_list_box_row_new();
                gchar *label = g_strdup_printf("[DRAFT] %s (%u x %u screens)",
                                               fields[1], width, height);
                GtkWidget *text = gtk_label_new(label);
                gtk_label_set_xalign(GTK_LABEL(text), 0.0f);
                gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), text);
                g_object_set_data(G_OBJECT(row), "mv-area", GUINT_TO_POINTER(area));
                g_object_set_data(G_OBJECT(row), "mv-authored-draft", GUINT_TO_POINTER(1));
                g_object_set_data_full(G_OBJECT(row), "mv-draft-identity",
                                       g_strdup(fields[0]), g_free);
                gchar *details = g_strdup_printf(
                    "PROJECT DRAFT (not playable)\n%s / %s\nID: %s\n"
                    "Dimensions: %u x %u GBA screens (240x160 px each)\n"
                    "No source-ROM room, layers, collision, runtime or engine adapter exists.",
                    expected, area_names(browser->world)[area], fields[0], width, height);
                g_object_set_data_full(G_OBJECT(row), "mv-details", details, g_free);
                gtk_list_box_append(GTK_LIST_BOX(browser->list), row);
                g_free(label);
                ++count;
            }
            g_strfreev(id);
        }
        g_strfreev(fields);
    }
    g_strfreev(lines);
    gtk_list_box_invalidate_filter(GTK_LIST_BOX(browser->list));
    return count;
}

static void room_drafts_loaded(GObject *object, GAsyncResult *result, gpointer userdata)
{
    RoomDraftListJob *job = userdata;
    GError *error = NULL;
    gchar *output = NULL, *diagnostic = NULL;
    gboolean ok = g_subprocess_communicate_utf8_finish(
        G_SUBPROCESS(object), result, &output, &diagnostic, &error);
    RoomBrowser *browser = g_object_get_data(G_OBJECT(job->page), "mv-room-browser-state");
    /* Closing/reloading a tab never revives it or mutates a detached widget. */
    if (browser && gtk_widget_get_parent(job->page) &&
        job->generation == browser->draft_refresh_generation) {
        if (ok && g_subprocess_get_successful(G_SUBPROCESS(object)) &&
            output && strlen(output) <= 1024 * 1024) {
            guint count = room_browser_replace_drafts(browser, output);
            if (count) {
                gchar *message = g_strdup_printf(
                    "%u private authored drafts listed (NOT playable). "
                    "Original room imports remain separate.", count);
                gtk_label_set_text(GTK_LABEL(browser->status), message);
                g_free(message);
            }
        } else {
            const char *reason = error ? error->message : diagnostic;
            gchar *message = g_strdup_printf("Draft list unavailable: %.400s",
                reason && *reason ? reason : "Python listing failed");
            gtk_label_set_text(GTK_LABEL(browser->status), message);
            g_free(message);
        }
    }
    g_clear_error(&error);
    g_free(output);
    g_free(diagnostic);
    g_object_unref(job->page);
    g_free(job);
}

static void room_browser_refresh_drafts(RoomBrowser *browser)
{
    const char *world = browser->world == ROOM_WORLD_ARIA ? "aria" : "zero_mission";
    GError *error = NULL;
    GSubprocess *process = g_subprocess_new(
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE,
        &error, "python3", "-m", "scripts.authored_rooms", "list",
        "--world", world, "--format", "tsv", NULL);
    if (!process) {
        gtk_label_set_text(GTK_LABEL(browser->status),
            error ? error->message : "Unable to start private draft listing.");
        g_clear_error(&error);
        return;
    }
    RoomDraftListJob *job = g_new0(RoomDraftListJob, 1);
    job->page = g_object_ref(browser->page);
    job->generation = ++browser->draft_refresh_generation;
    g_subprocess_communicate_utf8_async(process, NULL, NULL, room_drafts_loaded, job);
    g_object_unref(process);
}

static void room_draft_refresh_clicked(GtkButton *button, gpointer userdata)
{
    GtkWidget *page = GTK_WIDGET(userdata);
    RoomBrowser *browser = g_object_get_data(G_OBJECT(page), "mv-room-browser-state");
    (void)button;
    if (browser) room_browser_refresh_drafts(browser);
}

#ifdef FUSION_ROOM_BROWSER_TESTING
/* Test-only parser entry point; no ROM assets or private files required. */
void room_browser_test_replace_drafts(GtkWidget *page, const gchar *tsv)
{
    RoomBrowser *browser = g_object_get_data(G_OBJECT(page), "mv-room-browser-state");
    g_assert_nonnull(browser);
    room_browser_replace_drafts(browser, tsv);
}
#endif

static void room_draft_done(GObject *object, GAsyncResult *result, gpointer userdata)
{
    RoomDraftJob *job = userdata;
    GError *error = NULL;
    gchar *output = NULL, *diagnostic = NULL;
    gboolean communicated = g_subprocess_communicate_utf8_finish(
        G_SUBPROCESS(object), result, &output, &diagnostic, &error);
    gboolean created = communicated && g_subprocess_get_successful(G_SUBPROCESS(object));
    GtkWidget *window = g_weak_ref_get(&job->window_ref);
    GtkWidget *page = g_weak_ref_get(&job->page_ref);

    if (created && page && GTK_IS_WINDOW(gtk_widget_get_root(page))) {
        RoomBrowser *browser = g_object_get_data(G_OBJECT(page), "mv-room-browser-state");
        if (browser) {
            room_browser_refresh_drafts(browser);
            gchar *message = g_strdup_printf(
                "Created private draft %s (NOT playable). Updating browser list...",
                job->identity);
            gtk_label_set_text(GTK_LABEL(browser->status), message);
            g_free(message);
        }
    }

    if (window && GTK_IS_WINDOW(window)) {
        RoomDraftForm *form = g_object_get_data(G_OBJECT(window), "mv-room-draft-form");
        if (form && created) {
            gtk_window_destroy(GTK_WINDOW(window));
        } else if (form) {
            const char *reason = error ? error->message : diagnostic;
            gchar *message = g_strdup_printf("Create failed: %.650s",
                reason && *reason ? reason : "Unknown Python error");
            gtk_label_set_text(GTK_LABEL(form->feedback), message);
            g_free(message);
            gtk_widget_set_sensitive(form->submit, TRUE);
            gtk_widget_set_sensitive(form->cancel, TRUE);
            gtk_window_set_deletable(GTK_WINDOW(window), TRUE);
        }
    }
    g_clear_object(&page);
    g_clear_object(&window);
    g_clear_error(&error);
    g_free(output);
    g_free(diagnostic);
    room_draft_job_free(job);
}

static void room_draft_cancel(GtkButton *button, gpointer userdata)
{
    RoomDraftForm *form = userdata;
    (void)button;
    gtk_window_destroy(GTK_WINDOW(form->window));
}

static void room_draft_submit(GtkButton *button, gpointer userdata)
{
    RoomDraftForm *form = userdata;
    const char *slug = gtk_editable_get_text(GTK_EDITABLE(form->slug));
    const char *name = gtk_editable_get_text(GTK_EDITABLE(form->name));
    guint area = gtk_drop_down_get_selected(GTK_DROP_DOWN(form->area));
    char area_buf[12], width_buf[12], height_buf[12];
    const char *world = form->world == ROOM_WORLD_ARIA ? "aria" : "zero_mission";
    (void)button;
    if (!valid_draft_slug(slug)) {
        gtk_label_set_text(GTK_LABEL(form->feedback),
            "Slug: lowercase a-z first, then a-z, 0-9, '-' or '_'; max 40 characters.");
        return;
    }
    if (!valid_draft_name(name)) {
        gtk_label_set_text(GTK_LABEL(form->feedback),
            "Name: 1-80 printable characters, no outer spaces or '|'.");
        return;
    }
    if (area >= (form->world == ROOM_WORLD_ARIA ? 12u : 7u)) {
        gtk_label_set_text(GTK_LABEL(form->feedback), "Invalid area selection.");
        return;
    }
    if (!g_file_test("scripts/authored_rooms.py", G_FILE_TEST_IS_REGULAR)) {
        gtk_label_set_text(GTK_LABEL(form->feedback),
            "Run the editor from the metroidvania project root (scripts/authored_rooms.py missing).");
        return;
    }

    snprintf(area_buf, sizeof(area_buf), "%u", area);
    snprintf(width_buf, sizeof(width_buf), "%d",
             gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(form->width)));
    snprintf(height_buf, sizeof(height_buf), "%d",
             gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(form->height)));
    GError *error = NULL;
    /* Argument vector; no shell, no ROM file paths, create-only Python CLI. */
    GSubprocess *process = g_subprocess_new(
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE,
        &error, "python3", "-m", "scripts.authored_rooms", "create",
        "--world", world, "--area", area_buf, "--slug", slug,
        "--name", name, "--width-screens", width_buf,
        "--height-screens", height_buf, NULL);
    if (!process) {
        gtk_label_set_text(GTK_LABEL(form->feedback),
            error ? error->message : "Failed to launch Python.");
        g_clear_error(&error);
        return;
    }

    RoomDraftJob *job = g_new0(RoomDraftJob, 1);
    job->identity = g_strdup_printf("%s:%02u:%s", world, area, slug);
    g_weak_ref_init(&job->window_ref, form->window);
    GtkWidget *page = g_weak_ref_get(&form->page_ref);
    g_weak_ref_init(&job->page_ref, page);
    g_clear_object(&page);
    gtk_widget_set_sensitive(form->submit, FALSE);
    gtk_widget_set_sensitive(form->cancel, FALSE);
    gtk_window_set_deletable(GTK_WINDOW(form->window), FALSE);
    gtk_label_set_text(GTK_LABEL(form->feedback), "Creating private draft...");
    g_subprocess_communicate_utf8_async(process, NULL, NULL, room_draft_done, job);
    g_object_unref(process);
}

static void room_draft_grid_row(GtkGrid *grid, int row,
                                const char *label, GtkWidget *field)
{
    GtkWidget *text = gtk_label_new(label);
    gtk_label_set_xalign(GTK_LABEL(text), 0.0f);
    gtk_widget_set_hexpand(field, TRUE);
    gtk_grid_attach(grid, text, 0, row, 1, 1);
    gtk_grid_attach(grid, field, 1, row, 1, 1);
}

static void room_draft_open(GtkButton *button, gpointer userdata)
{
    GtkWidget *page = GTK_WIDGET(userdata);
    RoomBrowser *browser = g_object_get_data(G_OBJECT(page), "mv-room-browser-state");
    if (!browser) return;
    (void)button;
    GtkPopover *popover = g_object_get_data(G_OBJECT(page), "mv-create-popover");
    if (GTK_IS_POPOVER(popover)) gtk_popover_popdown(popover);

    RoomDraftForm *form = g_new0(RoomDraftForm, 1);
    form->world = browser->world;
    g_weak_ref_init(&form->page_ref, page);
    form->window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(form->window),
        form->world == ROOM_WORLD_ARIA ? "Create Aria room draft" : "Create Zero room draft");
    gtk_window_set_default_size(GTK_WINDOW(form->window), 440, 340);
    gtk_window_set_modal(GTK_WINDOW(form->window), TRUE);
    GtkRoot *root = gtk_widget_get_root(page);
    if (GTK_IS_WINDOW(root)) {
        gtk_window_set_transient_for(GTK_WINDOW(form->window), GTK_WINDOW(root));
        gtk_window_set_destroy_with_parent(GTK_WINDOW(form->window), TRUE);
    }

    GtkWidget *body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_widget_set_margin_start(body, 18);
    gtk_widget_set_margin_end(body, 18);
    gtk_widget_set_margin_top(body, 18);
    gtk_widget_set_margin_bottom(body, 18);
    gtk_window_set_child(GTK_WINDOW(form->window), body);
    GtkWidget *warning = gtk_label_new(
        "Project-authored draft only. No graphics, collision or gameplay runtime is created.");
    gtk_label_set_wrap(GTK_LABEL(warning), TRUE);
    gtk_label_set_selectable(GTK_LABEL(warning), TRUE);
    gtk_label_set_xalign(GTK_LABEL(warning), 0.0f);
    gtk_box_append(GTK_BOX(body), warning);
    gtk_box_append(GTK_BOX(body), grid);

    GtkStringList *areas = gtk_string_list_new(area_names(form->world));
    /* gtk_drop_down_new() consumes its model reference. */
    form->area = gtk_drop_down_new(G_LIST_MODEL(areas), NULL);
    guint selected_area = gtk_drop_down_get_selected(GTK_DROP_DOWN(browser->area_filter));
    gtk_drop_down_set_selected(GTK_DROP_DOWN(form->area),
        selected_area > 0 ? selected_area - 1 : 0);
    form->slug = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(form->slug), "my_new_room");
    gtk_entry_set_max_length(GTK_ENTRY(form->slug), 40);
    form->name = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(form->name), "Room name");
    gtk_entry_set_max_length(GTK_ENTRY(form->name), 80);
    form->width = gtk_spin_button_new_with_range(1, 8, 1);
    form->height = gtk_spin_button_new_with_range(1, 8, 1);
    room_draft_grid_row(GTK_GRID(grid), 0, "Area", form->area);
    room_draft_grid_row(GTK_GRID(grid), 1, "Slug", form->slug);
    room_draft_grid_row(GTK_GRID(grid), 2, "Name", form->name);
    room_draft_grid_row(GTK_GRID(grid), 3, "Width (screens)", form->width);
    room_draft_grid_row(GTK_GRID(grid), 4, "Height (screens)", form->height);

    form->feedback = gtk_label_new("Creates a validated, non-playable JSON draft.");
    gtk_label_set_xalign(GTK_LABEL(form->feedback), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(form->feedback), TRUE);
    gtk_label_set_selectable(GTK_LABEL(form->feedback), TRUE);
    gtk_box_append(GTK_BOX(body), form->feedback);
    GtkWidget *actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(actions, GTK_ALIGN_END);
    form->cancel = gtk_button_new_with_label("Cancel");
    form->submit = gtk_button_new_with_label("Create draft");
    gtk_box_append(GTK_BOX(actions), form->cancel);
    gtk_box_append(GTK_BOX(actions), form->submit);
    gtk_box_append(GTK_BOX(body), actions);
    g_signal_connect(form->cancel, "clicked", G_CALLBACK(room_draft_cancel), form);
    g_signal_connect(form->submit, "clicked", G_CALLBACK(room_draft_submit), form);

    g_object_set_data_full(G_OBJECT(form->window), "mv-room-draft-form",
                           form, room_draft_form_free);
    /* Non-owning test inspection handles; window owns these controls. */
    g_object_set_data(G_OBJECT(form->window), "mv-room-draft-world",
                      GUINT_TO_POINTER((guint)form->world + 1));
    g_object_set_data(G_OBJECT(form->window), "mv-room-draft-area", form->area);
    gtk_window_present(GTK_WINDOW(form->window));
}

static void show_context(GtkGestureClick *gesture, gint presses,
                         double x, double y, gpointer userdata)
{
    RoomBrowser *browser = userdata;
    (void)gesture; (void)presses; (void)x; (void)y;
    /* GtkMenuButton is a supported GtkPopover host in GTK4. Do not attach
     * native popovers to GtkListBox or to a GtkOverlay allocation slot. */
    if (browser->context_menu_button &&
        gtk_widget_get_root(browser->context_menu_button))
        gtk_menu_button_popup(GTK_MENU_BUTTON(browser->context_menu_button));
}

GtkWidget *room_browser_build(GtkWidget *center, NativeWorkspace *workspace,
                              RoomWorld world)
{
    RoomBrowser *browser = g_new0(RoomBrowser, 1);
    browser->world = world;
    browser->workspace = workspace;
    browser->page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    browser->list = gtk_list_box_new();
    browser->picture = gtk_picture_new();
    browser->details = gtk_label_new("Select a verified original room.");
    browser->status = gtk_label_new("Loading room catalog...");
    browser->render_button = gtk_button_new_with_label("Render selected room");
    browser->mode = gtk_drop_down_new_from_strings(
        world == ROOM_WORLD_ARIA ? aria_modes : zero_modes);
    GtkWidget *open = gtk_button_new_with_label("Open room in editor");
    GtkWidget *reload = gtk_button_new_with_label("Refresh drafts");
    browser->open_button = open;
    gtk_widget_set_sensitive(open, FALSE);
    gtk_widget_set_sensitive(browser->render_button, FALSE);
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *layout = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *left = gtk_scrolled_window_new();
    GtkWidget *right = gtk_scrolled_window_new();
    GtkWidget *menu_button = gtk_menu_button_new();
    browser->context_menu_button = menu_button;
    GtkStringList *areas = gtk_string_list_new(NULL);
    gtk_string_list_append(areas, "All areas");
    for (const char *const *a = area_names(world); *a; ++a)
        gtk_string_list_append(areas, *a);
    /* gtk_drop_down_new() consumes the model reference. Do not unref it. */
    browser->area_filter = gtk_drop_down_new(G_LIST_MODEL(areas), NULL);

    gtk_widget_set_margin_start(browser->page, 10);
    gtk_widget_set_margin_end(browser->page, 10);
    gtk_widget_set_margin_top(browser->page, 8);
    gtk_widget_set_margin_bottom(browser->page, 8);
    gtk_box_append(GTK_BOX(toolbar), gtk_label_new("Area:"));
    gtk_box_append(GTK_BOX(toolbar), browser->area_filter);
    gtk_box_append(GTK_BOX(toolbar), gtk_label_new("Layer:"));
    gtk_box_append(GTK_BOX(toolbar), browser->mode);
    gtk_box_append(GTK_BOX(toolbar), browser->render_button);
    gtk_box_append(GTK_BOX(toolbar), open);
    gtk_box_append(GTK_BOX(toolbar), reload);
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(menu_button), "view-more-symbolic");
    gtk_widget_set_tooltip_text(menu_button, "Room actions (also via right-click)");
    gtk_box_append(GTK_BOX(toolbar), menu_button);
    gtk_box_append(GTK_BOX(browser->page), toolbar);
    gtk_label_set_xalign(GTK_LABEL(browser->details), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(browser->details), TRUE);
    gtk_label_set_selectable(GTK_LABEL(browser->details), TRUE);
    gtk_box_append(GTK_BOX(browser->page), browser->details);
    gtk_widget_set_hexpand(layout, TRUE);
    gtk_widget_set_vexpand(layout, TRUE);
    gtk_box_append(GTK_BOX(browser->page), layout);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(left), browser->list);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(left),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(left, 250, -1);
    gtk_paned_set_start_child(GTK_PANED(layout), left);
    gtk_paned_set_end_child(GTK_PANED(layout), right);
    gtk_paned_set_position(GTK_PANED(layout), 360);
    gtk_picture_set_can_shrink(GTK_PICTURE(browser->picture), TRUE);
    gtk_widget_set_hexpand(browser->picture, TRUE);
    gtk_widget_set_vexpand(browser->picture, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(right), browser->picture);
    gtk_label_set_xalign(GTK_LABEL(browser->status), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(browser->status), TRUE);
    gtk_label_set_selectable(GTK_LABEL(browser->status), TRUE);
    gtk_box_append(GTK_BOX(browser->page), browser->status);

    /* Draft creation is available; gameplay/export remains unavailable. */
    GtkWidget *popover = gtk_popover_new();
    GtkWidget *note = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *create = gtk_button_new_with_label("Create room...");
    GtkWidget *hint = gtk_label_new(
        "Creates a private project draft only. Native gameplay is not implemented.");
    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_label_set_selectable(GTK_LABEL(hint), TRUE);
    gtk_widget_set_size_request(hint, 245, -1);
    gtk_box_append(GTK_BOX(note), create);
    gtk_box_append(GTK_BOX(note), hint);
    gtk_popover_set_child(GTK_POPOVER(popover), note);
    /* GtkMenuButton owns its popover and correctly manages its lifecycle. */
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(menu_button), popover);
    g_object_set_data(G_OBJECT(browser->page), "mv-create-popover", popover);
    g_object_set_data(G_OBJECT(browser->page), "mv-create-room-action", create);
    g_signal_connect_object(create, "clicked", G_CALLBACK(room_draft_open),
                            browser->page, 0);
    g_object_set_data(G_OBJECT(browser->page), "mv-room-browser-menu-button", menu_button);
    GtkGesture *right_click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(right_click), GDK_BUTTON_SECONDARY);
    gtk_widget_add_controller(browser->list, GTK_EVENT_CONTROLLER(right_click));
    g_signal_connect(right_click, "pressed", G_CALLBACK(show_context), browser);

    gtk_list_box_set_filter_func(GTK_LIST_BOX(browser->list), filter_room, browser, NULL);
    populate_rooms(browser);
    g_signal_connect(browser->list, "row-selected", G_CALLBACK(room_selected), browser);
    g_signal_connect(browser->list, "row-activated", G_CALLBACK(room_activated), browser);
    g_signal_connect(browser->area_filter, "notify::selected",
                     G_CALLBACK(area_changed), browser);
    g_signal_connect(browser->mode, "notify::selected", G_CALLBACK(mode_changed), browser);
    g_signal_connect(browser->render_button, "clicked", G_CALLBACK(render_selected), browser);
    g_signal_connect(open, "clicked", G_CALLBACK(edit_selected), browser);
    g_signal_connect_object(reload, "clicked", G_CALLBACK(room_draft_refresh_clicked),
                            browser->page, 0);
    g_object_set_data(G_OBJECT(browser->page), "mv-draft-refresh-action", reload);
    g_object_set_data(G_OBJECT(browser->page), "mv-room-open-action", open);

    /* The GTK container may dispose the toolbar before the filtered list.
     * Independent strong references prevent callbacks from seeing stale
     * GtkDropDown/GtkLabel/GtkPicture pointers during page destruction. */
    g_object_ref(browser->list);
    g_object_ref(browser->mode);
    g_object_ref(browser->area_filter);
    g_object_ref(browser->render_button);
    g_object_ref(browser->open_button);
    g_object_ref(browser->status);
    g_object_ref(browser->details);
    g_object_ref(browser->picture);
    g_object_ref(browser->context_menu_button);
    g_object_set_data_full(G_OBJECT(browser->page), "mv-room-browser-state",
                           browser, room_browser_state_free);
    g_object_set_data(G_OBJECT(browser->page), "mv-world-mode",
                      GUINT_TO_POINTER(world == ROOM_WORLD_ARIA ? 2 : 1));
    g_object_set_data(G_OBJECT(browser->page), "mv-room-browser-list", browser->list);
    gtk_notebook_append_page(GTK_NOTEBOOK(center), browser->page,
        gtk_label_new(world == ROOM_WORLD_ARIA ? "Aria rooms" : "Zero rooms"));
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(center), browser->page, TRUE);
    gtk_notebook_set_tab_detachable(GTK_NOTEBOOK(center), browser->page, TRUE);
#ifndef FUSION_ROOM_BROWSER_TESTING
    room_browser_refresh_drafts(browser);
#endif
    return browser->page;
}
