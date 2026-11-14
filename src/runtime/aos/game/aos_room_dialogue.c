/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_room_dialogue.h"

#include <string.h>

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
    if (dlg->host->set_input_lock)
        dlg->host->set_input_lock(dlg->host->context, true);
    dlg->input_locked = true;
    return true;
}

bool aos_room_dialogue_poll(AosRoomDialogue *dlg, uint16_t new_input) {
    if (!dlg || !dlg->window_open) return false;
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
    aos_opening_event_update(&dlg->event, new_input);
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
    if (request->window_poll)
        (void)aos_room_dialogue_poll(dlg, new_input);
    if (request->window_close)
        aos_room_dialogue_close(dlg);
}
