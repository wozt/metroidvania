/* SPDX-License-Identifier: GPL-3.0-only */
/* The game over screen (see mzm_game_over.h); comments name the pinned
 * decompilation's routines. */
#include "mzm_game_over.h"

#include <string.h>

#include "mzm_rom.h"
#include "mzm_sfx.h"

enum {
    /* GameOverCursorAction */
    CURSOR_LOADING = 0, CURSOR_MOVING, CURSOR_SELECTING, CURSOR_ACTIONS,
    LANGUAGE_ENGLISH = 2,
    MUSIC_GAME_OVER = 0x53,
    /* sMenuSounds: MENU_SOUND_REQUEST_GAME_OVER_MENU_CURSOR / _START_GAME */
    SOUND_REQUEST_GAME_OVER_MENU_CURSOR = 11, SOUND_REQUEST_GAME_OVER_START_GAME = 12,
    KEY_A = 1 << 0, KEY_START = 1 << 3, KEY_UP = 1 << 6, KEY_DOWN = 1 << 7,
    BLDCNT_SCREEN_FIRST = 0x3F, BLDCNT_BG2_FIRST = 1 << 2, BLDCNT_BRIGHTNESS_UP = 2 << 6,
    BLDCNT_BRIGHTNESS_DOWN = 3 << 6,
};

static uint32_t sym(const char *name) { return mzm_rom_symbol(name, NULL); }

static void vram_lz77(MzmScreen *s, const char *name, uint32_t offset) {
    mzm_lz77_from_rom(sym(name), s->ppu->vram + offset, GBA_VRAM_SIZE - offset);
}

/* FileSelectPlayMenuSound */
static void play_menu_sound(int request) { mzm_sfx_play(mzm_rom_u16(sym("sMenuSounds") + (uint32_t)request * 2u)); }

/* GameOverUpdateTextGfx: the selected option's rows take palette 2, the
 * other palette 3. */
static void update_text_gfx(MzmGameOver *g, MzmScreen *s) {
    uint16_t *dst = (uint16_t *)(s->ppu->vram + 0x340);
    uint16_t palette = g->option_selected == 0 ? 2 : 3;
    for (int i = 0; i < 64; ++i, ++dst) *dst = (uint16_t)((*dst & 0xFFF) | palette << 12);
    palette ^= 1;
    dst += 32;
    for (int i = 0; i < 64; ++i, ++dst) *dst = (uint16_t)((*dst & 0xFFF) | palette << 12);
}

/* GameOverUpdateSamusHead */
static void update_samus_head(MzmGameOver *g, int action) {
    uint8_t suit = g->suit < 3 ? g->suit : 0;
    mzm_menu_oam_set_id(&g->oam[0], mzm_rom_u8(sym("sGameOverSamusHeadOamIds") + suit * CURSOR_ACTIONS + (uint32_t)action));
    g->oam[0].x = (int16_t)mzm_rom_u16(sym("sGameOverSamusHeadXPositions") + LANGUAGE_ENGLISH * 2u);
    g->oam[0].y = (int16_t)mzm_rom_u16(sym("sGameOverSamusHeadYPositions") + g->option_selected * 2u);
    g->win1v = g->option_selected ? (uint16_t)(126 << 8 | (145 + 1)) : (uint16_t)(102 << 8 | (121 + 1));
    g->win1h = 0 << 8 | (239 + 1);
}

