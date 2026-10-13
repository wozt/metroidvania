/* SPDX-License-Identifier: GPL-3.0-only */
/* The pause screen's map screen (see mzm_pause_screen.h); comments name the
 * pinned decompilation's routines. */
#include "mzm_pause_screen.h"

#include <string.h>

#include "mzm_rom.h"
#include "mzm_sfx.h"

enum {
    BLOCK = 64, HALF_BLOCK = 32, QUARTER_BLOCK = 16, MINIMAP = 32, MINIMAP_TILE_BACKGROUND = 0x140,
    AREAS = 8, AREAS_NORMAL = 7, AREA_TEST = 7, AREA_CRATERIA = 5,
    /* PauseScreenState */
    STATE_MAP_SCREEN = 0,
    /* PauseScreenFading */
    FADING_NONE = 0, FADING_INIT, FADING_IN_INIT, FADING_IN, FADING_OUT_INIT, FADING_OUT,
    /* OAM ids */
    SAMUS_ICON_SUIT = 1, SAMUS_ICON_SUITLESS = 2, BOSS_ICON_CROSSMARK = 3,
    WORLD_MAP_TARGET = 15, WORLD_MAP_SAMUS_SUIT = 16, WORLD_MAP_SAMUS_SUITLESS = 17,
    OAM_ID_CHANGED = 2,
    /* Sounds */
    SOUND_MOVING_MAP = 512, SOUND_MOVING_MAP_MUFFLED = 513, SOUND_OPENING_WORLD_MAP = 514,
    SOUND_CLOSING_WORLD_MAP = 515, SOUND_MAP_SCREEN_CHANGE_MAP = 516,
    /* GBA keys */
    KEY_A = 1 << 0, KEY_B = 1 << 1, KEY_SELECT = 1 << 2, KEY_START = 1 << 3, KEY_RIGHT = 1 << 4,
    KEY_LEFT = 1 << 5, KEY_UP = 1 << 6, KEY_DOWN = 1 << 7, KEY_DIRECTIONS = 0xF0,
    /* BLDCNT */
    BLDCNT_ALPHA = 1 << 6, BLDCNT_SCREEN_FIRST = 0x3F, BLDCNT_BRIGHTNESS_DOWN = 3 << 6,
};

static uint32_t sym(const char *name) { return mzm_rom_symbol(name, NULL); }
static uint8_t rom8(const char *name, uint32_t index) { return mzm_rom_u8(sym(name) + index); }
static uint16_t rom16(const char *name, uint32_t index) { return mzm_rom_u16(sym(name) + index * 2); }

static void vram_lz77(MzmScreen *s, const char *name, uint32_t vram) {
    mzm_lz77_from_rom(sym(name), s->ppu->vram + vram, GBA_VRAM_SIZE - vram);
}

static void ewram_lz77(const char *name, uint16_t *dst) {
    static uint8_t bytes[2048];
    memset(bytes, 0, sizeof bytes);
    mzm_lz77_from_rom(sym(name), bytes, sizeof bytes);
    for (int i = 0; i < 1024; ++i) dst[i] = (uint16_t)(bytes[2 * i] | bytes[2 * i + 1] << 8);
}

static void vram_from16(MzmScreen *s, uint32_t vram, const uint16_t *src, uint32_t bytes) {
    for (uint32_t i = 0; i < bytes / 2; ++i) {
        s->ppu->vram[vram + 2 * i] = (uint8_t)src[i];
        s->ppu->vram[vram + 2 * i + 1] = (uint8_t)(src[i] >> 8);
    }
}

static uint16_t *vram16(MzmScreen *s, uint32_t vram) { return (uint16_t *)(s->ppu->vram + vram); }

/* CREATE_BGCNT with sPauseScreen_BgCntPriority */
static uint16_t bgcnt(int char_base, int screen_base, int priority, int size) {
    return (uint16_t)(size << 14 | screen_base << 8 | char_base << 2 |
                      rom16("sPauseScreen_BgCntPriority", (uint32_t)priority));
}

