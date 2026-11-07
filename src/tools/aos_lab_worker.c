/* SPDX-License-Identifier: GPL-3.0-only */
/* Automatic trial worker for the Aria emulator laboratory.
 *
 * Loads a save state from the lab database, applies a scripted action
 * (with parameter jitter), scores the outcome through memory probes and
 * records the trial plus its success weight. Runs fully offline in the
 * background; the GTK viewer watches the same database live.
 *
 * Usage:
 *   aos_lab_worker --db PATH --rom PATH [options]
 *     --state ID          state row to load (required for trials)
 *     --action ID         action row to run (repeatable; default: all
 *                         actions in the DB)
 *     --iterations N      trials per action (default 1)
 *     --frames N          override action frame count
 *     --explore P         probability of a random parameter draw
 *                         instead of the best-weighted one (default 0.15)
 *     --seed N            RNG seed (default time)
 *     --shots DIR         write DIR/trial_<id>.ppm per trial
 *     --record-state NAME run --frames from boot and store the resulting
 *                         state blob as a new state row (with --keys)
 *     --state-note TEXT   note for --record-state
 */
#define _GNU_SOURCE

#include "aos_mgba_session.h"
#include "aos_lab_db.h"
#include "aos_lab_vision.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

/* ---- tiny parameter/eval helpers ---------------------------------- */

static const struct { const char *name; uint32_t mask; } KEYS[] = {
    {"a", 1},       {"b", 2},     {"select", 4}, {"start", 8},
    {"right", 16},  {"left", 32}, {"up", 64},    {"down", 128},
    {"r", 256},     {"l", 512},   {NULL, 0},
};

static uint32_t key_mask(const char *name) {
    for (int i = 0; KEYS[i].name; ++i)
        if (!strcasecmp(KEYS[i].name, name)) return KEYS[i].mask;
    return 0;
}

/* params is a comma list key=value. Supported keys:
 *   key=NAME            button name (required for hold/tap)
 *   hold=N or hold=MIN-MAX   frames the button stays down
 *   at=N or at=MIN-MAX       frame the press starts
 *   jitter=N            +/- random added to hold/at (default 0)
 * Returns malloc'd display string "k=v,k=v" of the drawn values. */
typedef struct {
    char key_name[16];
    uint32_t mask;
    int hold, hold_min, hold_max;
    int at, at_min, at_max;
    int jitter;
} ActionParams;

static void params_init(ActionParams *p) {
    memset(p, 0, sizeof *p);
    p->hold = p->hold_min = p->hold_max = 1;
    p->at = p->at_min = p->at_max = 0;
}

static int parse_range(const char *value, int *lo, int *hi) {
    int a = 0, b = 0;
    if (sscanf(value, "%d-%d", &a, &b) == 2) {
        *lo = a < b ? a : b;
        *hi = a < b ? b : a;
        return 1;
    }
    if (sscanf(value, "%d", &a) == 1) {
        *lo = *hi = a;
        return 1;
    }
    return 0;
}

static void params_parse(const char *params, ActionParams *p) {
    params_init(p);
    if (!params) return;
    char *copy = strdup(params);
    char *save = NULL;
    for (char *tok = strtok_r(copy, ",", &save); tok;
         tok = strtok_r(NULL, ",", &save)) {
        char *eq = strchr(tok, '=');
        if (!eq) continue;
        *eq = 0;
        const char *k = tok, *v = eq + 1;
        if (!strcasecmp(k, "key")) {
            snprintf(p->key_name, sizeof p->key_name, "%s", v);
            p->mask = key_mask(v);
        } else if (!strcasecmp(k, "hold")) {
            parse_range(v, &p->hold_min, &p->hold_max);
        } else if (!strcasecmp(k, "at")) {
            parse_range(v, &p->at_min, &p->at_max);
        } else if (!strcasecmp(k, "jitter")) {
            p->jitter = atoi(v);
        }
    }
    free(copy);
}

