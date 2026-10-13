/* SPDX-License-Identifier: GPL-3.0-only */
/* menus/title_screen.c (US) on the shared non-gameplay screen (see
 * mzm_title_screen.h). */
#include "mzm_title_screen.h"

#include <string.h>

#include "mzm_rom.h"
#include "mzm_sfx.h"

enum {
    BLOCK = 64, HALF_BLOCK = 32, PIXEL = 4, EIGHTH_BLOCK = 8, THREE_QUARTER_BLOCK = 48,
    BLDY_MAX = 16,
    BLDCNT_SCREEN_FIRST = 0x3F, BLDCNT_ALPHA = 1 << 6, BLDCNT_BRIGHTNESS_DOWN = 3 << 6,
    BLDCNT_BG1_FIRST = 1 << 1, BLDCNT_SCREEN_SECOND = 0x3F00,
    TYPE_FIRST_COMET_ACTIVE = 1 << 0, TYPE_SECOND_COMET_ACTIVE = 1 << 2,
    TYPE_TOP_SPARKLE_ACTIVE = 1 << 4, TYPE_TOP_SPARKLE_ENDED = 1 << 5,
    TYPE_BOTTOM_SPARKLE_ACTIVE = 1 << 6, TYPE_BOTTOM_SPARKLE_ENDED = 1 << 7,
    TYPE_ALL_SPARKLES_ENDED = TYPE_TOP_SPARKLE_ENDED | TYPE_BOTTOM_SPARKLE_ENDED,
    IDLE_COMETS = 0, IDLE_TITLE_FADING = 1, IDLE_IDLE = 2,
    PAL_SKY = 1 << 0, PAL_TITLE = 1 << 1, PAL_PROMPT = 1 << 2,
    OAM_COMET_SPAWNING = 1, OAM_COMET_APPEARING = 2, OAM_COMET_FLYING = 3,
    OAM_SPARKLE_GROWING = 5,
    SPARKLE_DELAY = 22,             /* CONVERT_SECONDS(.35f + 1.f / 60) */
    GROUND_SCROLL_END = BLOCK * 12 + HALF_BLOCK,
    MUSIC_TITLE_SCREEN = 0x2, SOUND_PRESSING_START = 0x213,
    KEY_A = 1 << 0, KEY_START = 1 << 3  /* the GBA key bits */
};

/* sTitleScreenPageData[i]: tiletablePage, graphicsPage, priority,
 * screenSize, bg (DCNT flag) and the tile table pointer (12 bytes). */
typedef struct { uint8_t tiletable_page, graphics_page, priority; uint16_t screen_size, bg; uint32_t tiletable; } PageData;

static PageData page_data(int i) {
    uint32_t at = mzm_sym("sTitleScreenPageData") + (uint32_t)i * 12;
    return (PageData){mzm_rom_u8(at), mzm_rom_u8(at + 1), mzm_rom_u8(at + 2), mzm_rom_u16(at + 4),
                      mzm_rom_u16(at + 6), mzm_rom_u32(at + 8)};
}

static uint16_t *ewram16(MzmScreen *s, uint32_t offset) { return (uint16_t *)(s->ewram + offset); }

static uint8_t table_u8(const char *name, int index) { return mzm_rom_u8(mzm_sym(name) + (uint32_t)index); }

/* TitleScreenSetBGCNTPageData */
static void set_bgcnt(MzmScreen *s, const PageData *p) {
    uint16_t value = (uint16_t)(p->priority | p->screen_size | p->tiletable_page << 8 | p->graphics_page << 2);
    for (int bg = 0; bg < 4; ++bg)
        if (p->bg == (GBA_DCNT_BG0 << bg) || bg == 3) {
            gba_ppu_write16(s->ppu, GBA_REG_BG0CNT + 2u * (unsigned)bg, value);
            return;
        }
}

/* TitleScreenLoadPageData */
static void load_page_data(MzmScreen *s, const PageData *p) {
    uint32_t offset = p->tiletable_page * 0x800u;
    mzm_lz77_from_rom(p->tiletable, s->ppu->vram + offset, GBA_VRAM_SIZE - offset);
}

