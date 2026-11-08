/* SPDX-License-Identifier: GPL-3.0-only */
/* GTK4 laboratory viewer for the Aria emulator trial database.
 *
 * Polls the local SQLite database and shows the learning registry live:
 * eight parallel workers with pause/resume/stop, a live log streamed in
 * memory (nothing is written to disk by this app), recent trials,
 * per-parameter success weights, stored save states and worker runs.
 * Fully offline.
 *
 * Usage: aos_lab_viewer [--db PATH] [--rom PATH]
 */
#define _GNU_SOURCE

#include "aos_lab_db.h"

#include <gtk/gtk.h>

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#define POLL_MS 1000
#define MAX_ROWS 400
#define FIELD_SEP '\x1f'
#define MAX_LOG_BYTES (256 * 1024)

typedef struct {
    GtkWidget *card;
    GtkWidget *name;
    GtkWidget *phase;
    GtkWidget *detail;
    GtkWidget *pause_button;
    GtkWidget *speed_scale;
    GtkWidget *speed_label;
    GtkWidget *picture;
    /* live framebuffer mirror published by the worker via shm */
    int fb_fd;
    volatile uint64_t *fb_seq;
    const uint32_t *fb_pix;
    uint64_t last_seq;
    uint8_t *fb_rgba;
    GdkTexture *fb_tex;
} WorkerCard;

typedef struct {
    int fd;
    int worker_id;
    GtkTextTag *tag;
} LogPipe;

typedef struct {
    GtkWidget *window;
    GtkWidget *notebook;
    GListStore *trials_store;
    GListStore *weights_store;
    GListStore *states_store;
    GListStore *runs_store;
    GtkWidget *status;
    GtkWidget *score_label;
    GtkWidget *all_button;
    GtkWidget *all_stop_button;
    GtkWidget *all_speed_scale;
    int all_paused;
    GtkWidget *hint;
    GtkWidget *worker_label;
    GtkWidget *launch_button;
    GtkWidget *record_button;
    GtkWidget *iterations_spin;
    GtkWidget *worker_id_spin;
    GtkWidget *state_dropdown;
    GtkWidget *log_view;
    GtkTextBuffer *log_buffer;
    WorkerCard workers[8];
    char db_path[1024];
    char rom_path[1024];
    char worker_path[4200];
    AosLabDb *db;
    int last_state_count;
    GList *pipes;
    GPid pids[8];      /* live child worker per slot, -1 when none */
} Viewer;

static Viewer *g_viewer; /* set in on_activate; shared by log watches */

/* ---- list view helpers --------------------------------------------- */

static void col_setup(GtkListItemFactory *factory, GtkListItem *item,
                      gpointer user_data) {
    (void)factory; (void)user_data;
    gtk_list_item_set_child(item, gtk_label_new(NULL));
}

static void col_bind(GtkListItemFactory *factory, GtkListItem *item,
                     gpointer user_data) {
    (void)factory;
    int field = GPOINTER_TO_INT(user_data);
    GtkWidget *label = gtk_list_item_get_child(item);
    GtkStringObject *obj =
        GTK_STRING_OBJECT(gtk_list_item_get_item(item));
    const char *text = gtk_string_object_get_string(obj);
    const char *start = text;
    for (int i = 0; i < field && start; ++i) {
        start = strchr(start, FIELD_SEP);
        if (start) ++start;
    }
    char buf[512];
    if (!start) start = "";
    snprintf(buf, sizeof buf, "%s", start);
    char *sep = strchr(buf, FIELD_SEP);
    if (sep) *sep = 0;
    gtk_label_set_text(GTK_LABEL(label), buf);
}

/* Add one text column bound to field `field` of the row string. */
static void add_column(GtkWidget *column_view, const char *title, int field,
                       const char *tip) {
    GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
    g_signal_connect(factory, "setup", G_CALLBACK(col_setup), NULL);
    g_signal_connect(factory, "bind", G_CALLBACK(col_bind),
                     GINT_TO_POINTER(field));
    GtkColumnViewColumn *col = gtk_column_view_column_new(title, factory);
    (void)tip; /* GtkColumnViewColumn is not a widget: no tooltips */
    gtk_column_view_append_column(GTK_COLUMN_VIEW(column_view), col);
}

static GtkWidget *scrolled_page(GListStore **store_out,
                                const char **titles,
                                const char **tips) {
    GListStore *store = g_list_store_new(GTK_TYPE_STRING_OBJECT);
    GtkSingleSelection *selection =
        gtk_single_selection_new(G_LIST_MODEL(store));
    GtkWidget *column_view =
        gtk_column_view_new(GTK_SELECTION_MODEL(selection));
    for (int i = 0; titles[i]; ++i)
        add_column(column_view, titles[i], i, tips ? tips[i] : NULL);
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    /* give the page a real minimum so the internal scrollbar gizmos
     * never measure against a zero allocation (min size -2 warnings) */
    gtk_widget_set_size_request(scroll, 420, 200);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), column_view);
    *store_out = store;
    return scroll;
}

static void store_row(GListStore *store, const char *line) {
    if (!store || !G_IS_LIST_STORE(store)) return;
    GtkStringObject *obj = gtk_string_object_new(line);
    g_list_store_append(store, obj);
    g_object_unref(obj);
}

/* Build a '\x1f'-joined row. */
static void row_printf(GString *out, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    for (const char *p = buf; *p; ++p)
        g_string_append_c(out, *p == '|' ? ' ' : *p);
    g_string_append_c(out, FIELD_SEP);
}

/* ---- live log (in-memory only, worker stdout is piped here) ---------- */

static void log_append(Viewer *v, int worker_id, const char *text,
                       gsize len) {
    if (!v->log_buffer) return;
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(v->log_buffer, &end);
    if (worker_id >= 0) {
        char prefix[16];
        snprintf(prefix, sizeof prefix, "[W%d] ", worker_id);
        gtk_text_buffer_insert_with_tags_by_name(
            v->log_buffer, &end, prefix, -1, "worker_tag", NULL);
    }
    gtk_text_buffer_insert(v->log_buffer, &end, text, len);
    /* trim the buffer so the log never grows without bound */
    GtkTextIter start;
    gtk_text_buffer_get_start_iter(v->log_buffer, &start);
    gtk_text_iter_forward_chars(&start, 4096);
    if (gtk_text_iter_get_offset(&start) > 0 &&
        gtk_text_buffer_get_char_count(v->log_buffer) > MAX_LOG_BYTES)
        gtk_text_buffer_delete(v->log_buffer, &start, &end);
    /* autoscroll */
    GtkWidget *scroll = gtk_widget_get_ancestor(v->log_view,
                                                GTK_TYPE_SCROLLED_WINDOW);
    if (scroll) {
        GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment(
            GTK_SCROLLED_WINDOW(scroll));
        if (adj)
            gtk_adjustment_set_value(adj, gtk_adjustment_get_upper(adj));
    }
}

static gboolean on_log_pipe(GIOChannel *channel, GIOCondition cond,
                            gpointer data) {
    LogPipe *pipe = data;
    Viewer *v = g_viewer;
    if (cond & (G_IO_HUP | G_IO_ERR | G_IO_NVAL)) {
        /* do NOT unref the channel here: g_io_add_watch holds its own
         * reference and frees it when the watch is removed — an extra
         * unref inside the callback double-frees and segfaults when
         * several workers exit at once (the "stop all" crash) */
        v->pipes = g_list_remove(v->pipes, pipe);
        g_free(pipe);
        return G_SOURCE_REMOVE;
    }
    char buf[4096];
    gsize bytes = 0;
    GIOStatus st = g_io_channel_read_chars(channel, buf, sizeof buf,
                                           &bytes, NULL);
    if (st == G_IO_STATUS_NORMAL && bytes > 0) {
        /* strip trailing newline; the tag prefix marks the worker */
        while (bytes && (buf[bytes - 1] == '\n' || buf[bytes - 1] == '\r'))
            --bytes;
        if (bytes) {
            log_append(v, pipe->worker_id, buf, bytes);
            log_append(v, -1, "\n", 1);
        }
    }
    return G_SOURCE_CONTINUE;
}

/* Spawn a worker with stdout+stderr merged into one pipe watched by GTK.
 * Returns TRUE on success. Nothing touches the disk. */
static gboolean spawn_worker_keys(Viewer *v, const char *state_id_str,
                                  int iterations, int worker_id,
                                  const char *record_name, int record_frames,
                                  const char *keys_spec);

static gboolean spawn_worker(Viewer *v, const char *state_id_str,
                             int iterations, int worker_id,
                             const char *record_name, int record_frames) {
    return spawn_worker_keys(v, state_id_str, iterations, worker_id,
                             record_name, record_frames, NULL);
}

