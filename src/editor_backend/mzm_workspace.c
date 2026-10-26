/* SPDX-License-Identifier: GPL-3.0-only */
/* The private editable Zero Mission workroom (formerly
 * scripts/mzm_native_workspace.py) and what it exports beside the block
 * maps: the experimental BG3 preview (scripts/mzm_bg3_preview.py), the native
 * room annotations (scripts/room_annotations.py, scripts/mzm_hatch_events.py)
 * and the common hatch previews (scripts/mzm_hatch_preview.py). The ROM and
 * the upstream raw files are never patched. */
#define _DEFAULT_SOURCE
#include "mzm_workspace.h"

#include "fusion/assets/rom.h"
#include "generated.h"
#include "mzm_render.h"
#include "object_catalog.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT(array) (sizeof (array) / sizeof (array)[0])

/* A growable text buffer. */
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
        size_t capacity = buffer->capacity ? buffer->capacity * 2 : 4096;
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

static char *read_source(const char *repository, const char *relative, FebError *error) {
    char path[FEB_PATH_CAPACITY], *text = NULL;
    if (!feb_join(path, repository, relative) || !feb_read_text(path, 64 * 1024 * 1024, &text, NULL)) {
        feb_fail(error, FEB_EXIT_OPERATION, "[Errno 2] No such file or directory: '%s'", path);
        return NULL;
    }
    return text;
}

static const char *skip_space(const char *cursor) {
    while (*cursor && isspace((unsigned char)*cursor)) ++cursor;
    return cursor;
}

/* The end of "(.*?)\n\s*<closing>" from ``from``: the first newline whose
 * following whitespace run reaches ``closing``. */
static const char *block_end(const char *from, const char *closing, bool spaces) {
    for (const char *line = strchr(from, '\n'); line; line = strchr(line + 1, '\n')) {
        const char *next = spaces ? skip_space(line + 1) : line + 1;
        if (!strncmp(next, closing, strlen(closing))) return line;
    }
    return NULL;
}

static bool is_decimal_text(const char *text) {
    if (!*text) return false;
    for (; *text; ++text)
        if (!isdigit((unsigned char)*text)) return false;
    return true;
}

/* ---- BG3 -------------------------------------------------------------- */

static bool bg3_blob(const char *repository, const char *symbol, uint8_t **data, size_t *size,
                     FebError *error) {
    if (!strcmp(symbol, "sSaveRoom_Bg3"))
        return feb_mzm_read_raw(repository, "rooms/save_room_bg3.rle", data, size, error);
    if (!strcmp(symbol, "sMapRoom_Bg3"))
        return feb_mzm_read_raw(repository, "rooms/map_room_bg3.rle", data, size, error);
    if (!strcmp(symbol, "sBg3_Empty"))
        return feb_mzm_read_raw(repository, "rooms/bg3_empty.gfx.lz", data, size, error);
    /* s([A-Za-z0-9]+)_Bg3_(\d+) */
    const char *area = symbol + 1, *cursor = area;
    while (isalnum((unsigned char)*cursor)) ++cursor;
    const char *area_end = cursor;
    if (symbol[0] == 's' && area_end > area && !strncmp(cursor, "_Bg3_", 5) &&
        is_decimal_text(cursor + 5)) {
        char lower[64], name[192];
        snprintf(lower, sizeof lower, "%.*s", (int)(area_end - area), area);
        for (char *c = lower; *c; ++c) *c = (char)tolower((unsigned char)*c);
        snprintf(name, sizeof name, "rooms/%s/%s_bg3_%s.gfx", lower, lower, cursor + 5);
        return feb_mzm_read_raw(repository, name, data, size, error);
    }
    return feb_fail(error, FEB_EXIT_OPERATION, "BG3 resource not decoded: %s", symbol);
}

/* tileset_background_resource(). */
static bool background_graphics(const char *repository, int64_t tileset, uint8_t **data,
                                size_t *size, FebError *error) {
    char *source = read_source(repository, "third_party/mzm/src/data/rooms_data.c", error);
    if (!source) return false;
    const char *marker = "const struct TilesetEntry sTilesetEntries[79] = {";
    char *table = strstr(source, marker);
    bool ok = table != NULL || feb_fail(error, FEB_EXIT_OPERATION, "list index out of range");
    if (ok) {
        table += strlen(marker);
        char *end = strstr(table, "\n};");
        if (end) *end = '\0';
    }
    /* \[N\]\s*=\s*\{([^{}]+)\} */
    char label[32], symbol[96] = "";
    snprintf(label, sizeof label, "[%lld]", (long long)tileset);
    const char *body = NULL, *finish = NULL;
    for (const char *probe = ok ? strstr(table, label) : NULL; probe && !body;
         probe = strstr(probe + 1, label)) {
        const char *cursor = skip_space(probe + strlen(label));
        if (*cursor != '=') continue;
        cursor = skip_space(cursor + 1);
        if (*cursor != '{') continue;
        const char *close = cursor + 1;
        while (*close && *close != '{' && *close != '}') ++close;
        if (*close != '}' || close == cursor + 1) continue;
        body = cursor + 1;
        finish = close;
    }
    if (ok && !body)
        ok = feb_fail(error, FEB_EXIT_OPERATION, "no native tileset entry for %lld",
                      (long long)tileset);
    if (ok) {
        const char *field = NULL;
        for (const char *probe = body; probe < finish && !field; ++probe) {
            if (strncmp(probe, ".pBackgroundGraphics", 20)) continue;
            const char *cursor = probe + 20;
            while (cursor < finish && isspace((unsigned char)*cursor)) ++cursor;
            if (cursor >= finish || *cursor != '=') continue;
            ++cursor;
            while (cursor < finish && isspace((unsigned char)*cursor)) ++cursor;
            if (cursor >= finish || *cursor != 's') continue;
            const char *end = cursor + 1;
            while (end < finish && (isalnum((unsigned char)*end) || *end == '_')) ++end;
            if (end == cursor + 1) continue;
            snprintf(symbol, sizeof symbol, "%.*s", (int)(end - cursor), cursor);
            field = cursor;
        }
        if (!field)
            ok = feb_fail(error, FEB_EXIT_OPERATION, "no background tileset reference for %lld",
                          (long long)tileset);
    }
    free(source);
    if (!ok) return false;
    if (!strcmp(symbol, "sTileset_0_Bg_Gfx"))
        return feb_mzm_read_raw(repository, "rooms/tileset_0_background.gfx.lz", data, size,
                                error);
    const char *digits = symbol + 9, *cursor = digits;
    while (isdigit((unsigned char)*cursor)) ++cursor;
    if (strncmp(symbol, "sTileset_", 9) || cursor == digits || strcmp(cursor, "_Bg_Gfx"))
        return feb_fail(error, FEB_EXIT_OPERATION, "unknown BG tileset resource %s", symbol);
    char name[96];
    snprintf(name, sizeof name, "tilesets/%.*s_bg.gfx.lz", (int)(cursor - digits), digits);
    return feb_mzm_read_raw(repository, name, data, size, error);
}