static void update_oam_id(MzmTitleScreen *t, int offset, uint8_t oam_id) {
    t->oam[offset].oam_id = oam_id;
    t->oam[offset].animation_duration_counter = 0;
    t->oam[offset].current_animation_frame = 0;
}

/* TitleScreenCallProcessOam */
static void call_process_oam(MzmTitleScreen *t, MzmScreen *s) {
    s->next_oam_slot = 0;
    mzm_menu_oam_process(s, 7, t->oam, mzm_sym("sTitleScreenOam"));
    mzm_reset_free_oam(s);
}

/* TitleScreenResetOam */
static void reset_oam(MzmTitleScreen *t) {
    MzmMenuOam empty = mzm_menu_oam_from_rom(mzm_sym("sMenuOamData_Empty"));
    for (int i = 0; i < 7; ++i) t->oam[i] = empty;
    update_oam_id(t, 0, 0);
    t->oam[0].y = table_u8("sTitleScreenRomInfoPosition", 0);
    t->oam[0].x = table_u8("sTitleScreenRomInfoPosition", 1);
}

/* ApplySpecialBackgroundFadingColor of the BG and OBJ palettes saved at
 * EWRAM 0x8000 into the copy at 0x8400. */
static void fade_palettes(MzmScreen *s, uint8_t type, uint8_t color) {
    mzm_apply_fading_color(type, color, ewram16(s, 0x8000), ewram16(s, 0x8400), 0xFFFF);
    mzm_apply_fading_color(type, color, ewram16(s, 0x8200), ewram16(s, 0x8600), 0xFFFF);
}

/* TitleScreenFadingIn */
static bool fading_in(MzmTitleScreen *t, MzmScreen *s) {
    switch (t->fading_stage) {
    case 0:
        t->color_to_apply = 0;
        t->palette_updated = false;
        t->fading_timer = 0;
        t->fading_stage++;
        break;
    case 1:
        if (t->palette_updated) break;
        if (t->color_to_apply < 32) {
            fade_palettes(s, MZM_FADING_IN, t->color_to_apply);
            t->palette_updated = true;
            if (t->color_to_apply == 31) {
                t->color_to_apply++;
                break;
            }
            t->color_to_apply = t->color_to_apply + 4 > 31 ? 31 : (uint8_t)(t->color_to_apply + 4);
            break;
        }
        memcpy(s->ewram + 0x8400, s->ewram + 0x8000, 0x400);
        t->palette_updated = true;
        t->fading_stage++;
        break;
    case 2:
        if (!t->palette_updated) {
            t->color_to_apply = 0;
            t->fading_stage = 0;
            return true;
        }
        break;
    }
    return false;
}

/* TitleScreenFadingOut */
static bool fading_out(MzmTitleScreen *t, MzmScreen *s, uint8_t intensity, uint8_t delay) {
    t->fading_timer++;
    switch (t->fading_stage) {
    case 0:
        t->color_to_apply = 0;
        t->palette_updated = false;
        t->fading_timer = 0;
        t->fading_stage++;
        break;
    case 1:
        if (t->palette_updated || t->fading_timer < delay) break;
        t->fading_timer = 0;
        if (t->color_to_apply < 32) {
            fade_palettes(s, MZM_FADING_OUT, t->color_to_apply);
            t->palette_updated = true;
            if (t->color_to_apply == 31) {
                t->color_to_apply++;
                break;
            }
            t->color_to_apply =
                t->color_to_apply + intensity > 31 ? 31 : (uint8_t)(t->color_to_apply + intensity);
            break;
        }
        memset(s->ewram + 0x8400, 0, 0x400);
        t->palette_updated = true;
        t->fading_stage++;
        break;
    case 2:
        if (!t->palette_updated) {
            t->color_to_apply = 0;
            t->fading_stage = 0;
            return true;
        }
        break;
    }
    return false;
}

