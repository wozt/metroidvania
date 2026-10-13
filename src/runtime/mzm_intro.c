/* SPDX-License-Identifier: GPL-3.0-only */
/* The intro (see mzm_intro.h); comments name the pinned decompilation's
 * routines. */
#include "mzm_intro.h"

#include <string.h>

#include "mzm_rom.h"
#include "mzm_sfx.h"

enum {
    SCREEN_W = 240, SCREEN_H = 160, BLOCK = 64,
    /* IntroTextAction */
    ACTION_NONE = 0, ACTION_START, ACTION_SPACE, ACTION_SKIP_CHARACTER, ACTION_NEW_LINE, ACTION_COUNT,
    MUSIC_INTRO = 0x4, MUSIC_INTRO_MOTHER_BRAIN = 0x39,
    SOUND_INTRO_TEXT_LETTER = 0x226, SOUND_INTRO_SHIP_FLYING_TOWARDS_CAMERA, SOUND_INTRO_SHIP_INTERIOR,
    SOUND_INTRO_SHIP_FLYING_DOWN, SOUND_INTRO_MOTHER_BRAIN_JAR, SOUND_INTRO_FUZZ,
    KEY_A = 1 << 0, KEY_B = 1 << 1, KEY_START = 1 << 3,
    BLDCNT_SCREEN_FIRST = 0x3F, BLDCNT_ALPHA = 1 << 6, BLDCNT_BRIGHTNESS_DOWN = 3 << 6,
    BLDCNT_BG0_SECOND = 1 << 8, BLDCNT_OBJ_SECOND = 1 << 12,
    PI_2 = 64,
};

static uint32_t sym(const char *name) { return mzm_rom_symbol(name, NULL); }

static void vram_lz77(MzmScreen *s, const char *name, uint32_t offset) {
    mzm_lz77_from_rom(sym(name), s->ppu->vram + offset, GBA_VRAM_SIZE - offset);
}

/* gOamData[slot]'s attributes */
static uint16_t *oam(MzmScreen *s, int slot) { return &s->oam[slot * 4]; }
static void set_y(MzmScreen *s, int slot, int y) {
    uint16_t *a = oam(s, slot);
    a[0] = (uint16_t)((a[0] & 0xFF00) | (y & 0xFF));
}
static void set_x(MzmScreen *s, int slot, int x) {
    uint16_t *a = oam(s, slot);
    a[1] = (uint16_t)((a[1] & 0xFE00) | (x & 0x1FF));
}
static void set_palette(MzmScreen *s, int slot, int palette) {
    uint16_t *a = oam(s, slot);
    a[2] = (uint16_t)((a[2] & 0x0FFF) | (palette & 15) << 12);
}
static void set_obj_mode(MzmScreen *s, int slot, int mode) {
    uint16_t *a = oam(s, slot);
    a[0] = (uint16_t)((a[0] & ~(3 << 10)) | (mode & 3) << 10);
}

/* Copies a part's three attributes from the ROM frame data. */
static void copy_part(MzmScreen *s, int slot, uint32_t src) {
    uint16_t *a = oam(s, slot);
    a[0] = mzm_rom_u16(src);
    a[1] = mzm_rom_u16(src + 2);
    a[2] = mzm_rom_u16(src + 4);
}

static int16_t sine(uint16_t angle) { return (int16_t)mzm_rom_u16(sym("sSineTable") + (uint32_t)angle * 2u); }

/* FixedMultiplication, FixedInverse */
static int16_t fixed_mul(int16_t a, int16_t b) { return (int16_t)(((int32_t)a * b) >> 8); }
static int16_t fixed_inverse(int16_t a) { return (int16_t)(65536 / a); }