/* preview(): the BG3 text map, its BMP and its report. */
static bool bg3_preview(const uint8_t *blob, size_t blob_size, const uint8_t *background,
                        size_t background_size, const uint8_t *palette, size_t palette_size,
                        uint8_t **bmp, size_t *bmp_size, FusionJson *report, FebError *error) {
    if (blob_size < 9) return feb_fail(error, FEB_EXIT_OPERATION, "BG3 header too short");
    uint8_t *map = NULL, *gfx = NULL;
    size_t map_size = 0, gfx_size = 0;
    if (!feb_lz77(blob + 4, blob_size - 4, FEB_MZM_MAX_RAW, &map, &map_size, error)) return false;
    bool ok = map_size == 2048 || map_size == 4096 ||
              feb_fail(error, FEB_EXIT_OPERATION, "unexpected BG3 tilemap size %zu", map_size);
    ok = ok && feb_lz77(background, background_size, FEB_MZM_MAX_RAW, &gfx, &gfx_size, error);
    if (ok && (!gfx_size || gfx_size % 32))
        ok = feb_fail(error, FEB_EXIT_OPERATION, "BG3 background graphics not 4bpp tile-aligned");
    /* BG3 charbase 2 is VRAM+0x8000; RoomLoadTileset puts the background
     * graphics so that they end at VRAM+0xfde0. */
    int64_t base_byte = ok ? 0xfde0 - (int64_t)gfx_size - 0x8000 : 0;
    if (ok && (base_byte < 0 || base_byte % 32))
        ok = feb_fail(error, FEB_EXIT_OPERATION, "BG3 charbase offset not tile aligned");
    if (!ok) {
        free(map);
        free(gfx);
        return false;
    }
    int64_t base = base_byte / 32, tiles = (int64_t)(gfx_size / 32), cells = (int64_t)map_size / 2;
    int64_t referenced = 0, missing = 0, common = 0, own = 0, candidates[4] = {0, 0, 0, 0};
    const int64_t candidate_bases[4] = {0, 192, 704, base};
    bool banks[16] = {false};
    for (int64_t cell = 0; cell < cells; ++cell) {
        unsigned word = map[cell * 2] | (map[cell * 2 + 1] << 8);
        int64_t index = word & 1023;
        unsigned bank = (word >> 12) & 15;
        banks[bank] = true;
        if (base <= index && index < base + tiles) ++referenced;
        else ++missing;
        if (bank < 3) ++common;
        else ++own;
        for (int candidate = 0; candidate < 4; ++candidate)
            if (candidate_bases[candidate] <= index && index < candidate_bases[candidate] + tiles)
                ++candidates[candidate];
    }
    int64_t width = 32, height = cells / 32;
    uint8_t *pixels = calloc((size_t)(width * 8 * height * 8), 3);
    if (!pixels) {
        free(map);
        free(gfx);
        return feb_oom(error);
    }
    int64_t visible = 0, unresolved = 0, missing_palette = 0, transparent = 0, missing_graphics = 0;
    for (int64_t cell = 0; cell < cells; ++cell) {
        unsigned word = map[cell * 2] | (map[cell * 2 + 1] << 8);
        int64_t tile = (int64_t)(word & 1023) - base;
        unsigned bank = word >> 12;
        if (tile < 0 || tile >= tiles) {
            ++missing_graphics;
            ++unresolved;
            continue;
        }
        int64_t x0 = (cell % width) * 8, y0 = (cell / width) * 8;
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) {
                int tx = word & 0x400 ? 7 - x : x, ty = word & 0x800 ? 7 - y : y;
                int index = (gfx[tile * 32 + ty * 4 + tx / 2] >> (4 * (tx & 1))) & 15;
                if (!index) {
                    ++transparent;
                    continue;
                }
                size_t entry = (size_t)(bank - 2) * 16 + (size_t)index;
                if (bank < 3 || entry * 2 + 2 > palette_size) {
                    ++missing_palette;
                    continue;
                }
                unsigned value = palette[entry * 2] | (palette[entry * 2 + 1] << 8);
                uint8_t *pixel = pixels + ((y0 + y) * width * 8 + x0 + x) * 3;
                for (int channel = 0; channel < 3; ++channel)
                    pixel[channel] = (uint8_t)(((value >> (5 * channel)) & 31) * 255 / 31);
                ++visible;
            }
    }
    /* sorted(banks) and the candidate coverage, as Python prints them. */
    Buffer bank_text = {0}, coverage_text = {0};
    FusionJson bank_list = fusion_json_array(), coverage = fusion_json_object();
    append(&bank_text, "[");
    for (unsigned bank = 0; bank < 16; ++bank) {
        if (!banks[bank]) continue;
        append(&bank_text, "%s%u", bank_text.size > 1 ? ", " : "", bank);
        ok = ok && fusion_json_push(&bank_list, fusion_json_int(bank));
    }
    append(&bank_text, "]");
    append(&coverage_text, "{");
    for (int candidate = 0; candidate < 4; ++candidate) {
        char key[32];
        snprintf(key, sizeof key, "%lld", (long long)candidate_bases[candidate]);
        if (!fusion_json_member(&coverage, key))
            append(&coverage_text, "%s'%s': %lld", coverage_text.size > 1 ? ", " : "", key,
                   (long long)candidates[candidate]);
        ok = ok && feb_put_int(&coverage, key, candidates[candidate]);
    }
    append(&coverage_text, "}");
    char vram[32];
    snprintf(vram, sizeof vram, "0x%llx", (long long)(0xfde0 - (int64_t)gfx_size));
    *report = fusion_json_object();
    ok = ok && !bank_text.failed && !coverage_text.failed &&
         feb_put_int(report, "total_cells", cells) &&
         feb_put_int(report, "referenced_graphics_cells", referenced) &&
         feb_put_int(report, "missing_graphics_cells", missing) &&
         feb_put_int(report, "common_palette_cells", common) &&
         feb_put_int(report, "tileset_palette_cells", own) &&
         feb_put(report, "palette_banks", bank_list) &&
         feb_put_int(report, "graphics_base_tile", base) &&
         feb_put_int(report, "graphics_tile_count", tiles) &&
         feb_put(report, "candidate_base_coverage", coverage) &&
         feb_put_int(report, "bg3_charbase", 2) && feb_put_string(report, "graphics_vram_start", vram) &&
         feb_put_string(report, "status", visible ? "EXPERIMENTAL_BG3_TEXT_MAP" : "BG3_UNRESOLVED") &&
         feb_put_int(report, "visible_pixels", visible) &&
         feb_put_int(report, "unresolved_tiles", unresolved + missing_palette) &&
         feb_put_int(report, "missing_graphics_cells", missing_graphics) &&
         feb_put_int(report, "missing_palette_pixels", missing_palette) &&
         feb_put_int(report, "transparent_pixels", transparent) &&
         feb_put_int(report, "width", width * 8) && feb_put_int(report, "height", height * 8) &&
         feb_put_string(report, "caveat",
                        "BG3 rendered using character block 2 and native loader graphics region. "
                        "BG3CNT register state, priority/scroll and common palette rows 0..2 "
                        "remain unverified or unavailable");
    if (!ok && !error->message[0]) feb_oom(error);
    if (ok && !visible)
        ok = feb_fail(error, FEB_EXIT_OPERATION,
                      "BG3 has no validated visible pixels: graphics-missing cells=%lld/%lld; "
                      "palette-missing pixels=%lld; transparent pixels=%lld; palette banks=%s; "
                      "candidate base coverage=%s. Some BG3 graphics or GBA palette resources are "
                      "unresolved; do not invent pixels or palette colors.",
                      (long long)missing_graphics, (long long)cells, (long long)missing_palette,
                      (long long)transparent, bank_text.data, coverage_text.data);
    ok = ok && feb_bmp24(width * 8, height * 8, pixels, bmp, bmp_size, error);
    if (!ok) fusion_json_free(report);
    free(bank_text.data);
    free(coverage_text.data);
    free(pixels);
    free(map);
    free(gfx);
    return ok;
}

/* render_room() of the BG3 preview. */
static bool bg3_render(const char *repository, const char *area, int64_t number,
                       FusionJson *report, FebError *error) {
    FusionJson room;
    if (!feb_mzm_source_room(repository, area, number, &room, error)) return false;
    const FusionJson *fields = fusion_json_member(&room, "fields");
    const FusionJson *symbol = fusion_json_member(fields, "pBg3Data");
    const char *tileset_text = fusion_json_member(fields, "tileset")->string;
    int64_t tileset = strtoll(tileset_text, NULL, 10);
    uint8_t *blobs[3] = {NULL, NULL, NULL}, *blob = NULL, *background = NULL, *bmp = NULL;
    size_t sizes[3], blob_size = 0, background_size = 0, bmp_size = 0;
    bool ok = symbol || feb_fail(error, FEB_EXIT_OPERATION, "'pBg3Data'");
    ok = ok && feb_mzm_tileset_blobs(repository, tileset, blobs, sizes, error) &&
         bg3_blob(repository, symbol->string, &blob, &blob_size, error) &&
         background_graphics(repository, tileset, &background, &background_size, error) &&
         bg3_preview(blob, blob_size, background, background_size, blobs[1], sizes[1], &bmp,
                     &bmp_size, report, error);
    if (ok) {
        char lower[64], relative[160];
        snprintf(lower, sizeof lower, "%s", area);
        for (char *c = lower; *c; ++c) *c = (char)tolower((unsigned char)*c);
        snprintf(relative, sizeof relative, "rooms/metroid/previews/%s_%03lld_bg3.bmp", lower,
                 (long long)number);
        ok = feb_write_generated(repository, relative, bmp, bmp_size, NULL, error) &&
             ((feb_put_string(report, "path", relative) &&
               feb_put_string(report, "source_symbol", symbol->string)) || feb_oom(error));
        if (!ok) fusion_json_free(report);
    }
    for (int part = 0; part < 3; ++part) free(blobs[part]);
    free(blob);
    free(background);
    free(bmp);
    fusion_json_free(&room);
    return ok;
}

/* ---- Common hatch previews -------------------------------------------- */

