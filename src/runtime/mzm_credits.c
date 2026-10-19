/* SPDX-License-Identifier: GPL-3.0-only */
/* GM_CREDITS (see mzm_credits.h); comments name the pinned
 * decompilation's routines. */
#include "mzm_credits.h"

#include <string.h>

#include "gba_input.h"
#include "mzm_rom.h"
#include "mzm_sfx.h"
#include "mzm_sound_ids.h"

enum {
    /* CreditLineType */
    LINE_BLUE = 0, LINE_RED = 1, LINE_WHITE_BIG = 2, LINE_WHITE_SMALL = 3, LINE_BLANK = 5, LINE_END = 6,
    LINE_ALL_RIGHTS = 10, LINE_THE_COPYRIGHT = 11, LINE_SCENARIO = 12, LINE_RESERVED = 13,
    CREDITS_ENTRY_SIZE = 36, CREDITS_TEXT = 35,
    BLDCNT_BG0_FIRST = 1 << 0, BLDCNT_BG1_FIRST = 1 << 1, BLDCNT_BG2_FIRST = 1 << 2, BLDCNT_BG3_FIRST = 1 << 3,
    BLDCNT_OBJ_FIRST = 1 << 4, BLDCNT_BACKDROP_FIRST = 1 << 5, BLDCNT_SCREEN_FIRST = 0x3F,
    BLDCNT_ALPHA = 1 << 6, BLDCNT_BRIGHTNESS_UP = 2 << 6, BLDCNT_BRIGHTNESS_DOWN = 3 << 6,
    BLDCNT_BG1_SECOND = 1 << 9, BLDCNT_BG2_SECOND = 1 << 10, BLDCNT_BG3_SECOND = 1 << 11,
    BLDY_MAX = 16, BLDALPHA_MAX = 16,
    /* gSubGameMode1 of CreditsHandler */
    MODE_CREDITS = 2, MODE_END_SCREEN_INIT = 4, MODE_END_SCREEN = 6, MODE_ENDING_IMAGE_INIT = 8,
    MODE_ENDING_IMAGE_FADE_IN = 9, MODE_ENDING_IMAGE = 10, MODE_UNLOCKED_FADE_OUT = 11,
    MODE_UNLOCKED_INIT = 12, MODE_UNLOCKED_FADE_IN = 13, MODE_UNLOCKED_POP_UP = 14,
    MODE_UNLOCKED_END = 15,
    ENDING_OAM_NONE = 0, ENDING_OAM_CHAR = 1, ENDING_OAM_LINE = 2,
    ENDING_LINE_CLEAR_TIME = 0, ENDING_LINE_TIMER, ENDING_LINE_YOUR_RATE,
    ENDING_LINE_COLLECTING, ENDING_LINE_PERCENTAGE, ENDING_LINE_NEXT_MISSION,
    ENDING_LINE_COUNT,
    LANGUAGE_ENGLISH = 2,
    FILE_TEXT_GALLERY_UNLOCK = 33, FILE_TEXT_TIME_ATTACK_RECORD_UNLOCK = 34,
    FILE_TEXT_SOUND_TEST_UNLOCK = 35,
    POPUP_FLAGS = MZM_ENDING_FLAG_FIRST_TIME_ATTACK_CLEAR |
                  MZM_ENDING_FLAG_FIRST_HARD_MODE_CLEAR | MZM_ENDING_FLAG_FIRST_CLEAR,
};

static uint32_t sym(const char *name) { return mzm_rom_symbol(name, NULL); }

uint32_t mzm_ending_item_count_and_number(const MzmEndingStats *st) {
    uint32_t tanks = sym("sTankIncreaseAmount");
    if (!tanks || st->difficulty > 2) return 0;
    uint32_t at = tanks + st->difficulty * 4u;
    uint32_t energy = (uint32_t)(st->max_energy - 99) / mzm_rom_u8(at);
    uint32_t missiles = (uint32_t)st->max_missiles / mzm_rom_u8(at + 1u);
    uint8_t supers = (uint8_t)((uint32_t)st->max_super_missiles / mzm_rom_u8(at + 2u));
    uint8_t power_bombs = (uint8_t)((uint32_t)st->max_power_bombs / mzm_rom_u8(at + 3u));
    uint32_t abilities = (uint32_t)st->abilities;
    uint32_t completion = abilities + energy + missiles + supers + power_bombs;
    uint32_t ending = 0;                               /* ENDING_IMAGE_ZERO */
    if (st->difficulty != 0) {
        if (completion <= 15) ending = st->difficulty + 6u - 1u;
        else if (completion >= 100) ending = st->igt_hours >= 2 ? 3u : st->difficulty + 4u - 1u;
        else if (st->igt_hours < 2) ending = 2;
        else if (st->igt_hours < 4) ending = 1;
    }
    return (energy << 24) + (missiles << 16) + ((uint32_t)supers << 12) + ((uint32_t)power_bombs << 8) +
           (abilities << 4) + ending;
}

/* CREATE_BGCNT(charBase, screenBase, priority, size) */
static void bgcnt(MzmScreen *s, int bg, int char_base, int screen_base, int priority, int size) {
    gba_ppu_write16(s->ppu, GBA_REG_BG0CNT + 2u * (unsigned)bg,
                    (uint16_t)(size << 14 | screen_base << 8 | char_base << 2 | priority));
}

static void decompress(MzmScreen *s, const char *name, uint32_t offset) {
    mzm_lz77_from_rom(sym(name), s->ppu->vram + offset, GBA_VRAM_SIZE - offset);
}

static void decompress_address(MzmScreen *s, uint32_t address, uint32_t offset) {
    mzm_lz77_from_rom(address, s->ppu->vram + offset, GBA_VRAM_SIZE - offset);
}

static void copy_colors(MzmScreen *s, uint32_t address, int colors) {
    for (int i = 0; i < colors && i < 256; ++i) s->ppu->palette[i] = mzm_rom_u16(address + (uint32_t)i * 2u);
}

static void copy_palette(MzmScreen *s, const char *name, int first) {
    uint32_t size = 0, address = mzm_rom_symbol(name, &size);
    for (int i = 0; i < (int)(size / 2) && first + i < 512; ++i)
        s->ppu->palette[first + i] = mzm_rom_u16(address + (uint32_t)i * 2u);
}

static void copy_palette_address(MzmScreen *s, uint32_t address, int first, int colors) {
    for (int i = 0; i < colors && first + i < 512; ++i)
        s->ppu->palette[first + i] = mzm_rom_u16(address + (uint32_t)i * 2u);
}

static void write_bg(MzmScreen *s, int bg, bool vertical, uint16_t value) {
    gba_ppu_write16(s->ppu, (vertical ? GBA_REG_BG0VOFS : GBA_REG_BG0HOFS) + 4u * (unsigned)bg, value);
}

