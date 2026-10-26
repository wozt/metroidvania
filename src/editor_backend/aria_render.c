/* SPDX-License-Identifier: GPL-3.0-only */
/* The Aria room renderer (formerly scripts/aos_room_render.py), its editable
 * workroom export (scripts/aos_native_workspace.py) and the Aria room
 * annotations (scripts/room_annotations.py). The decoder mirrors cvaos room
 * loading: graphics and palette resources fill virtual GBA memory, the room
 * metadata expands 4x4 tile blocks and text backgrounds are rendered with
 * native flips and palette banks. Outputs stay under assets/extracted. */
#define _DEFAULT_SOURCE
#include "aria_render.h"

#include "fusion/assets/aos.h"
#include "fusion/assets/rom.h"
#include "generated.h"
#include "mzm_workspace.h"
#include "object_catalog.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum {
    GBA_BASE = 0x08000000,
    MAX_LZ77_OUTPUT = 256 * 1024,
    MAX_ROOM_PIXELS = 4096 * 4096,
    VRAM_SIZE = 0x10000,
    PALETTE_SIZE = 0x200,
    MAX_TILES = 1024,
    MAX_CELLS = 6144,
};

static const char aria_rom_path[] = "roms/Castlevania - Aria of Sorrow (USA).gba";

typedef struct {
    int layer;
    const char *status;
    const char *metadata_pointer;
    int64_t width, height, width_tiles, height_tiles, flags, control, depth_key;
    uint16_t *tiles;
    uint8_t *collision;
} Background;

static void background_free(Background *background) {
    free(background->tiles);
    free(background->collision);
}

static bool offset_of(const FusionRom *rom, uint32_t pointer, size_t size, size_t *offset,
                      FebError *error) {
    if (pointer < GBA_BASE || (size_t)(pointer - GBA_BASE) + size > rom->size)
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid Aria ROM pointer 0x%08x",
                        (unsigned)pointer);
    *offset = pointer - GBA_BASE;
    return true;
}

static uint32_t u32_at(const uint8_t *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static uint32_t hex_of(const FusionJson *value) {
    return value && value->kind == FUSION_JSON_STRING ? (uint32_t)strtoul(value->string, NULL, 16)
                                                      : 0;
}

static int64_t int_of(const FusionJson *object, const char *key) {
    const FusionJson *value = fusion_json_member(object, key);
    return value && value->kind == FUSION_JSON_NUMBER ? value->integer : 0;
}

/* lz77_at(): a bounded BIOS 0x10 stream at a ROM pointer. */
static bool lz77_at(const FusionRom *rom, uint32_t pointer, uint8_t **out, size_t *out_size,
                    FebError *error) {
    size_t position = 0;
    if (!offset_of(rom, pointer, 4, &position, error)) return false;
    if (rom->data[position] != 0x10)
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid LZ77 header at 0x%08x",
                        (unsigned)pointer);
    size_t size = rom->data[position + 1] | (rom->data[position + 2] << 8) |
                  ((size_t)rom->data[position + 3] << 16);
    if (!size || size > MAX_LZ77_OUTPUT)
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid LZ77 size at 0x%08x",
                        (unsigned)pointer);
    position += 4;
    uint8_t *output = malloc(size);
    if (!output) return feb_oom(error);
    size_t used = 0;
    const char *failure = NULL;
    while (!failure && used < size) {
        if (position >= rom->size) {
            failure = "truncated LZ77 controls";
            break;
        }
        uint8_t flags = rom->data[position++];
        for (int bit = 7; bit >= 0 && used < size; --bit) {
            if (flags & (1 << bit)) {
                if (position + 2 > rom->size) {
                    failure = "truncated LZ77 token";
                    break;
                }
                unsigned token = (unsigned)(rom->data[position] << 8) | rom->data[position + 1];
                position += 2;
                size_t amount = (token >> 12) + 3, distance = (token & 0xFFF) + 1;
                if (distance > used || used + amount > size) {
                    failure = "invalid LZ77 backreference";
                    break;
                }
                for (size_t index = 0; index < amount; ++index, ++used)
                    output[used] = output[used - distance];
            } else {
                if (position >= rom->size) {
                    failure = "truncated LZ77 literal";
                    break;
                }
                output[used++] = rom->data[position++];
            }
        }
    }
    if (failure) {
        free(output);
        return feb_fail(error, FEB_EXIT_OPERATION, "%s at 0x%08x", failure, (unsigned)pointer);
    }
    *out = output;
    *out_size = size;
    return true;
}

/* A resource or metadata payload: a view of the ROM or an owned buffer. */
typedef struct {
    const uint8_t *data;
    size_t size;
    uint8_t *owned;
} Payload;

/* resource_payload(); ``header`` receives the resource description. */
static bool resource_payload(const FusionRom *rom, uint32_t pointer, Payload *payload,
                             FusionJson *header, FebError *error) {
    size_t offset = 0;
    *payload = (Payload){0};
    if (!offset_of(rom, pointer, 8, &offset, error)) return false;
    const uint8_t *bytes = rom->data + offset;
    if (bytes[0] == 0) {
        payload->data = rom->data + offset + 4;
        payload->size = rom->size - offset - 4;
    } else if (bytes[0] == 1) {
        uint8_t *expanded = NULL;
        size_t size = 0;
        if (!lz77_at(rom, u32_at(bytes + 4), &expanded, &size, error)) return false;
        if (size < 4 || expanded[0] != 0) {
            free(expanded);
            return feb_fail(error, FEB_EXIT_OPERATION, "invalid expanded resource at 0x%08x",
                            (unsigned)pointer);
        }
        payload->owned = expanded;
        payload->data = expanded + 4;
        payload->size = size - 4;
    } else {
        return feb_fail(error, FEB_EXIT_OPERATION, "unsupported resource encoding %u at 0x%08x",
                        bytes[0], (unsigned)pointer);
    }
    char text[16];
    snprintf(text, sizeof text, "0x%08x", (unsigned)pointer);
    *header = fusion_json_object();
    if (!feb_put_string(header, "pointer", text) || !feb_put_int(header, "encoding", bytes[0]) ||
        !feb_put_int(header, "color_mode", bytes[1]) || !feb_put_int(header, "units", bytes[2]) ||
        !feb_put_int(header, "parameter", bytes[3])) {
        free(payload->owned);
        fusion_json_free(header);
        return feb_oom(error);
    }
    return true;
}

