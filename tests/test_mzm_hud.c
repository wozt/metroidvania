/* SPDX-License-Identifier: GPL-3.0-only */
/* The GBA PPU renderer, the ROM bank and its decompression, and the Zero
 * Mission HUD and minimap (synthetic ROM data; the private bank when it was
 * exported). */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gba_ppu.h"
#include "mzm_hud.h"
#include "mzm_rom.h"

static GbaPpu ppu;

static void put_tile4(uint32_t at, uint8_t color) {
    for (int i = 0; i < 32; ++i) ppu.vram[at + (uint32_t)i] = (uint8_t)(color | color << 4);
}

static uint16_t pixel(int x, int y) {
    uint16_t line[GBA_PPU_MAX_W];
    gba_ppu_render_line(&ppu, y, GBA_SCREEN_W, line);
    return line[x];
}

static void test_ppu(void) {
    gba_ppu_reset(&ppu);
    /* A text BG: tile 1 (color 2 of palette row 1) at map cell (1, 0). */
    put_tile4(32, 2);
    ppu.palette[16 + 2] = 0x001F;
    ppu.palette[0] = 0x7C00;
    gba_ppu_write16(&ppu, GBA_REG_BG0CNT, 8 << 8);       /* screen block 8 */
    ppu.vram[0x4000 + 2] = 1;
    ppu.vram[0x4000 + 3] = 0x10;                          /* palette row 1 */
    gba_ppu_write16(&ppu, GBA_REG_DISPCNT, GBA_DCNT_BG0);
    assert(pixel(8, 0) == 0x001F && pixel(0, 0) == (0x7C00 | 0x8000));
    /* Scrolling moves the map. */
    gba_ppu_write16(&ppu, GBA_REG_BG0HOFS, 8);
    assert(pixel(0, 0) == 0x001F && pixel(8, 0) == (0x7C00 | 0x8000));
    gba_ppu_write16(&ppu, GBA_REG_BG0HOFS, 0);
    /* An 8x8 object over it (priority 0, palette row 2 color 3). */
    put_tile4(GBA_OBJ_VRAM + 5 * 32, 3);
    ppu.palette[256 + 32 + 3] = 0x03E0;
    ppu.oam[0] = 0;                                      /* y 0 */
    ppu.oam[1] = 10;                                     /* x 10 */
    ppu.oam[2] = 5 | 2 << 12;
    for (int i = 1; i < 128; ++i) ppu.oam[i * 4] = 0x200;
    gba_ppu_write16(&ppu, GBA_REG_DISPCNT, GBA_DCNT_BG0 | GBA_DCNT_OBJ);
    assert(pixel(10, 0) == 0x03E0 && pixel(9, 0) == 0x001F && pixel(18, 0) == (0x7C00 | 0x8000));
    /* A lower priority object goes behind the background. */
    ppu.oam[2] = 5 | 2 << 12 | 1 << 10;
    gba_ppu_write16(&ppu, GBA_REG_BG0CNT, 8 << 8);
    assert(pixel(10, 0) == 0x001F && pixel(16, 0) == 0x03E0);
    ppu.oam[2] = 5 | 2 << 12;
    /* Alpha blending: the object over BG0 at 8/16 each. */
    gba_ppu_write16(&ppu, GBA_REG_BLDCNT, 1 << 4 | 1 << 6 | 1 << 8);
    gba_ppu_write16(&ppu, GBA_REG_BLDALPHA, 8 | 8 << 8);
    assert(pixel(10, 0) == ((0x1F / 2) | (0x1F / 2) << 5));
    /* Brightness down on BG0 only. */
    gba_ppu_write16(&ppu, GBA_REG_BLDCNT, 1 << 0 | 3 << 6);
    gba_ppu_write16(&ppu, GBA_REG_BLDY, 16);
    assert(pixel(9, 0) == 0 && pixel(10, 0) == 0x03E0);
    gba_ppu_write16(&ppu, GBA_REG_BLDCNT, 0);
    /* Window 0 over x 0-11 shows only the objects; outside, everything. */
    gba_ppu_write16(&ppu, GBA_REG_DISPCNT, GBA_DCNT_BG0 | GBA_DCNT_OBJ | GBA_DCNT_WIN0);
    gba_ppu_write16(&ppu, GBA_REG_WIN0H, 0 << 8 | 12);
    gba_ppu_write16(&ppu, GBA_REG_WIN0V, 0 << 8 | 160);
    gba_ppu_write16(&ppu, GBA_REG_WININ, 0x10);
    gba_ppu_write16(&ppu, GBA_REG_WINOUT, 0x3F);
    assert(pixel(9, 0) == (0x7C00 | 0x8000) && pixel(10, 0) == 0x03E0 && pixel(12, 0) == 0x03E0);
    /* A horizontally flipped 16x8 object: the half with the colored tile
     * moves to the left. */
    gba_ppu_write16(&ppu, GBA_REG_DISPCNT, GBA_DCNT_OBJ);
    memset(ppu.vram + GBA_OBJ_VRAM + 5 * 32, 0, 64);
    put_tile4(GBA_OBJ_VRAM + 6 * 32, 3);
    ppu.oam[0] = 1 << 14;                                /* horizontal */
    ppu.oam[1] = 40 | 0 << 14;                           /* 16x8 */
    assert(pixel(40, 0) & 0x8000 && pixel(48, 0) == 0x03E0);
    ppu.oam[1] = 40 | 1 << 12;
    assert(pixel(40, 0) == 0x03E0 && pixel(48, 0) & 0x8000);
    /* Negative coordinates wrap: y 250 shows the object's last rows. */
    ppu.oam[0] = 250 | 1 << 14;
    ppu.oam[1] = 40;
    assert(pixel(48, 1) == 0x03E0 && pixel(48, 2) & 0x8000);
    /* Forced blank is white. */
    gba_ppu_write16(&ppu, GBA_REG_DISPCNT, GBA_DCNT_BLANK);
    assert(pixel(0, 0) == 0x7FFF);
    uint32_t argb[GBA_SCREEN_W];
    gba_ppu_write16(&ppu, GBA_REG_DISPCNT, 0);
    gba_ppu_render(&ppu, GBA_SCREEN_W, 1, true, argb, (int)sizeof argb);
    assert(argb[0] == 0);
    assert(gba_color_argb(0x7FFF) == 0xFFFFFFFFu && gba_color_argb(0x001F) == 0xFFFF0000u);
}

