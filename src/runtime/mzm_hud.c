/* SPDX-License-Identifier: GPL-3.0-only */
/* hud.c, hud_generic.c and minimap.c (see mzm_hud.h). The graphics are read
 * from the ROM bank at the native symbols and copied where the native DMA
 * copies them; DMA3_COPY_16(src, dst, n) copies n halfwords. */
#include "mzm_hud.h"

#include <string.h>

#include "mzm_rom.h"

enum {
    VRAM_OBJ = 0x10000,
    HL_NOT_HIGHLIGHTED = 0, HL_SELECTED = 1, HL_HIGHLIGHTED = 2,
    MINIMAP_UPDATE_NONE = 0, MINIMAP_UPDATE_UPPER = 1, MINIMAP_UPDATE_MIDDLE = 2,
    MINIMAP_UPDATE_LOWER = 3,
    AREA_KRAID = 1, AREA_RIDLEY = 3, AREA_CRATERIA = 5, AREAS_NORMAL = 7,
    BLOCK = 64, SCREEN_X_BLOCKS = 15, SCREEN_Y_BLOCKS = 10, SCREEN_PADDING = 2,
    SUITLESS_BAR_ELEMS = 56, SUITLESS_BAR_SIZE = 256,
    MINIMAP_TILE_BACKGROUND = 0x140, ROOM_ENTRY_SIZE = 60, AREA_NAME_SIZE = 12
};

/* HUD_*_X / HUD_*_Y in pixels */
enum {
    HEALTH_Y = 2, HEALTH_X = 0, CHARGE_Y = 10, CHARGE_X = 1, MISSILES_Y = 2, MISSILES_X = 54,
    SUPERS_Y = 2, SUPERS_X = 80, POWER_BOMBS_Y = 2, POWER_BOMBS_X = 106, MINIMAP_Y = -6,
    MINIMAP_RIGHT = 26
};

static uint32_t sym(const char *name) { return mzm_rom_symbol(name, NULL); }

/* DMA3_COPY_16 of `bytes` from a ROM address to VRAM */
static void vram_from_rom(MzmHud *h, uint32_t vram, uint32_t address, uint32_t bytes) {
    const uint8_t *src = mzm_rom_ptr(address, bytes);
    if (src && vram + bytes <= GBA_VRAM_SIZE) memcpy(h->ppu->vram + vram, src, bytes);
}

static void vram_from(MzmHud *h, uint32_t vram, const void *src, uint32_t bytes) {
    if (vram + bytes <= GBA_VRAM_SIZE) memcpy(h->ppu->vram + vram, src, bytes);
}

uint8_t mzm_hud_area_index(const char *name) {
    static const char *const names[] = {"Brinstar", "Kraid", "Norfair", "Ridley",
                                        "Tourian", "Crateria", "Chozodia"};
    for (uint8_t i = 0; i < AREAS_NORMAL; ++i)
        if (name && !strcmp(name, names[i])) return i;
    return 0xFF;
}

bool mzm_hud_room_map(uint8_t area, uint8_t room, uint8_t *map_x, uint8_t *map_y) {
    uint32_t size;
    uint32_t table = mzm_rom_symbol("sAreaRoomEntryPointers", &size);
    if (!table || area >= size / 4) return false;
    uint32_t entries = mzm_rom_u32(table + 4u * area);
    const uint8_t *entry = mzm_rom_ptr(entries + (uint32_t)room * ROOM_ENTRY_SIZE, ROOM_ENTRY_SIZE);
    if (!entry) return false;
    *map_x = entry[53];
    *map_y = entry[54];
    return true;
}

/* HudGenericResetHudData (the particles are reset by their own module) */
static void reset_hud_data(MzmHud *h) {
    h->energy = h->max_energy = (MzmHudDigits){15, 15, 15, 15};
    h->missiles = h->power_bombs = h->super_missiles = (MzmHudDigits){15, 15, 15, 0};
    h->missile_highlight = h->power_bomb_highlight = h->super_missile_highlight = 0;
    h->update_minimap = MINIMAP_UPDATE_LOWER;
}

void mzm_hud_init(MzmHud *h, GbaPpu *ppu, MzmMinimapProgress *progress, int screen_width) {
    memset(h, 0, sizeof *h);
    h->ppu = ppu;
    h->progress = progress;
    h->screen_width = screen_width;
    h->area_before_transition = 0xFF;
    h->minimap_x = h->minimap_y = 0xFF;
    uint32_t size;
    const uint8_t *gfx = mzm_rom_data("sCommonSpritesGfx", &size);
    const uint8_t *pal = mzm_rom_data("sCommonSpritesPal", NULL);
    h->ready = gfx && pal && sym("sMinimapDataPointers") && sym("sMinimapTilesGfx");
    if (!h->ready) return;
    /* HudGenericLoadCommonSpriteGfx */
    vram_from(h, VRAM_OBJ + 0x800, gfx, size);
    /* DMA3_COPY_16(sCommonSpritesPal, PALRAM_BASE + 0x240, ...) */
    for (int i = 0; i < 96; ++i) ppu->palette[0x120 + i] = (uint16_t)(pal[2 * i] | pal[2 * i + 1] << 8);
    gba_ppu_write16(ppu, GBA_REG_DISPCNT, GBA_DCNT_OBJ);
    reset_hud_data(h);
}