/* load_video_memory(): the room's native graphics and palette load lists. */
static bool load_video_memory(const FusionRom *rom, const FusionJson *room, uint8_t *vram,
                              uint8_t *palette, FusionJson *loads, FebError *error) {
    memset(vram, 0, VRAM_SIZE);
    memset(palette, 0, PALETTE_SIZE);
    *loads = fusion_json_array();
    const FusionJson *graphics = fusion_json_member(room, "graphics_loads");
    bool ok = true;
    for (size_t index = 0; ok && graphics && index < graphics->count; ++index) {
        const FusionJson *entry = &graphics->items[index];
        const FusionJson *parameters = fusion_json_member(entry, "parameters");
        Payload payload;
        FusionJson header;
        if (!resource_payload(rom, hex_of(fusion_json_member(entry, "resource_pointer")), &payload,
                              &header, error)) {
            ok = false;
            break;
        }
        size_t destination = (size_t)parameters->items[0].integer * 0x800;
        size_t source = (size_t)parameters->items[1].integer * 0x800;
        size_t length = (size_t)parameters->items[2].integer * 0x800;
        const char *entry_pointer = fusion_json_member(entry, "entry_pointer")->string;
        if (source + length > payload.size || destination + length > VRAM_SIZE) {
            ok = feb_fail(error, FEB_EXIT_OPERATION, "graphics load exceeds bounds at %s",
                          entry_pointer);
        } else {
            memcpy(vram + destination, payload.data + source, length);
            FusionJson record = fusion_json_object();
            ok = (feb_put_string(&record, "kind", "graphics") &&
                  feb_put_string(&record, "entry_pointer", entry_pointer) &&
                  feb_put_int(&record, "destination", (int64_t)destination) &&
                  feb_put_int(&record, "source", (int64_t)source) &&
                  feb_put_int(&record, "length", (int64_t)length) &&
                  feb_put(&record, "resource", header) && fusion_json_push(loads, record)) ||
                 feb_oom(error);
            header = fusion_json_null();
        }
        fusion_json_free(&header);
        free(payload.owned);
    }
    const FusionJson *palettes = fusion_json_member(room, "palette_loads");
    for (size_t index = 0; ok && palettes && index < palettes->count; ++index) {
        const FusionJson *entry = &palettes->items[index];
        const FusionJson *parameters = fusion_json_member(entry, "parameters");
        uint32_t pointer = hex_of(fusion_json_member(entry, "resource_pointer"));
        const char *entry_pointer = fusion_json_member(entry, "entry_pointer")->string;
        size_t offset = 0;
        if (!offset_of(rom, pointer, 4, &offset, error)) {
            ok = false;
            break;
        }
        const uint8_t *bytes = rom->data + offset;
        if (bytes[0] != 0) {
            ok = feb_fail(error, FEB_EXIT_OPERATION, "unsupported palette encoding at 0x%08x",
                          (unsigned)pointer);
            break;
        }
        int64_t destination_row = parameters->items[0].integer;
        int64_t source_row = parameters->items[1].integer, count = parameters->items[2].integer;
        /* The header stores the final valid row index, not a row count. */
        if (source_row + count > bytes[2] + 1 || destination_row + count > 16) {
            ok = feb_fail(error, FEB_EXIT_OPERATION, "palette load exceeds bounds at %s",
                          entry_pointer);
            break;
        }
        size_t source = offset + 4 + (size_t)source_row * 0x20, length = (size_t)count * 0x20;
        if (source + length > rom->size) {
            ok = feb_fail(error, FEB_EXIT_OPERATION, "palette resource is truncated at 0x%08x",
                          (unsigned)pointer);
            break;
        }
        memcpy(palette + destination_row * 0x20, rom->data + source, length);
        char text[16];
        snprintf(text, sizeof text, "0x%08x", (unsigned)pointer);
        FusionJson resource = fusion_json_object(), record = fusion_json_object();
        ok = (feb_put_string(&resource, "pointer", text) &&
              feb_put_int(&resource, "encoding", bytes[0]) &&
              feb_put_int(&resource, "color_mode", bytes[1]) &&
              feb_put_int(&resource, "units", bytes[2]) &&
              feb_put_int(&resource, "parameter", bytes[3]) &&
              feb_put_string(&record, "kind", "palette") &&
              feb_put_string(&record, "entry_pointer", entry_pointer) &&
              feb_put_int(&record, "destination_row", destination_row) &&
              feb_put_int(&record, "source_row", source_row) &&
              feb_put_int(&record, "row_count", count) && feb_put(&record, "resource", resource) &&
              fusion_json_push(loads, record)) ||
             feb_oom(error);
    }
    if (!ok) fusion_json_free(loads);
    return ok;
}

static bool metadata_payload(const FusionRom *rom, uint32_t pointer, bool compressed,
                             Payload *payload, FebError *error) {
    *payload = (Payload){0};
    if (compressed) {
        uint8_t *expanded = NULL;
        size_t size = 0;
        if (!lz77_at(rom, pointer, &expanded, &size, error)) return false;
        if (size < 4 || expanded[0] != 0) {
            free(expanded);
            return feb_fail(error, FEB_EXIT_OPERATION, "invalid expanded room metadata at 0x%08x",
                            (unsigned)pointer);
        }
        payload->owned = expanded;
        payload->data = expanded + 4;
        payload->size = size - 4;
        return true;
    }
    size_t offset = 0;
    if (!offset_of(rom, pointer, 1, &offset, error)) return false;
    payload->data = rom->data + offset;
    payload->size = rom->size - offset;
    return true;
}

