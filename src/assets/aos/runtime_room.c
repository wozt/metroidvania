/* SPDX-License-Identifier: GPL-3.0-only */
#include "fusion/assets/aos.h"

#include "fusion/assets/gba.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    GBA_ROM_BASE = 0x08000000,
    AOS_BACKGROUND_METADATA_SIZE = 16,
    AOS_METADATA_OUTPUT_CAPACITY = 256 * 1024,
    AOS_GRAPHICS_BLOCK_SIZE = 0x800,
    AOS_PALETTE_ROW_SIZE = 0x20,
    AOS_PALETTE_ROW_COUNT = 16
};

typedef struct {
    const uint8_t *data;
    size_t size;
    uint8_t *owned;
} MetadataPayload;

static void set_error(char *error, size_t error_size, const char *message) {
    if (!error || error_size == 0) return;
    snprintf(error, error_size, "%s", message);
    error[error_size - 1] = '\0';
}

static uint16_t read_u16(const uint8_t *data) {
    return (uint16_t)data[0] | (uint16_t)data[1] << 8;
}

static uint32_t read_u32(const uint8_t *data) {
    return (uint32_t)data[0] | (uint32_t)data[1] << 8 |
           (uint32_t)data[2] << 16 | (uint32_t)data[3] << 24;
}

static bool pointer_offset(const FusionRom *rom, uint32_t pointer, size_t size,
                           size_t *offset) {
    if (pointer < GBA_ROM_BASE) return false;
    size_t value = (size_t)(pointer - GBA_ROM_BASE);
    if (value > rom->size || size > rom->size - value) return false;
    *offset = value;
    return true;
}

static void free_payload(MetadataPayload *payload) {
    free(payload->owned);
    *payload = (MetadataPayload){0};
}

static bool read_metadata_payload(const FusionRom *rom, uint32_t pointer,
                                  bool compressed, MetadataPayload *payload,
                                  char *error, size_t error_size) {
    *payload = (MetadataPayload){0};
    size_t offset;
    if (!pointer_offset(rom, pointer, compressed ? 4 : 1, &offset)) {
        set_error(error, error_size,
                  "Aria background payload points outside the ROM");
        return false;
    }
    if (!compressed) {
        payload->data = rom->data + offset;
        payload->size = rom->size - offset;
        return true;
    }
    uint32_t expanded_size = fusion_gba_uncompressed_size(
        rom->data + offset, rom->size - offset);
    if (expanded_size == 0 || expanded_size > AOS_METADATA_OUTPUT_CAPACITY) {
        set_error(error, error_size,
                  "Aria background payload has an invalid LZ77 size");
        return false;
    }
    payload->owned = malloc(expanded_size);
    if (!payload->owned) {
        set_error(error, error_size,
                  "cannot allocate Aria background metadata payload");
        return false;
    }
    size_t written = 0;
    if (!fusion_gba_lz77_uncompress(rom->data + offset, rom->size - offset,
                                    payload->owned, expanded_size, &written) ||
        written < 4 || payload->owned[0] != 0) {
        free_payload(payload);
        set_error(error, error_size,
                  "Aria background payload has invalid expanded metadata");
        return false;
    }
    payload->data = payload->owned + 4;
    payload->size = written - 4;
    return true;
}