/* ---------------------------------------------------------------- HUD -- */

/* HudUpdateEnergyTanks */
static void energy_tanks(MzmHud *h, uint32_t dst, uint32_t normal, uint32_t refill, uint8_t tanks,
                         uint8_t stage) {
    if (stage < 3) vram_from_rom(h, dst + 0x40, refill + 0xA0u * stage, 160);
    else if (stage == 3) vram_from_rom(h, dst + 0x40, normal + 0xA0u * tanks, 160);
}

/* HudDrawEnergy(0) */
static void draw_energy(MzmHud *h, const MzmHudFrame *f) {
    static const char *const tanks_gfx[13] = {
        "sEnergyTanksGfx_Zero", "sEnergyTanksGfx_One", "sEnergyTanksGfx_Two",
        "sEnergyTanksGfx_Three", "sEnergyTanksGfx_Four", "sEnergyTanksGfx_Five",
        "sEnergyTanksGfx_Six", "sEnergyTanksGfx_Seven", "sEnergyTanksGfx_Eight",
        "sEnergyTanksGfx_Nine", "sEnergyTanksGfx_Ten", "sEnergyTanksGfx_Eleven",
        "sEnergyTanksGfx_Twelve"};
    static const char *const refill_gfx[13] = {
        NULL, "sEnergyTanksRefillGfx_One", "sEnergyTanksRefillGfx_Two",
        "sEnergyTanksRefillGfx_Three", "sEnergyTanksRefillGfx_Four",
        "sEnergyTanksRefillGfx_Five", "sEnergyTanksRefillGfx_Six",
        "sEnergyTanksRefillGfx_Seven", "sEnergyTanksRefillGfx_Eight",
        "sEnergyTanksRefillGfx_Nine", "sEnergyTanksRefillGfx_Ten",
        "sEnergyTanksRefillGfx_Eleven", "sEnergyTanksRefillGfx_Twelve"};
    const uint32_t dst = VRAM_OBJ + 0xA00;
    uint8_t need = 0, digit;
    uint16_t v = (uint16_t)f->equipment->max_energy;
    digit = (uint8_t)((uint8_t)(v / 1000) % 10);
    if (h->max_energy.thousands != digit) { h->max_energy.thousands = digit; need++; }
    uint8_t tanks = (uint8_t)(v / 100);
    digit = tanks % 10;
    if (h->max_energy.hundreds != digit) { h->max_energy.hundreds = digit; need++; }
    digit = (uint8_t)((uint8_t)(v / 10) % 10);
    if (h->max_energy.tens != digit) { h->max_energy.tens = digit; need++; }
    digit = v % 10;
    if (h->max_energy.ones != digit) { h->max_energy.ones = digit; need++; }
    uint16_t energy = (uint16_t)f->equipment->energy;
    digit = (uint8_t)((uint8_t)(energy / 1000) % 10);
    if (h->energy.thousands != digit) { h->energy.thousands = digit; need++; }
    uint16_t full = (uint8_t)(energy / 100);
    digit = (uint8_t)(full % 10);
    if (h->energy.hundreds != digit) { h->energy.hundreds = digit; need++; }
    uint32_t tens_gfx = sym("sEnergyDigitsTensGfx"), ones_gfx = sym("sEnergyDigitsOnesGfx");
    digit = (uint8_t)((uint8_t)(energy / 10) % 10);
    if (h->energy.tens != digit) {
        h->energy.tens = digit;
        need++;
        vram_from_rom(h, dst, tens_gfx + digit * 32u, 32);
    }
    digit = energy % 10;
    if (h->energy.ones != digit) {
        h->energy.ones = digit;
        need++;
        vram_from_rom(h, dst + 32, ones_gfx + digit * 32u, 32);
    }
    if (need) {
        h->low_health = energy < 30;
        if (tanks == 0) vram_from_rom(h, dst + 64, sym(tanks_gfx[0]), 160);
        else if (tanks <= 12) vram_from_rom(h, dst + 64, sym(tanks_gfx[tanks]) + full * 160u, 160);
    }
    uint8_t refill = f->refill_animation ? f->refill_animation[0] : 0;
    if (!refill) return;
    const char *digits;
    if (refill == 13) { digits = "sEnergyDigitsRefill1Gfx"; need = 0; }
    else if (refill == 10) { digits = "sEnergyDigitsRefill2Gfx"; need = 1; }
    else if (refill == 4) { digits = "sEnergyDigitsRefill3Gfx"; need = 2; }
    else if (refill == 1) { digits = "sEnergyDigitsTensGfx"; need = 3; }
    else return;
    uint32_t gfx = sym(digits);
    vram_from_rom(h, dst, gfx + h->energy.tens * 32u, 32);
    vram_from_rom(h, dst + 32, gfx + h->energy.ones * 32u + 10 * 32, 32);
    if (tanks >= 1 && tanks <= 12)
        energy_tanks(h, dst, sym(tanks_gfx[tanks]), sym(refill_gfx[tanks]), tanks, need);
}