/* Respawn a worker from the launch config its last process persisted
 * in worker.N.launch ("state=ID|iters=N|mode=trial|frames=F" or
 * "mode=scout|chunks=C"). Returns FALSE when nothing is stored. */
static gboolean spawn_from_launch(Viewer *v, int worker_id) {
    char key[64];
    snprintf(key, sizeof key, "worker.%d.launch", worker_id);
    char *cfg = aos_labdb_meta_get(v->db, key);
    if (!cfg) return FALSE;
    char mode[16] = "", state[24] = "", iters[16] = "", frames[16] = "",
         chunks[16] = "";
    char *save = NULL;
    for (char *tok = strtok_r(cfg, "|", &save); tok;
         tok = strtok_r(NULL, "|", &save)) {
        char *eq = strchr(tok, '=');
        if (!eq) continue;
        *eq = 0;
        const char *val = eq + 1;
        if (!strcmp(tok, "mode")) snprintf(mode, sizeof mode, "%s", val);
        else if (!strcmp(tok, "state"))
            snprintf(state, sizeof state, "%s", val);
        else if (!strcmp(tok, "iters"))
            snprintf(iters, sizeof iters, "%s", val);
        else if (!strcmp(tok, "frames"))
            snprintf(frames, sizeof frames, "%s", val);
        else if (!strcmp(tok, "chunks"))
            snprintf(chunks, sizeof chunks, "%s", val);
    }
    free(cfg);
    if (!v->rom_path[0] || !v->worker_path[0]) {
        gtk_label_set_text(GTK_LABEL(v->worker_label),
                           "Set --rom to respawn workers");
        return FALSE;
    }
    char seed[32], wid[8];
    snprintf(seed, sizeof seed, "%ld",
             (long)(time(NULL) ^ (getpid() << (worker_id & 7))));
    snprintf(wid, sizeof wid, "%d", worker_id);
    const char *argv[24];
    int n = 0;
    argv[n++] = v->worker_path;
    argv[n++] = "--db";   argv[n++] = v->db_path;
    argv[n++] = "--rom";  argv[n++] = v->rom_path;
    argv[n++] = "--worker-id"; argv[n++] = wid;
    argv[n++] = "--seed"; argv[n++] = seed;
    if (!strcmp(mode, "scout")) {
        argv[n++] = "--scout";
        argv[n++] = "--scout-chunks";
        argv[n++] = chunks[0] ? chunks : "400";
    } else {
        argv[n++] = "--state";
        argv[n++] = state[0] ? state : "0";
        argv[n++] = "--iterations";
        argv[n++] = iters[0] ? iters : "100";
        if (frames[0]) { argv[n++] = "--frames"; argv[n++] = frames; }
    }
    argv[n] = NULL;
    int out_fd = -1, err_fd = -1;
    GPid pid = 0;
    GError *error = NULL;
    if (!g_spawn_async_with_pipes(
            NULL, (char **)argv, NULL,
            G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
            NULL, NULL, &pid, NULL, &out_fd, &err_fd, &error)) {
        gtk_label_set_text(GTK_LABEL(v->worker_label),
                           error ? error->message : "spawn failed");
        if (error) g_error_free(error);
        return FALSE;
    }
    if (worker_id >= 0 && worker_id < 8) v->pids[worker_id] = pid;
    for (int s = 0; s < 2; ++s) {
        int fd = s == 0 ? out_fd : err_fd;
        if (fd < 0) continue;
        GIOChannel *ch = g_io_channel_unix_new(fd);
        g_io_channel_set_encoding(ch, NULL, NULL);
        g_io_channel_set_buffered(ch, FALSE);
        LogPipe *pipe = g_new0(LogPipe, 1);
        pipe->fd = fd;
        pipe->worker_id = worker_id;
        char tagname[32];
        snprintf(tagname, sizeof tagname, "w%d", worker_id);
        GtkTextTagTable *tags =
            gtk_text_buffer_get_tag_table(v->log_buffer);
        pipe->tag = gtk_text_tag_table_lookup(tags, tagname);
        if (!pipe->tag) {
            pipe->tag = gtk_text_tag_new(tagname);
            gtk_text_tag_table_add(tags, pipe->tag);
        }
        v->pipes = g_list_prepend(v->pipes, pipe);
        g_io_add_watch(ch, G_IO_IN | G_IO_HUP,
                       (GIOFunc)on_log_pipe, pipe);
    }
    char msg[96];
    snprintf(msg, sizeof msg, "worker %d respawned (%s)", worker_id, mode);
    gtk_label_set_text(GTK_LABEL(v->worker_label), msg);
    return TRUE;
}

static gboolean spawn_worker_keys(Viewer *v, const char *state_id_str,
                                  int iterations, int worker_id,
                                  const char *record_name, int record_frames,
                                  const char *keys_spec) {
    if (!v->rom_path[0] || !v->worker_path[0]) {
        gtk_label_set_text(GTK_LABEL(v->worker_label),
                           "Set --rom and keep aos_lab_worker next to "
                           "this app");
        return FALSE;
    }
    char seed[32];
    snprintf(seed, sizeof seed, "%ld",
             (long)(time(NULL) ^ (getpid() << (worker_id & 7))));

    char state_buf[32], iters_buf[32], wid_buf[8], frames_buf[32];
    snprintf(state_buf, sizeof state_buf, "%s",
             state_id_str ? state_id_str : "0");
    snprintf(iters_buf, sizeof iters_buf, "%d", iterations);
    snprintf(wid_buf, sizeof wid_buf, "%d", worker_id);
    snprintf(frames_buf, sizeof frames_buf, "%d", record_frames);

    const char *argv[32];
    int n = 0;
    argv[n++] = v->worker_path;
    argv[n++] = "--db";       argv[n++] = v->db_path;
    argv[n++] = "--rom";      argv[n++] = v->rom_path;
    if (record_name) {
        argv[n++] = "--record-state"; argv[n++] = record_name;
        argv[n++] = "--frames";        argv[n++] = frames_buf;
        argv[n++] = "--state-note";    argv[n++] = "captured from viewer";
        if (keys_spec) {
            argv[n++] = "--keys";      argv[n++] = keys_spec;
        }
    } else {
        argv[n++] = "--state";     argv[n++] = state_buf;
        argv[n++] = "--iterations"; argv[n++] = iters_buf;
        argv[n++] = "--worker-id";  argv[n++] = wid_buf;
        argv[n++] = "--seed";       argv[n++] = seed;
    }
    argv[n] = NULL;

    int stdout_fd = -1, stderr_fd = -1;
    GPid pid = 0;
    GError *error = NULL;
    gboolean ok = g_spawn_async_with_pipes(
        NULL, (char **)argv, NULL,
        G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
        NULL, NULL, &pid, NULL, &stdout_fd, &stderr_fd, &error);
    if (!ok) {
        char msg[300];
        snprintf(msg, sizeof msg, "spawn failed: %s",
                 error ? error->message : "unknown");
        gtk_label_set_text(GTK_LABEL(v->worker_label), msg);
        if (error) g_error_free(error);
        return FALSE;
    }
    if (worker_id >= 0 && worker_id < 8) v->pids[worker_id] = pid;
    /* merge both streams through one watch each */
    for (int s = 0; s < 2; ++s) {
        int fd = s == 0 ? stdout_fd : stderr_fd;
        if (fd < 0) continue;
        GIOChannel *ch = g_io_channel_unix_new(fd);
        g_io_channel_set_encoding(ch, NULL, NULL);
        g_io_channel_set_buffered(ch, FALSE);
        LogPipe *pipe = g_new0(LogPipe, 1);
        pipe->fd = fd;
        pipe->worker_id = worker_id;
        /* colour errors red-ish via a per-worker tag */
        char tagname[32];
        snprintf(tagname, sizeof tagname, "w%d",
                 worker_id < 0 ? 8 : worker_id);
        GtkTextTagTable *tags = gtk_text_buffer_get_tag_table(v->log_buffer);
        pipe->tag = gtk_text_tag_table_lookup(tags, tagname);
        if (!pipe->tag) {
            pipe->tag = gtk_text_tag_new(tagname);
            gtk_text_tag_table_add(tags, pipe->tag);
        }
        v->pipes = g_list_prepend(v->pipes, pipe);
        g_io_add_watch(ch, G_IO_IN | G_IO_HUP,
                       (GIOFunc)on_log_pipe, pipe);
    }
    char msg[160];
    if (record_name)
        snprintf(msg, sizeof msg, "recording state '%s'…", record_name);
    else
        snprintf(msg, sizeof msg, "worker %d launched (%d iters)",
                 worker_id, iterations);
    gtk_label_set_text(GTK_LABEL(v->worker_label), msg);
    return TRUE;
}


/* ---- refresh -------------------------------------------------------- */

