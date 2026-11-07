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
    RK_COUNT = 8,
    RK_ACTION = 8 /* action probe occupies last two slots */
};

static const struct { uint32_t addr, size; } REWARD_PEEKS[RK_COUNT] = {
    {0x02000010, 4}, {0x02000090, 16}, {0x02000524, 8},
    {0x0201327A, 8}, {0x0201328C, 8}, {0x02013279, 4},
    {0x02013294, 136}, {0x0201331C, 122},
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

/* Returns bonus reward; appends human-readable tags to `tags`. */
static double compute_bonus(AosLabDb *db, const AosRunResult *res,
                            int nreward, int scene_is_new, double motion,
                            char *tags, size_t tags_size) {
    (void)scene_is_new;
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
    /* mode / area / room / song */
    if (rd32(&b[RK_MODE], 0) != rd32(&a[RK_MODE], 0)) {
        bonus += 0.5; TAG("mode_change");
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
    if (hp_a <= 0 && hp_b > 0) { bonus -= 1.0; TAG("death"); }
    else if (hp_a < hp_b) {
        double frac = (double)(hp_b - hp_a) / maxhp;
        bonus -= 0.3 + 0.7 * frac; TAG("damage");
    } else if (hp_a > hp_b) { bonus += 0.4; TAG("healed"); }
    if (mp_a < mp_b) { bonus += 0.1; TAG("ability_used"); }
    /* clean traverse: room changed and hp untouched */
    if (tags[0] && strstr(tags, "new_room") && hp_a == hp_b) {
        bonus += 0.5; TAG("clean_traverse");
    }
    /* xp / gold */
    if (rd32(&b[RK_XP], 0) != rd32(&a[RK_XP], 0)) {
        bonus += 0.5; TAG("exp_gain");
    }
    if (rd32(&b[RK_XP], 4) != rd32(&a[RK_XP], 4)) {
        bonus += 0.3; TAG("gold_gain");
    }
    if (b[RK_LVL].data && a[RK_LVL].data &&
        b[RK_LVL].data[0] != a[RK_LVL].data[0]) {
        bonus += 1.0; TAG("level_up");
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
    /* movement without damage: screen motion is the reliable signal
     * (WRAM entity position proved layout-dependent) */
    if (motion > 0.008 && hp_a == hp_b) {
        bonus += 0.2; TAG("clean_move");
    }
    if (bonus > 2.5) bonus = 2.5;
    if (bonus < -1.5) bonus = -1.5;
#undef TAG
    return bonus;
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
    aos_labdb_begin_run(db, rom, "scout early game");

    uint64_t seen[1024];
    int nseen = 0, validated = 0, resp_streak = 0, stop = 0;
    int last_new_chunk = 0;
    for (int chunk = 0; chunk < max_chunks && !stop; ++chunk) {
        char *ctl = aos_labdb_meta_get(db, ctrl_key);
        if (ctl && !strcmp(ctl, "stop")) stop = 1;
        free(ctl);
        if (stop) break;

        /* phase 1: 520 frames advancing text/menus (A taps, START) */
        AosKeyEvent ev[32];
        int ne = 0;
        ev[ne++] = (AosKeyEvent){0, 0};
        if (chunk % 3 == 0) {
            ev[ne++] = (AosKeyEvent){40, 8};
            ev[ne++] = (AosKeyEvent){55, 0};
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
        uint32_t pulse = (chunk & 1) ? 32 : 16; /* LEFT / RIGHT */
        AosKeyEvent pev[2] = {{0, pulse}, {79, 0}};
        AosPeekRequest pk[1] = {{-1, 0x02000010, 4}}; /* gameMode end */
        AosRunSpec spec2;
        memset(&spec2, 0, sizeof spec2);
        spec2.frames = 80;
        spec2.keys = pev;
        spec2.key_count = 2;
        spec2.peeks = pk;
        spec2.peek_count = 1;
        spec2.fps_cap = spec.fps_cap;
        if (fb_ok) {
            spec2.fb_mirror = fb.pixels;
            spec2.fb_seq = fb.seq;
            spec2.fb_interval = 2;
        }
        AosRunResult res2;
        aos_mgba_run(session, &spec2, &res2);

        int mode = res2.peeks[0].data ? res2.peeks[0].data[0] : -1;
        double mdir = 0.0;
        double motion = res2.fb_first
            ? aos_vision_motion(res2.fb_first,
                                aos_mgba_framebuffer(session),
                                AOS_MGBA_WIDTH, AOS_MGBA_HEIGHT, &mdir)
            : 0.0;
        /* >0.8% of central pixels changed during the 80-frame pulse:
         * walking moves far more, idle animation far less */
        int responsive = motion > 0.008;
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
            size_t ns = 0;
            uint8_t *blob = aos_mgba_save_state_blob(session, &ns);
            if (blob) {
                char nm[96], note[160];
                snprintf(nm, sizeof nm, "scout_c%03d_m%d", chunk, mode);
                snprintf(note, sizeof note,
                         "scene=%016llx mode=%d chunk=%d",
                         (unsigned long long)feat.scene_hash, mode, chunk);
                aos_labdb_add_state(db, nm, note, 0, blob, ns);
                free(blob);
            }
        }
        resp_streak = (responsive && mode == 4) ? resp_streak + 1 : 0;
        if (resp_streak >= 2) {
            /* validated free-gameplay checkpoint: two consecutive
             * responsive chunks rule out cutscene drift */
            ++validated;
            size_t ns = 0;
            uint8_t *blob = aos_mgba_save_state_blob(session, &ns);
            if (blob) {
                char nm[96], note[160];
                snprintf(nm, sizeof nm, "scout_play_%03d", chunk);
                snprintf(note, sizeof note,
                         "responsive pulse motion=%.4f scene=%016llx",
                         motion,
                         (unsigned long long)feat.scene_hash);
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
                 "chunk %d mode=%d motion=%.3f resp=%d states=%d",
                 chunk, mode, motion, responsive, nseen);
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
    static AosActionRow builtin_actions[4];
    if (chosen_count == 0) {
        /* built-in curriculum from the verified WRAM map so a fresh
         * database still learns something meaningful */
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
        for (int i = 0; i < 4; ++i) {
            builtin_actions[i].id = -(i + 1);
            builtin_actions[i].name = strdup(defs[i].n);
            builtin_actions[i].kind = strdup(defs[i].k);
            builtin_actions[i].params = strdup(defs[i].pa);
            builtin_actions[i].eval = strdup(defs[i].ev);
            chosen[chosen_count++] = builtin_actions[i];
        }
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

    int64_t run_id = aos_labdb_begin_run(db, rom, "worker batch");
    aos_labdb_meta_set(db, phase_key, "running");
    char run_buf[32];
    snprintf(run_buf, sizeof run_buf, "%lld", (long long)run_id);
    aos_labdb_meta_set(db, run_key, run_buf);
    int total_frames = 0, trials_done = 0;
    int stop_requested = 0;
    int stuck_streak = 0;
    uint64_t last_scene = 0;
    struct timeval t0, t1;

    /* bandit over actions: weights table gives each (action,params)
     * a success rate; 1-explore picks the best action, explore picks
     * a random one. Total budget = iterations trials. */
    int total_trials = iterations;
    uint64_t seen_scenes[512];
    int seen_count = 0;
    for (int t = 0; t < total_trials && !stop_requested; ++t) {
        {
            AosActionRow *action;
            if (chosen_count == 1 ||
                ((double)rand() / RAND_MAX) < explore) {
                action = &chosen[rand() % chosen_count];
            } else {
                /* pick randomly among the actions tied at the best
                 * observed average reward (success rate + exploration
                 * bonuses) */
                int wcount = 0;
                AosWeightRow *w = aos_labdb_weights(db, 8192, &wcount);
                double best_rate = -1.0;
                int rates[64];
                for (int ci = 0; ci < chosen_count && ci < 64; ++ci) {
                    double rate = -1;
                    for (int wi = 0; wi < wcount; ++wi)
                        if (w[wi].action_id == chosen[ci].id &&
                            w[wi].attempts > 0)
                            rate = w[wi].reward_sum / w[wi].attempts;
                    rates[ci] = rate > best_rate ? (int)(rate * 1000000) : -1;
                    if (rate > best_rate) best_rate = rate;
                }
                int ties[64], nt = 0;
                for (int ci = 0; ci < chosen_count && ci < 64; ++ci)
                    if (best_rate >= 0 &&
                        rates[ci] == (int)(best_rate * 1000000))
                        ties[nt++] = ci;
                if (nt > 0)
                    action = &chosen[ties[rand() % nt]];
                else
                    action = &chosen[rand() % chosen_count];
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

            /* OCR brick: post-run snapshot of the text-BG tilemaps.
             * IO regs (DISPCNT+BGCNT) at 0x04000000, VRAM window at
             * 0x06000000 — the vision module hashes each enabled BG's
             * 32x32 tile grid into the metrics JSON. */
            if (peek_count < 2 * RK_COUNT + 3) {
                peeks[peek_count].frame = -1;
                peeks[peek_count].address = 0x06000000;
                peeks[peek_count].size = 0x18000;
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
                AosStateRow *pick = &srows[rand() % scount];
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
            aos_mgba_run(session, &spec, &res);
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
                 * plus the session's video-register snapshot */
                {
                    int vram_i = -1;
                    for (int pi = 0; pi < res.peek_count; ++pi)
                        if (res.peeks[pi].address == 0x06000000)
                            vram_i = pi;
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
                if (!seen && seen_count < 512) {
                    seen_scenes[seen_count++] = scene_hash;
                    size_t ns = 0;
                    uint8_t *blob = aos_mgba_save_state_blob(session, &ns);
                    if (blob) {
                        char nm[96];
                        snprintf(nm, sizeof nm, "auto_w%d_%08llx",
                                 worker_id,
                                 (unsigned long long)(scene_hash & 0xFFFFFFFF));
                        char note[128];
                        snprintf(note, sizeof note,
                                 "auto scene=%016llx trial=%lld",
                                 (unsigned long long)scene_hash,
                                 (long long)t);
                        int64_t nid = aos_labdb_add_state(
                            db, nm, note, state_id, blob, (int)ns);
                        (void)nid;
                        free(blob);
                    }
                }
            }

            /* exploration bonus on top of the action probe */
            {
                int scene_new = 0;
                for (int si = 0; si < seen_count; ++si)
                    if (seen_scenes[si] == scene_hash) { scene_new = 0; break; }
                /* scene_hash was already registered above when new */
                char btags[256];
                double bonus = compute_bonus(db, &res, RK_COUNT, 0,
                                             trial_motion, btags,
                                             sizeof btags);
                /* novelty tag handled via seen set: recompute flag */
                {
                    /* if it was just added this trial it is 'new' */
                    scene_new = (seen_count > 0 &&
                                 seen_scenes[seen_count - 1] == scene_hash);
                    if (scene_new) {
                        bonus += 0.4;
                        size_t tl = strlen(btags);
                        snprintf(btags + tl, sizeof btags - tl,
                                 "%snew_scene", tl ? "," : "");
                    }
                }
                reward += bonus;
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
            }

            int64_t trial_id = aos_labdb_add_trial(
                db, run_id, used_state_id, action->id, drawn, keys_label,
                frames, outcome, reward, metrics, duration_ms);
            ++trials_done;
            total_frames += frames;
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
            /* stuck detection: long streak of zero-reward fails on the
             * same scene means the worker is spinning wheels */
            if (reward <= 0.0 && scene_hash == last_scene) ++stuck_streak;
            else if (reward > 0.0) { stuck_streak = 0; last_scene = scene_hash; }
            else last_scene = scene_hash;
            if (stuck_streak >= 400) {
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
