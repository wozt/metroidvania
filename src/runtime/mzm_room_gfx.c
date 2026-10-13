/* SPDX-License-Identifier: GPL-3.0-only */
/* room.c, transparency.c, animated_graphics.c and scroll.c's background
 * parts on a GbaPpu (see mzm_room_gfx.h). */
#include "mzm_room_gfx.h"

#include <string.h>

#include "mzm_rom.h"

enum {
    ROOM_ENTRY_SIZE = 60, TILESET_ENTRY_SIZE = 20,
    BG_PROP_RLE = 0x10, BG_PROP_LZ77 = 0x40, BG_PROP_MOVING = 0x31, BG_PROP_DISABLE_TRANSPARENCY = 0x12,
    BG_PROP_CLOSE_UP = 0x41, BG_PROP_43 = 0x43, BG_PROP_44 = 0x44, BG_PROP_DARK_ROOM = 0x45,
    ANIMATED_GFX_SIZE = 0x80, ANIMATED_GFX_VRAM = 0x4000, ANIMATED_GFX_VRAM_END = 0xFF60,
    ANIMATED_PALETTE_COLOR = 0xF0, TANK_FRAME_DELAY = 4,
    GFX_NONE = 0, GFX_NORMAL = 1, GFX_NORMAL_ONCE = 2, GFX_ALTERNATE = 3, GFX_REVERSE_ONCE = 4,
    GFX_REVERSE = 5,
    PAL_NORMAL = 1, PAL_ALTERNATE = 2, PAL_REVERSE = 3,
    ANIMATED_PALETTE_ZIPLINE = 1, BLOCK = 64,
    /* BGCNT: priorities and sizes */
    PRIO_HIGH = 0, PRIO_HIGH_MID = 1, PRIO_LOW_MID = 2, PRIO_LOW = 3,
    SIZE_512x256 = 1 << 14
};

static uint32_t sym(const char *name) { return mzm_rom_symbol(name, NULL); }

static void vram_lz77(MzmRoomGfx *g, uint32_t address, uint32_t vram) {
    uint32_t available;
    const uint8_t *src = mzm_rom_span(address, &available);
    if (src && vram < GBA_VRAM_SIZE) gba_lz77_uncompress(src, available, g->ppu.vram + vram, GBA_VRAM_SIZE - vram);
}

static void palette_from_rom(MzmRoomGfx *g, int color, uint32_t address, int count) {
    for (int i = 0; i < count && color + i < 512; ++i) g->ppu.palette[color + i] = mzm_rom_u16(address + 2u * (uint32_t)i);
}

/* CREATE_BGCNT(charBase, screenBase, prio, size) */
static uint16_t bgcnt(int char_base, int screen_base, int priority, uint16_t size) {
    return (uint16_t)(size | screen_base << 8 | char_base << 2 | priority);
}

/* TransparencyGetBgSizeFlag */
static uint16_t size_flag(uint8_t size) {
    return size == 0 ? 0 : size == 2 ? (uint16_t)(2 << 14) : (uint16_t)SIZE_512x256;
}

/* AnimatedGraphicsLoad */
static void animated_load(MzmRoomGfx *g, uint8_t animated_tileset) {
    uint32_t tilesets = sym("sAnimatedTilesetEntries"), entries = sym("sAnimatedGraphicsEntries");
    for (int i = 0; i < MZM_ROOM_GFX_ANIMATED; ++i) {
        MzmAnimatedGfx *a = &g->animated[i];
        a->entry = mzm_rom_u8(tilesets + animated_tileset * 48u + (uint32_t)i * 3);
        uint32_t data = entries + a->entry * 8u;
        a->type = mzm_rom_u8(data);
        a->frames_per_state = mzm_rom_u8(data + 1);
        a->states = mzm_rom_u8(data + 2);
        a->graphics = mzm_rom_u32(data + 4);
        a->counter = 0;
        a->frame = 0;
        if (a->type == GFX_NORMAL_ONCE || a->type == GFX_REVERSE) a->frame = (int8_t)(a->states - 1);
        const uint8_t *src = mzm_rom_ptr(a->graphics + (uint32_t)a->frame * ANIMATED_GFX_SIZE, ANIMATED_GFX_SIZE);
        if (src) memcpy(g->ppu.vram + ANIMATED_GFX_VRAM + i * ANIMATED_GFX_SIZE, src, ANIMATED_GFX_SIZE);
    }
    /* Slots 12 to 15 are also at the end of VRAM (for the LZ77 backgrounds),
     * in reverse order. */
    memcpy(g->ppu.vram + ANIMATED_GFX_VRAM_END - 3 * ANIMATED_GFX_SIZE,
           g->ppu.vram + ANIMATED_GFX_VRAM + 12 * ANIMATED_GFX_SIZE, 4 * ANIMATED_GFX_SIZE);
}

