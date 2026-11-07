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
    /* The first control-2 beat releases Object20's actor-entry state and
     * advances entity+0x38 to camera phase 1. The following beats select
     * sub_0805D410 states 2 through 6. */
    case 1: return AOS_OPENING_CUE_ENTRY_PAN;
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
    case 3: return "Genya Arikado";
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
        .soma_animation = 0x51,
        .soma_x = AOS_OPENING_SOMA_X << 16,
    };
}

void aos_opening_actors_set_motion(AosOpeningActors *actors,
                                   AosOpeningRandom random,
                                   AosOpeningSine sine,
                                   AosOpeningAngle angle,
                                   AosOpeningFloorDepth floor_depth,
                                   AosOpeningWallPush wall_push,
                                   void *context) {
    if (!actors) return;
    actors->random = random;
    actors->sine = sine;
    actors->angle = angle;
    actors->floor_depth = floor_depth;
    actors->wall_push = wall_push;
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

static void begin_opening_soul(AosOpeningActors *actors) {
    actors->soul_x = actors->x[2];
    actors->soul_y = actors->y[2] - (16 << 16);
    actors->soul_vx = 0;
    actors->soul_vy = 0;
    actors->soul_speed = 0x30000;
    actors->soul_turn = 0;
    actors->soul_age = 0;
    actors->soul_active = true;
    actors->soul_absorbed = false;
    if (actors->angle) {
        int16_t dx = (int16_t)(AOS_OPENING_SOMA_X - (actors->soul_x >> 16));
        int16_t dy = (int16_t)((AOS_OPENING_SOMA_Y - 16) -
                               (actors->soul_y >> 16));
        actors->soul_heading =
            (uint32_t)(actors->angle(dx, dy, actors->motion_context) + 0x8000u) &
            0xFFFFu;
    }
}

/* sub_0802D408's red-soul homing path. The opening owns a private instance
 * because the scripted reward is committed only when Object20 completes. */
static void update_opening_soul(AosOpeningActors *actors) {
    if (!actors->soul_active) return;
    ++actors->soul_age;
    int32_t target_x = AOS_OPENING_SOMA_X << 16;
    int32_t target_y = (AOS_OPENING_SOMA_Y - 16) << 16;
    if (!actors->angle || !actors->sine) {
        actors->soul_x += (target_x - actors->soul_x) / 8;
        actors->soul_y += (target_y - actors->soul_y) / 8;
        return;
    }
    int16_t dx = (int16_t)(AOS_OPENING_SOMA_X - (actors->soul_x >> 16));
    int16_t dy = (int16_t)((AOS_OPENING_SOMA_Y - 16) -
                           (actors->soul_y >> 16));
    int32_t difference =
        (int32_t)actors->angle(dx, dy, actors->motion_context) -
        (int32_t)actors->soul_heading;
    bool turn_up = difference > 0 ? difference <= 0x7FFF
                                  : difference < -0x8000;
    actors->soul_heading =
        (uint32_t)((int32_t)actors->soul_heading +
                   (turn_up ? actors->soul_turn : -actors->soul_turn)) &
        0xFFFFu;
    actors->soul_turn += 0x2A;
    if (actors->soul_turn > 0x4000) actors->soul_turn = 0x4000;
    actors->soul_speed += 0x1800;
    if (actors->soul_speed > 0x80000) actors->soul_speed = 0x80000;
    actors->soul_vx =
        (actors->sine(actors->soul_heading + 0x4000u,
                      actors->motion_context) >> 8) *
        (actors->soul_speed >> 8);
    actors->soul_vy =
        (actors->sine(actors->soul_heading, actors->motion_context) >> 8) *
        (actors->soul_speed >> 8);
    actors->soul_x += actors->soul_vx;
    actors->soul_y += actors->soul_vy;
}

static bool point_in_impact_margin(const AosOpeningActors *actors,
                                   int32_t x, int32_t y) {
    int32_t screen_x = (x >> 16) - (actors->camera_x >> 16);
    int32_t screen_y = (y >> 16) - actors->camera_y;
    /* sub_08068AD4 receives (-0x80, -0x40, 0x1F0, 0x120). The latter two
     * values are inclusive rectangle widths, not absolute coordinates. */
    return screen_x >= -0x80 && screen_x <= 0x170 &&
           screen_y >= -0x40 && screen_y <= 0xE0;
}

static bool emitted_in_impact_margin(const AosOpeningActors *actors,
                                     const AosOpeningEntity *entity) {
    return point_in_impact_margin(actors, entity->x, entity->y);
}

static bool emitted_selector(AosOpeningActors *actors,
                             const AosOpeningEntity *entity) {
    return actors->random &&
           actors->random(actors->motion_context) % 7 == entity->variant;
}

typedef struct {
    int8_t x;
    int8_t y;
    uint8_t frame;
    int8_t vx;
    int8_t vy;
    int8_t timer;
} OpeningDebrisSpec;

/* 0x085267D8 and 0x08526802, seven six-byte records consumed by
 * sub_0805E8C8 and sub_0805E9F0. Only the sprite-frame column differs. */
static const OpeningDebrisSpec skeleton_debris[7] = {
    {0, -30, 0x1B, 16, 64, 60}, {0, -5, 0x17, 16, 49, 38},
    {0, -3, 0x19, 17, 48, 23},  {4, -5, 0x15, 12, 56, 43},
    {4, -2, 0x16, 18, 51, 40},  {-4, -5, 0x18, 24, 50, 33},
    {-4, -2, 0x1A, 12, 40, 26},
};

static const OpeningDebrisSpec winged_debris[7] = {
    {0, -30, 0x24, 16, 64, 60}, {0, -5, 0x25, 16, 49, 38},
    {0, -3, 0x26, 17, 48, 23},  {4, -5, 0x27, 12, 56, 43},
    {4, -2, 0x28, 18, 51, 40},  {-4, -5, 0x29, 24, 50, 33},
    {-4, -2, 0x2A, 12, 40, 26},
};

static void append_debris(AosOpeningActors *actors,
                          const AosOpeningEntity *entity, unsigned piece_count) {
    const OpeningDebrisSpec *specs = entity->entity_type == 8
                                         ? winged_debris : skeleton_debris;
    for (unsigned i = 0; i < piece_count &&
                         actors->debris_count < AOS_OPENING_DEBRIS_CAPACITY;
         ++i) {
        const OpeningDebrisSpec *spec = &specs[i];
        int32_t vx = spec->vx;
        int32_t vy = spec->vy;
        if (entity->entity_type == 10 && actors->random) {
            vx += 0x10 - (int32_t)(actors->random(actors->motion_context) & 0x1F);
            vy += 0x10 - (int32_t)(actors->random(actors->motion_context) & 0x1F);
        }
        int32_t x_offset = spec->x;
        if (entity->mirrored) {
            x_offset = -x_offset;
            vx = -vx;
        }
        actors->debris[actors->debris_count++] = (AosOpeningDebris){
            .x = entity->x + x_offset * 0x10000,
            .y = entity->y + (int32_t)spec->y * 0x10000,
            .vx = vx * 0x1000,
            .vy = -vy * 0x1000,
            .frame = spec->frame,
            .source_type = entity->entity_type,
            .timer = (uint8_t)(spec->timer / 2),
            .active = true,
            .mirrored = entity->mirrored,
        };
    }
}

static uint16_t angle_to(AosOpeningActors *actors, int32_t from_x,
                         int32_t from_y, int32_t to_x, int32_t to_y) {
    if (!actors->angle) return 0;
    int16_t dx = (int16_t)((to_x >> 16) - (from_x >> 16));
    int16_t dy = (int16_t)((to_y >> 16) - (from_y >> 16));
    return actors->angle(dx, dy, actors->motion_context);
}

static void append_projectile(AosOpeningActors *actors,
                              const AosOpeningEntity *entity) {
    if (actors->projectile_count >= AOS_OPENING_PROJECTILE_CAPACITY) return;
    int32_t y = entity->y - 0x200000;
    uint32_t random = actors->random
                          ? actors->random(actors->motion_context) : 0;
    int32_t target_y = actors->y[2] - 0x80000 -
                       (int32_t)(random & 0x7F) * 0x1000;
    uint16_t heading = (uint16_t)(angle_to(
        actors, entity->x, y, actors->x[2], target_y) + 0x8000);
    actors->projectiles[actors->projectile_count++] = (AosOpeningProjectile){
        .x = entity->x,
        .y = y,
        .speed = 0x30000,
        .heading = heading,
        .timer = 0xC8,
        .active = true,
    };
}

static void update_debris(AosOpeningActors *actors) {
    for (unsigned i = 0; i < actors->debris_count; ++i) {
        AosOpeningDebris *debris = &actors->debris[i];
        if (!debris->active) continue;
        if (!point_in_impact_margin(actors, debris->x, debris->y) ||
            debris->timer-- == 0 || (debris->y >> 16) > 0xAA) {
            debris->active = false;
            continue;
        }
        debris->x += debris->vx;
        debris->y += debris->vy;
        debris->vy += 0x3000;
    }
}

/* 0x08118CE0: the shared enemy death-blink table consumed by
 * generic_death's `kind->blink[timer] & 1`; a set bit hides the actor. */
static const uint8_t opening_death_blink[40] = {
    0x00, 0x01, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00,
    0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x00,
    0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
};

/* sub_0806D578(x, y, priority): one flame at the death point, mirrored at
 * random like the ported enemy path. */
static void append_opening_flame(AosOpeningActors *actors, int32_t x,
                                 int32_t y) {
    if (actors->flame_count >= AOS_OPENING_FLAME_CAPACITY) return;
    actors->flames[actors->flame_count++] = (AosOpeningFlame){
        .x = x, .y = y, .timer = 0x20, .active = true,
        .mirrored = actors->random
                        ? (actors->random(actors->motion_context) & 1) != 0
                        : false,
    };
}

static void update_opening_flames(AosOpeningActors *actors) {
    for (unsigned i = 0; i < actors->flame_count; ++i) {
        AosOpeningFlame *flame = &actors->flames[i];
        if (!flame->active) continue;
        if (flame->age < UINT8_MAX) ++flame->age;
        if (flame->timer) --flame->timer;
        if (!flame->timer) flame->active = false;
    }
}

/* sub_0805D410 state 5 at timer 0xD0 on entity[0x1c]: the dying mark,
 * sub_0806D578's flame, sub_0805E9F0's six winged pieces (the first six
 * records of 0x08526802), sub_0805EAFC's soul orb and SE_0x6B all fire
 * together; the body then runs generic_death's 0x28-frame blink. */
static void begin_skeleton_death(AosOpeningActors *actors) {
    AosOpeningEntity body = {
        .x = actors->x[2],
        .y = actors->y[2],
        .entity_type = 8,
        .mirrored = true,
    };
    append_debris(actors, &body, 6);
    append_opening_flame(actors, actors->x[2], actors->y[2] - 0x60000);
    actors->skeleton_death = 0x28;
}

/* generic_death: hidden while the blink table says so, a flame every
 * fourth timer, removed when the 0x28-frame timer expires. */
static void update_skeleton_death(AosOpeningActors *actors) {
    if (!actors->skeleton_death) return;
    uint8_t timer = actors->skeleton_death;
    actors->visible[2] = timer > 0x27 || !(opening_death_blink[timer] & 1);
    if ((timer & 3) == 0)
        append_opening_flame(actors, actors->x[2],
                             actors->y[2] - 0x60000);
    if (--actors->skeleton_death == 0) actors->visible[2] = false;
}

static uint16_t turn_projectile(AosOpeningActors *actors,
                                const AosOpeningProjectile *projectile) {
    uint16_t wanted = angle_to(actors, projectile->x, projectile->y,
                               actors->x[2], actors->y[2] - 0x100000);
    int32_t difference = (int32_t)wanted - projectile->heading;
    bool up = difference > 0 ? difference <= 0x7FFF : difference < -0x8000;
    return (uint16_t)(projectile->heading +
                      (up ? projectile->turn : -projectile->turn));
}

static void update_projectiles(AosOpeningActors *actors) {
    for (unsigned i = 0; i < actors->projectile_count; ++i) {
        AosOpeningProjectile *projectile = &actors->projectiles[i];
        projectile->events = AOS_OPENING_ENTITY_EVENT_NONE;
        if (!projectile->active) continue;
        if (projectile->animation_age < UINT16_MAX)
            ++projectile->animation_age;
        if (projectile->state <= 1 && actors->sine) {
            projectile->heading = turn_projectile(actors, projectile);
            projectile->turn = (uint16_t)(projectile->turn + 0x2A);
            if (projectile->turn > 0x4000) projectile->turn = 0x4000;
            projectile->speed += 0x1800;
            if (projectile->speed > 0x80000) projectile->speed = 0x80000;
            projectile->vx =
                (actors->sine((uint16_t)(projectile->heading + 0x4000),
                              actors->motion_context) >> 8) *
                (projectile->speed >> 8);
            projectile->vy =
                (actors->sine(projectile->heading,
                              actors->motion_context) >> 8) *
                (projectile->speed >> 8);
            projectile->x += projectile->vx;
            projectile->y += projectile->vy;
        }
        int32_t dx = (actors->x[2] >> 16) - (projectile->x >> 16);
        int32_t dy = (actors->y[2] >> 16) - (projectile->y >> 16);
        if (dx >= -0x10 && dx <= 0x10 && dy >= -0x10 && dy <= 0x10) {
            projectile->events = AOS_OPENING_ENTITY_EVENT_SOUND_0XBC;
            projectile->active = false;
            continue;
        }
        if (projectile->timer) --projectile->timer;
        if (!projectile->timer) {
            if (projectile->state == 0) {
                projectile->state = 1;
                projectile->timer = 0x40;
            } else {
                projectile->active = false;
            }
        }
    }
}

static void finish_emitted(AosOpeningActors *actors, AosOpeningEntity *entity,
                           bool projectile) {
    if (emitted_in_impact_margin(actors, entity)) {
        entity->events |= AOS_OPENING_ENTITY_EVENT_SOUND_0X6B |
                          AOS_OPENING_ENTITY_EVENT_DEBRIS;
        append_debris(actors, entity, 7);
    }
    if (projectile) {
        entity->events |= AOS_OPENING_ENTITY_EVENT_PROJECTILE;
        append_projectile(actors, entity);
    }
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

static void update_skeleton_flight(AosOpeningActors *actors,
                                   AosOpeningEntity *entity) {
    /* sub_0805E5D4 state 2, parameter bit 3. sub_0806E140 tentatively
     * advances a quarter pixel toward the entity's facing side and two
     * pixels down. In open air it restores X and applies only the vertical
     * motion; on a floor it retains X and snaps Y by the returned depth. */
    if (entity->step == 0) {
        entity->animation = 1;
        entity->timer = 0xA0;
        entity->step = 1;
    }
    if (!actors->random || (actors->random(actors->motion_context) & 1) ||
        entity->timer == 0)
        return;

    int32_t candidate_x = entity->x + (entity->mirrored ? 0x4000 : -0x4000);
    int32_t candidate_y = entity->y + 0x20000;
    if (actors->wall_push) {
        int push = actors->wall_push(candidate_x >> 16, entity->y >> 16,
                                     !entity->mirrored,
                                     actors->motion_context);
        candidate_x += (int32_t)((uint32_t)push << 16);
    }
    int depth = actors->floor_depth
                    ? actors->floor_depth(candidate_x >> 16,
                                          candidate_y >> 16,
                                          actors->motion_context)
                    : 0;
    if (depth) {
        entity->x = candidate_x;
        entity->y = ((candidate_y >> 16) + depth) * 0x10000;
    } else {
        entity->y = candidate_y;
    }
    --entity->timer;
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
            uint8_t initial_state = entity->state;
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
            if (initial_state == 2 && entity->state == 2) {
                if (entity->parameter & 8)
                    update_skeleton_flight(actors, entity);
                else if (entity->parameter & 0x10) {
                    entity->animation = 2;
                    if (entity->step == 0) entity->step = 1;
                }
            }
        }
        if (entity->disappearing) update_emitted_tail(actors, entity);
    }
}

