/* SPDX-License-Identifier: GPL-3.0-only */
/* The non-gameplay cutscenes (see mzm_cutscene.h); comments name the pinned
 * decompilation's routines. */
#include "mzm_cutscene.h"

#include <string.h>

#include "mzm_rom.h"
#include "mzm_sfx.h"

enum {
    /* CUTSCENE_STAGE_* */
    STAGE_STARTING, STAGE_FADING_IN, STAGE_INIT, STAGE_ONGOING, STAGE_ENDING,
    /* CUTSCENE_SPECIAL_EFFECT_STATUS_* */
    EFFECT_ON_BG = 1 << 2, EFFECT_BG_ENDED = 1 << 3,
    BLDCNT_SCREEN_FIRST = 0x3F, BLDCNT_BRIGHTNESS_DOWN = 3 << 6, BLDCNT_ALPHA = 1 << 6,
    BLDCNT_BG0_FIRST = 1 << 0, BLDCNT_BACKDROP_SECOND = 1 << 13,
    BLDALPHA_MAX = 16, BLDY_MAX = 16,
    START_BG_POS = 64 * 32,                 /* NON_GAMEPLAY_START_BG_POS */
    KEY_A = 1 << 0, KEY_B = 1 << 1,
    SOUND_INTRO_TEXT_LANDING = 0x233,
    /* StoryTextId */
    STORY_TEXT_PLANET_ZEBES = 0, STORY_TEXT_THE_TIMING, STORY_TEXT_COULD_I_SURVIVE,
    /* sStoryTextCutscenePagesData[0] */
    STORY_GRAPHICS_PAGE = 2, STORY_TILETABLE_PAGE = 0x14, STORY_BG = GBA_DCNT_BG0,
    STORY_TEXT_VRAM = STORY_GRAPHICS_PAGE * 0x4000 + 0x3000,
};

static uint32_t sym(const char *name) { return mzm_rom_symbol(name, NULL); }

/* sCutsceneData[].storyText of the ported cutscenes */
static uint8_t story_text(uint8_t cutscene) {
    switch (cutscene) {
    case MZM_CUTSCENE_INTRO_TEXT: return STORY_TEXT_PLANET_ZEBES + 1;
    case MZM_CUTSCENE_MOTHERSHIP_MONOLOGUE: return STORY_TEXT_THE_TIMING + 1;
    case MZM_CUTSCENE_COULD_I_SURVIVE: return STORY_TEXT_COULD_I_SURVIVE + 1;
    default: return 0;
    }
}

/* CutsceneSetBackgroundPosition (on the *_NonGameplay subpixel copies) */
static void set_bg_position(MzmScreen *s, bool x, bool y, int bg, uint16_t value) {
    if (x) s->bg_hofs[bg] = value;
    if (y) s->bg_vofs[bg] = value;
}

/* CutsceneUpdateBackgroundsPosition(FALSE) */
static void update_backgrounds_position(MzmCutscene *c, MzmScreen *s) {
    for (int bg = 0; bg < 4; ++bg) {
        c->bg_hofs[bg] = (uint16_t)(s->bg_hofs[bg] / 4);
        c->bg_vofs[bg] = (uint16_t)(s->bg_vofs[bg] / 4);
    }
}

/* CutsceneStartBackgroundEffect */
static void start_background_effect(MzmCutscene *c, uint16_t bldcnt, uint8_t alpha_l, uint8_t alpha_h,
                                    uint8_t interval, uint8_t intensity) {
    MzmCutsceneEffect *e = &c->effect;
    e->status = (uint8_t)((e->status & ~EFFECT_BG_ENDED) | EFFECT_ON_BG);
    e->bg_alpha_l = alpha_l;
    e->bg_alpha_h = alpha_h;
    e->bg_intensity = intensity;
    e->bg_interval = e->bg_timer = interval;
    e->bg_bldcnt = bldcnt;
    c->bldcnt = bldcnt;
}