/* HudDrawMissileDigits / HudDrawSuperMissileDigits / HudDrawPowerBombDigits:
 * two 4-pixel digits per tile; `three` draws hundreds, tens and ones then
 * the symbol's lower half, otherwise tens and ones then the lower half. */
static void ammo_digits(MzmHud *h, uint32_t gfx, bool three, uint16_t a, uint16_t b, uint16_t c,
                        uint16_t highlight) {
    const uint8_t *src = mzm_rom_ptr(gfx, 2304);
    if (!src) return;
    uint8_t *out = h->ammo_digits_gfx;
    int row = 0;
    if (three) {
        for (int i = 0; i < 8; ++i) {
            out[row++] = src[a++]; out[row++] = src[a]; a += 3;
            out[row++] = src[b++]; out[row++] = src[b]; b += 3;
        }
        row = 0x20;
        for (int i = 0; i < 8; ++i) {
            out[row++] = src[c++]; out[row++] = src[c]; c += 3;
            out[row++] = src[highlight++]; out[row++] = src[highlight]; highlight += 3;
        }
    } else {
        for (int i = 0; i < 8; ++i) {
            out[row++] = src[b++]; out[row++] = src[b]; b += 3;
            out[row++] = src[c++]; out[row++] = src[c]; c += 3;
        }
        row = 0x20;
        for (int i = 0; i < 8; ++i) {
            out[row++] = src[highlight++]; out[row++] = src[highlight]; highlight += 3;
            out[row++] = 0; out[row++] = 0;
        }
    }
}

typedef struct {
    MzmHudDigits *digits;
    uint8_t *highlight;
    int amount;
    uint8_t refill;
    bool three;                     /* missiles: three digits */
    const char *digits_gfx;         /* sMissileDigitsGfx / sPowerBombDigitsGfx */
    const char *symbol_gfx;         /* sXHudGfx_Inactive */
    uint32_t symbol_vram, digits_vram;
} Ammo;

/* HudDrawMissiles / HudDrawPowerBomb / HudDrawSuperMissiles */
static void draw_ammo(MzmHud *h, const Ammo *a, bool update_highlight) {
    uint8_t need = 0;
    if (update_highlight) {
        need++;
    } else {
        uint16_t amount = (uint16_t)a->amount;
        uint8_t digit = (uint8_t)(amount / 100 % 10);
        if (a->digits->hundreds != digit) { a->digits->hundreds = digit; need++; }
        digit = (uint8_t)(amount / 10 % 10);
        if (a->digits->tens != digit) { a->digits->tens = digit; need++; }
        digit = (uint8_t)(amount % 10);
        if (a->digits->ones != digit) { a->digits->ones = digit; need++; }
    }
    uint32_t gfx = sym(a->digits_gfx);
    uint16_t hundreds, tens, ones, offset;
    if (need) {
        hundreds = a->digits->hundreds;
        tens = a->digits->tens;
        ones = a->digits->ones;
        if (hundreds == 0) {
            hundreds = 10;
            if (tens == 0) tens = 10;
        }
        offset = (uint16_t)(*a->highlight * 384);
        hundreds = (uint16_t)(hundreds * 32 + offset);
        tens = (uint16_t)(tens * 32 + offset);
        ones = (uint16_t)(ones * 32 + offset);
        ammo_digits(h, gfx, a->three, hundreds, tens, ones, (uint16_t)(offset + 352));
        vram_from(h, a->digits_vram, h->ammo_digits_gfx, 64);
    }
    if (!a->refill || update_highlight) return;
    hundreds = a->digits->hundreds;
    tens = a->digits->tens;
    ones = a->digits->ones;
    if (hundreds == 0) {
        hundreds = 10;
        if (tens == 0) tens = 10;
    }
    uint32_t symbol = sym(a->symbol_gfx);
    if (a->refill == 13) { offset = 0x480; vram_from_rom(h, a->symbol_vram, symbol + 3 * 64, 64); }
    else if (a->refill == 10) { offset = 0x600; vram_from_rom(h, a->symbol_vram, symbol + 4 * 64, 64); }
    else if (a->refill == 4) { offset = 0x780; vram_from_rom(h, a->symbol_vram, symbol + 5 * 64, 64); }
    else if (a->refill == 1) {
        offset = (uint16_t)(*a->highlight * 384);
        vram_from_rom(h, a->symbol_vram, symbol + *a->highlight * 64u, 64);
    } else {
        return;
    }
    ammo_digits(h, gfx, a->three, (uint16_t)(hundreds * 32 + offset), (uint16_t)(tens * 32 + offset),
                (uint16_t)(ones * 32 + offset), (uint16_t)(352 + offset));
    vram_from(h, a->digits_vram, h->ammo_digits_gfx, 64);
}

