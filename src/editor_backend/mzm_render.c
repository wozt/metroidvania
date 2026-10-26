/* SPDX-License-Identifier: GPL-3.0-only */
/* A partial offline reconstruction, not an emulated framebuffer and not
 * complete GBA compositing: the pinned RoomEntryRom declarations,
 * RoomRleDecompress, the tileset table and the verified raw imports. */
#define _DEFAULT_SOURCE
#include "mzm_render.h"

#include "catalog.h"
#include "generated.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char raw_prefix[] = "metroid/raw/data";

void feb_block_map_free(FebBlockMap *map) {
    free(map->blocks);
    *map = (FebBlockMap){0};
}

void feb_mzm_tileset_free(FebMzmTileset *tileset) {
    free(tileset->gfx);
    free(tileset->palette);
    free(tileset->table);
    *tileset = (FebMzmTileset){0};
}

/* read_raw(): one bounded, non-symlink raw import. */
bool feb_mzm_read_raw(const char *repository, const char *name, uint8_t **data, size_t *size,
                      FebError *error) {
    char path[FEB_PATH_CAPACITY];
    long long length = 0;
    snprintf(path, sizeof path, "%s/assets/extracted/%s/%s", repository, raw_prefix, name);
    if (feb_is_symlink(path) || !feb_is_regular(path, &length) || length > FEB_MZM_MAX_RAW ||
        !feb_read_text(path, FEB_MZM_MAX_RAW, (char **)data, size))
        return feb_fail(error, FEB_EXIT_OPERATION, "missing/unsafe extracted MZM resource: %s",
                        path);
    return true;
}

/* lz77(): the GBA BIOS LZ77 0x10 stream with strict back-reference bounds. */
bool feb_lz77(const uint8_t *data, size_t size, size_t limit, uint8_t **out, size_t *out_size,
              FebError *error) {
    if (size < 5 || data[0] != 0x10)
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid 0x10 LZ77 header");
    size_t total = data[1] | (data[2] << 8) | ((size_t)data[3] << 16);
    if (!total || total > limit)
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid LZ77 expanded size");
    uint8_t *buffer = malloc(total);
    if (!buffer) return feb_oom(error);
    size_t used = 0, position = 4;
    const char *failure = NULL;
    while (!failure && used < total) {
        if (position >= size) {
            failure = "truncated LZ77 control byte";
            break;
        }
        uint8_t flags = data[position++];
        for (int bit = 7; bit >= 0 && used < total; --bit) {
            if (flags & (1 << bit)) {
                if (position + 2 > size) {
                    failure = "truncated LZ77 backreference";
                    break;
                }
                unsigned token = (unsigned)(data[position] << 8) | data[position + 1];
                position += 2;
                size_t amount = (token >> 12) + 3, distance = (token & 4095) + 1;
                if (distance > used || used + amount > total) {
                    failure = "LZ77 invalid backreference";
                    break;
                }
                for (size_t index = 0; index < amount; ++index, ++used)
                    buffer[used] = buffer[used - distance];
            } else {
                if (position >= size) {
                    failure = "truncated LZ77 literal";
                    break;
                }
                buffer[used++] = data[position++];
            }
        }
    }
    for (size_t index = position; !failure && index < size; ++index)
        if (data[index]) failure = "unexpected nonzero LZ77 trailing data";
    if (failure) {
        free(buffer);
        return feb_fail(error, FEB_EXIT_OPERATION, "%s", failure);
    }
    *out = buffer;
    *out_size = total;
    return true;
}