/* PauseScreenUpdateOrStartFading */
static bool fading(MzmPauseScreen *p, MzmScreen *s, uint8_t stage) {
    if (stage) p->fading.stage = stage;
    switch (p->fading.stage) {
    case FADING_INIT:
        if (!p->fading.palette_updated) {
            p->fading.color = 0;
            p->fading.stage = FADING_NONE;
            return true;
        }
        break;
    case FADING_IN_INIT:
        memcpy(p->original_palette, s->ppu->palette, sizeof p->original_palette);
        memset(s->ppu->palette, 0, sizeof s->ppu->palette);
        memcpy(p->palette, s->ppu->palette, sizeof p->palette);
        p->fading.color = 0;
        p->fading.palette_updated = false;
        p->fading.stage = FADING_IN;
        break;
    case FADING_IN:
    case FADING_OUT: {
        if (p->fading.palette_updated) break;
        uint8_t type = p->fading.stage == FADING_IN ? MZM_FADING_IN : MZM_FADING_OUT;
        if (p->fading.color < 32) {
            mzm_apply_fading_color(type, p->fading.color, p->original_palette, p->palette, 0xFFFF);
            mzm_apply_fading_color(type, p->fading.color, p->original_palette + 256, p->palette + 256,
                                   0xFFFF);
            p->fading.palette_updated = true;
            if (p->fading.color == 31) {
                p->fading.color++;
                break;
            }
            p->fading.color = p->fading.color + 2 > 31 ? 31 : (uint8_t)(p->fading.color + 2);
        } else {
            if (type == MZM_FADING_IN) memcpy(p->palette, p->original_palette, sizeof p->palette);
            else memset(p->palette, 0, sizeof p->palette);
            p->fading.palette_updated = true;
            p->fading.stage = FADING_INIT;
        }
        break;
    }
    case FADING_OUT_INIT:
        memcpy(p->original_palette, s->ppu->palette, sizeof p->original_palette);
        p->fading.color = 0;
        p->fading.palette_updated = false;
        p->fading.stage = FADING_OUT;
        break;
    default:
        break;
    }
    if (p->fading.palette_updated) {
        memcpy(s->ppu->palette, p->palette, sizeof s->ppu->palette);
        p->fading.palette_updated = false;
    }
    return false;
}

/* sMapScreenArrowsData[i]: border arrow index, OAM id, x, y */
static uint16_t arrow_data(int i, int field) { return rom16("sMapScreenArrowsData", (uint32_t)(i * 4 + field)); }

/* PauseScreenUpdateMapArrows (the pause from the game) */
static void update_map_arrows(MzmPauseScreen *p) {
    bool on_map = p->state == STATE_MAP_SCREEN;
    if (on_map) {
        p->can_up = p->map_view_y > p->map_top;
        p->can_down = p->map_bottom > p->map_view_y;
        p->can_left = p->map_view_x > p->map_left;
        p->can_right = p->map_right > p->map_view_x;
    }
    bool can[4] = {p->can_right, p->can_left, p->can_up, p->can_down};
    for (int i = 0; i < 4; ++i) p->arrows[arrow_data(i, 0)].not_drawn = !on_map || !can[i];
    p->overlay[2].not_drawn = on_map ? p->on_world_map : true;
}

/* PauseScreenUpdateBossIcons */
static void update_boss_icons(MzmPauseScreen *p, const MzmPauseGame *g) {
    if (p->current_area >= AREAS_NORMAL) return;
    uint32_t icon = sym("sBossIcons") + p->current_area * 5u;
    uint8_t event = mzm_rom_u8(icon), status = event;
    if (event) {
        p->boss_icon[0].x = (int16_t)(mzm_rom_u8(icon + 2) * HALF_BLOCK + mzm_rom_u8(icon + 4) * 4);
        p->boss_icon[0].y = (int16_t)(mzm_rom_u8(icon + 3) * HALF_BLOCK);
        if (g->check_event && g->check_event(g->context, event)) {
            if (p->current_area != AREA_CRATERIA) {
                p->boss_icon[0].oam_id = BOSS_ICON_CROSSMARK;
                status = 2;
            } else {
                p->boss_icon[0].oam_id = 0;
                status = 0;
            }
        } else {
            p->boss_icon[0].oam_id = mzm_rom_u8(icon + 1);
            status = OAM_ID_CHANGED;
        }
    }
    p->boss_icon[0].exists = status;
}

/* sPauseScreenAreaIconsData[area]: name, name spawning and outline OAM ids,
 * x and y (8 bytes). */
static uint32_t area_icon(uint8_t area) { return sym("sPauseScreenAreaIconsData") + area * 8u; }
/* sWorldMapData[area]: name and outlined OAM ids, x and y (8 bytes). */
static uint32_t world_map_data(int area) { return sym("sWorldMapData") + (uint32_t)area * 8u; }

/* PauseScreenUpdateWorldMapHighlight */
static void update_world_map_highlight(MzmPauseScreen *p, uint8_t area) {
    if (area >= AREAS - 1) area = AREA_TEST;
    uint32_t data = area_icon(area);
    mzm_menu_oam_set_id(&p->overlay[0], mzm_rom_u8(data + 1));
    mzm_menu_oam_set_id(&p->overlay[2], mzm_rom_u8(data + 2));
    p->overlay[2].x = (int16_t)mzm_rom_u16(data + 4);
    p->overlay[2].y = (int16_t)mzm_rom_u16(data + 6);
}

