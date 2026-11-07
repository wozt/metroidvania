/* SPDX-License-Identifier: MIT */
/* An open double-armed Archimedean spiral: mathematically parametric. */
#include "parametric.h"
#include <math.h>
#include <string.h>

#define PI 3.14159265358979323846f
#define RANGE 168.0f
#define X_STEP (0.5f)

/* Stylized curling wave crest: a broad rising body flows into ONE
 * progressively narrowing tip. The final connector lies near the baseline,
 * avoiding the old symmetric "C" silhouette. The spiral radius tends to zero
 * before the connector, rather than creating another full-size arm.
 * A fraction of the shape is interpolated against the straight baseline when
 * curl is in [0, 1], keeping all intermediate stages continuous. */
void proto_spiral_point(float u, float cell_start, float cell_span,
                        float amplitude, float curl, float *x, float *y) {
    curl = fmaxf(0.0f, fminf(1.0f, curl));
    u = fmaxf(0.0f, fminf(1.0f, u));

    const float lead_end = 0.20f;
    const float coil_end = 0.86f;
    const float straight_x = u * cell_span;
    const float center_x = 0.50f * cell_span;
    const float center_y = 0.0f;
    const float outer_radius = fminf(0.60f * amplitude, 0.15f * cell_span);
    const float tail_start_x = center_x; /* Tangent-continuous join at curl centre. */
    float shape_x = straight_x;
    float shape_y = 0.0f;

    if (u <= lead_end) {
        const float v = u / lead_end;
        const float entry_x = center_x - outer_radius;
        shape_x = entry_x * v;
        shape_y = 0.0f;
    } else if (u < coil_end) {
        const float v = (u - lead_end) / (coil_end - lead_end);
        const float eased = v * v * (3.0f - 2.0f * v);
        const float turns = 1.65f;
        const float theta = PI + 2.0f * PI * turns * eased;
        const float radius = outer_radius * powf(1.0f - eased, 2.65f);
        shape_x = center_x + radius * cosf(theta);
        shape_y = center_y + radius * sinf(theta);
    } else {
        const float v = (u - coil_end) / (1.0f - coil_end);
        shape_x = tail_start_x + (cell_span - tail_start_x) * v;
        shape_y = 0.0f;
    }

    *x = cell_start + straight_x + curl * (shape_x - straight_x);
    *y = curl * shape_y;
}

/* For the entire rotated polyline, intersect a vertical ray with the open
 * separator in its local frame. Scan conversion happens once per frame.
 * The even/odd rule handles the folded spiral without flood-fill or art. */
/* Single-armed tentacle curl, joined to the separator at both endpoints.
 * The base of the curl is relatively round, its inner tip reaches radius 0,
 * and its centre remains on the horizontal line. This is NOT the old wave crest. */
void proto_tentacle_gradient_point(float u, float start, float span,
                                  float amplitude, float *x, float *y) {
    u = fmaxf(0.0f, fminf(1.0f, u));
    const float lead = 0.23f, finish = 0.83f;
    const float centre = 0.51f * span;
    const float radius0 = fminf(0.85f * amplitude, 0.335f * span);
    float px, py;
    if (u <= lead) {
        px = (centre - radius0) * (u / lead);
        py = 0.0f;
    } else if (u < finish) {
        float v = (u - lead) / (finish - lead);
        float theta = PI + 3.55f * PI * v;
        float radius = radius0 * powf(1.0f - v, 1.82f);
        px = centre + radius * cosf(theta);
        py = radius * sinf(theta);
    } else {
        float v = (u - finish) / (1.0f - finish);
        px = centre + (span - centre) * v;
        py = 0.0f;
    }
    *x = start + px;
    *y = py;
}

/* Wolfram Alpha: {(1-t)^2 cos(6*pi*t), (1-t)^2 sin(6*pi*t)},
 * t in [0,0.5].  The inner arc is EXACTLY this function, only translated
 * and uniformly scaled to fit a cell; the two baseline connectors are separate.
 * strength=0 yields a straight divider; strength=1 yields the full curve. */
void proto_wolfram_halfturn_point(float p, float cell_start,
                                  float cell_span, float amplitude,
                                  float strength, float *x, float *y) {
    p = fmaxf(0.0f, fminf(1.0f, p));
    strength = fmaxf(0.0f, fminf(1.0f, strength));
    const float lead = 0.12f, finish = 0.88f;
    const float centre_x = 0.5f * cell_span;
    const float scale = fminf(fmaxf(0.0f, amplitude), 0.42f * cell_span);
    const float baseline_x = p * cell_span;
    float shape_x, shape_y = 0.0f;
    if (p <= lead) {
        shape_x = (centre_x + scale) * (p / lead);
    } else if (p < finish) {
        const float t = 0.5f * (p - lead) / (finish - lead);
        const float r = (1.0f - t) * (1.0f - t);
        shape_x = centre_x + scale * r * cosf(6.0f * PI * t);
        shape_y = scale * r * sinf(6.0f * PI * t);
    } else {
        const float q = (p - finish) / (1.0f - finish);
        const float last_x = centre_x - 0.25f * scale;
        shape_x = last_x + (cell_span - last_x) * q;
    }
    *x = cell_start + baseline_x + strength * (shape_x - baseline_x);
    *y = strength * shape_y;
}

/* The Wolfram spiral, with an analytically derived unit normal for the
 * second nearby strand. The centreline equation itself is not modified. */
static void wolfram_tentacle_sample(float t, float *x, float *y,
                                    float *nx, float *ny) {
    const float radius = (1.0f - t) * (1.0f - t);
    const float dr = -2.0f * (1.0f - t);
    const float theta = 6.0f * PI * t;
    const float cs = cosf(theta), sn = sinf(theta);
    const float dx = dr * cs - 6.0f * PI * radius * sn;
    const float dy = dr * sn + 6.0f * PI * radius * cs;
    const float speed = sqrtf(dx * dx + dy * dy);
    *x = radius * cs;
    *y = radius * sn;
    *nx = dy / speed;
    *ny = -dx / speed;
}

/* Two near-parallel spirals form a continuous, pointed tentacle outline.
 * From p=.12 to .50 the first strand samples Wolfram t=0..t_limit.
 * From p=.50 to .88 the second strand samples t_limit..0 in reverse.
 * Its normal offset is largest at the base and tends to zero at the tip,
 * so both arms meet exactly at p=.50. The remaining span is a straight
 * connector to the separator. A zero t_limit gives an exact flat line. */
