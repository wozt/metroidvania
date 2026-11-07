/* SPDX-License-Identifier: MIT */
#include "curve.h"
#include "expression.h"
#include "parametric.h"

#include <math.h>
#include <string.h>

#define PROTO_PI 3.14159265358979323846f
#define PROTO_HALF_SPAN 168.0f
#define PROTO_MORPH_SECONDS 0.55f
/* Double-armed Archimedean spiral separator. */
#define PROTO_SPLIT_SPAN (2.0f * PROTO_HALF_SPAN)

static float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
static float fractf_pos(float v) { return v - floorf(v); }
static float smoothstep(float v) { v = clamp01(v); return v * v * (3.0f - 2.0f * v); }
static float apply_easing(float t, ProtoEasing easing) {
    t = clamp01(t);
    switch (easing) {
        case PROTO_EASE_LINEAR: return t;
        case PROTO_EASE_SMOOTHSTEP: return t * t * (3.0f - 2.0f * t);
        case PROTO_EASE_QUAD_IN: return t * t;
        case PROTO_EASE_QUAD_OUT: return t * (2.0f - t);
        case PROTO_EASE_QUAD_IN_OUT: return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
        case PROTO_EASE_CUBIC_IN: return t * t * t;
        case PROTO_EASE_CUBIC_OUT: { float f = t - 1.0f; return f * f * f + 1.0f; }
        case PROTO_EASE_CUBIC_IN_OUT: return t < 0.5f ? 4.0f * t * t * t : (t - 1.0f) * (2.0f * t - 2.0f) * (2.0f * t - 2.0f) + 1.0f;
        default: return t;
    }
}

bool proto_valid_section_count(int count) {
    return count == 1 || count == 2 || count == 4 || count == 8 || count == 16;
}

const char *proto_formula_name(ProtoFormula formula) {
    static const char *names[] = {"straight", "sine", "lightning", "fractal", "fourier",
                                   "chirp", "trochoid", "harmonic_fractal", "spiral", "triangle", "spatial_wave", "spatial_lightning", "wolfram_halfturn", "wolfram_tentacle", "wolfram_ribbon", "wolfram_double_ribbon", "pinched_rope", "fermat_spiral", "fermat_twist", "metroid_lightning", "fermat_tall", "metroid_eight", "fermat_round", "fermat_diagonal", "fermat_horizontal", "metroid_horizontal", "custom"};
    return formula >= 0 && formula < PROTO_FORMULA_COUNT ? names[formula] : "unknown";
}

bool proto_parse_formula(const char *name, ProtoFormula *out) {
    if (!name || !out) return false;
    for (int k = PROTO_STRAIGHT; k < PROTO_CUSTOM; ++k) {
        if (!strcmp(name, proto_formula_name((ProtoFormula)k))) {
            *out = (ProtoFormula)k;
            return true;
        }
    }
    return false;
}

void proto_config_defaults(ProtoConfig *config) {
    memset(config, 0, sizeof *config);
    config->section_count = 4;
    config->rotation_speed = 10.0f;
    config->line_width = 1.0f;
    config->show_divider = true;
    memcpy(config->violet, (unsigned char[]){135, 55, 205, 255}, 4);
    memcpy(config->cyan, (unsigned char[]){25, 219, 232, 255}, 4);
    memcpy(config->divider_color, (unsigned char[]){248, 248, 253, 255}, 4);
    const ProtoFormula patterns[] = {PROTO_FOURIER, PROTO_CHIRP, PROTO_TROCHOID, PROTO_HARMONIC_FRACTAL,
                                     PROTO_LIGHTNING, PROTO_SINE, PROTO_FRACTAL, PROTO_STRAIGHT};
    for (int i = 0; i < PROTO_MAX_SECTIONS; ++i) {
        config->sections[i] = (ProtoSection){.formula = patterns[i % 8],
                                             .amplitude = 14.0f,
                                             .cycles = 3.0f,
                                             .phase_speed = 0.14f,
                                             .animated = false,
                                             .loop = false,
                                             .transition_count = 0};
    }
}

