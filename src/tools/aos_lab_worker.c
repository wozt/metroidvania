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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
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
/* Fingerprint a params string for the lethal-combo memory. */
static unsigned params_fprint(const char *s) {
    unsigned h = 2166136261u;
    if (s) for (; *s; ++s) { h ^= (unsigned char)*s; h *= 16777619u; }
    return h;
}

/* Lethal-param memory: combos that ended in death or a damaged exit
 * get counted in meta lab.lethal.<action>.<fp>; proven killers are
 * avoided by the exploit path (exploration may still retry them). */
static int lethal_count(AosLabDb *db, int64_t action_id,
                        const char *params) {
    char k[96];
    snprintf(k, sizeof k, "lab.lethal.%lld.%08x",
             (long long)action_id, params_fprint(params));
    char *v = aos_labdb_meta_get(db, k);
    int n = v ? atoi(v) : 0;
    free(v);
    return n;
}

static void lethal_mark(AosLabDb *db, int64_t action_id,
                        const char *params) {
    char k[96], b[16];
    snprintf(k, sizeof k, "lab.lethal.%lld.%08x",
             (long long)action_id, params_fprint(params));
    char *v = aos_labdb_meta_get(db, k);
    snprintf(b, sizeof b, "%d", (v ? atoi(v) : 0) + 1);
    aos_labdb_meta_set(db, k, b);
    free(v);
}

/* ---- branch Elo -----------------------------------------------------
 * Branch strength is a relative rating, not an absolute score: every
 * trial is a match between its branch and the previous trial's
 * branch. Winner gains, loser drops (K=32, start 1500). The fleet
 * champion lives in meta lab.beststate="id:elo" — smart hops and
 * waypoint picks follow the Elo leader, so a consistently good
 * branch beats a one-off lucky reward. */
static double elo_get(AosLabDb *db, int64_t id) {
    char k[64];
    snprintf(k, sizeof k, "lab.elo.%lld", (long long)id);
    char *v = aos_labdb_meta_get(db, k);
    double r = v ? atof(v) : 1500.0;
    free(v);
    return r;
}

static void elo_set(AosLabDb *db, int64_t id, double rating) {
    char k[64], b[32];
    snprintf(k, sizeof k, "lab.elo.%lld", (long long)id);
    snprintf(b, sizeof b, "%.1f", rating);
    aos_labdb_meta_set(db, k, b);
}

static void elo_match(AosLabDb *db, int64_t cur, double rcur) {
    char *lm = aos_labdb_meta_get(db, "lab.lastmatch");
    long long oid = -1;
    double orw = 0.0;
    if (lm) sscanf(lm, "%lld:%lf", &oid, &orw);
    free(lm);
    if (oid > 0 && oid != cur) {
        double ra = elo_get(db, cur), rb = elo_get(db, oid);
        double ea = 1.0 / (1.0 + pow(10.0, (rb - ra) / 400.0));
        double sa = rcur > orw ? 1.0 : (rcur < orw ? 0.0 : 0.5);
        ra += 32.0 * (sa - ea);
        rb += 32.0 * ((1.0 - sa) - (1.0 - ea));
        elo_set(db, cur, ra);
        elo_set(db, oid, rb);
        /* fleet champion: only the two players' ratings moved, so the
         * leader can only change if one of them overtook it */
        char *bv = aos_labdb_meta_get(db, "lab.beststate");
        long long bid = -1;
        double be = 0.0;
        if (bv) sscanf(bv, "%lld:%lf", &bid, &be);
        free(bv);
        int64_t win = ra >= rb ? cur : oid;
        double we = ra >= rb ? ra : rb;
        if (we > be || bid < 0) {
            char nb[64];
            snprintf(nb, sizeof nb, "%lld:%.1f", (long long)win, we);
            aos_labdb_meta_set(db, "lab.beststate", nb);
        }
    }
    char mb[64];
    snprintf(mb, sizeof mb, "%lld:%.4f", (long long)cur, rcur);
    aos_labdb_meta_set(db, "lab.lastmatch", mb);
}