static uint8_t step_towards(uint8_t value, uint8_t target, uint8_t intensity) {
    if (value < target) return value + intensity > target ? target : (uint8_t)(value + intensity);
    if (value > target) return value - intensity < target ? target : (uint8_t)(value - intensity);
    return value;
}

/* CutsceneUpdateSpecialEffect, the background half (the sprite effects are
 * not used by the ported cutscenes). */
static void update_special_effect(MzmCutscene *c, MzmScreen *s) {
    MzmCutsceneEffect *e = &c->effect;
    if (!(e->status & EFFECT_ON_BG)) return;
    if (e->bg_timer > 0) {
        e->bg_timer--;
        return;
    }
    e->bg_timer = e->bg_interval;
    s->bldalpha_l = step_towards(s->bldalpha_l, e->bg_alpha_l, e->bg_intensity);
    s->bldalpha_h = step_towards(s->bldalpha_h, e->bg_alpha_h, e->bg_intensity);
    if (s->bldalpha_l != e->bg_alpha_l || s->bldalpha_h != e->bg_alpha_h) return;
    e->status = (uint8_t)((e->status & ~EFFECT_ON_BG) | EFFECT_BG_ENDED);
}

/* CutsceneVBlank */
static void vblank(MzmCutscene *c, MzmScreen *s) {
    GbaPpu *ppu = s->ppu;
    mzm_screen_copy_oam(s);
    for (int bg = 0; bg < 4; ++bg) {
        gba_ppu_write16(ppu, GBA_REG_BG0HOFS + 4u * (unsigned)bg, c->bg_hofs[bg]);
        gba_ppu_write16(ppu, GBA_REG_BG0VOFS + 4u * (unsigned)bg, c->bg_vofs[bg]);
        gba_ppu_write16(ppu, GBA_REG_BG0CNT + 2u * (unsigned)bg, c->bgcnt[bg]);
    }
    gba_ppu_write16(ppu, GBA_REG_BLDY, s->bldy);
    gba_ppu_write16(ppu, GBA_REG_BLDALPHA, (uint16_t)(s->bldalpha_h << 8 | s->bldalpha_l));
    gba_ppu_write16(ppu, GBA_REG_BLDCNT, c->bldcnt);
    gba_ppu_write16(ppu, GBA_REG_DISPCNT, c->dispcnt);
}

/* StoryTextCutsceneInit */
static void story_init(MzmCutscene *c, MzmScreen *s) {
    GbaPpu *ppu = s->ppu;
    memset(&c->message, 0, sizeof c->message);
    c->message.message_id = (uint8_t)(story_text(c->cutscene) - 1);
    /* CutsceneFadeScreenToBlack */
    c->dispcnt = 0;
    s->bldy = BLDY_MAX;
    c->bldcnt = BLDCNT_SCREEN_FIRST | BLDCNT_BRIGHTNESS_DOWN;
    mzm_copy_from_rom(&ppu->palette[15 * 16], sym("sStoryTextCutscenePal"), 32);
    ppu->palette[0] = 0;
    mzm_lz77_from_rom(sym("sStoryTextCutsceneTileTable"), ppu->vram + STORY_TILETABLE_PAGE * 0x800,
                      GBA_VRAM_SIZE - STORY_TILETABLE_PAGE * 0x800);
    memset(ppu->vram + STORY_TEXT_VRAM, 0, 0x5000);
    /* CutsceneSetBgcntPageData */
    c->bgcnt[0] = (uint16_t)(STORY_TILETABLE_PAGE << 8 | STORY_GRAPHICS_PAGE << 2);
    s->bldy = BLDY_MAX;
    s->bldalpha_l = BLDALPHA_MAX;
    s->bldalpha_h = 0;
    switch (c->message.message_id) {
    case STORY_TEXT_PLANET_ZEBES:
    case STORY_TEXT_THE_TIMING:
    case STORY_TEXT_COULD_I_SURVIVE:
        c->dispcnt = 0;
        c->bldcnt = BLDCNT_BG0_FIRST | BLDCNT_ALPHA | BLDCNT_BACKDROP_SECOND;
        s->bldalpha_l = 0;
        s->bldalpha_h = BLDALPHA_MAX;
        set_bg_position(s, true, true, 0, START_BG_POS);
        break;
    default:
        break;
    }
    c->timer = 0;
    c->sub_stage = 0;
    c->stage++;
}

