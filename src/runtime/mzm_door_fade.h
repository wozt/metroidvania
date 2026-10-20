/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission's door fades (color_fading.c): COLOR_FADING_DOOR_TRANSITION
 * for a hatch (the room fades to black but the hatches' palette row, the
 * door transition tilemap fades in on BG3 over BG1, slides to the arrival
 * door once the room is loaded, then the room fades in) and
 * COLOR_FADING_NO_TRANSITION for the other doors (a quick fade to black and
 * back). The state machine only: the runtime applies the palette fades
 * (ApplySpecialBackgroundFadingColor on the masked rows), the shown
 * backgrounds and the BG3 offsets it gives. */
#ifndef MZM_DOOR_FADE_H
#define MZM_DOOR_FADE_H

#include <stdbool.h>
#include <stdint.h>

/* The game's state during a fade (gSubGameMode1). */
enum { MZM_DOOR_FADE_NONE, MZM_DOOR_FADE_OUT, MZM_DOOR_FADE_IN };

/* ColorFadingEffect: a hatch's door transition, another door's quick fade,
 * and COLOR_FADING_CANCEL around the pause screen (the same quick fade on
 * every OBJ row, Samus's and the HUD's included). */
enum { MZM_FADE_DOOR_TRANSITION, MZM_FADE_NO_TRANSITION, MZM_FADE_CANCEL };

typedef struct {
    uint8_t phase;                  /* MZM_DOOR_FADE_*: LOADING_ROOM, DOOR_TRANSITION */
    bool hatch;                     /* COLOR_FADING_DOOR_TRANSITION, else NO_TRANSITION */
    uint8_t stage, fade_timer, unk_3;   /* gColorFading */
    /* The masked palette rows' fade (MZM_FADING_* of mzm_screen.h, 0-32),
     * none while `faded` is false. */
    bool faded;
    uint8_t fading_type, color;
    uint16_t bg_mask, obj_mask;
    /* gBackgroundPositions.doorTransition (BG3 offsets in pixels) and
     * gDoorPositionStart once used as the slide's target. */
    int door_x, door_y, target_x, target_y;
    /* Shown layers and drawing: BG3 holds the door transition tilemap, the
     * medium priority sprites are hidden (gDisableDrawingSprites), BG3 is
     * alpha blended over BG1 (`eva`, `evb`). */
    bool show_bg[4], door_bg3, sprites_hidden, blend;
    /* sDoorTransitionPal in BG rows 1 and 2 (from the fade out's end to the
     * room load). */
    bool door_palette;
    /* BG3 at priority 0 over the whole screen, Samus included: the door
     * transition tilemap (`door_bg3`) or ColorFadingFinishDoorFade's black
     * tiles (`cover` alone). */
    bool cover;
    uint8_t eva, evb;
    /* ColorFadingProcess_BeforeBlueShip instead of _Default (the same fade
     * with one more stage), and its gAnimatedGraphicsEntry.palette =
     * ANIMATED_PALETTE_ID_NONE, for the runtime to apply. */
    bool before_blue_ship, stop_animated_palette;
    /* ColorFadingProcess_ChozodiaEscape: no colour fade at all. */
    bool chozodia_escape;
} MzmDoorFade;

/* ConnectionCheckEnterDoor: a door's fade. `door_x` and `door_y` are the
 * BG3 offsets that put the door transition tilemap over the hatch (the
 * runtime computes them from the hatch's place on its view, as the native
 * 304 or 560 and its vertical formula do for the GBA screen). */
void mzm_door_fade_start(MzmDoorFade *f, bool hatch, int door_x, int door_y);
/* ColorFadingStart(COLOR_FADING_CANCEL) of ProcessPauseButtonPress. */
void mzm_door_fade_start_cancel(MzmDoorFade *f);
/* ColorFadingStart(COLOR_FADING_SAMUS_IN_BLUE_SHIP): the CANCEL fade's
 * colours (colour set 0, COLOR_FADING_FUNCTION_2, to black) through
 * ColorFadingProcess_BeforeBlueShip. */
void mzm_door_fade_start_before_blue_ship(MzmDoorFade *f);
/* ColorFadingStart(COLOR_FADING_CHOZODIA_ESCAPE): the screen cut to black
 * at once (the backgrounds and objects off, the backdrop black), then
 * unk_5d09c and the ending save stages; true on the fourth frame. */
void mzm_door_fade_start_chozodia_escape(MzmDoorFade *f);
/* ColorFadingProcess, once a frame; true once the room is to be loaded. */
bool mzm_door_fade_out(MzmDoorFade *f);
/* RoomLoad's ColorFadingTransferPaletteOnTransition, with the arrival
 * door's BG3 offsets (gDoorPositionStart of the new room). */
void mzm_door_fade_loaded(MzmDoorFade *f, int target_x, int target_y);
/* ColorFadingFinishDoorTransition, once a frame; true when it ended. */
bool mzm_door_fade_in(MzmDoorFade *f);

#endif
