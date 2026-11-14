/* SPDX-License-Identifier: GPL-3.0-only */
/* WAV recording of the Aria mixer output (unsigned 8-bit mono).
 *
 * The header is written with a zero data length up front and patched in
 * place by aria_wav_close once the byte count is known. */
#ifndef ARIA_WAV_H
#define ARIA_WAV_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    FILE *file;
    uint32_t bytes;
} AriaWav;

bool aria_wav_open(AriaWav *wav, const char *path);
void aria_wav_write(AriaWav *wav, const int8_t *pcm, size_t count);
void aria_wav_close(AriaWav *wav);

#endif