static bool read_resource_payload(const FusionRom *rom, uint32_t pointer,
                                  MetadataPayload *payload,
                                  char *error, size_t error_size) {
    *payload = (MetadataPayload){0};
    size_t offset;
    if (!pointer_offset(rom, pointer, 8, &offset)) {
        set_error(error, error_size,
                  "Aria graphics resource points outside the ROM");
        return false;
    }
    uint8_t encoding = rom->data[offset];
    if (encoding == 0) {
        payload->data = rom->data + offset + 4;
        payload->size = rom->size - offset - 4;
        return true;
    }
    if (encoding != 1) {
        set_error(error, error_size,
                  "Aria graphics resource uses an unsupported encoding");
        return false;
    }
    uint32_t compressed_pointer = read_u32(rom->data + offset + 4);
    size_t compressed_offset;
    if (!pointer_offset(rom, compressed_pointer, 4, &compressed_offset)) {
        set_error(error, error_size,
                  "Aria compressed graphics resource points outside the ROM");
        return false;
    }
    uint32_t expanded_size = fusion_gba_uncompressed_size(
        rom->data + compressed_offset, rom->size - compressed_offset);
    if (expanded_size == 0 || expanded_size > AOS_METADATA_OUTPUT_CAPACITY) {
        set_error(error, error_size,
                  "Aria graphics resource has an invalid LZ77 size");
        return false;
    }
    payload->owned = malloc(expanded_size);
    if (!payload->owned) {
        set_error(error, error_size,
                  "cannot allocate Aria graphics resource payload");
        return false;
    }
    size_t written = 0;
    if (!fusion_gba_lz77_uncompress(rom->data + compressed_offset,
                                    rom->size - compressed_offset,
                                    payload->owned, expanded_size, &written) ||
        written < 4 || payload->owned[0] != 0) {
        free_payload(payload);
        set_error(error, error_size,
                  "Aria graphics resource has invalid expanded data");
        return false;
    }
    payload->data = payload->owned + 4;
    payload->size = written - 4;
    return true;
}

bool fusion_aos_load_video_memory(const FusionRom *rom,
                                  const FusionAosRoomDescriptor *descriptor,
                                  FusionAosVideoMemory *memory,
                                  char *error, size_t error_size) {
    if (!rom || !rom->data || rom->game != FUSION_ASSET_GAME_ARIA ||
        !descriptor || !memory) {
        set_error(error, error_size, "invalid Aria video-memory request");
        return false;
    }
    *memory = (FusionAosVideoMemory){0};
    FusionAosLoadList graphics;
    if (!fusion_aos_read_load_list(rom, descriptor->graphics_pointer,
                                   &graphics, error, error_size)) {
        return false;
    }
    for (size_t index = 0; index < graphics.count; ++index) {
        const FusionAosLoadEntry *entry = &graphics.entries[index];
        MetadataPayload payload;
        if (!read_resource_payload(rom, entry->resource_pointer, &payload,
                                   error, error_size)) {
            return false;
        }
        size_t destination = (size_t)entry->parameters[0] * AOS_GRAPHICS_BLOCK_SIZE;
        size_t source = (size_t)entry->parameters[1] * AOS_GRAPHICS_BLOCK_SIZE;
        size_t length = (size_t)entry->parameters[2] * AOS_GRAPHICS_BLOCK_SIZE;
        if (source > payload.size || length > payload.size - source ||
            destination > sizeof memory->vram ||
            length > sizeof memory->vram - destination) {
            free_payload(&payload);
            set_error(error, error_size,
                      "Aria graphics load exceeds video-memory bounds");
            return false;
        }
        memcpy(memory->vram + destination, payload.data + source, length);
        free_payload(&payload);
    }
    memory->graphics_load_count = graphics.count;

    FusionAosLoadList palettes;
    if (!fusion_aos_read_load_list(rom, descriptor->palette_pointer,
                                   &palettes, error, error_size)) {
        return false;
    }
    for (size_t index = 0; index < palettes.count; ++index) {
        const FusionAosLoadEntry *entry = &palettes.entries[index];
        size_t offset;
        if (!pointer_offset(rom, entry->resource_pointer, 4, &offset)) {
            set_error(error, error_size,
                      "Aria palette resource points outside the ROM");
            return false;
        }
        if (rom->data[offset] != 0) {
            set_error(error, error_size,
                      "Aria palette resource uses an unsupported encoding");
            return false;
        }
        size_t destination_row = entry->parameters[0];
        size_t source_row = entry->parameters[1];
        size_t row_count = entry->parameters[2];
        size_t available_rows = (size_t)rom->data[offset + 2] + 1;
        if (source_row > available_rows || row_count > available_rows - source_row ||
            destination_row > AOS_PALETTE_ROW_COUNT ||
            row_count > AOS_PALETTE_ROW_COUNT - destination_row) {
            set_error(error, error_size,
                      "Aria palette load exceeds palette-memory bounds");
            return false;
        }
        size_t source = offset + 4 + source_row * AOS_PALETTE_ROW_SIZE;
        size_t length = row_count * AOS_PALETTE_ROW_SIZE;
        if (source > rom->size || length > rom->size - source) {
            set_error(error, error_size, "Aria palette resource is truncated");
            return false;
        }
        memcpy(memory->palette + destination_row * AOS_PALETTE_ROW_SIZE,
               rom->data + source, length);
    }
    memory->palette_load_count = palettes.count;
    if (error && error_size > 0) error[0] = '\0';
    return true;
}