static int rand_range(int lo, int hi) {
    if (hi <= lo) return lo;
    return lo + rand() % (hi - lo + 1);
}

/* Draw concrete values; fills display string (malloc'd). */
static char *params_draw(const ActionParams *base, char *display,
                         size_t display_size) {
    int hold = rand_range(base->hold_min, base->hold_max);
    int at = rand_range(base->at_min, base->at_max);
    if (base->jitter > 0) {
        hold += rand() % (2 * base->jitter + 1) - base->jitter;
        at += rand() % (2 * base->jitter + 1) - base->jitter;
        if (hold < 1) hold = 1;
        if (at < 0) at = 0;
    }
    snprintf(display, display_size, "key=%s,at=%d,hold=%d", base->key_name,
             at, hold);
    /* stash drawn values where the caller can reach them via static */
    ((ActionParams *)base)->hold = hold; /* draw is one-shot per trial */
    ((ActionParams *)base)->at = at;
    return display;
}

/* Build the key schedule for a hold/tap action. */
static AosKeyEvent *build_schedule(const ActionParams *p, int *count,
                                   int extra_frames) {
    int cap = 4;
    AosKeyEvent *ev = malloc(cap * sizeof *ev);
    int n = 0;
    ev[n].frame = p->at;
    ev[n].mask = p->mask;
    ++n;
    ev[n].frame = p->at + p->hold;
    ev[n].mask = 0;
    ++n;
    /* safety release well past the end */
    if (n < cap) {
        ev[n].frame = extra_frames + 60;
        ev[n].mask = 0;
        ++n;
    }
    *count = n;
    return ev;
}

/* ---- eval probe ---------------------------------------------------- */

typedef struct {
    uint32_t address;
    uint32_t size;
    char expect[16]; /* increase|decrease|changed|nonzero|zero|eq|ne */
    int32_t threshold;
    int has_threshold;
} EvalProbe;

static int eval_parse(const char *eval, EvalProbe *probe) {
    memset(probe, 0, sizeof *probe);
    if (!eval) return 0;
    char *copy = strdup(eval);
    char *save = NULL;
    int found = 0;
    for (char *tok = strtok_r(copy, ",", &save); tok;
         tok = strtok_r(NULL, ",", &save)) {
        char *eq = strchr(tok, '=');
        if (!eq) continue;
        *eq = 0;
        const char *k = tok, *v = eq + 1;
        if (!strcasecmp(k, "probe")) {
            unsigned a = 0, s = 0;
            if (sscanf(v, "%x:%x", &a, &s) >= 1) {
                probe->address = (uint32_t)a;
                probe->size = s ? s : 2;
                found = 1;
            }
        } else if (!strcasecmp(k, "expect")) {
            snprintf(probe->expect, sizeof probe->expect, "%s", v);
        } else if (!strcasecmp(k, "threshold")) {
            probe->threshold = atoi(v);
            probe->has_threshold = 1;
        }
    }
    free(copy);
    return found;
}

static int32_t peek_i32(const AosPeekResult *r) {
    if (!r->data || r->size < 4) return 0;
    uint32_t v;
    memcpy(&v, r->data, 4);
    return (int32_t)v;
}

