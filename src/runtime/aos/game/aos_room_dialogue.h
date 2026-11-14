/* SPDX-License-Identifier: GPL-3.0-only */
/* Room-dialogue bridge for Aria of Sorrow Object20 entities.
 *
 * Native Object20 story/NPC/shop variants emit text-window requests
 * through their update Out structs: `window_text` is the argument of
 * sub_08067934 (open the dialogue window on a text-table id),
 * `window_poll` marks a frame where sub_08067A04 ran (the 7-state
 * text machine; its nonzero return is what the entity reads back as
 * `text_done` on the next update's In struct) and `window_close` is
 * the sub_08067898 window close. update_entities in
 * src/runtime/aria_runtime.c has no text-window access of its own,
 * so this module owns the portable state machine that turns those
 * request triples into real window/text behaviour through an
 * injectable host context.
 *
 * Location: src/runtime/aos/game/ rather than src/runtime/aos/menus/
 * because the module owns gameplay-facing state (which entity asked
 * for which text, input-lock pairing, per-frame poll ordering), not
 * presentation; the actual glyph/frame drawing stays host-side via
 * the render callback, reusing AosOpeningEvent/AosIntroFont/
 * AosWindowFrame/aos_intro_resources_entry unchanged.
 *
 * Integration status: portable bridge, unit-tested with a mock host.
 * NOT yet called from aria_runtime.c; the Object20 SPECIAL id-32
 * dispatch case and the host adapter in
 * aos_room_dialogue_host.h are the pending wiring. Classified
 * partial in data/native_parity/annotations.tsv.
 *
 * Script engine: the text machine reuses AosOpeningEvent
 * (src/runtime/aos/title/aos_opening_event.c), the existing port of
 * the sub_08067A04 print/wait/advance script semantics used by the
 * first-room opening. Divergence: its control-2 beat cues are the
 * opening scene's private mapping (AOS_OPENING_CUE_*); room-dialogue
 * hosts must ignore dlg->event.cue unless the script is the opening
 * one. Glyph/page geometry (4 lines x 80 glyphs, command 5 wait,
 * A/START advance) matches sub_08067A04's dialogue canvas.
 */

#ifndef AOS_ROOM_DIALOGUE_H
#define AOS_ROOM_DIALOGUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../title/aos_opening_event.h"

typedef struct AosRoomDialogue AosRoomDialogue;

/* Host callbacks. Every entry may be NULL except resolve_text; a NULL
 * callback means the host does not provide that primitive and the
 * bridge skips it (documented gap, never a silent placeholder). */
typedef bool (*AosRoomDialogueResolve)(void *context, uint16_t id,
                                       const uint8_t **script,
                                       size_t *script_size);
typedef bool (*AosRoomDialogueOpen)(void *context, uint16_t id);
typedef void (*AosRoomDialogueRender)(void *context,
                                      const AosRoomDialogue *dlg,
                                      uint32_t frame);
typedef void (*AosRoomDialogueClose)(void *context);
typedef void (*AosRoomDialogueInputLock)(void *context, bool locked);

typedef struct {
    void *context;
    /* sub_08041434 text-table entry fetch (aos_intro_resources_entry
     * in the runtime adapter). Required for open to succeed. */
    AosRoomDialogueResolve resolve_text;
    /* sub_08067934's window chrome setup for the text id. Return
     * false when the window cannot be shown; the bridge then reports
     * failure and never takes the input lock. */
    AosRoomDialogueOpen open_window;
    /* Per-frame presentation hook, invoked by
     * aos_room_dialogue_render only while the window is open. The
     * host reads dlg->event glyphs/waiting/speaker exactly like the
     * opening path's rebuild_opening_text. */
    AosRoomDialogueRender render;
    /* sub_08067898's window teardown. */
    AosRoomDialogueClose close_window;
    /* Modal input lock acquire/release paired with open/close. */
    AosRoomDialogueInputLock set_input_lock;
} AosRoomDialogueHost;

/* sub_08067804's WIN0V open animation, run inside sub_08067A04's
 * poll states: gEwramData +0x41B climbs 0->0x24 in +3 steps, one
 * step per poll, so the empty WIN0V bounds (0xC0C1) that
 * sub_08067934 writes synchronously take 12 polls to reach the
 * full window. Hosts modelling the same trigger->visible delay
 * on a path that bypasses this bridge (the epilogue's direct
 * opening-event reuse) count the same number of frames. */
