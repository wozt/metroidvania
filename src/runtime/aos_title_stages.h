/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_TITLE_STAGES_H
#define AOS_TITLE_STAGES_H
#include <stdbool.h>
#include <stdint.h>

/* State held by the original title entity slots 0x12 and 0x13.
 * The caller owns image data, affine BG registers, and entity updates. */
typedef struct {
    int32_t motion;
    int32_t x;
    int32_t y;
    uint16_t flags;
    uint8_t progress;
    uint8_t entity_53e_1;
    int32_t other_timer;
    uint16_t other_flags;
    uint16_t stage;
    uint16_t stage_timer;
    int32_t affine_scale;
    uint16_t alpha;
    uint16_t affine_angle;
    int32_t reveal_motion;
    bool show_bg0;
    bool show_bg1;
    bool palette_event;
    uint16_t idle_timer;
} AosTitleStages;

void aos_title_stage4(AosTitleStages *state);
void aos_title_stage5(AosTitleStages *state);
void aos_title_stage6(AosTitleStages *state);
void aos_title_stage7(AosTitleStages *state);
#endif