static void refresh_trials(Viewer *v) {
    if (!v->trials_store || !G_IS_LIST_STORE(v->trials_store)) return;
    int count = 0;
    AosTrialRow *rows = aos_labdb_recent_trials(v->db, MAX_ROWS, &count);
    g_list_store_remove_all(v->trials_store);
    /* live "green scenarios" counter: successful trials in view */
    int pass = 0;
    for (int i = 0; i < count; ++i)
        if (rows[i].outcome && !strcmp(rows[i].outcome, "success")) ++pass;
    char score[160];
    char *prog = aos_labdb_meta_get(v->db, "lab.progress");
    snprintf(score, sizeof score, "Scenarios: %d pass / %d trials%s%s",
             pass, count, prog ? "  •  " : "", prog ? prog : "");
    free(prog);
    gtk_label_set_text(GTK_LABEL(v->score_label), score);
    gtk_widget_set_css_classes(v->score_label,
        (const char *[]){pass > 0 ? "score-pass" : "dim-label", NULL});
    for (int i = 0; i < count; ++i) {
        AosTrialRow *r = &rows[i];
        GString *line = g_string_new(NULL);
        char num[64];
        snprintf(num, sizeof num, "%lld", (long long)r->id);
        row_printf(line, "%s", num);
        snprintf(num, sizeof num, "%lld", (long long)r->run_id);
        row_printf(line, "%s", num);
        snprintf(num, sizeof num, "%lld", (long long)r->state_id);
        row_printf(line, "%s", num);
        snprintf(num, sizeof num, "%lld", (long long)r->action_id);
        row_printf(line, "%s", num);
        row_printf(line, "%s", r->outcome ? r->outcome : "?");
        row_printf(line, "%s", r->params ? r->params : "");
        snprintf(num, sizeof num, "%d", r->frames);
        row_printf(line, "%s", num);
        snprintf(num, sizeof num, "%.2f", r->reward);
        row_printf(line, "%s", num);
        snprintf(num, sizeof num, "%d", r->duration_ms);
        row_printf(line, "%s", num);
        row_printf(line, "%s", r->started_at ? r->started_at : "");
        store_row(v->trials_store, line->str);
        g_string_free(line, TRUE);
    }
    aos_labdb_free_trial_rows(rows, count);
}

static void refresh_weights(Viewer *v) {
    if (!v->weights_store || !G_IS_LIST_STORE(v->weights_store)) return;
    if (!v->weights_store || !G_IS_LIST_STORE(v->weights_store)) return;
    int count = 0;
    AosWeightRow *rows = aos_labdb_weights(v->db, MAX_ROWS, &count);
    g_list_store_remove_all(v->weights_store);
    for (int i = 0; i < count; ++i) {
        AosWeightRow *r = &rows[i];
        GString *line = g_string_new(NULL);
        char num[64];
        snprintf(num, sizeof num, "%lld", (long long)r->action_id);
        row_printf(line, "%s", num);
        row_printf(line, "%s", r->params ? r->params : "");
        snprintf(num, sizeof num, "%d", r->attempts);
        row_printf(line, "%s", num);
        snprintf(num, sizeof num, "%d", r->successes);
        row_printf(line, "%s", num);
        snprintf(num, sizeof num, "%.1f%%", r->success_rate * 100.0);
        row_printf(line, "%s", num);
        snprintf(num, sizeof num, "%.2f", r->reward_sum);
        row_printf(line, "%s", num);
        store_row(v->weights_store, line->str);
        g_string_free(line, TRUE);
    }
    aos_labdb_free_weight_rows(rows, count);
}

static void refresh_states(Viewer *v) {
    if (!v->states_store || !G_IS_LIST_STORE(v->states_store)) return;
    if (!v->states_store || !G_IS_LIST_STORE(v->states_store)) return;
    int count = 0;
    AosStateRow *rows = aos_labdb_states(v->db, MAX_ROWS, &count);
    g_list_store_remove_all(v->states_store);
    /* rebuild the launch picker only when the list changes */
    if (count != v->last_state_count) {
        v->last_state_count = count;
        GtkStringList *model = gtk_string_list_new(NULL);
        for (int i = 0; i < count; ++i) {
            char label[256];
            snprintf(label, sizeof label, "#%lld  %s",
                     (long long)rows[i].id,
                     rows[i].name ? rows[i].name : "(unnamed)");
            gtk_string_list_append(model, label);
        }
        gtk_drop_down_set_model(GTK_DROP_DOWN(v->state_dropdown),
                                G_LIST_MODEL(model));
        g_object_unref(model);
        gtk_widget_set_sensitive(v->launch_button, count > 0);
        gtk_widget_set_sensitive(v->hint, count == 0);
    }
    for (int i = 0; i < count; ++i) {
        AosStateRow *r = &rows[i];
        GString *line = g_string_new(NULL);
        char num[64];
        snprintf(num, sizeof num, "%lld", (long long)r->id);
        row_printf(line, "%s", num);
        row_printf(line, "%s", r->name ? r->name : "");
        row_printf(line, "%s", r->note ? r->note : "");
        snprintf(num, sizeof num, "%lld", (long long)r->parent_id);
        row_printf(line, "%s", num);
        snprintf(num, sizeof num, "%d", r->blob_size);
        row_printf(line, "%s", num);
        row_printf(line, "%s", r->created_at ? r->created_at : "");
        store_row(v->states_store, line->str);
        g_string_free(line, TRUE);
    }
    aos_labdb_free_state_rows(rows, count);
}

static void refresh_runs(Viewer *v) {
    if (!v->runs_store || !G_IS_LIST_STORE(v->runs_store)) return;
    if (!v->runs_store || !G_IS_LIST_STORE(v->runs_store)) return;
    int count = 0;
    AosRunRow *rows = aos_labdb_runs(v->db, MAX_ROWS, &count);
    g_list_store_remove_all(v->runs_store);
    for (int i = 0; i < count; ++i) {
        AosRunRow *r = &rows[i];
        GString *line = g_string_new(NULL);
        char num[64];
        snprintf(num, sizeof num, "%lld", (long long)r->id);
        row_printf(line, "%s", num);
        row_printf(line, "%s", r->started_at ? r->started_at : "");
        row_printf(line, "%s", r->rom ? r->rom : "");
        row_printf(line, "%s", r->note ? r->note : "");
        snprintf(num, sizeof num, "%d", r->frames);
        row_printf(line, "%s", num);
        snprintf(num, sizeof num, "%d", r->trials);
        row_printf(line, "%s", num);
        store_row(v->runs_store, line->str);
        g_string_free(line, TRUE);
    }
    aos_labdb_free_run_rows(rows, count);
}

/* Attach to a worker's shared framebuffer if it exists; retried by the
 * fast timer until the worker creates it. */
static void fb_try_attach(WorkerCard *wc, int id) {
    if (wc->fb_fd >= 0 || wc->fb_seq) return;
    char name[64];
    snprintf(name, sizeof name, "/aoslab_fb_%d", id);
    int fd = shm_open(name, O_RDONLY, 0600);
    if (fd < 0) return;
    /* header is 32 bytes then 240*160 u32 pixels */
    size_t sz = 32 + 240 * 160 * 4;
    void *map = mmap(NULL, sz, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) { close(fd); return; }
    uint32_t *hdr = map;
    if (hdr[0] != 0x414F5346u) { munmap(map, sz); close(fd); return; }
    wc->fb_fd = fd;
    wc->fb_seq = (volatile uint64_t *)((char *)map + 8);
    wc->fb_pix = (const uint32_t *)((char *)map + 32);
    wc->fb_rgba = malloc(240 * 160 * 4);
    wc->last_seq = 0;
}

/* Push a new frame to the widget when the worker produced one. */
static void fb_refresh_frame(WorkerCard *wc, GtkWidget *picture) {
    if (!wc->fb_seq || !wc->fb_rgba || !picture ||
        !GTK_IS_PICTURE(picture)) return;
    uint64_t seq = *wc->fb_seq;
    if (seq == wc->last_seq) return;
    wc->last_seq = seq;
    const uint32_t *px = wc->fb_pix;
    uint8_t *out = wc->fb_rgba;
    for (int i = 0; i < 240 * 160; ++i) {
        uint32_t c = px[i];
        *out++ = c & 0xFF;          /* R */
        *out++ = (c >> 8) & 0xFF;   /* G */
        *out++ = (c >> 16) & 0xFF;  /* B (mGBA 0xAABBGGRR) */
        *out++ = 0xFF;
    }
    GBytes *bytes = g_bytes_new(wc->fb_rgba, 240 * 160 * 4);
    wc->fb_tex = gdk_memory_texture_new(240, 160,
                                        GDK_MEMORY_R8G8B8A8, bytes,
                                        240 * 4);
    g_bytes_unref(bytes);
    if (wc->fb_tex)
        gtk_picture_set_paintable(GTK_PICTURE(picture),
                                  GDK_PAINTABLE(wc->fb_tex));
    g_object_unref(wc->fb_tex);
}