/* AnimatedPaletteCheckDisableOnTransition */
static void animated_palette_load(MzmRoomGfx *g, bool ziplines, bool alarm) {
    g->palette_timer = 0;
    g->palette_row = 0;
    g->hatch_timer = g->hatch_row = 0;
    g->disable_animated_palette = 0;
    if (g->animated_palette == 0) return;
    uint32_t entry = sym("sAnimatedPaletteEntries") + g->animated_palette * 8u;
    palette_from_rom(g, ANIMATED_PALETTE_COLOR, mzm_rom_u32(entry + 4), 16);
    switch (g->animated_palette) {
    case ANIMATED_PALETTE_ZIPLINE:
        if (!ziplines) g->disable_animated_palette = 1;
        break;
    case 4: case 6: case 8: case 9: case 10: case 11: case 12: case 16: case 17:
        if (!alarm) g->disable_animated_palette = 1;
        break;
    default:
        break;
    }
}

bool mzm_room_gfx_load(MzmRoomGfx *g, uint8_t area, uint8_t room, bool mother_ship_doors,
                       bool ziplines, bool alarm) {
    memset(g, 0, sizeof *g);
    uint32_t size;
    uint32_t table = mzm_rom_symbol("sAreaRoomEntryPointers", &size);
    if (!table || area >= size / 4) return false;
    const uint8_t *e = mzm_rom_ptr(mzm_rom_u32(table + 4u * area) + (uint32_t)room * ROOM_ENTRY_SIZE,
                                   ROOM_ENTRY_SIZE);
    if (!e) return false;
    g->area = area;
    g->room = room;
    g->mother_ship_doors = mother_ship_doors;
    /* RoomLoadEntry */
    g->tileset = e[0];
    for (int i = 0; i < 4; ++i) g->bg_prop[i] = e[1 + i];
    uint32_t bg0 = (uint32_t)(e[8] | e[9] << 8 | e[10] << 16 | (uint32_t)e[11] << 24);
    uint32_t bg3 = (uint32_t)(e[24] | e[25] << 8 | e[26] << 16 | (uint32_t)e[27] << 24);
    g->bg3_scrolling = e[28];
    g->transparency = e[29];
    g->visual_effect = e[55];
    g->effect_y = e[56] != 0xFF ? (uint16_t)(e[56] * BLOCK) : MZM_ROOM_GFX_NO_EFFECT_Y;
    /* RoomLoadTileset */
    uint32_t tileset = sym("sTilesetEntries") + g->tileset * (uint32_t)TILESET_ENTRY_SIZE;
    uint32_t tile_gfx = mzm_rom_u32(tileset), palette = mzm_rom_u32(tileset + 4);
    uint32_t bg_gfx = mzm_rom_u32(tileset + 8);
    g->tilemap = mzm_rom_u32(tileset + 12) + 2;     /* pTilemap (bytes) + 2 */
    g->common_tilemap = sym("sCommonTilemap");
    if (!tile_gfx || !palette || !g->common_tilemap) return false;
    gba_ppu_reset(&g->ppu);
    vram_lz77(g, tile_gfx, 0x5800);
    palette_from_rom(g, 3 * 16, palette + 16 * 2, 13 * 16);
    g->ppu.palette[0] = 0;          /* SET_BACKDROP_COLOR(COLOR_BLACK) */
    const uint8_t *common = mzm_rom_data(mother_ship_doors ? "sCommonTilesMothershipGfx" : "sCommonTilesGfx", &size);
    if (common) memcpy(g->ppu.vram + 0x4800, common, size);
    palette_from_rom(g, 1, sym(mother_ship_doors ? "sCommonTilesMotherShipPal" : "sCommonTilesPal") + 2, 3 * 16 - 1);
    uint32_t bg_size = (uint32_t)(mzm_rom_u8(bg_gfx + 2) << 8 | mzm_rom_u8(bg_gfx + 1));
    if (bg_size <= 0xFDE0) vram_lz77(g, bg_gfx, 0xFDE0 - bg_size);
    memset(g->ppu.vram + 0xFFE0, 0, 0x20);
    uint8_t animated_tileset = mzm_rom_u8(tileset + 16);
    g->animated_palette = mzm_rom_u8(tileset + 17);
    /* RoomSetInitialTilemap: a layer without data is filled with the empty
     * entry 0x40 (as is BG2 for BG_PROP_MOVING). */
    for (int bg = 0; bg < 3; ++bg) {
        bool data = g->bg_prop[bg] & BG_PROP_RLE || (bg == 0 && g->bg_prop[0] & BG_PROP_LZ77);
        if (data && !(bg == 2 && g->bg_prop[2] == BG_PROP_MOVING)) continue;
        for (int i = 0; i < 0x1000; i += 2) {
            g->ppu.vram[bg * 0x1000 + i] = 0x40;
            g->ppu.vram[bg * 0x1000 + i + 1] = 0;
        }
    }
    /* RoomLoadBackgrounds: BG3 (always LZ77) and an LZ77 BG0 */
    g->bg3_size = mzm_rom_u8(bg3);
    vram_lz77(g, bg3 + 4, 0x3000);
    if (g->bg_prop[0] & BG_PROP_LZ77 && !(g->bg_prop[0] & BG_PROP_RLE)) {
        g->bg0_size = mzm_rom_u8(bg0);
        vram_lz77(g, bg0 + 4, 0x0000);
    }
    /* TransparencySetRoomEffectsTransparency: the BG control registers */
    uint16_t cnt[4];
    cnt[0] = g->bg_prop[0] & BG_PROP_LZ77 ? (uint16_t)(size_flag(g->bg0_size) | 2 << 2)
                                         : bgcnt(1, 0, PRIO_HIGH, SIZE_512x256);
    cnt[1] = bgcnt(1, 2, PRIO_HIGH, SIZE_512x256);
    cnt[2] = bgcnt(1, 4, PRIO_HIGH, SIZE_512x256);
    cnt[3] = (uint16_t)(bgcnt(0, 6, PRIO_LOW, 0) | 2 << 2 | size_flag(g->bg3_size));
    uint8_t t = g->transparency;
    /* 0x5, 0x9 ... 0x31: BG1 in front of BG0; 0x6 ...: Samus too; 0x7 ...:
     * BG2 in front of BG0 as well. */
    int kind = t >= 5 && t <= 0x33 ? t & 3 : 0;
    g->samus_on_top = kind >= 2;
    static const uint8_t prios[4][3] = {{PRIO_HIGH, PRIO_HIGH_MID, PRIO_LOW_MID},
                                        {PRIO_HIGH_MID, PRIO_HIGH, PRIO_LOW_MID},
                                        {PRIO_HIGH_MID, PRIO_HIGH, PRIO_LOW_MID},
                                        {PRIO_LOW_MID, PRIO_HIGH, PRIO_HIGH_MID}};
    for (int i = 0; i < 3; ++i) cnt[i] |= prios[kind][i];
    for (int i = 0; i < 4; ++i) gba_ppu_write16(&g->ppu, GBA_REG_BG0CNT + 2u * (unsigned)i, cnt[i]);
    g->eva = 0;
    g->evb = 0;
    if (t >= 0x18 && t <= 0x33) {
        static const uint8_t evbs[] = {0, 3, 6, 9, 11, 13, 16};
        g->evb = evbs[(t - 0x18) / 4];
        g->eva = (uint8_t)(16 - g->evb);
    } else if (t >= 0x8 && t <= 0x17) {
        static const uint8_t evas[] = {7, 10, 13, 16};
        g->eva = evas[(t - 0x8) / 4];
        g->evb = 16;
    } else {
        g->eva = 16;
    }
    /* AnimatedGraphicsLoad, AnimatedGraphicsTanksAnimationReset */
    animated_load(g, animated_tileset);
    for (int i = 0; i < 4; ++i) g->tanks[i].timer = (uint8_t)(i + 1), g->tanks[i].frame = 0;
    animated_palette_load(g, ziplines, alarm);
    /* HazeSetBackgroundEffect: sHazeData[effect][2], the BG0 water wave */
    g->water_moving = g->visual_effect == 1;
    g->ready = true;
    return true;
}