/* unk_76710: saves the palettes (and blacks them out unless `keep`). */
static void save_palettes(MzmTitleScreen *t, MzmScreen *s, bool keep) {
    memcpy(s->ewram + 0x8000, s->ppu->palette, 0x400);
    if (!keep) {
        memset(s->ppu->palette, 0, sizeof s->ppu->palette);
        memcpy(s->ewram + 0x8400, s->ppu->palette, 0x400);
    }
    t->fading_stage = 0;
}

/* unk_767a4 */
static void apply_palettes(MzmTitleScreen *t, MzmScreen *s) {
    if (!t->palette_updated) return;
    memcpy(s->ppu->palette, s->ewram + 0x8400, 0x400);
    t->palette_updated = false;
}

static void copy_palette(MzmScreen *s, int color, const char *table, uint32_t index, uint32_t bytes) {
    mzm_copy_from_rom(&s->ppu->palette[color], mzm_sym(table) + index * 2, bytes);
}

/* TitleScreenUpdateAnimatedPalette */
static void update_animated_palette(MzmTitleScreen *t, MzmScreen *s) {
    MzmTitleAnimatedPalette *a;
    if (t->active_animated_palettes & PAL_SKY) {
        a = &t->animated_palettes[0];
        if (--a->timer == 0) {
            a->timer = a->max_timer;
            if (++a->palette_row >= 6) a->palette_row = 0;
            copy_palette(s, 0x80, "sTitleScreenPal",
                         table_u8("sTitleScreenSkyDecorationsPaletteRows", a->palette_row) * 16u, 0x20);
        }
    }
    if (t->active_animated_palettes & PAL_TITLE) {
        a = &t->animated_palettes[1];
        if (a->unk_4 == 0) {
            a->unk_4 = 2;           /* sTitleScreenAnimatedPaletteTemplates[1].unk_4 */
            t->active_animated_palettes ^= PAL_TITLE;
        } else if (--a->timer == 0) {
            a->timer = a->max_timer;
            if (++a->palette_row >= 14) {
                a->palette_row = 0;
                a->unk_4--;
            }
            copy_palette(s, 1, "sTitleScreenPal",
                         table_u8("sTitleScreenTitlePaletteRows", a->palette_row) * 16u + 1, 0x1E);
        }
    }
    if (t->active_animated_palettes & PAL_PROMPT) {
        a = &t->animated_palettes[2];
        if (a->unk_4 != 0) {
            if (--a->timer == 0) {
                a->timer = a->max_timer;
                if (++a->palette_row > 1) a->palette_row = 0;
                a->palette_row = (uint8_t)(a->palette_row * 4);
            }
            copy_palette(s, 0xD0, "sTitleScreenPromptPal", a->palette_row * 16u, 0x18);
        } else {
            if (--a->timer == 0) {
                a->timer = a->max_timer;
                if (++a->palette_row > 5) a->palette_row = 0;
            }
            copy_palette(s, 0xD0, "sTitleScreenPromptPal",
                         table_u8("sTitleScreenPromptPaletteRows", a->palette_row) * 16u, 0x18);
        }
    }
}

/* unk_76978: the ground layers kept in EWRAM to VRAM. */
static void copy_ground(MzmTitleScreen *t, MzmScreen *s, uint8_t parts) {
    for (int i = 0; i < 3; ++i)
        if (parts & 1 << i) memcpy(s->ppu->vram + 0x4000 + i * 0x1000, s->ewram + 0x4000 + i * 0x1000, 0x1000);
    if (parts & 8) {
        memcpy(s->ppu->vram + 0x7000, s->ewram + 0x7000, 0x400);
        t->unk_24 = s->bg_vofs[3] = GROUND_SCROLL_END;
    }
}

/* unk_76a98: START during the comets jumps to the title. */
static bool skip_comets(MzmTitleScreen *t, MzmScreen *s) {
    switch (t->unk_e) {
    case 1:
        t->oam_timings[0].stage = 16;
        t->oam_timings[1].stage = 16;
        t->dispcnt &= (uint16_t)~page_data(0).bg;
        t->bldcnt = BLDCNT_SCREEN_FIRST | BLDCNT_BRIGHTNESS_DOWN;
        t->unk_e++;
        break;
    case 2:
        s->bldy = (uint8_t)(s->bldy + 4);
        if (s->bldy >= BLDY_MAX) {
            s->bldy = BLDY_MAX;
            t->unk_e++;
        }
        break;
    case 3: case 4: case 5: case 6:
        copy_ground(t, s, (uint8_t)(1 << (t->unk_e - 3)));
        t->unk_e++;
        break;
    case 7:
        if (s->bldy >= 9) {
            s->bldy = (uint8_t)(s->bldy - 8);
            break;
        }
        s->bldy = 0;
        t->unk_e = 0;
        return true;
    default:
        break;
    }
    return false;
}