/* Score outcome: returns outcome string + reward via pointers. */
static void eval_score(const EvalProbe *probe, const AosPeekResult *before,
                       const AosPeekResult *after, char *outcome,
                       size_t outcome_size, double *reward,
                       char *metrics, size_t metrics_size) {
    if (!probe->address || !after->data) {
        snprintf(outcome, outcome_size, "error");
        *reward = 0.0;
        snprintf(metrics, metrics_size, "{\"error\":\"no probe data\"}");
        return;
    }
    int32_t b = before && before->data ? peek_i32(before) : 0;
    int32_t a = peek_i32(after);
    int32_t delta = a - b;
    int ok = 0, partial = 0;
    const char *e = probe->expect;
    if (!strcasecmp(e, "increase")) {
        ok = delta > (probe->has_threshold ? probe->threshold : 0);
        partial = delta > 0;
    } else if (!strcasecmp(e, "decrease")) {
        ok = delta < -(probe->has_threshold ? probe->threshold : 0);
        partial = delta < 0;
    } else if (!strcasecmp(e, "changed")) {
        ok = delta != 0;
    } else if (!strcasecmp(e, "nonzero")) {
        ok = a != 0;
    } else if (!strcasecmp(e, "zero")) {
        ok = a == 0;
    } else if (!strncasecmp(e, "eq", 2)) {
        ok = a == probe->threshold;
    } else if (!strncasecmp(e, "ne", 2)) {
        ok = a != probe->threshold;
    }
    if (ok) {
        snprintf(outcome, outcome_size, "success");
        *reward = 1.0;
    } else if (partial) {
        snprintf(outcome, outcome_size, "partial");
        *reward = 0.5;
    } else {
        snprintf(outcome, outcome_size, "fail");
        *reward = 0.0;
    }
    snprintf(metrics, metrics_size,
             "{\"before\":%d,\"after\":%d,\"delta\":%d,\"expect\":\"%s\"}",
             b, a, delta, e);
}

/* ---- weight-guided action choice ---------------------------------- */

typedef struct {
    AosActionRow action;
    AosWeightRow *weights;
    int weight_count;
} ActionChoice;

/* Pick the params string of the best weight row for this action; falls
 * back to the action's own params when no weights exist yet. Result is
 * copied into `out` (NUL-terminated). Returns true when a weight row
 * won. */
static bool best_params(AosLabDb *db, int64_t action_id,
                        const char *fallback, char *out, size_t out_size) {
    int count = 0;
    AosWeightRow *rows = aos_labdb_weights(db, 100000, &count);
    const char *best = NULL;
    double best_rate = -1.0;
    int best_attempts = -1;
    for (int i = 0; i < count; ++i) {
        if (rows[i].action_id != action_id) continue;
        if (rows[i].success_rate > best_rate ||
            (rows[i].success_rate == best_rate &&
             rows[i].attempts > best_attempts)) {
            best_rate = rows[i].success_rate;
            best_attempts = rows[i].attempts;
            best = rows[i].params;
        }
    }
    /* copy before freeing: best points into rows */
    char picked[512];
    picked[0] = 0;
    if (best) snprintf(picked, sizeof picked, "%s", best);
    aos_labdb_free_weight_rows(rows, count);
    if (picked[0]) {
        snprintf(out, out_size, "%s", picked);
        return true;
    }
    snprintf(out, out_size, "%s", fallback ? fallback : "");
    return false;
}

/* ---- live framebuffer mirror (shm) ---------------------------------
 * Each worker publishes its emulated screen to /dev/shm/aoslab_fb_<id>
 * so the GTK viewer can show up to eight live emulator windows. Layout:
 *   u32 magic 'AOSF' | u64 seq | u32 w | u32 h | u32 pad[3]  (32 bytes)
 *   then W*H u32 pixels (0xRRGGBB). The emulation thread bumps seq on
 *   every copy; viewers poll seq to detect new frames. */
#define AOS_FB_MAGIC 0x414F5346u
#define AOS_FB_HDR 32
#define AOS_FB_W 240
#define AOS_FB_H 160
#define AOS_FB_SIZE (AOS_FB_HDR + AOS_FB_W * AOS_FB_H * 4)

typedef struct {
    int fd;
    void *map;
    uint64_t *seq;
    uint32_t *pixels;
} AosFbMirror;

