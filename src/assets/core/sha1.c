/* SPDX-License-Identifier: GPL-3.0-only */
#include "sha1.h"

#include <string.h>

typedef struct {
    uint32_t h[5];
    uint64_t bits;
    uint8_t block[64];
    size_t used;
} FusionSha1;

static uint32_t rotate_left(uint32_t value, unsigned count) {
    return (value << count) | (value >> (32u - count));
}

static uint32_t load_be32(const uint8_t *bytes) {
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8) | bytes[3];
}

static void store_be32(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)(value >> 24);
    bytes[1] = (uint8_t)(value >> 16);
    bytes[2] = (uint8_t)(value >> 8);
    bytes[3] = (uint8_t)value;
}

static void transform(FusionSha1 *state, const uint8_t block[64]) {
    uint32_t words[80];
    for (unsigned i = 0; i < 16; ++i) words[i] = load_be32(block + i * 4u);
    for (unsigned i = 16; i < 80; ++i)
        words[i] = rotate_left(words[i - 3] ^ words[i - 8] ^ words[i - 14] ^ words[i - 16], 1);

    uint32_t a = state->h[0], b = state->h[1], c = state->h[2];
    uint32_t d = state->h[3], e = state->h[4];
    for (unsigned i = 0; i < 80; ++i) {
        uint32_t function, constant;
        if (i < 20) {
            function = (b & c) | ((~b) & d);
            constant = 0x5A827999u;
        } else if (i < 40) {
            function = b ^ c ^ d;
            constant = 0x6ED9EBA1u;
        } else if (i < 60) {
            function = (b & c) | (b & d) | (c & d);
            constant = 0x8F1BBCDCu;
        } else {
            function = b ^ c ^ d;
            constant = 0xCA62C1D6u;
        }
        uint32_t next = rotate_left(a, 5) + function + e + constant + words[i];
        e = d;
        d = c;
        c = rotate_left(b, 30);
        b = a;
        a = next;
    }
    state->h[0] += a;
    state->h[1] += b;
    state->h[2] += c;
    state->h[3] += d;
    state->h[4] += e;
}

static void update(FusionSha1 *state, const uint8_t *data, size_t size) {
    state->bits += (uint64_t)size * 8u;
    while (size > 0) {
        size_t count = 64u - state->used;
        if (count > size) count = size;
        memcpy(state->block + state->used, data, count);
        state->used += count;
        data += count;
        size -= count;
        if (state->used == 64u) {
            transform(state, state->block);
            state->used = 0;
        }
    }
}

void fusion_sha1_digest(const uint8_t *data, size_t size, uint8_t digest[20]) {
    FusionSha1 state = {{0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u,
                         0xC3D2E1F0u}, 0, {0}, 0};
    update(&state, data, size);
    state.block[state.used++] = 0x80;
    if (state.used > 56u) {
        memset(state.block + state.used, 0, 64u - state.used);
        transform(&state, state.block);
        state.used = 0;
    }
    memset(state.block + state.used, 0, 56u - state.used);
    for (unsigned i = 0; i < 8; ++i)
        state.block[56u + i] = (uint8_t)(state.bits >> (56u - 8u * i));
    transform(&state, state.block);
    for (unsigned i = 0; i < 5; ++i) store_be32(digest + i * 4u, state.h[i]);
    memset(&state, 0, sizeof state);
}