void proto_wolfram_tentacle_point(float p, float cell_start, float cell_span,
                                  float amplitude, float t_limit,
                                  float strand_offset, float *x, float *y) {
    p = fmaxf(0.0f, fminf(1.0f, p));
    t_limit = fmaxf(0.0f, fminf(0.5f, t_limit));
    const float lead = 0.12f, tip = 0.50f, tail = 0.88f;
    const float half_span = 0.5f * cell_span;
    const float scale = fminf(fmaxf(0.0f, amplitude), 0.39f * cell_span);
    const float offset = fminf(1.25f, fmaxf(0.0f, strand_offset));
    const float blend = fminf(1.0f, t_limit / 0.045f);
    const float fade = blend * blend * (3.0f - 2.0f * blend);
    float sx = p * cell_span, sy = 0.0f;
    float wx = 0.0f, wy = 0.0f, nx = 0.0f, ny = 0.0f;

    if (p <= lead) {
        sx = (half_span + scale) * p / lead;
    } else if (p <= tip) {
        const float q = (p - lead) / (tip - lead);
        wolfram_tentacle_sample(q * t_limit, &wx, &wy, &nx, &ny);
        sx = half_span + scale * wx;
        sy = scale * wy;
    } else if (p <= tail) {
        const float q = (tail - p) / (tail - tip);
        const float t = q * t_limit;
        wolfram_tentacle_sample(t, &wx, &wy, &nx, &ny);
        /* Width decreases continuously to zero at the moving inner tip. */
        const float width = offset * powf(1.0f - q, 1.4f);
        sx = half_span + scale * wx + width * nx;
        sy = scale * wy + width * ny;
    } else {
        /* Continue from the second arm's base without any positional jump. */
        wolfram_tentacle_sample(0.0f, &wx, &wy, &nx, &ny);
        const float q = (p - tail) / (1.0f - tail);
        const float start_x = half_span + scale + offset * nx;
        const float start_y = offset * ny;
        sx = start_x + (cell_span - start_x) * q;
        sy = start_y * (1.0f - q);
    }
    const float baseline_x = p * cell_span;
    *x = cell_start + baseline_x + fade * (sx - baseline_x);
    *y = fade * sy;
}

/* A variable-radius capsule gives a closed, filled strip along one segment
 * of the Wolfram centreline. Rasterizing the union of consecutive capsules
 * avoids the self-intersecting polygon / even-odd artifacts of double spirals.
 * Coordinates are pixel-centred, and all calculations are in logical pixels. */
static void ribbon_capsule(ProtoFrame *frame,
                           float ax, float ay, float ar,
                           float bx, float by, float br,
                           unsigned char ink) {
    const float rmax = fmaxf(ar, br);
    int xmin = (int)floorf(fminf(ax, bx) - rmax - 1.0f);
    int xmax = (int)ceilf(fmaxf(ax, bx) + rmax + 1.0f);
    int ymin = (int)floorf(fminf(ay, by) - rmax - 1.0f);
    int ymax = (int)ceilf(fmaxf(ay, by) + rmax + 1.0f);
    if (xmin < 0) xmin = 0;
    if (ymin < 0) ymin = 0;
    if (xmax >= PROTO_WIDTH) xmax = PROTO_WIDTH - 1;
    if (ymax >= PROTO_HEIGHT) ymax = PROTO_HEIGHT - 1;
    const float vx = bx - ax, vy = by - ay;
    const float length2 = vx*vx + vy*vy;
    for (int y = ymin; y <= ymax; ++y) {
        const float py = (float)y + 0.5f;
        for (int x = xmin; x <= xmax; ++x) {
            const float px = (float)x + 0.5f;
            float u = length2 > 1.0e-9f ?
                ((px-ax)*vx + (py-ay)*vy) / length2 : 0.0f;
            u = fmaxf(0.0f, fminf(1.0f, u));
            const float rx = px - (ax + u*vx), ry = py - (ay + u*vy);
            const float radius = ar + u * (br-ar);
            if (rx*rx + ry*ry <= radius*radius)
                frame->ribbon_mask[(size_t)y * PROTO_WIDTH + x] = ink;
        }
    }
}

/* Render one open spiral as a closed variable-width tube (round base cap,
 * tapered point). Its centreline is precisely the input Wolfram equation.
 * The centreline occupies t=0..t_limit, not the full .5 range at all times. */
static void ribbon_cell(ProtoFrame *frame, float cell_start, float cell_span,
                        float amplitude, float full_width, float t_limit) {
    if (t_limit <= 0.00001f || full_width <= 0.0f) return;
    const float scale = fminf(fmaxf(0.0f, amplitude), 0.39f * cell_span);
    const float centre = cell_start + 0.5f * cell_span;
    const float b = fminf(1.0f, t_limit / 0.045f);
    const float fade = b*b*(3.0f-2.0f*b);
    const float half_width = 0.5f * full_width * fade;
    const int samples = 128;
    float prev_x = 0.0f, prev_y = 0.0f, prev_r = 0.0f;
    for (int k = 0; k <= samples; ++k) {
        const float u = (float)k / (float)samples;
        const float t = t_limit * u;
        const float radius = (1.0f-t)*(1.0f-t);
        const float angle = 6.0f * PI * t;
        const float tangent = centre + scale * radius * cosf(angle);
        const float normal = scale * radius * sinf(angle);
        const float sx = PROTO_WIDTH * 0.5f +
                         tangent * frame->cos_angle - normal * frame->sin_angle;
        const float sy = PROTO_HEIGHT * 0.5f +
                         tangent * frame->sin_angle + normal * frame->cos_angle;
        const float width = half_width * powf(1.0f-u, 1.25f);
        if (k > 0)
            ribbon_capsule(frame, prev_x, prev_y, prev_r, sx, sy, width, 1);
        prev_x = sx;
        prev_y = sy;
        prev_r = width;
    }
}

/* Builds the filled cyan tentacle mask separately from the open world divider.
 * The inside of the tube never changes colour when a curl overlaps another. */
/* Two independent, genuinely coloured ribbons that coil around a common
 * centre. They are opposite-phase arms, not a cyan spiral with a white edge.
 * A short root ties the cyan arm to the cyan half and the violet arm to the
 * violet half. Each arm tapers to the animated endpoint (not a fixed tip). */
static void double_ribbon_point(float u, float t_limit, float centre,
                                float scale, float side, float *x, float *y) {
    const float t = t_limit * u;
    const float radius = scale * (1.0f-t)*(1.0f-t);
    const float angle = 6.0f * PI * t + side * (0.5f * PI);
    *x = centre + radius * cosf(angle);
    *y = radius * sinf(angle);
}

static void double_ribbon_world_to_screen(const ProtoFrame *frame,
                                          float tx, float ty,
                                          float *sx, float *sy) {
    *sx = PROTO_WIDTH * 0.5f + tx * frame->cos_angle - ty * frame->sin_angle;
    *sy = PROTO_HEIGHT * 0.5f + tx * frame->sin_angle + ty * frame->cos_angle;
}