static void write_common(MzmCredits *c, MzmScreen *s) {
    mzm_screen_copy_oam(s);
    gba_ppu_write16(s->ppu, GBA_REG_DISPCNT, c->data.dispcnt);
    gba_ppu_write16(s->ppu, GBA_REG_BLDCNT, c->data.bldcnt);
    gba_ppu_write16(s->ppu, GBA_REG_BLDALPHA, (uint16_t)(s->bldalpha_h << 8 | s->bldalpha_l));
    gba_ppu_write16(s->ppu, GBA_REG_BLDY, s->bldy);
}

/* GalleryVBlank: the credit line set this frame goes to BG0 (even width)
 * or to BG1, half a tile to the side (odd width), clearing the other. */
static void credits_vblank(MzmCredits *c, MzmScreen *s) {
    MzmEndingData *d = &c->data;
    if (d->unk_6) {
        uint32_t shown = d->unk_6 == 1 ? 0 : 0x800, cleared = d->unk_6 == 1 ? 0x800 : 0;
        uint8_t *vram = s->ppu->vram;
        for (int i = 0; i < 32; ++i) {
            vram[shown + d->line_offset_1 + 2u * (unsigned)i] = (uint8_t)d->line_tilemap_1[i];
            vram[shown + d->line_offset_1 + 2u * (unsigned)i + 1] = (uint8_t)(d->line_tilemap_1[i] >> 8);
            vram[shown + d->line_offset_2 + 2u * (unsigned)i] = (uint8_t)d->line_tilemap_2[i];
            vram[shown + d->line_offset_2 + 2u * (unsigned)i + 1] = (uint8_t)(d->line_tilemap_2[i] >> 8);
        }
        memset(vram + cleared + d->line_offset_1, 0, 64);
        memset(vram + cleared + d->line_offset_2, 0, 64);
    }
    write_common(c, s);
    uint16_t position = (uint16_t)((c->bg_y[0] / 16) & 0x1FF);
    write_bg(s, 0, true, position);
    write_bg(s, 1, true, position);
    write_bg(s, 2, true, (uint16_t)((c->bg_y[2] / 16) & 0x1FF));
    write_bg(s, 3, true, (uint16_t)((c->bg_y[3] / 16) & 0x1FF));
}

/* EndScreenVBlank */
static void end_screen_vblank(MzmCredits *c, MzmScreen *s) {
    write_common(c, s);
    for (int bg = 1; bg < 4; ++bg) write_bg(s, bg, false, c->bg_x[bg] & 0x1FF);
}

/* UnlockedOptionsVBlank */
static void unlocked_options_vblank(MzmCredits *c, MzmScreen *s) {
    write_common(c, s);
    gba_ppu_write16(s->ppu, GBA_REG_WIN0H,
                    (uint16_t)(c->data.oam_x[0] << 8 | (c->data.oam_x[1] & 0xFF)));
    gba_ppu_write16(s->ppu, GBA_REG_WIN0V,
                    (uint16_t)(c->data.oam_y[0] << 8 | (c->data.oam_y[1] & 0xFF)));
}

/* CreditsInit */
static void credits_init(MzmCredits *c, MzmScreen *s) {
    gba_ppu_write16(s->ppu, GBA_REG_DISPCNT, 0);
    s->ppu->win0h_per_line = false;
    memset(&c->data, 0, sizeof c->data);
    mzm_clear_gfx_ram(s);
    decompress(s, "sCreditsChozoWallBackgroundGfx", 0);
    decompress(s, "sCreditsChozoWallBackgroundTileTable", 0xE000);
    decompress(s, "sCreditsChozoWallBottomTileTable", 0xD800);
    decompress(s, "sCreditsCharactersGfx", 0x8000);
    copy_palette(s, "sCreditsChozoWallPal", 0);
    copy_palette(s, "sCreditsCharactersPal", 13 * 16);
    bgcnt(s, 0, 2, 30, 0, 0);
    bgcnt(s, 1, 2, 31, 1, 0);
    bgcnt(s, 2, 0, 28, 2, 2);
    bgcnt(s, 3, 2, 26, 3, 2);
    s->next_oam_slot = 0;
    mzm_reset_free_oam(s);
    memset(c->bg_x, 0, sizeof c->bg_x);
    memset(c->bg_y, 0, sizeof c->bg_y);
    c->bg_y[0] = c->bg_y[1] = 0x1000;
    write_bg(s, 0, false, 0);
    write_bg(s, 1, false, 4);
    write_bg(s, 2, false, 0);
    write_bg(s, 3, false, 0);
    c->data.unk_e = 0x80;
    c->data.dispcnt = GBA_DCNT_BG0 | GBA_DCNT_BG1 | GBA_DCNT_BG2 | GBA_DCNT_BG3;
    c->data.bldcnt = BLDCNT_BG2_FIRST | BLDCNT_BRIGHTNESS_DOWN;
    s->bldalpha_l = BLDALPHA_MAX;
    s->bldalpha_h = 0;
    s->bldy = BLDY_MAX;
    credits_vblank(c, s);
    mzm_sfx_music_play(MZM_MUSIC_CREDITS, 0);
}

/* CreditsDisplayLine: the line's tiles in sCreditsCharactersGfx (palette
 * row 13 white, 14 blue, 15 red); returns its height in rows (9 at the
 * end), with 0x80 for an odd width. */
