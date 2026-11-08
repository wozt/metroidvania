/* SPDX-License-Identifier: GPL-3.0-only */
#define _GNU_SOURCE
/* SQLite laboratory database (see aos_lab_db.h). */

#include "aos_lab_db.h"

#include <sqlite3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct AosLabDb {
    sqlite3 *handle;
    char error[512];
};

static const char *SCHEMA =
    /* lab data is reproducible experiment output, not user documents:
     * skip per-commit fsyncs and keep temp b-trees in RAM so the
     * workers do not hammer the disk */
    "PRAGMA journal_mode=WAL;"
    "PRAGMA synchronous=OFF;"
    "PRAGMA temp_store=MEMORY;"
    "PRAGMA wal_autocheckpoint=4000;"
    /* keep the database effectively in RAM: mmap the main file and a
     * 64 MiB page cache per connection — workers read state blobs
     * constantly and the OS page cache was already doing this, now it
     * is explicit */
    "PRAGMA mmap_size=1073741824;"
    "PRAGMA cache_size=-65536;"
    "CREATE TABLE IF NOT EXISTS meta("
    "  key TEXT PRIMARY KEY, value TEXT);"
    "CREATE TABLE IF NOT EXISTS runs("
    "  id INTEGER PRIMARY KEY, started_at TEXT, rom TEXT,"
    "  frames INTEGER DEFAULT 0, note TEXT, trials INTEGER DEFAULT 0);"
    "CREATE TABLE IF NOT EXISTS states("
    "  id INTEGER PRIMARY KEY, name TEXT, note TEXT, parent_id INTEGER,"
    "  blob BLOB, blob_size INTEGER, created_at TEXT);"
    "CREATE TABLE IF NOT EXISTS actions("
    "  id INTEGER PRIMARY KEY, name TEXT, kind TEXT, params TEXT,"
    "  eval TEXT, script TEXT, created_at TEXT);"
    "CREATE TABLE IF NOT EXISTS trials("
    "  id INTEGER PRIMARY KEY, run_id INTEGER, state_id INTEGER,"
    "  action_id INTEGER, params TEXT, keys TEXT, frames INTEGER,"
    "  outcome TEXT, reward REAL DEFAULT 0, metrics TEXT,"
    "  started_at TEXT, duration_ms INTEGER DEFAULT 0);"
    "CREATE TABLE IF NOT EXISTS weights("
    "  action_id INTEGER, params TEXT,"
    "  attempts INTEGER DEFAULT 0, successes INTEGER DEFAULT 0,"
    "  reward_sum REAL DEFAULT 0,"
    "  PRIMARY KEY(action_id, params));"
    "CREATE INDEX IF NOT EXISTS idx_trials_run ON trials(run_id);"
    "CREATE INDEX IF NOT EXISTS idx_trials_action ON trials(action_id);"
    "CREATE INDEX IF NOT EXISTS idx_trials_state ON trials(state_id);";

