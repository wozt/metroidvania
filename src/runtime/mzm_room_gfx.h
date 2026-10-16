/* SPDX-License-Identifier: GPL-3.0-only */
/* The Zero Mission room backgrounds on a GbaPpu, from the ROM bank: the
 * tileset and common tiles and palettes (RoomLoadTileset), the BG3 and LZ77
 * BG0 tile maps (RoomLoadBackgrounds), the BG control registers of
 * TransparencySetRoomEffectsTransparency, the animated tiles, tank tiles
 * and animated palette (animated_graphics.c), the hatch flashing palette,
 * the liquid BG0 position with its water wave (ScrollUpdateEffectAndHaze
 * Position), and the block layers drawn around the camera from their block
 * values (RoomSetInitialTilemap / RoomUpdate*Tilemap). Each background is
 * rendered on its own so the runtime keeps interleaving its sprites. */
#ifndef MZM_ROOM_GFX_H
#define MZM_ROOM_GFX_H

#include <stdbool.h>
#include <stdint.h>

#include "gba_ppu.h"

enum { MZM_ROOM_GFX_ANIMATED = 16, MZM_ROOM_GFX_NO_EFFECT_Y = 0xFFFF };

typedef struct {
    uint8_t entry, type, frames_per_state, states, counter;
    int8_t frame;
    uint32_t graphics;              /* ROM address */
} MzmAnimatedGfx;

typedef struct {
    GbaPpu ppu;
    bool ready;
    uint8_t area, room;
    /* RoomEntryRom */
    uint8_t tileset, bg_prop[4], bg3_scrolling, transparency, visual_effect;
    uint8_t bg0_size, bg3_size;
    uint16_t effect_y;              /* subpixels, MZM_ROOM_GFX_NO_EFFECT_Y: none */
    uint32_t tilemap, common_tilemap;   /* ROM: gTilemap and gCommonTilemap */
    bool mother_ship_doors;         /* gUseMotherShipDoors */
    /* gAnimatedGraphicsData, gTankAnimations, the animated palette */
    MzmAnimatedGfx animated[MZM_ROOM_GFX_ANIMATED];
    struct { uint8_t timer, frame; } tanks[4];
    uint8_t animated_palette;       /* 0: none */
    uint8_t palette_timer;
    int8_t palette_row;
    int8_t disable_animated_palette;
    uint8_t hatch_timer, hatch_row;
    /* gWaterMovement */
    bool water_moving;
    uint8_t water_stage, water_loop_counter;
    int16_t water_y_offset;
    /* gEffectYPositionOffset, controlled by room actors such as Acid Worm. */
    int16_t effect_y_offset;
    /* ColorFadingFinishDoorFade / ColorFadingProcess_DoorTransition: BG3
     * shows sDoorTransitionTilemap; the room's BG3 map and control are
     * kept to be restored. */
    bool door_bg3;
    uint8_t saved_bg3[0x1000];
    uint16_t saved_bg3cnt;
    /* TransparencySetRoomEffectsTransparency */
    bool samus_on_top;
    uint8_t eva, evb;
} MzmRoomGfx;

/* Loads a room of an area (gCurrentArea, gCurrentRoom); false without the
 * ROM bank or for an unknown room. `ziplines` (EVENT_ZIPLINES_ACTIVATED)
 * and `alarm` (gAlarmTimer) decide whether the animated palette starts
 * disabled. */
bool mzm_room_gfx_load(MzmRoomGfx *g, uint8_t area, uint8_t room, bool mother_ship_doors,
                       bool ziplines, bool alarm);
/* RoomUpdateAnimatedGraphicsAndPalettes' graphics part, once a frame:
 * skipped while movement is prevented; the animated palette also waits for
 * the end of a power bomb; the hatch flashing runs while `hatches_unlocking`. */
void mzm_room_gfx_animate(MzmRoomGfx *g, bool movement_prevented, bool power_bomb,
                          bool hatches_unlocking);
/* Writes the block layer `bg` (0-2) around the view: `blocks` are the
 * runtime's sheet indices (tileset blocks, then 1024 + common blocks), the
 * view's top-left corner is at (x, y) pixels of the layer. */
void mzm_room_gfx_blocks(MzmRoomGfx *g, int bg, const uint16_t *blocks, int width, int height,
                         int x, int y, int view_w, int view_h);
/* The BG0 position (pixels) for a liquid or special BG0 from BG1's
 * (ScrollUpdateEffectAndHazePosition); the water wave advances unless
 * movement is prevented. Returns false when BG0 follows BG1. */
bool mzm_room_gfx_bg0_position(MzmRoomGfx *g, int bg1_x, int bg1_y, bool movement_prevented,
                               int *x, int *y);
/* gEffectYPosition in subpixels (0 without an effect). */
uint16_t mzm_room_gfx_effect_y(const MzmRoomGfx *g);
/* Puts sDoorTransitionTilemap on BG3 (RoomRleDecompress, char base 1,
 * screen base 6, 512x256) or gives the room its BG3 back. */
void mzm_room_gfx_door_bg3(MzmRoomGfx *g, bool on);
/* The BG palette with sDoorTransitionPal (or its mother ship version) in
 * rows 1 and 2 (`door_palette`) and the masked rows faded
 * (ApplySpecialBackgroundFadingColor, `faded`), into `palette` (256 colors). */
void mzm_room_gfx_faded_palette(const MzmRoomGfx *g, bool door_palette, bool faded, uint8_t type,
                                uint8_t color, uint16_t mask, uint16_t *palette);
/* BGxHOFS / BGxVOFS in pixels */
void mzm_room_gfx_scroll(MzmRoomGfx *g, int bg, int x, int y);
/* One background alone (no blending) as 0xAARRGGBB with a transparent
 * backdrop. */
void mzm_room_gfx_render(MzmRoomGfx *g, int bg, int width, int height, uint32_t *pixels, int pitch);
/* The BG priority of TransparencySetRoomEffectsTransparency. */
int mzm_room_gfx_priority(const MzmRoomGfx *g, int bg);

#endif