static void double_ribbon_cell(ProtoFrame *frame, float cell_start,
                               float cell_span, float amplitude,
                               float ribbon_width, float t_limit) {
    if (t_limit <= 0.00001f) return;
    const float scale = fminf(fmaxf(amplitude, 0.0f), 0.35f * cell_span);
    if (scale <= 0.0f) return;
    const float centre = cell_start + 0.50f * cell_span;
    const float opacity = fminf(1.0f, t_limit / 0.045f);
    const float fade = opacity * opacity * (3.0f - 2.0f * opacity);
    const float half_width = 0.5f * ribbon_width * fade;
    const int segments = 96;
    float prev_x[2], prev_y[2], prev_w[2];
    float next_x[2], next_y[2], next_w[2];

    for (int arm = 0; arm < 2; ++arm) {
        const float side = arm == 0 ? +1.0f : -1.0f;
        float tx, ty;
        double_ribbon_point(0.0f, t_limit, centre, scale, side, &tx, &ty);
        double_ribbon_world_to_screen(frame, tx, ty,
                                       &prev_x[arm], &prev_y[arm]);
        prev_w[arm] = half_width;
        /* Attach each coloured ribbon to the matching side of the
         * separating line. This root lies just inside its own half-plane. */
        const float root_x = centre - 0.72f * scale;
        const float root_y = side * 0.55f;
        float sx, sy;
        double_ribbon_world_to_screen(frame, root_x, root_y, &sx, &sy);
        ribbon_capsule(frame, sx, sy, half_width * 0.55f,
                       prev_x[arm], prev_y[arm], half_width,
                       arm == 0 ? 2 : 3);
    }

    for (int k = 1; k <= segments; ++k) {
        const float u = (float)k / (float)segments;
        const float radius_factor = powf(1.0f-u, 1.15f);
        for (int arm = 0; arm < 2; ++arm) {
            float tx, ty;
            const float side = arm == 0 ? +1.0f : -1.0f;
            double_ribbon_point(u, t_limit, centre, scale, side, &tx, &ty);
            double_ribbon_world_to_screen(frame, tx, ty,
                                           &next_x[arm], &next_y[arm]);
            next_w[arm] = half_width * radius_factor;
        }
        /* Change the foreground strand at each half-turn, creating a
         * genuine over/under weave at intersections rather than one colour
         * always obscuring the other. */
        const int half_turn = (int)floorf(6.0f * t_limit * u);
        const int first = (half_turn & 1) ? 0 : 1;
        for (int pass = 0; pass < 2; ++pass) {
            const int arm = pass == 0 ? first : 1-first;
            ribbon_capsule(frame, prev_x[arm], prev_y[arm], prev_w[arm],
                           next_x[arm], next_y[arm], next_w[arm],
                           arm == 0 ? 2 : 3);
        }
        for (int arm = 0; arm < 2; ++arm) {
            prev_x[arm] = next_x[arm];
            prev_y[arm] = next_y[arm];
            prev_w[arm] = next_w[arm];
        }
    }
}

/* Rope-like deformation: a straight segment is pinched at the middle and
 * twisted, producing two mirrored semi-spirals around the pinch. Each colour
 * is offset from the same rope centreline using the local normal, so the
 * result reads as one twisted rope rather than two unrelated spirals. */
static void pinched_rope_centre_point(float q, float twist, float centre,
                                      float half_span, float amplitude,
                                      float direction, float *x, float *y) {
    const float advance = q * half_span;
    const float envelope = twist * q * powf(fmaxf(0.0f, 1.0f - q), 1.10f);
    const float radius = amplitude * 0.82f * envelope;
    const float angle = direction * (1.05f * PI + 5.1f * PI * twist * q);
    *x = centre + direction * advance + direction * radius * cosf(angle);
    *y = radius * sinf(angle);
}

static void pinched_rope_world_to_screen(const ProtoFrame *frame,
                                         float tx, float ty,
                                         float *sx, float *sy) {
    *sx = PROTO_WIDTH * 0.5f + tx * frame->cos_angle - ty * frame->sin_angle;
    *sy = PROTO_HEIGHT * 0.5f + tx * frame->sin_angle + ty * frame->cos_angle;
}

static void pinched_rope_cell(ProtoFrame *frame, float cell_start,
                              float cell_span, float amplitude,
                              float ribbon_width, float t_limit) {
    if (t_limit <= 0.00001f) return;
    const float centre = cell_start + 0.5f * cell_span;
    const float half_span = 0.5f * cell_span;
    const float scale = fminf(fmaxf(amplitude, 0.0f), 0.24f * cell_span);
    if (scale <= 0.0f) return;
    const float fade = t_limit <= 0.045f ?
        (t_limit / 0.045f) * (t_limit / 0.045f) * (3.0f - 2.0f * (t_limit / 0.045f)) : 1.0f;
    const float base_width = 0.5f * ribbon_width * fade;
    const int segments = 72;

    for (int side = -1; side <= 1; side += 2) {
        float prev_cx, prev_cy, prev_tx, prev_ty;
        pinched_rope_centre_point(0.0f, t_limit, centre, half_span, scale,
                                  (float)side, &prev_cx, &prev_cy);
        pinched_rope_centre_point(1.0f/segments, t_limit, centre, half_span, scale,
                                  (float)side, &prev_tx, &prev_ty);
        float prev_dx = prev_tx - prev_cx;
        float prev_dy = prev_ty - prev_cy;
        float prev_len = hypotf(prev_dx, prev_dy);
        if (prev_len < 1e-5f) { prev_dx = (float)side; prev_dy = 0.0f; prev_len = 1.0f; }
        float prev_nx = -prev_dy / prev_len;
        float prev_ny =  prev_dx / prev_len;
        float prev_sep = base_width * (0.30f + 0.60f * (1.0f - 0.0f));
        float prev_hw  = base_width * (0.35f + 0.65f * (1.0f - 0.0f));

        float prev_ax, prev_ay, prev_bx, prev_by;
        pinched_rope_world_to_screen(frame,
                                     prev_cx + prev_nx * prev_sep,
                                     prev_cy + prev_ny * prev_sep,
                                     &prev_ax, &prev_ay);
        pinched_rope_world_to_screen(frame,
                                     prev_cx - prev_nx * prev_sep,
                                     prev_cy - prev_ny * prev_sep,
                                     &prev_bx, &prev_by);

        for (int k = 1; k <= segments; ++k) {
            const float q = (float)k / (float)segments;
            float cx, cy;
            pinched_rope_centre_point(q, t_limit, centre, half_span, scale,
                                      (float)side, &cx, &cy);
            float fx, fy;
            if (k < segments) {
                pinched_rope_centre_point((float)(k+1) / (float)segments, t_limit,
                                          centre, half_span, scale, (float)side,
                                          &fx, &fy);
            } else {
                fx = cx + (float)side;
                fy = cy;
            }
            float dx = fx - cx;
            float dy = fy - cy;
            float len = hypotf(dx, dy);
            if (len < 1e-5f) { dx = (float)side; dy = 0.0f; len = 1.0f; }
            float nx = -dy / len;
            float ny =  dx / len;

            const float edge_falloff = powf(fmaxf(0.0f, 1.0f - q), 0.42f);
            const float sep = base_width * (0.30f + 0.60f * edge_falloff);
            const float hw  = base_width * (0.35f + 0.65f * edge_falloff);

            float ax, ay, bx, by;
            pinched_rope_world_to_screen(frame, cx + nx * sep, cy + ny * sep, &ax, &ay);
            pinched_rope_world_to_screen(frame, cx - nx * sep, cy - ny * sep, &bx, &by);

            const int weave = (int)floorf(4.0f * t_limit * q * 4.0f);
            if (weave & 1) {
                ribbon_capsule(frame, prev_bx, prev_by, prev_hw, bx, by, hw, 3);
                ribbon_capsule(frame, prev_ax, prev_ay, prev_hw, ax, ay, hw, 2);
            } else {
                ribbon_capsule(frame, prev_ax, prev_ay, prev_hw, ax, ay, hw, 2);
                ribbon_capsule(frame, prev_bx, prev_by, prev_hw, bx, by, hw, 3);
            }
            prev_ax = ax; prev_ay = ay;
            prev_bx = bx; prev_by = by;
            prev_hw = hw;
        }
    }
}