static char *now_iso(void) {
    time_t t = time(NULL);
    struct tm tm;
    gmtime_r(&t, &tm);
    char *buf = malloc(32);
    strftime(buf, 32, "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

static bool exec_sql(AosLabDb *db, const char *sql) {
    char *err = NULL;
    if (sqlite3_exec(db->handle, sql, NULL, NULL, &err) != SQLITE_OK) {
        snprintf(db->error, sizeof db->error, "%s", err ? err : "sql error");
        sqlite3_free(err);
        return false;
    }
    return true;
}

static bool bind_text(sqlite3_stmt *st, int idx, const char *value) {
    if (!value) return sqlite3_bind_null(st, idx) == SQLITE_OK;
    return sqlite3_bind_text(st, idx, value, -1, SQLITE_TRANSIENT) ==
           SQLITE_OK;
}

static char *col_text(sqlite3_stmt *st, int col) {
    const unsigned char *t = sqlite3_column_text(st, col);
    if (!t) return NULL;
    return strdup((const char *)t);
}

AosLabDb *aos_labdb_open(const char *path, char *err, size_t err_size) {
    AosLabDb *db = calloc(1, sizeof *db);
    if (!db) return NULL;
    if (sqlite3_open(path, &db->handle) != SQLITE_OK) {
        snprintf(db->error, sizeof db->error, "%s",
                 db->handle ? sqlite3_errmsg(db->handle) : "open failed");
        goto fail;
    }
    sqlite3_busy_timeout(db->handle, 5000);
    if (!exec_sql(db, SCHEMA)) goto fail;
    return db;

fail:
    if (err && err_size) snprintf(err, err_size, "%s", db->error);
    aos_labdb_close(db);
    return NULL;
}

void aos_labdb_close(AosLabDb *db) {
    if (!db) return;
    if (db->handle) sqlite3_close(db->handle);
    free(db);
}

const char *aos_labdb_error(AosLabDb *db) { return db ? db->error : "no db"; }

int64_t aos_labdb_add_state(AosLabDb *db, const char *name, const char *note,
                            int64_t parent_id, const void *blob,
                            size_t blob_size) {
    sqlite3_stmt *st = NULL;
    const char *sql =
        "INSERT INTO states(name,note,parent_id,blob,blob_size,created_at)"
        " VALUES(?,?,?,?,?,?)";
    if (sqlite3_prepare_v2(db->handle, sql, -1, &st, NULL) != SQLITE_OK)
        return -1;
    char *ts = now_iso();
    bind_text(st, 1, name);
    bind_text(st, 2, note);
    if (parent_id > 0) sqlite3_bind_int64(st, 3, parent_id);
    else sqlite3_bind_null(st, 3);
    if (blob && blob_size)
        sqlite3_bind_blob(st, 4, blob, (int)blob_size, SQLITE_TRANSIENT);
    else
        sqlite3_bind_null(st, 4);
    sqlite3_bind_int(st, 5, (int)blob_size);
    bind_text(st, 6, ts);
    free(ts);
    int64_t id = sqlite3_step(st) == SQLITE_DONE
                     ? sqlite3_last_insert_rowid(db->handle)
                     : -1;
    sqlite3_finalize(st);
    return id;
}

uint8_t *aos_labdb_state_blob(AosLabDb *db, int64_t id, size_t *out_size) {
    sqlite3_stmt *st = NULL;
    const char *sql = "SELECT blob, blob_size FROM states WHERE id=?";
    if (sqlite3_prepare_v2(db->handle, sql, -1, &st, NULL) != SQLITE_OK)
        return NULL;
    sqlite3_bind_int64(st, 1, id);
    uint8_t *blob = NULL;
    *out_size = 0;
    if (sqlite3_step(st) == SQLITE_ROW) {
        const void *data = sqlite3_column_blob(st, 0);
        int size = sqlite3_column_int(st, 1);
        if (data && size > 0) {
            blob = malloc((size_t)size);
            memcpy(blob, data, (size_t)size);
            *out_size = (size_t)size;
        }
    }
    sqlite3_finalize(st);
    return blob;
}

int64_t aos_labdb_update_action(AosLabDb *db, const char *name,
                              const char *kind, const char *params,
                              const char *eval, const char *script) {
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->handle,
            "UPDATE actions SET kind=?,params=?,eval=?,script=?"
            " WHERE name=?", -1, &st, NULL) != SQLITE_OK)
        return -1;
    bind_text(st, 1, kind);
    bind_text(st, 2, params);
    bind_text(st, 3, eval);
    bind_text(st, 4, script);
    bind_text(st, 5, name);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? sqlite3_changes(db->handle) : -1;
}

int64_t aos_labdb_add_action(AosLabDb *db, const char *name, const char *kind,
                             const char *params, const char *eval,
                             const char *script) {
    sqlite3_stmt *st = NULL;
    const char *sql =
        "INSERT INTO actions(name,kind,params,eval,script,created_at)"
        " VALUES(?,?,?,?,?,?)";
    if (sqlite3_prepare_v2(db->handle, sql, -1, &st, NULL) != SQLITE_OK)
        return -1;
    char *ts = now_iso();
    bind_text(st, 1, name);
    bind_text(st, 2, kind);
    bind_text(st, 3, params);
    bind_text(st, 4, eval);
    bind_text(st, 5, script);
    bind_text(st, 6, ts);
    free(ts);
    int64_t id = sqlite3_step(st) == SQLITE_DONE
                     ? sqlite3_last_insert_rowid(db->handle)
                     : -1;
    sqlite3_finalize(st);
    return id;
}