/* ProcessComplexOam */
static void process_complex_oam(MzmScreen *s, int slot, int16_t x_position, int16_t y_position, uint16_t rotation,
                                int16_t scaling, bool double_size, uint16_t matrix) {
    uint16_t *a = oam(s, slot);
    int32_t x_origin = (int16_t)((uint16_t)x_position + BLOCK), y_origin = (int16_t)((uint16_t)y_position + BLOCK);
    int shape = a[0] >> 14, size = a[1] >> 14;
    uint8_t x_half = mzm_rom_u8(sym("sSpriteXHalfRadius") + (uint32_t)(shape * 4 + size));
    uint8_t y_half = mzm_rom_u8(sym("sSpriteYHalfRadius") + (uint32_t)(shape * 4 + size));
    int32_t x = (int16_t)((a[1] & 0x1FF) + x_origin) & 511;
    int32_t y = (int16_t)((a[0] & 0xFF) + y_origin) & 255;
    int32_t tmp_x = (int16_t)(x - x_origin + x_half), tmp_y = (int16_t)(y - y_origin + y_half);
    int32_t scaled_x = (int16_t)(((tmp_x * scaling) >> 8) - tmp_x);
    int32_t scaled_y = (int16_t)(((tmp_y * scaling) >> 8) - tmp_y);
    x = (int16_t)(x + scaled_x);
    y = (int16_t)(y + scaled_y);
    int32_t u = (int16_t)(x - x_origin + x_half), v = (int16_t)(y - y_origin + y_half);
    int32_t cos = sine((uint16_t)(rotation + PI_2)), sin = sine(rotation);
    x = (int16_t)((u * cos - v * sin) >> 8);
    y = (int16_t)((u * sin + v * cos) >> 8);
    a[0] = (uint16_t)((a[0] & ~(3 << 8)) | (double_size ? 3 : 1) << 8);
    x = (int16_t)(x - x_half * (double_size ? 2 : 1));
    y = (int16_t)(y - y_half * (double_size ? 2 : 1));
    set_x(s, slot, x + x_origin - BLOCK);
    set_y(s, slot, y + y_origin - BLOCK);
    bool x_flip = a[1] & 1 << 12, y_flip = a[1] & 1 << 13;
    uint16_t number = (uint16_t)(matrix + (x_flip ? (y_flip ? 3 : 1) : (y_flip ? 2 : 0)));
    /* matrixNum replaces the flips (bits 9 to 13) */
    a[1] = (uint16_t)((a[1] & ~(0x1F << 9)) | (number & 0x1F) << 9);
}

/* CalculateOamPart4: the four matrices (plain and flipped) at a slot. */
static void calculate_oam_part4(MzmScreen *s, uint16_t rotation, int16_t scaling, int slot) {
    int16_t cos = sine((uint16_t)(rotation + PI_2)), sin = sine(rotation);
    int16_t dy1 = fixed_mul(cos, fixed_inverse(scaling));
    int16_t dmy1 = fixed_mul(sin, fixed_inverse(scaling));
    int16_t dx1 = fixed_mul((int16_t)-sin, fixed_inverse(scaling));
    int16_t negative = (int16_t)-scaling;
    int16_t dmx1 = fixed_mul(cos, fixed_inverse(negative));
    int16_t dy2 = fixed_mul(sin, fixed_inverse(negative));
    int16_t dmy2 = fixed_mul((int16_t)-sin, fixed_inverse(negative));
    const int16_t params[16] = {dy1, dmy1, dx1, dy1, dmx1, dy2, dx1, dy1,
                                dy1, dmy1, dmy2, dmx1, dmx1, dy2, dmy2, dmx1};
    for (int i = 0; i < 16; ++i) oam(s, slot + i)[3] = (uint16_t)params[i];
}

/* IntroVBlank / IntroFuzzVBlank */
static void vblank(MzmIntro *in, MzmScreen *s) {
    GbaPpu *ppu = s->ppu;
    mzm_screen_copy_oam(s);
    gba_ppu_write16(ppu, GBA_REG_DISPCNT, in->dispcnt);
    gba_ppu_write16(ppu, GBA_REG_BLDCNT, in->bldcnt);
    if (in->fuzz_vblank) {
        memcpy(&ppu->palette[256], in->fuzz_palette, sizeof in->fuzz_palette);
        return;
    }
    gba_ppu_write16(ppu, GBA_REG_BLDALPHA, (uint16_t)(s->bldalpha_h << 8 | s->bldalpha_l));
    gba_ppu_write16(ppu, GBA_REG_BLDY, s->bldy);
    gba_ppu_write16(ppu, GBA_REG_BG0HOFS, in->bg0_x & 511);
    gba_ppu_write16(ppu, GBA_REG_BG0VOFS, in->bg0_y & 511);
}