/* PauseScreenUpdateWorldMap (no Chozo hint targets yet) */
static void update_world_map(MzmPauseScreen *p, const MzmPauseGame *g, int on_world_map) {
    MzmMenuOam *o;
    if (on_world_map == 0) {
        o = &p->world_map[0];
        o->priority = 2;
        o->oam_id = g->suitless ? WORLD_MAP_SAMUS_SUITLESS : WORLD_MAP_SAMUS_SUIT;
        o->exists = 1;
        for (int i = 0; i < 7; ++i) {
            o = &p->world_map[1 + i];
            o->x = (int16_t)mzm_rom_u16(world_map_data(i) + 2);
            o->y = (int16_t)mzm_rom_u16(world_map_data(i) + 4);
            o->priority = 2;
        }
        for (int i = 0; i < 16; ++i) {
            o = &p->world_map[8 + i];
            o->x = (int16_t)rom16("sWorldMapTargetPositions", (uint32_t)i * 2);
            o->y = (int16_t)rom16("sWorldMapTargetPositions", (uint32_t)i * 2 + 1);
            o->priority = 2;
            o->exists = 0;
            o->oam_id = 0;
        }
    }
    uint8_t status = on_world_map == 0 ? 1 : OAM_ID_CHANGED;
    for (int i = 0; i < AREAS_NORMAL; ++i) {
        o = &p->world_map[1 + i];
        o->oam_id = mzm_rom_u8(world_map_data(i) + (p->current_area != i ? 0 : 1));
        o->exists = p->areas_viewable >> i & 1 ? status : 0;
    }
    uint8_t area = g->area < AREAS_NORMAL ? g->area : 0;
    p->world_map[0].x = (int16_t)mzm_rom_u16(world_map_data(area) + 2);
    p->world_map[0].y = (int16_t)(mzm_rom_u16(world_map_data(area) + 4) - 0x18);
}

/* PauseScreenUpdateBottomVisorOverlay */
static void update_bottom_visor(MzmPauseScreen *p, MzmScreen *s, int a, int b) {
    uint16_t *dst = vram16(s, 0xCC40);
    const uint16_t *src = &p->visor_overlay[0x380];
    int i0 = -1, i1 = -1;
    if (a) i0 = a == 1 && !p->on_world_map ? 0x16 : 0;
    if (b) i1 = b == 1 && p->areas_viewable_total > 1 ? 0x2D : 0;
    if (i0 == 0 && i1 == 0) {
        memset(dst, 0, 0x80);
        return;
    }
    if (i0 >= 0) {
        dst[0x16] = src[i0];
        dst[0x17] = src[i0 + 1];
        dst[0x16 + 32] = src[i0 + 32];
        dst[0x17 + 32] = src[i0 + 1 + 32];
    }
    if (i1 >= 0)
        for (int i = 0; i < 4; ++i) dst[0x2D + i] = src[i1 + i];
}

/* PauseScreenMapGetAbsoluteMapBordersPositions */
static void map_borders(MzmPauseScreen *p) {
    uint8_t area = p->current_area < AREAS_NORMAL ? p->current_area : AREA_TEST;
    const uint16_t *map = p->maps[area];
    bool found = false;
    for (int i = 0; i < MINIMAP && !found; ++i)
        for (int j = 0; j < MINIMAP; ++j)
            if (map[i * MINIMAP + j] != MINIMAP_TILE_BACKGROUND) { p->map_top = (int8_t)i; found = true; break; }
    found = false;
    for (int i = MINIMAP - 1; i >= 0 && !found; --i)
        for (int j = 0; j < MINIMAP; ++j)
            if (map[i * MINIMAP + j] != MINIMAP_TILE_BACKGROUND) { p->map_bottom = (int8_t)i; found = true; break; }
    found = false;
    for (int i = 0; i < MINIMAP && !found; ++i)
        for (int j = 0; j < MINIMAP; ++j)
            if (map[i + j * MINIMAP] != MINIMAP_TILE_BACKGROUND) { p->map_left = (int8_t)i; found = true; break; }
    found = false;
    for (int i = MINIMAP - 1; i >= 0 && !found; --i)
        for (int j = 0; j < MINIMAP; ++j)
            if (map[i + j * MINIMAP] != MINIMAP_TILE_BACKGROUND) { p->map_right = (int8_t)i; found = true; break; }
}

/* DIV_SHIFT(a, 2) */
static int half(int value) { return value >> 1; }