static uint8_t credits_display_line(MzmCredits *c, uint32_t line) {
    MzmEndingData *d = &c->data;
    uint32_t entry = sym("sCredits") + line * CREDITS_ENTRY_SIZE;
    uint8_t type = mzm_rom_u8(entry), text[CREDITS_TEXT + 1];
    for (int i = 0; i < CREDITS_TEXT; ++i) text[i] = mzm_rom_u8(entry + 1u + (uint32_t)i);
    text[CREDITS_TEXT] = 0;
    memset(d->line_tilemap_1, 0, sizeof d->line_tilemap_1);
    memset(d->line_tilemap_2, 0, sizeof d->line_tilemap_2);
    uint16_t tile = type == LINE_BLUE ? 14 << 12 : type == LINE_RED ? 15 << 12 : 13 << 12;
    uint8_t ret_0 = 0, ret_1 = 0, length, offset;
    switch (type) {
    case LINE_BLUE:
    case LINE_RED:
    case LINE_WHITE_SMALL:
    case LINE_WHITE_BIG: {
        uint8_t limit = type == LINE_WHITE_BIG ? 30 : 30;
        for (length = 0; length < limit && text[length]; ++length) {}
        if (length & 1) {
            ret_1 = 0x80;
            length--;
        }
        offset = (uint8_t)((30 - length) >> 1);
        for (int i = 0; text[i] && i < CREDITS_TEXT; ++i, ++offset) {
            if (offset >= 32) continue;
            uint8_t ch = text[i];
            uint16_t *t1 = &d->line_tilemap_1[offset], *t2 = &d->line_tilemap_2[offset];
            if (type != LINE_WHITE_BIG) {
                if (ch >= 'A' && ch <= 'Z') *t1 = (uint16_t)(ch + tile - 0x40);
                else if (ch >= 'a' && ch <= 'z') *t1 = (uint16_t)(ch + tile - 0x41);
                else if (ch == '.') *t1 = (uint16_t)(tile + 0x1B);
                else if (ch == ',') *t1 = (uint16_t)(tile + 0x1C);
                else if (ch == '&') *t1 = (uint16_t)(tile + 0x1D);
            } else if (ch >= 'A' && ch <= 'Z') {
                *t1 = (uint16_t)(ch + tile - 1);
                *t2 = (uint16_t)(ch + tile + 0x1F);
            } else if (ch >= 'a' && ch <= 'z') {
                *t1 = (uint16_t)(ch + tile + 0x1F);
                *t2 = (uint16_t)(ch + tile + 0x3F);
            } else if (ch == '.') {
                *t2 = (uint16_t)(tile + 0x7B);
            } else if (ch == ',') {
                *t2 = (uint16_t)(tile + 0x7C);
            } else if (ch == '-') {
                *t1 = (uint16_t)(tile + 0x5A);
            } else if (ch == '+') {
                *t1 = (uint16_t)(tile + 0x9A);
                *t2 = (uint16_t)(tile + 0xBA);
            }
        }
        ret_0 = type == LINE_WHITE_BIG ? 2 : 1;
        break;
    }
    case LINE_END: ret_0 = 9; break;
    case LINE_ALL_RIGHTS:
    case LINE_THE_COPYRIGHT:
    case LINE_SCENARIO:
    case LINE_RESERVED: {
        static const uint16_t first[4] = {0xC0, 0xE0, 0x100, 0x120};
        for (int i = 0; i < 20; ++i)
            d->line_tilemap_1[i + 5] = (uint16_t)(first[type - LINE_ALL_RIGHTS] + i + tile);
        ret_0 = 1;
        if (type == LINE_ALL_RIGHTS) ret_1 = 0x10;
        break;
    }
    case LINE_BLANK: ret_0 = 1; break;
    default: break;
    }
    return (uint8_t)(ret_0 | ret_1);
}

/* CreditsDisplay: a line every 128 sixteenths scrolled, then the wall
 * fading in and the text fading out. */
static int credits_display(MzmCredits *c, MzmScreen *s) {
    MzmEndingData *d = &c->data;
    if (d->unk_1) {
        int ended = 0;
        switch (d->timer++) {
        case 464: d->unk_1++; break;                   /* CONVERT_SECONDS(7.f) + CONVERT_SECONDS(11.f / 15.f) */
        case 528:                                      /* CONVERT_SECONDS(8.8f) */
            d->bldcnt = BLDCNT_BG0_FIRST | BLDCNT_ALPHA | BLDCNT_BG2_SECOND;
            s->bldy = 0;
            d->unk_1++;
            break;
        case 560:                                      /* CONVERT_SECONDS(9.f) + ONE_THIRD_SECOND */
            d->dispcnt = GBA_DCNT_BG2 | GBA_DCNT_BG3;
            d->bldcnt = 0;
            ended = 1;
            break;
        default: break;
        }
        if (d->unk_1 == 2) {
            if ((d->end_screen_timer++ & 3) == 0 && s->bldy) s->bldy--;
        } else if (d->unk_1 == 3 && (d->timer & 1)) {
            if (s->bldalpha_l) {
                s->bldalpha_l--;
                s->bldalpha_h = (uint8_t)(BLDALPHA_MAX - s->bldalpha_l);
            } else {
                d->unk_1++;
            }
        }
        return ended;
    }
    if (d->unk_e > 0x7F) {
        d->unk_e &= 0x7F;
        if (d->unk_8 == d->unk_a) {
            uint8_t result = credits_display_line(c, d->current_line);
            uint8_t width = result & 0xF0, rows = result & 0xF;
            if (rows == 9) {
                d->unk_1 = 1;
            } else {
                uint32_t row = d->unk_a * 0x40u + 0x500u;
                if (row > 0x7FF) row &= 0x7FF;
                d->line_offset_1 = row + 0xF000;
                row = d->unk_a * 0x40u + 0x540u;
                if (row > 0x7FF) row &= 0x7FF;
                d->line_offset_2 = row + 0xF000;
                d->unk_a = (uint16_t)(d->unk_a + rows);
                d->current_line++;
                d->unk_6++;
                if (width == 0x80) d->unk_6++;
            }
        }
        d->unk_8++;
    }
    d->unk_e = (uint16_t)(d->unk_e + 7);
    c->bg_y[0] = (uint16_t)(c->bg_y[0] + 7);
    return 0;
}

/* CreditsChozoWallMovement: the wall scrolling down to its bottom. */
static int chozo_wall_movement(MzmCredits *c, MzmScreen *s) {
    MzmEndingData *d = &c->data;
    int ended = 0;
    switch (d->timer++) {
    case 0: decompress(s, "sCreditsChozoWallBottomGfx", 0x8000); break;
    case 1: d->unk_1++; break;
    case 512:                                          /* CONVERT_SECONDS(8.f) + CONVERT_SECONDS(8.f / 15.f) */
        write_bg(s, 1, false, 0);
        ended = 1;
        break;
    default: break;
    }
    if (d->unk_1) {
        c->bg_y[2] = (uint16_t)(c->bg_y[2] + 8);
        c->bg_y[3] = (uint16_t)(c->bg_y[3] + 8);
        if (c->bg_y[2] == 64 * 56) d->unk_1 = 0;
    }
    return ended;
}

/* CreditsChozoWallZoom: the zoomed wall bottom, then the Chozo drawing
 * faded in and out to black. */
