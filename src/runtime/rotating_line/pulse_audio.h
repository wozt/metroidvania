/* SPDX-License-Identifier: MIT */
#ifndef PROTO_PULSE_AUDIO_H
#define PROTO_PULSE_AUDIO_H
#include <stdbool.h>
#include <SDL3/SDL.h>
/* SDL3 queued floating-point PCM; the emitter reports exact PCM duration. */
SDL_AudioStream *proto_pulse_audio_open(void);
/* Return emitted PCM duration (frames / Hz) even without an audio device. */
float proto_pulse_audio_emit(SDL_AudioStream *audio, bool aria);
#endif