/* AnimatedGraphicsUpdate and AnimatedGraphicsTransfer */
static void animated_update(MzmRoomGfx *g) {
    for (int i = 0; i < MZM_ROOM_GFX_ANIMATED; ++i) {
        MzmAnimatedGfx *a = &g->animated[i];
        bool update = false;
        switch (a->type) {
        case GFX_NORMAL:
            if (++a->counter == a->frames_per_state) {
                update = true;
                a->counter = 0;
                if (++a->frame == a->states) a->frame = 0;
            }
            break;
        case GFX_NORMAL_ONCE:
            if (a->frame != a->states - 1 && ++a->counter == a->frames_per_state) {
                update = true;
                a->counter = 0;
                a->frame++;
            }
            break;
        case GFX_ALTERNATE:
            if (++a->counter == a->frames_per_state) {
                update = true;
                a->counter = 0;
                a->frame++;
                if (a->frame == a->states) a->frame = (int8_t)(2 - a->frame);
            }
            break;
        case GFX_REVERSE_ONCE:
            if (a->frame != 0 && ++a->counter == a->frames_per_state) {
                update = true;
                a->counter = 0;
                a->frame--;
            }
            break;
        case GFX_REVERSE:
            if (++a->counter == a->frames_per_state) {
                update = true;
                a->counter = 0;
                if (--a->frame < 0) a->frame = (int8_t)(a->states - 1);
            }
            break;
        default:
            break;
        }
        if (!update) continue;
        int frame = a->frame < 0 ? -a->frame : a->frame;
        const uint8_t *src = mzm_rom_ptr(a->graphics + (uint32_t)frame * ANIMATED_GFX_SIZE, ANIMATED_GFX_SIZE);
        if (!src) continue;
        memcpy(g->ppu.vram + ANIMATED_GFX_VRAM + i * ANIMATED_GFX_SIZE, src, ANIMATED_GFX_SIZE);
        if (i >= 12)
            memcpy(g->ppu.vram + ANIMATED_GFX_VRAM_END - (15 - i) * ANIMATED_GFX_SIZE, src, ANIMATED_GFX_SIZE);
    }
}