void proto_animation_init(ProtoAnimation *animation, const ProtoConfig *config) {
    memset(animation, 0, sizeof *animation);
    animation->aria_pulse_age = -1.0f;
    animation->metroid_pulse_age = -1.0f;
    animation->aria_wave_age = -1.0f;
    animation->metroid_wave_age = -1.0f;
    for (int j = 0; j < 7; ++j) {
        animation->aria_pulse_history[j] = -1.0f;
        animation->metroid_pulse_history[j] = -1.0f;
    }
    for (int i = 0; i < PROTO_MAX_SECTIONS; ++i) {
        animation->previous[i] = config->sections[i];
        animation->morph[i] = 1.0f;
        animation->morph_seconds[i] = PROTO_MORPH_SECONDS;
        animation->active_easing[i] = PROTO_EASE_SMOOTHSTEP;
        animation->current_transition[i] = 0;
        animation->transition_time[i] = 0.0f;
        animation->current_formula[i] = config->sections[i].formula;
        strncpy(animation->current_expression[i], config->sections[i].expression, PROTO_EXPRESSION_LENGTH);
        animation->current_expression[i][PROTO_EXPRESSION_LENGTH - 1] = '\0';
    }
}
void proto_animation_tick(ProtoAnimation *animation, const ProtoConfig *config, float dt) {
    if (animation->paused || !isfinite(dt) || dt <= 0.0f) return;
    if (dt > 0.1f) dt = 0.1f;
    animation->accumulator += dt;
    while (animation->accumulator >= PROTO_FIXED_TIMESTEP) {
        animation->time += PROTO_FIXED_TIMESTEP;
        animation->rotation_degrees = fmodf(animation->rotation_degrees +
                                            config->rotation_speed * PROTO_FIXED_TIMESTEP, 360.0f);
        for (int i = 0; i < PROTO_MAX_SECTIONS; ++i) {
            float seconds = animation->morph_seconds[i];
            if (!(seconds > 0.0f)) seconds = PROTO_MORPH_SECONDS;
            animation->morph[i] = clamp01(animation->morph[i] + PROTO_FIXED_TIMESTEP / seconds);
            const ProtoSection *s = &config->sections[i];
            if (!s->animated || s->transition_count == 0 || animation->finished[i] ||
                animation->morph[i] < 1.0f) continue;
            animation->transition_time[i] += PROTO_FIXED_TIMESTEP;
            const ProtoTransition *trans = &s->transitions[animation->current_transition[i]];
            if (animation->transition_time[i] + 1.0e-5f < trans->duration) continue;
            animation->transition_time[i] = 0.0f;
            bool changed = animation->current_formula[i] != trans->formula ||
                (trans->formula == PROTO_CUSTOM &&
                 strcmp(animation->current_expression[i], trans->expression) != 0);
            if (changed) {
                ProtoSection prior = *s;
                prior.formula = animation->current_formula[i];
                memcpy(prior.expression, animation->current_expression[i], sizeof prior.expression);
                animation->previous[i] = prior;
                animation->current_formula[i] = trans->formula;
                memcpy(animation->current_expression[i], trans->expression,
                       sizeof animation->current_expression[i]);
                animation->active_easing[i] = trans->easing;
                animation->morph_seconds[i] = trans->morph_duration > 0.0f ?
                    trans->morph_duration : PROTO_MORPH_SECONDS;
                animation->morph[i] = 0.0f;
            }
            ++animation->current_transition[i];
            if (animation->current_transition[i] >= s->transition_count) {
                if (s->loop) animation->current_transition[i] = 0;
                else {
                    animation->finished[i] = true;
                    animation->current_transition[i] = s->transition_count - 1;
                }
            }
        }
        animation->accumulator -= PROTO_FIXED_TIMESTEP;
    }
}

void proto_animation_change_formula(ProtoAnimation *animation, ProtoConfig *config,
                                    int index, ProtoFormula formula) {
    if (index < 0 || index >= PROTO_MAX_SECTIONS || formula < 0 ||
        formula >= PROTO_CUSTOM || config->sections[index].formula == formula) return;
    animation->previous[index] = config->sections[index];
    animation->morph[index] = 0.0f;
    config->sections[index].formula = formula;
}

static float formula_value(const ProtoSection *s, float u, float time) {
    if (s->formula == PROTO_STRAIGHT) return 0.0f;
    float edge = sinf(PROTO_PI * u);
    edge *= edge; /* Each segment returns to baseline at its endpoints. */
    float phase = time * s->phase_speed;
    float a = s->cycles * u;
    float v = 0.0f;
    switch (s->formula) {
    case PROTO_SINE:
        v = sinf(2.0f * PROTO_PI * (a + phase));
        break;
    case PROTO_LIGHTNING:
        v = 1.0f - 4.0f * fabsf(fractf_pos(a + phase + 0.25f) - 0.5f);
        /* Stationary integer-cycle lightning has uniform teeth and joins at zero. */
        if (s->phase_speed == 0.0f && fabsf(s->cycles - roundf(s->cycles)) < 1e-5f)
            return s->amplitude * v;
        break;
    case PROTO_FRACTAL: {
        float sum = 0.0f, weight = 1.0f, total = 0.0f, multiplier = 1.0f;
        for (int i = 0; i < 4; ++i) {
            sum += weight * sinf(2.0f * PROTO_PI *
                                  (a * multiplier + phase * (1.0f + 0.17f * i)));
            total += weight;
            weight *= 0.50f;
            multiplier *= 2.0f;
        }
        v = sum / total;
        break;
    }
    case PROTO_FOURIER: {
        /* Superposition of sine waves with harmonic frequencies */
        float sum = 0.0f;
        sum += 1.0f * sinf(2.0f * PROTO_PI * (a + phase));
        sum += 0.5f * sinf(2.0f * PROTO_PI * (2.0f * a + 1.5f * phase));
        sum += 0.25f * sinf(2.0f * PROTO_PI * (3.0f * a + 2.0f * phase));
        sum += 0.125f * sinf(2.0f * PROTO_PI * (4.0f * a + 2.5f * phase));
        v = sum / 1.875f; /* Normalize by sum of weights */
        break;
    }
    case PROTO_CHIRP: {
        /* Frequency varies linearly with position: f(u) = f0 + k*u */
        float f0 = 1.0f;
        float k = 2.0f;
        /* Integral of frequency over position gives phase */
        float chirp_phase = 2.0f * PROTO_PI * (f0 * u + 0.5f * k * u * u + phase);
        v = sinf(chirp_phase);
        break;
    }
    case PROTO_TROCHOID: {
        /* Cycloid-like curve: creates loops when amplitude > radius */
        float R = 1.0f; /* Rolling circle radius */
        float r = 0.5f; /* Point offset from center */
        float theta = 2.0f * PROTO_PI * (a + phase);
        /* Trochoid: x = R*theta - r*sin(theta), y = R - r*cos(theta) */
        /* We only use the y component for vertical displacement */
        v = (R - r * cosf(theta)) / (R + r); /* Normalize to [-1, 1] */
        break;
    }
    case PROTO_HARMONIC_FRACTAL: {
        /* More controlled harmonic series with finer detail */
        float sum = 0.0f, weight = 1.0f, total = 0.0f;
        for (int i = 0; i < 6; ++i) {
            float freq = (float)(i + 1);
            float phase_offset = phase * (1.0f + 0.2f * i);
            sum += weight * sinf(2.0f * PROTO_PI * (a * freq + phase_offset));
            total += weight;
            weight *= 0.6f; /* Slower decay for more detail */
        }
        v = sum / total;
        break;
    }
    case PROTO_TRIANGLE:
        v = 0.38f * (1.0f - 4.0f * fabsf(fractf_pos(a + 0.25f) - 0.5f));
        break;
    case PROTO_SPIRAL:
        return 0.0f; /* Handled by parametric x(u),y(u), not y=f(x). */
    case PROTO_CUSTOM: {
        bool ok = false;
        v = proto_expression_evaluate(s->expression, u, time, s->amplitude,
                                      s->cycles, s->phase_speed, &ok);
        if (!ok) v = 0.0f;
        break;
    }
    default: break;
    }
    if (!isfinite(v)) return 0.0f;
    /* Limit extremely large formulas to a predictable screen-space radius. */
    if (v < -1.0f) v = -1.0f;
    if (v > 1.0f) v = 1.0f;
    return s->amplitude * edge * v;
}