static int chozo_wall_zoom(MzmCredits *c, MzmScreen *s) {
    MzmEndingData *d = &c->data;
    int ended = 0;
    switch (d->timer++) {
    case 0:
        decompress(s, "sCreditsChozoWallBottomZoomedGfx", 0);
        copy_palette(s, "sCreditsChozoWallPal", 0);
        c->bg_y[0] = 0;
        s->bldalpha_l = 0;
        s->bldalpha_h = 16;
        break;
    case 1:
        decompress(s, "sCreditsChozoWallBottomZoomedTileTable", 0xF000);
        bgcnt(s, 0, 0, 30, 0, 0);
        d->dispcnt = GBA_DCNT_BG0 | GBA_DCNT_BG2 | GBA_DCNT_BG3;
        d->bldcnt = BLDCNT_BG0_FIRST | BLDCNT_ALPHA | BLDCNT_BG2_SECOND | BLDCNT_BG3_SECOND;
        d->unk_1++;
        break;
    case 192:                                          /* CONVERT_SECONDS(3.2f) */
        d->dispcnt = GBA_DCNT_BG0;
        d->bldcnt = 0;
        d->unk_1++;
        c->bg_y[1] = 0;
        s->bldalpha_l = 16;
        s->bldalpha_h = 0;
        break;
    case 193: decompress(s, "sCreditsChozoDrawingGfx", 0x8000); break;
    case 194:
        decompress(s, "sCreditsChozoDrawingTileTable", 0xF800);
        d->dispcnt = GBA_DCNT_BG0 | GBA_DCNT_BG1;
        d->bldcnt = BLDCNT_BG0_FIRST | BLDCNT_ALPHA | BLDCNT_BG1_SECOND;
        d->unk_1++;
        break;
    case 800:                                          /* CONVERT_SECONDS(13.f) + ONE_THIRD_SECOND */
        d->dispcnt = GBA_DCNT_BG1;
        d->bldcnt = BLDCNT_SCREEN_FIRST | BLDCNT_BRIGHTNESS_DOWN;
        d->unk_1++;
        break;
    case 960: ended = 2; break;                        /* CONVERT_SECONDS(16.f) */
    default: break;
    }
    if ((d->timer & 7) == 0) {
        if (d->unk_1 == 1) {
            if (s->bldalpha_l < BLDALPHA_MAX) s->bldalpha_l++;
            if (s->bldalpha_h) s->bldalpha_h--;
        } else if (d->unk_1 == 3) {
            if (s->bldalpha_l) s->bldalpha_l--;
            if (s->bldalpha_h < BLDALPHA_MAX) s->bldalpha_h++;
        } else if (d->unk_1 == 4) {
            if (s->bldy < BLDY_MAX) s->bldy++;
        }
    }
    return ended;
}

/* EndScreenInit */
static void end_screen_init(MzmCredits *c, MzmScreen *s) {
    gba_ppu_write16(s->ppu, GBA_REG_DISPCNT, 0);
    decompress(s, "sEndingSamusPosingSpaceBackgroundGfx", 0x3000);
    decompress(s, "sEndingLightGfx", 0xB000);
    decompress(s, "sEndingSamusPosingGfx_1", 0);
    decompress(s, "sEndingSamusPosingGfx_2", 0x8000);
    decompress(s, "sEndingSamusPosingSpaceBackgroundTileTable", 0xF800);
    decompress(s, "sEndingLightTileTable", 0xF000);
    decompress(s, "sEndingSamusPosingTileTable_1", 0xD000);
    decompress(s, "sEndingSamusPosingTileTable_2", 0xE000);
    memset(s->ppu->vram + 0xD800, 0, 0x800);
    memset(s->ppu->vram + 0xE800, 0, 0x800);
    copy_palette(s, "sEndingPosingPal", 0);
    bgcnt(s, 0, 2, 30, 0, 0);
    bgcnt(s, 1, 0, 26, 1, 1);
    bgcnt(s, 2, 2, 28, 2, 1);
    bgcnt(s, 3, 0, 31, 3, 0);
    s->next_oam_slot = 0;
    mzm_reset_free_oam(s);
    mzm_screen_copy_oam(s);
    memset(c->bg_x, 0, sizeof c->bg_x);
    memset(c->bg_y, 0, sizeof c->bg_y);
    c->bg_x[1] = c->bg_x[2] = 0x100;
    for (int bg = 0; bg < 4; ++bg) {
        write_bg(s, bg, false, c->bg_x[bg]);
        write_bg(s, bg, true, 0);
    }
    memset(&c->data, 0, sizeof c->data);
    c->data.ending_number = (uint8_t)(c->pen & 0xF & 7);
    c->data.dispcnt = GBA_DCNT_BG1 | GBA_DCNT_BG2 | GBA_DCNT_BG3 | GBA_DCNT_OBJ;
    s->bldalpha_l = BLDALPHA_MAX;
    s->bldalpha_h = 0;
    s->bldy = BLDY_MAX;
    end_screen_vblank(c, s);
}

/* The pose swaps of EndScreenSamusPosing: BG1 and BG2 trade priorities
 * and one of them takes the next pose. */
static void load_pose(MzmCredits *c, MzmScreen *s, bool bg1_front, const char *gfx, const char *tiles) {
    bgcnt(s, 1, 0, 26, bg1_front ? 1 : 2, 1);
    bgcnt(s, 2, 2, 28, bg1_front ? 2 : 1, 1);
    if (gfx) {
        decompress(s, gfx, bg1_front ? 0x8000 : 0);
        decompress(s, tiles, bg1_front ? 0xE000 : 0xD000);
    }
    c->data.bldcnt = 0;
    s->bldalpha_l = BLDALPHA_MAX;
    s->bldalpha_h = 0;
}

/* EndScreenSamusPosing: Samus's poses sliding in and blending one into
 * the next, the last one by ending (full suit, helmet off or suitless),
 * with the space background drifting. */