static Ammo missiles(MzmHud *h, const MzmHudFrame *f) {
    return (Ammo){&h->missiles, &h->missile_highlight, f->equipment->missiles,
                  f->refill_animation ? f->refill_animation[1] : 0, true, "sMissileDigitsGfx",
                  "sMissileHudGfx_Inactive", VRAM_OBJ + 0xE00, VRAM_OBJ + 0xE40};
}
static Ammo super_missiles(MzmHud *h, const MzmHudFrame *f) {
    return (Ammo){&h->super_missiles, &h->super_missile_highlight, f->equipment->super_missiles,
                  f->refill_animation ? f->refill_animation[2] : 0, false, "sMissileDigitsGfx",
                  "sSuperMissileHudGfx_Inactive", VRAM_OBJ + 0xE80, VRAM_OBJ + 0xEC0};
}
static Ammo power_bombs(MzmHud *h, const MzmHudFrame *f) {
    return (Ammo){&h->power_bombs, &h->power_bomb_highlight, f->equipment->power_bombs,
                  f->refill_animation ? f->refill_animation[3] : 0, false, "sPowerBombDigitsGfx",
                  "sPowerBombHudGfx_Inactive", VRAM_OBJ + 0xB00, VRAM_OBJ + 0xB40};
}

/* One symbol of HudUpdateGfx: `status` takes the Inactive (0), Selected (1)
 * or Active (2) graphics. */
static void set_symbol(MzmHud *h, const Ammo *a, uint8_t status) {
    vram_from_rom(h, a->symbol_vram, sym(a->symbol_gfx) + status * 64u, 64);
    *a->highlight = status;
    draw_ammo(h, a, true);
}

/* HudUpdateGfx */
static void update_gfx(MzmHud *h, const MzmHudFrame *f) {
    const uint8_t wh = f->weapon_highlighted;
    Ammo m = missiles(h, f), p = power_bombs(h, f), s = super_missiles(h, f);
    bool missile_off = f->missiles_selected || f->equipment->missiles == 0;
    if (h->missile_highlight == HL_HIGHLIGHTED) {
        if (!(wh & MZM_WH_MISSILE)) set_symbol(h, &m, missile_off ? HL_NOT_HIGHLIGHTED : HL_SELECTED);
    } else if (h->missile_highlight == HL_SELECTED) {
        if (!(wh & MZM_WH_MISSILE)) {
            if (missile_off) set_symbol(h, &m, HL_NOT_HIGHLIGHTED);
        } else {
            set_symbol(h, &m, HL_HIGHLIGHTED);
        }
    } else if (h->missile_highlight == HL_NOT_HIGHLIGHTED) {
        if (wh & MZM_WH_MISSILE) set_symbol(h, &m, HL_HIGHLIGHTED);
        else if (!missile_off) set_symbol(h, &m, HL_SELECTED);
    }
    /* The power bomb's SELECTED check follows its HIGHLIGHTED one without
     * an else, as in the native. */
    if (h->power_bomb_highlight == HL_HIGHLIGHTED && !(wh & MZM_WH_POWER_BOMB))
        set_symbol(h, &p, f->equipment->power_bombs ? HL_SELECTED : HL_NOT_HIGHLIGHTED);
    if (h->power_bomb_highlight == HL_SELECTED) {
        if (!(wh & MZM_WH_POWER_BOMB)) {
            if (f->equipment->power_bombs == 0) set_symbol(h, &p, HL_NOT_HIGHLIGHTED);
        } else {
            set_symbol(h, &p, HL_HIGHLIGHTED);
        }
    } else if (h->power_bomb_highlight == HL_NOT_HIGHLIGHTED) {
        if (wh & MZM_WH_POWER_BOMB) set_symbol(h, &p, HL_HIGHLIGHTED);
        else if (f->equipment->power_bombs != 0) set_symbol(h, &p, HL_SELECTED);
    }
    if (h->super_missile_highlight == HL_HIGHLIGHTED) {
        if (!(wh & MZM_WH_SUPER_MISSILE))
            set_symbol(h, &s, f->missiles_selected ? HL_SELECTED : HL_NOT_HIGHLIGHTED);
    } else if (h->super_missile_highlight == HL_SELECTED) {
        if (!(wh & MZM_WH_SUPER_MISSILE)) {
            if (!f->missiles_selected) set_symbol(h, &s, HL_NOT_HIGHLIGHTED);
        } else {
            set_symbol(h, &s, HL_HIGHLIGHTED);
        }
    } else if (h->super_missile_highlight == HL_NOT_HIGHLIGHTED) {
        if (wh & MZM_WH_SUPER_MISSILE) set_symbol(h, &s, HL_HIGHLIGHTED);
        else if (f->missiles_selected) set_symbol(h, &s, HL_SELECTED);
    }
    if (h->update_minimap != MINIMAP_UPDATE_NONE) {
        h->update_minimap--;
        vram_from(h, VRAM_OBJ + 0x1F80 + h->update_minimap * 1024u,
                  &h->minimap_gfx[h->update_minimap * 24], 24 * 4);
    }
}

/* HudDrawSuitless */
static void draw_suitless(MzmHud *h, const MzmHudFrame *f) {
    uint8_t counter = f->charge_counter;
    if (counter >= SUITLESS_BAR_ELEMS * 2)
        counter = (uint8_t)((uint8_t)(f->charge_counter - SUITLESS_BAR_ELEMS * 2) / 4 +
                            SUITLESS_BAR_ELEMS - 4);
    else
        counter /= 2;
    if (counter < SUITLESS_BAR_ELEMS)
        vram_from_rom(h, VRAM_OBJ + 0x1100, sym("sSuitlessHudChargeBarGfx") + counter * SUITLESS_BAR_SIZE,
                      SUITLESS_BAR_SIZE);
}

