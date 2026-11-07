/* SPDX-License-Identifier: GPL-3.0-only */
#include "metroidvania_split.h"

#include "rotating_line/config.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

enum { MV_SPLIT_CELLS = 8 };
static const float MV_HOLD_SECONDS = 3.0f;
static const float MV_PULSE_SECONDS = 0.5f;

void mv_split_config_defaults(ProtoConfig *config) {
    if (!config) return;
    proto_config_defaults(config);
    config->section_count = 2;
    config->rotation_speed = 10.0f;
    config->line_width = 0.0f;
    config->show_divider = false;
    memcpy(config->violet, (unsigned char[]){0x87, 0x37, 0xCD, 0xFF}, 4);
    memcpy(config->cyan, (unsigned char[]){0x19, 0xDB, 0xE8, 0xFF}, 4);
    memcpy(config->divider_color, (unsigned char[]){0xFF, 0xFF, 0xFF, 0xFF}, 4);
    config->sections[0] = (ProtoSection){
        .formula = PROTO_METROID_HORIZONTAL,
        .amplitude = 30.0f,
        .cycles = MV_SPLIT_CELLS,
        .phase_speed = 0.2f,
        .stagger_delay = 0.4f,
        .repeat_visible = true,
        .loop = true,
    };
    config->sections[1] = (ProtoSection){
        .formula = PROTO_FERMAT_HORIZONTAL,
        .amplitude = 15.0f,
        .cycles = MV_SPLIT_CELLS,
        .phase_speed = 0.2f,
        .stagger_delay = 0.4f,
        .spiral_layout = 1,
        .repeat_visible = true,
        .loop = true,
    };
}

bool mv_split_init(MvSplit *split, const char *json_override,
                   char *error, size_t error_size) {
    if (!split) {
        if (error && error_size) snprintf(error, error_size, "invalid split state");
        return false;
    }
    memset(split, 0, sizeof *split);
    mv_split_config_defaults(&split->config);
    if (json_override && *json_override &&
        !proto_config_load(json_override, &split->config, error, error_size))
        return false;
    proto_animation_init(&split->animation, &split->config);
    proto_make_frame(&split->config, &split->animation, &split->frame);
    if (error && error_size) error[0] = '\0';
    return true;
}

void mv_split_update_frame(MvSplit *split) {
    if (split) proto_make_frame(&split->config, &split->animation, &split->frame);
}

void mv_split_tick(MvSplit *split, float dt, bool rotate) {
    if (!split) return;
    mv_split_pulse_tick(&split->animation, dt);
    float speed = split->config.rotation_speed;
    if (!rotate) split->config.rotation_speed = 0.0f;
    proto_animation_tick(&split->animation, &split->config, dt);
    split->config.rotation_speed = speed;
    mv_split_update_frame(split);
}

void mv_split_trigger_pulse(MvSplit *split, bool aria, float audio_duration) {
    if (!split) return;
    ProtoAnimation *animation = &split->animation;
    float *history = aria ? animation->aria_pulse_history
                          : animation->metroid_pulse_history;
    float *current = aria ? &animation->aria_pulse_age
                          : &animation->metroid_pulse_age;
    for (int index = 6; index > 0; --index) history[index] = history[index - 1];
    history[0] = *current;
    *current = 0.0f;
    float *wave = aria ? &animation->aria_wave_age
                       : &animation->metroid_wave_age;
    unsigned *pending = aria ? &animation->aria_wave_pending
                             : &animation->metroid_wave_pending;
    float *duration = aria ? &animation->aria_wave_duration
                           : &animation->metroid_wave_duration;
    float *queue = aria ? animation->aria_wave_queue_durations
                        : animation->metroid_wave_queue_durations;
    if (*wave < 0.0f) {
        *duration = audio_duration;
        *wave = 0.0f;
    } else if (*pending < 8u) {
        queue[(*pending)++] = audio_duration;
    }
}