static void on_purge_clicked(GtkButton *button, gpointer data) {
    (void)button;
    Viewer *v = data;
    if (!v->worker_path[0] || !v->db_path[0]) {
        log_append(v, -1, "purge: worker path or db missing", 32);
        return;
    }
    char *wq = g_shell_quote(v->worker_path);
    char *dq = g_shell_quote(v->db_path);
    char *cmd = g_strdup_printf("%s --db %s --purge", wq, dq);
    gchar *out = NULL, *err = NULL;
    gint rc = 1;
    gboolean ok = g_spawn_command_line_sync(cmd, &out, &err, &rc, NULL);
    if (ok && out) {
        char *save = NULL;
        for (char *line = strtok_r(out, "\n", &save); line;
             line = strtok_r(NULL, "\n", &save)) {
            log_append(v, -1, line, (int)strlen(line));
            log_append(v, -1, "\n", 1);
        }
    } else if (err && *err) {
        log_append(v, -1, err, (int)strlen(err));
        log_append(v, -1, "\n", 1);
    } else {
        log_append(v, -1, "purge failed to launch\n", 22);
    }
    g_free(wq); g_free(dq); g_free(cmd);
    g_free(out); g_free(err);
}

static void refresh_workers(Viewer *v) {
    static const char *phase_css[] = {"", "w-running", "w-paused",
                                      "w-stuck"};
    /* best worker = highest mean reward (rsum/rn metas written by
     * each trial); highlighted with a thick green border */
    double best_score = -1.0;
    int best_i = -1;
    for (int i = 0; i < 8; ++i) {
        char rk[64], nk[64];
        snprintf(rk, sizeof rk, "worker.%d.rsum", i);
        snprintf(nk, sizeof nk, "worker.%d.rn", i);
        char *rs = aos_labdb_meta_get(v->db, rk);
        char *rn = aos_labdb_meta_get(v->db, nk);
        if (rs && rn && atol(rn) > 0) {
            double sc = atof(rs) / (double)atol(rn);
            if (sc > best_score) { best_score = sc; best_i = i; }
        }
        free(rs); free(rn);
    }
    for (int i = 0; i < 8; ++i) {
        WorkerCard *wc = &v->workers[i];
        char pk[64], sk[64], ck[64], vk[64];
        snprintf(pk, sizeof pk, "worker.%d.phase", i);
        snprintf(sk, sizeof sk, "worker.%d.status", i);
        snprintf(ck, sizeof ck, "lab.control.%d", i);
        snprintf(vk, sizeof vk, "worker.%d.speed", i);
        char hk[64];
        snprintf(hk, sizeof hk, "worker.%d.hb", i);
        char *phase = aos_labdb_meta_get(v->db, pk);
        char *wstatus = aos_labdb_meta_get(v->db, sk);
        char *control = aos_labdb_meta_get(v->db, ck);
        char *speed = aos_labdb_meta_get(v->db, vk);
        const char *phase_str = phase ? phase : "idle";
        int phase_id = !strcmp(phase_str, "running") ? 1 :
                       !strcmp(phase_str, "paused") ? 2 :
                       !strcmp(phase_str, "scouting") ? 1 :
                       !strcmp(phase_str, "stuck") ? 3 : 0;
        /* stale worker: claims to run but heartbeat is old — its
         * process died or wedged without updating phase */
        int stale = 0;
        if (phase_id == 1) {
            char *hb = aos_labdb_meta_get(v->db, hk);
            if (hb) {
                long long now_ms = (long long)time(NULL) * 1000;
                if (now_ms - atoll(hb) > 15000) stale = 1;
                free(hb);
            } else stale = 1; /* never beat: old worker binary */
        }
        if (stale) phase_id = 3;
        if (i == best_i) {
            const char *cls = phase_css[phase_id] ? phase_css[phase_id]
                                                  : "";
            gtk_widget_set_css_classes(
                wc->card, (const char *[]){"w-best", cls, NULL});
        } else {
            gtk_widget_set_css_classes(wc->card,
                                      phase_css[phase_id] ?
                                      (const char *[]){phase_css[phase_id],
                                                       NULL}
                                      : NULL);
        }
        char head[64];
        snprintf(head, sizeof head, "Worker %d", i);
        gtk_label_set_text(GTK_LABEL(wc->name), head);
        char phase_disp[80];
        snprintf(phase_disp, sizeof phase_disp, "%s%s",
                 phase_str, stale ? " STALE" : "");
        gtk_label_set_text(GTK_LABEL(wc->phase), phase_disp);
        gtk_label_set_text(GTK_LABEL(wc->detail),
                           wstatus ? wstatus : "");
        gtk_button_set_label(GTK_BUTTON(wc->pause_button),
                             control && !strcmp(control, "pause")
                                 ? "Resume" : "Pause");
        gtk_widget_set_sensitive(wc->pause_button, phase_id != 0);
        /* speed slider: meta wins unless the user is mid-drag */
        int speed_val = speed ? atoi(speed) : 0;
        if (gtk_spin_button_get_value_as_int(
                GTK_SPIN_BUTTON(wc->speed_scale)) != speed_val)
            gtk_spin_button_set_value(
                GTK_SPIN_BUTTON(wc->speed_scale), speed_val);
        char speed_txt[16];
        snprintf(speed_txt, sizeof speed_txt, "%s",
                 speed_val > 0 ? "" : "Auto");
        if (speed_val > 0) {
            char tmp[16];
            snprintf(tmp, sizeof tmp, "x%d", speed_val);
            snprintf(speed_txt, sizeof speed_txt, "%s", tmp);
        }
        gtk_label_set_text(GTK_LABEL(wc->speed_label), speed_txt);
        free(phase);
        free(wstatus);
        free(control);
        free(speed);
    }
}

static gboolean poll_fb(gpointer data) {
    Viewer *v = data;
    for (int i = 0; i < 8; ++i) {
        if (!v->workers[i].picture) continue;
        fb_try_attach(&v->workers[i], i);
        fb_refresh_frame(&v->workers[i], v->workers[i].picture);
    }
    return G_SOURCE_CONTINUE;
}

static gboolean poll_refresh(gpointer data) {
    Viewer *v = data;
    refresh_trials(v);
    refresh_weights(v);
    refresh_states(v);
    refresh_runs(v);
    refresh_workers(v);
    gtk_label_set_text(GTK_LABEL(v->status), v->db_path);
    return G_SOURCE_CONTINUE;
}

/* ---- actions --------------------------------------------------------- */

static void worker_control(Viewer *v, int worker_id, const char *control) {
    char key[64];
    snprintf(key, sizeof key, "lab.control.%d", worker_id);
    aos_labdb_meta_set(v->db, key, control);
}

static void on_pause_clicked(GtkButton *button, gpointer user_data) {
    Viewer *v = user_data;
    int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "wid"));
    char key[64];
    snprintf(key, sizeof key, "lab.control.%d", id);
    char *control = aos_labdb_meta_get(v->db, key);
    worker_control(v, id,
                   control && !strcmp(control, "pause") ? "resume"
                                                         : "pause");
    free(control);
}

static void on_stop_clicked(GtkButton *button, gpointer user_data) {
    Viewer *v = user_data;
    int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "wid"));
    worker_control(v, id, "stop");
}

/* Play: resume a paused worker, or respawn a dead one from its stored
 * launch config. Never double-spawns a live worker. */
static void play_worker(Viewer *v, int id) {
    char pk[64], ck[64];
    snprintf(pk, sizeof pk, "worker.%d.phase", id);
    snprintf(ck, sizeof ck, "lab.control.%d", id);
    char *ph = aos_labdb_meta_get(v->db, pk);
    char *ctl = aos_labdb_meta_get(v->db, ck);
    int alive = ph && (!strcmp(ph, "running") ||
                       !strcmp(ph, "scouting") ||
                       !strcmp(ph, "paused"));
    if (alive) {
        worker_control(v, id,
                       ctl && !strcmp(ctl, "pause") ? "resume" : "run");
    } else {
        worker_control(v, id, "run");
        if (!spawn_from_launch(v, id))
            gtk_label_set_text(GTK_LABEL(v->worker_label),
                               "Worker has no stored launch config — "
                               "start a batch or scout first");
    }
    free(ph);
    free(ctl);
}

static void on_play_clicked(GtkButton *button, gpointer user_data) {
    Viewer *v = user_data;
    int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "wid"));
    play_worker(v, id);
}

/* Best: ask the worker to hop to the fleet's highest-value checkpoint
 * (smart restart, implemented worker-side). */