static void next_stage(MzmCutscene *c) {
    c->timer = 0;
    c->sub_stage = 0;
    c->stage++;
}

/* StoryTextCutsceneProcessText */
static void story_process_text(MzmCutscene *c, MzmScreen *s) {
    uint8_t *dst = s->ppu->vram + STORY_TEXT_VRAM;
    MzmMessage *m = &c->message;
    switch (c->sub_stage) {
    case 0:
        m->gfx_slot = 0;
        m->line = 0;
        m->stage = c->stage;
        c->sub_stage++;
        /* fall through */
    case 1:
        m->indent = 0;
        m->timer = 0;
        if (m->line > 9) m->line = 0;
        memset(dst + m->line * 0x800u, 0, 0x800);
        c->sub_stage++;
        break;
    case 2: {
        uint32_t text = mzm_rom_u32(sym("sEnglishTextPointers_Story") + m->message_id * 4u);
        for (int line = 8; line != 0; --line) {
            int result = mzm_text_process_message(m, text, dst + m->line * 0x800u, s->button_input);
            bool more = false;
            switch (result) {
            case MZM_TEXT_ENDED:
            case MZM_TEXT_NEW_PAGE:
                next_stage(c);
                break;
            case MZM_TEXT_UNK_3:
                m->gfx_slot = 1;
                /* fall through */
            case MZM_TEXT_NEW_LINE:
                c->sub_stage--;
                break;
            default:
                more = true;
                break;
            }
            if (!more) break;
            if (m->line > 9) {
                next_stage(c);
                break;
            }
        }
        break;
    }
    default:
        break;
    }
}

/* StoryTextCutsceneSetVerticalOffset */
static void story_set_vertical_offset(MzmCutscene *c, MzmScreen *s) {
    uint32_t position = START_BG_POS;
    position -= (uint32_t)((10 - c->message.line) / 2 * 64);
    if (c->message.line & 1) position -= 32;  /* HALF_BLOCK_SIZE */
    set_bg_position(s, false, true, 0, (uint16_t)position);
    next_stage(c);
}

/* StoryTextCutsceneFadeIn */
static void story_fade_in(MzmCutscene *c) {
    switch (c->sub_stage) {
    case 0:
        c->dispcnt |= STORY_BG;
        start_background_effect(c, c->bldcnt, BLDALPHA_MAX, 0, 4, 1);
        if (c->cutscene == MZM_CUTSCENE_INTRO_TEXT) mzm_sfx_play(SOUND_INTRO_TEXT_LANDING);
        c->sub_stage++;
        break;
    case 1:
        if (c->effect.status & EFFECT_BG_ENDED) next_stage(c);
        break;
    default:
        break;
    }
}

/* StoryTextCutsceneCheckInput */
static void story_check_input(MzmCutscene *c, MzmScreen *s) {
    switch (c->sub_stage) {
    case 0:
        if (c->timer > 120) c->sub_stage = c->message.gfx_slot != 0 ? 2 : 1;
        break;
    case 1:
    case 2:
        if (s->changed_input & (KEY_A | KEY_B)) next_stage(c);
        break;
    default:
        break;
    }
}

/* StoryTextCutsceneFadeOut */
static void story_fade_out(MzmCutscene *c) {
    switch (c->sub_stage) {
    case 0:
        start_background_effect(c, c->bldcnt, 0, BLDALPHA_MAX, 4, 1);
        c->sub_stage++;
        break;
    case 1:
        if (c->effect.status & EFFECT_BG_ENDED) {
            if (c->message.message_ended) c->dispcnt = 0;
            else c->dispcnt &= (uint16_t)~STORY_BG;
            next_stage(c);
        }
        break;
    default:
        break;
    }
}

