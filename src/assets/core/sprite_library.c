/* SPDX-License-Identifier: GPL-3.0-only */
#define _POSIX_C_SOURCE 200809L

#include "sprite_library.h"

#include "fusion/assets/rom.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#else
#include <dirent.h>
#endif

static void set_error(char *error, size_t error_size, const char *message) {
    if (!error || error_size == 0) return;
    snprintf(error, error_size, "%s", message);
    error[error_size - 1] = '\0';
}

static bool reserve(char **text, size_t *capacity, size_t needed) {
    if (needed <= *capacity) return true;
    size_t next = *capacity ? *capacity : 4096;
    while (next < needed) {
        if (next > SIZE_MAX / 2) return false;
        next *= 2;
    }
    char *grown = realloc(*text, next);
    if (!grown) return false;
    *text = grown;
    *capacity = next;
    return true;
}

static bool append_row(FusionSpriteLibrary *library, const char *row) {
    size_t length = strlen(row);
    if (!reserve(&library->rows, &library->rows_capacity,
                 library->rows_size + length + 1)) return false;
    memcpy(library->rows + library->rows_size, row, length + 1);
    library->rows_size += length;
    return true;
}

bool fusion_sprite_library_init(FusionSpriteLibrary *library,
                                const char *output, const char *index_prefix,
                                char *error, size_t error_size) {
    if (!library || !output || !index_prefix || strlen(index_prefix) >= 256) {
        set_error(error, error_size, "invalid sprite-library request");
        return false;
    }
    *library = (FusionSpriteLibrary){0};
    if (strlen(output) >= sizeof library->output ||
        !fusion_export_join(library->objects, output, "objects")) {
        set_error(error, error_size, "sprite-library path is too long");
        return false;
    }
    snprintf(library->output, sizeof library->output, "%s", output);
    snprintf(library->index_prefix, sizeof library->index_prefix, "%s",
             index_prefix);
    if (!fusion_export_ensure_directory(output, error, error_size) ||
        !fusion_export_ensure_directory(library->objects, error, error_size) ||
        !append_row(library, "schema\tmetroidvania-sprite-index-v1\n")) {
        if (!error || error[0] == '\0')
            set_error(error, error_size, "cannot initialize sprite library");
        fusion_sprite_library_free(library);
        return false;
    }
    return true;
}

bool fusion_sprite_library_begin(FusionSpriteLibrary *library,
                                 char *error, size_t error_size) {
    if (!library || !library->rows) {
        set_error(error, error_size, "sprite library is not initialized");
        return false;
    }
    ++library->sequences;
    return true;
}

static bool remember_object(FusionSpriteLibrary *library, const char *digest,
                            size_t size, bool *fresh) {
    for (size_t index = 0; index < library->packed_count; ++index) {
        if (!strcmp(library->packed[index].digest, digest)) {
            *fresh = false;
            return library->packed[index].size == size;
        }
    }
    if (library->packed_count == library->packed_capacity) {
        size_t next = library->packed_capacity ? library->packed_capacity * 2 : 64;
        FusionSpriteObject *grown =
            realloc(library->packed, next * sizeof *grown);
        if (!grown) return false;
        library->packed = grown;
        library->packed_capacity = next;
    }
    FusionSpriteObject *object = &library->packed[library->packed_count++];
    snprintf(object->digest, sizeof object->digest, "%s", digest);
    object->size = size;
    *fresh = true;
    return true;
}

bool fusion_sprite_library_add(FusionSpriteLibrary *library, const char *key,
                               size_t frame, unsigned ticks, int offset_x,
                               int offset_y, const uint8_t *bmp, size_t bmp_size,
                               char *error, size_t error_size) {
    if (!library || !key || !*key || strlen(key) > 159 || strchr(key, '\t') ||
        strchr(key, '\r') || strchr(key, '\n') || ticks < 1 || ticks > 255 ||
        offset_x < -256 || offset_x > 256 || offset_y < -256 || offset_y > 256 ||
        !bmp || bmp_size < 2 || bmp[0] != 'B' || bmp[1] != 'M') {
        set_error(error, error_size, "invalid sprite-library frame");
        return false;
    }
    char digest[65];
    if (!fusion_sha256_hex(bmp, bmp_size, digest)) {
        set_error(error, error_size, "cannot hash sprite-library frame");
        return false;
    }
    bool fresh = false;
    if (!remember_object(library, digest, bmp_size, &fresh)) {
        set_error(error, error_size, "cannot record sprite-library object");
        return false;
    }
    char filename[80], path[FUSION_EXPORT_PATH_CAPACITY];
    snprintf(filename, sizeof filename, "%s.bmp", digest);
    if (!fusion_export_join(path, library->objects, filename) ||
        (fresh && !fusion_export_write_file(path, bmp, bmp_size, error, error_size)))
        return false;
    char row[768];
    int written = snprintf(row, sizeof row,
                           "%s\t%zu\t%u\t%d\t%d\t%s/objects/%s\n",
                           key, frame, ticks, offset_x, offset_y,
                           library->index_prefix, filename);
    if (written < 0 || (size_t)written >= sizeof row || !append_row(library, row)) {
        set_error(error, error_size, "cannot append sprite-library index");
        return false;
    }
    ++library->frames;
    return true;
}