/* AnimatedGraphicsTanksAnimationUpdate */
static void tanks_update(MzmRoomGfx *g) {
    uint32_t gfx = sym("sAnimatedTankGfx");
    for (int i = 3; i >= 0; --i) {
        if (++g->tanks[i].timer <= TANK_FRAME_DELAY) continue;
        g->tanks[i].timer = 0;
        if (++g->tanks[i].frame >= 4) g->tanks[i].frame = 0;
        const uint8_t *src = mzm_rom_ptr(gfx + (uint32_t)(ANIMATED_GFX_SIZE * 4 * i + ANIMATED_GFX_SIZE * g->tanks[i].frame),
                                         ANIMATED_GFX_SIZE);
        if (src)
            memcpy(g->ppu.vram + ANIMATED_GFX_VRAM + ANIMATED_GFX_SIZE * 16 + ANIMATED_GFX_SIZE / 4 +
                       ANIMATED_GFX_SIZE * i,
                   src, ANIMATED_GFX_SIZE);
    }
}

/* AnimatedPaletteUpdate */
static void animated_palette_update(MzmRoomGfx *g) {
    if (g->animated_palette == 0 || g->disable_animated_palette > 0) return;
    uint32_t entry = sym("sAnimatedPaletteEntries") + g->animated_palette * 8u;
    uint8_t type = mzm_rom_u8(entry), frames = mzm_rom_u8(entry + 1), states = mzm_rom_u8(entry + 2);
    bool update = false;
    g->palette_timer++;
    switch (type) {
    case PAL_NORMAL:
        if (frames <= g->palette_timer) {
            g->palette_timer = 0;
            if (states <= ++g->palette_row) {
                g->palette_row = 0;
                if (g->disable_animated_palette < 0) g->disable_animated_palette = 1;
            }
            update = true;
        }
        break;
    case PAL_ALTERNATE:
        if (frames <= g->palette_timer) {
            g->palette_timer = 0;
            if (states <= ++g->palette_row) g->palette_row = (int8_t)-(states - 1);
            if (g->disable_animated_palette < 0 && g->palette_row == 0) g->disable_animated_palette = 1;
            update = true;
        }
        break;
    case PAL_REVERSE:
        if (frames <= g->palette_timer) {
            g->palette_timer = 0;
            if (--g->palette_row < 0) g->palette_row = (int8_t)(states - 1);
            update = true;
        }
        break;
    default:
        break;
    }
    if (!update) return;
    int row = g->palette_row < 0 ? -g->palette_row : g->palette_row;
    palette_from_rom(g, ANIMATED_PALETTE_COLOR, mzm_rom_u32(entry + 4) + (uint32_t)row * 32, 16);
}

