/*
 * aos_lab_vision.h — visual feature extraction for lab trials.
 *
 * Analyzes the last emulated framebuffer (240x160, 0xAABBGGRR pixels)
 * into compact, storable signatures: tile-grid hash, HUD strip hash,
 * text-ink row count, color histogram and brightness.  The features are
 * stored per trial in the metrics JSON so every learning signal is
 * persisted in the lab database.
 */
#ifndef AOS_LAB_VISION_H
#define AOS_LAB_VISION_H

#include <stddef.h>
#include <stdint.h>

#define AOS_VISION_HIST 216 /* 6x6x6 RGB bins */

typedef struct {
    uint64_t scene_hash;   /* FNV-1a over quantized 8x8 tile colors */
    uint64_t hud_hash;     /* same over top 24 + bottom 16 pixel rows */
    int ink_rows;          /* 8px bands with text-like edge density */
    int distinct_tiles;    /* distinct quantized tile colors on screen */
    double brightness;     /* mean luminance 0..255 */
    uint16_t hist[AOS_VISION_HIST];
} AosSceneFeat;

void aos_vision_analyze(const uint32_t *fb, int w, int h,
                        AosSceneFeat *out);

/* Append scene fields into a metrics JSON object (must end with '}'). */
void aos_vision_metrics_append(char *metrics, size_t size,
                               const AosSceneFeat *feat);

#endif