static const struct {
    const char *style;
    int base;
} hatch_bases[] = {{"gray", 0x90},          {"normal", 0x92},       {"missile", 0x94},
                   {"super_missile", 0x96}, {"power_bomb", 0x98},   {"no_hatch", 0x9A},
                   {"locked_navigation", 0x2A}};

/* common_resource(): the private raw extraction, or the verified ROM. */
static bool common_resource(const char *repository, const char *name, size_t expected,
                            uint8_t **data, FebError *error) {
    char relative[160], path[FEB_PATH_CAPACITY];
    snprintf(relative, sizeof relative, "metroid/raw/data/%s", name);
    snprintf(path, sizeof path, "%s/assets/extracted/%s", repository, relative);
    if (feb_is_symlink(path))
        return feb_fail(error, FEB_EXIT_OPERATION, "symlinked MZM private common resource");
    long long size = 0;
    if (feb_is_regular(path, &size)) {
        if ((size_t)size != expected)
            return feb_fail(error, FEB_EXIT_OPERATION, "unexpected common resource size");
        size_t read = 0;
        return feb_read_text(path, expected, (char **)data, &read) ||
               feb_fail(error, FEB_EXIT_OPERATION, "cannot read %s", path);
    }
    FusionJson database;
    char detail[256];
    if (!feb_join(path, repository, "third_party/mzm/database.json") ||
        !feb_load_json(path, 256 * 1024 * 1024, &database, detail, sizeof detail))
        return feb_fail(error, FEB_EXIT_OPERATION, "%s", detail);
    const FusionJson *entry = NULL;
    for (size_t index = 0; index < database.count && !entry; ++index) {
        const FusionJson *value = fusion_json_member(&database.items[index], "path");
        if (value && value->kind == FUSION_JSON_STRING && !strcmp(value->string, name))
            entry = &database.items[index];
    }
    bool ok = entry != NULL || feb_fail(error, FEB_EXIT_OPERATION, "'%s'", name);
    unsigned long offset = 0, total = 0;
    if (ok) {
        offset = strtoul(fusion_json_member(fusion_json_member(entry, "addr"), "us")->string, NULL,
                         16);
        const FusionJson *count = fusion_json_member(entry, "count");
        const FusionJson *width = fusion_json_member(entry, "size");
        total = strtoul(count->string, NULL, 16) *
                (width->kind == FUSION_JSON_NUMBER ? (unsigned long)width->integer
                                                   : strtoul(width->string, NULL, 10));
    }
    fusion_json_free(&database);
    if (!ok) return false;
    FusionRom rom = {0};
    if (!feb_join(path, repository, "roms/Metroid - Zero Mission (USA).gba") ||
        !fusion_rom_open(FUSION_ASSET_GAME_METROID, path, &rom, detail, sizeof detail))
        return feb_fail(error, FEB_EXIT_OPERATION, "%s", detail);
    if (total != expected || offset + total > rom.size) {
        fusion_rom_close(&rom);
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid original common resource address/size");
    }
    *data = malloc(total);
    if (*data) memcpy(*data, rom.data + offset, total);
    fusion_rom_close(&rom);
    return *data != NULL || feb_oom(error);
}

/* hatch_metatile_index(); -1 for an unsupported frame. */
static int hatch_index(int style, bool right, int state, int step) {
    bool no_hatch = !strcmp(hatch_bases[style].style, "no_hatch");
    if (state == 0) return hatch_bases[style].base + (right ? 1 : 0);
    int base = right ? 0x16 : 0x11, index;
    if (state == 1) {
        index = base + step - 1;
    } else {
        index = base + 3 - step;
        if (!no_hatch) index += 0x40;
    }
    if (no_hatch) index += 0x80;
    return index >= 0 && index <= 0xCF ? index : -1;
}

/* render_hatch(): the 16x64 BG1 hatch; false when it is not common art. */
static bool render_hatch(const uint8_t *tilemap, const uint8_t *graphics, const uint8_t *palette,
                         int first, uint8_t **png, size_t *size) {
    uint8_t pixels[16 * 64 * 4] = {0};
    for (int metatile_y = 0; metatile_y < 4; ++metatile_y) {
        int index = first + 16 * metatile_y;
        for (int quarter = 0; quarter < 4; ++quarter) {
            const uint8_t *entry = tilemap + index * 8 + quarter * 2;
            unsigned word = entry[0] | (entry[1] << 8);
            int tile = word & 1023, bank = (word >> 12) & 15;
            if (tile < 64 || tile >= 64 + 128 || bank > 2) return false;
            int start_x = (quarter % 2) * 8, start_y = metatile_y * 16 + (quarter / 2) * 8;
            for (int dy = 0; dy < 8; ++dy)
                for (int dx = 0; dx < 8; ++dx) {
                    int sx = word & 0x400 ? 7 - dx : dx, sy = word & 0x800 ? 7 - dy : dy;
                    uint8_t packed = graphics[(tile - 64) * 32 + sy * 4 + sx / 2];
                    int color = (packed >> (4 * (sx & 1))) & 15;
                    if (!color) continue;
                    const uint8_t *value = palette + (bank * 16 + color) * 2;
                    unsigned rgb = value[0] | (value[1] << 8);
                    uint8_t *pixel = pixels + ((start_y + dy) * 16 + start_x + dx) * 4;
                    for (int channel = 0; channel < 3; ++channel)
                        pixel[channel] = (uint8_t)(((rgb >> (5 * channel)) & 31) * 255 / 31);
                    pixel[3] = 255;
                }
        }
    }
    return feb_png_rgba(16, 64, pixels, png, size);
}

/* export_hatches(): the number of previews, or false with the reason. */
static bool export_hatches(const char *repository, int64_t *count, FebError *error) {
    *count = 0;
    for (int mother_ship = 0; mother_ship < 2; ++mother_ship) {
        const char *suffix = mother_ship ? "_mother_ship" : "";
        char names[3][96];
        snprintf(names[0], sizeof names[0], "common/common_tiles%s.gfx", suffix);
        snprintf(names[1], sizeof names[1], "common/common_tiles%s.pal", suffix);
        snprintf(names[2], sizeof names[2], "common/door_transition%s.pal", suffix);
        uint8_t *tilemap = NULL, *graphics = NULL, *first = NULL, *rest = NULL;
        bool ok = common_resource(repository, "common/common_tilemap.tt", 0x680, &tilemap, error) &&
                  common_resource(repository, names[0], 0x1000, &graphics, error) &&
                  common_resource(repository, names[1], 32, &first, error) &&
                  common_resource(repository, names[2], 0x1E0, &rest, error);
        uint8_t palette[96];
        if (ok) {
            memcpy(palette, first, 32);
            memcpy(palette + 32, rest, 64);
        }
        const char *folder = mother_ship ? "mothership" : "zebes";
        for (size_t style = 0; ok && style < COUNT(hatch_bases); ++style) {
            for (int right = 0; ok && right < 2; ++right) {
                char name[64], path[192];
                snprintf(name, sizeof name, "%s_%s", hatch_bases[style].style,
                         right ? "right" : "left");
                snprintf(path, sizeof path, "sprite_previews/mzm_hatches/%s/%s.png", folder, name);
                uint8_t *png = NULL;
                size_t size = 0;
                if (!render_hatch(tilemap, graphics, palette, hatch_index((int)style, right, 0, 0),
                                  &png, &size))
                    continue;
                ok = feb_write_generated(repository, path, png, size, NULL, error);
                free(png);
                ++*count;
                if (!ok || !strcmp(hatch_bases[style].style, "locked_navigation")) continue;
                for (int state = 1; ok && state <= 2; ++state) {
                    for (int step = 1; ok && step <= (state == 1 ? 4 : 3); ++step) {
                        int index = hatch_index((int)style, right, state, step);
                        if (index < 0) continue;
                        snprintf(path, sizeof path, "sprite_previews/mzm_hatches/%s/%s_%s_%d.png",
                                 folder, name, state == 1 ? "opening" : "closing", step);
                        png = NULL;
                        if (!render_hatch(tilemap, graphics, palette, index, &png, &size)) continue;
                        ok = feb_write_generated(repository, path, png, size, NULL, error);
                        free(png);
                        ++*count;
                    }
                }
            }
        }
        free(tilemap);
        free(graphics);
        free(first);
        free(rest);
        if (!ok) return false;
    }
    return *count ||
           feb_fail(error, FEB_EXIT_OPERATION, "no genuine common hatch graphics could be decoded");
}

/* ---- Native room annotations ------------------------------------------ */

