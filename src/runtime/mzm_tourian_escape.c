/* SPDX-License-Identifier: GPL-3.0-only */
#include "mzm_tourian_escape.h"

#include <string.h>

#include "mzm_rom.h"
#include "mzm_sfx.h"

enum {
    AREA_TOURIAN = 4, AREA_CRATERIA = 5, OAM_BLOCK = 64, PI_2 = 64,
    BLDCNT_BG0_FIRST = 1 << 0, BLDCNT_BG1_FIRST = 1 << 1,
    BLDCNT_BG2_FIRST = 1 << 2, BLDCNT_BG3_FIRST = 1 << 3,
    BLDCNT_OBJ_FIRST = 1 << 4, BLDCNT_BACKDROP_FIRST = 1 << 5,
    BLDCNT_ALPHA = 1 << 6, BLDCNT_BRIGHTNESS_UP = 2 << 6,
    BLDCNT_BRIGHTNESS_DOWN = 3 << 6,
    BLDCNT_SCREEN_FIRST = 0x3F, BLDCNT_SCREEN_SECOND = 0x3F << 8,
    WIN_ALL = 0x3F, WIN_ALL_NO_COLOR = 0x1F,
};

static const char *const room_animation_tables[4] = {
    "sTourianEscapeOam_HugeShipExplosion", "sChozodiaEscapeOam_47cc64",
    "sChozodiaEscapeOam_47ccbc", "sTourianEscapeOam_HugeShipExplosion",
};
static const char *const room_position_tables[4] = {
    "sTourianEscape_47ced0", "sTourianEscape_47cef0",
    "sTourianEscape_47cf10", "sTourianEscape_47cf30",
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

static void copy_bg_palette(MzmTourianEscape *e, const char *name, int colors) {
    uint32_t address = mzm_sym(name);
    for (int i = 0; address && i < colors; ++i)
        e->room.ppu.palette[i] = mzm_rom_u16(address + (uint32_t)i * 2u);
}

static void copy_obj_palette(MzmTourianEscape *e, const char *name, int colors) {
    uint32_t address = mzm_sym(name);
    for (int i = 0; address && i < colors; ++i)
        e->room.ppu.palette[256 + i] = mzm_rom_u16(address + (uint32_t)i * 2u);
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

static void place_oam_address(MzmScreen *s, int *slot, uint32_t address, int x, int y,
                              unsigned priority) {
    uint32_t size = 0;
    const uint8_t *data = mzm_rom_span(address, &size);
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
        s->oam[*slot * 4 + 2] = (uint16_t)((a2 & ~(3u << 10)) | priority << 10);
        s->oam[*slot * 4 + 3] = 0;
    }
}

static void place_oam(MzmScreen *s, int *slot, const char *name, int x, int y) {
    place_oam_address(s, slot, mzm_sym(name), x, y, 0);
}

static void clear_oam_buffer(MzmScreen *s) {
    for (int i = 0; i < MZM_OAM_BUFFER; ++i) {
        s->oam[i * 4] = 0x200;
        s->oam[i * 4 + 1] = 0;
        s->oam[i * 4 + 2] = 0;
        s->oam[i * 4 + 3] = 0;
    }
    s->next_oam_slot = 0;
}

static void hide_oam(MzmScreen *s) {
    clear_oam_buffer(s);
    mzm_screen_copy_oam(s);
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

static uint32_t frame_data(const char *table, unsigned frame, uint8_t *duration) {
    uint32_t address = mzm_sym(table);
    if (!address) {
        *duration = 0;
        return 0;
    }
    address += frame * 8u;
    *duration = mzm_rom_u8(address + 4);
    return mzm_rom_u32(address);
}

static void object_position(MzmTourianEscape *e, int object, const char *table,
                            unsigned index) {
    uint32_t address = mzm_sym(table) + index * 4u;
    e->object_x[object] = mzm_rom_u16(address);
    e->object_y[object] = mzm_rom_u16(address + 2);
}

static void initialize_room_objects(MzmTourianEscape *e) {
    memset(e->object_active, 0, sizeof e->object_active);
    memset(e->object_frame, 0, sizeof e->object_frame);
    memset(e->object_timer, 0, sizeof e->object_timer);
    memset(e->object_oam_address, 0, sizeof e->object_oam_address);
    for (int i = 0; i < 4; ++i) {
        e->object_active[i] = 1;
        object_position(e, i, room_position_tables[i], 0);
    }
    e->object_frame[3] = 8;
    for (int i = 4; i < MZM_TOURIAN_ESCAPE_OBJECTS - 1; ++i) {
        e->object_active[i] = 1;
        e->object_frame[i] = (uint8_t)i;
        /* The native loop also reads index 8, immediately beyond the table's
         * declared rows; preserve that ROM-contiguous lookup. */
        object_position(e, i, "sTourianEscape_47cf50", (unsigned)i);
    }
}

static void advance_room_object(MzmTourianEscape *e, int object) {
    uint8_t duration;
    frame_data(room_animation_tables[object], e->object_frame[object], &duration);
    if (e->object_timer[object] >= duration) {
        e->object_timer[object] = 0;
        e->object_frame[object]++;
        frame_data(room_animation_tables[object], e->object_frame[object], &duration);
        if (duration == 0) {
            e->object_frame[object] = 0;
            e->object_active[object] =
                (uint8_t)(((e->object_active[object] + 1u) & 7u) + 1u);
            object_position(e, object, room_position_tables[object],
                            (unsigned)e->object_active[object] - 1u);
        }
    }
    e->object_timer[object]++;
}

static void shake_room(MzmTourianEscape *e) {
    int phase = e->timer & 7;
    for (int bg = 0; bg < 4; ++bg) {
        bool forward = (bg == 3 && phase == 3) || (bg != 3 && phase == 1);
        bool backward = (bg == 3 && phase == 6) || (bg != 3 && phase == 4);
        if (!forward && !backward) continue;
        uint32_t hreg = GBA_REG_BG0HOFS + (uint32_t)bg * 4u;
        uint32_t vreg = GBA_REG_BG0VOFS + (uint32_t)bg * 4u;
        int delta = forward ? 2 : -2;
        gba_ppu_write16(&e->room.ppu, hreg,
                        (uint16_t)(gba_ppu_read16(&e->room.ppu, hreg) + delta));
        gba_ppu_write16(&e->room.ppu, vreg,
                        (uint16_t)(gba_ppu_read16(&e->room.ppu, vreg) - delta));
    }
}

static void draw_room_oam(MzmTourianEscape *e, MzmScreen *s) {
    clear_oam_buffer(s);
    int slot = 0;
    for (int i = 0; i < 4; ++i) {
        advance_room_object(e, i);
        uint8_t duration;
        uint32_t frame = frame_data(room_animation_tables[i], e->object_frame[i], &duration);
        place_oam_address(s, &slot, frame, e->object_x[i], e->object_y[i], 0);
    }
    uint32_t debris = mzm_sym("sTourianEscape_47cec0");
    for (int i = 4; i < MZM_TOURIAN_ESCAPE_OBJECTS - 1; ++i) {
        if (e->object_timer[i] < 15) e->object_timer[i]++;
        unsigned frame = e->object_timer[i] / 4u;
        uint32_t address = mzm_rom_u32(debris + frame * 4u);
        e->object_y[i] += 4;
        place_oam_address(s, &slot, address, e->object_x[i], e->object_y[i], 0);
    }
    s->next_oam_slot = (uint8_t)slot;
    mzm_screen_copy_oam(s);
    shake_room(e);
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
    e->flash = false;
    e->flash_level = 0;
    s->ppu = p;
    mzm_sfx_play(MZM_SOUND_TOURIAN_ESCAPE_SAMUS_LEAVING_PLANET);
}

bool mzm_tourian_escape_enter(MzmTourianEscape *e, MzmScreen *s, bool varia) {
    if (!e || !s || !mzm_rom_ready()) return false;
    memset(e, 0, sizeof *e);
    e->varia = varia;
    if (!load_room(e, s, AREA_TOURIAN, 4, 80, 160)) return false;
    load_room_explosion_assets(e);
    hide_oam(s);
    e->ready = true;
    return true;
}

static void reset_stage(MzmTourianEscape *e, MzmScreen *s, uint8_t stage) {
    e->stage = stage;
    e->timer = 0;
    e->effect_phase = 0;
    e->effect_timer = 0;
    e->flash = false;
    e->flash_timer = 0;
    e->flash_level = 0;
    e->planet = false;
    memset(e->object_active, 0, sizeof e->object_active);
    memset(e->object_frame, 0, 4 * sizeof e->object_frame[0]);
    memset(e->object_timer, 0, sizeof e->object_timer);
    memset(e->object_oam_address, 0, sizeof e->object_oam_address);
    hide_oam(s);
}

static void update_zebes_exploding(MzmTourianEscape *e, MzmScreen *s) {
    uint16_t frame = e->timer++;
    switch (frame) {
    case 0:
        initialize_room_objects(e);
        mzm_sfx_play(MZM_SOUND_TOURIAN_ESCAPE_FIRST_ROOM_EXPLODING);
        break;
    case 64: case 136: case 208: case 352:
        e->flash = true;
        e->flash_timer = 0;
        break;
    case 104:
        load_room(e, s, AREA_TOURIAN, 5, 32, 192);
        load_room_explosion_assets(e);
        for (int i = 4; i < MZM_TOURIAN_ESCAPE_OBJECTS - 1; ++i) {
            e->object_timer[i] = 0;
            e->object_frame[i] = (uint8_t)((e->object_frame[i] + 1u) & 7u);
            object_position(e, i, "sTourianEscape_47cf50", e->object_frame[i]);
        }
        e->flash = false;
        e->flash_level = 0;
        mzm_sfx_play(MZM_SOUND_TOURIAN_ESCAPE_OTHER_ROOMS_EXPLODING);
        break;
    case 176:
        load_room(e, s, AREA_CRATERIA, 1, 32, 256);
        load_room_explosion_assets(e);
        for (int i = 4; i < MZM_TOURIAN_ESCAPE_OBJECTS - 1; ++i) {
            e->object_timer[i] = 0;
            e->object_frame[i] = (uint8_t)((e->object_frame[i] + 1u) & 7u);
            object_position(e, i, "sTourianEscape_47cf50", e->object_frame[i]);
        }
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
        for (int bg = 0; bg < 3; ++bg) mzm_room_gfx_scroll(&e->room, bg, 0, 0);
        e->planet = true;
        e->ship_x = 48;
        e->explosion_x = 16;
        e->explosion_timer = 0;
        mzm_sfx_music_fade_and_queue(0, MZM_MUSIC_ESCAPING_ZEBES_CUTSCENE);
        break;
    case 248:
        load_planet(e, s);
        break;
    case 512:
        gba_ppu_write16(&e->room.ppu, GBA_REG_BG0HOFS, 0);
        gba_ppu_write16(&e->room.ppu, GBA_REG_BG0VOFS, 0);
        reset_stage(e, s, 1);
        return;
    default:
        break;
    }
    if (!e->planet && e->timer < 240)
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
    } else {
        draw_room_oam(e, s);
    }
}

static void write_window(MzmTourianEscape *e) {
    gba_ppu_write16(&e->room.ppu, GBA_REG_WIN0H,
                    (uint16_t)(e->win0_left << 8 | e->win0_right));
    gba_ppu_write16(&e->room.ppu, GBA_REG_WIN0V,
                    (uint16_t)(e->win0_top << 8 | e->win0_bottom));
}

static void update_samus_in_ship_effect(MzmTourianEscape *e) {
    GbaPpu *p = &e->room.ppu;
    if (e->effect_phase == 1) {
        if (e->effect_timer++ < 40) {
            e->win0_top -= 3;
            e->win0_bottom += 3;
        } else {
            e->win0_left -= 4;
            e->win0_right += 4;
        }
        if (e->win0_top < 0) e->win0_top = 0;
        if (e->win0_bottom > GBA_SCREEN_H) e->win0_bottom = GBA_SCREEN_H;
        if (e->win0_left < 0) e->win0_left = 0;
        if (e->win0_right > GBA_SCREEN_W) e->win0_right = GBA_SCREEN_W;
        write_window(e);
        if (e->timer >= 128 && (e->timer & 1)) {
            uint16_t bldy = gba_ppu_read16(p, GBA_REG_BLDY);
            if (bldy < 16) gba_ppu_write16(p, GBA_REG_BLDY, (uint16_t)(bldy + 1));
        }
    } else if (e->effect_phase == 2) {
        if (e->timer & 1) {
            uint16_t bldy = gba_ppu_read16(p, GBA_REG_BLDY);
            if (bldy) gba_ppu_write16(p, GBA_REG_BLDY, (uint16_t)(bldy - 1));
        }
    } else if (e->effect_phase != 0 && (e->timer & 3) == 0) {
        uint16_t alpha = gba_ppu_read16(p, GBA_REG_BLDALPHA);
        unsigned eva = alpha & 31u;
        if (eva) {
            eva--;
            gba_ppu_write16(p, GBA_REG_BLDALPHA, (uint16_t)((16u - eva) << 8 | eva));
        }
    }
}

static void update_samus_in_ship(MzmTourianEscape *e, MzmScreen *s) {
    GbaPpu *p = &e->room.ppu;
    uint16_t frame = e->timer++;
    switch (frame) {
    case 0:
        decompress("sTourianEscapeSamusInHerShipSuitGfx", p->vram, GBA_VRAM_SIZE);
        break;
    case 1:
        decompress("sMotherShipExplodingFlashGfx", p->vram + 0x8000,
                   GBA_VRAM_SIZE - 0x8000);
        break;
    case 2:
        decompress("sTourianEscapeSamusInHerShipSuitTileTable", p->vram + 0x7000,
                   GBA_VRAM_SIZE - 0x7000);
        decompress("sMotherShipExplodingFlashTileTable", p->vram + 0xF000,
                   GBA_VRAM_SIZE - 0xF000);
        copy_bg_palette(e, e->varia ? "sTourianEscapeSamusInHerShipVariaSuitPal" :
                                     "sTourianEscapeSamusInHerShipPowerSuitPal", 7 * 16);
        break;
    case 3:
        gba_ppu_write16(p, GBA_REG_BG0CNT, 2 << 2 | 30 << 8);
        gba_ppu_write16(p, GBA_REG_BG0CNT + 2, 1 | 14 << 8);
        dispcnt(e, GBA_DCNT_BG1 | GBA_DCNT_OBJ);
        gba_ppu_write16(p, GBA_REG_BLDCNT, BLDCNT_SCREEN_SECOND);
        gba_ppu_write16(p, GBA_REG_BLDALPHA, 16);
        gba_ppu_write16(p, GBA_REG_BLDY, 0);
        break;
    case 56:
        dispcnt(e, GBA_DCNT_BG1 | GBA_DCNT_OBJ | GBA_DCNT_WIN0);
        gba_ppu_write16(p, GBA_REG_BLDCNT,
                        BLDCNT_SCREEN_FIRST | BLDCNT_BRIGHTNESS_UP);
        gba_ppu_write16(p, GBA_REG_BLDY, 10);
        gba_ppu_write16(p, GBA_REG_WININ, WIN_ALL);
        gba_ppu_write16(p, GBA_REG_WINOUT, WIN_ALL_NO_COLOR);
        e->win0_left = GBA_SCREEN_W / 2 - 1;
        e->win0_right = GBA_SCREEN_W / 2 + 1;
        e->win0_top = GBA_SCREEN_H / 2 - 1;
        e->win0_bottom = GBA_SCREEN_H / 2 + 1;
        e->effect_phase = 1;
        mzm_sfx_play(MZM_SOUND_TOURIAN_ESCAPE_SAMUS_REMOVING_SUIT_1);
        break;
    case 160:
        decompress("sTourianEscapeSamusInHerShipSuitlessGfx", p->vram, GBA_VRAM_SIZE);
        break;
    case 161:
        decompress("sTourianEscapeSamusInHerShipSuitlessTileTable", p->vram + 0x7000,
                   GBA_VRAM_SIZE - 0x7000);
        decompress("sTourianEscapeSamusInHerShipSuitlessEyesOpenedTileTable",
                   p->vram + 0x7800, GBA_VRAM_SIZE - 0x7800);
        copy_bg_palette(e, "sTourianEscapeSamusInHerShipSuitlessPal", 8 * 16);
        break;
    case 162:
        dispcnt(e, GBA_DCNT_BG0 | GBA_DCNT_BG1 | GBA_DCNT_OBJ);
        e->effect_phase = 2;
        mzm_sfx_play(MZM_SOUND_TOURIAN_ESCAPE_SAMUS_REMOVING_SUIT_2);
        break;
    case 200:
        gba_ppu_write16(p, GBA_REG_BLDCNT,
                        BLDCNT_BG0_FIRST | BLDCNT_ALPHA | 1 << 9);
        gba_ppu_write16(p, GBA_REG_BLDALPHA, 16);
        e->effect_phase = 3;
        mzm_sfx_play(MZM_SOUND_TOURIAN_ESCAPE_SAMUS_REMOVING_SUIT_3);
        break;
    case 352:
        gba_ppu_write16(p, GBA_REG_BG0CNT + 2, 15 << 8);
        break;
    case 372:
        dispcnt(e, 0);
        gba_ppu_write16(p, GBA_REG_BLDCNT, 0);
        gba_ppu_write16(p, GBA_REG_BLDALPHA, 16);
        gba_ppu_write16(p, GBA_REG_BLDY, 0);
        reset_stage(e, s, 2);
        return;
    default:
        break;
    }
    update_samus_in_ship_effect(e);
}

static void draw_looking_around_oam(MzmTourianEscape *e, MzmScreen *s) {
    clear_oam_buffer(s);
    int slot = 0;
    if (e->object_active[0])
        place_oam(s, &slot, "sTourianEscape_47a554",
                  e->object_x[0], e->object_y[0]);
    if (e->object_active[1])
        place_oam(s, &slot, "sTourianEscape_47a56e",
                  e->object_x[1], e->object_y[1]);
    s->next_oam_slot = (uint8_t)slot;
    mzm_screen_copy_oam(s);
}

static void update_samus_looking_around(MzmTourianEscape *e, MzmScreen *s) {
    GbaPpu *p = &e->room.ppu;
    uint16_t frame = e->timer++;
    switch (frame) {
    case 0:
        decompress("sTourianEscapeSamusSamusInHerShipLookingGfx",
                   p->vram, GBA_VRAM_SIZE);
        break;
    case 1:
        decompress("sTourianEscapeSamusSamusInHerShipLookingGfx",
                   p->vram + GBA_OBJ_VRAM, GBA_VRAM_SIZE - GBA_OBJ_VRAM);
        gba_ppu_write16(p, GBA_REG_BLDALPHA, 8 << 8 | 8);
        e->object_active[0] = 1;
        e->object_x[0] = 128;
        e->object_y[0] = 0;
        e->object_active[1] = 1;
        e->object_x[1] = 128;
        e->object_y[1] = 160;
        break;
    case 2:
        decompress("sTourianEscapeSamusSamusInHerShipLookingLeftTileTable",
                   p->vram + 0x8000, GBA_VRAM_SIZE - 0x8000);
        decompress("sTourianEscapeSamusSamusInHerShipLookingRightTileTable",
                   p->vram + 0x8800, GBA_VRAM_SIZE - 0x8800);
        copy_palette(e, "sTourianEscapeSamusLookingAroundPal", 7 * 16);
        break;
    case 3:
        gba_ppu_write16(p, GBA_REG_BG0CNT, 16 << 8);
        dispcnt(e, GBA_DCNT_BG0 | GBA_DCNT_OBJ);
        gba_ppu_write16(p, GBA_REG_BLDCNT,
                        BLDCNT_OBJ_FIRST | BLDCNT_ALPHA | 1 << 8);
        break;
    case 16:
        e->effect_phase = 1;
        break;
    case 32:
        gba_ppu_write16(p, GBA_REG_BG0CNT, 17 << 8);
        break;
    case 56:
        dispcnt(e, 0);
        gba_ppu_write16(p, GBA_REG_BLDCNT, 0);
        e->bg2_scale = 1536;
        reset_stage(e, s, 3);
        return;
    default:
        break;
    }
    if (e->effect_phase && e->object_y[0] < GBA_SCREEN_H / 2) {
        e->object_y[0] += 8;
        e->object_y[1] -= 8;
    }
    draw_looking_around_oam(e, s);
}

static void calculate_surrounded_bg2(MzmTourianEscape *e) {
    GbaPpu *p = &e->room.ppu;
    int16_t pa = (int16_t)(65536u / e->bg2_scale);
    int32_t x = (GBA_SCREEN_W / 2 << 8) - pa * (GBA_SCREEN_W / 2);
    int32_t y = (GBA_SCREEN_H / 2 << 8) - pa * (GBA_SCREEN_H / 2);
    gba_ppu_write16(p, GBA_REG_BG2PA, (uint16_t)pa);
    gba_ppu_write16(p, GBA_REG_BG2PB, 0);
    gba_ppu_write16(p, GBA_REG_BG2PC, 0);
    gba_ppu_write16(p, GBA_REG_BG2PD, (uint16_t)pa);
    gba_ppu_write32(p, GBA_REG_BG2X, (uint32_t)x);
    gba_ppu_write32(p, GBA_REG_BG2Y, (uint32_t)y);
}

static void update_samus_surrounded(MzmTourianEscape *e, MzmScreen *s) {
    GbaPpu *p = &e->room.ppu;
    uint16_t frame = e->timer++;
    switch (frame) {
    case 0:
        decompress("sTourianEscapeSamusSurroundedBackgroundGfx",
                   p->vram, GBA_VRAM_SIZE);
        break;
    case 1:
        decompress("sTourianEscape_49cb90", p->vram + 0x8000,
                   GBA_VRAM_SIZE - 0x8000);
        break;
    case 2:
        decompress("sTourianEscapeSamusSurroundedBackgroundTileTable",
                   p->vram + 0x7000, GBA_VRAM_SIZE - 0x7000);
        decompress("sTourianEscape_49fb70", p->vram + 0x7800,
                   GBA_VRAM_SIZE - 0x7800);
        copy_bg_palette(e, "sTourianEscapeSamusSurroundedPal", 16 * 16);
        gba_ppu_write16(p, GBA_REG_BG0HOFS, 0);
        gba_ppu_write16(p, GBA_REG_BG0VOFS, 0);
        gba_ppu_write16(p, GBA_REG_BLDALPHA, 16);
        break;
    case 4:
        gba_ppu_write16(p, GBA_REG_BG0CNT, 1 | 14 << 8);
        gba_ppu_write16(p, GBA_REG_BG0CNT + 2, 2 | 14 << 8);
        gba_ppu_write16(p, GBA_REG_BG0CNT + 4,
                        2 << 2 | 15 << 8 | 1 << 7 | 1 << 14);
        dispcnt(e, 1 | GBA_DCNT_BG0 | GBA_DCNT_BG2 | GBA_DCNT_OBJ);
        break;
    case 32:
        e->effect_phase = 1;
        break;
    case 112:
        dispcnt(e, 0);
        gba_ppu_write16(p, GBA_REG_BLDCNT, BLDCNT_SCREEN_SECOND);
        reset_stage(e, s, 4);
        return;
    default:
        break;
    }
    if (e->effect_phase) {
        if (e->bg2_scale > 256) e->bg2_scale -= 80;
        else e->bg2_scale = 256;
    }
    calculate_surrounded_bg2(e);
}

static uint16_t *oam_attributes(MzmScreen *s, int slot) {
    return &s->oam[slot * 4];
}

static void set_oam_x(MzmScreen *s, int slot, int x) {
    uint16_t *a = oam_attributes(s, slot);
    a[1] = (uint16_t)((a[1] & 0xFE00) | (x & 0x1FF));
}

static void set_oam_y(MzmScreen *s, int slot, int y) {
    uint16_t *a = oam_attributes(s, slot);
    a[0] = (uint16_t)((a[0] & 0xFF00) | (y & 0xFF));
}

static int16_t affine_sine(uint16_t angle) {
    return (int16_t)mzm_rom_u16(mzm_sym("sSineTable") + (uint32_t)angle * 2u);
}

static int16_t fixed_multiply(int16_t a, int16_t b) {
    return (int16_t)(((int32_t)a * b) >> 8);
}

static int16_t fixed_inverse(int16_t value) {
    return (int16_t)(65536 / value);
}

static void process_affine_oam(MzmScreen *s, int slot, int16_t x_position,
                               int16_t y_position, uint16_t rotation,
                               int16_t scaling, uint16_t matrix) {
    uint16_t *a = oam_attributes(s, slot);
    int32_t x_origin = (int16_t)((uint16_t)x_position + OAM_BLOCK);
    int32_t y_origin = (int16_t)((uint16_t)y_position + OAM_BLOCK);
    int shape = a[0] >> 14, size = a[1] >> 14;
    uint32_t radius = mzm_sym("sSpriteXHalfRadius") + (uint32_t)(shape * 4 + size);
    uint8_t x_half = mzm_rom_u8(radius);
    uint8_t y_half = mzm_rom_u8(mzm_sym("sSpriteYHalfRadius") +
                                (uint32_t)(shape * 4 + size));
    int32_t x = (int16_t)(((a[1] & 0x1FF) + x_origin) & 511);
    int32_t y = (int16_t)(((a[0] & 0xFF) + y_origin) & 255);
    int32_t local_x = (int16_t)(x - x_origin + x_half);
    int32_t local_y = (int16_t)(y - y_origin + y_half);
    x += (int16_t)(((local_x * scaling) >> 8) - local_x);
    y += (int16_t)(((local_y * scaling) >> 8) - local_y);
    int32_t u = (int16_t)(x - x_origin + x_half);
    int32_t v = (int16_t)(y - y_origin + y_half);
    int32_t cosine = affine_sine((uint16_t)(rotation + PI_2));
    int32_t sine = affine_sine(rotation);
    x = (int16_t)((u * cosine - v * sine) >> 8);
    y = (int16_t)((u * sine + v * cosine) >> 8);
    a[0] = (uint16_t)((a[0] & ~(3 << 8)) | 3 << 8);
    x -= x_half * 2;
    y -= y_half * 2;
    set_oam_x(s, slot, x + x_origin - OAM_BLOCK);
    set_oam_y(s, slot, y + y_origin - OAM_BLOCK);
    bool x_flip = (a[1] & 1 << 12) != 0;
    bool y_flip = (a[1] & 1 << 13) != 0;
    uint16_t number = (uint16_t)(matrix +
        (x_flip ? (y_flip ? 3 : 1) : (y_flip ? 2 : 0)));
    a[1] = (uint16_t)((a[1] & ~(0x1F << 9)) | (number & 0x1F) << 9);
}

static void write_affine_oam_matrices(MzmScreen *s, uint16_t rotation,
                                      int16_t scaling, int slot) {
    int16_t cosine = affine_sine((uint16_t)(rotation + PI_2));
    int16_t sine = affine_sine(rotation);
    int16_t inverse = fixed_inverse(scaling);
    int16_t negative_inverse = fixed_inverse((int16_t)-scaling);
    int16_t dy1 = fixed_multiply(cosine, inverse);
    int16_t dmy1 = fixed_multiply(sine, inverse);
    int16_t dx1 = fixed_multiply((int16_t)-sine, inverse);
    int16_t dmx1 = fixed_multiply(cosine, negative_inverse);
    int16_t dy2 = fixed_multiply(sine, negative_inverse);
    int16_t dmy2 = fixed_multiply((int16_t)-sine, negative_inverse);
    const int16_t parameters[16] = {
        dy1, dmy1, dx1, dy1, dmx1, dy2, dx1, dy1,
        dy1, dmy1, dmy2, dmx1, dmx1, dy2, dmy2, dmx1,
    };
    for (int i = 0; i < 16; ++i)
        oam_attributes(s, slot + i)[3] = (uint16_t)parameters[i];
}

static void draw_flying_in_oam(MzmTourianEscape *e, MzmScreen *s) {
    clear_oam_buffer(s);
    if (!e->object_active[0]) {
        mzm_screen_copy_oam(s);
        return;
    }
    const char *name = e->object_frame[0] == 0 ?
        "sIntroShipFlyingTowardsCameraOam_1" :
        "sIntroShipFlyingTowardsCameraOam_2";
    uint32_t source = mzm_sym(name);
    unsigned count = mzm_rom_u16(source) & 0xFFu;
    source += 2;
    for (unsigned i = 0; i < count && i < MZM_OAM_BUFFER; ++i, source += 6) {
        uint16_t *a = oam_attributes(s, (int)i);
        a[0] = mzm_rom_u16(source);
        a[1] = mzm_rom_u16(source + 2);
        a[2] = mzm_rom_u16(source + 4);
        process_affine_oam(s, (int)i, (int16_t)e->object_x[0],
                           (int16_t)e->object_y[0], 0,
                           (int16_t)e->object_scaling, 0);
    }
    s->next_oam_slot = (uint8_t)count;
    write_affine_oam_matrices(s, 0, (int16_t)e->object_scaling, 0);
    mzm_screen_copy_oam(s);
}

static void update_samus_flying_in(MzmTourianEscape *e, MzmScreen *s) {
    GbaPpu *p = &e->room.ppu;
    uint16_t frame = e->timer++;
    switch (frame) {
    case 0:
        decompress("sIntroTextAndShipFlyingInGfx", p->vram + GBA_OBJ_VRAM,
                   GBA_VRAM_SIZE - GBA_OBJ_VRAM);
        mzm_sfx_play(MZM_SOUND_TOURIAN_ESCAPE_SAMUS_FLYING_IN);
        break;
    case 1:
        decompress("sIntroSpaceBackgroundGfx", p->vram, GBA_VRAM_SIZE);
        break;
    case 2:
        decompress("sIntroSpaceBackgroundTileTable", p->vram + 0xF000,
                   GBA_VRAM_SIZE - 0xF000);
        copy_palette(e, "sIntroTextAndShipPal", 7 * 16);
        gba_ppu_write16(p, GBA_REG_BG0CNT, 30 << 8);
        dispcnt(e, GBA_DCNT_BG0 | GBA_DCNT_OBJ);
        gba_ppu_write16(p, GBA_REG_BLDALPHA, 7 << 8 | 9);
        e->object_active[0] = 1;
        e->object_scaling = 32;
        e->object_x[0] = GBA_SCREEN_W / 2;
        e->object_y[0] = GBA_SCREEN_H / 8 + 8;
        e->object_timer[0] = 0;
        break;
    case 32:
        dispcnt(e, 0);
        gba_ppu_write16(p, GBA_REG_BLDCNT, 0);
        reset_stage(e, s, 5);
        return;
    default:
        break;
    }
    if (e->object_active[0]) {
        if (e->object_timer[0] < 4) e->object_y[0] += 8;
        else if (e->object_timer[0] < 8) e->object_y[0] += 6;
        else if (e->object_timer[0] < 16) e->object_y[0] += 3;
        else if (e->object_timer[0] < 20) e->object_y[0] += 1;
        else if (e->object_timer[0] < 28) e->object_y[0] -= 1;
        if (e->object_scaling < 128) e->object_scaling += 8;
        else if (e->object_scaling < 320) e->object_scaling += 16;
        else if (e->object_scaling < 480) e->object_scaling += 32;
        e->object_frame[0] = (uint8_t)((e->object_timer[0] & 7) < 4 ? 0 : 1);
        e->object_timer[0]++;
    }
    draw_flying_in_oam(e, s);
}

static void chased_particle_position(MzmTourianEscape *e, int object) {
    uint32_t address = mzm_sym("sTourianEscape_47ce20") +
                       (uint32_t)e->object_frame[object] * 6u;
    e->object_x[object] = mzm_rom_u16(address);
    e->object_y[object] = mzm_rom_u16(address + 2);
    e->object_velocity[object] = (int16_t)mzm_rom_u16(address + 4);
}

static void initialize_chased_objects(MzmTourianEscape *e) {
    static const uint16_t x[4] = { 0xDC, 0x100, 0xB4, 0xD4 };
    static const uint16_t y[4] = { 0xDC, 0xF0, 0xF0, 0xBE };
    static const uint8_t active[6] = { 1, 2, 1, 1, 2, 2 };
    static const uint8_t frame[6] = { 0, 6, 14, 9, 3, 11 };
    for (int i = 0; i < 4; ++i) {
        e->object_active[i] = 0;
        e->object_frame[i] = 0;
        e->object_timer[i] = 0;
        e->object_x[i] = x[i];
        e->object_y[i] = y[i];
    }
    for (int i = 4; i < MZM_TOURIAN_ESCAPE_OBJECTS; ++i) {
        e->object_active[i] = active[i - 4];
        e->object_frame[i] = frame[i - 4];
        e->object_timer[i] = 0;
        chased_particle_position(e, i);
    }
    e->bg0_x = 0;
}

static uint32_t chased_particle_oam(const MzmTourianEscape *e, int object) {
    const char *table = (e->object_active[object] & 1) ?
        "sTourianEscape_47ce00" : "sTourianEscape_47ce10";
    return mzm_rom_u32(mzm_sym(table) +
                       (uint32_t)(e->object_timer[object] / 4u) * 4u);
}

static void advance_chased_particles(MzmTourianEscape *e, bool vertical) {
    for (int i = 4; i < MZM_TOURIAN_ESCAPE_OBJECTS; ++i) {
        e->object_oam_address[i] = chased_particle_oam(e, i);
        if (!vertical && e->object_x[i] < 256) {
            e->object_x[i] = (uint16_t)(e->object_x[i] + e->object_velocity[i]);
        } else if (vertical && e->object_y[i] < 252) {
            e->object_y[i] = (uint16_t)(e->object_y[i] - e->object_velocity[i]);
        } else {
            e->object_active[i] ^= 3;
            e->object_frame[i] = (uint8_t)((e->object_frame[i] + 1u) & 15u);
            chased_particle_position(e, i);
        }
        e->object_timer[i] = (uint8_t)((e->object_timer[i] + 1u) & 15u);
    }
}

static const char *chased_main_oam(const MzmTourianEscape *e, int object) {
    static const char *const first[4] = {
        "sTourianEscape_47a602", "sTourianEscape_47a622",
        "sTourianEscape_47a642", "sTourianEscape_47a662",
    };
    if (object == 0 && e->object_active[0] == 2)
        return e->object_timer[0] > 0 && e->object_timer[0] <= 4 ?
            "sTourianEscape_47a588" : "sTourianEscape_47a5ae";
    if (e->object_frame[object])
        return object == 1 ? "sTourianEscape_47a5d4" :
                             "sTourianEscape_47a5f4";
    return first[object];
}

static void draw_chased_oam(MzmTourianEscape *e, MzmScreen *s) {
    clear_oam_buffer(s);
    int slot = 0;
    for (int i = 0; i < 4; ++i) {
        if (!e->object_active[i]) continue;
        place_oam(s, &slot, chased_main_oam(e, i),
                  e->object_x[i], e->object_y[i]);
    }
    for (int i = 4; i < MZM_TOURIAN_ESCAPE_OBJECTS; ++i) {
        if (!e->object_active[i]) continue;
        place_oam_address(s, &slot, e->object_oam_address[i],
                          e->object_x[i], e->object_y[i], 0);
    }
    s->next_oam_slot = (uint8_t)slot;
    mzm_screen_copy_oam(s);
}

static void update_samus_chased(MzmTourianEscape *e, MzmScreen *s) {
    GbaPpu *p = &e->room.ppu;
    bool ended = false;
    uint16_t frame = e->timer++;
    switch (frame) {
    case 0:
        decompress("sTourianEscapeSamusChasedBackgroundGfx", p->vram,
                   GBA_VRAM_SIZE);
        break;
    case 1:
        decompress("sTourianEscapeSamusSamusChasedShipsGfx",
                   p->vram + GBA_OBJ_VRAM, GBA_VRAM_SIZE - GBA_OBJ_VRAM);
        break;
    case 2:
        decompress("sTourianEscapeSamusChasedBackgroundTileTable",
                   p->vram + 0xE000, GBA_VRAM_SIZE - 0xE000);
        copy_bg_palette(e, "sTourianEscapeSamusChasedBackgroundPal", 5 * 16);
        copy_obj_palette(e, "sTourianEscapeSamusChasedShipsPal", 6 * 16);
        initialize_chased_objects(e);
        gba_ppu_write16(p, GBA_REG_BG0HOFS, 0);
        gba_ppu_write16(p, GBA_REG_BG0VOFS, 0);
        break;
    case 3:
        gba_ppu_write16(p, GBA_REG_BG0CNT, 28 << 8);
        dispcnt(e, GBA_DCNT_BG0 | GBA_DCNT_OBJ);
        break;
    case 40:
        e->object_active[0] = 1;
        mzm_sfx_play(MZM_SOUND_TOURIAN_ESCAPE_SAMUS_FLEEING_RIGHT_TO_LEFT);
        break;
    case 64:
        e->object_active[1] = 1;
        e->object_active[2] = 1;
        e->object_active[3] = 1;
        mzm_sfx_play(MZM_SOUND_TOURIAN_ESCAPE_SAMUS_FLEEING_LEFT_TO_RIGHT);
        break;
    case 72:
        e->object_active[0] = 0;
        e->object_x[0] = 7 * 64 + 16;
        e->object_y[0] = 3 * 64 + 32;
        break;
    case 116:
        e->object_active[1] = 0;
        e->object_frame[1] = 1;
        e->object_x[1] = 7 * 64 - 16;
        e->object_y[1] = 3 * 64 + 32;
        e->object_active[2] = 0;
        e->object_frame[2] = 1;
        e->object_x[2] = 8 * 64 - 16;
        e->object_y[2] = 3 * 64 + 32;
        break;
    case 119:
        e->object_active[3] = 0;
        e->object_frame[3] = 1;
        e->object_x[3] = 7 * 64 - 16;
        e->object_y[3] = 3 * 64 + 32;
        break;
    case 160:
        e->object_active[0] = 2;
        mzm_sfx_play(MZM_SOUND_TOURIAN_ESCAPE_PIRATES_CHASING_RIGHT_TO_LEFT);
        break;
    case 176:
        e->object_active[1] = 2;
        mzm_sfx_play(MZM_SOUND_TOURIAN_ESCAPE_PIRATES_CHASING_LEFT_TO_RIGHT);
        break;
    case 177:
        e->object_active[0] = 0;
        break;
    case 182:
        e->object_active[2] = 2;
        break;
    case 184:
        e->object_active[3] = 2;
        break;
    case 192:
        e->object_active[1] = 0;
        break;
    case 198:
        e->object_active[2] = 0;
        break;
    case 200:
        e->object_active[3] = 0;
        break;
    case 256:
        dispcnt(e, 0);
        ended = true;
        break;
    default:
        break;
    }
    if ((e->timer & 7) == 0) e->bg0_x--;
    gba_ppu_write16(p, GBA_REG_BG0HOFS, (uint16_t)e->bg0_x & 0x1FF);
    for (int i = 0; i < 4; ++i) {
        if (e->object_active[i] == 1) {
            e->object_x[i] -= 6;
            e->object_y[i] += 6;
        } else if (e->object_active[i] == 2) {
            e->object_x[i] += 14;
            e->object_y[i] -= 14;
        }
    }
    if (e->object_active[4]) advance_chased_particles(e, false);
    if (e->object_active[0] == 2)
        e->object_timer[0] = (uint8_t)((e->object_timer[0] + 1u) & 7u);
    draw_chased_oam(e, s);
    if (ended) reset_stage(e, s, 6);
}

static int16_t firing_table_value(const char *name, uint8_t index) {
    return (int16_t)mzm_rom_u16(mzm_sym(name) + (uint32_t)index * 2u);
}

static void load_firing_trajectory(MzmTourianEscape *e) {
    uint8_t index = e->object_frame[3];
    e->object_velocity[2] = firing_table_value("sTourianEscape_47cfa4", index);
    e->firing_vertical_velocity =
        firing_table_value("sTourianEscape_47cfb4", index);
    e->object_velocity[3] = firing_table_value("sTourianEscape_47cfc4", index);
    e->firing_vertical_acceleration =
        firing_table_value("sTourianEscape_47cfd4", index);
}

static uint32_t firing_oam(const char *table, unsigned frame) {
    return mzm_rom_u32(mzm_sym(table) + frame * 4u);
}

static void update_firing_samus(MzmTourianEscape *e) {
    if (!e->object_active[0]) return;
    if (e->object_active[0] == 1) {
        int velocity = e->object_velocity[2];
        if (velocity > 48) velocity = 48;
        if (velocity < -48) velocity = -48;
        e->object_x[2] = (uint16_t)(e->object_x[2] + velocity);
        e->object_x[0] = e->object_x[2] >> 3;

        velocity = e->firing_vertical_velocity;
        if (velocity > 24) velocity = 24;
        if (velocity < -24) velocity = -24;
        e->object_y[2] = (uint16_t)(e->object_y[2] + velocity);
        e->object_y[0] = e->object_y[2] >> 3;

        e->object_velocity[2] += e->object_velocity[3];
        e->firing_vertical_velocity += e->firing_vertical_acceleration;
        unsigned change = 0;
        switch (e->object_frame[2]) {
        case 0:
            if (e->object_velocity[2] >= 0) change = 1;
            break;
        case 1:
        case 3:
            if (e->object_timer[2]++ >= 20) {
                e->object_timer[2] = 0;
                change = 2;
            }
            break;
        case 2:
            if (e->object_velocity[2] <= 0) change = 1;
            break;
        default:
            break;
        }
        if (change == 1) {
            e->object_frame[2]++;
            e->object_velocity[3] = 0;
            e->firing_vertical_acceleration = 0;
        } else if (change == 2) {
            e->object_frame[2] = (uint8_t)((e->object_frame[2] + 1u) & 3u);
            e->object_frame[3] = (uint8_t)((e->object_frame[3] + 1u) & 7u);
            load_firing_trajectory(e);
        }
        if (e->object_timer[0] & 1) e->object_scaling++;
    }

    if (e->object_active[0] < 3) {
        if (e->object_frame[0] > 14) e->object_frame[0] = 0;
    } else {
        if (e->object_frame[0] >= 23) e->object_frame[0] = 23;
        if (e->object_timer[0] < 4) e->object_y[0] += 4;
        else if (e->object_timer[0] < 8) e->object_y[0] += 2;
        else e->object_y[0] += 1;
        if (e->object_scaling < 0x1F8) e->object_scaling += 8;
    }
    e->object_oam_address[0] = firing_oam(
        "sTourianEscape_47cf70", e->object_frame[0] / 4u);
    e->object_frame[0]++;
    e->object_timer[0]++;
}

static void update_firing_pirates(MzmTourianEscape *e) {
    if (!e->object_active[1]) return;
    unsigned frame = 0;
    if (e->secondary_scaling > 160) {
        if (e->object_frame[1] > 26) e->object_frame[1] = 0;
        frame = e->object_frame[1] / 4u;
    }
    e->object_oam_address[1] = firing_oam("sTourianEscape_47cf88", frame);
    e->secondary_scaling -= e->object_active[1] == 1 ? 1 : 2;
    if (e->secondary_scaling < 8) e->object_active[1] = 0;
    e->object_frame[1]++;
    e->object_timer[1]++;
}

static void place_firing_affine(MzmScreen *s, int *slot, uint32_t address,
                                int x, int y, int16_t scaling,
                                uint16_t matrix) {
    uint32_t size = 0;
    const uint8_t *data = mzm_rom_span(address, &size);
    if (!data || size < 2) return;
    unsigned count = (unsigned)(data[0] | data[1] << 8) & 0xFFu;
    if (2u + count * 6u > size) return;
    for (unsigned i = 0; i < count && *slot < MZM_OAM_BUFFER; ++i, ++*slot) {
        const uint8_t *part = data + 2 + i * 6;
        uint16_t *a = oam_attributes(s, *slot);
        a[0] = (uint16_t)(part[0] | part[1] << 8);
        a[1] = (uint16_t)(part[2] | part[3] << 8);
        a[2] = (uint16_t)(part[4] | part[5] << 8);
        a[3] = 0;
        process_affine_oam(s, *slot, (int16_t)x, (int16_t)y,
                           0, scaling, matrix);
    }
}

static void draw_firing_oam(MzmTourianEscape *e, MzmScreen *s) {
    clear_oam_buffer(s);
    int slot = 0;
    if (e->object_active[0])
        place_firing_affine(s, &slot, e->object_oam_address[0],
                            e->object_x[0], e->object_y[0],
                            (int16_t)e->object_scaling, 0);
    if (e->object_active[1])
        place_firing_affine(s, &slot, e->object_oam_address[1],
                            e->object_x[1], e->object_y[1],
                            (int16_t)e->secondary_scaling, 4);
    for (int i = 4; i < MZM_TOURIAN_ESCAPE_OBJECTS; ++i) {
        if (!e->object_active[i]) continue;
        place_oam_address(s, &slot, e->object_oam_address[i],
                          e->object_x[i], e->object_y[i], 0);
    }
    if (e->object_active[0])
        write_affine_oam_matrices(s, 0, (int16_t)e->object_scaling, 0);
    if (e->object_active[1])
        write_affine_oam_matrices(s, 0, (int16_t)e->secondary_scaling, 16);
    s->next_oam_slot = (uint8_t)slot;
    mzm_screen_copy_oam(s);
}

static void update_samus_chased_firing(MzmTourianEscape *e, MzmScreen *s) {
    GbaPpu *p = &e->room.ppu;
    bool ended = false;
    uint16_t frame = e->timer++;
    switch (frame) {
    case 0:
        decompress("sTourianEscapeShipsAndProjectilesGfx",
                   p->vram + GBA_OBJ_VRAM, GBA_VRAM_SIZE - GBA_OBJ_VRAM);
        break;
    case 1:
        copy_obj_palette(e, "sTourianEscapeSamusChasedByPiratesFiringPal", 4 * 16);
        e->object_active[0] = 1;
        e->object_x[0] = 0x78;
        e->object_y[0] = 64 + 32;
        e->object_scaling = 0xC0;
        e->object_active[1] = 1;
        e->object_x[1] = 0x78;
        e->object_y[1] = 32;
        e->secondary_scaling = 256;
        e->object_active[4] = 1;
        e->object_active[5] = 2;
        e->object_active[6] = 2;
        e->object_active[7] = 1;
        e->object_active[8] = 2;
        e->object_active[9] = 1;
        e->object_x[2] = (uint16_t)(e->object_x[0] * 8);
        e->object_y[2] = (uint16_t)(e->object_y[0] * 8);
        load_firing_trajectory(e);
        e->bg0_x = 64 - 16;
        e->bg0_y = 64 + 16;
        gba_ppu_write16(p, GBA_REG_BG0HOFS, (uint16_t)e->bg0_x);
        gba_ppu_write16(p, GBA_REG_BG0VOFS, (uint16_t)e->bg0_y);
        break;
    case 2:
        dispcnt(e, GBA_DCNT_BG0 | GBA_DCNT_OBJ);
        mzm_sfx_play(MZM_SOUND_TOURIAN_ESCAPE_SAMUS_DODGING_SHOTS);
        mzm_sfx_play(MZM_SOUND_TOURIAN_ESCAPE_PIRATES_FIRING);
        break;
    case 120:
        e->object_active[1]++;
        break;
    case 126:
        e->object_active[0]++;
        break;
    case 160:
        e->object_timer[0] = 0;
        e->object_frame[0] = 0;
        e->object_active[0]++;
        break;
    case 192:
        dispcnt(e, 0);
        break;
    case 368:
        mzm_sfx_play(MZM_SOUND_TOURIAN_ESCAPE_SAMUS_FLYING_AFTER_SHOTS);
        break;
    case 384:
        ended = true;
        break;
    default:
        break;
    }
    if ((e->timer & 7) == 0) e->bg0_y--;
    gba_ppu_write16(p, GBA_REG_BG0VOFS, (uint16_t)e->bg0_y & 0xFF);
    update_firing_samus(e);
    if (e->object_active[4]) advance_chased_particles(e, true);
    update_firing_pirates(e);
    draw_firing_oam(e, s);
    if (ended) reset_stage(e, s, 7);
}

static void draw_getting_shot_oam(MzmTourianEscape *e, MzmScreen *s) {
    clear_oam_buffer(s);
    int slot = 0;
    if (e->object_active[0])
        place_oam_address(s, &slot, e->object_oam_address[0],
                          e->object_x[0], e->object_y[0], 0);
    for (int i = 4; i < MZM_TOURIAN_ESCAPE_OBJECTS; ++i) {
        if (!e->object_active[i]) continue;
        place_oam_address(s, &slot, e->object_oam_address[i],
                          e->object_x[i], e->object_y[i], 0);
    }
    s->next_oam_slot = (uint8_t)slot;
    mzm_screen_copy_oam(s);
}

static void update_getting_shot_oscillation(MzmTourianEscape *e) {
    switch (e->object_frame[1]) {
    case 0:
        if (--e->object_velocity[1] < -27) e->object_frame[1]++;
        break;
    case 1:
        if (++e->object_velocity[1] == 0) e->object_frame[1]++;
        break;
    case 2:
    case 5:
        if (e->object_timer[3]++ > 3) {
            e->object_frame[1] = e->object_frame[1] == 2 ? 3 : 0;
            e->object_timer[3] = 0;
        }
        break;
    case 3:
        if (++e->object_velocity[1] > 39) e->object_frame[1]++;
        break;
    case 4:
        if (--e->object_velocity[1] == 0) e->object_frame[1]++;
        break;
    default:
        break;
    }
    int velocity = e->object_velocity[1];
    if (velocity > 16) velocity = 16;
    if (velocity < -16) velocity = -16;
    e->object_x[1] = (uint16_t)(e->object_x[1] + velocity);
    e->object_x[0] = e->object_x[1] / 32u;

    switch (e->object_frame[2]) {
    case 0:
        if (--e->object_velocity[2] < -19) e->object_frame[2]++;
        break;
    case 1:
        if (++e->object_velocity[2] == 0) e->object_frame[2]++;
        break;
    case 2:
    case 5:
        /* The native structure aliases this pause counter with particle 4's
         * animation counter. Preserve that ordering and shared state. */
        if (e->object_timer[4]++ > 3) {
            e->object_frame[2] = e->object_frame[2] == 2 ? 3 : 0;
            e->object_timer[4] = 0;
        }
        break;
    case 3:
        if (++e->object_velocity[2] > 19) e->object_frame[2]++;
        break;
    case 4:
        if (--e->object_velocity[2] == 0) e->object_frame[2]++;
        break;
    default:
        break;
    }
    velocity = e->object_velocity[2];
    if (velocity > 8) velocity = 8;
    if (velocity < -8) velocity = -8;
    e->object_x[2] = (uint16_t)(e->object_x[2] + velocity);
    e->object_y[0] = e->object_x[2] / 32u;
}

static void update_samus_getting_shot(MzmTourianEscape *e, MzmScreen *s) {
    GbaPpu *p = &e->room.ppu;
    bool ended = false;
    uint16_t frame = e->timer++;
    switch (frame) {
    case 0:
        decompress("sTourianEscapeSamusGettingShotGfx", p->vram + 0x8000,
                   GBA_VRAM_SIZE - 0x8000);
        break;
    case 1:
        decompress("sTourianEscapeSamusGettingShotShipGfx",
                   p->vram + GBA_OBJ_VRAM, GBA_VRAM_SIZE - GBA_OBJ_VRAM);
        gba_ppu_write16(p, GBA_REG_BLDCNT,
                        BLDCNT_SCREEN_FIRST | BLDCNT_BRIGHTNESS_DOWN);
        gba_ppu_write16(p, GBA_REG_BLDY, 16);
        break;
    case 2:
        decompress("sTourianEscapeSamusGettingShotTileTable",
                   p->vram + 0xF000, GBA_VRAM_SIZE - 0xF000);
        copy_obj_palette(e, "sTourianEscapeSamusGettingShotPal", 5 * 16);
        e->bg0_x = 96;
        e->bg0_y = 48;
        e->effect_phase++;
        break;
    case 3: {
        static const uint8_t active[6] = { 1, 2, 2, 1, 2, 1 };
        gba_ppu_write16(p, GBA_REG_BG0CNT, 28 << 8);
        dispcnt(e, GBA_DCNT_BG0 | GBA_DCNT_OBJ);
        for (int i = 4; i < MZM_TOURIAN_ESCAPE_OBJECTS; ++i)
            e->object_active[i] = active[i - 4];
        break;
    }
    case 24:
        e->object_active[0]++;
        e->object_oam_address[0] = mzm_sym("sTourianEscape_47a904");
        e->object_x[0] = 0x12D;
        e->object_y[0] = 0xAE;
        e->object_x[1] = (uint16_t)(e->object_x[0] * 32u);
        e->object_x[2] = (uint16_t)(e->object_y[0] * 32u);
        e->object_velocity[1] = 0x38;
        e->object_velocity[2] = 0x1C;
        break;
    case 128:
        e->object_velocity[1] = 0;
        e->object_velocity[2] = 0;
        /* Fall through: frames 128 and 136 advance the native state. */
        /* fallthrough */
    case 136:
        e->object_active[0]++;
        break;
    case 336:
        copy_bg_palette(e, "sTourianEscapeSamusGettingShotPal", 5 * 16);
        gba_ppu_write16(p, GBA_REG_BG0CNT + 2, 1 | 2 << 2 | 30 << 8);
        dispcnt(e, GBA_DCNT_BG1 | GBA_DCNT_OBJ);
        e->object_active[0] = 0;
        mzm_sfx_play(MZM_SOUND_TOURIAN_ESCAPE_DECISIVE_SHOT);
        break;
    case 400:
        dispcnt(e, 0);
        ended = true;
        break;
    default:
        break;
    }

    if (e->effect_phase) {
        uint16_t level = gba_ppu_read16(p, GBA_REG_BLDY);
        if (level) {
            if ((e->timer & 7) == 0)
                gba_ppu_write16(p, GBA_REG_BLDY, (uint16_t)(level - 1));
        } else {
            gba_ppu_write16(p, GBA_REG_BLDCNT, 0);
            e->effect_phase = 0;
        }
    }
    if ((e->timer & 7) == 0) e->bg0_x--;
    gba_ppu_write16(p, GBA_REG_BG0HOFS, (uint16_t)e->bg0_x & 0x1FF);
    gba_ppu_write16(p, GBA_REG_BG0VOFS, (uint16_t)e->bg0_y & 0xFF);
    if (e->object_active[4]) advance_chased_particles(e, false);

    if (e->object_active[0] == 1) {
        if (e->timer > 111) {
            e->object_velocity[1]--;
            if (e->timer & 1) e->object_velocity[2]--;
        }
        e->object_x[1] = (uint16_t)(e->object_x[1] - e->object_velocity[1]);
        e->object_x[2] = (uint16_t)(e->object_x[2] - e->object_velocity[2]);
        e->object_x[0] = e->object_x[1] / 32u;
        e->object_y[0] = e->object_x[2] / 32u;
    } else if (e->object_active[0] == 3) {
        update_getting_shot_oscillation(e);
    }

    draw_getting_shot_oam(e, s);
    if (ended) reset_stage(e, s, 8);
}

static void copy_going_to_crash_fire_palette(MzmTourianEscape *e) {
    const char *name = (e->timer & 7) < 4 ?
        "sTourianEscape_479ee0" : "sTourianEscape_479f00";
    uint32_t address = mzm_sym(name);
    for (int i = 0; address && i < 16; ++i)
        e->room.ppu.palette[256 + 5 * 16 + i] =
            mzm_rom_u16(address + (uint32_t)i * 2u);
}

static void advance_going_to_crash_smoke(MzmTourianEscape *e, int object,
                                         bool mirror_y) {
    uint8_t offset = (uint8_t)(e->object_timer[object]++ / 4u);
    if (offset >= 8) {
        e->object_timer[object] = 0;
        e->object_x[object] = 0x6D;
        e->object_y[object] = 0x46;
        offset = 0;
    }
    e->object_oam_address[object] = mzm_rom_u32(
        mzm_sym("sTourianEscape_47cea0") + (uint32_t)offset * 4u);
    uint16_t x = mzm_rom_u16(
        mzm_sym("sTourianEscape_47ce80") + (uint32_t)offset * 2u);
    uint16_t y = mzm_rom_u16(
        mzm_sym("sTourianEscape_47ce90") + (uint32_t)offset * 2u);
    e->object_x[object] = (uint16_t)(e->object_x[object] + x);
    e->object_y[object] = (uint16_t)(e->object_y[object] +
                                     (mirror_y ? -(int)y : (int)y));
}

static void place_blended_oam_address(MzmScreen *s, int *slot,
                                      uint32_t address, int x, int y) {
    int first = *slot;
    place_oam_address(s, slot, address, x, y, 0);
    for (int i = first; i < *slot; ++i)
        s->oam[i * 4] = (uint16_t)((s->oam[i * 4] & ~(3u << 10)) | 1u << 10);
}

static void update_going_to_crash_objects(MzmTourianEscape *e,
                                          MzmScreen *s) {
    if ((e->object_timer[0] & 3) < 2) {
        e->object_x[0] = 0x6D;
        e->object_y[0] = 0x46;
    } else {
        e->object_x[0] = 0x6C;
        e->object_y[0] = 0x47;
    }
    e->object_timer[0]++;
    if (e->object_active[1]) advance_going_to_crash_smoke(e, 1, false);
    if (e->object_active[2]) advance_going_to_crash_smoke(e, 2, true);
    copy_going_to_crash_fire_palette(e);

    clear_oam_buffer(s);
    int slot = 0;
    for (int i = 0; i < 4; ++i) {
        if (!e->object_active[i]) continue;
        place_blended_oam_address(s, &slot, e->object_oam_address[i],
                                  e->object_x[i], e->object_y[i]);
    }
    s->next_oam_slot = (uint8_t)slot;
    mzm_screen_copy_oam(s);
}

static void update_samus_going_to_crash(MzmTourianEscape *e, MzmScreen *s) {
    GbaPpu *p = &e->room.ppu;
    bool ended = false;
    uint16_t frame = e->timer++;
    switch (frame) {
    case 0:
        decompress("sTourianEscapeZebesGfx", p->vram, GBA_VRAM_SIZE);
        break;
    case 1:
        decompress("sTourianEscapeShipGoingToCrashGfx",
                   p->vram + GBA_OBJ_VRAM, GBA_VRAM_SIZE - GBA_OBJ_VRAM);
        e->bg0_x = 16;
        e->bg0_y = 0;
        gba_ppu_write16(p, GBA_REG_BLDALPHA, 7 << 8 | 9);
        break;
    case 2:
        decompress("sTourianEscapeZebesTileTable", p->vram + 0xF000,
                   GBA_VRAM_SIZE - 0xF000);
        copy_bg_palette(e, "sTourianEscapeExplodingPal", 9 * 16);
        copy_obj_palette(e, "sTourianEscapeSamusGoingToCrashPal", 7 * 16);
        e->object_active[0] = 1;
        e->object_oam_address[0] = mzm_sym("sTourianEscape_47a98a");
        e->object_active[3] = 1;
        e->object_oam_address[3] = mzm_sym("sTourianEscape_47a958");
        e->object_x[3] = 0x88;
        e->object_y[3] = 0x58;
        break;
    case 3:
        gba_ppu_write16(p, GBA_REG_BG0CNT, 30 << 8);
        dispcnt(e, GBA_DCNT_BG0 | GBA_DCNT_OBJ);
        gba_ppu_write16(p, GBA_REG_BLDCNT,
                        BLDCNT_SCREEN_SECOND | BLDCNT_ALPHA);
        mzm_sfx_play(MZM_SOUND_TOURIAN_ESCAPE_SHIP_FREE_FALLING);
        break;
    case 6:
        e->object_active[1] = 1;
        e->object_x[1] = 0x6D;
        e->object_y[1] = 0x46;
        break;
    case 12:
        e->object_active[2] = 1;
        e->object_x[2] = 0x6D;
        e->object_y[2] = 0x46;
        break;
    case 80:
        dispcnt(e, 0);
        gba_ppu_write16(p, GBA_REG_BLDCNT, 0);
        ended = true;
        break;
    default:
        break;
    }

    if (e->effect_timer++ > 4) {
        e->effect_timer = 0;
        e->bg0_x--;
    }
    gba_ppu_write16(p, GBA_REG_BG0HOFS, (uint16_t)e->bg0_x & 0x1FF);
    gba_ppu_write16(p, GBA_REG_BG0VOFS, (uint16_t)e->bg0_y & 0xFF);
    update_going_to_crash_objects(e, s);
    if (ended) reset_stage(e, s, 9);
}

void mzm_tourian_escape_update(MzmTourianEscape *e, MzmScreen *s) {
    if (!e || !e->ready || !s) return;
    if (e->stage == 0) update_zebes_exploding(e, s);
    else if (e->stage == 1) update_samus_in_ship(e, s);
    else if (e->stage == 2) update_samus_looking_around(e, s);
    else if (e->stage == 3) update_samus_surrounded(e, s);
    else if (e->stage == 4) update_samus_flying_in(e, s);
    else if (e->stage == 5) update_samus_chased(e, s);
    else if (e->stage == 6) update_samus_chased_firing(e, s);
    else if (e->stage == 7) update_samus_getting_shot(e, s);
    else if (e->stage == 8) update_samus_going_to_crash(e, s);
}
