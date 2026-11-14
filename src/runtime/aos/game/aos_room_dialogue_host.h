/* SPDX-License-Identifier: GPL-3.0-only */
/* PROPOSED HOST ADAPTER — SKETCH ONLY, NOT WIRED.
 *
 * This header documents the adapter code that would live in a clearly
 * marked section of src/runtime/aria_runtime.c once the parent agent
 * wires the Object20 SPECIAL id-32 dispatch. It is compiled here as
 * static inline functions so the exact call list stays honest and
 * syntax-checked; nothing in aria_runtime.c references it yet.
 *
 * ------------------------------------------------------------------
 * Where it plugs in
 * ------------------------------------------------------------------
 * update_entities (aria_runtime.c ~line 4223) gains, in the
 * ARIA_KIND_SPECIAL / id-32 (Object20) branch, per-entity:
 *
 *   - before the variant update:  in.text_done =
 *       aos_room_dialogue_text_done(&room_dialogue);
 *   - after the variant update:   build an AosRoomDialogueRequest
 *       {out.window_text, out.window_poll, out.window_close} and call
 *       aos_room_dialogue_apply(&room_dialogue, aria_input.new_input,
 *                               &request);
 *   - in the draw pass, beside draw_opening_text: while
 *       aos_room_dialogue_active(&room_dialogue), call
 *       aos_room_dialogue_render(&room_dialogue, step) and compose
 *       the tiles through aos_window_frame_dialogue_pixel exactly
 *       like draw_opening_text does for the opening.
 *   - while aos_room_dialogue_input_locked() is true, gameplay input
 *       (movement/jump/fire) is suppressed; A/START still reach
 *       aos_room_dialogue_poll through apply.
 *
 * One shared AosRoomDialogue instance suffices: the native routines
 * never run two text windows at once, and sub_08067934 re-open while
 * active closes the previous window first.
 *
 * ------------------------------------------------------------------
 * Existing functions the adapter calls (exact signatures)
 * ------------------------------------------------------------------
 * Text id -> script bytes (same wrapper as resolve_opening_text,
 * aria_runtime.c ~8733):
 *   bool aos_intro_resources_entry(AosIntroResources *resources,
 *       uint8_t language, uint16_t id, const uint8_t **entry,
 *       size_t *entry_size);          // aos/intro/aos_intro_resources.h
 *
 * Window/font/canvas bring-up (mirrors the opening init block,
 * aria_runtime.c ~9922-9936):
 *   bool aos_intro_resources_init(AosIntroResources *resources,
 *       const FusionRom *rom, char *error, size_t error_size);
 *   bool aos_intro_font_init(AosIntroFont *font, const FusionRom *rom,
 *       char *error, size_t error_size);
 *   bool aos_window_frame_init(AosWindowFrame *frame,
 *       const FusionRom *rom, uint8_t language, char *error,
 *       size_t error_size);
 *   void aos_intro_text_tiles_clear(AosIntroTextTiles *tiles);
 *   const char *aos_opening_event_speaker_name(uint8_t speaker);
 *   bool aos_intro_font_render_small_span(const AosIntroFont *font,
 *       AosIntroTextTiles *tiles, unsigned pixel_x, unsigned pixel_y,
 *       unsigned max_width, uint8_t color, const uint8_t *glyphs,
 *       size_t glyph_count);
 *   bool aos_intro_font_render_span(const AosIntroFont *font,
 *       AosIntroTextTiles *tiles, unsigned pixel_x, unsigned pixel_y,
 *       unsigned max_width, uint8_t color, const uint8_t *glyphs,
 *       size_t glyph_count);
 *   bool aos_window_frame_dialogue_pixel(const AosWindowFrame *frame,
 *       const AosIntroTextTiles *text, unsigned screen_x,
 *       unsigned screen_y, uint16_t *color);
 *   bool aos_window_frame_entry_pixel(const AosWindowFrame *frame,
 *       uint16_t entry, unsigned x, unsigned y, uint16_t *color);
 *
 * KNOWN GAPS the adapter must document, not paper over:
 *  - sub_08067934's window-register setup (WIN0H=0x08E8, WIN0V,
 *    DISPCNT) and sub_0805AFC0's open animation have no standalone
 *    primitive; the opening path presents the dialogue frame
 *    unconditionally while its scene is active. open_window below
 *    therefore only verifies the frame is initialised — the same
 *    approximation the opening uses.
 *  - sub_08067898's close has no primitive either; close_window only
 *    clears the adapter's presentation flag. The frame tiles persist
 *    in VRAM natively until overwritten; matching that exactly is
 *    unnecessary while no other BG0 user exists.
 *  - The input lock is a new bool consulted by the input stage; no
 *    existing modal-lock primitive covers gameplay input (the soul
 *    menu and pause have their own paths).
 * ------------------------------------------------------------------ */