static const char *const mzm_areas[] = {"Brinstar", "Kraid",  "Norfair", "Ridley",
                                        "Tourian",  "Crateria", "Chozodia"};

typedef struct {
    char name[96];
    int slot;
} SpritesetEntry;

typedef struct {
    int number;
    SpritesetEntry *entries;
    size_t count;
} Spriteset;

typedef struct {
    int variant;
    int (*items)[3];  /* y, x, index */
    size_t count;
} Placement;

typedef struct {
    char kind[16], variant[32], native_type[128], label[160];
    int64_t index, x, y, width, height;
    char *details;
} Row;

static void free_spritesets(Spriteset *sets, size_t count) {
    for (size_t index = 0; index < count; ++index) free(sets[index].entries);
    free(sets);
}

static void free_placements(Placement *placements, size_t count) {
    for (size_t index = 0; index < count; ++index) free(placements[index].items);
    free(placements);
}

/* SET_RE and SET_ITEM_RE of spriteset.c. */
static bool parse_spritesets(const char *source, Spriteset **sets, size_t *count, FebError *error) {
    *sets = NULL;
    *count = 0;
    const char *prefix = "const u8 sSpriteset";
    for (const char *cursor = source; (cursor = strstr(cursor, prefix)) != NULL;) {
        const char *digits = cursor + strlen(prefix), *probe = digits;
        cursor += 1;
        while (isdigit((unsigned char)*probe)) ++probe;
        if (probe == digits || *probe != '[') continue;
        const char *bracket = probe + 1;
        while (*bracket && *bracket != ']') ++bracket;
        if (bracket == probe + 1 || *bracket != ']') continue;
        const char *after = skip_space(bracket + 1);
        if (*after != '=') continue;
        after = skip_space(after + 1);
        if (*after != '{') continue;
        const char *body = after + 1, *end = block_end(body, "};", true);
        if (!end) continue;
        int number = atoi(digits);
        for (size_t index = 0; index < *count; ++index)
            if ((*sets)[index].number == number) {
                free_spritesets(*sets, *count);
                return feb_fail(error, FEB_EXIT_OPERATION, "duplicate MZM spriteset %d", number);
            }
        Spriteset *grown = realloc(*sets, (*count + 1) * sizeof *grown);
        if (!grown) {
            free_spritesets(*sets, *count);
            return feb_oom(error);
        }
        *sets = grown;
        Spriteset *set = &(*sets)[(*count)++];
        *set = (Spriteset){number, NULL, 0};
        for (const char *item = body; item < end;) {
            const char *found = strstr(item, "PSPRITE_");
            if (!found || found >= end) break;
            const char *name_end = found + 8;
            while (isupper((unsigned char)*name_end) || isdigit((unsigned char)*name_end) ||
                   *name_end == '_')
                ++name_end;
            const char *comma = name_end;
            while (comma < end && isspace((unsigned char)*comma)) ++comma;
            const char *slot = comma + 1;
            while (slot < end && isspace((unsigned char)*slot)) ++slot;
            if (name_end == found + 8 || *comma != ',' || !isdigit((unsigned char)*slot)) {
                item = found + 1;
                continue;
            }
            SpritesetEntry *entries = realloc(set->entries, (set->count + 1) * sizeof *entries);
            if (!entries) {
                free_spritesets(*sets, *count);
                return feb_oom(error);
            }
            set->entries = entries;
            snprintf(entries[set->count].name, sizeof entries[0].name, "%.*s",
                     (int)(name_end - found), found);
            entries[set->count].slot = atoi(slot);
            ++set->count;
            while (isdigit((unsigned char)*slot)) ++slot;
            item = slot;
        }
        cursor = end;
    }
    if (*count < 100) {
        free_spritesets(*sets, *count);
        *sets = NULL;
        return feb_fail(error, FEB_EXIT_OPERATION, "incomplete MZM spriteset table");
    }
    return true;
}

static const char *parse_number(const char *cursor, int *value) {
    if (!isdigit((unsigned char)*cursor)) return NULL;
    *value = atoi(cursor);
    while (isdigit((unsigned char)*cursor)) ++cursor;
    return cursor;
}

/* PLACEMENT_RE and PLACEMENT_ITEM_RE of one room source. */
static bool parse_placements(const char *source, const char *area, int room,
                             Placement **placements, size_t *count, FebError *error) {
    *placements = NULL;
    *count = 0;
    const char *prefix = "const u8 s";
    for (const char *cursor = source; (cursor = strstr(cursor, prefix)) != NULL;) {
        const char *name = cursor + strlen(prefix), *probe = name;
        cursor += 1;
        while (isalnum((unsigned char)*probe)) ++probe;
        const char *name_end = probe;
        int room_number = 0, variant = 0;
        if (name_end == name || *probe != '_') continue;
        probe = parse_number(probe + 1, &room_number);
        if (!probe || strncmp(probe, "_Spriteset", 10)) continue;
        probe = parse_number(probe + 10, &variant);
        if (!probe || *probe != '[') continue;
        const char *bracket = probe + 1;
        while (*bracket && *bracket != ']') ++bracket;
        if (bracket == probe + 1 || *bracket != ']') continue;
        const char *after = skip_space(bracket + 1);
        if (*after != '=') continue;
        after = skip_space(after + 1);
        if (*after != '{') continue;
        const char *body = after + 1, *end = block_end(body, "};", true);
        if (!end) continue;
        cursor = end;
        if ((size_t)(name_end - name) != strlen(area) || strncmp(name, area, strlen(area)) ||
            room_number != room)
            continue;
        for (size_t index = 0; index < *count; ++index)
            if ((*placements)[index].variant == variant) {
                free_placements(*placements, *count);
                return feb_fail(error, FEB_EXIT_OPERATION,
                                "duplicate MZM room placement variant");
            }
        Placement *grown = realloc(*placements, (*count + 1) * sizeof *grown);
        if (!grown) {
            free_placements(*placements, *count);
            return feb_oom(error);
        }
        *placements = grown;
        Placement *placement = &(*placements)[(*count)++];
        *placement = (Placement){variant, NULL, 0};
        /* (\d+)\s*,\s*(\d+)\s*,\s*SPRITESET_IDX\((\d+)\) */
        for (const char *item = body; item < end; ++item) {
            if (!isdigit((unsigned char)*item) || (item > body && isdigit((unsigned char)item[-1])))
                continue;
            int values[3];
            const char *next = parse_number(item, &values[0]);
            while (next < end && isspace((unsigned char)*next)) ++next;
            if (*next != ',') continue;
            next = skip_space(next + 1);
            next = parse_number(next, &values[1]);
            if (!next) continue;
            while (next < end && isspace((unsigned char)*next)) ++next;
            if (*next != ',') continue;
            next = skip_space(next + 1);
            if (strncmp(next, "SPRITESET_IDX(", 14)) continue;
            next = parse_number(next + 14, &values[2]);
            if (!next || *next != ')') continue;
            int (*items)[3] = realloc(placement->items, (placement->count + 1) * sizeof *items);
            if (!items) {
                free_placements(*placements, *count);
                return feb_oom(error);
            }
            placement->items = items;
            memcpy(items[placement->count++], values, sizeof values);
            item = next;
        }
    }
    return true;
}

typedef struct {
    int slot;
    char event[96], when[8], lock[16];
} LockRule;

