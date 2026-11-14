/* SPDX-License-Identifier: GPL-3.0-only */
/* Opening-scene host cluster of the Aria of Sorrow room runtime, moved
 * verbatim out of aria_runtime.c (AGENTS.md rule 8, decomposition wave
 * 4). See aria_opening.h. */
#include "aria_opening.h"

#include "aos/core/aos_audio.h"
#include "aos/core/aos_input.h"
#include "aos/menus/aos_portrait.h"

#include <stdio.h>
#include <string.h>

bool room_has_opening_controller(const AriaRoom *room) {
    if (!room) return false;
    for (size_t index = 0; index < room->entity_count; ++index) {
        const AriaEntity *entity = &room->entities[index];
        if (entity->kind == ARIA_KIND_SPECIAL &&
            entity->id == AOS_OPENING_OBJECT_ID &&
            entity->param0 == AOS_OPENING_OBJECT_PARAMETER)
            return true;
    }
    return false;
}

bool resolve_opening_text(void *context, uint8_t id,
                                 const uint8_t **script,
                                 size_t *script_size) {
    return aos_intro_resources_entry(context, 1, id, script, script_size);
}

bool rebuild_opening_text(const AosOpeningEvent *event,
                                 const AosIntroFont *font,
                                 AosIntroTextTiles *tiles) {
    aos_intro_text_tiles_clear(tiles);
    const char *speaker = aos_opening_event_speaker_name(event->speaker);
    /* Context B maps its row 6 (tiles 0xC0..0xCA) at screen (40,100).
     * Context A maps tiles 0x00..0x95 at screen (40,116). */
    if (!aos_intro_font_render_small_span(font, tiles, 0, 48, 88, 8,
                                          (const uint8_t *)speaker,
                                          strlen(speaker)))
        return false;
    for (unsigned line = 0; line < AOS_OPENING_LINES; ++line)
        if (!aos_intro_font_render_span(font, tiles, 0, line * 12,
                                        176, 8, event->glyphs[line],
                                        event->glyph_counts[line]))
            return false;
    return true;
}

/* sub_0800E708(0, 0, 1) selects text 0xE3 for Winged Skeleton and
 * sub_0800E540 selects 0x1E4 for its description. Decode those verified-ROM
 * scripts through the same bounded parser as dialogue; no localized string
 * is embedded in the runtime. */
bool decode_opening_soul_entry(AosIntroResources *resources,
                                      uint16_t id, AosOpeningEvent *text) {
    const uint8_t *script = NULL;
    size_t script_size = 0;
    if (!aos_intro_resources_entry(resources, 1, id, &script, &script_size))
        return false;
    if (!aos_opening_event_init(text, script, script_size)) return false;
    aos_opening_event_set_resolver(text, resolve_opening_text, resources);
    for (unsigned frame = 0; frame < 1024 && text->active; ++frame)
        aos_opening_event_update(text, text->waiting ? AOS_KEY_A : 0);
    return text->complete && !text->malformed;
}

size_t flatten_opening_text(const AosOpeningEvent *text, uint8_t *out,
                                   size_t capacity) {
    size_t count = 0;
    for (unsigned line = 0; line < AOS_OPENING_LINES; ++line)
        for (unsigned i = 0; i < text->glyph_counts[line] && count < capacity; ++i)
            out[count++] = text->glyphs[line][i];
    return count;
}

void draw_opening_soul_notice(SDL_Renderer *renderer,
                                     const AosSoulNotice *notice,
                                     uint32_t frame) {
    for (unsigned y = 0; y < VIEW_H; ++y)
        for (unsigned x = 0; x < 240; ++x)
        {
            uint16_t color;
            if (!aos_soul_notice_pixel(notice, frame, x, y, &color)) continue;
            SDL_SetRenderDrawColor(renderer,
                                   (uint8_t)((color & 31u) * 255u / 31u),
                                   (uint8_t)(((color >> 5) & 31u) * 255u / 31u),
                                   (uint8_t)(((color >> 10) & 31u) * 255u / 31u), 255);
            SDL_RenderPoint(renderer, (float)x, (float)y);
        }
}

static bool load_opening_portrait(SDL_Renderer *renderer, const FusionRom *rom,
                                  uint8_t portrait, SDL_Texture **texture,
                                  uint8_t *loaded_portrait) {
    if (!renderer || !rom || !texture || !loaded_portrait ||
        portrait >= AOS_PORTRAIT_COUNT)
        return false;
    if (*texture && *loaded_portrait == portrait) return true;
    if (*texture) {
        SDL_DestroyTexture(*texture);
        *texture = NULL;
    }
    AosPortraitImage image;
    char error[128];
    if (!aos_portrait_decode(rom, portrait, &image, error, sizeof error))
        return false;
    SDL_Texture *decoded = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STATIC,
                                             AOS_PORTRAIT_WIDTH, AOS_PORTRAIT_HEIGHT);
    if (!decoded) return false;
    if (!SDL_UpdateTexture(decoded, NULL, image.rgba, AOS_PORTRAIT_WIDTH * 4)) {
        SDL_DestroyTexture(decoded);
        return false;
    }
    SDL_SetTextureBlendMode(decoded, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(decoded, SDL_SCALEMODE_NEAREST);
    *texture = decoded;
    *loaded_portrait = portrait;
    return true;
}

