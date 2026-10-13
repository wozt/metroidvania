/* SPDX-License-Identifier: GPL-3.0-only */
/* The Zero Mission HUD (hud.c, hud_generic.c) and minimap (minimap.c,
 * PauseScreenGetMinimapData): the energy digits and tanks, the missile,
 * super missile and power bomb counters with their highlight and refill
 * graphics, the suitless charge bar and the 3x3 minimap with Samus's square,
 * drawn as the native OAM and OBJ VRAM of a GbaPpu from the ROM bank's
 * graphics (src/runtime/mzm_rom.c). */
#ifndef MZM_HUD_H
#define MZM_HUD_H

#include <stdbool.h>
#include <stdint.h>

#include "gba_ppu.h"
#include "mzm_samus.h"

enum { MZM_MINIMAP_SIZE = 32, MZM_HUD_AREAS = 8 };

typedef struct { uint8_t ones, tens, hundreds, thousands; } MzmHudDigits;

/* The game's minimap progress (saved with the file): gVisitedMinimapTiles,
 * gMinimapTilesWithObtainedItems and gEquipment.downloadedMapStatus. */
typedef struct {
    uint32_t visited[MZM_HUD_AREAS][MZM_MINIMAP_SIZE];
    uint32_t obtained[MZM_HUD_AREAS][MZM_MINIMAP_SIZE];
    uint8_t downloaded_maps;
} MzmMinimapProgress;

/* The room Samus is in: gCurrentArea, gCurrentRoomEntry.mapX / mapY (read
 * from sAreaRoomEntryPointers by mzm_hud_room_map) and the clipdata size in
 * blocks. */
typedef struct {
    uint8_t area, room;
    uint8_t map_x, map_y;
    uint16_t width_blocks, height_blocks;
} MzmHudRoom;

/* What HudDraw and MinimapUpdate read each frame. */
typedef struct {
    const MzmEquipment *equipment;
    uint8_t weapon_highlighted;     /* gSamusWeaponInfo.weaponHighlighted */
    bool missiles_selected;         /* gSamusWeaponInfo.missilesSelected (supers) */
    uint8_t charge_counter;         /* gSamusWeaponInfo.chargeCounter */
    uint8_t frame8;                 /* gFrameCounter8Bit */
    uint16_t samus_x, samus_y;      /* gSamusData position, subpixels */
    /* gEnergyRefillAnimation, gMissileRefillAnimation,
     * gSuperMissileRefillAnimation, gPowerBombRefillAnimation (NULL: none) */
    const uint8_t *refill_animation;
    /* CHECK_EVENT for the boss icons (NULL: no event set). */
    bool (*check_event)(void *context, uint8_t event);
    void *context;
} MzmHudFrame;

typedef struct {
    GbaPpu *ppu;
    bool ready;
    bool hidden;                    /* gHideHud */
    int screen_width;               /* the minimap is drawn at its right */
    MzmHudDigits energy, max_energy, missiles, super_missiles, power_bombs;
    uint8_t missile_highlight, super_missile_highlight, power_bomb_highlight;
    uint8_t ammo_digits_gfx[64];    /* gAmmoDigitsGfx */
    uint8_t update_minimap;         /* gUpdateMinimapFlag */
    uint32_t minimap_gfx[5 * 24];   /* gMinimapTilesGfx */
    uint8_t minimap_x, minimap_y;   /* gMinimapX / gMinimapY */
    uint8_t area_before_transition; /* gAreaBeforeTransition */
    struct { uint8_t flags, area, map_x, map_y; } last_area_name;
    uint16_t minimap_data[MZM_MINIMAP_SIZE * MZM_MINIMAP_SIZE];   /* gDecompressedMinimapData */
    uint16_t minimap_visited[MZM_MINIMAP_SIZE * MZM_MINIMAP_SIZE];  /* ...VisitedTiles */
    MzmMinimapProgress *progress;
    bool low_health;                /* gEquipment.lowHealth */
    bool ship_landing;              /* gShipLandingFlag */
} MzmHud;

/* The room entry's map position from sAreaRoomEntryPointers; false without
 * the ROM bank or for an unknown room. */
bool mzm_hud_room_map(uint8_t area, uint8_t room, uint8_t *map_x, uint8_t *map_y);
/* An area's index from its exported name ("Brinstar"...), 0xFF if unknown. */
uint8_t mzm_hud_area_index(const char *name);

/* HudGenericLoadCommonSpriteGfx, the sCommonSpritesPal load of the in-game
 * init and HudGenericResetHudData; the PPU shows objects with the 2D
 * mapping. `ready` stays false without the ROM bank. */
void mzm_hud_init(MzmHud *hud, GbaPpu *ppu, MzmMinimapProgress *progress, int screen_width);
/* MinimapCheckOnTransition, when a room is entered. */
void mzm_hud_room_entered(MzmHud *hud, const MzmHudRoom *room, const MzmHudFrame *frame);
/* MinimapUpdate (RoomUpdateGfxInfo) then HudDraw, once a frame. */
void mzm_hud_update(MzmHud *hud, const MzmHudRoom *room, const MzmHudFrame *frame);
/* MinimapUpdateForCollectedItem at a block of the current room. */
void mzm_hud_collected_item(MzmHud *hud, const MzmHudRoom *room, int x_block, int y_block);
/* MinimapCheckIsTileExplored at a block of the current room. */
bool mzm_hud_tile_explored(const MzmHud *hud, const MzmHudRoom *room, int x_block, int y_block);
/* MinimapUpdateChunk: a boss icon of the current area changes. */
void mzm_hud_update_chunk(MzmHud *hud, const MzmHudRoom *room, uint8_t event);

#endif