/* rle_room(): the two-pass RLE of RoomRleDecompress(TRUE, src + 2, dst). */
bool feb_mzm_rle(const uint8_t *data, size_t size, FebBlockMap *map, FebError *error) {
    if (size < 6) return feb_fail(error, FEB_EXIT_OPERATION, "truncated room RLE");
    int64_t width = data[0], height = data[1], total = width * height;
    if (!width || !height || total > FEB_MZM_MAX_BLOCKS)
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid room dimensions");
    uint8_t *halves = calloc(2, (size_t)total);
    if (!halves) return feb_oom(error);
    size_t position = 2;
    const char *failure = NULL;
    char message[96];
    for (int pass = 0; !failure && pass < 2; ++pass) {
        uint8_t *half = halves + pass * total;
        int64_t used = 0;
        if (position >= size) {
            failure = "missing room RLE pass";
            break;
        }
        unsigned count_bytes = data[position++];
        if (count_bytes != 1 && count_bytes != 2) {
            failure = "invalid room RLE pass width";
            break;
        }
        unsigned high_bit = count_bytes == 1 ? 128 : 32768;
        for (;;) {
            if (position + count_bytes > size) {
                failure = "truncated room RLE count";
                break;
            }
            unsigned count = count_bytes == 1 ? data[position]
                                              : (unsigned)(data[position] << 8) | data[position + 1];
            position += count_bytes;
            if (!count) break;
            bool encoded = count & high_bit;
            count &= high_bit - 1;
            if (!count || used + count > total) {
                failure = "room RLE pass overflow";
                break;
            }
            if (encoded) {
                if (position >= size) {
                    failure = "truncated room RLE repeat";
                    break;
                }
                memset(half + used, data[position++], count);
            } else {
                if (position + count > size) {
                    failure = "truncated room RLE literals";
                    break;
                }
                memcpy(half + used, data + position, count);
                position += count;
            }
            used += count;
        }
        if (!failure && used != total) {
            snprintf(message, sizeof message, "room RLE pass length %lld != %lld",
                     (long long)used, (long long)total);
            failure = message;
        }
    }
    if (failure) {
        free(halves);
        return feb_fail(error, FEB_EXIT_OPERATION, "%s", failure);
    }
    map->blocks = malloc((size_t)total * sizeof *map->blocks);
    if (!map->blocks) {
        free(halves);
        return feb_oom(error);
    }
    for (int64_t index = 0; index < total; ++index)
        map->blocks[index] = (uint16_t)(halves[index] | (halves[total + index] << 8));
    map->width = width;
    map->height = height;
    free(halves);
    return true;
}

/* metatiles(): the u16 header and the quadruples after pTilemap + 2. */
static bool metatiles(const uint8_t *data, size_t size, FebMzmTileset *tileset, FebError *error) {
    if (size < 10 || size > 32768)
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid tileset metatile table size");
    size_t entries = (size - 2) / 8;
    if (!entries || entries > 1024)
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid metatile count");
    tileset->table = malloc(entries * sizeof *tileset->table);
    if (!tileset->table) return feb_oom(error);
    for (size_t entry = 0; entry < entries; ++entry)
        for (size_t word = 0; word < 4; ++word) {
            const uint8_t *value = data + 2 + entry * 8 + word * 2;
            tileset->table[entry][word] = (uint16_t)(value[0] | (value[1] << 8));
        }
    tileset->entries = entries;
    return true;
}

static int strcasecmp_ascii(const char *left, const char *right) {
    for (;; ++left, ++right) {
        int a = tolower((unsigned char)*left), b = tolower((unsigned char)*right);
        if (a != b || !a) return a - b;
    }
}

bool feb_mzm_source_room(const char *repository, const char *area, int64_t number,
                         FusionJson *room, FebError *error) {
    FusionJson rooms;
    if (!feb_native_catalog(repository, "zero_mission", &rooms, error)) return false;
    for (size_t index = 0; index < rooms.count; ++index) {
        FusionJson *entry = &rooms.items[index];
        if (strcasecmp_ascii(fusion_json_member(entry, "area")->string, area) ||
            fusion_json_member(entry, "room")->integer != number)
            continue;
        *room = *entry;
        *entry = fusion_json_null();
        fusion_json_free(&rooms);
        return true;
    }
    fusion_json_free(&rooms);
    return feb_fail(error, FEB_EXIT_OPERATION, "unknown Zero Mission room %s:%lld", area,
                    (long long)number);
}