/* decode_background(): one text background expanded to 8x8 tile entries. */
static bool decode_background(const FusionRom *rom, const FusionJson *descriptor,
                              Background *background, FebError *error) {
    *background = (Background){.layer = (int)int_of(descriptor, "layer"), .status = "ABSENT"};
    const FusionJson *metadata = fusion_json_member(descriptor, "metadata_pointer");
    if (!metadata || metadata->kind != FUSION_JSON_STRING) return true;
    background->metadata_pointer = metadata->string;
    uint32_t pointer = hex_of(metadata);
    size_t offset = 0;
    if (!offset_of(rom, pointer, 16, &offset, error)) return false;
    const uint8_t *bytes = rom->data + offset;
    int64_t width = bytes[0], height = bytes[1];
    unsigned flags = bytes[2] | (bytes[3] << 8);
    uint32_t blocks_pointer = u32_at(bytes + 4), collision_pointer = u32_at(bytes + 8);
    uint32_t map_pointer = u32_at(bytes + 12);
    if (!width || !height || width > 16 || height > 16)
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid background dimensions at %s",
                        metadata->string);
    background->width = width;
    background->height = height;
    background->flags = flags;
    if (flags & 1) {
        background->status = "UNSUPPORTED_AFFINE";
        return true;
    }
    size_t map_count = (size_t)(width * height * 64), map_offset = 0;
    if (!offset_of(rom, map_pointer, map_count * 2, &map_offset, error)) return false;
    const uint8_t *map = rom->data + map_offset;
    unsigned maximum = 0;
    for (size_t index = 0; index < map_count; ++index) {
        unsigned value = (map[index * 2] | (map[index * 2 + 1] << 8)) & 0x3FFF;
        if (value > maximum) maximum = value;
    }
    bool compressed = flags & 2;
    Payload blocks = {0}, collision = {0};
    if (!metadata_payload(rom, blocks_pointer, compressed, &blocks, error)) return false;
    bool ok = (size_t)maximum * 32 <= blocks.size ||
              feb_fail(error, FEB_EXIT_OPERATION,
                       "background block table is truncated at 0x%08x", (unsigned)blocks_pointer);
    bool has_collision = collision_pointer != 0;
    if (ok && has_collision) {
        ok = metadata_payload(rom, collision_pointer, compressed, &collision, error);
        if (ok && (size_t)maximum * 16 > collision.size)
            ok = feb_fail(error, FEB_EXIT_OPERATION,
                          "background collision table is truncated at 0x%08x",
                          (unsigned)collision_pointer);
    }
    int64_t width_tiles = width * 32, height_tiles = height * 32;
    if (ok && width_tiles * height_tiles * 64 > MAX_ROOM_PIXELS)
        ok = feb_fail(error, FEB_EXIT_OPERATION, "Aria background exceeds pixel safety cap");
    size_t total = (size_t)(width_tiles * height_tiles);
    if (ok) {
        background->tiles = calloc(total, sizeof *background->tiles);
        background->collision = has_collision ? calloc(total, 1) : NULL;
        if (!background->tiles || (has_collision && !background->collision)) ok = feb_oom(error);
    }
    for (int64_t tile_y = 0; ok && tile_y < height_tiles; ++tile_y) {
        int64_t block_y = tile_y / 4, row_y = tile_y % 4;
        for (int64_t tile_x = 0; tile_x < width_tiles; ++tile_x) {
            int64_t block_x = tile_x / 4, local_x = tile_x % 4, local_y = row_y;
            size_t at = (size_t)(block_x + block_y * width * 8);
            unsigned value = map[at * 2] | (map[at * 2 + 1] << 8);
            unsigned block = value & 0x3FFF;
            if (!block) continue;
            if (value & 0x4000) local_x = 3 - local_x;
            if (value & 0x8000) local_y = 3 - local_y;
            size_t cell = (size_t)(block - 1) * 16 + (size_t)(local_x + local_y * 4);
            unsigned word = blocks.data[cell * 2] | (blocks.data[cell * 2 + 1] << 8);
            word ^= ((value >> 12) & 0xC) << 8;
            size_t destination = (size_t)(tile_x + tile_y * width_tiles);
            background->tiles[destination] = (uint16_t)word;
            if (has_collision) background->collision[destination] = collision.data[cell];
        }
    }
    free(blocks.owned);
    free(collision.owned);
    if (!ok) {
        background_free(background);
        return false;
    }
    background->status = "DECODED_TEXT_BACKGROUND";
    background->width_tiles = width_tiles;
    background->height_tiles = height_tiles;
    background->control = int_of(descriptor, "control");
    background->depth_key = int_of(descriptor, "field_0");
    return true;
}

static bool decoded(const Background *background) {
    return !strcmp(background->status, "DECODED_TEXT_BACKGROUND");
}

/* render_background(): a 4bpp text background as RGBA. */
static bool render_background(const Background *background, const uint8_t *vram,
                              const uint8_t *palette, uint8_t **rgba, int64_t *unresolved,
                              FebError *error) {
    if (!decoded(background))
        return feb_fail(error, FEB_EXIT_OPERATION, "background is not a decoded text layer");
    if (background->control & 0x80)
        return feb_fail(error, FEB_EXIT_OPERATION, "8 bpp text backgrounds are not supported");
    int64_t width = background->width_tiles * 8, height = background->height_tiles * 8;
    *rgba = calloc((size_t)(width * height), 4);
    if (!*rgba) return feb_oom(error);
    *unresolved = 0;
    size_t character_base = (size_t)((background->control >> 2) & 3) * 0x4000;
    for (int64_t tile_y = 0; tile_y < background->height_tiles; ++tile_y)
        for (int64_t tile_x = 0; tile_x < background->width_tiles; ++tile_x) {
            unsigned word = background->tiles[tile_x + tile_y * background->width_tiles];
            size_t tile_offset = character_base + (word & 0x3FF) * 32;
            if (tile_offset + 32 > VRAM_SIZE) {
                ++*unresolved;
                continue;
            }
            unsigned bank = (word >> 12) & 0xF;
            for (int y = 0; y < 8; ++y) {
                int source_y = word & 0x800 ? 7 - y : y;
                for (int x = 0; x < 8; ++x) {
                    int source_x = word & 0x400 ? 7 - x : x;
                    uint8_t packed = vram[tile_offset + source_y * 4 + source_x / 2];
                    unsigned index = (packed >> (4 * (source_x & 1))) & 0xF;
                    if (!index) continue;
                    size_t color_offset = (bank * 16 + index) * 2;
                    if (color_offset + 2 > PALETTE_SIZE) {
                        ++*unresolved;
                        continue;
                    }
                    unsigned value = palette[color_offset] | (palette[color_offset + 1] << 8);
                    uint8_t *pixel = *rgba + ((tile_y * 8 + y) * width + tile_x * 8 + x) * 4;
                    for (int channel = 0; channel < 3; ++channel)
                        pixel[channel] = (uint8_t)(((value >> (5 * channel)) & 31) * 255 / 31);
                    pixel[3] = 255;
                }
            }
        }
    return true;
}

