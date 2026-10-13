/* SPDX-License-Identifier: GPL-3.0-only */
/* The door fades (see mzm_door_fade.h); comments name the pinned
 * decompilation's routines. */
#include "mzm_door_fade.h"

#include <string.h>

#include "mzm_screen.h"

enum { BLDALPHA_MAX = 16 };

/* sColorFading_345f58 (COLOR_FADING_SPEED_FASTER, NO_TRANSITION) and
 * sColorFading_345f60 (COLOR_FADING_SPEED_FAST, DOOR_TRANSITION) */
static const uint8_t faster[8] = {3, 7, 11, 15, 19, 23, 27, 32};
static const uint8_t fast[16] = {2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32};

/* ColorFadingUpdate with COLOR_FADING_FUNCTION_1: one color of the speed's
 * array a frame (out: towards black, in: from black), true a frame after
 * the last; ending a fade in restores the palette. */
static bool update(MzmDoorFade *f, bool out) {
    const uint8_t *colors = f->hatch ? fast : faster;
    int size = f->hatch ? 16 : 8;
    if (f->fade_timer < size) {
        f->faded = true;
        f->fading_type = out ? MZM_FADING_OUT : MZM_FADING_IN;
        f->color = colors[f->fade_timer++];
        return false;
    }
    /* COLOR_FADING_STAGE_FINISHED: unk_5b2c4 after a fade in. */
    if (!out) f->faded = false;
    f->fade_timer = 0;
    return true;
}

void mzm_door_fade_start(MzmDoorFade *f, bool hatch, int door_x, int door_y) {
    memset(f, 0, sizeof *f);
    f->phase = MZM_DOOR_FADE_OUT;
    f->hatch = hatch;
    /* sColorFadingData[].bgColorMask / objColorMask */
    f->bg_mask = hatch ? 0xFFFD : 0xFFFF;
    f->obj_mask = 0xFFC7;
    f->door_x = door_x;
    f->door_y = door_y;
    for (int bg = 0; bg < 4; ++bg) f->show_bg[bg] = true;
}

/* ColorFadingProcess_DoorTransition */
static bool process_door_transition(MzmDoorFade *f) {
    switch (f->stage) {
    case 0:
        /* unk_5c220 */
        f->unk_3 = 0;
        f->stage++;
        break;
    case 1:
        if (update(f, true)) {
            f->sprites_hidden = true;
            f->stage++;
        }
        break;
    case 2:
        /* The backdrop black, sDoorTransitionPal in rows 1 and 2 (the
         * runtime's `door_bg3` palette), BG2, BG3 and WIN1 off. */
        f->show_bg[2] = f->show_bg[3] = false;
        f->door_palette = true;
        f->stage++;
        break;
    case 3:
        /* The door transition tilemap on BG3, blended over BG1. */
        f->door_bg3 = f->cover = true;
        f->show_bg[3] = true;
        f->show_bg[0] = false;
        f->blend = true;
        f->evb = BLDALPHA_MAX;
        f->eva = 0;
        f->stage = 4;
        f->unk_3 = 0;
        break;
    case 4:
        if (f->evb != 0 || f->eva < BLDALPHA_MAX) {
            f->evb = f->evb >= 2 ? (uint8_t)(f->evb - 2) : 0;
            f->eva = f->eva + 2 <= BLDALPHA_MAX ? (uint8_t)(f->eva + 2) : BLDALPHA_MAX;
        } else {
            f->show_bg[1] = false;
            f->blend = false;
            f->unk_3 = 0;
            f->stage = 5;
        }
        break;
    case 5:
        f->stage = 0;
        return true;
    default:
        break;
    }
    return false;
}

/* ColorFadingProcess_Default and ColorFadingFinishDoorFade */
static bool process_default(MzmDoorFade *f) {
    switch (f->stage) {
    case 0:
        f->stage++;
        break;
    case 1:
        if (update(f, true)) f->stage++;
        break;
    case 2:
        for (int bg = 0; bg < 4; ++bg) f->show_bg[bg] = false;
        f->cover = true;
        f->stage = 0;
        return true;
    default:
        break;
    }
    return false;
}

void mzm_door_fade_start_cancel(MzmDoorFade *f) {
    mzm_door_fade_start(f, false, 0, 0);
    f->obj_mask = 0xFFFF;
}

bool mzm_door_fade_out(MzmDoorFade *f) {
    if (f->phase != MZM_DOOR_FADE_OUT) return false;
    if (f->unk_3 != 0xFF) f->unk_3++;
    return f->hatch ? process_door_transition(f) : process_default(f);
}

void mzm_door_fade_loaded(MzmDoorFade *f, int target_x, int target_y) {
    f->phase = MZM_DOOR_FADE_IN;
    f->stage = 0;
    f->door_palette = false;
    f->fade_timer = 0;
    /* ColorFadingTransferPaletteOnTransition: the masked rows black. */
    f->faded = true;
    f->fading_type = MZM_FADING_IN;
    f->color = 0;
    f->target_x = target_x;
    f->target_y = target_y;
}

/* ColorFadingUpdate_DoorTransition: the tilemap slides to the arrival door,
 * vertically then horizontally, the room comes back and fades in. */
static bool update_door_transition(MzmDoorFade *f) {
    if (f->unk_3 != 0xFF) f->unk_3++;
    switch (f->stage) {
    case 0:
        f->stage++;
        break;
    case 1:
        if (f->target_y > f->door_y) {
            f->door_y += 3;
            if (f->door_y > f->target_y) f->door_y = f->target_y;
        } else if (f->target_y < f->door_y) {
            f->door_y -= 3;
            if (f->door_y < f->target_y) f->door_y = f->target_y;
        } else {
            f->unk_3 = 0;
            f->stage = 2;
        }
        break;
    case 2:
        if (f->unk_3 > 2) {
            f->unk_3 = 0;
            f->stage++;
        }
        break;
    case 3:
        if (f->target_x > f->door_x) {
            f->door_x += 6;
            if (f->target_x < f->door_x) f->door_x = f->target_x;
        } else if (f->target_x < f->door_x) {
            f->door_x -= 6;
            if (f->target_x > f->door_x) f->door_x = f->target_x;
        } else {
            /* The room's backgrounds and BG3 back. */
            for (int bg = 0; bg < 4; ++bg) f->show_bg[bg] = true;
            f->door_bg3 = f->cover = false;
            f->sprites_hidden = false;
            f->unk_3 = 0;
            f->stage = 5;
        }
        break;
    case 5:
        if (update(f, false)) f->stage++;
        break;
    case 6:
        f->stage = 0;
        return true;
    default:
        break;
    }
    return false;
}

/* ColorFadingUpdate_Default */
static bool update_default(MzmDoorFade *f) {
    if (f->stage == 0) {
        /* ColorFadingStartDefault */
        for (int bg = 0; bg < 4; ++bg) f->show_bg[bg] = true;
        f->cover = false;
        f->stage++;
        return false;
    }
    return update(f, false);
}

bool mzm_door_fade_in(MzmDoorFade *f) {
    if (f->phase != MZM_DOOR_FADE_IN) return false;
    if (!(f->hatch ? update_door_transition(f) : update_default(f))) return false;
    f->phase = MZM_DOOR_FADE_NONE;
    f->faded = false;
    return true;
}