/* One OAM entry of HudUpdateOam: the attributes start at zero. */
static void oam(MzmHud *h, int *slot, int y, int x, int shape, int size, int tile, int palette) {
    uint16_t *o = &h->ppu->oam[*slot * 4];
    o[0] = (uint16_t)((y & 0xFF) | shape << 14);
    o[1] = (uint16_t)((x & 0x1FF) | size << 14);
    o[2] = (uint16_t)(tile | palette << 12);
    ++*slot;
}

/* HudUpdateOam (the slots after the HUD are hidden: the runtime draws the
 * other sprites itself) */
static void update_oam(MzmHud *h, const MzmHudFrame *f) {
    enum { SQUARE = 0, HORIZONTAL = 1, H_32x8 = 1, S_8x8 = 0, S_32x32 = 2 };
    int slot = 0;
    if (!h->hidden) {
        const MzmEquipment *e = f->equipment;
        oam(h, &slot, HEALTH_Y, HEALTH_X, HORIZONTAL, H_32x8, 0x20 * 2 + 0x10, 4);
        oam(h, &slot, HEALTH_Y, HEALTH_X + 32, HORIZONTAL, H_32x8, 0x20 * 2 + 0x14, 4);
        if (e->suit == MZM_SUIT_SUITLESS) {
            oam(h, &slot, CHARGE_Y, CHARGE_X, HORIZONTAL, H_32x8, 0x20 * 4 + 0x8, 4);
            oam(h, &slot, CHARGE_Y, CHARGE_X + 32, HORIZONTAL, H_32x8, 0x20 * 4 + 0xC, 4);
        } else {
            if (e->max_missiles)
                oam(h, &slot, MISSILES_Y, MISSILES_X, HORIZONTAL, H_32x8, 0x20 * 3 + 0x10, 4);
            if (e->max_super_missiles)
                oam(h, &slot, SUPERS_Y, SUPERS_X, HORIZONTAL, H_32x8, 0x20 * 3 + 0x14, 4);
            if (e->max_power_bombs)
                oam(h, &slot, POWER_BOMBS_Y, POWER_BOMBS_X, HORIZONTAL, H_32x8, 0x20 * 2 + 0x18, 4);
        }
        int minimap_x = h->screen_width - MINIMAP_RIGHT;
        oam(h, &slot, MINIMAP_Y + 16, minimap_x + 8, SQUARE, S_8x8,
            f->frame8 & 8 ? 0x20 * 5 + 0x1F : 0x20 * 10, 5);
        oam(h, &slot, MINIMAP_Y, minimap_x, SQUARE, S_32x32, 0x20 * 6 + 0x1C, 5);
    }
    for (int i = slot; i < 128; ++i) {
        h->ppu->oam[i * 4] = 0x200;
        h->ppu->oam[i * 4 + 1] = h->ppu->oam[i * 4 + 2] = 0;
    }
}

/* ------------------------------------------------------------ minimap -- */

static uint32_t *visited_row(MzmHud *h, uint8_t area, uint32_t y) {
    return area < MZM_HUD_AREAS && y < MZM_MINIMAP_SIZE ? &h->progress->visited[area][y] : NULL;
}

/* MinimapSetTileAsExplored */
static void set_tile_explored(MzmHud *h, const MzmHudRoom *r) {
    if (h->ship_landing || h->minimap_x >= MZM_MINIMAP_SIZE) return;
    uint32_t *row = visited_row(h, r->area, h->minimap_y);
    if (row) *row |= 1u << h->minimap_x;
}

/* sMinimapAreaNames[i]: area, mapX, mapY, xOffset, yOffset of side 1 or 2 */
static const uint8_t *area_name(int i) {
    uint32_t base = sym("sMinimapAreaNames");
    return base ? mzm_rom_ptr(base + (uint32_t)i * AREA_NAME_SIZE, AREA_NAME_SIZE) : NULL;
}