void draw_opening_backdrop(SDL_Renderer *renderer,
                                  const AosOpeningEvent *event,
                                  const FusionRom *rom,
                                  SDL_Texture **portrait_texture,
                                  uint8_t *portrait_id) {
    if (!event->active) return;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    /* sub_08067804 selects brightness decrease and BLDY=8 inside WIN0.
     * BG1-3, OBJ and the backdrop are targets; BG0 (portrait and text) is
     * deliberately excluded. A half-black veil reproduces that operation. */
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 128);
    SDL_FRect panel = {AOS_OPENING_TEXT_PANEL_X,
                       AOS_OPENING_TEXT_PANEL_Y,
                       AOS_OPENING_TEXT_PANEL_WIDTH,
                       AOS_OPENING_TEXT_PANEL_HEIGHT};
    SDL_RenderFillRect(renderer, &panel);
    /* sub_080676BC draws the portrait as BG0, using the 12-byte descriptor
     * at 0x08526C78: tiles, palette row 13 and native metatile map. The
     * decoded texture is a full transparent 240x160 screen layer. Only the
     * native portrait resource is drawn; a decode failure is reported once
     * per portrait id instead of substituting gameplay sprites. */
    static uint8_t reported_portrait = 0xFF;
    bool native_portrait = load_opening_portrait(renderer, rom, event->portrait,
                                                 portrait_texture, portrait_id);
    if (!native_portrait && reported_portrait != event->portrait) {
        reported_portrait = event->portrait;
        fprintf(stderr, "Aria opening portrait 0x%02X unavailable: "
                "native portrait resource could not be decoded\n",
                event->portrait);
    }
    if (native_portrait && portrait_texture && *portrait_texture) {
        /* The decoded map already includes BG0VOFS. Crop its first four
         * output rows at composition time to match screenshots/dialogue.jpeg's
         * visible portrait anchor without altering the reusable resource. */
        SDL_FRect source = {0, 4, VIEW_W, VIEW_H - 4};
        SDL_FRect destination = {0, 0, VIEW_W, VIEW_H - 4};
        SDL_RenderTexture(renderer, *portrait_texture, &source, &destination);
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

void draw_opening_text(SDL_Renderer *renderer,
                              const AosOpeningEvent *event,
                              const AosIntroTextTiles *tiles,
                              const AosWindowFrame *window,
                              uint32_t frame) {
    if (!event->active) return;
    if (!window) return;
    for (unsigned y = 88; y < 160; ++y)
        for (unsigned x = 8; x < 232; ++x) {
            uint16_t color;
            if (!aos_window_frame_dialogue_pixel(window, tiles, x, y, &color))
                continue;
            SDL_SetRenderDrawColor(renderer,
                                   (uint8_t)((color & 31u) * 255u / 31u),
                                   (uint8_t)(((color >> 5) & 31u) * 255u / 31u),
                                   (uint8_t)(((color >> 10) & 31u) * 255u / 31u),
                                   255);
            SDL_RenderPoint(renderer, (float)x, (float)y);
        }
    if (!event->waiting) return;
    /* sub_08067A04 writes one of two 8x16 pairs at BG0 cells (27,18) and
     * (27,19). gEwramData bit 3 selects the pair every eight frames. */
    unsigned variant = (frame & 8u) ? 0u : 1u;
    for (unsigned y = 0; y < 16; ++y) {
        uint16_t entry = (uint16_t)(0xF0CE + variant + (y >= 8 ? 0x10 : 0));
        for (unsigned x = 0; x < 8; ++x) {
            uint16_t color;
            if (!aos_window_frame_entry_pixel(window, entry, x, y & 7u, &color))
                continue;
            SDL_SetRenderDrawColor(renderer,
                                   (uint8_t)((color & 31u) * 255u / 31u),
                                   (uint8_t)(((color >> 5) & 31u) * 255u / 31u),
                                   (uint8_t)(((color >> 10) & 31u) * 255u / 31u),
                                   255);
            SDL_RenderPoint(renderer, AOS_OPENING_PROMPT_X + (float)x,
                            AOS_OPENING_PROMPT_Y + (float)y);
        }
    }
}

void play_opening_cue(AosOpeningCue cue, AosRoomMusic *music) {
    switch (cue) {
    case AOS_OPENING_CUE_CONFRONTATION:
        /* sub_0805D410 state 2: MUS_16, Confrontation. */
        aos_room_music_change(music, 0x10);
        break;
    case AOS_OPENING_CUE_ENEMY_APPEARS:
        /* State 3's monster reveal. */
        aos_audio_play_song(0x1B4);
        break;
    case AOS_OPENING_CUE_ENEMY_ATTACKS:
        /* State 4 starts Arikado's first attack animation. */
        aos_audio_play_song(0x1DF);
        break;
    case AOS_OPENING_CUE_ENEMY_ATTACKS_AGAIN:
        /* State 4, 0x10 frames after Arikado's first attack ends. */
        aos_audio_play_song(0x1E0);
        break;
    case AOS_OPENING_CUE_ENEMY_DEFEATED:
        /* State 5 removes the Winged Skeleton at timer 0xD0. */
        aos_audio_play_song(0x6B);
        break;
    case AOS_OPENING_CUE_SOUL_ABSORBED:
        /* State 5: SE_0x197 followed by sub_0805B878(0x1040), which mutes
         * music player 0 while sub_0800E708 displays the obtained soul. */
        aos_audio_play_song(0x197);
        aos_audio_play_song(AOS_AUDIO_PLAYER_MUTE_BASE);
        break;
    case AOS_OPENING_CUE_SOUL_NOTICE_DISMISS:
        /* State 5's modal window advance sound. */
        aos_audio_play_song(0x132);
        break;
    case AOS_OPENING_CUE_AFTER_BATTLE:
        /* sub_0805D410 state 6: MUS_23, Premonition. */
        aos_room_music_change(music, 0x17);
        break;
    case AOS_OPENING_CUE_COMPLETE:
        /* sub_0805D410 state 7's one-shot sub_0805B878(0x1041). */
        aos_audio_play_song(0x1041);
        break;
    default:
        break;
    }
}
