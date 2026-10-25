/* SPDX-License-Identifier: GPL-3.0-only */
/* The Zero Mission runtime-room export (formerly scripts/mzm_runtime_room.py
 * --all): every exportable room's room.tsv, background.bmp and text layers,
 * the used tileset sheets, blocks.tsv, power_bomb.tsv and export_index.tsv. */
#include "fusion/assets/mzm.h"

#include "export.h"
#include "fusion/assets/rom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { EXPORT_SKIP_CAPACITY = FUSION_MZM_EXPORT_SKIP_CAPACITY };

static void set_error(char *error, size_t error_size, const char *format,
                      const char *detail) {
    if (!error || error_size == 0) return;
    snprintf(error, error_size, format, detail ? detail : "");
    error[error_size - 1] = '\0';
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
    FusionExportRoom *rooms = calloc(FUSION_MZM_ROOM_AREA_COUNT * FUSION_MZM_ROOM_CAPACITY,
                                 sizeof *rooms);
    FusionExportSkip skipped[EXPORT_SKIP_CAPACITY];
    size_t room_count = 0, skipped_count = 0;
    bool sheet_written[FUSION_MZM_TILESET_COUNT] = {false};
    bool shared_written = false;
    char detail[256] = "";
    if (!tables || !spritesets || !sprites || !memory || !metatiles || !rooms) {
        set_error(error, error_size, "cannot allocate the Zero Mission export%s",
                  NULL);
        goto done;
    }
    char tilesets[FUSION_EXPORT_PATH_CAPACITY];
    if (!fusion_export_ensure_directory(output, error, error_size) ||
        !fusion_export_join(tilesets, output, "tilesets") ||
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
            char folder[FUSION_EXPORT_PATH_CAPACITY], path[FUSION_EXPORT_PATH_CAPACITY];
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
            if (!fusion_export_join(folder, output, name) ||
                !fusion_export_ensure_directory(folder, error, error_size)) {
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
                bool written = serialized && fusion_export_join(path, folder, file) &&
                               fusion_export_write_file(path, bitmap.data, bitmap.size, error,
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
                bool written = built && fusion_export_join(path, output, "blocks.tsv") &&
                               fusion_export_write_file(path, blocks.data, blocks.size, error,
                                          error_size) &&
                               fusion_export_join(path, output, "power_bomb.tsv") &&
                               fusion_export_write_file(path, windows.data, windows.size, error,
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
                if (!fusion_export_ensure_directory(tilesets, error, error_size)) goto room_done;
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
                bool written = serialized && fusion_export_join(path, tilesets, file) &&
                               fusion_export_write_file(path, bitmap.data, bitmap.size, error,
                                          error_size);
                if (serialized) fusion_mzm_free_bitmap(&bitmap);
                if (!serialized) set_error(error, error_size, "%s", detail);
                if (!written) goto room_done;
                sheet_written[entry.tileset] = true;
                ++summary->sheet_count;
            }
            FusionExportRoom *exported = &rooms[room_count];
            snprintf(exported->folder, sizeof exported->folder, "%s", name);
            if (!fusion_export_join(path, folder, "room.tsv") ||
                !fusion_export_write_file(path, payload.data, payload.size, error, error_size) ||
                !fusion_export_join(path, folder, "background.bmp") ||
                !fusion_export_write_file(path, background.bitmap, background.bitmap_size, error,
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
    if (!fusion_export_write_room_index(output, rooms, room_count, skipped, skipped_count, error,
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
