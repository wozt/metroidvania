/* SPDX-License-Identifier: GPL-3.0-only */
#include "metroidvania_split.h"
#include "mzm_sound_ids.h"
#include "rotating_line/parametric.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void check_defaults(const ProtoConfig *config) {
    assert(config->section_count == 2);
    assert(config->rotation_speed == 10.0f);
    assert(config->line_width == 0.0f && !config->show_divider);
    assert(config->sections[0].formula == PROTO_METROID_HORIZONTAL);
    assert(config->sections[0].cycles == 8.0f);
    assert(config->sections[0].amplitude == 30.0f);
    assert(config->sections[0].phase_speed == 0.2f);
    assert(config->sections[0].stagger_delay == 0.4f);
    assert(config->sections[1].formula == PROTO_FERMAT_HORIZONTAL);
    assert(config->sections[1].cycles == 8.0f);
    assert(config->sections[1].amplitude == 15.0f);
    assert(config->sections[1].spiral_layout == 1);
    assert(config->sections[1].phase_speed == 0.2f);
    assert(config->sections[1].stagger_delay == 0.4f);
}

int main(void) {
    assert(mv_split_native_sound(true) == 0x081);
    assert(mv_split_native_sound(false) ==
           MZM_SOUND_RUINS_TEST_SHOOTING_LIGHTNING);
    assert(mv_split_angle(0) == 0 && mv_split_angle(540) == 0x4000 &&
           mv_split_angle(1080) == 0x8000 && mv_split_angle(2160) == 0);

    static MvSplit split;
    char error[256];
    assert(mv_split_init(&split, NULL, error, sizeof error));
    check_defaults(&split.config);
    assert(split.frame.parametric);
    assert(mv_split_pixel(&split, 120, 10) == MV_GAME_ARIA);
    assert(mv_split_pixel(&split, 120, 150) == MV_GAME_MZM);
    assert(mv_split_top(mv_split_rotation_angle(&split)) == MV_GAME_ARIA);

    /* Patch 0046: the first four teeth are clean bipolar triangles. There
     * is deliberately no 0047/0048 per-cell polarity mirror. */
    for (int tooth = 0; tooth < 4; ++tooth) {
        assert(fabsf(proto_metroid_bipolar_value(tooth, 0.0f)) < 0.00001f);
        assert(fabsf(proto_metroid_bipolar_value(tooth, 0.25f) - 1.0f) < 0.00001f);
        assert(fabsf(proto_metroid_bipolar_value(tooth, 0.50f)) < 0.00001f);
        assert(fabsf(proto_metroid_bipolar_value(tooth, 0.75f) + 1.0f) < 0.00001f);
        assert(fabsf(proto_metroid_bipolar_value(tooth, 1.0f)) < 0.00001f);
    }
    float synchronized = proto_fermat_visual_sync_progress(0.5f);
    assert(fabsf(powf(synchronized, 1.18f) - 0.5f) < 0.0001f);

    static uint8_t aria[MV_SCREEN_W * MV_SCREEN_H * 4];
    static uint8_t mzm[MV_SCREEN_W * MV_SCREEN_H * 4];
    static uint8_t out[MV_SCREEN_W * MV_SCREEN_H * 4];
    memset(aria, 0xAA, sizeof aria);
    memset(mzm, 0x55, sizeof mzm);
    mv_split_compose(&split, aria, mzm, out);
    for (int y = 0; y < MV_SCREEN_H; y += 7)
        for (int x = 0; x < MV_SCREEN_W; x += 5)
            assert(out[(y * MV_SCREEN_W + x) * 4] ==
                   (mv_split_pixel(&split, x, y) == MV_GAME_ARIA ? 0xAA : 0x55));

    for (int frame = 0; frame < 540; ++frame)
        mv_split_tick(&split, PROTO_FIXED_TIMESTEP, true);
    uint16_t quarter = mv_split_rotation_angle(&split);
    assert(quarter >= 0x3FF0 && quarter <= 0x4010);
    assert(mv_split_top(quarter) == MV_GAME_MZM);

    mv_split_trigger_pulse(&split, false, 0.22f);
    assert(split.animation.metroid_wave_age == 0.0f);
    assert(split.animation.metroid_wave_duration == 0.22f);
    mv_split_trigger_pulse(&split, true, 0.22f);
    assert(split.animation.aria_wave_age == 0.0f);
    mv_split_tick(&split, PROTO_FIXED_TIMESTEP, true);
    assert(split.animation.metroid_wave_age > 0.0f);
    assert(split.animation.aria_wave_age > 0.0f);

    static MvSplit ambient, pulsed;
    assert(mv_split_init(&ambient, NULL, error, sizeof error));
    assert(mv_split_init(&pulsed, NULL, error, sizeof error));
    for (int frame = 0; frame < 60; ++frame) {
        mv_split_tick(&ambient, PROTO_FIXED_TIMESTEP, true);
        mv_split_tick(&pulsed, PROTO_FIXED_TIMESTEP, true);
    }
    mv_split_trigger_pulse(&pulsed, false, 0.22f);
    mv_split_trigger_pulse(&pulsed, true, 0.22f);
    for (int frame = 0; frame < 6; ++frame) {
        mv_split_tick(&ambient, PROTO_FIXED_TIMESTEP, true);
        mv_split_tick(&pulsed, PROTO_FIXED_TIMESTEP, true);
    }
    bool metroid_changed = false, aria_changed = false;
    for (int index = 0; index < PROTO_PROFILE_SAMPLES / 2; ++index)
        if (fabsf(ambient.frame.offsets[index] - pulsed.frame.offsets[index]) >
            0.001f)
            metroid_changed = true;
    for (int index = PROTO_PROFILE_SAMPLES / 2;
         index < PROTO_PROFILE_SAMPLES; ++index)
        if (fabsf(ambient.frame.offsets[index] - pulsed.frame.offsets[index]) >
                0.001f ||
            fabsf(ambient.frame.x_coords[index] - pulsed.frame.x_coords[index]) >
                0.001f)
            aria_changed = true;
    assert(metroid_changed && aria_changed);

    MvSplitHold hold = {0};
    assert(mv_split_hold_press(&hold));
    assert(!mv_split_hold_press(&hold));
    unsigned pulse_total = 0;
    for (int frame = 0; frame < 120; ++frame) {
        unsigned pulses;
        assert(!mv_split_hold_tick(&hold, PROTO_FIXED_TIMESTEP, &pulses));
        pulse_total += pulses;
    }
    assert(pulse_total >= 3 && !hold.launched);
    /* Each held beat climbs one semitone from six below the source pitch,
     * saturating at +12; releasing the key resets the progression. */
    assert(mv_split_pulse_semitones(0) == -6);
    assert(mv_split_pulse_semitones(1) == -5);
    assert(mv_split_pulse_semitones(2) == -4);
    assert(mv_split_pulse_semitones(17) == 11);
    assert(mv_split_pulse_semitones(18) == 12);
    assert(mv_split_pulse_semitones(100) == 12);
    hold.pulse_index = 4;
    mv_split_hold_release(&hold);
    assert(!hold.held && hold.elapsed == 0.0f && hold.pulse_index == 0);
    assert(mv_split_hold_press(&hold));
    assert(hold.pulse_index == 0);
    bool launched = false;
    for (int frame = 0; frame < 182; ++frame) {
        unsigned pulses;
        launched |= mv_split_hold_tick(&hold, PROTO_FIXED_TIMESTEP, &pulses);
    }
    assert(launched && hold.launched);
    unsigned pulses;
    assert(!mv_split_hold_tick(&hold, PROTO_FIXED_TIMESTEP, &pulses));

    static MvSplit overridden;
    assert(mv_split_init(&overridden, MV_SPLIT_SAMPLE_CONFIG,
                         error, sizeof error));
    check_defaults(&overridden.config);

    puts("metroidvania_split: ok");
    return 0;
}