void proto_wolfram_ribbon_prepare(ProtoFrame *frame,
                                   const ProtoConfig *config,
                                   const ProtoAnimation *animation) {
    memset(frame->ribbon_mask, 0, sizeof frame->ribbon_mask);
    const int count = config->section_count;
    const float visible = PROTO_WIDTH * 0.5f * fabsf(frame->cos_angle) +
                          PROTO_HEIGHT * 0.5f * fabsf(frame->sin_angle);
    for (int side = 0; side < count; ++side) {
        const ProtoSection *section = &config->sections[side];
        if (section->formula != PROTO_WOLFRAM_RIBBON &&
            section->formula != PROTO_WOLFRAM_DOUBLE_RIBBON &&
            section->formula != PROTO_PINCHED_ROPE) continue;
        const float section_span = 2.0f*RANGE / count;
        const float section_start = -RANGE + side*section_span;
        float start = section_start, span = section_span;
        if (count == 2 && section->repeat_visible) {
            start = side == 0 ? -visible : 0.0f;
            span = visible;
        }
        int cells = (int)lroundf(section->cycles);
        if (cells < 1) cells = 1;
        if (cells > 16) cells = 16;
        const float cell = span / cells;
        const float gap = fminf(fmaxf(0.0f, section->gap_pixels), 0.65f * cell);
        const float width = section->ribbon_width > 0.0f ?
            section->ribbon_width : 7.0f;
        const float t_limit = section->phase_speed == 0.0f ? 0.5f :
            0.25f*(1.0f-cosf(2.0f*PI*section->phase_speed*animation->time));
        for (int i = 0; i < cells; ++i) {
            const float cell_start = start+i*cell+0.5f*gap;
            if (section->formula == PROTO_WOLFRAM_DOUBLE_RIBBON)
                double_ribbon_cell(frame, cell_start, cell-gap,
                                   section->amplitude, width, t_limit);
            else if (section->formula == PROTO_PINCHED_ROPE)
                pinched_rope_cell(frame, cell_start, cell-gap,
                                  section->amplitude, width, t_limit);
            else
                ribbon_cell(frame, cell_start, cell-gap,
                            section->amplitude, width, t_limit);
        }
    }
}

/* True two-arm Fermat spiral: polar r = +/- a*sqrt(theta).
 * The growing theta_max simultaneously increases turns and outside radius.
 * Rotating by -theta_max fixes both outer endpoints on the baseline: the
 * complete path left edge -> negative arm -> centre -> positive arm ->
 * right edge is continuous. No ribbons or white strokes are involved. */
/* Patch 0021: two separate smoothstep position curves per cycle.
 * Their velocity, 6u(1-u), is parabolic: slow -> fast -> slow on BOTH halves.
 * With phase_speed == 0, show the fully extended shape for inspection. */
float proto_fermat_parabolic_progress(float time, float phase_speed) {
    if (phase_speed == 0.0f) return 1.0f;
    float phase = fmodf(time * phase_speed, 1.0f);
    if (phase < 0.0f) phase += 1.0f;
    const float u = phase < 0.5f ? 2.0f * phase : 2.0f * (1.0f - phase);
    return u * u * (3.0f - 2.0f * u);
}

void proto_fermat_spiral_point(float p, float cell_start, float cell_span,
                               float amplitude, float progress,
                               float *x, float *y) {
    p = fmaxf(0.0f, fminf(1.0f, p));
    progress = fmaxf(0.0f, fminf(1.0f, progress));
    const float centre = cell_start + 0.5f * cell_span;
    const float max_radius = fminf(fmaxf(0.0f, amplitude), 0.49f * cell_span);
    const float outer_radius = max_radius * powf(progress, 0.58f);
    const float theta_max = 7.0f * PI * powf(progress, 1.18f);
    const float lead = 0.12f, trail = 0.88f;

    *y = 0.0f;
    if (progress <= 0.0001f || theta_max <= 0.0001f || outer_radius <= 0.0001f) {
        *x = cell_start + p * cell_span;
        return;
    }

    if (p <= lead) {
        *x = cell_start + (centre - outer_radius - cell_start) * (p / lead);
        return;
    }
    if (p >= trail) {
        const float u = (p - trail) / (1.0f - trail);
        *x = centre + outer_radius +
             (cell_start + cell_span - centre - outer_radius) * u;
        return;
    }

    float arm_u, side;
    if (p <= 0.5f) {
        arm_u = (p - lead) / (0.5f - lead);      /* 0 at base, 1 at tip */
        side = -1.0f;
    } else {
        arm_u = (trail - p) / (trail - 0.5f);    /* 0 at base, 1 at tip */
        side = 1.0f;
    }
    arm_u = fmaxf(0.0f, fminf(1.0f, arm_u));

    /* Broad near the base, strongly tapered near the tip. */
    const float radius = outer_radius * powf(1.0f - arm_u, 0.58f);

    /* Curl is concentrated near the tip to avoid a teardrop silhouette. */
    /* Quintic shoulder blends the flat connector into the coil:
     * curvature starts at zero rather than turning abruptly. */
    const float shoulder_t = fminf(1.0f, arm_u / 0.22f);
    const float shoulder = shoulder_t * shoulder_t * shoulder_t *
        (shoulder_t * (6.0f * shoulder_t - 15.0f) + 10.0f);
    const float turn_mix = powf(arm_u, 2.35f) * shoulder;
    const float phase = -theta_max * turn_mix;

    *x = centre + side * radius * cosf(phase);
    *y = side * radius * sinf(phase);
}


/* Patch 0022: make the motion noticeably faster in the middle of EACH
 * expansion and retraction half. The velocity envelope is the cube of the
 * normalized parabola 4u(1-u), rather than a constant-rate or sine ease.
 * Integrated exact position: 35u^4 - 84u^5 + 70u^6 - 20u^7. */
float proto_fermat_twist_progress(float time, float phase_speed) {
    if (phase_speed == 0.0f) return 1.0f;
    const double cycles = (double)time * fabs((double)phase_speed);
    double phase = cycles - floor(cycles);
    if (phase < 0.0) phase += 1.0;
    const double u = phase <= 0.5 ? 2.0 * phase : 2.0 * (1.0 - phase);
    const double u2 = u * u;
    const double u4 = u2 * u2;
    return (float)(u4 * (35.0 + u * (-84.0 + u * (70.0 - 20.0 * u))));
}

