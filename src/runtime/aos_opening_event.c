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
            /* Native sub_08067A04 advances Object20's callback state and
             * keeps parsing. The following command 5, when present, owns the
             * actual input wait. */
            break;
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
    /* sub_08067A04 exits its print state after one visible glyph. A pressed
     * on a wait releases that wait, but never collapses the rest of a page
     * into the same host frame. */
    run_until_glyph_or_wait(event);
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
        .visible = {true, true, true},
        .timer = 0x40,
        .active = true,
    };
}

static void actor_animation(AosOpeningActors *actors, unsigned actor,
                            uint8_t animation) {
    actors->animation[actor] = animation;
    actors->animation_age[actor] = 0;
}

void aos_opening_actors_trigger(AosOpeningActors *actors, AosOpeningCue cue) {
    if (!actors || !actors->active || cue == AOS_OPENING_CUE_NONE) return;
    switch (cue) {
    case AOS_OPENING_CUE_CONFRONTATION:
        actors->scene_state = 2;
        actors->pending_cue = cue;
        break;
    case AOS_OPENING_CUE_ENEMY_APPEARS:
        actors->scene_state = 3;
        actors->scene_step = 0;
        actors->scene_timer = 32;
        break;
    case AOS_OPENING_CUE_ENEMY_ATTACKS:
        actors->scene_state = 4;
        actors->scene_step = 0;
        actor_animation(actors, 1, 2);
        actors->pending_cue = cue;
        break;
    case AOS_OPENING_CUE_SOUL_ABSORBED:
        actors->scene_state = 5;
        actors->scene_step = 0;
        actors->scene_timer = 16;
        actor_animation(actors, 2, 5);
        break;
    case AOS_OPENING_CUE_AFTER_BATTLE:
        actors->scene_state = 6;
        actors->scene_step = 0;
        actors->pending_cue = cue;
        break;
    case AOS_OPENING_CUE_COMPLETE:
        actors->scene_state = 7;
        actors->pending_cue = cue;
        break;
    default:
        break;
    }
}

AosOpeningCue aos_opening_actors_update(
    AosOpeningActors *actors,
    const uint16_t animation_durations[AOS_OPENING_ACTOR_COUNT]) {
    if (!actors || !actors->active) return AOS_OPENING_CUE_NONE;
    AosOpeningCue output = actors->pending_cue;
    actors->pending_cue = AOS_OPENING_CUE_NONE;
    ++actors->frame;
    for (unsigned actor = 0; actor < AOS_OPENING_ACTOR_COUNT; ++actor)
        if (actors->animation_age[actor] < UINT16_MAX)
            ++actors->animation_age[actor];
    /* sub_0805D410 state 0: its entry timer is 0x40 frames. */
    if (actors->state == 0) {
        if (actors->timer) --actors->timer;
        if (!actors->timer) {
            actors->state = 1;
            actors->step = 0;
        }
        goto scene;
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

scene:
    switch (actors->scene_state) {
    case 3:
        /* State 3 waits 0x20 frames after the reveal transition, then puts
         * Mina in animation 5 and plays SE_436. */
        if (actors->scene_timer && --actors->scene_timer == 0) {
            actor_animation(actors, 0, 5);
            if (output == AOS_OPENING_CUE_NONE)
                output = AOS_OPENING_CUE_ENEMY_APPEARS;
            else
                actors->pending_cue = AOS_OPENING_CUE_ENEMY_APPEARS;
            actors->scene_step = 1;
        }
        break;
    case 4: {
        /* Arikado's animation 2 completes, the controller waits 0x10 frames,
         * then animation 3 and SE_480 begin. */
        uint16_t duration = animation_durations ? animation_durations[1] : 0;
        if (actors->scene_step == 0 && duration &&
            actors->animation_age[1] >= duration) {
            actors->scene_step = 1;
            actors->scene_timer = 16;
        } else if (actors->scene_step == 1 && actors->scene_timer &&
                   --actors->scene_timer == 0) {
            actor_animation(actors, 1, 3);
            if (output == AOS_OPENING_CUE_NONE)
                output = AOS_OPENING_CUE_ENEMY_ATTACKS_AGAIN;
            else
                actors->pending_cue = AOS_OPENING_CUE_ENEMY_ATTACKS_AGAIN;
            actors->scene_step = 2;
        }
        break;
    }
    case 5:
        /* State 5 waits 0x10 frames, starts its 0xD2-frame battle timer,
         * removes the enemy at 0xD0 and plays the impact cue at 0x8F. */
        if (actors->scene_step == 0) {
            if (actors->scene_timer && --actors->scene_timer == 0) {
                actors->scene_step = 1;
                actors->scene_timer = 0xD2;
            }
        } else if (actors->scene_step == 1 && actors->scene_timer) {
            --actors->scene_timer;
            AosOpeningCue due = AOS_OPENING_CUE_NONE;
            if (actors->scene_timer == 0xD0) {
                actors->visible[2] = false;
                due = AOS_OPENING_CUE_ENEMY_DEFEATED;
            } else if (actors->scene_timer == 0x8F) {
                due = AOS_OPENING_CUE_SOUL_ABSORBED;
                actors->scene_step = 2;
            }
            if (due != AOS_OPENING_CUE_NONE) {
                if (output == AOS_OPENING_CUE_NONE) output = due;
                else actors->pending_cue = due;
            }
        }
        break;
    default:
        break;
    }
    return output;
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
