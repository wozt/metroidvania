/* SPDX-License-Identifier: MIT */
#ifndef PROTO_PARAMETRIC_H
#define PROTO_PARAMETRIC_H
#include "curve.h"
void proto_spiral_point(float u, float section_start, float section_span,
                        float amplitude, float curl, float *x, float *y);
/* Thin centred curl used by the spatial-gradient mode. */
void proto_tentacle_gradient_point(float u, float start, float span,
                                  float amplitude, float *x, float *y);
/* Wolfram Alpha arc on t=0..0.5 with straight connectors. */
void proto_wolfram_halfturn_point(float p, float cell_start,
                                  float cell_span, float radius,
                                  float strength, float *x, float *y);
/* Two strands on the same Wolfram spiral, joined at the animated tip. */
void proto_wolfram_tentacle_point(float p, float cell_start, float cell_span,
                                  float amplitude, float t_limit,
                                  float strand_offset, float *x, float *y);
/* Rasterize closed, tapered Wolfram ribbons into the colour mask. */
void proto_wolfram_ribbon_prepare(ProtoFrame *frame,
                                   const ProtoConfig *config,
                                   const ProtoAnimation *animation);
/* Two-arm Fermat spiral, one continuous divider. */
void proto_fermat_spiral_point(float p, float cell_start,
                               float cell_span, float amplitude,
                               float progress, float *x, float *y);
/* Piecewise cubic position; parabolic velocity per half-cycle. */
float proto_fermat_parabolic_progress(float time, float phase_speed);
/* Faster 5s two-arm Fermat with independent winding depth. */
float proto_fermat_twist_progress(float time, float phase_speed);
float proto_fermat_twist_direction(float time, float phase_speed);
void proto_fermat_twist_point(float p, float cell_start,
                              float cell_span, float amplitude,
                              float progress, float winding,
                              float direction, float *x, float *y);
/* Peak-radius-weighted positions for eight Fermat twists. */
void proto_fermat_packed_cell(float start, float span, int count, int index,
                              float *cell_start, float *cell_span);
float proto_metroid_lightning_value(float x);
/* Centered, +/-peak-balanced eight-tooth Metroid waveform. */
float proto_metroid_bipolar_value(int tooth, float p);
float proto_fermat_stagger_progress(float time, float speed,
                                    int index, float delay);
float proto_fermat_bothways_progress(float time, float speed,
                                    int from_centre, float delay);
float proto_fermat_tall_reference_peak(int from_centre);
float proto_fermat_round_grade(int index);
float proto_fermat_round_radius(int index, float amplitude);
float proto_fermat_round_center(float start, float span, int index,
                                float amplitude);
void proto_fermat_round_point(float p, float packed_start, float packed_span,
                              float section_start, float section_span,
                              int index, float amplitude, float progress,
                              float direction, float *x, float *y);
void proto_fermat_diagonal_cell(float start,float span,int layout,int index,
                                 float amplitude,float *cell_start,float *cell_span);
void proto_fermat_diagonal_point(float p,float cell_start,float cell_span,
                                  float progress,float winding,float direction,
                                  float *x,float *y);
float proto_fermat_offset_progress(float time,float speed,int index,float delay);
void proto_fermat_horizontal_cell(float start, float span, int layout,
                                   int index, float *cell_start, float *cell_span);
void proto_fermat_horizontal_point(float p, float cell_start, float cell_span,
                                    int index, float amplitude, float growth,
                                    float winding, float direction, float *x, float *y);
float proto_fermat_burst_progress(float age);
float proto_button_wave_delay(float clip_duration);
float proto_button_wave_cell_duration(float clip_duration);
float proto_fermat_audio_progress(float age, float clip_duration);
float proto_fermat_visual_sync_progress(float progress);
void proto_parametric_prepare(ProtoFrame *frame);
float proto_parametric_side(const ProtoFrame *frame, float tangent, float normal);
#endif
