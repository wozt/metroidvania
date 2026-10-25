/* SPDX-License-Identifier: GPL-3.0-only */
#include "fusion/assets/aos.h"

#include "fusion/assets/gba.h"

#include <stdarg.h>
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

static void write_u16(uint8_t *data, uint16_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}

static void write_u32(uint8_t *data, uint32_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

static bool append_format(uint8_t *data, size_t capacity, size_t *used,
                          const char *format, ...) {
    if (*used >= capacity) return false;
    va_list arguments;
    va_start(arguments, format);
    int written = vsnprintf((char *)data + *used, capacity - *used,
                            format, arguments);
    va_end(arguments);
    if (written < 0 || (size_t)written >= capacity - *used) return false;
    *used += (size_t)written;
    return true;
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

void fusion_aos_free_rendered_background(FusionAosRenderedBackground *rendered) {
    if (!rendered) return;
    free(rendered->rgba);
    *rendered = (FusionAosRenderedBackground){0};
}

bool fusion_aos_render_background(const FusionAosBackground *background,
                                  const FusionAosBackgroundTilemap *tilemap,
                                  const FusionAosVideoMemory *memory,
                                  FusionAosRenderedBackground *rendered,
                                  char *error, size_t error_size) {
    if (!rendered) {
        set_error(error, error_size, "invalid Aria background-render request");
        return false;
    }
    *rendered = (FusionAosRenderedBackground){0};
    if (!background || !tilemap || !memory ||
        tilemap->status != FUSION_AOS_TILEMAP_DECODED || !tilemap->cells ||
        tilemap->width_tiles == 0 || tilemap->height_tiles == 0 ||
        tilemap->cell_count != tilemap->width_tiles * tilemap->height_tiles) {
        set_error(error, error_size, "invalid Aria background-render request");
        return false;
    }
    if (background->control & 0x80) {
        set_error(error, error_size,
                  "Aria 8-bpp text backgrounds are not supported");
        return false;
    }
    size_t width = tilemap->width_tiles * 8;
    size_t height = tilemap->height_tiles * 8;
    if (width == 0 || height == 0 ||
        width > FUSION_AOS_RENDER_PIXEL_CAPACITY / height) {
        set_error(error, error_size,
                  "Aria rendered background exceeds the pixel safety cap");
        return false;
    }
    size_t pixel_count = width * height;
    if (pixel_count > FUSION_AOS_RENDER_PIXEL_CAPACITY ||
        pixel_count > SIZE_MAX / 4) {
        set_error(error, error_size,
                  "Aria rendered background exceeds the pixel safety cap");
        return false;
    }
    uint8_t *rgba = calloc(pixel_count, 4);
    if (!rgba) {
        set_error(error, error_size,
                  "cannot allocate Aria rendered background");
        return false;
    }
    size_t unresolved = 0;
    size_t character_base = (size_t)((background->control >> 2) & 3) * 0x4000;
    for (size_t tile_y = 0; tile_y < tilemap->height_tiles; ++tile_y) {
        for (size_t tile_x = 0; tile_x < tilemap->width_tiles; ++tile_x) {
            uint16_t word = tilemap->cells[
                tile_x + tile_y * tilemap->width_tiles];
            size_t tile_offset = character_base + (size_t)(word & 0x3FF) * 32;
            if (tile_offset > sizeof memory->vram ||
                32 > sizeof memory->vram - tile_offset) {
                ++unresolved;
                continue;
            }
            size_t palette_bank = (word >> 12) & 0xF;
            for (size_t y = 0; y < 8; ++y) {
                size_t source_y = (word & 0x800) ? 7 - y : y;
                for (size_t x = 0; x < 8; ++x) {
                    size_t source_x = (word & 0x400) ? 7 - x : x;
                    uint8_t packed = memory->vram[
                        tile_offset + source_y * 4 + source_x / 2];
                    uint8_t color_index =
                        (packed >> (4 * (source_x & 1))) & 0xF;
                    if (color_index == 0) continue;
                    size_t color_offset =
                        (palette_bank * 16 + color_index) * 2;
                    if (color_offset > sizeof memory->palette ||
                        2 > sizeof memory->palette - color_offset) {
                        ++unresolved;
                        continue;
                    }
                    uint16_t color = read_u16(memory->palette + color_offset);
                    size_t destination =
                        ((tile_y * 8 + y) * width + tile_x * 8 + x) * 4;
                    rgba[destination] =
                        (uint8_t)(((color & 31) * 255) / 31);
                    rgba[destination + 1] =
                        (uint8_t)((((color >> 5) & 31) * 255) / 31);
                    rgba[destination + 2] =
                        (uint8_t)((((color >> 10) & 31) * 255) / 31);
                    rgba[destination + 3] = 255;
                }
            }
        }
    }
    *rendered = (FusionAosRenderedBackground){
        width, height, rgba, pixel_count, unresolved
    };
    if (error && error_size > 0) error[0] = '\0';
    return true;
}

void fusion_aos_free_composite_background(
    FusionAosCompositeBackground *composite) {
    if (!composite) return;
    free(composite->rgb);
    *composite = (FusionAosCompositeBackground){0};
}

bool fusion_aos_composite_backgrounds(
    const FusionAosCompositeLayer *layers, size_t layer_count,
    FusionAosCompositeBackground *composite,
    char *error, size_t error_size) {
    if (!composite) {
        set_error(error, error_size,
                  "invalid Aria background-composite request");
        return false;
    }
    *composite = (FusionAosCompositeBackground){0};
    if (!layers || layer_count == 0 ||
        layer_count > FUSION_AOS_BACKGROUND_COUNT) {
        set_error(error, error_size,
                  "room has no supported Aria text background layers");
        return false;
    }
    size_t primary = 0;
    bool found_primary = false;
    for (size_t index = 0; index < layer_count; ++index) {
        const FusionAosCompositeLayer *layer = &layers[index];
        if (!layer->background || !layer->rendered ||
            !layer->rendered->rgba || layer->rendered->width_pixels == 0 ||
            layer->rendered->height_pixels == 0 ||
            layer->rendered->width_pixels >
                FUSION_AOS_RENDER_PIXEL_CAPACITY /
                    layer->rendered->height_pixels ||
            layer->rendered->pixel_count !=
                layer->rendered->width_pixels *
                    layer->rendered->height_pixels) {
            set_error(error, error_size,
                      "invalid Aria background-composite layer");
            return false;
        }
        if (!found_primary && layer->background->layer == 1) {
            primary = index;
            found_primary = true;
        }
    }
    size_t width = layers[primary].rendered->width_pixels;
    size_t height = layers[primary].rendered->height_pixels;
    if (width > FUSION_AOS_RENDER_PIXEL_CAPACITY / height) {
        set_error(error, error_size,
                  "Aria background composite exceeds the pixel safety cap");
        return false;
    }
    size_t pixel_count = width * height;
    if (pixel_count > FUSION_AOS_RENDER_PIXEL_CAPACITY ||
        pixel_count > SIZE_MAX / 3) {
        set_error(error, error_size,
                  "Aria background composite exceeds the pixel safety cap");
        return false;
    }
    uint8_t *rgb = calloc(pixel_count, 3);
    if (!rgb) {
        set_error(error, error_size,
                  "cannot allocate Aria background composite");
        return false;
    }
    size_t order[FUSION_AOS_BACKGROUND_COUNT];
    for (size_t index = 0; index < layer_count; ++index) {
        order[index] = index;
        size_t position = index;
        while (position > 0 &&
               layers[order[position - 1]].background->field_0 <
                   layers[order[position]].background->field_0) {
            size_t swap = order[position - 1];
            order[position - 1] = order[position];
            order[position] = swap;
            --position;
        }
    }
    for (size_t sorted = 0; sorted < layer_count; ++sorted) {
        const FusionAosRenderedBackground *source =
            layers[order[sorted]].rendered;
        for (size_t y = 0; y < height; ++y) {
            size_t source_row = (y % source->height_pixels) *
                                source->width_pixels;
            size_t destination_row = y * width;
            for (size_t x = 0; x < width; ++x) {
                size_t source_pixel =
                    source_row + x % source->width_pixels;
                size_t source_offset = source_pixel * 4;
                if (source->rgba[source_offset + 3] == 0) continue;
                memcpy(rgb + (destination_row + x) * 3,
                       source->rgba + source_offset, 3);
            }
        }
    }
    *composite = (FusionAosCompositeBackground){
        width, height, rgb, pixel_count
    };
    if (error && error_size > 0) error[0] = '\0';
    return true;
}

void fusion_aos_free_bitmap24(FusionAosBitmap24 *bitmap) {
    if (!bitmap) return;
    free(bitmap->data);
    *bitmap = (FusionAosBitmap24){0};
}

bool fusion_aos_serialize_composite_bmp24(
    const FusionAosCompositeBackground *composite,
    FusionAosBitmap24 *bitmap, char *error, size_t error_size) {
    enum { BMP_HEADER_SIZE = 54 };
    if (!bitmap) {
        set_error(error, error_size, "invalid Aria bitmap output request");
        return false;
    }
    *bitmap = (FusionAosBitmap24){0};
    if (!composite || !composite->rgb || composite->width_pixels == 0 ||
        composite->height_pixels == 0 || composite->width_pixels > 4096 ||
        composite->height_pixels > 4096 ||
        composite->width_pixels > SIZE_MAX / composite->height_pixels ||
        composite->pixel_count !=
            composite->width_pixels * composite->height_pixels ||
        composite->width_pixels > (SIZE_MAX - 3) / 3) {
        set_error(error, error_size, "invalid Aria RGB bitmap");
        return false;
    }
    size_t row_bytes = composite->width_pixels * 3;
    size_t pitch = (row_bytes + 3) & ~(size_t)3;
    if (composite->height_pixels > (SIZE_MAX - BMP_HEADER_SIZE) / pitch) {
        set_error(error, error_size, "Aria bitmap exceeds the size bound");
        return false;
    }
    size_t pixel_bytes = pitch * composite->height_pixels;
    size_t bitmap_size = BMP_HEADER_SIZE + pixel_bytes;
    if (bitmap_size > UINT32_MAX || pixel_bytes > UINT32_MAX ||
        composite->width_pixels > INT32_MAX ||
        composite->height_pixels > INT32_MAX) {
        set_error(error, error_size, "Aria bitmap exceeds the BMP format bound");
        return false;
    }
    uint8_t *data = calloc(bitmap_size, 1);
    if (!data) {
        set_error(error, error_size, "cannot allocate Aria bitmap");
        return false;
    }
    data[0] = 'B';
    data[1] = 'M';
    write_u32(data + 2, (uint32_t)bitmap_size);
    write_u32(data + 10, BMP_HEADER_SIZE);
    write_u32(data + 14, 40);
    write_u32(data + 18, (uint32_t)composite->width_pixels);
    write_u32(data + 22, (uint32_t)composite->height_pixels);
    write_u16(data + 26, 1);
    write_u16(data + 28, 24);
    write_u32(data + 34, (uint32_t)pixel_bytes);
    for (size_t y = 0; y < composite->height_pixels; ++y) {
        const uint8_t *source = composite->rgb + y * row_bytes;
        uint8_t *destination = data + BMP_HEADER_SIZE +
            (composite->height_pixels - y - 1) * pitch;
        for (size_t x = 0; x < composite->width_pixels; ++x) {
            destination[x * 3] = source[x * 3 + 2];
            destination[x * 3 + 1] = source[x * 3 + 1];
            destination[x * 3 + 2] = source[x * 3];
        }
    }
    *bitmap = (FusionAosBitmap24){data, bitmap_size};
    if (error && error_size > 0) error[0] = '\0';
    return true;
}

void fusion_aos_free_runtime_room_payload(
    FusionAosRuntimeRoomPayload *payload) {
    if (!payload) return;
    free(payload->data);
    *payload = (FusionAosRuntimeRoomPayload){0};
}

bool fusion_aos_serialize_runtime_room(
    size_t area, size_t room, const FusionAosCollisionMap *collision,
    const FusionAosTransitionList *transitions,
    const FusionAosEntityList *entities, uint8_t room_flags,
    FusionAosRuntimeRoomPayload *payload, char *error, size_t error_size) {
    if (!payload) {
        set_error(error, error_size,
                  "invalid Aria runtime-room output request");
        return false;
    }
    *payload = (FusionAosRuntimeRoomPayload){0};
    if (area >= FUSION_AOS_AREA_COUNT || !collision || !transitions ||
        !entities || collision->status != FUSION_AOS_COLLISION_DECODED ||
        !collision->cells || collision->width_screens == 0 ||
        collision->height_screens == 0 || collision->width_tiles == 0 ||
        collision->height_tiles == 0 ||
        collision->width_tiles != (size_t)collision->width_screens * 32 ||
        collision->height_tiles != (size_t)collision->height_screens * 32 ||
        collision->width_tiles >
            FUSION_AOS_COLLISION_CELL_CAPACITY / collision->height_tiles ||
        collision->cell_count !=
            collision->width_tiles * collision->height_tiles ||
        transitions->count > FUSION_AOS_ROOM_TRANSITION_CAPACITY ||
        entities->count > FUSION_AOS_ENTITY_CAPACITY ||
        room_flags >
            (FUSION_AOS_ROOM_FLAG_SAVE | FUSION_AOS_ROOM_FLAG_WARP)) {
        set_error(error, error_size, "invalid Aria runtime-room payload");
        return false;
    }
    size_t capacity = 256 + collision->cell_count * 2 +
        collision->height_tiles * 3 + transitions->count * 128 +
        entities->count * 128;
    uint8_t *data = malloc(capacity);
    if (!data) {
        set_error(error, error_size,
                  "cannot allocate Aria runtime-room payload");
        return false;
    }
    size_t used = 0;
    if (!append_format(
            data, capacity, &used,
            "AOSROOM-NATIVE\t4\t%zu\t%zu\t%u\t%u\t%zu\t%zu\t%u\n",
            area, room, (unsigned)collision->width_screens,
            (unsigned)collision->height_screens, collision->width_tiles,
            collision->height_tiles, (unsigned)room_flags)) {
        goto overflow;
    }
    static const char hex[] = "0123456789abcdef";
    for (size_t y = 0; y < collision->height_tiles; ++y) {
        size_t row_size = 3 + collision->width_tiles * 2;
        if (row_size > capacity - used) goto overflow;
        data[used++] = 'R';
        data[used++] = '\t';
        for (size_t x = 0; x < collision->width_tiles; ++x) {
            uint8_t value = collision->cells[y * collision->width_tiles + x];
            data[used++] = (uint8_t)hex[value >> 4];
            data[used++] = (uint8_t)hex[value & 15];
        }
        data[used++] = '\n';
    }
    for (size_t index = 0; index < transitions->count; ++index) {
        const FusionAosTransition *transition = &transitions->entries[index];
        if (!append_format(
                data, capacity, &used,
                "T\t%d\t%d\t%d\t%u\t%u\t%zu\t%zu\n",
                transition->source_screen_x, transition->source_screen_y,
                (int16_t)transition->field_6,
                (unsigned)transition->load_x,
                (unsigned)transition->load_y, transition->target_area,
                transition->target_room)) {
            goto overflow;
        }
    }
    for (size_t index = 0; index < entities->count; ++index) {
        const FusionAosEntity *entity = &entities->entries[index];
        if (!append_format(
                data, capacity, &used,
                "E\t%u\t%u\t%d\t%d\t%u\t%u\t%u\n",
                (unsigned)entity->kind, (unsigned)entity->entity_id,
                entity->x, entity->y, (unsigned)entity->parameters[0],
                (unsigned)entity->parameters[1], (unsigned)entity->flags)) {
            goto overflow;
        }
    }
    if (5 > capacity - used) goto overflow;
    memcpy(data + used, "END\n", 4);
    used += 4;
    data[used] = '\0';
    *payload = (FusionAosRuntimeRoomPayload){data, used};
    if (error && error_size > 0) error[0] = '\0';
    return true;

overflow:
    free(data);
    set_error(error, error_size,
              "Aria runtime-room payload exceeds the size bound");
    return false;
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
