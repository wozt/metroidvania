#include "aos_lab_vision.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(uint64_t h, uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        h ^= (v >> (i * 8)) & 0xFF;
        h *= 1099511628211ULL;
    }
    return h;
}

static uint32_t quantize(uint32_t c) {
    /* 5-6-5 -> pack into a single u32 tile color id */
    unsigned r = c & 0xFF, g = (c >> 8) & 0xFF, b = (c >> 16) & 0xFF;
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

static double lum(uint32_t c) {
    return 0.299 * (c & 0xFF) + 0.587 * ((c >> 8) & 0xFF) +
           0.114 * ((c >> 16) & 0xFF);
}

void aos_vision_analyze(const uint32_t *fb, int w, int h,
                        AosSceneFeat *out) {
    memset(out, 0, sizeof *out);
    if (!fb || w <= 0 || h <= 0) return;

    uint64_t scene = 1469598103934665603ULL;
    uint64_t hud = 1469598103934665603ULL;
    int tw = w / 8, th = h / 8;

    /* seen-tile bitmap over 5-6-5 space (32*64*32 = 65536) */
    static uint8_t seen[8192];
    memset(seen, 0, sizeof seen);
    int distinct = 0;
    double lum_sum = 0.0;

    /* per 8px row-band edge density for text detection */
    double band_edge[64];
    int band_n[64];
    int nb = h / 8;
    if (nb > 64) nb = 64;
    memset(band_edge, 0, sizeof band_edge);
    memset(band_n, 0, sizeof band_n);

    for (int ty = 0; ty < th; ++ty) {
        for (int tx = 0; tx < tw; ++tx) {
            /* average color of the 8x8 cell */
            unsigned sr = 0, sg = 0, sb = 0, n = 0;
            double edge = 0.0;
            for (int y = 0; y < 8; ++y) {
                const uint32_t *row = fb + (ty * 8 + y) * w + tx * 8;
                for (int x = 0; x < 8; ++x) {
                    uint32_t c = row[x];
                    sr += c & 0xFF;
                    sg += (c >> 8) & 0xFF;
                    sb += (c >> 16) & 0xFF;
                    ++n;
                    if (x && labs((int)lum(row[x]) - (int)lum(row[x - 1])) > 60)
                        edge += 1.0;
                }
            }
            uint32_t avg = ((sr / n) << 16) | ((sg / n) << 8) | (sb / n);
            uint32_t q = quantize(avg);
            scene = fnv1a(scene, q);
            if (!(seen[q >> 3] & (1 << (q & 7)))) {
                seen[q >> 3] |= (uint8_t)(1 << (q & 7));
                ++distinct;
            }
            if (ty < nb) {
                band_edge[ty] += edge / (8.0 * 8.0);
                band_n[ty] += 1;
            }
            lum_sum += lum(avg);
        }
    }

    /* HUD strips: top 24 rows + bottom 16 rows */
    for (int y = 0; y < h && y < 24; ++y)
        for (int x = 0; x < w; x += 4)
            hud = fnv1a(hud, quantize(fb[y * w + x]));
    for (int y = h - 16; y < h; ++y)
        for (int x = 0; x < w; x += 4)
            hud = fnv1a(hud, quantize(fb[y * w + x]));

    /* ink rows: bands whose edge density is in the upper half and
     * whose mean brightness is mid-range (text/dialogue signature) */
    double mean_edge = 0.0;
    int bands = 0;
    for (int i = 0; i < nb; ++i)
        if (band_n[i]) { mean_edge += band_edge[i] / band_n[i]; ++bands; }
    mean_edge = bands ? mean_edge / bands : 0.0;
    int ink = 0;
    for (int i = 0; i < nb; ++i) {
        if (!band_n[i]) continue;
        double e = band_edge[i] / band_n[i];
        if (e > mean_edge * 1.5 && e > 0.05) ++ink;
    }

    /* coarse color histogram over the whole frame */
    for (int y = 0; y < h; y += 2)
        for (int x = 0; x < w; x += 2) {
            uint32_t c = fb[y * w + x];
            int ri = (c & 0xFF) * 6 / 256;
            int gi = ((c >> 8) & 0xFF) * 6 / 256;
            int bi = ((c >> 16) & 0xFF) * 6 / 256;
            ++out->hist[ri * 36 + gi * 6 + bi];
        }

    out->scene_hash = scene;
    out->hud_hash = hud;
    out->ink_rows = ink;
    out->distinct_tiles = distinct;
    out->brightness = tw * th ? lum_sum / (tw * th) : 0.0;
}

void aos_vision_metrics_append(char *metrics, size_t size,
                               const AosSceneFeat *feat) {
    if (!metrics || !feat) return;
    size_t len = strlen(metrics);
    if (len == 0 || metrics[len - 1] != '}') return;
    char add[160];
    snprintf(add, sizeof add,
             ",\"scene\":\"%016llx\",\"hud\":\"%016llx\",\"ink\":%d,"
             "\"tiles\":%d,\"bright\":%.0f}",
             (unsigned long long)feat->scene_hash,
             (unsigned long long)feat->hud_hash,
             feat->ink_rows, feat->distinct_tiles, feat->brightness);
    /* replace trailing '}' with the additions */
    size_t add_len = strlen(add);
    if (len + add_len + 1 <= size) {
        metrics[len - 1] = 0;
        strncat(metrics, add, size - len);
    }
}

/* ---- BG tilemap signature ------------------------------------------
 * Reads the emulated IO registers (DISPCNT + BGCNT0..3, 16 bytes from
 * 0x04000000) and the 96 KiB VRAM window, then hashes the 32x32 tile
 * index grid of every enabled text-mode background.  This is the first
 * OCR brick: it tells the lab *which tiles* (glyphs, sprites-in-BG,
 * HUD elements) occupy the screen without decoding them yet. */
static uint16_t vrd16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

uint64_t aos_vision_tilemap(const uint16_t *regs,
                            const uint8_t *vram, size_t vram_size,
                            char *out, size_t out_size) {
    if (!regs || !vram || vram_size < 0x10000 ||
        !out || out_size < 8) {
        if (out && out_size) out[0] = 0;
        return 0;
    }
    /* regs[0]=DISPCNT, regs[1..4]=BGCNT0..3 */
    uint16_t dispcnt = regs[0];
    int mode = dispcnt & 7;
    int enabled = (dispcnt >> 8) & 0xF;
    uint64_t all = 1469598103934665603ULL;
    size_t ol = 0;
    out[0] = 0;
    int txt_rows_total = 0, tile_total = 0;

    for (int bg = 0; bg < 4; ++bg) {
        int is_text = (mode == 0) || (mode == 1 && bg < 2);
        if (!is_text || !(enabled & (1 << bg))) continue;
        uint16_t bgcnt = regs[1 + bg];
        uint32_t screen = ((bgcnt >> 8) & 0x1F) * 0x800;
        if (screen + 0x800 > vram_size) continue;

        uint64_t h = 1469598103934665603ULL;
        uint16_t seen[1024];
        int nseen = 0;
        int txt_rows = 0;
        for (int ty = 0; ty < 32; ++ty) {
            int row_tiles = 0;
            uint16_t row_idx[32];
            for (int tx = 0; tx < 32; ++tx) {
                uint16_t e = vrd16(vram + screen +
                                   (ty * 32 + tx) * 2);
                uint16_t tile = e & 0x3FF;
                uint16_t cell = (uint16_t)(tile |
                                           ((e >> 10) & 3) << 10);
                h ^= cell; h *= 1099511628211ULL;
                all ^= cell; all *= 1099511628211ULL;
                row_idx[tx] = tile;
                if (tile) {
                    ++tile_total;
                    int dup = 0;
                    for (int k = 0; k < nseen; ++k)
                        if (seen[k] == tile) { dup = 1; break; }
                    if (!dup && nseen < 1024) seen[nseen++] = tile;
                    ++row_tiles;
                }
            }
            /* a row with many distinct nonzero tiles smells like a
             * line of text or a dense sprite strip */
            int distinct = 0;
            for (int i = 0; i < 32; ++i) {
                int dup = 0;
                for (int j = 0; j < i; ++j)
                    if (row_idx[j] == row_idx[i]) { dup = 1; break; }
                if (row_idx[i] && !dup) ++distinct;
            }
            if (distinct >= 5) ++txt_rows;
        }
        txt_rows_total += txt_rows;
        int w = snprintf(out + ol, out_size - ol,
                         "bg%d:%016llx,", bg,
                         (unsigned long long)h);
        if (w > 0 && (size_t)w < out_size - ol) ol += (size_t)w;
    }
    int w = snprintf(out + ol, out_size - ol,
                     "txtrows:%d,tileuses:%d", txt_rows_total, tile_total);
    (void)w;
    return all;
}

/* Motion between two frames: fraction of central-screen pixels whose
 * RGB changed meaningfully, plus a left/right direction signal.
 * Robust to entity layout — pure pixels. */
double aos_vision_motion(const uint32_t *a, const uint32_t *b,
                         int w, int h, double *dir_out) {
    if (!a || !b || w <= 0 || h <= 0) return 0.0;
    /* central band where the player lives */
    int x0 = w / 5, x1 = w - w / 5;
    int y0 = h / 4, y1 = h - h / 5;
    long changed = 0, total = 0, dl = 0, dr = 0;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            uint32_t pa = a[y * w + x], pb = b[y * w + x];
            int dr_ = abs((int)(pa & 0xFF) - (int)(pb & 0xFF));
            int dg = abs((int)((pa >> 8) & 0xFF) -
                         (int)((pb >> 8) & 0xFF));
            int db = abs((int)((pa >> 16) & 0xFF) -
                         (int)((pb >> 16) & 0xFF));
            if (dr_ + dg + db > 60) {
                ++changed;
                if (x < (x0 + x1) / 2) ++dl; else ++dr;
            }
            ++total;
        }
    }
    if (dir_out) *dir_out = dr + dl ? (double)(dr - dl) / (dr + dl)
                                    : 0.0;
    return total ? (double)changed / (double)total : 0.0;
}
