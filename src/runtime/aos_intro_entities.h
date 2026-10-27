/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_INTRO_ENTITIES_H
#define AOS_INTRO_ENTITIES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    AOS_INTRO_TEXT_X = 4,
    AOS_INTRO_TEXT_WIDTH = 0x25,
    AOS_INTRO_TEXT_HEIGHT = 1,
    AOS_INTRO_TEXT_ROWS = 0x15,
    AOS_INTRO_TEXT_GLYPH_CAPACITY = 128,
    AOS_INTRO_TEXT_INSERT_CAPACITY = 8,
};

typedef bool (*AosIntroTextResolver)(void *user, uint8_t id,
                                     const uint8_t **script,
                                     size_t *script_size);

enum {
    AOS_INTRO_ENTITY_MAIN,
    AOS_INTRO_ENTITY_SCROLL_FOLLOWER,
    AOS_INTRO_ENTITY_AFFINE_RISE,
    AOS_INTRO_ENTITY_ORBIT,
    AOS_INTRO_ENTITY_PANEL_LEFT,
    AOS_INTRO_ENTITY_PANEL_RIGHT,
    AOS_INTRO_ENTITY_COUNT,
};

typedef struct {
    uint8_t kind;
    uint8_t state;
    uint8_t variant;
    uint8_t animation;
    uint8_t animation_action;
    uint8_t frame_modulo;
    bool alive;
    bool visible;
    bool affine;
    bool double_draw;
    int32_t x;
    int32_t y;
    int32_t vx;
    int32_t vy;
    uint32_t scale;
    uint32_t phase_x;
    uint32_t phase_y;
    uint32_t timer;
} AosIntroEntity;

/* Controller-only representation of the entity group created by
 * sub_080094AC. Sprite graphics stay host requests; the emitted glyph rows
 * feed the separate native-format font rasterizer. */
typedef struct {
    AosIntroEntity entities[AOS_INTRO_ENTITY_COUNT];
    int16_t bg0_vertical;
    uint8_t text_column;
    uint8_t text_draw_column;
    const uint8_t *text_script;
    size_t text_script_size;
    int32_t text_script_position;
    AosIntroTextResolver text_resolver;
    void *text_resolver_user;
    uint8_t text_glyphs[AOS_INTRO_TEXT_GLYPH_CAPACITY];
    size_t text_glyph_count;
    uint8_t text_insert_ids[AOS_INTRO_TEXT_INSERT_CAPACITY];
    size_t text_insert_count;
    bool text_step_requested;
    bool text_clear_requested;
    bool text_script_missing;
    bool text_script_finished;
    bool text_script_malformed;
    bool text_output_truncated;
    bool affine_sound_requested;
    bool alpha_override_valid;
    uint16_t alpha_override;
    bool graphics_release_requested;
} AosIntroEntities;

void aos_intro_entities_init(AosIntroEntities *group);
void aos_intro_entities_clear(AosIntroEntities *group);
void aos_intro_entities_set_text_script(AosIntroEntities *group,
                                        const uint8_t *script,
                                        size_t script_size,
                                        AosIntroTextResolver resolver,
                                        void *resolver_user);
void aos_intro_entities_update(AosIntroEntities *group, int32_t mode_stage,
                               int32_t bg1_y, int32_t bg1_velocity,
                               int32_t bg3_y);

#endif
