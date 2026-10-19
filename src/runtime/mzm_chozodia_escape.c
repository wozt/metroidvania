/* SPDX-License-Identifier: GPL-3.0-only */
/* GM_CHOZODIA_ESCAPE (see mzm_chozodia_escape.h); comments name the
 * pinned decompilation's routines. */
#include "mzm_chozodia_escape.h"

#include <string.h>

#include "mzm_rom.h"
#include "mzm_sfx.h"
#include "mzm_sound_ids.h"

enum {
    OAM_BLUE_SHIP = 0, OAM_MOTHER_SHIP_DOOR = 1, OAM_SHIP_EXTERIOR = 2, OAM_MISSION_ACCOMPLISHED = 4,
    TYPE_NONE = 0, TYPE_NORMAL = 1, TYPE_SCALING = 2,
    BLDCNT_SCREEN_FIRST = 0x3F, BLDCNT_BRIGHTNESS_UP = 2 << 6, BLDCNT_BRIGHTNESS_DOWN = 3 << 6,
    BLDCNT_ALPHA = 1 << 6, BLDCNT_BG0_FIRST = 1 << 0, BLDCNT_BG1_SECOND = 1 << 9,
    BLDY_MAX = 16, BLDALPHA_MAX = 16,
    DCNT_OAM_HBL = 1 << 5,
    /* WIN0_BG0 ... WIN0_OBJ, WIN0_COLOR_EFFECT */
    WIN0_LAYERS = 0x1F, WIN0_COLOR_EFFECT = 1 << 5,
    SCREEN_W = 240, SCREEN_H = 160,
};

static uint32_t sym(const char *name) { return mzm_rom_symbol(name, NULL); }

/* CREATE_BGCNT(charBase, screenBase, priority, size) */
static void bgcnt(MzmScreen *s, int bg, int char_base, int screen_base, int priority, int size) {
    gba_ppu_write16(s->ppu, GBA_REG_BG0CNT + 2u * (unsigned)bg,
                    (uint16_t)(size << 14 | screen_base << 8 | char_base << 2 | priority));
}

static void decompress(MzmScreen *s, const char *name, uint32_t offset) {
    mzm_lz77_from_rom(sym(name), s->ppu->vram + offset, GBA_VRAM_SIZE - offset);
}

/* DMA3_COPY_16 of a palette (its whole symbol, less `drop` colors) */
static void copy_palette(MzmScreen *s, const char *name, int first, int drop) {
    uint32_t size = 0, address = mzm_rom_symbol(name, &size);
    int colors = (int)(size / 2) - drop;
    if (first + colors > 512) colors = 512 - first;
    for (int i = 0; i < colors; ++i) s->ppu->palette[first + i] = mzm_rom_u16(address + (uint32_t)i * 2u);
}

/* struct FrameData of a ROM table: pFrame, then timer. */
static uint32_t frame_pointer(uint32_t table, unsigned frame) { return mzm_rom_u32(table + frame * 8u); }
static uint8_t frame_timer(uint32_t table, unsigned frame) { return mzm_rom_u8(table + frame * 8u + 4u); }

/* ChozodiaEscapeVBlank: the haze buffer filled this frame is read next. */
static void vblank(MzmChozodiaEscape *e, MzmScreen *s) {
    GbaPpu *ppu = s->ppu;
    mzm_screen_copy_oam(s);
    gba_ppu_write16(ppu, GBA_REG_DISPCNT, e->dispcnt);
    gba_ppu_write16(ppu, GBA_REG_BLDCNT, e->bldcnt);
    gba_ppu_write16(ppu, GBA_REG_BLDALPHA, (uint16_t)(s->bldalpha_h << 8 | s->bldalpha_l));
    gba_ppu_write16(ppu, GBA_REG_BLDY, s->bldy);
    for (int bg = 0; bg < 3; ++bg) {
        gba_ppu_write16(ppu, GBA_REG_BG0HOFS + 4u * (unsigned)bg, e->bg_x[bg] & 0x1FF);
        gba_ppu_write16(ppu, GBA_REG_BG0VOFS + 4u * (unsigned)bg, e->bg_y[bg] & 0xFF);
    }
    e->haze_reading = e->haze_writing;
    /* ChozodiaEscapeHBlank: the WIN0H written at the end of line y - 1
     * shows on line y (line 0 takes line 0's value). */
    ppu->win0h_per_line = e->hblank;
    for (int y = 0; e->hblank && y < MZM_CHOZODIA_ESCAPE_LINES; ++y)
        ppu->win0h_lines[y] = e->haze[e->haze_reading][y > 0 ? y - 1 : 0];
}

