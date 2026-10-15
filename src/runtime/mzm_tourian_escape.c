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
    BLDCNT_ALPHA = 1 << 6, BLDCNT_BRIGHTNESS_UP = 2 << 6,
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
    for (int i = 0; i < 4; ++i) {
        e->object_active[i] = 1;
        object_position(e, i, room_position_tables[i], 0);
    }
    e->object_frame[3] = 8;
    for (int i = 4; i < MZM_TOURIAN_ESCAPE_OBJECTS; ++i) {
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
    for (int i = 4; i < MZM_TOURIAN_ESCAPE_OBJECTS; ++i) {
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
    memset(e->object_frame, 0, sizeof e->object_frame);
    memset(e->object_timer, 0, sizeof e->object_timer);
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
        for (int i = 4; i < MZM_TOURIAN_ESCAPE_OBJECTS; ++i) {
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
        for (int i = 4; i < MZM_TOURIAN_ESCAPE_OBJECTS; ++i) {
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

void mzm_tourian_escape_update(MzmTourianEscape *e, MzmScreen *s) {
    if (!e || !e->ready || !s) return;
    if (e->stage == 0) update_zebes_exploding(e, s);
    else if (e->stage == 1) update_samus_in_ship(e, s);
    else if (e->stage == 2) update_samus_looking_around(e, s);
    else if (e->stage == 3) update_samus_surrounded(e, s);
}