/* TitleScreenTransferGroundGraphics */
static void transfer_ground(MzmTitleScreen *t, MzmScreen *s) {
    int32_t a = -1, b = -1;
    const uint8_t *src = NULL;
    int32_t vofs = s->bg_vofs[3], old = t->unk_24;
    if (old != vofs) {
        if (old < vofs) {
            if (vofs >= BLOCK * 5 + HALF_BLOCK) {
                a = (vofs - (BLOCK * 5 + HALF_BLOCK)) >> 5;
                b = (old - (BLOCK * 5 + HALF_BLOCK)) >> 5;
                src = s->ewram + 0x4000;
            }
        } else if (vofs <= BLOCK * 7 + HALF_BLOCK) {
            a = 13 - (((BLOCK * 7 + HALF_BLOCK) - vofs) >> 5);
            b = 13 - (((BLOCK * 7 + HALF_BLOCK) - old) >> 5);
            src = s->ewram;
        }
    }
    if ((uint32_t)a < 14u && a != b && src) memcpy(s->ppu->vram + 0x4000 + a * 0x400, src + a * 0x400, 0x400);
}

/* TitleScreenCometsView */
static bool comets_view(MzmTitleScreen *t, MzmScreen *s) {
    int32_t offset = 0;
    bool ended = false;
    t->comets_timer++;
    if (s->changed_input & (KEY_A | KEY_START)) {
        t->unk_e = 1;
        return false;
    }
    switch (t->comets_stage) {
    case 0:
        t->comets_stage++;
        t->comets_timer = 0;
        break;
    case 1:
        if (t->comets_timer > 60) {
            t->type |= TYPE_FIRST_COMET_ACTIVE;
            t->oam_timings[0].stage = 0;
            t->oam_timings[0].timer = 0;
            t->comets_stage++;
            t->comets_timer = 0;
        }
        break;
    case 2:
        if (t->comets_timer > 120) {
            t->type |= TYPE_SECOND_COMET_ACTIVE;
            t->oam_timings[1].stage = 0;
            t->oam_timings[1].timer = 0;
            t->comets_stage++;
            t->comets_timer = 0;
        }
        break;
    case 3:
        if (t->comets_timer > 30) {
            t->comets_stage++;
            t->comets_timer = 0;
        }
        break;
    case 4:
        if (s->bg_vofs[3] >= GROUND_SCROLL_END) {
            s->bg_vofs[3] = GROUND_SCROLL_END;
            t->comets_stage++;
            break;
        }
        offset += GROUND_SCROLL_END - s->bg_vofs[3] < 9 ? PIXEL / 2 : PIXEL;
        break;
    default:
        ended = true;
        break;
    }
    if (!ended) {
        offset += s->bg_vofs[3];
        if (offset > GROUND_SCROLL_END) offset = GROUND_SCROLL_END;
        else if (offset < 0) offset = 0;
        s->bg_vofs[3] = (uint16_t)offset;
        transfer_ground(t, s);
    }
    t->unk_24 = s->bg_vofs[3];
    return ended;
}