void fusion_aos_free_background_tilemap(FusionAosBackgroundTilemap *tilemap) {
    if (!tilemap) return;
    free(tilemap->cells);
    *tilemap = (FusionAosBackgroundTilemap){0};
}

bool fusion_aos_read_background_tiles(const FusionRom *rom,
                                      const FusionAosBackground *background,
                                      FusionAosBackgroundTilemap *tilemap,
                                      char *error, size_t error_size) {
    if (!rom || !rom->data || rom->game != FUSION_ASSET_GAME_ARIA ||
        !background || !tilemap) {
        set_error(error, error_size, "invalid Aria background-tile request");
        return false;
    }
    *tilemap = (FusionAosBackgroundTilemap){0};
    tilemap->metadata_pointer = background->metadata_pointer;
    if (background->metadata_pointer == 0) {
        tilemap->status = FUSION_AOS_TILEMAP_ABSENT;
        if (error && error_size > 0) error[0] = '\0';
        return true;
    }

    size_t metadata_offset;
    if (!pointer_offset(rom, background->metadata_pointer,
                        AOS_BACKGROUND_METADATA_SIZE, &metadata_offset)) {
        set_error(error, error_size,
                  "Aria background metadata points outside the ROM");
        return false;
    }
    const uint8_t *metadata = rom->data + metadata_offset;
    tilemap->width_screens = metadata[0];
    tilemap->height_screens = metadata[1];
    tilemap->flags = read_u16(metadata + 2);
    tilemap->blocks_pointer = read_u32(metadata + 4);
    tilemap->map_pointer = read_u32(metadata + 12);
    if (tilemap->width_screens == 0 || tilemap->height_screens == 0 ||
        tilemap->width_screens > FUSION_AOS_BACKGROUND_SCREEN_LIMIT ||
        tilemap->height_screens > FUSION_AOS_BACKGROUND_SCREEN_LIMIT) {
        set_error(error, error_size,
                  "Aria background dimensions exceed the safety bounds");
        return false;
    }
    if (tilemap->flags & 1) {
        tilemap->status = FUSION_AOS_TILEMAP_UNSUPPORTED_AFFINE;
        if (error && error_size > 0) error[0] = '\0';
        return true;
    }

    size_t map_count = (size_t)tilemap->width_screens *
                       tilemap->height_screens * 64;
    size_t map_offset;
    if (!pointer_offset(rom, tilemap->map_pointer,
                        map_count * sizeof(uint16_t), &map_offset)) {
        set_error(error, error_size,
                  "Aria background block map points outside the ROM");
        return false;
    }
    const uint8_t *block_map = rom->data + map_offset;
    for (size_t index = 0; index < map_count; ++index) {
        uint16_t block = read_u16(block_map + index * 2) & 0x3FFF;
        if (block > tilemap->maximum_block) tilemap->maximum_block = block;
    }

    MetadataPayload blocks;
    if (!read_metadata_payload(rom, tilemap->blocks_pointer,
                               (tilemap->flags & 2) != 0,
                               &blocks, error, error_size)) {
        return false;
    }
    size_t required_blocks = (size_t)tilemap->maximum_block * 16 * 2;
    if (required_blocks > blocks.size) {
        free_payload(&blocks);
        set_error(error, error_size,
                  "Aria background block table is truncated");
        return false;
    }

    tilemap->width_tiles = (size_t)tilemap->width_screens * 32;
    tilemap->height_tiles = (size_t)tilemap->height_screens * 32;
    tilemap->cell_count = tilemap->width_tiles * tilemap->height_tiles;
    if (tilemap->cell_count > FUSION_AOS_COLLISION_CELL_CAPACITY) {
        free_payload(&blocks);
        set_error(error, error_size,
                  "Aria background tilemap exceeds the safety cap");
        return false;
    }
    tilemap->cells = calloc(tilemap->cell_count, sizeof *tilemap->cells);
    if (!tilemap->cells) {
        free_payload(&blocks);
        set_error(error, error_size,
                  "cannot allocate Aria background tilemap");
        return false;
    }
    for (size_t tile_y = 0; tile_y < tilemap->height_tiles; ++tile_y) {
        size_t block_y = tile_y / 4;
        size_t initial_local_y = tile_y % 4;
        for (size_t tile_x = 0; tile_x < tilemap->width_tiles; ++tile_x) {
            size_t block_x = tile_x / 4;
            size_t local_x = tile_x % 4;
            size_t local_y = initial_local_y;
            uint16_t map_value = read_u16(
                block_map + (block_x + block_y *
                             tilemap->width_screens * 8) * 2);
            uint16_t block = map_value & 0x3FFF;
            if (block == 0) continue;
            if (map_value & 0x4000) local_x = 3 - local_x;
            if (map_value & 0x8000) local_y = 3 - local_y;
            size_t cell = (size_t)(block - 1) * 16 + local_x + local_y * 4;
            uint16_t word = read_u16(blocks.data + cell * 2);
            word ^= (uint16_t)(((map_value >> 12) & 0xC) << 8);
            tilemap->cells[tile_x + tile_y * tilemap->width_tiles] = word;
        }
    }
    free_payload(&blocks);
    tilemap->status = FUSION_AOS_TILEMAP_DECODED;
    if (error && error_size > 0) error[0] = '\0';
    return true;
}