static int fb_attach(AosFbMirror *fb, int worker_id) {
    memset(fb, 0, sizeof *fb);
    char name[64];
    snprintf(name, sizeof name, "/aoslab_fb_%d", worker_id & 7);
    fb->fd = shm_open(name, O_RDWR | O_CREAT, 0600);
    if (fb->fd < 0) return -1;
    if (ftruncate(fb->fd, AOS_FB_SIZE) != 0) { close(fb->fd); return -1; }
    fb->map = mmap(NULL, AOS_FB_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
                   fb->fd, 0);
    if (fb->map == MAP_FAILED) { close(fb->fd); return -1; }
    uint32_t *hdr = fb->map;
    hdr[0] = AOS_FB_MAGIC;      /* offset 0  */
    hdr[1] = 0;                 /* offset 4  */
    fb->seq = (uint64_t *)((char *)fb->map + 8);   /* offset 8  */
    *fb->seq = 0;
    hdr[4] = AOS_FB_W;          /* offset 16 */
    hdr[5] = AOS_FB_H;          /* offset 20 */
    *fb->seq = 0;
    fb->pixels = (uint32_t *)((char *)fb->map + AOS_FB_HDR);
    return 0;
}

static void fb_detach(AosFbMirror *fb) {
    if (fb->map && fb->map != MAP_FAILED) munmap(fb->map, AOS_FB_SIZE);
    if (fb->fd >= 0) close(fb->fd);
    memset(fb, 0, sizeof *fb);
}

/* ---- modes --------------------------------------------------------- */

static int record_state(AosMgbaSession *session, AosLabDb *db,
                        const char *name, const char *note,
                        const char *keys_spec, int frames) {
    AosRunSpec spec;
    memset(&spec, 0, sizeof spec);
    spec.frames = frames;
    int kc = 0;
    AosKeyEvent *keys = aos_mgba_parse_keys(keys_spec, &kc);
    spec.keys = keys;
    spec.key_count = kc;
    AosRunResult res;
    aos_mgba_run(session, &spec, &res);
    aos_mgba_free_result(&res);
    aos_mgba_free_keys(keys);

    size_t blob_size = 0;
    uint8_t *blob = aos_mgba_save_state_blob(session, &blob_size);
    if (!blob) {
        fprintf(stderr, "state capture failed\n");
        return 1;
    }
    int64_t id = aos_labdb_add_state(db, name, note, 0, blob, blob_size);
    free(blob);
    if (id < 0) {
        fprintf(stderr, "state store failed\n");
        return 1;
    }
    printf("state %lld stored (%zu bytes)\n", (long long)id, blob_size);
    return 0;
}