static int end_screen_samus_posing(MzmCredits *c, MzmScreen *s) {
    MzmEndingData *d = &c->data;
    int ended = 0;
    d->end_screen_timer++;
    switch (d->unk_1) {
    case 4:
        if (d->timer++ <= 7) break;
        /* fallthrough */
    case 0:
        c->bg_x[1] = (uint16_t)(c->bg_x[1] + 8);
        if (c->bg_x[1] >= 64 * 8) {
            c->bg_x[1] = 0;
            d->bldcnt = BLDCNT_BG1_FIRST | BLDCNT_ALPHA | BLDCNT_BG2_SECOND | BLDCNT_BG3_SECOND;
            d->timer = 0;
            d->unk_1++;
        }
        break;
    case 1:
    case 3:
    case 5:
        if (d->timer++ > 24) {
            d->oam_types[0] = 1;
            d->timer = 0;
            d->unk_1++;
        }
        break;
    case 2:
    case 6:
        if (d->timer++ > 7) {
            c->bg_x[2] = (uint16_t)(c->bg_x[2] + 8);
            if (c->bg_x[2] >= 64 * 8) {
                c->bg_x[2] = 64 * 8;
                d->bldcnt = BLDCNT_BG2_FIRST | BLDCNT_ALPHA | BLDCNT_BG1_SECOND | BLDCNT_BG3_SECOND;
                d->timer = 0;
                d->unk_1++;
            }
        }
        break;
    case 7:
        if (d->timer++ > 64) {
            d->oam_types[0] = 1;
            d->timer = 0;
            d->unk_1++;
        }
        break;
    case 8:
        if (d->timer++ > 23) {
            if (c->bg_x[1] >= 10) {
                c->bg_x[1] = (uint16_t)(c->bg_x[1] - 10);
            } else {
                c->bg_x[1] = 0;
                d->timer = 0;
                d->unk_1++;
            }
        }
        break;
    case 9:
        if (d->timer++ > 31) {
            d->dispcnt = GBA_DCNT_BG1 | GBA_DCNT_BG2 | GBA_DCNT_BG3;
            if (d->ending_number != 0) {
                d->oam_types[0] = 3;
            } else {
                d->bldcnt = BLDCNT_BG1_FIRST | BLDCNT_ALPHA | BLDCNT_BG2_SECOND | BLDCNT_BG3_SECOND;
                d->oam_types[0] = 1;
            }
            d->timer = 0;
            d->unk_1++;
        }
        break;
    case 10:
        if (d->timer++ > 288) d->oam_types[1] = 32;
        break;
    default: break;
    }
    switch (d->oam_types[0]) {
    case 1:
        if (d->end_screen_timer & 1) {
            if (s->bldalpha_l) {
                s->bldalpha_l--;
            } else {
                d->oam_types[0] = 0;
                d->oam_types[1]++;
            }
            s->bldalpha_h = (uint8_t)(BLDALPHA_MAX - s->bldalpha_l);
        }
        break;
    case 2:
        if (d->end_screen_timer & 3) break;
        if (s->bldalpha_l < BLDALPHA_MAX) {
            s->bldalpha_l++;
        } else {
            d->oam_types[0] = 0;
            d->oam_types[1]++;
        }
        break;
    case 3: {
        uint8_t stage = ++d->pose_palette_timer;
        if (stage >= 16) {
            d->pose_palette_timer = 0;
            d->oam_types[1] = 16;
            break;
        }
        /* sEndingWhitePalPointers: the poses whitening. */
        copy_colors(s, mzm_rom_u32(sym("sEndingWhitePalPointers") + stage / 4u * 4u), 5 * 16);
        break;
    }
    case 4:
        if ((d->end_screen_timer & 1) == 0) break;
        if (s->bldy < BLDY_MAX) s->bldy++;
        else d->oam_types[1]++;
        break;
    case 5:
        if (d->end_screen_timer & 3) break;
        if (s->bldy) {
            s->bldy--;
        } else {
            d->oam_types[0] = 0;
            d->oam_types[1]++;
        }
        break;
    default: break;
    }
    switch (d->oam_types[1]) {
    case 1:
        load_pose(c, s, false, "sEndingSamusPosingGfx_3", "sEndingSamusPosingTileTable_3");
        c->bg_x[1] = 64 * 4;
        d->oam_types[1]++;
        break;
    case 3:
        load_pose(c, s, true, "sEndingSamusPosingGfx_4", "sEndingSamusPosingTileTable_4");
        c->bg_x[2] = 64 * 4;
        d->oam_types[1]++;
        break;
    case 5:
        load_pose(c, s, false, "sEndingSamusPosingGfx_5", "sEndingSamusPosingTileTable_5");
        c->bg_x[1] = 64 * 4;
        d->oam_types[1]++;
        break;
    case 7:
        if (d->ending_number == 0)
            load_pose(c, s, true, "sEndingSamusPosingFullSuitGfx", "sEndingSamusPosingFullSuitTileTable");
        else if (d->ending_number == 1)
            load_pose(c, s, true, "sEndingSamusPosingHelmetOffGfx", "sEndingSamusPosingHelmetOffTileTable");
        else
            load_pose(c, s, true, "sEndingSamusPosingSuitlessGfx", "sEndingSamusPosingSuitlessTileTable");
        d->dispcnt = GBA_DCNT_BG1 | GBA_DCNT_BG3;
        c->bg_x[2] = 0;
        d->oam_types[1]++;
        break;
    case 9:
        d->dispcnt = GBA_DCNT_BG0 | GBA_DCNT_BG2 | GBA_DCNT_BG3;
        d->bldcnt = BLDCNT_BG0_FIRST | BLDCNT_ALPHA | BLDCNT_BG2_SECOND | BLDCNT_BG3_SECOND;
        s->bldalpha_l = 0;
        s->bldalpha_h = BLDALPHA_MAX;
        d->oam_types[0] = 2;
        d->oam_types[1]++;
        break;
    case 16:
        d->dispcnt = GBA_DCNT_BG1 | GBA_DCNT_BG2 | GBA_DCNT_BG3;
        d->bldcnt = BLDCNT_BG0_FIRST | BLDCNT_BG2_FIRST | BLDCNT_BG3_FIRST | BLDCNT_OBJ_FIRST |
                    BLDCNT_BACKDROP_FIRST | BLDCNT_BRIGHTNESS_UP;
        d->oam_types[0]++;
        d->oam_types[1]++;
        break;
    case 18:
        d->dispcnt = GBA_DCNT_BG2 | GBA_DCNT_BG3;
        d->bldcnt = BLDCNT_SCREEN_FIRST | BLDCNT_BRIGHTNESS_UP;
        d->oam_types[0]++;
        d->oam_types[1]++;
        break;
    case 19:
        copy_colors(s, sym("sEndingPosingPal"), 0x50);
        d->oam_types[1]++;
        break;
    case 32:
        d->dispcnt = GBA_DCNT_BG2 | GBA_DCNT_BG3;
        d->bldcnt = BLDCNT_SCREEN_FIRST | BLDCNT_BRIGHTNESS_DOWN;
        s->bldalpha_l = BLDALPHA_MAX;
        s->bldalpha_h = 0;
        ended++;
        break;
    default: break;
    }
    if ((d->end_screen_timer & 7) == 0) c->bg_x[3]++;
    return ended;
}

/* The US ROM's struct EndingImageOam entries are twelve bytes. */
static void ending_load_oam_entry(MzmEndingData *d, int slot, uint32_t address) {
    if (slot < 0 || slot >= MZM_ENDING_OAM) return;
    d->oam_types[slot] = mzm_rom_u8(address);
    d->ending_letters_spawn_delay[slot] = mzm_rom_u8(address + 1u);
    d->letter_palette_timer[slot] = mzm_rom_u8(address + 2u);
    d->oam_x[slot] = mzm_rom_u16(address + 4u);
    d->oam_y[slot] = mzm_rom_u16(address + 6u);
    d->oam_frame[slot] = mzm_rom_u32(address + 8u);
}

