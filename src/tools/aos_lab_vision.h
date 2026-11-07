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

/* BG tilemap signature: hashes each enabled text BG's 32x32 tile-index
 * grid (VRAM screen blocks) from DISPCNT+BGCNT0..3 (`regs`, as
 * snapshot by the session at run end) and the VRAM window.
 * Appends "bgN:hash,..." into `out`; returns combined hash. */
uint64_t aos_vision_tilemap(const uint16_t *regs,
                            const uint8_t *vram, size_t vram_size,
                            char *out, size_t out_size);

/* Fraction of central-band pixels that changed between two frames,
 * plus optional direction (-1 left .. +1 right). */
double aos_vision_motion(const uint32_t *a, const uint32_t *b,
                         int w, int h, double *dir_out);

/* Dump every tile used by enabled text BGs as 8x8 PGM bitmaps plus a
 * per-BG ASCII tile-index map. Returns tiles written. */
int aos_vision_glyph_dump(const uint16_t *regs,
                          const uint8_t *vram, size_t vram_size,
                          const char *dir);
