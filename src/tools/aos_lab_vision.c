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