/* collision_preview(): the raw collision bytes in a diagnostic color key. */
static bool collision_preview(const Background *background, uint8_t **bmp, size_t *size,
                              FebError *error) {
    int64_t width = background->width_tiles * 8, height = background->height_tiles * 8;
    uint8_t *rgb = calloc((size_t)(width * height), 3);
    if (!rgb) return feb_oom(error);
    for (int64_t tile_y = 0; tile_y < background->height_tiles; ++tile_y)
        for (int64_t tile_x = 0; tile_x < background->width_tiles; ++tile_x) {
            uint8_t value = background->collision[tile_x + tile_y * background->width_tiles];
            uint8_t color[3];
            if (value == 0) memcpy(color, (uint8_t[]){8, 8, 12}, 3);
            else if (value == 0xFF) memcpy(color, (uint8_t[]){240, 240, 240}, 3);
            else if (value & 0xC0) memcpy(color, (uint8_t[]){245, 160, 48}, 3);
            else if (value & 2) memcpy(color, (uint8_t[]){220, 55, 75}, 3);
            else memcpy(color, (uint8_t[]){70, 145, 235}, 3);
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x) {
                    uint8_t *pixel = rgb + ((tile_y * 8 + y) * width + tile_x * 8 + x) * 3;
                    for (int channel = 0; channel < 3; ++channel)
                        pixel[channel] = x == 0 || y == 0 ? color[channel] / 2 : color[channel];
                }
        }
    bool ok = feb_bmp24(width, height, rgb, bmp, size, error);
    free(rgb);
    return ok;
}

/* The native world catalog of the verified ROM, as the importer writes it. */
static bool aria_world(const char *repository, FusionRom *rom, FusionJson *world,
                       FebError *error) {
    char path[FEB_PATH_CAPACITY], detail[256] = "";
    if (!feb_join(path, repository, aria_rom_path) ||
        !fusion_rom_open(FUSION_ASSET_GAME_ARIA, path, rom, detail, sizeof detail))
        return feb_fail(error, FEB_EXIT_OPERATION, "%s", detail[0] ? detail : "missing Aria ROM");
    char directory[] = "/tmp/fusion-aria-world-XXXXXX";
    if (!mkdtemp(directory)) {
        fusion_rom_close(rom);
        return feb_fail(error, FEB_EXIT_OPERATION, "cannot create a temporary directory");
    }
    snprintf(path, sizeof path, "%s/world.json", directory);
    bool ok = fusion_aos_write_world_catalog(rom, path, detail, sizeof detail) ||
              feb_fail(error, FEB_EXIT_OPERATION, "%s", detail);
    ok = ok && (feb_load_json(path, 256 * 1024 * 1024, world, detail, sizeof detail) ||
                feb_fail(error, FEB_EXIT_OPERATION, "%s", detail));
    unlink(path);
    rmdir(directory);
    if (!ok) fusion_rom_close(rom);
    return ok;
}

static const FusionJson *find_room(const FusionJson *world, int64_t area, int64_t number) {
    const FusionJson *rooms = fusion_json_member(world, "rooms");
    for (size_t index = 0; rooms && index < rooms->count; ++index)
        if (int_of(&rooms->items[index], "engine_area") == area &&
            int_of(&rooms->items[index], "room") == number)
            return &rooms->items[index];
    return NULL;
}

static bool layer_record(const Background *background, FusionJson *record) {
    *record = fusion_json_object();
    bool ok = feb_put_string(record, "status", background->status) &&
              feb_put_int(record, "layer", background->layer);
    if (!ok || !strcmp(background->status, "ABSENT")) return ok;
    ok = feb_put_string(record, "metadata_pointer", background->metadata_pointer) &&
         feb_put_int(record, "width_screens", background->width) &&
         feb_put_int(record, "height_screens", background->height);
    if (ok && decoded(background))
        ok = feb_put_int(record, "width_tiles", background->width_tiles) &&
             feb_put_int(record, "height_tiles", background->height_tiles);
    ok = ok && feb_put_int(record, "flags", background->flags);
    if (ok && decoded(background))
        ok = feb_put_int(record, "control", background->control) &&
             feb_put_int(record, "depth_key", background->depth_key);
    return ok;
}

static bool write_rgb_bitmap(const char *repository, const char *relative, int64_t width,
                             int64_t height, const uint8_t *rgb, bool write, FebError *error) {
    uint8_t *bmp = NULL;
    size_t size = 0;
    if (!feb_bmp24(width, height, rgb, &bmp, &size, error)) return false;
    bool ok = !write || feb_write_generated(repository, relative, bmp, size, NULL, error);
    free(bmp);
    return ok;
}

static bool decode_all(const FusionRom *rom, const FusionJson *room, Background backgrounds[3],
                       size_t *count, FebError *error) {
    const FusionJson *descriptors = fusion_json_member(room, "backgrounds");
    *count = 0;
    for (size_t index = 0; descriptors && index < descriptors->count && index < 3; ++index) {
        if (!decode_background(rom, &descriptors->items[index], &backgrounds[index], error))
            return false;
        ++*count;
    }
    return true;
}

bool feb_aria_open_world(const char *repository, FusionRom *rom, FusionJson *world,
                         FebError *error) {
    return aria_world(repository, rom, world, error);
}

bool feb_aria_render_room(const char *repository, int64_t area, int64_t number,
                          bool write_outputs, FusionJson *result, FebError *error) {
    if (area < 0 || area >= 12 || number < 0)
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid Aria room selection");
    FusionRom rom = {0};
    FusionJson world;
    if (!aria_world(repository, &rom, &world, error)) return false;
    bool ok = feb_aria_render_loaded(repository, &rom, &world, area, number, write_outputs, result,
                                     error);
    fusion_json_free(&world);
    fusion_rom_close(&rom);
    return ok;
}