/* GameOverInit */
static void init(MzmGameOver *g, MzmScreen *s) {
    GbaPpu *ppu = s->ppu;
    uint8_t suit = g->suit;
    memset(g, 0, sizeof *g);
    g->suit = suit;
    g->bldcnt = BLDCNT_SCREEN_FIRST | BLDCNT_BRIGHTNESS_UP;
    gba_ppu_write16(ppu, GBA_REG_BLDCNT, g->bldcnt);
    s->bldy = 16;
    gba_ppu_write16(ppu, GBA_REG_BLDY, 16);
    gba_ppu_write16(ppu, GBA_REG_DISPCNT, 0);
    s->next_oam_slot = 0;
    mzm_clear_gfx_ram(s);
    mzm_reset_free_oam(s);
    s->oam_x_offset = s->oam_y_offset = 0;
    mzm_sfx_stop_all();
    mzm_copy_from_rom(&ppu->palette[0], sym("sGameOverMenuPal"), 160);
    ppu->palette[0] = 0;
    mzm_copy_from_rom(&ppu->palette[256], sym("sFileSelectIconsPal"), 192);
    vram_lz77(s, "sGameOverBackgroundTileTable", 0x1800);
    vram_lz77(s, "sGameOverTextTileTable", 0x800);
    vram_lz77(s, "sGameOver_454520", 0);
    memcpy(ppu->vram + 0x1000, ppu->vram + 0x1800, 0x800);
    vram_lz77(s, "sGameOverTextAndBackgroundGfx", 0x4000);
    vram_lz77(s, "sFileSelectObjIconsGfx", GBA_OBJ_VRAM);
    vram_lz77(s, "sGameOverTextPromptEnglishGfx", 0xA800);
    for (int bg = 0; bg < 4; ++bg)
        gba_ppu_write16(ppu, GBA_REG_BG0CNT + 2u * (unsigned)bg, (uint16_t)(bg << 8 | 1 << 2 | bg));
    g->dispcnt = GBA_DCNT_BG0 | GBA_DCNT_BG1 | GBA_DCNT_BG2 | GBA_DCNT_BG3 | GBA_DCNT_OBJ;
    gba_ppu_write16(ppu, GBA_REG_WINOUT, 0x1F);
    gba_ppu_write16(ppu, GBA_REG_WININ, 0x3F << 8);
    /* sGameOverDynamicPalette_Empty */
    g->dynamic_palette.unk_4 = 0xFF;
    memset(s->bg_hofs, 0, sizeof s->bg_hofs);
    memset(s->bg_vofs, 0, sizeof s->bg_vofs);
    g->option_selected = g->unk_35 = 0;
    update_text_gfx(g, s);
    g->oam[0] = mzm_menu_oam_from_rom(sym("sMenuOamData_Empty"));
    update_samus_head(g, CURSOR_LOADING);
}

/* GameOverProcessInput */
static bool process_input(MzmGameOver *g, MzmScreen *s) {
    if (s->changed_input & (KEY_A | KEY_START)) {
        play_menu_sound(SOUND_REQUEST_GAME_OVER_START_GAME);
        update_samus_head(g, CURSOR_SELECTING);
        return true;
    }
    bool moved = false;
    if (s->changed_input & KEY_UP) {
        if (g->option_selected != 0) {
            g->option_selected = 0;
            moved = true;
        }
    } else if (s->changed_input & KEY_DOWN) {
        if (g->option_selected != 1) {
            g->option_selected = 1;
            moved = true;
        }
    }
    if (!moved) return false;
    play_menu_sound(SOUND_REQUEST_GAME_OVER_MENU_CURSOR);
    update_text_gfx(g, s);
    update_samus_head(g, CURSOR_MOVING);
    return false;
}

/* GameOverUpdateLettersPalette: the GAME OVER letters "breathe". */
static void update_letters_palette(MzmGameOver *g, MzmScreen *s) {
    MzmGameOverPalette *d = &g->dynamic_palette;
    if (d->enable_flags == 0) return;
    if (d->enable_flags & 0x80) {
        if ((d->enable_flags & 0x7F) != d->unk_4) {
            d->unk_4 = d->enable_flags & 0x7F;
            d->timer = 0;
            d->current_palette_row = 0;
            d->unk_13 = 0;
            d->timer_limit = mzm_rom_u8(sym("sGameOverTextPaletteMaxTimers") + d->unk_4);
        }
        d->enable_flags = 1;
        return;
    }
    d->timer++;
    if (d->timer_limit > d->timer) return;
    uint32_t src = sym("sGameOverMenuPal") + 4 * 2;
    if (d->unk_4 == 0) {
        if (++d->current_palette_row >= 6) d->current_palette_row = 0;
        int row = d->current_palette_row;
        for (int i = 0; i < 6; ++i, ++row) {
            if (row >= 6) row = 0;
            d->palette[i] = mzm_rom_u16(src + (uint32_t)row * 2u);
        }
    } else if (d->unk_4 == 1) {
        if (++d->current_palette_row >= 9) d->current_palette_row = 0;
        int j = (int8_t)mzm_rom_u8(sym("sGameOverTextGradientPaletteOffset") + d->current_palette_row);
        int i = 0;
        for (; i < j; ++i) d->palette[i] = mzm_rom_u16(src);
        for (int k = 0; i < 6; ++i, ++k) d->palette[i] = mzm_rom_u16(src + (uint32_t)k * 2u);
    }
    d->timer = 0;
    memcpy(&s->ppu->palette[4], d->palette, sizeof d->palette);
}