static void test_decompression(void) {
    /* "ABCABCABCX": three literals, a back reference of 6 at distance 3,
     * one literal. */
    static const uint8_t lz[] = {0x10, 10, 0, 0, 0x10, 'A', 'B', 'C', 0x30, 0x02, 'X'};
    uint8_t out[16];
    assert(gba_uncompressed_size(lz, sizeof lz) == 10);
    assert(gba_lz77_uncompress(lz, sizeof lz, out, sizeof out) == 10 && !memcmp(out, "ABCABCABCX", 10));
    assert(gba_lz77_uncompress(lz, 6, out, sizeof out) == -1);
    assert(gba_lz77_uncompress(lz, sizeof lz, out, 4) == -1);
    static const uint8_t rl[] = {0x30, 7, 0, 0, 0x81, 'Z', 0x02, 'a', 'b', 'c'};
    assert(gba_rl_uncompress(rl, sizeof rl, out, sizeof out) == 7 && !memcmp(out, "ZZZZabc", 7));
}

/* A synthetic ROM: every symbol the HUD reads, each filled with its index
 * plus the byte offset, at 0x08000000. */
static const struct { const char *name; uint32_t size; } hud_symbols[] = {
    {"sCommonSpritesGfx", 3584 * 4}, {"sCommonSpritesPal", 192},
    {"sMissileDigitsGfx", 2304}, {"sPowerBombDigitsGfx", 2304},
    {"sMissileHudGfx_Inactive", 6 * 64}, {"sSuperMissileHudGfx_Inactive", 6 * 64},
    {"sPowerBombHudGfx_Inactive", 6 * 64}, {"sEnergyDigitsTensGfx", 320},
    {"sEnergyDigitsOnesGfx", 320}, {"sEnergyDigitsRefill1Gfx", 640},
    {"sEnergyDigitsRefill2Gfx", 640}, {"sEnergyDigitsRefill3Gfx", 640},
    {"sEnergyTanksGfx_Zero", 160}, {"sEnergyTanksGfx_One", 320}, {"sEnergyTanksGfx_Two", 480},
    {"sEnergyTanksRefillGfx_One", 480}, {"sEnergyTanksRefillGfx_Two", 480},
    {"sSuitlessHudChargeBarGfx", 56 * 256}, {"sMinimapTilesGfx", 0x3000},
    {"sPauseScreen_40d74c", 80}, {"sPauseScreen_40d6fc", 80}, {"sMinimapDataPointers", 32},
    {"sBossIcons", 35}, {"sMapChunksToUpdate", 6}, {"sMinimapAreaNames", 24},
    {"sAreaRoomEntryPointers", 28}, {"sBrinstarRoomEntries", 120}, {"sMinimapBrinstar", 0x980},
};