/* The full 0->100->0 loop (5 s in the supplied profile) keeps one direction.
 * The NEXT loop is mirrored. Switch only when the shape is fully flat. */
float proto_fermat_twist_direction(float time, float phase_speed) {
    if (phase_speed == 0.0f) return 1.0f;
    const double cycle = floor((double)time * fabs((double)phase_speed));
    return fmod(cycle, 2.0) == 0.0 ? 1.0f : -1.0f;
}

/* Same softened Fermat-tentacle outline as patch 0021, but with separate
 * independent controls for physical growth and winding development.
 *  - progress: physical base grows identically for ALL FOUR cells
 *  - winding: .25 / .50 / .75 / 1.00 of the total tip turns
 *  - direction: +/- to mirror the rotation on alternating cycles
 * This is a continuous two-arm dividing curve, not an overlaid tube. */
void proto_fermat_twist_point(float p, float cell_start, float cell_span,
                              float amplitude, float progress, float winding,
                              float direction, float *x, float *y) {
    p = fmaxf(0.0f, fminf(1.0f, p));
    progress = fmaxf(0.0f, fminf(1.0f, progress));
    winding = fmaxf(0.0f, fminf(1.0f, winding));
    const float centre = cell_start + 0.5f * cell_span;
    /* Allow roots to touch exactly at full extension. */
    const float max_radius = fminf(fmaxf(amplitude, 0.0f), 0.50f * cell_span);
    const float outer_radius = max_radius * powf(progress, 0.58f);
    const float theta_max = 7.0f * PI * powf(progress, 1.18f) * winding;
    /* Wide shoulders and zero endpoint tangents, rather than two joined
     * straight pieces with visible corners. */
    const float lead = 0.20f, trail = 0.80f;
    *y = 0.0f;

    if (progress <= 0.0001f || theta_max <= 0.0001f || outer_radius <= 0.0001f) {
        *x = cell_start + p * cell_span;
        return;
    }
    if (p <= lead) {
        const float q = p / lead;
        const float smooth = q * q * (3.0f - 2.0f * q);
        *x = cell_start + (centre - outer_radius - cell_start) * smooth;
        return;
    }
    if (p >= trail) {
        const float q = (p - trail) / (1.0f - trail);
        const float smooth = q * q * (3.0f - 2.0f * q);
        *x = centre + outer_radius +
             (cell_start + cell_span - centre - outer_radius) * smooth;
        return;
    }

    float arm_u, side;
    if (p <= 0.5f) {
        arm_u = (p - lead) / (0.5f - lead);
        side = -1.0f;
    } else {
        arm_u = (trail - p) / (trail - 0.5f);
        side = 1.0f;
    }
    arm_u = fmaxf(0.0f, fminf(1.0f, arm_u));

    /* Broad radius at the root, with ZERO slope at u=0 (C1 join). The
     * extra term cancels the derivative of (1-u)^0.64 at u=0 and fades
     * smoothly by u=0.4, without softening the final pointed tip. */
    const float root_q = fminf(1.0f, arm_u / 0.40f);
    const float root_fade = 1.0f - root_q * root_q * (3.0f - 2.0f * root_q);
    const float radius = outer_radius * powf(1.0f - arm_u, 0.64f) *
                         (1.0f + 0.64f * arm_u * root_fade);

    /* Much rounder shoulder at the root. */
    const float shoulder_t = fminf(1.0f, arm_u / 0.40f);
    const float shoulder = shoulder_t * shoulder_t * shoulder_t *
        (shoulder_t * (shoulder_t * 6.0f - 15.0f) + 10.0f);

    /* Most of the turns still happen near the tip. */
    const float tip_turn = powf(arm_u, 2.55f) * shoulder * shoulder;
    const float angle = -(direction < 0.0f ? -1.0f : 1.0f) * theta_max * tip_turn;

    *x = centre + side * radius * cosf(angle);
    *y = side * radius * sinf(angle);
}


/* Peak-radius-weighted packed layout for Fermat twists.
 * Eight weights are 0.5, 0.5+0.5/7, ..., 1.0; their sum is 6.
 * Prefix sums produce shared boundaries (the same formula for both cells)
 * instead of accumulating rounded fixed-size centre positions. */
/* Patch 0028: exponential packed widths. Prefix-sum boundaries mean
 * adjacent cells share the same exact border even with unequal widths. */
void proto_fermat_packed_cell(float start, float span, int count, int index,
                              float *cell_start, float *cell_span) {
    if (count < 2) {
        *cell_start = start;
        *cell_span = span;
        return;
    }
    if (index < 0) index = 0;
    if (index >= count) index = count - 1;
    double total = 0.0, before = 0.0, after = 0.0;
    const double denom = (double)(count-1);
    for (int i = 0; i < count; ++i) {
        const double weight = 0.5 * exp2((double)i / denom);
        total += weight;
        if (i < index) before += weight;
        if (i <= index) after += weight;
    }
    const float left = start + (float)(span * before / total);
    const float right = index == count - 1 ? start + span :
                        start + (float)(span * after / total);
    *cell_start = left;
    *cell_span = right - left;
}


/* Literal form of the requested Metroid expression, x in [0,9].
 * mod() is the positive fractional part (works for negative x-3). */
float proto_metroid_lightning_value(float x) {
    const float sigmoid = 1.0f/(1.0f+expf(-3.0f*(x-1.0f)));
    const float z = x-3.0f;
    const float mod1 = z-floorf(z);
    const float tooth = 1.0f-2.0f*fabsf(mod1-0.5f)-0.5f;
    return sigmoid*(0.15f*z+0.5f)*tooth +
           fmaxf(0.0f,x-6.5f)*0.2f*sinf(50.0f*x);
}

/* Patch 0038: preserve the original triangle's broad +/- silhouette.
 * The source formula is a triangular wave weighted by a sigmoid and a
 * linearly growing envelope. Subtracting the secant destroys its negative
 * half; do NOT subtract a per-tooth mean or put radio noise in its place.
 * From tooth 3 onward, add subdued, tapered electrical detail, then normalize
 * positive and negative peaks independently without changing the outline. */
