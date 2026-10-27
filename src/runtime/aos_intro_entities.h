/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_INTRO_ENTITIES_H
#define AOS_INTRO_ENTITIES_H

#include <stdbool.h>
#include <stdint.h>

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
 * sub_080094AC. Sprite graphics and script drawing stay host requests. */
typedef struct {
    AosIntroEntity entities[AOS_INTRO_ENTITY_COUNT];
    int16_t bg0_vertical;
    uint8_t text_column;
    uint8_t text_draw_column;
    bool text_step_requested;
    bool affine_sound_requested;
    bool alpha_override_valid;
    uint16_t alpha_override;
    bool graphics_release_requested;
} AosIntroEntities;

void aos_intro_entities_init(AosIntroEntities *group);
void aos_intro_entities_clear(AosIntroEntities *group);
void aos_intro_entities_update(AosIntroEntities *group, int32_t mode_stage,
                               int32_t bg1_y, int32_t bg1_velocity,
                               int32_t bg3_y);

#endif