/* PauseScreenMapSetSpawnPosition (no Chozo hint targets: the borders for
 * targets do not move) */
static void set_spawn_position(MzmPauseScreen *p, MzmScreen *s, int option) {
    p->can_right = p->can_left = p->can_up = p->can_down = false;
    p->map_left = MINIMAP - 1;
    p->map_top = MINIMAP - 1;
    p->map_right = 0;
    p->map_bottom = 0;
    map_borders(p);
    if (p->map_left > p->map_right) p->map_right = p->map_left;
    if (p->map_top > p->map_bottom) p->map_bottom = p->map_top;
    int map_x, map_y;
    if (option == 1) {
        map_x = p->map_x;
        map_y = p->map_y;
    } else {
        map_x = half(p->map_left + p->map_right);
        map_y = half(p->map_top + p->map_bottom);
        if (option != 2) {
            /* Close to Samus's position (the "Chozo hint" option). */
            int high = map_y + 5, low = map_y - 5;
            if (high < p->map_y) map_y += p->map_y - high;
            else if (low > p->map_y) map_y -= low - p->map_y;
            high = map_x + 6;
            low = map_x - 6;
            if (high < p->map_x) map_x += p->map_x - high;
            else if (low > p->map_x) map_x -= low - p->map_x;
        }
    }
    p->map_view_x = (int8_t)map_x;
    p->map_view_y = (int8_t)map_y;
    if (half(p->map_bottom - p->map_top) > 5) {
        p->map_top = (int8_t)(p->map_top + 5);
        p->map_bottom = (int8_t)(p->map_bottom - 5);
    } else {
        p->map_top = p->map_bottom = (int8_t)half(p->map_bottom + p->map_top);
    }
    if (half(p->map_right - p->map_left) > 6) {
        p->map_left = (int8_t)(p->map_left + 6);
        p->map_right = (int8_t)(p->map_right - 6);
    } else {
        p->map_left = p->map_right = (int8_t)half(p->map_right + p->map_left);
    }
    if (p->map_view_x < p->map_left) p->map_left = p->map_view_x;
    else if (p->map_view_x > p->map_right) p->map_right = p->map_view_x;
    if (p->map_view_y < p->map_top) p->map_top = p->map_view_y;
    else if (p->map_view_y > p->map_bottom) p->map_bottom = p->map_view_y;
    s->bg_hofs[3] = (uint16_t)((0x204 - (15 - p->map_view_x) * 8) * 4);
    s->bg_vofs[3] = (uint16_t)((0x204 - (10 - p->map_view_y) * 8) * 4);
}

/* UpdateMinimapAnimatedPalette */
static void minimap_animated_palette(MzmPauseScreen *p, MzmScreen *s) {
    uint16_t *pal = s->ppu->palette;
    if (!p->on_world_map) {
        if (++p->unk_18 >= rom8("sPauseScreen_40dcf0", (uint32_t)p->unk_1a)) {
            p->unk_18 = 0;
            if (++p->unk_1a >= 4) p->unk_1a = 0;
            uint16_t v = rom16("sPauseScreen_40dcb0", (uint32_t)p->unk_1a);
            pal[18] = pal[19] = v;
            v = rom16("sPauseScreen_40dc90", (uint32_t)p->unk_1a);
            pal[50] = pal[51] = v;
            v = rom16("sPauseScreen_40dcd0", (uint32_t)p->unk_1a);
            pal[34] = pal[35] = pal[66] = pal[67] = v;
        }
    }
    if (p->current_area >= AREAS) return;
    p->unk_1c++;
    int value = p->unk_1e < 0 ? -p->unk_1e : p->unk_1e;
    if (p->unk_1c >= rom8("sPauseScreen_40dcf4", (uint32_t)value)) {
        p->unk_1c = 0;
        if (++p->unk_1e >= 16) p->unk_1e = -14;
        value = p->unk_1e < 0 ? -p->unk_1e : p->unk_1e;
        pal[rom8("sMinimapAnimatedPaletteOffsets", p->current_area)] =
            rom16("sMinimapAnimatedPalette", (uint32_t)value);
    }
}

/* LoadPauseScreenBgPalette */
static void load_bg_palette(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g) {
    uint8_t base = rom8("sMinimapAnimatedPaletteOffsets", AREAS);
    uint16_t fill = rom16("sPauseScreen_3fcef0", 79);
    for (int i = 0; i < 9; ++i) p->palette[base + i] = fill;
    for (int i = 0; i < AREAS - 1; ++i)
        if (!(p->areas_viewable >> i & 1)) p->palette[rom8("sMinimapAnimatedPaletteOffsets", (uint32_t)i)] = 0;
    if (g->area < AREAS)
        p->palette[rom8("sMinimapAnimatedPaletteOffsets", g->area)] = rom16("sMinimapAnimatedPalette", 0);
    p->palette[rom8("sMinimapAnimatedPaletteOffsets", AREA_TEST)] = 0;
    p->palette[base] = 0;
    memcpy(&s->ppu->palette[base], &p->palette[base], 9 * 2);
}