/* ChozodiaEscapeSetHBlank and ChozodiaEscapeSetupHBlankRegisters */
static void enable_hblank(MzmChozodiaEscape *e, MzmScreen *s) {
    gba_ppu_write16(s->ppu, GBA_REG_WIN0H, 0);
    gba_ppu_write16(s->ppu, GBA_REG_WIN0V, SCREEN_H);
    gba_ppu_write16(s->ppu, GBA_REG_WININ, WIN0_LAYERS | WIN0_COLOR_EFFECT);
    gba_ppu_write16(s->ppu, GBA_REG_WINOUT, WIN0_LAYERS);
    e->dispcnt |= DCNT_OAM_HBL | GBA_DCNT_WIN0;
    e->hblank = true;
}

/* The H-blank disabled: WIN0H keeps the last value it wrote (the last
 * line's here). */
static void disable_hblank(MzmChozodiaEscape *e, MzmScreen *s) {
    e->hblank = false;
    s->ppu->win0h_per_line = false;
    gba_ppu_write16(s->ppu, GBA_REG_WIN0H, e->haze[e->haze_reading][MZM_CHOZODIA_ESCAPE_LINES - 1]);
}

/* ChozodiaEscapeUpdateExplosionHaze: the white ellipse growing from the
 * screen's centre (sHaze_PowerBomb_WindowValuesPointers, stretched by
 * 1.2). */
static void update_explosion_haze(MzmChozodiaEscape *e) {
    uint32_t semi = e->semi_minor_axis;
    uint32_t src = mzm_rom_u32(sym("sHaze_PowerBomb_WindowValuesPointers") + semi * 4u);
    int start_y, end_y;
    uint32_t slice;
    if (semi < 4) {
        start_y = SCREEN_H / 2 - 4;
        end_y = SCREEN_H / 2 + 4;
        slice = 0;
    } else if (semi <= SCREEN_H / 2) {
        start_y = (int16_t)(SCREEN_H / 2 - semi);
        end_y = (int16_t)(SCREEN_H / 2 + semi);
        slice = 0;
    } else {
        start_y = 0;
        end_y = SCREEN_H;
        slice = (uint32_t)(int16_t)(semi - SCREEN_H / 2);
    }
    e->haze_writing = (uint8_t)((e->haze_writing + 1) % 2);
    for (int y = start_y; y < end_y; ++y, ++slice) {
        int32_t half = (int16_t)mzm_rom_u16(src + slice * 4u) * 12 / 10;
        int32_t left = (int16_t)(SCREEN_W / 2 - half), right = (int16_t)(SCREEN_W / 2 + half);
        if (left < 0) left = 0;
        if (right > SCREEN_W) right = SCREEN_W;
        e->haze[e->haze_writing][y] = (uint16_t)(right | left << 8);
    }
}

/* Copies an OAM frame's parts at a position (the raw attributes, y and x
 * moved). Returns the next slot. */
static int copy_parts(MzmScreen *s, int slot, uint32_t frame, int parts, uint16_t x, uint16_t y) {
    uint32_t src = frame + 2u;
    for (int i = 0; i < parts && slot < MZM_OAM_BUFFER; ++i, ++slot, src += 6u) {
        uint16_t *a = &s->oam[slot * 4];
        a[0] = mzm_rom_u16(src);
        a[1] = mzm_rom_u16(src + 2u);
        a[2] = mzm_rom_u16(src + 4u);
        a[0] = (uint16_t)((a[0] & 0xFF00) | ((a[0] + y) & 0xFF));
        a[1] = (uint16_t)((a[1] & 0xFE00) | ((a[1] + x) & 0x1FF));
    }
    return slot;
}

/* ChozodiaEscapeProcessOam_1: every object; the letters (the last one)
 * only up to their revealed count. */
static void process_oam_1(MzmChozodiaEscape *e, MzmScreen *s) {
    int slot = 0;
    for (int i = 0; i < MZM_CHOZODIA_ESCAPE_OBJECTS; ++i) {
        if (e->oam_types[i] == TYPE_NONE) continue;
        int parts = mzm_rom_u16(e->oam_pointers[i]) & 0xFF;
        if (i == MZM_CHOZODIA_ESCAPE_OBJECTS - 1 && parts > e->oam_frames[i]) parts = e->oam_frames[i];
        int next = slot + (mzm_rom_u16(e->oam_pointers[i]) & 0xFF);
        int end = copy_parts(s, slot, e->oam_pointers[i], parts, e->oam_x[i], e->oam_y[i]);
        slot = i == MZM_CHOZODIA_ESCAPE_OBJECTS - 1 ? end : next;
    }
    s->next_oam_slot = (uint8_t)slot;
}

