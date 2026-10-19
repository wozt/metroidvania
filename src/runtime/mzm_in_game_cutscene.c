/* SPDX-License-Identifier: GPL-3.0-only */
/* The suit upgrade in-game cutscene (see mzm_in_game_cutscene.h); comments
 * name the pinned decompilation's routines. */
#include "mzm_in_game_cutscene.h"

#include <string.h>

#include "mzm_sfx.h"
#include "mzm_sound_ids.h"

enum { SCREEN_W = 240, SCREEN_H = 160, BLDALPHA_MAX = 16 };

void mzm_upgrading_suit_start(MzmUpgradingSuit *c, uint8_t item) {
    /* InGameCutsceneStart: only when no cutscene is set */
    if (c->queued) return;
    memset(c, 0, sizeof *c);
    c->queued = true;
    c->item = item;
    c->left = 0;
    c->right = SCREEN_W;
    c->top = 0;
    c->bottom = SCREEN_H;
}

uint16_t mzm_suit_flash_offset(uint8_t intensity, uint8_t start, uint8_t end) {
    uint8_t new_start = 0, new_end = 0;
    if (start > end) {
        if (end > start - intensity) new_start = (uint8_t)(end - (start - intensity));
        new_end = intensity;
    } else if (start < end) {
        if (start > end - intensity) new_end = (uint8_t)(start - (end - intensity));
        new_start = intensity;
    } else {
        new_start = new_end = intensity;
    }
    return (uint16_t)(new_end << 8 | new_start);
}

/* One step of a BLDALPHA coefficient towards its target. */
static int step_coefficient(uint8_t *value, uint8_t target) {
    if (*value == target) return 1;
    *value = (uint8_t)(*value < target ? *value + 1 : *value - 1);
    return 0;
}