/* RoomUpdateHatchFlashingAnimation (the navigation doors are Fusion's) */
static void hatch_flashing(MzmRoomGfx *g) {
    if (++g->hatch_timer < 8) return;           /* CONVERT_SECONDS(2.f / 15) */
    g->hatch_timer = 0;
    if (++g->hatch_row >= 6) g->hatch_row = 0;
    uint32_t pal = sym(g->mother_ship_doors ? "sHatchFlashingMotherShipPal" : "sHatchFlashingPal");
    palette_from_rom(g, 16 + 6, pal + (uint32_t)(g->hatch_row * 16 + 6) * 2, 2);
}

void mzm_room_gfx_animate(MzmRoomGfx *g, bool movement_prevented, bool power_bomb,
                          bool hatches_unlocking) {
    if (!g->ready || movement_prevented) return;
    animated_update(g);
    tanks_update(g);
    if (!power_bomb) animated_palette_update(g);
    if (hatches_unlocking) hatch_flashing(g);
}

/* One block of a layer: gTilemap for the tileset blocks, gCommonTilemap
 * after them. */
static void block_tiles(const MzmRoomGfx *g, uint16_t sheet, uint16_t tiles[4]) {
    uint32_t at = sheet < 1024 ? g->tilemap + sheet * 8u : g->common_tilemap + (sheet - 1024u) * 8u;
    for (int i = 0; i < 4; ++i) tiles[i] = mzm_rom_u16(at + 2u * (uint32_t)i);
}