/* IntroInit */
static void init(MzmIntro *in, MzmScreen *s) {
    GbaPpu *ppu = s->ppu;
    gba_ppu_write16(ppu, GBA_REG_DISPCNT, 0);
    memset(in, 0, sizeof *in);
    in->scaling = 32;                               /* Q_8_8(.125f) */
    in->char_drawer_x = SCREEN_W / 5 + 8;
    in->char_drawer_y = SCREEN_H / 2 + 16;
    mzm_clear_gfx_ram(s);
    vram_lz77(s, "sIntroTextAndShipFlyingInGfx", GBA_OBJ_VRAM);
    vram_lz77(s, "sIntroSpaceBackgroundGfx", 0);
    vram_lz77(s, "sIntroSpaceBackgroundTileTable", 0x8000);
    vram_lz77(s, "sIntro_47920c", 0x9000);
    mzm_copy_from_rom(&ppu->palette[256], sym("sIntroTextAndShipPal"), 192);
    mzm_copy_from_rom(&ppu->palette[0], sym("sIntroTextAndShipPal"), 192);
    mzm_copy_from_rom(&ppu->palette[15 * 16], sym("sIntroPal_45f9d4"), 32);
    gba_ppu_write16(ppu, GBA_REG_BG0CNT, 16 << 8 | 0);
    gba_ppu_write16(ppu, GBA_REG_BG0CNT + 2, 18 << 8 | 1);
    s->next_oam_slot = 0;
    mzm_reset_free_oam(s);
    in->bg0_x = in->bg0_y = 0;
    for (int bg = 0; bg < 4; ++bg) {
        gba_ppu_write16(ppu, GBA_REG_BG0HOFS + 4u * (unsigned)bg, 0);
        gba_ppu_write16(ppu, GBA_REG_BG0VOFS + 4u * (unsigned)bg, 0);
    }
    mzm_sfx_music_priority(1);
    in->dispcnt = GBA_DCNT_OBJ;
    in->bldcnt = BLDCNT_SCREEN_FIRST | BLDCNT_BRIGHTNESS_DOWN;
    s->bldy = 16;
    vblank(in, s);
}

/* IntroTextProcessOam: the typed characters (the last one with its own
 * palette) and the blinking marker. */
static void text_process_oam(MzmIntro *in, MzmScreen *s) {
    switch (in->char_drawer_stage++) {
    case 0:
    case 3:
        in->char_drawer_palette = 1;
        break;
    case 1:
        in->char_drawer_palette = 2;
        break;
    case 4:
        in->char_drawer_palette = 0;
        in->char_drawer_stage = 0;
        break;
    default:
        break;
    }
    int i = 0;
    int y = (uint16_t)(SCREEN_H * 4 / 5), x = SCREEN_W / 2;
    if (in->final_character != 0) {
        uint32_t src = in->text_oam;
        int count = mzm_rom_u16(src);
        src += 2;
        for (; i < count && i < in->final_character; ++i, src += 6) {
            copy_part(s, i, src);
            set_y(s, i, mzm_rom_u16(src) + y);
            set_x(s, i, mzm_rom_u16(src + 2) + x);
            set_palette(s, i, i == in->final_character - 1 ? in->character_palette : 0);
        }
    }
    uint32_t src = sym("sIntroTextMarkerOam");
    int count = i + mzm_rom_u16(src);
    src += 2;
    for (; i < count; ++i, src += 6) {
        copy_part(s, i, src);
        set_y(s, i, mzm_rom_u16(src) + in->char_drawer_y);
        set_x(s, i, mzm_rom_u16(src + 2) + (uint16_t)in->char_drawer_x);
        set_palette(s, i, in->char_drawer_palette);
    }
    s->next_oam_slot = (uint8_t)i;
}

