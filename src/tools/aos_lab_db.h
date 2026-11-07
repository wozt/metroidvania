/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_LAB_DB_H
#define AOS_LAB_DB_H

/* Local SQLite laboratory database for emulator-driven trials.
 *
 * Stores save-state blobs, scripted actions, trial outcomes and the
 * derived success weights used by the automatic learning loop. Fully
 * offline: a single file, WAL journal, no network.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AosLabDb AosLabDb;

typedef struct {
    int64_t id;
    char *name;
    char *note;
    int64_t parent_id;
    int blob_size;
    char *created_at;
} AosStateRow;

typedef struct {
    int64_t id;
    char *name;
    char *kind;    /* hold | tap | script | custom */
    char *params;  /* JSON-ish parameter string */
    char *eval;    /* JSON-ish success probe description */
    char *script;  /* optional raw "f:m,f:m" key schedule */
} AosActionRow;

typedef struct {
    int64_t id;
    int64_t run_id;
    int64_t state_id;
    int64_t action_id;
    char *params;
    char *keys;
    int frames;
    char *outcome; /* success | fail | partial | error */
    double reward;
    char *metrics;
    char *started_at;
    int duration_ms;
} AosTrialRow;

typedef struct {
    int64_t action_id;
    char *params;
    int attempts;
    int successes;
    double reward_sum;
    double success_rate; /* successes / attempts */
} AosWeightRow;

typedef struct {
    int64_t id;
    char *started_at;
    char *rom;
    int frames;
    char *note;
    int trials;
} AosRunRow;

AosLabDb *aos_labdb_open(const char *path, char *err, size_t err_size);
void aos_labdb_close(AosLabDb *db);
const char *aos_labdb_error(AosLabDb *db);

int64_t aos_labdb_add_state(AosLabDb *db, const char *name, const char *note,
                            int64_t parent_id, const void *blob,
                            size_t blob_size);
/* Fetch a state blob; returns malloc'd buffer, *out_size set. */
uint8_t *aos_labdb_state_blob(AosLabDb *db, int64_t id, size_t *out_size);
int64_t aos_labdb_update_action(AosLabDb *db, const char *name,
                              const char *kind, const char *params,
                              const char *eval, const char *script);
int64_t aos_labdb_add_action(AosLabDb *db, const char *name, const char *kind,
                             const char *params, const char *eval,
                             const char *script);

int64_t aos_labdb_begin_run(AosLabDb *db, const char *rom, const char *note);
void aos_labdb_end_run(AosLabDb *db, int64_t run_id, int frames, int trials);

int64_t aos_labdb_add_trial(AosLabDb *db, int64_t run_id, int64_t state_id,
                            int64_t action_id, const char *params,
                            const char *keys, int frames, const char *outcome,
                            double reward, const char *metrics,
                            int duration_ms);

/* Row queries: caller frees each row's strings and the array. */
AosTrialRow *aos_labdb_recent_trials(AosLabDb *db, int limit, int *out_count);
AosWeightRow *aos_labdb_weights(AosLabDb *db, int limit, int *out_count);
AosStateRow *aos_labdb_states(AosLabDb *db, int limit, int *out_count);
AosActionRow *aos_labdb_actions(AosLabDb *db, int *out_count);
AosRunRow *aos_labdb_runs(AosLabDb *db, int limit, int *out_count);
void aos_labdb_free_trial_rows(AosTrialRow *rows, int count);
void aos_labdb_free_weight_rows(AosWeightRow *rows, int count);
void aos_labdb_free_state_rows(AosStateRow *rows, int count);
void aos_labdb_free_action_rows(AosActionRow *rows, int count);
void aos_labdb_free_run_rows(AosRunRow *rows, int count);

/* Convenience metric read/write through meta table. */
bool aos_labdb_meta_set(AosLabDb *db, const char *key, const char *value);
char *aos_labdb_meta_get(AosLabDb *db, const char *key);

/* Delete barren rows: zero-reward fail trials, hopeless weight combos
 * and unreferenced auto checkpoints. Returns total rows removed. */
int aos_labdb_purge(AosLabDb *db, int *out_trials, int *out_weights,
                    int *out_states);

#ifdef __cplusplus
}
#endif

#endif /* AOS_LAB_DB_H */

/* Highest-value checkpoint: state_id whose trials reached the max
 * reward (most interesting branch so far). -1 when no trials. */
int64_t aos_labdb_best_state(AosLabDb *db, double *out_value);

/* States worth launching from: trial-referenced, scout_play_* and
 * manual records — mirrors the purge keep-rules. */
AosStateRow *aos_labdb_valuable_states(AosLabDb *db, int limit,
                                       int *out_count);