void mv_split_pulse_tick(ProtoAnimation *animation, float dt) {
    if (!animation || !isfinite(dt) || dt <= 0.0f) return;
    float *current[] = {&animation->aria_pulse_age,
                        &animation->metroid_pulse_age};
    for (int side = 0; side < 2; ++side)
        if (*current[side] >= 0.0f) {
            *current[side] += dt;
            if (*current[side] >= 1.95f) *current[side] = -1.0f;
        }
    for (int index = 0; index < 7; ++index) {
        float *history[] = {&animation->aria_pulse_history[index],
                            &animation->metroid_pulse_history[index]};
        for (int side = 0; side < 2; ++side)
            if (*history[side] >= 0.0f) {
                *history[side] += dt;
                if (*history[side] >= 1.95f) *history[side] = -1.0f;
            }
    }
    float *wave[] = {&animation->aria_wave_age,
                     &animation->metroid_wave_age};
    unsigned *pending[] = {&animation->aria_wave_pending,
                           &animation->metroid_wave_pending};
    float *duration[] = {&animation->aria_wave_duration,
                         &animation->metroid_wave_duration};
    float *queue[] = {animation->aria_wave_queue_durations,
                      animation->metroid_wave_queue_durations};
    for (int side = 0; side < 2; ++side)
        if (*wave[side] >= 0.0f) {
            *wave[side] += dt;
            float length = *duration[side];
            if (length > 0.0f &&
                *wave[side] >= length - fmaxf(0.000001f, length * 0.00001f)) {
                if (*pending[side]) {
                    *duration[side] = queue[side][0];
                    for (unsigned index = 1; index < *pending[side]; ++index)
                        queue[side][index - 1] = queue[side][index];
                    --*pending[side];
                    *wave[side] = 0.0f;
                } else {
                    *wave[side] = -1.0f;
                }
            }
        }
}

uint16_t mv_split_native_sound(bool aria) {
    return aria ? MV_SPLIT_ARIA_HEARTBEAT_SOUND
                : MV_SPLIT_MZM_LIGHTNING_SOUND;
}

bool mv_split_hold_press(MvSplitHold *hold) {
    if (!hold || hold->held) return false;
    *hold = (MvSplitHold){.held = true};
    return true;
}

void mv_split_hold_release(MvSplitHold *hold) {
    if (hold) *hold = (MvSplitHold){0};
}

bool mv_split_hold_tick(MvSplitHold *hold, float dt, unsigned *pulse_count) {
    if (pulse_count) *pulse_count = 0;
    if (!hold || !hold->held || !isfinite(dt) || dt <= 0.0f) return false;
    hold->elapsed += dt;
    hold->pulse_elapsed += dt;
    while (hold->pulse_elapsed >= MV_PULSE_SECONDS) {
        hold->pulse_elapsed -= MV_PULSE_SECONDS;
        if (pulse_count) ++*pulse_count;
    }
    if (!hold->launched && hold->elapsed >= MV_HOLD_SECONDS) {
        hold->launched = true;
        return true;
    }
    return false;
}

int mv_split_pulse_semitones(unsigned pulse_index) {
    int semitones = -2 + (int)pulse_index;
    return semitones > 12 ? 12 : semitones;
}

uint16_t mv_split_angle(uint32_t frame) {
    return (uint16_t)(((uint64_t)(frame % MV_FRAMES_PER_TURN) << 16) /
                      MV_FRAMES_PER_TURN);
}

uint16_t mv_split_rotation_angle(const MvSplit *split) {
    if (!split) return 0;
    float degrees = fmodf(split->animation.rotation_degrees, 360.0f);
    if (degrees < 0.0f) degrees += 360.0f;
    return (uint16_t)lroundf(degrees * (65536.0f / 360.0f));
}

MvGame mv_split_top(uint16_t angle) {
    return angle < 0x4000 || angle >= 0xC000 ? MV_GAME_ARIA : MV_GAME_MZM;
}

MvGame mv_split_pixel(const MvSplit *split, int x, int y) {
    if (!split || x < 0 || x >= MV_SCREEN_W || y < 0 || y >= MV_SCREEN_H)
        return MV_GAME_MZM;
    return proto_signed_side(&split->frame, x + 0.5f, y + 0.5f) < 0.0f
               ? MV_GAME_ARIA
               : MV_GAME_MZM;
}

void mv_split_compose(const MvSplit *split, const uint8_t *aria,
                      const uint8_t *mzm, uint8_t *out) {
    if (!split || !aria || !mzm || !out) return;
    for (int y = 0; y < MV_SCREEN_H; ++y)
        for (int x = 0; x < MV_SCREEN_W; ++x) {
            size_t at = ((size_t)y * MV_SCREEN_W + (size_t)x) * 4;
            const uint8_t *source =
                mv_split_pixel(split, x, y) == MV_GAME_ARIA ? aria : mzm;
            memcpy(out + at, source + at, 4);
        }
}