bool feb_aria_render_loaded(const char *repository, const FusionRom *loaded,
                            const FusionJson *catalog, int64_t area, int64_t number,
                            bool write_outputs, FusionJson *result, FebError *error) {
    if (area < 0 || area >= 12 || number < 0)
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid Aria room selection");
    const FusionRom rom = *loaded;
    const FusionJson world = *catalog;
    const FusionJson *room = find_room(&world, area, number);
    uint8_t *vram = malloc(VRAM_SIZE), *palette = malloc(PALETTE_SIZE);
    FusionJson loads = fusion_json_null(), layers = fusion_json_array();
    Background backgrounds[3] = {{0}};
    size_t count = 0;
    uint8_t *rgba[3] = {NULL, NULL, NULL};
    bool ok = room != NULL ||
              feb_fail(error, FEB_EXIT_OPERATION, "unknown Aria room %lld:%lld", (long long)area,
                       (long long)number);
    if (ok && (!vram || !palette)) ok = feb_oom(error);
    ok = ok && load_video_memory(&rom, room, vram, palette, &loads, error) &&
         decode_all(&rom, room, backgrounds, &count, error);
    char prefix[96];
    snprintf(prefix, sizeof prefix, "rooms/aria/previews/area_%02lld_room_%03lld", (long long)area,
             (long long)number);
    for (size_t index = 0; ok && index < count; ++index) {
        Background *background = &backgrounds[index];
        FusionJson record;
        ok = layer_record(background, &record) || feb_oom(error);
        if (ok && decoded(background)) {
            int64_t unresolved = 0;
            ok = render_background(background, vram, palette, &rgba[index], &unresolved, error);
            int64_t width = background->width_tiles * 8, height = background->height_tiles * 8;
            uint8_t *rgb = ok ? malloc((size_t)(width * height) * 3) : NULL;
            if (ok && !rgb) ok = feb_oom(error);
            for (int64_t pixel = 0; ok && pixel < width * height; ++pixel)
                memcpy(rgb + pixel * 3, rgba[index] + pixel * 4, 3);
            char relative[128];
            snprintf(relative, sizeof relative, "%s_bg%d.bmp", prefix, background->layer);
            ok = ok && write_rgb_bitmap(repository, relative, width, height, rgb, write_outputs,
                                        error);
            free(rgb);
            ok = ok && ((feb_put_string(&record, "path", relative) &&
                         feb_put_int(&record, "unresolved_tiles_or_pixels", unresolved)) ||
                        feb_oom(error));
        }
        if (ok) ok = fusion_json_push(&layers, record) || feb_oom(error);
        else fusion_json_free(&record);
    }
    /* composite_backgrounds(): a static-origin composite by native depth key. */
    int64_t width = 0, height = 0;
    char composite_relative[128], collision_relative[128];
    snprintf(composite_relative, sizeof composite_relative, "%s_composite.bmp", prefix);
    snprintf(collision_relative, sizeof collision_relative, "%s_collision.bmp", prefix);
    if (ok) {
        int primary = -1, first = -1;
        for (size_t index = 0; index < count; ++index) {
            if (!rgba[index]) continue;
            if (first < 0) first = (int)index;
            if (primary < 0 && backgrounds[index].layer == 1) primary = (int)index;
        }
        if (primary < 0) primary = first;
        if (primary < 0)
            ok = feb_fail(error, FEB_EXIT_OPERATION, "room has no supported text background layers");
        uint8_t *rgb = NULL;
        if (ok) {
            width = backgrounds[primary].width_tiles * 8;
            height = backgrounds[primary].height_tiles * 8;
            rgb = calloc((size_t)(width * height), 3);
            if (!rgb) ok = feb_oom(error);
        }
        /* sorted(reverse=True) keeps equal depth keys in their order. */
        size_t order[3], ordered = 0;
        for (size_t index = 0; index < count; ++index)
            if (rgba[index]) order[ordered++] = index;
        for (size_t pass = 1; pass < ordered; ++pass)
            for (size_t index = pass; index > 0 &&
                 backgrounds[order[index]].depth_key > backgrounds[order[index - 1]].depth_key;
                 --index) {
                size_t swap = order[index];
                order[index] = order[index - 1];
                order[index - 1] = swap;
            }
        for (size_t item = 0; ok && item < ordered; ++item) {
            const Background *background = &backgrounds[order[item]];
            int64_t source_width = background->width_tiles * 8;
            int64_t source_height = background->height_tiles * 8;
            for (int64_t y = 0; y < height; ++y)
                for (int64_t x = 0; x < width; ++x) {
                    const uint8_t *source = rgba[order[item]] +
                        ((y % source_height) * source_width + x % source_width) * 4;
                    if (!source[3]) continue;
                    memcpy(rgb + (y * width + x) * 3, source, 3);
                }
        }
        ok = ok && write_rgb_bitmap(repository, composite_relative, width, height, rgb,
                                    write_outputs, error);
        free(rgb);
    }
    const Background *bg1 = NULL;
    for (size_t index = 0; index < count; ++index)
        if (backgrounds[index].layer == 1 && !bg1) bg1 = &backgrounds[index];
    bool has_collision = ok && bg1 && bg1->collision;
    if (has_collision) {
        uint8_t *bmp = NULL;
        size_t size = 0;
        ok = collision_preview(bg1, &bmp, &size, error) &&
             (!write_outputs ||
              feb_write_generated(repository, collision_relative, bmp, size, NULL, error));
        free(bmp);
    }
    *result = fusion_json_object();
    if (ok) {
        static const char *const limitations[] = {
            "static-origin composite does not simulate parallax camera movement",
            "affine/8 bpp backgrounds, blending, animation and entities are omitted",
            "collision preview colors are diagnostic and are not original game art"};
        FusionJson list = fusion_json_array(), copy;
        for (size_t index = 0; ok && index < 3; ++index)
            ok = fusion_json_push(&list, fusion_json_string(limitations[index]));
        const FusionJson *entities = fusion_json_member(room, "entities");
        const FusionJson *transitions = fusion_json_member(room, "transitions");
        ok = (ok &&
              fusion_json_clone(fusion_json_member(&world, "source_revision"), &copy) &&
              feb_put_string(result, "format", "MV_AOS_RENDER_1") &&
              feb_put_string(result, "status", "NATIVE_TEXT_BACKGROUND_RENDER") &&
              feb_put(result, "source_revision", copy) && feb_put_int(result, "engine_area", area) &&
              feb_put_string(result, "area", fusion_json_member(room, "area")->string) &&
              feb_put_int(result, "room", number) &&
              feb_put_string(result, "room_pointer",
                             fusion_json_member(room, "source_pointer")->string) &&
              feb_put_int(result, "map_x", int_of(room, "map_x")) &&
              feb_put_int(result, "map_y", int_of(room, "map_y")) &&
              feb_put_int(result, "width", width) && feb_put_int(result, "height", height) &&
              feb_put_string(result, "composite_path", composite_relative) &&
              (has_collision ? feb_put_string(result, "collision_path", collision_relative)
                             : feb_put(result, "collision_path", fusion_json_null())) &&
              feb_put(result, "loads", loads) && feb_put(result, "layers", layers) &&
              feb_put_int(result, "entity_count", entities ? (int64_t)entities->count : 0) &&
              feb_put_int(result, "transition_count",
                          transitions ? (int64_t)transitions->count : 0) &&
              feb_put_bool(result, "outputs_written", write_outputs) &&
              feb_put(result, "limitations", list)) ||
             feb_oom(error);
        loads = layers = fusion_json_null();
    }
    if (ok && write_outputs) {
        char relative[128], *text = NULL;
        size_t size = 0;
        snprintf(relative, sizeof relative, "%s.json", prefix);
        ok = fusion_json_write(result, 2, &text, &size) || feb_oom(error);
        char *line = ok ? realloc(text, size + 1) : NULL;
        if (ok && !line) ok = feb_oom(error);
        if (ok) {
            line[size] = '\n';
            text = line;
            ok = feb_write_generated(repository, relative, (uint8_t *)text, size + 1, NULL, error);
        }
        free(text);
    }
    for (size_t index = 0; index < 3; ++index) {
        background_free(&backgrounds[index]);
        free(rgba[index]);
    }
    fusion_json_free(&loads);
    fusion_json_free(&layers);
    free(vram);
    free(palette);
    if (!ok) fusion_json_free(result);
    return ok;
}

