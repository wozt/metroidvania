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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
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
} Viewer;

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
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), column_view);
    *store_out = store;
    return scroll;
}

static void store_row(GListStore *store, const char *line) {
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
        GtkAdjustment *adj = gtk_scrollable_get_vadjustment(
            GTK_SCROLLABLE(scroll));
        if (adj)
            gtk_adjustment_set_value(adj, gtk_adjustment_get_upper(adj));
    }
}

static gboolean on_log_pipe(GIOChannel *channel, GIOCondition cond,
                            gpointer data) {
    LogPipe *pipe = data;
    Viewer *v = g_object_get_data(G_OBJECT(channel), "viewer");
    if (cond & (G_IO_HUP | G_IO_ERR | G_IO_NVAL)) {
        g_io_channel_unref(channel);
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
        g_object_set_data_full(G_OBJECT(ch), "viewer", v, NULL);
        g_object_set_data(G_OBJECT(ch), "pipe", pipe);
        /* store pipe on the channel so the callback can find it */
        g_object_set_data(G_OBJECT(ch), "logpipe", pipe);
        v->pipes = g_list_prepend(v->pipes, pipe);
        /* viewer must be attached before the watch can fire */
        g_object_set_data(G_OBJECT(ch), "viewer", v);
        g_io_add_watch(ch, G_IO_IN | G_IO_HUP,
                       (GIOFunc)on_log_pipe, pipe);
        pipe->fd = fd;
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

/* The callback above needs the Viewer; we stored it on the channel but
 * the watch data is the LogPipe. To keep it simple the callback resolves
 * the viewer through a file-scope pointer set at activate time. */
static Viewer *g_viewer;

/* ---- refresh -------------------------------------------------------- */

static void refresh_trials(Viewer *v) {
    int count = 0;
    AosTrialRow *rows = aos_labdb_recent_trials(v->db, MAX_ROWS, &count);
    g_list_store_remove_all(v->trials_store);
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

static void refresh_workers(Viewer *v) {
    static const char *phase_css[] = {"", "w-running", "w-paused"};
    for (int i = 0; i < 8; ++i) {
        WorkerCard *wc = &v->workers[i];
        char pk[64], sk[64], ck[64], vk[64];
        snprintf(pk, sizeof pk, "worker.%d.phase", i);
        snprintf(sk, sizeof sk, "worker.%d.status", i);
        snprintf(ck, sizeof ck, "lab.control.%d", i);
        snprintf(vk, sizeof vk, "worker.%d.speed", i);
        char *phase = aos_labdb_meta_get(v->db, pk);
        char *wstatus = aos_labdb_meta_get(v->db, sk);
        char *control = aos_labdb_meta_get(v->db, ck);
        char *speed = aos_labdb_meta_get(v->db, vk);
        const char *phase_str = phase ? phase : "idle";
        int phase_id = !strcmp(phase_str, "running") ? 1 :
                       !strcmp(phase_str, "paused") ? 2 : 0;
        gtk_widget_set_css_classes(wc->card,
                                  phase_css[phase_id] ?
                                  (const char *[]){phase_css[phase_id], NULL}
                                  : NULL);
        char head[64];
        snprintf(head, sizeof head, "Worker %d", i);
        gtk_label_set_text(GTK_LABEL(wc->name), head);
        gtk_label_set_text(GTK_LABEL(wc->phase), phase_str);
        gtk_label_set_text(GTK_LABEL(wc->detail),
                           wstatus ? wstatus : "");
        gtk_button_set_label(GTK_BUTTON(wc->pause_button),
                             control && !strcmp(control, "pause")
                                 ? "Resume" : "Pause");
        gtk_widget_set_sensitive(wc->pause_button, phase_id != 0);
        /* speed slider: meta wins unless the user is mid-drag */
        int speed_val = speed ? atoi(speed) : 0;
        double cur = gtk_range_get_value(GTK_RANGE(wc->speed_scale));
        if (fabs(cur - speed_val) >= 0.5)
            gtk_range_set_value(GTK_RANGE(wc->speed_scale), speed_val);
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

static void on_speed_changed(GtkRange *range, gpointer user_data) {
    Viewer *v = user_data;
    int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(range), "wid"));
    int speed = (int)gtk_range_get_value(range);
    char key[64], val[8];
    snprintf(key, sizeof key, "worker.%d.speed", id);
    snprintf(val, sizeof val, "%d", speed);
    aos_labdb_meta_set(v->db, key, val);
}

static void on_launch_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    Viewer *v = user_data;
    int count = 0;
    AosStateRow *states = aos_labdb_states(v->db, 512, &count);
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

static void on_activate(GtkApplication *app, gpointer user_data) {
    Viewer *v = user_data;
    g_viewer = v;
    v->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(v->window), "Aria Lab — learning registry");
    gtk_window_set_default_size(GTK_WINDOW(v->window), 1180, 780);

    /* status colours for worker cards */
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_string(
        css,
        "frame.w-running { border: 2px solid #43a047; border-radius: 6px; }"
        "frame.w-paused  { border: 2px solid #fb8c00; border-radius: 6px; }"
        "frame           { border: 1px solid #546e7a; border-radius: 6px; }"
        "label.w-phase   { font-weight: bold; }"
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

    /* ---- workers section ------------------------------------------ */
    GtkWidget *workers_label = gtk_label_new("Workers — eight parallel "
                                             "learning slots");
    gtk_label_set_xalign(GTK_LABEL(workers_label), 0.0);
    gtk_widget_add_css_class(workers_label, "title-4");
    gtk_box_append(GTK_BOX(root), workers_label);
    GtkWidget *workers_tip = gtk_label_new(
        "Each card is one emulator process. Pause freezes it between "
        "trials, Stop ends the batch, Resume continues.");
    gtk_label_set_xalign(GTK_LABEL(workers_tip), 0.0);
    gtk_widget_add_css_class(workers_tip, "dim-label");
    gtk_box_append(GTK_BOX(root), workers_tip);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    for (int i = 0; i < 8; ++i) {
        GtkWidget *card = gtk_frame_new(NULL);
        WorkerCard *wc = &v->workers[i];
        wc->card = card;
        GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_frame_set_child(GTK_FRAME(card), vbox);
        gtk_widget_set_margin_start(vbox, 8);
        gtk_widget_set_margin_end(vbox, 8);
        gtk_widget_set_margin_top(vbox, 6);
        gtk_widget_set_margin_bottom(vbox, 6);

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
        gtk_box_append(GTK_BOX(vbox), btns);

        GtkWidget *sbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_box_append(GTK_BOX(vbox), sbox);
        gtk_box_append(GTK_BOX(sbox), gtk_label_new("Speed:"));
        wc->speed_label = gtk_label_new("Auto");
        gtk_widget_set_size_request(wc->speed_label, 42, -1);
        gtk_label_set_xalign(GTK_LABEL(wc->speed_label), 0.0);
        gtk_box_append(GTK_BOX(sbox), wc->speed_label);
        wc->speed_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
                                                 0, 16, 1);
        gtk_scale_set_draw_value(GTK_SCALE(wc->speed_scale), FALSE);
        gtk_scale_add_mark(GTK_SCALE(wc->speed_scale), 0, GTK_POS_BOTTOM,
                           "Auto");
        gtk_scale_add_mark(GTK_SCALE(wc->speed_scale), 16, GTK_POS_BOTTOM,
                           "x16");
        gtk_widget_set_hexpand(wc->speed_scale, TRUE);
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
    gtk_box_append(GTK_BOX(root), workers_scroll);

    /* ---- launch controls ------------------------------------------- */
    GtkWidget *launch_frame = gtk_frame_new("Start a learning batch");
    GtkWidget *lbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_frame_set_child(GTK_FRAME(launch_frame), lbox);
    gtk_widget_set_margin_start(lbox, 10);
    gtk_widget_set_margin_end(lbox, 10);
    gtk_widget_set_margin_top(lbox, 8);
    gtk_widget_set_margin_bottom(lbox, 8);
    gtk_box_append(GTK_BOX(root), launch_frame);

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
    gtk_box_append(GTK_BOX(root), v->hint);

    v->worker_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(v->worker_label), 0.0);
    gtk_widget_add_css_class(v->worker_label, "dim-label");
    gtk_box_append(GTK_BOX(root), v->worker_label);

    /* ---- live log ----------------------------------------------------- */
    GtkWidget *log_label = gtk_label_new("Live log — worker output "
                                         "(memory only, nothing written "
                                         "to disk)");
    gtk_label_set_xalign(GTK_LABEL(log_label), 0.0);
    gtk_widget_add_css_class(log_label, "title-5");
    gtk_box_append(GTK_BOX(root), log_label);
    GtkWidget *log_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(log_scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_height(
        GTK_SCROLLED_WINDOW(log_scroll), 140);
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
    gtk_box_append(GTK_BOX(root), log_scroll);

    /* ---- registry tables --------------------------------------------- */
    v->notebook = gtk_notebook_new();
    gtk_box_append(GTK_BOX(root), v->notebook);

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
