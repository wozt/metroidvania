/* SPDX-License-Identifier: GPL-3.0-only */
#include "aria_wav.h"

#include "aos/core/aos_mp2k.h" /* AOS_MP2K_MIX_RATE */

static void wav_header(FILE *file, uint32_t bytes) {
    uint8_t header[44] = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ',
                          16, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 8, 0,
                          'd', 'a', 't', 'a', 0, 0, 0, 0};
    uint32_t riff = 36 + bytes, rate = AOS_MP2K_MIX_RATE;
    for (int i = 0; i < 4; ++i) {
        header[4 + i] = (uint8_t)(riff >> (8 * i));
        header[24 + i] = (uint8_t)(rate >> (8 * i));
        header[28 + i] = (uint8_t)(rate >> (8 * i));
        header[40 + i] = (uint8_t)(bytes >> (8 * i));
    }
    fwrite(header, 1, sizeof header, file);
}

bool aria_wav_open(AriaWav *wav, const char *path) {
    wav->file = fopen(path, "wb");
    if (!wav->file) return false;
    wav_header(wav->file, 0);
    return true;
}

void aria_wav_write(AriaWav *wav, const int8_t *pcm, size_t count) {
    if (!wav->file) return;
    for (size_t i = 0; i < count; ++i) fputc((uint8_t)(pcm[i] + 128), wav->file);
    wav->bytes += (uint32_t)count;
}

void aria_wav_close(AriaWav *wav) {
    if (!wav->file) return;
    fseek(wav->file, 0, SEEK_SET);
    wav_header(wav->file, wav->bytes);
    fclose(wav->file);
    wav->file = NULL;
}
