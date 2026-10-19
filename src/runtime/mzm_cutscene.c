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

/* CutsceneFadeScreenToBlack */
static void fade_screen_to_black(MzmCutscene *c, MzmScreen *s) {
    c->dispcnt = 0;
    s->bldy = BLDY_MAX;
    c->bldcnt = BLDCNT_SCREEN_FIRST | BLDCNT_BRIGHTNESS_DOWN;
}

/* PAL_TO_FADE and PAL_WITH_FADE: the BG then OBJ palettes at the start of
 * the EWRAM work area. */
static uint16_t *pal_to_fade(MzmScreen *s) { return (uint16_t *)(void *)s->ewram; }
static uint16_t *pal_with_fade(MzmScreen *s) { return (uint16_t *)(void *)(s->ewram + 0x400); }

/* CutsceneStartBackgroundFading(COLOR_FADING_CANCEL), the only fade the
 * ported cutscene starts: from black, one step every frame after 4. */
static void start_fading_cancel(MzmCutscene *c, MzmScreen *s) {
    c->fading_color = 0;
    c->fading_ready = false;
    c->fading_delay = 0;
    memcpy(pal_to_fade(s), s->ppu->palette, sizeof s->ppu->palette);
    memset(s->ppu->palette, 0, sizeof s->ppu->palette);
    memcpy(pal_with_fade(s), s->ppu->palette, sizeof s->ppu->palette);
    c->fading_stage = 0;
    c->fading_intensity = 1;
    c->fading_max_delay = 4;
    c->fading_type = MZM_FADING_IN;
}

/* CutsceneUpdateFading, stages 0 and 1 (the fade in) */
static bool update_fading(MzmCutscene *c, MzmScreen *s) {
    c->fading_delay++;
    switch (c->fading_stage) {
    case 0:
        if (c->fading_ready || c->fading_delay < c->fading_max_delay) break;
        if (c->fading_color < 32) {
            mzm_apply_fading_color(c->fading_type, c->fading_color, pal_to_fade(s), pal_with_fade(s), 0xFFFF);
            mzm_apply_fading_color(c->fading_type, c->fading_color, pal_to_fade(s) + 256, pal_with_fade(s) + 256,
                                   0xFFFF);
            c->fading_ready = true;
            if (c->fading_color == 31) c->fading_color++;
            else c->fading_color = (uint8_t)(c->fading_color + c->fading_intensity > 31
                                                 ? 31 : c->fading_color + c->fading_intensity);
        } else {
            memcpy(pal_with_fade(s), pal_to_fade(s), 0x400);
            c->fading_ready = true;
            c->fading_stage++;
        }
        break;
    case 1:
        if (!c->fading_ready) {
            c->fading_color = 0;
            c->fading_stage = 0;
            return true;
        }
        break;
    default:
        break;
    }
    return false;
}

/* CutsceneTransferAndUpdateFade */
static bool transfer_and_update_fade(MzmCutscene *c, MzmScreen *s) {
    if (c->fading_ready) {
        memcpy(s->ppu->palette, pal_with_fade(s), 0x400);
        c->fading_ready = false;
    }
    return update_fading(c, s);
}

/* UpdateCutsceneOamDataId */
static void cutscene_oam_set_id(MzmCutsceneOam *o, uint8_t oam_id) {
    o->oam.oam_id = oam_id;
    o->oam.exists = oam_id ? 2 : 0;
}

enum {
    /* sSamusInBlueShipPageData[0] */
    BLUE_SHIP_BG = GBA_DCNT_BG3, BLUE_SHIP_BG_INDEX = 3, BLUE_SHIP_TILETABLE_PAGE = 0x1F,
    BLUE_SHIP_PRIORITY = 3,
    BLDCNT_OBJ_FIRST = 1 << 4, BLDCNT_SCREEN_SECOND = 0x3F00,
    SOUND_BLUE_SHIP_TURNING_ON = 0x253, SOUND_BLUE_SHIP_POWERING_UP = 0x254,
};

/* SamusInBlueShipInit */
static void blue_ship_init(MzmCutscene *c, MzmScreen *s) {
    GbaPpu *ppu = s->ppu;
    fade_screen_to_black(c, s);
    uint32_t size = 0, palette = mzm_rom_symbol("sSamusInBlueShipPal", &size);
    if (size > 0x200) size = 0x200;
    mzm_copy_from_rom(&ppu->palette[256], palette, size);
    mzm_copy_from_rom(&ppu->palette[0], palette, size);
    ppu->palette[0] = 0;
    mzm_lz77_from_rom(sym("sSamusInBlueShipSamusGfx"), ppu->vram, GBA_OBJ_VRAM);
    mzm_lz77_from_rom(sym("sSamusInBlueShipSamusTileTable"), ppu->vram + BLUE_SHIP_TILETABLE_PAGE * 0x800,
                      GBA_OBJ_VRAM - BLUE_SHIP_TILETABLE_PAGE * 0x800);
    mzm_lz77_from_rom(sym("sSamusInBlueShipControlsGfx"), ppu->vram + GBA_OBJ_VRAM, GBA_VRAM_SIZE - GBA_OBJ_VRAM);
    /* CutsceneSetBgcntPageData */
    c->bgcnt[BLUE_SHIP_BG_INDEX] = (uint16_t)(BLUE_SHIP_TILETABLE_PAGE << 8 | BLUE_SHIP_PRIORITY);
    /* CutsceneReset */
    s->bldy = 0;
    s->bldalpha_l = 16;
    s->bldalpha_h = 0;
    c->effect.status = 0;
    c->bldcnt = 0;
    memset(c->oam, 0, sizeof c->oam);
    c->oam[0].oam.bound_background = 4;
    memset(c->graphics, 0, sizeof c->graphics);
    c->bldcnt = BLDCNT_OBJ_FIRST | BLDCNT_ALPHA | BLDCNT_SCREEN_SECOND;
    s->bldalpha_l = BLDALPHA_MAX;
    s->bldalpha_h = 0;
    set_bg_position(s, true, true, BLUE_SHIP_BG_INDEX, START_BG_POS);
    cutscene_oam_set_id(&c->oam[0], 1);
    c->oam[0].oam.x = START_BG_POS + 64 * 7 + 32;
    c->oam[0].oam.y = START_BG_POS + 64 * 5 - 4;
    c->oam[0].oam.priority = 0;
    c->oam[0].oam.bound_background = 3;
    start_fading_cancel(c, s);
    c->dispcnt = GBA_DCNT_OBJ | BLUE_SHIP_BG;
    next_stage(c);
}

