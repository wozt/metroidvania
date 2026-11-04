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
    AOS_OPENING_CUE_SOUL_ABSORBED,
    AOS_OPENING_CUE_AFTER_BATTLE,
    AOS_OPENING_CUE_COMPLETE,
} AosOpeningCue;

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
    uint8_t state;
    uint8_t step;
    uint8_t timer;
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
void aos_opening_actors_update(AosOpeningActors *actors);
bool aos_opening_event_commit(AosOpeningEvent *event, uint32_t events[8],
                              AosStats *stats);

#endif