/* TitleScreenProcessComets */
static void process_comet(MzmTitleScreen *t, MzmScreen *s, MzmTitleOamTiming *timing, MzmMenuOam *o, int comet) {
    timing->timer++;
    switch (timing->stage) {
    case 0:
        if (comet == 0) {
            o->y = BLOCK * 2;
            o->x = BLOCK * 8;
        } else {
            o->y = BLOCK * 2 + HALF_BLOCK;
            o->x = BLOCK * 13 + 8;
        }
        o->animation_duration_counter = 0;
        o->current_animation_frame = 0;
        o->oam_id = OAM_COMET_SPAWNING;
        o->exists = 1;
        o->bound_background = 3;
        o->not_drawn = false;
        timing->stage++;
        timing->timer = 0;
        break;
    case 1:
        if (timing->timer > 20) {
            o->oam_id = OAM_COMET_APPEARING;
            timing->stage++;
            timing->timer = 0;
        }
        break;
    case 2:
        if (o->oam_id == OAM_COMET_FLYING) {
            timing->stage++;
            timing->timer = 0;
        }
        break;
    case 3: {
        uint32_t movement = timing->timer + 8u;
        o->x = (int16_t)(o->x - (int32_t)movement);
        o->y = (int16_t)(o->y + (int32_t)movement);
        uint32_t y_limit = (uint32_t)(o->y - (int16_t)s->bg_vofs[3]);
        uint32_t x_limit = (uint32_t)(o->x - (int16_t)s->bg_hofs[3]);
        o->not_drawn = y_limit + BLOCK * 2 > BLOCK * 14 || x_limit + BLOCK * 2 > BLOCK * 19;
        if (o->not_drawn) {
            timing->stage++;
            timing->timer = 0;
        }
        break;
    }
    case 4:
        t->type ^= (uint16_t)table_u8("sTitleScreenCometsFlags", comet * 2);
        t->type |= (uint16_t)table_u8("sTitleScreenCometsFlags", comet * 2 + 1);
        break;
    default:
        t->type &= (uint16_t)~table_u8("sTitleScreenCometsFlags", comet * 2);
        o->not_drawn = true;
        break;
    }
}

/* TitleScreenProcessTopSparkle */
static void process_top_sparkle(MzmTitleScreen *t, MzmTitleOamTiming *timing, MzmMenuOam *o) {
    timing->timer++;
    switch (timing->stage) {
    case 0:
        *o = mzm_menu_oam_from_rom(mzm_sym("sTitleScreenTopSparkleBaseOam"));
        timing->stage++;
        timing->timer = 0;
        break;
    case 1:
        if (timing->timer > SPARKLE_DELAY) {
            timing->stage++;
            timing->timer = 0;
        }
        break;
    case 2:
        o->x = (int16_t)(o->x + 12);
        if (o->oam_id == 0) {
            timing->stage++;
            timing->timer = 0;
        }
        break;
    case 3:
        t->type ^= TYPE_TOP_SPARKLE_ACTIVE;
        t->type |= TYPE_TOP_SPARKLE_ENDED;
        break;
    default:
        break;
    }
}

/* TitleScreenProcessBottomSparkle */
static bool process_bottom_sparkle(MzmTitleScreen *t, MzmTitleOamTiming *timing, MzmMenuOam *o) {
    bool update = false;
    timing->timer++;
    switch (timing->stage) {
    case 0:
        *o = mzm_menu_oam_from_rom(mzm_sym("sTitleScreenBottomSparkleBaseOam"));
        timing->stage++;
        timing->timer = 0;
        break;
    case 1:
        if (timing->timer > SPARKLE_DELAY) {
            timing->stage++;
            timing->timer = 0;
        }
        break;
    case 2:
        o->x = (int16_t)(o->x - PIXEL * 3);
        if (o->x <= BLOCK * 11 + HALF_BLOCK) {
            timing->stage++;
            timing->timer = 0;
        }
        break;
    case 3:
        o->y = (int16_t)(o->y + EIGHTH_BLOCK);
        if (o->y >= BLOCK * 5 + THREE_QUARTER_BLOCK) {
            timing->stage++;
            timing->timer = 0;
        }
        break;
    case 4:
        o->x = (int16_t)(o->x - PIXEL * 3);
        if (o->x <= BLOCK * 10) {
            update = true;
            timing->stage++;
            timing->timer = 0;
        }
        break;
    case 5:
        o->x = (int16_t)(o->x - PIXEL * 3);
        if (o->oam_id == 0) {
            timing->stage++;
            timing->timer = 0;
        }
        break;
    case 6:
        t->type ^= TYPE_BOTTOM_SPARKLE_ACTIVE;
        t->type |= TYPE_BOTTOM_SPARKLE_ENDED;
        break;
    default:
        break;
    }
    return update;
}