/* ChozodiaEscapeProcessOam_2: the blue ship scaled (double-sized affine,
 * matrix 0), then the mother ship door and exterior. */
static void process_oam_2(MzmChozodiaEscape *e, MzmScreen *s) {
    int slot = 0;
    if (e->oam_types[OAM_BLUE_SHIP] != TYPE_NONE) {
        uint32_t frame = e->oam_pointers[OAM_BLUE_SHIP];
        int parts = mzm_rom_u16(frame) & 0xFF;
        for (int i = 0; i < parts && slot < MZM_OAM_BUFFER; ++i, ++slot) {
            uint16_t *a = &s->oam[slot * 4];
            a[0] = mzm_rom_u16(frame + 2u + (uint32_t)i * 6u);
            a[1] = mzm_rom_u16(frame + 4u + (uint32_t)i * 6u);
            a[2] = mzm_rom_u16(frame + 6u + (uint32_t)i * 6u);
            mzm_process_complex_oam(s, slot, (int16_t)e->oam_x[OAM_BLUE_SHIP], (int16_t)e->oam_y[OAM_BLUE_SHIP], 0,
                                    (int16_t)e->scaling, 0);
        }
        mzm_calculate_oam_part4(s, 0, (int16_t)e->scaling, 0);
    }
    for (int i = 1; i < MZM_CHOZODIA_ESCAPE_OBJECTS - 2; ++i) {
        if (e->oam_types[i] == TYPE_NONE) continue;
        slot = copy_parts(s, slot, e->oam_pointers[i], mzm_rom_u16(e->oam_pointers[i]) & 0xFF, e->oam_x[i],
                          e->oam_y[i]);
    }
    s->next_oam_slot = (uint8_t)slot;
}

/* ApplyMonochromeToPalette(src, dst, 0) */
static void monochrome(uint32_t src, uint16_t *dst) {
    for (int i = 0; i < 256; ++i) {
        uint16_t c = mzm_rom_u16(src + (uint32_t)i * 2u);
        int grey = ((c & 31) + (c >> 5 & 31) + (c >> 10 & 31)) / 3;
        dst[i] = (uint16_t)(grey | grey << 5 | grey << 10);
    }
}

/* ChozodiaEscapeInit */
static void init(MzmChozodiaEscape *e, MzmScreen *s) {
    gba_ppu_write16(s->ppu, GBA_REG_DISPCNT, 0);
    s->ppu->win0h_per_line = false;
    mzm_clear_gfx_ram(s);
    decompress(s, "sCutsceneMotherShipEscapeShipParticlesGfx", GBA_OBJ_VRAM);
    decompress(s, "sCutsceneZebesMotherShipBackgroundGfx", 0);
    decompress(s, "sCutsceneZebesGroundGfx", 0x8000);
    decompress(s, "sCutsceneZebesRockyBackgroundGfx", 0xC800);
    decompress(s, "sCutsceneZebesGroundTileTable", 0xA000);
    decompress(s, "sCutscene_3b5168_TileTable", 0xA800);
    decompress(s, "sCutsceneZebesMotherShipBackgroundTileTable", 0xB000);
    copy_palette(s, "sCutsceneZebesPal", 0, 0);
    copy_palette(s, "sCutsceneMotherShipPal", 256, 0);
    bgcnt(s, 0, 2, 20, 0, 0);
    bgcnt(s, 1, 2, 21, 1, 0);
    bgcnt(s, 2, 0, 22, 2, 2);
    uint8_t sub_mode = e->sub_mode;
    memset(e, 0, sizeof *e);
    e->sub_mode = sub_mode;
    e->bg_y[0] = e->bg_y[1] = 64 + 32;
    e->bg_y[2] = 64 * 2 + 32 + 8;
    s->next_oam_slot = 0;
    e->oam_pointers[OAM_BLUE_SHIP] = sym("sChozodiaEscapeOam_BlueShipAngledDown_Frame0");
    e->oam_x[OAM_BLUE_SHIP] = 0x78;
    e->oam_y[OAM_BLUE_SHIP] = 0x63;
    e->oam_y_offset = 8;
    e->scaling = 32;                                   /* Q_8_8(.25f / 2) */
    e->oam_types[OAM_MOTHER_SHIP_DOOR]++;
    e->oam_pointers[OAM_MOTHER_SHIP_DOOR] = sym("sChozodiaEscapeOam_MotherShipDoorClosed_Frame0");
    e->oam_x[OAM_MOTHER_SHIP_DOOR] = 0x78;
    e->oam_y[OAM_MOTHER_SHIP_DOOR] = 0x54;
    e->oam_types[OAM_SHIP_EXTERIOR]++;
    e->oam_pointers[OAM_SHIP_EXTERIOR] = sym("sChozodiaEscapeOam_ShipExterior_Frame0");
    e->oam_x[OAM_SHIP_EXTERIOR] = 0x78;
    e->oam_y[OAM_SHIP_EXTERIOR] = 0x54;
    process_oam_2(e, s);
    mzm_reset_free_oam(s);
    monochrome(sym("sChozodiaEscapeMissionAccomplishedPal"), e->monochrome);
    e->dispcnt = GBA_DCNT_BG0 | GBA_DCNT_BG1 | GBA_DCNT_BG2 | GBA_DCNT_OBJ;
    e->bldcnt = BLDCNT_SCREEN_FIRST | BLDCNT_BRIGHTNESS_DOWN;
    s->bldalpha_l = BLDALPHA_MAX;
    s->bldalpha_h = 0;
    s->bldy = BLDY_MAX;
}