void fusion_aos_free_collision_map(FusionAosCollisionMap *collision) {
    if (!collision) return;
    free(collision->cells);
    *collision = (FusionAosCollisionMap){0};
}

bool fusion_aos_read_background_collision(const FusionRom *rom,
                                          const FusionAosBackground *background,
                                          FusionAosCollisionMap *collision,
                                          char *error, size_t error_size) {
    if (!rom || !rom->data || rom->game != FUSION_ASSET_GAME_ARIA ||
        !background || !collision) {
        set_error(error, error_size,
                  "invalid Aria background-collision request");
        return false;
    }
    *collision = (FusionAosCollisionMap){0};
    collision->metadata_pointer = background->metadata_pointer;
    if (background->metadata_pointer == 0) {
        collision->status = FUSION_AOS_COLLISION_ABSENT;
        if (error && error_size > 0) error[0] = '\0';
        return true;
    }

    size_t metadata_offset;
    if (!pointer_offset(rom, background->metadata_pointer,
                        AOS_BACKGROUND_METADATA_SIZE, &metadata_offset)) {
        set_error(error, error_size,
                  "Aria background metadata points outside the ROM");
        return false;
    }
    const uint8_t *metadata = rom->data + metadata_offset;
    collision->width_screens = metadata[0];
    collision->height_screens = metadata[1];
    collision->flags = read_u16(metadata + 2);
    collision->blocks_pointer = read_u32(metadata + 4);
    collision->collision_pointer = read_u32(metadata + 8);
    collision->map_pointer = read_u32(metadata + 12);
    if (collision->width_screens == 0 || collision->height_screens == 0 ||
        collision->width_screens > FUSION_AOS_BACKGROUND_SCREEN_LIMIT ||
        collision->height_screens > FUSION_AOS_BACKGROUND_SCREEN_LIMIT) {
        set_error(error, error_size,
                  "Aria background dimensions exceed the safety bounds");
        return false;
    }
    if (collision->flags & 1) {
        collision->status = FUSION_AOS_COLLISION_UNSUPPORTED_AFFINE;
        if (error && error_size > 0) error[0] = '\0';
        return true;
    }

    size_t map_count = (size_t)collision->width_screens *
                       collision->height_screens * 64;
    size_t map_offset;
    if (!pointer_offset(rom, collision->map_pointer,
                        map_count * sizeof(uint16_t), &map_offset)) {
        set_error(error, error_size,
                  "Aria background block map points outside the ROM");
        return false;
    }
    const uint8_t *block_map = rom->data + map_offset;
    for (size_t index = 0; index < map_count; ++index) {
        uint16_t block = read_u16(block_map + index * 2) & 0x3FFF;
        if (block > collision->maximum_block)
            collision->maximum_block = block;
    }

    bool compressed = (collision->flags & 2) != 0;
    MetadataPayload blocks;
    if (!read_metadata_payload(rom, collision->blocks_pointer, compressed,
                               &blocks, error, error_size)) {
        return false;
    }
    size_t required_blocks = (size_t)collision->maximum_block * 16 * 2;
    if (required_blocks > blocks.size) {
        free_payload(&blocks);
        set_error(error, error_size,
                  "Aria background block table is truncated");
        return false;
    }
    free_payload(&blocks);
    if (collision->collision_pointer == 0) {
        collision->status = FUSION_AOS_COLLISION_NONE;
        if (error && error_size > 0) error[0] = '\0';
        return true;
    }

    MetadataPayload values;
    if (!read_metadata_payload(rom, collision->collision_pointer, compressed,
                               &values, error, error_size)) {
        return false;
    }
    size_t required_values = (size_t)collision->maximum_block * 16;
    if (required_values > values.size) {
        free_payload(&values);
        set_error(error, error_size,
                  "Aria background collision table is truncated");
        return false;
    }

    collision->width_tiles = (size_t)collision->width_screens * 32;
    collision->height_tiles = (size_t)collision->height_screens * 32;
    collision->cell_count = collision->width_tiles * collision->height_tiles;
    if (collision->cell_count > FUSION_AOS_COLLISION_CELL_CAPACITY) {
        free_payload(&values);
        set_error(error, error_size,
                  "Aria background collision map exceeds the safety cap");
        return false;
    }
    collision->cells = calloc(collision->cell_count, 1);
    if (!collision->cells) {
        free_payload(&values);
        set_error(error, error_size,
                  "cannot allocate Aria background collision map");
        return false;
    }
    for (size_t tile_y = 0; tile_y < collision->height_tiles; ++tile_y) {
        size_t block_y = tile_y / 4;
        size_t initial_local_y = tile_y % 4;
        for (size_t tile_x = 0; tile_x < collision->width_tiles; ++tile_x) {
            size_t block_x = tile_x / 4;
            size_t local_x = tile_x % 4;
            size_t local_y = initial_local_y;
            uint16_t map_value = read_u16(
                block_map + (block_x + block_y *
                             collision->width_screens * 8) * 2);
            uint16_t block = map_value & 0x3FFF;
            if (block == 0) continue;
            bool xflip = (map_value & 0x4000) != 0;
            if (xflip) local_x = 3 - local_x;
            if (map_value & 0x8000) local_y = 3 - local_y;
            size_t cell = (size_t)(block - 1) * 16 + local_x + local_y * 4;
            uint8_t value = values.data[cell];
            if ((value & 0xC0) && xflip) value ^= 4;
            collision->cells[tile_x + tile_y * collision->width_tiles] = value;
        }
    }
    free_payload(&values);
    collision->status = FUSION_AOS_COLLISION_DECODED;
    if (error && error_size > 0) error[0] = '\0';
    return true;
}