static bool is_object_name(const char *name) {
    if (strlen(name) != 68 || strcmp(name + 64, ".bmp")) return false;
    for (size_t index = 0; index < 64; ++index)
        if (!isxdigit((unsigned char)name[index])) return false;
    return true;
}

static bool object_is_used(const FusionSpriteLibrary *library, const char *name) {
    for (size_t index = 0; index < library->packed_count; ++index)
        if (!strncmp(name, library->packed[index].digest, 64)) return true;
    return false;
}

static bool prune_objects(FusionSpriteLibrary *library, size_t *removed,
                          char *error, size_t error_size) {
#ifdef _WIN32
    char pattern[FUSION_EXPORT_PATH_CAPACITY];
    if (!fusion_export_join(pattern, library->objects, "*.bmp")) {
        set_error(error, error_size, "sprite-library path is too long");
        return false;
    }
    struct _finddata_t entry;
    intptr_t search = _findfirst(pattern, &entry);
    *removed = 0;
    if (search == -1) return true;
    do {
        if (!is_object_name(entry.name) || object_is_used(library, entry.name))
            continue;
        char path[FUSION_EXPORT_PATH_CAPACITY];
        if (!fusion_export_join(path, library->objects, entry.name) ||
            remove(path) != 0) {
            _findclose(search);
            set_error(error, error_size, "cannot prune sprite-library object");
            return false;
        }
        ++*removed;
    } while (_findnext(search, &entry) == 0);
    _findclose(search);
    return true;
#else
    DIR *directory = opendir(library->objects);
    if (!directory) {
        set_error(error, error_size, "cannot inspect sprite-library objects");
        return false;
    }
    *removed = 0;
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        if (!is_object_name(entry->d_name) || object_is_used(library, entry->d_name))
            continue;
        char path[FUSION_EXPORT_PATH_CAPACITY];
        if (!fusion_export_join(path, library->objects, entry->d_name) ||
            remove(path) != 0) {
            closedir(directory);
            set_error(error, error_size, "cannot prune sprite-library object");
            return false;
        }
        ++*removed;
    }
    closedir(directory);
    return true;
#endif
}

bool fusion_sprite_library_finish(FusionSpriteLibrary *library,
                                  FusionSpriteLibrarySummary *summary,
                                  char *error, size_t error_size) {
    if (!library || !summary || !library->rows) {
        set_error(error, error_size, "invalid sprite-library completion");
        return false;
    }
    size_t removed = 0;
    char path[FUSION_EXPORT_PATH_CAPACITY];
    if (!prune_objects(library, &removed, error, error_size) ||
        !fusion_export_join(path, library->output, "runtime_index.tsv") ||
        !fusion_export_write_file(path, (const uint8_t *)library->rows,
                                  library->rows_size, error, error_size))
        return false;
    *summary = (FusionSpriteLibrarySummary){
        library->sequences, library->frames, library->packed_count, removed};
    return true;
}

void fusion_sprite_library_free(FusionSpriteLibrary *library) {
    if (!library) return;
    free(library->rows);
    free(library->packed);
    *library = (FusionSpriteLibrary){0};
}

static void write_u32(uint8_t *bytes, size_t offset, uint32_t value) {
    bytes[offset] = (uint8_t)value;
    bytes[offset + 1] = (uint8_t)(value >> 8);
    bytes[offset + 2] = (uint8_t)(value >> 16);
    bytes[offset + 3] = (uint8_t)(value >> 24);
}