bool feb_mzm_room_map(const char *repository, const char *symbol, FebBlockMap *map,
                      FebError *error) {
    /* SYM: s<Area>_<index>_(Bg0|Bg1|Bg2|Clipdata) */
    const char *cursor = symbol + 1, *area = cursor;
    bool valid = symbol[0] == 's';
    while (valid && isalnum((unsigned char)*cursor)) ++cursor;
    const char *area_end = cursor;
    valid = valid && area_end > area && *cursor == '_';
    const char *digits = ++cursor;
    while (valid && isdigit((unsigned char)*cursor)) ++cursor;
    valid = valid && cursor > digits && *cursor == '_';
    /* A backtracking area such as "s1_2_3_Bg1" never matches: no '_' in it. */
    const char *kind = cursor + 1;
    valid = valid && (!strcmp(kind, "Bg0") || !strcmp(kind, "Bg1") || !strcmp(kind, "Bg2") ||
                      !strcmp(kind, "Clipdata"));
    if (!valid)
        return feb_fail(error, FEB_EXIT_OPERATION, "unsupported room resource symbol: %s", symbol);
    char lower_area[64], lower_kind[16], name[192];
    snprintf(lower_area, sizeof lower_area, "%.*s", (int)(area_end - area), area);
    snprintf(lower_kind, sizeof lower_kind, "%s", kind);
    bool test = !strcmp(lower_area, "Test");
    for (char *c = lower_area; *c; ++c) *c = (char)tolower((unsigned char)*c);
    for (char *c = lower_kind; *c; ++c) *c = (char)tolower((unsigned char)*c);
    if (test)
        snprintf(name, sizeof name, "rooms/test/test_%.*s_%s.rle", (int)(cursor - digits), digits,
                 lower_kind);
    else
        snprintf(name, sizeof name, "rooms/%s/%s_%.*s_%s.gfx", lower_area, lower_area,
                 (int)(cursor - digits), digits, lower_kind);
    uint8_t *data = NULL;
    size_t size = 0;
    if (!feb_mzm_read_raw(repository, name, &data, &size, error)) return false;
    bool ok = feb_mzm_rle(data, size, map, error);
    free(data);
    return ok;
}

/* tileset_resource_indices(): the sTilesetEntries graphics, palette and
 * tilemap of one entry, resolving aliases such as 41 reusing 40. */
static bool tileset_resources(const char *repository, int64_t index, int resources[3],
                              FebError *error) {
    char path[FEB_PATH_CAPACITY], *source = NULL;
    if (!feb_join(path, repository, "third_party/mzm/src/data/rooms_data.c") ||
        !feb_read_text(path, 64 * 1024 * 1024, &source, NULL))
        return feb_fail(error, FEB_EXIT_OPERATION, "pinned MZM tileset table missing");
    const char *start = strstr(source, "const struct TilesetEntry sTilesetEntries[79] = {");
    if (!start) {
        free(source);
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid pinned MZM tileset table");
    }
    start += strlen("const struct TilesetEntry sTilesetEntries[79] = {");
    char *end = strstr(start, "\n};");
    if (end) *end = '\0';
    bool ok = true;
    /* Each entry is validated in order, as the cached table is. */
    for (int64_t entry = 0; ok && entry <= index; ++entry) {
        char label[32];
        snprintf(label, sizeof label, "[%lld]", (long long)entry);
        const char *body = NULL, *finish = NULL;
        for (const char *probe = strstr(start, label); probe && !body;
             probe = strstr(probe + 1, label)) {
            const char *cursor = probe + strlen(label);
            while (isspace((unsigned char)*cursor)) ++cursor;
            if (*cursor != '=') continue;
            ++cursor;
            while (isspace((unsigned char)*cursor)) ++cursor;
            if (*cursor != '{') continue;
            for (const char *line = strchr(cursor + 1, '\n'); line; line = strchr(line + 1, '\n')) {
                const char *next = line + 1;
                while (isspace((unsigned char)*next) && *next != '\0') {
                    if (*next == '}') break;
                    ++next;
                }
                if (*next == '}') {
                    body = cursor + 1;
                    finish = line;
                    break;
                }
            }
        }
        if (!body) {
            ok = feb_fail(error, FEB_EXIT_OPERATION, "missing MZM tileset entry %lld",
                          (long long)entry);
            break;
        }
        static const char *const fields[] = {"pTileGraphics", "pPalette", "pTilemap"};
        static const char *const suffixes[] = {"Gfx", "Pal", "Tilemap"};
        for (int field = 0; ok && field < 3; ++field) {
            bool found = false;
            for (const char *probe = body; !found && probe < finish; ++probe) {
                if (*probe != '.' || strncmp(probe + 1, fields[field], strlen(fields[field])))
                    continue;
                const char *cursor = probe + 1 + strlen(fields[field]);
                while (cursor < finish && isspace((unsigned char)*cursor)) ++cursor;
                if (cursor >= finish || *cursor != '=') continue;
                ++cursor;
                while (cursor < finish && isspace((unsigned char)*cursor)) ++cursor;
                if (strncmp(cursor, "sTileset_", 9)) continue;
                cursor += 9;
                const char *digits = cursor;
                while (isdigit((unsigned char)*cursor)) ++cursor;
                if (cursor == digits || *cursor != '_' ||
                    strncmp(cursor + 1, suffixes[field], strlen(suffixes[field])))
                    continue;
                char after = cursor[1 + strlen(suffixes[field])];
                if (isalnum((unsigned char)after) || after == '_') continue;
                if (entry == index) resources[field] = atoi(digits);
                found = true;
            }
            if (!found)
                ok = feb_fail(error, FEB_EXIT_OPERATION, "invalid MZM tileset %lld %s",
                              (long long)entry, fields[field]);
        }
    }
    free(source);
    return ok;
}