/* EndingImageLoadTextOam, using the English tables of the pinned US ROM. */
static void ending_load_text_oam(MzmEndingData *d, const char *name, int count) {
    uint32_t address = sym(name);
    if (!address || count + ENDING_LINE_COUNT > MZM_ENDING_OAM) return;
    d->oam_length = (uint8_t)(count + ENDING_LINE_COUNT);
    for (int i = 0; i < count; ++i)
        ending_load_oam_entry(d, i + ENDING_LINE_COUNT, address + (uint32_t)i * 12u);
}

/* EndingImageDisplayLinePermanently. */
static void ending_display_line_permanently(MzmEndingData *d, int line) {
    uint32_t address = sym("sEndingImageOam_FullLines_English");
    if (!address || line < 0 || line >= ENDING_LINE_COUNT) return;
    ending_load_oam_entry(d, line, address + (uint32_t)line * 12u);
    for (int i = ENDING_LINE_COUNT; i < d->oam_length; ++i) d->oam_types[i] = ENDING_OAM_NONE;
    d->oam_length = ENDING_LINE_COUNT;
}

static void ending_copy_digit(MzmScreen *s, int digit, uint32_t upper_offset) {
    if (digit < 0 || digit > 9) return;
    uint32_t source = (uint32_t)digit * 64u;
    mzm_copy_from_rom(s->ppu->vram + GBA_OBJ_VRAM + upper_offset,
                      sym("sEndingImageNumbersGfx_Upper") + source, 64);
    mzm_copy_from_rom(s->ppu->vram + GBA_OBJ_VRAM + upper_offset + 0x400u,
                      sym("sEndingImageNumbersGfx_Lower") + source, 64);
}

/* EndingImageLoadIGTAndPercentageGraphics. */
static void ending_load_numbers(MzmCredits *c, MzmScreen *s) {
    int hours = c->igt_hours > 99 ? 99 : c->igt_hours;
    int minutes = c->igt_minutes > 59 ? 59 : c->igt_minutes;
    int seconds = c->igt_seconds > 59 ? 59 : c->igt_seconds;
    int percentage = c->data.completion_percentage;
    if (hours / 10) ending_copy_digit(s, hours / 10, 0x000);
    ending_copy_digit(s, hours % 10, 0x040);
    ending_copy_digit(s, minutes / 10, 0x0A0);
    ending_copy_digit(s, minutes % 10, 0x0E0);
    ending_copy_digit(s, seconds / 10, 0x140);
    ending_copy_digit(s, seconds % 10, 0x180);
    if (percentage / 100) ending_copy_digit(s, percentage / 100, 0x1C0);
    if (percentage >= 10) ending_copy_digit(s, percentage / 10 % 10, 0x200);
    ending_copy_digit(s, percentage % 10, 0x240);
}

/* EndingImageInit. */
static void ending_image_init(MzmCredits *c, MzmScreen *s) {
    MzmEndingData *d = &c->data;
    uint32_t pen = c->pen;
    uint8_t ending = (uint8_t)(pen & 7u);
    memset(d, 0, sizeof *d);
    d->ending_number = ending;
    d->completion_percentage = (uint8_t)(((pen >> 24) & 0xFFu) + ((pen >> 16) & 0xFFu) +
                                         ((pen >> 12) & 0xFu) + ((pen >> 8) & 0xFu) +
                                         ((pen >> 4) & 0xFu));
    d->language = LANGUAGE_ENGLISH;
    decompress_address(s, mzm_rom_u32(sym("sEndingImagesTopGfxPointers") + ending * 4u), 0);
    decompress_address(s, mzm_rom_u32(sym("sEndingImagesBottomGfxPointers") + ending * 4u), 0x8000);
    decompress_address(s, mzm_rom_u32(sym("sEndingImagesTopTileTablePointers") + ending * 4u), 0xE000);
    decompress_address(s, mzm_rom_u32(sym("sEndingImagesHalfTileTablePointers") + ending * 4u), 0xF800);
    for (uint32_t i = 0; i < 0x800; i += 2) {
        s->ppu->vram[0xE800 + i] = 0xFF;
        s->ppu->vram[0xE800 + i + 1] = 0x04;
    }
    copy_palette_address(s, mzm_rom_u32(sym("sEndingImagesPalPointers") + ending * 4u), 0, 256);
    decompress(s, "sEndingImageNumbersMiscEnglishGfx", GBA_OBJ_VRAM);
    decompress(s, "sEndingImageTextEnglishGfx", GBA_OBJ_VRAM + 0x1000);
    copy_palette(s, "sEndingImageTextPal", 256);
    ending_load_numbers(c, s);
    bgcnt(s, 0, 0, 28, 0, 1);
    bgcnt(s, 1, 2, 30, 1, 1);
    s->next_oam_slot = 0;
    mzm_reset_free_oam(s);
    memset(c->bg_x, 0, sizeof c->bg_x);
    memset(c->bg_y, 0, sizeof c->bg_y);
    c->bg_y[0] = c->bg_y[1] = 0x1000;
    d->dispcnt = GBA_DCNT_OBJ | GBA_DCNT_BG0 | GBA_DCNT_BG1;
    d->bldcnt = BLDCNT_SCREEN_FIRST | BLDCNT_BRIGHTNESS_DOWN;
    s->bldalpha_l = BLDALPHA_MAX;
    s->bldalpha_h = 0;
    s->bldy = BLDY_MAX;
    credits_vblank(c, s);
}

static void ending_update_letter_delay(MzmEndingData *d, int slot) {
    if (d->oam_types[slot] == ENDING_OAM_NONE) return;
    if (d->ending_letters_spawn_delay[slot] &&
        --d->ending_letters_spawn_delay[slot] == 0)
        d->oam_types[slot]++;
}

static int ending_draw_frame(MzmScreen *s, int slot, uint32_t frame, uint16_t x, uint16_t y,
                             uint8_t palette) {
    int count = mzm_rom_u16(frame) & 0xFF;
    if (slot + count > 0x7F) count = 0x7F - slot;
    uint32_t source = frame + 2u;
    for (int i = 0; i < count; ++i, source += 6u) {
        uint16_t a0 = mzm_rom_u16(source);
        uint16_t a1 = mzm_rom_u16(source + 2u);
        uint16_t a2 = mzm_rom_u16(source + 4u);
        uint16_t *target = &s->oam[(slot + i) * 4];
        target[0] = (uint16_t)((a0 & 0xFF00u) | ((a0 + y) & 0xFFu));
        target[1] = (uint16_t)((a1 & 0xFE00u) | ((a1 + x) & 0x1FFu));
        target[2] = (uint16_t)((a2 & 0x0FFFu) | (uint16_t)palette << 12);
    }
    return slot + count;
}