static float proto_metroid_0037_raw(int tooth, float p) {
    p=fminf(1.0f,fmaxf(0.0f,p));
    /* Patch 0046: first four clean bipolar triangle periods.
     * Start HALF WAY through the rising slope, reach the positive crest,
     * descend linearly to the negative trough, then rise to the halfway
     * point of the next rising slope. No harmonics or tiny join shoulders.
     * 0->0, 0.25->+1, 0.5->0, 0.75->-1, 1->0.
     * Only the first four teeth use this exact piecewise waveform. */
    if (tooth < 4) {
        if (p <= 0.25f) return 4.0f*p;
        if (p <= 0.75f) return 2.0f-4.0f*p;
        return 4.0f*p-4.0f;
    }
    const float x=1.0f+(float)tooth+p;
    const float sigmoid=1.0f/(1.0f+expf(-3.0f*(x-1.0f)));
    const float envelope=sigmoid*(0.15f*(x-3.0f)+0.5f);
    /* The triangular part of the exact supplied expression, with the same
     * phase as mod(x-3,1). Endpoints=-0.5, midpoint=+0.5. */
    const float phase=x-3.0f-floorf(x-3.0f);
    const float triangle=1.0f-2.0f*fabsf(phase-0.5f)-0.5f;
    const float q=fminf(p,1.0f-p);
    const float v=fminf(1.0f,q/0.08f);
    const float root=v*v*(3.0f-2.0f*v);
    float electric=0.0f;
    if(tooth>=2) {
        /* Noise stays subordinate to the broad triangle, even on tooth 8.
         * Retain a small share of the formula's 50*x tail on last teeth. */
        const float strength=0.11f+0.014f*(float)(tooth-2);
        const float smooth=sinf(PI*p);
        electric=envelope*strength*smooth*smooth*(
            0.66f*sinf(2.0f*PI*(2.5f+0.10f*tooth)*p+0.31f*tooth)+
            0.34f*sinf(2.0f*PI*(5.1f+0.18f*tooth)*p-0.25f*tooth));
        electric+=0.10f*fmaxf(0.0f,x-6.5f)*0.2f*sinf(50.0f*x)*smooth*smooth;
    }
    return root*(envelope*triangle+electric);
}

float proto_metroid_bipolar_value(int tooth,float p) {
    if(tooth<0) tooth=0;
    if(tooth>7) tooth=7;
    p=fminf(1.0f,fmaxf(0.0f,p));
    if(p<=0.0f||p>=1.0f) return 0.0f;
    /* Cached once per tooth. No mean subtraction: that was the regression
     * that flattened the triangles into something like radio noise. */
    static float positive[8]={0};
    static float negative[8]={0};
    if(positive[tooth]==0.0f) {
        float pos=0.0f,neg=0.0f;
        for(int k=0;k<=4096;++k) {
            const float val=proto_metroid_0037_raw(tooth,(float)k/4096.0f);
            pos=fmaxf(pos,val);
            neg=fmaxf(neg,-val);
        }
        negative[tooth]=fmaxf(neg,1.0e-6f);
        positive[tooth]=fmaxf(pos,1.0e-6f);
    }
    const float v=proto_metroid_0037_raw(tooth,p);
    return v>=0.0f ? v/positive[tooth] : v/negative[tooth];
}

/* The same 7th-degree fast-midpoint easing as the Fermat twist. Each cell's
 * expansion begins late but is guaranteed to hit its shared peak at the SAME
 * global half-cycle boundary. Retraction remains synchronized and smooth. */
float proto_fermat_stagger_progress(float time, float speed,
                                    int index, float delay) {
    if (speed == 0.0f) return 1.0f;
    const double period = 1.0/fabs((double)speed);
    double phase = fmod((double)time, period);
    if (phase < 0.0) phase += period;
    const double half = period*0.5;
    const double offset = fmin(half*0.65, (double)index * (double)delay);
    double u;
    if (phase <= half)
        u = (phase-offset)/(half-offset);
    else
        u = (period-phase)/half;
    if (u <= 0.0) return 0.0f;
    if (u >= 1.0) return 1.0f;
    const double u4 = u*u*u*u;
    return (float)(u4*(35.0+u*(-84.0+u*(70.0-20.0*u))));
}

/* Patch 0030: the SAME 5-second clock with a 0.2s outward delay on
 * both expansion and retraction. The last cell starts both half-phases 1.4s
 * after the first; the common expansion peak stays fixed at t=2.5s. */
float proto_fermat_bothways_progress(float time, float speed,
                                    int from_centre, float delay) {
    if (speed == 0.0f) return 1.0f;
    const double period = 1.0 / fabs((double)speed);
    double phase = fmod((double)time, period);
    if (phase < 0.0) phase += period;
    const double half = period * 0.5;
    const double offset = fmin(0.9*half, fmax(0.0, (double)from_centre * (double)delay));
    const double duration = half - offset;
    /* Retracting: pause at full deployment until the delayed start time. */
    const double u = phase <= half ? (phase-offset)/duration :
                     (period-phase)/duration;
    if (u <= 0.0) return 0.0f;
    if (u >= 1.0) return 1.0f;
    const double u2 = u*u;
    const double u4 = u2*u2;
    return (float)(u4*(35.0+u*(-84.0+u*(70.0-20.0*u))));
}

/* Normalize against the ACTUAL softened Fermat outline at unit radius. The
 * eight cached reference maxima are shared by both mirrored directions;
 * the expensive sample loop runs only once per index, not once per pixel. */
float proto_fermat_tall_reference_peak(int from_centre) {
    static float cached[8] = {0};
    if (from_centre < 0) from_centre = 0;
    if (from_centre > 7) from_centre = 7;
    if (cached[from_centre] > 0.0f) return cached[from_centre];
    const float winding = 0.25f + 0.75f * (float)from_centre / 7.0f;
    float maximum = 0.0f;
    for (int k = 0; k <= 2048; ++k) {
        float x, y;
        proto_fermat_twist_point((float)k/2048.0f, 0.0f, 2.0f,
                                 1.0f, 1.0f, winding, 1.0f, &x, &y);
        maximum = fmaxf(maximum, fabsf(y));
    }
    cached[from_centre] = fmaxf(0.05f, maximum);
    return cached[from_centre];
}

/* Patch 0031: dramatic eight-step radial sizes, centre -> exterior.
 * The user supplied seven percentages for eight positions; 40% fills the missing step.
 * These are true two-dimensional sizes, not separate x/y multipliers. */
float proto_fermat_round_grade(int index) {
    static const float steps[8] = {0.10f, 0.15f, 0.21f, 0.32f,
                                   0.40f, 0.50f, 0.70f, 1.00f};
    if (index < 0) index = 0;
    if (index > 7) index = 7;
    return steps[index];
}

/* Radius needed to obtain a target height. The reference max is obtained
 * directly from the UNCHANGED softened Fermat curve for this winding level. */
float proto_fermat_round_radius(int index, float amplitude) {
    const float target_y = 40.0f * proto_fermat_round_grade(index) *
                           fmaxf(0.0f, amplitude) / 15.0f;
    return target_y / proto_fermat_tall_reference_peak(index);
}

/* Ordered centres, compressed to keep the largest circle on screen at zero
 * rotation. Overlap is inevitable for eight large round circles in 120px;
 * the original packed-cell boundaries still define their flat endpoints. */
float proto_fermat_round_center(float start, float span, int index,
                                float amplitude) {
    if (index < 0) index = 0;
    if (index > 7) index = 7;
    float c0, w0, c7, w7, ci, wi;
    proto_fermat_packed_cell(start, span, 8, 0, &c0, &w0);
    proto_fermat_packed_cell(start, span, 8, 7, &c7, &w7);
    proto_fermat_packed_cell(start, span, 8, index, &ci, &wi);
    c0 += .5f*w0;
    c7 += .5f*w7;
    ci += .5f*wi;
    const float fraction = (ci-c0) / fmaxf(1.0e-4f, c7-c0);
    const float first = start + proto_fermat_round_radius(0, amplitude);
    const float last = start + span - proto_fermat_round_radius(7, amplitude);
    return first + (last-first) * fraction;
}