bool fusion_sprite_bmp32_crop(const uint8_t *rgba, size_t width, size_t height,
                              int origin_x, int origin_y, uint8_t **bmp,
                              size_t *bmp_size, int *left, int *top,
                              char *error, size_t error_size) {
    if (!rgba || !bmp || !bmp_size || !left || !top || width < 1 || height < 1 ||
        width > 1024 || height > 1024) {
        set_error(error, error_size, "invalid sprite bitmap request");
        return false;
    }
    size_t min_x = width, min_y = height, max_x = 0, max_y = 0;
    bool visible = false;
    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            if (rgba[(y * width + x) * 4 + 3] == 0) continue;
            if (x < min_x) min_x = x;
            if (y < min_y) min_y = y;
            if (x > max_x) max_x = x;
            if (y > max_y) max_y = y;
            visible = true;
        }
    }
    if (!visible) {
        set_error(error, error_size, "sprite frame has no visible pixels");
        return false;
    }
    size_t cropped_width = max_x - min_x + 1;
    size_t cropped_height = max_y - min_y + 1;
    if (cropped_width > 512 || cropped_height > 512) {
        set_error(error, error_size, "sprite frame exceeds 512 pixels");
        return false;
    }
    size_t data_size = cropped_width * cropped_height * 4;
    *bmp_size = 122 + data_size;
    *bmp = calloc(1, *bmp_size);
    if (!*bmp) {
        set_error(error, error_size, "cannot allocate sprite bitmap");
        return false;
    }
    uint8_t *output = *bmp;
    output[0] = 'B'; output[1] = 'M';
    write_u32(output, 2, (uint32_t)*bmp_size);
    write_u32(output, 10, 122);
    write_u32(output, 14, 108);
    write_u32(output, 18, (uint32_t)cropped_width);
    write_u32(output, 22, (uint32_t)cropped_height);
    output[26] = 1; output[28] = 32; output[30] = 3;
    write_u32(output, 34, (uint32_t)data_size);
    write_u32(output, 54, 0x00FF0000);
    write_u32(output, 58, 0x0000FF00);
    write_u32(output, 62, 0x000000FF);
    write_u32(output, 66, 0xFF000000);
    write_u32(output, 70, 0x73524742);
    for (size_t y = min_y; y <= max_y; ++y) {
        for (size_t x = min_x; x <= max_x; ++x) {
            const uint8_t *source = rgba + (y * width + x) * 4;
            if (!source[3]) continue;
            size_t row = max_y - y;
            uint8_t *target = output + 122 +
                (row * cropped_width + (x - min_x)) * 4;
            target[0] = source[2]; target[1] = source[1];
            target[2] = source[0]; target[3] = 255;
        }
    }
    *left = (int)min_x - origin_x;
    *top = (int)min_y - origin_y;
    return true;
}

bool fusion_sprite_bmp8_crop(const uint8_t *indices, size_t width, size_t height,
                             int origin_x, int origin_y, const uint8_t *colors,
                             size_t color_count, uint8_t **bmp, size_t *bmp_size,
                             int *left, int *top, char *error, size_t error_size) {
    if (!indices || !colors || !bmp || !bmp_size || !left || !top || width < 1 ||
        height < 1 || width > 1024 || height > 1024 || color_count < 1 ||
        color_count > 256) {
        set_error(error, error_size, "invalid indexed sprite bitmap request");
        return false;
    }
    size_t min_x = width, min_y = height, max_x = 0, max_y = 0;
    bool visible = false;
    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            uint8_t index = indices[y * width + x];
            if (!index) continue;
            if (index >= color_count) {
                set_error(error, error_size,
                          "palette index outside the preview palette");
                return false;
            }
            if (x < min_x) min_x = x;
            if (y < min_y) min_y = y;
            if (x > max_x) max_x = x;
            if (y > max_y) max_y = y;
            visible = true;
        }
    }
    if (!visible) {
        set_error(error, error_size, "sprite frame has no visible pixels");
        return false;
    }
    size_t cropped_width = max_x - min_x + 1;
    size_t cropped_height = max_y - min_y + 1;
    if (cropped_width > 512 || cropped_height > 512) {
        set_error(error, error_size, "sprite frame exceeds 512 pixels");
        return false;
    }
    size_t pitch = (cropped_width + 3) & ~(size_t)3;
    size_t data_size = pitch * cropped_height;
    size_t data_offset = 14 + 40 + 1024;
    *bmp_size = data_offset + data_size;
    *bmp = calloc(1, *bmp_size);
    if (!*bmp) {
        set_error(error, error_size, "cannot allocate sprite bitmap");
        return false;
    }
    uint8_t *output = *bmp;
    output[0] = 'B'; output[1] = 'M';
    write_u32(output, 2, (uint32_t)*bmp_size);
    write_u32(output, 10, (uint32_t)data_offset);
    write_u32(output, 14, 40);
    write_u32(output, 18, (uint32_t)cropped_width);
    write_u32(output, 22, (uint32_t)cropped_height);
    output[26] = 1; output[28] = 8;
    write_u32(output, 34, (uint32_t)data_size);
    write_u32(output, 38, 2835);
    write_u32(output, 42, 2835);
    write_u32(output, 46, 256);
    for (size_t index = 0; index < color_count; ++index) {
        output[54 + index * 4] = colors[index * 3 + 2];
        output[54 + index * 4 + 1] = colors[index * 3 + 1];
        output[54 + index * 4 + 2] = colors[index * 3];
    }
    for (size_t y = min_y; y <= max_y; ++y) {
        uint8_t *row = output + data_offset + (max_y - y) * pitch;
        for (size_t x = min_x; x <= max_x; ++x)
            row[x - min_x] = indices[y * width + x];
    }
    *left = (int)min_x - origin_x;
    *top = (int)min_y - origin_y;
    return true;
}