/* Periodic motifs are repeated along the visible half-line. A flat gap
 * occupies the requested number of logical pixels between successive cells.
 * Keeping motif endpoints on y=0 preserves the region boundary. */
static float triangle_unit(float p) {
    return 1.0f - 4.0f * fabsf(fractf_pos(p + 0.25f) - 0.5f);
}

/* Deterministic, angular three-scale lightning: large hooked stroke first,
 * two nested zigzags second/third.  Unlike a triangular wave it contains
 * several sharp, asymmetric changes of direction inside each motif. */
static float lightning_unit(float p) {
    const float at[] = {0.0f, 0.11f, 0.22f, 0.33f, 0.43f,
                        0.55f, 0.65f, 0.76f, 0.88f, 1.0f};
    const float h[] = {0.0f, -0.18f, 0.96f, 0.12f, 0.38f,
                       -1.00f, -0.10f, 0.42f, -0.22f, 0.0f};
    float large = 0.0f;
    for (int i = 0; i < 9; ++i) {
        if (p <= at[i + 1]) {
            float q = (p - at[i]) / (at[i + 1] - at[i]);
            large = h[i] + (h[i + 1] - h[i]) * q;
            break;
        }
    }
    return 0.86f * large + 0.10f * triangle_unit(3.0f * p) +
           0.04f * triangle_unit(9.0f * p);
}

/* Spatial gradient: one breathing state, four simultaneous complexities.
 * Unlike legacy transitions, the curve never switches its formula per step.
 * The entire half-line goes flat -> sine/triangle -> partial curl/zigzag ->
 * fully developed tentacle/lightning -> ... -> flat, continuously. */
/* Patch 0013: seam-safe outward growth on a single shared breathing clock.
 * All four portions coexist. Complexity AND size increase away from origin.
 * Distortion is always zero at the cell edges (both x and y), even mid-morph. */
static void spatial_gradient_point(const ProtoSection *s, float baseline,
                                   float u, float start, float span,
                                   float time, float *x, float *y) {
    *x = baseline;
    *y = 0.0f;
    if (u < 0.0f || u > 1.0f) return;
    int n = (int)lroundf(s->cycles);
    if (n < 1) n = 1;
    if (n > 16) n = 16;
    const float cell = span / n;
    const float gap = fminf(fmaxf(0.0f, s->gap_pixels), cell * 0.65f);
    int index = (int)(u * n);
    if (index >= n) index = n - 1;
    const float local = u * span - index * cell;
    if (local <= 0.5f * gap || local >= cell - 0.5f * gap) return;
    const float length = cell - gap;
    const float p = (local - 0.5f * gap) / length;
    const float cell_start = start + index * cell + 0.5f * gap;

    /* On the negative half, cell 0 is the outermost; on the positive half,
     * cell n-1 is outermost. Distance from the centre is signed correctly. */
    const float outward = n == 1 ? 1.0f :
        (start < 0.0f ? (float)(n - 1 - index) / (float)(n - 1) :
                        (float)index / (float)(n - 1));
    const float size_scale = 0.72f + 0.82f * outward; /* 1.54/0.72 ~= 2.14 */
    const float complexity = smoothstep(outward);
    const float phase = 2.0f * PROTO_PI * time * s->phase_speed;
    const float breath = 0.5f - 0.5f * cosf(phase);
    const float detail = smoothstep((breath - 0.10f) / 0.90f);
    const float edge = smoothstep(fminf(10.0f * p, 10.0f * (1.0f - p)));
    const float window = sinf(PROTO_PI * p) * sinf(PROTO_PI * p);

    if (s->formula == PROTO_SPATIAL_WAVE) {
        /* Centre: sine. Far edge: full single-arm pointed spiral.
         * The outer radius grows with distance, with no constant-width loop. */
        const float sine = 0.36f * s->amplitude * size_scale *
                           sinf(2.0f * PROTO_PI * p) * edge;
        float curl_x, curl_y;
        proto_tentacle_gradient_point(p, cell_start, length,
                                      s->amplitude * size_scale,
                                      &curl_x, &curl_y);
        const float mix = complexity * detail;
        /* A small extra push along the cell axis makes the outer spiral
         * develop farther. sin^2 makes this push exactly 0 at both joins. */
        const float push = 0.16f * s->amplitude * outward * size_scale * window;
        const float direction = start < 0.0f ? -1.0f : 1.0f;
        *x = baseline + breath * mix * (curl_x - baseline + direction * push);
        *y = breath * ((1.0f - mix) * sine + mix * curl_y);
    } else {
        /* Lightning also grows outward and develops progressively sharper
         * branches; the high-frequency detail never changes the endpoints. */
        const float triangle = 0.56f * (1.0f - fabsf(2.0f * p - 1.0f));
        const float jagged = lightning_unit(p);
        const float branch = 0.10f * triangle_unit(5.0f * p) +
                             0.035f * triangle_unit(11.0f * p);
        const float fast = smoothstep(fminf(1.0f, 1.50f * breath));
        const float mix = complexity * smoothstep((fast - 0.12f) / 0.88f);
        const float shape = (1.0f - mix) * triangle + mix * (jagged + branch);
        *y = s->amplitude * size_scale * fast * shape * edge;
    }
}

