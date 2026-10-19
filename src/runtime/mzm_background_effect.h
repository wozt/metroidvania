/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission's palette-based background flashes (animated_graphics.c).
 * MakeBackgroundFlash starts one shared effect; the room update advances one
 * native behavior command a frame and recolors BG palette rows 0 to 14 from
 * the snapshot taken at the start. OBJ palettes and animated BG row 15 are
 * deliberately excluded. */
#ifndef MZM_BACKGROUND_EFFECT_H
#define MZM_BACKGROUND_EFFECT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MZM_BACKGROUND_EFFECT_NONE,
    MZM_BACKGROUND_EFFECT_SLIGHT_YELLOW,
    MZM_BACKGROUND_EFFECT_HEAVY_YELLOW,
    MZM_BACKGROUND_EFFECT_QUICK_FLASH,
} MzmBackgroundEffectType;

typedef struct {
    uint16_t timer;
    uint8_t color_stage, type, stage;
    bool needs_palette;
    uint16_t palette[15 * 16];
} MzmBackgroundEffect;

extern MzmBackgroundEffect mzm_background_effect;

/* BackgroundEffectStart: a request is ignored while another effect is live. */
bool mzm_background_effect_start(MzmBackgroundEffectType type);
/* BackgroundEffectUpdate: `frozen` models transitions and power bombs. The
 * starting palette is still captured while frozen, as the native start call
 * copies it immediately. */
void mzm_background_effect_update(uint16_t palette[256], bool frozen);
/* Room load's empty background effect. */
void mzm_background_effect_clear(void);

#endif