/* ---- Aria room annotations -------------------------------------------- */

typedef struct {
    char *data;
    size_t size, capacity;
    bool failed;
} Buffer;

static void append(Buffer *buffer, const char *format, ...) {
    if (buffer->failed) return;
    va_list arguments;
    for (;;) {
        size_t room = buffer->capacity - buffer->size;
        va_start(arguments, format);
        int length = vsnprintf(buffer->data ? buffer->data + buffer->size : NULL, room, format,
                               arguments);
        va_end(arguments);
        if (length < 0) {
            buffer->failed = true;
            return;
        }
        if ((size_t)length < room) {
            buffer->size += (size_t)length;
            return;
        }
        size_t capacity = buffer->capacity ? buffer->capacity * 2 : 256;
        while (capacity - buffer->size <= (size_t)length) capacity *= 2;
        char *grown = realloc(buffer->data, capacity);
        if (!grown) {
            buffer->failed = true;
            return;
        }
        buffer->data = grown;
        buffer->capacity = capacity;
    }
}

/* aria_transition_geometry(); false for an unresolved placement. */
static bool transition_geometry(const FusionJson *room, const FusionJson *transition, int64_t *x,
                                int64_t *y, const char **placement) {
    int64_t sx = int_of(transition, "source_screen_x"), sy = int_of(transition, "source_screen_y");
    int64_t width = 0, height = 0;
    bool extent = false;
    const FusionJson *backgrounds = fusion_json_member(room, "backgrounds");
    for (int64_t layer = 1; layer <= 3 && !extent; ++layer)
        for (size_t index = 0; backgrounds && index < backgrounds->count; ++index) {
            const FusionJson *background = &backgrounds->items[index];
            if (int_of(background, "layer") != layer) continue;
            const FusionJson *w = fusion_json_member(background, "width_screens");
            const FusionJson *h = fusion_json_member(background, "height_screens");
            if (w && h && w->kind == FUSION_JSON_NUMBER && h->kind == FUSION_JSON_NUMBER &&
                w->integer >= 1 && w->integer <= 16 && h->integer >= 1 && h->integer <= 16) {
                width = w->integer;
                height = h->integer;
                extent = true;
            }
            break;
        }
    *placement = "screen-location-only";
    if (!extent) {
        *x = sx * 256 + 120 > 0 ? sx * 256 + 120 : 0;
        *y = sy * 256 + 120 > 0 ? sy * 256 + 120 : 0;
        return true;
    }
    if (sx == -1 && sy >= 0 && sy < height) {
        *x = 0;
        *y = sy * 256 + 120;
        *placement = "left-edge";
    } else if (sx == width && sy >= 0 && sy < height) {
        *x = width * 256 - 16;
        *y = sy * 256 + 120;
        *placement = "right-edge";
    } else if (sy == -1 && sx >= 0 && sx < width) {
        *x = sx * 256 + 120;
        *y = 0;
        *placement = "top-edge";
    } else if (sy == height && sx >= 0 && sx < width) {
        *x = sx * 256 + 120;
        *y = height * 256 - 16;
        *placement = "bottom-edge";
    } else if (sx >= 0 && sx < width && sy >= 0 && sy < height) {
        *x = sx * 256 + 120;
        *y = sy * 256 + 120;
    } else {
        return false;
    }
    return true;
}

/* export_aria(): the annotation TSV of one room; ``relative`` its path. */
static bool export_annotations(const char *repository, const FusionJson *room, char *relative,
                               size_t relative_size, FebError *error) {
    static char enemies[0x71][96];
    if (!feb_aria_enemy_names(repository, enemies, error)) return false;
    const FusionJson *entities = fusion_json_member(room, "entities");
    const FusionJson *transitions = fusion_json_member(room, "transitions");
    size_t count = (entities ? entities->count : 0) + (transitions ? transitions->count : 0);
    FebAnnotation *rows = calloc(count ? count : 1, sizeof *rows);
    char (*strings)[4][256] = calloc(count ? count : 1, sizeof *strings);
    char **details = calloc(count ? count : 1, sizeof *details);
    bool ok = (rows && strings && details) || feb_oom(error);
    size_t row = 0;
    for (size_t index = 0; ok && entities && index < entities->count; ++index, ++row) {
        const FusionJson *entity = &entities->items[index];
        int64_t kind = int_of(entity, "kind"), id = int_of(entity, "entity_id");
        const char *category = NULL;
        feb_aria_entity_identity(kind, id, enemies, strings[row][0], sizeof strings[row][0],
                                 &category);
        snprintf(strings[row][1], sizeof strings[row][1], "kind-%02llX:id-%02llX", (long long)kind,
                 (long long)id);
        const FusionJson *parameters = fusion_json_member(entity, "parameters");
        Buffer text = {0};
        append(&text, "pointer=%s; persistent=%lld; flags=0x%02llx; parameters=[",
               fusion_json_member(entity, "entry_pointer")->string,
               (long long)int_of(entity, "persistent_index"), (long long)int_of(entity, "flags"));
        for (size_t value = 0; parameters && value < parameters->count; ++value)
            append(&text, "%s%lld", value ? ", " : "", (long long)parameters->items[value].integer);
        append(&text, "]; category=%s", category);
        details[row] = text.data;
        if (text.failed) ok = feb_oom(error);
        const char *annotation = "OTHER";
        if (kind == 1 || (kind == 2 && (id == 0x0A || id == 0x0B))) annotation = "ENEMY";
        else if (kind >= 4 && kind <= 6) annotation = "ITEM";
        else if (kind == 2 && (id == 0 || (id >= 2 && id <= 6))) annotation = "DOOR";
        else if (kind == 2 || kind == 3) annotation = "OBJECT";
        int64_t x = int_of(entity, "x") - 8, y = int_of(entity, "y") - 8;
        snprintf(strings[row][2], sizeof strings[row][2], "native");
        rows[row] = (FebAnnotation){annotation, (int64_t)index, x > 0 ? x : 0, y > 0 ? y : 0, 16, 16,
                                    strings[row][2], strings[row][1], strings[row][0],
                                    details[row]};
    }
    for (size_t index = 0; ok && transitions && index < transitions->count; ++index, ++row) {
        const FusionJson *transition = &transitions->items[index];
        int64_t x = 0, y = 0;
        const char *placement = NULL;
        if (!transition_geometry(room, transition, &x, &y, &placement)) {
            x = y = 0;
            placement = "unresolved";
        }
        Buffer text = {0};
        append(&text,
               "pointer=%s; target=%lld:%lld; load=(%lld,%lld); source_screen=(%lld,%lld); "
               "placement=%s; native_screen=256x256; pixel_exact=false",
               fusion_json_member(transition, "entry_pointer")->string,
               (long long)int_of(transition, "target_engine_area"),
               (long long)int_of(transition, "target_room"), (long long)int_of(transition, "load_x"),
               (long long)int_of(transition, "load_y"),
               (long long)int_of(transition, "source_screen_x"),
               (long long)int_of(transition, "source_screen_y"), placement);
        details[row] = text.data;
        if (text.failed) ok = feb_oom(error);
        snprintf(strings[row][0], sizeof strings[row][0], "Transition %zu (%s)", index, placement);
        snprintf(strings[row][1], sizeof strings[row][1], "%zu", index);
        snprintf(strings[row][2], sizeof strings[row][2], "transition-%s", placement);
        rows[row] = (FebAnnotation){strcmp(placement, "unresolved") ? "DOOR" : "OTHER",
                                    (int64_t)index, x, y, 16, 16, strings[row][2], strings[row][1],
                                    strings[row][0], details[row]};
    }
    char *text = NULL;
    size_t size = 0;
    snprintf(relative, relative_size, "rooms/aria/annotations/area_%02lld_room_%03lld.tsv",
             (long long)int_of(room, "engine_area"), (long long)int_of(room, "room"));
    ok = ok && feb_annotations_serialize(rows, row, &text, &size, error) &&
         feb_write_generated(repository, relative, (const uint8_t *)text, size, NULL, error);
    free(text);
    for (size_t index = 0; details && index < count; ++index) free(details[index]);
    free(details);
    free(strings);
    free(rows);
    return ok;
}