int main(int argc, char **argv) {
    const char *db_path = NULL, *rom = NULL, *shots_dir = NULL,
               *record_name = NULL, *state_note = NULL, *keys_spec = NULL;
    int64_t state_id = -1;
    int iterations = 1, frames_override = -1, seed = -1;
    double explore = 0.15;
    int action_ids[64];
    int action_count = 0;
    const char *define_action = NULL;
    int worker_id = 0;

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        const char *value = i + 1 < argc ? argv[i + 1] : NULL;
        if (!strcmp(arg, "--db") && value) { db_path = value; ++i; }
        else if (!strcmp(arg, "--rom") && value) { rom = value; ++i; }
        else if (!strcmp(arg, "--state") && value) { state_id = atoll(value); ++i; }
        else if (!strcmp(arg, "--action") && value && action_count < 64) {
            action_ids[action_count++] = atoi(value); ++i;
        }
        else if (!strcmp(arg, "--iterations") && value) { iterations = atoi(value); ++i; }
        else if (!strcmp(arg, "--frames") && value) { frames_override = atoi(value); ++i; }
        else if (!strcmp(arg, "--explore") && value) { explore = atof(value); ++i; }
        else if (!strcmp(arg, "--seed") && value) { seed = atoi(value); ++i; }
        else if (!strcmp(arg, "--shots") && value) { shots_dir = value; ++i; }
        else if (!strcmp(arg, "--record-state") && value) { record_name = value; ++i; }
        else if (!strcmp(arg, "--state-note") && value) { state_note = value; ++i; }
        else if (!strcmp(arg, "--keys") && value) { keys_spec = value; ++i; }
        else if (!strcmp(arg, "--define-action") && value) {
            define_action = value; ++i;
        }
        else if (!strcmp(arg, "--worker-id") && value) {
            worker_id = atoi(value); ++i;
        }
        else {
            fprintf(stderr, "unknown argument %s\n", arg);
            return 2;
        }
    }
    if (!db_path || (!rom && !define_action)) {
        fprintf(stderr, "usage: aos_lab_worker --db PATH --rom PATH ...\n");
        return 2;
    }
    if (seed < 0) seed = (int)(time(NULL) ^ (long)getpid());
    srand(seed);

    char err[512];
    AosLabDb *db = aos_labdb_open(db_path, err, sizeof err);
    if (!db) {
        fprintf(stderr, "db open failed: %s\n", err);
        return 1;
    }
    if (define_action) {
        /* "name|kind|params|eval|script" — inserts and exits */
        char *copy = strdup(define_action);
        char *fields[5] = {0};
        int nf = 0;
        char *p = copy;
        for (char *c = copy; ; ++c) {
            if (*c == '|' || *c == 0) {
                char save = *c;
                *c = 0;
                if (nf < 5) fields[nf++] = p;
                p = c + 1;
                if (!save) break;
            }
        }
        while (nf < 5) fields[nf++] = (char *)"";
        int64_t id = aos_labdb_add_action(db, fields[0], fields[1],
                                          fields[2], fields[3], fields[4]);
        printf("action %lld defined\n", (long long)id);
        free(copy);
        aos_labdb_close(db);
        return id < 0 ? 1 : 0;
    }

    char emu_err[256];
    AosMgbaSession *session = aos_mgba_open(rom, emu_err, sizeof emu_err);
    if (!session) {
        fprintf(stderr, "emu open failed: %s\n", emu_err);
        aos_labdb_close(db);
        return 1;
    }

    if (shots_dir) mkdir(shots_dir, 0755);

    if (record_name) {
        int rc = record_state(session, db, record_name,
                              state_note ? state_note : "", keys_spec,
                              frames_override > 0 ? frames_override : 120);
        aos_mgba_close(session);
        aos_labdb_close(db);
        return rc;
    }

    if (state_id < 0) {
        fprintf(stderr, "--state ID is required for trials\n");
        aos_mgba_close(session);
        aos_labdb_close(db);
        return 2;
    }

    /* Load the state blob once; each trial reloads it. */
    size_t blob_size = 0;
    uint8_t *state_blob = aos_labdb_state_blob(db, state_id, &blob_size);
    if (!state_blob) {
        fprintf(stderr, "state %lld not found\n", (long long)state_id);
        aos_mgba_close(session);
        aos_labdb_close(db);
        return 1;
    }

    /* publish the live framebuffer for the GTK viewer */
    AosFbMirror fb;
    int fb_ok = fb_attach(&fb, worker_id) == 0;

    /* Resolve actions: explicit ids or all rows. */
    int acount = 0;
    AosActionRow *all = aos_labdb_actions(db, &acount);
    AosActionRow chosen[64];
    int chosen_count = 0;
    if (action_count == 0) {
        for (int i = 0; i < acount && chosen_count < 64; ++i)
            chosen[chosen_count++] = all[i];
    } else {
        for (int i = 0; i < action_count && chosen_count < 64; ++i) {
            for (int j = 0; j < acount; ++j)
                if (all[j].id == action_ids[i]) {
                    chosen[chosen_count++] = all[j];
                    break;
                }
        }
    }
    if (chosen_count == 0) {
        fprintf(stderr, "no matching actions in db\n");
        free(state_blob);
        aos_labdb_free_action_rows(all, acount);
        aos_mgba_close(session);
        aos_labdb_close(db);
        return 1;
    }

    char phase_key[64], status_key[64], run_key[64], ctrl_key[64];
    snprintf(phase_key, sizeof phase_key, "worker.%d.phase", worker_id);
    snprintf(status_key, sizeof status_key, "worker.%d.status", worker_id);
    snprintf(run_key, sizeof run_key, "worker.%d.run_id", worker_id);
    snprintf(ctrl_key, sizeof ctrl_key, "lab.control.%d", worker_id);
    aos_labdb_meta_set(db, ctrl_key, "run");

    int64_t run_id = aos_labdb_begin_run(db, rom, "worker batch");
    aos_labdb_meta_set(db, phase_key, "running");
    char run_buf[32];
    snprintf(run_buf, sizeof run_buf, "%lld", (long long)run_id);
    aos_labdb_meta_set(db, run_key, run_buf);
    int total_frames = 0, trials_done = 0;
    int stop_requested = 0;
    struct timeval t0, t1;

    for (int ci = 0; ci < chosen_count && !stop_requested; ++ci) {
        AosActionRow *action = &chosen[ci];
        for (int it = 0; it < iterations && !stop_requested; ++it) {
            /* cooperative control through the meta table: the GTK
             * viewer writes lab.control = pause|resume|stop */
            char *control = aos_labdb_meta_get(db, ctrl_key);
            if (control) {
                if (!strcmp(control, "stop")) stop_requested = 1;
                while (!strcmp(control, "pause")) {
                    aos_labdb_meta_set(db, phase_key, "paused");
                    free(control);
                    usleep(400 * 1000);
                    control = aos_labdb_meta_get(db, ctrl_key);
                    if (!control) break;
                }
                if (control && !strcmp(control, "resume")) {
                    aos_labdb_meta_set(db, ctrl_key, "run");
                }
                if (control && !strcmp(control, "stop")) stop_requested = 1;
                free(control);
            }
            if (stop_requested) break;
            aos_labdb_meta_set(db, phase_key, "running");
            /* choose params: exploit best weight or explore randomly */
            char params_buf[512];
            int exploring = ((double)rand() / RAND_MAX) < explore;
            if (!exploring)
                best_params(db, action->id, action->params, params_buf,
                            sizeof params_buf);
            else
                snprintf(params_buf, sizeof params_buf, "%s",
                         action->params ? action->params : "");
            const char *params_src = params_buf;
            if (getenv("AOS_LAB_DEBUG"))
                fprintf(stderr, "  [dbg] action=%lld explore=%d src='%s'\n",
                        (long long)action->id, exploring, params_src);
            ActionParams base;
            params_parse(params_src, &base);
            char drawn[128];
            params_draw(&base, drawn, sizeof drawn);

            EvalProbe probe;
            int has_probe = eval_parse(action->eval, &probe);
            int frames = frames_override > 0 ? frames_override : 120;

            /* key schedule */
            AosKeyEvent *schedule = NULL;
            int schedule_count = 0;
            char keys_label[256] = "";
            if (action->script) {
                schedule = aos_mgba_parse_keys(action->script,
                                               &schedule_count);
                snprintf(keys_label, sizeof keys_label, "%s", action->script);
            } else if (base.mask) {
                schedule = build_schedule(&base, &schedule_count, frames);
                snprintf(keys_label, sizeof keys_label,
                         "%d:%x,%d:0", base.at, base.mask,
                         base.at + base.hold);
            }

            /* baseline probe at frame 0, final at last frame */
            AosPeekRequest peeks[2];
            int peek_count = 0;
            if (has_probe) {
                peeks[0].frame = 0;
                peeks[0].address = probe.address;
                peeks[0].size = probe.size;
                peeks[1].frame = frames - 1;
                peeks[1].address = probe.address;
                peeks[1].size = probe.size;
                peek_count = 2;
            }

            AosRunSpec spec;
            memset(&spec, 0, sizeof spec);
            spec.frames = frames;
            spec.keys = schedule;
            spec.key_count = schedule_count;
            spec.peeks = peeks;
            spec.peek_count = peek_count;
            /* live speed slider: 0 = auto (unthrottled), N = N×60 fps */
            {
                char sk[64];
                snprintf(sk, sizeof sk, "worker.%d.speed", worker_id);
                char *sv = aos_labdb_meta_get(db, sk);
                int speed = sv ? atoi(sv) : 0;
                free(sv);
                spec.fps_cap = speed > 0 ? 60.0 * speed : 0.0;
            }
            if (fb_ok) {
                spec.fb_mirror = fb.pixels;
                spec.fb_seq = fb.seq;
                spec.fb_interval = 2;
            }
            char shot_path[1100];
            if (shots_dir) {
                /* capture only after we know the trial id: use a temp
                 * prefix then rename is overkill; skip shots for now. */
                (void)shot_path;
            }

            gettimeofday(&t0, NULL);
            /* reload state blob */
            if (!aos_mgba_load_state_blob(session, state_blob, blob_size)) {
                fprintf(stderr, "state reload failed\n");
            }
            AosRunResult res;
            aos_mgba_run(session, &spec, &res);
            gettimeofday(&t1, NULL);
            int duration_ms = (int)((t1.tv_sec - t0.tv_sec) * 1000 +
                                    (t1.tv_usec - t0.tv_usec) / 1000);

            char outcome[32] = "fail";
            double reward = 0.0;
            char metrics[512] = "{}";
            if (has_probe && res.peek_count == 2) {
                eval_score(&probe, &res.peeks[0], &res.peeks[1], outcome,
                           sizeof outcome, &reward, metrics,
                           sizeof metrics);
            } else if (!has_probe) {
                /* no probe: outcome unknown -> partial credit for a
                 * completed run so weights still accumulate */
                snprintf(outcome, sizeof outcome, "partial");
                reward = 0.25;
                snprintf(metrics, sizeof metrics,
                         "{\"note\":\"no eval probe\"}");
            } else {
                snprintf(metrics, sizeof metrics,
                         "{\"note\":\"probe data missing\"}");
            }

            /* visual signature of the final frame -> metrics JSON */
            {
                const uint32_t *fb = aos_mgba_framebuffer(session);
                AosSceneFeat feat;
                aos_vision_analyze(fb, AOS_MGBA_WIDTH, AOS_MGBA_HEIGHT,
                                   &feat);
                aos_vision_metrics_append(metrics, sizeof metrics, &feat);
            }

            int64_t trial_id = aos_labdb_add_trial(
                db, run_id, state_id, action->id, drawn, keys_label,
                frames, outcome, reward, metrics, duration_ms);
            ++trials_done;
            total_frames += frames;
            char status[256];
            snprintf(status, sizeof status,
                     "trial %lld action=%lld %s -> %s",
                     (long long)trial_id, (long long)action->id, drawn,
                     outcome);
            aos_labdb_meta_set(db, status_key, status);
            printf("trial %lld action=%lld params=%s -> %s (r=%.2f, %dms)\n",
                   (long long)trial_id, (long long)action->id, drawn,
                   outcome, reward, duration_ms);
            fflush(stdout);

            aos_mgba_free_result(&res);
            aos_mgba_free_keys(schedule);
        }
    }

    aos_labdb_end_run(db, run_id, total_frames, trials_done);
    aos_labdb_meta_set(db, phase_key, "idle");
    {
        char done[64];
        snprintf(done, sizeof done, "%d trials, %s", trials_done,
                 stop_requested ? "stopped" : "done");
        aos_labdb_meta_set(db, status_key, done);
    }
    char *rate = NULL;
    {
        /* report top weight for visibility */
        int wc = 0;
        AosWeightRow *w = aos_labdb_weights(db, 3, &wc);
        for (int i = 0; i < wc; ++i) {
            printf("weight action=%lld params=%s rate=%.0f%% (%d/%d)\n",
                   (long long)w[i].action_id,
                   w[i].params ? w[i].params : "",
                   w[i].success_rate * 100.0, w[i].successes,
                   w[i].attempts);
        }
        aos_labdb_free_weight_rows(w, wc);
    }
    (void)rate;

    free(state_blob);
    aos_labdb_free_action_rows(all, acount);
    fb_detach(&fb);
    aos_mgba_close(session);
    aos_labdb_close(db);
    return 0;
}