static void on_best_clicked(GtkButton *button, gpointer user_data) {
    Viewer *v = user_data;
    int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "wid"));
    worker_control(v, id, "best");
    char msg[80];
    snprintf(msg, sizeof msg, "worker %d: switch to best branch", id);
    gtk_label_set_text(GTK_LABEL(v->worker_label), msg);
}

static void on_play_all_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    Viewer *v = user_data;
    for (int i = 0; i < 8; ++i) play_worker(v, i);
}

static void on_speed_changed(GtkSpinButton *spin, gpointer user_data) {
    Viewer *v = user_data;
    int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(spin), "wid"));
    int speed = gtk_spin_button_get_value_as_int(spin);
    char key[64], val[8];
    snprintf(key, sizeof key, "worker.%d.speed", id);
    snprintf(val, sizeof val, "%d", speed);
    aos_labdb_meta_set(v->db, key, val);
}

static void on_all_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    Viewer *v = user_data;
    v->all_paused = !v->all_paused;
    const char *ctl = v->all_paused ? "pause" : "resume";
    for (int i = 0; i < 8; ++i)
        worker_control(v, i, ctl);
    gtk_button_set_label(GTK_BUTTON(v->all_button),
                         v->all_paused ? "Resume all" : "Pause all");
    char msg[96];
    snprintf(msg, sizeof msg, "Sent %s to every worker slot",
             ctl);
    gtk_label_set_text(GTK_LABEL(v->worker_label), msg);
}

static void on_all_stop_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    Viewer *v = user_data;
    for (int i = 0; i < 8; ++i)
        worker_control(v, i, "stop");
    gtk_label_set_text(GTK_LABEL(v->worker_label),
                       "Sent stop to every worker slot");
}

static void on_all_speed_changed(GtkSpinButton *spin, gpointer user_data) {
    Viewer *v = user_data;
    int speed = gtk_spin_button_get_value_as_int(spin);
    char val[8];
    snprintf(val, sizeof val, "%d", speed);
    for (int i = 0; i < 8; ++i) {
        char key[64];
        snprintf(key, sizeof key, "worker.%d.speed", i);
        aos_labdb_meta_set(v->db, key, val);
        gtk_spin_button_set_value(
            GTK_SPIN_BUTTON(v->workers[i].speed_scale), speed);
    }
}

static void on_launch_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    Viewer *v = user_data;
    int count = 0;
    AosStateRow *states = aos_labdb_valuable_states(v->db, 200, &count);
    if (count == 0) {
        gtk_label_set_text(GTK_LABEL(v->worker_label),
                           "No save state yet — press \"Record state\" "
                           "first");
        aos_labdb_free_state_rows(states, count);
        return;
    }
    int sel = gtk_drop_down_get_selected(GTK_DROP_DOWN(v->state_dropdown));
    if (sel < 0 || sel >= count) sel = 0;
    int iterations = gtk_spin_button_get_value_as_int(
        GTK_SPIN_BUTTON(v->iterations_spin));
    int worker_id = gtk_spin_button_get_value_as_int(
        GTK_SPIN_BUTTON(v->worker_id_spin));
    char idbuf[32];
    snprintf(idbuf, sizeof idbuf, "%lld", (long long)states[sel].id);
    aos_labdb_free_state_rows(states, count);
    spawn_worker(v, idbuf, iterations, worker_id, NULL, 0);
}

typedef struct {
    Viewer *viewer;
    GtkWidget *name_entry;
    GtkWidget *frames_spin;
    GtkWidget *keys_entry;
} RecordDialog;

static void on_record_window_destroy(GtkWidget *widget,
                                     gpointer user_data) {
    (void)widget;
    g_free(user_data);
}

static void on_record_confirm(GtkButton *button, gpointer user_data) {
    (void)button;
    RecordDialog *d = user_data;
    const char *name = gtk_editable_get_text(GTK_EDITABLE(d->name_entry));
    int frames_n = gtk_spin_button_get_value_as_int(
        GTK_SPIN_BUTTON(d->frames_spin));
    const char *keys = gtk_editable_get_text(GTK_EDITABLE(d->keys_entry));
    char clean[128];
    snprintf(clean, sizeof clean, "%s",
             (name && *name) ? name : "gui-state");
    spawn_worker_keys(d->viewer, NULL, 0, -1, clean, frames_n,
                      (keys && *keys) ? keys : NULL);
    gtk_window_destroy(GTK_WINDOW(gtk_widget_get_ancestor(
        GTK_WIDGET(button), GTK_TYPE_WINDOW)));
}

static void on_record_cancel(GtkButton *button, gpointer user_data) {
    (void)button; (void)user_data;
    gtk_window_destroy(GTK_WINDOW(gtk_widget_get_ancestor(
        GTK_WIDGET(button), GTK_TYPE_WINDOW)));
}

static void on_record_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    Viewer *v = user_data;
    if (!v->rom_path[0]) {
        gtk_label_set_text(GTK_LABEL(v->worker_label),
                           "Set --rom to record a state");
        return;
    }
    RecordDialog *d = g_new0(RecordDialog, 1);
    d->viewer = v;
    GtkWidget *win = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(win), "Record save state");
    gtk_window_set_transient_for(GTK_WINDOW(win), GTK_WINDOW(v->window));
    gtk_window_set_modal(GTK_WINDOW(win), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(win), 460, -1);
    g_signal_connect(win, "destroy",
                     G_CALLBACK(on_record_window_destroy), d);
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_widget_set_margin_start(grid, 14);
    gtk_widget_set_margin_end(grid, 14);
    gtk_widget_set_margin_top(grid, 14);
    gtk_widget_set_margin_bottom(grid, 14);
    gtk_window_set_child(GTK_WINDOW(win), grid);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Name:"), 0, 0, 1, 1);
    d->name_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(d->name_entry), "gui-state");
    gtk_widget_set_hexpand(d->name_entry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), d->name_entry, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Frames:"), 0, 1, 1, 1);
    d->frames_spin = gtk_spin_button_new_with_range(60, 100000, 60);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(d->frames_spin), 6000);
    gtk_widget_set_tooltip_text(
        d->frames_spin,
        "How many emulated frames to run before the checkpoint "
        "(60 fps: 6000 = 100 s)");
    gtk_grid_attach(GTK_GRID(grid), d->frames_spin, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Keys:"), 0, 2, 1, 1);
    d->keys_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(d->keys_entry),
        "frame:mask,…  e.g. 900:8,950:0 (8=START,1=A)");
    gtk_widget_set_hexpand(d->keys_entry, TRUE);
    gtk_widget_set_tooltip_text(
        d->keys_entry,
        "Optional input script: frame:mask pairs, mask held until the "
        "next event. A=1 B=2 SELECT=4 START=8 arrows=16/32/64/128 "
        "R=256 L=512");
    gtk_grid_attach(GTK_GRID(grid), d->keys_entry, 1, 2, 1, 1);
    GtkWidget *btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(btns, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), btns, 0, 3, 2, 1);
    GtkWidget *cancel = gtk_button_new_with_label("Cancel");
    g_signal_connect(cancel, "clicked", G_CALLBACK(on_record_cancel), d);
    gtk_box_append(GTK_BOX(btns), cancel);
    GtkWidget *ok = gtk_button_new_with_label("Record");
    gtk_widget_add_css_class(ok, "suggested-action");
    g_signal_connect(ok, "clicked", G_CALLBACK(on_record_confirm), d);
    gtk_box_append(GTK_BOX(btns), ok);
    gtk_window_present(GTK_WINDOW(win));
}

/* ---- UI --------------------------------------------------------------- */

static GtkWidget *make_tab_page(GListStore **store, const char **titles,
                                const char **tips, const char *tip) {
    GtkWidget *page = scrolled_page(store, titles, tips);
    (void)tip;
    return page;
}

/* ---- background policy ----------------------------------------------
 * The viewer owns the worker processes it spawns. Unless the user
 * opted in via Settings ("allow background"), closing the window
 * stops every worker; with the option on, workers keep running
 * unsupervised and a later viewer session reattaches to them through
 * the shared database + framebuffer segments. */

static int bg_allowed(Viewer *v) {
    char *val = aos_labdb_meta_get(v->db, "lab.bg");
    int on = val && !strcmp(val, "1");
    free(val);
    return on;
}

typedef struct {
    Viewer *viewer;
    GtkWidget *switch_;
} SettingsDialog;

static void on_settings_destroy(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    g_free(user_data);
}

static void on_settings_confirm(GtkButton *button, gpointer user_data) {
    (void)button;
    SettingsDialog *d = user_data;
    int on = gtk_switch_get_active(GTK_SWITCH(d->switch_));
    aos_labdb_meta_set(d->viewer->db, "lab.bg", on ? "1" : "0");
    gtk_window_destroy(GTK_WINDOW(gtk_widget_get_ancestor(
        GTK_WIDGET(button), GTK_TYPE_WINDOW)));
}