/* ---- The editable workroom -------------------------------------------- */

typedef struct {
    uint8_t (*tiles)[16 * 16 * 4];
    size_t count;
} Atlas;

/* split_tiles(): 16x16 RGBA cells deduplicated into the atlas. */
static bool split_tiles(const uint8_t *rgba, int64_t width, int64_t height, Atlas *atlas,
                        int64_t *w, int64_t *h, uint16_t **cells, FebError *error) {
    *w = width / 16;
    *h = height / 16;
    if (*w * *h > MAX_CELLS)
        return feb_fail(error, FEB_EXIT_OPERATION,
                        "Aria room exceeds %d editable cells per layer", MAX_CELLS);
    *cells = malloc((size_t)(*w * *h) * sizeof **cells);
    if (!*cells) return feb_oom(error);
    uint8_t tile[16 * 16 * 4];
    for (int64_t cy = 0; cy < *h; ++cy)
        for (int64_t cx = 0; cx < *w; ++cx) {
            for (int y = 0; y < 16; ++y)
                memcpy(tile + y * 64, rgba + ((cy * 16 + y) * width + cx * 16) * 4, 64);
            size_t found = atlas->count;
            for (size_t index = 0; index < atlas->count; ++index)
                if (!memcmp(atlas->tiles[index], tile, sizeof tile)) {
                    found = index;
                    break;
                }
            if (found == atlas->count) {
                if (atlas->count >= MAX_TILES) {
                    free(*cells);
                    *cells = NULL;
                    return feb_fail(error, FEB_EXIT_OPERATION,
                                    "more than 1024 unique 16x16 visual tiles: atlas expansion "
                                    "required");
                }
                memcpy(atlas->tiles[atlas->count++], tile, sizeof tile);
            }
            (*cells)[cy * *w + cx] = (uint16_t)found;
        }
    return true;
}

static bool exists(const char *repository, const char *relative) {
    char path[FEB_PATH_CAPACITY];
    long long size = 0;
    snprintf(path, sizeof path, "%s/assets/extracted/%s", repository, relative);
    return feb_is_regular(path, &size) || feb_is_directory(path);
}

