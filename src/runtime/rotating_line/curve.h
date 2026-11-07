/* SPDX-License-Identifier: MIT */
#ifndef PROTO_CURVE_H
#define PROTO_CURVE_H

#include <stdbool.h>

#define PROTO_WIDTH 240
#define PROTO_HEIGHT 160
#define PROTO_MAX_SECTIONS 16
#define PROTO_PROFILE_SAMPLES 2049
#define PROTO_EXPRESSION_LENGTH 160
#define PROTO_FIXED_TIMESTEP (1.0f / 60.0f)
#define PROTO_MAX_TRANSITIONS 8
#define PROTO_CROSS_COLS 672
#define PROTO_CROSS_MAX 64

typedef enum {
    PROTO_STRAIGHT,
    PROTO_SINE,
    PROTO_LIGHTNING,
    PROTO_FRACTAL,
    PROTO_FOURIER,
    PROTO_CHIRP,
    PROTO_TROCHOID,
    PROTO_HARMONIC_FRACTAL,
    PROTO_SPIRAL,
    PROTO_TRIANGLE,
    PROTO_SPATIAL_WAVE,
    PROTO_SPATIAL_LIGHTNING,
    PROTO_WOLFRAM_HALFTURN,
    PROTO_WOLFRAM_TENTACLE,
    PROTO_WOLFRAM_RIBBON,
    PROTO_WOLFRAM_DOUBLE_RIBBON,
    PROTO_PINCHED_ROPE,
    PROTO_FERMAT_SPIRAL,
    PROTO_FERMAT_TWIST,
    PROTO_METROID_LIGHTNING,
    PROTO_FERMAT_TALL,
    PROTO_METROID_EIGHT,
    PROTO_FERMAT_ROUND,
    PROTO_FERMAT_DIAGONAL,
    PROTO_FERMAT_HORIZONTAL,
    PROTO_METROID_HORIZONTAL,
    PROTO_CUSTOM,
    PROTO_FORMULA_COUNT
} ProtoFormula;

typedef enum {
    PROTO_EASE_LINEAR,
    PROTO_EASE_SMOOTHSTEP,
    PROTO_EASE_QUAD_IN,
    PROTO_EASE_QUAD_OUT,
    PROTO_EASE_QUAD_IN_OUT,
    PROTO_EASE_CUBIC_IN,
    PROTO_EASE_CUBIC_OUT,
    PROTO_EASE_CUBIC_IN_OUT,
    PROTO_EASE_COUNT
} ProtoEasing;

typedef struct {
    ProtoFormula formula;
    float duration;  /* Hold seconds before the next morph starts. */
    float morph_duration; /* Seconds to morph (zero means legacy 0.55 s). */
    ProtoEasing easing;
    char expression[PROTO_EXPRESSION_LENGTH]; /* For formula = custom in transitions */
} ProtoTransition;

typedef struct {
    ProtoFormula formula;
    float amplitude;     /* Pixels perpendicular to the original divider. */
    float cycles;        /* Repetitions within one section. */
    float phase_speed;   /* Cycles per second, signed. */
    float ribbon_width; /* Full tentacle width in logical pixels. */
    int spiral_layout; /* Patch 0032: 1=A, 2=B, 3=C, 4=D */
    float stagger_delay; /* Seconds between adjacent shapes. */
    float spiral_scale; /* 0=legacy 1.0, or 0.35..1.0 */
    float gap_pixels;    /* Flat gap between motifs, 0..5 logical px. */
    bool repeat_visible; /* Eight motifs span the visible half-line when selected. */
    char expression[PROTO_EXPRESSION_LENGTH]; /* For formula = custom. */
    bool animated;       /* Whether this section uses automatic transitions */
    bool loop;           /* Whether transitions repeat cyclically */
    int transition_count;
    ProtoTransition transitions[PROTO_MAX_TRANSITIONS];
} ProtoSection;

typedef struct {
    int section_count;       /* 1, 2, 4, 8 or 16. */
    float rotation_speed;    /* Clockwise degrees per second. */
    float line_width;        /* Logical pixels, 0..4. */
    bool show_divider;
    unsigned char violet[4]; /* RGBA. */
    unsigned char cyan[4];
    unsigned char divider_color[4];
    ProtoSection sections[PROTO_MAX_SECTIONS];
} ProtoConfig;

typedef struct {
    float time;
    /* -1 = inactive; age in seconds during a 0.24s transient pulse. */
    float aria_pulse_age;
    float metroid_pulse_age;
    /* Patch 0038: one accelerated curve at a time, with queued beats. */
    float aria_wave_age, metroid_wave_age;
    /* The actual length of the PCM event currently being replayed. */
    float aria_wave_duration, metroid_wave_duration;
    unsigned aria_wave_pending, metroid_wave_pending;
    /* Preserve each queued sound duration for X and Y independently. */
    float aria_wave_queue_durations[8], metroid_wave_queue_durations[8];
    float aria_pulse_history[7];
    float metroid_pulse_history[7];
    float rotation_degrees;
    bool paused;
    float accumulator;  /* Delta time accumulator for fixed timestep */
    ProtoSection previous[PROTO_MAX_SECTIONS];
    float morph[PROTO_MAX_SECTIONS];
    int current_transition[PROTO_MAX_SECTIONS];  /* Index in transitions array */
    float transition_time[PROTO_MAX_SECTIONS];   /* Time elapsed in current transition */
    ProtoFormula current_formula[PROTO_MAX_SECTIONS];  /* Current formula for each section */
    bool finished[PROTO_MAX_SECTIONS];
    ProtoEasing active_easing[PROTO_MAX_SECTIONS];
    float morph_seconds[PROTO_MAX_SECTIONS];
    char current_expression[PROTO_MAX_SECTIONS][PROTO_EXPRESSION_LENGTH];  /* For custom formulas in transitions */
} ProtoAnimation;

typedef struct {
    float cos_angle, sin_angle;
    float offsets[PROTO_PROFILE_SAMPLES];
    float x_coords[PROTO_PROFILE_SAMPLES];
    bool parametric;
    /* Filled cyan ribbon; rasterized per frame, never a crossing parity. */
    unsigned char ribbon_mask[PROTO_WIDTH * PROTO_HEIGHT];
    unsigned char cross_count[PROTO_CROSS_COLS];
    float cross_y[PROTO_CROSS_COLS][PROTO_CROSS_MAX];
} ProtoFrame;

bool proto_valid_section_count(int count);
const char *proto_formula_name(ProtoFormula formula);
bool proto_parse_formula(const char *name, ProtoFormula *out);
void proto_config_defaults(ProtoConfig *config);
void proto_animation_init(ProtoAnimation *animation, const ProtoConfig *config);
void proto_animation_tick(ProtoAnimation *animation, const ProtoConfig *config, float dt);
void proto_animation_change_formula(ProtoAnimation *animation, ProtoConfig *config,
                                    int index, ProtoFormula formula);
void proto_make_frame(const ProtoConfig *config, const ProtoAnimation *animation,
                      ProtoFrame *frame);
float proto_signed_side(const ProtoFrame *frame, float x, float y);
/* Negative: violet; positive: cyan. Independent of line width. */

#endif