static bool best_params(AosLabDb *db, int64_t action_id,
                        const char *fallback, char *out, size_t out_size) {
    int count = 0;
    AosWeightRow *rows = aos_labdb_weights(db, 100000, &count);
    const char *best = NULL;
    /* rank by mean reward — success_rate alone keeps combos that
     * "succeed" while scoring negative exploration bonuses. Keep the
     * top candidates first; the lethal-memory lookup is a SQL query so
     * it only runs on the finalists, not on every weight row. */
    enum { TOPK = 6 };
    const char *cand[TOPK] = {0};
    double cand_score[TOPK];
    int cand_att[TOPK];
    for (int i = 0; i < TOPK; ++i) { cand_score[i] = -1e9; cand_att[i] = -1; }
    for (int i = 0; i < count; ++i) {
        if (rows[i].action_id != action_id || rows[i].attempts <= 0)
            continue;
        double score = rows[i].reward_sum / rows[i].attempts;
        int slot = TOPK - 1;
        if (score > cand_score[slot] ||
            (score == cand_score[slot] && rows[i].attempts > cand_att[slot])) {
            while (slot > 0 &&
                   (score > cand_score[slot - 1] ||
                    (score == cand_score[slot - 1] &&
                     rows[i].attempts > cand_att[slot - 1]))) --slot;
            for (int j = TOPK - 1; j > slot; --j) {
                cand[j] = cand[j - 1];
                cand_score[j] = cand_score[j - 1];
                cand_att[j] = cand_att[j - 1];
            }
            cand[slot] = rows[i].params;
            cand_score[slot] = score;
            cand_att[slot] = rows[i].attempts;
        }
    }
    /* lethal memory: each proven kill on a combo subtracts a full
     * point so it sinks below any neutral rival */
    double best_score = -1e9;
    int best_attempts = -1;
    for (int i = 0; i < TOPK; ++i) {
        if (!cand[i]) continue;
        double score = cand_score[i] -
                       1.0 * lethal_count(db, action_id, cand[i]);
        if (score > best_score ||
            (score == best_score && cand_att[i] > best_attempts)) {
            best_score = score;
            best_attempts = cand_att[i];
            best = cand[i];
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
 *   then W*H u32 pixels (0xAABBGGRR). The emulation thread bumps seq on
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

/* ---- reward layer ---------------------------------------------------
 * Every trial peeks a fixed set of WRAM regions before and after the
 * run; compute_bonus turns the diffs into a score for exploration
 * events the action probe cannot see: new scenes/rooms, items, souls,
 * level-ups, clean traversals, damage and death. */
enum {
    RK_MODE = 0,   /* 0x02000010:4  gameMode..input */
    RK_AREA = 1,   /* 0x02000090:16 song,pad,area,room,mapXY */
    RK_POS  = 2,   /* 0x02000524:8  player posX,posY (16.16) */
    RK_VIT  = 3,   /* 0x0201327A:8  hp,mp,maxhp,maxmp */
    RK_XP   = 4,   /* 0x0201328C:8  exp,gold */
    RK_LVL  = 5,   /* 0x02013279:4  level + hp head */
    RK_ITEM = 6,   /* 0x02013294:136 inventories */
    RK_SOUL = 7,   /* 0x0201331C:122 soul inventories */
    RK_ENT  = 8,   /* 0x02000400:8192 entity table — enemy AI, HP, AI ticks */
    RK_VRAM = 9,   /* 0x06000000:98304 tilemaps — text/dialogue activity */
    RK_COUNT = 10,
    RK_ACTION = 8 /* action probe occupies last two slots */
};

static const struct { uint32_t addr, size; } REWARD_PEEKS[RK_COUNT] = {
    {0x02000010, 4}, {0x02000090, 16}, {0x02000524, 8},
    {0x0201327A, 8}, {0x0201328C, 8}, {0x02013279, 4},
    {0x02013294, 136}, {0x0201331C, 122}, {0x02000400, 8192},
    {0x06000000, 0x18000},
};

static int16_t rd16(const AosPeekResult *r, int off) {
    if (!r->data || r->size < (uint32_t)(off + 2)) return 0;
    uint16_t v;
    memcpy(&v, r->data + off, 2);
    return (int16_t)v;
}
static uint32_t rd32(const AosPeekResult *r, int off) {
    if (!r->data || r->size < (uint32_t)(off + 4)) return 0;
    uint32_t v;
    memcpy(&v, r->data + off, 4);
    return v;
}

/* Global explored-rooms registry kept in meta "lab.rooms" as a
 * comma list of "area:room" pairs shared by every worker, so the whole
 * fleet knows which rooms the campaign has already discovered. */
static int room_seen(AosLabDb *db, int area, int room) {
    char key[32];
    snprintf(key, sizeof key, "%d:%d", area, room);
    char *list = aos_labdb_meta_get(db, "lab.rooms");
    int found = 0;
    if (list) {
        found = strstr(list, key) != NULL;
        /* avoid prefix collisions like 1:2 matching 1:23 */
        if (found) {
            char *p = strstr(list, key);
            char next = p[strlen(key)];
            if (next && (isdigit(next) || next == ':')) found = 0;
        }
    }
    free(list);
    return found;
}

static void room_mark(AosLabDb *db, int area, int room) {
    char key[32];
    snprintf(key, sizeof key, "%d:%d", area, room);
    char *list = aos_labdb_meta_get(db, "lab.rooms");
    size_t need = (list ? strlen(list) : 0) + strlen(key) + 2;
    char *nl = malloc(need);
    snprintf(nl, need, "%s%s%s", list ? list : "", list ? "," : "", key);
    aos_labdb_meta_set(db, "lab.rooms", nl);
    free(nl);
    free(list);
}

static int room_count(AosLabDb *db) {
    char *list = aos_labdb_meta_get(db, "lab.rooms");
    int n = 0;
    if (list) for (char *p = list; *p; ++p) if (*p == ',') ++n;
    free(list);
    return list ? n + 1 : 0;
}

/* ---- adaptive traversal --------------------------------------------
 * kind="traverse" actions play the run as ~60-frame segments instead
 * of one fixed key schedule: hold the direction, and whenever a
 * segment shows almost no screen motion the next segment reacts —
 * jump first, then attack, then UP (doors), then back to cruising.
 * This encodes the actual room-crossing rules: keep walking, hop
 * obstacles, swing through enemies, open doors. Reward peeks are
 * collected on every segment so the merged result feeds compute_bonus
 * exactly like a normal trial. */
static void run_traverse(AosMgbaSession *session, int frames,
                         uint32_t dir_mask, double fps_cap,
                         uint32_t *fb_pix, AosRunResult *out) {
    memset(out, 0, sizeof *out);
    if (dir_mask == 0) dir_mask = 16;
    const int SEG = 60;
    AosRunResult *first = NULL, *last = NULL;
    int nseg = 0, stuck = 0, hp_prev = -1;
    for (int off = 0; off < frames; off += SEG) {
        int f = frames - off < SEG ? frames - off : SEG;
        /* reaction ladder: 0 cruise, 1 jump, 2 attack, 3 door-up */
        AosKeyEvent keys[6];
        int kn = 0;
        uint32_t b = 2, a = 1, up = 64;
        if (stuck == 1) {           /* hop the obstacle */
            keys[kn++] = (AosKeyEvent){0, dir_mask | b};
            keys[kn++] = (AosKeyEvent){18 < f ? 18 : f, dir_mask};
            keys[kn++] = (AosKeyEvent){f, 0};
        } else if (stuck == 2) {    /* swing through whatever blocks */
            keys[kn++] = (AosKeyEvent){0, dir_mask | a};
            keys[kn++] = (AosKeyEvent){16 < f ? 16 : f, dir_mask};
            keys[kn++] = (AosKeyEvent){f, 0};
        } else if (stuck == 3) {    /* maybe a door: push UP then resume */
            keys[kn++] = (AosKeyEvent){0, up};
            keys[kn++] = (AosKeyEvent){40 < f ? 40 : f, 0};
            keys[kn++] = (AosKeyEvent){45, dir_mask};
            keys[kn++] = (AosKeyEvent){f, 0};
        } else {
            /* proactive path clearing: every 3rd cruise segment also
             * attacks (kills enemies on the way), every 4th holds UP
             * (Aria doors open on UP and smooth walks never stall,
             * so the reaction ladder alone would miss doorways) */
            uint32_t cm = dir_mask;
            if (nseg % 3 == 2) cm |= 1;
            else if (nseg % 4 == 3) cm |= 64;
            keys[kn++] = (AosKeyEvent){0, cm};
            keys[kn++] = (AosKeyEvent){f, 0};
        }
        AosPeekRequest pk[2 * RK_COUNT];
        for (int side = 0; side < 2; ++side)
            for (int r = 0; r < RK_COUNT; ++r) {
                pk[side * RK_COUNT + r].frame = side ? f - 1 : 0;
                pk[side * RK_COUNT + r].address = REWARD_PEEKS[r].addr;
                pk[side * RK_COUNT + r].size = REWARD_PEEKS[r].size;
            }
        AosRunSpec spec;
        memset(&spec, 0, sizeof spec);
        spec.frames = f;
        spec.keys = keys;
        spec.key_count = kn;
        spec.peeks = pk;
        spec.peek_count = 2 * RK_COUNT;
        spec.fps_cap = fps_cap;
        spec.fb_mirror = fb_pix;
        AosRunResult *r = calloc(1, sizeof *r);
        aos_mgba_run(session, &spec, r);
        ++nseg;
        /* per-segment motion = did the screen actually move while we
         * held the direction; a frozen segment means wall, enemy or
         * a gap ahead -> climb the reaction ladder next segment */
        double m = 0.0, md = 0.0;
        if (r->fb_first) {
            const uint32_t *fb = aos_mgba_framebuffer(session);
            m = aos_vision_motion(r->fb_first, fb,
                                  AOS_MGBA_WIDTH, AOS_MGBA_HEIGHT, &md);
        }
        if (m < 0.003) {
            if (stuck < 3) ++stuck;
        } else {
            stuck = 0;
        }
        /* damage reaction: HP after this segment is peeked from WRAM —
         * a drop means we got hit, so the next segment counter-attacks
         * instead of whatever the stall ladder chose */
        if (r->peeks && r->peek_count > RK_COUNT + RK_VIT &&
            r->peeks[RK_COUNT + RK_VIT].data) {
            int hp_now = rd16(&r->peeks[RK_COUNT + RK_VIT], 0);
            if (hp_prev >= 0 && hp_now >= 0 && hp_now < hp_prev)
                stuck = 2;
            if (hp_now >= 0) hp_prev = hp_now;
        }
        if (!first) first = r; else { aos_mgba_free_result(last); last = r; }
    }
    if (!first) return;
    if (!last) last = first;
    /* merge: before-peeks from the first segment, after-peeks from the
     * last; fb_first likewise so the trial sees the whole-run motion */
    out->peeks = calloc(2 * RK_COUNT, sizeof(AosPeekResult));
    if (out->peeks && first->peeks && last->peeks &&
        first->peek_count >= 2 * RK_COUNT &&
        last->peek_count >= 2 * RK_COUNT) {
        for (int i = 0; i < RK_COUNT; ++i) {
            out->peeks[i] = first->peeks[i];
            /* ownership of the peek data moves to `out` — NULL the
             * source pointers so free_result does not double-free */
            first->peeks[i].data = NULL;
            first->peeks[i].size = 0;
        }
        for (int i = 0; i < RK_COUNT; ++i) {
            out->peeks[RK_COUNT + i] = last->peeks[RK_COUNT + i];
            last->peeks[RK_COUNT + i].data = NULL;
            last->peeks[RK_COUNT + i].size = 0;
        }
        out->peek_count = 2 * RK_COUNT;
    }
    out->fb_first = first->fb_first;
    first->fb_first = NULL;
    if (last != first) aos_mgba_free_result(last);
    aos_mgba_free_result(first);
    (void)nseg;
}

/* Returns bonus reward; appends human-readable tags to `tags`. */
/* exact-token match inside the comma-separated bonus tag list —
 * plain strstr would let "new_area" match "new_area_song" */
static int btag_has(const char *tags, const char *t) {
    size_t n = strlen(t);
    const char *p = tags;
    while ((p = strstr(p, t))) {
        int lb = (p == tags) || p[-1] == ',';
        int rb = p[n] == ',' || p[n] == 0;
        if (lb && rb) return 1;
        ++p;
    }
    return 0;
}

static double compute_bonus(AosLabDb *db, const AosRunResult *res,
                            int nreward, int scene_is_new, double motion,
                            char *tags, size_t tags_size) {
    if (!res->peeks || res->peek_count < 2 * RK_COUNT) return 0.0;
    double bonus = 0.0;
    size_t tl = 0;
    tags[0] = 0;
#define TAG(t) do {         if (tl + sizeof(t) + 1 < tags_size) {             if (tl) { tags[tl++] = ','; tags[tl] = 0; }             memcpy(tags + tl, t, sizeof(t) - 1); tl += sizeof(t) - 1;             tags[tl] = 0; }     } while (0)
    if (scene_is_new) { bonus += 0.4; TAG("new_scene"); }

    /* before-side reward peeks occupy [0,RK_COUNT), after-side
     * [RK_COUNT,2*RK_COUNT); nreward arg kept for clarity */
    const AosPeekResult *b = res->peeks, *a = res->peeks + RK_COUNT;
    (void)nreward;
    /* mode / area / room / song. Leaving gameplay mode is almost
     * always a game over or menu in trial context (no action presses
     * START) — penalize it, entering gameplay is progress. */
    {
        /* only the low byte is the mode — the upper bytes are frame
         * counters that always differ, which is why rd32 produced a
         * mode_change on nearly every trial and enter/exit never fired */
        int mb = b[RK_MODE].data ? b[RK_MODE].data[0] : -1;
        int ma = a[RK_MODE].data ? a[RK_MODE].data[0] : -1;
        if (mb == 4 && ma != 4) { bonus -= 0.6; TAG("exit_gameplay"); }
        else if (mb != 4 && ma == 4) { bonus += 0.3; TAG("enter_gameplay"); }
        else if (mb != ma) { TAG("mode_change"); }
    }
    /* area=+0xE room=+0xF inside the 0x90 block; song=+0x0 u16 */
    if (b[RK_AREA].data && a[RK_AREA].data &&
        b[RK_AREA].size >= 16 && a[RK_AREA].size >= 16) {
        if (b[RK_AREA].data[14] != a[RK_AREA].data[14]) {
            bonus += 0.5; TAG("new_area");
        }
        if (b[RK_AREA].data[15] != a[RK_AREA].data[15]) {
            int area = b[RK_AREA].data[14], room = a[RK_AREA].data[15];
            if (!room_seen(db, area, room)) {
                bonus += 0.8 + 0.2 * (room_count(db) > 20 ? 1 : 0);
                TAG("new_room");
                room_mark(db, area, room);
                /* reaching a room the fleet never saw is the campaign
                 * progressing: extra credit scaled by exploration depth */
            } else {
                /* backtracking into a known room is usually wasted
                 * motion — small penalty so the bandit prefers pushes
                 * forward over loops */
                bonus -= 0.05; TAG("room_revisit");
            }
        }
        uint16_t s0, s1;
        memcpy(&s0, b[RK_AREA].data, 2);
        memcpy(&s1, a[RK_AREA].data, 2);
        if (s0 != s1) { bonus += 0.6; TAG("new_area_song"); }
    }
    /* vitals: hp@0 mp@2 maxhp@4 maxmp@6 (s16/u16 LE) */
    int hp_b = rd16(&b[RK_VIT], 0), hp_a = rd16(&a[RK_VIT], 0);
    int mp_b = rd16(&b[RK_VIT], 2), mp_a = rd16(&a[RK_VIT], 2);
    int maxhp = rd16(&a[RK_VIT], 4);
    if (maxhp <= 0) maxhp = 1;
    if (hp_a <= 0) { bonus -= 1.0; TAG("death"); }
    else if (hp_a < hp_b) {
        double frac = (double)(hp_b - hp_a) / maxhp;
        bonus -= 0.3 + 0.7 * frac; TAG("damage");
    } else if (hp_a > hp_b) { bonus += 0.4; TAG("healed"); }
    if (mp_a < mp_b) { bonus += 0.1; TAG("ability_used"); }
    /* MP fully drained: cannot attack or use abilities until a refill —
     * a soft negative so combos that burn MP for nothing lose ground */
    if (mp_b > 0 && mp_a <= 0) { bonus -= 0.1; TAG("mp_empty"); }
    /* clean traverse: room changed and hp untouched */
    if (tags[0] && strstr(tags, "new_room") && hp_a == hp_b) {
        bonus += 0.5; TAG("clean_traverse");
    }
    /* xp / gold — magnitude matters: a big XP jump means kills or a
     * boss, a tiny one is noise */
    {
        uint32_t xb = rd32(&b[RK_XP], 0), xa = rd32(&a[RK_XP], 0);
        if (xa > xb) {
            bonus += 0.5; TAG("exp_gain");
            if (xa - xb > 64) { bonus += 0.4; TAG("exp_big"); }
        }
        if (rd32(&b[RK_XP], 4) != rd32(&a[RK_XP], 4)) {
            bonus += 0.3; TAG("gold_gain");
        }
    }
    if (b[RK_LVL].data && a[RK_LVL].data &&
        b[RK_LVL].data[0] != a[RK_LVL].data[0]) {
        /* plausible level-up only: both readings in range and a
         * small increment — the raw byte is noisy, real level-ups
         * are +1 */
        int lb = b[RK_LVL].data[0], la = a[RK_LVL].data[0];
        if (lb >= 1 && lb <= 99 && la >= 1 && la <= 99 &&
            la > lb && la - lb <= 3) {
            bonus += 1.0; TAG("level_up");
        }
    }
    /* items / souls inventories: any byte diff counts */
    if (b[RK_ITEM].data && a[RK_ITEM].data &&
        b[RK_ITEM].size == a[RK_ITEM].size &&
        memcmp(b[RK_ITEM].data, a[RK_ITEM].data, b[RK_ITEM].size)) {
        bonus += 0.6; TAG("item_found");
    }
    if (b[RK_SOUL].data && a[RK_SOUL].data &&
        b[RK_SOUL].size == a[RK_SOUL].size &&
        memcmp(b[RK_SOUL].data, a[RK_SOUL].data, b[RK_SOUL].size)) {
        bonus += 0.5; TAG("soul_gained");
    }
    /* tilemap activity on the game image itself: dialogue text
     * advancing, HUD refreshes, tile swaps. Sampled byte diff keeps
     * it cheap; only counts when the scene itself did not change —
     * new_scene already covers full-screen transitions */
    if (!scene_is_new && b[RK_VRAM].data && a[RK_VRAM].data &&
        b[RK_VRAM].size == a[RK_VRAM].size && b[RK_VRAM].size > 0x1000) {
        long td = 0;
        for (uint32_t i = 0; i + 1 < b[RK_VRAM].size; i += 97) {
            uint16_t v0, v1;
            memcpy(&v0, b[RK_VRAM].data + i, 2);
            memcpy(&v1, a[RK_VRAM].data + i, 2);
            if (v0 != v1) ++td;
        }
        if (td > 25) { bonus += 0.25; TAG("tile_change"); }
    }
    /* entity-table analysis (pure WRAM, no image needed): the 8 KiB
     * region holds entity slots. A slot that was non-empty before and
     * all-zero after means an enemy died or despawned — kills, boss
     * deaths, room clears all surface here. Reverse = spawns. */
    if (b[RK_ENT].data && a[RK_ENT].data &&
        b[RK_ENT].size == a[RK_ENT].size && b[RK_ENT].size >= 8192) {
        int emptied = 0, filled = 0;
        for (int s = 0; s + 256 <= (int)b[RK_ENT].size; s += 256) {
            int nb = 0, na = 0;
            for (int j = 0; j < 256; j += 16) {
                if (b[RK_ENT].data[s + j] || b[RK_ENT].data[s + j + 1] ||
                    b[RK_ENT].data[s + j + 2] || b[RK_ENT].data[s + j + 3])
                    ++nb;
                if (a[RK_ENT].data[s + j] || a[RK_ENT].data[s + j + 1] ||
                    a[RK_ENT].data[s + j + 2] || a[RK_ENT].data[s + j + 3])
                    ++na;
            }
            if (nb > 0 && na == 0) ++emptied;
            else if (nb == 0 && na > 0) ++filled;
        }
        /* during a scene/room transition the whole entity table is
         * torn down and rebuilt — emptied/filled slots there are
         * despawns, not kills. Only trust them inside stable scenes */
        if (!scene_is_new && emptied > 0) {
            bonus += emptied > 2 ? 0.6 : 0.3 * emptied;
            if (bonus > 2.5) bonus = 2.5;
            TAG("enemy_down");
        }
        if (!scene_is_new && filled > emptied) {
            bonus -= 0.1; TAG("enemy_spawn");
        }
        /* raw churn stays as the "world is alive" signal */
        long diff = 0;
        for (uint32_t i = 0; i < b[RK_ENT].size; ++i)
            if (b[RK_ENT].data[i] != a[RK_ENT].data[i]) ++diff;
        if (diff > 400) { bonus += 0.1; TAG("entity_active"); }
        else if (diff < 30) { bonus -= 0.05; TAG("entity_still"); }
    }
    /* movement without damage: screen motion is the reliable signal
     * (WRAM entity position proved layout-dependent) */
    if (motion > 0.008 && hp_a == hp_b) {
        bonus += 0.2; TAG("clean_move");
    }
    /* nothing happened at all: no motion, no reward, no penalty —
     * a frozen spin against a wall or an idle loop */
    if (motion < 0.002 && bonus <= 0.0) {
        bonus -= 0.1; TAG("no_progress");
    }
    if (bonus > 2.5) bonus = 2.5;
    if (bonus < -1.5) bonus = -1.5;
#undef TAG
    return bonus;
}




/* Death-scene registry: hashes of screens seen right after a lethal
 * outcome, kept in meta lab.deathscenes. Any later trial or scout that
 * lands on one of these hashes is standing on the game-over screen —
 * penalized as gameover_screen. This is the visual (OCR-free) half of
 * death detection; the memory half is the exit_gameplay/death tags. */
static int death_scene_known(AosLabDb *db, uint64_t hash) {
    char key[32];
    snprintf(key, sizeof key, "%016llx",
             (unsigned long long)hash);
    char *list = aos_labdb_meta_get(db, "lab.deathscenes");
    int found = list && strstr(list, key) != NULL;
    free(list);
    return found;
}

static void death_scene_mark(AosLabDb *db, uint64_t hash) {
    char key[32];
    snprintf(key, sizeof key, "%016llx",
             (unsigned long long)hash);
    if (death_scene_known(db, hash)) return;
    char *list = aos_labdb_meta_get(db, "lab.deathscenes");
    size_t need = (list ? strlen(list) : 0) + strlen(key) + 2;
    char *nl = malloc(need);
    snprintf(nl, need, "%s%s%s", list ? list : "", list ? "," : "", key);
    /* cap the registry at 64 hashes */
    int commas = 0;
    for (char *q = nl; *q; ++q) if (*q == ',') ++commas;
    if (commas > 64) {
        char *first = strchr(nl, ',');
        if (first) memmove(nl, first + 1, strlen(first));
    }
    aos_labdb_meta_set(db, "lab.deathscenes", nl);
    free(nl);
    free(list);
}

/* Fleet culling: every call ranks this worker's mean reward against
 * every other worker's (worker.N.rsum/rn metas). A worker that is
 * clearly in the bottom while a much better branch exists stops
 * itself — its slot frees for a fresh candidate that smart-restart
 * will pull toward the best checkpoint anyway. */
static int fleet_cull_check(AosLabDb *db, int worker_id,
                            double own_ema, int trials_done) {
    (void)own_ema;
    if (trials_done < 300) return 0;
    double means[8], best = -1e9;
    int active = 0, own_rank_bottom = 1;
    for (int i = 0; i < 8; ++i) {
        char rk[64], nk[64];
        snprintf(rk, sizeof rk, "worker.%d.rsum", i);
        snprintf(nk, sizeof nk, "worker.%d.rn", i);
        char *rs = aos_labdb_meta_get(db, rk);
        char *rn = aos_labdb_meta_get(db, nk);
        means[i] = -1.0;
        if (rs && rn && atol(rn) > 0) {
            means[i] = atof(rs) / (double)atol(rn);
            if (means[i] > best) best = means[i];
            ++active;
        }
        free(rs); free(rn);
    }
    if (active < 3 || means[worker_id] < 0) return 0;
    /* bottom-half check: at least half the fleet must beat us */
    int beaten = 0;
    for (int i = 0; i < 8; ++i)
        if (means[i] >= 0 && means[i] > means[worker_id]) ++beaten;
    if (beaten * 2 < active) own_rank_bottom = 0;
    /* cull only when clearly outclassed: best mean exceeds ours by a
     * wide margin and we are not producing anything */
    if (own_rank_bottom && best > means[worker_id] + 0.5 &&
        means[worker_id] < 0.15) {
        char st[128];
        snprintf(st, sizeof st,
                 "culled: mean=%.2f best=%.2f — freeing slot",
                 means[worker_id], best);
        char pk[64];
        snprintf(pk, sizeof pk, "worker.%d.status", worker_id);
        aos_labdb_meta_set(db, pk, st);
        printf("worker %d %s\n", worker_id, st);
        return 1;
    }
    return 0;
}

/* ---- lethal branches + macro synthesis ------------------------------
 * Game-over rule: a trial that ends in death marks the state it
 * started from as lethal — waypoint restarts and smart hops forget
 * that branch so the fleet never replays a known kill. */
static int state_dead(AosLabDb *db, int64_t id) {
    char k[64];
    snprintf(k, sizeof k, "lab.dead.%lld", (long long)id);
    char *v = aos_labdb_meta_get(db, k);
    int d = v && atoi(v) > 0;
    free(v);
    return d;
}

static void state_dead_mark(AosLabDb *db, int64_t id) {
    char k[64], b[16];
    snprintf(k, sizeof k, "lab.dead.%lld", (long long)id);
    char *v = aos_labdb_meta_get(db, k);
    snprintf(b, sizeof b, "%d", (v ? atoi(v) : 0) + 1);
    aos_labdb_meta_set(db, k, b);
    free(v);
}

/* Frontier pick: parse lab.graph ("A:R>B:S,...") once, compute each
 * room's depth = longest transition chain ending there, then return
 * the index of the non-lethal state sitting in the deepest room.
 * Waypoint picks use it to push trials along discovered passages
 * toward unmapped exits instead of re-farming early rooms. */
static int frontier_pick(AosLabDb *db, AosStateRow *rows, int n) {
    char *g = aos_labdb_meta_get(db, "lab.graph");
    if (!g || !*g) { free(g); return -1; }
    int ea[512], er[512], da[512], dr[512], ne = 0;
    for (char *tok = strtok(g, ","); tok && ne < 512;
         tok = strtok(NULL, ",")) {
        if (sscanf(tok, "%d:%d>%d:%d", &ea[ne], &er[ne],
                   &da[ne], &dr[ne]) == 4) ++ne;
    }
    free(g);
    if (!ne) return -1;
    /* unique room keys (area*256+room) and edge endpoint indices */
    int keys[1024], nk = 0, esi[512], edi[512];
    int depth[1024];
    memset(depth, 0, sizeof depth);
    for (int i = 0; i < ne; ++i) {
        int kk[2] = {ea[i] * 256 + er[i], da[i] * 256 + dr[i]};
        for (int k = 0; k < 2; ++k) {
            int f = 0;
            for (int j = 0; j < nk; ++j)
                if (keys[j] == kk[k]) { f = 1; break; }
            if (!f && nk < 1024) keys[nk++] = kk[k];
        }
    }
    for (int i = 0; i < ne; ++i) {
        esi[i] = edi[i] = -1;
        for (int j = 0; j < nk; ++j) {
            if (keys[j] == ea[i] * 256 + er[i]) esi[i] = j;
            if (keys[j] == da[i] * 256 + dr[i]) edi[i] = j;
        }
    }
    /* depth = longest chain of transitions ending at the room */
    for (int it = 0; it < ne; ++it)
        for (int i = 0; i < ne; ++i)
            if (esi[i] >= 0 && edi[i] >= 0 &&
                depth[edi[i]] < depth[esi[i]] + 1)
                depth[edi[i]] = depth[esi[i]] + 1;
    int best = -1, bestd = -1;
    for (int i = 0; i < n; ++i) {
        if (state_dead(db, rows[i].id) || !rows[i].note) continue;
        const char *rp = strstr(rows[i].note, "room=");
        int ra, rr;
        if (!rp || sscanf(rp, "room=%d:%d", &ra, &rr) != 2) continue;
        int k = ra * 256 + rr, d = 0;
        for (int j = 0; j < nk; ++j)
            if (keys[j] == k) { d = depth[j]; break; }
        if (d > bestd) { bestd = d; best = i; }
    }
    return best;
}

/* Convert a hold-style action's template params into a key script
 * "frame:mask,frame:mask" using the low ends of its ranges. */
static char *hold_to_script(const char *params, int *out_end) {
    ActionParams p;
    params_parse(params, &p);
    if (!p.mask) { *out_end = 0; return NULL; }
    int at = p.at_min, hold = p.hold_min > 0 ? p.hold_min : 1;
    char *sc = malloc(64);
    snprintf(sc, 64, "%d:%u,%d:0", at, p.mask, at + hold);
    *out_end = at + hold;
    return sc;
}

/* Long-sequence construction: every call picks the two best actions
 * by accumulated reward and fuses them into one reusable script
 * action ("macro_<a>+<b>"). The bandit then weighs the macro like any
 * other action — short proven primitives compound into longer
 * campaigns. Capped at 40 macros. */
static void maybe_make_macro(AosLabDb *db, AosActionRow *all, int acount) {
    char *mc = aos_labdb_meta_get(db, "lab.macros");
    int made = mc ? atoi(mc) : 0;
    free(mc);
    if (made >= 40) return;
    int wcount = 0;
    AosWeightRow *w = aos_labdb_weights(db, 512, &wcount);
    if (!w || wcount < 2) { aos_labdb_free_weight_rows(w, wcount); return; }
    /* top-2 distinct actions by MEAN reward — only sequences that
     * actually scored get composed; a macro of failures is just a
     * longer failure. Requires >=8 attempts so one lucky trial cannot
     * fuse into a "proven" primitive. */
    AosActionRow *top[2] = {NULL, NULL};
    double tops[2] = {0, 0};
    for (int wi = 0; wi < wcount; ++wi) {
        if (w[wi].attempts < 8) continue;
        double avg = w[wi].reward_sum / w[wi].attempts;
        for (int ai = 0; ai < acount; ++ai) {
            if (all[ai].id != w[wi].action_id) continue;
            if (!top[0] || avg > tops[0]) {
                if (top[0] && top[0] != top[1]) {
                    top[1] = top[0]; tops[1] = tops[0];
                }
                top[0] = &all[ai]; tops[0] = avg;
            } else if (!top[1] || avg > tops[1]) {
                if (top[1] != top[0]) {
                    top[1] = &all[ai]; tops[1] = avg;
                }
            }
            break;
        }
    }
    aos_labdb_free_weight_rows(w, wcount);
    /* both primitives must be net-positive; macros themselves qualify
     * so proven sequences can chain into longer campaigns */
    if (!top[0] || !top[1] || tops[0] < 0.15 || tops[1] < 0.10) return;
    if (!strcmp(top[0]->name, top[1]->name)) return;
    /* build scripts for both */
    char *sa = NULL, *sb = NULL;
    int ea = 0, eb = 0;
    if (top[0]->script && *top[0]->script) {
        sa = strdup(top[0]->script);
        for (char *p = sa; *p; ++p) if (*p == ',') {}
        /* end frame = last "N:" number */
        int last = 0;
        for (char *p = sa; *p; ++p)
            if (*p >= '0' && *p <= '9') { int v = atoi(p); if (v > last) last = v; }
        ea = last;
    } else {
        sa = hold_to_script(top[0]->params, &ea);
    }
    if (top[1]->script && *top[1]->script) {
        sb = strdup(top[1]->script);
        int last = 0;
        for (char *p = sb; *p; ++p)
            if (*p >= '0' && *p <= '9') { int v = atoi(p); if (v > last) last = v; }
        eb = last;
    } else {
        sb = hold_to_script(top[1]->params, &eb);
    }
    (void)eb;
    if (!sa || !sb) { free(sa); free(sb); return; }
    int gap = 20, off = ea + gap;
    /* offset every frame number in sb by `off` */
    char sb2[512];
    size_t sl = 0;
    sb2[0] = 0;
    char *p = sb;
    while (*p && sl < sizeof sb2 - 16) {
        if (*p >= '0' && *p <= '9') {
            int v = atoi(p);
            sl += snprintf(sb2 + sl, sizeof sb2 - sl, "%d:", v + off);
            while (*p >= '0' && *p <= '9') ++p;
            if (*p == ':') ++p;
        } else {
            sb2[sl++] = *p++;
            sb2[sl] = 0;
        }
    }
    char name[96];
    snprintf(name, sizeof name, "macro_%s+%s",
             top[0]->name ? top[0]->name : "a",
             top[1]->name ? top[1]->name : "b");
    /* dedupe: skip if a macro with this name exists */
    for (int ai = 0; ai < acount; ++ai)
        if (all[ai].name && !strcmp(all[ai].name, name)) {
            free(sa); free(sb); return;
        }
    char script[768];
    snprintf(script, sizeof script, "%s,%s", sa, sb2);
    aos_labdb_add_action(db, name, "script", "", "", script);
    char nb[16];
    snprintf(nb, sizeof nb, "%d", made + 1);
    aos_labdb_meta_set(db, "lab.macros", nb);
    printf("macro synthesized: %s (%s + %s)\n", name,
           top[0]->name, top[1]->name);
    free(sa); free(sb);
}

/* ---- scout mode: autonomous early-game player ----------------------
 * Boots the ROM and plays chunks of 600 frames: A taps advance text
 * and menus, a RIGHT pulse at the end probes whether the player entity
 * responds. Every novel scene hash and every validated gameplay
 * checkpoint is stored as a state blob in the database. */
static int scout_loop(AosMgbaSession *session, AosLabDb *db,
                      const char *rom, int worker_id, int max_chunks) {
    AosFbMirror fb;
    int fb_ok = fb_attach(&fb, worker_id) == 0;
    char phase_key[64], status_key[64], ctrl_key[64];
    snprintf(phase_key, sizeof phase_key, "worker.%d.phase", worker_id);
    snprintf(status_key, sizeof status_key, "worker.%d.status", worker_id);
    snprintf(ctrl_key, sizeof ctrl_key, "lab.control.%d", worker_id);
    aos_labdb_meta_set(db, ctrl_key, "run");
    aos_labdb_meta_set(db, phase_key, "scouting");
    {
        char lcfg[128], lkey[64];
        snprintf(lcfg, sizeof lcfg, "mode=scout|chunks=%d", max_chunks);
        snprintf(lkey, sizeof lkey, "worker.%d.launch", worker_id);
        aos_labdb_meta_set(db, lkey, lcfg);
    }
    aos_labdb_begin_run(db, rom, "scout early game");

    uint64_t seen[1024];
    int nseen = 0, validated = 0, resp_streak = 0, stop = 0;
    int last_new_chunk = 0;
    int in_game = 0;      /* sticky: mode 4 was reached once */
    int prev_mode = -1;   /* gameMode at end of previous chunk */
    int scout_area = -1, scout_room = -1, scout_rooms = 0;
    int scene_saves = 0;
    int64_t scout_reset = -1;
    for (int chunk = 0; chunk < max_chunks && !stop; ++chunk) {
        char *ctl = aos_labdb_meta_get(db, ctrl_key);
        if (ctl && !strcmp(ctl, "stop")) stop = 1;
        free(ctl);
        if (stop) break;
        /* frontier reset: relocate scouting to the promoted branch so
         * the whole fleet maps ahead from the same deep checkpoint */
        {
            char *rs = aos_labdb_meta_get(db, "lab.reset_state");
            if (rs) {
                int64_t rid = atoll(rs);
                free(rs);
                if (rid > 0 && rid != scout_reset) {
                    scout_reset = rid;
                    size_t nsz = 0;
                    uint8_t *nb = aos_labdb_state_blob(db, rid, &nsz);
                    if (nb) {
                        if (aos_mgba_load_state_blob(session, nb, nsz)) {
                            in_game = 1;
                            prev_mode = 4;
                        }
                        free(nb);
                    }
                }
            }
        }

        /* phase 1: 520 frames advancing text/menus.
         * START only navigates title/menu phases — once free gameplay
         * (mode 4) was reached it would just reopen the pause menu and
         * trap every later chunk, so it is disabled from then on. */
        AosKeyEvent ev[32];
        int ne = 0;
        ev[ne++] = (AosKeyEvent){0, 0};
        if (!in_game && chunk % 3 == 0) {
            ev[ne++] = (AosKeyEvent){40, 8};
            ev[ne++] = (AosKeyEvent){55, 0};
        }
        /* if the previous chunk ended outside gameplay while we were
         * in game, a menu is probably open: B cancels back out */
        if (in_game && prev_mode != 4 && prev_mode >= 0) {
            ev[ne++] = (AosKeyEvent){30, 2};
            ev[ne++] = (AosKeyEvent){45, 0};
        }
        for (int f = 120; f < 510; f += 140) {
            ev[ne++] = (AosKeyEvent){f, 1};
            ev[ne++] = (AosKeyEvent){f + 12, 0};
        }
        AosRunSpec spec;
        memset(&spec, 0, sizeof spec);
        spec.frames = 520;
        spec.keys = ev;
        spec.key_count = ne;
        {
            char sk[64];
            snprintf(sk, sizeof sk, "worker.%d.speed", worker_id);
            char *sv = aos_labdb_meta_get(db, sk);
            int spd = sv ? atoi(sv) : 0;
            free(sv);
            spec.fps_cap = spd > 0 ? 60.0 * spd : 0.0;
        }
        if (fb_ok) {
            spec.fb_mirror = fb.pixels;
            spec.fb_seq = fb.seq;
            spec.fb_interval = 2;
        }
        AosRunResult res1;
        aos_mgba_run(session, &spec, &res1);
        aos_mgba_free_result(&res1);

        /* phase 2: 80-frame directional pulse. fb_first = the frame
         * right before the pulse, so screen motion measures exactly
         * the player's response — no WRAM entity layout needed. */
        /* game knowledge: the early game pushes RIGHT (start ->
         * chapel -> study); LEFT only 30% to backtrack */
        uint32_t pulse = (chunk % 10 < 7) ? 16 : 32; /* RIGHT / LEFT */
        AosKeyEvent pev[12];
        int pne = 0;
        pev[pne++] = (AosKeyEvent){0, pulse};
        /* every 6th chunk: tap UP mid-pulse — doors in Aria open with
         * UP, so this is how the scout actually changes rooms */
        if (chunk % 6 == 5) {
            pev[pne++] = (AosKeyEvent){20, 64};
            pev[pne++] = (AosKeyEvent){34, 0};
        }
        /* every 5th chunk: jump (B) mid-pulse to clear gaps */
        if (chunk % 5 == 4) {
            pev[pne++] = (AosKeyEvent){30, 2};
            pev[pne++] = (AosKeyEvent){40, 0};
        }
        /* every 4th chunk: attack (A) while moving — early rooms have
         * mandatory fights (Manticore room, Great Armor approach) */
        if (chunk % 4 == 3) {
            pev[pne++] = (AosKeyEvent){45, 1};
            pev[pne++] = (AosKeyEvent){57, 0};
        }
        pev[pne++] = (AosKeyEvent){79, 0};
        AosPeekRequest pk[2] = {{-1, 0x02000010, 4},
                                {-1, 0x02000090, 16}}; /* mode+area/room */
        AosRunSpec spec2;
        memset(&spec2, 0, sizeof spec2);
        spec2.frames = 80;
        spec2.keys = pev;
        spec2.key_count = pne;
        spec2.peeks = pk;
        spec2.peek_count = 2;
        spec2.fps_cap = spec.fps_cap;
        if (fb_ok) {
            spec2.fb_mirror = fb.pixels;
            spec2.fb_seq = fb.seq;
            spec2.fb_interval = 2;
        }
        AosRunResult res2;
        aos_mgba_run(session, &spec2, &res2);

        int mode = res2.peeks[0].data ? res2.peeks[0].data[0] : -1;
        /* room registry: area=+14 room=+15 of the 0x90 block — mark
         * every (area,room) the scout reaches so the fleet knows the
         * campaign frontier */
        if (res2.peek_count > 1 && res2.peeks[1].data &&
            res2.peeks[1].size >= 16) {
            int area = res2.peeks[1].data[14];
            int room = res2.peeks[1].data[15];
            if ((area || room) &&
                (area != scout_area || room != scout_room)) {
                scout_area = area; scout_room = room;
                if (!room_seen(db, area, room)) {
                    room_mark(db, area, room);
                    ++scout_rooms;
                    /* scouts feed the same weighted progress score —
                     * a genuinely new room is real advancement even
                     * though no trial produced it */
                    char *ps = aos_labdb_meta_get(db, "lab.progress_score");
                    double cur = ps ? atof(ps) : 0;
                    free(ps);
                    char nb[32];
                    snprintf(nb, sizeof nb, "%.1f", cur + 10);
                    aos_labdb_meta_set(db, "lab.progress_score", nb);
                }
            }
        }
        double mdir = 0.0;
        double motion = res2.fb_first
            ? aos_vision_motion(res2.fb_first,
                                aos_mgba_framebuffer(session),
                                AOS_MGBA_WIDTH, AOS_MGBA_HEIGHT, &mdir)
            : 0.0;
        /* >0.8% of central pixels changed during the 80-frame pulse:
         * walking moves far more, idle animation far less */
        int responsive = motion > 0.008;
        if (mode == 4) in_game = 1;
        prev_mode = mode;
        aos_mgba_free_result(&res2);

        const uint32_t *fbpx = aos_mgba_framebuffer(session);
        AosSceneFeat feat;
        aos_vision_analyze(fbpx, AOS_MGBA_WIDTH, AOS_MGBA_HEIGHT, &feat);
        int is_new;
        {
            int found = 0;
            for (int i = 0; i < nseen; ++i)
                if (seen[i] == feat.scene_hash) { found = 1; break; }
            is_new = !found;
        }
        if (is_new) last_new_chunk = chunk;
        if (is_new && nseen < 1024) {
            seen[nseen++] = feat.scene_hash;
            /* disk budget: scene dumps are exploratory footage — keep
             * at most 80 per scout run; validated play checkpoints
             * below are the ones that matter */
            if (scene_saves >= 80) goto scout_skip_scene;
            ++scene_saves;
            {
            size_t ns = 0;
            uint8_t *blob = aos_mgba_save_state_blob(session, &ns);
            if (blob) {
                char nm[96], note[160];
                snprintf(nm, sizeof nm, "scout_c%03d_m%d", chunk, mode);
                snprintf(note, sizeof note,
                         "scene=%016llx mode=%d chunk=%d room=%d:%d",
                         (unsigned long long)feat.scene_hash, mode, chunk,
                         scout_area, scout_room);
                aos_labdb_add_state(db, nm, note, 0, blob, ns);
                free(blob);
            }
            }
        }
        scout_skip_scene:
        resp_streak = (responsive && mode == 4) ? resp_streak + 1 : 0;
        /* save a play checkpoint only when the scene actually changed
         * (real progression) or every 50 chunks as a safety net —
         * saving every chunk flooded the DB with ~300KB blobs */
        if (resp_streak >= 2 && (is_new || chunk % 50 == 0)) {
            /* validated free-gameplay checkpoint: two consecutive
             * responsive chunks rule out cutscene drift */
            ++validated;
            size_t ns = 0;
            uint8_t *blob = aos_mgba_save_state_blob(session, &ns);
            if (blob) {
                char nm[96], note[160];
                snprintf(nm, sizeof nm, "scout_play_%03d", chunk);
                snprintf(note, sizeof note,
                         "responsive pulse motion=%.4f scene=%016llx mode=%d room=%d:%d",
                         motion,
                         (unsigned long long)feat.scene_hash, mode,
                         scout_area, scout_room);
                aos_labdb_add_state(db, nm, note, 0, blob, ns);
                free(blob);
            }
        }
        /* heartbeat + stuck detection: no novel scene for 80 chunks
         * means the scout is wandering in circles */
        {
            struct timeval hb;
            gettimeofday(&hb, NULL);
            char hbv[32];
            snprintf(hbv, sizeof hbv, "%lld",
                     (long long)hb.tv_sec * 1000 + hb.tv_usec / 1000);
            char hbk[64];
            snprintf(hbk, sizeof hbk, "worker.%d.hb", worker_id);
            aos_labdb_meta_set(db, hbk, hbv);
        }
        if (chunk - last_new_chunk > 80) {
            aos_labdb_meta_set(db, phase_key, "stuck");
            aos_labdb_meta_set(db, status_key,
                               "stuck: no new scene for 80 chunks");
            printf("scout worker %d stuck at chunk %d — exiting\n",
                   worker_id, chunk);
            break;
        }
        char st[160];
        snprintf(st, sizeof st,
                 "chunk %d mode=%d motion=%.3f resp=%d states=%d rooms=%d",
                 chunk, mode, motion, responsive, nseen, scout_rooms);
        aos_labdb_meta_set(db, status_key, st);
        printf("scout %s\n", st);
        fflush(stdout);
    }
    aos_labdb_meta_set(db, phase_key, "idle");
    char st[96];
    snprintf(st, sizeof st, "scout done: %d states, %d validated",
             nseen, validated);
    aos_labdb_meta_set(db, status_key, st);
    fb_detach(&fb);
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
    int worker_id = 0, scout = 0, scout_chunks = 100, purge = 0;
    const char *glyphs_dir = NULL;

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
        else if (!strcmp(arg, "--scout")) { scout = 1; }
        else if (!strcmp(arg, "--purge")) { purge = 1; }
        else if (!strcmp(arg, "--glyphs") && value) {
            glyphs_dir = value; ++i;
        }
        else if (!strcmp(arg, "--scout-chunks") && value) {
            scout_chunks = atoi(value); ++i;
        }
        else {
            fprintf(stderr, "unknown argument %s\n", arg);
            return 2;
        }
    }
    if (!db_path || (!rom && !define_action && !purge)) {
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
    if (purge) {
        int t = 0, w = 0, st = 0;
        int n = aos_labdb_purge(db, &t, &w, &st);
        printf("purge: %d rows removed (trials=%d weights=%d states=%d)\n",
               n, t, w, st);
        aos_labdb_close(db);
        return 0;
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
        int64_t id = aos_labdb_update_action(db, fields[0], fields[1],
                                             fields[2], fields[3], fields[4]);
        if (id <= 0)
            id = aos_labdb_add_action(db, fields[0], fields[1],
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

    if (scout) {
        int rc = scout_loop(session, db, rom, worker_id, scout_chunks);
        aos_mgba_close(session);
        aos_labdb_close(db);
        return rc;
    }

    if (record_name) {
        int rc = record_state(session, db, record_name,
                              state_note ? state_note : "", keys_spec,
                              frames_override > 0 ? frames_override : 120);
        aos_mgba_close(session);
        aos_labdb_close(db);
        return rc;
    }

    /* per-slot explore spread applied after arg parsing: slots cover
     * 10-25% exploration so the fleet spans exploit..explore */
    explore = 0.10 + 0.05 * (worker_id & 3);
    if (getenv("AOS_LAB_EXPLORE")) explore = atof(getenv("AOS_LAB_EXPLORE"));

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

    if (glyphs_dir) {
        /* one-shot OCR dump: run 120 frames from the state, snapshot
         * VRAM + video regs, write every used tile as a PGM bitmap */
        AosPeekRequest gp[1] = {{-1, 0x06000000, 0x18000}};
        AosRunSpec gspec;
        memset(&gspec, 0, sizeof gspec);
        gspec.frames = 120;
        gspec.peeks = gp;
        gspec.peek_count = 1;
        if (!aos_mgba_load_state_blob(session, state_blob, blob_size))
            fprintf(stderr, "state reload failed\n");
        AosRunResult gres;
        aos_mgba_run(session, &gspec, &gres);
        int n = 0;
        if (gres.peek_count > 0 && gres.peeks[0].data)
            n = aos_vision_glyph_dump(gres.video_regs,
                                      gres.peeks[0].data,
                                      gres.peeks[0].size, glyphs_dir);
        printf("glyphs: %d tiles dumped to %s\n", n, glyphs_dir);
        aos_mgba_free_result(&gres);
        free(state_blob);
        aos_mgba_close(session);
        aos_labdb_close(db);
        return n > 0 ? 0 : 1;
    }

    /* Waypoint picker: every stored checkpoint stays reachable so a
     * fraction of trials restart from a different saved state — the
     * campaign front advances from whichever checkpoint proved most
     * interesting instead of replaying only the base state forever. */
    int scount = 0;
    AosStateRow *srows = aos_labdb_states(db, 512, &scount);

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
    /* built-in curriculum is ALWAYS appended (not only on an empty
     * DB): DB-defined combos like attack-retreat or spells join the
     * bandit alongside the primitives so a fresh database still learns
     * movement basics while scripted combos are tested too */
    {
        static AosActionRow builtin_actions[4];
        memset(builtin_actions, 0, sizeof builtin_actions);
        struct { const char *n, *k, *pa, *ev; } defs[] = {
            {"walk-right", "hold", "key=right,hold=80-140,at=5-20,jitter=8",
             "probe=0x02000524:4,expect=increase,threshold=4096"},
            {"walk-left", "hold", "key=left,hold=80-140,at=5-20,jitter=8",
             "probe=0x02000524:4,expect=decrease,threshold=4096"},
            {"attack", "hold", "key=a,hold=8-24,at=5-30,jitter=6",
             "probe=0x0201327A:4,expect=decrease"},
            {"explore-input", "hold", "",
             "probe=0x02000000:4,expect=changed"},
        };
        for (int i = 0; i < 4 && chosen_count < 64; ++i) {
            builtin_actions[i].id = -(i + 1);
            builtin_actions[i].name = strdup(defs[i].n);
            builtin_actions[i].kind = strdup(defs[i].k);
            builtin_actions[i].params = strdup(defs[i].pa);
            builtin_actions[i].eval = strdup(defs[i].ev);
            chosen[chosen_count++] = builtin_actions[i];
        }
        if (!action_count && acount == 0)
            fprintf(stderr, "no actions in db - using built-in curriculum\n");
    }

    char phase_key[64], status_key[64], run_key[64], ctrl_key[64],
         hb_key[64];
    snprintf(phase_key, sizeof phase_key, "worker.%d.phase", worker_id);
    snprintf(status_key, sizeof status_key, "worker.%d.status", worker_id);
    snprintf(hb_key, sizeof hb_key, "worker.%d.hb", worker_id);
    snprintf(run_key, sizeof run_key, "worker.%d.run_id", worker_id);
    snprintf(ctrl_key, sizeof ctrl_key, "lab.control.%d", worker_id);
    aos_labdb_meta_set(db, ctrl_key, "run");

    /* persist the launch config so the viewer can respawn this exact
     * batch after a stop (Play button reads worker.N.launch) */
    {
        char lcfg[160];
        snprintf(lcfg, sizeof lcfg,
                 "state=%lld|iters=%d|mode=trial|frames=%d",
                 (long long)state_id, iterations,
                 frames_override > 0 ? frames_override : 120);
        char lkey[64];
        snprintf(lkey, sizeof lkey, "worker.%d.launch", worker_id);
        aos_labdb_meta_set(db, lkey, lcfg);
    }

    int64_t run_id = aos_labdb_begin_run(db, rom, "worker batch");
    aos_labdb_meta_set(db, phase_key, "running");
    char run_buf[32];
    snprintf(run_buf, sizeof run_buf, "%lld", (long long)run_id);
    aos_labdb_meta_set(db, run_key, run_buf);
    int total_frames = 0, trials_done = 0;
    int stop_requested = 0;
    int stuck_streak = 0;
    uint64_t last_scene = 0;
    /* smart restart: EMA of this worker's rewards vs the fleet's best
     * branch — hop to the best checkpoint when clearly behind, and on
     * demand via the viewer's "best" control */
    double reward_ema = 0.0;
    int branch_hops = 0, branch_request = 0;
    int64_t reset_seen = -1;
    struct timeval t0, t1;

    /* bandit over actions: weights table gives each (action,params)
     * a success rate; 1-explore picks the best action, explore picks
     * a random one. Total budget = iterations trials. */
    int total_trials = iterations;
    uint64_t seen_scenes[512];
    int seen_count = 0, scene_saves2 = 0;
    for (int t = 0; t < total_trials && !stop_requested; ++t) {
        {
            AosActionRow *action;
            if (chosen_count == 1 ||
                ((double)rand() / RAND_MAX) < explore) {
                action = &chosen[rand() % chosen_count];
            } else {
                /* aggregate each action's weights into one mean reward
                 * and one attempt count — taking a single row let one
                 * lucky params combo speak for the whole action */
                int wcount = 0;
                AosWeightRow *w = aos_labdb_weights(db, 8192, &wcount);
                double best_rate = -1.0;
                int rates[64];
                long attempts[64];
                for (int ci = 0; ci < chosen_count && ci < 64; ++ci) {
                    double rsum = 0.0;
                    long att = 0;
                    for (int wi = 0; wi < wcount; ++wi)
                        if (w[wi].action_id == chosen[ci].id &&
                            w[wi].attempts > 0) {
                            rsum += w[wi].reward_sum;
                            att += w[wi].attempts;
                        }
                    rates[ci] = att > 0 ? (int)(rsum / att * 1000000) : -1;
                    attempts[ci] = att;
                    /* explore-input (-4) is a discovery primitive, not a
                     * strategy: random key mashing wins reward averages
                     * by luck and then starves every real action. It
                     * stays in the random-explore pool and warmup, but
                     * can never win exploitation */
                    if (chosen[ci].id != -4 && att > 0 &&
                        (double)rsum / att > best_rate)
                        best_rate = (double)rsum / att;
                }
                /* warmup floor: an action with almost no attempts has
                 * not been fairly tested — sample it before letting
                 * the incumbent run away with the budget */
                int cold[64], nc = 0;
                for (int ci = 0; ci < chosen_count && ci < 64; ++ci)
                    if (attempts[ci] < 40) cold[nc++] = ci;
                int ties[64], nt = 0;
                if (nc > 0) {
                    long min_att = attempts[cold[0]];
                    for (int k = 1; k < nc; ++k)
                        if (attempts[cold[k]] < min_att)
                            min_att = attempts[cold[k]];
                    for (int k = 0; k < nc; ++k)
                        if (attempts[cold[k]] == min_att)
                            ties[nt++] = cold[k];
                    action = &chosen[ties[rand() % nt]];
                } else {
                    for (int ci = 0; ci < chosen_count && ci < 64; ++ci)
                        if (chosen[ci].id != -4 && best_rate >= 0 &&
                            rates[ci] == (int)(best_rate * 1000000))
                            ties[nt++] = ci;
                    if (nt > 0)
                        action = &chosen[ties[rand() % nt]];
                    else
                        action = &chosen[rand() % chosen_count];
                }
                aos_labdb_free_weight_rows(w, wcount);
            }
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
                if (control && !strcmp(control, "best")) {
                    aos_labdb_meta_set(db, ctrl_key, "run");
                    branch_request = 1;
                }
                if (control && !strcmp(control, "stop")) stop_requested = 1;
                free(control);
            }
            /* frontier reset: another worker consolidated progress and
             * promoted a branch — relocate this worker's base state so
             * the whole fleet continues from the promising path */
            {
                char *rs = aos_labdb_meta_get(db, "lab.reset_state");
                if (rs) {
                    int64_t rid = atoll(rs);
                    free(rs);
                    if (rid > 0 && rid != reset_seen) {
                        reset_seen = rid;
                        size_t nsz = 0;
                        uint8_t *nb = aos_labdb_state_blob(db, rid, &nsz);
                        if (nb) {
                            free(state_blob);
                            state_blob = nb;
                            blob_size = nsz;
                            state_id = rid;
                            aos_labdb_meta_set(db, phase_key, "reset");
                        }
                    }
                }
            }
            if (stop_requested) break;
            /* smart restart: every 60 trials, or on demand, hop to the
             * fleet's highest-value checkpoint when this worker's EMA
             * reward is clearly below it */
            if (branch_request || (trials_done && trials_done % 60 == 0)) {
                double bv = 0.0;
                int64_t bs = aos_labdb_elo_best(db, &bv);
                if (bs > 0 && state_dead(db, bs)) bs = -1;
                if (bs > 0 && bs != state_id && branch_hops < 1000 &&
                    (branch_request || 1)) {
                    size_t nsz = 0;
                    uint8_t *nb = aos_labdb_state_blob(db, bs, &nsz);
                    if (nb) {
                        free(state_blob);
                        state_blob = nb;
                        blob_size = nsz;
                        state_id = bs;
                        ++branch_hops;
                        branch_request = 0;
                        char st[96];
                        snprintf(st, sizeof st,
                                 "branch->state %lld (v=%.2f)",
                                 (long long)bs, bv);
                        aos_labdb_meta_set(db, status_key, st);
                        printf("worker %d %s\n", worker_id, st);
                    }
                } else {
                    branch_request = 0;
                }
            }
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
            /* traversal needs a real room-crossing budget — a full
             * room with fights takes 10-20s; 720f (~12s) lets the
             * reaction ladder actually reach doors and clears */
            if (action->kind && !strcmp(action->kind, "traverse") &&
                frames < 720) frames = 720;
            /* completion rule: ~15% of trials run 3x longer — short
             * probes cannot reach doors, so occasionally give the
             * agent a real traversal budget */
            if ((rand() % 100) < 15) frames *= 3;

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
            } else {
                /* no scripted input: synthesize a random press so every
                 * trial is a real experiment, not a frozen frame */
                static const uint32_t btns[] = {1, 2, 16, 32, 64, 128,
                                               256, 512};
                base.mask = btns[rand() % 8];
                base.at = 5 + rand() % 40;
                base.hold = 15 + rand() % 80;
                schedule = build_schedule(&base, &schedule_count, frames);
                snprintf(keys_label, sizeof keys_label,
                         "rnd:%x@%d+%d", base.mask, base.at, base.hold);
            }

            /* reward peeks always run (before@0 / after@last); the
             * action probe, when present, occupies the last two slots */
            AosPeekRequest peeks[2 * RK_COUNT + 3];
            int peek_count = 0;
            for (int side = 0; side < 2; ++side) {
                int fr = side ? frames - 1 : 0;
                for (int r = 0; r < RK_COUNT; ++r) {
                    peeks[peek_count].frame = fr;
                    peeks[peek_count].address = REWARD_PEEKS[r].addr;
                    peeks[peek_count].size = REWARD_PEEKS[r].size;
                    ++peek_count;
                }
            }
            if (has_probe) {
                peeks[peek_count].frame = 0;
                peeks[peek_count].address = probe.address;
                peeks[peek_count].size = probe.size;
                ++peek_count;
                peeks[peek_count].frame = frames - 1;
                peeks[peek_count].address = probe.address;
                peeks[peek_count].size = probe.size;
                ++peek_count;
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

            /* Waypoint restart: ~25% of trials begin from a random
             * stored checkpoint (novel scenes, scout finds, manual
             * records) so learning branches off the deepest states. */
            int64_t used_state_id = state_id;
            const uint8_t *load_blob = state_blob;
            size_t load_size = blob_size;
            uint8_t *alt_blob = NULL;
            if (scount > 1 && (rand() % 100) < 25) {
                /* frontier-first: most waypoint picks jump straight to
                 * the fleet's best-valued state so a discovered passage
                 * pulls every worker forward; the rest stay random for
                 * coverage. No branch separation — the whole fleet
                 * exploits the frontier together. */
                int pick_i = -1, play_n = 0;
                double bv2 = 0.0;
                int64_t bs2 = aos_labdb_elo_best(db, &bv2);
                if (bs2 > 0 && !state_dead(db, bs2) &&
                    (rand() % 100) < 60) {
                    for (int si = 0; si < scount; ++si)
                        if (srows[si].id == bs2) { pick_i = si; break; }
                }
                if (pick_i < 0) {
                for (int si = 0; si < scount; ++si)
                    if (srows[si].name &&
                        strstr(srows[si].name, "play")) ++play_n;
                if (play_n && (rand() % 100) < 60) {
                    /* depth bias: scout_play_NNN numbers grow with
                     * exploration depth — half the picks take the
                     * deepest available checkpoint so trials push the
                     * frontier instead of re-farming early rooms */
                    if ((rand() % 100) < 50) {
                        int best_suf = -1;
                        for (int si = 0; si < scount; ++si) {
                            const char *nm = srows[si].name;
                            if (!nm || !strstr(nm, "play")) continue;
                            const char *us = strrchr(nm, '_');
                            int suf = us ? atoi(us + 1) : 0;
                            if (suf > best_suf &&
                                !state_dead(db, srows[si].id)) {
                                best_suf = suf;
                                pick_i = si;
                            }
                        }
                    }
                    if (pick_i < 0) {
                        int want = rand() % play_n;
                        for (int si = 0; si < scount; ++si)
                            if (srows[si].name &&
                                strstr(srows[si].name, "play") &&
                                want-- == 0) { pick_i = si; break; }
                    }
                }
                if (pick_i < 0) pick_i = rand() % scount;
                /* never restart from a lethal branch */
                for (int tr = 0; tr < 8 && state_dead(db, srows[pick_i].id);
                     ++tr)
                    pick_i = rand() % scount;
                }
                /* prefer states captured in gameplay mode — a trial
                 * starting inside a menu or cutscene is a wasted run.
                 * Notes carry "mode=N" since the scout/auto saves began
                 * recording it; unmarked legacy states stay eligible */
                if (pick_i >= 0 && srows[pick_i].note &&
                    strstr(srows[pick_i].note, "mode=") &&
                    !strstr(srows[pick_i].note, "mode=4")) {
                    for (int si = 0; si < scount; ++si) {
                        if (si == pick_i || !srows[si].note ||
                            !strstr(srows[si].note, "mode=4") ||
                            state_dead(db, srows[si].id)) continue;
                        pick_i = si;
                        break;
                    }
                }
                /* risk-aware pick: states tagged room=A:R whose room
                 * accumulated heavy damage/death penalties get passed
                 * over — the fleet knows which rooms hurt before it
                 * commits branches there */
                for (int tr = 0; tr < 4 && pick_i >= 0 &&
                     srows[pick_i].note; ++tr) {
                    int ra = -1, rr = -1;
                    const char *rp = srows[pick_i].note
                        ? strstr(srows[pick_i].note, "room=") : NULL;
                    if (rp && sscanf(rp, "room=%d:%d", &ra, &rr) == 2 &&
                        ra >= 0) {
                        char rk[80];
                        snprintf(rk, sizeof rk, "lab.risk.%d.%d", ra, rr);
                        char *rv = aos_labdb_meta_get(db, rk);
                        double risk = rv ? atof(rv) : 0;
                        free(rv);
                        if (risk > 3.0) {
                            int alt = rand() % scount;
                            if (alt != pick_i && !state_dead(db, srows[alt].id))
                                pick_i = alt;
                            continue;
                        }
                    }
                    break;
                }
                /* frontier-depth pick: ~40% of waypoint restarts jump
                 * to a state in the graph-deepest room — a systematic
                 * push toward unmapped exits along discovered
                 * passages instead of re-farming early rooms */
                if ((rand() % 100) < 40) {
                    int fi = frontier_pick(db, srows, scount);
                    if (fi >= 0) pick_i = fi;
                }
                AosStateRow *pick = &srows[pick_i];
                size_t asz = 0;
                alt_blob = aos_labdb_state_blob(db, pick->id, &asz);
                if (alt_blob) {
                    load_blob = alt_blob;
                    load_size = asz;
                    used_state_id = pick->id;
                }
            }

            gettimeofday(&t0, NULL);
            /* reload state blob */
            if (!aos_mgba_load_state_blob(session, load_blob, load_size)) {
                fprintf(stderr, "state reload failed\n");
            }
            AosRunResult res;
            if (action->kind && !strcmp(action->kind, "traverse")) {
                /* adaptive room-crossing: segmented direction hold with
                 * jump/attack/door reactions on detected stalls */
                uint32_t dirmask = base.mask & (16 | 32 | 64 | 128);
                run_traverse(session, frames, dirmask, spec.fps_cap,
                             fb_ok ? fb.pixels : NULL, &res);
            } else {
                aos_mgba_run(session, &spec, &res);
            }
            free(alt_blob);
            gettimeofday(&t1, NULL);
            int duration_ms = (int)((t1.tv_sec - t0.tv_sec) * 1000 +
                                    (t1.tv_usec - t0.tv_usec) / 1000);

            char outcome[32] = "fail";
            double reward = 0.0;
            char metrics[1024] = "{}";
            int probe_base = 2 * RK_COUNT;
            if (has_probe && res.peek_count >= probe_base + 2) {
                eval_score(&probe, &res.peeks[probe_base],
                           &res.peeks[probe_base + 1], outcome,
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

            double trial_motion = 0.0;
            /* visual signature of the final frame -> metrics JSON;
             * a never-seen scene hash becomes a new auto checkpoint */
            uint64_t scene_hash = 0;
            {
                const uint32_t *fb = aos_mgba_framebuffer(session);
                AosSceneFeat feat;
                aos_vision_analyze(fb, AOS_MGBA_WIDTH, AOS_MGBA_HEIGHT,
                                   &feat);
                aos_vision_metrics_append(metrics, sizeof metrics, &feat);
                scene_hash = feat.scene_hash;
                /* motion between frame 0 and the last frame — the
                 * reliable "did anything move" signal that does not
                 * depend on WRAM entity layout */
                double movedir = 0.0;
                if (res.fb_first) {
                    trial_motion = aos_vision_motion(res.fb_first, fb,
                        AOS_MGBA_WIDTH, AOS_MGBA_HEIGHT, &movedir);
                    double motion = trial_motion;
                    size_t ml = strlen(metrics);
                    if (ml && metrics[ml - 1] == '}') {
                        char extra[80];
                        snprintf(extra, sizeof extra,
                                 ",\"motion\":%.4f,\"mdir\":%.2f}",
                                 motion, movedir);
                        metrics[ml - 1] = 0;
                        strncat(metrics, extra,
                                sizeof metrics - ml - 1);
                    }
                }
                /* tilemap OCR signature from the post-run VRAM peek
                 * (the after-side reward region) plus the session's
                 * video-register snapshot */
                {
                    int vram_i = RK_COUNT + RK_VRAM;
                    if (vram_i >= res.peek_count ||
                        res.peeks[vram_i].address != 0x06000000)
                        vram_i = -1;
                    if (vram_i >= 0 && res.peeks[vram_i].data) {
                        char tm[256];
                        aos_vision_tilemap(res.video_regs,
                            res.peeks[vram_i].data,
                            res.peeks[vram_i].size,
                            tm, sizeof tm);
                        if (tm[0]) {
                            size_t ml = strlen(metrics);
                            if (ml && metrics[ml - 1] == '}') {
                                metrics[ml - 1] = 0;
                                strncat(metrics, ",\"tilemap\":\"",
                                        sizeof metrics - ml - 1);
                                ml = strlen(metrics);
                                strncat(metrics, tm,
                                        sizeof metrics - ml - 1);
                                ml = strlen(metrics);
                                strncat(metrics, "\"}",
                                        sizeof metrics - ml - 1);
                            }
                        }
                    }
                }
                int seen = 0;
                for (int si = 0; si < seen_count; ++si)
                    if (seen_scenes[si] == scene_hash) { seen = 1; break; }
                /* disk budget: auto checkpoints only while this worker
                 * has fewer than 120 scene saves — novelty is still
                 * scored via the hash registry above */
                if (!seen && seen_count < 512 && scene_saves2 < 120) {
                    ++scene_saves2;
                    seen_scenes[seen_count++] = scene_hash;
                    size_t ns = 0;
                    uint8_t *blob = aos_mgba_save_state_blob(session, &ns);
                    if (blob) {
                        char nm[96];
                        snprintf(nm, sizeof nm, "auto_w%d_%08llx",
                                 worker_id,
                                 (unsigned long long)(scene_hash & 0xFFFFFFFF));
                        char note[160];
                        int amode = -1, a_area = -1, a_room = -1;
                        if (res.peek_count > RK_COUNT + RK_MODE &&
                            res.peeks[RK_COUNT + RK_MODE].data)
                            amode = res.peeks[RK_COUNT + RK_MODE].data[0];
                        if (res.peek_count > RK_COUNT + RK_AREA &&
                            res.peeks[RK_COUNT + RK_AREA].data &&
                            res.peeks[RK_COUNT + RK_AREA].size >= 16) {
                            a_area = res.peeks[RK_COUNT + RK_AREA].data[14];
                            a_room = res.peeks[RK_COUNT + RK_AREA].data[15];
                        }
                        snprintf(note, sizeof note,
                                 "auto scene=%016llx trial=%lld mode=%d room=%d:%d",
                                 (unsigned long long)scene_hash,
                                 (long long)t, amode, a_area, a_room);
                        int64_t nid = aos_labdb_add_state(
                            db, nm, note, state_id, blob, (int)ns);
                        (void)nid;
                        free(blob);
                    }
                }
            }

            /* exploration bonus on top of the action probe */
            int trial_died = 0, trial_exit_dmg = 0;
            {
                /* scene_is_new = this trial registered the hash */
                int scene_new = (seen_count > 0 &&
                                 seen_scenes[seen_count - 1] == scene_hash);
                char btags[256];
                double bonus = compute_bonus(db, &res, RK_COUNT, scene_new,
                                             trial_motion, btags,
                                             sizeof btags);
                reward += bonus;
                trial_died = strstr(btags, "death") != NULL;
                trial_exit_dmg = strstr(btags, "exit_gameplay") &&
                                 strstr(btags, "damage");
                if (reward > 4.0) reward = 4.0;
                if (reward < -1.5) reward = -1.5;
                /* merge tags into metrics before the closing brace */
                size_t ml = strlen(metrics);
                if (ml && metrics[ml - 1] == '}') {
                    char extra[320];
                    snprintf(extra, sizeof extra,
                             ",\"bonus\":%.2f,\"tags\":\"%s\"}",
                             bonus, btags);
                    metrics[ml - 1] = 0;
                    strncat(metrics, extra, sizeof metrics - ml - 1);
                }
                /* campaign progress score: weighted ADVANCEMENT only.
                 * New rooms/areas/objectives/kills count; free-roam
                 * motion, tile churn and item vacuuming do not — raw
                 * distance confused wandering with progress. This
                 * score, not lab.motion_total, drives consolidation. */
                {
                    double pts = 0;
                    if (btag_has(btags, "new_room")) pts += 10;
                    if (btag_has(btags, "new_area")) pts += 10;
                    if (btag_has(btags, "new_area_song")) pts += 4;
                    if (btag_has(btags, "new_scene")) pts += 1;
                    if (btag_has(btags, "level_up")) pts += 5;
                    if (btag_has(btags, "exp_big")) pts += 4;
                    else if (btag_has(btags, "exp_gain")) pts += 1;
                    if (btag_has(btags, "item_found")) pts += 1;
                    if (btag_has(btags, "soul_gained")) pts += 1;
                    if (btag_has(btags, "gold_gain")) pts += 0.5;
                    if (btag_has(btags, "enemy_down")) pts += 0.5;
                    if (btag_has(btags, "healed")) pts += 0.5;
                    if (btag_has(btags, "enter_gameplay")) pts += 0.5;
                    if (btag_has(btags, "damage")) pts -= 1;
                    if (btag_has(btags, "death")) pts -= 20;
                    if (btag_has(btags, "gameover_screen")) pts -= 10;
                    if (btag_has(btags, "exit_gameplay")) pts -= 3;
                    if (btag_has(btags, "room_revisit")) pts -= 0.3;
                    if (btag_has(btags, "no_progress")) pts -= 0.5;
                    if (btag_has(btags, "mp_empty")) pts -= 0.2;
                    if (btag_has(btags, "enemy_spawn")) pts -= 0.2;
                    if (pts != 0) {
                        char pk[64], pv[64];
                        snprintf(pk, sizeof pk, "lab.progress_score");
                        char *pv0 = aos_labdb_meta_get(db, pk);
                        double cur = pv0 ? atof(pv0) : 0;
                        free(pv0);
                        snprintf(pv, sizeof pv, "%.1f", cur + pts);
                        aos_labdb_meta_set(db, pk, pv);
                    }
                }
                /* room graph + per-room risk memory: the area peek
                 * (0x02000090) carries area=data[14], room=data[15].
                 * A change between run start and end is a real
                 * transition edge — the fleet's map. Damage/death in
                 * a room accumulates lab.risk.A.R so dangerous rooms
                 * are known before pathfinding routes through them. */
                if (res.peek_count > RK_COUNT + RK_AREA &&
                    res.peeks[RK_AREA].data &&
                    res.peeks[RK_COUNT + RK_AREA].data &&
                    res.peeks[RK_AREA].size >= 16 &&
                    res.peeks[RK_COUNT + RK_AREA].size >= 16) {
                    const uint8_t *ab = res.peeks[RK_AREA].data;
                    const uint8_t *aa = res.peeks[RK_COUNT + RK_AREA].data;
                    int ba = ab[14], br = ab[15], aa2 = aa[14], ar = aa[15];
                    if ((ba || br) && (aa2 || ar) &&
                        (ba != aa2 || br != ar)) {
                        char ek[80];
                        snprintf(ek, sizeof ek, "lab.edge.%d.%d>%d.%d",
                                 ba, br, aa2, ar);
                        char *ev = aos_labdb_meta_get(db, ek);
                        int n = ev ? atoi(ev) : 0;
                        free(ev);
                        char nb[16];
                        snprintf(nb, sizeof nb, "%d", n + 1);
                        aos_labdb_meta_set(db, ek, nb);
                        /* adjacency list for pathfinding: one meta
                         * value "A:R>B:S,A:R>B:S,..." — deduped */
                        char em[32];
                        snprintf(em, sizeof em, "%d:%d>%d:%d",
                                 ba, br, aa2, ar);
                        char *gv = aos_labdb_meta_get(db, "lab.graph");
                        if (!gv || !strstr(gv, em)) {
                            size_t gl = gv ? strlen(gv) : 0;
                            char *ng = malloc(gl + strlen(em) + 2);
                            snprintf(ng, gl + strlen(em) + 2, "%s%s%s",
                                     gv ? gv : "", gl ? "," : "", em);
                            aos_labdb_meta_set(db, "lab.graph", ng);
                            free(ng);
                        }
                        free(gv);
                    }
                    /* risk accumulates in the room the run STARTED in —
                     * that is where the damage happened */
                    if ((ba || br) &&
                        (btag_has(btags, "damage") ||
                         btag_has(btags, "death") ||
                         btag_has(btags, "no_progress"))) {
                        char rk[80];
                        snprintf(rk, sizeof rk, "lab.risk.%d.%d", ba, br);
                        char *rv = aos_labdb_meta_get(db, rk);
                        double cur = rv ? atof(rv) : 0;
                        free(rv);
                        double w = btag_has(btags, "death") ? 5.0 :
                                   btag_has(btags, "damage") ? 1.0 : 0.3;
                        char nb[32];
                        snprintf(nb, sizeof nb, "%.1f", cur + w);
                        aos_labdb_meta_set(db, rk, nb);
                    }
                }
            }

            /* distance travelled: screen-motion integral per trial —
             * the WRAM entity position proved layout-dependent, so
             * accumulated pixel-motion is the honest proxy for how far
             * the campaign has actually pushed forward */
            {
                char mk[64], nb[32];
                snprintf(mk, sizeof mk, "lab.motion_total");
                char *mv = aos_labdb_meta_get(db, mk);
                long tot = mv ? atol(mv) : 0;
                free(mv);
                snprintf(nb, sizeof nb, "%ld",
                         tot + (long)(trial_motion * 100000.0));
                aos_labdb_meta_set(db, mk, nb);
            }
            /* game-over rule: a lethal branch is forgotten — mark the
             * starting state so waypoint restarts and smart hops skip
             * it from now on */
            /* visual death detection: the scene hash at end of run —
             * mark lethal screens for next time and penalize landing
             * on a known game-over screen */
            if (scene_hash) {
                if (trial_died) death_scene_mark(db, scene_hash);
                else if (death_scene_known(db, scene_hash)) {
                    reward -= 0.8;
                    if (reward < -1.5) reward = -1.5;
                }
            }
            /* lethal branch = death, or leaving gameplay with damage —
             * game overs reload the save so HP alone misses them.
             * Ban the branch AND remember the exact params combo that
             * killed so the bandit stops replaying it. */
            if (trial_died || trial_exit_dmg) {
                state_dead_mark(db, used_state_id);
                lethal_mark(db, action->id, drawn);
            }
            /* refined success rule: a probe-fail that still scored
             * exploration reward is a partial, not a dead spin */
            if (!strcmp(outcome, "fail") && reward > 0.0)
                snprintf(outcome, sizeof outcome, "partial");
            /* branch Elo: this trial's reward is a match result
             * against the previous trial's branch — relative rating,
             * not absolute score */
            elo_match(db, used_state_id, reward);
            int64_t trial_id = aos_labdb_add_trial(
                db, run_id, used_state_id, action->id, drawn, keys_label,
                frames, outcome, reward, metrics, duration_ms);
            ++trials_done;
            total_frames += frames;
            /* REWIND — emulator-style undo through save states: a
             * net-negative trial earns one corrective retry from the
             * same branch state. The evasive input (attack tap, then a
             * direction hold alternating sides) either recovers the
             * run or confirms the branch is genuinely bad; the retry
             * is logged as its own trial (action -99) so the failure
             * stays on record while any recovery still counts. */
            if (reward < -0.10) {
                size_t rsz = 0;
                uint8_t *rblob =
                    aos_labdb_state_blob(db, used_state_id, &rsz);
                if (rblob &&
                    aos_mgba_load_state_blob(session, rblob, rsz)) {
                    int dir = (rand() & 1) ? 16 : 32;
                    AosKeyEvent rev[4] = {
                        {12, 1}, {20, 0},
                        {30, (uint32_t)dir},
                        {frames > 20 ? frames - 10 : frames, 0},
                    };
                    AosPeekRequest rpeeks[2 * RK_COUNT];
                    for (int side = 0; side < 2; ++side)
                        for (int r = 0; r < RK_COUNT; ++r) {
                            rpeeks[side * RK_COUNT + r].frame =
                                side ? frames - 1 : 0;
                            rpeeks[side * RK_COUNT + r].address =
                                REWARD_PEEKS[r].addr;
                            rpeeks[side * RK_COUNT + r].size =
                                REWARD_PEEKS[r].size;
                        }
                    AosRunSpec rspec;
                    memset(&rspec, 0, sizeof rspec);
                    rspec.frames = frames;
                    rspec.keys = rev;
                    rspec.key_count = 4;
                    rspec.peeks = rpeeks;
                    rspec.peek_count = 2 * RK_COUNT;
                    rspec.fps_cap = spec.fps_cap;
                    if (fb_ok) rspec.fb_mirror = fb.pixels;
                    AosRunResult rres;
                    aos_mgba_run(session, &rspec, &rres);
                    double rmotion = 0.0, rmdir = 0.0;
                    if (rres.fb_first) {
                        const uint32_t *fb2 =
                            aos_mgba_framebuffer(session);
                        rmotion = aos_vision_motion(
                            rres.fb_first, fb2,
                            AOS_MGBA_WIDTH, AOS_MGBA_HEIGHT, &rmdir);
                    }
                    char rtags[256];
                    double rbonus = compute_bonus(
                        db, &rres, RK_COUNT, 0, rmotion,
                        rtags, sizeof rtags);
                    char rout[32];
                    if (rbonus > 0.3) snprintf(rout, sizeof rout, "success");
                    else if (rbonus > 0.0)
                        snprintf(rout, sizeof rout, "partial");
                    else snprintf(rout, sizeof rout, "fail");
                    char rmetrics[384];
                    snprintf(rmetrics, sizeof rmetrics,
                             "{\"rewind\":1,\"tags\":\"%s\",\"mdir\":%.2f}",
                             rtags, rmdir);
                    aos_labdb_add_trial(db, run_id, used_state_id, -99,
                                        "rewind:evasive", "rewind",
                                        frames, rout, rbonus,
                                        rmetrics, 0);
                    aos_mgba_free_result(&rres);
                    printf("worker %d rewind r=%.2f -> %.2f (%s)\n",
                           worker_id, reward, rbonus, rout);
                }
                free(rblob);
            }
            /* campaign progress metric: deepest room count + max
             * level seen by the whole fleet */
            {
                int lvl = (res.peek_count > RK_COUNT + RK_LVL &&
                           res.peeks[RK_COUNT + RK_LVL].data)
                              ? res.peeks[RK_COUNT + RK_LVL].data[0] : 0;
                if (lvl < 1 || lvl > 99) lvl = 0;
                char *ml = aos_labdb_meta_get(db, "lab.maxlevel");
                int cur = ml ? atoi(ml) : 0;
                if (lvl > cur) {
                    char b[16];
                    snprintf(b, sizeof b, "%d", lvl);
                    aos_labdb_meta_set(db, "lab.maxlevel", b);
                }
                free(ml);
                char prog[96], old[64] = "";
                char *ml2 = aos_labdb_meta_get(db, "lab.maxlevel");
                char *md = aos_labdb_meta_get(db, "lab.motion_total");
                char *ps = aos_labdb_meta_get(db, "lab.progress_score");
                snprintf(prog, sizeof prog,
                         "rooms=%d level=%d dist=%d score=%.0f",
                         room_count(db), ml2 ? atoi(ml2) : 0,
                         md ? atoi(md) : 0, ps ? atof(ps) : 0);
                free(ps);
                free(md);
                free(ml2);
                char *op = aos_labdb_meta_get(db, "lab.progress");
                if (op) snprintf(old, sizeof old, "%s", op);
                if (strcmp(old, prog))
                    aos_labdb_meta_set(db, "lab.progress", prog);
                free(op);
                /* frontier consolidation: when the weighted progress
                 * score (real advancement — rooms, areas, objectives,
                 * kills — not free-roam motion) grows meaningfully, the
                 * fleet found a promising branch — purge everything
                 * else and point every worker at the Elo champion via
                 * lab.reset_state */
                {
                    char *md3 = aos_labdb_meta_get(db, "lab.progress_score");
                    double score_now = md3 ? atof(md3) : 0;
                    free(md3);
                    char *cd = aos_labdb_meta_get(db, "lab.consol_score");
                    double score_ref = cd ? atof(cd) : -1;
                    free(cd);
                    if (score_ref < 0) {
                        char nb[32];
                        snprintf(nb, sizeof nb, "%.1f", score_now);
                        aos_labdb_meta_set(db, "lab.consol_score", nb);
                    } else if (score_now > score_ref + score_ref / 10 + 5 &&
                               score_now > 0) {
                        int cn = aos_labdb_consolidate(db);
                        double cbv = 0.0;
                        int64_t cb = aos_labdb_elo_best(db, &cbv);
                        if (cb > 0) {
                            char nb[32];
                            snprintf(nb, sizeof nb, "%lld", (long long)cb);
                            aos_labdb_meta_set(db, "lab.reset_state", nb);
                        }
                        char nb[32];
                        snprintf(nb, sizeof nb, "%.1f", score_now);
                        aos_labdb_meta_set(db, "lab.consol_score", nb);
                        printf("worker %d frontier consolidation: %d rows, reset->%lld (score %.1f)\n",
                               worker_id, cn, (long long)cb, score_now);
                    }
                }
            }
            /* long-sequence construction: every 250 trials fuse the
             * two best actions into a reusable macro action */
            if (trials_done % 250 == 249)
                maybe_make_macro(db, all, acount);
            /* automatic aggressive purge: every 200 trials drop the
             * dead weight so the DB stays lean */
            if (trials_done % 200 == 0) {
                int pt = 0, pw = 0, ps = 0;
                int pn = aos_labdb_purge(db, &pt, &pw, &ps);
                if (pn)
                    printf("auto-purge: %d rows (t%d w%d s%d)\n",
                           pn, pt, pw, ps);
            }
            char status[256];
            snprintf(status, sizeof status,
                     "trial %lld action=%lld %s -> %s",
                     (long long)trial_id, (long long)action->id, drawn,
                     outcome);
            aos_labdb_meta_set(db, status_key, status);
            /* heartbeat: epoch ms so the viewer can flag stale workers */
            {
                struct timeval hb;
                gettimeofday(&hb, NULL);
                char hbv[32];
                snprintf(hbv, sizeof hbv, "%lld",
                         (long long)hb.tv_sec * 1000 + hb.tv_usec / 1000);
                aos_labdb_meta_set(db, hb_key, hbv);
            }
            /* cumulative reward score for the viewer's best-worker
             * highlight: rsum/rn = mean reward over this worker's
             * trials */
            {
                char rk[80], sk2[80];
                snprintf(rk, sizeof rk, "worker.%d.rsum", worker_id);
                snprintf(sk2, sizeof sk2, "worker.%d.rn", worker_id);
                char *os = aos_labdb_meta_get(db, rk);
                char *on = aos_labdb_meta_get(db, sk2);
                double rsum = (os ? atof(os) : 0.0) + reward;
                long rn = (on ? atol(on) : 0) + 1;
                char bv[48];
                snprintf(bv, sizeof bv, "%.4f", rsum);
                aos_labdb_meta_set(db, rk, bv);
                snprintf(bv, sizeof bv, "%ld", rn);
                aos_labdb_meta_set(db, sk2, bv);
                free(os); free(on);
            }
            /* fleet culling: stop when clearly outclassed so the
             * slot can be reused by a better candidate */
            if (fleet_cull_check(db, worker_id, reward_ema,
                                 trials_done)) {
                aos_labdb_meta_set(db, phase_key, "idle");
                break;
            }
            /* stuck detection: long streak of zero-reward fails on the
             * same scene means the worker is spinning wheels */
            reward_ema = reward_ema * 0.92 + reward * 0.08;
            if (reward <= 0.0 && scene_hash == last_scene) ++stuck_streak;
            else if (reward > 0.0) { stuck_streak = 0; last_scene = scene_hash; }
            else last_scene = scene_hash;
            if (stuck_streak >= 400) {
                /* barren streak: hop to the best branch once before
                 * giving up — a fresh frontier beats spinning wheels */
                double bv = 0.0;
                int64_t bs = aos_labdb_elo_best(db, &bv);
                if (bs > 0 && state_dead(db, bs)) bs = -1;
                if (bs > 0 && bs != state_id && branch_hops < 24) {
                    size_t nsz = 0;
                    uint8_t *nb = aos_labdb_state_blob(db, bs, &nsz);
                    if (nb) {
                        free(state_blob);
                        state_blob = nb;
                        blob_size = nsz;
                        state_id = bs;
                        ++branch_hops;
                        stuck_streak = 0;
                        last_scene = 0;
                        char st[96];
                        snprintf(st, sizeof st,
                                 "stuck->branch state %lld (v=%.2f)",
                                 (long long)bs, bv);
                        aos_labdb_meta_set(db, status_key, st);
                        printf("worker %d %s\n", worker_id, st);
                        fflush(stdout);
                        continue;
                    }
                }
                aos_labdb_meta_set(db, phase_key, "stuck");
                aos_labdb_meta_set(db, status_key,
                                   "stuck: 400 barren trials");
                printf("worker %d stuck after %d barren trials — exiting\n",
                       worker_id, stuck_streak);
                break;
            }
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
    aos_labdb_free_state_rows(srows, scount);
    aos_labdb_free_action_rows(all, acount);
    fb_detach(&fb);
    aos_mgba_close(session);
    aos_labdb_close(db);
    return 0;
}
