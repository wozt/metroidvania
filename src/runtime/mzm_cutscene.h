/* SPDX-License-Identifier: GPL-3.0-only */
/* The Zero Mission non-gameplay cutscenes (cutscenes/cutscene_utils.c):
 * CutsceneHandler's stages, CutsceneInit, the special effects' blending
 * fades, the backgrounds' positions and CutsceneVBlank, with the story text
 * cutscenes (cutscenes/story_text_cutscene.c, English text): Samus's
 * monologues drawn line by line with the native font, faded in by alpha
 * blending, dismissed with A or B. The intro, mothership and post-Tourian
 * fullscreen stories are ported with their pre-cutscene palette fades
 * skipped; the other stories are not. Of the gameplay cutscenes, only
 * Samus in the blue ship (cutscenes/samus_in_blue_ship.c) is ported, with
 * the cutscene palette fades (CutsceneStartBackgroundFading,
 * CutsceneUpdateFading) and objects (ProcessCutsceneOam). */
#ifndef MZM_CUTSCENE_H
#define MZM_CUTSCENE_H

#include <stdbool.h>
#include <stdint.h>

#include "mzm_screen.h"
#include "mzm_text.h"

/* Cutscene (constants/cutscene.h) */
enum {
    MZM_CUTSCENE_NONE = 0, MZM_CUTSCENE_INTRO_TEXT = 1,
    MZM_CUTSCENE_MOTHERSHIP_MONOLOGUE = 2, MZM_CUTSCENE_COULD_I_SURVIVE = 3,
    MZM_CUTSCENE_SAMUS_IN_BLUE_SHIP = 14
};

/* struct CutsceneSpecialEffect, the parts the ported cutscenes use */
typedef struct {
    uint8_t status;
    uint16_t bg_bldcnt;
    uint8_t bg_alpha_l, bg_alpha_h, bg_intensity, bg_interval, bg_timer;
} MzmCutsceneEffect;

/* struct CutsceneOamData: the menu object part and the cutscene's own
 * counters. */
typedef struct {
    MzmMenuOam oam;
    uint8_t actions;
    uint16_t timer, unk_1a;
    uint8_t unk_1e;
} MzmCutsceneOam;

typedef struct {
    uint8_t cutscene;               /* gCurrentCutscene */
    uint8_t game_mode_stage;        /* gSubGameModeStage (CUTSCENE_STAGE_*) */
    uint8_t stage, sub_stage;       /* CUTSCENE_DATA.timeInfo */
    uint16_t timer;
    uint16_t dispcnt, bldcnt, bgcnt[4];
    uint16_t bg_hofs[4], bg_vofs[4];   /* CUTSCENE_DATA.bg0hofs...: pixels */
    MzmCutsceneEffect effect;
    MzmMessage message;             /* gCurrentMessage */
    /* CUTSCENE_DATA.fading* (CutsceneStartBackgroundFading) */
    uint8_t fading_color, fading_stage, fading_intensity, fading_max_delay, fading_type, fading_delay;
    bool fading_ready;
    MzmCutsceneOam oam[1];          /* CUTSCENE_DATA.oam */
    struct { bool active; uint16_t timer; } graphics[1];   /* CUTSCENE_DATA.graphicsData */
} MzmCutscene;

/* gCurrentCutscene = cutscene, gSubGameModeStage = CUTSCENE_STAGE_STARTING;
 * false for a cutscene that is not ported. */
bool mzm_cutscene_enter(MzmCutscene *c, uint8_t cutscene);
/* CutsceneHandler then CutsceneVBlank for one frame; true once it ended. */
bool mzm_cutscene_update(MzmCutscene *c, MzmScreen *s);

#endif