/* TitleScreenProcessOam */
static void process_oam(MzmTitleScreen *t, MzmScreen *s) {
    if (t->type & TYPE_TOP_SPARKLE_ACTIVE) process_top_sparkle(t, &t->oam_timings[0], &t->oam[5]);
    else if (t->type & TYPE_FIRST_COMET_ACTIVE) process_comet(t, s, &t->oam_timings[0], &t->oam[5], 0);
    if (t->type & TYPE_BOTTOM_SPARKLE_ACTIVE) {
        if (process_bottom_sparkle(t, &t->oam_timings[1], &t->oam[6])) {
            mzm_menu_oam_set_id(&t->oam[5], OAM_SPARKLE_GROWING);
            mzm_menu_oam_set_id(&t->oam[6], OAM_SPARKLE_GROWING);
        }
    } else if (t->type & TYPE_SECOND_COMET_ACTIVE) {
        process_comet(t, s, &t->oam_timings[1], &t->oam[6], 1);
    }
}

/* TitleScreenCheckPlayEffects: 0 nothing, 1 START, 2 demo */
static int check_play_effects(MzmTitleScreen *t, MzmScreen *s) {
    if (++t->demo_timer > 60 * 17) return 2;
    t->effects_timer++;
    if ((t->type & TYPE_ALL_SPARKLES_ENDED) == TYPE_ALL_SPARKLES_ENDED) {
        t->effects_timer = 0;
        t->type ^= TYPE_ALL_SPARKLES_ENDED;
    } else if (t->effects_timer == 60 * 4) {
        t->active_animated_palettes |= PAL_TITLE;
    } else if (t->effects_timer == 60 * 6) {
        t->type |= TYPE_TOP_SPARKLE_ACTIVE | TYPE_BOTTOM_SPARKLE_ACTIVE;
        t->oam_timings[0] = (MzmTitleOamTiming){0, 0};
        t->oam_timings[1] = (MzmTitleOamTiming){0, 0};
    }
    if (!t->unk_f) {
        if (t->timer > 30) t->unk_f = true;
    } else if (s->changed_input & (KEY_A | KEY_START)) {
        return 1;
    }
    return 0;
}

/* TitleScreenSetIdleStage */
static void set_idle_stage(MzmTitleScreen *t, MzmScreen *s, uint8_t stage) {
    t->oam_timings[2].stage = stage;
    if (stage != IDLE_TITLE_FADING && stage != IDLE_IDLE) return;
    t->dispcnt |= (uint16_t)(page_data(1).bg | page_data(0).bg);
    t->bldcnt = BLDCNT_BG1_FIRST | BLDCNT_ALPHA | BLDCNT_SCREEN_SECOND;
    if (stage == IDLE_TITLE_FADING) {
        t->timer = 0;
        s->bldalpha_l = 0;
        s->bldalpha_h = 16;
    } else {
        s->bldalpha_l = 16;
        s->bldalpha_h = 0;
        t->effects_timer = 210;
        t->unk_f = false;
        t->timer = 0;
    }
}

/* TitleScreenIdle */
static int idle(MzmTitleScreen *t, MzmScreen *s) {
    switch (t->oam_timings[2].stage) {
    case IDLE_COMETS:
        if (t->unk_e != 0) {
            if (skip_comets(t, s)) set_idle_stage(t, s, IDLE_IDLE);
        } else if (comets_view(t, s)) {
            set_idle_stage(t, s, IDLE_TITLE_FADING);
        }
        break;
    case IDLE_TITLE_FADING:
        if (s->changed_input) {
            s->bldalpha_l = 16;
            s->bldalpha_h = 0;
        } else if (t->timer % 10 == 0) {
            if (s->bldalpha_l >= 16) {
                set_idle_stage(t, s, IDLE_IDLE);
                break;
            }
            s->bldalpha_l++;
            s->bldalpha_h = (uint8_t)(16 - s->bldalpha_l);
        }
        if (s->bldalpha_l >= 16) set_idle_stage(t, s, IDLE_IDLE);
        break;
    case IDLE_IDLE:
        return check_play_effects(t, s);
    default:
        break;
    }
    return 0;
}