bool feb_mzm_tileset_blobs(const char *repository, int64_t index, uint8_t *blobs[3],
                          size_t sizes[3], FebError *error) {
    for (int part = 0; part < 3; ++part) {
        blobs[part] = NULL;
        sizes[part] = 0;
    }
    if (index < 0 || index > 78)
        return feb_fail(error, FEB_EXIT_OPERATION, "unsupported Zero Mission tileset");
    int resources[3] = {0, 0, 0};
    if (!tileset_resources(repository, index, resources, error)) return false;
    static const char *const suffixes[] = {"gfx.lz", "pal", "tm"};
    static const char *const zero[] = {"tileset_0_tiles.gfx.lz", "tileset_0.pal", "tileset_0.tm"};
    for (int part = 0; part < 3; ++part) {
        char name[96];
        if (resources[part] == 0) snprintf(name, sizeof name, "rooms/test/%s", zero[part]);
        else snprintf(name, sizeof name, "tilesets/%d.%s", resources[part], suffixes[part]);
        if (!feb_mzm_read_raw(repository, name, &blobs[part], &sizes[part], error)) {
            for (int other = 0; other < part; ++other) free(blobs[other]);
            return false;
        }
    }
    return true;
}

bool feb_mzm_tileset(const char *repository, int64_t index, FebMzmTileset *tileset,
                     FebError *error) {
    *tileset = (FebMzmTileset){0};
    uint8_t *blobs[3];
    size_t sizes[3];
    if (!feb_mzm_tileset_blobs(repository, index, blobs, sizes, error)) return false;
    bool ok = feb_lz77(blobs[0], sizes[0], FEB_MZM_MAX_RAW, &tileset->gfx, &tileset->gfx_size,
                       error);
    if (ok) {
        tileset->palette = blobs[1];
        tileset->palette_size = sizes[1];
        blobs[1] = NULL;
        ok = metatiles(blobs[2], sizes[2], tileset, error);
    }
    for (int part = 0; part < 3; ++part) free(blobs[part]);
    if (!ok) feb_mzm_tileset_free(tileset);
    return ok;
}

