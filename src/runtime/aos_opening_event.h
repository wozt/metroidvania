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
    /* sub_08067934 writes WIN0H=0x08E8 (x=8..232). sub_08067804 opens
     * WIN0V around y=124 to y=88..160. screenshots/dialogue.jpeg shows
     * the first glyph at the x=40 split used by sub_0806724C; the software
     * canvas keeps its native six-pixel left inset. */
    AOS_OPENING_TEXT_PANEL_X = 8,
    AOS_OPENING_TEXT_PANEL_Y = 88,
    AOS_OPENING_TEXT_PANEL_WIDTH = 224,
    AOS_OPENING_TEXT_PANEL_HEIGHT = 72,
    AOS_OPENING_TEXT_X = 34,
    AOS_OPENING_TEXT_Y = 96,
    AOS_OPENING_PROMPT_X = 220,
    AOS_OPENING_PROMPT_Y = 154,
    AOS_OPENING_LINES = 4,
    AOS_OPENING_GLYPHS_PER_LINE = 80,
    AOS_OPENING_EMISSION_MAX_SPAWNS = 6,
    AOS_OPENING_EMITTED_CAPACITY = 10,
    AOS_OPENING_DEBRIS_CAPACITY = AOS_OPENING_EMITTED_CAPACITY * 7,
    AOS_OPENING_PROJECTILE_CAPACITY = 7,
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
    AOS_OPENING_CUE_SOUL_NOTICE_DISMISS,
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

/* Portable requests raised by the two private assault callbacks. Hosts may
 * consume these after aos_opening_actors_update without coupling the story
 * controller to audio, particles or the combat entity allocator. */
typedef enum {
    AOS_OPENING_ENTITY_EVENT_NONE = 0,
    AOS_OPENING_ENTITY_EVENT_SOUND_0X6B = 1u << 0,
    AOS_OPENING_ENTITY_EVENT_DEBRIS = 1u << 1,
    AOS_OPENING_ENTITY_EVENT_PROJECTILE = 1u << 2,
    AOS_OPENING_ENTITY_EVENT_SOUND_0XBC = 1u << 3,
} AosOpeningEntityEvent;

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
    uint8_t events;
    bool active;
    bool mirrored;
    bool hidden;
    bool disappearing;
} AosOpeningEntity;

/* Seven pieces created by sub_0805E8C8/sub_0805E9F0 and updated by
 * sub_0805F044. source_type selects the Skeleton or Winged Skeleton sheet. */
typedef struct {
    int32_t x;
    int32_t y;
    int32_t vx;
    int32_t vy;
    uint8_t frame;
    uint8_t source_type;
    uint8_t timer;
    bool active;
    bool mirrored;
} AosOpeningDebris;

/* sub_0805ED80/sub_0805EE88's attack aimed at Object20's principal enemy. */
typedef struct {
    int32_t x;
    int32_t y;
    int32_t vx;
    int32_t vy;
    int32_t speed;
    uint16_t heading;
    uint16_t turn;
    uint16_t animation_age;
    uint8_t state;
    uint8_t timer;
    uint8_t events;
    bool active;
} AosOpeningProjectile;

/* sub_0806D578's death flame and generic_death's four-frame respawns around
 * the scripted Winged Skeleton. */
typedef struct {
    int32_t x;
    int32_t y;
    uint8_t age;
    uint8_t timer;
    bool active;
    bool mirrored;
} AosOpeningFlame;

#define AOS_OPENING_FLAME_CAPACITY 12