/* Short, root-preserving 0.24 s fast ripple: approximately two rapid
 * spins around the motif centre, greatest away from the straight joins. */
static void proto_fermat_impulse(float *x, float *y,
                                 float cell_start, float cell_span,
                                 float p, float age) {
    if (age < 0.0f || age >= 0.24f || p <= 0.20f || p >= 0.80f) return;
    const float f = age/0.24f;
    const float envelope = sinf(PROTO_PI*f);
    float q = p <= 0.50f ? (p-0.20f)/0.30f : (0.80f-p)/0.30f;
    q = smoothstep(q);
    const float angle = (4.0f*PROTO_PI*f*envelope)*q*q;
    const float scale = 1.0f + 0.18f*envelope*q*q;
    const float cx = cell_start+0.5f*cell_span;
    const float dx = (*x-cx)*scale, dy = (*y)*scale;
    *x = cx + dx*cosf(angle) - dy*sinf(angle);
    *y = dx*sinf(angle) + dy*cosf(angle);
}

/* Patch 0041: button waves follow each emitted PCM clip length.
 * One natural curve per cell, no second geometry layer. */
static float proto_visual_burst(const ProtoAnimation *a,bool aria,int index,
                                float *weight) {
    *weight=0.0f;
    if(!a) return -1.0f;
    const float duration=aria?a->aria_wave_duration:a->metroid_wave_duration;
    const float cell=proto_button_wave_cell_duration(duration);
    const float age=(aria?a->aria_wave_age:a->metroid_wave_age)-
                    proto_button_wave_delay(duration)*(float)index;
    const float phase=proto_fermat_audio_progress(age,duration);
    if(phase<0.0f) return -1.0f;
    /* Also scale the edge blend: no fixed 12ms when sound length changes. */
    const float edge=cell*0.08f;
    *weight=smoothstep(age/edge)*smoothstep((cell-age)/edge);
    return phase;
}