/* EndingImageDisplayText. */
static void ending_display_text(MzmEndingData *d, MzmScreen *s) {
    int slot = 0;
    for (int i = 0; i < d->oam_length && i < MZM_ENDING_OAM; ++i) {
        if (d->oam_types[i] == ENDING_OAM_NONE || d->oam_types[i] == ENDING_OAM_CHAR) continue;
        if (d->letter_palette_timer[i] < 64) d->letter_palette_timer[i]++;
        uint8_t palette = 0;
        if (i > 5) {
            if (d->letter_palette_timer[i] < 4) palette = 11;
            else if (d->letter_palette_timer[i] < 8) palette = 12;
            else if (d->letter_palette_timer[i] < 12) palette = 13;
        } else if (i == 5) {
            d->letter_palette_timer[i] &= 0x3F;
            palette = mzm_rom_u8(sym("sEndingImage_54e2dc") + d->letter_palette_timer[i] / 8u);
        }
        slot = ending_draw_frame(s, slot, d->oam_frame[i], d->oam_x[i], d->oam_y[i], palette);
    }
    if (d->has_new_record) {
        if (d->new_record_palette_timer++ > 70) d->new_record_palette_timer = 0;
        uint8_t palette = mzm_rom_u8(sym("sEndingImageNewRecordPalettes") +
                                     d->new_record_palette_timer / 6u);
        slot = ending_draw_frame(s, slot, sym("sEndingImageOam_NewRecordEnglish"),
                                 48, 59, palette);
    }
    s->next_oam_slot = (uint8_t)slot;
}

/* EndingImageDisplay. */
static bool ending_image_display(MzmCredits *c, MzmScreen *s) {
    MzmEndingData *d = &c->data;
    bool ended = false;
    switch (d->timer++) {
    case 0: d->unk_8++; break;
    case 30:
        ending_load_text_oam(d, "sEndingImageOam_ClearTime_English", 9);
        d->unk_1 = 1;
        break;
    case 110: ending_display_line_permanently(d, ENDING_LINE_CLEAR_TIME); break;
    case 180:
        ending_display_line_permanently(d, ENDING_LINE_TIMER);
        if (c->ending_flags & MZM_ENDING_FLAG_NEW_TIME_ATTACK_RECORD) d->has_new_record++;
        break;
    case 330: ending_load_text_oam(d, "sEndingImageOam_YourRate_English", 11); break;
    case 375: ending_display_line_permanently(d, ENDING_LINE_YOUR_RATE); break;
    case 380: ending_load_text_oam(d, "sEndingImageOam_Collecting_English", 17); break;
    case 460: ending_display_line_permanently(d, ENDING_LINE_COLLECTING); break;
    case 530: ending_display_line_permanently(d, ENDING_LINE_PERCENTAGE); break;
    case 780: ending_display_line_permanently(d, ENDING_LINE_NEXT_MISSION); break;
    case 1376:
        if (s->changed_input & (GBA_KEY_A | GBA_KEY_B | GBA_KEY_START))
            mzm_sfx_music_fade(256);
        else
            d->timer--;
        break;
    case 1664: ended = true; break;
    default: break;
    }
    if (d->unk_8) {
        if (c->bg_y[0] > 6) {
            c->bg_y[0] = (uint16_t)(c->bg_y[0] - 6);
            c->bg_y[1] = (uint16_t)(c->bg_y[1] - 6);
        } else {
            c->bg_y[0] = c->bg_y[1] = 0;
            d->unk_8 = 0;
        }
    }
    if (d->timer > 1380 || (d->timer >= 810 && s->button_input & (GBA_KEY_R | GBA_KEY_L)))
        return ended;
    if (d->unk_1)
        for (int i = ENDING_LINE_COUNT; i < d->oam_length; ++i) ending_update_letter_delay(d, i);
    ending_display_text(d, s);
    return ended;
}

static uint32_t popup_text_address(uint8_t message) {
    uint32_t pointers = sym("sEnglishTextPointers_FileScreen");
    return pointers ? mzm_rom_u32(pointers + (uint32_t)message * 4u) : 0;
}

/* TextStartFileScreen and TextProcessFileScreenPopUp, for the English ROM. */
static void popup_start_text(MzmCredits *c, uint8_t message) {
    c->popup_message = mzm_text_message_from_rom(sym("sMessageFileScreen_Empty"));
    c->popup_message.message_id = message;
    c->popup_text = popup_text_address(message);
}

static uint8_t popup_process_text(MzmCredits *c, MzmScreen *s) {
    MzmMessage *m = &c->popup_message;
    if (m->stage == 0) {
        if (m->line > 3 || m->message_ended) {
            m->stage = 1;
            return 0;
        }
        uint8_t *dst = s->ppu->vram + (uint32_t)m->line * 0x800u;
        for (int i = 0; i < 8; ++i) {
            int state = mzm_text_process_message(m, c->popup_text, dst, s->button_input);
            if (state == MZM_TEXT_ENDED) {
                m->stage++;
                m->indent = 0;
                break;
            }
            if (state == MZM_TEXT_NEW_LINE || state == MZM_TEXT_NEW_PAGE) {
                m->indent = 0;
                break;
            }
            if (m->line > 3) break;
        }
        return 0;
    }
    if (m->stage == 1) {
        m->line++;
        m->stage++;
    }
    return m->line;
}

/* UnlockedOptionsInit. */
static void unlocked_options_init(MzmCredits *c, MzmScreen *s) {
    MzmEndingData *d = &c->data;
    gba_ppu_write16(s->ppu, GBA_REG_DISPCNT, 0);
    memset(d, 0, sizeof *d);
    mzm_clear_gfx_ram(s);
    decompress(s, "sUnlockedOptionsTileTable", 0x8000);
    memset(s->ppu->vram + 0x7FE0, 0xFF, 0x20);
    for (uint32_t i = 0; i < 0x800; i += 4) {
        s->ppu->vram[0x8800 + i] = 0xFF;
        s->ppu->vram[0x8800 + i + 1] = 0xF3;
        s->ppu->vram[0x8800 + i + 2] = 0xFF;
        s->ppu->vram[0x8800 + i + 3] = 0xF3;
    }
    copy_palette(s, "sUnlockedOptionsPal", 15 * 16);
    bgcnt(s, 0, 0, 16, 0, 0);
    bgcnt(s, 1, 0, 17, 1, 0);
    s->next_oam_slot = 0;
    mzm_reset_free_oam(s);
    write_bg(s, 0, false, 0);
    write_bg(s, 0, true, 0);
    d->dispcnt = 0;
    d->bldcnt = 0;
    s->bldalpha_l = BLDALPHA_MAX;
    s->bldalpha_h = 0;
    s->bldy = BLDY_MAX;
    unlocked_options_vblank(c, s);
}

/* UnlockedOptionsPopUp: 0 while running, 2 once the supported pop-up ends.
 * The bundled NES game is deliberately outside this runtime's scope, so a
 * first clear only announces the gallery. */
