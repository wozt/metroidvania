/* Unit tests for src/runtime/aos/game/aos_room_dialogue.c — the
 * Object20 room-dialogue bridge (sub_08067934 open / sub_08067A04
 * poll / sub_08067898 close) with a mock host context. */
#include "../src/runtime/aos/game/aos_room_dialogue.h"
#include "../src/runtime/aos/core/aos_audio.h"
#include "../src/runtime/aos/core/aos_input.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Two canned text-table entries in the sub_08041434 script format:
 * a two-byte header, glyph bytes, command 5 (wait) and command 10
 * (end). */
static const uint8_t script_a[] = {0x00, 0x00, 'H', 'i', 5, 10};
static const uint8_t script_b[] = {0x00, 0x00, 'B', 'y', 'e', 5, 10};
static const uint8_t script_bad[] = {0x00, 0x00, 0x01}; /* truncated arg */

typedef struct {
    unsigned resolve_calls;
    unsigned open_calls;
    unsigned close_calls;
    unsigned render_calls;
    unsigned lock_acquires;
    unsigned lock_releases;
    uint16_t opened_ids[8];
    bool open_result;
    bool fail_open_id;      /* make open_window fail for this id */
} MockHost;

static bool mock_resolve(void *context, uint16_t id,
                         const uint8_t **script, size_t *script_size) {
    MockHost *m = context;
    m->resolve_calls++;
    switch (id) {
    case 0x14:
        *script = script_a;
        *script_size = sizeof script_a;
        return true;
    case 0x15:
        *script = script_b;
        *script_size = sizeof script_b;
        return true;
    case 0x77:
        *script = script_bad;
        *script_size = sizeof script_bad;
        return true;
    default:
        return false;
    }
}

static bool mock_open(void *context, uint16_t id) {
    MockHost *m = context;
    m->open_calls++;
    if (m->open_calls <= 8) m->opened_ids[m->open_calls - 1] = id;
    return !m->fail_open_id || id != 0x15;
}

static void mock_render(void *context, const AosRoomDialogue *dlg,
                        uint32_t frame) {
    MockHost *m = context;
    (void)dlg;
    (void)frame;
    m->render_calls++;
}

static void mock_close(void *context) {
    MockHost *m = context;
    m->close_calls++;
}

static void mock_lock(void *context, bool locked) {
    MockHost *m = context;
    if (locked) m->lock_acquires++;
    else m->lock_releases++;
}

static AosRoomDialogueHost mock_host(MockHost *m) {
    AosRoomDialogueHost host;
    memset(&host, 0, sizeof host);
    host.context = m;
    host.resolve_text = mock_resolve;
    host.open_window = mock_open;
    host.render = mock_render;
    host.close_window = mock_close;
    host.set_input_lock = mock_lock;
    return host;
}

/* Drive a dialogue to completion: poll with no input until the
 * script waits, press A through each wait, then confirm text_done. */
static void run_dialogue_to_done(AosRoomDialogue *dlg) {
    for (unsigned i = 0; i < 64 && !aos_room_dialogue_text_done(dlg); ++i) {
        const AosOpeningEvent *event = aos_room_dialogue_event(dlg);
        uint16_t input = event && event->waiting ? AOS_KEY_A : 0;
        assert(aos_room_dialogue_poll(dlg, input) ==
               aos_room_dialogue_text_done(dlg));
    }
    assert(aos_room_dialogue_text_done(dlg));
}