/* The actual coil is an ISOTROPIC transform of the existing Fermat curve.
 * Unlike patch 0030, BOTH x and y use the same scale. The smooth mask on
 * the short horizontal connectors ensures p=0 and p=1 still equal the
 * original neighbouring segment boundaries at every phase of animation. */
void proto_fermat_round_point(float p, float packed_start, float packed_span,
                              float section_start, float section_span,
                              int index, float amplitude, float progress,
                              float direction, float *x, float *y) {
    p = fminf(1.0f, fmaxf(0.0f, p));
    progress = fminf(1.0f, fmaxf(0.0f, progress));
    const float old_centre = packed_start + .5f*packed_span;
    const float base_radius = .5f*packed_span;
    const float winding = .25f+.75f*(float)index/7.0f;
    float old_x, old_y;
    proto_fermat_twist_point(p, packed_start, packed_span, base_radius,
                            progress, winding, direction, &old_x, &old_y);
    const float scale = proto_fermat_round_radius(index, amplitude) /
                        fmaxf(base_radius, 1.0e-5f);
    const float target_centre = proto_fermat_round_center(section_start,
                                   section_span, index, amplitude);
    const float left_t = fminf(1.0f, p/.20f);
    const float right_t = fminf(1.0f, (1.0f-p)/.20f);
    const float left_w = left_t*left_t*(3.0f-2.0f*left_t);
    const float right_w = right_t*right_t*(3.0f-2.0f*right_t);
    const float blend = progress*left_w*right_w;
    const float ratio = 1.0f + blend*(scale-1.0f);
    *x = old_centre + blend*(target_centre-old_centre) +
         (old_x-old_centre)*ratio;
    *y = old_y*ratio;
}

/* Patch 0032: four 2D-round patterns on an angle-invariant half diagonal.
 * Only the first fifth of the final (80px high) spiral lies along the
 * visible diagonal at corner alignment; the remainder extends beyond. */
void proto_fermat_diagonal_cell(float start, float span, int layout,
                               int index, float amplitude,
                               float *cell_start, float *cell_span) {
    static const float weights[4][7] = {
        {3.0f,4.8f,7.6f,12.2f,19.5f,31.2f,50.0f},
        {6.0f,6.9f,8.9f,12.4f,18.4f,28.7f,46.9f},
        {8.4f,8.8f,10.1f,12.7f,17.6f,26.6f,44.0f},
        {11.6f,11.7f,12.2f,13.5f,16.7f,23.5f,39.2f}
    };
    if (layout < 1 || layout > 4) layout = 3;
    if (index < 0) index = 0;
    if (index > 7) index = 7;
    const float ref = proto_fermat_tall_reference_peak(7);
    const float outer = 80.0f * fmaxf(0.0f, amplitude)/15.0f / fmaxf(ref,0.01f);
    const float first_span = fmaxf(0.0f, span - 0.20f*outer);
    float sum = 0.0f, before = 0.0f, after = 0.0f;
    for (int i = 0; i < 7; ++i) {
        sum += weights[layout-1][i];
        if (i < index) before += weights[layout-1][i];
        if (i <= index) after += weights[layout-1][i];
    }
    if (index == 7) {
        *cell_start = start + first_span;
        *cell_span = outer;
    } else {
        const float left = start + first_span * before/sum;
        const float right = index == 6 ? start + first_span :
                            start + first_span * after/sum;
        *cell_start = left;
        *cell_span = right-left;
    }
}

/* Reparameterizing a curve changes when it is traversed, NOT its shape.
 * Shorten its flat root connectors to 4% so the outer giant curl genuinely
 * appears in the first 20% of its axis instead of showing only a straight
 * connector, as it would with the previous 20%-long root. */
void proto_fermat_diagonal_point(float p,float cell_start,float cell_span,
                                 float progress,float winding,float direction,
                                 float *x,float *y) {
    p = fmaxf(0.0f, fminf(1.0f, p));
    const float q = p < 0.04f ? 5.0f*p :
                    p > 0.96f ? 0.80f+5.0f*(p-0.96f) :
                    0.20f+(p-0.04f)*(.60f/.92f);
    proto_fermat_twist_point(q,cell_start,cell_span,.5f*cell_span,
                             progress,winding,direction,x,y);
}

/* A genuine 0.4s stagger: each shape completes its own 5-second cycle,
 * with phase and direction offset. The first eight-element wave ends at
 * 5 + 7*0.4 = 7.8s. Earlier cells may start a new wave meanwhile. */
float proto_fermat_offset_progress(float time,float speed,int index,float delay) {
    if (speed == 0.0f) return 1.0f;
    const float offset = fmaxf(0.0f,delay)*(float)index;
    if (time <= offset) return 0.0f;
    return proto_fermat_twist_progress(time-offset,speed);
}

/* Patch 0033: horizontal reference: 120px visible per side, +30% offset.
 * Endpoints are +/-156px; 45px of the final 45px cell extend from 111 to
 * 156px, of which 9/45 = 20% precede the visible edge at 120px. */
void proto_fermat_horizontal_cell(float start, float span, int layout, int index,
                                  float *cell_start, float *cell_span) {
    static const float weights[4][7] = {
        {3.0f, 4.8f, 7.6f, 12.2f, 19.5f, 31.2f, 50.0f},
        {6.0f, 6.9f, 8.9f, 12.4f, 18.4f, 28.7f, 46.9f},
        {8.4f, 8.8f, 10.1f, 12.7f, 17.6f, 26.6f, 44.0f},
        {11.6f, 11.7f, 12.2f, 13.5f, 16.7f, 23.5f, 39.2f}
    };
    if (layout < 1 || layout > 4) layout = 3;
    if (index < 0) index = 0;
    if (index > 7) index = 7;
    const float outer = span * (0.30f / 1.30f) / 0.80f; /* 45px at 156px. */
    const float inner = span - outer;                 /* 111px at 156px. */
    if (index == 7) {
        *cell_start = start + inner;
        *cell_span = outer;
        return;
    }
    float sum = 0.0f, before = 0.0f, after = 0.0f;
    for (int i = 0; i < 7; ++i) {
        const float w = weights[layout - 1][i];
        sum += w;
        if (i < index) before += w;
        if (i <= index) after += w;
    }
    const float left = start + inner * before / sum;
    const float right = index == 6 ? start + inner :
                        start + inner * after / sum;
    *cell_start = left;
    *cell_span = right - left;
}

/* Round two-arm curl: radius is the SAME on X and Y inside the core.
 * The final coil has a full 80px transverse envelope, but its straight
 * anchors still occupy only a 45px local cell. No elliptical stretch.
 * Four-percent parameter connectors preserve exact zero-height joins. */
/* Patch 0035: the outer circle must not reach back into cell 6.
 * A circular 53.33px-high coil has an actual radial scale larger than half
 * the 45px anchor cell. Move its CENTER, not its x/y relative scale, toward
 * the outside just enough for centre - animated_radius >= cell_start.
 * This holds at every development fraction and both winding directions. */
