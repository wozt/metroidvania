/* SPDX-License-Identifier: GPL-3.0-only */
#include "mzm_tourian_escape.h"

#include <string.h>

#include "mzm_rom.h"
#include "mzm_sfx.h"

enum {
    AREA_TOURIAN = 4, AREA_CRATERIA = 5,
    BLDCNT_BG0_FIRST = 1 << 0, BLDCNT_BG1_FIRST = 1 << 1,
    BLDCNT_BG2_FIRST = 1 << 2, BLDCNT_BG3_FIRST = 1 << 3,
    BLDCNT_OBJ_FIRST = 1 << 4, BLDCNT_BACKDROP_FIRST = 1 << 5,
    BLDCNT_BRIGHTNESS_UP = 2 << 6, BLDCNT_SCREEN_SECOND = 0x3F << 8,
};

static void dispcnt(MzmTourianEscape *e, uint16_t value) {
    gba_ppu_write16(&e->room.ppu, GBA_REG_DISPCNT, value);
}

static bool load_room(MzmTourianEscape *e, MzmScreen *s, uint8_t area, uint8_t room,
                      int x, int y) {
    if (!mzm_room_gfx_load(&e->room, area, room, false, false, false)) return false;
    s->ppu = &e->room.ppu;
    for (int bg = 0; bg < 4; ++bg) mzm_room_gfx_scroll(&e->room, bg, x, y);
    dispcnt(e, GBA_DCNT_BG1 | GBA_DCNT_BG2 | GBA_DCNT_BG3 | GBA_DCNT_OBJ);
    return true;
}

static bool decompress(const char *name, uint8_t *dst, size_t size) {
    uint32_t address = mzm_sym(name);
    return address && mzm_lz77_from_rom(address, dst, size);
}

static void copy_palette(MzmTourianEscape *e, const char *name, int colors) {
    uint32_t address = mzm_sym(name);
    for (int i = 0; address && i < colors; ++i) {
        uint16_t color = mzm_rom_u16(address + (uint32_t)i * 2u);
        e->room.ppu.palette[i] = color;
        e->room.ppu.palette[256 + i] = color;
    }
}

static void load_room_explosion_assets(MzmTourianEscape *e) {
    decompress("sMotherShipBlowingUpExplosionsGfx", e->room.ppu.vram + GBA_OBJ_VRAM,
               GBA_VRAM_SIZE - GBA_OBJ_VRAM);
    decompress("sTourianEscapeDebrisGfx", e->room.ppu.vram + 0x13000,
               GBA_VRAM_SIZE - 0x13000);
    uint32_t size = 0;
    uint32_t pal = mzm_rom_symbol("sMotherShipBlowingUpExplosionsPal", &size);
    unsigned colors = size / 2u;
    if (colors > 256) colors = 256;
    for (unsigned i = 0; pal && i < colors; ++i)
        e->room.ppu.palette[256 + i] = mzm_rom_u16(pal + i * 2u);
}

static void place_oam(MzmScreen *s, int *slot, const char *name, int x, int y) {
    uint32_t size;
    const uint8_t *data = mzm_rom_data(name, &size);
    if (!data || size < 2) return;
    unsigned count = (unsigned)(data[0] | data[1] << 8) & 0xFFu;
    if (2u + count * 6u > size) return;
    for (unsigned i = 0; i < count && *slot < MZM_OAM_BUFFER; ++i, ++*slot) {
        const uint8_t *part = data + 2 + i * 6;
        uint16_t a0 = (uint16_t)(part[0] | part[1] << 8);
        uint16_t a1 = (uint16_t)(part[2] | part[3] << 8);
        uint16_t a2 = (uint16_t)(part[4] | part[5] << 8);
        s->oam[*slot * 4] = (uint16_t)((a0 & 0xFF00) | ((a0 + y) & 0xFF));
        s->oam[*slot * 4 + 1] = (uint16_t)((a1 & 0xFE00) | ((a1 + x) & 0x1FF));
        s->oam[*slot * 4 + 2] = a2;
        s->oam[*slot * 4 + 3] = 0;
    }
}

static void draw_planet_oam(MzmTourianEscape *e, MzmScreen *s) {
    for (int i = 0; i < MZM_OAM_BUFFER; ++i) s->oam[i * 4] = 0x200;
    int slot = 0;
    const char *ship = (e->timer & 7) < 4 ? "sTourianEscape_47a4e0" : "sTourianEscape_47a506";
    const char *explosion = (e->timer & 3) < 2 ? "sTourianEscape_47a52c" : "sTourianEscape_47a540";
    place_oam(s, &slot, ship, e->ship_x, 16);
    place_oam(s, &slot, explosion, e->explosion_x, 89);
    mzm_screen_copy_oam(s);
}

