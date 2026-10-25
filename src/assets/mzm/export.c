/* SPDX-License-Identifier: GPL-3.0-only */
/* The Zero Mission runtime-room export (scripts/mzm_runtime_room.py --all):
 * every exportable room's room.tsv, background.bmp and text layers, the
 * used tileset sheets, blocks.tsv, power_bomb.tsv and export_index.tsv. */
#define _POSIX_C_SOURCE 200809L  /* lstat */

#include "fusion/assets/mzm.h"

#include "fusion/assets/rom.h"
#include "output.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#endif

enum {
    EXPORT_PATH_CAPACITY = 1024,
    EXPORT_SKIP_CAPACITY = FUSION_MZM_EXPORT_SKIP_CAPACITY
};

static void set_error(char *error, size_t error_size, const char *format,
                      const char *detail) {
    if (!error || error_size == 0) return;
    snprintf(error, error_size, format, detail ? detail : "");
    error[error_size - 1] = '\0';
}

static bool is_symlink(const char *path) {
#ifdef _WIN32
    (void)path;
    return false;
#else
    struct stat info;
    return lstat(path, &info) == 0 && S_ISLNK(info.st_mode);
#endif
}

/* private_path(create=True): a real directory, created when missing. */
static bool ensure_directory(const char *path, char *error, size_t error_size) {
    if (is_symlink(path)) {
        set_error(error, error_size, "symlink private asset component refused: %s",
                  path);
        return false;
    }
    struct stat info;
    if (stat(path, &info) == 0) {
        if (S_ISDIR(info.st_mode)) return true;
        set_error(error, error_size, "private asset parent is not a directory: %s",
                  path);
        return false;
    }
#ifdef _WIN32
    int made = _mkdir(path);
#else
    int made = mkdir(path, 0777);
#endif
    if (made != 0 && errno != EEXIST) {
        set_error(error, error_size, "cannot create private asset directory: %s",
                  path);
        return false;
    }
    return true;
}

static bool join_path(char *output, const char *directory, const char *name) {
    int written = snprintf(output, EXPORT_PATH_CAPACITY, "%s/%s", directory, name);
    return written > 0 && written < EXPORT_PATH_CAPACITY;
}

typedef struct {
    const uint8_t *data;
    size_t size;
} ExportBytes;

static bool write_bytes(FILE *file, const void *context) {
    const ExportBytes *bytes = context;
    return fwrite(bytes->data, 1, bytes->size, file) == bytes->size;
}

/* sprite_library.write_atomic: no symlinked target or temporary. */
static bool write_file(const char *path, const uint8_t *data, size_t size,
                       char *error, size_t error_size) {
    char temporary[EXPORT_PATH_CAPACITY + 8];
    snprintf(temporary, sizeof temporary, "%s.tmp", path);
    if (is_symlink(path) || is_symlink(temporary)) {
        set_error(error, error_size, "symlink output refused: %s", path);
        return false;
    }
    ExportBytes bytes = {data, size};
    char detail[128];
    if (!fusion_asset_write_atomic(path, write_bytes, &bytes, detail,
                                   sizeof detail)) {
        set_error(error, error_size, "cannot write %s", path);
        return false;
    }
    return true;
}

typedef struct {
    char folder[40];
    char sha_room[65];
    char sha_background[65];
} ExportedRoom;

typedef struct {
    char item[40];
    char reason[96];
} SkippedRoom;

static int compare_skipped(const void *left, const void *right) {
    const SkippedRoom *a = left, *b = right;
    int order = strcmp(a->item, b->item);
    return order ? order : strcmp(a->reason, b->reason);
}

static int compare_exported(const void *left, const void *right) {
    return strcmp(((const ExportedRoom *)left)->folder,
                  ((const ExportedRoom *)right)->folder);
}

/* export_index.write: every room.tsv and background.bmp sorted by path,
 * then the skipped rooms sorted, each reason's whitespace collapsed. */
