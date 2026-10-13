/* SPDX-License-Identifier: GPL-3.0-only */
/* The Zero Mission pause screen's map screen (menus/pause_screen.c,
 * pause_screen_map.c, the palette helpers of status_screen.c): the fade
 * in and out, the visor overlays, the current area's map scrolled with the
 * D-pad, Samus's and the boss icons, the area name, the scroll arrows, the
 * world map (A or B), the other viewable areas (SELECT) and START or B to
 * leave, and the status screen (R, status_screen.c): the equipment rows
 * revealed one by one, the tank amounts, Samus's wireframe and the headers,
 * the item cursor and the item descriptions (text.c), and easy sleep (L,
 * pause_screen_sub_menus.c): its text, the YES/NO cursor and the sleep until
 * SELECT, L and R. The Chozo hints and their targets, the map download,
 * the item toggling of the suitless and item acquisition sequences and the
 * completion info are not ported yet. */
#ifndef MZM_PAUSE_SCREEN_H
#define MZM_PAUSE_SCREEN_H

#include <stdbool.h>
#include <stdint.h>

#include "mzm_hud.h"
#include "mzm_screen.h"
#include "mzm_text.h"

/* What the pause screen reads from the game. */
typedef struct {
    MzmHud *hud;                    /* the minimap (gDecompressedMinimapVisitedTiles...) */
    uint8_t area;                   /* gCurrentArea */
    const MzmEquipment *equipment;  /* gEquipment */
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
    /* The status screen */
    struct {
        uint8_t unk_0, current_slot, previous_left_slot, previous_right_slot;
        uint8_t beam[5], suit[2], misc[6], bomb[2], missiles[2];
    } status;                       /* statusScreenData */
    uint8_t current_status_slot, current_equipment, description_stage;    /* unk_56 */
    uint8_t is_fading, bld_alpha_step_level, bld_alpha_step_timer, target_l, target_h;
    uint16_t target_bld_alpha;
    MzmMessage message;             /* gCurrentMessage */
    /* Easy sleep */
    int8_t easy_sleep_text_state;
    bool sleeping;                  /* the GBA's Stop mode, until SELECT + L + R */
    /* PAUSE_SCREEN_EWRAM */
    uint16_t maps[8][1024];
    uint16_t original_palette[512], palette[512];
    uint16_t visor_overlay[1024], map_overlay[1024], world_map_overlay[1024];
    uint16_t status_tilemap[1024], status_base_tilemap[1024] /* unk_7800 */,
        status_background_tilemap[1024];
    /* easySleepTextFormatted_1 / _2, unk_5000, unk_5800, unk_6000 */
    uint8_t easy_sleep_text[5][0x800];
    uint16_t easy_sleep_tilemap[1024];
    uint8_t gfx_backup[4][0x800];   /* equipmentNamesGfxBackup, unk_b000 ... */
} MzmPauseScreen;

/* gSubGameModeStage = 0: the next update runs PauseScreenInit. */
void mzm_pause_enter(MzmPauseScreen *p);
/* PauseScreenHandler then PauseScreenVBlank; true when the screen has
 * faded out. */
bool mzm_pause_update(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *game);

#endif