/* InGameCutsceneUpgradingSuit: true on stage 19 (control given back). */
static bool upgrading_suit(MzmUpgradingSuit *c, const MzmUpgradingSuitInput *in, MzmEquipment *equipment,
                           uint8_t *ruins_stage) {
    int change = 0, temp, left, right, top, bottom;
    bool released = false;
    /* The window's smallest rectangle: 2 pixels at Samus's centre */
    temp = in->samus_x - in->bg1_x;
    left = temp < 0 ? 0 : temp > 0x3B8 ? 0xEE : temp >> 2;
    right = left + 2;
    temp = in->samus_y - 0x60 - in->bg1_y;
    top = temp < 0 ? 0 : temp > 0x278 ? 0x9E : temp >> 2;
    bottom = top + 2;
    if (c->flash_timer != 0xFF) c->flash_timer++;

    switch (c->stage) {
    case 0:
        if (c->item == MZM_ACQUISITION_VARIA ||
            (c->item == MZM_ACQUISITION_GRAVITY && ruins_stage &&
             *ruins_stage == MZM_RUINS_TEST_STARTING_SUIT_ANIM))
            change = 1;
        break;
    case 1:
        c->transparency_held = true;
        c->bldalpha_h = c->evb_backup;
        c->bldalpha_l = c->eva_backup;
        change = 1;
        break;
    case 2:
        /* BG0 faded out, a step every other frame */
        if (c->timer & 1) break;
        if (c->bldalpha_h != BLDALPHA_MAX) {
            if (++c->bldalpha_h > BLDALPHA_MAX) c->bldalpha_h = BLDALPHA_MAX;
        } else {
            change++;
        }
        if (c->bldalpha_l != 0) c->bldalpha_l--;
        else change++;
        change >>= 1;
        break;
    case 3:
        /* BG0 hidden, its tile map backed up */
        c->bg0_hidden = true;
        change = 1;
        break;
    case 4:
        /* BG0 filled with tile 0xC0 of palette 9 */
    case 5:
    case 6:
        change = 1;
        break;
    case 7:
        c->bg0_hidden = false;
        c->flash = true;
        c->left = (uint8_t)left;
        c->right = (uint8_t)right;
        c->top = (uint8_t)top;
        c->bottom = (uint8_t)bottom;
        c->flash_timer = 0;
        if (c->item == MZM_ACQUISITION_VARIA) mzm_sfx_play(MZM_SOUND_ACQUIRING_VARIA);
        else if (c->item == MZM_ACQUISITION_GRAVITY) mzm_sfx_play(MZM_SOUND_ACQUIRING_GRAVITY);
        change = 1;
        break;
    case 8: {
        /* The flash growing vertically */
        int increment = SCREEN_H / 40;
        if (c->top != 0) c->top = (uint8_t)(c->top > increment ? c->top - increment : 0);
        else change++;
        if (c->bottom < SCREEN_H) {
            c->bottom = (uint8_t)(c->bottom + increment);
            if (c->bottom > SCREEN_H) c->bottom = SCREEN_H;
        } else {
            change++;
        }
        change >>= 1;
        break;
    }
    case 9: {
        /* Then horizontally */
        int increment = SCREEN_W / 30 - 1;
        if (c->left != 0) c->left = (uint8_t)(c->left > increment ? c->left - increment : 0);
        else change++;
        if (c->right < SCREEN_W) {
            c->right = (uint8_t)(c->right + increment);
            if (c->right > SCREEN_W) c->right = SCREEN_W;
        } else {
            change++;
        }
        change >>= 1;
        break;
    }
    case 10:
    case 12:
        if (c->timer > 5) change = 1;                   /* CONVERT_SECONDS(1.f / 12) */
        break;
    case 11:
        /* The suit activated (its graphics follow) */
        if (c->item == MZM_ACQUISITION_VARIA) {
            equipment->items |= MZM_ITEM_VARIA_SUIT;
            equipment->unknown_items &= ~(uint32_t)MZM_ITEM_VARIA_SUIT;
        } else if (c->item == MZM_ACQUISITION_GRAVITY) {
            equipment->items |= MZM_ITEM_GRAVITY_SUIT;
            equipment->unknown_items &= ~(uint32_t)MZM_ITEM_GRAVITY_SUIT;
        }
        change = 1;
        break;
    case 13: {
        /* The flash shrinking vertically back to Samus */
        uint16_t offset = mzm_suit_flash_offset(3, (uint8_t)(top - c->top), (uint8_t)(c->bottom - bottom));
        uint8_t new_start = (uint8_t)offset, new_end = (uint8_t)(offset >> 8);
        if (c->top < top) {
            c->top = (uint8_t)(c->top + new_end);
            if (c->top > top) c->top = (uint8_t)top;
        } else {
            change++;
        }
        if (c->bottom > bottom) {
            c->bottom = (uint8_t)(c->bottom - new_start);
            if (c->bottom < bottom) c->bottom = (uint8_t)bottom;
        } else {
            change++;
        }
        change >>= 1;
        break;
    }
    case 14: {
        /* Then horizontally */
        uint16_t offset = mzm_suit_flash_offset(9, (uint8_t)(left - c->left), (uint8_t)(c->right - right));
        uint8_t new_start = (uint8_t)offset, new_end = (uint8_t)(offset >> 8);
        if (c->left < left) {
            c->left = (uint8_t)(c->left + new_end);
            if (c->left > left) c->left = (uint8_t)left;
        } else {
            change++;
        }
        if (c->right > right) {
            c->right = (uint8_t)(c->right - new_start);
            if (c->right < right) c->right = (uint8_t)right;
        } else {
            change++;
        }
        change >>= 1;
        break;
    }
    case 15:
        /* The windows and blending restored, BG0 still hidden */
        c->bg0_hidden = true;
        c->flash = false;
        c->left = 0;
        c->right = SCREEN_W;
        c->top = 0;
        c->bottom = SCREEN_H;
        change = 1;
        break;
    case 16:
        /* BG0's tile map and control restored */
        change = 1;
        break;
    case 17:
        c->bg0_hidden = false;
        change = 1;
        break;
    case 18:
        /* BG0 faded back in */
        if (c->timer & 1) break;
        change = step_coefficient(&c->bldalpha_h, c->evb_backup);
        change += step_coefficient(&c->bldalpha_l, c->eva_backup);
        change >>= 1;
        break;
    case 19:
        if (c->item == MZM_ACQUISITION_GRAVITY && ruins_stage) *ruins_stage = MZM_RUINS_TEST_SUIT_ANIM_ENDED;
        c->transparency_held = false;
        released = true;
        break;
    default:
        break;
    }
    if (change) {
        c->stage++;
        c->timer = 0;
    }
    /* Stage 19 returns IGC_RESULT_NEXT_STAGE (US): the handler keeps being
     * called on stage 20, where nothing happens. */
    if (released) {
        c->timer = 0;
        c->stage++;
    }
    return released;
}

bool mzm_upgrading_suit_process(MzmUpgradingSuit *c, const MzmUpgradingSuitInput *in,
                                MzmEquipment *equipment, uint8_t *ruins_stage) {
    c->timer++;
    if (!c->queued) return false;
    if (!c->started) {
        /* InGameCutsceneInit: waits for no power bomb */
        if (in->power_bomb) return false;
        c->stage = 0;
        c->timer = 0;
        c->started = true;
        c->eva_backup = in->eva;
        c->evb_backup = in->evb;
        return false;
    }
    return upgrading_suit(c, in, equipment, ruins_stage);
}