static void shape_point(const ProtoSection *s, float baseline, float u,
                        float start, float span, float t, float curl,
                        float pulse_age, const ProtoAnimation *pulse_animation,
                        float *x, float *y) {
    *x = baseline;
    *y = 0.0f;
    /* Patch 0032: permit diagonal curls to continue off-screen. */
    if ((u < 0.0f || u > 1.0f) && s->formula != PROTO_FERMAT_DIAGONAL) return;
    if (s->formula == PROTO_WOLFRAM_RIBBON ||
        s->formula == PROTO_WOLFRAM_DOUBLE_RIBBON ||
        s->formula == PROTO_PINCHED_ROPE) return; /* Straight parent divider; tube is a filled mask. */
    if (s->formula == PROTO_METROID_HORIZONTAL) {
        /* Single triangle, re-timed for an active button wave. Do not layer
         * an extra electric line on top of the natural Metroid animation. */
        const float distance=-baseline;
        if(distance<0.0f||distance>span) return;
        const float outer=span*(0.30f/1.30f)/0.80f;
        const float inner=span-outer;
        const float step=inner/7.0f;
        const int cell=distance<inner ?
            (int)fminf(6.0f,floorf(distance/step)):7;
        const float local=cell==7 ?
            (distance-inner)/outer:(distance-step*(float)cell)/step;
        const float p=fminf(1.0f,fmaxf(0.0f,local));
        const float scale=0.25f*exp2f(2.0f*(float)cell/7.0f);
        const float normal=proto_fermat_offset_progress(t,s->phase_speed,
                                                         cell,s->stagger_delay);
        float mix=0.0f;
        const float beat=proto_visual_burst(pulse_animation,false,cell,&mix);
        const float growth=beat>=0.0f ? normal+(beat-normal)*mix : normal;
        *y=s->amplitude*scale*growth*proto_metroid_bipolar_value(cell,p);
        return;
    }
    if (s->formula == PROTO_METROID_EIGHT) {
        /* Eight actual contiguous triangular periods of the given plot,
         * ordered from the line centre to the exterior of the Metroid half. */
        const int nbolts = 8;
        const float position = fminf(8.0f, fmaxf(0.0f, u*8.0f));
        int cell = (int)position;
        if (cell >= nbolts) cell = nbolts - 1;
        const float local = position - (float)cell;
        const int outward_index = 7-cell;
        const float tlocal = 1.0f - local;
        const float left_x = 1.0f + (float)outward_index;
        const float signal_x = left_x+tlocal;
        const float raw = proto_metroid_lightning_value(signal_x);
        const float y0 = proto_metroid_lightning_value(left_x);
        const float y1 = proto_metroid_lightning_value(left_x+1.0f);
        /* Remove the secant so individual sections meet at y=0. */
        const float signal = raw - (y0+(y1-y0)*tlocal);
        const float join = smoothstep(tlocal/0.14f) *
                           smoothstep((1.0f-tlocal)/0.14f);
        const float scale = 0.25f*exp2f(2.0f*(float)outward_index/7.0f);
        const float growth = s->stagger_delay > 0.0f ?
            proto_fermat_offset_progress(t,s->phase_speed,outward_index,
                                          s->stagger_delay) :
            proto_fermat_bothways_progress(t,s->phase_speed,outward_index,0.2f);
        const float pulse = pulse_age >= 0.0f && pulse_age < 0.24f ?
                            sinf(PROTO_PI*pulse_age/0.24f) : 0.0f;
        /* The quick sine component is already present in the supplied plot. */
        *y = s->amplitude * scale * growth * join *
             (signal * (1.0f+1.6f*pulse) +
              0.11f*pulse*sinf(50.0f*signal_x+8.0f*PROTO_PI*pulse_age/0.24f));
        return;
    }
    if (s->formula == PROTO_FERMAT_HORIZONTAL) {
        /* An input wave REPLAYS the SAME circular Fermat outline at a faster
         * phase. No sum of two shapes, which previously caused self-overlap. */
        if(s->cycles<7.5f||s->cycles>8.5f) return;
        const float delay=s->stagger_delay>0.0f ? s->stagger_delay : 0.4f;
        for(int i=0;i<8;++i) {
            float begin,length;
            proto_fermat_horizontal_cell(start,span,s->spiral_layout,i,
                                          &begin,&length);
            if(baseline<begin-0.0002f||baseline>begin+length+0.0002f)
                continue;
            const float p=fminf(1.0f,fmaxf(0.0f,(baseline-begin)/length));
            const float normal=proto_fermat_offset_progress(t,s->phase_speed,i,delay);
            float mix=0.0f;
            const float beat=proto_visual_burst(pulse_animation,true,i,&mix);
            const float growth=beat>=0.0f ? normal+(beat-normal)*mix : normal;
            /* Patch 0046: the SAME wave clock as the Metroid side; correct
             * the Fermat winding-angle exponent only while X/Y is active.
             * No extra spiral is drawn; ambient movement is unchanged. */
            const float visual_growth=beat>=0.0f ?
                growth+mix*(proto_fermat_visual_sync_progress(growth)-growth) :
                growth;
            const float shifted=t-delay*(float)i;
            const float direction=proto_fermat_twist_direction(shifted,
                s->phase_speed)*((i&1)?-1.0f:1.0f);
            const float winding=0.25f+0.75f*(float)i/7.0f;
            proto_fermat_horizontal_point(p,begin,length,i,s->amplitude,
                                          visual_growth,winding,direction,x,y);
            return;
        }
        return;
    }
    if (s->formula == PROTO_FERMAT_DIAGONAL) {
        /* Fixed diagonal, never per-angle visible extent, with 4 candidate
         * root distributions and no oval/anisotropic transformations. */
        if (s->cycles < 7.5f || s->cycles > 8.5f) return;
        const float delay = s->stagger_delay > 0.0f ? s->stagger_delay : .4f;
        for (int i = 0; i < 8; ++i) {
            float begin, length;
            proto_fermat_diagonal_cell(start,span,s->spiral_layout,i,
                                       s->amplitude,&begin,&length);
            if (baseline < begin-0.0002f || baseline > begin+length+0.0002f)
                continue;
            const float p = fminf(1.0f,fmaxf(0.0f,(baseline-begin)/length));
            const float growth=proto_fermat_offset_progress(t,s->phase_speed,i,delay);
            const float shifted_time=t-delay*(float)i;
            const float direction=proto_fermat_twist_direction(shifted_time,
                s->phase_speed)*((i&1)?-1.0f:1.0f);
            const float winding=.25f+.75f*(float)i/7.0f;
            proto_fermat_diagonal_point(p,begin,length,growth,winding,direction,x,y);
            if (pulse_age>=0.0f && pulse_age<0.24f)
                proto_fermat_impulse(x,y,begin,length,p,pulse_age);
            return;
        }
        return;
    }
    if (s->formula == PROTO_FERMAT_ROUND) {
        /* Patch 0031: eight genuinely round coils with extreme size steps.
         * Independent winding 25%..100%, same delayed five-second clock. */
        if (s->cycles < 7.5f || s->cycles > 8.5f) return;
        for (int i = 0; i < 8; ++i) {
            float packed_start, packed_span;
            proto_fermat_packed_cell(start, span, 8, i, &packed_start, &packed_span);
            if (baseline < packed_start - 0.0002f ||
                baseline > packed_start + packed_span + 0.0002f) continue;
            const float p = fminf(1.0f, fmaxf(0.0f,
                                         (baseline-packed_start)/packed_span));
            const float growth = proto_fermat_bothways_progress(t, s->phase_speed,
                                                                 i, 0.2f);
            const float direction = proto_fermat_twist_direction(t, s->phase_speed) *
                                    ((i & 1) ? -1.0f : 1.0f);
            proto_fermat_round_point(p, packed_start, packed_span,
                                     start, span, i, s->amplitude,
                                     growth, direction, x, y);
            /* Retain the momentary X-key ripple. The longer animation
             * keeps its exact isotropic round geometry outside pulses. */
            if (pulse_age >= 0.0f && pulse_age < 0.24f)
                proto_fermat_impulse(x, y, packed_start, packed_span, p, pulse_age);
            return;
        }
        return;
    }
    if (s->formula == PROTO_FERMAT_TALL) {
        /* Packed x-centres, but exponentially growing vertical extent.
         * This deliberately decouples screen height from packed cell width. */
        if (s->cycles < 7.5f || s->cycles > 8.5f) return;
        for (int i = 0; i < 8; ++i) {
            float packed_start, packed_span;
            proto_fermat_packed_cell(start, span, 8, i, &packed_start, &packed_span);
            if (baseline < packed_start - 0.0002f ||
                baseline > packed_start+packed_span+0.0002f) continue;
            const float p = fminf(1.0f, fmaxf(0.0f,
                                          (baseline-packed_start)/packed_span));
            const float radius = 0.5f*packed_span;
            const float winding = 0.25f+0.75f*(float)i/7.0f;
            const float growth = proto_fermat_bothways_progress(t, s->phase_speed,
                                                                 i, 0.2f);
            const float direction = proto_fermat_twist_direction(t, s->phase_speed) *
                                    ((i & 1) ? -1.0f : 1.0f);
            proto_fermat_twist_point(p, packed_start, packed_span,
                                    radius, growth, winding, direction, x, y);
            proto_fermat_impulse(x, y, packed_start, packed_span, p, pulse_age);
            const float target_half_height = 24.0f*exp2f((float)i/7.0f) *
                                             fminf(2.0f,fmaxf(0.0f,s->amplitude/15.0f));
            const float reference = proto_fermat_tall_reference_peak(i);
            *y *= target_half_height/(radius*reference);
            return;
        }
        return;
    }
    if (s->formula == PROTO_METROID_LIGHTNING) {
        /* Four Metroid teeth along the left visible half-line. The curve
         * is the requested 0..9 expression, NOT a generic noise effect. */
        int bolts = (int)lroundf(s->cycles);
        if (bolts < 1) bolts = 1;
        if (bolts > 16) bolts = 16;
        const float position = fminf((float)bolts, fmaxf(0.0f, u*bolts));
        int bolt = (int)position;
        if (bolt >= bolts) bolt = bolts-1;
        const float local = position - bolt;
        const float fade = smoothstep(local/0.12f) *
                           smoothstep((1.0f-local)/0.12f);
        /* Left half: delay propagates outward from the central divider. */
        const int from_centre = bolts-1-bolt;
        const float growth = proto_fermat_stagger_progress(t, s->phase_speed,
                                                            from_centre, 0.13f);
        const float pulse_envelope = pulse_age >= 0.0f && pulse_age < 0.24f ?
            sinf(PROTO_PI*pulse_age/0.24f) : 0.0f;
        *y = s->amplitude * growth * fade *
             (proto_metroid_lightning_value(9.0f*local) *
             (1.0f + 1.6f*pulse_envelope) +
             0.25f*pulse_envelope*sinf(50.0f*9.0f*local +
             8.0f*PROTO_PI*pulse_age/0.24f));
        return;
    }
    if (s->formula == PROTO_SPATIAL_WAVE ||
        s->formula == PROTO_SPATIAL_LIGHTNING) {
        spatial_gradient_point(s, baseline, u, start, span, t, x, y);
        return;
    }
    bool repeated = s->formula == PROTO_FERMAT_TWIST ||
                    s->formula == PROTO_FERMAT_SPIRAL ||
                    s->formula == PROTO_SPIRAL ||
                    s->formula == PROTO_WOLFRAM_HALFTURN ||
                    s->formula == PROTO_WOLFRAM_TENTACLE ||
        ((s->repeat_visible || s->gap_pixels > 0.0f) &&
         (s->formula == PROTO_SINE || s->formula == PROTO_LIGHTNING ||
          s->formula == PROTO_TRIANGLE));
    if (!repeated) {
        *y = formula_value(s, u, t);
        return;
    }
    int n = (int)lroundf(s->cycles);
    if (n < 1) n = 1;
    if (n > 16) n = 16;
    /* Patch 0027: pack eight Fermat twists according to their peak diameters. */
    if (s->formula == PROTO_FERMAT_TWIST && n == 8) {
        /* Distinct cell widths: 50/57.14/.../100% of the outermost.
         * The max of each spiral touches its neighbours at the roots. */
        const float position = fminf(span, fmaxf(0.0f, u * span));
        for (int packed_index = 0; packed_index < n; ++packed_index) {
            float packed_start, packed_span;
            proto_fermat_packed_cell(start, span, n, packed_index,
                                     &packed_start, &packed_span);
            if (position > packed_start + packed_span - start &&
                packed_index != n - 1) continue;
            const float p = (start + position - packed_start) / packed_span;
            const float outward = (float)packed_index / (float)(n - 1);
            const float winding = 0.25f + 0.75f * outward;
            const float radius = fminf(fmaxf(s->amplitude, 0.0f),
                                       0.50f * packed_span);
            const float progress = proto_fermat_stagger_progress(t, s->phase_speed, packed_index, 0.075f);
            const float direction = proto_fermat_twist_direction(t, s->phase_speed) *
                                    ((packed_index & 1) ? -1.0f : 1.0f);
            proto_fermat_twist_point(p, packed_start, packed_span,
                                    radius, progress, winding, direction, x, y);
            proto_fermat_impulse(x, y, packed_start, packed_span, p, pulse_age);
            return;
        }
        return;
    }
    float cell = span / n;
    float gap = fminf(s->gap_pixels, cell * 0.65f);
    float scaled = u * n;
    int index = (int)scaled;
    if (index >= n) index = n - 1;
    float local = u * span - index * cell;
    float start_local = gap * 0.5f;
    float length = cell - gap;
    if (local <= start_local || local >= start_local + length) return;
    float p = (local - start_local) / length;
    float cell_start = start + index * cell + start_local;
    if (s->formula == PROTO_WOLFRAM_TENTACLE) {
        /* Animate the actual Wolfram parameter bound, not the shape scale. */
        const float limit = s->phase_speed == 0.0f ? 0.5f :
            0.25f * (1.0f - cosf(2.0f * PROTO_PI * s->phase_speed * t));
        proto_wolfram_tentacle_point(p, cell_start, length, s->amplitude,
                                     limit, 0.18f * s->amplitude, x, y);
        return;
    }
    if (s->formula == PROTO_FERMAT_TWIST) {
        /* Patch 0025: six outward-growing coils, separate size and winding. */
        /* Interpolate to 100% from centre to outside, independently of
         * the common fast 5-second motion. n is the local motif count. */
        const float outward = n > 1 ? (float)index / (float)(n - 1) : 1.0f;
        const float size_grade = 0.50f + 0.50f * outward;
        const float winding = 0.25f + 0.75f * outward;
        /* Clamp the full-size radius FIRST; otherwise six short cells
         * saturate to the same outer radius and lose the visual gradient. */
        const float max_radius = fminf(fmaxf(s->amplitude, 0.0f), 0.49f * length);
        const float progress = proto_fermat_twist_progress(t, s->phase_speed);
        const float direction = proto_fermat_twist_direction(t, s->phase_speed) *
                                ((index & 1) ? -1.0f : 1.0f);
        proto_fermat_twist_point(p, cell_start, length, max_radius * size_grade,
                                progress, winding, direction, x, y);
        return;
    }
    if (s->formula == PROTO_FERMAT_SPIRAL) {
        /* Every coil uses the SAME parabolic-speed 18-second clock;
         * only amplitude grows 25/50/75/100% from centre to outside. */
        const float grade = (float)(index + 1) / (float)n;
        const float progress = proto_fermat_parabolic_progress(t, s->phase_speed);
        proto_fermat_spiral_point(p, cell_start, length,
                                  s->amplitude * grade, progress, x, y);
        return;
    }
    if (s->formula == PROTO_WOLFRAM_HALFTURN) {
        /* phase_speed=0: exact static arc. Otherwise breathe in/out. */
        const float strength = s->phase_speed == 0.0f ? 1.0f :
            0.5f - 0.5f * cosf(2.0f * PROTO_PI * s->phase_speed * t);
        proto_wolfram_halfturn_point(p, cell_start, length,
                                     s->amplitude, strength, x, y);
        return;
    }
    if (s->formula == PROTO_SPIRAL) {
        /* Contract both spiral dimensions inside the original motif. */
        const float scale = s->spiral_scale > 0.0f ?
            fminf(1.0f, fmaxf(0.35f, s->spiral_scale)) : 1.0f;
        const float compact_span = length * scale;
        const float compact_start = cell_start + 0.5f * (length - compact_span);
        if (baseline <= compact_start || baseline >= compact_start + compact_span)
            return; /* remaining gap is exactly straight */
        const float compact_u = (baseline - compact_start) / compact_span;
        proto_spiral_point(compact_u, compact_start, compact_span,
                           s->amplitude * scale, curl, x, y);
        return;
    }
    float wave;
    switch (s->formula) {
    case PROTO_LIGHTNING:
        wave = lightning_unit(p);
        break;
    case PROTO_TRIANGLE:
        /* One compact, single-sided triangular tooth per cell. */
        wave = 0.42f * (1.0f - fabsf(2.0f * p - 1.0f));
        break;

    case PROTO_SINE:
        wave = sinf(2.0f * PROTO_PI * (p + s->phase_speed * t));
        break;
    default:
        return;
    }
    /* Fade only within the last tenth of a motif: exact flat junctions. */
    float edge = smoothstep(fminf(p * 10.0f, (1.0f - p) * 10.0f));
    *y = s->amplitude * wave * edge;
}
void proto_make_frame(const ProtoConfig *config, const ProtoAnimation *animation,
                      ProtoFrame *frame) {
    const float radians = animation->rotation_degrees * (PROTO_PI / 180.0f);
    frame->cos_angle = cosf(radians);
    frame->sin_angle = sinf(radians);
    frame->parametric = false;
    const int count = config->section_count;
    /* Projected half-extent of the rotated 240x160 rectangle. With
     * repeat_visible enabled, eight motifs are spread over the actual
     * on-screen half-line rather than over its invisible off-screen tail. */
    const float visible = PROTO_WIDTH * 0.5f * fabsf(frame->cos_angle) +
                          PROTO_HEIGHT * 0.5f * fabsf(frame->sin_angle);
    for (int i = 0; i < PROTO_PROFILE_SAMPLES; ++i) {
        float along = (float)i / (PROTO_PROFILE_SAMPLES - 1) * count;
        int index = (int)along;
        if (index >= count) index = count - 1;
        const float local_u = along - index;
        const float section_span = PROTO_SPLIT_SPAN / count;
        const float section_start = -PROTO_HALF_SPAN + index * section_span;
        const float base = section_start + local_u * section_span;
        const ProtoSection *section = &config->sections[index];
        float start = section_start, span = section_span;
        if (count == 2 && section->repeat_visible) {
            /* Screen-diagonal coordinates do not slide as the line rotates.
             * Keep legacy angle-aware profiles unchanged. */
            const float fixed_half_diagonal =
                0.5f*hypotf((float)PROTO_WIDTH,(float)PROTO_HEIGHT);
            const float extent = section->formula == PROTO_FERMAT_HORIZONTAL ||
                                 section->formula == PROTO_METROID_HORIZONTAL ?
                                 0.65f * (float)PROTO_WIDTH :
                                 section->formula == PROTO_FERMAT_DIAGONAL ||
                                 section->formula == PROTO_METROID_EIGHT ?
                                 fixed_half_diagonal : visible;
            start = index == 0 ? -extent : 0.0f;
            span = extent;
        }
        const float u = (base - start) / span;
        ProtoSection next = *section;
        if (section->animated) {
            next.formula = animation->current_formula[index];
            if (next.formula == PROTO_CUSTOM)
                memcpy(next.expression, animation->current_expression[index],
                       sizeof next.expression);
        }
        const ProtoSection *previous = &animation->previous[index];
        float blend = smoothstep(animation->morph[index]);
        if (section->animated && section->transition_count > 0)
            blend = apply_easing(animation->morph[index], animation->active_easing[index]);
        float xn, yn, xp, yp;
        shape_point(&next, base, u, start, span, animation->time,
                    next.formula == PROTO_SPIRAL ? blend : 1.0f,
                    next.formula == PROTO_FERMAT_TWIST ? animation->aria_pulse_age :
                    next.formula == PROTO_FERMAT_TALL ? animation->aria_pulse_age :
                    next.formula == PROTO_FERMAT_ROUND ? animation->aria_pulse_age :
                    next.formula == PROTO_METROID_LIGHTNING ? animation->metroid_pulse_age :
                    next.formula == PROTO_METROID_EIGHT ? animation->metroid_pulse_age : -1.0f,
                    animation, &xn, &yn);
        shape_point(previous, base, u, start, span, animation->time,
                    previous->formula == PROTO_SPIRAL ? 1.0f - blend : 1.0f,
                    previous->formula == PROTO_FERMAT_TWIST ? animation->aria_pulse_age :
                    previous->formula == PROTO_FERMAT_TALL ? animation->aria_pulse_age :
                    previous->formula == PROTO_FERMAT_ROUND ? animation->aria_pulse_age :
                    previous->formula == PROTO_METROID_LIGHTNING ? animation->metroid_pulse_age :
                    previous->formula == PROTO_METROID_EIGHT ? animation->metroid_pulse_age : -1.0f,
                    animation, &xp, &yp);
        /* Spiral curl is already driven by morph. Applying the blend a
         * second time would square its radius and hide the actual coil. */
        if (next.formula == PROTO_SPIRAL && previous->formula != PROTO_SPIRAL) {
            frame->x_coords[i] = xn + (xp - base) * (1.0f - blend);
            frame->offsets[i] = yn + yp * (1.0f - blend);
        } else if (previous->formula == PROTO_SPIRAL && next.formula != PROTO_SPIRAL) {
            frame->x_coords[i] = xp + (xn - base) * blend;
            frame->offsets[i] = yp + yn * blend;
        } else {
            frame->x_coords[i] = xp + (xn - xp) * blend;
            frame->offsets[i] = yp + (yn - yp) * blend;
        }

        if ((next.formula == PROTO_SPIRAL && blend > 0.0f) ||
            (previous->formula == PROTO_SPIRAL && blend < 1.0f))
            frame->parametric = true;
    }
    /* Curled spatial motifs can double back along x; always use the
     * parametric crossing classifier for the wave half-line. */
    for (int side = 0; side < config->section_count; ++side)
        if (config->sections[side].formula == PROTO_SPATIAL_WAVE ||
            config->sections[side].formula == PROTO_WOLFRAM_HALFTURN ||
            config->sections[side].formula == PROTO_WOLFRAM_TENTACLE)
            frame->parametric = true;
    for (int side = 0; side < config->section_count; ++side)
        if (config->sections[side].formula == PROTO_FERMAT_SPIRAL)
            frame->parametric = true;
    for (int side = 0; side < config->section_count; ++side)
        if (config->sections[side].formula == PROTO_FERMAT_TWIST)
            frame->parametric = true;
    for (int side = 0; side < config->section_count; ++side)
        if (config->sections[side].formula == PROTO_FERMAT_TALL)
            frame->parametric = true;
    for (int side = 0; side < config->section_count; ++side)
        if (config->sections[side].formula == PROTO_FERMAT_ROUND)
            frame->parametric = true;
    for (int side = 0; side < config->section_count; ++side)
        if (config->sections[side].formula == PROTO_FERMAT_DIAGONAL)
            frame->parametric = true;
    for (int side = 0; side < config->section_count; ++side)
        if (config->sections[side].formula == PROTO_FERMAT_HORIZONTAL)
            frame->parametric = true;
    if (frame->parametric) proto_parametric_prepare(frame);
    proto_wolfram_ribbon_prepare(frame, config, animation);
}