/* PauseScreenMapCheckExploredAreas and PauseScreenDetermineMapsViewable
 * (no Chozo hints: areasWithHints stays empty) */
static void determine_maps_viewable(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g) {
    p->areas_visited = 0;
    for (int i = 0; i < AREAS_NORMAL; ++i)
        for (int j = 0; j < MINIMAP; ++j)
            if (g->hud->progress->visited[i][j]) {
                p->areas_visited |= (uint8_t)(1 << i);
                break;
            }
    p->areas_viewable = (uint8_t)(g->hud->progress->downloaded_maps | p->areas_visited);
    p->areas_viewable_total = 0;
    for (int i = 0; i < AREAS_NORMAL; ++i) p->areas_viewable_total += p->areas_viewable >> i & 1;
    if (p->areas_viewable_total <= 1) {
        /* The "press SELECT" text leaves the overlays. */
        for (int i = 0x24C; i < 0x252; ++i) {
            p->map_overlay[i] = p->map_overlay[i + 32] = 0;
            p->world_map_overlay[i] = p->world_map_overlay[i + 32] = 9 << 12 | 0xD0;
        }
    }
    update_bottom_visor(p, s, 1, 1);
}

/* PauseScreenLoadAreaNamesAndIcons (the pause from the game) */
static void load_area_names_and_icons(MzmPauseScreen *p, const MzmPauseGame *g) {
    MzmMenuOam empty = mzm_menu_oam_from_rom(sym("sMenuOamData_Empty"));
    for (int i = 0; i < 23; ++i) p->misc[i] = empty;
    for (int i = 0; i < 4; ++i) p->overlay[i] = p->arrows[i] = empty;
    for (int i = 0; i < 24; ++i) p->world_map[i] = empty;
    p->samus_icon[0].oam_id = g->suitless ? SAMUS_ICON_SUITLESS : SAMUS_ICON_SUIT;
    p->samus_icon[0].exists = 1;
    p->samus_icon[0].x = (int16_t)(g->hud->minimap_x * HALF_BLOCK);
    p->samus_icon[0].y = (int16_t)(g->hud->minimap_y * HALF_BLOCK);
    p->samus_icon[0].bound_background = 3;
    p->boss_icon[0].bound_background = 3;
    p->samus_icon[0].priority = 3;
    p->boss_icon[0].priority = 3;
    p->misc[0].oam_id = 0;
    p->misc[0].y = p->misc[0].x = 0;
    p->overlay[1].oam_id = 0;
    p->overlay[1].y = QUARTER_BLOCK + 8;
    p->overlay[1].x = BLOCK * 7 + QUARTER_BLOCK;
    p->overlay[1].priority = 1;
    update_world_map_highlight(p, g->area);
    p->overlay[0].y = 12;
    p->overlay[0].x = BLOCK * 7 + QUARTER_BLOCK;
    p->overlay[0].priority = 1;
    p->overlay[2].priority = 1;
    p->overlay[3].oam_id = 0;
    p->overlay[3].y = BLOCK * 6 + QUARTER_BLOCK + 8;
    p->overlay[3].x = BLOCK * 8 + HALF_BLOCK;
    update_world_map(p, g, 0);
    for (int i = 3; i >= 0; --i) {
        MzmMenuOam *o = &p->arrows[arrow_data(i, 0)];
        o->exists = 1;
        o->oam_id = (uint8_t)arrow_data(i, 1);
        o->x = (int16_t)arrow_data(i, 2);
        o->y = (int16_t)arrow_data(i, 3);
        o->priority = 3;
    }
}

/* PauseScreenProcessOam */
static void process_oam(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g) {
    s->next_oam_slot = 0;
    mzm_menu_oam_process(s, 4, p->overlay, sym("sPauseScreenOverlayOam"));
    if (p->on_world_map) mzm_menu_oam_process(s, 24, p->world_map, sym("sPauseScreenWorldMapOam"));
    else mzm_menu_oam_process(s, 4, p->arrows, sym("sPauseScreenBorderArrowsOam"));
    mzm_menu_oam_process(s, 23, p->misc, sym("sPauseScreenMiscOam"));
    if (!p->on_world_map) {
        if (p->current_area == g->area)
            mzm_menu_oam_process(s, 1, p->samus_icon, sym("sPauseScreenSamusIconOam"));
        mzm_menu_oam_process(s, 12, p->targets, sym("sPauseScreenTargetsOam"));
        mzm_menu_oam_process(s, 1, p->boss_icon, sym("sPauseScreenBossIconsOam"));
    }
    mzm_reset_free_oam(s);
}

