/* SPDX-License-Identifier: GPL-3.0-only */
/* GTK4 laboratory viewer for the Aria emulator trial database.
 *
 * Polls the local SQLite database and shows the learning registry live:
 * recent trials, per-parameter success weights, stored save states and
 * worker runs. Fully offline; the worker writes, this app only reads.
 *
 * Usage: aos_lab_viewer --db PATH [--rom PATH]
 */
#define _GNU_SOURCE

#include "aos_lab_db.h"

#include <gtk/gtk.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define POLL_MS 1000
#define MAX_ROWS 400
#define FIELD_SEP '\x1f'

typedef struct {
    GtkWidget *window;
    GtkWidget *notebook;
    GListStore *trials_store;
    GListStore *weights_store;
    GListStore *states_store;
    GListStore *runs_store;
    GtkWidget *status;
    GtkWidget *worker_label;
    GtkWidget *launch_button;
    GtkWidget *iterations_spin;
    GtkWidget *worker_id_spin;
    GtkWidget *worker_labels[8];
    char db_path[1024];
    char rom_path[1024];
    char worker_path[4200];
    AosLabDb *db;
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
static void add_column(GtkWidget *column_view, const char *title, int field) {
    GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
    g_signal_connect(factory, "setup", G_CALLBACK(col_setup), NULL);
    g_signal_connect(factory, "bind", G_CALLBACK(col_bind),
                     GINT_TO_POINTER(field));
    GtkColumnViewColumn *col = gtk_column_view_column_new(title, factory);
    gtk_column_view_append_column(GTK_COLUMN_VIEW(column_view), col);
}

static GtkWidget *scrolled_page(GListStore **store_out,
                                const char **titles) {
    GListStore *store = g_list_store_new(GTK_TYPE_STRING_OBJECT);
    GtkSingleSelection *selection =
        gtk_single_selection_new(G_LIST_MODEL(store));
    GtkWidget *column_view =
        gtk_column_view_new(GTK_SELECTION_MODEL(selection));
    for (int i = 0; titles[i]; ++i) add_column(column_view, titles[i], i);
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

static void worker_control(int worker_id, const char *control,
                           gpointer user_data) {
    Viewer *v = user_data;
    char key[64];
    snprintf(key, sizeof key, "lab.control.%d", worker_id);
    aos_labdb_meta_set(v->db, key, control);
}

static void on_worker_pause(GtkButton *button, gpointer user_data) {
    Viewer *v = user_data;
    int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "wid"));
    char key[64];
    snprintf(key, sizeof key, "lab.control.%d", id);
    char *control = aos_labdb_meta_get(v->db, key);
    worker_control(id, control && !strcmp(control, "pause") ? "resume"
                                                          : "pause", v);
    free(control);
}

static void on_worker_stop(GtkButton *button, gpointer user_data) {
    Viewer *v = user_data;
    int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "wid"));
    worker_control(id, "stop", v);
}

static void on_launch(GtkButton *button, gpointer user_data) {
    (void)button;
    Viewer *v = user_data;
    if (!v->rom_path[0] || !v->worker_path[0]) {
        gtk_label_set_text(GTK_LABEL(v->worker_label),
                           "need --rom and aos_lab_worker beside this app");
        return;
    }
    /* latest state id */
    int scount = 0;
    AosStateRow *states = aos_labdb_states(v->db, 1, &scount);
    if (scount == 0) {
        gtk_label_set_text(GTK_LABEL(v->worker_label),
                           "no state in db — record one first");
        aos_labdb_free_state_rows(states, scount);
        return;
    }
    int iterations = gtk_spin_button_get_value_as_int(
        GTK_SPIN_BUTTON(v->iterations_spin));
    int worker_id = gtk_spin_button_get_value_as_int(
        GTK_SPIN_BUTTON(v->worker_id_spin));
    char cmd[8192];
    snprintf(cmd, sizeof cmd,
             "'%s' --db '%s' --rom '%s' --state %lld --iterations %d"
             " --worker-id %d --seed %ld"
             " >> '%s.worker%d.log' 2>&1",
             v->worker_path, v->db_path, v->rom_path,
             (long long)states[0].id, iterations, worker_id,
             (long)(time(NULL) ^ (getpid() << worker_id)),
             v->db_path, worker_id);
    aos_labdb_free_state_rows(states, scount);
    GError *error = NULL;
    char *argv[] = {"/bin/sh", "-c", cmd, NULL};
    if (!g_spawn_async(NULL, argv, NULL,
                       G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                       NULL, NULL, NULL, &error)) {
        char msg[256];
        snprintf(msg, sizeof msg, "spawn failed: %s",
                 error ? error->message : "unknown");
        gtk_label_set_text(GTK_LABEL(v->worker_label), msg);
        if (error) g_error_free(error);
        return;
    }
    gtk_label_set_text(GTK_LABEL(v->worker_label), "worker launched");
}

static gboolean poll_refresh(gpointer data) {
    Viewer *v = data;
    refresh_trials(v);
    refresh_weights(v);
    refresh_states(v);
    refresh_runs(v);
    /* per-worker status cards */
    for (int i = 0; i < 8; ++i) {
        char pk[64], sk[64];
        snprintf(pk, sizeof pk, "worker.%d.phase", i);
        snprintf(sk, sizeof sk, "worker.%d.status", i);
        char *phase = aos_labdb_meta_get(v->db, pk);
        char *wstatus = aos_labdb_meta_get(v->db, sk);
        char text[600];
        snprintf(text, sizeof text, "W%d  %s%s%s", i,
                 phase ? phase : "-", wstatus ? " — " : "",
                 wstatus ? wstatus : "");
        gtk_label_set_text(GTK_LABEL(v->worker_labels[i]), text);
        free(phase);
        free(wstatus);
    }
    gtk_label_set_text(GTK_LABEL(v->status), v->db_path);
    return G_SOURCE_CONTINUE;
}