/* GameOverVBlank */
static void vblank(MzmGameOver *g, MzmScreen *s) {
    GbaPpu *ppu = s->ppu;
    mzm_screen_copy_oam(s);
    for (int bg = 0; bg < 4; ++bg) {
        gba_ppu_write16(ppu, GBA_REG_BG0HOFS + 4u * (unsigned)bg, (uint16_t)(s->bg_hofs[bg] / 4));
        gba_ppu_write16(ppu, GBA_REG_BG0VOFS + 4u * (unsigned)bg, (uint16_t)(s->bg_vofs[bg] / 4));
    }
    gba_ppu_write16(ppu, GBA_REG_DISPCNT, g->dispcnt);
    gba_ppu_write16(ppu, GBA_REG_BLDY, s->bldy);
    gba_ppu_write16(ppu, GBA_REG_BLDALPHA, (uint16_t)(s->bldalpha_h << 8 | s->bldalpha_l));
    gba_ppu_write16(ppu, GBA_REG_BLDCNT, g->bldcnt);
    gba_ppu_write16(ppu, GBA_REG_WIN1H, g->win1h);
    gba_ppu_write16(ppu, GBA_REG_WIN1V, g->win1v);
}

void mzm_game_over_enter(MzmGameOver *g, uint8_t suit) {
    memset(g, 0, sizeof *g);
    g->suit = suit;
}

int mzm_game_over_update(MzmGameOver *g, MzmScreen *s) {
    int result = MZM_GAME_OVER_STAY;
    g->timer++;
    switch (g->sub_mode1) {
    case 0:
        init(g, s);
        g->timer = 0;
        g->sub_mode1++;
        break;
    case 1:
        if (s->bldy > 2) {
            s->bldy--;
            break;
        }
        g->dispcnt |= GBA_DCNT_WIN1;
        g->bldcnt = BLDCNT_BG2_FIRST | BLDCNT_BRIGHTNESS_DOWN;
        g->sub_mode1++;
        break;
    case 2:
        if (s->bldy < 6) {
            s->bldy = (uint8_t)(s->bldy + 2 > 6 ? 6 : s->bldy + 2);
            break;
        }
        g->dynamic_palette.enable_flags = 0x80 | 1;
        mzm_sfx_music_play(MUSIC_GAME_OVER, 0);
        g->sub_mode1++;
        break;
    case 3:
        if (process_input(g, s)) {
            g->timer = 0;
            g->sub_mode1++;
        }
        update_letters_palette(g, s);
        break;
    case 4:
        if (g->oam[0].ended) {
            g->timer = 0;
            g->sub_mode1++;
        }
        update_letters_palette(g, s);
        break;
    case 5:
        update_letters_palette(g, s);
        s->bldy = 4;
        g->bldcnt = BLDCNT_SCREEN_FIRST | BLDCNT_BRIGHTNESS_DOWN;
        g->dispcnt ^= GBA_DCNT_WIN1;
        mzm_sfx_music_fade(20);
        g->sub_mode1++;
        break;
    case 6:
        if (s->bldy < 16) s->bldy++;
        else g->sub_mode1++;
        break;
    case 7:
        /* Quit: the title screen; continue: the most recent file
         * (unk_75c04). */
        result = g->option_selected == 1 ? MZM_GAME_OVER_QUIT : MZM_GAME_OVER_CONTINUE;
        break;
    default:
        break;
    }
    if (result != MZM_GAME_OVER_STAY) return result;
    /* GameOverProcessOam (gCurrentOamRotation 0, gCurrentOamScaling 1.0:
     * the head is never rotated or scaled) */
    s->next_oam_slot = 0;
    mzm_complex_menu_oam_process(s, 1, g->oam, sym("sGameOverOam"), 0, 0x100);
    mzm_reset_free_oam(s);
    vblank(g, s);
    return result;
}