static void on_settings_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    Viewer *v = user_data;
    SettingsDialog *d = g_new0(SettingsDialog, 1);
    d->viewer = v;
    GtkWidget *win = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(win), "Lab settings");
    gtk_window_set_transient_for(GTK_WINDOW(win), GTK_WINDOW(v->window));
    gtk_window_set_modal(GTK_WINDOW(win), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(win), 420, -1);
    g_signal_connect(win, "destroy",
                     G_CALLBACK(on_settings_destroy), d);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(box, 16);
    gtk_widget_set_margin_end(box, 16);
    gtk_widget_set_margin_top(box, 16);
    gtk_widget_set_margin_bottom(box, 16);
    gtk_window_set_child(GTK_WINDOW(win), box);
    GtkWidget *srow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(box), srow);
    gtk_box_append(GTK_BOX(srow), gtk_label_new("Background workers:"));
    d->switch_ = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(d->switch_), bg_allowed(v));
    gtk_widget_set_tooltip_text(
        d->switch_,
        "OFF (default): closing this window stops every worker. "
        "ON: workers survive the viewer and keep learning; reopen "
        "the viewer later to reattach.");
    gtk_box_append(GTK_BOX(srow), d->switch_);
    GtkWidget *note = gtk_label_new(
        "Workers coordinate through the SQLite database, so they do "
        "not need this window to keep working.");
    gtk_label_set_wrap(GTK_LABEL(note), TRUE);
    gtk_label_set_xalign(GTK_LABEL(note), 0.0);
    gtk_widget_add_css_class(note, "dim-label");
    gtk_box_append(GTK_BOX(box), note);
    GtkWidget *btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(btns, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(box), btns);
    GtkWidget *ok = gtk_button_new_with_label("OK");
    gtk_widget_add_css_class(ok, "suggested-action");
    g_signal_connect(ok, "clicked", G_CALLBACK(on_settings_confirm), d);
    gtk_box_append(GTK_BOX(btns), ok);
    gtk_window_present(GTK_WINDOW(win));
}

/* On close: cooperative stop to every slot, then SIGTERM the children
 * we spawned — unless background mode is allowed, in which case they
 * are simply detached and keep running. */
static gboolean on_close_request(GtkWindow *window, gpointer user_data) {
    (void)window;
    Viewer *v = user_data;
    if (bg_allowed(v)) return FALSE; /* let it close, workers survive */
    for (int i = 0; i < 8; ++i) worker_control(v, i, "stop");
    for (int i = 0; i < 8; ++i) {
        if (v->pids[i] > 0) {
            kill(v->pids[i], SIGTERM);
            v->pids[i] = -1;
        }
    }
    return FALSE;
}