/* ChozodiaEscapeShipLeaving: the door opens, the ship grows and dives,
 * then climbs away. */
static int ship_leaving(MzmChozodiaEscape *e, MzmScreen *s) {
    int ended = 0;
    switch (e->timer++) {
    case 0: mzm_sfx_play(MZM_SOUND_CHOZODIA_ESCAPE_MOTHER_SHIP_DOOR_OPENING); break;
    case 160:                                          /* CONVERT_SECONDS(2.f) + TWO_THIRD_SECOND */
        e->oam_types[OAM_BLUE_SHIP]++;
        mzm_sfx_play(MZM_SOUND_CHOZODIA_ESCAPE_BLUE_SHIP_TAKING_OFF);
        break;
    case 294:                                          /* CONVERT_SECONDS(4.9f) */
        e->oam_types[OAM_BLUE_SHIP] = TYPE_SCALING;
        e->oam_pointers[OAM_BLUE_SHIP] = sym("sChozodiaEscapeOam_BlueShipAngledUp_Frame0");
        e->oam_y[OAM_BLUE_SHIP] = 64 * 3 - 16 + 8;
        e->oam_y_offset = -16;
        e->scaling = 256;
        break;
    case 312: ended = 1; break;                        /* CONVERT_SECONDS(5.2f) */
    default: break;
    }
    if (e->oam_types[OAM_BLUE_SHIP] == TYPE_NORMAL) {
        e->scaling += 5;                               /* Q_8_8(0.02f) */
        if (e->scaling > 256) {
            if ((e->timer & 7) == 0) e->oam_y_offset++;
        } else if (e->scaling > 56) {                  /* Q_8_8(0.22f) */
            if ((e->timer & 3) == 0) e->oam_y_offset++;
        }
        if (e->oam_y[OAM_BLUE_SHIP] > 64 * 3 - 16) e->oam_types[OAM_BLUE_SHIP] = TYPE_NONE;
    } else if (e->oam_types[OAM_BLUE_SHIP] == TYPE_SCALING) {
        e->scaling += 16;                              /* Q_8_8(.25f / 4) */
        e->oam_y_offset -= 8;
    }
    if (e->oam_types[OAM_BLUE_SHIP] != TYPE_NONE) {
        if (e->scaling > 512) e->scaling = 512;
        e->oam_y[OAM_BLUE_SHIP] = (uint16_t)(e->oam_y[OAM_BLUE_SHIP] + (e->oam_y_offset >> 3));
    }
    if (e->oam_types[OAM_MOTHER_SHIP_DOOR] == TYPE_NORMAL) {
        uint32_t table = sym("sChozodiaEscapeOam_MotherShipDoorOpening");
        if (e->oam_timers[OAM_MOTHER_SHIP_DOOR]++ >= frame_timer(table, e->oam_frames[OAM_MOTHER_SHIP_DOOR])) {
            e->oam_frames[OAM_MOTHER_SHIP_DOOR]++;
            e->oam_timers[OAM_MOTHER_SHIP_DOOR] = 0;
            if (frame_timer(table, e->oam_frames[OAM_MOTHER_SHIP_DOOR]) == 0) {
                e->oam_frames[OAM_MOTHER_SHIP_DOOR]--;
                e->oam_types[OAM_MOTHER_SHIP_DOOR]++;
            }
        }
        e->oam_pointers[OAM_MOTHER_SHIP_DOOR] = frame_pointer(table, e->oam_frames[OAM_MOTHER_SHIP_DOOR]);
    }
    process_oam_2(e, s);
    return ended;
}