bool feb_mzm_graphic_base(const FebMzmTileset *tileset, const FebBlockMap *bg1, int64_t *base,
                          int64_t *coverage, FebError *error) {
    /* Charbase 1 -> 0x5800 - 0x4000 = 192 tiles; charbase 0 -> 704 tiles. */
    int64_t tiles = (int64_t)(tileset->gfx_size / 32), counts[2] = {0, 0};
    const int64_t bases[2] = {192, 704};
    uint8_t *seen = calloc(65536, 1);
    if (!seen) return feb_oom(error);
    int64_t total = bg1->width * bg1->height;
    for (int64_t index = 0; index < total; ++index) {
        uint16_t block = bg1->blocks[index];
        if (seen[block] || block >= tileset->entries) continue;
        seen[block] = 1;
        for (int word = 0; word < 4; ++word) {
            int64_t id = tileset->table[block][word] & 1023;
            for (int candidate = 0; candidate < 2; ++candidate)
                if (bases[candidate] <= id && id < bases[candidate] + tiles) ++counts[candidate];
        }
    }
    free(seen);
    int best = counts[1] > counts[0] ? 1 : 0;
    if (!counts[best])
        return feb_fail(error, FEB_EXIT_OPERATION, "no BG metatile indices refer to tileset graphics");
    *base = bases[best];
    *coverage = counts[best];
    return true;
}

bool feb_mzm_render_layer(int64_t width, int64_t height, const uint16_t *blocks,
                          const FebMzmTileset *tileset, int64_t base, uint8_t **rgb,
                          uint8_t *visibility, int64_t *unresolved, int64_t *painted,
                          FebError *error) {
    int64_t pixel_width = width * 16, pixel_height = height * 16;
    *rgb = calloc((size_t)(pixel_width * pixel_height), 3);
    if (!*rgb) return feb_oom(error);
    int64_t tiles = (int64_t)(tileset->gfx_size / 32);
    *unresolved = *painted = 0;
    for (int64_t block_y = 0; block_y < height; ++block_y) {
        for (int64_t block_x = 0; block_x < width; ++block_x) {
            uint16_t block = blocks[block_y * width + block_x];
            if (block >= tileset->entries) {
                ++*unresolved;
                continue;
            }
            for (int cell = 0; cell < 4; ++cell) {
                uint16_t word = tileset->table[block][cell];
                int64_t tile = (word & 1023) - base;
                int bank = (word >> 12) & 15;
                if (tile < 0 || tile >= tiles || bank < 3) {
                    ++*unresolved;
                    continue;
                }
                for (int y = 0; y < 8; ++y) {
                    for (int x = 0; x < 8; ++x) {
                        int px = word & 0x400 ? 7 - x : x, py = word & 0x800 ? 7 - y : y;
                        uint8_t byte = tileset->gfx[tile * 32 + py * 4 + px / 2];
                        int color = (byte >> (4 * (px & 1))) & 15;
                        /* Source palette rows 1..13 become BG rows 3..15. */
                        size_t entry = (size_t)(bank - 2) * 16 + (size_t)color;
                        if (entry * 2 + 2 > tileset->palette_size) {
                            ++*unresolved;
                            continue;
                        }
                        unsigned value = tileset->palette[entry * 2] |
                                         (tileset->palette[entry * 2 + 1] << 8);
                        size_t pixel = (size_t)((block_y * 16 + (cell / 2) * 8 + y) * pixel_width +
                                                block_x * 16 + (cell % 2) * 8 + x);
                        for (int channel = 0; channel < 3; ++channel)
                            (*rgb)[pixel * 3 + channel] =
                                (uint8_t)(((value >> (5 * channel)) & 31) * 255 / 31);
                        if (visibility && color) visibility[pixel] = 1;
                        ++*painted;
                    }
                }
            }
        }
    }
    return true;
}

bool feb_mzm_collision_preview(const FebBlockMap *map, uint8_t **bmp, size_t *size,
                               FebError *error) {
    int64_t width = map->width * 16, height = map->height * 16;
    uint8_t *rgb = calloc((size_t)(width * height), 3);
    if (!rgb) return feb_oom(error);
    for (int64_t block_y = 0; block_y < map->height; ++block_y) {
        for (int64_t block_x = 0; block_x < map->width; ++block_x) {
            uint16_t value = map->blocks[block_y * map->width + block_x];
            uint8_t color[3] = {235, 55, 75};
            if (value == 0) memset(color, 0, 3);
            else if (value >= 6 && value <= 11) memcpy(color, (uint8_t[]){245, 178, 52}, 3);
            else if (value >= 33 && value <= 37) memcpy(color, (uint8_t[]){185, 88, 245}, 3);
            for (int y = 0; y < 16; ++y)
                for (int x = 0; x < 16; ++x) {
                    uint8_t *pixel = rgb + ((block_y * 16 + y) * width + block_x * 16 + x) * 3;
                    for (int channel = 0; channel < 3; ++channel)
                        pixel[channel] = x == 0 || y == 0 ? color[channel] / 2 : color[channel];
                }
        }
    }
    bool ok = feb_bmp24(width, height, rgb, bmp, size, error);
    free(rgb);
    return ok;
}

