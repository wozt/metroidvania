/* SPDX-License-Identifier: GPL-3.0-only */
#include "mzm_sfx.h"

#include <stddef.h>

static MzmSfxSink g_sink;
static void *g_context;
static MzmSfxRequest g_log[MZM_SFX_LOG];
static int g_count;

void mzm_sfx_set_sink(MzmSfxSink sink, void *context) {
    g_sink = sink;
    g_context = context;
}

static void request(MzmSfxAction action, uint16_t sound, uint16_t timer) {
    if (g_count < MZM_SFX_LOG) g_log[g_count++] = (MzmSfxRequest){action, sound, timer};
    if (g_sink) g_sink(g_context, action, sound, timer);
}

void mzm_sfx_play(uint16_t sound) { request(MZM_SFX_PLAY, sound, 0); }
void mzm_sfx_play_not_playing(uint16_t sound) { request(MZM_SFX_PLAY_NOT_PLAYING, sound, 0); }
void mzm_sfx_stop(uint16_t sound) { request(MZM_SFX_STOP, sound, 0); }
void mzm_sfx_fade(uint16_t sound, uint16_t timer) { request(MZM_SFX_FADE, sound, timer); }
void mzm_sfx_stop_all(void) { request(MZM_SFX_STOP_ALL, 0, 0); }

void mzm_sfx_clear(void) { g_count = 0; }
int mzm_sfx_count(void) { return g_count; }

MzmSfxRequest mzm_sfx_get(int index) {
    return index >= 0 && index < g_count ? g_log[index] : (MzmSfxRequest){MZM_SFX_PLAY, 0, 0};
}

int mzm_sfx_requested(MzmSfxAction action, uint16_t sound) {
    for (int i = 0; i < g_count; ++i)
        if (g_log[i].action == action && g_log[i].sound == sound) return 1;
    return 0;
}