static void move_mother_ship(MzmChozodiaEscape *e, int delta) {
    e->oam_y[OAM_MOTHER_SHIP_DOOR] = (uint16_t)(e->oam_y[OAM_MOTHER_SHIP_DOOR] + delta);
    e->oam_y[OAM_SHIP_EXTERIOR] = (uint16_t)(e->oam_y[OAM_SHIP_EXTERIOR] + delta);
}

/* ChozodiaEscapeShipHeatingUp: the mother ship reddening and shaking ever
 * faster, then the white explosion growing over it. */
static int ship_heating_up(MzmChozodiaEscape *e, MzmScreen *s) {
    int ended = 0;
    switch (e->timer++) {
    case 0:
        e->oam_types[OAM_MOTHER_SHIP_DOOR] = TYPE_NORMAL;
        e->oam_types[OAM_SHIP_EXTERIOR] = TYPE_NORMAL;
        mzm_sfx_play(MZM_SOUND_CHOZODIA_ESCAPE_MOTHER_SHIP_HEATING_UP);
        break;
    case 152:                                          /* CONVERT_SECONDS(2.5f + 1.f / 30) */
        enable_hblank(e, s);
        s->bldy = BLDY_MAX / 2;
        break;
    case 160:                                          /* CONVERT_SECONDS(2.f) + TWO_THIRD_SECOND */
        e->bldcnt = BLDCNT_SCREEN_FIRST | BLDCNT_BRIGHTNESS_UP;
        e->unk_1++;
        mzm_sfx_play(MZM_SOUND_CHOZODIA_ESCAPE_MOTHER_SHIP_BLOWING_AURA);
        break;
    case 224:                                          /* CONVERT_SECONDS(3.7f + 1.f / 30) */
        disable_hblank(e, s);
        ended = 1;
        break;
    default: break;
    }
    update_explosion_haze(e);
    if (e->unk_1) {
        if (e->unk_2++ >= 6) {                         /* CONVERT_SECONDS(.1f) */
            if (s->bldy < BLDY_MAX) s->bldy++;
            e->unk_2 = 0;
        }
        uint32_t size = 0;
        mzm_rom_symbol("sHaze_PowerBomb_WindowValuesPointers", &size);
        uint16_t last = (uint16_t)(size / 4 - 1);
        e->semi_minor_axis = (uint16_t)(e->semi_minor_axis + 4);
        if (e->semi_minor_axis > last) e->semi_minor_axis = last;
    }
    if (e->timer < 127) {
        uint8_t offset = mzm_rom_u8(sym("sChozodiaEscapeHeatingUpPalOffsets") + e->timer / 16u);
        uint32_t ship = sym("sChozodiaEscapeShipHeatingUpPal"), ground = sym("sChozodiaEscapeGroundHeatingUpPal");
        for (int i = 0; i < 16; ++i) {
            s->ppu->palette[256 + i] = mzm_rom_u16(ship + (offset + (uint32_t)i) * 2u);
            s->ppu->palette[256 + 64 + i] = mzm_rom_u16(ground + (offset + (uint32_t)i) * 2u);
        }
    }
    uint8_t *shake = &e->oam_timers[OAM_MOTHER_SHIP_DOOR];
    if (e->timer > 128) {
        if ((e->timer & 3) == 1) move_mother_ship(e, -1);
        if ((e->timer & 3) == 3) move_mother_ship(e, 1);
    } else if (e->timer > 96) {
        if (*shake == 1) move_mother_ship(e, -1);
        if (*shake == 4) move_mother_ship(e, 1);
        if ((*shake)++ > 7) *shake = 0;
    } else if (e->timer > 48) {
        if (*shake == 1) move_mother_ship(e, -1);
        if (*shake == 6) move_mother_ship(e, 1);
        if ((*shake)++ > 9) *shake = 0;
    }
    process_oam_1(e, s);
    return ended;
}

/* sChozodiaEscape_5ca0d8 / _5ca0f8: the explosions' positions */
static void explosion_position(MzmChozodiaEscape *e, int i, const char *table, unsigned index) {
    uint32_t at = sym(table) + index * 4u;
    e->oam_x[i] = mzm_rom_u16(at);
    e->oam_y[i] = mzm_rom_u16(at + 2u);
}

/* ChozodiaEscapeShipBlowingUp: the crater, the mother ship decaying in
 * explosions with the flash fading, then the fade to black. */