void aos_opening_actors_input(AosOpeningActors *actors, uint16_t new_input) {
    if (!actors) return;
    actors->notice_input |= new_input;
}

void aos_opening_actors_trigger(AosOpeningActors *actors, AosOpeningCue cue) {
    if (!actors || !actors->active || cue == AOS_OPENING_CUE_NONE) return;
    switch (cue) {
    case AOS_OPENING_CUE_ENTRY_PAN:
        /* +0xb==1 writes entity+0x38=1 before its sub_0805CCE4 wait. */
        actors->camera_mode = 1;
        actors->camera_active = true;
        actors->camera_initialized = false;
        actors->scene_state = 1;
        actors->pending_cue = cue;
        break;
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
        /* +0xb==4 inner case 0 writes entity+0x38=3: the return pan. */
        actors->camera_mode = 3;
        actors->camera_active = true;
        actors->camera_initialized = false;
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
        actors->soul_active = false;
        actors->soul_notice = 0;
        actors->notice_input = 0;
        actors->soma_hidden = false;
        actors->soma_animation = 0x51;
        actors->scene_state = 6;
        actors->scene_step = 0;
        actors->pending_cue = cue;
        break;
    case AOS_OPENING_CUE_COMPLETE:
        actors->scene_state = 7;
        actors->pending_cue = cue;
        /* +0xb==8 sub-state 0 writes entity+0x38=4: the return pan that
         * recenters on Soma before the scene flags complete. */
        actors->camera_mode = 4;
        actors->camera_active = true;
        actors->camera_initialized = false;
        break;
    default:
        break;
    }
}