typedef uint32_t (*AosOpeningRandom)(void *context);
typedef int32_t (*AosOpeningSine)(uint32_t angle, void *context);
typedef uint16_t (*AosOpeningAngle)(int16_t x, int16_t y, void *context);
typedef int (*AosOpeningFloorDepth)(int32_t x, int32_t y, void *context);
typedef int (*AosOpeningWallPush)(int32_t x, int32_t y, bool moving_left,
                                  void *context);

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
    int32_t camera_y;
    uint8_t camera_mode;
    uint8_t emitted_batches;
    uint8_t emitted_count;
    bool camera_active;
    bool camera_initialized;
    AosOpeningEntity emitted[AOS_OPENING_EMITTED_CAPACITY];
    AosOpeningDebris debris[AOS_OPENING_DEBRIS_CAPACITY];
    AosOpeningProjectile projectiles[AOS_OPENING_PROJECTILE_CAPACITY];
    AosOpeningFlame flames[AOS_OPENING_FLAME_CAPACITY];
    uint8_t debris_count;
    uint8_t projectile_count;
    uint8_t flame_count;
    /* sub_0805D410 state 5, timer 0xD0: entity[0x1c] + 0x58 bit 0x20 enters
     * the Winged Skeleton's generic death, a 0x28-frame blink (table
     * 0x08118CE0) with sub_0806D578 flames while sub_0805E9F0's six pieces
     * and sub_0805EAFC's soul orb are already in flight. */
    uint8_t skeleton_death;
    AosOpeningRandom random;
    AosOpeningSine sine;
    AosOpeningAngle angle;
    AosOpeningFloorDepth floor_depth;
    AosOpeningWallPush wall_push;
    void *motion_context;
    AosOpeningCue pending_cue;
    uint32_t frame;
    uint8_t soma_animation;
    bool active;
    bool soma_hidden;
    /* The opening's guaranteed Winged Skeleton soul uses the same curved
     * homing motion as an ordinary red-soul pickup, but remains owned by the
     * story controller so editor previews do not mutate an inventory. */
    int32_t soul_x;
    int32_t soul_y;
    int32_t soul_vx;
    int32_t soul_vy;
    int32_t soul_speed;
    int32_t soul_turn;
    uint32_t soul_heading;
    uint16_t soul_age;
    bool soul_active;
    bool soul_absorbed;
    /* sub_0800E708's obtained-soul window: 0 closed, 1 opening, 2 waiting
     * for the player, 3 closing after sub_0800F0AC. The script stays
     * blocked while it is 1 or 2; native state 5 continues as soon as the
     * window starts closing, so 3 does not block. */
    uint8_t soul_notice;
    uint8_t soul_notice_timer;
    uint16_t notice_input;
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
/* Injects the native RNG, sine, ArcTan2 and collision probes without coupling the
 * portable scene model to the gameplay runtime. NULL callbacks leave their
 * corresponding query unavailable; a missing floor probe means open air. */
void aos_opening_actors_set_motion(AosOpeningActors *actors,
                                   AosOpeningRandom random,
                                   AosOpeningSine sine,
                                   AosOpeningAngle angle,
                                   AosOpeningFloorDepth floor_depth,
                                   AosOpeningWallPush wall_push,
                                   void *context);
/* Connects a script control-2 beat to sub_0805D410's actor state. */
void aos_opening_actors_trigger(AosOpeningActors *actors, AosOpeningCue cue);
/* Feeds edge input to the modal obtained-soul window; consumed by the next
 * aos_opening_actors_update. Other scene input stays with the text parser. */
void aos_opening_actors_input(AosOpeningActors *actors, uint16_t new_input);
/* True while an Object20 beat owns the scene. The text parser waits instead
 * of accepting the next page early, so rapid A presses cannot skip or strand
 * the enemy reveal, attack or soul absorption. */
bool aos_opening_actors_blocks_script(const AosOpeningActors *actors);
/* Advances one frame and returns one audio cue due on that frame. Animation
 * durations are supplied by the exported actor library, keeping this model
 * independent from SDL and usable by editor previews. camera_x is a whole-
 * pixel host camera position; Object20 retains the horizontal fractional
 * part itself. camera_y is read by sub_08068AD4's portable margin test. */
AosOpeningCue aos_opening_actors_update(
    AosOpeningActors *actors,
    const uint16_t animation_durations[AOS_OPENING_ACTOR_COUNT],
    int32_t *camera_x, int32_t camera_y);
/* Returns the source-derived spawn descriptors for one emission bit. Soma's
 * world X is required because sub_0805D35C places two entities relative to
 * him. The return value is the full batch size, even when capacity is lower. */
size_t aos_opening_emission_spawns(AosOpeningEmission emission,
                                   int16_t soma_x, AosOpeningSpawn *spawns,
                                   size_t capacity);
bool aos_opening_event_commit(AosOpeningEvent *event, uint32_t events[8],
                              AosStats *stats);

#endif