static bool write_bitmap(const char *repository, const char *relative, int64_t width,
                         int64_t height, const uint8_t *rgb, bool write, FebError *error) {
    uint8_t *bmp = NULL;
    size_t size = 0;
    if (!feb_bmp24(width, height, rgb, &bmp, &size, error)) return false;
    bool ok = !write || feb_write_generated(repository, relative, bmp, size, NULL, error);
    free(bmp);
    return ok;
}

static bool put_strings(FusionJson *object, const char *key, const char *const *values,
                        size_t count) {
    FusionJson array = fusion_json_array();
    for (size_t index = 0; index < count; ++index)
        if (!fusion_json_push(&array, fusion_json_string(values[index]))) {
            fusion_json_free(&array);
            return false;
        }
    return feb_put(object, key, array);
}

bool feb_mzm_decode_room(const char *repository, const char *area, int64_t number,
                         bool write_outputs, FusionJson *result, FebError *error) {
    FusionJson room;
    if (!feb_mzm_source_room(repository, area, number, &room, error)) return false;
    const FusionJson *fields = fusion_json_member(&room, "fields");
    const char *tileset_text = fusion_json_member(fields, "tileset")->string;
    char *tail = NULL;
    long tileset_id = strtol(tileset_text, &tail, 10);
    FebMzmTileset tileset = {0};
    FebBlockMap maps[2] = {{0}, {0}}, clip = {0};
    bool decoded[2] = {false, false};
    uint8_t *pixels[2] = {NULL, NULL}, *masks[2] = {NULL, NULL};
    FusionJson layers = fusion_json_object();
    *result = fusion_json_object();
    char lower[64];
    snprintf(lower, sizeof lower, "%s", area);
    for (char *c = lower; *c; ++c) *c = (char)tolower((unsigned char)*c);
    bool ok = (*tail == '\0' && tail != tileset_text) ||
              feb_fail(error, FEB_EXIT_OPERATION, "invalid literal for int() with base 10: '%s'",
                       tileset_text);
    ok = ok && feb_mzm_tileset(repository, tileset_id, &tileset, error);
    if (ok && tileset.gfx_size % 32)
        ok = feb_fail(error, FEB_EXIT_OPERATION, "tileset is not 4bpp 8x8 aligned");
    static const char *const names[2] = {"Bg1", "Bg2"};
    for (int layer = 0; ok && layer < 2; ++layer) {
        char key[16], prop[16];
        snprintf(key, sizeof key, "p%sData", names[layer]);
        snprintf(prop, sizeof prop, "bg%dProp", layer + 1);
        const FusionJson *property = fusion_json_member(fields, prop);
        const FusionJson *source = fusion_json_member(fields, key);
        FusionJson entry = fusion_json_object(), copy = fusion_json_null();
        if (!property || strcmp(property->string, "BG_PROP_RLE_COMPRESSED")) {
            ok = (feb_put_string(&entry, "status", "NOT_RLE") &&
                  (!source || fusion_json_clone(source, &copy)) && feb_put(&entry, "source", copy) &&
                  feb_put(&layers, names[layer], entry)) || feb_oom(error);
            continue;
        }
        if (!feb_mzm_room_map(repository, source->string, &maps[layer], error)) {
            fusion_json_free(&entry);
            ok = false;
            break;
        }
        decoded[layer] = true;
        int64_t total = maps[layer].width * maps[layer].height, unique = 0;
        uint8_t *seen = calloc(65536, 1);
        for (int64_t index = 0; seen && index < total; ++index)
            if (!seen[maps[layer].blocks[index]]) {
                seen[maps[layer].blocks[index]] = 1;
                ++unique;
            }
        free(seen);
        ok = (feb_put_string(&entry, "status", "DECODED_METATILES") &&
              feb_put_int(&entry, "width_blocks", maps[layer].width) &&
              feb_put_int(&entry, "height_blocks", maps[layer].height) &&
              feb_put_int(&entry, "block_count", total) &&
              feb_put_int(&entry, "unique_blocks", unique) &&
              feb_put(&layers, names[layer], entry)) || feb_oom(error);
    }
    if (ok && !decoded[0]) ok = feb_fail(error, FEB_EXIT_OPERATION, "room BG1 RLE not available");
    int64_t base = 0, coverage = 0;
    ok = ok && feb_mzm_graphic_base(&tileset, &maps[0], &base, &coverage, error);
    if (ok) {
        static const char *const limitations[] = {
            "common GBA tiles and palette rows 0..2 not decoded",
            "BG0, BG3, blending, animated graphics and entities omitted",
            "rendered BG1/BG2 images are independent opaque diagnostic previews"};
        ok = (feb_put_string(result, "format", "MV_MZM_RENDER_1") &&
              feb_put_string(result, "room_id", fusion_json_member(&room, "native_id")->string) &&
              feb_put_string(result, "area", fusion_json_member(&room, "area")->string) &&
              feb_put_int(result, "index", number) && feb_put_int(result, "tileset", tileset_id) &&
              feb_put_string(result, "native_music",
                             fusion_json_member(fields, "musicTrack")->string) &&
              feb_put_string(result, "status", "PARTIAL_NATIVE_BG_RENDER") &&
              feb_put_int(result, "graphics_tile_base", base) &&
              feb_put_int(result, "graphics_tiles", (int64_t)(tileset.gfx_size / 32)) &&
              feb_put_int(result, "tileset_entries", (int64_t)tileset.entries) &&
              feb_put_int(result, "sample_index_coverage", coverage) &&
              feb_put(result, "layers", layers) &&
              put_strings(result, "limitations", limitations, 3)) || feb_oom(error);
        layers = fusion_json_null();
    }
    for (int layer = 0; ok && layer < 2; ++layer) {
        if (!decoded[layer]) continue;
        int64_t width = maps[layer].width, height = maps[layer].height, missing = 0, painted = 0;
        masks[layer] = calloc((size_t)(width * 16 * height * 16), 1);
        if (!masks[layer]) {
            ok = feb_oom(error);
            break;
        }
        ok = feb_mzm_render_layer(width, height, maps[layer].blocks, &tileset, base,
                                  &pixels[layer], masks[layer], &missing, &painted, error);
        char relative[160];
        snprintf(relative, sizeof relative, "rooms/metroid/previews/%s_%03lld_%s.bmp", lower,
                 (long long)number, layer ? "bg2" : "bg1");
        ok = ok && write_bitmap(repository, relative, width * 16, height * 16, pixels[layer],
                                write_outputs, error);
        FusionJson *entry = ok ? fusion_json_member_mut(fusion_json_member_mut(result, "layers"),
                                                        names[layer])
                               : NULL;
        ok = ok && ((feb_put_string(entry, "path", relative) &&
                     feb_put_int(entry, "unresolved_pixels_or_cells", missing) &&
                     feb_put_int(entry, "painted_pixels", painted)) || feb_oom(error));
    }
    FusionJson composite = fusion_json_object();
    if (ok && decoded[1] && maps[0].width == maps[1].width && maps[0].height == maps[1].height) {
        int64_t width = maps[0].width * 16, height = maps[0].height * 16;
        size_t length = (size_t)(width * height);
        uint8_t *merged = calloc(length, 3), *mask = calloc(length, 1);
        int64_t front = 0, behind = 0, unknown = 0;
        if (!merged || !mask) ok = feb_oom(error);
        for (size_t index = 0; ok && index < length; ++index) {
            if (masks[0][index]) {
                memcpy(merged + index * 3, pixels[0] + index * 3, 3);
                ++front;
            } else if (masks[1][index]) {
                memcpy(merged + index * 3, pixels[1] + index * 3, 3);
                ++behind;
            } else {
                ++unknown;
            }
            mask[index] = masks[0][index] || masks[1][index];
        }
        char relative[160], mask_relative[160];
        snprintf(relative, sizeof relative, "rooms/metroid/previews/%s_%03lld_bg12_composite.bmp",
                 lower, (long long)number);
        snprintf(mask_relative, sizeof mask_relative,
                 "rooms/metroid/previews/%s_%03lld_bg12_visibility.mask", lower,
                 (long long)number);
        ok = ok && write_bitmap(repository, relative, width, height, merged, write_outputs, error);
        ok = ok && (!write_outputs ||
                    feb_write_generated(repository, mask_relative, mask, length, NULL, error));
        ok = ok && ((feb_put_string(&composite, "visibility_mask_path", mask_relative) &&
                     feb_put_string(&composite, "status", "PARTIAL_BG1_OVER_BG2_DIAGNOSTIC") &&
                     feb_put_string(&composite, "path", relative) &&
                     feb_put_int(&composite, "width", width) &&
                     feb_put_int(&composite, "height", height) &&
                     feb_put_int(&composite, "bg1_visible_pixels", front) &&
                     feb_put_int(&composite, "bg2_visible_pixels", behind) &&
                     feb_put_int(&composite, "unresolved_pixels", unknown) &&
                     feb_put_string(&composite, "caveat",
                                    "BG1-over-BG2 order is diagnostic, not verified GBA hardware "
                                    "priority; BG0/BG3, sprites, common gfx, palette effects and "
                                    "blending unavailable")) ||
                    feb_oom(error));
        free(merged);
        free(mask);
    } else if (ok) {
        ok = (feb_put_string(&composite, "status", "UNAVAILABLE") &&
              feb_put_string(&composite, "reason",
                             "BG2 not decoded or dimensions differ from BG1; no invented placement")) ||
             feb_oom(error);
    }
    if (ok) {
        ok = feb_put(result, "composite", composite) || feb_oom(error);
        composite = fusion_json_null();
    }
    fusion_json_free(&composite);
    ok = ok && feb_mzm_room_map(repository, fusion_json_member(fields, "pClipData")->string, &clip,
                                error);
    if (ok) {
        char relative[160];
        snprintf(relative, sizeof relative, "rooms/metroid/previews/%s_%03lld_collision.bmp",
                 lower, (long long)number);
        uint8_t *bmp = NULL;
        size_t size = 0;
        ok = feb_mzm_collision_preview(&clip, &bmp, &size, error) &&
             (!write_outputs || feb_write_generated(repository, relative, bmp, size, NULL, error));
        free(bmp);
        FusionJson collision = fusion_json_object();
        ok = ok && ((feb_put_string(&collision, "path", relative) &&
                     feb_put_int(&collision, "width_blocks", clip.width) &&
                     feb_put_int(&collision, "height_blocks", clip.height) &&
                     feb_put_string(&collision, "meaning",
                                    "diagnostic native Clipdata IDs; not an editable layer") &&
                     feb_put(result, "collision", collision)) || feb_oom(error));
    }
    ok = ok && (feb_put_bool(result, "outputs_written", write_outputs) || feb_oom(error));
    if (ok && write_outputs) {
        char relative[160], *text = NULL;
        size_t size = 0;
        snprintf(relative, sizeof relative, "rooms/metroid/previews/%s_%03lld.json", lower,
                 (long long)number);
        ok = fusion_json_write(result, 2, &text, &size) || feb_oom(error);
        char *line = ok ? realloc(text, size + 1) : NULL;
        if (ok && !line) ok = feb_oom(error);
        if (ok) {
            line[size] = '\n';
            ok = feb_write_generated(repository, relative, (uint8_t *)line, size + 1, NULL, error);
            text = line;
        }
        free(text);
    }
    fusion_json_free(&layers);
    for (int layer = 0; layer < 2; ++layer) {
        feb_block_map_free(&maps[layer]);
        free(pixels[layer]);
        free(masks[layer]);
    }
    feb_block_map_free(&clip);
    feb_mzm_tileset_free(&tileset);
    fusion_json_free(&room);
    if (!ok) fusion_json_free(result);
    return ok;
}