bool aos_opening_actors_blocks_script(const AosOpeningActors *actors) {
    if (!actors || !actors->active) return false;
    switch (actors->scene_state) {
    case 3: return actors->scene_step < 3;
    case 4: return actors->scene_step < 2;
    case 5: return !actors->soul_absorbed || actors->soul_notice == 1 ||
                   actors->soul_notice == 2;
    default: return false;
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
     * threshold: sub_0805CCE4's sixth case compares the player position. */
    case 4: return (actors->soma_x >> 16) - *camera_x >= 0x78;
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
    update_debris(actors);
    update_projectiles(actors);
    update_emitted(actors);
    update_opening_soul(actors);
    update_opening_flames(actors);
    update_skeleton_death(actors);
    /* Every active pan advances each frame; scene states consume the
     * completion flag. State 3 keeps ownership of its step transition. */
    bool camera_done = camera_update(actors, camera_x);
    if (camera_done && actors->scene_state != 3)
        actors->camera_active = false;
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
        if (actors->scene_step == 0 && camera_done) {
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
                begin_skeleton_death(actors);
                begin_opening_soul(actors);
                due = AOS_OPENING_CUE_ENEMY_DEFEATED;
            } else if (actors->scene_timer == 0x8F) {
                due = AOS_OPENING_CUE_SOUL_ABSORBED;
                actors->soul_active = false;
                actors->soul_absorbed = true;
                actors->soul_notice = 1;
                actors->soul_x = AOS_OPENING_SOMA_X << 16;
                actors->soul_y = (AOS_OPENING_SOMA_Y - 16) << 16;
                actors->soma_animation = 0x52;
                actors->soma_hidden = false;
                actors->scene_step = 2;
            }
            if (due != AOS_OPENING_CUE_NONE) {
                if (output == AOS_OPENING_CUE_NONE) output = due;
                else actors->pending_cue = due;
            }
        }
        /* sub_0805D410 state 5 keeps pose 0x52 from the start of the
         * 0xD2 countdown through the window wait, alternating the player
         * object's visibility on the timer's bit 0. The blink stops once
         * the window starts closing (sub-state 5 sets the pose once more
         * without blinking). */
        if (actors->scene_step >= 1) {
            actors->soma_animation = 0x52;
            bool flashing = actors->scene_step == 1 ||
                            actors->soul_notice == 1 ||
                            actors->soul_notice == 2;
            actors->soma_hidden = flashing && (actors->frame & 1u) != 0;
        }
        break;
    default:
        break;
    }
    /* sub_0805D410 state 5, sub-state 3: the sub_0800E708 obtained-soul
     * window opens with the absorption, stays modal while the player has
     * not pressed A (each press plays SE_0x132), then sub_0800F0AC closes
     * it before the script may continue to state 6. */
    if (actors->soul_notice == 1) {
        actors->soul_notice = 2;
    } else if (actors->soul_notice == 2 &&
               (actors->notice_input & (AOS_KEY_A | AOS_KEY_START)) != 0) {
        actors->notice_input = 0;
        actors->soul_notice = 3;
        actors->soul_notice_timer = 0x20;
        if (output == AOS_OPENING_CUE_NONE)
            output = AOS_OPENING_CUE_SOUL_NOTICE_DISMISS;
        else
            actors->pending_cue = AOS_OPENING_CUE_SOUL_NOTICE_DISMISS;
    } else if (actors->soul_notice == 3 && actors->soul_notice_timer &&
               --actors->soul_notice_timer == 0) {
        actors->soul_notice = 0;
    }
    actors->notice_input = 0;
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