/* MinimapCheckSetAreaNameAsExplored */
static void check_area_name_explored(MzmHud *h, bool after_transition) {
    if (h->ship_landing || h->last_area_name.flags == 0) return;
    int set = 0, i = 0;
    const uint8_t *n = NULL;
    if (after_transition) {
        if (h->last_area_name.flags & 0x80) {
            i = h->last_area_name.flags & 0x7F;
            n = area_name(i);
            if (n && n[0] == h->area_before_transition) set = 1;
            else if (n && n[5] == h->area_before_transition) set = 2;
        }
    } else {
        for (i = 0; (n = area_name(i)) && n[0] != 0xFF; ++i) {
            if (n[0] == h->last_area_name.area) {
                if (h->last_area_name.map_x == n[1] && h->last_area_name.map_y == n[2]) set = 1;
            } else if (n[5] == h->last_area_name.area) {
                if (h->last_area_name.map_x == n[6] && h->last_area_name.map_y == n[7]) set = 2;
            }
            if (set) break;
        }
        if (set) h->last_area_name.flags = (uint8_t)(i | 0x80);
    }
    if (!set || !n) {
        h->last_area_name.flags = 0;
        return;
    }
    const uint8_t *side = set == 1 ? n : n + 5;
    uint8_t area = side[0];
    uint32_t x = (uint32_t)(side[1] + (int8_t)side[3] - 1), y = (uint32_t)(side[2] + (int8_t)side[4] - 1);
    h->last_area_name.map_x = (uint8_t)(side[1] - 1);
    h->last_area_name.map_y = (uint8_t)(side[2] - 1);
    uint32_t *row = visited_row(h, area, y);
    for (uint32_t j = 0; row && x + j < MZM_MINIMAP_SIZE && y < MZM_MINIMAP_SIZE; ++j) {
        uint16_t tile = h->minimap_data[x + j + y * MZM_MINIMAP_SIZE] & 0x3FF;
        if (tile < 0x141 || tile > 0x15E) break;
        *row |= 1u << (x + j);
    }
    if (after_transition) {
        row = visited_row(h, area, h->last_area_name.map_y);
        if (row && h->last_area_name.map_x < MZM_MINIMAP_SIZE) *row |= 1u << h->last_area_name.map_x;
        h->last_area_name.flags = 0;
    }
}

/* MinimapCheckForUnexploredTile */
static void check_unexplored(MzmHud *h, const MzmHudRoom *r, const MzmHudFrame *f) {
    if (h->update_minimap != MINIMAP_UPDATE_NONE) return;
    uint16_t x = (uint16_t)(f->samus_x - BLOCK * 2), y = (uint16_t)(f->samus_y - BLOCK * 2);
    if (x & 0x8000) {
        x = 0;
    } else {
        uint16_t clip = (uint16_t)(r->width_blocks * BLOCK - BLOCK * 4);
        if (f->samus_x >= clip) x = (uint16_t)(clip - 1);
    }
    if (y & 0x8000) {
        y = 0;
    } else {
        uint16_t clip = (uint16_t)(r->height_blocks * BLOCK - BLOCK * 4);
        if (f->samus_y >= clip) y = (uint16_t)(clip - 1);
    }
    x = (uint16_t)(x / BLOCK / SCREEN_X_BLOCKS);
    y = (uint16_t)(y / BLOCK / SCREEN_Y_BLOCKS);
    if (h->minimap_x != (uint8_t)(x + r->map_x)) {
        h->minimap_x = (uint8_t)(x + r->map_x);
        h->update_minimap = MINIMAP_UPDATE_LOWER;
    }
    if (h->minimap_y != (uint8_t)(y + r->map_y)) {
        h->minimap_y = (uint8_t)(y + r->map_y);
        h->update_minimap = MINIMAP_UPDATE_LOWER;
    }
}

/* MinimapUpdateForExploredTiles */
static void update_explored_tiles(MzmHud *h, const MzmHudRoom *r) {
    if (h->ship_landing || h->minimap_x >= MZM_MINIMAP_SIZE || h->minimap_y >= MZM_MINIMAP_SIZE) return;
    uint32_t offset = h->minimap_x + h->minimap_y * MZM_MINIMAP_SIZE;
    uint16_t *tile = &h->minimap_visited[offset];
    if (*tile & 0xF000) return;
    uint16_t map = h->minimap_data[offset];
    *tile = map & 0xF000 ? map : (uint16_t)(map | 0x1000);
    if ((uint32_t)((map & 0x3FF) - 0x141) < 4) {
        h->last_area_name.flags = 1;
        h->last_area_name.area = r->area;
        h->last_area_name.map_x = (uint8_t)(h->minimap_x + 1);
        h->last_area_name.map_y = (uint8_t)(h->minimap_y + 1);
    }
}

/* MinimapCopyTile*Gfx: each source byte is two pixels whose colors the
 * sPauseScreen_40d74c / _40d6fc tables move to the palette. */
static void copy_tile(uint32_t *dst, uint32_t tile, uint8_t palette, int flip) {
    uint32_t gfx = sym("sMinimapTilesGfx");
    uint32_t hi = sym("sPauseScreen_40d74c"), lo = sym("sPauseScreen_40d6fc");
    if (!gfx || !hi || !lo) return;
    int p = palette << 4;
    for (int i = 0; i < 8; ++i) {
        int row = flip & 2 ? 7 - i : i;
        uint32_t value = 0;
        for (int k = 0; k < 4; ++k) {
            int column = flip & 1 ? 3 - k : k;
            uint8_t t = mzm_rom_u8(gfx + tile + (uint32_t)(row * 4 + column));
            uint32_t first = flip & 1 ? t % 16u : t / 16u, second = flip & 1 ? t / 16u : t % 16u;
            uint8_t b = (uint8_t)(mzm_rom_u8(hi + first + (uint32_t)p) |
                                  mzm_rom_u8(lo + second + (uint32_t)p));
            value |= (uint32_t)b << (8 * k);
        }
        dst[i] = value;
    }
}