void mzm_room_gfx_blocks(MzmRoomGfx *g, int bg, const uint16_t *blocks, int width, int height,
                         int x, int y, int view_w, int view_h) {
    if (!g->ready || bg < 0 || bg > 2) return;
    int bx0 = (x < 0 ? x - 15 : x) / 16, by0 = (y < 0 ? y - 15 : y) / 16;
    int bx1 = bx0 + view_w / 16 + 1, by1 = by0 + view_h / 16 + 1;
    for (int by = by0; by <= by1; ++by)
        for (int bx = bx0; bx <= bx1; ++bx) {
            uint16_t tiles[4] = {0x40, 0x40, 0x40, 0x40};
            if (bx >= 0 && by >= 0 && bx < width && by < height) block_tiles(g, blocks[by * width + bx], tiles);
            /* The 512x256 map: 32 x 16 blocks, the right half in the next
             * screen block. */
            int mx = bx & 31, my = by & 15;
            uint32_t base = (uint32_t)bg * 0x1000 + (mx & 16 ? 0x800u : 0) + (uint32_t)((mx & 15) * 2 + my * 64) * 2;
            uint8_t *v = g->ppu.vram + base;
            const uint32_t offsets[4] = {0, 2, 64, 66};
            for (int i = 0; i < 4; ++i) {
                v[offsets[i]] = (uint8_t)tiles[i];
                v[offsets[i] + 1] = (uint8_t)(tiles[i] >> 8);
            }
        }
}

bool mzm_room_gfx_bg0_position(MzmRoomGfx *g, int bg1_x, int bg1_y, bool movement_prevented,
                               int *x, int *y) {
    if (!g->ready) return false;
    uint8_t prop = g->bg_prop[0];
    if (prop & BG_PROP_RLE) {
        if (prop == 0x11) {
            *x = bg1_x / 2;
            *y = bg1_y;
            return true;
        }
        return false;
    }
    if (g->effect_y != MZM_ROOM_GFX_NO_EFFECT_Y) {
        *x = bg1_x;
        int position = (g->effect_y - bg1_y * 4) >> 2;
        int wave = 0;
        uint32_t table = sym("sWaterLoopCounterArray");
        if (g->water_moving) {
            if (!movement_prevented) {
                if (g->water_loop_counter != 0) {
                    g->water_loop_counter--;
                } else {
                    if (++g->water_stage > 7) g->water_stage = 0;
                    g->water_loop_counter = mzm_rom_u8(table + g->water_stage * 2u + 1);
                }
            }
            wave = (int8_t)mzm_rom_u8(table + g->water_stage * 2u);
        }
        g->water_y_offset = (int16_t)((wave - 8) * 4);
        position += wave;
        if (position > BLOCK * 4) position = BLOCK * 4;
        *y = -position;
        return true;
    }
    switch (prop) {
    case BG_PROP_CLOSE_UP:
        *x = 0;
        *y = 0;
        return true;
    default:
        /* BG_PROP_43, BG_PROP_DARK_ROOM and BG_PROP_44 follow positions of
         * the room or the waiting pirates that are not tracked yet. */
        return false;
    }
}

uint16_t mzm_room_gfx_effect_y(const MzmRoomGfx *g) {
    if (!g->ready || g->effect_y == MZM_ROOM_GFX_NO_EFFECT_Y) return 0;
    int position = g->effect_y + g->water_y_offset;
    return (uint16_t)(position < 0 ? 0 : position);
}

void mzm_room_gfx_scroll(MzmRoomGfx *g, int bg, int x, int y) {
    if (bg < 0 || bg > 3) return;
    gba_ppu_write16(&g->ppu, GBA_REG_BG0HOFS + 4u * (unsigned)bg, (uint16_t)(x & 0x1FF));
    gba_ppu_write16(&g->ppu, GBA_REG_BG0VOFS + 4u * (unsigned)bg, (uint16_t)(y & 0x1FF));
}

void mzm_room_gfx_render(MzmRoomGfx *g, int bg, int width, int height, uint32_t *pixels, int pitch) {
    gba_ppu_write16(&g->ppu, GBA_REG_DISPCNT, (uint16_t)(GBA_DCNT_BG0 << bg));
    gba_ppu_write16(&g->ppu, GBA_REG_BLDCNT, 0);
    gba_ppu_render(&g->ppu, width, height, true, pixels, pitch);
}

int mzm_room_gfx_priority(const MzmRoomGfx *g, int bg) {
    return bg < 0 || bg > 3 ? 3 : g->ppu.io[(GBA_REG_BG0CNT + 2 * bg) >> 1] & 3;
}