/* load_hatch_lock_events(): the native hatch-slot lock conditions. */
static bool lock_rules(const char *repository, const char *area, int room, LockRule **rules,
                       size_t *count, FebError *error) {
    *rules = NULL;
    *count = 0;
    char *source = read_source(repository, "third_party/mzm/src/data/hatch_data.c", error);
    if (!source) return false;
    const char *prefix = "const struct HatchLockEvent sHatchLockEvents";
    const char *table = NULL, *table_end = NULL;
    int declared = 0, tables = 0;
    for (const char *cursor = source; (cursor = strstr(cursor, prefix)) != NULL;) {
        const char *name = cursor + strlen(prefix), *probe = name;
        cursor += 1;
        while (isalpha((unsigned char)*probe)) ++probe;
        int size = 0;
        if (probe == name || *probe != '[') continue;
        const char *next = parse_number(probe + 1, &size);
        if (!next || *next != ']') continue;
        next = skip_space(next + 1);
        if (*next != '=') continue;
        next = skip_space(next + 1);
        if (*next != '{') continue;
        const char *end = strstr(next + 1, "\n};");
        if (!end) continue;
        cursor = end;
        if ((size_t)(probe - name) != strlen(area) || strncmp(name, area, strlen(area))) continue;
        ++tables;
        table = next + 1;
        table_end = end;
        declared = size;
    }
    bool ok = true;
    if (tables > 1) ok = feb_fail(error, FEB_EXIT_OPERATION, "duplicate native hatch event tables");
    size_t records = 0;
    for (const char *open = table; ok && table && open < table_end; ++open) {
        if (*open != '{') continue;
        const char *close = open + 1;
        while (close < table_end && *close != '{' && *close != '}') ++close;
        if (close >= table_end || *close != '}') continue;
        ++records;
        open = close;
    }
    if (ok && table && (records != (size_t)declared || declared > 256))
        ok = feb_fail(error, FEB_EXIT_OPERATION, "incomplete native hatch event table");
    for (const char *open = table; ok && table && open < table_end; ++open) {
        if (*open != '{') continue;
        const char *close = open + 1;
        while (close < table_end && *close != '{' && *close != '}') ++close;
        if (close >= table_end || *close != '}') continue;
        /* FIELD: .name = [A-Za-z0-9_]+ (the last value of a key wins). */
        FusionJson fields = fusion_json_object();
        for (const char *dot = open + 1; ok && dot < close; ++dot) {
            if (*dot != '.' || !isalpha((unsigned char)dot[1])) continue;
            const char *key = dot + 1, *key_end = key;
            while (key_end < close && (isalnum((unsigned char)*key_end) || *key_end == '_')) ++key_end;
            const char *value = key_end;
            while (value < close && isspace((unsigned char)*value)) ++value;
            if (value >= close || *value != '=') continue;
            ++value;
            while (value < close && isspace((unsigned char)*value)) ++value;
            const char *value_end = value;
            while (value_end < close && (isalnum((unsigned char)*value_end) || *value_end == '_'))
                ++value_end;
            if (value_end == value) continue;
            char name[64], text[128];
            snprintf(name, sizeof name, "%.*s", (int)(key_end - key), key);
            snprintf(text, sizeof text, "%.*s", (int)(value_end - value), value);
            ok = feb_put_string(&fields, name, text) || feb_oom(error);
            dot = value_end - 1;
        }
        const FusionJson *room_value = fusion_json_member(&fields, "room");
        if (ok && (!room_value || !is_decimal_text(room_value->string)))
            ok = feb_fail(error, FEB_EXIT_OPERATION, "invalid native hatch lock room");
        if (ok && atoi(room_value->string) == room) {
            const FusionJson *type = fusion_json_member(&fields, "type");
            const FusionJson *event = fusion_json_member(&fields, "event");
            static const char *const types[][3] = {
                {"HATCH_LOCK_EVENT_TYPE_BEFORE", "BEFORE", "PERMANENT"},
                {"HATCH_LOCK_EVENT_TYPE_AFTER", "AFTER", "PERMANENT"},
                {"HATCH_LOCK_EVENT_TYPE_BEFORE_UNLOCKABLE", "BEFORE", "UNLOCKABLE"},
                {"HATCH_LOCK_EVENT_TYPE_AFTER_UNLOCKABLE", "AFTER", "UNLOCKABLE"}};
            int kind = -1;
            for (int index = 0; type && index < 4; ++index)
                if (!strcmp(types[index][0], type->string)) kind = index;
            bool event_ok = event && !strncmp(event->string, "EVENT_", 6) && event->string[6];
            for (const char *c = event_ok ? event->string + 6 : ""; *c; ++c)
                if (!(isupper((unsigned char)*c) || isdigit((unsigned char)*c) || *c == '_'))
                    event_ok = false;
            if (kind < 0 || !event_ok)
                ok = feb_fail(error, FEB_EXIT_OPERATION, "unsupported native hatch event condition");
            for (int slot = 0; ok && slot < 16; ++slot) {
                char key[32];
                snprintf(key, sizeof key, "hatchesToLock_%d", slot);
                const FusionJson *flag = fusion_json_member(&fields, key);
                if (!flag || (strcmp(flag->string, "TRUE") && strcmp(flag->string, "FALSE"))) {
                    ok = feb_fail(error, FEB_EXIT_OPERATION, "missing native hatch lock flag %d",
                                  slot);
                    break;
                }
                if (strcmp(flag->string, "TRUE")) continue;
                LockRule *grown = realloc(*rules, (*count + 1) * sizeof *grown);
                if (!grown) {
                    ok = feb_oom(error);
                    break;
                }
                *rules = grown;
                LockRule *rule = &(*rules)[(*count)++];
                rule->slot = slot;
                snprintf(rule->event, sizeof rule->event, "%s", event->string);
                snprintf(rule->when, sizeof rule->when, "%s", types[kind][1]);
                snprintf(rule->lock, sizeof rule->lock, "%s", types[kind][2]);
            }
        }
        fusion_json_free(&fields);
        open = close;
    }
    free(source);
    if (!ok) {
        free(*rules);
        *rules = NULL;
        *count = 0;
    }
    return ok;
}

typedef struct {
    int64_t index, destination, x_start, x_end, y_start, y_end;
    char type[256];
} Door;

/* _mzm_door_entries(): the doors of one room in the native door table. */
static bool door_entries(const char *source, const char *area, int room, Door **doors,
                         size_t *count, FebError *error) {
    *doors = NULL;
    *count = 0;
    const char *prefix = "const struct Door s", *table = NULL, *table_end = NULL;
    int tables = 0, declared = 0;
    for (const char *cursor = source; (cursor = strstr(cursor, prefix)) != NULL;) {
        const char *name = cursor + strlen(prefix), *probe = name;
        cursor += 1;
        while (isalnum((unsigned char)*probe)) ++probe;
        if (probe - name < 6 || strncmp(probe - 5, "Doors", 5) || *probe != '[') continue;
        int size = 0;
        const char *next = parse_number(probe + 1, &size);
        if (!next || *next != ']') continue;
        next = skip_space(next + 1);
        if (*next != '=') continue;
        next = skip_space(next + 1);
        if (*next != '{') continue;
        const char *end = strstr(next + 1, "\n};");
        if (!end) continue;
        cursor = end;
        size_t length = (size_t)(probe - 5 - name);
        if (length != strlen(area) || strncmp(name, area, length)) continue;
        ++tables;
        table = next + 1;
        table_end = end;
        declared = size;
    }
    if (tables != 1)
        return feb_fail(error, FEB_EXIT_OPERATION, "missing or duplicate %s door table", area);
    static const char *const required[] = {"type", "sourceRoom", "destinationDoor", "xStart",
                                           "xEnd", "yStart", "yEnd"};
    int index = 0;
    bool ok = true;
    for (const char *open = table; ok && open < table_end; ++open) {
        if (*open != '{') continue;
        const char *close = open + 1;
        while (close < table_end && *close != '{' && *close != '}') ++close;
        if (close >= table_end || *close != '}') continue;
        /* FIELD_RE: .name = value up to ',', '\n' or '}', stripped. */
        FusionJson fields = fusion_json_object();
        for (const char *dot = open + 1; ok && dot < close; ++dot) {
            if (*dot != '.' || !isalnum((unsigned char)dot[1])) continue;
            const char *key = dot + 1, *key_end = key;
            while (key_end < close && isalnum((unsigned char)*key_end)) ++key_end;
            const char *value = key_end;
            while (value < close && isspace((unsigned char)*value)) ++value;
            if (value >= close || *value != '=') continue;
            ++value;
            while (value < close && isspace((unsigned char)*value)) ++value;
            const char *value_end = value;
            while (value_end < close && *value_end != ',' && *value_end != '\n') ++value_end;
            if (value_end == value) continue;
            const char *trimmed = value_end;
            while (trimmed > value && isspace((unsigned char)trimmed[-1])) --trimmed;
            char name[64], text[256];
            snprintf(name, sizeof name, "%.*s", (int)(key_end - key), key);
            snprintf(text, sizeof text, "%.*s", (int)(trimmed - value), value);
            ok = feb_put_string(&fields, name, text) || feb_oom(error);
            dot = value_end - 1;
        }
        for (size_t key = 0; ok && key < COUNT(required); ++key)
            if (!fusion_json_member(&fields, required[key]))
                ok = feb_fail(error, FEB_EXIT_OPERATION, "%s door %d: missing fields", area, index);
        if (ok && strcmp(fusion_json_member(&fields, "type")->string, "DOOR_TYPE_NONE")) {
            const char *source_room = fusion_json_member(&fields, "sourceRoom")->string;
            if (is_decimal_text(source_room) && atoi(source_room) == room) {
                int64_t values[6];
                for (size_t key = 1; ok && key < COUNT(required); ++key) {
                    const char *text = fusion_json_member(&fields, required[key])->string;
                    if (!is_decimal_text(text))
                        ok = feb_fail(error, FEB_EXIT_OPERATION, "%s door %d: invalid coordinate",
                                      area, index);
                    else
                        values[key - 1] = strtoll(text, NULL, 10);
                }
                Door *grown = ok ? realloc(*doors, (*count + 1) * sizeof *grown) : NULL;
                if (ok && !grown) ok = feb_oom(error);
                if (ok) {
                    *doors = grown;
                    Door *door = &(*doors)[(*count)++];
                    door->index = index;
                    door->destination = values[1];
                    door->x_start = values[2];
                    door->x_end = values[3];
                    door->y_start = values[4];
                    door->y_end = values[5];
                    snprintf(door->type, sizeof door->type, "%s",
                             fusion_json_member(&fields, "type")->string);
                }
            }
        }
        fusion_json_free(&fields);
        ++index;
        open = close;
    }
    if (ok && index != declared)
        ok = feb_fail(error, FEB_EXIT_OPERATION, "%s: incomplete door table", area);
    if (!ok) {
        free(*doors);
        *doors = NULL;
        *count = 0;
    }
    return ok;
}

