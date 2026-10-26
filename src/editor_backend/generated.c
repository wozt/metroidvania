/* SPDX-License-Identifier: GPL-3.0-only */
#define _DEFAULT_SOURCE
#include "generated.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

static bool allowed_part(const char *part, size_t length) {
    if (!length || (length == 1 && part[0] == '.') ||
        (length == 2 && part[0] == '.' && part[1] == '.'))
        return false;
    for (size_t index = 0; index < length; ++index) {
        char c = part[index];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '.' || c == '-'))
            return false;
    }
    return true;
}

bool feb_generated_path(const char *repository, const char *relative, char *path,
                        FebError *error) {
    if (!relative[0] || relative[0] == '/' || strchr(relative, '\\'))
        return feb_fail(error, FEB_EXIT_OPERATION, "unsafe extracted path: '%s'", relative);
    for (const char *part = relative; *part;) {
        const char *slash = strchr(part, '/');
        size_t length = slash ? (size_t)(slash - part) : strlen(part);
        /* Path() collapses repeated separators; an empty part only ends it. */
        if (length && !allowed_part(part, length))
            return feb_fail(error, FEB_EXIT_OPERATION, "unsafe extracted path: '%s'", relative);
        part += length + (slash ? 1 : 0);
    }
    char current[FEB_PATH_CAPACITY];
    const char *const fixed[] = {"", "/assets", "/assets/extracted"};
    for (size_t index = 0; index < 3; ++index) {
        snprintf(current, sizeof current, "%s%s", repository, fixed[index]);
        if (feb_is_symlink(current))
            return feb_fail(error, FEB_EXIT_OPERATION,
                            "symlink extraction destination refused: %s", current);
    }
    if (snprintf(path, FEB_PATH_CAPACITY, "%s/assets/extracted/%s", repository, relative) >=
        FEB_PATH_CAPACITY)
        return feb_fail(error, FEB_EXIT_OPERATION, "extracted path is too long");
    size_t base = strlen(repository) + strlen("/assets/extracted");
    for (const char *slash = path + base + 1; slash;) {
        const char *next = strchr(slash, '/');
        size_t end = next ? (size_t)(next - path) : strlen(path);
        snprintf(current, sizeof current, "%.*s", (int)end, path);
        if (feb_is_symlink(current))
            return feb_fail(error, FEB_EXIT_OPERATION,
                            "symlink extraction destination refused: %s", current);
        slash = next ? next + 1 : NULL;
    }
    return true;
}

bool feb_write_generated(const char *repository, const char *relative, const uint8_t *data,
                         size_t size, bool *written, FebError *error) {
    char path[FEB_PATH_CAPACITY], temp[FEB_PATH_CAPACITY + 8];
    if (written) *written = false;
    if (!feb_generated_path(repository, relative, path, error)) return false;
    long long existing = 0;
    if (feb_is_regular(path, &existing) && (size_t)existing == size) {
        char *current = NULL;
        size_t current_size = 0;
        bool same = feb_read_text(path, size + 1, &current, &current_size) &&
                    current_size == size && !memcmp(current, data, size);
        free(current);
        if (same) return true;
    }
    char parent[FEB_PATH_CAPACITY];
    snprintf(parent, sizeof parent, "%s", path);
    for (char *slash = parent + 1; (slash = strchr(slash, '/')) != NULL; ++slash) {
        *slash = '\0';
        mkdir(parent, 0777);
        *slash = '/';
    }
    snprintf(temp, sizeof temp, "%s.tmp", path);
    if (feb_is_symlink(temp))
        return feb_fail(error, FEB_EXIT_OPERATION, "symlink extraction temp refused: %s", temp);
    FILE *file = fopen(temp, "wb");
    bool ok = file && fwrite(data, 1, size, file) == size;
    if (file) ok = fclose(file) == 0 && ok;
    if (ok) ok = rename(temp, path) == 0;
    if (!ok) {
        unlink(temp);
        return feb_fail(error, FEB_EXIT_OPERATION, "cannot write %s", path);
    }
    if (written) *written = true;
    return true;
}

