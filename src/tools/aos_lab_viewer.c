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
    char db_path[1024];
    char rom_path[1024];
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

static gboolean poll_refresh(gpointer data) {
    Viewer *v = data;
    refresh_trials(v);
    refresh_weights(v);
    refresh_states(v);
    refresh_runs(v);
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

    gtk_window_present(GTK_WINDOW(v->window));
    g_timeout_add(POLL_MS, poll_refresh, v);
    poll_refresh(v);
}

int main(int argc, char **argv) {
    Viewer v;
    memset(&v, 0, sizeof v);
    snprintf(v.db_path, sizeof v.db_path, "aos_lab.db");

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

    GtkApplication *app = gtk_application_new("dev.metroidvania.aos_lab",
                                              G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), &v);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    aos_labdb_close(v.db);
    return status;
}