/* str.title() of a sprite name without its PSPRITE_ prefix. */
static void sprite_title(const char *symbol, char *out, size_t size) {
    const char *name = !strncmp(symbol, "PSPRITE_", 8) ? symbol + 8 : symbol;
    size_t used = 0;
    bool cased = false;
    for (; *name && used + 1 < size; ++name) {
        char c = *name == '_' ? ' ' : *name;
        if (isalpha((unsigned char)c)) {
            c = cased ? (char)tolower((unsigned char)c) : (char)toupper((unsigned char)c);
            cased = true;
        } else {
            cased = false;
        }
        out[used++] = c;
    }
    out[used] = '\0';
}

static bool push_row(Row **rows, size_t *count, const Row *row) {
    Row *grown = realloc(*rows, (*count + 1) * sizeof *grown);
    if (!grown) return false;
    *rows = grown;
    (*rows)[(*count)++] = *row;
    return true;
}

static void free_rows(Row *rows, size_t count) {
    for (size_t index = 0; index < count; ++index) free(rows[index].details);
    free(rows);
}

/* _safe(): str(value) without tabs or newlines, at most 300 code points. */
static void safe_field(Buffer *buffer, const char *value) {
    size_t points = 0;
    for (const unsigned char *c = (const unsigned char *)value; *c && points < 300; ++c) {
        if ((*c & 0xC0) != 0x80) {
            if (points == 300) break;
            ++points;
        }
        char out = *c == '\t' || *c == '\r' || *c == '\n' ? ' ' : (char)*c;
        append(buffer, "%c", out);
    }
}

static bool hatch_at(const FebBlockMap *clip, int64_t x, int64_t y, const char **style,
                     const char **facing, int64_t *position) {
    static const struct {
        int value;
        const char *style;
    } styles[] = {{48, "gray"}, {54, "normal"}, {64, "missile"}, {70, "super_missile"},
                  {76, "power_bomb"}};
    if (y < 0 || y >= clip->height) return false;
    const int64_t candidates[2] = {x + 1, x - 1};
    for (int side = 0; side < 2; ++side) {
        int64_t at = candidates[side];
        if (at < 0 || at >= clip->width) continue;
        uint16_t value = clip->blocks[y * clip->width + at];
        for (size_t index = 0; index < COUNT(styles); ++index)
            if (styles[index].value == value) {
                *style = styles[index].style;
                *facing = side ? "left" : "right";
                *position = at;
                return true;
            }
    }
    return false;
}

/* build_mzm(): the native sprite placements and doors of one room. */
static bool build_annotations(const char *repository, const FusionJson *room, const char *area,
                              int number, Row **rows, size_t *row_count, FebError *error) {
    *rows = NULL;
    *row_count = 0;
    bool known_area = false;
    for (size_t index = 0; index < COUNT(mzm_areas); ++index)
        if (!strcmp(mzm_areas[index], area)) known_area = true;
    if (!known_area || number < 0 || number >= 256)
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid MZM room identity");
    char lower[64], relative[192];
    snprintf(lower, sizeof lower, "%s", area);
    for (char *c = lower; *c; ++c) *c = (char)tolower((unsigned char)*c);
    snprintf(relative, sizeof relative, "third_party/mzm/src/data/rooms/%s/%s_%d.c", lower, lower,
             number);
    char *room_source = read_source(repository, relative, error);
    if (!room_source) return false;
    Placement *placements = NULL;
    size_t placement_count = 0;
    bool ok = parse_placements(room_source, area, number, &placements, &placement_count, error);
    free(room_source);
    Spriteset *sets = NULL;
    size_t set_count = 0;
    char *spriteset_source = ok ? read_source(repository, "third_party/mzm/src/data/spriteset.c",
                                              error) : NULL;
    ok = ok && spriteset_source && parse_spritesets(spriteset_source, &sets, &set_count, error);
    free(spriteset_source);
    FusionJson catalog = fusion_json_array();
    ok = ok && feb_mzm_object_records(repository, &catalog, error);
    const FusionJson *fields = fusion_json_member(room, "fields");
    const char *default_set = fusion_json_member(fields, "defaultSpriteset")->string;
    const struct {
        const char *name, *event_key, *symbol_key, *set_key, *event_default, *symbol_default;
    } variants[3] = {
        {"default", NULL, "pDefaultSpriteData", "defaultSpriteset", "0", NULL},
        {"event-1", "firstSpritesetEvent", "pFirstSpriteData", "firstSpriteset", "EVENT_NONE",
         "sEnemyRoomData_Empty"},
        {"event-2", "secondSpritesetEvent", "pSecondSpriteData", "secondSpriteset", "EVENT_NONE",
         "sEnemyRoomData_Empty"}};
    FebBlockMap clip = {0};
    FebError ignored = {0};
    bool has_clip = feb_mzm_room_map(repository, fusion_json_member(fields, "pClipData")->string,
                                     &clip, &ignored);
    int64_t source_index = 0;
    for (int variant = 0; ok && variant < 3; ++variant) {
        const FusionJson *value = variants[variant].event_key
            ? fusion_json_member(fields, variants[variant].event_key) : NULL;
        const char *event = value ? value->string : variants[variant].event_default;
        value = fusion_json_member(fields, variants[variant].symbol_key);
        const char *symbol = value ? value->string : variants[variant].symbol_default;
        value = fusion_json_member(fields, variants[variant].set_key);
        const char *set_text = value ? value->string : default_set;
        /* s[A-Za-z0-9]+_\d+_Spriteset(\d+) */
        const char *cursor = symbol + 1;
        while (isalnum((unsigned char)*cursor)) ++cursor;
        int dummy = 0, number_variant = 0;
        const char *next = symbol[0] == 's' && cursor > symbol + 1 && *cursor == '_'
            ? parse_number(cursor + 1, &dummy) : NULL;
        if (!next || strncmp(next, "_Spriteset", 10)) continue;
        next = parse_number(next + 10, &number_variant);
        if (!next || *next) continue;
        if (!is_decimal_text(set_text)) {
            ok = feb_fail(error, FEB_EXIT_OPERATION, "non-numeric MZM spriteset ID");
            break;
        }
        const Spriteset *definitions = NULL;
        for (size_t index = 0; index < set_count; ++index)
            if (sets[index].number == atoi(set_text)) definitions = &sets[index];
        if (!definitions) {
            ok = feb_fail(error, FEB_EXIT_OPERATION, "unknown MZM spriteset ID");
            break;
        }
        const Placement *placement = NULL;
        for (size_t index = 0; index < placement_count; ++index)
            if (placements[index].variant == number_variant) placement = &placements[index];
        for (size_t item = 0; ok && placement && item < placement->count; ++item) {
            int y = placement->items[item][0], x = placement->items[item][1];
            int index = placement->items[item][2];
            if ((size_t)index >= definitions->count) {
                ok = feb_fail(error, FEB_EXIT_OPERATION,
                              "MZM placement references absent spriteset entry");
                break;
            }
            const SpritesetEntry *entry = &definitions->entries[index];
            const char *category = NULL;
            for (size_t record = 0; record < catalog.count; ++record) {
                const FusionJson *known = &catalog.items[record];
                if (!strcmp(fusion_json_member(known, "native_type")->string, entry->name))
                    category = fusion_json_member(known, "category")->string;
            }
            const char *kind = NULL;
            static const char *const kinds[][2] = {
                {"Enemy / actor", "ENEMY"}, {"Item / pickup", "ITEM"},
                {"Door / gate sprite", "DOOR"}, {"World object", "OBJECT"},
                {"Unclassified native sprite", "OTHER"}};
            for (size_t candidate = 0; category && candidate < COUNT(kinds); ++candidate)
                if (!strcmp(kinds[candidate][0], category)) kind = kinds[candidate][1];
            if (!kind) kind = feb_mzm_sprite_kind(entry->name);
            Buffer details = {0};
            append(&details, "native=%s; spriteset=%s; graphics_slot=%d; variant=%s; event=%s",
                   entry->name, set_text, entry->slot, variants[variant].name, event);
            Row row = {.index = source_index++, .x = x * 16, .y = y * 16, .width = 16,
                       .height = 16, .details = details.data};
            snprintf(row.kind, sizeof row.kind, "%s", kind);
            snprintf(row.variant, sizeof row.variant, "%s", variants[variant].name);
            snprintf(row.native_type, sizeof row.native_type, "%s", entry->name);
            sprite_title(entry->name, row.label, sizeof row.label);
            if (details.failed || !push_row(rows, row_count, &row)) {
                free(details.data);
                ok = feb_oom(error);
            }
        }
    }
    LockRule *rules = NULL;
    size_t rule_count = 0;
    ok = ok && lock_rules(repository, area, number, &rules, &rule_count, error);
    char *doors_source = ok ? read_source(repository, "third_party/mzm/src/data/rooms_data.c",
                                          error) : NULL;
    Door *doors = NULL;
    size_t door_count = 0;
    ok = ok && doors_source && door_entries(doors_source, area, number, &doors, &door_count, error);
    free(doors_source);
    int64_t hatch_slot = 0;
    bool slots_known = has_clip;
    for (size_t index = 0; ok && index < door_count; ++index) {
        const Door *door = &doors[index];
        bool is_hatch = strstr(door->type, "DOOR_TYPE_CLOSED_HATCH") ||
                        strstr(door->type, "DOOR_TYPE_OPEN_HATCH");
        const char *style = NULL, *facing = NULL;
        int64_t hatch_x = 0;
        bool detected = is_hatch && has_clip &&
                        hatch_at(&clip, door->x_start, door->y_start, &style, &facing, &hatch_x);
        bool slot_known = slots_known && detected;
        int64_t current_slot = hatch_slot;
        if (is_hatch) {
            if (!detected) slots_known = false;
            else ++hatch_slot;
        }
        Row row = {.index = door->index, .x = door->x_start * 16, .y = door->y_start * 16,
                   .width = (door->x_end - door->x_start + 1) * 16,
                   .height = (door->y_end - door->y_start + 1) * 16};
        Buffer details = {0};
        append(&details, "native_door=%lld; type=%s; destination_door=%lld",
               (long long)door->index, door->type, (long long)door->destination);
        if (detected) {
            row.x = hatch_x * 16;
            row.width = 16;
            row.height = 64;
            append(&details, "; hatch_preview=%s_%s; hatch_family=%s; native_transition_x=%lld",
                   style, facing, !strcmp(area, "Chozodia") ? "mothership" : "zebes",
                   (long long)door->x_start);
            if (slot_known) {
                append(&details, "; hatch_slot=%lld", (long long)current_slot);
                bool first = true;
                for (size_t rule = 0; rule < rule_count; ++rule) {
                    if (rules[rule].slot != current_slot) continue;
                    append(&details, "%s%s:%s:%s", first ? "; hatch_lock_rules=" : ",",
                           rules[rule].event, rules[rule].when, rules[rule].lock);
                    first = false;
                }
            }
        }
        row.details = details.data;
        snprintf(row.kind, sizeof row.kind, "DOOR");
        snprintf(row.variant, sizeof row.variant, "native");
        snprintf(row.native_type, sizeof row.native_type, "%lld", (long long)door->index);
        snprintf(row.label, sizeof row.label, "Door %lld", (long long)door->index);
        if (details.failed || !push_row(rows, row_count, &row)) {
            free(details.data);
            ok = feb_oom(error);
        }
    }
    free(doors);
    free(rules);
    feb_block_map_free(&clip);
    fusion_json_free(&catalog);
    free_spritesets(sets, set_count);
    free_placements(placements, placement_count);
    if (!ok) {
        free_rows(*rows, *row_count);
        *rows = NULL;
        *row_count = 0;
    }
    return ok;
}