/* SamusInBlueShipShakeScreen: BG3 shaking a pixel either way. */
static void blue_ship_shake(MzmCutscene *c, MzmScreen *s) {
    if (!c->graphics[0].active) return;
    c->graphics[0].timer++;
    if (c->graphics[0].timer & 1) return;
    if (c->graphics[0].timer & 2) s->bg_hofs[BLUE_SHIP_BG_INDEX] += 4;
    else s->bg_hofs[BLUE_SHIP_BG_INDEX] -= 4;
}

/* SamusInBlueShipUpdateControlPanel: the panel lighting up, then its
 * glow flickering through sSamusInBlueShipPanelTransparency. */
static void blue_ship_update_panel(MzmCutsceneOam *o, MzmScreen *s) {
    static const uint8_t transparency[4] = {16, 15, 14, 15};
    switch (o->actions) {
    case 1:
        cutscene_oam_set_id(o, 2);
        mzm_sfx_play(SOUND_BLUE_SHIP_TURNING_ON);
        s->bldalpha_l = BLDALPHA_MAX;
        s->bldalpha_h = 0;
        o->actions++;
        break;
    case 2:
        if (o->oam.ended) {
            o->timer = 0;
            o->actions++;
        }
        break;
    case 3:
        o->unk_1a = 0;
        o->unk_1e = 2;
        o->timer = 0;
        o->actions++;
        break;
    case 4:
        if (o->unk_1a == 0) {
            o->unk_1a = o->unk_1e;
            if (++o->timer >= 4) o->timer = 0;
            s->bldalpha_l = transparency[o->timer];
            s->bldalpha_h = (uint8_t)(BLDALPHA_MAX - s->bldalpha_l);
        } else {
            o->unk_1a--;
        }
        break;
    default:
        break;
    }
}

/* SamusInBlueShipPoweringUp */
static void blue_ship_powering_up(MzmCutscene *c, MzmScreen *s) {
    switch (c->sub_stage) {
    case 0:
        if (transfer_and_update_fade(c, s)) {
            c->timer = 0;
            c->sub_stage++;
        }
        break;
    case 1:
        if (c->timer > 30) {                           /* CONVERT_SECONDS(.5f) */
            c->oam[0].actions = 1;
            c->timer = 0;
            c->sub_stage++;
        }
        break;
    case 2:
        if (c->oam[0].actions == 4) {
            c->timer = 0;
            c->sub_stage++;
        }
        break;
    case 3:
        if (c->timer > 30) {
            mzm_sfx_play(SOUND_BLUE_SHIP_POWERING_UP);
            c->graphics[0].active = true;
            c->graphics[0].timer = 0;
            c->timer = 0;
            c->sub_stage++;
        }
        break;
    case 4:
        if (c->timer > 90) {                           /* CONVERT_SECONDS(1.5f) */
            c->timer = 0;
            c->sub_stage++;
        }
        break;
    case 5:
        fade_screen_to_black(c, s);
        next_stage(c);
        break;
    default:
        break;
    }
    blue_ship_shake(c, s);
    blue_ship_update_panel(&c->oam[0], s);
}

/* SamusInBlueShipHandler: the stage, the backgrounds' positions, then
 * SamusInBlueShipProcessOam. */
static bool blue_ship_handler(MzmCutscene *c, MzmScreen *s) {
    bool ended = false;
    switch (c->stage) {
    case 0: blue_ship_init(c, s); break;
    case 1: blue_ship_powering_up(c, s); break;
    default: ended = true; break;                      /* CutsceneEndFunction */
    }
    update_backgrounds_position(c, s);
    s->next_oam_slot = 0;
    MzmMenuOam *o = &c->oam[0].oam;
    mzm_menu_oam_process(s, 1, o, sym("sSamusInBlueShipOam"));
    mzm_reset_free_oam(s);
    return ended;
}

static bool handler(MzmCutscene *c, MzmScreen *s) {
    return c->cutscene == MZM_CUTSCENE_SAMUS_IN_BLUE_SHIP ? blue_ship_handler(c, s) : story_handler(c, s);
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
    handler(c, s);
}

bool mzm_cutscene_enter(MzmCutscene *c, uint8_t cutscene) {
    memset(c, 0, sizeof *c);
    c->cutscene = cutscene;
    c->game_mode_stage = STAGE_STARTING;
    return story_text(cutscene) != 0 || cutscene == MZM_CUTSCENE_SAMUS_IN_BLUE_SHIP;
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
        if (handler(c, s)) {
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