static int ship_blowing_up(MzmChozodiaEscape *e, MzmScreen *s) {
    int ended = 0;
    switch (e->timer++) {
    case 0: decompress(s, "sChozodiaEscapeCraterBackgroundGfx", 0); break;
    case 1: decompress(s, "sMotherShipExplodingFlashGfx", 0x8000); break;
    case 2: decompress(s, "sMotherShipBlowingUpExplosionsGfx", GBA_OBJ_VRAM); break;
    case 3:
        decompress(s, "sChozodiaEscapeCraterBackgroundTileTable", 0xE800);
        decompress(s, "sMotherShipExplodingFlashTileTable", 0xF000);
        copy_palette(s, "sChozodiaEscapeShipExplodingPal", 0, 32);
        copy_palette(s, "sMotherShipBlowingUpExplosionsPal", 256, 0);
        bgcnt(s, 0, 2, 30, 0, 0);
        bgcnt(s, 1, 0, 29, 1, 0);
        e->dispcnt = GBA_DCNT_BG0 | GBA_DCNT_BG1 | GBA_DCNT_OBJ;
        e->bldcnt = BLDCNT_BG0_FIRST | BLDCNT_ALPHA | BLDCNT_BG1_SECOND;
        s->bldy = 0;
        e->oam_types[0]++;
        e->oam_x[0] = 64 * 2 - 8;
        e->oam_y[0] = 64 + 16;
        e->oam_types[3]++;
        e->oam_x[3] = 64 * 2 - 8;
        e->oam_y[3] = 64 + 16;
        mzm_sfx_play(MZM_SOUND_CHOZODIA_ESCAPE_MOTHER_SHIP_BLOWING_UP);
        break;
    case 32:
        e->unk_1++;
        mzm_sfx_music_fade(240);                       /* FadeMusic(CONVERT_SECONDS(4.f)) */
        break;
    case 64:
        e->oam_types[1]++;
        explosion_position(e, 1, "sChozodiaEscape_5ca0d8", 0);
        break;
    case 76:
        e->oam_types[2]++;
        explosion_position(e, 2, "sChozodiaEscape_5ca0f8", 0);
        break;
    case 176:
        e->dispcnt = GBA_DCNT_BG1 | GBA_DCNT_OBJ;
        e->bldcnt = BLDCNT_SCREEN_FIRST | BLDCNT_BRIGHTNESS_DOWN;
        e->unk_1++;
        s->bldalpha_l = 16;
        s->bldalpha_h = 0;
        s->bldy = 0;
        break;
    case 288: e->dispcnt = 0; break;
    case 304: ended = 1; break;
    default: break;
    }
    if (e->unk_1 == 1 && e->unk_2++ > 2) {
        if (s->bldalpha_l) s->bldalpha_l--;
        s->bldalpha_h = (uint8_t)(16 - s->bldalpha_l);
        e->unk_2 = 0;
    }
    if (e->unk_1 == 2 && e->unk_2++ > 5) {
        if (s->bldy < BLDY_MAX) s->bldy++;
        e->unk_2 = 0;
    }
    uint16_t interval = e->timer & 3;
    if (interval == 0) {
        e->bg_y[1]--;
        e->oam_y[3]++;
    }
    if (interval == 2) {
        e->bg_y[1]++;
        e->oam_y[3]--;
    }
    uint32_t bases = sym("sChozodiaEscape_5ca0c4");
    for (int i = 0; i < MZM_CHOZODIA_ESCAPE_OBJECTS - 1; ++i) {
        if (e->oam_types[i] == TYPE_NONE) continue;
        uint32_t base = mzm_rom_u32(bases + (i == 0 && e->oam_types[i] > 1 ? 4u : (uint32_t)i) * 4u);
        unsigned frame = e->oam_frames[i];
        if (e->oam_timers[i]++ >= frame_timer(base, frame)) {
            e->oam_frames[i]++;
            e->oam_timers[i] = 0;
            frame++;
            if (frame_timer(base, frame) == 0) {
                if (i == 0) {
                    e->oam_types[i] = TYPE_SCALING;
                    e->oam_frames[i] = 0;
                } else if (i == 3) {
                    e->oam_frames[i]--;
                } else {
                    e->oam_frames[i] = 0;
                    e->unk_3e[i] = (uint8_t)((e->unk_3e[i] + 1) & 7);
                    explosion_position(e, i, i == 2 ? "sChozodiaEscape_5ca0f8" : "sChozodiaEscape_5ca0d8",
                                       e->unk_3e[i]);
                }
                frame = 0;
            }
        }
        e->oam_pointers[i] = frame_pointer(base, frame);
    }
    process_oam_1(e, s);
    return ended;
}

/* The ship's subpixel track in object 1's position (an eighth of a pixel
 * each). */
static void move_ship_track(MzmChozodiaEscape *e) {
    uint16_t x = e->oam_x[1] = (uint16_t)(e->oam_x[1] + e->oam_x_offset);
    uint16_t y = e->oam_y[1] = (uint16_t)(e->oam_y[1] + e->oam_y_offset);
    e->oam_x[OAM_BLUE_SHIP] = x / 8;
    e->oam_y[OAM_BLUE_SHIP] = y / 8;
}