/* IntroProcessText: 1 when a character was shown, 2 at the end. */
static int process_text(MzmIntro *in, uint8_t action, uint16_t indent) {
    bool dont_process = false, skip_character = false, new_line = false, shown = false;
    int16_t previous_x = in->char_drawer_x;
    switch (action) {
    case ACTION_COUNT:
        if (in->unk_a != 1) {
            if (in->unk_a > 20) return 2;
        } else {
            in->char_drawer_x = (int16_t)(in->char_drawer_x + 8);
            mzm_sfx_play(SOUND_INTRO_TEXT_LETTER);
        }
        return 0;
    case ACTION_START:
        if (in->unk_a > 3) {
            in->final_character = (uint8_t)(in->final_character + 2);
            in->current_character++;
            shown = true;
        }
        dont_process = true;
        break;
    case ACTION_SPACE:
        if (in->unk_a == 1) {
            in->char_drawer_x = (int16_t)(in->char_drawer_x + 8);
        } else if (in->unk_a > 3) {
            in->final_character++;
            in->current_character++;
            shown = true;
        }
        dont_process = true;
        break;
    case ACTION_NEW_LINE:
        new_line = true;
        /* fall through */
    case ACTION_SKIP_CHARACTER:
        skip_character = true;
        break;
    default:
        break;
    }
    if (!dont_process) {
        switch (in->unk_a) {
        case 0:
        case 3:
            in->character_palette = 1;
            break;
        case 1:
            in->character_palette = 2;
            if (new_line) {
                in->char_drawer_x = (int16_t)indent;
                in->char_drawer_y = (uint16_t)(in->char_drawer_y + 24);
            } else {
                in->char_drawer_x = (int16_t)(in->char_drawer_x + 8);
            }
            break;
        case 4:
            in->character_palette = 0;
            break;
        case 5:
            in->current_character++;
            in->final_character = (uint8_t)(in->final_character + (skip_character ? 2 : 1));
            shown = true;
            break;
        default:
            break;
        }
    }
    if (previous_x != in->char_drawer_x && action != ACTION_SPACE) mzm_sfx_play(SOUND_INTRO_TEXT_LETTER);
    return shown ? 1 : 0;
}

/* IntroEmergencyOrder, IntroExterminate and IntroDefeat: a typed text. */
static void text_stage(MzmIntro *in, MzmScreen *s, const char *actions, const char *text_oam, uint16_t indent,
                       bool reload_gfx) {
    switch (in->timer++) {
    case 0:
        if (reload_gfx) vram_lz77(s, "sIntroTextAndShipFlyingInGfx", GBA_OBJ_VRAM);
        else if (in->stage != 0) in->dispcnt = GBA_DCNT_OBJ;
        break;
    case 1:
        if (reload_gfx) in->dispcnt = GBA_DCNT_OBJ;
        break;
    case 10:                                        /* CONVERT_SECONDS(1.f / 6) */
        in->dispcnt = GBA_DCNT_BG1 | GBA_DCNT_OBJ;
        break;
    default:
        break;
    }
    int result = process_text(in, mzm_rom_u8(sym(actions) + in->current_character), indent);
    if (result == 2) {
        in->stage++;
        in->final_character = in->current_character = in->unk_a = 0;
        in->timer = 0;
        in->char_drawer_x = SCREEN_W / 10;
        in->char_drawer_y = SCREEN_H / 2;
        return;
    }
    in->text_oam = sym(text_oam);
    text_process_oam(in, s);
    in->unk_a = result != 0 ? 0 : (uint8_t)(in->unk_a + 1);
}