static void on_activate(GtkApplication *app, gpointer user_data) {
    Viewer *v = user_data;
    v->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(v->window), "Aria Lab — trial registry");
    gtk_window_set_default_size(GTK_WINDOW(v->window), 1100, 700);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_window_set_child(GTK_WINDOW(v->window), box);
    v->notebook = gtk_notebook_new();
    gtk_box_append(GTK_BOX(box), v->notebook);

    const char *trial_titles[] = {"id", "run", "state", "action",
                                  "outcome", "params", "frames",
                                  "reward", "ms", "time", NULL};
    GtkWidget *p1 = scrolled_page(&v->trials_store, trial_titles);
    gtk_notebook_append_page(GTK_NOTEBOOK(v->notebook), p1,
                             gtk_label_new("Trials"));

    const char *weight_titles[] = {"action", "params", "attempts",
                                   "successes", "rate", "reward", NULL};
    GtkWidget *p2 = scrolled_page(&v->weights_store, weight_titles);
    gtk_notebook_append_page(GTK_NOTEBOOK(v->notebook), p2,
                             gtk_label_new("Weights"));

    const char *state_titles[] = {"id", "name", "note", "parent", "bytes",
                                  "created", NULL};
    GtkWidget *p3 = scrolled_page(&v->states_store, state_titles);
    gtk_notebook_append_page(GTK_NOTEBOOK(v->notebook), p3,
                             gtk_label_new("States"));

    const char *run_titles[] = {"id", "started", "rom", "note", "frames",
                                "trials", NULL};
    GtkWidget *p4 = scrolled_page(&v->runs_store, run_titles);
    gtk_notebook_append_page(GTK_NOTEBOOK(v->notebook), p4,
                             gtk_label_new("Runs"));

    v->status = gtk_label_new("");
    gtk_widget_set_halign(v->status, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), v->status);

    /* Workers tab: 8 parallel slots, each with pause/stop controls. */
    {
        GtkWidget *grid = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
        gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
        for (int i = 0; i < 8; ++i) {
            GtkWidget *card = gtk_frame_new(NULL);
            GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
            gtk_frame_set_child(GTK_FRAME(card), vbox);
            gtk_box_append(GTK_BOX(vbox), gtk_label_new(""));
            v->worker_labels[i] = gtk_label_new("idle");
            gtk_widget_set_halign(v->worker_labels[i], GTK_ALIGN_START);
            gtk_box_append(GTK_BOX(vbox), v->worker_labels[i]);
            GtkWidget *btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
            GtkWidget *pb = gtk_button_new_with_label("Pause");
            GtkWidget *sb = gtk_button_new_with_label("Stop");
            g_object_set_data(G_OBJECT(pb), "wid", GINT_TO_POINTER(i));
            g_object_set_data(G_OBJECT(sb), "wid", GINT_TO_POINTER(i));
            g_signal_connect(pb, "clicked", G_CALLBACK(on_worker_pause), v);
            g_signal_connect(sb, "clicked", G_CALLBACK(on_worker_stop), v);
            gtk_box_append(GTK_BOX(btns), pb);
            gtk_box_append(GTK_BOX(btns), sb);
            gtk_box_append(GTK_BOX(vbox), btns);
            gtk_grid_attach(GTK_GRID(grid), card, i % 4, i / 4, 1, 1);
        }
        GtkWidget *scroll = gtk_scrolled_window_new();
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), grid);
        gtk_notebook_append_page(GTK_NOTEBOOK(v->notebook), scroll,
                                 gtk_label_new("Workers"));
    }

    GtkWidget *controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append(GTK_BOX(box), controls);
    gtk_box_append(GTK_BOX(controls), gtk_label_new("worker:"));
    v->worker_id_spin = gtk_spin_button_new_with_range(0, 7, 1);
    gtk_box_append(GTK_BOX(controls), v->worker_id_spin);
    gtk_box_append(GTK_BOX(controls), gtk_label_new("iters:"));
    v->iterations_spin = gtk_spin_button_new_with_range(1, 100000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(v->iterations_spin), 50);
    gtk_box_append(GTK_BOX(controls), v->iterations_spin);
    v->launch_button = gtk_button_new_with_label("Lancer batch");
    g_signal_connect(v->launch_button, "clicked", G_CALLBACK(on_launch), v);
    gtk_box_append(GTK_BOX(controls), v->launch_button);
    v->worker_label = gtk_label_new("");
    gtk_widget_set_halign(v->worker_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), v->worker_label);

    gtk_window_present(GTK_WINDOW(v->window));
    g_timeout_add(POLL_MS, poll_refresh, v);
    poll_refresh(v);
}

int main(int argc, char **argv) {
    Viewer v;
    memset(&v, 0, sizeof v);
    /* database/ is a symlink to the bulk-storage lab directory (gitignored). */
    snprintf(v.db_path, sizeof v.db_path, "database/lab.db");

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--db") && i + 1 < argc) {
            snprintf(v.db_path, sizeof v.db_path, "%s", argv[++i]);
        } else if (!strcmp(argv[i], "--rom") && i + 1 < argc) {
            snprintf(v.rom_path, sizeof v.rom_path, "%s", argv[++i]);
        } else {
            fprintf(stderr, "usage: aos_lab_viewer [--db PATH] [--rom PATH]\n");
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
