/* SPDX-License-Identifier: GPL-3.0-only */
#include "rotating_line/parametric.h"
#include "rotating_line/pulse_audio.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static void check_clip(bool aria) {
    float duration = proto_pulse_audio_emit(NULL, aria);
    float delay = proto_button_wave_delay(duration);
    float cell = proto_button_wave_cell_duration(duration);
    assert(isfinite(duration) && duration > 0.0f);
    assert(fabsf(cell + 7.0f * delay - duration) < 0.000002f);
    assert(fabsf(proto_fermat_audio_progress(0.5f * cell, duration) - 1.0f) <
           0.0001f);
    for (int index = 0; index < 8; ++index) {
        float start = index * delay;
        assert(proto_fermat_audio_progress(-0.0005f, duration) < 0.0f);
        assert(fabsf(proto_fermat_audio_progress(0.0f, duration)) < 0.00001f);
        assert(fabsf(proto_fermat_audio_progress(0.5f * cell, duration) - 1.0f) <
               0.0001f);
        if (index == 7) assert(fabsf(start + cell - duration) < 0.000002f);
        assert(proto_fermat_audio_progress(cell - 0.001f, duration) >= 0.0f);
        assert(proto_fermat_audio_progress(cell + 0.001f, duration) < 0.0f);
    }
}

int main(void) {
    check_clip(true);
    check_clip(false);
    puts("metroidvania_pulse_audio: ok");
    return 0;
}