/* ChozodiaEscapeShipLeavingPlanet: the ship far away over Zebes, then
 * flying past the camera. */
static int ship_leaving_planet(MzmChozodiaEscape *e, MzmScreen *s) {
    int ended = 0;
    switch (e->timer++) {
    case 0:
        decompress(s, "sChozodiaEscapeZebesAndSkyGfx", 0);
        mzm_sfx_music_play(MZM_MUSIC_ESCAPE_SUCCESFUL, 0);
        break;
    case 1: decompress(s, "sChozodiaEscapeSamusInBlueShipGfx", 0x8000); break;
    case 2: decompress(s, "sChozodiaEscapeBlueShipVeryCloseGfx", GBA_OBJ_VRAM); break;
    case 3:
        decompress(s, "sChozodiaEscapeZebesBackgroundTileTable", 0xE800);
        decompress(s, "sChozodiaEscapeZebesSkyTileTable", 0xF000);
        decompress(s, "sChozodiaEscapeSamusInBlueShipTileTable", 0xF800);
        copy_palette(s, "sChozodiaEscapeMissionAccomplishedPal", 0, 0);
        copy_palette(s, "sChozodiaEscapeMissionAccomplishedPal", 256, 0);
        e->oam_types[OAM_BLUE_SHIP]++;
        e->oam_pointers[OAM_BLUE_SHIP] = sym("sChozodiaEscapeOam_BlueShipFarAway_Frame0");
        e->oam_x[OAM_BLUE_SHIP] = 0x28;
        e->oam_y[OAM_BLUE_SHIP] = 0x70;
        e->oam_x_offset = 4;
        e->oam_y_offset = -5;
        e->oam_x[1] = (uint16_t)(e->oam_x[OAM_BLUE_SHIP] * 8);
        e->oam_y[1] = (uint16_t)(e->oam_y[OAM_BLUE_SHIP] * 8);
        bgcnt(s, 0, 2, 31, 0, 0);
        bgcnt(s, 1, 0, 29, 1, 0);
        bgcnt(s, 2, 0, 30, 2, 0);
        e->dispcnt = GBA_DCNT_BG1 | GBA_DCNT_BG2 | GBA_DCNT_OBJ;
        e->bg_x[1] = 16;
        break;
    case 224: e->oam_types[OAM_BLUE_SHIP] = TYPE_NONE; break;
    case 256:
        e->oam_types[OAM_BLUE_SHIP] = TYPE_SCALING;
        e->oam_pointers[OAM_BLUE_SHIP] = sym("sChozodiaEscapeOam_BlueShipVeryClose_Frame0");
        e->oam_timers[OAM_BLUE_SHIP] = 0;
        e->oam_x[OAM_BLUE_SHIP] = 0x128;
        e->oam_y[OAM_BLUE_SHIP] = 0x26;
        e->oam_x_offset = -39;
        e->oam_y_offset = 6;
        e->scaling = 232;                              /* Q_8_8(0.91f) */
        e->oam_x[1] = (uint16_t)(e->oam_x[OAM_BLUE_SHIP] * 8);
        e->oam_y[1] = (uint16_t)(e->oam_y[OAM_BLUE_SHIP] * 8);
        break;
    case 280:
        e->dispcnt = GBA_DCNT_BG0 | GBA_DCNT_BG1 | GBA_DCNT_BG2;
        ended = 1;
        break;
    default: break;
    }
    /* The fade in from the previous stage's black. */
    if (!e->unk_1 && !(e->timer & 3)) {
        if (s->bldy) s->bldy--;
        else e->unk_1++;
    }
    if (e->oam_types[OAM_BLUE_SHIP] == TYPE_NORMAL) {
        if (e->timer < 96) {
            if (e->oam_timers[OAM_BLUE_SHIP]++ > 13) {
                e->oam_x_offset++;
                e->oam_timers[OAM_BLUE_SHIP] = 0;
            }
        } else if (e->oam_timers[OAM_BLUE_SHIP]++ > 1) {
            e->oam_x_offset++;
            e->oam_timers[OAM_BLUE_SHIP] = 0;
        }
        move_ship_track(e);
        process_oam_1(e, s);
    } else if (e->oam_types[OAM_BLUE_SHIP] == TYPE_SCALING) {
        if (e->oam_timers[OAM_BLUE_SHIP]++ & 1) {
            e->oam_x_offset--;
            e->oam_y_offset++;
            e->oam_timers[OAM_BLUE_SHIP] = 0;
        }
        move_ship_track(e);
        e->scaling += 12;                              /* Q_8_8(0.05f) */
        if (e->scaling > 512) e->scaling = 512;
        process_oam_2(e, s);
    }
    if ((e->timer & 15) == 0) {
        e->bg_x[1]--;
        e->bg_x[2]--;
    }
    return ended;
}