static void put_u16(uint8_t *out, uint32_t value) {
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t *out, uint32_t value) {
    put_u16(out, value);
    put_u16(out + 2, value >> 16);
}

bool feb_bmp24(int64_t width, int64_t height, const uint8_t *rgb, uint8_t **bmp, size_t *size,
               FebError *error) {
    if (width <= 0 || width > 4096 || height <= 0 || height > 4096)
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid RGB bitmap");
    size_t pitch = ((size_t)width * 3 + 3) & ~(size_t)3;
    size_t pixels = pitch * (size_t)height;
    *size = 54 + pixels;
    *bmp = calloc(1, *size);
    if (!*bmp) return feb_oom(error);
    uint8_t *out = *bmp;
    out[0] = 'B';
    out[1] = 'M';
    put_u32(out + 2, (uint32_t)*size);
    put_u32(out + 10, 54);
    put_u32(out + 14, 40);
    put_u32(out + 18, (uint32_t)width);
    put_u32(out + 22, (uint32_t)height);
    put_u16(out + 26, 1);
    put_u16(out + 28, 24);
    put_u32(out + 34, (uint32_t)pixels);
    for (int64_t y = 0; y < height; ++y) {
        uint8_t *row = out + 54 + (size_t)(height - y - 1) * pitch;
        for (int64_t x = 0; x < width; ++x) {
            const uint8_t *source = rgb + (size_t)(y * width + x) * 3;
            row[x * 3] = source[2];
            row[x * 3 + 1] = source[1];
            row[x * 3 + 2] = source[0];
        }
    }
    return true;
}

static void put_be32(uint8_t *out, uint32_t value) {
    out[0] = (uint8_t)(value >> 24);
    out[1] = (uint8_t)(value >> 16);
    out[2] = (uint8_t)(value >> 8);
    out[3] = (uint8_t)value;
}

bool feb_png_rgba(int64_t width, int64_t height, const uint8_t *pixels, uint8_t **png,
                  size_t *size) {
    return feb_png_rgba_level(width, height, pixels, 9, png, size);
}

bool feb_png_rgba_level(int64_t width, int64_t height, const uint8_t *pixels, int level,
                        uint8_t **png, size_t *size) {
    size_t row = (size_t)width * 4, raw_size = (size_t)height * (row + 1);
    uint8_t *raw = malloc(raw_size);
    if (!raw) return false;
    for (int64_t y = 0; y < height; ++y) {
        raw[y * (row + 1)] = 0;
        memcpy(raw + y * (row + 1) + 1, pixels + y * row, row);
    }
    uLongf compressed_size = compressBound(raw_size);
    uint8_t *compressed = malloc(compressed_size);
    if (!compressed || compress2(compressed, &compressed_size, raw, raw_size, level) != Z_OK) {
        free(compressed);
        free(raw);
        return false;
    }
    free(raw);
    *size = 8 + (12 + 13) + (12 + compressed_size) + 12;
    *png = malloc(*size);
    if (!*png) {
        free(compressed);
        return false;
    }
    uint8_t *out = *png, header[13] = {0, 0, 0, 0, 0, 0, 0, 0, 8, 6, 0, 0, 0};
    put_be32(header, (uint32_t)width);
    put_be32(header + 4, (uint32_t)height);
    memcpy(out, "\x89PNG\r\n\x1a\n", 8);
    size_t used = 8;
    const struct {
        const char *tag;
        const uint8_t *body;
        size_t length;
    } chunks[3] = {{"IHDR", header, 13}, {"IDAT", compressed, compressed_size}, {"IEND", NULL, 0}};
    for (int chunk = 0; chunk < 3; ++chunk) {
        put_be32(out + used, (uint32_t)chunks[chunk].length);
        memcpy(out + used + 4, chunks[chunk].tag, 4);
        if (chunks[chunk].length) memcpy(out + used + 8, chunks[chunk].body, chunks[chunk].length);
        uLong crc = crc32(0, out + used + 4, (uInt)(4 + chunks[chunk].length));
        put_be32(out + used + 8 + chunks[chunk].length, (uint32_t)crc);
        used += 12 + chunks[chunk].length;
    }
    free(compressed);
    return true;
}