/* MinimapDraw */
static void minimap_draw(MzmHud *h) {
    if (h->update_minimap == MINIMAP_UPDATE_NONE) return;
    int y_offset;
    if (h->update_minimap == MINIMAP_UPDATE_LOWER) y_offset = 1;
    else if (h->update_minimap == MINIMAP_UPDATE_MIDDLE) y_offset = 0;
    else if (h->update_minimap == MINIMAP_UPDATE_UPPER) y_offset = -1;
    else {
        h->update_minimap = MINIMAP_UPDATE_NONE;
        return;
    }
    uint32_t *dst = &h->minimap_gfx[(h->update_minimap - 1) * 24];
    for (int x_offset = -1; x_offset < 2; ++x_offset, dst += 8) {
        int limit = MZM_MINIMAP_SIZE - 1;
        int x = h->minimap_x + x_offset, y = h->minimap_y + y_offset;
        if (x > limit) x = -1;
        if (y > limit) y = -1;
        if (y < 0 || x < 0) x = y = limit;
        uint16_t raw = h->minimap_visited[y * MZM_MINIMAP_SIZE + x];
        uint32_t tile = raw & 0x3FF;   /* English: no Hiragana offset */
        copy_tile(dst, tile << 5, (uint8_t)(raw >> 12), (raw & 0xC00) >> 10);
    }
}

/* MinimapSetTilesWithObtainedItems */
static void set_tiles_with_obtained_items(MzmHud *h, uint8_t area, uint16_t *dst) {
    if (area >= MZM_HUD_AREAS) return;
    for (int i = 0; i < MZM_MINIMAP_SIZE; ++i) {
        uint32_t tile = h->progress->obtained[area][i];
        for (int j = 0; j < MZM_MINIMAP_SIZE && tile; ++j)
            if (tile & 1u << j) {
                tile ^= 1u << j;
                dst[i * MZM_MINIMAP_SIZE + j]++;
            }
    }
}

/* The boss icon change of PauseScreenGetMinimapData and MinimapUpdateChunk:
 * the ship's 3x1 tiles or Kraid's and Ridley's 2x2 tiles. */
static bool update_boss_icon(uint8_t area, uint16_t *map, uint16_t *visited) {
    const uint8_t *chunks = mzm_rom_data("sMapChunksToUpdate", NULL);
    if (!chunks) return false;
    uint16_t ship = (uint16_t)(chunks[4] | chunks[5] << 8), kraid = (uint16_t)(chunks[0] | chunks[1] << 8),
             ridley = (uint16_t)(chunks[2] | chunks[3] << 8);
    if (area == AREA_CRATERIA) {
        if ((map[0] & 0x3FF) != ship) return false;
        for (int i = 0; i < 3; ++i) {
            map[i]++;
            if (visited && visited[i] & 0xF000) visited[i]++;
        }
        return true;
    }
    if ((area == AREA_KRAID && (map[0] & 0x3FF) == kraid) ||
        (area == AREA_RIDLEY && (map[0] & 0x3FF) == ridley)) {
        for (int row = 0; row < 2; ++row)
            for (int i = 0; i < 2; ++i) {
                map[row * MZM_MINIMAP_SIZE + i] += 0x20;
                if (visited) visited[row * MZM_MINIMAP_SIZE + i] += 0x20;
            }
        return true;
    }
    return false;
}

/* PauseScreenGetMinimapData */
static void get_minimap_data(MzmHud *h, uint8_t area, uint16_t *dst, const MzmHudFrame *f) {
    memset(dst, 0, sizeof h->minimap_data);
    uint32_t pointers = sym("sMinimapDataPointers");
    uint32_t data = mzm_rom_u32(pointers + 4u * area);
    uint32_t available;
    const uint8_t *src = mzm_rom_span(data, &available);
    uint32_t size = gba_uncompressed_size(src, available);
    if (!src || size > sizeof h->minimap_data) return;
    /* CallLZ77UncompWram */
    uint8_t bytes[sizeof h->minimap_data];
    if (gba_lz77_uncompress(src, available, bytes, sizeof bytes) < 0) return;
    for (uint32_t i = 0; i < size / 2; ++i) dst[i] = (uint16_t)(bytes[2 * i] | bytes[2 * i + 1] << 8);
    set_tiles_with_obtained_items(h, area, dst);
    const uint8_t *icons = mzm_rom_data("sBossIcons", NULL);
    if (!icons || area >= AREAS_NORMAL) return;
    const uint8_t *icon = icons + area * 5;
    if (icon[0] && f && f->check_event && f->check_event(f->context, icon[0]))
        update_boss_icon(area, &dst[icon[2] + icon[3] * MZM_MINIMAP_SIZE], NULL);
    /* The Varia statue's tile once the suit was skipped
     * (EVENT_SKIPPED_VARIA_SUIT). */
    if (area == 0 && f && f->check_event && f->check_event(f->context, 68))
        dst[MZM_MINIMAP_SIZE * 2 + 14]++;
}