bool feb_annotations_serialize(const FebAnnotation *rows, size_t count, char **text,
                               size_t *size, FebError *error) {
    Buffer buffer = {0};
    append(&buffer, "# MV_ROOM_ANNOTATIONS_1\n# kind|index|x|y|width|height|variant|native_type|"
                    "label|details\n");
    for (size_t index = 0; index < count; ++index) {
        const FebAnnotation *row = &rows[index];
        char numbers[160];
        safe_field(&buffer, row->kind);
        snprintf(numbers, sizeof numbers, "|%lld|%lld|%lld|%lld|%lld|", (long long)row->index,
                 (long long)row->x, (long long)row->y, (long long)row->width,
                 (long long)row->height);
        append(&buffer, "%s", numbers);
        safe_field(&buffer, row->variant);
        append(&buffer, "|");
        safe_field(&buffer, row->native_type);
        append(&buffer, "|");
        safe_field(&buffer, row->label);
        append(&buffer, "|");
        safe_field(&buffer, row->details ? row->details : "");
        append(&buffer, "\n");
    }
    if (buffer.failed) {
        free(buffer.data);
        return feb_oom(error);
    }
    *text = buffer.data;
    *size = buffer.size;
    return true;
}

/* export_mzm(): the annotation TSV of one room; ``relative`` gets its path. */
static bool export_annotations(const char *repository, const FusionJson *room, const char *area,
                               int number, char *relative, size_t relative_size, FebError *error) {
    Row *rows = NULL;
    size_t count = 0;
    if (!build_annotations(repository, room, area, number, &rows, &count, error)) return false;
    FebAnnotation *annotations = calloc(count ? count : 1, sizeof *annotations);
    bool ok = annotations != NULL || feb_oom(error);
    for (size_t index = 0; ok && index < count; ++index)
        annotations[index] = (FebAnnotation){rows[index].kind, rows[index].index, rows[index].x,
                                             rows[index].y, rows[index].width, rows[index].height,
                                             rows[index].variant, rows[index].native_type,
                                             rows[index].label, rows[index].details};
    char *text = NULL;
    size_t size = 0;
    char lower[64];
    snprintf(lower, sizeof lower, "%s", area);
    for (char *c = lower; *c; ++c) *c = (char)tolower((unsigned char)*c);
    snprintf(relative, relative_size, "rooms/metroid/annotations/%s_%03d.tsv", lower, number);
    ok = ok && feb_annotations_serialize(annotations, count, &text, &size, error) &&
         feb_write_generated(repository, relative, (const uint8_t *)text, size, NULL, error);
    free(text);
    free(annotations);
    free_rows(rows, count);
    return ok;
}

/* ---- The workroom export ---------------------------------------------- */

static bool exists(const char *path) {
    long long size = 0;
    return feb_is_regular(path, &size) || feb_is_directory(path);
}

