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

void aos_opening_actors_set_motion(AosOpeningActors *actors,
                                   AosOpeningRandom random,
                                   AosOpeningSine sine, void *context) {
    if (!actors) return;
    actors->random = random;
    actors->sine = sine;
    actors->motion_context = context;
}

static void actor_animation(AosOpeningActors *actors, unsigned actor,
                            uint8_t animation) {
    actors->animation[actor] = animation;
    actors->animation_age[actor] = 0;
}

static void append_emission(AosOpeningActors *actors,
                            AosOpeningEmission emission) {
    AosOpeningSpawn spawns[AOS_OPENING_EMISSION_MAX_SPAWNS];
    size_t count = aos_opening_emission_spawns(
        emission, AOS_OPENING_SOMA_X, spawns,
        AOS_OPENING_EMISSION_MAX_SPAWNS);
    for (size_t i = 0; i < count &&
                       actors->emitted_count < AOS_OPENING_EMITTED_CAPACITY;
         ++i) {
        AosOpeningEntity *entity = &actors->emitted[actors->emitted_count++];
        *entity = (AosOpeningEntity){
            .x = (int32_t)spawns[i].x << 16,
            .y = (int32_t)spawns[i].y << 16,
            .entity_type = spawns[i].entity_type,
            .parameter = spawns[i].parameter,
            .variant = spawns[i].variant,
            .animation = spawns[i].entity_type == 8 ? 4 : 0,
            .active = true,
            .mirrored = spawns[i].x < AOS_OPENING_SOMA_X,
        };
        if (entity->entity_type == 8 && actors->random)
            entity->phase = (uint16_t)((actors->random(actors->motion_context) & 0x7F) << 9);
        /* sub_0805E528 advances parameter-bit-2 Skeleton Knights by sixteen
         * half-pixel steps during creation. */
        if (entity->entity_type == 10 && (entity->parameter & 4))
            entity->y += 8 << 16;
    }
}

static void begin_emitted_disappearance(AosOpeningActors *actors) {
    for (unsigned i = 0; i < actors->emitted_count; ++i) {
        AosOpeningEntity *entity = &actors->emitted[i];
        if (!entity->active) continue;
        entity->disappearing = true;
        entity->state = entity->entity_type == 8 ? 4 : 3;
        entity->step = 0;
        entity->timer = 0;
        entity->hidden = false;
    }
}

static bool emitted_in_impact_margin(const AosOpeningActors *actors,
                                     const AosOpeningEntity *entity) {
    int32_t screen_x = (entity->x >> 16) - (actors->camera_x >> 16);
    int32_t screen_y = (entity->y >> 16) - actors->camera_y;
    /* sub_08068AD4 receives (-0x80, -0x40, 0x1F0, 0x120). The latter two
     * values are inclusive rectangle widths, not absolute coordinates. */
    return screen_x >= -0x80 && screen_x <= 0x170 &&
           screen_y >= -0x40 && screen_y <= 0xE0;
}

static bool emitted_selector(AosOpeningActors *actors,
                             const AosOpeningEntity *entity) {
    return actors->random &&
           actors->random(actors->motion_context) % 7 == entity->variant;
}

static void finish_emitted(AosOpeningActors *actors, AosOpeningEntity *entity,
                           bool projectile) {
    if (emitted_in_impact_margin(actors, entity))
        entity->events |= AOS_OPENING_ENTITY_EVENT_SOUND_0X6B |
                          AOS_OPENING_ENTITY_EVENT_DEBRIS;
    if (projectile)
        entity->events |= AOS_OPENING_ENTITY_EVENT_PROJECTILE;
    entity->state = entity->entity_type == 8 ? 5 : 4;
    entity->step = 0;
}