#ifndef AOS_ROOM_DIALOGUE_HOST_H
#define AOS_ROOM_DIALOGUE_HOST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../intro/aos_intro_font.h"
#include "../intro/aos_intro_resources.h"
#include "../menus/aos_window_frame.h"
#include "aos_room_dialogue.h"

/* Everything the runtime section would own. Pointers reference the
 * same statics the opening path uses (opening_resources,
 * opening_font, opening_window, opening_tiles) or parallel room-
 * dialogue instances; sharing is safe because only one dialogue can
 * be active at a time natively. */
typedef struct {
    AosIntroResources *resources;
    AosIntroFont *font;
    AosIntroTextTiles *tiles;
    AosWindowFrame *window;
    uint8_t language;
    bool window_ready;       /* frame+font+resources initialised */
    bool input_locked;       /* consulted by the gameplay input stage */
    bool render_failed;      // sticky presentation failure flag
    /* Function-scope storage for the callback table; context points
     * back at this struct so the table can live as long as the
     * adapter does. */
    AosRoomDialogueHost host;
} AriaRoomDialogueAdapter;

static bool aria_room_dialogue_resolve(void *context, uint16_t id,
                                       const uint8_t **script,
                                       size_t *script_size) {
    AriaRoomDialogueAdapter *a = context;
    /* Identical to resolve_opening_text, widened to uint16_t ids. */
    return aos_intro_resources_entry(a->resources, a->language, id,
                                     script, script_size);
}

static bool aria_room_dialogue_open_window(void *context, uint16_t id) {
    AriaRoomDialogueAdapter *a = context;
    (void)id;
    /* Gap: sub_08067934's WIN0H/WIN0V/DISPCNT writes and the
     * sub_0805AFC0 open animation are not separately modelled; the
     * dialogue frame is composed whenever the bridge reports the
     * window open, matching the opening path's presentation. */
    return a->window_ready;
}

/* Mirrors rebuild_opening_text (aria_runtime.c ~8739): speaker row
 * through the small context-B font at (40,100), body glyphs at
 * (40,116) on 12-pixel rows. */
static void aria_room_dialogue_render(void *context,
                                      const AosRoomDialogue *dlg,
                                      uint32_t frame) {
    AriaRoomDialogueAdapter *a = context;
    const AosOpeningEvent *event = aos_room_dialogue_event(dlg);
    if (!event) return;
    aos_intro_text_tiles_clear(a->tiles);
    const char *speaker = aos_opening_event_speaker_name(event->speaker);
    bool ok =
        aos_intro_font_render_small_span(a->font, a->tiles, 0, 48, 88, 8,
                                         (const uint8_t *)speaker,
                                         strlen(speaker));
    for (unsigned line = 0; ok && line < AOS_OPENING_LINES; ++line)
        ok = aos_intro_font_render_span(a->font, a->tiles, 0, line * 12,
                                        176, 8, event->glyphs[line],
                                        event->glyph_counts[line]);
    a->render_failed = !ok;
    (void)frame;    /* the wait-marker blink uses the draw pass frame */
}

static void aria_room_dialogue_close_window(void *context) {
    AriaRoomDialogueAdapter *a = context;
    /* Gap: no sub_08067898 primitive; the frame stays in the canvas
     * until the next open overwrites it, as native VRAM does. */
    (void)a;
}

static void aria_room_dialogue_input_lock(void *context, bool locked) {
    AriaRoomDialogueAdapter *a = context;
    a->input_locked = locked;
}

/* One-time wiring the runtime section would run beside the opening
 * resources init (~line 8811): reuse the already-initialised
 * opening_resources/opening_font/opening_window when present, else
 * initialise parallel instances from the same FusionRom. */
static void aria_room_dialogue_bind(AriaRoomDialogueAdapter *a,
                                    AosRoomDialogue *dlg) {
    a->host.context = a;
    a->host.resolve_text = aria_room_dialogue_resolve;
    a->host.open_window = aria_room_dialogue_open_window;
    a->host.render = aria_room_dialogue_render;
    a->host.close_window = aria_room_dialogue_close_window;
    a->host.set_input_lock = aria_room_dialogue_input_lock;
    aos_room_dialogue_set_host(dlg, &a->host);
}

#endif