int64_t aos_labdb_begin_run(AosLabDb *db, const char *rom, const char *note) {
    sqlite3_stmt *st = NULL;
    const char *sql =
        "INSERT INTO runs(started_at,rom,note) VALUES(?,?,?)";
    if (sqlite3_prepare_v2(db->handle, sql, -1, &st, NULL) != SQLITE_OK)
        return -1;
    char *ts = now_iso();
    bind_text(st, 1, ts);
    bind_text(st, 2, rom);
    bind_text(st, 3, note);
    free(ts);
    int64_t id = sqlite3_step(st) == SQLITE_DONE
                     ? sqlite3_last_insert_rowid(db->handle)
                     : -1;
    sqlite3_finalize(st);
    return id;
}

void aos_labdb_end_run(AosLabDb *db, int64_t run_id, int frames, int trials) {
    sqlite3_stmt *st = NULL;
    const char *sql = "UPDATE runs SET frames=?, trials=? WHERE id=?";
    if (sqlite3_prepare_v2(db->handle, sql, -1, &st, NULL) != SQLITE_OK) return;
    sqlite3_bind_int(st, 1, frames);
    sqlite3_bind_int(st, 2, trials);
    sqlite3_bind_int64(st, 3, run_id);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

int64_t aos_labdb_add_trial(AosLabDb *db, int64_t run_id, int64_t state_id,
                            int64_t action_id, const char *params,
                            const char *keys, int frames, const char *outcome,
                            double reward, const char *metrics,
                            int duration_ms) {
    sqlite3_stmt *st = NULL;
    const char *sql =
        "INSERT INTO trials(run_id,state_id,action_id,params,keys,frames,"
        "outcome,reward,metrics,started_at,duration_ms)"
        " VALUES(?,?,?,?,?,?,?,?,?,?,?)";
    if (sqlite3_prepare_v2(db->handle, sql, -1, &st, NULL) != SQLITE_OK)
        return -1;
    char *ts = now_iso();
    sqlite3_bind_int64(st, 1, run_id);
    sqlite3_bind_int64(st, 2, state_id);
    sqlite3_bind_int64(st, 3, action_id);
    bind_text(st, 4, params);
    bind_text(st, 5, keys);
    sqlite3_bind_int(st, 6, frames);
    bind_text(st, 7, outcome);
    sqlite3_bind_double(st, 8, reward);
    bind_text(st, 9, metrics);
    bind_text(st, 10, ts);
    sqlite3_bind_int(st, 11, duration_ms);
    free(ts);
    int64_t id = sqlite3_step(st) == SQLITE_DONE
                     ? sqlite3_last_insert_rowid(db->handle)
                     : -1;
    sqlite3_finalize(st);
    if (id < 0) return id;

    /* Update the weight row for (action_id, params). */
    int success = outcome && !strcmp(outcome, "success");
    const char *wsql =
        "INSERT INTO weights(action_id,params,attempts,successes,reward_sum)"
        " VALUES(?,?,1,?,?)"
        " ON CONFLICT(action_id,params) DO UPDATE SET"
        " attempts=attempts+1,"
        " successes=successes+excluded.successes,"
        " reward_sum=reward_sum+excluded.reward_sum";
    if (sqlite3_prepare_v2(db->handle, wsql, -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(st, 1, action_id);
        bind_text(st, 2, params ? params : "");
        sqlite3_bind_int(st, 3, success ? 1 : 0);
        sqlite3_bind_double(st, 4, reward);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }
    return id;
}

AosTrialRow *aos_labdb_recent_trials(AosLabDb *db, int limit, int *out_count) {
    *out_count = 0;
    sqlite3_stmt *st = NULL;
    const char *sql =
        "SELECT id,run_id,state_id,action_id,params,keys,frames,outcome,"
        "reward,metrics,started_at,duration_ms FROM trials"
        " ORDER BY id DESC LIMIT ?";
    if (sqlite3_prepare_v2(db->handle, sql, -1, &st, NULL) != SQLITE_OK)
        return NULL;
    sqlite3_bind_int(st, 1, limit);
    int cap = 16, n = 0;
    AosTrialRow *rows = malloc(cap * sizeof *rows);
    while (sqlite3_step(st) == SQLITE_ROW) {
        if (n == cap) {
            cap *= 2;
            rows = realloc(rows, cap * sizeof *rows);
        }
        AosTrialRow *r = &rows[n++];
        memset(r, 0, sizeof *r);
        r->id = sqlite3_column_int64(st, 0);
        r->run_id = sqlite3_column_int64(st, 1);
        r->state_id = sqlite3_column_int64(st, 2);
        r->action_id = sqlite3_column_int64(st, 3);
        r->params = col_text(st, 4);
        r->keys = col_text(st, 5);
        r->frames = sqlite3_column_int(st, 6);
        r->outcome = col_text(st, 7);
        r->reward = sqlite3_column_double(st, 8);
        r->metrics = col_text(st, 9);
        r->started_at = col_text(st, 10);
        r->duration_ms = sqlite3_column_int(st, 11);
    }
    sqlite3_finalize(st);
    *out_count = n;
    return rows;
}

static AosStateRow *states_query(AosLabDb *db, int limit, int *out_count,
                               const char *where) {
    *out_count = 0;
    sqlite3_stmt *st = NULL;
    char sql[512];
    snprintf(sql, sizeof sql,
        "SELECT id,name,note,parent_id,blob_size,created_at FROM states"
        " %s ORDER BY id DESC LIMIT ?", where);
    if (sqlite3_prepare_v2(db->handle, sql, -1, &st, NULL) != SQLITE_OK)
        return NULL;
    sqlite3_bind_int(st, 1, limit);
    int cap = 16, n = 0;
    AosStateRow *rows = malloc(cap * sizeof *rows);
    while (sqlite3_step(st) == SQLITE_ROW) {
        if (n == cap) {
            cap *= 2;
            rows = realloc(rows, cap * sizeof *rows);
        }
        AosStateRow *r = &rows[n++];
        memset(r, 0, sizeof *r);
        r->id = sqlite3_column_int64(st, 0);
        r->name = col_text(st, 1);
        r->note = col_text(st, 2);
        r->parent_id = sqlite3_column_int64(st, 3);
        r->blob_size = sqlite3_column_int(st, 4);
        r->created_at = col_text(st, 5);
    }
    sqlite3_finalize(st);
    *out_count = n;
    return rows;
}

AosStateRow *aos_labdb_states(AosLabDb *db, int limit, int *out_count) {
    return states_query(db, limit, out_count, "");
}

/* Launch-worthy checkpoints only: states a trial actually used, the
 * validated scout_play_* checkpoints, and manual/other records —
 * everything the purge would keep. Follows the same rhythm as the
 * cleanup rules so the picker never lists dead scene dumps. */
AosStateRow *aos_labdb_valuable_states(AosLabDb *db, int limit,
                                       int *out_count) {
    return states_query(db, limit, out_count,
        "WHERE (name LIKE 'scout_play%'"
        " AND id IN (SELECT id FROM states WHERE name LIKE 'scout_play%'"
        " ORDER BY id DESC LIMIT 40))"
        " OR (name NOT LIKE 'scout_c%' AND name NOT LIKE 'scout_play%'"
        " AND name NOT LIKE 'auto_%')"
        " OR id IN (SELECT state_id FROM trials WHERE reward > 0.5"
        " AND state_id IS NOT NULL)");
}

AosWeightRow *aos_labdb_weights(AosLabDb *db, int limit, int *out_count) {
    *out_count = 0;
    sqlite3_stmt *st = NULL;
    const char *sql =
        "SELECT w.action_id, w.params, w.attempts, w.successes,"
        " w.reward_sum,"
        " CASE WHEN w.attempts>0 THEN 1.0*w.successes/w.attempts ELSE 0 END"
        " FROM weights w ORDER BY"
        " (1.0*w.successes/w.attempts) DESC, w.attempts DESC LIMIT ?";
    /* SQLite division by zero yields NULL; guard with attempts>0. */
    if (sqlite3_prepare_v2(db->handle, sql, -1, &st, NULL) != SQLITE_OK)
        return NULL;
    sqlite3_bind_int(st, 1, limit);
    int cap = 16, n = 0;
    AosWeightRow *rows = malloc(cap * sizeof *rows);
    while (sqlite3_step(st) == SQLITE_ROW) {
        if (n == cap) {
            cap *= 2;
            rows = realloc(rows, cap * sizeof *rows);
        }
        AosWeightRow *r = &rows[n++];
        memset(r, 0, sizeof *r);
        r->action_id = sqlite3_column_int64(st, 0);
        r->params = col_text(st, 1);
        r->attempts = sqlite3_column_int(st, 2);
        r->successes = sqlite3_column_int(st, 3);
        r->reward_sum = sqlite3_column_double(st, 4);
        r->success_rate = sqlite3_column_double(st, 5);
    }
    sqlite3_finalize(st);
    *out_count = n;
    return rows;
}


AosActionRow *aos_labdb_actions(AosLabDb *db, int *out_count) {
    *out_count = 0;
    sqlite3_stmt *st = NULL;
    const char *sql =
        "SELECT id,name,kind,params,eval,script FROM actions ORDER BY id";
    if (sqlite3_prepare_v2(db->handle, sql, -1, &st, NULL) != SQLITE_OK)
        return NULL;
    int cap = 16, n = 0;
    AosActionRow *rows = malloc(cap * sizeof *rows);
    while (sqlite3_step(st) == SQLITE_ROW) {
        if (n == cap) {
            cap *= 2;
            rows = realloc(rows, cap * sizeof *rows);
        }
        AosActionRow *r = &rows[n++];
        memset(r, 0, sizeof *r);
        r->id = sqlite3_column_int64(st, 0);
        r->name = col_text(st, 1);
        r->kind = col_text(st, 2);
        r->params = col_text(st, 3);
        r->eval = col_text(st, 4);
        r->script = col_text(st, 5);
    }
    sqlite3_finalize(st);
    *out_count = n;
    return rows;
}

AosRunRow *aos_labdb_runs(AosLabDb *db, int limit, int *out_count) {
    *out_count = 0;
    sqlite3_stmt *st = NULL;
    const char *sql =
        "SELECT id,started_at,rom,frames,note,trials FROM runs"
        " ORDER BY id DESC LIMIT ?";
    if (sqlite3_prepare_v2(db->handle, sql, -1, &st, NULL) != SQLITE_OK)
        return NULL;
    sqlite3_bind_int(st, 1, limit);
    int cap = 16, n = 0;
    AosRunRow *rows = malloc(cap * sizeof *rows);
    while (sqlite3_step(st) == SQLITE_ROW) {
        if (n == cap) {
            cap *= 2;
            rows = realloc(rows, cap * sizeof *rows);
        }
        AosRunRow *r = &rows[n++];
        memset(r, 0, sizeof *r);
        r->id = sqlite3_column_int64(st, 0);
        r->started_at = col_text(st, 1);
        r->rom = col_text(st, 2);
        r->frames = sqlite3_column_int(st, 3);
        r->note = col_text(st, 4);
        r->trials = sqlite3_column_int(st, 5);
    }
    sqlite3_finalize(st);
    *out_count = n;
    return rows;
}

void aos_labdb_free_trial_rows(AosTrialRow *rows, int count) {
    if (!rows) return;
    for (int i = 0; i < count; ++i) {
        free(rows[i].params);
        free(rows[i].keys);
        free(rows[i].outcome);
        free(rows[i].metrics);
        free(rows[i].started_at);
    }
    free(rows);
}

void aos_labdb_free_weight_rows(AosWeightRow *rows, int count) {
    if (!rows) return;
    for (int i = 0; i < count; ++i) free(rows[i].params);
    free(rows);
}

void aos_labdb_free_state_rows(AosStateRow *rows, int count) {
    if (!rows) return;
    for (int i = 0; i < count; ++i) {
        free(rows[i].name);
        free(rows[i].note);
        free(rows[i].created_at);
    }
    free(rows);
}

void aos_labdb_free_action_rows(AosActionRow *rows, int count) {
    if (!rows) return;
    for (int i = 0; i < count; ++i) {
        free(rows[i].name);
        free(rows[i].kind);
        free(rows[i].params);
        free(rows[i].eval);
        free(rows[i].script);
    }
    free(rows);
}

void aos_labdb_free_run_rows(AosRunRow *rows, int count) {
    if (!rows) return;
    for (int i = 0; i < count; ++i) {
        free(rows[i].started_at);
        free(rows[i].rom);
        free(rows[i].note);
    }
    free(rows);
}

bool aos_labdb_meta_set(AosLabDb *db, const char *key, const char *value) {
    sqlite3_stmt *st = NULL;
    const char *sql =
        "INSERT INTO meta(key,value) VALUES(?,?)"
        " ON CONFLICT(key) DO UPDATE SET value=excluded.value";
    if (sqlite3_prepare_v2(db->handle, sql, -1, &st, NULL) != SQLITE_OK)
        return false;
    bind_text(st, 1, key);
    bind_text(st, 2, value);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

char *aos_labdb_meta_get(AosLabDb *db, const char *key) {
    sqlite3_stmt *st = NULL;
    const char *sql = "SELECT value FROM meta WHERE key=?";
    if (sqlite3_prepare_v2(db->handle, sql, -1, &st, NULL) != SQLITE_OK)
        return NULL;
    bind_text(st, 1, key);
    char *value = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) value = col_text(st, 0);
    sqlite3_finalize(st);
    return value;
}

/* Purge dead weight: zero-reward fail trials, hopeless weight rows and
 * auto checkpoints no trial ever used. Returns per-table counts. */
int aos_labdb_purge(AosLabDb *db, int *out_trials, int *out_weights,
                    int *out_states) {
    static const char *sqls[] = {
        /* barren trials: hard fails that scored nothing */
        "DELETE FROM trials WHERE outcome='fail' AND reward<=0",
        /* action/param combos barely tried with nothing learned —
         * rows with real attempt counts or nonzero reward stay: a
         * negative reward_sum IS the failure memory that stops the
         * bandit from retrying proven losers */
        "DELETE FROM weights WHERE attempts>0 AND attempts<=2"
        " AND reward_sum<=0",
        /* auto checkpoints never referenced by any trial */
        "DELETE FROM states WHERE name LIKE 'auto_%'"
        " AND id NOT IN (SELECT DISTINCT state_id FROM trials"
        " WHERE state_id IS NOT NULL)",
        /* scout scene states: keep only the newest 100 that no trial
         * referenced — older ones are superseded intro footage */
        "DELETE FROM states WHERE name LIKE 'scout_c%'"
        " AND id NOT IN (SELECT DISTINCT state_id FROM trials"
        " WHERE state_id IS NOT NULL)"
        " AND id NOT IN (SELECT id FROM states WHERE name LIKE 'scout_c%'"
        " ORDER BY id DESC LIMIT 100)",
        /* duplicate scout states: same name saved by several workers —
         * keep only the newest copy of each unreferenced name */
        "DELETE FROM states WHERE name LIKE 'scout_c%'"
        " AND id NOT IN (SELECT DISTINCT state_id FROM trials"
        " WHERE state_id IS NOT NULL)"
        " AND id NOT IN (SELECT MAX(id) FROM states"
        " WHERE name LIKE 'scout_c%' GROUP BY name)",
        /* same dedup for validated play checkpoints: 4 scouts save the
         * same scout_play_NNN names — keep the newest copy each */
        "DELETE FROM states WHERE name LIKE 'scout_play%'"
        " AND id NOT IN (SELECT DISTINCT state_id FROM trials"
        " WHERE state_id IS NOT NULL)"
        " AND id NOT IN (SELECT MAX(id) FROM states"
        " WHERE name LIKE 'scout_play%' GROUP BY name)",
        /* same cap for any other unreferenced auto-ish states */
        "DELETE FROM states WHERE name LIKE 'auto_%'"
        " AND id NOT IN (SELECT DISTINCT state_id FROM trials"
        " WHERE state_id IS NOT NULL)"
        " AND id < (SELECT COALESCE(MAX(id),0) FROM states"
        " WHERE name LIKE 'auto_%') - 50",
        /* hard cap: keep only the 300 most valuable states overall.
         * Score = best trial reward on that state + bonuses for
         * validated play checkpoints and manual records, minus a
         * penalty for raw scene dumps. Everything else goes — this
         * is what makes the list actually shrink over time. */
        "DELETE FROM states WHERE id NOT IN ("
        " SELECT id FROM ("
        "  SELECT s.id,"
        "   COALESCE((SELECT MAX(t.reward) FROM trials t"
        "    WHERE t.state_id=s.id),0)"
        "   + CASE WHEN s.name LIKE 'scout_play%' THEN 2.0 ELSE 0 END"
        "   + CASE WHEN s.name LIKE 'scout_c%' OR s.name LIKE 'auto_%'"
        "     THEN 0.0 ELSE 1.0 END"
        "   - CASE WHEN s.name LIKE 'scout_c%' THEN 0.5 ELSE 0 END"
        "  FROM states s ORDER BY 2 DESC, s.id DESC LIMIT 300) k)",
        /* trials pointing at deleted states are dead weight too */
        "DELETE FROM trials WHERE state_id IS NOT NULL"
        " AND state_id NOT IN (SELECT id FROM states)",
        /* bound the trial log itself: newest 8000 rows is plenty of
         * learning history for the weight tables */
        "DELETE FROM trials WHERE id NOT IN ("
        " SELECT id FROM trials ORDER BY id DESC LIMIT 8000)",
    };
    int counts[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    for (int i = 0; i < 10; ++i) {
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(db->handle, sqls[i], -1, &st, NULL) !=
            SQLITE_OK)
            continue;
        if (sqlite3_step(st) == SQLITE_DONE)
            counts[i] = sqlite3_changes(db->handle);
        sqlite3_finalize(st);
    }
    if (out_trials) *out_trials = counts[0];
    if (out_weights) *out_weights = counts[1];
    if (out_states) *out_states = counts[2] + counts[3] + counts[4] +
                                   counts[5] + counts[6] + counts[7];
    return counts[0] + counts[1] + counts[2] + counts[3] + counts[4] +
           counts[5] + counts[6] + counts[7] + counts[8] + counts[9];
}

/* Best branch by observed value: the state whose trials produced the
 * highest reward, tie-broken by the most recent trial. Used by workers
 * for smart restarts — jump to whatever checkpoint proved most
 * interesting instead of replaying a stale base state. */
int64_t aos_labdb_best_state(AosLabDb *db, double *out_value) {
    static const char *sql =
        "SELECT state_id, MAX(reward) FROM trials"
        " WHERE state_id IS NOT NULL"
        " GROUP BY state_id ORDER BY MAX(reward) DESC,"
        " MAX(id) DESC LIMIT 1";
    sqlite3_stmt *st = NULL;
    int64_t id = -1;
    double val = 0.0;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &st, NULL) == SQLITE_OK) {
        if (sqlite3_step(st) == SQLITE_ROW) {
            id = sqlite3_column_int64(st, 0);
            val = sqlite3_column_double(st, 1);
        }
        sqlite3_finalize(st);
    }
    if (out_value) *out_value = val;
    return id;
}