bool feb_mzm_workspace_export(const char *repository, const char *area, int64_t number,
                              FusionJson *info, FebError *error) {
    bool valid = isalpha((unsigned char)area[0]) && strlen(area) <= 24;
    for (const char *c = area; valid && *c; ++c)
        if (!isalnum((unsigned char)*c)) valid = false;
    if (!valid || number < 0 || number >= 1000)
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid area or room number");
    FusionJson room;
    if (!feb_mzm_source_room(repository, area, number, &room, error)) return false;
    const FusionJson *fields = fusion_json_member(&room, "fields");
    const char *room_id = fusion_json_member(&room, "native_id")->string;
    const char *tileset_text = fusion_json_member(fields, "tileset")->string;
    int64_t tileset_id = strtoll(tileset_text, NULL, 10);
    FebMzmTileset tileset = {0};
    FebBlockMap maps[2] = {{0}, {0}}, clip = {0};
    char lower[64];
    snprintf(lower, sizeof lower, "%s", area);
    for (char *c = lower; *c; ++c) *c = (char)tolower((unsigned char)*c);
    bool ok = feb_mzm_tileset(repository, tileset_id, &tileset, error);
    static const char *const names[2] = {"Bg1", "Bg2"};
    for (int layer = 0; ok && layer < 2; ++layer) {
        char prop[16], key[16];
        snprintf(prop, sizeof prop, "bg%dProp", layer + 1);
        snprintf(key, sizeof key, "p%sData", names[layer]);
        const FusionJson *property = fusion_json_member(fields, prop);
        if (!property) {
            ok = feb_fail(error, FEB_EXIT_OPERATION, "'%s'", prop);
            break;
        }
        if (strcmp(property->string, "BG_PROP_RLE_COMPRESSED")) {
            ok = feb_fail(error, FEB_EXIT_OPERATION, "%s %s not supported by current RLE decoder",
                          room_id, names[layer]);
            break;
        }
        ok = feb_mzm_room_map(repository, fusion_json_member(fields, key)->string, &maps[layer],
                              error);
    }
    ok = ok && feb_mzm_room_map(repository, fusion_json_member(fields, "pClipData")->string, &clip,
                                error);
    char collision_relative[160];
    snprintf(collision_relative, sizeof collision_relative,
             "rooms/metroid/previews/%s_%03lld_collision.bmp", lower, (long long)number);
    if (ok) {
        uint8_t *bmp = NULL;
        size_t size = 0;
        ok = feb_mzm_collision_preview(&clip, &bmp, &size, error) &&
             feb_write_generated(repository, collision_relative, bmp, size, NULL, error);
        free(bmp);
    }
    int64_t base = 0, coverage = 0, missing = 0;
    ok = ok && feb_mzm_graphic_base(&tileset, &maps[0], &base, &coverage, error);
    /* A 16xN atlas, each entry rendered with the exact native metatile rules. */
    int64_t rows = ((int64_t)tileset.entries + 15) / 16, atlas_width = 256,
            atlas_height = rows * 16;
    uint8_t *image = ok ? calloc((size_t)(atlas_width * atlas_height), 3) : NULL;
    if (ok && !image) ok = feb_oom(error);
    for (size_t entry = 0; ok && entry < tileset.entries; ++entry) {
        uint16_t block = (uint16_t)entry;
        uint8_t *tile = NULL;
        int64_t unresolved = 0, painted = 0;
        ok = feb_mzm_render_layer(1, 1, &block, &tileset, base, &tile, NULL, &unresolved, &painted,
                                  error);
        if (!ok) break;
        missing += unresolved;
        int64_t ox = (int64_t)(entry % 16) * 16, oy = (int64_t)(entry / 16) * 16;
        for (int row = 0; row < 16; ++row)
            memcpy(image + ((oy + row) * atlas_width + ox) * 3, tile + row * 48, 48);
        free(tile);
    }
    char atlas_relative[96], map_relative[160], override_relative[160], path[FEB_PATH_CAPACITY];
    snprintf(atlas_relative, sizeof atlas_relative, "rooms/metroid/tilesets/%lld_atlas.bmp",
             (long long)tileset_id);
    snprintf(map_relative, sizeof map_relative, "rooms/metroid/workrooms/%s_%03lld.mvnative", lower,
             (long long)number);
    snprintf(override_relative, sizeof override_relative, "overrides/metroid/%s_%03lld.mvnative",
             lower, (long long)number);
    snprintf(path, sizeof path, "%s/assets/extracted/%s", repository, atlas_relative);
    if (ok && !exists(path)) {
        uint8_t *bmp = NULL;
        size_t size = 0;
        ok = feb_bmp24(atlas_width, atlas_height, image, &bmp, &size, error) &&
             feb_write_generated(repository, atlas_relative, bmp, size, NULL, error);
        free(bmp);
    }
    free(image);
    snprintf(path, sizeof path, "%s/assets/extracted/%s", repository, map_relative);
    if (ok && !exists(path)) {
        /* serialize(): the MVNATIVE 1 block maps. */
        bool id_ok = !strncmp(room_id, "mzm:", 4);
        const char *colon = id_ok ? strchr(room_id + 4, ':') : NULL;
        id_ok = colon && colon > room_id + 4 && strlen(colon + 1) == 3 &&
                is_decimal_text(colon + 1);
        for (const char *c = room_id + 4; id_ok && c < colon; ++c)
            if (!(islower((unsigned char)*c) || isdigit((unsigned char)*c))) id_ok = false;
        if (tileset.entries < 1 || tileset.entries > 1024 || !id_ok)
            ok = feb_fail(error, FEB_EXIT_OPERATION, "invalid room identity or metatile count");
        if (ok && (tileset_id < 0 || tileset_id > 99))
            ok = feb_fail(error, FEB_EXIT_OPERATION, "invalid atlas path");
        Buffer text = {0};
        append(&text, "MVNATIVE 1\nROOM %s\nTILESET %s\nTILES %zu\nATLAS %s\n", room_id,
               tileset_text, tileset.entries, atlas_relative);
        for (int layer = 0; ok && layer < 2; ++layer) {
            append(&text, "LAYER BG%d %lld %lld\n", layer + 1, (long long)maps[layer].width,
                   (long long)maps[layer].height);
            for (int64_t y = 0; y < maps[layer].height; ++y)
                for (int64_t x = 0; x < maps[layer].width; ++x)
                    append(&text, "%04X%s", maps[layer].blocks[y * maps[layer].width + x],
                           x + 1 == maps[layer].width ? "\n" : " ");
        }
        append(&text, "END\n");
        if (ok && text.failed) ok = feb_oom(error);
        ok = ok && feb_write_generated(repository, map_relative, (const uint8_t *)text.data,
                                       text.size, NULL, error);
        free(text.data);
    }
    *info = fusion_json_object();
    if (ok) {
        char absolute[5][FEB_PATH_CAPACITY];
        const char *relatives[4] = {atlas_relative, map_relative, override_relative,
                                    collision_relative};
        for (int index = 0; index < 4; ++index)
            snprintf(absolute[index], sizeof absolute[index], "%s/assets/extracted/%s", repository,
                     relatives[index]);
        FusionJson layers = fusion_json_object();
        for (int layer = 0; ok && layer < 2; ++layer) {
            FusionJson size = fusion_json_object();
            ok = (feb_put_int(&size, "width", maps[layer].width) &&
                  feb_put_int(&size, "height", maps[layer].height) &&
                  feb_put(&layers, names[layer], size)) || feb_oom(error);
        }
        ok = ok && ((feb_put_string(info, "room_id", room_id) &&
                     feb_put_int(info, "tile_count", (int64_t)tileset.entries) &&
                     feb_put(info, "layers", layers) && feb_put_string(info, "atlas", absolute[0]) &&
                     feb_put_string(info, "base", absolute[1]) &&
                     feb_put_string(info, "override", absolute[2]) &&
                     feb_put_int(info, "unresolved_atlas_cells", missing) &&
                     feb_put_string(info, "collision", absolute[3]) &&
                     feb_put_string(info, "limitations",
                                    "BG0, BG3, common tiles and animated palette omitted; edit "
                                    "blocks BG1/BG2 only")) ||
                    feb_oom(error));
        /* BG3 is an optional read-only preview: it never blocks editing. */
        FusionJson background;
        FebError reason = {0};
        const char *canonical = fusion_json_member(&room, "area")->string;
        if (ok && !bg3_render(repository, canonical, number, &background, &reason)) {
            background = fusion_json_object();
            ok = (feb_put_string(&background, "status", "NOT_DECODED") &&
                  feb_put_string(&background, "reason", reason.message)) || feb_oom(error);
        }
        ok = ok && (feb_put(info, "background", background) || feb_oom(error));
        char annotations[192];
        ok = ok && export_annotations(repository, &room, canonical, (int)number, annotations,
                                      sizeof annotations, error);
        if (ok) {
            snprintf(absolute[4], sizeof absolute[4], "%s/assets/extracted/%s", repository,
                     annotations);
            ok = feb_put_string(info, "annotations", absolute[4]) || feb_oom(error);
        }
        int64_t hatches = 0;
        FebError hatch_reason = {0};
        if (ok && export_hatches(repository, &hatches, &hatch_reason)) {
            ok = feb_put_int(info, "native_hatch_previews", hatches) || feb_oom(error);
        } else if (ok) {
            FusionJson status = fusion_json_object();
            ok = (feb_put_string(&status, "status", "NOT_AVAILABLE") &&
                  feb_put_string(&status, "reason", hatch_reason.message) &&
                  feb_put(info, "native_hatch_previews", status)) || feb_oom(error);
        }
    }
    for (int layer = 0; layer < 2; ++layer) feb_block_map_free(&maps[layer]);
    feb_block_map_free(&clip);
    feb_mzm_tileset_free(&tileset);
    fusion_json_free(&room);
    if (!ok) fusion_json_free(info);
    return ok;
}
