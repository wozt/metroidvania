/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_room_dialogue.h"

#include <string.h>

#include "../core/aos_audio.h"

/* AosOpeningEvent's resolver slot is typed for the opening scene's
 * uint8_t sub-entry ids; room-dialogue text ids are uint16_t, so the
 * bridge adapts through this trampoline. Nested control-1 ids are
 * bytes natively, so widening is lossless. */
static bool room_dialogue_resolve(void *context, uint8_t id,
                                  const uint8_t **script,
                                  size_t *script_size) {
    AosRoomDialogue *dlg = context;
    if (!dlg->host || !dlg->host->resolve_text) return false;
    return dlg->host->resolve_text(dlg->host->context, id, script,
                                   script_size);
}

static void room_dialogue_unlock(AosRoomDialogue *dlg) {
    if (dlg->input_locked) {
        dlg->input_locked = false;
        if (dlg->host && dlg->host->set_input_lock)
            dlg->host->set_input_lock(dlg->host->context, false);
    }
}

void aos_room_dialogue_init(AosRoomDialogue *dlg) {
    if (!dlg) return;
    memset(dlg, 0, sizeof *dlg);
    dlg->text_id = 0xFFFF;
}

void aos_room_dialogue_set_host(AosRoomDialogue *dlg,
                                const AosRoomDialogueHost *host) {
    if (!dlg) return;
    dlg->host = host;
}

bool aos_room_dialogue_active(const AosRoomDialogue *dlg) {
    return dlg && dlg->window_open;
}

bool aos_room_dialogue_shown(const AosRoomDialogue *dlg) {
    return dlg && dlg->window_open && dlg->window_shown;
}

const AosOpeningEvent *aos_room_dialogue_event(const AosRoomDialogue *dlg) {
    return dlg && dlg->window_open ? &dlg->event : NULL;
}

bool aos_room_dialogue_text_done(const AosRoomDialogue *dlg) {
    return dlg && dlg->text_done;
}

bool aos_room_dialogue_failed(const AosRoomDialogue *dlg) {
    return dlg && dlg->failed;
}

void aos_room_dialogue_ewram_state(const AosRoomDialogue *dlg,
                                   uint8_t *byte415, uint8_t *byte416) {
    if (byte415) *byte415 = dlg ? dlg->ewram_415 : 0;
    if (byte416) *byte416 = dlg ? dlg->ewram_416 : 0;
}

bool aos_room_dialogue_open(AosRoomDialogue *dlg, uint16_t text_id) {
    if (!dlg || !dlg->host || !dlg->host->resolve_text) {
        if (dlg) dlg->failed = true;
        return false;
    }
    /* Re-opening while active retires the previous window first; the
     * native routines never run two dialogues at once. */
    if (dlg->window_open) aos_room_dialogue_close(dlg);
    const uint8_t *script = NULL;
    size_t script_size = 0;
    if (!dlg->host->resolve_text(dlg->host->context, text_id, &script,
                                 &script_size) ||
        !aos_opening_event_init(&dlg->event, script, script_size)) {
        dlg->failed = true;
        return false;
    }
    dlg->event.resolve = room_dialogue_resolve;
    dlg->event.resolve_context = dlg;
    /* sub_08067934's chrome: optional host hook; a NULL open_window
     * means the host presents the dialogue frame unconditionally. */
    if (dlg->host->open_window &&
        !dlg->host->open_window(dlg->host->context, text_id)) {
        dlg->failed = true;
        return false;
    }
    dlg->text_id = text_id;
    dlg->window_open = true;
    /* sub_08067934 writes WIN0V=0xC0C1 (empty): the window is not
     * visible yet. sub_08067A04's open states advance +0x41B by 3
     * per poll until it saturates at 0x24 — window_shown latches
     * when that animation completes (aos_room_dialogue_poll). */
    dlg->open_anim = 0;
    dlg->window_shown = false;
    dlg->text_done = false;
    dlg->failed = false;
    /* sub_08067934's gEwramData +0x416 store: the typewriter arm
     * starts set, so the first print polls reveal one glyph each
     * through the +0x414 nibble-4 branch until an A-edge clears
     * it (_08067BC4/_08067BD8, _08068076). */
    dlg->ewram_416 = 1;
    dlg->song = -1;
    dlg->palette_write = false;
    if (dlg->host->set_input_lock)
        dlg->host->set_input_lock(dlg->host->context, true);
    dlg->input_locked = true;
    return true;
}

