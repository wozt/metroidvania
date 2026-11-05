/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_OPENING_EVENT_H
#define AOS_OPENING_EVENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aos_stats.h"

enum {
    AOS_OPENING_TEXT_ID = 0x000C,
    AOS_OPENING_EVENT_ID = 0x20,
    AOS_OPENING_OBJECT_ID = 0x20,
    AOS_OPENING_OBJECT_PARAMETER = 3,
    /* sub_0805D198 places Soma here before the first-room scene. */
    AOS_OPENING_SOMA_X = 0xA8,
    AOS_OPENING_SOMA_Y = 0x2C0,
    /* Object20's actor table places Mina, Arikado and the distant winged
     * skeleton at these world coordinates before the dialogue starts. */
    AOS_OPENING_ACTOR_COUNT = 3,
    AOS_OPENING_MINA_X = 0xA8,
    AOS_OPENING_MINA_Y = 0x2C0,
    AOS_OPENING_ARIKADO_X = 0x119,
    AOS_OPENING_ARIKADO_Y = 0x2C0,
    AOS_OPENING_SKELETON_X = 0x16F,
    AOS_OPENING_SKELETON_Y = 0x268,
    AOS_OPENING_LINES = 4,
    AOS_OPENING_GLYPHS_PER_LINE = 80,
    AOS_OPENING_EMISSION_MAX_SPAWNS = 6,
    AOS_OPENING_EMITTED_CAPACITY = 10,
};

typedef bool (*AosOpeningTextResolver)(void *context, uint8_t id,
                                       const uint8_t **script,
                                       size_t *script_size);

/* Semantic Object20 beats exposed to hosts and editor previews. The event
 * parser does not play audio directly. */
typedef enum {
    AOS_OPENING_CUE_NONE,
    AOS_OPENING_CUE_CONFRONTATION,
    AOS_OPENING_CUE_ENEMY_APPEARS,
    AOS_OPENING_CUE_ENEMY_ATTACKS,
    AOS_OPENING_CUE_ENEMY_ATTACKS_AGAIN,
    AOS_OPENING_CUE_ENEMY_DEFEATED,
    AOS_OPENING_CUE_SOUL_ABSORBED,
    AOS_OPENING_CUE_AFTER_BATTLE,
    AOS_OPENING_CUE_COMPLETE,
} AosOpeningCue;

/* The two native sub_0805D2C8/sub_0805D35C emission groups. Entity types
 * refer to Object20's private 0x081186DC resource table, not room objects. */
typedef enum {
    AOS_OPENING_EMISSION_NONE = 0,
    AOS_OPENING_EMISSION_DISTANT = 1u << 0,
    AOS_OPENING_EMISSION_NEAR_SOMA = 1u << 1,
} AosOpeningEmission;

typedef struct {
    int16_t x;
    int16_t y;
    uint8_t entity_type;
    uint8_t parameter;
    uint8_t variant;
} AosOpeningSpawn;

/* One private child allocated by sub_0805E260 or sub_0805E528. The resource
 * table type stays explicit so hosts can select an exported sprite without
 * turning these story actors into ordinary room enemies. */
typedef struct {
    int32_t x;
    int32_t y;
    int32_t vx;
    int32_t vy;
    uint16_t animation_age;
    uint16_t phase;
    uint8_t entity_type;
    uint8_t parameter;
    uint8_t variant;
    uint8_t animation;
    uint8_t state;
    uint8_t step;
    uint8_t timer;
    uint8_t disappear_timer;
    bool active;
    bool mirrored;
    bool hidden;
    bool disappearing;
} AosOpeningEntity;

typedef uint32_t (*AosOpeningRandom)(void *context);
typedef int32_t (*AosOpeningSine)(uint32_t angle, void *context);

typedef struct {
    const uint8_t *script;
    size_t script_size;
    size_t position;
    uint8_t glyphs[AOS_OPENING_LINES][AOS_OPENING_GLYPHS_PER_LINE];
    uint8_t glyph_counts[AOS_OPENING_LINES];
    uint8_t line;
    uint8_t speaker;
    uint8_t portrait;
    uint8_t beat;
    uint8_t beat_count;
    AosOpeningCue cue;
    bool active;
    bool waiting;
    bool complete;
    bool malformed;
    bool dirty;
    bool advanced;
    AosOpeningTextResolver resolve;
    void *resolve_context;
} AosOpeningEvent;

/* The three child entities created by Object20/sub_0805D198. This compact
 * controller covers the source-visible entry movement while the complete
 * sub_0805D410 callback graph remains a later parity slice. */
typedef struct {
    int32_t x[AOS_OPENING_ACTOR_COUNT];
    int32_t y[AOS_OPENING_ACTOR_COUNT];
    uint8_t animation[AOS_OPENING_ACTOR_COUNT];
    uint16_t animation_age[AOS_OPENING_ACTOR_COUNT];
    bool visible[AOS_OPENING_ACTOR_COUNT];
    uint8_t state;
    uint8_t step;
    uint8_t timer;
    uint8_t scene_state;
    uint8_t scene_step;
    uint16_t scene_timer;
    int32_t camera_x;
    uint8_t camera_mode;
    uint8_t emitted_batches;
    uint8_t emitted_count;
    bool camera_active;
    bool camera_initialized;
    AosOpeningEntity emitted[AOS_OPENING_EMITTED_CAPACITY];
    AosOpeningRandom random;
    AosOpeningSine sine;
    void *motion_context;
    AosOpeningCue pending_cue;
    uint32_t frame;
    bool active;
} AosOpeningActors;

bool aos_opening_event_should_start(uint8_t area, uint16_t room,
                                    uint8_t character,
                                    const uint32_t events[8],
                                    bool room_has_controller);
bool aos_opening_event_init(AosOpeningEvent *event, const uint8_t *script,
                            size_t script_size);
void aos_opening_event_set_resolver(AosOpeningEvent *event,
                                    AosOpeningTextResolver resolve,
                                    void *context);
void aos_opening_event_update(AosOpeningEvent *event, uint16_t new_input);
/* Localized speaker ids used by the first-room Object20 script. */
const char *aos_opening_event_speaker_name(uint8_t speaker);
void aos_opening_actors_init(AosOpeningActors *actors);
/* Injects the native RNG and sine primitive without coupling the portable
 * scene model to the gameplay runtime. NULL callbacks leave motion inert. */
void aos_opening_actors_set_motion(AosOpeningActors *actors,
                                   AosOpeningRandom random,
                                   AosOpeningSine sine, void *context);
/* Connects a script control-2 beat to sub_0805D410's actor state. */
void aos_opening_actors_trigger(AosOpeningActors *actors, AosOpeningCue cue);
/* Advances one frame and returns one audio cue due on that frame. Animation
 * durations are supplied by the exported actor library, keeping this model
 * independent from SDL and usable by editor previews. camera_x is a whole-
 * pixel host camera position; Object20 retains the fractional part itself. */
AosOpeningCue aos_opening_actors_update(
    AosOpeningActors *actors,
    const uint16_t animation_durations[AOS_OPENING_ACTOR_COUNT],
    int32_t *camera_x);
/* Returns the source-derived spawn descriptors for one emission bit. Soma's
 * world X is required because sub_0805D35C places two entities relative to
 * him. The return value is the full batch size, even when capacity is lower. */
size_t aos_opening_emission_spawns(AosOpeningEmission emission,
                                   int16_t soma_x, AosOpeningSpawn *spawns,
                                   size_t capacity);
bool aos_opening_event_commit(AosOpeningEvent *event, uint32_t events[8],
                              AosStats *stats);

#endif
