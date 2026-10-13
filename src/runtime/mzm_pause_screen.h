/* SPDX-License-Identifier: GPL-3.0-only */
/* The Zero Mission pause screen's map screen (menus/pause_screen.c,
 * pause_screen_map.c, the palette helpers of status_screen.c): the fade
 * in and out, the visor overlays, the current area's map scrolled with the
 * D-pad, Samus's and the boss icons, the area name, the scroll arrows, the
 * world map (A or B), the other viewable areas (SELECT) and START or B to
 * leave. The status screen (R), easy sleep (L), the Chozo hints and their
 * targets, the map download and the completion info are not ported yet. */
#ifndef MZM_PAUSE_SCREEN_H
#define MZM_PAUSE_SCREEN_H

#include <stdbool.h>
#include <stdint.h>

#include "mzm_hud.h"
#include "mzm_screen.h"

/* What the pause screen reads from the game. */
typedef struct {
    MzmHud *hud;                    /* the minimap (gDecompressedMinimapVisitedTiles...) */
    uint8_t area;                   /* gCurrentArea */
    bool suitless;                  /* gEquipment.suitType == SUIT_SUITLESS */
    bool (*check_event)(void *context, uint8_t event);
    void *context;
} MzmPauseGame;

typedef struct {
    uint16_t type_flags;
    uint8_t state, stage;           /* stateInfo */
    uint16_t timer;
    uint8_t sub_stage;              /* gSubGameModeStage */
    bool on_world_map;
    uint8_t current_area, map_x, map_y;
    uint16_t unk_18;
    int8_t unk_1a;
    uint16_t unk_1c;
    int8_t unk_1e;
    int8_t map_view_x, map_view_y, map_right, map_left, map_top, map_bottom;
    bool can_right, can_left, can_up, can_down;
    uint16_t dispcnt, bg0cnt, bg1cnt, bg2cnt, bg3cnt, bldcnt;
    uint16_t unk_6a, unk_6c, unk_6e, unk_70, unk_72, unk_74;
    uint8_t changing_minimap_stage;
    uint8_t areas_visited, areas_viewable, areas_viewable_total;
    struct { uint8_t stage, color; bool palette_updated; } fading;
    struct { uint8_t delay, set; } maintained;      /* gMaintainedInputData */
    MzmMenuOam misc[23], targets[12], samus_icon[1], boss_icon[1], overlay[4], arrows[4],
        world_map[24];
    /* PAUSE_SCREEN_EWRAM */
    uint16_t maps[8][1024];
    uint16_t original_palette[512], palette[512];
    uint16_t visor_overlay[1024], map_overlay[1024], world_map_overlay[1024];
} MzmPauseScreen;

/* gSubGameModeStage = 0: the next update runs PauseScreenInit. */
void mzm_pause_enter(MzmPauseScreen *p);
/* PauseScreenHandler then PauseScreenVBlank; true when the screen has
 * faded out. */
bool mzm_pause_update(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *game);

#endif
