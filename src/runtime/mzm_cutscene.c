/* SPDX-License-Identifier: GPL-3.0-only */
/* The non-gameplay cutscenes (see mzm_cutscene.h); comments name the pinned
 * decompilation's routines. */
#include "mzm_cutscene.h"

#include <string.h>

#include "mzm_rom.h"
#include "mzm_sfx.h"
#include "mzm_sound_ids.h"

enum {
    /* CUTSCENE_STAGE_* */
    STAGE_STARTING, STAGE_FADING_IN, STAGE_INIT, STAGE_ONGOING, STAGE_ENDING,
    /* CUTSCENE_SPECIAL_EFFECT_STATUS_* */
    EFFECT_ON_SPRITE = 1 << 0, EFFECT_SPRITE_ENDED = 1 << 1, EFFECT_ON_BG = 1 << 2, EFFECT_BG_ENDED = 1 << 3,
    BLDCNT_SCREEN_FIRST = 0x3F, BLDCNT_BRIGHTNESS_UP = 2 << 6, BLDCNT_BRIGHTNESS_DOWN = 3 << 6, BLDCNT_ALPHA = 1 << 6,
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

/* CutsceneUpdateSpecialEffect: the sprite effect (BLDY) first, then the
 * background one (BLDALPHA). */
static void update_special_effect(MzmCutscene *c, MzmScreen *s) {
    MzmCutsceneEffect *e = &c->effect;
    if (!(e->status & (EFFECT_ON_BG | EFFECT_ON_SPRITE))) return;
    if (e->status & EFFECT_ON_SPRITE) {
        if (e->s_timer > 0) {
            e->s_timer--;
            return;
        }
        e->s_timer = e->s_interval;
        s->bldy = step_towards(s->bldy, e->s_bldy, e->s_intensity);
        if (s->bldy != e->s_bldy) return;
        e->status = (uint8_t)((e->status & ~EFFECT_ON_SPRITE) | EFFECT_SPRITE_ENDED);
        if (e->status & EFFECT_ON_BG) c->bldcnt = e->bg_bldcnt;
        return;
    }
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

/* CutsceneFadeScreenToWhite */
static void fade_screen_to_white(MzmCutscene *c, MzmScreen *s) {
    c->dispcnt = 0;
    s->bldy = BLDY_MAX;
    c->bldcnt = BLDCNT_SCREEN_FIRST | BLDCNT_BRIGHTNESS_UP;
}

/* CutsceneStartSpriteEffect */
static void start_sprite_effect(MzmCutscene *c, uint16_t bldcnt, uint8_t bldy, uint8_t interval,
                                uint8_t intensity) {
    MzmCutsceneEffect *e = &c->effect;
    e->status = (uint8_t)((e->status & ~EFFECT_SPRITE_ENDED) | EFFECT_ON_SPRITE);
    e->s_bldy = bldy;
    e->s_intensity = intensity;
    e->s_interval = e->s_timer = interval;
    c->bldcnt = e->s_bldcnt = bldcnt;
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

/* CutsceneSetBgcntPageData */
static void set_bgcnt_page(MzmCutscene *c, int bg, uint16_t screen_size, uint8_t priority, uint8_t tiletable,
                           uint8_t graphics) {
    c->bgcnt[bg] = (uint16_t)(screen_size | priority | tiletable << 8 | graphics << 2);
}

/* CutsceneReset's objects: sCutsceneOam_Empty (no bound background). */
static void reset_objects(MzmCutscene *c) {
    memset(c->oam, 0, sizeof c->oam);
    memset(c->oam_data, 0, sizeof c->oam_data);
    for (int i = 0; i < MZM_CUTSCENE_OBJECTS; ++i) c->oam[i].bound_background = 4;
}

/* CutsceneReset */
static void cutscene_reset(MzmCutscene *c, MzmScreen *s) {
    s->bldy = 0;
    s->bldalpha_l = 16;
    s->bldalpha_h = 0;
    c->effect.status = 0;
    c->bldcnt = 0;
    reset_objects(c);
    memset(c->graphics, 0, sizeof c->graphics);
    memset(&c->palette, 0, sizeof c->palette);
}

/* UpdateCutsceneOamDataId */
static void cutscene_oam_set_id(MzmMenuOam *o, uint8_t oam_id) {
    o->oam_id = oam_id;
    o->exists = oam_id ? 2 : 0;
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
    cutscene_reset(c, s);
    c->bldcnt = BLDCNT_OBJ_FIRST | BLDCNT_ALPHA | BLDCNT_SCREEN_SECOND;
    s->bldalpha_l = BLDALPHA_MAX;
    s->bldalpha_h = 0;
    set_bg_position(s, true, true, BLUE_SHIP_BG_INDEX, START_BG_POS);
    cutscene_oam_set_id(&c->oam[0], 1);
    c->oam[0].x = START_BG_POS + 64 * 7 + 32;
    c->oam[0].y = START_BG_POS + 64 * 5 - 4;
    c->oam[0].priority = 0;
    c->oam[0].bound_background = 3;
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
static void blue_ship_update_panel(MzmMenuOam *oam, MzmCutsceneOamData *o, MzmScreen *s) {
    static const uint8_t transparency[4] = {16, 15, 14, 15};
    switch (o->actions) {
    case 1:
        cutscene_oam_set_id(oam, 2);
        mzm_sfx_play(SOUND_BLUE_SHIP_TURNING_ON);
        s->bldalpha_l = BLDALPHA_MAX;
        s->bldalpha_h = 0;
        o->actions++;
        break;
    case 2:
        if (oam->ended) {
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
            c->oam_data[0].actions = 1;
            c->timer = 0;
            c->sub_stage++;
        }
        break;
    case 2:
        if (c->oam_data[0].actions == 4) {
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
    blue_ship_update_panel(&c->oam[0], &c->oam_data[0], s);
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
    mzm_menu_oam_process(s, 1, c->oam, sym("sSamusInBlueShipOam"));
    mzm_reset_free_oam(s);
    return ended;
}

enum {
    /* sGettingFullyPoweredSuitPageData[0] (Samus) and [1] (background) */
    FP_SAMUS_BG = 2, FP_SAMUS_TILETABLE = 0x1E, FP_SAMUS_PRIORITY = 2, FP_SAMUS_GRAPHICS = 0,
    FP_BACK_BG = 3, FP_BACK_TILETABLE = 0x1C, FP_BACK_PRIORITY = 3, FP_BACK_GRAPHICS = 2,
    FP_SCREEN_SIZE = 0x8000,
    /* GETTING_FULLY_POWERED_SUIT_OAM_ID_* */
    FP_OAM_SPARKLE_GOING_UP = 1, FP_OAM_SPARKLE_AROUND_RING1, FP_OAM_SPARKLE_AROUND_RING2,
    FP_OAM_RING_BOTTOM, FP_OAM_RING_TOP,
    /* The object slots */
    FP_RING_SPARKLES = 6, FP_SLOT_RING_BOTTOM = 6, FP_UP_SPARKLES_START = 7, FP_UP_SPARKLES_END = 12,
    FP_SLOT_RING_TOP = 13, FP_OAM_LENGTH = 14,
    FP_RING_ACTION_MOVE = 2, FP_RING_ACTION_SCALING_VELOCITY = 4,
    FP_RING_SPARKLE_SPAWN = 1, FP_RING_SPARKLE_FOLLOW = 2, FP_UP_SPARKLE_MOVE = 2,
    FP_RING_SPEED = 4,                                  /* PIXEL_SIZE */
    BLDCNT_BG2_FIRST = 1 << 2, BLDCNT_SECOND_ALL_BUT_BG2 = 0x3B00,
    WIN0_ALL = 0x3F, WIN1_ALL_NO_COLOR_EFFECT = 0x1F,
};

/* sGettingFullyPoweredSuitRingSparklesPositions (u16, added to the ring's
 * position) and sGettingFullyPoweredSuitUpSparklesXPositions */
static const int16_t fp_ring_sparkle_offsets[FP_RING_SPARKLES][2] = {
    {-(64 * 3 + 32 + 8), 0x18}, {-(64 + 32 + 8), 32 + 8}, {64 * 3 + 8, 0x18},
    {64 + 16, 32 + 12}, {64 * 4 + 32 - 8, -16}, {-(64 * 4 + 32), -20},
};
static const int16_t fp_up_sparkle_x[6] = {64 * 6 + 32, 64 * 10 + 8, 64 * 5, 64 * 12 + 16, 64 * 3, 0};

/* sRandomNumberTable */
static uint8_t random_number(unsigned index) { return mzm_rom_u8(sym("sRandomNumberTable") + (index & 0xFF)); }

/* sGettingFullyPoweredSuitBackgroundGfx's compressed bytes, read as noise */
static uint8_t fp_background_byte(unsigned index) {
    return mzm_rom_u8(sym("sGettingFullyPoweredSuitBackgroundGfx") + index);
}

/* GettingFullyPoweredSuitInit (the US fade to black) */
static void fully_powered_init(MzmCutscene *c, MzmScreen *s) {
    GbaPpu *ppu = s->ppu;
    fade_screen_to_black(c, s);
    mzm_copy_from_rom(&ppu->palette[0], sym("sGettingFullyPoweredSuitPal"), 11 * 32);
    memcpy(&ppu->palette[256], &ppu->palette[0], 512);
    ppu->palette[0] = 0;
    mzm_lz77_from_rom(sym("sGettingFullyPoweredSuitSamusGfx"), ppu->vram + FP_SAMUS_GRAPHICS * 0x4000,
                      GBA_OBJ_VRAM - FP_SAMUS_GRAPHICS * 0x4000);
    mzm_lz77_from_rom(sym("sGettingFullyPoweredSuitSamusTileTable"), ppu->vram + FP_SAMUS_TILETABLE * 0x800,
                      GBA_OBJ_VRAM - FP_SAMUS_TILETABLE * 0x800);
    mzm_lz77_from_rom(sym("sGettingFullyPoweredSuitBackgroundGfx"), ppu->vram + FP_BACK_GRAPHICS * 0x4000,
                      GBA_OBJ_VRAM - FP_BACK_GRAPHICS * 0x4000);
    mzm_lz77_from_rom(sym("sGettingFullyPoweredSuitBackgroundTileTable"), ppu->vram + FP_BACK_TILETABLE * 0x800,
                      GBA_OBJ_VRAM - FP_BACK_TILETABLE * 0x800);
    mzm_lz77_from_rom(sym("sGettingFullyPoweredSuitRingSparklesGfx"), ppu->vram + GBA_OBJ_VRAM,
                      GBA_VRAM_SIZE - GBA_OBJ_VRAM);
    set_bgcnt_page(c, FP_SAMUS_BG, FP_SCREEN_SIZE, FP_SAMUS_PRIORITY, FP_SAMUS_TILETABLE, FP_SAMUS_GRAPHICS);
    set_bgcnt_page(c, FP_BACK_BG, FP_SCREEN_SIZE, FP_BACK_PRIORITY, FP_BACK_TILETABLE, FP_BACK_GRAPHICS);
    cutscene_reset(c, s);
    /* The light below the ring: Samus brightened outside window 1 */
    c->bldcnt = BLDCNT_BG2_FIRST | BLDCNT_BRIGHTNESS_UP | BLDCNT_SECOND_ALL_BUT_BG2;
    s->bldy = 0;
    s->bldalpha_l = 0;
    s->bldalpha_h = BLDALPHA_MAX;
    set_bg_position(s, true, false, FP_SAMUS_BG, START_BG_POS);
    set_bg_position(s, true, false, FP_BACK_BG, START_BG_POS);
    set_bg_position(s, false, true, FP_SAMUS_BG, START_BG_POS + 64 * 10);
    set_bg_position(s, false, true, FP_BACK_BG, START_BG_POS + 64 * 10);

    MzmMenuOam *ring = &c->oam[FP_SLOT_RING_BOTTOM];
    ring->x = 64 * 8 - 16;
    ring->y = 64 * 22;
    ring->priority = FP_SAMUS_PRIORITY;
    ring->obj_mode = 0;
    ring->bound_background = FP_SAMUS_BG;
    cutscene_oam_set_id(ring, FP_OAM_RING_BOTTOM);
    c->oam[FP_SLOT_RING_TOP] = *ring;
    c->oam_data[FP_SLOT_RING_TOP] = c->oam_data[FP_SLOT_RING_BOTTOM];
    c->oam[FP_SLOT_RING_TOP].priority = FP_SAMUS_PRIORITY + 1;
    cutscene_oam_set_id(&c->oam[FP_SLOT_RING_TOP], FP_OAM_RING_TOP);
    for (int i = 0; i < FP_RING_SPARKLES; ++i) {
        c->oam[i].priority = FP_SAMUS_PRIORITY;
        c->oam[i].bound_background = FP_SAMUS_BG;
    }
    MzmMenuOam *up = &c->oam[FP_UP_SPARKLES_START];
    up->x = fp_up_sparkle_x[0];
    up->y = 64 * 11;
    up->priority = FP_SAMUS_PRIORITY;
    /* The native loop variable still indexes the ring bottom here, so this
     * timer lands on it rather than on the first rising sparkle. */
    c->oam_data[FP_RING_SPARKLES].timer = random_number(c->frame8) & 63;
    cutscene_oam_set_id(up, FP_OAM_SPARKLE_GOING_UP);
    for (int i = FP_UP_SPARKLES_START + 1; i < FP_UP_SPARKLES_END; ++i) {
        c->oam[i] = *up;
        c->oam_data[i] = c->oam_data[FP_UP_SPARKLES_START];
        c->oam[i].x = fp_up_sparkle_x[i - FP_UP_SPARKLES_START];
        c->oam_data[i].timer = fp_background_byte(c->frame8 + (unsigned)i) & 63;
    }
    c->palette.active = true;
    c->palette.timer = 0;
    c->palette.row = 0;
    c->palette.max_timer = 2;
    c->dispcnt = GBA_DCNT_OBJ | GBA_DCNT_WIN1 | GBA_DCNT_BG2 | GBA_DCNT_BG3;
    gba_ppu_write16(ppu, GBA_REG_WINOUT,
                    (uint16_t)((gba_ppu_read16(ppu, GBA_REG_WINOUT) & 0xFF00) | WIN0_ALL));
    gba_ppu_write16(ppu, GBA_REG_WININ,
                    (uint16_t)((gba_ppu_read16(ppu, GBA_REG_WININ) & 0xFF) | WIN1_ALL_NO_COLOR_EFFECT << 8));
    gba_ppu_write16(ppu, GBA_REG_WIN1H, 240);
    gba_ppu_write16(ppu, GBA_REG_WIN1V, 160);
    mzm_sfx_music_play(MZM_MUSIC_GETTING_FULLY_POWERED_SUIT_CUTSCENE, 0);
    next_stage(c);
}

/* GettingFullyPoweredSuitUpdateRingPalette: sGettingFullyPoweredSuitRingPal
 * cycled into OBJ row 10 every third frame. */
static void fully_powered_update_ring_palette(MzmCutscene *c, MzmScreen *s) {
    if (!c->palette.active) return;
    if (c->palette.timer != 0) {
        c->palette.timer--;
        return;
    }
    c->palette.timer = c->palette.max_timer;
    if (++c->palette.row >= 7) c->palette.row = 0;
    mzm_copy_from_rom(&s->ppu->palette[256 + 10 * 16], sym("sGettingFullyPoweredSuitRingPal") + c->palette.row * 32u,
                      32);
}

/* GettingFullyPoweredSuitUpdateRing */
static void fully_powered_update_ring(MzmCutscene *c) {
    MzmMenuOam *oam = &c->oam[FP_SLOT_RING_BOTTOM];
    MzmCutsceneOamData *o = &c->oam_data[FP_SLOT_RING_BOTTOM];
    if (o->actions & FP_RING_ACTION_SCALING_VELOCITY) {
        uint16_t unk = ++o->unk_18;
        o->y_velocity = (int16_t)(-FP_RING_SPEED - unk / FP_RING_SPEED);
    } else {
        o->y_velocity = -FP_RING_SPEED;
        o->unk_18 = 0;
    }
    if (o->actions & FP_RING_ACTION_MOVE) {
        oam->y = (int16_t)(oam->y + o->y_velocity);
        if (oam->y <= -(64 * 3)) o->actions = 0;
    }
    c->oam[FP_SLOT_RING_TOP].y = oam->y;
}

/* GettingFullyPoweredSuitUpdateSparkleAroundRing */
static void fully_powered_update_ring_sparkle(MzmCutscene *c, int id) {
    MzmMenuOam *oam = &c->oam[id];
    MzmCutsceneOamData *o = &c->oam_data[id];
    if (o->actions == 0) {
        o->timer = random_number((unsigned)(c->frame8 + id)) & 31;
        o->actions = FP_RING_SPARKLE_SPAWN;
    } else if (o->actions == FP_RING_SPARKLE_SPAWN) {
        if (o->timer != 0) {
            o->timer--;
        } else {
            cutscene_oam_set_id(oam, random_number(c->frame8) & 1 ? FP_OAM_SPARKLE_AROUND_RING2
                                                                  : FP_OAM_SPARKLE_AROUND_RING1);
            o->actions = FP_RING_SPARKLE_FOLLOW;
        }
    } else if (!oam->exists) {
        o->actions = 0;
    }
    if (oam->exists) {
        oam->x = (int16_t)(c->oam[FP_SLOT_RING_BOTTOM].x + fp_ring_sparkle_offsets[id][0]);
        oam->y = (int16_t)(c->oam[FP_SLOT_RING_BOTTOM].y + fp_ring_sparkle_offsets[id][1]);
    }
}

/* GettingFullyPoweredSuitUpdateSparkleGoingUp */
static void fully_powered_update_up_sparkle(MzmCutscene *c, int id) {
    MzmMenuOam *oam = &c->oam[id];
    MzmCutsceneOamData *o = &c->oam_data[id];
    if (o->actions & FP_UP_SPARKLE_MOVE) {
        uint16_t unk = ++o->unk_18;
        o->y_velocity = (int16_t)(o->y_velocity + unk / 16);
        if (o->y_velocity > 32) o->y_velocity = 32;
        oam->y = (int16_t)(oam->y - o->y_velocity);
        if (oam->y <= -(64 * 3 + 32)) {
            o->actions = 0;
            o->timer = fp_background_byte(c->frame8 + (unsigned)id) & 63;
        }
    } else if (o->timer == 0) {
        o->actions = FP_UP_SPARKLE_MOVE;
        o->y_velocity = 0;
        oam->x = (int16_t)(fp_up_sparkle_x[id - FP_UP_SPARKLES_START] + (random_number(c->frame8) & 63));
        oam->y = 160 * 4 + 64;
        o->unk_18 = 0;
    } else {
        o->timer--;
    }
}

/* GettingFullyPoweredSuitAnimation */
static void fully_powered_animation(MzmCutscene *c, MzmScreen *s) {
    MzmMenuOam *ring = &c->oam[FP_SLOT_RING_BOTTOM];
    switch (c->sub_stage) {
    case 0:
        start_sprite_effect(c, c->bldcnt, 12, 8, 1);
        c->timer = 0;
        c->sub_stage++;
        break;
    case 1:
        if (c->timer > 20) {                            /* ONE_THIRD_SECOND */
            c->oam_data[FP_SLOT_RING_BOTTOM].actions = FP_RING_ACTION_MOVE;
            c->timer = 0;
            c->sub_stage++;
        }
        break;
    case 2:
        if (ring->y <= 64 * 17) {
            c->timer = 0;
            c->sub_stage++;
        }
        break;
    case 3:
        if (s->bg_vofs[FP_SAMUS_BG] - FP_RING_SPEED > START_BG_POS) {
            s->bg_vofs[FP_SAMUS_BG] -= FP_RING_SPEED;
            break;
        }
        s->bg_vofs[FP_SAMUS_BG] = START_BG_POS;
        c->timer = 0;
        c->sub_stage++;
        break;
    case 4:
        if (ring->y < 64 * 8) {
            /* Window 1 off, against a white square during the final fade */
            c->dispcnt ^= GBA_DCNT_WIN1;
            start_sprite_effect(c, (uint16_t)(c->bldcnt | BLDCNT_SCREEN_FIRST), BLDY_MAX, 8, 1);
            c->oam_data[FP_SLOT_RING_BOTTOM].actions |= FP_RING_ACTION_SCALING_VELOCITY;
            c->timer = 0;
            c->sub_stage++;
        }
        break;
    case 5:
        if (c->timer > 60) {
            c->timer = 0;
            c->sub_stage++;
        }
        break;
    case 6:
        fade_screen_to_white(c, s);
        c->stage++;
        c->sub_stage = 0;
        c->timer = 0;
        break;
    default:
        break;
    }
    fully_powered_update_ring_palette(c, s);
    s->bg_vofs[FP_BACK_BG] = s->bg_vofs[FP_SAMUS_BG];
    fully_powered_update_ring(c);
    for (int i = 0; i < FP_RING_SPARKLES; ++i) fully_powered_update_ring_sparkle(c, i);
    /* Window 1 ends half a block below the ring's centre. */
    int line = s->bg_vofs[FP_SAMUS_BG] - START_BG_POS;
    line = ((ring->y - line) >> 2) + 8;
    if (line < 0) line = 0;
    if (line > 160) line = 160;
    gba_ppu_write16(s->ppu, GBA_REG_WIN1V, (uint16_t)line);
    for (int i = FP_UP_SPARKLES_START; i < FP_UP_SPARKLES_END; ++i) fully_powered_update_up_sparkle(c, i);
}

/* GettingFullyPoweredSuitHandler then GettingFullyPoweredSuitProcessOam */
static bool fully_powered_handler(MzmCutscene *c, MzmScreen *s) {
    bool ended = false;
    switch (c->stage) {
    case 0: fully_powered_init(c, s); break;
    case 1: fully_powered_animation(c, s); break;
    default: ended = true; break;                      /* CutsceneEndFunction */
    }
    update_backgrounds_position(c, s);
    s->next_oam_slot = 0;
    mzm_menu_oam_process(s, c->stage < 2 ? FP_OAM_LENGTH : 0, c->oam, sym("sGettingFullyPoweredSuitCutsceneOam"));
    mzm_reset_free_oam(s);
    return ended;
}

static bool handler(MzmCutscene *c, MzmScreen *s) {
    switch (c->cutscene) {
    case MZM_CUTSCENE_SAMUS_IN_BLUE_SHIP: return blue_ship_handler(c, s);
    case MZM_CUTSCENE_GETTING_FULLY_POWERED: return fully_powered_handler(c, s);
    default: return story_handler(c, s);
    }
}

/* CutsceneInit */
static void cutscene_init(MzmCutscene *c, MzmScreen *s) {
    uint8_t cutscene = c->cutscene, game_mode_stage = c->game_mode_stage, frame8 = c->frame8;
    memset(c, 0, sizeof *c);
    c->cutscene = cutscene;
    c->game_mode_stage = game_mode_stage;
    c->frame8 = frame8;
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
    return story_text(cutscene) != 0 || cutscene == MZM_CUTSCENE_SAMUS_IN_BLUE_SHIP ||
           cutscene == MZM_CUTSCENE_GETTING_FULLY_POWERED;
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
    c->frame8++;
    return false;
}