/* PauseScreenVBlank */
static void vblank(MzmPauseScreen *p, MzmScreen *s) {
    mzm_screen_copy_oam(s);
    GbaPpu *ppu = s->ppu;
    gba_ppu_write16(ppu, GBA_REG_DISPCNT, p->dispcnt);
    gba_ppu_write16(ppu, GBA_REG_BLDY, s->bldy);
    gba_ppu_write16(ppu, GBA_REG_MOSAIC, 0);
    for (int bg = 0; bg < 4; ++bg) {
        gba_ppu_write16(ppu, GBA_REG_BG0HOFS + 4u * (unsigned)bg, (uint16_t)((s->bg_hofs[bg] / 4) & 0x1FF));
        gba_ppu_write16(ppu, GBA_REG_BG0VOFS + 4u * (unsigned)bg, (uint16_t)((s->bg_vofs[bg] / 4) & 0x1FF));
    }
    gba_ppu_write16(ppu, GBA_REG_BG0CNT, p->bg0cnt);
    gba_ppu_write16(ppu, GBA_REG_BG0CNT + 2, p->bg1cnt);
    gba_ppu_write16(ppu, GBA_REG_BG0CNT + 4, p->bg2cnt);
    gba_ppu_write16(ppu, GBA_REG_BG0CNT + 6, p->bg3cnt);
    gba_ppu_write16(ppu, GBA_REG_BLDALPHA, (uint16_t)(s->bldalpha_h << 8 | s->bldalpha_l));
    gba_ppu_write16(ppu, GBA_REG_BLDCNT, p->bldcnt);
}

