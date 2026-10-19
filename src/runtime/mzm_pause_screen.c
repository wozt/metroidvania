/* SPDX-License-Identifier: GPL-3.0-only */
/* The pause screen's map screen (see mzm_pause_screen.h); comments name the
 * pinned decompilation's routines. */
#include "mzm_pause_screen.h"

#include <string.h>

#include "mzm_rom.h"
#include "mzm_sfx.h"
#include "mzm_sound_ids.h"

enum {
    BLOCK = 64, HALF_BLOCK = 32, QUARTER_BLOCK = 16, MINIMAP = 32, MINIMAP_TILE_BACKGROUND = 0x140,
    AREAS = 8, AREAS_NORMAL = 7, AREA_TEST = 7, AREA_CRATERIA = 5,
    /* PauseScreenState */
    STATE_MAP_SCREEN = 0, STATE_STATUS_SCREEN = 1, STATE_STATUS_SCREEN_INIT = 11,
    STATE_STATUS_SCREEN_LEAVING = 12, STATE_EASY_SLEEP = 10, STATE_EASY_SLEEP_INIT = 16,
    STATE_EASY_SLEEP_LEAVING = 17, STATE_WAIT_TO_LEAVE = 3, STATE_MAP_DOWNLOAD = 9,
    STATE_MAP_DOWNLOAD_ENDED = 18, STATE_LEAVING_AFTER_HINT = 2, STATE_CHOZO_HINT = 8,
    STATE_CHOZO_HINT_ENDED = 14, STATE_CHOZO_HINT_INIT = 15,
    STATE_FULLY_POWERED_ITEMS = 6, STATE_FULLY_POWERED_ITEMS_ENDED = 7, STATE_FULLY_POWERED_ITEMS_INIT = 20,
    STATE_SUITLESS_ITEMS = 4, STATE_SUITLESS_ITEMS_ENDED = 5, STATE_SUITLESS_ITEMS_INIT = 19,
    /* PauseScreenType */
    TYPE_ON_MAP_SCREEN = 1 << 1, TYPE_CHOZO_STATUE_HINT = 1 << 4, TYPE_DOWNLOADING_MAP = 1 << 5,
    TYPE_ON_STATUS_SCREEN = 1 << 7, TYPE_GETTING_NEW_ITEM = 1 << 8, TYPE_GETTING_SUITLESS = 1 << 9,
    TYPE_GETTING_FULLY_POWERED = 1 << 10,
    /* PauseScreenFading */
    FADING_NONE = 0, FADING_INIT, FADING_IN_INIT, FADING_IN, FADING_OUT_INIT, FADING_OUT,
    /* OAM ids */
    SAMUS_ICON_SUIT = 1, SAMUS_ICON_SUITLESS = 2, BOSS_ICON_CROSSMARK = 3,
    WORLD_MAP_TARGET = 15, WORLD_MAP_SAMUS_SUIT = 16, WORLD_MAP_SAMUS_SUITLESS = 17,
    OAM_ID_CHANGED = 2,
    MISC_MAP_DOWNLOAD_LINE_TRAIL = 10, MISC_MAP_DOWNLOAD_LINE = 11, MISC_TEXT_MARKER_DOWN = 4, MISC_TEXT_MARKER_UP = 5, MISC_ITEM_CURSOR_FOCUSING = 6,
    MISC_GUN_HEADER = 41, MISC_POWER_SUIT_WIREFRAME = 44, MISC_FULL_SUIT_WIREFRAME = 45,
    MISC_SUITLESS_WIREFRAME = 46, OVERLAY_R_PROMPT_PRESSED = 15, OVERLAY_L_PROMPT_PRESSED = 16,
    MISC_ITEM_CURSOR_FOCUSING_DESTROY = 13, MISC_PLASMA_UNKNOWN = 47, MISC_SPACE_JUMP_UNKNOWN = 49,
    MISC_GRAVITY_UNKNOWN = 51, OVERLAY_SELECT_ON = 17, OVERLAY_SELECT_OFF = 18,
    /* ItemAcquisition */
    ITEM_ACQUISITION_MISSILES = 3, ITEM_ACQUISITION_SUPER_MISSILES = 5, ITEM_ACQUISITION_POWER_BOMB = 7,
    ITEM_ACQUISITION_LONG_BEAM = 8, ITEM_ACQUISITION_PLASMA_BEAM = 12, ITEM_ACQUISITION_GRAVITY = 15,
    ITEM_ACQUISITION_SPACE_JUMP = 20,
    /* ItemToggleAction */
    TOGGLE_CHECKING = 0, TOGGLE_TOGGLING, TOGGLE_CHECKING2,
    /* Sounds */
    SOUND_MOVING_MAP = 512, SOUND_MOVING_MAP_MUFFLED = 513, SOUND_OPENING_WORLD_MAP = 514,
    SOUND_CLOSING_WORLD_MAP = 515, SOUND_MAP_SCREEN_CHANGE_MAP = 516, SOUND_OPENING_STATUS_SCREEN = 500,
    SOUND_LEAVING_STATUS_SCREEN = 501, SOUND_STATUS_SCREEN_CURSOR_MOVING = 502,
    SOUND_OPENING_EASY_SLEEP_SCREEN = 517, SOUND_CLOSING_EASY_SLEEP_SCREEN = 518,
    SOUND_YES_NO_CURSOR_MOVING = 519, SOUND_YES_NO_CURSOR_SELECTING_YES = 520, SOUND_REFUSE_MENU = 521,
    /* GBA keys */
    KEY_A = 1 << 0, KEY_B = 1 << 1, KEY_SELECT = 1 << 2, KEY_START = 1 << 3, KEY_RIGHT = 1 << 4,
    KEY_LEFT = 1 << 5, KEY_UP = 1 << 6, KEY_DOWN = 1 << 7, KEY_R = 1 << 8, KEY_L = 1 << 9,
    KEY_DIRECTIONS = 0xF0,
    /* BLDCNT */
    BLDCNT_ALPHA = 1 << 6, BLDCNT_SCREEN_FIRST = 0x3F, BLDCNT_BRIGHTNESS_DOWN = 3 << 6,
    BLDCNT_BG2_FIRST = 1 << 2, BLDCNT_BG2_SECOND = 1 << 10, BLDCNT_BG3_SECOND = 1 << 11,
    BLDCNT_OBJ_SECOND = 1 << 12, BLDCNT_BACKDROP_SECOND = 1 << 13,
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

static bool suitless(const MzmPauseGame *g) { return g->equipment->suit == MZM_SUIT_SUITLESS; }
/* gCurrentItemBeingAcquired: the suitless sequence sets the Long Beam
 * (PauseScreenInit). */
static uint8_t item_being_acquired(const MzmPauseGame *g) {
    return g->flag == MZM_PAUSE_FLAG_SUITLESS_ITEMS ? 8 : g->item_being_acquired;
}

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
static void update_map_arrows(MzmPauseScreen *p, const MzmPauseGame *g) {
    if (g->flag != MZM_PAUSE_FLAG_PAUSE) return;
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

/* PauseScreenUpdateWorldMap */
static void update_world_map(MzmPauseScreen *p, const MzmPauseGame *g, int on_world_map) {
    MzmMenuOam *o;
    if (on_world_map == 0) {
        o = &p->world_map[0];
        o->priority = 2;
        o->oam_id = suitless(g) ? WORLD_MAP_SAMUS_SUITLESS : WORLD_MAP_SAMUS_SUIT;
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
            o->exists = p->hint.activated >> i & 1 && o->x + o->y != 0;
            o->oam_id = o->exists ? WORLD_MAP_TARGET : 0;
        }
    } else if (on_world_map == 2) {
        for (int i = 0; i < 16; ++i) {
            o = &p->world_map[8 + i];
            bool on = p->hint.activated >> i & 1;
            o->oam_id = on ? WORLD_MAP_TARGET : 0;
            o->exists = on;
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

/* PauseScreenMapUpdateMapBordersForTargets */
static void map_borders_for_targets(MzmPauseScreen *p) {
    for (int i = 0; i < 12; ++i) {
        const MzmMenuOam *o = &p->targets[i];
        if (!o->exists) continue;
        int x = o->x < 0 ? 0 : o->x >= MINIMAP * HALF_BLOCK ? MINIMAP - 1 : o->x / HALF_BLOCK;
        int y = o->y < 0 ? 0 : o->y >= MINIMAP * HALF_BLOCK ? MINIMAP - 1 : o->y / HALF_BLOCK;
        if (y < p->map_top) p->map_top = (int8_t)y;
        if (y > p->map_bottom) p->map_bottom = (int8_t)y;
        if (x < p->map_left) p->map_left = (int8_t)x;
        if (x > p->map_right) p->map_right = (int8_t)x;
    }
}

/* DIV_SHIFT(a, 2) */
static int half(int value) { return value >> 1; }

/* PauseScreenMapSetSpawnPosition */
static void set_spawn_position(MzmPauseScreen *p, MzmScreen *s, int option) {
    p->can_right = p->can_left = p->can_up = p->can_down = false;
    p->map_left = MINIMAP - 1;
    p->map_top = MINIMAP - 1;
    p->map_right = 0;
    p->map_bottom = 0;
    map_borders(p);
    map_borders_for_targets(p);
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

/* PauseScreenMapCheckExploredAreas and PauseScreenDetermineMapsViewable */
static void determine_maps_viewable(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g) {
    p->areas_visited = 0;
    for (int i = 0; i < AREAS_NORMAL; ++i)
        for (int j = 0; j < MINIMAP; ++j)
            if (g->hud->progress->visited[i][j]) {
                p->areas_visited |= (uint8_t)(1 << i);
                break;
            }
    p->areas_viewable = (uint8_t)(g->hud->progress->downloaded_maps | p->areas_visited | p->areas_with_hints);
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
static bool update_status_oam(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g, int option);

static void load_area_names_and_icons(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g) {
    MzmMenuOam empty = mzm_menu_oam_from_rom(sym("sMenuOamData_Empty"));
    for (int i = 0; i < 23; ++i) p->misc[i] = empty;
    for (int i = 0; i < 4; ++i) p->overlay[i] = p->arrows[i] = empty;
    for (int i = 0; i < 24; ++i) p->world_map[i] = empty;
    if (p->type_flags & TYPE_GETTING_NEW_ITEM) {
        update_status_oam(p, s, g, 0);
        return;
    }
    p->samus_icon[0].oam_id = suitless(g) ? SAMUS_ICON_SUITLESS : SAMUS_ICON_SUIT;
    p->samus_icon[0].exists = 1;
    p->samus_icon[0].x = (int16_t)(g->hud->minimap_x * HALF_BLOCK);
    p->samus_icon[0].y = (int16_t)(g->hud->minimap_y * HALF_BLOCK);
    p->samus_icon[0].bound_background = 3;
    p->boss_icon[0].bound_background = 3;
    uint8_t icon_priority = p->type_flags & TYPE_ON_MAP_SCREEN ? 1 : 3;
    p->samus_icon[0].priority = icon_priority;
    p->boss_icon[0].priority = icon_priority;
    p->misc[0].oam_id = 0;
    p->misc[0].y = p->misc[0].x = 0;
    if (p->type_flags & TYPE_CHOZO_STATUE_HINT) {
        p->overlay[0].oam_id = rom8("sChozoHintAreaNamesOamIds", p->current_area);
        p->overlay[0].y = (int16_t)rom16("sChozoHintAreaNamesPosition", 1);
        p->overlay[0].x = (int16_t)rom16("sChozoHintAreaNamesPosition", 0);
        p->overlay[0].priority = 0;
        p->overlay[0].bound_background = 4;
        p->overlay[0].exists = 1;
    } else if (p->type_flags & TYPE_DOWNLOADING_MAP) {
        /* sPauseScreenAreaIconsData[area].nameOamId */
        mzm_menu_oam_set_id(&p->overlay[0], mzm_rom_u8(area_icon(g->area < AREA_TEST ? g->area : AREA_TEST)));
        p->overlay[0].y = 12;
        p->overlay[0].x = BLOCK * 7 + QUARTER_BLOCK;
        p->overlay[0].priority = 0;
    } else {
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
    }
    for (int i = 3; i >= 0; --i) {
        MzmMenuOam *o = &p->arrows[arrow_data(i, 0)];
        if (g->flag == MZM_PAUSE_FLAG_PAUSE) o->exists = 1;
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
    if (p->type_flags & TYPE_CHOZO_STATUE_HINT)
        mzm_menu_oam_process(s, 5, p->chozo_hint, sym("sPauseScreenTargetsOam"));
    if (!p->on_world_map && !(p->type_flags & TYPE_ON_STATUS_SCREEN)) {
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

/* The status screen (status_screen.c) */

enum {
    /* AbilityGroup */
    GROUP_BEAMS = 0, GROUP_BOMBS, GROUP_SUITS, GROUP_MISC, GROUP_MISSILES, GROUP_CURRENT_ENERGY,
    GROUP_MAX_ENERGY, GROUP_CURRENT_MISSILES, GROUP_MAX_MISSILES, GROUP_CURRENT_SUPER_MISSILES,
    GROUP_MAX_SUPER_MISSILES, GROUP_CURRENT_POWER_BOMBS, GROUP_MAX_POWER_BOMBS,
    /* StatusSlots */
    SLOT_0 = 0, SLOT_BEAM = 1, SLOT_MISSILE = 6, SLOT_BOMB = 8, SLOT_SUIT = 10, SLOT_MISC = 12, SLOT_COUNT = 18,
    /* BeamBombFlags, SuitMiscFlags */
    BBF_LONG_BEAM = 1 << 0, BBF_ICE_BEAM = 1 << 1, BBF_WAVE_BEAM = 1 << 2, BBF_PLASMA_BEAM = 1 << 3,
    BBF_CHARGE_BEAM = 1 << 4, BBF_BOMBS = 1 << 7,
    SMF_HIGH_JUMP = 1 << 0, SMF_SPEEDBOOSTER = 1 << 1, SMF_SPACE_JUMP = 1 << 2, SMF_SCREW_ATTACK = 1 << 3,
    SMF_VARIA_SUIT = 1 << 4, SMF_GRAVITY_SUIT = 1 << 5, SMF_MORPH_BALL = 1 << 6, SMF_POWER_GRIP = 1 << 7,
    /* BombActivation, MissileActivation */
    BOMB_COLLECTED = 1 << 0, BOMB_ACTIVATED = 1 << 1, BOMB_HAS_AMMO = 1 << 2, BOMB_MORPH_ACTIVATED = 1 << 3,
    MISSILE_COLLECTED = 1 << 0, MISSILE_HAS_AMMO = 1 << 1,
    /* DescriptionTextId */
    TEXT_LONG_BEAM = 0, TEXT_CHARGE_BEAM, TEXT_ICE_BEAM, TEXT_WAVE_BEAM, TEXT_PLASMA_BEAM, TEXT_MISSILES,
    TEXT_SUPER_MISSILES, TEXT_BOMBS, TEXT_POWER_BOMBS, TEXT_VARIA_SUIT, TEXT_GRAVITY_SUIT, TEXT_MORPH_BALL,
    TEXT_POWER_GRIP, TEXT_SPEEDBOOSTER, TEXT_HIGH_JUMP, TEXT_SCREW_ATTACK, TEXT_SPACE_JUMP,
    TEXT_UNKNOWN_ITEM, TEXT_PISTOL, TEXT_COUNT,
    STATUS_ROW = 32, WIREFRAME_PARTS = 7,
};

/* gEquipment's beamBombs / suitMisc (obtained) and their activation. */
typedef struct {
    uint8_t beam_bombs, beam_bombs_activation, suit_misc, suit_misc_activation;
} Abilities;

static Abilities abilities(const MzmPauseGame *g) {
    static const struct { uint32_t item; uint8_t flag; bool beam_bomb; } flags[] = {
        {MZM_ITEM_LONG_BEAM, BBF_LONG_BEAM, true}, {MZM_ITEM_ICE_BEAM, BBF_ICE_BEAM, true},
        {MZM_ITEM_WAVE_BEAM, BBF_WAVE_BEAM, true}, {MZM_ITEM_PLASMA_BEAM, BBF_PLASMA_BEAM, true},
        {MZM_ITEM_CHARGE_BEAM, BBF_CHARGE_BEAM, true}, {MZM_ITEM_BOMBS, BBF_BOMBS, true},
        {MZM_ITEM_HIGH_JUMP, SMF_HIGH_JUMP, false}, {MZM_ITEM_SPEED_BOOSTER, SMF_SPEEDBOOSTER, false},
        {MZM_ITEM_SPACE_JUMP, SMF_SPACE_JUMP, false}, {MZM_ITEM_SCREW_ATTACK, SMF_SCREW_ATTACK, false},
        {MZM_ITEM_VARIA_SUIT, SMF_VARIA_SUIT, false}, {MZM_ITEM_GRAVITY_SUIT, SMF_GRAVITY_SUIT, false},
        {MZM_ITEM_MORPH_BALL, SMF_MORPH_BALL, false}, {MZM_ITEM_POWER_GRIP, SMF_POWER_GRIP, false},
    };
    Abilities a = {0, 0, 0, 0};
    const MzmEquipment *e = g->equipment;
    for (size_t i = 0; i < sizeof flags / sizeof flags[0]; ++i) {
        bool obtained = (e->items | e->unknown_items) & flags[i].item, active = e->items & flags[i].item;
        uint8_t *have = flags[i].beam_bomb ? &a.beam_bombs : &a.suit_misc;
        uint8_t *on = flags[i].beam_bomb ? &a.beam_bombs_activation : &a.suit_misc_activation;
        if (obtained) *have |= flags[i].flag;
        if (active) *on |= flags[i].flag;
    }
    return a;
}

/* The Chozo statue hints (pause_screen_sub_menus.c). */
enum {
    TARGETS = 16, TARGET_ITEM_COUNT = 8, TARGET_KRAID_FLAME = 8, TARGET_RIDLEY_FLAME = 9,
    TARGET_OAM_UP_ARROW = 2, TARGET_OAM_DOWN_ARROW = 3,
    TARGET_OAM_GREEN_FLAME_MOVING = 10, TARGET_OAM_PURPLE_FLAME_MOVING = 14,
    HINT_CONDITION_BEAM_BOMBS = 0, HINT_CONDITION_SUIT_MISC, HINT_CONDITION_EVENT,
    TARGET_MOVEMENT_FLAG = 1 << 0, TARGET_SCROLLING_FLAG = 1 << 1,
    /* ChozoHintMovementStage, ChozoHintScrollingStage, ChozoHintStage */
    MOVE_FINISHED = 0, MOVE_CALCULATE, MOVE_DELAY_BEFORE, MOVE_UPDATE, MOVE_DELAY_AFTER,
    MOVE_DELAY_BEFORE_FINISHED, MOVE_DELAY_BEFORE_CONTINUE,
    SCROLL_FINISHED = 0, SCROLL_CALCULATE, SCROLL_DELAY_BEFORE, SCROLL_UPDATE, SCROLL_DELAY_BEFORE_FINISHED,
    HINT_DELAY_BEFORE_UPDATE = 0, HINT_BEGIN_UPDATE, HINT_WAIT_UPDATE_FINISHED, HINT_CHECK_FINISHED,
    HINT_DELAY_BEFORE_FADE_OUT, HINT_UPDATE_FADE_OUT, HINT_SETUP_NEXT_AREA, HINT_UPDATE_FADE_IN,
    HINT_DELAY_BEFORE_FINISHED, HINT_FINISHED,
    BLDCNT_BG3_FIRST = 1 << 3, BLDCNT_OBJ_FIRST = 1 << 4,
    AREA_NONE = 0xFF,
};

/* sChozoStatueTargets[i]: statue area, x and y ranges, start icon, target
 * area, x, y and end icon (12 bytes). */
typedef struct {
    uint8_t statue_area, statue_x_start, statue_x_end, statue_y_start, statue_y_end, start_icon;
    uint8_t target_area, target_x, target_y, end_icon;
} HintTarget;

static HintTarget hint_target(int i) {
    uint32_t at = sym("sChozoStatueTargets") + (uint32_t)i * 12u;
    return (HintTarget){mzm_rom_u8(at), mzm_rom_u8(at + 1), mzm_rom_u8(at + 2), mzm_rom_u8(at + 3),
                        mzm_rom_u8(at + 4), mzm_rom_u8(at + 5), mzm_rom_u8(at + 6), mzm_rom_u8(at + 7),
                        mzm_rom_u8(at + 8), mzm_rom_u8(at + 9)};
}

/* sElevatorRoomPairs[route]: area, room, map x and y of both ends. */
static uint8_t elevator(int route, int field) {
    return mzm_rom_u8(sym("sElevatorRoomPairs") + (uint32_t)route * 8u + (uint32_t)field);
}

/* Sqrt (the BIOS's integer square root) */
static uint32_t isqrt(uint32_t n) {
    uint32_t root = 0, bit = 1u << 30;
    while (bit > n) bit >>= 2;
    while (bit) {
        if (n >= root + bit) {
            n -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    return root;
}

/* ChozoStatueHintCheckTargetIsActivated: -1 not activated, 0 done, 1 to do */
static int target_activated(const MzmPauseGame *g, int target) {
    uint8_t event = rom8("sChozoStatueHintEvents", (uint32_t)target);
    if (event == 0xFF || !g->check_event || !g->check_event(g->context, event)) return -1;
    uint8_t type = rom8("sChozoStatueTargetConditions", (uint32_t)target * 2u);
    uint8_t flag = rom8("sChozoStatueTargetConditions", (uint32_t)target * 2u + 1u);
    Abilities a = abilities(g);
    int result = -1;
    if (type == HINT_CONDITION_BEAM_BOMBS) result = (a.beam_bombs & flag) != 0;
    else if (type == HINT_CONDITION_SUIT_MISC) result = (a.suit_misc & flag) != 0;
    else if (type == HINT_CONDITION_EVENT) result = g->check_event(g->context, flag);
    if (result >= 0) result ^= 1;
    return result;
}

/* PauseScreenCheckActivatedTargets */
static void check_activated_targets(MzmPauseScreen *p, const MzmPauseGame *g) {
    p->hint.activated = 0;
    for (int i = 0; i < TARGETS; ++i)
        if (target_activated(g, i) > 0) p->hint.activated |= (uint16_t)(1u << i);
}

/* PauseScreenCheckAreasWithTargets */
static void check_areas_with_targets(MzmPauseScreen *p, const MzmPauseGame *g) {
    p->areas_with_hints = 0;
    for (int i = 0; i < TARGETS; ++i)
        if (target_activated(g, i) > 0) p->areas_with_hints |= (uint8_t)(1u << hint_target(i).target_area);
}

/* PauseScreenDrawBossFlames: Kraid's and Ridley's flames in Brinstar. */
static void draw_boss_flames(MzmPauseScreen *p) {
    memset(p->boss_flames, 0, sizeof p->boss_flames);
    if (p->current_area != 0) return;
    static const struct { int target, oam, dx; } flames[2] = {
        {TARGET_KRAID_FLAME, TARGET_OAM_GREEN_FLAME_MOVING, -8}, {TARGET_RIDLEY_FLAME, TARGET_OAM_PURPLE_FLAME_MOVING, 8}};
    for (int f = 0; f < 2; ++f) {
        if (!(p->hint.activated >> flames[f].target & 1)) continue;
        for (int i = 0; i < 12; ++i) {
            if (p->targets[i].oam_id != flames[f].oam) continue;
            p->boss_flames[f].oam_offset = (uint8_t)i;
            p->boss_flames[f].direction = 1;
            p->targets[i].x = (int16_t)(p->targets[i].x + flames[f].dx);
            p->boss_flames[f].x = (uint16_t)p->targets[i].x;
            p->boss_flames[f].y = (uint16_t)p->targets[i].y;
            break;
        }
    }
}

/* ChozoStatueHintChangeArea: an elevator of the path (`path`: its route
 * and direction) as the place on the map where the hint changes area. */
static void hint_change_area(MzmPauseScreen *p, uint8_t *x, uint8_t *y, MzmMenuOam *o, uint32_t path) {
    int route = (int8_t)mzm_rom_u8(path);
    int8_t direction = (int8_t)mzm_rom_u8(path + 1);
    if (elevator(route, 0) == p->current_area) {
        p->hint.unk_42 = (int8_t)elevator(route, 4);
        *x = elevator(route, 2);
        o->x = (int16_t)(*x * HALF_BLOCK);
        *y = elevator(route, 3);
        o->y = (int16_t)((*y + 4) * HALF_BLOCK);
    } else {
        p->hint.unk_42 = (int8_t)elevator(route, 0);
        *x = elevator(route, 6);
        o->x = (int16_t)(*x * HALF_BLOCK);
        *y = elevator(route, 7);
        o->y = (int16_t)((*y - 3) * HALF_BLOCK);
    }
    o->oam_id = direction < 0 ? TARGET_OAM_UP_ARROW : TARGET_OAM_DOWN_ARROW;
}

/* ChozoStatueHintDeterminePath: the targets' icons on the shown area's map,
 * the hint's start and end (or the elevators towards its area). */
static void hint_determine_path(MzmPauseScreen *p, const MzmPauseGame *g, bool next_area) {
    MzmMenuOam empty = mzm_menu_oam_from_rom(sym("sMenuOamDataChozoHint_Empty"));
    empty.priority = p->type_flags & TYPE_ON_MAP_SCREEN ? 0 : 3;
    for (int i = 0; i < 12; ++i) p->targets[i] = empty;
    for (int i = 0; i < 5; ++i) p->chozo_hint[i] = empty;
    if (!next_area) {
        p->hint.unk_41 = 0;
        p->hint.index = -1;
        p->hint.unk_42 = -1;
        check_activated_targets(p, g);
    }
    MzmMenuOam *o = p->targets;
    uint32_t paths = mzm_rom_u32(sym("sChozoStatueTargetPathPointers") + (uint32_t)(g->area < 6 ? g->area : 0) * 4u);
    if (p->type_flags & TYPE_CHOZO_STATUE_HINT) {
        if (!next_area) {
            for (int i = 0; i < TARGETS; ++i) {
                if (!(p->hint.activated >> i & 1)) continue;
                HintTarget t = hint_target(i);
                if (t.statue_area != p->current_area || t.statue_y_start > p->map_y || p->map_y > t.statue_y_end ||
                    t.statue_x_start > p->map_x || p->map_x > t.statue_x_end)
                    continue;
                p->hint.index = (int8_t)i;
                break;
            }
        }
        if (p->hint.index < 0) return;
        HintTarget t = hint_target(p->hint.index);
        if (t.statue_area == t.target_area) {
            o->x = (int16_t)(p->map_x * HALF_BLOCK);
            o->y = (int16_t)(p->map_y * HALF_BLOCK);
            o->oam_id = t.start_icon;
            o++;
            p->hint_target_x = t.target_x;
            p->hint_target_y = t.target_y;
            o->oam_id = t.end_icon;
            o->x = (int16_t)(p->hint_target_x * HALF_BLOCK);
            o->y = (int16_t)(p->hint_target_y * HALF_BLOCK);
            o++;
        } else if (paths) {
            uint32_t path = paths + t.target_area * 6u;
            if (p->current_area == t.statue_area) {
                o->x = (int16_t)(p->map_x * HALF_BLOCK);
                o->y = (int16_t)(p->map_y * HALF_BLOCK);
                o->oam_id = t.start_icon;
                o++;
                hint_change_area(p, &p->hint_target_x, &p->hint_target_y, o, path);
                o++;
            } else if (p->hint.unk_42 == (int8_t)t.target_area) {
                hint_change_area(p, &p->map_x, &p->map_y, o, path + (uint32_t)(p->hint.unk_41 - 1) * 2u);
                o->exists = 1;
                o++;
                p->hint_target_x = t.target_x;
                p->hint_target_y = t.target_y;
                o->x = (int16_t)(t.target_x * HALF_BLOCK);
                o->y = (int16_t)(t.target_y * HALF_BLOCK);
                o->oam_id = t.end_icon;
                o++;
                p->hint.unk_42 = (int8_t)AREA_NONE;
            } else {
                hint_change_area(p, &p->map_x, &p->map_y, o, path + (uint32_t)(p->hint.unk_41 - 1) * 2u);
                o->exists = 1;
                o++;
                hint_change_area(p, &p->hint_target_x, &p->hint_target_y, o, path + (uint32_t)p->hint.unk_41 * 2u);
                o++;
            }
        }
    } else if (p->hint.activated && g->area < 6 && paths) {
        /* The pause: the elevators towards the first item target's area. */
        int i;
        for (i = 0; i < 15; ++i) {
            if (!(p->hint.activated >> i & 1)) continue;
            uint8_t type = rom8("sChozoStatueTargetConditions", (uint32_t)i * 2u);
            if (type == HINT_CONDITION_BEAM_BOMBS || type == HINT_CONDITION_SUIT_MISC) break;
        }
        HintTarget t = hint_target(i);
        if (t.target_area <= 2) {
            uint32_t path = paths + t.target_area * 6u;
            for (int k = 0; k < 6; k += 2) {
                int route = (int8_t)mzm_rom_u8(path + (uint32_t)k);
                int8_t direction = (int8_t)mzm_rom_u8(path + (uint32_t)k + 1);
                if (elevator(route, 0) == p->current_area) {
                    o->x = (int16_t)(elevator(route, 2) * HALF_BLOCK);
                    o->y = (int16_t)((elevator(route, 3) + 4) * HALF_BLOCK);
                    o->exists = 1;
                    o->oam_id = direction < 0 ? TARGET_OAM_UP_ARROW : TARGET_OAM_DOWN_ARROW;
                    o++;
                }
                if (elevator(route, 4) == p->current_area) {
                    o->x = (int16_t)(elevator(route, 6) * HALF_BLOCK);
                    o->y = (int16_t)((elevator(route, 7) - 3) * HALF_BLOCK);
                    o->exists = 1;
                    o->oam_id = direction < 0 ? TARGET_OAM_UP_ARROW : TARGET_OAM_DOWN_ARROW;
                    o++;
                }
            }
        }
    }
    for (int i = 0; i < TARGETS && o < p->targets + 12; ++i) {
        if (i == p->hint.index || !(p->hint.activated >> i & 1)) continue;
        HintTarget t = hint_target(i);
        if (t.target_area != p->current_area) continue;
        o->oam_id = t.end_icon;
        o->x = (int16_t)(t.target_x * HALF_BLOCK);
        o->y = (int16_t)(t.target_y * HALF_BLOCK);
        o->exists = 1;
        o++;
    }
    draw_boss_flames(p);
}

/* ChozoStatueHintCalculateCoordinates: the next position (y << 16 | x). */
static uint32_t hint_coordinates(MzmChozoHintMapData *d) {
    int32_t x = 0, y = 0;
    if (d->dir_x != 0) {
        if (d->map_x != d->target_x) {
            x = d->dist_x * d->unk_10 * d->speed * 256 / d->abs_dist;
            x = d->map_x + x / 256 * d->dir_x;
            if (d->dir_x > 0 ? d->target_x < x : d->target_x > x) d->dir_x = 0;
        } else {
            d->dir_x = 0;
        }
    }
    if (d->dir_y != 0) {
        if (d->map_y != d->target_y) {
            y = d->dist_y * d->unk_10 * d->speed * 256 / d->abs_dist;
            y = d->map_y + y / 256 * d->dir_y;
            if (d->dir_y > 0 ? d->target_y < y : d->target_y > y) d->dir_y = 0;
        } else {
            d->dir_y = 0;
        }
    }
    if (d->dir_x == 0) x = d->target_x;
    if (d->dir_y == 0) y = d->target_y;
    return (uint32_t)(uint16_t)y << 16 | (uint16_t)x;
}

/* The straight line of a movement or a scrolling (their CALCULATE stage). */
static void hint_line(MzmChozoHintMapData *d) {
    d->dist_x = (int16_t)(d->target_x - d->map_x);
    if (d->dist_x == 0) {
        d->dir_y = 0;
    } else {
        d->dir_x = d->dist_x > 0 ? 1 : -1;
        d->dist_x = (int16_t)(d->dist_x * d->dir_x);
    }
    d->dist_y = (int16_t)(d->target_y - d->map_y);
    if (d->dist_y == 0) {
        d->dir_y = 0;
    } else {
        d->dir_y = d->dist_y > 0 ? 1 : -1;
        d->dist_y = (int16_t)(d->dist_y * d->dir_y);
    }
    uint32_t norm = isqrt((uint32_t)(d->dist_x * d->dist_x + d->dist_y * d->dist_y) * 4u) / 4u;
    if (norm == 0) d->dir_y = d->dir_x = 0;
    d->abs_dist = (uint16_t)norm;
    d->unk_10 = 1;
    d->speed = 0;
}

/* ChozoStatueHintMovement: the target travels from the statue, leaving a
 * trail. */
static void hint_movement(MzmPauseScreen *p) {
    MzmChozoHintMapData *d = &p->hint_movement;
    switch (p->hint.movement_stage) {
    case MOVE_CALCULATE: {
        p->hint.flags |= TARGET_MOVEMENT_FLAG;
        d->map_x = (uint16_t)(p->map_x * HALF_BLOCK);
        d->map_y = (uint16_t)(p->map_y * HALF_BLOCK);
        d->target_x = (uint16_t)(p->hint_target_x * HALF_BLOCK);
        d->target_y = (uint16_t)(p->hint_target_y * HALF_BLOCK);
        p->chozo_hint[0].y = (int16_t)d->map_y;
        p->chozo_hint[0].x = (int16_t)d->map_x;
        p->chozo_hint[0].oam_id = hint_target(p->hint.index).start_icon;
        hint_line(d);
        p->hint.movement_timer = 0;
        if (p->hint.index < TARGET_ITEM_COUNT || p->hint.unk_41 != 0) {
            p->chozo_hint[0].exists = OAM_ID_CHANGED;
            p->hint.movement_stage++;
        } else {
            p->hint.movement_stage = MOVE_DELAY_BEFORE_CONTINUE;
        }
        break;
    }
    case MOVE_DELAY_BEFORE:
        if (++p->hint.movement_timer > 60) {
            p->hint.scrolling_stage = SCROLL_CALCULATE;
            p->hint.movement_timer = 0;
            p->hint.movement_stage++;
        }
        break;
    case MOVE_UPDATE: {
        p->hint.movement_timer++;
        int dx = (d->target_x - p->chozo_hint[0].x) * d->dir_x;
        int dy = (d->target_y - p->chozo_hint[0].y) * d->dir_y;
        if (dy < dx) dy = dx;
        int far = dy >> 2;
        d->speed = (uint16_t)(d->speed + (far <= 0x20 ? 2 : far <= 0x40 ? 3 : far <= 0x60 ? 4 : 5));
        if ((p->hint.movement_timer & 7) == 0) {
            int n = (p->hint.movement_timer >> 3 & 3) + 1;
            p->chozo_hint[n] = p->chozo_hint[0];
            if (p->hint.index < TARGET_ITEM_COUNT) {
                p->chozo_hint[n].oam_id = 7;
                p->chozo_hint[n].exists = OAM_ID_CHANGED;
            } else {
                p->chozo_hint[n].oam_id = p->hint.index == TARGET_KRAID_FLAME ? 0xB : 0xF;
                p->chozo_hint[n].animation_duration_counter = 3;
                p->chozo_hint[n].current_animation_frame = 5;
                p->chozo_hint[n].exists = 1;
            }
        }
        uint32_t at = hint_coordinates(d);
        p->chozo_hint[0].y = (int16_t)(at >> 16);
        p->chozo_hint[0].x = (int16_t)(at & 0xFFFF);
        bool y_done = (uint16_t)(at >> 16) == d->target_y, x_done = (uint16_t)at == d->target_x;
        if (y_done) d->dir_y = 0;
        if (x_done) {
            d->dir_x = 0;
            if (y_done) {
                p->hint.movement_timer = 0;
                p->hint.movement_stage++;
            }
        }
        break;
    }
    case MOVE_DELAY_AFTER:
        if (++p->hint.movement_timer > 20) {
            p->targets[1].exists = OAM_ID_CHANGED;
            p->chozo_hint[0].exists = 0;
            p->hint.movement_timer = 0;
            p->hint.movement_stage++;
        }
        break;
    case MOVE_DELAY_BEFORE_FINISHED:
        if (++p->hint.movement_timer > 20) {
            p->hint.movement_timer = 0;
            p->hint.movement_stage = MOVE_FINISHED;
            p->hint.flags ^= TARGET_MOVEMENT_FLAG;
        }
        break;
    case MOVE_DELAY_BEFORE_CONTINUE:
        if (++p->hint.movement_timer > 60) {
            p->chozo_hint[0].exists = OAM_ID_CHANGED;
            p->hint.movement_timer = 0;
            p->hint.movement_stage = MOVE_DELAY_BEFORE;
        }
        break;
    default:
        break;
    }
}

/* ChozoStatueHintScrolling: the map follows the target. */
static void hint_scrolling(MzmPauseScreen *p, MzmScreen *s) {
    MzmChozoHintMapData *d = &p->hint_scrolling;
    switch (p->hint.scrolling_stage) {
    case SCROLL_CALCULATE:
        p->hint.flags |= TARGET_SCROLLING_FLAG;
        d->target_y = (uint16_t)((0x204 - (10 - p->hint_target_y) * 8) * 4);
        d->target_x = (uint16_t)((0x204 - (15 - p->hint_target_x) * 8) * 4);
        d->map_y = s->bg_vofs[3];
        d->map_x = s->bg_hofs[3];
        hint_line(d);
        p->hint.scrolling_stage++;
        break;
    case SCROLL_DELAY_BEFORE:
        if (++p->hint.scrolling_timer > 20) {
            p->hint.scrolling_timer = 0;
            p->hint.scrolling_stage++;
        }
        break;
    case SCROLL_UPDATE: {
        d->speed = (uint16_t)(d->speed + 4);
        uint32_t at = hint_coordinates(d);
        s->bg_vofs[3] = (uint16_t)(at >> 16);
        s->bg_hofs[3] = (uint16_t)at;
        bool y_done = (uint16_t)(at >> 16) == d->target_y, x_done = (uint16_t)at == d->target_x;
        if (y_done) d->dir_y = 0;
        if (x_done) d->dir_x = 0;
        if (y_done && x_done) {
            p->hint.scrolling_timer = 0;
            p->hint.scrolling_stage++;
        }
        break;
    }
    case SCROLL_DELAY_BEFORE_FINISHED:
        if (++p->hint.scrolling_timer > 20) {
            p->hint.scrolling_timer = 0;
            p->hint.scrolling_stage = SCROLL_FINISHED;
            p->hint.flags ^= TARGET_SCROLLING_FLAG;
        }
        break;
    default:
        break;
    }
}

static void set_spawn_position(MzmPauseScreen *p, MzmScreen *s, int option);
static void update_boss_icons(MzmPauseScreen *p, const MzmPauseGame *g);

/* ChozoStatueHintHandler: true once A or START ends the hint. */
static bool chozo_hint(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g) {
    hint_movement(p);
    hint_scrolling(p, s);
    switch (p->stage) {
    case HINT_DELAY_BEFORE_UPDATE:
        if (p->timer > 40) {
            p->stage++;
            p->timer = 0;
        }
        break;
    case HINT_BEGIN_UPDATE:
        p->hint.movement_stage = MOVE_CALCULATE;
        p->stage++;
        p->timer = 0;
        break;
    case HINT_WAIT_UPDATE_FINISHED:
        if (!(p->hint.flags & (TARGET_SCROLLING_FLAG | TARGET_MOVEMENT_FLAG))) {
            p->stage++;
            p->timer = 0;
        }
        break;
    case HINT_CHECK_FINISHED:
        p->stage = p->hint.unk_42 > -1 ? (uint8_t)(p->stage + 1) : HINT_DELAY_BEFORE_FINISHED;
        p->timer = 0;
        break;
    case HINT_DELAY_BEFORE_FADE_OUT:
        if (p->timer > 60) {
            p->bldcnt |= BLDCNT_BG3_FIRST | BLDCNT_OBJ_FIRST;
            p->stage++;
            p->timer = 0;
        }
        break;
    case HINT_UPDATE_FADE_OUT:
        if (s->bldalpha_h + s->bldalpha_l != 0) {
            if (s->bldalpha_h) s->bldalpha_h--;
            if (s->bldalpha_l) s->bldalpha_l--;
        } else {
            p->stage++;
            p->timer = 0;
        }
        break;
    case HINT_SETUP_NEXT_AREA:
        /* The next area on the path. */
        p->hint.unk_41++;
        p->current_area = (uint8_t)p->hint.unk_42;
        vram_from16(s, 0xE000, p->maps[p->current_area < AREAS ? p->current_area : AREA_TEST], 2048);
        hint_determine_path(p, g, true);
        set_spawn_position(p, s, 1);
        mzm_menu_oam_set_id(&p->overlay[0], rom8("sChozoHintAreaNamesOamIds", p->current_area));
        update_boss_icons(p, g);
        p->stage++;
        p->timer = 0;
        break;
    case HINT_UPDATE_FADE_IN:
        if ((p->target_bld_alpha >> 8 & 0xFF) != s->bldalpha_h || (p->target_bld_alpha & 0xFF) != s->bldalpha_l) {
            if ((p->target_bld_alpha >> 8 & 0xFF) > s->bldalpha_h) s->bldalpha_h++;
            if ((p->target_bld_alpha & 0xFF) > s->bldalpha_l) s->bldalpha_l++;
        } else {
            p->bldcnt &= (uint16_t)~(BLDCNT_BG3_FIRST | BLDCNT_OBJ_FIRST);
            p->stage = HINT_DELAY_BEFORE_UPDATE;
            p->timer = 0;
        }
        break;
    case HINT_DELAY_BEFORE_FINISHED:
        if (p->timer > 40) {
            p->stage++;
            p->timer = 0;
        }
        break;
    case HINT_FINISHED:
        return (s->changed_input & (KEY_START | KEY_A)) != 0;
    default:
        break;
    }
    return false;
}

/* sStatusScreenGroupsPositions / sStatusScreenUnknownItemsPositions:
 * top, bottom, left, right, group */
typedef struct { uint8_t top, bottom, left, right; } Box;

static Box box(const char *table, int group) {
    uint32_t at = sym(table) + (uint32_t)group * 5u;
    return (Box){mzm_rom_u8(at), mzm_rom_u8(at + 1), mzm_rom_u8(at + 2), mzm_rom_u8(at + 3)};
}
static Box group_box(int group) { return box("sStatusScreenGroupsPositions", group); }
static Box unknown_box(int group) { return box("sStatusScreenUnknownItemsPositions", group); }

/* sStatusScreenFlagsOrderPointers[group][i], sStatusScreenFlagsSize */
static uint8_t flag_order(int group, int i) {
    return mzm_rom_u8(mzm_rom_u32(sym("sStatusScreenFlagsOrderPointers") + (uint32_t)group * 4u) + (uint32_t)i);
}
static int flags_size(int group) { return rom8("sStatusScreenFlagsSize", (uint32_t)group); }

/* sStatusScreenItemsData[slot]: group, abilityOffset, row */
static uint8_t item_data(int slot, int field) { return rom8("sStatusScreenItemsData", (uint32_t)(slot * 4 + field)); }

static void copy_row(uint16_t *tilemap, uint32_t dst, uint32_t src, const Box *b) {
    for (int i = 0; i <= b->right - b->left; ++i) tilemap[dst + (uint32_t)i] = tilemap[src + (uint32_t)i];
}

/* StatusScreenUpdateRow */
static void update_row(MzmPauseScreen *p, MzmScreen *s, int group, int row, bool activated, bool draw) {
    Box b = group_box(group);
    uint32_t position = (uint32_t)(row + b.top) * STATUS_ROW + b.left;
    uint16_t base = activated ? 11 << 12 : 12 << 12;
    for (int i = 1; i < b.right - b.left; ++i) {
        uint16_t *t = &p->status_tilemap[position + (uint32_t)i];
        *t = (uint16_t)((*t & 0xFFF) | base);
        if (draw) {
            uint16_t *v = vram16(s, 0xC000) + position + i;
            *v = (uint16_t)((*v & 0xFFF) | base);
        }
    }
}

/* StatusScreenDrawSingleTankAmount */
static void draw_tank_amount(MzmPauseScreen *p, int group, int amount, uint16_t palette, bool is_max) {
    uint16_t base = group == GROUP_CURRENT_ENERGY || group == GROUP_MAX_ENERGY ? 0xB2E0 : 0xB08C;
    Box b = group_box(group);
    uint16_t *t = &p->status_tilemap[b.top * STATUS_ROW + b.left];
    int digits = b.right - b.left;
    int value = rom16("sPowersOfTen", (uint32_t)digits);
    digits++;
    bool started = false;
    int i = 0;
    for (; value > 0; value /= 10, i++) {
        int tile = amount / value % 10;
        if (tile == 0) {
            tile = 128;
            if (value != 1 && !started) {
                if (!is_max) {
                    tile = base & 0xFFF;
                } else {
                    i--;
                    continue;
                }
            }
        } else {
            started = true;
            tile += 128;
        }
        t[i] = (uint16_t)(palette << 12 | tile);
    }
    while (digits != i) t[i++] = (uint16_t)((base & 0xFFF) | palette << 12);
}

/* StatusScreenSetPistolVisibility */
static void set_pistol_visibility(MzmPauseScreen *p, MzmScreen *s) {
    Box b = group_box(GROUP_BEAMS), u = unknown_box(GROUP_BEAMS);
    p->status.beam[0] = flag_order(GROUP_BEAMS, 0);
    copy_row(p->status_tilemap, (b.top + 1u) * STATUS_ROW + b.left, u.top * STATUS_ROW + u.left, &b);
    update_row(p, s, GROUP_BEAMS, 1, true, false);
    if (p->status.unk_0 == 0) p->status.unk_0 = 0x80;
    if (p->status.current_slot == SLOT_0) p->status.current_slot = SLOT_BEAM;
    copy_row(p->status_tilemap, (b.top + 2u) * STATUS_ROW + b.left, (u.top + 1u) * STATUS_ROW + u.left, &b);
}

/* StatusScreenSetBeamsVisibility and StatusScreenSetSuitsVisibility (the
 * plasma beam and the gravity suit stay unknown items before the fully
 * powered suit) */
static void set_group_visibility(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g, int group) {
    Abilities a = abilities(g);
    bool beams = group == GROUP_BEAMS;
    uint8_t *visibility = beams ? p->status.beam : p->status.suit;
    uint8_t have = beams ? a.beam_bombs : a.suit_misc;
    uint8_t on = beams ? a.beam_bombs_activation : a.suit_misc_activation;
    Box b = group_box(group), u = unknown_box(group);
    int size = flags_size(group);
    for (int i = 0; i < size; ++i) visibility[i] = 0;
    int row = 0;
    for (int i = 0; i < size; ++i) {
        uint8_t flag = flag_order(group, i);
        int j = i;
        uint32_t src = 0;
        if (g->equipment->suit != MZM_SUIT_FULLY_POWERED || g->flag == MZM_PAUSE_FLAG_FULLY_POWERED_SUIT_ITEMS) {
            if (beams && flag == BBF_PLASMA_BEAM) j = 9, src = 10;
            if (!beams && flag == SMF_GRAVITY_SUIT) j = 3;
        }
        src += (uint32_t)(u.top + j) * STATUS_ROW + u.left;
        uint32_t dst = (uint32_t)(b.top + 1 + row) * STATUS_ROW + b.left;
        if (!(have & flag)) continue;
        visibility[row] = flag;
        copy_row(p->status_tilemap, dst, src, &b);
        update_row(p, s, group, row + 1, on & visibility[row], false);
        row++;
    }
    if (row == 0) return;
    if (p->status.unk_0 == 0) p->status.unk_0 = beams ? 0x80 : 0x82;
    if (p->status.current_slot == SLOT_0) p->status.current_slot = beams ? SLOT_BEAM : SLOT_SUIT;
    row++;
    copy_row(p->status_tilemap, (uint32_t)(b.top + row) * STATUS_ROW + b.left, u.bottom * STATUS_ROW + u.left, &b);
}

/* StatusScreenSetMiscsVisibility (the space jump stays an unknown item
 * before the fully powered suit and during its items sequence) */
static void set_miscs_visibility(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g) {
    Abilities a = abilities(g);
    Box b = group_box(GROUP_MISC), u = unknown_box(GROUP_MISC);
    int size = flags_size(GROUP_MISC);
    for (int i = 0; i < size; ++i) p->status.misc[i] = 0;
    int j = 0;
    for (int i = 0; i < size; ++i) {
        uint8_t flag = flag_order(GROUP_MISC, i);
        int k = i + 1;
        if (flag == SMF_SPACE_JUMP &&
            (g->equipment->suit != MZM_SUIT_FULLY_POWERED || g->flag == MZM_PAUSE_FLAG_FULLY_POWERED_SUIT_ITEMS))
            k = -1;
        uint32_t src = (uint32_t)(u.top + k) * STATUS_ROW + u.left;
        uint32_t dst = (uint32_t)(b.top + 1 + j) * STATUS_ROW + b.left;
        if (!(a.suit_misc & flag)) continue;
        p->status.misc[j] = flag;
        copy_row(p->status_tilemap, dst, src, &b);
        update_row(p, s, GROUP_MISC, j + 1, a.suit_misc_activation & p->status.misc[j], false);
        j++;
    }
    if (j == 0) return;
    if (p->status.unk_0 == 0) p->status.unk_0 = 0x83;
    if (p->status.current_slot == SLOT_0) p->status.current_slot = SLOT_MISC;
    copy_row(p->status_tilemap, b.top * STATUS_ROW + b.left, u.top * STATUS_ROW + u.left, &b);
    j++;
    copy_row(p->status_tilemap, (uint32_t)(b.top + j) * STATUS_ROW + b.left, u.bottom * STATUS_ROW + u.left, &b);
}

/* StatusScreenSetBombsVisibility */
static void set_bombs_visibility(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g) {
    Abilities a = abilities(g);
    const MzmEquipment *e = g->equipment;
    Box b = group_box(GROUP_BOMBS), u = unknown_box(GROUP_BOMBS);
    uint8_t bombs = flag_order(GROUP_BOMBS, 0);
    p->status.bomb[0] = p->status.bomb[1] = 0;
    int count = 0;
    if (a.beam_bombs & bombs) {
        count = 2;
        p->status.bomb[0] |= BOMB_COLLECTED;
    }
    if (count == 0) return;
    if (e->max_power_bombs != 0) {
        count = 3;
        p->status.bomb[1] |= BOMB_COLLECTED;
    }
    for (int i = 0; i < count; ++i) {
        uint32_t dst = (uint32_t)(b.top + i) * STATUS_ROW + b.left;
        uint32_t src = (uint32_t)(u.top + rom8("sPauseScreen_7603ea", (uint32_t)i)) * STATUS_ROW + u.left;
        copy_row(p->status_tilemap, dst, src, &b);
        int value = 0;
        if (i == 1 || i == 2) {
            uint8_t *bomb = &p->status.bomb[i - 1];
            if (*bomb & BOMB_COLLECTED) {
                if (a.beam_bombs_activation & bombs) *bomb |= BOMB_ACTIVATED;
                if (i == 1 || e->power_bombs != 0) *bomb |= BOMB_HAS_AMMO;
                if (a.suit_misc_activation & SMF_MORPH_BALL) *bomb |= BOMB_MORPH_ACTIVATED;
            }
            value = *bomb;
        }
        if (value == 0) continue;
        bool on = value == (BOMB_MORPH_ACTIVATED | BOMB_HAS_AMMO | BOMB_ACTIVATED | BOMB_COLLECTED);
        /* The power bomb being acquired shows inactive */
        if (g->flag == MZM_PAUSE_FLAG_ITEM_ACQUISITION && g->item_being_acquired == ITEM_ACQUISITION_POWER_BOMB &&
            i == 2)
            on = false;
        update_row(p, s, GROUP_BOMBS, i, on, false);
    }
    uint32_t dst = (uint32_t)(b.top + count) * STATUS_ROW + b.left;
    if (count != 2) count++;
    copy_row(p->status_tilemap, dst, (uint32_t)(u.top + count) * STATUS_ROW + u.left, &b);
    if (p->status.current_slot == SLOT_0) p->status.current_slot = SLOT_BOMB;
}

/* StatusScreenSetMissilesVisibility */
static void set_missiles_visibility(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g) {
    const MzmEquipment *e = g->equipment;
    Box b = group_box(GROUP_MISSILES), u = unknown_box(GROUP_MISSILES);
    int count = 0;
    if (e->max_missiles != 0) {
        count = 3;
        p->status.missiles[0] |= MISSILE_COLLECTED;
    }
    if (e->max_super_missiles != 0) {
        count = 5;
        p->status.missiles[1] |= MISSILE_COLLECTED;
        p->status.missiles[0] |= MISSILE_COLLECTED;
    }
    if (count == 0) return;
    for (int i = 0; i < count; ++i) {
        copy_row(p->status_tilemap, (uint32_t)(b.top + i) * STATUS_ROW + b.left,
                 (uint32_t)(u.top + i) * STATUS_ROW + u.left, &b);
        int value = 0;
        if (i == 1) {
            if (p->status.missiles[0] & MISSILE_COLLECTED && e->missiles != 0) p->status.missiles[0] |= MISSILE_HAS_AMMO;
            value = p->status.missiles[0];
        } else if (i == 3) {
            if (p->status.missiles[1] & MISSILE_COLLECTED && e->super_missiles != 0)
                p->status.missiles[1] |= MISSILE_HAS_AMMO;
            value = p->status.missiles[1];
        }
        if (value == 0) continue;
        /* Not activated while the fully powered suit's items are shown */
        bool on = value == (MISSILE_HAS_AMMO | MISSILE_COLLECTED);
        /* The missiles being acquired show inactive */
        if (g->flag == MZM_PAUSE_FLAG_ITEM_ACQUISITION) {
            if ((i == 1 && g->item_being_acquired == ITEM_ACQUISITION_MISSILES) ||
                (i == 3 && g->item_being_acquired == ITEM_ACQUISITION_SUPER_MISSILES))
                on = false;
        } else if (g->flag == MZM_PAUSE_FLAG_FULLY_POWERED_SUIT_ITEMS) {
            on = false;
        }
        update_row(p, s, GROUP_MISSILES, i, on, false);
    }
    copy_row(p->status_tilemap, (uint32_t)(b.top + count) * STATUS_ROW + b.left, u.bottom * STATUS_ROW + u.left, &b);
    if (p->status.current_slot == SLOT_0) p->status.current_slot = SLOT_MISSILE;
}

/* StatusScreenIsStatusSlotEnabled */
static bool slot_enabled(const MzmPauseScreen *p, int slot) {
    int offset = item_data(slot, 1);
    switch (item_data(slot, 0)) {
    case GROUP_BEAMS: return p->status.beam[offset] != 0;
    case GROUP_MISSILES: return p->status.missiles[offset] != 0;
    case GROUP_BOMBS: return p->status.bomb[offset] != 0;
    case GROUP_SUITS: return p->status.suit[offset] != 0;
    case GROUP_MISC: return p->status.misc[offset] != 0;
    default: return false;
    }
}

/* StatusScreenDraw */
static void status_draw(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g) {
    const MzmEquipment *e = g->equipment;
    if (suitless(g)) {
        memcpy(p->status_tilemap, p->status_background_tilemap, sizeof p->status_tilemap);
        memset(&p->status, 0, sizeof p->status);
        set_pistol_visibility(p, s);
        draw_tank_amount(p, GROUP_CURRENT_ENERGY, e->energy, 11, false);
        draw_tank_amount(p, GROUP_MAX_ENERGY, e->max_energy, 11, true);
        return;
    }
    memcpy(p->status_tilemap, p->status_base_tilemap, sizeof p->status_tilemap);
    uint8_t previous[3] = {p->status.current_slot, p->status.previous_left_slot, p->status.previous_right_slot};
    memset(&p->status, 0, sizeof p->status);
    set_group_visibility(p, s, g, GROUP_BEAMS);
    set_group_visibility(p, s, g, GROUP_SUITS);
    set_bombs_visibility(p, s, g);
    set_miscs_visibility(p, s, g);
    set_missiles_visibility(p, s, g);
    draw_tank_amount(p, GROUP_CURRENT_ENERGY, e->energy, 11, false);
    draw_tank_amount(p, GROUP_MAX_ENERGY, e->max_energy, 11, true);
    if (e->max_missiles != 0) {
        draw_tank_amount(p, GROUP_CURRENT_MISSILES, e->missiles, 11, false);
        draw_tank_amount(p, GROUP_MAX_MISSILES, e->max_missiles, 11, true);
    }
    if (e->max_super_missiles != 0) {
        draw_tank_amount(p, GROUP_CURRENT_SUPER_MISSILES, e->super_missiles, 11, false);
        draw_tank_amount(p, GROUP_MAX_SUPER_MISSILES, e->max_super_missiles, 11, true);
    }
    if (e->max_power_bombs != 0) {
        draw_tank_amount(p, GROUP_CURRENT_POWER_BOMBS, e->power_bombs, 11, false);
        draw_tank_amount(p, GROUP_MAX_POWER_BOMBS, e->max_power_bombs, 11, true);
    }
    if (slot_enabled(p, previous[0])) p->status.current_slot = previous[0];
    if (slot_enabled(p, previous[1])) p->status.previous_left_slot = previous[1];
    if (slot_enabled(p, previous[2])) p->status.previous_right_slot = previous[2];
}

/* StatusScreenDrawItems: one row of every group is revealed a frame. */
static bool draw_items(MzmPauseScreen *p, MzmScreen *s, uint8_t row) {
    if (row >= 8) return true;
    uint32_t dimensions = sym("sStatusScreenGroupsDimensions");
    for (uint32_t i = 0; i < 6; ++i) {
        uint8_t group = mzm_rom_u8(dimensions + i * 3), rows = mzm_rom_u8(dimensions + i * 3 + 1),
                width = mzm_rom_u8(dimensions + i * 3 + 2);
        if (row == 0 && group == GROUP_SUITS) continue;
        Box b = group_box(group);
        uint32_t position = (uint32_t)(b.top + row) * STATUS_ROW + b.left;
        if (rows <= row) continue;
        for (uint32_t j = 0; j < width; ++j) vram16(s, 0xC000)[position + j] = p->status_tilemap[position + j];
    }
    return row >= 7;
}

/* PauseScreenInitFading */
static bool init_fading(MzmPauseScreen *p, uint16_t target, uint8_t step) {
    if (p->is_fading || step == 0) return false;
    p->is_fading = 1;
    p->bld_alpha_step_level = step;
    p->target_l = target & 0x1F;
    p->target_h = target >> 8 & 0x1F;
    p->bld_alpha_step_timer = 0;
    return true;
}

static uint8_t fade_towards(uint8_t value, uint8_t target, uint8_t step) {
    if (value > target) return value - target > step ? (uint8_t)(value - step) : target;
    if (value < target) return value + step > target ? target : (uint8_t)(value + step);
    return value;
}

/* PauseScreenApplyFading (the step delay threshold is always 0) */
static void apply_fading(MzmPauseScreen *p, MzmScreen *s) {
    if (!p->is_fading) return;
    p->bld_alpha_step_timer = 0;
    s->bldalpha_l = fade_towards(s->bldalpha_l, p->target_l, p->bld_alpha_step_level);
    s->bldalpha_h = fade_towards(s->bldalpha_h, p->target_h, p->bld_alpha_step_level);
    if (s->bldalpha_l == p->target_l && s->bldalpha_h == p->target_h) p->is_fading = 0;
}

/* PauseScreenUpdateTopVisorOverlay */
static void update_top_visor(MzmPauseScreen *p, MzmScreen *s, uint8_t oam_id) {
    if (oam_id != 0xFF) mzm_menu_oam_set_id(&p->overlay[1], oam_id);
    int offset = oam_id == OVERLAY_R_PROMPT_PRESSED ? 1 : oam_id == OVERLAY_L_PROMPT_PRESSED ? 2 : oam_id == 0 ? 0 : 3;
    vram_from16(s, 0xC800, &p->visor_overlay[0x280 + offset * 0x40], 0x80);
}

/* PauseScreenUpdateWireframeSamus: option 0 for a new item (the suitless
 * wireframe before the fully powered suit's), 1 without a suit change, 2
 * starts the fade to the new suit (PauseScreenFadeWireframeSamus). */
static void update_wireframe(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g, int option) {
    if (option == 2) {
        if (p->fade_wireframe_stage == 0) {
            p->fade_wireframe_stage = 1;
            p->fade_wireframe_timer = 0;
        }
        return;
    }
    uint8_t oam_id = MISC_POWER_SUIT_WIREFRAME;
    if (g->equipment->suit == MZM_SUIT_FULLY_POWERED) oam_id = MISC_FULL_SUIT_WIREFRAME;
    else if (suitless(g)) oam_id = MISC_SUITLESS_WIREFRAME;
    if (option == 0 && g->flag == MZM_PAUSE_FLAG_FULLY_POWERED_SUIT_ITEMS) oam_id = MISC_SUITLESS_WIREFRAME;
    p->misc[8].oam_id = oam_id;
    p->misc[8].exists = OAM_ID_CHANGED;
    p->misc[9] = p->misc[8];
    p->misc[9].obj_mode = 2;
    Abilities a = abilities(g);
    int palette = oam_id == MISC_SUITLESS_WIREFRAME           ? 3
                  : a.suit_misc_activation & SMF_GRAVITY_SUIT ? 2
                  : a.suit_misc_activation & SMF_VARIA_SUIT   ? 1
                                                              : 0;
    mzm_copy_from_rom(&s->ppu->palette[256 + 16 * 4], sym("sSamusWireframePal") + (uint32_t)palette * 32u, 32);
}

/* PauseScreenStatusScreenShouldDrawHeader */
static uint8_t should_draw_header(MzmPauseScreen *p, const MzmPauseGame *g, int part) {
    if (suitless(g)) {
        if (part == 1) p->misc[3].oam_id = MISC_GUN_HEADER;
        return part == 0 || part == 1 ? 2 : 0;
    }
    switch (part) {
    case 2: return g->equipment->max_missiles + g->equipment->max_super_missiles != 0 ? 2 : 0;
    case 3: return p->status.bomb[0] ? 2 : 0;
    case 5: return p->status.misc[0] ? 2 : 0;
    case 0: case 1: case 4: return 2;
    default: return 0;
    }
}

/* sSamusWireframeData[part]: oamId, xPosition, yPosition, xOffset,
 * objMode, xPosition2, yPosition2 */
static int16_t wireframe(int part, uint32_t offset) {
    return (int16_t)mzm_rom_u16(sym("sSamusWireframeData") + (uint32_t)part * 16u + offset);
}

/* PauseScreenUpdateStatusScreenOam */
static bool update_status_oam(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g, int option) {
    bool result = false;
    MzmMenuOam *o = &p->misc[2];
    switch (option) {
    case 0:
        /* PAUSE_SCREEN_TYPE_GETTING_NEW_ITEM: the parts at their final
         * positions */
        for (int i = 0; i < WIREFRAME_PARTS; ++i, ++o) {
            o->oam_id = (uint8_t)wireframe(i, 0);
            o->x = wireframe(i, 2);
            o->y = wireframe(i, 4);
            o->obj_mode = (uint8_t)wireframe(i, 8);
            o->exists = should_draw_header(p, g, i);
        }
        update_wireframe(p, s, g, 0);
        break;
    case 1:
        for (int i = 0; i < WIREFRAME_PARTS; ++i, ++o) {
            o->oam_id = (uint8_t)wireframe(i, 0);
            o->x = (int16_t)(wireframe(i, 2) + wireframe(i, 6));
            o->y = wireframe(i, 4);
            o->obj_mode = (uint8_t)wireframe(i, 8);
            o->exists = should_draw_header(p, g, i);
        }
        update_wireframe(p, s, g, 1);
        p->dispcnt |= GBA_DCNT_OBJWIN;
        p->samus_icon[0].exists = 0;
        break;
    case 2:
        result = true;
        for (int i = 0; i < WIREFRAME_PARTS - 1; ++i, ++o) {
            if (!o->exists) continue;
            int16_t x = wireframe(i, 2);
            if (o->x != x) {
                result = false;
                o->x = (int16_t)(o->x - BLOCK);
                if (o->x < x) o->x = x;
            }
        }
        break;
    case 3:
        o->exists = 0;
        ++o;
        for (int i = 1; i < WIREFRAME_PARTS - 1; ++i, ++o) {
            if (!o->exists) continue;
            o->exists = 2;
            o->oam_id++;
            o->x = wireframe(i, 10);
            o->y = wireframe(i, 12);
        }
        break;
    case 4:
        for (int i = 0; i < WIREFRAME_PARTS - 1; ++i, ++o) {
            if (!o->exists) continue;
            o->obj_mode = 1;
            if (i == 0) continue;
            o->oam_id++;
            o->animation_duration_counter = 0;
            if (o->current_animation_frame) o->current_animation_frame--;
        }
        break;
    case 5:
        for (int i = 2; i <= 9; ++i) p->misc[i].exists = 0;
        p->dispcnt &= (uint16_t)~GBA_DCNT_OBJWIN;
        p->samus_icon[0].exists = OAM_ID_CHANGED;
        break;
    default:
        break;
    }
    return result;
}

/* StatusScreenUpdateCursorPosition */
static void update_cursor_position(MzmPauseScreen *p, int slot) {
    Box b = group_box(item_data(slot, 0));
    p->misc[0].y = (int16_t)((b.top + item_data(slot, 2)) * HALF_BLOCK);
    p->misc[0].x = (int16_t)((b.left + 1) * HALF_BLOCK);
}

/* The equipment item of a gEquipment.beamBombs or suitMisc flag. */
static uint32_t item_of_flag(bool beam_bombs, uint8_t flag) {
    static const struct { uint8_t flag; bool beam_bombs; uint32_t item; } map[] = {
        {BBF_LONG_BEAM, true, MZM_ITEM_LONG_BEAM}, {BBF_ICE_BEAM, true, MZM_ITEM_ICE_BEAM},
        {BBF_WAVE_BEAM, true, MZM_ITEM_WAVE_BEAM}, {BBF_PLASMA_BEAM, true, MZM_ITEM_PLASMA_BEAM},
        {BBF_CHARGE_BEAM, true, MZM_ITEM_CHARGE_BEAM}, {BBF_BOMBS, true, MZM_ITEM_BOMBS},
        {SMF_HIGH_JUMP, false, MZM_ITEM_HIGH_JUMP}, {SMF_SPEEDBOOSTER, false, MZM_ITEM_SPEED_BOOSTER},
        {SMF_SPACE_JUMP, false, MZM_ITEM_SPACE_JUMP}, {SMF_SCREW_ATTACK, false, MZM_ITEM_SCREW_ATTACK},
        {SMF_VARIA_SUIT, false, MZM_ITEM_VARIA_SUIT}, {SMF_GRAVITY_SUIT, false, MZM_ITEM_GRAVITY_SUIT},
        {SMF_MORPH_BALL, false, MZM_ITEM_MORPH_BALL}, {SMF_POWER_GRIP, false, MZM_ITEM_POWER_GRIP},
    };
    uint32_t items = 0;
    for (size_t i = 0; i < sizeof map / sizeof map[0]; ++i)
        if (map[i].beam_bombs == beam_bombs && flag & map[i].flag) items |= map[i].item;
    return items;
}

/* The activation (gEquipment.*Activation) of an obtained item set: an
 * inactive item is kept obtained among the unknown items. */
static void set_items_active(MzmEquipment *e, uint32_t items, bool active) {
    if (active) {
        e->items |= items;
        e->unknown_items &= ~items;
    } else {
        e->unknown_items |= e->items & items;
        e->items &= ~items;
    }
}

static void update_wireframe(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g, int option);
static void fade_wireframe(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g);

/* StatusScreenToggleItem: TRUE / FALSE whether the slot's item is on after
 * checking or toggling it, 0xFF when it cannot be toggled (only in the
 * suitless and item acquisition sequences can it be). */
static uint8_t toggle_item(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g, int slot, int action) {
    MzmEquipment *e = g->equipment;
    Abilities a = abilities(g);
    bool fully_powered = e->suit == MZM_SUIT_FULLY_POWERED;
    int group = item_data(slot, 0), offset = item_data(slot, 1);
    uint8_t flag = 0, activation = 0;
    bool beam_bombs = true;
    switch (group) {
    case GROUP_BEAMS:
        flag = p->status.beam[offset];
        activation = a.beam_bombs_activation;
        if (flag == BBF_PLASMA_BEAM && !fully_powered) flag = 0;
        break;
    case GROUP_BOMBS:
        if (slot == SLOT_BOMB && a.suit_misc_activation & SMF_MORPH_BALL && !(a.beam_bombs_activation & BBF_BOMBS)) {
            flag = BBF_BOMBS;
            activation = a.beam_bombs_activation;
        }
        break;
    case GROUP_SUITS:
        flag = p->status.suit[offset];
        activation = a.suit_misc_activation;
        beam_bombs = false;
        if (flag == SMF_GRAVITY_SUIT && !fully_powered) flag = 0;
        if (flag && activation & flag) flag = 0;
        break;
    case GROUP_MISC:
        flag = p->status.misc[offset];
        activation = a.suit_misc_activation;
        beam_bombs = false;
        if (flag == SMF_SPACE_JUMP && !fully_powered) flag = 0;
        break;
    default:
        break;
    }
    if (!(p->type_flags & TYPE_GETTING_SUITLESS) && g->flag != MZM_PAUSE_FLAG_ITEM_ACQUISITION) flag = 0;
    uint8_t activated = 0xFF, oam_id = 0;
    if (flag != 0) {
        if (action == TOGGLE_CHECKING || action == TOGGLE_CHECKING2) {
            activated = (activation & flag) != 0;
        } else {
            activated = !(activation & flag);
            set_items_active(e, item_of_flag(beam_bombs, flag), activated);
            update_row(p, s, group, item_data(slot, 2), activated, true);
            if (group == GROUP_MISC && flag & SMF_MORPH_BALL) {
                for (int i = 0; i < 2; ++i) {
                    uint8_t *bomb = &p->status.bomb[i];
                    if (!(*bomb & BOMB_COLLECTED)) continue;
                    *bomb = (uint8_t)((*bomb & (BOMB_HAS_AMMO | BOMB_ACTIVATED | BOMB_COLLECTED)) | activated << 3);
                    update_row(p, s, GROUP_BOMBS, i + 1,
                               *bomb == (BOMB_MORPH_ACTIVATED | BOMB_HAS_AMMO | BOMB_ACTIVATED | BOMB_COLLECTED), true);
                }
            } else if (group == GROUP_SUITS) {
                update_wireframe(p, s, g, 2);
            } else if (group == GROUP_BOMBS) {
                p->status.bomb[0] = (uint8_t)((p->status.bomb[0] & (BOMB_MORPH_ACTIVATED | BOMB_HAS_AMMO |
                                                                   BOMB_COLLECTED)) | activated * 2);
                if (p->status.bomb[1] & BOMB_COLLECTED) {
                    p->status.bomb[1] = (uint8_t)((p->status.bomb[1] & (BOMB_MORPH_ACTIVATED | BOMB_HAS_AMMO |
                                                                       BOMB_COLLECTED)) | activated * 2);
                    update_row(p, s, GROUP_BOMBS, 2,
                               p->status.bomb[1] ==
                                   (BOMB_MORPH_ACTIVATED | BOMB_HAS_AMMO | BOMB_ACTIVATED | BOMB_COLLECTED),
                               true);
                }
            }
        }
        if (group == GROUP_BOMBS || group == GROUP_SUITS) activated = 0xFF;
        else if (group == GROUP_MISC && flag == SMF_SPACE_JUMP && !fully_powered) activated = 0xFF;
        oam_id = activated == 1 ? OVERLAY_SELECT_ON : activated == 0 ? OVERLAY_SELECT_OFF : 0;
    }
    if (p->type_flags & TYPE_GETTING_NEW_ITEM) oam_id = 0;
    mzm_menu_oam_set_id(&p->overlay[3], oam_id);
    return activated;
}

/* StatusScreenInitCursorAndItems */
static void init_cursor_and_items(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g) {
    if (p->status.current_slot == SLOT_0) return;
    update_cursor_position(p, p->status.current_slot);
    if (!(p->type_flags & TYPE_GETTING_NEW_ITEM)) mzm_menu_oam_set_id(&p->misc[0], MISC_ITEM_CURSOR_FOCUSING);
    toggle_item(p, s, g, p->status.current_slot, TOGGLE_CHECKING);
}

/* StatusScreenGetDestinationSlot */
static uint8_t destination_slot(MzmPauseScreen *p, int offset, uint8_t previous) {
    int slot = previous, off = offset, upper, lower;
    if (off == 0) return (uint8_t)slot;
    if (off >= 2) {
        if (previous > 7) p->status.previous_right_slot = previous;
        else p->status.previous_left_slot = previous;
        if (previous > 7) {
            slot = p->status.previous_left_slot;
            if (slot != 0) return (uint8_t)slot;
            upper = 7;
            lower = 1;
            if (previous >= 12) slot = 6, off = -1;
            else slot = 1, off = 1;
        } else {
            slot = p->status.previous_right_slot;
            if (slot != 0) return (uint8_t)slot;
            upper = 17;
            lower = 8;
            if (previous > 5) slot = 12, off = -1;
            else slot = 8, off = 1;
        }
        int first = slot;
        while (!slot_enabled(p, slot)) {
            slot += off;
            if (slot < lower) slot = upper;
            else if (slot > upper) slot = lower;
            if (slot == first) {
                slot = previous;
                break;
            }
        }
    } else {
        if (previous > 7) upper = 17, lower = 8;
        else upper = 7, lower = 1;
        int first = slot;
        do {
            slot += off;
            if (slot < lower) slot = upper;
            else if (slot > upper) slot = lower;
            if (slot == first) {
                slot = previous;
                break;
            }
        } while (!slot_enabled(p, slot));
    }
    return (uint8_t)slot;
}

/* StatusScreenMoveCursor */
static void move_cursor(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g) {
    if (p->misc[0].oam_id == MISC_ITEM_CURSOR_FOCUSING) return;
    uint8_t slot = p->status.current_slot;
    int offset;
    if (s->changed_input & KEY_UP) {
        offset = -1;
    } else {
        offset = s->changed_input & KEY_DOWN ? 1 : 0;
        if (offset == 0) {
            if (slot < 8) {
                if (s->changed_input & KEY_RIGHT) offset = 2;
            } else if (s->changed_input & KEY_LEFT) {
                offset = 2;
            }
        }
    }
    if (offset) slot = destination_slot(p, offset, p->status.current_slot);
    if (p->status.current_slot == slot) return;
    p->status.current_slot = slot;
    update_cursor_position(p, slot);
    toggle_item(p, s, g, slot, TOGGLE_CHECKING);
    mzm_menu_oam_set_id(&p->misc[0], MISC_ITEM_CURSOR_FOCUSING);
    mzm_sfx_play(SOUND_STATUS_SCREEN_CURSOR_MOVING);
}

/* StatusScreenGetCurrentEquipmentSelected */
static uint8_t equipment_selected(const MzmPauseScreen *p, const MzmPauseGame *g, int slot) {
    if ((uint8_t)(slot - 1) > 16) return 0x80;
    if (suitless(g)) return TEXT_PISTOL;
    bool fully_powered = g->equipment->suit == MZM_SUIT_FULLY_POWERED;
    int offset = item_data(slot, 1);
    uint8_t activation;
    switch (item_data(slot, 0)) {
    case GROUP_BEAMS:
        activation = p->status.beam[offset];
        if (activation & BBF_LONG_BEAM) return TEXT_LONG_BEAM;
        if (activation & BBF_CHARGE_BEAM) return TEXT_CHARGE_BEAM;
        if (activation & BBF_ICE_BEAM) return TEXT_ICE_BEAM;
        if (activation & BBF_WAVE_BEAM) return TEXT_WAVE_BEAM;
        if (activation & BBF_PLASMA_BEAM) return fully_powered ? TEXT_PLASMA_BEAM : TEXT_UNKNOWN_ITEM;
        break;
    case GROUP_MISSILES:
        if (p->status.missiles[offset]) return offset == 0 ? TEXT_MISSILES : TEXT_SUPER_MISSILES;
        break;
    case GROUP_BOMBS:
        if (p->status.bomb[offset]) return offset == 0 ? TEXT_BOMBS : TEXT_POWER_BOMBS;
        break;
    case GROUP_SUITS:
        activation = p->status.suit[offset];
        if (activation & SMF_VARIA_SUIT) return TEXT_VARIA_SUIT;
        if (activation & SMF_GRAVITY_SUIT) return fully_powered ? TEXT_GRAVITY_SUIT : TEXT_UNKNOWN_ITEM;
        break;
    case GROUP_MISC:
        activation = p->status.misc[offset];
        if (activation & SMF_MORPH_BALL) return TEXT_MORPH_BALL;
        if (activation & SMF_POWER_GRIP) return TEXT_POWER_GRIP;
        if (activation & SMF_SPEEDBOOSTER) return TEXT_SPEEDBOOSTER;
        if (activation & SMF_HIGH_JUMP) return TEXT_HIGH_JUMP;
        if (activation & SMF_SCREW_ATTACK) return TEXT_SCREW_ATTACK;
        if (activation & SMF_SPACE_JUMP) return fully_powered ? TEXT_SPACE_JUMP : TEXT_UNKNOWN_ITEM;
        break;
    default:
        break;
    }
    return 0x80;
}

static void clear_text(MzmScreen *s) { memset(s->ppu->vram + 0x7800, 0, 0x800); }

/* TextProcessDescription (no demo: the line changes wait for A) */
static void process_description(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g) {
    if (p->current_status_slot != p->status.current_slot) {
        p->misc[1].exists = 0;
        p->description_stage = 0;
        p->current_status_slot = p->status.current_slot;
        p->current_equipment = TEXT_LONG_BEAM;
        p->message = mzm_text_message_from_rom(sym("sMessageDescription_Empty"));
    }
    MzmMessage *m = &p->message;
    switch (p->description_stage) {
    case 0:
        p->current_equipment = equipment_selected(p, g, p->current_status_slot);
        clear_text(s);
        p->description_stage = p->current_equipment < TEXT_COUNT ? 1 : 0x80;
        break;
    case 1:
        m->timer++;
        if (m->timer > m->delay || s->changed_input & KEY_A) {
            m->delay = 0;
            p->description_stage++;
            m->timer = 0;
        }
        break;
    case 2: {
        uint32_t text = mzm_rom_u32(sym("sEnglishTextPointers_Description") + p->current_equipment * 4u);
        int state = mzm_text_process_message(m, text, s->ppu->vram + 0x7800, s->button_input);
        uint8_t marker = 0;
        if (state == MZM_TEXT_ENDED) {
            /* A multi line message ends on the up marker. */
            marker = m->line != 0 ? MISC_TEXT_MARKER_UP : 0;
            p->description_stage = m->line != 0 ? 4 : 6;
        } else if (state == MZM_TEXT_NEW_LINE) {
            marker = MISC_TEXT_MARKER_DOWN;
            m->timer = 0;
            p->description_stage = 3;
        }
        if (marker) {
            p->misc[1].x = (int16_t)((m->indent + 10) * 4);
            p->misc[1].y = marker == MISC_TEXT_MARKER_DOWN ? BLOCK * 9 : BLOCK * 8 + HALF_BLOCK + 4;
            /* No end marker while getting a new item */
            if (marker == MISC_TEXT_MARKER_UP && p->type_flags & TYPE_GETTING_NEW_ITEM) marker = 0;
            mzm_menu_oam_set_id(&p->misc[1], marker);
        }
        break;
    }
    case 3:
        if (s->changed_input & KEY_A) {
            clear_text(s);
            p->description_stage = 1;
            m->timer = 0;
            m->indent = 0;
            m->delay = 10;
            p->misc[1].exists = 0;
        }
        break;
    case 4:
        if (s->changed_input & KEY_A) {
            clear_text(s);
            p->description_stage = 1;
            *m = mzm_text_message_from_rom(sym("sMessageDescription_Empty"));
            p->misc[1].exists = 0;
        }
        break;
    default:
        break;
    }
}

/* StatusScreenHandler (the suit change fade only follows an item toggle) */
static void status_screen(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g) {
    if (s->changed_input & (KEY_B | KEY_L | KEY_R) && p->stage == 0) {
        p->state = STATE_STATUS_SCREEN_LEAVING;
        p->timer = 0;
        p->stage = 0;
        return;
    }
    if (p->fade_wireframe_stage != 0) {
        fade_wireframe(p, s, g);
    } else if (p->status.current_slot != SLOT_0) {
        if (s->changed_input & KEY_SELECT) {
            uint8_t result = toggle_item(p, s, g, p->status.current_slot, TOGGLE_TOGGLING);
            if (result == 1) mzm_sfx_play(MZM_SOUND_TOGGLING_ITEM_ON);
            else if (result == 0) mzm_sfx_play(MZM_SOUND_TOGGLING_ITEM_OFF);
        } else {
            move_cursor(p, s, g);
        }
    }
    process_description(p, s, g);
}

/* The stage results of PauseScreenStatusScreenInit and
 * PauseScreenQuitStatusScreen: a positive one moves to that stage (above
 * 255: the next), a negative one ends. */
static int advance_stage(MzmPauseScreen *p, int stage) {
    if (stage > 0) {
        if (stage > 0xFF) p->stage++;
        else p->stage = (uint8_t)stage;
        p->timer = 0;
        return 0;
    }
    return stage;
}

/* PauseScreenStatusScreenInit */
static bool status_screen_init(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g) {
    int stage = 0;
    switch (p->stage) {
    case 0:
        vram_from16(s, 0xC000, suitless(g) ? p->status_background_tilemap : p->status_base_tilemap, 0x500);
        stage = 1;
        break;
    case 1:
        update_top_visor(p, s, OVERLAY_R_PROMPT_PRESSED);
        stage = 0x100;
        mzm_sfx_play(SOUND_OPENING_STATUS_SCREEN);
        break;
    case 2:
        if (init_fading(p, 16 << 8 | 0, 2)) {
            update_bottom_visor(p, s, 2, 2);
            stage = 0x100;
        }
        break;
    case 3:
        p->type_flags |= TYPE_ON_STATUS_SCREEN;
        p->bg2cnt = p->unk_74;
        p->bldcnt = BLDCNT_BG2_FIRST | BLDCNT_ALPHA | BLDCNT_BG2_SECOND | BLDCNT_BG3_SECOND | BLDCNT_OBJ_SECOND |
                    BLDCNT_BACKDROP_SECOND;
        update_status_oam(p, s, g, 1);
        stage = 0x100;
        break;
    case 4:
        if (update_status_oam(p, s, g, 2)) stage = 0x100;
        break;
    case 5:
        if (init_fading(p, 0 << 8 | 16, 2)) stage = 0x100;
        break;
    case 6:
        update_status_oam(p, s, g, 3);
        stage = 0x100;
        break;
    case 7:
        if (draw_items(p, s, (uint8_t)(p->timer - 1))) stage = 0x100;
        break;
    case 8:
        init_cursor_and_items(p, s, g);
        p->dispcnt |= GBA_DCNT_BG0;
        clear_text(s);
        stage = -1;
        break;
    default:
        break;
    }
    stage = advance_stage(p, stage);
    if (stage < 0) {
        s->bldalpha_h = 0;
        s->bldalpha_l = 16;
    }
    return stage != 0;
}

/* PauseScreenQuitStatusScreen */
static bool quit_status_screen(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g) {
    int stage = 0;
    switch (p->stage) {
    case 0:
        mzm_sfx_play(SOUND_LEAVING_STATUS_SCREEN);
        p->misc[0].oam_id = 0;
        update_top_visor(p, s, 0);
        stage = 0x100;
        break;
    case 1:
        update_status_oam(p, s, g, 4);
        p->misc[1].exists = 0;
        stage = 0x100;
        break;
    case 2:
        if (p->timer > 12) stage = 0x100;
        break;
    case 3:
        if (init_fading(p, 16 << 8 | 0, 2)) {
            p->dispcnt &= (uint16_t)~GBA_DCNT_BG0;
            p->overlay[3].exists = 0;
            stage = 0x100;
        }
        break;
    case 4:
        p->bldcnt = BLDCNT_BG2_FIRST | BLDCNT_ALPHA | BLDCNT_BG3_SECOND | BLDCNT_OBJ_SECOND | BLDCNT_BACKDROP_SECOND;
        update_status_oam(p, s, g, 5);
        p->current_status_slot = 0;
        stage = 0x100;
        break;
    case 5:
        s->bldalpha_h = 16;
        s->bldalpha_l = 0;
        p->dispcnt |= GBA_DCNT_BG1;
        p->overlay[0].exists = 1;
        p->stage++;
        /* fall through */
    case 6:
        p->bg2cnt = p->unk_72;
        stage = 0x100;
        break;
    case 7:
        if (init_fading(p, p->target_bld_alpha, 2)) {
            p->type_flags &= (uint16_t)~TYPE_ON_STATUS_SCREEN;
            stage = 0x100;
        }
        break;
    case 8:
        update_bottom_visor(p, s, 1, 1);
        stage = -1;
        break;
    default:
        break;
    }
    stage = advance_stage(p, stage);
    if (stage < 0) {
        s->bldalpha_h = (uint8_t)(p->target_bld_alpha >> 8);
        s->bldalpha_l = (uint8_t)p->target_bld_alpha;
    }
    return stage != 0;
}

/* Easy sleep (pause_screen_sub_menus.c, the easy sleep parts of
 * pause_screen.c and text.c) */

enum {
    MESSAGE_ACTIVATE_EASY_SLEEP = 36, MESSAGE_PRESS_SELECT_L_AND_R = 37, MESSAGE_EASY_SLEEP_PROMPT = 38,
    /* EasySleepMenuStage */
    SLEEP_NO_OPTION = 0, SLEEP_YES_OPTION, SLEEP_DELAY, SLEEP_DISABLE_DISPLAY, SLEEP_HANDLE_WAKE,
    SLEEP_WAIT_FOR_NO_INPUT,
    MISC_RIGHT_CURSOR = 2,
    CHAR_TERMINATOR = 0xFF00, CHAR_NEW_LINE = 0xFE00, CHAR_WIDTH_MASK = 0x8000, CHAR_COLOR_MASK = 0x8100,
};

/* sMessageTextPointers[LANGUAGE_ENGLISH][message] */
static uint32_t message_text(int message) {
    return mzm_rom_u32(sym("sEnglishTextPointers_Message") + (uint32_t)message * 4u);
}

/* TextDrawYesNoEasySleep: YES and NO side by side in unk_6000. */
static void draw_yes_no(MzmPauseScreen *p) {
    memset(p->easy_sleep_text, 0xFF, sizeof p->easy_sleep_text);
    p->easy_sleep_text_state = 0;
    uint32_t text = message_text(MESSAGE_EASY_SLEEP_PROMPT);
    memset(&p->message, 0, sizeof p->message);
    for (uint16_t c; (c = mzm_rom_u16(text)) != CHAR_TERMINATOR; text += 2) {
        bool draw = true;
        if (c == CHAR_NEW_LINE) {
            if (p->message.indent > 112) return;
            p->message.indent = 112;
            draw = false;
        } else if (c & CHAR_WIDTH_MASK) {
            if ((c & CHAR_TERMINATOR) == CHAR_COLOR_MASK) p->message.color = (uint8_t)c;
            draw = false;
        }
        if (!draw) continue;
        mzm_text_draw_message_character(c, p->easy_sleep_text[4], p->message.indent, p->message.color);
        p->message.indent = (uint16_t)(p->message.indent + mzm_text_char_width(c));
    }
}

/* One message of TextDrawEasySleep: up to four steps a frame, each line in
 * the next buffer. */
static bool draw_sleep_message(MzmPauseScreen *p, MzmScreen *s, int message, int first_buffer) {
    for (int i = 3; i != -1; --i) {
        if (!mzm_text_process_message(&p->message, message_text(message),
                                      p->easy_sleep_text[first_buffer + p->message.line], s->button_input))
            continue;
        p->message.indent = 0;
        if (p->message.message_ended) return true;
    }
    return false;
}

/* TextDrawEasySleep */
static void draw_easy_sleep_text(MzmPauseScreen *p, MzmScreen *s) {
    switch ((uint8_t)p->easy_sleep_text_state) {
    case 0:
        memset(&p->message, 0, sizeof p->message);
        p->message.is_message = true;
        p->easy_sleep_text_state++;
        /* fall through */
    case 1:
        if (draw_sleep_message(p, s, MESSAGE_ACTIVATE_EASY_SLEEP, 0)) p->easy_sleep_text_state++;
        break;
    case 2:
        memset(&p->message, 0, sizeof p->message);
        p->message.is_message = true;
        p->easy_sleep_text_state++;
        /* fall through */
    case 3:
        if (draw_sleep_message(p, s, MESSAGE_PRESS_SELECT_L_AND_R, 2)) p->easy_sleep_text_state = INT8_MIN;
        break;
    default:
        break;
    }
}

/* PauseScreenEasySleepInit: the map fades out, the easy sleep tilemap and
 * text replace the equipment names (backed up), then it fades in. */
static bool easy_sleep_init(MzmPauseScreen *p, MzmScreen *s) {
    uint8_t *vram = s->ppu->vram;
    if (p->easy_sleep_text_state >= 0) draw_easy_sleep_text(p, s);
    switch (p->stage) {
    case 0:
        if (!init_fading(p, 16 << 8 | 0, 4)) p->stage--;
        break;
    case 1:
        update_bottom_visor(p, s, 2, 2);
        update_top_visor(p, s, OVERLAY_L_PROMPT_PRESSED);
        p->dispcnt &= (uint16_t)~GBA_DCNT_BG2;
        break;
    case 2:
        vram_from16(s, 0xD000, p->easy_sleep_tilemap, sizeof p->easy_sleep_tilemap);
        break;
    case 3:
        memcpy(p->gfx_backup[0], vram + 0x6000, 0x800);
        memcpy(p->gfx_backup[1], vram + 0x6800, 0x800);
        break;
    case 4:
        memcpy(p->gfx_backup[2], vram + 0x7000, 0x800);
        memcpy(p->gfx_backup[3], vram + 0x7800, 0x800);
        break;
    case 6:
        /* YES after the first line, NO after the second */
        memcpy(p->easy_sleep_text[0] + 0x380, p->easy_sleep_text[4], 0x80);
        memcpy(p->easy_sleep_text[0] + 0x780, p->easy_sleep_text[4] + 0x400, 0x80);
        memcpy(p->easy_sleep_text[1] + 0x380, p->easy_sleep_text[4] + 0x1C0, 0x80);
        memcpy(p->easy_sleep_text[1] + 0x780, p->easy_sleep_text[4] + 0x5C0, 0x80);
        break;
    case 7:
        if ((uint8_t)p->easy_sleep_text_state > 1) {
            memcpy(vram + 0x6000, p->easy_sleep_text[0], 0x800);
            memcpy(vram + 0x6800, p->easy_sleep_text[1], 0x800);
        } else {
            p->stage--;
        }
        break;
    case 8:
        if (p->easy_sleep_text_state < 0) {
            memcpy(vram + 0x7000, p->easy_sleep_text[2], 0x800);
            memcpy(vram + 0x7800, p->easy_sleep_text[3], 0x800);
        } else {
            p->stage--;
        }
        break;
    case 9:
        p->bldcnt = BLDCNT_BG2_FIRST | BLDCNT_ALPHA | BLDCNT_BG2_SECOND | BLDCNT_BG3_SECOND | BLDCNT_OBJ_SECOND |
                    BLDCNT_BACKDROP_SECOND;
        p->dispcnt |= GBA_DCNT_BG2;
        p->stage++;
        /* fall through */
    case 10:
        if (!init_fading(p, 0 << 8 | 16, 4)) p->stage--;
        break;
    case 11:
        mzm_menu_oam_set_id(&p->misc[1], MISC_RIGHT_CURSOR);
        p->misc[1].x = BLOCK * 8 + QUARTER_BLOCK;
        p->misc[1].y = BLOCK * 4 + QUARTER_BLOCK;
        p->misc[1].obj_mode = 1;
        p->stage = 0;
        p->timer = 0;
        return true;
    default:
        break;
    }
    p->stage++;
    return false;
}

/* PauseScreenQuitEasySleep */
static bool quit_easy_sleep(MzmPauseScreen *p, MzmScreen *s) {
    uint8_t *vram = s->ppu->vram;
    switch (p->stage) {
    case 0:
        update_top_visor(p, s, 0);
        p->stage++;
        /* fall through */
    case 1:
        if (!init_fading(p, 16 << 8 | 0, 4)) p->stage--;
        break;
    case 2:
        p->dispcnt &= (uint16_t)~GBA_DCNT_BG2;
        mzm_menu_oam_set_id(&p->misc[1], 0);
        p->misc[1].obj_mode = 0;
        break;
    case 3:
        vram_from16(s, 0xD000, p->map_overlay, sizeof p->map_overlay);
        break;
    case 4:
        memcpy(vram + 0x6000, p->gfx_backup[0], 0x800);
        memcpy(vram + 0x6800, p->gfx_backup[1], 0x800);
        break;
    case 5:
        memcpy(vram + 0x7000, p->gfx_backup[2], 0x800);
        memcpy(vram + 0x7800, p->gfx_backup[3], 0x800);
        break;
    case 6:
        p->bldcnt = BLDCNT_BG2_FIRST | BLDCNT_ALPHA | BLDCNT_BG3_SECOND | BLDCNT_OBJ_SECOND | BLDCNT_BACKDROP_SECOND;
        p->dispcnt |= GBA_DCNT_BG2;
        p->stage++;
        /* fall through */
    case 7:
        if (!init_fading(p, p->target_bld_alpha, 4)) p->stage--;
        break;
    case 8:
        update_bottom_visor(p, s, 1, 1);
        p->stage = 0;
        p->timer = 0;
        return true;
    default:
        break;
    }
    p->stage++;
    return false;
}

/* PauseScreenEasySleepHandler. The Stop syscall becomes `sleeping`: the
 * screen stays blank until SELECT, L and R are held together. */
static bool easy_sleep(MzmPauseScreen *p, MzmScreen *s) {
    int action = 0;
    switch (p->stage) {
    case SLEEP_NO_OPTION:
        if (s->changed_input & KEY_A) {
            mzm_sfx_play(SOUND_REFUSE_MENU);
            action = 2;
        } else if (s->changed_input & KEY_LEFT) {
            mzm_sfx_play(SOUND_YES_NO_CURSOR_MOVING);
            p->stage = SLEEP_YES_OPTION;
            p->misc[1].x = BLOCK * 3 + QUARTER_BLOCK;
        } else if (s->changed_input & (KEY_B | KEY_L | KEY_R)) {
            action = 1;
        }
        break;
    case SLEEP_YES_OPTION:
        if (s->changed_input & KEY_A) {
            mzm_sfx_play(SOUND_YES_NO_CURSOR_SELECTING_YES);
            p->stage = SLEEP_DELAY;
            p->timer = 0;
        } else if (s->changed_input & KEY_RIGHT) {
            mzm_sfx_play(SOUND_YES_NO_CURSOR_MOVING);
            p->stage = SLEEP_NO_OPTION;
            p->misc[1].x = BLOCK * 8 + QUARTER_BLOCK;
        } else if (s->changed_input & (KEY_B | KEY_L | KEY_R)) {
            action = 1;
        }
        break;
    case SLEEP_DELAY:
        if (p->timer > 30) {
            p->stage++;
            p->timer = 0;
        }
        break;
    case SLEEP_DISABLE_DISPLAY:
        p->dispcnt ^= GBA_DCNT_BLANK;
        p->stage++;
        break;
    case SLEEP_HANDLE_WAKE:
        p->sleeping = (s->button_input & (KEY_SELECT | KEY_L | KEY_R)) != (KEY_SELECT | KEY_L | KEY_R);
        if (p->sleeping) break;
        p->dispcnt ^= GBA_DCNT_BLANK;
        p->misc[1].x = BLOCK * 8 + QUARTER_BLOCK;
        p->stage++;
        break;
    case SLEEP_WAIT_FOR_NO_INPUT:
        if (s->button_input == 0) p->stage = SLEEP_NO_OPTION;
        break;
    default:
        break;
    }
    if (!action) return false;
    p->stage = 0;
    p->timer = 0;
    if (action == 1) mzm_sfx_play(SOUND_CLOSING_EASY_SLEEP_SCREEN);
    return true;
}

/* PauseScreenInitMapDownload (the unused instant types 1 and 2 are not
 * ported) */
static void init_map_download(MzmPauseScreen *p) {
    p->download_timer = 0;
    p->download_stage = 0;
    p->current_downloaded_line = 0;
    p->unk_4f = 0;
}

/* sMapDownloadLineTrailOamOffsets */
static uint8_t trail_oam(int i) { return rom8("sMapDownloadLineTrailOamOffsets", (uint32_t)i); }

static void reset_download_oam(MzmMenuOam *o, uint8_t oam_id) {
    o->oam_id = oam_id;
    o->animation_duration_counter = 0;
    o->current_animation_frame = 0;
}

/* PauseScreenMapDownload: a line sweeps down the map, revealing a row of
 * the downloaded map every eight frames. */
static bool map_download(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g) {
    p->download_timer++;
    switch (p->download_stage) {
    case 0:
        if (p->download_timer <= 8) break;
        reset_download_oam(&p->misc[2], MISC_MAP_DOWNLOAD_LINE);
        p->misc[2].x = BLOCK * 7 + QUARTER_BLOCK;
        p->misc[2].y = -HALF_BLOCK;
        p->misc[2].priority = 1;
        p->misc[2].exists = 1;
        for (int i = 3; i >= 0; --i) {
            MzmMenuOam *o = &p->misc[trail_oam(i)];
            reset_download_oam(o, 0);
            o->x = BLOCK * 7 + QUARTER_BLOCK;
            o->y = -HALF_BLOCK;
            o->priority = 1;
            o->obj_mode = 1;
            o->exists = 0;
        }
        p->download_stage++;
        p->download_timer = 0;
        p->unk_4f = 0;
        p->line_trail_index = 0x80;
        break;
    case 1:
        p->current_downloaded_line = 0;
        p->unk_4f = 0;
        if (s->bg_vofs[3] < BLOCK * 32)
            p->current_downloaded_line = (uint8_t)(0x3F - ((BLOCK * 32 - s->bg_vofs[3]) >> 5));
        else if (s->bg_vofs[3] > BLOCK * 32)
            p->current_downloaded_line = (uint8_t)((s->bg_vofs[3] - BLOCK * 32) >> 5);
        p->download_stage++;
        p->download_timer = 0;
        break;
    case 2: {
        bool move = true;
        if (p->download_timer >= 8) {
            if (p->unk_4f < 21) {
                if (p->current_downloaded_line < MINIMAP)
                    vram_from16(s, 0xE000u + p->current_downloaded_line * MINIMAP * 2u,
                                &g->hud->minimap_visited[p->current_downloaded_line * MINIMAP], MINIMAP * 2);
                p->unk_4f++;
                p->current_downloaded_line++;
                if (p->current_downloaded_line >= MINIMAP * 2) p->current_downloaded_line = 0;
                /* The boss icon shows once the line is below it. */
                if (p->boss_icon[0].oam_id != 0 && p->current_area < AREAS_NORMAL &&
                    p->current_downloaded_line == mzm_rom_u8(sym("sBossIcons") + p->current_area * 5u + 3) + 2)
                    p->boss_icon[0].not_drawn = false;
            } else {
                p->download_stage++;
                move = false;
            }
            p->download_timer = 0;
        }
        if (move) {
            p->misc[2].y = (int16_t)(((p->unk_4f - 1) * 8 + p->download_timer) * 4);
            if (++p->line_trail_index >= 4) p->line_trail_index = 0;
            MzmMenuOam *o = &p->misc[trail_oam(p->line_trail_index)];
            o->y = (int16_t)(p->misc[2].y - 4);
            reset_download_oam(o, MISC_MAP_DOWNLOAD_LINE_TRAIL);
            o->exists = 1;
        }
        break;
    }
    case 3:
        vram_from16(s, 0xE000, g->hud->minimap_visited, 2048);
        p->download_stage++;
        p->download_timer = 0;
        break;
    case 4:
        /* MinimapDraw, so that the HUD's minimap shows the download */
        mzm_hud_redraw_minimap(g->hud);
        p->download_stage++;
        p->download_timer = 0;
        break;
    case 5:
        reset_download_oam(&p->misc[2], 0);
        p->misc[2].exists = 0;
        for (int i = 3; i >= 0; --i) {
            reset_download_oam(&p->misc[trail_oam(i)], 0);
            p->misc[trail_oam(i)].exists = 0;
        }
        p->download_timer = 0;
        p->download_stage++;
        break;
    case 6:
        if (p->download_timer >= 30) {
            p->download_timer = 0;
            p->download_stage = 0;
            return true;
        }
        break;
    default:
        break;
    }
    return false;
}

/* PauseScreenInit (the pause from the game, the Chozo hints and the map
 * download) */

/* UpdateSuitType(SUIT_FULLY_POWERED): refilled, every item obtained but
 * not activated. */
static void update_suit_type_fully_powered(MzmEquipment *e) {
    e->suit = MZM_SUIT_FULLY_POWERED;
    e->energy = e->max_energy;
    e->missiles = e->max_missiles;
    e->super_missiles = e->max_super_missiles;
    e->power_bombs = e->max_power_bombs;
    e->unknown_items |= e->items;
    e->items = 0;
}

/* WINOUT's high byte (the OBJ window's layers). */
static void winout_obj(MzmScreen *s, uint8_t value) {
    gba_ppu_write16(s->ppu, GBA_REG_WINOUT,
                    (uint16_t)((gba_ppu_read16(s->ppu, GBA_REG_WINOUT) & 0xFF) | value << 8));
}

/* PauseScreenFadeWireframeSamus: the wireframe faded out, changed, then
 * faded back in. */
static void fade_wireframe(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g) {
    p->fade_wireframe_timer++;
    switch (p->fade_wireframe_stage) {
    case 1:
        p->bldcnt &= (uint16_t)~BLDCNT_BG2_FIRST;
        winout_obj(s, 0xD0);
        p->fade_wireframe_stage++;
        break;
    case 2:
        if (s->bldalpha_h < 16) s->bldalpha_h += 2;
        if (s->bldalpha_h > 16) s->bldalpha_h = 16;
        s->bldalpha_l = (uint8_t)(16 - s->bldalpha_h);
        if (s->bldalpha_l == 0) {
            p->fade_wireframe_stage++;
            p->fade_wireframe_timer = 0;
        }
        break;
    case 3:
        update_wireframe(p, s, g, 1);
        p->fade_wireframe_stage++;
        p->fade_wireframe_timer = 0;
        break;
    case 4:
        p->fade_wireframe_stage++;
        p->fade_wireframe_timer = 0;
        /* fall through */
    case 5:
        if (s->bldalpha_l < 16) s->bldalpha_l += 2;
        if (s->bldalpha_l > 16) s->bldalpha_l = 16;
        s->bldalpha_h = (uint8_t)(16 - s->bldalpha_l);
        if (s->bldalpha_h == 0) {
            p->fade_wireframe_stage++;
            p->fade_wireframe_timer = 0;
        }
        break;
    case 6:
        p->bldcnt = BLDCNT_BG2_FIRST | BLDCNT_ALPHA | BLDCNT_BG2_SECOND | BLDCNT_BG3_SECOND | BLDCNT_OBJ_SECOND |
                    BLDCNT_BACKDROP_SECOND;
        winout_obj(s, 1 << 3 | 1 << 4);
        p->fade_wireframe_stage = 0;
        break;
    default:
        break;
    }
}

/* StatusScreenGetSlotForNewItem(2, item): the item's slot for a suit
 * change. */
static uint8_t slot_for_suit_change(MzmPauseScreen *p, uint8_t item) {
    if (item >= 22) return SLOT_0;
    uint32_t at = sym("sStatusScreenItemsAcquisitionData") + item * 4u;
    uint8_t kind = mzm_rom_u8(at), group = mzm_rom_u8(at + 1), offset = mzm_rom_u8(at + 2);
    uint8_t slot = mzm_rom_u8(at + 3);
    if (kind != 3) return slot;
    uint8_t *activation;
    switch (group) {
    case GROUP_BEAMS: activation = p->status.beam; break;
    case GROUP_BOMBS: activation = p->status.bomb; break;
    case GROUP_SUITS: activation = p->status.suit; break;
    case GROUP_MISC: activation = p->status.misc; break;
    default: return slot;
    }
    uint8_t flag = flag_order(group, offset);
    for (int i = 0; i < flags_size(group); ++i, ++slot)
        if (activation[i] == flag) break;
    return slot;
}

/* StatusScreenFindUnknownItemSlot: 0 an invalid slot, 1 found (an unknown
 * item when `want_unknown`, else a normal one), 2 every slot searched. */
static int find_unknown_item_slot(MzmPauseScreen *p, bool want_unknown) {
    for (;; p->status.current_slot++) {
        if (p->status.current_slot >= SLOT_COUNT) return 2;
        int slot = p->status.current_slot;
        const uint8_t *activation;
        uint8_t item;
        switch (item_data(slot, 0)) {
        case GROUP_BEAMS: activation = p->status.beam; item = BBF_PLASMA_BEAM; break;
        case GROUP_MISSILES: activation = p->status.missiles; item = 0xFF; break;
        case GROUP_BOMBS: activation = p->status.bomb; item = 0xFF; break;
        case GROUP_SUITS: activation = p->status.suit; item = SMF_GRAVITY_SUIT; break;
        case GROUP_MISC: activation = p->status.misc; item = SMF_SPACE_JUMP; break;
        default: return 0;
        }
        uint8_t activated = activation[item_data(slot, 1)];
        if (!activated) continue;
        if (!want_unknown ? activated != item : activated == item) return 1;
    }
}

/* StatusScreenUpdateUnknownItemPalette: OBJ row 12 flashed white (two
 * partial flashes, a full one, back down), then the name's idle cycle. */
static bool update_unknown_palette(MzmPauseScreen *p, MzmScreen *s, int stage) {
    bool ended = false;
    uint32_t offset = 0;
    uint16_t *row = &s->ppu->palette[256 + 12 * 16];
    p->unknown_palette.timer++;
    switch (stage) {
    case 0:
        p->unknown_palette.timer = 0;
        p->unknown_palette.row = 0;
        p->unknown_palette.flashing = 0;
        break;
    case 1:
        if (p->unknown_palette.flashing < 2) {
            if (p->unknown_palette.timer > 3) {                 /* CONVERT_SECONDS(.05f) */
                p->unknown_palette.timer = 0;
                p->unknown_palette.row++;
                if ((uint8_t)p->unknown_palette.row >= 8) {
                    p->unknown_palette.flashing++;
                    p->unknown_palette.row = 0;
                }
                offset = rom8("sStatusScreenUnknownItemsPartialFlash", (uint8_t)p->unknown_palette.row) * 16u;
            }
        } else if (p->unknown_palette.timer >= 3) {
            p->unknown_palette.timer = 0;
            p->unknown_palette.row++;
            if ((uint8_t)p->unknown_palette.row >= 6) {
                p->unknown_palette.row = 5;
                p->unknown_palette.flashing++;
                ended = true;
            }
            offset = rom8("sStatusScreenUnknownItemsFullFlash", (uint8_t)p->unknown_palette.row) * 16u;
        }
        if (offset == 0) break;
        mzm_copy_from_rom(row, sym("sStatusScreenUnknownItemsWhiteFlashingPal") + offset * 2, 32);
        break;
    case 2:
        if (p->unknown_palette.timer < 2) break;               /* CONVERT_SECONDS(1.f / 30) */
        p->unknown_palette.timer = 0;
        p->unknown_palette.row--;
        if (p->unknown_palette.row == 0) ended = true;
        offset = rom8("sStatusScreenUnknownItemsFullFlash", (uint8_t)p->unknown_palette.row) * 16u;
        mzm_copy_from_rom(row, sym("sStatusScreenUnknownItemsWhiteFlashingPal") + offset * 2, 32);
        break;
    case 3:
        if (p->unknown_palette.timer <= 3) break;
        p->unknown_palette.timer = 0;
        p->unknown_palette.row++;
        if ((uint8_t)p->unknown_palette.row >= 4) p->unknown_palette.row = 0;
        offset = rom8("sStatusScreenUnknownItemsNamesFlash", (uint8_t)p->unknown_palette.row) * 16u;
        mzm_copy_from_rom(row, sym("sStatusScreenUnknownItemsNamesFlashingPal") + offset * 2, 32);
        break;
    default:
        break;
    }
    return ended;
}

/* StatusScreenEnableUnknownItem: the row's name written from the unknown
 * items' last row of the status tile map. */
static void enable_unknown_item(MzmPauseScreen *p, MzmScreen *s, int group, int row) {
    if (group != GROUP_BEAMS && group != GROUP_SUITS && group != GROUP_MISC) return;
    Box u = unknown_box(group), b = group_box(group);
    uint32_t position = (uint32_t)(u.bottom - 1) * STATUS_ROW + u.left;
    if (position == 0) return;
    uint16_t *dst = vram16(s, 0xC000) + (b.top + row) * STATUS_ROW + b.left + 1;
    position++;
    for (int i = 1; i < b.right - b.left; ++i) *dst++ = p->status_tilemap[position++];
}

/* StatusScreenGetSlotForNewItem(1, item): the new item deactivated so its
 * toggling shows it (STATUS_SLOT_BEAM when it was active, else 0). */
static uint8_t slot_for_new_item(const MzmPauseGame *g, uint8_t item) {
    if (item >= 22) return SLOT_0;
    uint32_t at = sym("sStatusScreenItemsAcquisitionData") + item * 4u;
    uint8_t kind = mzm_rom_u8(at), group = mzm_rom_u8(at + 1), offset = mzm_rom_u8(at + 2);
    if (kind < 2) return SLOT_0;
    const char *order;
    switch (group) {
    case GROUP_BEAMS: order = "sStatusScreenBeamFlagsOrder"; break;
    case GROUP_BOMBS: order = "sStatusScreenBombFlagsOrder"; break;
    case GROUP_SUITS: order = "sStatusScreenSuitFlagsOrder"; break;
    case GROUP_MISC: order = "sStatusScreenMiscFlagsOrder"; break;
    default: return SLOT_0;
    }
    uint32_t items = item_of_flag(group == GROUP_BEAMS || group == GROUP_BOMBS, rom8(order, offset));
    uint8_t slot = g->equipment->items & items ? SLOT_BEAM : SLOT_0;
    set_items_active(g->equipment, items, false);
    return slot;
}

/* UpdateSuitType(SUIT_SUITLESS): 99 energy, only the Power Grip, the Long
 * Beam and the Charge Beam active (the pistol). */
static void update_suit_type_suitless(MzmEquipment *e) {
    uint32_t owned = e->items | e->unknown_items;
    e->suit = MZM_SUIT_SUITLESS;
    e->energy = 99;
    e->items = MZM_ITEM_POWER_GRIP | MZM_ITEM_LONG_BEAM | MZM_ITEM_CHARGE_BEAM;
    e->unknown_items = owned & ~e->items;
}

enum {
    /* StatusScreenSuitlessItems' stages */
    NEW_ITEM_CHECK = 0, NEW_ITEM_TOGGLE, NEW_ITEM_WAIT_FADE, NEW_ITEM_DEMO_DESCRIPTION, NEW_ITEM_DEMO_END,
    NEW_ITEM_DESCRIPTION, NEW_ITEM_UNUSED, NEW_ITEM_DONE,
};

/* StatusScreenSuitlessItems (also the item acquisition): the new item's
 * slot toggled on with its sound, then its description; any key ends it
 * once described. No demo runs, so the demo stages are never used. */
static bool suitless_items(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g) {
    if (p->stage > NEW_ITEM_DESCRIPTION && s->changed_input != 0) return true;
    if (p->fade_wireframe_stage != 0) {
        fade_wireframe(p, s, g);
        return false;
    }
    int slot = p->status.current_slot;
    switch (p->stage) {
    case NEW_ITEM_CHECK:
        if (p->timer <= 16) break;                          /* CONVERT_SECONDS(4.f / 15) */
        if (slot != SLOT_0) {
            mzm_menu_oam_set_id(&p->misc[0], MISC_ITEM_CURSOR_FOCUSING);
            /* The native result (u32, 0xFF when it cannot toggle) is stored
             * in an s32 and tested below 0, which never holds: the toggle
             * stage always follows. */
            toggle_item(p, s, g, slot, TOGGLE_CHECKING2);
            p->stage = NEW_ITEM_TOGGLE;
            p->message.message_ended = false;
        } else {
            p->message.message_ended = true;
            p->stage = NEW_ITEM_DONE;
        }
        p->timer = 0;
        break;
    case NEW_ITEM_TOGGLE:
        if (p->timer <= 8) break;                           /* CONVERT_SECONDS(2.f / 15) */
        toggle_item(p, s, g, slot, TOGGLE_TOGGLING);
        switch (g->item_being_acquired) {
        case ITEM_ACQUISITION_PLASMA_BEAM:
        case ITEM_ACQUISITION_GRAVITY:
        case ITEM_ACQUISITION_SPACE_JUMP:
            mzm_sfx_play(MZM_SOUND_UNKNOWN_ITEM_ACQUISITION);
            break;
        case ITEM_ACQUISITION_MISSILES:
        case ITEM_ACQUISITION_SUPER_MISSILES:
        case ITEM_ACQUISITION_POWER_BOMB:
            update_row(p, s, item_data(slot, 0), item_data(slot, 2), true, true);
            /* fall through */
        default:
            mzm_sfx_play(MZM_SOUND_TOGGLING_ITEM_ON);
            break;
        }
        p->timer = 0;
        p->stage++;
        break;
    case NEW_ITEM_WAIT_FADE:
        if (p->fade_wireframe_stage != 0) break;
        p->timer = 0;
        p->stage = NEW_ITEM_DESCRIPTION;
        break;
    case NEW_ITEM_DESCRIPTION:
        process_description(p, s, g);
        if (p->message.message_ended) p->stage = NEW_ITEM_DONE;
        break;
    default:
        break;
    }
    return false;
}

/* PauseScreenSuitChangingStart: the headers, the rows revealed, the
 * cursor on the acquired item's slot and the text background. */
static bool suit_changing_start(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g) {
    int stage = 0;
    switch (p->stage) {
    case 0:
        stage = 0x100;
        break;
    case 1:
        update_status_oam(p, s, g, 3);
        stage = 0x100;
        break;
    case 2:
        if (draw_items(p, s, (uint8_t)(p->timer - 1))) stage = 0x100;
        break;
    case 3:
        p->status.current_slot = slot_for_suit_change(p, item_being_acquired(g));
        init_cursor_and_items(p, s, g);
        p->dispcnt |= GBA_DCNT_BG0;
        clear_text(s);
        stage = -1;
        break;
    default:
        break;
    }
    return advance_stage(p, stage) != 0;
}

enum {
    /* FullyPoweredItemsStage */
    FP_CHECK_ENABLE_SUIT = 0, FP_DELAY, FP_CHECK_NEXT_SLOT, FP_ACTIVATE_NORMAL_SLOT, FP_SEARCH_FOR_UNKNOWN_ITEM,
    FP_ENABLE_UNKNOWN_ITEM_INIT, FP_UNKNOWN_ITEM_ANIMATION_PART1, FP_ENABLE_UNKNOWN_ITEM,
    FP_UNKNOWN_ITEM_ANIMATION_PART2, FP_UNKNOWN_ITEM_DESCRIPTION, FP_FINISH,
};

static void remember_slot(MzmPauseScreen *p) {
    if (p->status.current_slot >= SLOT_BOMB) p->status.previous_right_slot = p->status.current_slot;
    else p->status.previous_left_slot = p->status.current_slot;
}

/* StatusScreenFullyPoweredItems */
static bool fully_powered_items(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g) {
    MzmEquipment *e = g->equipment;
    if (p->fade_wireframe_stage != 0) {
        fade_wireframe(p, s, g);
        return false;
    }
    int slot = p->status.current_slot, result;
    switch (p->stage) {
    case FP_CHECK_ENABLE_SUIT:
        if (p->timer > 50) {                                /* CONVERT_SECONDS(5.f / 6) */
            update_wireframe(p, s, g, 2);
            p->timer = 0;
            p->stage++;
        }
        break;
    case FP_DELAY:
        if (p->timer > 30) {
            p->status.current_slot = SLOT_BEAM;
            p->not_playing_enabling_sound = true;
            p->timer = 0;
            p->stage++;
        }
        break;
    case FP_CHECK_NEXT_SLOT:
        result = find_unknown_item_slot(p, false);
        if (result == 2) {
            p->status.current_slot = SLOT_BEAM;
            if (!p->not_playing_enabling_sound) mzm_sfx_fade(MZM_SOUND_ENABLING_NORMAL_ITEM, 15);
        }
        p->stage = (uint8_t)(p->stage + result);
        p->timer = 0;
        break;
    case FP_ACTIVATE_NORMAL_SLOT:
        if (p->timer <= 6) break;                           /* CONVERT_SECONDS(.1f) */
        update_row(p, s, item_data(slot, 0), item_data(slot, 2), true, true);
        update_cursor_position(p, slot);
        mzm_menu_oam_set_id(&p->misc[0], MISC_ITEM_CURSOR_FOCUSING_DESTROY);
        remember_slot(p);
        if (p->not_playing_enabling_sound) {
            mzm_sfx_play(MZM_SOUND_ENABLING_NORMAL_ITEM);
            p->not_playing_enabling_sound = false;
        }
        p->status.current_slot++;
        p->stage--;
        p->timer = 0;
        break;
    case FP_SEARCH_FOR_UNKNOWN_ITEM:
        if (find_unknown_item_slot(p, true) == 1) {
            clear_text(s);
            p->stage++;
        } else {
            p->stage = FP_FINISH;
        }
        p->timer = 0;
        break;
    case FP_ENABLE_UNKNOWN_ITEM_INIT: {
        int group = item_data(slot, 0);
        mzm_menu_oam_set_id(&p->misc[10], group == GROUP_BEAMS   ? MISC_PLASMA_UNKNOWN
                                          : group == GROUP_SUITS ? MISC_GRAVITY_UNKNOWN
                                                                 : MISC_SPACE_JUMP_UNKNOWN);
        Box b = group_box(group);
        p->misc[10].y = (int16_t)((b.top + item_data(slot, 2)) * HALF_BLOCK);
        p->misc[10].x = (int16_t)((b.left + 1) * HALF_BLOCK);
        update_unknown_palette(p, s, 0);
        mzm_sfx_play(MZM_SOUND_ENABLING_UNKNOWN_ITEM);
        p->stage++;
        p->timer = 0;
        break;
    }
    case FP_UNKNOWN_ITEM_ANIMATION_PART1:
        if (update_unknown_palette(p, s, 1)) {
            p->stage++;
            p->timer = 0;
        }
        break;
    case FP_ENABLE_UNKNOWN_ITEM:
        enable_unknown_item(p, s, item_data(slot, 0), item_data(slot, 2));
        update_cursor_position(p, slot);
        mzm_menu_oam_set_id(&p->misc[0], MISC_ITEM_CURSOR_FOCUSING);
        mzm_sfx_play(MZM_SOUND_TOGGLING_ITEM_ON);
        remember_slot(p);
        p->stage++;
        p->timer = 0;
        p->misc[10].oam_id++;
        if (item_data(slot, 0) == GROUP_SUITS) {
            /* The wireframe turns to the Gravity Suit. */
            e->items |= MZM_ITEM_GRAVITY_SUIT;
            e->unknown_items &= ~(uint32_t)MZM_ITEM_GRAVITY_SUIT;
            update_wireframe(p, s, g, 2);
        }
        break;
    case FP_UNKNOWN_ITEM_ANIMATION_PART2:
        if (update_unknown_palette(p, s, 2)) {
            p->stage++;
            p->timer = 0;
        }
        break;
    case FP_UNKNOWN_ITEM_DESCRIPTION:
        update_unknown_palette(p, s, 3);
        if (p->message.message_ended) {
            if (s->changed_input & KEY_A) {
                p->status.current_slot++;
                p->message.message_ended = false;
                mzm_menu_oam_set_id(&p->misc[10], 0);
                mzm_menu_oam_set_id(&p->misc[0], 0);
                p->stage = FP_SEARCH_FOR_UNKNOWN_ITEM;
            }
            break;
        }
        process_description(p, s, g);
        break;
    case FP_FINISH:
        /* Every obtained item activated */
        e->items |= e->unknown_items;
        e->unknown_items = 0;
        if (p->status.previous_right_slot != 0) p->status.current_slot = p->status.previous_right_slot;
        else if (p->status.previous_left_slot != 0) p->status.current_slot = p->status.previous_left_slot;
        else p->status.current_slot = p->status.previous_right_slot;
        p->stage++;
        p->timer = 0;
        break;
    default:
        return true;
    }
    return false;
}

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
    if (g->flag == MZM_PAUSE_FLAG_CHOZO_HINT) p->type_flags |= TYPE_ON_MAP_SCREEN | TYPE_CHOZO_STATUE_HINT;
    if (g->flag == MZM_PAUSE_FLAG_MAP_DOWNLOAD) p->type_flags |= TYPE_ON_MAP_SCREEN | TYPE_DOWNLOADING_MAP;
    if (g->flag == MZM_PAUSE_FLAG_ITEM_ACQUISITION) p->type_flags |= TYPE_ON_STATUS_SCREEN | TYPE_GETTING_NEW_ITEM;
    if (g->flag == MZM_PAUSE_FLAG_SUITLESS_ITEMS) {
        update_suit_type_suitless(g->equipment);
        p->type_flags = TYPE_ON_STATUS_SCREEN | TYPE_GETTING_NEW_ITEM | TYPE_GETTING_SUITLESS;
    }
    if (g->flag == MZM_PAUSE_FLAG_FULLY_POWERED_SUIT_ITEMS) {
        update_suit_type_fully_powered(g->equipment);
        p->type_flags = TYPE_ON_STATUS_SCREEN | TYPE_GETTING_NEW_ITEM | TYPE_GETTING_FULLY_POWERED;
    }
    bool downloading = p->type_flags & TYPE_DOWNLOADING_MAP;
    bool hint = p->type_flags & TYPE_CHOZO_STATUE_HINT;
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
    if (hint) {
        vram_lz77(s, "sChozoHintBackgroundGfx", 0);
        vram_lz77(s, "sMinimapTilesAreaNamesEnglishGfx", 0xA800);
    } else {
        vram_lz77(s, "sMotifBehindWireframeSamusGfx", 0);
        vram_lz77(s, "sPauseScreenHudGfx", 0x1000);
        vram_lz77(s, "sMinimapLettersGfx", 0x7400);
        vram_lz77(s, "sEquipmentNamesEnglishGfx", 0x6000);
        vram_lz77(s, "sMenuNamesEnglishGfx", 0xC00);
        memcpy(ppu->vram + GBA_OBJ_VRAM, ppu->vram + 0x6000, 0x800);
    }
    for (uint32_t i = 0; i < 0x1800; i += 2) ppu->vram[0xE800 + i] = 0x40, ppu->vram[0xE801 + i] = 0x11;
    vram_from16(s, 0xE000, g->hud->minimap_visited, 2048);
    if (hint) {
        vram_lz77(s, "sChozoHintBackgroundTileTable", 0xB800);
        for (uint32_t i = 0; i < 0x800; i += 2) ppu->vram[0xC000 + i] = 0x5F, ppu->vram[0xC001 + i] = 0x11;
    } else {
        ewram_lz77("sMapScreenVisorOverlayTilemap", p->visor_overlay);
        vram_from16(s, 0xC800, p->visor_overlay, 2048);
        update_bottom_visor(p, s, 2, 2);
        if (downloading) {
            update_top_visor(p, s, 0xFF);
        } else {
            ewram_lz77("sStatusScreenTilemap", p->status_base_tilemap);
            ewram_lz77("sStatusScreenBackgroundTilemap", p->status_background_tilemap);
            vram_from16(s, 0xC000, suitless(g) ? p->status_background_tilemap : p->status_base_tilemap, 0x500);
            ewram_lz77("sMapScreenOverlayTilemap", p->map_overlay);
            ewram_lz77("sWorldMapOverlayTilemap", p->world_map_overlay);
            p->on_world_map = false;
            vram_lz77(s, "sMapScreenTextBg0TileTable", 0xD800);
            ewram_lz77("sEasySleepTilemap", p->easy_sleep_tilemap);
        }
    }
    memset(s->bg_hofs, 0, sizeof s->bg_hofs);
    memset(s->bg_vofs, 0, sizeof s->bg_vofs);
    p->state = STATE_MAP_SCREEN;
    s->bg_vofs[1] = BLOCK * 16;
    if (hint) {
        p->state = STATE_CHOZO_HINT_INIT;
    } else if (downloading) {
        /* The area's map is downloaded: the BG3 map keeps the explored
         * tiles until the line reveals the new ones. */
        g->hud->progress->downloaded_maps |= (uint8_t)(1u << g->area);
        p->state = STATE_MAP_DOWNLOAD;
        mzm_hud_area_map(g->hud, g->area, g->hud->minimap_visited, g->check_event, g->context);
        init_map_download(p);
    } else if (p->type_flags & TYPE_GETTING_FULLY_POWERED) {
        p->state = STATE_FULLY_POWERED_ITEMS_INIT;
    } else if (p->type_flags & TYPE_GETTING_NEW_ITEM) {
        p->state = STATE_SUITLESS_ITEMS_INIT;
        slot_for_new_item(g, item_being_acquired(g));
    }
    /* PauseScreenGetAllMinimapData */
    for (int i = 0; i < AREAS; ++i)
        mzm_hud_area_map(g->hud, rom8("sMapScreenAreaIds", (uint32_t)i), p->maps[i], g->check_event,
                         g->context);
    if (!hint) status_draw(p, s, g);
    hint_determine_path(p, g, false);
    set_spawn_position(p, s, 0);
    if (!hint) draw_yes_no(p);
    if (p->type_flags & TYPE_ON_MAP_SCREEN) {
        p->areas_viewable = p->areas_viewable_total = p->areas_with_hints = 0;
        s->bg_hofs[1] = s->bg_vofs[1] = QUARTER_BLOCK;
    } else {
        check_areas_with_targets(p, g);
        determine_maps_viewable(p, s, g);
        load_bg_palette(p, s, g);
        vram_from16(s, 0xD000, p->map_overlay, 2048);
    }
    s->bldy = 0;
    if (hint) {
        /* BG1 (the screen's tint) over everything. */
        p->bldcnt = 1 << 1 | BLDCNT_ALPHA | 0x3F << 8;
        p->target_bld_alpha = 7 << 8 | 9;
        s->bldalpha_h = 7;
        s->bldalpha_l = 9;
    } else if (p->type_flags & TYPE_GETTING_NEW_ITEM) {
        p->bldcnt = BLDCNT_BG2_FIRST | BLDCNT_ALPHA | BLDCNT_BG2_SECOND | BLDCNT_BG3_SECOND | BLDCNT_OBJ_SECOND |
                    BLDCNT_BACKDROP_SECOND;
        p->target_bld_alpha = 0 << 8 | 22;
        s->bldalpha_h = 0;
        s->bldalpha_l = 22;
    } else {
        p->bldcnt = 1 << 2 | BLDCNT_ALPHA | 1 << 11 | 1 << 12 | 1 << 13;
        p->target_bld_alpha = 6 << 8 | 10;
        s->bldalpha_h = 6;
        s->bldalpha_l = 10;
    }
    /* Outside the windows everything blends; in the OBJ window (Samus's
     * wireframe) only BG3 and the objects show. */
    gba_ppu_write16(ppu, GBA_REG_WINOUT, 0x3F | (1 << 3 | 1 << 4) << 8);
    if (hint) {
        p->dispcnt = GBA_DCNT_BG1 | GBA_DCNT_BG2 | GBA_DCNT_BG3 | GBA_DCNT_OBJ;
        p->unk_6c = (uint16_t)(bgcnt(2, 28, 1, 3) | 1 << 6);
        p->bg3cnt = p->unk_6c;
        p->bg2cnt = bgcnt(0, 23, 3, 0);
        p->bg1cnt = bgcnt(2, 24, 2, 0);
        p->bg0cnt = 0;
    } else if (downloading) {
        /* BG1 and BG2 show screens 24 and 23, which nothing loads here (the
         * game's leftover VRAM is not kept: they are cleared). */
        p->dispcnt = GBA_DCNT_BG0 | GBA_DCNT_BG1 | GBA_DCNT_BG2 | GBA_DCNT_BG3 | GBA_DCNT_OBJ;
        p->unk_70 = bgcnt(0, 25, 0, 0);
        p->unk_6c = (uint16_t)(bgcnt(2, 28, 1, 3) | 1 << 6);
        p->bg3cnt = p->unk_6c;
        p->bg2cnt = bgcnt(0, 23, 3, 0);
        p->bg1cnt = bgcnt(2, 24, 2, 0);
        p->bg0cnt = p->unk_70;
    } else {
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
        if (p->type_flags & TYPE_GETTING_NEW_ITEM) {
            p->dispcnt = GBA_DCNT_BG2 | GBA_DCNT_OBJ | GBA_DCNT_OBJWIN;
            p->bg2cnt = p->unk_74;
        } else {
            p->bg2cnt = p->unk_72;
        }
    }
    load_area_names_and_icons(p, s, g);
    s->oam_y_offset = s->oam_x_offset = 4;
    update_map_arrows(p, g);
    update_boss_icons(p, g);
    if (downloading && p->boss_icon[0].oam_id != BOSS_ICON_CROSSMARK) p->boss_icon[0].not_drawn = true;
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

/* MapScreenHandler */
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
        if (action == 1) {
            mzm_sfx_play(p->on_world_map ? SOUND_MOVING_MAP_MUFFLED : SOUND_MOVING_MAP);
        } else if (s->changed_input & (KEY_R | KEY_L)) {
            /* The status screen or easy sleep */
            if (s->changed_input & KEY_R) {
                p->state = STATE_STATUS_SCREEN_INIT;
            } else {
                mzm_sfx_play(SOUND_OPENING_EASY_SLEEP_SCREEN);
                p->state = STATE_EASY_SLEEP_INIT;
            }
            p->timer = 0;
            p->stage = 0;
            toggle_world_map(p, s, g, true);
            action = 2;
        }
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
 * only off the world map), the status screen and easy sleep. */
static bool call_state_handler(MzmPauseScreen *p, MzmScreen *s, const MzmPauseGame *g) {
    maintained_input(p, s);
    p->timer++;
    switch (p->state) {
    case STATE_MAP_SCREEN:
        if (!(s->changed_input & (KEY_START | KEY_B)) || p->on_world_map) {
            map_screen(p, s, g);
            return false;
        }
        return true;
    case STATE_STATUS_SCREEN:
        status_screen(p, s, g);
        break;
    case STATE_STATUS_SCREEN_INIT:
        if (p->is_fading) {
            apply_fading(p, s);
        } else if (status_screen_init(p, s, g)) {
            p->state = STATE_STATUS_SCREEN;
            p->stage = 0;
            p->timer = 0;
        }
        break;
    case STATE_STATUS_SCREEN_LEAVING:
        if (p->is_fading) {
            apply_fading(p, s);
        } else if (quit_status_screen(p, s, g)) {
            p->state = STATE_MAP_SCREEN;
            p->stage = 0;
            p->timer = 0;
        }
        break;
    case STATE_EASY_SLEEP:
        if (easy_sleep(p, s)) p->state = STATE_EASY_SLEEP_LEAVING;
        break;
    case STATE_MAP_DOWNLOAD:
        if (map_download(p, s, g)) p->state = STATE_MAP_DOWNLOAD_ENDED;
        break;
    case STATE_CHOZO_HINT_INIT:
        p->state = STATE_CHOZO_HINT;
        p->timer = 0;
        p->stage = 0;
        break;
    case STATE_CHOZO_HINT:
        if (chozo_hint(p, s, g)) p->state = STATE_CHOZO_HINT_ENDED;
        break;
    case STATE_CHOZO_HINT_ENDED:
        p->state = p->type_flags & TYPE_ON_MAP_SCREEN ? STATE_LEAVING_AFTER_HINT : STATE_MAP_SCREEN;
        break;
    case STATE_LEAVING_AFTER_HINT:
        mzm_sfx_music_fade(160);    /* CONVERT_SECONDS(2.f) + TWO_THIRD_SECOND */
        return true;
    case STATE_MAP_DOWNLOAD_ENDED:
        p->state = STATE_WAIT_TO_LEAVE;
        p->stage = 0;
        p->timer = 0;
        break;
    case STATE_WAIT_TO_LEAVE:
        return s->changed_input & (KEY_START | KEY_A | KEY_B) && !p->on_world_map;
    case STATE_SUITLESS_ITEMS_INIT:
        if (suit_changing_start(p, s, g)) {
            p->state = STATE_SUITLESS_ITEMS;
            p->stage = 0;
            p->timer = 0;
        }
        break;
    case STATE_SUITLESS_ITEMS:
        if (suitless_items(p, s, g)) {
            update_status_oam(p, s, g, 4);
            p->timer = 0;
            p->state = STATE_SUITLESS_ITEMS_ENDED;
        }
        break;
    case STATE_SUITLESS_ITEMS_ENDED:
        return p->timer > 12;                           /* CONVERT_SECONDS(.2f) */
    case STATE_FULLY_POWERED_ITEMS_INIT:
        if (suit_changing_start(p, s, g)) {
            p->state = STATE_FULLY_POWERED_ITEMS;
            p->stage = 0;
            p->timer = 0;
        }
        break;
    case STATE_FULLY_POWERED_ITEMS:
        if (fully_powered_items(p, s, g)) {
            p->timer = 0;
            p->state = STATE_FULLY_POWERED_ITEMS_ENDED;
        }
        break;
    case STATE_FULLY_POWERED_ITEMS_ENDED:
        return p->timer > 12;                           /* CONVERT_SECONDS(.2f) */
    case STATE_EASY_SLEEP_INIT:
        if (p->is_fading) {
            apply_fading(p, s);
        } else if (easy_sleep_init(p, s)) {
            p->state = STATE_EASY_SLEEP;
            p->stage = 0;
            p->timer = 0;
        }
        break;
    case STATE_EASY_SLEEP_LEAVING:
        if (p->is_fading) {
            apply_fading(p, s);
        } else if (quit_easy_sleep(p, s)) {
            p->state = STATE_MAP_SCREEN;
            p->stage = 0;
            p->timer = 0;
        }
        break;
    default:
        break;
    }
    return false;
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
        update_map_arrows(p, g);
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