static bool write_index(const char *output, ExportedRoom *rooms, size_t room_count,
                        SkippedRoom *skipped, size_t skipped_count,
                        char *error, size_t error_size) {
    qsort(rooms, room_count, sizeof *rooms, compare_exported);
    qsort(skipped, skipped_count, sizeof *skipped, compare_skipped);
    size_t capacity = 64 + room_count * 2 * 128 + skipped_count * 160;
    char *text = malloc(capacity);
    if (!text) {
        set_error(error, error_size, "cannot allocate the export index%s", NULL);
        return false;
    }
    size_t used = (size_t)snprintf(text, capacity,
                                   "schema\tmetroidvania-export-index-v1\n");
    for (size_t index = 0; index < room_count; ++index) {
        used += (size_t)snprintf(text + used, capacity - used,
                                 "file\t%s/background.bmp\t%s\n"
                                 "file\t%s/room.tsv\t%s\n",
                                 rooms[index].folder, rooms[index].sha_background,
                                 rooms[index].folder, rooms[index].sha_room);
    }
    for (size_t index = 0; index < skipped_count; ++index) {
        used += (size_t)snprintf(text + used, capacity - used, "skipped\t%s\t",
                                 skipped[index].item);
        bool space = false, started = false;
        for (const char *cursor = skipped[index].reason; *cursor; ++cursor) {
            if (*cursor == ' ' || *cursor == '\t' || *cursor == '\n' ||
                *cursor == '\r' || *cursor == '\v' || *cursor == '\f') {
                space = started;
                continue;
            }
            if (space && used + 1 < capacity) text[used++] = ' ';
            if (used + 1 < capacity) text[used++] = *cursor;
            space = false;
            started = true;
        }
        if (used + 1 < capacity) text[used++] = '\n';
        text[used] = '\0';
    }
    char path[EXPORT_PATH_CAPACITY];
    bool written = used < capacity && join_path(path, output, "export_index.tsv") &&
                   write_file(path, (const uint8_t *)text, used, error, error_size);
    free(text);
    return written;
}