/* PauseScreenInit (the pause from the game, PAUSE_SCREEN_PAUSE_OR_CUTSCENE) */
static void pause_init(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g) {
    GbaPpu *ppu = s->ppu;
    gba_ppu_write16(ppu, GBA_REG_BLDCNT, BLDCNT_SCREEN_FIRST | BLDCNT_BRIGHTNESS_DOWN);
    s->bldy = 16;
    gba_ppu_write16(ppu, GBA_REG_BLDY, 16);
    gba_ppu_write16(ppu, GBA_REG_DISPCNT, 0);
    s->next_oam_slot = 0;
    uint8_t stage = p->sub_stage;
    memset(p, 0, sizeof *p);
    p->sub_stage = stage;
    mzm_reset_free_oam(s);
    mzm_screen_copy_oam(s);
    p->bldcnt = BLDCNT_SCREEN_FIRST | BLDCNT_BRIGHTNESS_DOWN;
    p->current_area = g->area;
    p->map_x = g->hud->minimap_x;
    p->map_y = g->hud->minimap_y;
    /* The palettes and graphics */
    memset(ppu->vram, 0, sizeof ppu->vram);
    memset(ppu->palette, 0, sizeof ppu->palette);
    mzm_copy_from_rom(&ppu->palette[256], sym("sTankIconsPal"), 512);
    mzm_copy_from_rom(&ppu->palette[0], sym("sMinimapTilesPal"), 160);
    mzm_copy_from_rom(&ppu->palette[80], sym("sPauseScreen_3fcef0"), 352);
    ppu->palette[0] = 0;
    mzm_copy_from_rom(ppu->vram + 0x8000, sym("sMinimapTilesGfx"), 0x3000);
    memcpy(ppu->vram + 0xAC20, ppu->vram + 0xA820, 0x3E0);
    vram_lz77(s, "sTankIconsGfx", 0x13000);
    vram_lz77(s, "sMapScreenAreaNamesEnglishGfx", 0x10800);
    vram_lz77(s, "sMapScreenUnknownItemsNamesEnglishGfx", 0x11400);
    vram_lz77(s, "sMapScreenChozoStatueAreaNamesEnglishGfx", 0x11800);
    vram_lz77(s, "sMotifBehindWireframeSamusGfx", 0);
    vram_lz77(s, "sPauseScreenHudGfx", 0x1000);
    vram_lz77(s, "sMinimapLettersGfx", 0x7400);
    vram_lz77(s, "sEquipmentNamesEnglishGfx", 0x6000);
    vram_lz77(s, "sMenuNamesEnglishGfx", 0xC00);
    memcpy(ppu->vram + GBA_OBJ_VRAM, ppu->vram + 0x6000, 0x800);
    for (uint32_t i = 0; i < 0x1800; i += 2) ppu->vram[0xE800 + i] = 0x40, ppu->vram[0xE801 + i] = 0x11;
    vram_from16(s, 0xE000, g->hud->minimap_visited, 2048);
    ewram_lz77("sMapScreenVisorOverlayTilemap", p->visor_overlay);
    vram_from16(s, 0xC800, p->visor_overlay, 2048);
    update_bottom_visor(p, s, 2, 2);
    static uint16_t status[1024];
    ewram_lz77("sStatusScreenTilemap", status);
    vram_from16(s, 0xC000, status, 0x500);
    ewram_lz77("sMapScreenOverlayTilemap", p->map_overlay);
    ewram_lz77("sWorldMapOverlayTilemap", p->world_map_overlay);
    p->on_world_map = false;
    vram_lz77(s, "sMapScreenTextBg0TileTable", 0xD800);
    memset(s->bg_hofs, 0, sizeof s->bg_hofs);
    memset(s->bg_vofs, 0, sizeof s->bg_vofs);
    p->state = STATE_MAP_SCREEN;
    s->bg_vofs[1] = BLOCK * 16;
    /* PauseScreenGetAllMinimapData */
    for (int i = 0; i < AREAS; ++i)
        mzm_hud_area_map(g->hud, rom8("sMapScreenAreaIds", (uint32_t)i), p->maps[i], g->check_event,
                         g->context);
    set_spawn_position(p, s, 0);
    determine_maps_viewable(p, s, g);
    load_bg_palette(p, s, g);
    vram_from16(s, 0xD000, p->map_overlay, 2048);
    s->bldy = 0;
    p->bldcnt = 1 << 2 | BLDCNT_ALPHA | 1 << 11 | 1 << 12 | 1 << 13;
    s->bldalpha_h = 6;
    s->bldalpha_l = 10;
    p->dispcnt = GBA_DCNT_BG1 | GBA_DCNT_BG2 | GBA_DCNT_BG3 | GBA_DCNT_OBJ;
    p->unk_6a = bgcnt(1, 27, 0, 0);
    p->unk_6c = bgcnt(2, 28, 3, 3);
    p->unk_6e = bgcnt(2, 31, 3, 0);
    p->unk_70 = bgcnt(0, 25, 1, 0);
    p->unk_72 = bgcnt(0, 26, 2, 0);
    p->unk_74 = bgcnt(0, 24, 2, 0);
    p->bg3cnt = p->unk_6c;
    p->bg1cnt = p->unk_70;
    p->bg0cnt = p->unk_6a;
    p->bg2cnt = p->unk_72;
    load_area_names_and_icons(p, g);
    s->oam_y_offset = s->oam_x_offset = 4;
    update_map_arrows(p);
    update_boss_icons(p, g);
    process_oam(p, s, g);
    mzm_screen_copy_oam(s);
    fading(p, s, FADING_IN_INIT);
}

/* MapScreenToggleWorldMap */
static void toggle_world_map(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g, bool force_off) {
    if (!force_off) {
        p->on_world_map = !p->on_world_map;
    } else {
        if (!p->on_world_map) return;
        p->on_world_map = false;
    }
    if (p->on_world_map) {
        vram_from16(s, 0xD000, p->world_map_overlay, 2048);
        mzm_copy_from_rom(&s->ppu->palette[1], sym("sPauseScreen_3fd250") + 2, 160 - 2);
        update_world_map(p, g, 1);
        mzm_sfx_play(SOUND_OPENING_WORLD_MAP);
    } else {
        vram_from16(s, 0xD000, p->map_overlay, 2048);
        mzm_copy_from_rom(&s->ppu->palette[1], sym("sMinimapTilesPal") + 2, 160 - 2);
        if (!force_off) mzm_sfx_play(SOUND_CLOSING_WORLD_MAP);
    }
    update_bottom_visor(p, s, 1, 0);
}

/* MapScreenChangeMap (the tank and in-game time counts it redraws only show
 * after the game is completed: not drawn) */
