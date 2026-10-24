/* SPDX-License-Identifier: GPL-3.0-only */
#include "fusion/assets/mzm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { GBA_ROM_BASE = 0x08000000 };

static void set_error(char *error, size_t error_size, const char *message) {
    if (!error || error_size == 0) return;
    snprintf(error, error_size, "%s", message);
    error[error_size - 1] = '\0';
}

static bool pointer_offset(const FusionRom *rom, uint32_t pointer, size_t size,
                           size_t *offset) {
    if (pointer < GBA_ROM_BASE) return false;
    size_t value = (size_t)(pointer - GBA_ROM_BASE);
    if (value > rom->size || size > rom->size - value) return false;
    *offset = value;
    return true;
}

static bool read_count(const uint8_t *data, size_t size, size_t *position,
                       uint8_t count_bytes, uint16_t *count) {
    if (*position > size || count_bytes > size - *position) return false;
    if (count_bytes == 1) {
        *count = data[(*position)++];
    } else {
        *count = (uint16_t)data[*position] << 8 | data[*position + 1];
        *position += 2;
    }
    return true;
}

static bool decode_pass(const uint8_t *data, size_t size, size_t *position,
                        uint8_t *output, size_t total,
                        char *error, size_t error_size) {
    if (*position >= size) {
        set_error(error, error_size, "missing Zero Mission room RLE pass");
        return false;
    }
    uint8_t count_bytes = data[(*position)++];
    if (count_bytes != 1 && count_bytes != 2) {
        set_error(error, error_size, "invalid Zero Mission room RLE pass width");
        return false;
    }
    uint16_t encoded_bit = count_bytes == 1 ? 0x80 : 0x8000;
    size_t written = 0;
    for (;;) {
        uint16_t raw_count;
        if (!read_count(data, size, position, count_bytes, &raw_count)) {
            set_error(error, error_size, "truncated Zero Mission room RLE count");
            return false;
        }
        if (raw_count == 0) break;
        bool repeated = (raw_count & encoded_bit) != 0;
        size_t count = raw_count & (encoded_bit - 1);
        if (count == 0 || count > total - written) {
            set_error(error, error_size, "Zero Mission room RLE pass overflows its map");
            return false;
        }
        if (repeated) {
            if (*position >= size) {
                set_error(error, error_size,
                          "truncated Zero Mission room RLE repeat");
                return false;
            }
            memset(output + written, data[(*position)++], count);
        } else {
            if (*position > size || count > size - *position) {
                set_error(error, error_size,
                          "truncated Zero Mission room RLE literals");
                return false;
            }
            memcpy(output + written, data + *position, count);
            *position += count;
        }
        written += count;
    }
    if (written != total) {
        set_error(error, error_size,
                  "Zero Mission room RLE pass does not fill its map");
        return false;
    }
    return true;
}

void fusion_mzm_free_room_rle(FusionMzmRoomRleMap *map) {
    if (!map) return;
    free(map->cells);
    *map = (FusionMzmRoomRleMap){0};
}

bool fusion_mzm_read_room_rle(const FusionRom *rom, uint32_t pointer,
                              FusionMzmRoomRleMap *map,
                              char *error, size_t error_size) {
    if (!rom || !rom->data || rom->game != FUSION_ASSET_GAME_METROID || !map) {
        set_error(error, error_size, "invalid Zero Mission room RLE request");
        return false;
    }
    *map = (FusionMzmRoomRleMap){0};
    size_t offset;
    if (!pointer_offset(rom, pointer, 6, &offset)) {
        set_error(error, error_size, "Zero Mission room RLE points outside the ROM");
        return false;
    }
    const uint8_t *data = rom->data + offset;
    size_t size = rom->size - offset;
    size_t total = (size_t)data[0] * data[1];
    if (data[0] == 0 || data[1] == 0 ||
        total > FUSION_MZM_ROOM_RLE_CELL_CAPACITY) {
        set_error(error, error_size,
                  "invalid Zero Mission room RLE dimensions");
        return false;
    }
    uint8_t *halves = malloc(total * 2);
    uint16_t *cells = malloc(total * sizeof *cells);
    if (!halves || !cells) {
        free(halves);
        free(cells);
        set_error(error, error_size, "cannot allocate Zero Mission room RLE map");
        return false;
    }
    size_t position = 2;
    if (!decode_pass(data, size, &position, halves, total, error, error_size) ||
        !decode_pass(data, size, &position, halves + total, total,
                     error, error_size)) {
        free(halves);
        free(cells);
        return false;
    }
    for (size_t index = 0; index < total; ++index)
        cells[index] = (uint16_t)halves[index] |
                       (uint16_t)halves[total + index] << 8;
    free(halves);
    *map = (FusionMzmRoomRleMap){
        pointer, data[0], data[1], cells, total, position
    };
    if (error && error_size > 0) error[0] = '\0';
    return true;
}
