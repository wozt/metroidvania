/* SPDX-License-Identifier: MIT */
/* Original synthesized placeholders: Aria double heart pulse, Metroid spark. */
#include "pulse_audio.h"
#include <math.h>
#include <stdint.h>

#define PULSE_AUDIO_HZ 48000
#define PULSE_PI 3.14159265358979323846f
#define PULSE_DURATION 0.22f

SDL_AudioStream *proto_pulse_audio_open(void) {
    const SDL_AudioSpec spec = { SDL_AUDIO_F32, 1, PULSE_AUDIO_HZ };
    SDL_AudioStream *stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (stream && !SDL_ResumeAudioStreamDevice(stream)) {
        SDL_DestroyAudioStream(stream);
        return NULL;
    }
    return stream;
}

float proto_pulse_audio_emit(SDL_AudioStream *audio, bool aria) {
    const int count = (int)(PULSE_DURATION * PULSE_AUDIO_HZ);
    const float duration = (float)count / (float)PULSE_AUDIO_HZ;
    /* The length of the actual generated PCM, not a visual constant. */
    if (!audio) return duration;
    float samples[(int)(PULSE_DURATION * PULSE_AUDIO_HZ)];
    uint32_t seed = 0x0b19c001u;
    float last_noise = 0.0f;
    for (int i = 0; i < count; ++i) {
        const float t = (float)i / PULSE_AUDIO_HZ;
        const float attack = fminf(1.0f, t / 0.006f);
        const float tail = fminf(1.0f, (duration-t) / 0.014f);
        float sample;
        if (aria) {
            const float beat1 = expf(-34.0f*t)*sinf(2.0f*PULSE_PI*(65.0f*t-14.0f*t*t));
            const float second_t = fmaxf(0.0f, t-0.103f);
            const float beat2 = t >= .103f ?
                0.72f*expf(-39.0f*second_t)*
                sinf(2.0f*PULSE_PI*(78.0f*second_t-10.0f*second_t*second_t)) : 0.0f;
            sample = 0.28f*(beat1+beat2);
        } else {
            seed ^= seed<<13;
            seed ^= seed>>17;
            seed ^= seed<<5;
            const float noise = ((float)(seed & 65535) / 32767.5f) - 1.0f;
            const float highpass = noise - 0.62f*last_noise;
            last_noise = noise;
            const float buzz = sinf(2.0f*PULSE_PI*(810.0f*t+1900.0f*t*t));
            sample = 0.19f*expf(-17.0f*t)*(highpass*0.85f+buzz*0.35f);
        }
        samples[i] = fmaxf(-0.8f, fminf(0.8f, sample*attack*tail));
    }
    (void)SDL_PutAudioStreamData(audio, samples, count*(int)sizeof(float));
    return duration;
}