/* IntroShipFlyingTowardsCameraProcessOam */
static void ship_process_oam(MzmIntro *in, MzmScreen *s) {
    if (in->timer < 4) in->ship_y = (uint16_t)(in->ship_y + 8);
    else if (in->timer < 8) in->ship_y = (uint16_t)(in->ship_y + 6);
    else if (in->timer < 16) in->ship_y = (uint16_t)(in->ship_y + 3);
    else if (in->timer < 20) in->ship_y = (uint16_t)(in->ship_y + 1);
    else if (in->timer < 28) in->ship_y = (uint16_t)(in->ship_y - 1);
    /* Q_8_8(.5f), (1.25f), (1.875f); Q_8_8(.035f) = 8, (.065f) = 16, (.125f) = 32 */
    if (in->scaling < 128) in->scaling = (uint16_t)(in->scaling + 8);
    else if (in->scaling < 320) in->scaling = (uint16_t)(in->scaling + 16);
    else if (in->scaling < 480) in->scaling = (uint16_t)(in->scaling + 32);
    uint32_t src = sym((in->timer & 7) < 4 ? "sIntroShipFlyingTowardsCameraOam_1" : "sIntroShipFlyingTowardsCameraOam_2");
    int count = mzm_rom_u16(src);
    src += 2;
    int i;
    for (i = 0; i < count; ++i, src += 6) {
        copy_part(s, i, src);
        process_complex_oam(s, i, (int16_t)in->ship_x, (int16_t)in->ship_y, in->rotation, (int16_t)in->scaling, true, 0);
    }
    s->next_oam_slot = (uint8_t)i;
    calculate_oam_part4(s, in->rotation, (int16_t)in->scaling, 0);
}

/* IntroShipFlyingTowardsCamera */
static void ship_flying_towards_camera(MzmIntro *in, MzmScreen *s) {
    bool ended = false;
    switch (in->timer) {
    case 0:
        in->bldcnt = BLDCNT_ALPHA | BLDCNT_BG0_SECOND | BLDCNT_OBJ_SECOND;
        s->bldalpha_l = 16 / 2 + 1;
        s->bldalpha_h = 16 / 2 - 1;
        break;
    case 1:
        in->dispcnt = GBA_DCNT_BG0 | GBA_DCNT_OBJ;
        mzm_sfx_play(MUSIC_INTRO);
        mzm_sfx_play(SOUND_INTRO_SHIP_FLYING_TOWARDS_CAMERA);
        break;
    case 30 + 2:
        in->dispcnt = 0;
        in->bldcnt = 0;
        in->stage++;
        ended = true;
        break;
    default:
        break;
    }
    ship_process_oam(in, s);
    if (ended) in->timer = 0;
    else in->timer++;
}

/* IntroSamusInHerShip */
static void samus_in_her_ship(MzmIntro *in, MzmScreen *s) {
    bool ended = false;
    switch (in->timer) {
    case 0:
        vram_lz77(s, "sIntroSamusInHerShipGfx", 0);
        break;
    case 1:
        vram_lz77(s, "sIntroSamusInHerShipTileTable", 0x8000);
        break;
    case 2:
        mzm_copy_from_rom(&s->ppu->palette[0], sym("sIntroSamusInHerShipPal"), 160);
        break;
    case 3:
        in->dispcnt = GBA_DCNT_BG0;
        mzm_sfx_play(SOUND_INTRO_SHIP_INTERIOR);
        break;
    case 60 + 20:
        in->dispcnt = 0;
        mzm_sfx_fade(SOUND_INTRO_SHIP_INTERIOR, 0);
        in->stage++;
        ended = true;
        break;
    default:
        break;
    }
    /* The camera shakes. */
    if ((in->unk_a & 3) < 2) {
        in->bg0_x = in->bg0_y = 0;
    } else {
        in->bg0_x--;
        in->bg0_y--;
    }
    in->unk_a++;
    if (ended) {
        in->timer = 0;
        in->unk_a = 0;
    } else {
        in->timer++;
    }
}

