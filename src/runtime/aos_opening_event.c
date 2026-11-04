/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_opening_event.h"

#include <string.h>

#include "aos_input.h"

static void clear_page(AosOpeningEvent *event) {
    memset(event->glyphs, 0, sizeof event->glyphs);
    memset(event->glyph_counts, 0, sizeof event->glyph_counts);
    event->line = 0;
    event->dirty = true;
}

static bool event_is_set(const uint32_t events[8]) {
    return events && (events[AOS_OPENING_EVENT_ID >> 5] &
                      (1u << (AOS_OPENING_EVENT_ID & 31)));
}

bool aos_opening_event_should_start(uint8_t area, uint16_t room,
                                    uint8_t character,
                                    const uint32_t events[8],
                                    bool room_has_controller) {
    return room_has_controller && character == 0 && area == 0 && room == 0 &&
           !event_is_set(events);
}

bool aos_opening_event_init(AosOpeningEvent *event, const uint8_t *script,
                            size_t script_size) {
    if (!event || !script || script_size < 3) return false;
    *event = (AosOpeningEvent){
        .script = script,
        .script_size = script_size,
        .position = 2,
        .active = true,
        .dirty = true,
    };
    return true;
}

void aos_opening_event_set_resolver(AosOpeningEvent *event,
                                    AosOpeningTextResolver resolve,
                                    void *context) {
    if (!event) return;
    event->resolve = resolve;
    event->resolve_context = context;
}

static bool argument(AosOpeningEvent *event, uint8_t *value) {
    if (event->position >= event->script_size) {
        event->malformed = true;
        event->active = false;
        return false;
    }
    *value = event->script[event->position++];
    return true;
}

static void run_until_glyph_or_wait(AosOpeningEvent *event) {
    while (event->active && event->position < event->script_size) {
        uint8_t command = event->script[event->position++];
        if (command >= 0x20) {
            uint8_t *count = &event->glyph_counts[event->line];
            if (*count >= AOS_OPENING_GLYPHS_PER_LINE) {
                event->malformed = true;
                event->active = false;
                return;
            }
            event->glyphs[event->line][(*count)++] = command;
            event->dirty = true;
            return;
        }
        uint8_t value;
        switch (command) {
        case 1:
            if (!argument(event, &value)) return;
            if (!event->resolve ||
                !event->resolve(event->resolve_context, value, &event->script,
                                &event->script_size) ||
                event->script_size < 3) {
                event->malformed = true;
                event->active = false;
                return;
            }
            event->position = 2;
            break;
        case 2:
            if (!argument(event, &event->beat)) return;
            event->waiting = true;
            event->advanced = true;
            return;
        case 3:
            if (!argument(event, &event->portrait)) return;
            event->dirty = true;
            break;
        case 4:
        case 9:
            clear_page(event);
            break;
        case 5:
            event->waiting = true;
            return;
        case 6:
            if (event->line + 1 >= AOS_OPENING_LINES) {
                event->malformed = true;
                event->active = false;
                return;
            }
            ++event->line;
            event->dirty = true;
            break;
        case 7:
            if (!argument(event, &event->speaker)) return;
            event->dirty = true;
            break;
        case 8:
            if (!argument(event, &value)) return;
            break;
        case 10:
            event->active = false;
            event->complete = true;
            event->dirty = true;
            return;
        default:
            event->malformed = true;
            event->active = false;
            return;
        }
    }
    if (event->active) {
        event->malformed = true;
        event->active = false;
    }
}

void aos_opening_event_update(AosOpeningEvent *event, uint16_t new_input) {
    if (!event || !event->active) return;
    event->advanced = false;
    bool advance = (new_input & (AOS_KEY_A | AOS_KEY_START)) != 0;
    if (event->waiting) {
        if (!advance) return;
        event->waiting = false;
        event->advanced = true;
        if (event->beat == 0) clear_page(event);
        event->beat = 0;
    }
    do {
        run_until_glyph_or_wait(event);
    } while (advance && event->active && !event->waiting);
}

const char *aos_opening_event_speaker_name(uint8_t speaker) {
    switch (speaker) {
    case 1: return "Soma Cruz";
    case 2: return "Mina Hakuba";
    case 7: return "Genya Arikado";
    default: return "";
    }
}

bool aos_opening_event_commit(AosOpeningEvent *event, uint32_t events[8],
                              AosStats *stats) {
    if (!event || !event->complete || !events || !stats || event_is_set(events))
        return false;
    events[AOS_OPENING_EVENT_ID >> 5] |= 1u << (AOS_OPENING_EVENT_ID & 31);
    aos_souls_add(stats, AOS_SOUL_RED, 0, 1);
    if (stats->red_soul == 0) stats->red_soul = 1;
    return true;
}