static void load_planet(MzmTourianEscape *e, MzmScreen *s) {
    GbaPpu *p = &e->room.ppu;
    decompress("sTourianEscapeZebesTileTable", p->vram + 0xF000, GBA_VRAM_SIZE - 0xF000);
    copy_palette(e, "sTourianEscapeExplodingPal", 144);
    gba_ppu_write16(p, GBA_REG_BG0CNT, 30 << 8);
    gba_ppu_write16(p, GBA_REG_BG0HOFS, 0);
    gba_ppu_write16(p, GBA_REG_BG0VOFS, 0);
    gba_ppu_write16(p, GBA_REG_BLDCNT, BLDCNT_SCREEN_SECOND);
    gba_ppu_write16(p, GBA_REG_BLDALPHA, 6 << 8 | 10);
    gba_ppu_write16(p, GBA_REG_BLDY, 0);
    dispcnt(e, GBA_DCNT_BG0 | GBA_DCNT_OBJ);
    e->planet = true;
    e->flash = false;
    e->flash_level = 0;
    e->ship_x = 48;
    e->explosion_x = 16;
    /* Frame 247 has already advanced this timer in the native handler. */
    e->explosion_timer = 1;
    s->ppu = p;
    mzm_sfx_play(MZM_SOUND_TOURIAN_ESCAPE_SAMUS_LEAVING_PLANET);
}

bool mzm_tourian_escape_enter(MzmTourianEscape *e, MzmScreen *s) {
    if (!e || !s || !mzm_rom_ready()) return false;
    memset(e, 0, sizeof *e);
    if (!load_room(e, s, AREA_TOURIAN, 4, 80, 160)) return false;
    load_room_explosion_assets(e);
    e->ready = true;
    return true;
}

void mzm_tourian_escape_update(MzmTourianEscape *e, MzmScreen *s) {
    if (!e || !e->ready || !s || e->stage != 0) return;
    uint16_t frame = e->timer++;
    switch (frame) {
    case 0:
        mzm_sfx_play(MZM_SOUND_TOURIAN_ESCAPE_FIRST_ROOM_EXPLODING);
        break;
    case 64: case 136: case 208: case 352:
        e->flash = true;
        e->flash_timer = 0;
        break;
    case 104:
        load_room(e, s, AREA_TOURIAN, 5, 32, 192);
        load_room_explosion_assets(e);
        e->flash = false;
        e->flash_level = 0;
        mzm_sfx_play(MZM_SOUND_TOURIAN_ESCAPE_OTHER_ROOMS_EXPLODING);
        break;
    case 176:
        load_room(e, s, AREA_CRATERIA, 1, 32, 256);
        load_room_explosion_assets(e);
        e->flash = false;
        e->flash_level = 0;
        mzm_sfx_play(MZM_SOUND_TOURIAN_ESCAPE_OTHER_ROOMS_EXPLODING);
        break;
    case 246:
        decompress("sTourianEscapeZebesGfx", e->room.ppu.vram, GBA_VRAM_SIZE);
        break;
    case 247:
        decompress("sTourianEscapeZebesExplodingShipAndExplosionsGfx",
                   e->room.ppu.vram + GBA_OBJ_VRAM, GBA_VRAM_SIZE - GBA_OBJ_VRAM);
        mzm_sfx_music_fade_and_queue(0, MZM_MUSIC_ESCAPING_ZEBES_CUTSCENE);
        break;
    case 248:
        load_planet(e, s);
        break;
    case 512:
        e->stage = 1;
        return;
    default:
        break;
    }
    if (!e->planet && frame < 240)
        mzm_room_gfx_animate(&e->room, false, false, false);
    if (e->flash) {
        unsigned interval = e->timer > 255 ? 7u : 2u;
        if (++e->flash_timer >= interval) {
            e->flash_timer = 0;
            if (e->flash_level < 16) e->flash_level++;
        }
        gba_ppu_write16(&e->room.ppu, GBA_REG_BLDCNT,
                        BLDCNT_BG0_FIRST | BLDCNT_BG1_FIRST |
                        BLDCNT_BG2_FIRST | BLDCNT_BG3_FIRST |
                        BLDCNT_OBJ_FIRST | BLDCNT_BACKDROP_FIRST |
                        BLDCNT_BRIGHTNESS_UP);
        gba_ppu_write16(&e->room.ppu, GBA_REG_BLDY, e->flash_level);
    }
    if (e->planet) {
        if (e->explosion_timer++ > 11) {
            e->explosion_timer = 0;
            e->explosion_x--;
            uint16_t x = gba_ppu_read16(&e->room.ppu, GBA_REG_BG0HOFS);
            gba_ppu_write16(&e->room.ppu, GBA_REG_BG0HOFS, (uint16_t)(x + 1));
        }
        draw_planet_oam(e, s);
    }
}