bool fusion_mzm_export_runtime_rooms(
    const FusionRom *rom, const uint8_t *elf, size_t elf_size,
    const char *output, FusionMzmExportSummary *summary,
    char *error, size_t error_size) {
    if (summary) *summary = (FusionMzmExportSummary){0};
    if (!rom || !elf || !output || !summary) {
        set_error(error, error_size, "invalid Zero Mission export request%s", NULL);
        return false;
    }
    bool ok = false;
    FusionMzmRomSymbolTable symbols = {0};
    FusionMzmRoomDirectory directory;
    FusionMzmClipdataTables *tables = malloc(sizeof *tables);
    FusionMzmDoorDirectory doors = {0};
    FusionMzmHatchTables hatches = {0};
    FusionMzmSpritesetTable *spritesets = malloc(sizeof *spritesets);
    FusionMzmRoomSprites *sprites = malloc(sizeof *sprites);
    FusionMzmVideoMemory *memory = malloc(sizeof *memory);
    FusionMzmMetatileTable *metatiles = malloc(sizeof *metatiles);
    ExportedRoom *rooms = calloc(FUSION_MZM_ROOM_AREA_COUNT * FUSION_MZM_ROOM_CAPACITY,
                                 sizeof *rooms);
    SkippedRoom skipped[EXPORT_SKIP_CAPACITY];
    size_t room_count = 0, skipped_count = 0;
    bool sheet_written[FUSION_MZM_TILESET_COUNT] = {false};
    bool shared_written = false;
    char detail[256] = "";
    if (!tables || !spritesets || !sprites || !memory || !metatiles || !rooms) {
        set_error(error, error_size, "cannot allocate the Zero Mission export%s",
                  NULL);
        goto done;
    }
    char tilesets[EXPORT_PATH_CAPACITY];
    if (!ensure_directory(output, error, error_size) ||
        !join_path(tilesets, output, "tilesets") ||
        !fusion_mzm_read_rom_symbols(rom, elf, elf_size, &symbols, detail,
                                     sizeof detail) ||
        !fusion_mzm_read_room_directory(rom, &symbols, &directory, detail,
                                        sizeof detail) ||
        !fusion_mzm_read_clipdata_tables(rom, &symbols, tables, detail,
                                         sizeof detail) ||
        !fusion_mzm_read_door_directory(rom, &symbols, &doors, detail,
                                        sizeof detail) ||
        !fusion_mzm_read_hatch_tables(rom, &symbols, &hatches, detail,
                                      sizeof detail) ||
        !fusion_mzm_read_spritesets(rom, &symbols, spritesets, detail,
                                    sizeof detail)) {
        if (detail[0]) set_error(error, error_size, "%s", detail);
        goto done;
    }
    for (size_t area = 0; area < FUSION_MZM_ROOM_AREA_COUNT; ++area) {
        const FusionMzmRoomAreaSpec *spec = fusion_mzm_room_area_spec(area);
        for (size_t room = 0; room < directory.areas[area].room_count; ++room) {
            FusionMzmRoomEntry entry;
            FusionMzmRoomRleMap clipdata = {0};
            FusionMzmRoomBackgrounds backgrounds = {0};
            FusionMzmCompositeBackground composite = {0};
            FusionMzmSerializedBackground background = {0};
            FusionMzmRuntimeRoomPayload payload = {0};
            bool no_base = false, room_ok = false;
            const char *skip = NULL;
            char folder[EXPORT_PATH_CAPACITY], path[EXPORT_PATH_CAPACITY];
            char name[40];
            if (!fusion_mzm_read_room_entry(rom, &directory, area, room, &entry,
                                            detail, sizeof detail)) {
                set_error(error, error_size, "%s", detail);
                goto done;
            }
            if (entry.bg1_property != FUSION_MZM_BG_PROPERTY_RLE_COMPRESSED) {
                skip = "room BG1 is not RLE compressed; no runtime export";
            } else if (!fusion_mzm_read_room_rle(rom, entry.clip_pointer, &clipdata,
                                                 detail, sizeof detail) ||
                       !fusion_mzm_read_room_backgrounds(rom, &entry, &backgrounds,
                                                         detail, sizeof detail)) {
                set_error(error, error_size, "%s", detail);
                goto room_done;
            } else if (!fusion_mzm_render_export_background(
                           rom, &symbols, &entry, &backgrounds, &composite,
                           &no_base, detail, sizeof detail)) {
                if (!no_base) {
                    set_error(error, error_size, "%s", detail);
                    goto room_done;
                }
                skip = "no BG metatile indices refer to tileset graphics";
            } else {
                /* produce() then reads the hatch lock events, whose parser
                 * rejects an area name that is not purely alphabetic (the
                 * Test1 to Test3 areas). */
                for (const char *cursor = spec->id; *cursor; ++cursor) {
                    if (!((*cursor >= 'A' && *cursor <= 'Z') ||
                          (*cursor >= 'a' && *cursor <= 'z'))) {
                        skip = "invalid native hatch room";
                        break;
                    }
                }
            }
            if (skip) {
                if (skipped_count == EXPORT_SKIP_CAPACITY) {
                    set_error(error, error_size, "too many skipped rooms%s", NULL);
                    goto room_done;
                }
                snprintf(skipped[skipped_count].item, sizeof skipped[0].item,
                         "%s/%zu", spec->id, room);
                snprintf(skipped[skipped_count].reason, sizeof skipped[0].reason,
                         "%s", skip);
                ++skipped_count;
                room_ok = true;
                goto room_done;
            }
            /* The folder, as <area>_<NNN> in lowercase. */
            size_t length = 0;
            for (; spec->id[length] && length < 24; ++length) {
                char value = spec->id[length];
                name[length] = value >= 'A' && value <= 'Z'
                    ? (char)(value - 'A' + 'a') : value;
            }
            snprintf(name + length, sizeof name - length, "_%03zu", room);
            if (!join_path(folder, output, name) ||
                !ensure_directory(folder, error, error_size)) {
                goto room_done;
            }
            if (!fusion_mzm_read_room_sprites(rom, &symbols, spritesets, &entry,
                                              sprites, detail, sizeof detail) ||
                !fusion_mzm_serialize_runtime_room_sprites(
                    area, room, &clipdata, tables, &doors, &hatches, &entry,
                    &backgrounds, spritesets, sprites, &payload, detail,
                    sizeof detail) ||
                !fusion_mzm_serialize_background(&composite, &background, detail,
                                                 sizeof detail) ||
                !fusion_mzm_load_tileset_video_memory(rom, &symbols, entry.tileset,
                                                      memory, detail,
                                                      sizeof detail)) {
                set_error(error, error_size, "%s", detail);
                goto room_done;
            }
            /* The text layers, bg0.bmp then bg3.bmp. */
            for (size_t layer = 0; layer < FUSION_MZM_ROOM_BACKGROUND_COUNT;
                 ++layer) {
                if ((layer != 0 && layer != 3) ||
                    backgrounds.layers[layer].status != FUSION_MZM_BACKGROUND_TEXT)
                    continue;
                FusionMzmRenderedBackground text;
                FusionMzmBitmap bitmap;
                char file[16];
                snprintf(file, sizeof file, "bg%zu.bmp", layer);
                if (!fusion_mzm_render_text_background(
                        &backgrounds.layers[layer].text, memory, &text, detail,
                        sizeof detail)) {
                    set_error(error, error_size, "%s", detail);
                    goto room_done;
                }
                bool serialized = fusion_mzm_serialize_bmp32(&text, &bitmap, detail,
                                                             sizeof detail);
                fusion_mzm_free_rendered_background(&text);
                bool written = serialized && join_path(path, folder, file) &&
                               write_file(path, bitmap.data, bitmap.size, error,
                                          error_size);
                if (serialized) fusion_mzm_free_bitmap(&bitmap);
                if (!serialized) set_error(error, error_size, "%s", detail);
                if (!written) goto room_done;
                ++summary->text_layer_count;
            }
            if (!shared_written) {
                FusionMzmTextFile blocks = {0}, windows = {0};
                bool built =
                    fusion_mzm_serialize_block_tables(rom, &symbols, tables, &blocks,
                                                      detail, sizeof detail) &&
                    fusion_mzm_serialize_power_bomb_windows(rom, &symbols, &windows,
                                                            detail, sizeof detail);
                bool written = built && join_path(path, output, "blocks.tsv") &&
                               write_file(path, blocks.data, blocks.size, error,
                                          error_size) &&
                               join_path(path, output, "power_bomb.tsv") &&
                               write_file(path, windows.data, windows.size, error,
                                          error_size);
                fusion_mzm_free_text_file(&blocks);
                fusion_mzm_free_text_file(&windows);
                if (!built) set_error(error, error_size, "%s", detail);
                if (!written) goto room_done;
                shared_written = true;
            }
            if (!sheet_written[entry.tileset]) {
                FusionMzmRenderedBackground sheet;
                FusionMzmBitmap bitmap;
                char file[32];
                snprintf(file, sizeof file, "tileset_%02u.bmp",
                         (unsigned)entry.tileset);
                if (!ensure_directory(tilesets, error, error_size)) goto room_done;
                if (!fusion_mzm_read_tileset_metatiles(rom, &symbols, entry.tileset,
                                                       metatiles, detail,
                                                       sizeof detail) ||
                    !fusion_mzm_render_metatile_sheet(metatiles, memory, &sheet,
                                                      detail, sizeof detail)) {
                    set_error(error, error_size, "%s", detail);
                    goto room_done;
                }
                bool serialized = fusion_mzm_serialize_bmp32(&sheet, &bitmap, detail,
                                                             sizeof detail);
                fusion_mzm_free_rendered_background(&sheet);
                bool written = serialized && join_path(path, tilesets, file) &&
                               write_file(path, bitmap.data, bitmap.size, error,
                                          error_size);
                if (serialized) fusion_mzm_free_bitmap(&bitmap);
                if (!serialized) set_error(error, error_size, "%s", detail);
                if (!written) goto room_done;
                sheet_written[entry.tileset] = true;
                ++summary->sheet_count;
            }
            ExportedRoom *exported = &rooms[room_count];
            snprintf(exported->folder, sizeof exported->folder, "%s", name);
            if (!join_path(path, folder, "room.tsv") ||
                !write_file(path, payload.data, payload.size, error, error_size) ||
                !join_path(path, folder, "background.bmp") ||
                !write_file(path, background.bitmap, background.bitmap_size, error,
                            error_size) ||
                !fusion_sha256_hex(payload.data, payload.size, exported->sha_room) ||
                !fusion_sha256_hex(background.bitmap, background.bitmap_size,
                                   exported->sha_background)) {
                goto room_done;
            }
            ++room_count;
            summary->room_bytes += payload.size;
            summary->background_bytes += background.bitmap_size;
            room_ok = true;
        room_done:
            fusion_mzm_free_runtime_room_payload(&payload);
            fusion_mzm_free_serialized_background(&background);
            fusion_mzm_free_composite_background(&composite);
            fusion_mzm_free_room_backgrounds(&backgrounds);
            fusion_mzm_free_room_rle(&clipdata);
            if (!room_ok) {
                if (error && error_size && !error[0])
                    set_error(error, error_size, "cannot export a room%s", NULL);
                goto done;
            }
        }
    }
    for (size_t index = 0; index < skipped_count; ++index) {
        snprintf(summary->skipped[index], sizeof summary->skipped[index],
                 "%s: %s", skipped[index].item, skipped[index].reason);
    }
    if (!write_index(output, rooms, room_count, skipped, skipped_count, error,
                     error_size)) {
        goto done;
    }
    summary->room_count = room_count;
    summary->skipped_count = skipped_count;
    ok = true;
    if (error && error_size) error[0] = '\0';

done:
    fusion_mzm_free_hatch_tables(&hatches);
    fusion_mzm_free_door_directory(&doors);
    fusion_mzm_free_symbols(&symbols);
    free(tables);
    free(spritesets);
    free(sprites);
    free(memory);
    free(metatiles);
    free(rooms);
    return ok;
}