void proto_fermat_horizontal_point(float p, float cell_start, float cell_span,
                                    int index, float amplitude, float growth,
                                    float winding, float direction,
                                    float *x, float *y) {
    p = fmaxf(0.0f, fminf(1.0f, p));
    growth = fmaxf(0.0f, fminf(1.0f, growth));
    if (growth <= 0.00001f) {
        *x = cell_start + p * cell_span;
        *y = 0.0f;
        return;
    }
    const float radius = index == 7 ?
        ((float)PROTO_HEIGHT / 6.0f / proto_fermat_tall_reference_peak(7)) *
        fmaxf(0.0f, amplitude) / 15.0f :
        0.5f * cell_span * fmaxf(0.0f, amplitude) / 15.0f;
    if (radius <= 0.00001f) {
        *x = cell_start + p * cell_span;
        *y = 0.0f;
        return;
    }
    const float extent = radius * powf(growth, 0.58f);
    const float original_centre = cell_start + 0.5f * cell_span;
    /* By shifting only at the same rate as expansion, the limiting left
     * extent is always >= cell_start, without introducing an x/y ellipse. */
    const float centre = original_centre +
        fmaxf(0.0f, radius - 0.5f * cell_span) * powf(growth, 0.58f);

    /* Retain the generous smoothed connectors and rounded Fermat profile. */
    const float q = p < 0.04f ? 5.0f * p :
                    p > 0.96f ? 0.80f + 5.0f * (p - 0.96f) :
                    0.20f + (p - 0.04f) * (0.60f / 0.92f);
    if (q <= 0.20f) {
        const float t = q / 0.20f;
        const float smooth = t * t * (3.0f - 2.0f * t);
        *x = cell_start + (centre - extent - cell_start) * smooth;
        *y = 0.0f;
    } else if (q >= 0.80f) {
        const float t = (q - 0.80f) / 0.20f;
        const float smooth = t * t * (3.0f - 2.0f * t);
        *x = centre + extent +
             (cell_start + cell_span - centre - extent) * smooth;
        *y = 0.0f;
    } else {
        proto_fermat_twist_point(q, centre - radius, 2.0f * radius,
                                 radius, growth, winding, direction, x, y);
    }
}

/* Each individual 1.6s beat runs 0->1->0, 0.8s on each half.
 * Pass age - index*0.3f to start the eight cells progressively. */
float proto_fermat_burst_progress(float age) {
    /* Account for float rounding at 3.7f - 7*0.3f (1.5999999f). */
    if (!(age >= 0.0f && age < 1.6f - 1.0e-6f)) return -1.0f;
    const float u = age < 0.8f ? age / 0.8f : (1.6f - age) / 0.8f;
    const float t = fmaxf(0.0f, fminf(1.0f, u));
    const float t2 = t * t;
    const float t4 = t2 * t2;
    return t4 * (35.0f + t * (-84.0f + t * (70.0f - 20.0f * t)));
}


/* Patch 0041: exactly one timing source, the length of emitted audio PCM.
 * Fractions reproduce the former 10ms stagger + 150ms cell for T=220ms.
 * The last cell ends exactly when the corresponding audio clip ends. */
float proto_button_wave_delay(float clip_duration) {
    if (!isfinite(clip_duration) || clip_duration <= 0.0f) return 0.0f;
    return clip_duration / 22.0f;
}

float proto_button_wave_cell_duration(float clip_duration) {
    if (!isfinite(clip_duration) || clip_duration <= 0.0f) return 0.0f;
    return clip_duration - 7.0f*proto_button_wave_delay(clip_duration);
}

float proto_fermat_audio_progress(float age, float clip_duration) {
    const float cell = proto_button_wave_cell_duration(clip_duration);
    if (!(cell > 0.0f) || !isfinite(age)) return -1.0f;
    const float eps = fmaxf(0.00000001f, cell*0.00001f);
    if (!(age >= 0.0f && age < cell-eps)) return -1.0f;
    const float half = 0.5f*cell;
    const float u = age <= half ? age/half : (cell-age)/half;
    const float t = fmaxf(0.0f, fminf(1.0f, u));
    const float t2 = t*t;
    const float t4 = t2*t2;
    return t4*(35.0f+t*(-84.0f+t*(70.0f-20.0f*t)));
}

/* Patch 0046: match the perceived angle-opening speed of Fermat curls
 * to the linear envelope already used by Metroid teeth during X/Y. The
 * spiral builder applies progress^1.18 to its winding angle. The reciprocal
 * cancels that exponent without changing the fully deployed geometry. */
float proto_fermat_visual_sync_progress(float progress) {
    if (!isfinite(progress)) return 0.0f;
    progress=fminf(1.0f,fmaxf(0.0f,progress));
    return powf(progress,1.0f/1.18f);
}

void proto_parametric_prepare(ProtoFrame *frame) {
    memset(frame->cross_count, 0, sizeof frame->cross_count);
    for (int i = 0; i < PROTO_PROFILE_SAMPLES - 1; ++i) {
        float x0 = frame->x_coords[i], x1 = frame->x_coords[i+1];
        float y0 = frame->offsets[i], y1 = frame->offsets[i+1];
        if (fabsf(x1 - x0) < 1.0e-6f) continue;
        float lo = fminf(x0,x1), hi = fmaxf(x0,x1);
        int begin = (int)ceilf((lo + RANGE) / X_STEP - 0.5f);
        int end = (int)ceilf((hi + RANGE) / X_STEP - 0.5f);
        if (begin < 0) begin = 0;
        if (end > PROTO_CROSS_COLS) end = PROTO_CROSS_COLS;
        for (int col = begin; col < end; ++col) {
            float px = -RANGE + (col + 0.5f) * X_STEP;
            float py = y0 + (px - x0) * (y1 - y0) / (x1 - x0);
            unsigned n = frame->cross_count[col];
            if (n >= PROTO_CROSS_MAX) continue;
            while (n > 0 && frame->cross_y[col][n-1] > py) {
                frame->cross_y[col][n] = frame->cross_y[col][n-1];
                --n;
            }
            frame->cross_y[col][n] = py;
            ++frame->cross_count[col];
        }
    }
}

float proto_parametric_side(const ProtoFrame *frame, float tangent, float normal) {
    int col = (int)floorf((tangent + RANGE) / X_STEP);
    if (col < 0) col = 0;
    if (col >= PROTO_CROSS_COLS) col = PROTO_CROSS_COLS - 1;
    unsigned count = frame->cross_count[col];
    float nearest = 1.0e6f;
    unsigned crossings = 0;
    for (unsigned i = 0; i < count; ++i) {
        float difference = normal - frame->cross_y[col][i];
        float distance = fabsf(difference);
        if (distance < nearest) nearest = distance;
        if (difference >= 0.0f) ++crossings;
    }
    /* Top of the local frame starts violet. One crossing flips to cyan. */
    return crossings & 1u ? nearest : -nearest;
}