static void on_activate(GtkApplication *app, gpointer user_data) {
    Viewer *v = user_data;
    g_viewer = v;
    for (int i = 0; i < 8; ++i) v->pids[i] = -1;
    v->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(v->window), "Aria Lab — learning registry");
    gtk_window_set_default_size(GTK_WINDOW(v->window), 1280, 860);
    g_signal_connect(v->window, "close-request",
                     G_CALLBACK(on_close_request), v);

    /* status colours for worker cards */
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_string(
        css,
        "frame.w-running { border: 2px solid #43a047; border-radius: 6px; }"
        "frame.w-paused  { border: 2px solid #fb8c00; border-radius: 6px; }"
        "frame.w-stuck   { border: 2px solid #e53935; border-radius: 6px; }"
        "frame.w-best    { border: 3px solid #00c853; border-radius: 6px; }"
        "frame           { border: 1px solid #546e7a; border-radius: 6px; }"
        "label.w-phase   { font-weight: bold; }"
        "label.score-pass{ color: #66cc66; font-weight: bold; }"
        "textview        { font-family: monospace; font-size: 11px; }");
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(), GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(root, 10);
    gtk_widget_set_margin_end(root, 10);
    gtk_widget_set_margin_top(root, 10);
    gtk_widget_set_margin_bottom(root, 10);
    gtk_window_set_child(GTK_WINDOW(v->window), root);

    /* ---- header: what this is ------------------------------------- */
    GtkWidget *header = gtk_label_new(
        "Aria emulator lab — workers play the real ROM from save states, "
        "trials are scored by memory probes, and the best parameter sets "
        "accumulate weight. Everything is local.");
    gtk_label_set_wrap(GTK_LABEL(header), TRUE);
    gtk_label_set_xalign(GTK_LABEL(header), 0.0);
    gtk_widget_add_css_class(header, "dim-label");
    gtk_box_append(GTK_BOX(root), header);

    v->score_label = gtk_label_new("Scenarios: 0 pass / 0 trials");
    gtk_label_set_xalign(GTK_LABEL(v->score_label), 0.0);
    gtk_widget_add_css_class(v->score_label, "score-pass");
    gtk_box_append(GTK_BOX(root), v->score_label);

    /* ---- global fleet controls -------------------------------------- */
    GtkWidget *fleet = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_top(fleet, 4);
    gtk_box_append(GTK_BOX(root), fleet);

    v->all_button = gtk_button_new_with_label("Pause all");
    gtk_widget_set_tooltip_text(
        v->all_button,
        "Pause or resume every worker slot at once (cooperative: "
        "each worker checks between trials)");
    g_signal_connect(v->all_button, "clicked",
                     G_CALLBACK(on_all_clicked), v);
    gtk_box_append(GTK_BOX(fleet), v->all_button);

    v->all_stop_button = gtk_button_new_with_label("Stop all");
    gtk_widget_add_css_class(v->all_stop_button, "destructive-action");
    gtk_widget_set_tooltip_text(
        v->all_stop_button,
        "Stop every worker slot at once (cooperative: each worker "
        "checks between trials)");
    g_signal_connect(v->all_stop_button, "clicked",
                     G_CALLBACK(on_all_stop_clicked), v);
    gtk_box_append(GTK_BOX(fleet), v->all_stop_button);

    GtkWidget *all_play_button = gtk_button_new_with_label("Play all");
    gtk_widget_add_css_class(all_play_button, "suggested-action");
    gtk_widget_set_tooltip_text(
        all_play_button,
        "Resume every paused worker and respawn every stopped one "
        "from its stored launch config");
    g_signal_connect(all_play_button, "clicked",
                     G_CALLBACK(on_play_all_clicked), v);
    gtk_box_append(GTK_BOX(fleet), all_play_button);

    GtkWidget *settings_button = gtk_button_new_with_label("Settings…");
    gtk_widget_set_tooltip_text(
        settings_button,
        "Lab settings: background mode, purge policy");
    g_signal_connect(settings_button, "clicked",
                     G_CALLBACK(on_settings_clicked), v);
    gtk_box_append(GTK_BOX(fleet), settings_button);

    GtkWidget *purge_button = gtk_button_new_with_label("Purge DB");
    gtk_widget_add_css_class(purge_button, "destructive-action");
    gtk_widget_set_tooltip_text(
        purge_button,
        "Delete dead weight from the database: zero-reward fail "
        "trials, hopeless weight combos and auto checkpoints no "
        "trial ever used. Validated states and successful trials "
        "are kept.");
    g_signal_connect(purge_button, "clicked",
                     G_CALLBACK(on_purge_clicked), v);
    gtk_box_append(GTK_BOX(fleet), purge_button);

    GtkWidget *gsp = gtk_label_new("Global speed:");
    gtk_box_append(GTK_BOX(fleet), gsp);
    v->all_speed_scale = gtk_spin_button_new_with_range(0, 16, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(v->all_speed_scale), 0);
    gtk_widget_set_size_request(v->all_speed_scale, 64, -1);
    gtk_widget_set_valign(v->all_speed_scale, GTK_ALIGN_CENTER);
    gtk_widget_set_tooltip_text(
        v->all_speed_scale,
        "Speed cap applied to ALL workers: Auto = unthrottled, "
        "1-16 = N*60 fps. Overwrites each card's speed.");
    g_signal_connect(v->all_speed_scale, "value-changed",
                     G_CALLBACK(on_all_speed_changed), v);
    gtk_box_append(GTK_BOX(fleet), v->all_speed_scale);
    GtkWidget *srange = gtk_label_new("Auto..x16");
    gtk_widget_add_css_class(srange, "dim-label");
    gtk_box_append(GTK_BOX(fleet), srange);

    /* Resizable sections with draggable splitters (FileZilla-style):
     * top pane = live emulator grid, bottom split = controls/log vs
     * registry tables. */
    GtkWidget *paned_main = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_paned_set_wide_handle(GTK_PANED(paned_main), TRUE);
    gtk_widget_set_hexpand(paned_main, TRUE);
    gtk_widget_set_vexpand(paned_main, TRUE);
    gtk_widget_set_margin_top(paned_main, 4);
    gtk_box_append(GTK_BOX(root), paned_main);

    GtkWidget *top_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_paned_set_start_child(GTK_PANED(paned_main), top_box);

    GtkWidget *paned_low = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_paned_set_wide_handle(GTK_PANED(paned_low), TRUE);
    gtk_paned_set_end_child(GTK_PANED(paned_main), paned_low);

    GtkWidget *mid_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_vexpand(mid_box, TRUE);
    gtk_paned_set_start_child(GTK_PANED(paned_low), mid_box);
    /* initial splitter positions: video grid on top, controls+log
     * middle, tables bottom */
    gtk_paned_set_position(GTK_PANED(paned_main), 470);
    gtk_paned_set_position(GTK_PANED(paned_low), 260);

    /* ---- workers section ------------------------------------------ */
    GtkWidget *workers_label = gtk_label_new("Workers — eight parallel "
                                             "learning slots");
    gtk_label_set_xalign(GTK_LABEL(workers_label), 0.0);
    gtk_widget_add_css_class(workers_label, "title-4");
    gtk_box_append(GTK_BOX(top_box), workers_label);
    GtkWidget *workers_tip = gtk_label_new(
        "Each card is one emulator process. Pause freezes it between "
        "trials, Stop ends the batch, Resume continues.");
    gtk_label_set_xalign(GTK_LABEL(workers_tip), 0.0);
    gtk_widget_add_css_class(workers_tip, "dim-label");
    gtk_box_append(GTK_BOX(top_box), workers_tip);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    for (int i = 0; i < 8; ++i) {
        GtkWidget *card = gtk_frame_new(NULL);
        WorkerCard *wc = &v->workers[i];
        wc->fb_fd = -1;
        wc->card = card;
        GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_frame_set_child(GTK_FRAME(card), vbox);
        gtk_widget_set_margin_start(vbox, 8);
        gtk_widget_set_margin_end(vbox, 8);
        gtk_widget_set_margin_top(vbox, 6);
        gtk_widget_set_margin_bottom(vbox, 6);

        wc->picture = gtk_picture_new();
        gtk_picture_set_content_fit(GTK_PICTURE(wc->picture),
                                    GTK_CONTENT_FIT_CONTAIN);
        gtk_widget_set_size_request(wc->picture, 300, 200);
        gtk_widget_set_tooltip_text(
            wc->picture,
            "Live emulated screen of this worker (240x160, scaled)");
        gtk_box_append(GTK_BOX(vbox), wc->picture);

        wc->name = gtk_label_new("Worker 0");
        gtk_label_set_xalign(GTK_LABEL(wc->name), 0.0);
        gtk_widget_add_css_class(wc->name, "title-5");
        gtk_box_append(GTK_BOX(vbox), wc->name);

        wc->phase = gtk_label_new("idle");
        gtk_label_set_xalign(GTK_LABEL(wc->phase), 0.0);
        gtk_widget_add_css_class(wc->phase, "w-phase");
        gtk_box_append(GTK_BOX(vbox), wc->phase);

        wc->detail = gtk_label_new("");
        gtk_label_set_xalign(GTK_LABEL(wc->detail), 0.0);
        gtk_label_set_wrap(GTK_LABEL(wc->detail), TRUE);
        gtk_widget_add_css_class(wc->detail, "dim-label");
        gtk_box_append(GTK_BOX(vbox), wc->detail);

        GtkWidget *btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        wc->pause_button = gtk_button_new_with_label("Pause");
        gtk_widget_set_tooltip_text(
            wc->pause_button,
            "Freeze this worker between trials (it keeps its state)");
        g_object_set_data(G_OBJECT(wc->pause_button), "wid",
                          GINT_TO_POINTER(i));
        g_signal_connect(wc->pause_button, "clicked",
                         G_CALLBACK(on_pause_clicked), v);
        gtk_box_append(GTK_BOX(btns), wc->pause_button);

        GtkWidget *stop = gtk_button_new_with_label("Stop");
        gtk_widget_set_tooltip_text(
            stop, "End this worker's batch after the current trial");
        g_object_set_data(G_OBJECT(stop), "wid", GINT_TO_POINTER(i));
        g_signal_connect(stop, "clicked", G_CALLBACK(on_stop_clicked), v);
        gtk_box_append(GTK_BOX(btns), stop);

        GtkWidget *play = gtk_button_new_with_label("Play");
        gtk_widget_set_tooltip_text(
            play,
            "Resume a paused worker, or respawn a stopped one from "
            "its last launch config");
        g_object_set_data(G_OBJECT(play), "wid", GINT_TO_POINTER(i));
        g_signal_connect(play, "clicked", G_CALLBACK(on_play_clicked), v);
        gtk_box_append(GTK_BOX(btns), play);

        GtkWidget *best = gtk_button_new_with_label("Best");
        gtk_widget_set_tooltip_text(
            best,
            "Smart restart: hop this worker to the fleet's "
            "highest-value checkpoint");
        g_object_set_data(G_OBJECT(best), "wid", GINT_TO_POINTER(i));
        g_signal_connect(best, "clicked", G_CALLBACK(on_best_clicked), v);
        gtk_box_append(GTK_BOX(btns), best);
        gtk_box_append(GTK_BOX(vbox), btns);

        GtkWidget *sbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_box_append(GTK_BOX(vbox), sbox);
        gtk_box_append(GTK_BOX(sbox), gtk_label_new("Speed:"));
        wc->speed_label = gtk_label_new("Auto");
        gtk_widget_set_size_request(wc->speed_label, 42, -1);
        gtk_label_set_xalign(GTK_LABEL(wc->speed_label), 0.0);
        gtk_box_append(GTK_BOX(sbox), wc->speed_label);
        wc->speed_scale = gtk_spin_button_new_with_range(0, 16, 1);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(wc->speed_scale), 0);
        gtk_widget_set_size_request(wc->speed_scale, 64, -1);
        gtk_widget_set_valign(wc->speed_scale, GTK_ALIGN_CENTER);
        gtk_widget_set_tooltip_text(
            wc->speed_scale,
            "Emulation speed cap: Auto = unthrottled (fastest), "
            "1-16 = capped at that multiple of 60 fps. Applies to the "
            "next trial of this worker.");
        g_object_set_data(G_OBJECT(wc->speed_scale), "wid",
                          GINT_TO_POINTER(i));
        g_signal_connect(wc->speed_scale, "value-changed",
                         G_CALLBACK(on_speed_changed), v);
        gtk_box_append(GTK_BOX(sbox), wc->speed_scale);

        gtk_grid_attach(GTK_GRID(grid), card, i % 4, i / 4, 1, 1);
    }
    GtkWidget *workers_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(workers_scroll),
                                   GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(workers_scroll), grid);
    gtk_scrolled_window_set_min_content_height(
        GTK_SCROLLED_WINDOW(workers_scroll), 170);
    gtk_widget_set_vexpand(workers_scroll, TRUE);
    gtk_box_append(GTK_BOX(top_box), workers_scroll);

    /* ---- launch controls ------------------------------------------- */
    GtkWidget *launch_frame = gtk_frame_new("Start a learning batch");
    GtkWidget *lbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_frame_set_child(GTK_FRAME(launch_frame), lbox);
    gtk_widget_set_margin_start(lbox, 10);
    gtk_widget_set_margin_end(lbox, 10);
    gtk_widget_set_margin_top(lbox, 8);
    gtk_widget_set_margin_bottom(lbox, 8);
    gtk_box_append(GTK_BOX(mid_box), launch_frame);

    gtk_box_append(GTK_BOX(lbox), gtk_label_new("Worker slot:"));
    v->worker_id_spin = gtk_spin_button_new_with_range(0, 7, 1);
    gtk_widget_set_tooltip_text(GTK_WIDGET(v->worker_id_spin),
                                "Which of the eight cards this batch "
                                "reports to");
    gtk_widget_set_size_request(v->worker_id_spin, 70, -1);
    gtk_box_append(GTK_BOX(lbox), v->worker_id_spin);

    gtk_box_append(GTK_BOX(lbox), gtk_label_new("Save state:"));
    v->state_dropdown = gtk_drop_down_new(NULL, NULL);
    gtk_widget_set_tooltip_text(
        GTK_WIDGET(v->state_dropdown),
        "Checkpoint the worker reloads before every trial");
    gtk_widget_set_hexpand(v->state_dropdown, TRUE);
    gtk_box_append(GTK_BOX(lbox), v->state_dropdown);

    gtk_box_append(GTK_BOX(lbox), gtk_label_new("Trials:"));
    v->iterations_spin = gtk_spin_button_new_with_range(1, 100000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(v->iterations_spin), 50);
    gtk_widget_set_tooltip_text(GTK_WIDGET(v->iterations_spin),
                                "Number of experiments in this batch");
    gtk_widget_set_size_request(v->iterations_spin, 80, -1);
    gtk_box_append(GTK_BOX(lbox), v->iterations_spin);

    v->launch_button = gtk_button_new_with_label("Start batch");
    gtk_widget_add_css_class(v->launch_button, "suggested-action");
    gtk_widget_set_tooltip_text(
        v->launch_button,
        "Spawn a worker that tries random and best-known parameter sets "
        "from the chosen save state");
    g_signal_connect(v->launch_button, "clicked",
                     G_CALLBACK(on_launch_clicked), v);
    gtk_box_append(GTK_BOX(lbox), v->launch_button);

    v->record_button = gtk_button_new_with_label("Record state…");
    gtk_widget_set_tooltip_text(
        v->record_button,
        "Boot the ROM, run some frames, and store the result as a new "
        "save-state checkpoint in the database");
    g_signal_connect(v->record_button, "clicked",
                     G_CALLBACK(on_record_clicked), v);
    gtk_box_append(GTK_BOX(lbox), v->record_button);

    v->hint = gtk_label_new(
        "No save state yet — press \"Record state…\" to capture one "
        "from the ROM, then start batches.");
    gtk_label_set_xalign(GTK_LABEL(v->hint), 0.0);
    gtk_widget_add_css_class(v->hint, "warning");
    gtk_box_append(GTK_BOX(mid_box), v->hint);

    v->worker_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(v->worker_label), 0.0);
    gtk_widget_add_css_class(v->worker_label, "dim-label");
    gtk_box_append(GTK_BOX(mid_box), v->worker_label);

    /* ---- live log ----------------------------------------------------- */
    GtkWidget *log_label = gtk_label_new("Live log — worker output "
                                         "(memory only, nothing written "
                                         "to disk)");
    gtk_label_set_xalign(GTK_LABEL(log_label), 0.0);
    gtk_widget_add_css_class(log_label, "title-5");
    gtk_box_append(GTK_BOX(mid_box), log_label);
    GtkWidget *log_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(log_scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_height(
        GTK_SCROLLED_WINDOW(log_scroll), 140);
    gtk_widget_set_size_request(log_scroll, 400, 140);
    v->log_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(v->log_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(v->log_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(v->log_view),
                                GTK_WRAP_NONE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(v->log_view), TRUE);
    v->log_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(v->log_view));
    GtkTextTagTable *tags = gtk_text_buffer_get_tag_table(v->log_buffer);
    GtkTextTag *tag = gtk_text_tag_new("worker_tag");
    GdkRGBA rgba = {0.39, 0.63, 1.0, 1.0};
    g_object_set(tag, "foreground-rgba", &rgba, NULL);
    gtk_text_tag_table_add(tags, tag);
    for (int i = 0; i < 8; ++i) {
        char name[8];
        snprintf(name, sizeof name, "w%d", i);
        GtkTextTag *wt = gtk_text_tag_new(name);
        static const GdkRGBA colors[8] = {
            {0.40, 0.80, 0.40, 1}, {0.30, 0.70, 1.00, 1},
            {1.00, 0.75, 0.30, 1}, {1.00, 0.45, 0.45, 1},
            {0.80, 0.50, 1.00, 1}, {0.40, 0.90, 0.90, 1},
            {1.00, 0.60, 0.80, 1}, {0.90, 0.90, 0.40, 1}};
        g_object_set(wt, "foreground-rgba", &colors[i], NULL);
        gtk_text_tag_table_add(tags, wt);
    }
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(log_scroll),
                                  v->log_view);
    gtk_widget_set_vexpand(log_scroll, TRUE);
    gtk_box_append(GTK_BOX(mid_box), log_scroll);

    /* ---- registry tables --------------------------------------------- */
    v->notebook = gtk_notebook_new();
    gtk_widget_set_vexpand(v->notebook, TRUE);
    gtk_paned_set_end_child(GTK_PANED(paned_low), v->notebook);

    const char *trial_titles[] = {"id", "run", "state", "action",
                                  "outcome", "params", "frames",
                                  "reward", "ms", "time", NULL};
    const char *trial_tips[] = {
        "Trial number", "Worker run id", "Save state used",
        "Action tested", "pass / fail / timeout", "Exact inputs tried",
        "Emulated frames", "Score 0..1 from the probe", "Wall time",
        "Start timestamp", NULL};
    GtkWidget *p1 = make_tab_page(&v->trials_store, trial_titles,
                                  trial_tips,
                                  "Every experiment the workers ran");
    GtkWidget *t1 = gtk_label_new("Trials");
    gtk_widget_set_tooltip_text(t1, "History of every experiment");
    gtk_notebook_append_page(GTK_NOTEBOOK(v->notebook), p1, t1);

    const char *weight_titles[] = {"action", "params", "attempts",
                                   "successes", "rate", "reward", NULL};
    const char *weight_tips[] = {
        "Action id", "Parameter set", "Times tried",
        "Times it scored", "Success percentage — higher is better",
        "Total reward accumulated", NULL};
    GtkWidget *p2 = make_tab_page(&v->weights_store, weight_titles,
                                  weight_tips,
                                  "Learned parameter rankings");
    GtkWidget *t2 = gtk_label_new("Weights");
    gtk_widget_set_tooltip_text(
        t2, "Which parameter sets work best — the 'brain' of the lab");
    gtk_notebook_append_page(GTK_NOTEBOOK(v->notebook), p2, t2);

    const char *state_titles[] = {"id", "name", "note", "parent",
                                  "bytes", "created", NULL};
    const char *state_tips[] = {
        "State id", "Checkpoint name", "Free-form note",
        "Parent state id", "Save-state size in bytes",
        "Creation timestamp", NULL};
    GtkWidget *p3 = make_tab_page(&v->states_store, state_titles,
                                  state_tips,
                                  "Stored emulator checkpoints");
    GtkWidget *t3 = gtk_label_new("States");
    gtk_widget_set_tooltip_text(t3, "Saved emulator checkpoints");
    gtk_notebook_append_page(GTK_NOTEBOOK(v->notebook), p3, t3);

    const char *run_titles[] = {"id", "started", "rom", "note",
                                "frames", "trials", NULL};
    const char *run_tips[] = {
        "Run id", "Start timestamp", "ROM path", "Run note",
        "Total emulated frames", "Trials completed", NULL};
    GtkWidget *p4 = make_tab_page(&v->runs_store, run_titles, run_tips,
                                  "Worker batch history");
    GtkWidget *t4 = gtk_label_new("Runs");
    gtk_widget_set_tooltip_text(t4, "History of worker batches");
    gtk_notebook_append_page(GTK_NOTEBOOK(v->notebook), p4, t4);

    v->status = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(v->status), 0.0);
    gtk_widget_add_css_class(v->status, "dim-label");
    gtk_box_append(GTK_BOX(root), v->status);

    gtk_window_present(GTK_WINDOW(v->window));
    g_timeout_add(POLL_MS, poll_refresh, v);
    g_timeout_add(100, poll_fb, v);
    poll_refresh(v);
}

