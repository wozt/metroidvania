/* SPDX-License-Identifier: GPL-3.0-only */
/* Animated native-title divider. The geometry implementation in
 * rotating_line/ retains the prototype's MIT license. */
#ifndef METROIDVANIA_SPLIT_H
#define METROIDVANIA_SPLIT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "metroidvania_split_audio.h"
#include "rotating_line/curve.h"

typedef enum { MV_GAME_ARIA = 0, MV_GAME_MZM = 1 } MvGame;

enum {
    MV_SCREEN_W = PROTO_WIDTH,
    MV_SCREEN_H = PROTO_HEIGHT,
    MV_FRAMES_PER_TURN = 2160,
};

typedef struct {
    ProtoConfig config;
    ProtoAnimation animation;
    ProtoFrame frame;
} MvSplit;

typedef struct {
    bool held;
    bool launched;
    float elapsed;
    float pulse_elapsed;
} MvSplitHold;

/* Patch-0046 defaults from prototype_aria_metroid_horizontal_a.json. */
void mv_split_config_defaults(ProtoConfig *config);
bool mv_split_init(MvSplit *split, const char *json_override,
                   char *error, size_t error_size);
void mv_split_update_frame(MvSplit *split);
void mv_split_tick(MvSplit *split, float dt, bool rotate);

/* Audio duration controls the eight-cell visual wave's synchronization. */
void mv_split_trigger_pulse(MvSplit *split, bool aria, float audio_duration);
void mv_split_pulse_tick(ProtoAnimation *animation, float dt);
/* ROM-native sound selected for the corresponding side. */
uint16_t mv_split_native_sound(bool aria);

bool mv_split_hold_press(MvSplitHold *hold);
void mv_split_hold_release(MvSplitHold *hold);
/* Returns true once at three seconds. pulse_count reports each 0.5 s beat. */
bool mv_split_hold_tick(MvSplitHold *hold, float dt, unsigned *pulse_count);

uint16_t mv_split_angle(uint32_t frame);
uint16_t mv_split_rotation_angle(const MvSplit *split);
MvGame mv_split_top(uint16_t angle);
static inline MvGame mv_split_bottom(uint16_t angle) {
    return mv_split_top(angle) == MV_GAME_ARIA ? MV_GAME_MZM : MV_GAME_ARIA;
}
MvGame mv_split_pixel(const MvSplit *split, int x, int y);
void mv_split_compose(const MvSplit *split, const uint8_t *aria,
                      const uint8_t *mzm, uint8_t *out);

#endif