float proto_signed_side(const ProtoFrame *frame, float x, float y) {
    const float dx = x - PROTO_WIDTH * 0.5f;
    const float dy = y - PROTO_HEIGHT * 0.5f;
    const float tangent = dx * frame->cos_angle + dy * frame->sin_angle;
    const float normal = -dx * frame->sin_angle + dy * frame->cos_angle;
    /* A filled cyan ribbon overrides only the colour INSIDE its closed
     * contour. Pixels near its edge return distance zero so the
     * existing divider colour also outlines the spiral. */
    const int ix = (int)floorf(x), iy = (int)floorf(y);
    if (ix >= 0 && ix < PROTO_WIDTH && iy >= 0 && iy < PROTO_HEIGHT) {
        const int idx = iy * PROTO_WIDTH + ix;
        if (frame->ribbon_mask[idx] == 2) return 3.0f; /* cyan body */
        if (frame->ribbon_mask[idx] == 3) return -3.0f; /* violet body */
        if (frame->ribbon_mask[idx]) {
            bool edge = ix == 0 || ix == PROTO_WIDTH-1 ||
                        iy == 0 || iy == PROTO_HEIGHT-1;
            if (!edge) {
                edge = !frame->ribbon_mask[idx-1] ||
                       !frame->ribbon_mask[idx+1] ||
                       !frame->ribbon_mask[idx-PROTO_WIDTH] ||
                       !frame->ribbon_mask[idx+PROTO_WIDTH];
            }
            return edge ? 0.0f : 3.0f; /* white outline / cyan body */
        }
    }
    if (frame->parametric) return proto_parametric_side(frame, tangent, normal);
    float position = clamp01((tangent + PROTO_HALF_SPAN) / (2.0f * PROTO_HALF_SPAN));
    float sample = position * (PROTO_PROFILE_SAMPLES - 1);
    int index = (int)sample;
    if (index >= PROTO_PROFILE_SAMPLES - 1) index = PROTO_PROFILE_SAMPLES - 2;
    float blend = sample - index;
    float offset = frame->offsets[index] * (1.0f - blend) +
                   frame->offsets[index + 1] * blend;
    return normal - offset;
}