/* StoryTextCutsceneEnd */
static bool story_end(MzmCutscene *c, MzmScreen *s) {
    if (c->message.message_ended) return true;
    memset(s->ppu->vram + STORY_TEXT_VRAM + c->sub_stage * 0x1000u, 0, 0x1000);
    c->sub_stage++;
    if (c->sub_stage > 4) {
        c->timer = 0;
        c->sub_stage = 0;
        c->stage = c->message.stage;
    }
    return false;
}

/* StoryTextCutsceneHandler */
static bool story_handler(MzmCutscene *c, MzmScreen *s) {
    bool ended = false;
    switch (c->stage) {
    case 0: story_init(c, s); break;
    case 1: story_process_text(c, s); break;
    case 2: story_set_vertical_offset(c, s); break;
    case 3: story_fade_in(c); break;
    case 4: story_check_input(c, s); break;
    case 5: story_fade_out(c); break;
    case 6: ended = story_end(c, s); break;
    default: break;
    }
    update_backgrounds_position(c, s);
    return ended;
}

/* CutsceneInit */
static void cutscene_init(MzmCutscene *c, MzmScreen *s) {
    uint8_t cutscene = c->cutscene, game_mode_stage = c->game_mode_stage;
    memset(c, 0, sizeof *c);
    c->cutscene = cutscene;
    c->game_mode_stage = game_mode_stage;
    s->oam_x_offset = s->oam_y_offset = 0;
    s->next_oam_slot = 0;
    mzm_reset_free_oam(s);
    gba_ppu_write16(s->ppu, GBA_REG_BLDCNT, c->bldcnt = 0xFF);
    gba_ppu_write16(s->ppu, GBA_REG_BLDY, s->bldy = BLDY_MAX);
    gba_ppu_write16(s->ppu, GBA_REG_DISPCNT, c->dispcnt = 0);
    mzm_clear_gfx_ram(s);
    for (int bg = 0; bg < 4; ++bg) s->bg_hofs[bg] = s->bg_vofs[bg] = START_BG_POS;
    c->stage = 0;
    c->timer = 0;
    story_handler(c, s);
}

bool mzm_cutscene_enter(MzmCutscene *c, uint8_t cutscene) {
    memset(c, 0, sizeof *c);
    c->cutscene = cutscene;
    c->game_mode_stage = STAGE_STARTING;
    return story_text(cutscene) != 0;
}

bool mzm_cutscene_update(MzmCutscene *c, MzmScreen *s) {
    switch (c->game_mode_stage) {
    case STAGE_STARTING:
        /* CutsceneStartBackgroundFading(COLOR_FADING_DOOR_TRANSITION) cannot
         * start: the fade is skipped. */
        c->game_mode_stage = STAGE_INIT;
        return false;
    case STAGE_INIT:
        cutscene_init(c, s);
        c->game_mode_stage++;
        break;
    case STAGE_ONGOING:
        c->timer++;
        update_special_effect(c, s);
        if (story_handler(c, s)) {
            c->dispcnt = 0;
            c->game_mode_stage++;
        }
        break;
    case STAGE_ENDING: {
        /* The palettes, the graphics and the OAM are cleared, then
         * CutsceneEnd. */
        GbaPpu *ppu = s->ppu;
        memset(ppu->palette, 0, sizeof ppu->palette);
        for (int at = 0; at < GBA_OBJ_VRAM; at += 2) {
            ppu->vram[at] = 0x40;
            ppu->vram[at + 1] = 0;
        }
        memset(ppu->vram + GBA_OBJ_VRAM, 0, GBA_VRAM_SIZE - GBA_OBJ_VRAM);
        memset(s->oam, 0xFF, sizeof s->oam);
        mzm_screen_copy_oam(s);
        c->game_mode_stage = STAGE_STARTING;
        c->cutscene = MZM_CUTSCENE_NONE;
        return true;
    }
    default:
        break;
    }
    vblank(c, s);
    return false;
}