int main(void) {
    AosRoomDialogue dlg;
    MockHost mock;
    AosRoomDialogueHost host;

    /* open -> poll -> advance -> close lifecycle through apply(). */
    memset(&mock, 0, sizeof mock);
    host = mock_host(&mock);
    aos_room_dialogue_init(&dlg);
    aos_room_dialogue_set_host(&dlg, &host);
    assert(!aos_room_dialogue_active(&dlg));
    assert(!aos_room_dialogue_text_done(&dlg));

    /* Poll and close before any open are harmless no-ops. */
    assert(!aos_room_dialogue_poll(&dlg, AOS_KEY_A));
    aos_room_dialogue_close(&dlg);
    assert(mock.close_calls == 0 && mock.lock_releases == 0);

    AosRoomDialogueRequest req = {0x14, true, false};
    aos_room_dialogue_apply(&dlg, 0, &req);
    assert(aos_room_dialogue_active(&dlg));
    assert(mock.resolve_calls == 1 && mock.open_calls == 1);
    assert(mock.opened_ids[0] == 0x14);
    assert(mock.lock_acquires == 1 && mock.lock_releases == 0);
    assert(aos_room_dialogue_event(&dlg) != NULL);

    /* The apply above also polled: one glyph parsed, not done. */
    assert(!aos_room_dialogue_text_done(&dlg));
    assert(aos_room_dialogue_event(&dlg)->glyph_counts[0] == 1);

    /* The next poll parses the second glyph. */
    req.window_text = -1;
    aos_room_dialogue_apply(&dlg, 0, &req);
    assert(!aos_room_dialogue_text_done(&dlg));
    assert(aos_room_dialogue_event(&dlg)->glyph_counts[0] == 2);

    /* Render hook fires once per dirty page. */
    assert(aos_room_dialogue_render(&dlg, 10));
    assert(mock.render_calls == 1);
    assert(!aos_room_dialogue_event(&dlg)->dirty);
    assert(!aos_room_dialogue_render(&dlg, 11));
    assert(mock.render_calls == 1);

    run_dialogue_to_done(&dlg);
    assert(aos_room_dialogue_event(&dlg)->complete);
    /* The end command left the page dirty; one more render fires. */
    assert(aos_room_dialogue_render(&dlg, 12));
    assert(mock.render_calls == 2);

    /* Close pairs the input lock exactly once. */
    req.window_poll = false;
    req.window_close = true;
    aos_room_dialogue_apply(&dlg, 0, &req);
    assert(!aos_room_dialogue_active(&dlg));
    assert(aos_room_dialogue_event(&dlg) == NULL);
    assert(mock.close_calls == 1);
    assert(mock.lock_acquires == 1 && mock.lock_releases == 1);
    assert(!aos_room_dialogue_text_done(&dlg));

    /* Second close is a no-op: lock stays balanced. */
    aos_room_dialogue_close(&dlg);
    assert(mock.lock_releases == 1 && mock.close_calls == 1);

    /* Text id resolution failure: no window, no lock, failed flag. */
    req.window_close = false;
    req.window_text = 0x99;
    req.window_poll = true;
    aos_room_dialogue_apply(&dlg, 0, &req);
    assert(!aos_room_dialogue_active(&dlg));
    assert(aos_room_dialogue_failed(&dlg));
    assert(mock.open_calls == 1);
    assert(mock.lock_acquires == 1 && mock.lock_releases == 1);
    assert(!aos_room_dialogue_poll(&dlg, AOS_KEY_A));

    /* A failed open clears the flag on the next successful open. */
    req.window_text = 0x15;
    req.window_poll = false;
    aos_room_dialogue_apply(&dlg, 0, &req);
    assert(aos_room_dialogue_active(&dlg));
    assert(!aos_room_dialogue_failed(&dlg));
    assert(mock.opened_ids[1] == 0x15);
    run_dialogue_to_done(&dlg);
    aos_room_dialogue_close(&dlg);
    assert(mock.lock_acquires == 2 && mock.lock_releases == 2);
    assert(mock.close_calls == 2);

    /* Sequential dialogues keep independent scripts: script B has
     * three glyphs before its wait. */
    req.window_text = 0x14;
    req.window_close = false;
    req.window_poll = true;
    aos_room_dialogue_apply(&dlg, 0, &req);
    assert(aos_room_dialogue_active(&dlg));
    const AosOpeningEvent *ev = aos_room_dialogue_event(&dlg);
    assert(ev && ev->glyphs[0][0] == 'H');
    run_dialogue_to_done(&dlg);
    aos_room_dialogue_close(&dlg);
    assert(mock.lock_acquires == 3 && mock.lock_releases == 3);

    /* open_window host failure: script resolved but chrome refused;
     * no lock taken, failed reported. */
    mock.fail_open_id = true;
    req.window_text = 0x15;
    req.window_poll = false;
    aos_room_dialogue_apply(&dlg, 0, &req);
    assert(!aos_room_dialogue_active(&dlg));
    assert(aos_room_dialogue_failed(&dlg));
    assert(mock.lock_acquires == 3 && mock.lock_releases == 3);
    mock.fail_open_id = false;

    /* Malformed script: poll reports done (entity can reach close)
     * and the failure is observable. */
    req.window_text = 0x77;
    aos_room_dialogue_apply(&dlg, 0, &req);
    assert(aos_room_dialogue_active(&dlg));
    aos_room_dialogue_poll(&dlg, 0);
    assert(aos_room_dialogue_failed(&dlg));
    assert(aos_room_dialogue_text_done(&dlg));
    aos_room_dialogue_close(&dlg);
    assert(mock.lock_acquires == 4 && mock.lock_releases == 4);

    /* Re-open while active closes the previous window first. */
    req.window_text = 0x14;
    req.window_poll = false;
    aos_room_dialogue_apply(&dlg, 0, &req);
    assert(mock.close_calls == 4);
    /* opened_ids[3] is the refused 0x15 open_window attempt. */
    assert(mock.opened_ids[5] == 0x14);
    req.window_text = 0x15;
    aos_room_dialogue_apply(&dlg, 0, &req);
    assert(mock.close_calls == 5);
    assert(mock.opened_ids[6] == 0x15);
    assert(mock.lock_acquires == 6 && mock.lock_releases == 5);
    aos_room_dialogue_close(&dlg);
    assert(mock.lock_releases == 6);

    /* WIN0V open delay: sub_08067934 leaves the window invisible
     * (empty 0xC0C1 bounds); window_shown latches only after
     * AOS_ROOM_DIALOGUE_OPEN_FRAMES polls of the +0x41B animation.
     * The session stays active and the script keeps polling. */
    req.window_text = 0x14;
    req.window_poll = false;
    req.window_close = false;
    aos_room_dialogue_apply(&dlg, 0, &req);
    assert(aos_room_dialogue_active(&dlg));
    assert(!aos_room_dialogue_shown(&dlg));
    for (unsigned i = 1; i < AOS_ROOM_DIALOGUE_OPEN_FRAMES; ++i) {
        aos_room_dialogue_poll(&dlg, 0);
        assert(!aos_room_dialogue_shown(&dlg));
        assert(aos_room_dialogue_active(&dlg));
    }
    aos_room_dialogue_poll(&dlg, 0);
    assert(aos_room_dialogue_shown(&dlg));
    run_dialogue_to_done(&dlg);
    aos_room_dialogue_close(&dlg);
    assert(!aos_room_dialogue_shown(&dlg));

    /* +0x415/+0x416 print-state latch (_08067BC4/_08067BD8,
     * _08068076): open arms +0x416; each armed print poll writes
     * +0x415 = (new_input & 2) ? 0 : +0x416 and reveals one glyph;
     * an A-edge while printing clears +0x416 and the next poll
     * fast-forwards the rest of the page; an A-edge at the wait
     * re-arms it. */
    aos_room_dialogue_init(&dlg);
    aos_room_dialogue_set_host(&dlg, &host);
    req = (AosRoomDialogueRequest){0x15, true, false};
    aos_room_dialogue_apply(&dlg, 0, &req);
    assert(aos_room_dialogue_active(&dlg));
    {
        uint8_t b415 = 0xEE, b416 = 0xEE;
        aos_room_dialogue_ewram_state(&dlg, &b415, &b416);
        assert(b416 == 1);          /* sub_08067934's +0x416 store */
    }
    /* First poll (inside apply): armed typewriter step, 'B' only. */
    assert(aos_room_dialogue_event(&dlg)->glyph_counts[0] == 1);
    /* A-edge during printing: still one glyph this frame (+0x416
     * was armed when the glyph check ran), then the tail clears
     * the arm — script_b's 'y' lands, 'e' is still pending. */
    assert(!aos_room_dialogue_poll(&dlg, AOS_KEY_A));
    assert(aos_room_dialogue_event(&dlg)->glyph_counts[0] == 2);
    {
        uint8_t b415 = 0, b416 = 1;
        aos_room_dialogue_ewram_state(&dlg, &b415, &b416);
        assert(b416 == 0);          /* _08068076 A-edge rewrite */
        assert(b415 == 1);          /* +0x415 copied +0x416=1:
                                     * the check ran before the
                                     * tail cleared the arm */
    }
    /* Disarmed: the next poll rips through 'e' to the wait in one
     * frame — the native sl=1 loop continuation. */
    assert(!aos_room_dialogue_poll(&dlg, 0));
    assert(aos_room_dialogue_event(&dlg)->glyph_counts[0] == 3);
    assert(aos_room_dialogue_event(&dlg)->waiting);
    assert(!aos_room_dialogue_text_done(&dlg));
    /* A-edge at the wait releases it and re-arms +0x416; script_b
     * ends right after, so the poll reports done. */
    assert(aos_room_dialogue_poll(&dlg, AOS_KEY_A));
    assert(aos_room_dialogue_text_done(&dlg));
    {
        uint8_t b415 = 0, b416 = 0;
        aos_room_dialogue_ewram_state(&dlg, &b415, &b416);
        assert(b416 == 1);          /* wait-state A-edge re-arm */
    }
    aos_room_dialogue_close(&dlg);

    /* B-edge (new_input bit 1) masks the +0x415 copy to 0 but does
     * not disarm +0x416 — the typewriter keeps its one-glyph
     * cadence. */
    req.window_text = 0x15;
    req.window_poll = true;
    aos_room_dialogue_apply(&dlg, 0, &req);
    assert(aos_room_dialogue_active(&dlg));
    assert(aos_room_dialogue_event(&dlg)->glyph_counts[0] == 1);
    assert(!aos_room_dialogue_poll(&dlg, AOS_KEY_B));
    {
        uint8_t b415 = 1, b416 = 0;
        aos_room_dialogue_ewram_state(&dlg, &b415, &b416);
        assert(b415 == 0);          /* (new_input & 2) ? 0 : +0x416 */
        assert(b416 == 1);          /* B is not the A-edge: arm kept */
    }
    /* Still armed: the next poll reveals only the third glyph —
     * the typewriter cadence is unchanged. */
    assert(!aos_room_dialogue_poll(&dlg, 0));
    assert(aos_room_dialogue_event(&dlg)->glyph_counts[0] == 3);
    assert(!aos_room_dialogue_event(&dlg)->waiting);
    {
        uint8_t b415 = 0, b416 = 0;
        aos_room_dialogue_ewram_state(&dlg, &b415, &b416);
        assert(b415 == 1);          /* unmasked copy while armed */
        assert(b416 == 1);
    }
    aos_room_dialogue_close(&dlg);

    /* The state-1 tail branch also fires when the wait is reached
     * by this poll's own parse: the native _08068076 tail reads the
     * post-case nibble, so an A-edge landing on the command-5 frame
     * still re-arms +0x416 and reports song 0x132 / the OBJ palette
     * write (consumers unported). script_a is 'H','i',wait,end. */
    req.window_text = 0x14;
    req.window_poll = true;
    aos_room_dialogue_apply(&dlg, 0, &req);
    assert(aos_room_dialogue_active(&dlg));
    assert(aos_room_dialogue_event(&dlg)->glyph_counts[0] == 1);
    assert(dlg.song == -1 && !dlg.palette_write);
    /* Armed typewriter step, then an A-edge disarms it. */
    assert(!aos_room_dialogue_poll(&dlg, AOS_KEY_A));
    assert(aos_room_dialogue_event(&dlg)->glyph_counts[0] == 2);
    assert(dlg.song == -1 && !dlg.palette_write);
    /* Disarmed rip with an A-edge: the parse stops at the command-5
     * wait and the same frame's tail sees the wait state — +0x416
     * re-arms instead of clearing. */
    assert(!aos_room_dialogue_poll(&dlg, AOS_KEY_A));
    assert(aos_room_dialogue_event(&dlg)->waiting);
    {
        uint8_t b415 = 0, b416 = 0;
        aos_room_dialogue_ewram_state(&dlg, &b415, &b416);
        assert(b416 == 1);
    }
    assert(dlg.song == 0x132 && dlg.palette_write);
    /* A non-A poll clears the per-poll reports but keeps the arm. */
    assert(!aos_room_dialogue_poll(&dlg, 0));
    assert(dlg.song == -1 && !dlg.palette_write);
    {
        uint8_t b415 = 0, b416 = 0;
        aos_room_dialogue_ewram_state(&dlg, &b415, &b416);
        assert(b416 == 1);
    }
    /* The wait release reports the branch again and ends the script. */
    assert(aos_room_dialogue_poll(&dlg, AOS_KEY_A));
    assert(aos_room_dialogue_text_done(&dlg));
    assert(dlg.song == 0x132 && dlg.palette_write);
    aos_room_dialogue_close(&dlg);

    /* _08068076's state-1 A-edge branch calls PlaySong(0x132)
     * inline; aos_room_dialogue_apply consumes the report through
     * aos_audio_play_song. Drive the same script through apply so
     * the A-edge lands on the wait frame. */
    aos_audio_reset();
    aos_audio_clear();
    req.window_text = 0x14;
    req.window_poll = true;
    aos_room_dialogue_apply(&dlg, 0, &req);       /* open + first poll */
    assert(aos_room_dialogue_active(&dlg));
    req.window_text = -1;
    aos_room_dialogue_apply(&dlg, AOS_KEY_A, &req); /* disarm typewriter */
    aos_room_dialogue_apply(&dlg, AOS_KEY_A, &req); /* wait + tail: 0x132 */
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x132) == 1);
    aos_room_dialogue_close(&dlg);

    /* Missing host / missing resolver are handled, not crashed. */
    aos_room_dialogue_init(&dlg);
    req.window_text = 0x14;
    aos_room_dialogue_apply(&dlg, 0, &req);
    assert(!aos_room_dialogue_active(&dlg));
    AosRoomDialogueHost empty;
    memset(&empty, 0, sizeof empty);
    aos_room_dialogue_set_host(&dlg, &empty);
    aos_room_dialogue_apply(&dlg, 0, &req);
    assert(!aos_room_dialogue_active(&dlg));
    assert(aos_room_dialogue_failed(&dlg));

    printf("aos_room_dialogue: all tests passed\n");
    return 0;
}
