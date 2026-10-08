#include "core/sha1.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint32_t state[5];
    uint64_t bit_count;
    uint8_t buffer[64];
    size_t buffered;
} Sha1Context;

static uint32_t rotate_left(uint32_t value, unsigned int count)
{
    return (value << count) | (value >> (32u - count));
}

static void transform(Sha1Context *ctx, const uint8_t block[64])
{
    uint32_t words[80];
    uint32_t a, b, c, d, e;
    unsigned int i;

    for (i = 0; i < 16; ++i) {
        words[i] = ((uint32_t)block[i * 4] << 24) |
                   ((uint32_t)block[i * 4 + 1] << 16) |
                   ((uint32_t)block[i * 4 + 2] << 8) |
                   (uint32_t)block[i * 4 + 3];
    }
    for (i = 16; i < 80; ++i)
        words[i] = rotate_left(words[i - 3] ^ words[i - 8] ^ words[i - 14] ^ words[i - 16], 1);

    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2];
    d = ctx->state[3]; e = ctx->state[4];
    for (i = 0; i < 80; ++i) {
        uint32_t f;
        uint32_t k;
        uint32_t temp;
        if (i < 20) { f = (b & c) | ((~b) & d); k = 0x5A827999u; }
        else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1u; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCu; }
        else { f = b ^ c ^ d; k = 0xCA62C1D6u; }
        temp = rotate_left(a, 5) + f + e + k + words[i];
        e = d; d = c; c = rotate_left(b, 30); b = a; a = temp;
    }
    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c;
    ctx->state[3] += d; ctx->state[4] += e;
}

static void sha1_init(Sha1Context *ctx)
{
    ctx->state[0] = 0x67452301u; ctx->state[1] = 0xEFCDAB89u;
    ctx->state[2] = 0x98BADCFEu; ctx->state[3] = 0x10325476u;
    ctx->state[4] = 0xC3D2E1F0u; ctx->bit_count = 0; ctx->buffered = 0;
}

static void sha1_update(Sha1Context *ctx, const uint8_t *data, size_t length)
{
    ctx->bit_count += (uint64_t)length * 8u;
    while (length > 0) {
        size_t room = sizeof(ctx->buffer) - ctx->buffered;
        size_t count = length < room ? length : room;
        memcpy(ctx->buffer + ctx->buffered, data, count);
        ctx->buffered += count; data += count; length -= count;
        if (ctx->buffered == sizeof(ctx->buffer)) {
            transform(ctx, ctx->buffer);
            ctx->buffered = 0;
        }
    }
}

static void sha1_final(Sha1Context *ctx, uint8_t digest[20])
{
    uint64_t bits = ctx->bit_count;
    uint8_t pad[72] = {0x80};
    uint8_t encoded[8];
    size_t pad_length = ctx->buffered < 56 ? 56 - ctx->buffered : 120 - ctx->buffered;
    unsigned int i;

    for (i = 0; i < 8; ++i)
        encoded[7 - i] = (uint8_t)(bits >> (i * 8));
    sha1_update(ctx, pad, pad_length);
    sha1_update(ctx, encoded, sizeof(encoded));
    for (i = 0; i < 20; ++i)
        digest[i] = (uint8_t)(ctx->state[i / 4] >> (24 - (i % 4) * 8));
}

bool sha1_file(const char *path, uint8_t digest[20], char *error, size_t error_size)
{
    uint8_t buffer[16384];
    Sha1Context context;
    FILE *file = fopen(path, "rb");
    size_t count;

    if (file == NULL) {
        if (error_size > 0) snprintf(error, error_size, "%s", strerror(errno));
        return false;
    }
    sha1_init(&context);
    while ((count = fread(buffer, 1, sizeof(buffer), file)) > 0)
        sha1_update(&context, buffer, count);
    if (ferror(file)) {
        if (error_size > 0) snprintf(error, error_size, "lecture impossible: %s", strerror(errno));
        fclose(file);
        return false;
    }
    fclose(file);
    sha1_final(&context, digest);
    return true;
}

void sha1_hex(const uint8_t digest[20], char output[41])
{
    static const char digits[] = "0123456789abcdef";
    size_t i;
    for (i = 0; i < 20; ++i) {
        output[i * 2] = digits[digest[i] >> 4];
        output[i * 2 + 1] = digits[digest[i] & 15];
    }
    output[40] = '\0';
}