/* ChozodiaEscapeMissionAccomplished: Samus in her ship, the scene turning
 * grey, the English letters appearing one by one, then the fade. */
static int mission_accomplished(MzmChozodiaEscape *e, MzmScreen *s) {
    int ended = 0;
    switch (e->timer++) {
    case 0:
        decompress(s, "sChozodiaEscapeMissionAccomplishedLettersGfx", GBA_OBJ_VRAM);
        copy_palette(s, "sChozodiaEscapeSamusInBlueShipPal", 256, 0);
        e->dispcnt = GBA_DCNT_BG0 | GBA_DCNT_BG1 | GBA_DCNT_BG2 | GBA_DCNT_OBJ;
        break;
    case 48:                                           /* CONVERT_SECONDS(.8f) */
        memcpy(s->ppu->palette, e->monochrome, sizeof e->monochrome);
        e->unk_1++;
        break;
    case 64:                                           /* CONVERT_SECONDS(1.f + 1.f / 15) */
        e->oam_types[OAM_MISSION_ACCOMPLISHED]++;
        e->oam_pointers[OAM_MISSION_ACCOMPLISHED] = sym("sChozodiaEscapeOam_MissionAccomplishedEnglish_Frame0");
        e->oam_frames[OAM_MISSION_ACCOMPLISHED] = 1;
        e->oam_x[OAM_MISSION_ACCOMPLISHED] = SCREEN_W / 2;
        e->oam_y[OAM_MISSION_ACCOMPLISHED] = 88;       /* SCREEN_SIZE_Y * .55f */
        break;
    case 472:                                          /* CONVERT_SECONDS(7.8f + 1.f / 15) */
        e->bldcnt = BLDCNT_SCREEN_FIRST | BLDCNT_BRIGHTNESS_DOWN;
        s->bldy = 0;
        ended = 2;
        break;
    default: break;
    }
    if (e->oam_types[OAM_MISSION_ACCOMPLISHED] != TYPE_NONE &&
        e->oam_timers[OAM_MISSION_ACCOMPLISHED]++ > 3) {   /* CONVERT_SECONDS(0.05f) */
        if (e->oam_frames[OAM_MISSION_ACCOMPLISHED] < 48) e->oam_frames[OAM_MISSION_ACCOMPLISHED]++;
        e->oam_timers[OAM_MISSION_ACCOMPLISHED] = 0;
    }
    process_oam_1(e, s);
    if (!e->unk_1 && !(e->timer & 7)) e->bg_x[2]--;
    return ended;
}

bool mzm_chozodia_escape_enter(MzmChozodiaEscape *e, MzmScreen *s) {
    memset(e, 0, sizeof *e);
    return s && s->ppu && sym("sChozodiaEscapeOam_BlueShipAngledDown_Frame0") != 0;
}

/* ChozodiaEscapeHandler */
bool mzm_chozodia_escape_update(MzmChozodiaEscape *e, MzmScreen *s) {
    bool ended = false;
    s->next_oam_slot = 0;
    switch (e->sub_mode) {
    case 0:
        init(e, s);
        e->sub_mode++;
        break;
    case 1:
        if (s->bldy) {
            s->bldy--;
            break;
        }
        e->bldcnt = 0;
        e->sub_mode++;
        break;
    case 2: {
        int result;
        switch (e->stage) {
        case 0: result = ship_leaving(e, s); break;
        case 1: result = ship_heating_up(e, s); break;
        case 2: result = ship_blowing_up(e, s); break;
        case 3: result = ship_leaving_planet(e, s); break;
        default: result = mission_accomplished(e, s); break;
        }
        if (result == 1) {
            e->stage++;
            e->unk_1 = e->unk_2 = 0;
            e->timer = 0;
            for (int i = 0; i < MZM_CHOZODIA_ESCAPE_OBJECTS; ++i)
                e->oam_types[i] = e->oam_frames[i] = e->oam_timers[i] = e->unk_3e[i] = 0;
            if (e->stage > 1)
                for (int bg = 0; bg < 3; ++bg) e->bg_x[bg] = e->bg_y[bg] = 0;
        }
        if (result == 2) e->sub_mode++;
        mzm_reset_free_oam(s);
        break;
    }
    default:
        if (s->bldy < BLDY_MAX) {
            s->bldy++;
            break;
        }
        ended = true;
        break;
    }
    vblank(e, s);
    return ended;
}