int main(int argc, char **argv) {
    Viewer v;
    memset(&v, 0, sizeof v);
    /* database/ is a symlink to the bulk-storage lab directory
     * (gitignored); everything heavy stays off the system drive. */
    snprintf(v.db_path, sizeof v.db_path, "database/lab.db");

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--db") && i + 1 < argc) {
            snprintf(v.db_path, sizeof v.db_path, "%s", argv[++i]);
        } else if (!strcmp(argv[i], "--rom") && i + 1 < argc) {
            snprintf(v.rom_path, sizeof v.rom_path, "%s", argv[++i]);
        } else {
            fprintf(stderr,
                    "usage: aos_lab_viewer [--db PATH] [--rom PATH]\n");
            return 2;
        }
    }

    char err[512];
    v.db = aos_labdb_open(v.db_path, err, sizeof err);
    if (!v.db) {
        fprintf(stderr, "db open failed: %s\n", err);
        return 1;
    }

    /* locate aos_lab_worker next to this executable */
    {
        char exe[4096];
        ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
        if (n > 0) {
            exe[n] = 0;
            char *slash = strrchr(exe, '/');
            if (slash) {
                slash[1] = 0;
                snprintf(v.worker_path, sizeof v.worker_path,
                         "%saos_lab_worker", exe);
            }
        }
    }

    /* GApplication re-parses argv and rejects unknown options, so hand it
     * a clean argv containing only the program name. */
    char *app_argv[] = {argv[0], NULL};
    int app_argc = 1;
    GtkApplication *app = gtk_application_new("dev.metroidvania.aos_lab",
                                              G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), &v);
    int status = g_application_run(G_APPLICATION(app), app_argc, app_argv);
    g_object_unref(app);
    aos_labdb_close(v.db);
    return status;
}