bool feb_aria_workspace_export(const char *repository, int64_t area, int64_t number,
                               FusionJson *info, FebError *error) {
    if (area < 0 || area >= 12 || number < 0 || number >= 1000)
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid Aria room identity");
    FusionRom rom = {0};
    FusionJson world;
    if (!aria_world(repository, &rom, &world, error)) return false;
    const FusionJson *room = find_room(&world, area, number);
    uint8_t *vram = malloc(VRAM_SIZE), *palette = malloc(PALETTE_SIZE);
    FusionJson loads = fusion_json_null();
    Background backgrounds[3] = {{0}};
    size_t count = 0;
    Atlas atlas = {calloc(MAX_TILES, sizeof *atlas.tiles), 1};
    int64_t widths[2] = {0, 0}, heights[2] = {0, 0};
    uint16_t *cells[2] = {NULL, NULL};
    bool ok = room != NULL || feb_fail(error, FEB_EXIT_OPERATION, "unknown Aria room");
    if (ok && (!vram || !palette || !atlas.tiles)) ok = feb_oom(error);
    ok = ok && load_video_memory(&rom, room, vram, palette, &loads, error) &&
         decode_all(&rom, room, backgrounds, &count, error);
    const Background *layers[4] = {NULL, NULL, NULL, NULL}, *master = NULL;
    for (size_t index = 0; ok && index < count; ++index) {
        if (backgrounds[index].layer >= 1 && backgrounds[index].layer <= 3)
            layers[backgrounds[index].layer] = &backgrounds[index];
        if (!master && decoded(&backgrounds[index])) master = &backgrounds[index];
    }
    if (ok && !master)
        ok = feb_fail(error, FEB_EXIT_OPERATION, "this Aria room has no supported 4bpp background");
    for (int layer = 0; ok && layer < 2; ++layer) {
        const Background *item = layers[layer + 1];
        uint8_t *rgba = NULL;
        int64_t unresolved = 0, width, height;
        if (item && decoded(item)) {
            ok = render_background(item, vram, palette, &rgba, &unresolved, error);
            width = item->width_tiles * 8;
            height = item->height_tiles * 8;
        } else {
            width = master->width_tiles * 8;
            height = master->height_tiles * 8;
            rgba = calloc((size_t)(width * height), 4);
            if (!rgba) ok = feb_oom(error);
        }
        ok = ok && split_tiles(rgba, width, height, &atlas, &widths[layer], &heights[layer],
                               &cells[layer], error);
        free(rgba);
    }
    char basename[64], bg3_relative[128] = "", atlas_relative[128], workroom_relative[128],
         override_relative[128], collision_relative[128] = "";
    snprintf(basename, sizeof basename, "area_%02lld_room_%03lld", (long long)area,
             (long long)number);
    /* BG3 is decoded independently, keeping palette index 0 transparent. */
    const Background *bg3 = layers[3];
    if (ok && bg3 && decoded(bg3)) {
        int64_t width = bg3->width_tiles * 8, height = bg3->height_tiles * 8;
        if (width >= 1 && width <= 4096 && height >= 1 && height <= 4096 &&
            width * height <= 4194304) {
            uint8_t *rgba = NULL;
            int64_t unresolved = 0;
            ok = render_background(bg3, vram, palette, &rgba, &unresolved, error);
            if (ok && !unresolved) {
                snprintf(bg3_relative, sizeof bg3_relative, "rooms/aria/previews/%s_bg3.png",
                         basename);
                uint8_t *png = NULL;
                size_t size = 0;
                ok = (feb_png_rgba_level(width, height, rgba, 6, &png, &size) || feb_oom(error)) &&
                     feb_write_generated(repository, bg3_relative, png, size, NULL, error);
                free(png);
            }
            free(rgba);
        }
    }
    snprintf(atlas_relative, sizeof atlas_relative, "rooms/aria/tilesets/%s_atlas.png", basename);
    snprintf(workroom_relative, sizeof workroom_relative, "rooms/aria/workrooms/%s.mvnative",
             basename);
    snprintf(override_relative, sizeof override_relative, "overrides/aria/%s.mvnative", basename);
    if (ok && !exists(repository, atlas_relative)) {
        /* pack_atlas() */
        int64_t rows = ((int64_t)atlas.count + 15) / 16, width = 256, height = rows * 16;
        if (height < 16 || height > 1024)
            ok = feb_fail(error, FEB_EXIT_OPERATION, "invalid Aria atlas image");
        uint8_t *pixels = ok ? calloc((size_t)(width * height), 4) : NULL;
        if (ok && !pixels) ok = feb_oom(error);
        for (size_t index = 0; ok && index < atlas.count; ++index) {
            int64_t px = (int64_t)(index % 16) * 16, py = (int64_t)(index / 16) * 16;
            for (int y = 0; y < 16; ++y)
                memcpy(pixels + ((py + y) * width + px) * 4, atlas.tiles[index] + y * 64, 64);
        }
        uint8_t *png = NULL;
        size_t size = 0;
        ok = ok && (feb_png_rgba_level(width, height, pixels, 6, &png, &size) || feb_oom(error)) &&
             feb_write_generated(repository, atlas_relative, png, size, NULL, error);
        free(png);
        free(pixels);
    }
    if (ok && !exists(repository, workroom_relative)) {
        /* serialize() */
        if (atlas.count < 1 || atlas.count > MAX_TILES)
            ok = feb_fail(error, FEB_EXIT_OPERATION, "invalid Aria room identity or atlas size");
        Buffer text = {0};
        append(&text, "MVNATIVE 1\nROOM aria:%02lld:%03lld\nTILESET %lld\nTILES %zu\nATLAS %s\n",
               (long long)area, (long long)number, (long long)area, atlas.count, atlas_relative);
        for (int layer = 0; ok && layer < 2; ++layer) {
            if (widths[layer] < 1 || widths[layer] > 255 || heights[layer] < 1 ||
                heights[layer] > 255 || widths[layer] * heights[layer] > MAX_CELLS) {
                ok = feb_fail(error, FEB_EXIT_OPERATION, "invalid editable layer dimensions");
                break;
            }
            append(&text, "LAYER BG%d %lld %lld\n", layer + 1, (long long)widths[layer],
                   (long long)heights[layer]);
            for (int64_t y = 0; y < heights[layer]; ++y)
                for (int64_t x = 0; x < widths[layer]; ++x)
                    append(&text, "%04X%s", cells[layer][y * widths[layer] + x],
                           x + 1 == widths[layer] ? "\n" : " ");
        }
        append(&text, "END\n");
        if (ok && text.failed) ok = feb_oom(error);
        ok = ok && feb_write_generated(repository, workroom_relative, (const uint8_t *)text.data,
                                       text.size, NULL, error);
        free(text.data);
    }
    const Background *bg1 = layers[1];
    if (ok && bg1 && bg1->collision) {
        snprintf(collision_relative, sizeof collision_relative,
                 "rooms/aria/previews/%s_collision.bmp", basename);
        uint8_t *bmp = NULL;
        size_t size = 0;
        ok = collision_preview(bg1, &bmp, &size, error) &&
             feb_write_generated(repository, collision_relative, bmp, size, NULL, error);
        free(bmp);
    }
    char annotations[160];
    ok = ok && export_annotations(repository, room, annotations, sizeof annotations, error);
    *info = fusion_json_object();
    if (ok) {
        char room_id[32], paths[6][FEB_PATH_CAPACITY];
        snprintf(room_id, sizeof room_id, "aria:%02lld:%03lld", (long long)area, (long long)number);
        const char *relatives[6] = {atlas_relative, workroom_relative, override_relative,
                                    collision_relative, bg3_relative, annotations};
        for (int index = 0; index < 6; ++index)
            snprintf(paths[index], sizeof paths[index], "%s/assets/extracted/%s", repository,
                     relatives[index]);
        ok = (feb_put_string(info, "room_id", room_id) &&
              feb_put_int(info, "tile_count", (int64_t)atlas.count) &&
              feb_put_string(info, "atlas", paths[0]) && feb_put_string(info, "workroom", paths[1]) &&
              feb_put_string(info, "override", paths[2]) &&
              (collision_relative[0] ? feb_put_string(info, "collision", paths[3])
                                     : feb_put(info, "collision", fusion_json_null())) &&
              (bg3_relative[0] ? feb_put_string(info, "background", paths[4])
                               : feb_put(info, "background", fusion_json_null())) &&
              feb_put_string(info, "annotations", paths[5]) &&
              feb_put_string(info, "limitation",
                             "BG3 is a static private visual preview; parallax, affine/8bpp and "
                             "animated layers are not emulated or re-encoded.")) ||
             feb_oom(error);
    }
    for (int layer = 0; layer < 2; ++layer) free(cells[layer]);
    for (size_t index = 0; index < 3; ++index) background_free(&backgrounds[index]);
    free(atlas.tiles);
    fusion_json_free(&loads);
    free(vram);
    free(palette);
    fusion_json_free(&world);
    fusion_rom_close(&rom);
    if (!ok) fusion_json_free(info);
    return ok;
}