bool aos_room_dialogue_poll(AosRoomDialogue *dlg, uint16_t new_input) {
    if (!dlg || !dlg->window_open) return false;
    dlg->song = -1;
    dlg->palette_write = false;
    /* sub_08067A04's window-open states call sub_08067804(+3) once
     * per poll: +0x41B climbs 0->0x24 and WIN0V grows from the empty
     * bounds sub_08067934 wrote. The script machine keeps running
     * alongside (the native print states are not gated on the
     * animation either); only the visible-window latch waits for
     * saturation — AOS_ROOM_DIALOGUE_OPEN_FRAMES polls. */
    if (!dlg->window_shown) {
        unsigned anim = (unsigned)dlg->open_anim + 3;
        dlg->open_anim = anim > 0x24 ? 0x24 : (uint8_t)anim;
        if (dlg->open_anim >= 0x24) dlg->window_shown = true;
    }
    if (!dlg->event.active) {
        /* The script ran to its end command; keep reporting done so a
         * frame that skipped the poll cannot strand the entity. */
        dlg->text_done = true;
        return true;
    }
    /* sub_08067A04's print-state glyph check (_08067BC4/_08067BD8)
     * runs on every literal glyph byte the parse loop reaches:
     * +0x415 = (new_input bit 1) ? 0 : +0x416, and a nonzero +0x416
     * takes the +0x414 nibble-4 branch (sl=0) which ends the poll
     * — the typewriter step, one glyph per frame. While +0x416 is
     * clear the same check falls through with sl=1 and the loop
     * keeps consuming glyph bytes, so the whole remaining run
     * prints in this one poll: the fast-forward. `was_waiting`
     * is the +0x414 nibble==1 proxy for the _08068076 tail below
     * (the wait state is the only state that re-arms +0x416). */
    bool was_waiting = dlg->event.waiting;
    if (!was_waiting) {
        /* Per-poll approximation of the per-glyph-byte +0x415
         * copy; in the rip case +0x416 is already 0 so the latch
         * lands 0 either way. */
        dlg->ewram_415 = (new_input & AOS_KEY_B) ? 0 : dlg->ewram_416;
        if (dlg->ewram_416 != 0) {
            /* nibble|=4 branch: park after this frame's glyph. */
            aos_opening_event_update(&dlg->event, new_input);
        } else {
            /* +0x416 == 0: the native print loop does not stop at
             * glyph bytes, so it consumes commands and glyphs
             * until the next state command (wait/end/page). Run
             * the machine with no input to the same boundary;
             * the budget only guards a malformed script. */
            size_t budget = dlg->event.script_size + 8;
            do {
                aos_opening_event_update(&dlg->event, 0);
                if (dlg->event.malformed) dlg->failed = true;
            } while (dlg->event.active && !dlg->event.waiting &&
                     budget-- != 0);
        }
    } else {
        aos_opening_event_update(&dlg->event, new_input);
    }
    /* _08068076 tail: on A-edge frames +0x416 is rewritten — 1
     * while the post-case nibble sits at the wait state (that
     * branch also plays song 0x132, writes the OBJ palette tiles
     * and clears +0x414 bit 4; the palette/VRAM work stays
     * unported and is reported through song/palette_write), 0
     * otherwise. The wait may have been entered before this poll
     * (was_waiting) or reached by this poll's own parse — the
     * native tail reads the nibble after the case work, so both
     * count. An A-edge during printing is what disarms the
     * typewriter and fast-forwards the rest of the page. */
    if (new_input & AOS_KEY_A) {
        if (was_waiting || dlg->event.waiting) {
            dlg->ewram_416 = 1;
            dlg->song = 0x132;
            dlg->palette_write = true;
        } else {
            dlg->ewram_416 = 0;
        }
    }
    if (dlg->event.malformed) dlg->failed = true;
    if (!dlg->event.active) {
        /* control 10 ended the script, or the parser rejected it.
         * Either way sub_08067A04's machine has nothing left to
         * print, so the poll reports done. */
        dlg->text_done = true;
        return true;
    }
    dlg->text_done = false;
    return false;
}

void aos_room_dialogue_close(AosRoomDialogue *dlg) {
    if (!dlg) return;
    if (dlg->window_open) {
        dlg->window_open = false;
        dlg->window_shown = false;
        dlg->open_anim = 0;
        if (dlg->host && dlg->host->close_window)
            dlg->host->close_window(dlg->host->context);
    }
    room_dialogue_unlock(dlg);
    dlg->text_done = false;
}

bool aos_room_dialogue_render(AosRoomDialogue *dlg, uint32_t frame) {
    if (!dlg || !dlg->window_open || !dlg->host || !dlg->host->render)
        return false;
    if (!dlg->event.dirty) return false;
    dlg->event.dirty = false;
    dlg->frame = frame;
    dlg->host->render(dlg->host->context, dlg, frame);
    return true;
}

void aos_room_dialogue_apply(AosRoomDialogue *dlg, uint16_t new_input,
                             const AosRoomDialogueRequest *request) {
    if (!dlg || !request) return;
    if (request->window_text >= 0)
        aos_room_dialogue_open(dlg, (uint16_t)request->window_text);
    if (request->window_poll) {
        (void)aos_room_dialogue_poll(dlg, new_input);
        /* _08068076's state-1 A-edge branch calls PlaySong(0x132)
         * inline; the poll reports it on dlg->song, so the apply
         * path plays it here like the native routine does. The OBJ
         * palette upload (palette_write) has no portable primitive
         * and stays a report. Gate on the window still being open:
         * a poll that early-returned on a closed window leaves a
         * stale song value. */
        if (dlg->song >= 0 && aos_room_dialogue_active(dlg))
            aos_audio_play_song((uint16_t)dlg->song);
    }
    if (request->window_close)
        aos_room_dialogue_close(dlg);
}
