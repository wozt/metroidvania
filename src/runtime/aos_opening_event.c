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

static AosOpeningCue beat_cue(uint8_t beat_count) {
    switch (beat_count) {
    /* The first control-2 beat releases Object20's actor-entry state. The
     * following beats select sub_0805D410 states 2 through 6. */
    case 2: return AOS_OPENING_CUE_CONFRONTATION;
    case 3: return AOS_OPENING_CUE_ENEMY_APPEARS;
    case 4: return AOS_OPENING_CUE_ENEMY_ATTACKS;
    case 5: return AOS_OPENING_CUE_SOUL_ABSORBED;
    case 6: return AOS_OPENING_CUE_AFTER_BATTLE;
    default: return AOS_OPENING_CUE_NONE;
    }
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
            ++event->beat_count;
            event->cue = beat_cue(event->beat_count);
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
            event->cue = AOS_OPENING_CUE_COMPLETE;
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
    event->cue = AOS_OPENING_CUE_NONE;
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

void aos_opening_actors_init(AosOpeningActors *actors) {
    if (!actors) return;
    *actors = (AosOpeningActors){
        .x = {AOS_OPENING_MINA_X << 16, AOS_OPENING_ARIKADO_X << 16,
              AOS_OPENING_SKELETON_X << 16},
        .y = {AOS_OPENING_MINA_Y << 16, AOS_OPENING_ARIKADO_Y << 16,
              AOS_OPENING_SKELETON_Y << 16},
        .animation = {4, 1, 4},
        .timer = 0x40,
        .active = true,
    };
}

void aos_opening_actors_update(AosOpeningActors *actors) {
    if (!actors || !actors->active) return;
    ++actors->frame;
    /* sub_0805D410 state 0: its entry timer is 0x40 frames. */
    if (actors->state == 0) {
        if (actors->timer) --actors->timer;
        if (!actors->timer) {
            actors->state = 1;
            actors->step = 0;
        }
        return;
    }
    /* sub_0805D198 gives Arikado -0xA000 fixed-point velocity. The native
     * callback eventually stops him when the scene's proximity condition is
     * met; the source-position guard keeps this partial controller bounded
     * until that condition is ported. */
    if (actors->state == 1) {
        const int32_t stop_x = 0xD0 << 16;
        if (actors->x[1] > stop_x) {
            actors->x[1] -= 0xA000;
            if (actors->x[1] < stop_x) actors->x[1] = stop_x;
        } else {
            actors->state = 2;
            actors->step = 0;
        }
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