static uint8_t unlocked_options_popup(MzmCredits *c, MzmScreen *s) {
    MzmEndingData *d = &c->data;
    uint8_t ended = 0;
    switch (d->timer++) {
    case 0: {
        memset(s->ppu->vram, 0, 0x2000);
        uint8_t message = c->ending_flags & MZM_ENDING_FLAG_FIRST_CLEAR ? FILE_TEXT_GALLERY_UNLOCK :
                          c->ending_flags & MZM_ENDING_FLAG_FIRST_HARD_MODE_CLEAR ? FILE_TEXT_SOUND_TEST_UNLOCK :
                          FILE_TEXT_TIME_ATTACK_RECORD_UNLOCK;
        popup_start_text(c, message);
        d->oam_types[0] = 0;
        break;
    }
    case 32:
        d->dispcnt = GBA_DCNT_BG1 | GBA_DCNT_WIN0;
        gba_ppu_write16(s->ppu, GBA_REG_WININ, 3);
        gba_ppu_write16(s->ppu, GBA_REG_WINOUT, 0);
        d->unk_1++;
        d->oam_x[0] = d->oam_x[1] = GBA_SCREEN_W / 2;
        d->oam_y[0] = d->oam_y[1] = GBA_SCREEN_H / 2;
        break;
    case 128:
        d->timer--;
        break;
    default:
        break;
    }
    if (d->oam_types[0] == 0) d->oam_types[0] = popup_process_text(c, s);
    if (d->unk_1 == 1) {
        if (d->oam_x[0] > 0x30) {
            d->oam_x[0] -= 8;
            d->oam_x[1] += 8;
            d->oam_y[0] -= 4;
            d->oam_y[1] += 4;
        } else {
            d->dispcnt = GBA_DCNT_BG0 | GBA_DCNT_BG1 | GBA_DCNT_WIN0;
            d->oam_x[0] = 0x2C;
            d->oam_x[1] = 0xBC;
            d->oam_y[0] = 0x2C;
            d->oam_y[1] = 0x74;
            d->unk_1++;
        }
    } else if (d->unk_1 == 2) {
        if (s->changed_input & GBA_KEY_A) {
            d->dispcnt = GBA_DCNT_BG1 | GBA_DCNT_WIN0;
            d->unk_1++;
            d->timer++;
        }
    } else if (d->unk_1 == 3) {
        if (d->oam_x[0] < 0x70) {
            d->oam_x[0] += 8;
            d->oam_x[1] -= 8;
            d->oam_y[0] += 4;
            d->oam_y[1] -= 4;
        } else {
            d->dispcnt = 0;
            ended = 2;
        }
    }
    return ended;
}

bool mzm_credits_enter(MzmCredits *c, MzmScreen *s, uint32_t pen) {
    memset(c, 0, sizeof *c);
    c->pen = pen;
    return s && s->ppu && sym("sCredits") != 0;
}

void mzm_credits_set_time(MzmCredits *c, uint8_t hours, uint8_t minutes, uint8_t seconds) {
    c->igt_hours = hours;
    c->igt_minutes = minutes;
    c->igt_seconds = seconds;
}

void mzm_credits_set_ending_flags(MzmCredits *c, uint8_t flags) { c->ending_flags = flags; }

/* CreditsHandler. */
bool mzm_credits_update(MzmCredits *c, MzmScreen *s) {
    MzmEndingData *d = &c->data;
    d->unk_6 = 0;
    s->next_oam_slot = 0;
    bool end_screen = c->sub_mode >= MODE_END_SCREEN_INIT && c->sub_mode < MODE_ENDING_IMAGE_INIT;
    bool finished = false;
    switch (c->sub_mode) {
    case 0:
        credits_init(c, s);
        c->sub_mode++;
        return false;
    case MODE_CREDITS: {
        int result = d->stage == 0 ? credits_display(c, s) : d->stage == 1 ? chozo_wall_movement(c, s)
                                                                           : chozo_wall_zoom(c, s);
        if (result) {
            d->stage++;
            d->unk_1 = 0;
            d->end_screen_timer = 0;
            d->timer = 0;
            if (result > 1) c->sub_mode++;
        }
        mzm_reset_free_oam(s);
        break;
    }
    case 7:
        if (s->bldy < BLDY_MAX) s->bldy++;
        else c->sub_mode++;
        break;
    case MODE_END_SCREEN_INIT:
        end_screen_init(c, s);
        c->sub_mode++;
        return false;
    case 5:
        if (s->bldy) {
            s->bldy--;
        } else {
            d->bldcnt = 0;
            c->sub_mode++;
        }
        break;
    case MODE_END_SCREEN:
        if (end_screen_samus_posing(c, s)) c->sub_mode++;
        break;
    case MODE_ENDING_IMAGE_INIT:
        ending_image_init(c, s);
        c->sub_mode++;
        return false;
    case MODE_ENDING_IMAGE_FADE_IN:
        if (s->bldy) {
            s->bldy--;
        } else {
            d->bldcnt = 0;
            c->sub_mode++;
        }
        break;
    case MODE_ENDING_IMAGE:
        if (ending_image_display(c, s)) {
            if (c->ending_flags & POPUP_FLAGS) {
                d->bldcnt = BLDCNT_SCREEN_FIRST | BLDCNT_BRIGHTNESS_DOWN;
                s->bldy = 0;
                c->sub_mode++;
            } else {
                finished = true;
            }
        }
        mzm_reset_free_oam(s);
        break;
    case MODE_UNLOCKED_FADE_OUT:
        if (s->bldy < BLDY_MAX) s->bldy++;
        else c->sub_mode++;
        break;
    case MODE_UNLOCKED_INIT:
        unlocked_options_init(c, s);
        c->sub_mode++;
        return false;
    case MODE_UNLOCKED_FADE_IN:
        if (s->bldy) {
            s->bldy--;
        } else {
            d->bldcnt = 0;
            c->sub_mode++;
        }
        break;
    case MODE_UNLOCKED_POP_UP: {
        uint8_t result = unlocked_options_popup(c, s);
        if (result) {
            c->ending_flags = 0;
            d->stage++;
            d->unk_1 = 0;
            d->end_screen_timer = 0;
            d->timer = 0;
            if (result > 1) c->sub_mode++;
        }
        mzm_reset_free_oam(s);
        break;
    }
    case MODE_UNLOCKED_END:
        finished = true;
        break;
    default:
        c->sub_mode++;                                 /* 1, 3 */
        break;
    }
    if (c->sub_mode >= MODE_UNLOCKED_INIT) unlocked_options_vblank(c, s);
    else if (end_screen) end_screen_vblank(c, s);
    else credits_vblank(c, s);
    return finished;
}