/* MinimapSetDownloadedTiles */
static void set_downloaded_tiles(MzmHud *h, uint8_t area, uint16_t *dst) {
    bool downloaded = (h->progress->downloaded_maps >> area & 1) || (uint8_t)(area - MZM_HUD_AREAS) <= 2;
    for (int i = 0; i < MZM_MINIMAP_SIZE; ++i) {
        uint32_t row = area < MZM_HUD_AREAS ? h->progress->visited[area][i] : 0;
        for (int j = 0; j < MZM_MINIMAP_SIZE; ++j, ++dst) {
            if (row & 1u << j) {
                if (!(*dst & 0xF000)) *dst |= 0x1000;
            } else if (downloaded) {
                if (*dst >= 0x3000) *dst = MINIMAP_TILE_BACKGROUND;
                else if (*dst & 0x3000) *dst &= 0xFFF;
            } else if (*dst & 0xF000) {
                *dst = MINIMAP_TILE_BACKGROUND;
            }
        }
    }
}

void mzm_hud_area_map(MzmHud *h, uint8_t area, uint16_t *dst,
                      bool (*check_event)(void *context, uint8_t event), void *context) {
    MzmHudFrame f = {0};
    f.check_event = check_event;
    f.context = context;
    get_minimap_data(h, area, dst, &f);
    set_downloaded_tiles(h, area, dst);
}

void mzm_hud_room_entered(MzmHud *h, const MzmHudRoom *r, const MzmHudFrame *f) {
    if (!h->ready) return;
    /* MinimapCheckOnTransition */
    if (h->area_before_transition != r->area) {
        check_area_name_explored(h, false);
        h->area_before_transition = r->area;
        get_minimap_data(h, r->area, h->minimap_data, f);
        memcpy(h->minimap_visited, h->minimap_data, sizeof h->minimap_visited);
        check_area_name_explored(h, true);
        set_downloaded_tiles(h, r->area, h->minimap_visited);
        h->minimap_x = h->minimap_y = 0xFF;
    }
    h->update_minimap = MINIMAP_UPDATE_NONE;
    check_unexplored(h, r, f);
    if (h->update_minimap == MINIMAP_UPDATE_LOWER) {
        set_tile_explored(h, r);
        update_explored_tiles(h, r);
    }
    for (uint8_t flag = MINIMAP_UPDATE_UPPER; flag <= MINIMAP_UPDATE_LOWER; ++flag) {
        h->update_minimap = flag;
        minimap_draw(h);
    }
}

void mzm_hud_update(MzmHud *h, const MzmHudRoom *r, const MzmHudFrame *f) {
    if (!h->ready) return;
    /* MinimapUpdate (RoomUpdateGfxInfo) */
    check_unexplored(h, r, f);
    if (h->update_minimap == MINIMAP_UPDATE_LOWER) {
        set_tile_explored(h, r);
        update_explored_tiles(h, r);
    }
    minimap_draw(h);
    /* HudDraw */
    draw_energy(h, f);
    Ammo m = missiles(h, f), p = power_bombs(h, f), s = super_missiles(h, f);
    draw_ammo(h, &m, false);
    draw_ammo(h, &p, false);
    draw_ammo(h, &s, false);
    update_gfx(h, f);
    if (f->equipment->suit == MZM_SUIT_SUITLESS) draw_suitless(h, f);
    update_oam(h, f);
}

void mzm_hud_collected_item(MzmHud *h, const MzmHudRoom *r, int x_block, int y_block) {
    if (!h->ready || r->area >= MZM_HUD_AREAS) return;
    uint32_t x = (uint32_t)((x_block - SCREEN_PADDING) / SCREEN_X_BLOCKS + r->map_x);
    uint32_t y = (uint32_t)((y_block - SCREEN_PADDING) / SCREEN_Y_BLOCKS + r->map_y);
    if (x >= MZM_MINIMAP_SIZE || y >= MZM_MINIMAP_SIZE) return;
    h->progress->obtained[r->area][y] |= 1u << x;
    h->minimap_visited[x + y * MZM_MINIMAP_SIZE]++;
    h->minimap_data[x + y * MZM_MINIMAP_SIZE]++;
    h->update_minimap = MINIMAP_UPDATE_LOWER;
    minimap_draw(h);
}

bool mzm_hud_tile_explored(const MzmHud *h, const MzmHudRoom *r, int x_block, int y_block) {
    if (!h->ready || r->area >= AREAS_NORMAL) return true;
    uint32_t x = (uint32_t)((x_block - SCREEN_PADDING) / SCREEN_X_BLOCKS + r->map_x);
    uint32_t y = (uint32_t)((y_block - SCREEN_PADDING) / SCREEN_Y_BLOCKS + r->map_y);
    if (x >= MZM_MINIMAP_SIZE || y >= MZM_MINIMAP_SIZE) return false;
    return h->progress->visited[r->area][y] >> x & 1;
}

void mzm_hud_update_chunk(MzmHud *h, const MzmHudRoom *r, uint8_t event) {
    const uint8_t *icons = mzm_rom_data("sBossIcons", NULL);
    if (!h->ready || !icons) return;
    int i;
    for (i = 0; i < AREAS_NORMAL; ++i)
        if (event == icons[i * 5]) break;
    if (i >= AREAS_NORMAL || i != r->area) return;
    int at = icons[i * 5 + 3] * MZM_MINIMAP_SIZE + icons[i * 5 + 2];
    if (!update_boss_icon(r->area, &h->minimap_data[at], &h->minimap_visited[at])) return;
    h->update_minimap = MINIMAP_UPDATE_LOWER;
    minimap_draw(h);
}