/* TitleScreenVBlank */
static void vblank(MzmTitleScreen *t, MzmScreen *s) {
    mzm_screen_copy_oam(s);
    for (int bg = 0; bg < 4; ++bg) {
        /* SUB_PIXEL_TO_PIXEL is a division */
        gba_ppu_write16(s->ppu, GBA_REG_BG0HOFS + 4u * (unsigned)bg, (uint16_t)(s->bg_hofs[bg] / 4));
        gba_ppu_write16(s->ppu, GBA_REG_BG0VOFS + 4u * (unsigned)bg, (uint16_t)(s->bg_vofs[bg] / 4));
    }
    gba_ppu_write16(s->ppu, GBA_REG_DISPCNT, t->dispcnt);
    gba_ppu_write16(s->ppu, GBA_REG_BLDY, s->bldy);
    gba_ppu_write16(s->ppu, GBA_REG_BLDALPHA, (uint16_t)(s->bldalpha_h << 8 | s->bldalpha_l));
    gba_ppu_write16(s->ppu, GBA_REG_BLDCNT, t->bldcnt);
}

/* TitleScreenSetCopyrightSymbol(TITLE_SCREEN_COPYRIGHT_SYMBOL_TRADEMARK) */
static void set_trademark(MzmScreen *s) {
    uint32_t at = 0x178 + page_data(0).tiletable_page * 2048u;
    for (int i = 0; i < 2; ++i, at += 2) {
        uint16_t v = (uint16_t)(s->ppu->vram[at] | s->ppu->vram[at + 1] << 8);
        v = (uint16_t)((v & 0xFC00) | (0x12D + i));
        s->ppu->vram[at] = (uint8_t)v;
        s->ppu->vram[at + 1] = (uint8_t)(v >> 8);
    }
}

static const MzmTitleAnimatedPalette templates[4] = {
    {0, 17, 17, 0}, {0, 3, 3, 2}, {0, 9, 9, 0}, {0, 4, 4, 1}};