/* IntroViewOfZebesProcessOam: the ship and its heat. */
static void view_of_zebes_process_oam(MzmIntro *in, MzmScreen *s) {
    int y, x;
    if ((in->unk_3d & 3) < 2) {
        y = SCREEN_H * 3 / 5 - 1;
        x = SCREEN_W / 2 + 8;
    } else {
        y = SCREEN_H * 3 / 5;
        x = SCREEN_W / 2 + 8 - 1;
    }
    uint32_t src = sym("sIntroViewOfZebesShipOam");
    int count = mzm_rom_u16(src);
    src += 2;
    int i;
    for (i = 0; i < count; ++i, src += 6) {
        copy_part(s, i, src);
        set_y(s, i, mzm_rom_u16(src) + y);
        set_obj_mode(s, i, 1);
        set_x(s, i, mzm_rom_u16(src + 2) + x);
    }
    y = SCREEN_H * 3 / 5;
    x = SCREEN_W / 2 + 8;
    src = sym((in->unk_3d & 7) < 4 ? "sIntroViewOfZebesHeatOam_2" : "sIntroViewOfZebesHeatOam_1");
    count += mzm_rom_u16(src);
    src += 2;
    for (; i < count; ++i, src += 6) {
        copy_part(s, i, src);
        set_y(s, i, mzm_rom_u16(src) + y);
        set_obj_mode(s, i, 1);
        set_x(s, i, mzm_rom_u16(src + 2) + x);
    }
    s->next_oam_slot = (uint8_t)i;
    in->unk_3d++;
}

/* IntroViewOfZebes */
static void view_of_zebes(MzmIntro *in, MzmScreen *s) {
    bool ended = false;
    switch (in->timer) {
    case 0:
        vram_lz77(s, "sIntroSamusShipViewOfZebesGfx", 0x10800);
        break;
    case 1:
        vram_lz77(s, "sIntroViewOfZebesGfx", 0);
        break;
    case 2:
        vram_lz77(s, "sIntroViewOfZebesTileTable", 0x8000);
        mzm_copy_from_rom(&s->ppu->palette[0], sym("sIntroViewOfZebesPal"), 320);
        mzm_copy_from_rom(&s->ppu->palette[256], sym("sIntroViewOfZebesPal"), 320);
        in->bg0_x = 16;                             /* QUARTER_BLOCK_SIZE */
        break;
    case 3:
        /* WRITE_16(REG_BLDALPHA, ...) directly: the V-blank rewrites it
         * from gWrittenToBldalpha, which still holds 7 and 9. */
        gba_ppu_write16(s->ppu, GBA_REG_BLDALPHA, (16 / 2 - 1) << 8 | (16 / 2 + 1));
        in->dispcnt = GBA_DCNT_BG0 | GBA_DCNT_OBJ;
        in->bldcnt = BLDCNT_ALPHA | BLDCNT_BG0_SECOND | BLDCNT_OBJ_SECOND;
        mzm_sfx_play(SOUND_INTRO_SHIP_FLYING_DOWN);
        break;
    case 60 + 20:
        in->dispcnt = 0;
        in->bldcnt = 0;
        mzm_sfx_fade(SOUND_INTRO_SHIP_FLYING_DOWN, 0);
        in->stage++;
        ended = true;
        break;
    default:
        break;
    }
    if (in->unk_a++ > 4) {
        in->unk_a = 0;
        in->bg0_x--;
    }
    if (ended) {
        in->unk_a = 0;
        in->timer = 0;
    } else {
        in->timer++;
    }
    view_of_zebes_process_oam(in, s);
}

/* IntroMotherBrain */
static void mother_brain(MzmIntro *in, MzmScreen *s) {
    bool ended = false;
    switch (in->timer) {
    case 0:
        vram_lz77(s, "sIntroFuzzGfx", GBA_OBJ_VRAM);
        break;
    case 1:
        vram_lz77(s, "sIntroMotherBrainGfx", 0);
        break;
    case 2:
        vram_lz77(s, "sIntroMotherBrainTileTable", 0x8000);
        in->bg0_x = 0;
        in->bg0_y = 0x60;
        break;
    case 3:
        mzm_copy_from_rom(&s->ppu->palette[0], sym("sIntroMotherBrainPal"), 160);
        in->dispcnt = GBA_DCNT_BG0;
        mzm_sfx_play(SOUND_INTRO_MOTHER_BRAIN_JAR);
        mzm_sfx_play(MUSIC_INTRO_MOTHER_BRAIN);
        break;
    case 192 - 1:                                   /* CONVERT_SECONDS(3.2f) - 1 */
        mzm_sfx_fade(SOUND_INTRO_MOTHER_BRAIN_JAR, 0);
        break;
    case 192:
        in->fuzz_vblank = true;
        in->dispcnt = GBA_DCNT_BG0 | GBA_DCNT_OBJ;
        in->stage++;
        ended = true;
        break;
    default:
        break;
    }
    if (in->timer > 40 && in->bg0_y != 0) in->bg0_y--;
    if (ended) in->timer = 0;
    else in->timer++;
}