static void change_map(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g) {
    switch (p->changing_minimap_stage) {
    case 1: {
        int i;
        for (i = 0; i < AREAS_NORMAL; ++i)
            if (p->current_area == rom8("sMapScreenAreasViewOrder", (uint32_t)i)) break;
        for (;;) {
            if (++i >= AREA_TEST) i = 0;
            uint8_t area = rom8("sMapScreenAreasViewOrder", (uint32_t)i);
            if (p->areas_viewable >> area & 1) {
                p->current_area = area;
                break;
            }
        }
        for (int j = 0; j < 12; ++j) p->targets[j].exists = 0;
        p->samus_icon[0].exists = 0;
        p->bg3cnt = p->unk_6e;
        mzm_sfx_play(SOUND_MAP_SCREEN_CHANGE_MAP);
        p->changing_minimap_stage++;
        break;
    }
    case 2:
        vram_from16(s, 0xE000, p->maps[p->current_area], 2048);
        set_spawn_position(p, s, p->current_area != g->area ? 2 : 0);
        p->samus_icon[0].exists = OAM_ID_CHANGED;
        update_world_map_highlight(p, p->current_area);
        if (p->on_world_map) update_world_map(p, g, 1);
        p->bg3cnt = p->unk_6c;
        load_bg_palette(p, s, g);
        update_boss_icons(p, g);
        p->changing_minimap_stage++;
        break;
    case 3:
    case 4:
        /* PauseScreenCountTanksInArea and PauseScreenDrawIgtAndTanks: what
         * they draw only shows after the game is completed. */
        p->changing_minimap_stage = 0;
        break;
    default:
        break;
    }
}

/* MapScreenHandler (R, the status screen, and L, easy sleep, are not
 * ported) */
static void map_screen(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g) {
    if (p->changing_minimap_stage) {
        change_map(p, s, g);
        return;
    }
    int action = 0;
    if (!(s->button_input & KEY_A)) {
        if (s->changed_input & KEY_UP) {
            if (p->can_up) { p->map_view_y--; action = 1; }
        } else if (s->changed_input & KEY_DOWN) {
            if (p->can_down) { p->map_view_y++; action = 1; }
        }
        if (s->changed_input & KEY_LEFT) {
            if (p->can_left) { p->map_view_x--; action = 1; }
        } else if (s->changed_input & KEY_RIGHT) {
            if (p->can_right) { p->map_view_x++; action = 1; }
        }
        s->bg_hofs[3] = (uint16_t)((0x204 - (15 - p->map_view_x) * 8) * 4);
        s->bg_vofs[3] = (uint16_t)((0x204 - (10 - p->map_view_y) * 8) * 4);
        if (action == 1) mzm_sfx_play(p->on_world_map ? SOUND_MOVING_MAP_MUFFLED : SOUND_MOVING_MAP);
    }
    if (action) return;
    if (s->changed_input & (KEY_A | KEY_B)) {
        toggle_world_map(p, s, g, false);
        return;
    }
    if (s->changed_input & KEY_SELECT && p->areas_viewable_total > 1 && p->current_area < AREAS_NORMAL)
        p->changing_minimap_stage = 1;
}

/* CheckForMaintainedInput (sMaintainedInputDelays_Fast) */
static void maintained_input(MzmPauseScreen *p, MzmScreen *s) {
    if (s->button_input & KEY_DIRECTIONS) {
        p->maintained.delay++;
    } else {
        p->maintained.delay = 0;
        p->maintained.set = 0;
    }
    if (p->maintained.delay >= rom8("sMaintainedInputDelays_Fast", p->maintained.set)) {
        s->changed_input |= s->button_input & KEY_DIRECTIONS;
        p->maintained.delay = 0;
        if (p->maintained.set < 6) p->maintained.set++;
    }
}

/* PauseScreenCallStateHandler: the map screen, left with START or B (B
 * only off the world map). */
static bool call_state_handler(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g) {
    maintained_input(p, s);
    p->timer++;
    if (p->state != STATE_MAP_SCREEN) return false;
    if (!(s->changed_input & (KEY_START | KEY_B)) || p->on_world_map) {
        map_screen(p, s, g);
        return false;
    }
    return true;
}

void mzm_pause_enter(MzmPauseScreen *p) {
    memset(p, 0, sizeof *p);
}

bool mzm_pause_update(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g) {
    bool leaving = false;
    switch (p->sub_stage) {
    case 0:
        p->sub_stage = 1;
        pause_init(p, s, g);
        break;
    case 2:
        fading(p, s, 0);
        if (call_state_handler(p, s, g)) {
            fading(p, s, FADING_OUT_INIT);
            p->sub_stage++;
        }
        if (p->fading.stage == FADING_NONE) minimap_animated_palette(p, s);
        update_map_arrows(p);
        process_oam(p, s, g);
        break;
    case 4:
        p->sub_stage = 0;
        leaving = true;
        break;
    case 1:
    case 3:
        if (fading(p, s, 0)) p->sub_stage++;
        break;
    default:
        break;
    }
    if (!leaving) vblank(p, s);
    return leaving;
}