static uint32_t synthetic_rom(uint8_t **bytes) {
    uint32_t size = 0;
    for (size_t i = 0; i < sizeof hud_symbols / sizeof *hud_symbols; ++i) size += hud_symbols[i].size;
    *bytes = calloc(size, 1);
    uint32_t at = 0;
    for (size_t i = 0; i < sizeof hud_symbols / sizeof *hud_symbols; ++i) {
        for (uint32_t j = 0; j < hud_symbols[i].size; ++j) (*bytes)[at + j] = (uint8_t)(i * 16 + j);
        at += hud_symbols[i].size;
    }
    return size;
}

static uint8_t *symbol_bytes(uint8_t *rom, const char *name) {
    return rom + (mzm_rom_symbol(name, NULL) - 0x08000000u);
}

static void test_hud_synthetic(void) {
    uint8_t *rom;
    uint32_t size = synthetic_rom(&rom);
    uint32_t at = 0x08000000;
    /* The symbols first, to place the patched tables. */
    assert(mzm_rom_open_memory(0x08000000, rom, size));
    for (size_t i = 0; i < sizeof hud_symbols / sizeof *hud_symbols; ++i) {
        assert(mzm_rom_add_symbol(hud_symbols[i].name, at, hud_symbols[i].size));
        at += hud_symbols[i].size;
    }
    uint32_t data = mzm_rom_symbol("sMinimapBrinstar", NULL);
    /* sMinimapDataPointers[0]: an LZ77 map of 0x800 bytes, every tile 0x0010
     * except (5, 3) = 0x3011 (a palette 3 room tile). */
    uint8_t *map = symbol_bytes(rom, "sMinimapBrinstar");
    {
        static uint8_t raw[0x800];
        for (int i = 0; i < 0x400; ++i) raw[2 * i] = 0x10, raw[2 * i + 1] = 0;
        raw[2 * (3 * 32 + 5)] = 0x11;
        raw[2 * (3 * 32 + 5) + 1] = 0x30;
        /* Stored without back references. */
        size_t out = 0;
        map[out++] = 0x10; map[out++] = 0x00; map[out++] = 0x08; map[out++] = 0;
        for (int i = 0; i < 0x800; i += 8) {
            map[out++] = 0;
            for (int k = 0; k < 8; ++k) map[out++] = raw[i + k];
        }
        assert(out <= 0x980);
    }
    uint8_t *pointers = symbol_bytes(rom, "sMinimapDataPointers");
    memcpy(pointers, &data, 4);
    uint32_t entries = mzm_rom_symbol("sBrinstarRoomEntries", NULL);
    memcpy(symbol_bytes(rom, "sAreaRoomEntryPointers"), &entries, 4);
    uint8_t *entry = symbol_bytes(rom, "sBrinstarRoomEntries");
    entry[60 + 53] = 4;                                 /* room 1 at map (4, 2) */
    entry[60 + 54] = 2;
    memset(symbol_bytes(rom, "sBossIcons"), 0, 35);
    memset(symbol_bytes(rom, "sMinimapAreaNames"), 0xFF, 24);
    /* Reload with the patched bytes (the symbols stay registered). */
    {
        uint8_t *copy = malloc(size);
        memcpy(copy, rom, size);
        assert(mzm_rom_open_memory(0x08000000, copy, size));
        at = 0x08000000;
        for (size_t i = 0; i < sizeof hud_symbols / sizeof *hud_symbols; ++i) {
            assert(mzm_rom_add_symbol(hud_symbols[i].name, at, hud_symbols[i].size));
            at += hud_symbols[i].size;
        }
        free(copy);
    }
    assert(mzm_rom_u32(mzm_rom_symbol("sMinimapDataPointers", NULL)) == data);
    uint8_t map_x, map_y;
    assert(mzm_hud_room_map(0, 1, &map_x, &map_y) && map_x == 4 && map_y == 2);
    assert(!mzm_hud_room_map(9, 1, &map_x, &map_y));
    assert(mzm_hud_area_index("Crateria") == 5 && mzm_hud_area_index("Moon") == 0xFF);

    static MzmMinimapProgress progress;
    static MzmHud hud;
    gba_ppu_reset(&ppu);
    mzm_hud_init(&hud, &ppu, &progress, 240);
    assert(hud.ready && ppu.io[0] == GBA_DCNT_OBJ);
    /* HudGenericLoadCommonSpriteGfx and sCommonSpritesPal at OBJ row 2. */
    assert(!memcmp(ppu.vram + 0x10800, symbol_bytes(rom, "sCommonSpritesGfx"), 64));
    const uint8_t *pal = symbol_bytes(rom, "sCommonSpritesPal");
    assert(ppu.palette[0x120] == (uint16_t)(pal[0] | pal[1] << 8));

    MzmEquipment equipment = {.energy = 199, .max_energy = 299, .missiles = 5, .max_missiles = 10};
    uint8_t refill[4] = {0};
    MzmHudFrame frame = {&equipment, 0, false, 0, 0, 0, 0, refill, NULL, NULL};
    /* Samus at block (40, 24) of room 1: minimap (4 + 2, 2 + 2). */
    MzmHudRoom room = {0, 1, map_x, map_y, 80, 60};
    frame.samus_x = 40 * 64;
    frame.samus_y = 24 * 64;
    mzm_hud_room_entered(&hud, &room, &frame);
    assert(hud.minimap_x == 6 && hud.minimap_y == 4 && progress.visited[0][4] == 1u << 6);
    assert(hud.minimap_data[3 * 32 + 5] == 0x3011 && hud.minimap_visited[4 * 32 + 6] == 0x1010);
    /* The room tile (5, 3), not visited and no map: replaced by the
     * background tile. */
    assert(hud.minimap_visited[3 * 32 + 5] == 0x140);
    mzm_hud_update(&hud, &room, &frame);
    /* Energy 199: tens 9 and ones 9 digits, then one full tank of two. */
    const uint8_t *tens = symbol_bytes(rom, "sEnergyDigitsTensGfx");
    const uint8_t *ones = symbol_bytes(rom, "sEnergyDigitsOnesGfx");
    assert(!memcmp(ppu.vram + 0x10A00, tens + 9 * 32, 32));
    assert(!memcmp(ppu.vram + 0x10A20, ones + 9 * 32, 32));
    assert(!memcmp(ppu.vram + 0x10A40, symbol_bytes(rom, "sEnergyTanksGfx_Two") + 1 * 160, 160));
    assert(!hud.low_health && hud.energy.hundreds == 1 && hud.max_energy.hundreds == 2);
    /* Missiles 5: the hundreds and tens use the empty digit (10), the
     * missile symbol turns Selected (missiles, not supers). */
    const uint8_t *digits = symbol_bytes(rom, "sMissileDigitsGfx");
    assert(hud.missile_highlight == 1);
    assert(ppu.vram[0x10E40] == digits[1 * 384 + 10 * 32] && ppu.vram[0x10E42] == digits[1 * 384 + 10 * 32]);
    assert(ppu.vram[0x10E60] == digits[1 * 384 + 5 * 32]);
    assert(!memcmp(ppu.vram + 0x10E00, symbol_bytes(rom, "sMissileHudGfx_Inactive") + 64, 64));
    /* OAM: health bar (2), missiles, minimap square and minimap. */
    assert(ppu.oam[0] == (2 | 1 << 14) && ppu.oam[1] == (0 | 1 << 14) && ppu.oam[2] == (0x50 | 4 << 12));
    assert(ppu.oam[2 * 4 + 1] == (54 | 1 << 14) && ppu.oam[2 * 4 + 2] == (0x70 | 4 << 12));
    assert(ppu.oam[3 * 4 + 2] == (0x140 | 5 << 12) && ppu.oam[4 * 4] == (250 | 0 << 14));
    assert(ppu.oam[4 * 4 + 1] == (214 | 2 << 14) && ppu.oam[5 * 4] == 0x200);
    /* The minimap lines reach VRAM one per frame. */
    mzm_hud_update(&hud, &room, &frame);
    mzm_hud_update(&hud, &room, &frame);
    assert(hud.update_minimap == 0);
    assert(!memcmp(ppu.vram + 0x11F80 + 2 * 1024, &hud.minimap_gfx[2 * 24], 96));
    /* Highlighting missiles makes the symbol Active. */
    frame.weapon_highlighted = MZM_WH_MISSILE;
    mzm_hud_update(&hud, &room, &frame);
    assert(hud.missile_highlight == 2 &&
           !memcmp(ppu.vram + 0x10E00, symbol_bytes(rom, "sMissileHudGfx_Inactive") + 128, 64));
    /* Low health under 30. */
    equipment.energy = 29;
    mzm_hud_update(&hud, &room, &frame);
    assert(hud.low_health);
    /* The refill animation draws its stages. */
    refill[0] = 13;
    mzm_hud_update(&hud, &room, &frame);
    assert(!memcmp(ppu.vram + 0x10A00, symbol_bytes(rom, "sEnergyDigitsRefill1Gfx") + 2 * 32, 32));
    assert(!memcmp(ppu.vram + 0x10A40, symbol_bytes(rom, "sEnergyTanksRefillGfx_Two"), 160));
    refill[0] = 0;
    /* A collected item adds one to the map tile and is remembered. */
    mzm_hud_collected_item(&hud, &room, 40, 24);
    assert(progress.obtained[0][4] == 1u << 6 && hud.minimap_data[4 * 32 + 6] == 0x0011);
    assert(mzm_hud_tile_explored(&hud, &room, 40, 24) && !mzm_hud_tile_explored(&hud, &room, 2, 2));
    /* Hidden: no OAM entry. */
    hud.hidden = true;
    mzm_hud_update(&hud, &room, &frame);
    assert(ppu.oam[0] == 0x200);
    /* Suitless: the charge bar replaces the ammunition. */
    hud.hidden = false;
    equipment.suit = MZM_SUIT_SUITLESS;
    frame.charge_counter = 10;
    mzm_hud_update(&hud, &room, &frame);
    assert(ppu.oam[2 * 4] == (10 | 1 << 14) && ppu.oam[2 * 4 + 2] == (0x88 | 4 << 12));
    assert(!memcmp(ppu.vram + 0x11100, symbol_bytes(rom, "sSuitlessHudChargeBarGfx") + 5 * 256, 256));
    free(rom);
    mzm_rom_close();
}