/* IntroFuzz: OBJ noise with a palette from scattered ROM data. */
static bool fuzz(MzmIntro *in, MzmScreen *s) {
    switch (in->timer++) {
    case 30 + 2:
        in->dispcnt = 0;
        break;
    case 96:                                        /* CONVERT_SECONDS(1.6f) */
        return true;
    case 0:
        mzm_sfx_play(SOUND_INTRO_FUZZ);
        break;
    case 1:
        in->dispcnt = GBA_DCNT_OBJ;
        break;
    default:
        break;
    }
    uint32_t source = 0;
    switch (in->unk_a & 7) {
    case 0: source = sym("sIntroFuzzRandomValues_1"); break;
    case 2: source = sym("sIntroFuzzRandomValues_2"); break;
    case 4: source = sym("sTimeAttackPasswordCharacters"); break;
    case 6: source = sym("sSpriteYHalfRadius") + 4; break;      /* &sSpriteYHalfRadius[1] */
    default: break;
    }
    if (source)
        for (int i = 0; i < 16; ++i) in->fuzz_palette[i] = mzm_rom_u16(source + (uint32_t)i * 2u);
    in->unk_a++;
    /* IntroFuzzProcessOam */
    uint32_t src = sym("sIntroFuzzOam");
    int count = mzm_rom_u16(src);
    src += 2;
    int i;
    for (i = 0; i < count; ++i, src += 6) {
        copy_part(s, i, src);
        set_y(s, i, mzm_rom_u16(src) - BLOCK * 2);
        set_x(s, i, mzm_rom_u16(src + 2) + BLOCK * 2);
    }
    s->next_oam_slot = (uint8_t)i;
    return false;
}

void mzm_intro_enter(MzmIntro *intro) { memset(intro, 0, sizeof *intro); }

bool mzm_intro_update(MzmIntro *in, MzmScreen *s) {
    bool ended = false;
    s->next_oam_slot = 0;
    switch (in->sub_mode1) {
    case 0:
        init(in, s);
        in->sub_mode1++;
        return false;
    case 1:
        if (s->bldy != 0) {
            s->bldy--;
            break;
        }
        in->bldcnt = 0;
        in->sub_mode1++;
        break;
    case 2: {
        bool finished = false;
        if (s->changed_input & (KEY_A | KEY_B | KEY_START)) {
            in->sub_mode1++;
            in->sub_mode2 = 1;
            mzm_sfx_fade_all(10);
            mzm_sfx_music_fade(10);
        } else {
            switch (in->stage) {
            case 0:
                text_stage(in, s, "sIntroEmergencyOrderActions", "sIntroEmergencyOrderTextOam", SCREEN_W / 4 - 4,
                           false);
                if (in->stage == 1) {
                    in->ship_x = SCREEN_W / 2;
                    in->ship_y = SCREEN_H / 5 - 4;
                }
                break;
            case 1: ship_flying_towards_camera(in, s); break;
            case 2: samus_in_her_ship(in, s); break;
            case 3:
                text_stage(in, s, "sIntroExterminateAllActions", "sIntroExterminateAllTextOam", 8, false);
                if (in->stage == 4) in->dispcnt = 0;
                break;
            case 4: view_of_zebes(in, s); break;
            case 5:
                text_stage(in, s, "sIntroDefeatTheActions", "sIntroDefeatTheTextOam", SCREEN_W / 8 + 2, true);
                if (in->stage == 6) in->dispcnt = 0;
                break;
            case 6: mother_brain(in, s); break;
            case 7: finished = fuzz(in, s); break;
            default: finished = true; break;
            }
            if (finished) {
                in->sub_mode1++;
                in->sub_mode2 = 0;
            }
        }
        mzm_reset_free_oam(s);
        break;
    }
    case 3:
        ended = true;
        break;
    default:
        break;
    }
    if (!ended) vblank(in, s);
    return ended;
}
