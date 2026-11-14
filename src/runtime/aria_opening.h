/* SPDX-License-Identifier: GPL-3.0-only */
/* Opening-scene host cluster of the Aria of Sorrow room runtime,
 * extracted from aria_runtime.c (AGENTS.md rule 8, decomposition wave
 * 4): the first-room opening story's verified-ROM text pipeline
 * (resolve/rebuild/decode/flatten), its backdrop, portrait, dialogue
 * and obtained-soul notice drawing, the Object20 cue-to-audio bridge
 * and the room-side controller probe that decides whether the scene
 * starts. The scene state itself (AosOpeningEvent, AosOpeningActors,
 * the tiles and the portrait texture) stays owned by aria_runtime.c's
 * main loop; the actor draw and motion callbacks live in aria_draw.c.
 * draw_opening_soul_notice is shared with the gameplay obtained-soul
 * banner, which stays in aria_runtime.c. */
#ifndef ARIA_OPENING_H
#define ARIA_OPENING_H

/* Same include order as aria_runtime.c: the opening-event header
 * precedes the shared types (AOS_OPENING_ACTOR_COUNT). */
#include "aos/title/aos_opening_event.h"

#include "aria_runtime_types.h"

#include "aos/intro/aos_intro_font.h"
#include "aos/intro/aos_intro_resources.h"
#include "aos/souls/aos_soul_notice.h"
#include "aos/menus/aos_window_frame.h"
#include "aos/core/aos_room_music.h"

#include "fusion/assets/rom.h"

/* Whether the room carries Object20's opening controller
 * (AOS_OPENING_OBJECT_ID / AOS_OPENING_OBJECT_PARAMETER). */
bool room_has_opening_controller(const AriaRoom *room);

/* Text-table resolver handed to aos_opening_event_set_resolver. */
bool resolve_opening_text(void *context, uint8_t id,
                          const uint8_t **script, size_t *script_size);
/* Rebuilds the dialogue glyph tiles for the current event entry. */
bool rebuild_opening_text(const AosOpeningEvent *event,
                          const AosIntroFont *font,
                          AosIntroTextTiles *tiles);
/* Decodes one verified-ROM text entry through the bounded parser. */
bool decode_opening_soul_entry(AosIntroResources *resources,
                               uint16_t id, AosOpeningEvent *text);
/* Flattens an event's glyph lines into a linear buffer. */
size_t flatten_opening_text(const AosOpeningEvent *text, uint8_t *out,
                            size_t capacity);

/* Draws an AosSoulNotice banner (opening soul notice and the
 * gameplay obtained-soul notice share it). */
void draw_opening_soul_notice(SDL_Renderer *renderer,
                              const AosSoulNotice *notice,
                              uint32_t frame);
/* WIN0 half-black veil plus the decoded native portrait BG0. */
void draw_opening_backdrop(SDL_Renderer *renderer,
                           const AosOpeningEvent *event,
                           const FusionRom *rom,
                           SDL_Texture **portrait_texture,
                           uint8_t *portrait_id);
/* The window-frame dialogue pixels and the blinking advance prompt. */
void draw_opening_text(SDL_Renderer *renderer,
                       const AosOpeningEvent *event,
                       const AosIntroTextTiles *tiles,
                       const AosWindowFrame *window,
                       uint32_t frame);
/* Maps an AosOpeningCue to its native song/MUS_ change. */
void play_opening_cue(AosOpeningCue cue, AosRoomMusic *music);

#endif