#define AOS_ROOM_DIALOGUE_OPEN_FRAMES 12

/* Normalized view of one entity update's text-window outputs, shared
 * by aos_object20_story/dirs, aos_npc_talk and aos_shop Out structs. */
typedef struct {
    int window_text;    /* sub_08067934 argument, -1 none */
    bool window_poll;   /* sub_08067A04 called this frame */
    bool window_close;  /* sub_08067898 called this frame */
} AosRoomDialogueRequest;

struct AosRoomDialogue {
    AosOpeningEvent event;      /* sub_08067A04's script machine */
    const AosRoomDialogueHost *host;
    uint16_t text_id;           /* id passed to the last open */
    uint32_t frame;             /* render frame counter */
    uint8_t open_anim;          /* gEwramData +0x41B WIN0V counter */
    bool window_open;           /* chrome currently shown */
    bool window_shown;          /* WIN0V open animation complete */
    bool input_locked;          /* set_input_lock(true) outstanding */
    bool text_done;             /* last sub_08067A04-style result */
    bool failed;                /* resolve/open/render failure seen */
};

void aos_room_dialogue_init(AosRoomDialogue *dlg);
void aos_room_dialogue_set_host(AosRoomDialogue *dlg,
                                const AosRoomDialogueHost *host);
bool aos_room_dialogue_active(const AosRoomDialogue *dlg);
/* Window-visible predicate: sub_08067934's register writes land on
 * the open frame, but WIN0V stays at the empty 0xC0C1 bounds until
 * sub_08067A04's open states finish the +0x41B animation
 * (AOS_ROOM_DIALOGUE_OPEN_FRAMES polls). Presentation and open-frame
 * diagnostics must use this; the session lifecycle (poll/close/input
 * lock) stays on aos_room_dialogue_active. */
bool aos_room_dialogue_shown(const AosRoomDialogue *dlg);
/* The live script machine while the window is open (glyphs, line,
 * speaker, waiting, complete, malformed, dirty, cue). NULL when
 * closed; the pointer stays valid until the next open/close. */
const AosOpeningEvent *aos_room_dialogue_event(const AosRoomDialogue *dlg);
/* Last poll result: nonzero-equivalent of sub_08067A04() != 0. Feed
 * into the entity In.text_done on the following update. */
bool aos_room_dialogue_text_done(const AosRoomDialogue *dlg);
bool aos_room_dialogue_failed(const AosRoomDialogue *dlg);

/* sub_08067934(text_id): resolve the script, initialise the text
 * machine, open the window chrome and take the modal input lock.
 * Re-opening while active first runs the close path. Returns false
 * on resolution/open failure (failed flag set, no lock taken). */
bool aos_room_dialogue_open(AosRoomDialogue *dlg, uint16_t text_id);

/* sub_08067A04(): pump the script one frame with edge input
 * (AOS_KEY_* mask, same convention as aos_opening_event_update).
 * Returns the text_done result for this frame. No-op (false) while
 * no window is open; after the script completes the result stays
 * true until close so a missed poll cannot strand the entity. A
 * malformed script reports failed and counts as done so the entity
 * sequence can still reach its close call. */
bool aos_room_dialogue_poll(AosRoomDialogue *dlg, uint16_t new_input);

/* sub_08067898(): close the window chrome and release the input
 * lock. Safe to call while idle. */
void aos_room_dialogue_close(AosRoomDialogue *dlg);

/* Presentation hook for the draw pass: invokes host->render with the
 * current frame counter while the window is open and the script is
 * dirty, then clears event.dirty so each page renders once. Returns
 * true when the host rendered. */
bool aos_room_dialogue_render(AosRoomDialogue *dlg, uint32_t frame);

/* Convenience for the future update_entities Object20 dispatch: feed
 * one entity update's normalized Out triple plus the frame's edge
 * input. Processing order is open -> poll -> close, matching the
 * order the native routines appear within a single update. */
void aos_room_dialogue_apply(AosRoomDialogue *dlg, uint16_t new_input,
                             const AosRoomDialogueRequest *request);

#endif