/* With the exported bank: Brinstar's first room draws a HUD. */
static void test_hud_bank(void) {
    if (!mzm_rom_open("assets/extracted/metroid/rom/rom_bank.bin",
                      "assets/extracted/metroid/rom/rom_symbols.tsv")) {
        printf("mzm_hud: no ROM bank, the exported checks are skipped\n");
        return;
    }
    uint8_t map_x, map_y;
    assert(mzm_hud_room_map(0, 0, &map_x, &map_y) && map_x < 32 && map_y < 32);
    static MzmMinimapProgress progress;
    static MzmHud hud;
    gba_ppu_reset(&ppu);
    mzm_hud_init(&hud, &ppu, &progress, 240);
    assert(hud.ready);
    MzmEquipment equipment = {.energy = 99, .max_energy = 99};
    MzmHudFrame frame = {&equipment, 0, false, 0, 0, 20 * 64, 10 * 64, NULL, NULL, NULL};
    MzmHudRoom room = {0, 0, map_x, map_y, 79, 44};
    mzm_hud_room_entered(&hud, &room, &frame);
    for (int i = 0; i < 4; ++i) mzm_hud_update(&hud, &room, &frame);
    int drawn = 0, minimap = 0;
    uint16_t line[GBA_PPU_MAX_W];
    for (int y = 0; y < 26; ++y) {
        gba_ppu_render_line(&ppu, y, GBA_SCREEN_W, line);
        for (int x = 0; x < 64; ++x) drawn += !(line[x] & 0x8000);
        for (int x = 214; x < 240; ++x) minimap += !(line[x] & 0x8000);
    }
    assert(drawn > 60 && minimap > 100);
    assert(progress.visited[0][hud.minimap_y] & 1u << hud.minimap_x);
    mzm_rom_close();
}

int main(void) {
    test_ppu();
    test_decompression();
    test_hud_synthetic();
    test_hud_bank();
    printf("mzm_hud: ok\n");
    return 0;
}