/* TitleScreenInit */
static void title_init(MzmTitleScreen *t, MzmScreen *s) {
    uint8_t sub_mode2 = t->sub_mode2, sub_mode1 = t->sub_mode1;
    memset(t, 0, sizeof *t);
    t->sub_mode1 = sub_mode1;
    t->sub_mode2 = sub_mode2;
    t->bldcnt = BLDCNT_SCREEN_FIRST | BLDCNT_BRIGHTNESS_DOWN;
    gba_ppu_write16(s->ppu, GBA_REG_BLDCNT, t->bldcnt);
    s->bldy = BLDY_MAX;
    gba_ppu_write16(s->ppu, GBA_REG_BLDY, s->bldy);
    gba_ppu_write16(s->ppu, GBA_REG_DISPCNT, t->dispcnt = 0);
    s->next_oam_slot = 0;
    mzm_clear_gfx_ram(s);
    mzm_reset_free_oam(s);
    s->oam_x_offset = s->oam_y_offset = 0;
    mzm_sfx_stop_all();
    mzm_copy_from_rom(s->ppu->palette, mzm_sym("sTitleScreenPal"), 480);
    mzm_copy_from_rom(&s->ppu->palette[256], mzm_sym("sTitleScreenPal"), 480);
    s->ppu->palette[0] = 0;         /* SET_BACKDROP_COLOR(COLOR_BLACK) */
    PageData pages[2] = {page_data(0), page_data(1)};
    load_page_data(s, &pages[0]);
    load_page_data(s, &pages[1]);
    set_trademark(s);
    mzm_lz77_from_rom(mzm_sym("sTitleScreenTitleGfx"), s->ppu->vram + 0xC000, GBA_VRAM_SIZE - 0xC000);
    mzm_lz77_from_rom(mzm_sym("sTitleScreenSpaceBackgroundGfx"), s->ppu->vram + 0x4000, GBA_VRAM_SIZE - 0x4000);
    mzm_lz77_from_rom(mzm_sym("sTitleScreenSpaceBackgroundDecorationGfx"), s->ppu->vram + 0xA400,
                      GBA_VRAM_SIZE - 0xA400);
    mzm_lz77_from_rom(mzm_sym("sTitleScreenSpaceAndGroundBackgroundGfx"), s->ewram + 0x4000,
                      MZM_EWRAM_SIZE - 0x4000);
    memcpy(s->ewram, s->ppu->vram + 0x4000, 0x4000);
    mzm_lz77_from_rom(mzm_sym("sTitleScreenSparklesGfx"), s->ppu->vram + GBA_OBJ_VRAM,
                      GBA_VRAM_SIZE - GBA_OBJ_VRAM);
    set_bgcnt(s, &pages[0]);
    set_bgcnt(s, &pages[1]);
    memset(s->bg_hofs, 0, sizeof s->bg_hofs);
    memset(s->bg_vofs, 0, sizeof s->bg_vofs);
    s->bldalpha_h = 16;
    s->bldalpha_l = 0;
    t->bldcnt = BLDCNT_BG1_FIRST | BLDCNT_ALPHA | BLDCNT_SCREEN_SECOND;
    t->demo_timer = 0;
    if (t->sub_mode2 == 0) {
        t->oam_timings[2].stage = IDLE_COMETS;
    } else {
        copy_ground(t, s, 0xFF);
        t->oam_timings[2].stage = IDLE_IDLE;
        s->bldalpha_l = 16;
        s->bldalpha_h = 0;
        t->effects_timer = 210;
        t->unk_f = false;
        t->timer = 0;
    }
    reset_oam(t);
    call_process_oam(t, s);
    vblank(t, s);
    save_palettes(t, s, false);
    t->active_animated_palettes = PAL_SKY | PAL_PROMPT;
    for (int i = 0; i < 3; ++i) t->animated_palettes[i] = templates[i];
    t->dispcnt = (uint16_t)(GBA_DCNT_OBJ | pages[0].bg | pages[1].bg);
    gba_ppu_write16(s->ppu, GBA_REG_DISPCNT, t->dispcnt);
}

void mzm_title_enter(MzmTitleScreen *t, bool after_demo) {
    memset(t, 0, sizeof *t);
    t->sub_mode2 = after_demo ? 2 : 0;
}

bool mzm_title_update(MzmTitleScreen *t, MzmScreen *s) {
    bool leaving = false;
    t->timer++;
    switch (t->sub_mode1) {
    case 0:
        title_init(t, s);
        t->sub_mode1 = 1;
        t->timer = 0;
        break;
    case 1:
        apply_palettes(t, s);
        if (fading_in(t, s)) {
            mzm_sfx_music_priority(2);
            mzm_sfx_music_play(MUSIC_TITLE_SCREEN, 2);
            t->timer = 0;
            t->sub_mode1++;
        }
        break;
    case 2:
        t->sub_mode2 = (uint8_t)idle(t, s);
        if (t->sub_mode2 != 0) {
            t->timer = 0;
            if (t->sub_mode2 == 2) {
                mzm_sfx_music_priority(4);
                t->sub_mode1 = 3;
            } else {
                mzm_sfx_play(SOUND_PRESSING_START);
                t->animated_palettes[2] = templates[3];
                t->sub_mode1 = 3;
            }
            if (t->animated_palettes[1].unk_4 != 0) t->animated_palettes[1].unk_4 = 1;
        }
        update_animated_palette(t, s);
        process_oam(t, s);
        break;
    case 3:
        update_animated_palette(t, s);
        process_oam(t, s);
        if (t->animated_palettes[2].palette_row == 0 && t->timer > 40) {
            save_palettes(t, s, true);
            t->sub_mode1++;
            t->timer = 0;
        }
        break;
    case 4:
        apply_palettes(t, s);
        if (fading_out(t, s, 1, 1)) leaving = true;
        break;
    default:
        break;
    }
    if (!leaving) call_process_oam(t, s);
    if (t->sub_mode1 > 0) vblank(t, s);
    return leaving;
}
