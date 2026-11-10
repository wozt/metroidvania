/* SPDX-License-Identifier: GPL-3.0-only */
/* BackgroundEffectStart, BackgroundEffectUpdate and BackgroundEffectProcess
 * for the three palette effects used by MakeBackgroundFlash. */
#include "mzm_background_effect.h"

#include <stddef.h>
#include <string.h>

enum {
    CMD_WAIT_BEFORE,
    CMD_FIRST_COLOR,
    CMD_SECOND_COLOR,
    CMD_FINISH_AND_KILL,
};

typedef struct {
    uint8_t command, timer, argument;
} Behavior;

#define WAIT(frames) {CMD_WAIT_BEFORE, (frames), 0}
#define FIRST(stage) {CMD_FIRST_COLOR, 0, (stage)}
#define SECOND(stage) {CMD_SECOND_COLOR, 0, (stage)}
#define FINISH {CMD_FINISH_AND_KILL, 0, 0}

/* sBackgroundEffectBehavior_QuickFlash. CONVERT_SECONDS(1/15) is four
 * frames, and CONVERT_SECONDS(1/30) is two. */
static const Behavior quick_flash[] = {
    FIRST(16), WAIT(4), SECOND(16), WAIT(2), FIRST(14), FIRST(12),
    FIRST(8), FIRST(6), FIRST(4), FIRST(2), FIRST(0), FINISH,
};

/* sBackgroundEffectBehavior_SlightYellow. */
static const Behavior slight_yellow[] = {
    WAIT(13), FIRST(3), WAIT(12), FIRST(4), WAIT(12), FIRST(5), WAIT(12),
    FIRST(4), WAIT(12), FIRST(3), WAIT(12), FIRST(4), WAIT(12), FIRST(5),
    WAIT(12), FIRST(4), WAIT(10), FIRST(3), WAIT(10), FIRST(0), FINISH,
};

/* sBackgroundEffectBehavior_HeavyYellow. */
static const Behavior heavy_yellow[] = {
    FIRST(2), WAIT(12), FIRST(4), WAIT(12), FIRST(6), WAIT(12), FIRST(8),
    WAIT(12), FIRST(10), WAIT(12), FIRST(8), WAIT(12), FIRST(6), WAIT(12),
    FIRST(4), WAIT(12), FIRST(2), WAIT(12), FIRST(0), FINISH,
};

MzmBackgroundEffect mzm_background_effect;

static const Behavior *behavior(uint8_t type, size_t *count) {
    switch (type) {
    case MZM_BACKGROUND_EFFECT_SLIGHT_YELLOW:
        *count = sizeof slight_yellow / sizeof slight_yellow[0];
        return slight_yellow;
    case MZM_BACKGROUND_EFFECT_HEAVY_YELLOW:
        *count = sizeof heavy_yellow / sizeof heavy_yellow[0];
        return heavy_yellow;
    case MZM_BACKGROUND_EFFECT_QUICK_FLASH:
        *count = sizeof quick_flash / sizeof quick_flash[0];
        return quick_flash;
    default:
        *count = 0;
        return NULL;
    }
}

bool mzm_background_effect_start(MzmBackgroundEffectType type) {
    size_t count;
    if (mzm_background_effect.type != MZM_BACKGROUND_EFFECT_NONE || !behavior((uint8_t)type, &count))
        return false;
    mzm_background_effect.type = (uint8_t)type;
    mzm_background_effect.color_stage = 0;
    mzm_background_effect.stage = 0;
    mzm_background_effect.timer = 0;
    mzm_background_effect.needs_palette = true;
    return true;
}

static void apply_color(uint16_t palette[256], uint16_t color, uint8_t stage) {
    if (stage == 0) {
        memcpy(palette, mzm_background_effect.palette, sizeof mzm_background_effect.palette);
        return;
    }
    int target_r = color & 31;
    int target_g = color >> 5 & 31;
    int target_b = color >> 10 & 31;
    for (size_t i = 0; i < sizeof mzm_background_effect.palette /
                               sizeof mzm_background_effect.palette[0]; ++i) {
        uint16_t source = mzm_background_effect.palette[i];
        int r = source & 31;
        int g = source >> 5 & 31;
        int b = source >> 10 & 31;
        r += (target_r - r) * stage / 16;
        g += (target_g - g) * stage / 16;
        b += (target_b - b) * stage / 16;
        palette[i] = (uint16_t)(r | g << 5 | b << 10);
    }
}

void mzm_background_effect_update(uint16_t palette[256], bool frozen) {
    MzmBackgroundEffect *effect = &mzm_background_effect;
    if (effect->type == MZM_BACKGROUND_EFFECT_NONE || !palette) return;
    if (effect->needs_palette) {
        memcpy(effect->palette, palette, sizeof effect->palette);
        effect->needs_palette = false;
    }
    if (frozen) return;

    size_t count;
    const Behavior *table = behavior(effect->type, &count);
    if (!table || effect->stage >= count) {
        mzm_background_effect_clear();
        return;
    }
    effect->timer++;
    const Behavior *entry = &table[effect->stage];
    switch (entry->command) {
    case CMD_WAIT_BEFORE:
        /* BACKGROUND_EFFECT_CMD_WAIT_FOR_TIMER_BEFORE compares against the
         * requested time minus DELTA_TIME. */
        if (effect->timer >= (uint16_t)(entry->timer - 1)) {
            effect->stage++;
            effect->timer = 0;
        }
        break;
    case CMD_FIRST_COLOR:
    case CMD_SECOND_COLOR: {
        effect->color_stage = entry->argument;
        effect->stage++;
        effect->timer = 0;
        uint16_t color;
        if (effect->type == MZM_BACKGROUND_EFFECT_QUICK_FLASH)
            color = entry->command == CMD_SECOND_COLOR ? (uint16_t)(31 << 10) : 0x7FFF;
        else
            color = (uint16_t)(22 | 22 << 5);
        apply_color(palette, color, effect->color_stage);
        break;
    }
    case CMD_FINISH_AND_KILL:
        mzm_background_effect_clear();
        break;
    default:
        mzm_background_effect_clear();
        break;
    }
}

void mzm_background_effect_clear(void) {
    memset(&mzm_background_effect, 0, sizeof mzm_background_effect);
}

#undef WAIT
#undef FIRST
#undef SECOND
#undef FINISH