static void update_emitted_tail(AosOpeningActors *actors,
                                AosOpeningEntity *entity) {
    uint8_t terminal = entity->entity_type == 8 ? 5 : 4;
    if (entity->state == terminal) {
        entity->active = false;
        entity->hidden = true;
        return;
    }
    switch (entity->step) {
    case 0:
        entity->timer = 0x20;
        entity->step = 1;
        break;
    case 1:
        /* Both callbacks select the alternate OBJ attribute on odd timer
         * values. Hidden is the portable renderer's approximation until the
         * native palette/attribute path is available. */
        entity->hidden = (entity->timer & 1) != 0;
        if (--entity->timer == UINT8_MAX) {
            entity->hidden = false;
            entity->step = 2;
            if (entity->entity_type == 8) entity->timer = 0x10;
        }
        break;
    case 2:
        if (entity->entity_type == 8) {
            if (--entity->timer == UINT8_MAX) entity->step = 3;
        } else if (emitted_selector(actors, entity)) {
            entity->step = 3;
        }
        break;
    case 3:
        if (emitted_selector(actors, entity))
            finish_emitted(actors, entity, entity->entity_type == 10);
        break;
    default:
        break;
    }
}

static void update_emitted(AosOpeningActors *actors) {
    for (unsigned i = 0; i < actors->emitted_count; ++i) {
        AosOpeningEntity *entity = &actors->emitted[i];
        if (!entity->active) continue;
        entity->events = AOS_OPENING_ENTITY_EVENT_NONE;
        if (entity->animation_age < UINT16_MAX) ++entity->animation_age;
        if (!entity->disappearing && entity->entity_type == 8) {
            /* sub_0805E2D8 states 0-2: a phase-randomized quarter-amplitude
             * sine bob, followed by the parent-bit-8 state handoff. */
            if (entity->state <= 2 && actors->sine) {
                int32_t dy = actors->sine((uint16_t)(entity->phase + 0x4000),
                                          actors->motion_context);
                if (dy < 0) dy += 3;
                entity->y += dy >> 2;
                entity->phase = (uint16_t)(entity->phase + 0x200);
            }
            if (entity->state == 0) entity->state = 1;
            else if (entity->state == 1) entity->state = 2;
        } else if (!entity->disappearing && entity->entity_type == 10 &&
                   actors->random) {
            /* sub_0805E5D4 states 0-2: each child waits for its 0..6
             * selector. Children without parameter bit 1 then wait for the
             * selector again and rise by 0.5 pixel on sixteen successful
             * one-in-three clocks before joining state 2. */
            if (entity->state == 0 &&
                actors->random(actors->motion_context) % 7 == entity->variant) {
                entity->state = (entity->parameter & 2) ? 2 : 1;
                entity->step = 0;
            } else if (entity->state == 1 && entity->step == 0 &&
                       actors->random(actors->motion_context) % 7 == entity->variant) {
                entity->timer = 16;
                entity->step = 1;
            } else if (entity->state == 1 && entity->step == 1 &&
                       actors->random(actors->motion_context) % 3 == 0) {
                if (entity->timer == 0) {
                    entity->state = 2;
                    entity->step = 0;
                } else {
                    --entity->timer;
                    entity->y -= 0x8000;
                }
            }
            if (entity->state == 2) {
                if (entity->parameter & 8) entity->animation = 1;
                else if (entity->parameter & 0x10) entity->animation = 2;
            }
        }
        if (entity->disappearing) update_emitted_tail(actors, entity);
    }
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
        actors->camera_mode = 2;
        actors->camera_active = true;
        actors->camera_initialized = false;
        actors->emitted_batches |= AOS_OPENING_EMISSION_DISTANT;
        append_emission(actors, AOS_OPENING_EMISSION_DISTANT);
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

static bool camera_update(AosOpeningActors *actors, int32_t *camera_x) {
    static const int32_t velocity[] = {
        0x80000, 0x40000, 0x38000, -0x40000, -0x10000, -0x80000,
    };
    if (!actors->camera_active || !camera_x || actors->camera_mode >= 6)
        return false;
    if (!actors->camera_initialized) {
        actors->camera_x = *camera_x << 16;
        actors->camera_initialized = true;
    }
    actors->camera_x += velocity[actors->camera_mode];
    *camera_x = actors->camera_x >> 16;
    switch (actors->camera_mode) {
    case 0: return *camera_x > 0x1F;
    case 1: return *camera_x > 0x5F;
    case 2: return *camera_x > 0xDD;
    case 3: return *camera_x <= 0x60;
    /* Mode 4 ends when Soma reaches screen X 0x78 rather than at a camera
     * threshold. It is not used by the opening state reconstructed here. */
    case 5: return *camera_x <= 0x20;
    default: return false;
    }
}

AosOpeningCue aos_opening_actors_update(
    AosOpeningActors *actors,
    const uint16_t animation_durations[AOS_OPENING_ACTOR_COUNT],
    int32_t *camera_x, int32_t camera_y) {
    if (!actors || !actors->active) return AOS_OPENING_CUE_NONE;
    actors->camera_y = camera_y;
    AosOpeningCue output = actors->pending_cue;
    actors->pending_cue = AOS_OPENING_CUE_NONE;
    ++actors->frame;
    for (unsigned actor = 0; actor < AOS_OPENING_ACTOR_COUNT; ++actor)
        if (actors->animation_age[actor] < UINT16_MAX)
            ++actors->animation_age[actor];
    update_emitted(actors);
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
        /* State 3 emits sub_0805D2C8's distant group, pans at 3.5 pixels per
         * frame until the camera passes 0xDD, then waits 0x20 frames before
         * sub_0805D35C's near group, Mina animation 5 and SE_436. */
        if (actors->scene_step == 0 && camera_update(actors, camera_x)) {
            actors->camera_active = false;
            actors->scene_step = 1;
            actors->scene_timer = 32;
        } else if (actors->scene_step == 1 && actors->scene_timer &&
                   --actors->scene_timer == 0) {
            actors->emitted_batches |= AOS_OPENING_EMISSION_NEAR_SOMA;
            append_emission(actors, AOS_OPENING_EMISSION_NEAR_SOMA);
            actor_animation(actors, 0, 5);
            if (output == AOS_OPENING_CUE_NONE)
                output = AOS_OPENING_CUE_ENEMY_APPEARS;
            else
                actors->pending_cue = AOS_OPENING_CUE_ENEMY_APPEARS;
            actors->scene_step = 2;
            actors->scene_timer = 32;
        } else if (actors->scene_step == 2 && actors->scene_timer &&
                   --actors->scene_timer == 0) {
            actors->scene_step = 3;
        }
        break;
    case 4: {
        /* Arikado's animation 2 completes, the controller waits 0x10 frames,
         * then animation 3 and SE_480 begin. */
        uint16_t duration = animation_durations ? animation_durations[1] : 0;
        if (actors->scene_step == 0 && duration &&
            actors->animation_age[1] >= duration) {
            /* sub_0805D410 raises parent bit 2 here. Both private child
             * callbacks enter their two-stage 0x20 + 0x10 blink sequence. */
            begin_emitted_disappearance(actors);
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

size_t aos_opening_emission_spawns(AosOpeningEmission emission,
                                   int16_t soma_x, AosOpeningSpawn *spawns,
                                   size_t capacity) {
    static const AosOpeningSpawn distant[] = {
        {0x158, 0x29F, 10, 4, 0},
        {0x170, 0x29F, 10, 4, 1},
        {0x148, 0x29F, 10, 2, 2},
        {0x168, 0x29F, 10, 2, 3},
        {0x188, 0x29F, 10, 2, 4},
        {0x14F, 0x288, 8, 0, 0},
    };
    AosOpeningSpawn near[] = {
        {(int16_t)(soma_x + 0x18), 0x29F, 10, 0x12, 5},
        {(int16_t)(soma_x - 0x30), 0x29F, 10, 2, 6},
        {0x7C, 0x290, 8, 0, 0},
        {0xC4, 0x280, 8, 0, 0},
    };
    const AosOpeningSpawn *source = NULL;
    size_t count = 0;
    if (emission == AOS_OPENING_EMISSION_DISTANT) {
        source = distant;
        count = sizeof distant / sizeof distant[0];
    } else if (emission == AOS_OPENING_EMISSION_NEAR_SOMA) {
        source = near;
        count = sizeof near / sizeof near[0];
    }
    size_t copied = count < capacity ? count : capacity;
    if (spawns && copied) memcpy(spawns, source, copied * sizeof *spawns);
    return count;
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
