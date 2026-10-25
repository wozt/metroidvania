/* SPDX-License-Identifier: GPL-3.0-only */
/* Native asset pipeline entry point. Each C replacement becomes authoritative
 * only after an equivalence fixture and integration test. */
#include "fusion/assets/aos.h"
#include "fusion/assets/mzm.h"
#include "fusion/assets/rom.h"
#include "fusion/assets/gba.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *name) {
    fprintf(stderr,
            "Usage: %s --list\n"
            "       %s --verify [--rom-dir DIR] [--aria FILE] [--metroid FILE]\n"
            "       %s --rom-manifest GAME ROM OUTPUT_JSON\n"
            "       %s --header FILE\n"
            "       %s --aria-directory FILE\n"
            "       %s --aria-map FILE\n"
            "       %s --aria-rooms FILE\n"
            "       %s --aria-transitions FILE\n"
            "       %s --aria-backgrounds FILE\n"
            "       %s --aria-background-tiles FILE\n"
            "       %s --aria-background-renders FILE\n"
            "       %s --aria-background-composites FILE\n"
            "       %s --aria-collisions FILE\n"
            "       %s --aria-runtime-rooms FILE\n"
            "       %s --aria-loads FILE\n"
            "       %s --aria-video-memory FILE\n"
            "       %s --aria-entities FILE\n"
            "       %s --aria-bosses FILE\n"
            "       %s --aria-summary FILE\n"
            "       %s --aria-catalog ROM WORLD_JSON ROOMS_TSV\n"
            "       %s --aria-export-rooms ROM OUTPUT_DIR\n"
            "       %s --aria-export-soma ROM OUTPUT_DIR\n"
            "       %s --aria-export-weapons ROM METADATA_DIR RUNTIME_DIR\n"
            "       %s --aria-export-objects ROM METADATA_DIR RUNTIME_DIR\n"
            "       %s --mzm-symbols ROM ELF\n"
            "       %s --mzm-rom-bank ROM ELF OUTPUT_DIR\n"
            "       %s --mzm-rooms ROM ELF\n"
            "       %s --mzm-clipdata ROM ELF\n"
            "       %s --mzm-runtime-room-cores ROM ELF\n"
            "       %s --mzm-runtime-room-doors ROM ELF\n"
            "       %s --mzm-runtime-room-hatches ROM ELF\n"
            "       %s --mzm-runtime-room-metadata ROM ELF\n"
            "       %s --mzm-runtime-room-sprites ROM ELF\n"
            "       %s --mzm-bitmaps ROM ELF\n"
            "       %s --mzm-runtime-tables ROM ELF [FILE]\n"
            "       %s --mzm-export-backgrounds ROM ELF\n"
            "       %s --mzm-export-rooms ROM ELF OUTPUT_DIR\n"
            "       %s --mzm-export-samus ROM ELF SAMUS_DIR PROJECTILE_DIR\n"
            "       %s --mzm-backgrounds ROM ELF\n"
            "       %s --mzm-tilesets ROM ELF\n"
            "       %s --mzm-metatiles ROM ELF\n"
            "       %s --mzm-background-renders ROM ELF\n"
            "       %s --mzm-background-composites ROM ELF\n"
            "       %s --dry-run [--force]\n"
            "       %s --assets|--all [--dry-run] [--force]\n",
            name, name, name, name, name, name, name, name, name, name, name,
            name, name, name, name, name,
            name, name, name, name, name, name, name, name, name, name, name,
            name,
            name, name, name, name,
            name, name, name, name, name, name, name, name, name, name, name,
            name, name);
}

static bool read_file(const char *path, uint8_t **data, size_t *size,
                      char *error, size_t error_size) {
    enum { FILE_SIZE_LIMIT = 64 * 1024 * 1024 };
    *data = NULL;
    *size = 0;
    FILE *file = fopen(path, "rb");
    if (!file) {
        snprintf(error, error_size, "cannot open %s", path);
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        snprintf(error, error_size, "cannot seek %s", path);
        return false;
    }
    long length = ftell(file);
    if (length < 0 || (unsigned long)length > FILE_SIZE_LIMIT ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        snprintf(error, error_size, "%s exceeds the native file safety bound", path);
        return false;
    }
    *size = (size_t)length;
    *data = malloc(*size == 0 ? 1 : *size);
    if (!*data) {
        fclose(file);
        snprintf(error, error_size, "cannot allocate buffer for %s", path);
        return false;
    }
    size_t count = fread(*data, 1, *size, file);
    bool complete = count == *size && ferror(file) == 0;
    fclose(file);
    if (!complete) {
        free(*data);
        *data = NULL;
        *size = 0;
        snprintf(error, error_size, "cannot read all of %s", path);
        return false;
    }
    return true;
}

static void list_games(void) {
    for (int i = 0; i < FUSION_ASSET_GAME_COUNT; ++i) {
        const FusionAssetGameSpec *spec = fusion_asset_game_spec((FusionAssetGame)i);
        printf("%s\t%s\t%zu bytes\tSHA-1 %s\t%s\n", spec->id, spec->title,
               spec->expected_size, spec->sha1, spec->filename);
    }
}

static int verify_games(const char *rom_dir, const char *aria_path, const char *metroid_path) {
    const char *paths[FUSION_ASSET_GAME_COUNT] = {metroid_path, aria_path};
    int failures = 0;
    char path[1024], error[256];
    for (int i = 0; i < FUSION_ASSET_GAME_COUNT; ++i) {
        const FusionAssetGameSpec *spec = fusion_asset_game_spec((FusionAssetGame)i);
        if (!paths[i]) {
            int written = snprintf(path, sizeof path, "%s/%s", rom_dir, spec->filename);
            if (written < 0 || (size_t)written >= sizeof path) {
                fprintf(stderr, "%s: ROM path is too long\n", spec->id);
                ++failures;
                continue;
            }
            paths[i] = path;
        }
        FusionRom rom = {0};
        if (fusion_rom_open((FusionAssetGame)i, paths[i], &rom, error, sizeof error)) {
            FusionGbaHeader header;
            if (!fusion_rom_read_gba_header(&rom, &header)) {
                fprintf(stderr, "%s: verified ROM has an invalid GBA header\n", spec->id);
                fusion_rom_close(&rom);
                ++failures;
                continue;
            }
            printf("%s: valid ROM (%s), title=%s, game_code=%s, version=%u, checksum=%s\n",
                   spec->id, spec->sha1, header.title, header.game_code,
                   header.software_version, header.checksum_valid ? "valid" : "invalid");
            fusion_rom_close(&rom);
        } else {
            fprintf(stderr, "%s: %s\n", spec->id, error);
            ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}

static int write_rom_manifest(const char *game_id, const char *rom_path,
                              const char *output_path) {
    const FusionAssetGameSpec *spec = fusion_asset_game_from_id(game_id);
    if (!spec) {
        fprintf(stderr, "rom-manifest: unsupported game %s\n", game_id);
        return 1;
    }
    FusionRom rom = {0};
    char error[256];
    if (!fusion_rom_open(spec->game, rom_path, &rom, error, sizeof error) ||
        !fusion_rom_write_manifest(&rom, output_path, error, sizeof error)) {
        fprintf(stderr, "rom-manifest: %s\n", error);
        fusion_rom_close(&rom);
        return 1;
    }
    fusion_rom_close(&rom);
    printf("%s: verified ROM manifest -> %s\n", spec->id, output_path);
    return 0;
}

static int inspect_header(const char *path) {
    uint8_t bytes[0xC0];
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "header: cannot open %s\n", path);
        return 1;
    }
    size_t count = fread(bytes, 1, sizeof bytes, file);
    bool read_error = ferror(file) != 0;
    fclose(file);
    if (read_error || count != sizeof bytes) {
        fprintf(stderr, "header: %s is shorter than the 0xC0-byte GBA header\n", path);
        return 1;
    }
    FusionGbaHeader header;
    if (!fusion_gba_read_header(bytes, sizeof bytes, &header)) {
        fprintf(stderr, "header: %s is not a valid standard GBA header\n", path);
        return 1;
    }
    printf("title=%s\n", header.title);
    printf("game_code=%s\n", header.game_code);
    printf("maker_code=%s\n", header.maker_code);
    printf("main_unit_code=0x%02X\n", header.main_unit_code);
    printf("device_type=0x%02X\n", header.device_type);
    printf("software_version=%u\n", header.software_version);
    printf("complement_checksum=0x%02X\n", header.complement_checksum);
    printf("checksum=%s\n", header.checksum_valid ? "valid" : "invalid");
    return header.checksum_valid ? 0 : 1;
}

static int inspect_aria_directory(const char *path) {
    FusionRom rom = {0};
    char error[256];
    if (!fusion_rom_open(FUSION_ASSET_GAME_ARIA, path, &rom, error, sizeof error)) {
        fprintf(stderr, "aria-directory: %s\n", error);
        return 1;
    }
    FusionAosAreaDirectory directory;
    if (!fusion_aos_read_area_directory(&rom, &directory, error, sizeof error)) {
        fprintf(stderr, "aria-directory: %s\n", error);
        fusion_rom_close(&rom);
        return 1;
    }
    for (size_t area = 0; area < FUSION_AOS_AREA_COUNT; ++area) {
        printf("area_%02zu=%zu\n", area, directory.areas[area].room_count);
    }
    printf("total_rooms=%zu\n", directory.total_rooms);
    fusion_rom_close(&rom);
    return 0;
}

static int inspect_aria_map(const char *path) {
    FusionRom rom = {0};
    char error[256];
    if (!fusion_rom_open(FUSION_ASSET_GAME_ARIA, path, &rom, error, sizeof error)) {
        fprintf(stderr, "aria-map: %s\n", error);
        return 1;
    }
    FusionAosAreaDirectory directory;
    FusionAosMap map;
    if (!fusion_aos_read_area_directory(&rom, &directory, error, sizeof error) ||
        !fusion_aos_read_map(&rom, &directory, &map, error, sizeof error)) {
        fprintf(stderr, "aria-map: %s\n", error);
        fusion_rom_close(&rom);
        return 1;
    }
    for (size_t index = 0; index < map.count; ++index) {
        const FusionAosMapCell *cell = &map.cells[index];
        printf("cell\t%u\t%u\t%u\t%u\t%u\t%u\t0x%08X\n",
               cell->map_x, cell->map_y, cell->area, cell->room,
               cell->save ? 1u : 0u, cell->warp ? 1u : 0u,
               cell->room_pointer);
    }
    printf("summary\t%zu\t%zu\t%zu\n", map.count, map.save_count, map.warp_count);
    fusion_rom_close(&rom);
    return 0;
}

static int inspect_aria_rooms(const char *path) {
    FusionRom rom = {0};
    char error[256];
    if (!fusion_rom_open(FUSION_ASSET_GAME_ARIA, path, &rom, error, sizeof error)) {
        fprintf(stderr, "aria-rooms: %s\n", error);
        return 1;
    }
    FusionAosAreaDirectory directory;
    if (!fusion_aos_read_area_directory(&rom, &directory, error, sizeof error)) {
        fprintf(stderr, "aria-rooms: %s\n", error);
        fusion_rom_close(&rom);
        return 1;
    }
    for (size_t area = 0; area < FUSION_AOS_AREA_COUNT; ++area) {
        for (size_t room = 0; room < directory.areas[area].room_count; ++room) {
            FusionAosRoomDescriptor descriptor;
            if (!fusion_aos_read_room_descriptor(&rom, &directory, area, room,
                                                 &descriptor, error, sizeof error)) {
                fprintf(stderr, "aria-rooms: area %zu room %zu: %s\n", area, room, error);
                fusion_rom_close(&rom);
                return 1;
            }
            printf("room\t%zu\t%zu\t0x%08X\t0x%08X\t%u\t0x%08X\t0x%08X\t"
                   "0x%08X\t0x%08X\t0x%08X\t0x%08X\t%zu\t%u\t%u\t%u\t%u\t%u\t%zu\t",
                   area, room, descriptor.source_pointer, descriptor.resolved_pointer,
                   descriptor.display_control, descriptor.next_pointer,
                   descriptor.backgrounds_pointer, descriptor.graphics_pointer,
                   descriptor.palette_pointer, descriptor.entities_pointer,
                   descriptor.transitions_pointer, descriptor.transition_count,
                   descriptor.blend_control, descriptor.map_x, descriptor.map_y,
                   descriptor.coordinate_flag_0 ? 1u : 0u,
                   descriptor.coordinate_flag_1 ? 1u : 0u, descriptor.variant_count);
            for (size_t index = 0; index < descriptor.variant_count; ++index) {
                if (index > 0) putchar(',');
                printf("0x%08X:0x%04X", descriptor.variants[index].pointer,
                       descriptor.variants[index].selector);
            }
            putchar('\n');
        }
    }
    fusion_rom_close(&rom);
    return 0;
}

static int inspect_aria_transitions(const char *path) {
    FusionRom rom = {0};
    char error[256];
    if (!fusion_rom_open(FUSION_ASSET_GAME_ARIA, path, &rom, error, sizeof error)) {
        fprintf(stderr, "aria-transitions: %s\n", error);
        return 1;
    }
    FusionAosAreaDirectory directory;
    if (!fusion_aos_read_area_directory(&rom, &directory, error, sizeof error)) {
        fprintf(stderr, "aria-transitions: %s\n", error);
        fusion_rom_close(&rom);
        return 1;
    }
    for (size_t area = 0; area < FUSION_AOS_AREA_COUNT; ++area) {
        for (size_t room = 0; room < directory.areas[area].room_count; ++room) {
            FusionAosRoomDescriptor descriptor;
            FusionAosTransitionList transitions;
            if (!fusion_aos_read_room_descriptor(&rom, &directory, area, room,
                                                 &descriptor, error, sizeof error) ||
                !fusion_aos_read_room_transitions(&rom, &directory, &descriptor,
                                                  &transitions, error, sizeof error)) {
                fprintf(stderr, "aria-transitions: area %zu room %zu: %s\n",
                        area, room, error);
                fusion_rom_close(&rom);
                return 1;
            }
            for (size_t index = 0; index < transitions.count; ++index) {
                const FusionAosTransition *transition = &transitions.entries[index];
                printf("transition\t%zu\t%zu\t%zu\t0x%08X\t0x%08X\t%d\t%d\t"
                       "%u\t%u\t%u\t%u\t%u\t%zu\t%zu\n",
                       area, room, index, transition->entry_pointer,
                       transition->target_pointer, transition->source_screen_x,
                       transition->source_screen_y, transition->field_6,
                       transition->field_8, transition->load_x, transition->load_y,
                       transition->field_e, transition->target_area,
                       transition->target_room);
            }
        }
    }
    fusion_rom_close(&rom);
    return 0;
}

static int inspect_aria_backgrounds(const char *path) {
    FusionRom rom = {0};
    char error[256];
    if (!fusion_rom_open(FUSION_ASSET_GAME_ARIA, path, &rom, error, sizeof error)) {
        fprintf(stderr, "aria-backgrounds: %s\n", error);
        return 1;
    }
    FusionAosAreaDirectory directory;
    if (!fusion_aos_read_area_directory(&rom, &directory, error, sizeof error)) {
        fprintf(stderr, "aria-backgrounds: %s\n", error);
        fusion_rom_close(&rom);
        return 1;
    }
    for (size_t area = 0; area < FUSION_AOS_AREA_COUNT; ++area) {
        for (size_t room = 0; room < directory.areas[area].room_count; ++room) {
            FusionAosRoomDescriptor descriptor;
            FusionAosBackgroundList backgrounds;
            if (!fusion_aos_read_room_descriptor(&rom, &directory, area, room,
                                                 &descriptor, error, sizeof error) ||
                !fusion_aos_read_room_backgrounds(&rom, &descriptor, &backgrounds,
                                                  error, sizeof error)) {
                fprintf(stderr, "aria-backgrounds: area %zu room %zu: %s\n",
                        area, room, error);
                fusion_rom_close(&rom);
                return 1;
            }
            for (size_t index = 0; index < FUSION_AOS_BACKGROUND_COUNT; ++index) {
                const FusionAosBackground *background = &backgrounds.entries[index];
                int width = background->has_dimensions ? background->width_screens : -1;
                int height = background->has_dimensions ? background->height_screens : -1;
                printf("background\t%zu\t%zu\t%u\t0x%08X\t%u\t%u\t%u\t%u\t%u\t"
                       "0x%08X\t%d\t%d\n",
                       area, room, background->layer, background->descriptor_pointer,
                       background->field_0, background->field_1, background->control,
                       background->field_4, background->field_6,
                       background->metadata_pointer, width, height);
            }
        }
    }
    fusion_rom_close(&rom);
    return 0;
}

static const char *collision_status_name(FusionAosCollisionStatus status) {
    switch (status) {
        case FUSION_AOS_COLLISION_ABSENT: return "absent";
        case FUSION_AOS_COLLISION_UNSUPPORTED_AFFINE: return "affine";
        case FUSION_AOS_COLLISION_NONE: return "none";
        case FUSION_AOS_COLLISION_DECODED: return "decoded";
    }
    return "invalid";
}

static const char *tilemap_status_name(FusionAosTilemapStatus status) {
    switch (status) {
        case FUSION_AOS_TILEMAP_ABSENT: return "absent";
        case FUSION_AOS_TILEMAP_UNSUPPORTED_AFFINE: return "affine";
        case FUSION_AOS_TILEMAP_DECODED: return "decoded";
    }
    return "invalid";
}

static int inspect_aria_background_tiles(const char *path) {
    FusionRom rom = {0};
    char error[256];
    if (!fusion_rom_open(FUSION_ASSET_GAME_ARIA, path, &rom, error, sizeof error)) {
        fprintf(stderr, "aria-background-tiles: %s\n", error);
        return 1;
    }
    FusionAosAreaDirectory directory;
    if (!fusion_aos_read_area_directory(&rom, &directory, error, sizeof error)) {
        fprintf(stderr, "aria-background-tiles: %s\n", error);
        fusion_rom_close(&rom);
        return 1;
    }
    uint8_t *canonical = malloc(FUSION_AOS_COLLISION_CELL_CAPACITY * 2);
    if (!canonical) {
        fprintf(stderr, "aria-background-tiles: cannot allocate hash buffer\n");
        fusion_rom_close(&rom);
        return 1;
    }
    size_t status_counts[3] = {0};
    size_t total_cells = 0;
    for (size_t area = 0; area < FUSION_AOS_AREA_COUNT; ++area) {
        for (size_t room = 0; room < directory.areas[area].room_count; ++room) {
            FusionAosRoomDescriptor descriptor;
            FusionAosBackgroundList backgrounds;
            if (!fusion_aos_read_room_descriptor(&rom, &directory, area, room,
                                                 &descriptor, error, sizeof error) ||
                !fusion_aos_read_room_backgrounds(&rom, &descriptor, &backgrounds,
                                                  error, sizeof error)) {
                fprintf(stderr, "aria-background-tiles: area %zu room %zu: %s\n",
                        area, room, error);
                free(canonical);
                fusion_rom_close(&rom);
                return 1;
            }
            for (size_t layer = 0; layer < FUSION_AOS_BACKGROUND_COUNT; ++layer) {
                FusionAosBackgroundTilemap tilemap;
                if (!fusion_aos_read_background_tiles(&rom,
                                                      &backgrounds.entries[layer],
                                                      &tilemap, error,
                                                      sizeof error)) {
                    fprintf(stderr,
                            "aria-background-tiles: area %zu room %zu layer %zu: %s\n",
                            area, room, layer, error);
                    free(canonical);
                    fusion_rom_close(&rom);
                    return 1;
                }
                char digest[41] = "-";
                if (tilemap.status == FUSION_AOS_TILEMAP_DECODED) {
                    for (size_t index = 0; index < tilemap.cell_count; ++index) {
                        canonical[index * 2] = (uint8_t)tilemap.cells[index];
                        canonical[index * 2 + 1] =
                            (uint8_t)(tilemap.cells[index] >> 8);
                    }
                    if (!fusion_rom_sha1_hex(canonical, tilemap.cell_count * 2,
                                             digest)) {
                        fprintf(stderr,
                                "aria-background-tiles: cannot hash area %zu room %zu layer %zu\n",
                                area, room, layer);
                        fusion_aos_free_background_tilemap(&tilemap);
                        free(canonical);
                        fusion_rom_close(&rom);
                        return 1;
                    }
                }
                ++status_counts[tilemap.status];
                total_cells += tilemap.cell_count;
                printf("tilemap\t%zu\t%zu\t%u\t%s\t0x%08X\t%u\t%u\t%u\t"
                       "0x%08X\t0x%08X\t%u\t%zu\t%zu\t%zu\t%s\n",
                       area, room, backgrounds.entries[layer].layer,
                       tilemap_status_name(tilemap.status),
                       tilemap.metadata_pointer, tilemap.width_screens,
                       tilemap.height_screens, tilemap.flags,
                       tilemap.blocks_pointer, tilemap.map_pointer,
                       tilemap.maximum_block, tilemap.width_tiles,
                       tilemap.height_tiles, tilemap.cell_count, digest);
                fusion_aos_free_background_tilemap(&tilemap);
            }
        }
    }
    printf("summary\t%zu\t%zu\t%zu\t%zu\n",
           status_counts[FUSION_AOS_TILEMAP_DECODED],
           status_counts[FUSION_AOS_TILEMAP_ABSENT],
           status_counts[FUSION_AOS_TILEMAP_UNSUPPORTED_AFFINE], total_cells);
    free(canonical);
    fusion_rom_close(&rom);
    return 0;
}

static int inspect_aria_background_renders(const char *path) {
    FusionRom rom = {0};
    char error[256];
    if (!fusion_rom_open(FUSION_ASSET_GAME_ARIA, path, &rom, error, sizeof error)) {
        fprintf(stderr, "aria-background-renders: %s\n", error);
        return 1;
    }
    FusionAosAreaDirectory directory;
    if (!fusion_aos_read_area_directory(&rom, &directory, error, sizeof error)) {
        fprintf(stderr, "aria-background-renders: %s\n", error);
        fusion_rom_close(&rom);
        return 1;
    }
    FusionAosVideoMemory *memory = malloc(sizeof *memory);
    if (!memory) {
        fprintf(stderr, "aria-background-renders: cannot allocate video memory\n");
        fusion_rom_close(&rom);
        return 1;
    }
    size_t rendered_count = 0, skipped_count = 0;
    size_t total_pixels = 0, unresolved_count = 0;
    for (size_t area = 0; area < FUSION_AOS_AREA_COUNT; ++area) {
        for (size_t room = 0; room < directory.areas[area].room_count; ++room) {
            FusionAosRoomDescriptor descriptor;
            FusionAosBackgroundList backgrounds;
            if (!fusion_aos_read_room_descriptor(&rom, &directory, area, room,
                                                 &descriptor, error, sizeof error) ||
                !fusion_aos_read_room_backgrounds(&rom, &descriptor, &backgrounds,
                                                  error, sizeof error) ||
                !fusion_aos_load_video_memory(&rom, &descriptor, memory,
                                              error, sizeof error)) {
                fprintf(stderr,
                        "aria-background-renders: area %zu room %zu: %s\n",
                        area, room, error);
                free(memory);
                fusion_rom_close(&rom);
                return 1;
            }
            for (size_t index = 0; index < FUSION_AOS_BACKGROUND_COUNT; ++index) {
                FusionAosBackgroundTilemap tilemap;
                if (!fusion_aos_read_background_tiles(&rom,
                                                      &backgrounds.entries[index],
                                                      &tilemap, error,
                                                      sizeof error)) {
                    fprintf(stderr,
                            "aria-background-renders: area %zu room %zu layer %u: %s\n",
                            area, room, backgrounds.entries[index].layer, error);
                    free(memory);
                    fusion_rom_close(&rom);
                    return 1;
                }
                if (tilemap.status != FUSION_AOS_TILEMAP_DECODED) {
                    ++skipped_count;
                    fusion_aos_free_background_tilemap(&tilemap);
                    continue;
                }
                FusionAosRenderedBackground rendered;
                if (!fusion_aos_render_background(&backgrounds.entries[index],
                                                  &tilemap, memory, &rendered,
                                                  error, sizeof error)) {
                    fprintf(stderr,
                            "aria-background-renders: area %zu room %zu layer %u: %s\n",
                            area, room, backgrounds.entries[index].layer, error);
                    fusion_aos_free_background_tilemap(&tilemap);
                    free(memory);
                    fusion_rom_close(&rom);
                    return 1;
                }
                char digest[41];
                if (!fusion_rom_sha1_hex(rendered.rgba,
                                         rendered.pixel_count * 4, digest)) {
                    fprintf(stderr,
                            "aria-background-renders: cannot hash area %zu room %zu layer %u\n",
                            area, room, backgrounds.entries[index].layer);
                    fusion_aos_free_rendered_background(&rendered);
                    fusion_aos_free_background_tilemap(&tilemap);
                    free(memory);
                    fusion_rom_close(&rom);
                    return 1;
                }
                printf("render\t%zu\t%zu\t%u\t%zu\t%zu\t%zu\t%zu\t%s\n",
                       area, room, backgrounds.entries[index].layer,
                       rendered.width_pixels, rendered.height_pixels,
                       rendered.pixel_count, rendered.unresolved_count, digest);
                ++rendered_count;
                total_pixels += rendered.pixel_count;
                unresolved_count += rendered.unresolved_count;
                fusion_aos_free_rendered_background(&rendered);
                fusion_aos_free_background_tilemap(&tilemap);
            }
        }
    }
    printf("summary\t%zu\t%zu\t%zu\t%zu\n", rendered_count, skipped_count,
           total_pixels, unresolved_count);
    free(memory);
    fusion_rom_close(&rom);
    return 0;
}

static void free_aria_rendered_layers(FusionAosRenderedBackground *rendered,
                                      size_t count) {
    for (size_t index = 0; index < count; ++index)
        fusion_aos_free_rendered_background(&rendered[index]);
}

static int inspect_aria_background_composites(const char *path) {
    FusionRom rom = {0};
    char error[256];
    if (!fusion_rom_open(FUSION_ASSET_GAME_ARIA, path, &rom,
                         error, sizeof error)) {
        fprintf(stderr, "aria-background-composites: %s\n", error);
        return 1;
    }
    FusionAosAreaDirectory directory;
    if (!fusion_aos_read_area_directory(&rom, &directory,
                                        error, sizeof error)) {
        fprintf(stderr, "aria-background-composites: %s\n", error);
        fusion_rom_close(&rom);
        return 1;
    }
    FusionAosVideoMemory *memory = malloc(sizeof *memory);
    if (!memory) {
        fprintf(stderr,
                "aria-background-composites: cannot allocate video memory\n");
        fusion_rom_close(&rom);
        return 1;
    }
    size_t composite_count = 0, skipped_count = 0;
    size_t rendered_layer_count = 0, total_pixels = 0;
    size_t total_bitmap_bytes = 0;
    for (size_t area = 0; area < FUSION_AOS_AREA_COUNT; ++area) {
        for (size_t room = 0; room < directory.areas[area].room_count; ++room) {
            FusionAosRoomDescriptor descriptor;
            FusionAosBackgroundList backgrounds;
            if (!fusion_aos_read_room_descriptor(&rom, &directory, area, room,
                                                 &descriptor, error, sizeof error) ||
                !fusion_aos_read_room_backgrounds(&rom, &descriptor,
                                                  &backgrounds,
                                                  error, sizeof error) ||
                !fusion_aos_load_video_memory(&rom, &descriptor, memory,
                                              error, sizeof error)) {
                fprintf(stderr,
                        "aria-background-composites: area %zu room %zu: %s\n",
                        area, room, error);
                free(memory);
                fusion_rom_close(&rom);
                return 1;
            }
            FusionAosRenderedBackground rendered[FUSION_AOS_BACKGROUND_COUNT] = {0};
            FusionAosCompositeLayer layers[FUSION_AOS_BACKGROUND_COUNT];
            size_t layer_count = 0;
            for (size_t index = 0; index < FUSION_AOS_BACKGROUND_COUNT; ++index) {
                FusionAosBackgroundTilemap tilemap;
                if (!fusion_aos_read_background_tiles(
                        &rom, &backgrounds.entries[index], &tilemap,
                        error, sizeof error)) {
                    fprintf(stderr,
                            "aria-background-composites: area %zu room %zu layer %u: %s\n",
                            area, room, backgrounds.entries[index].layer, error);
                    free_aria_rendered_layers(rendered, layer_count);
                    free(memory);
                    fusion_rom_close(&rom);
                    return 1;
                }
                if (tilemap.status == FUSION_AOS_TILEMAP_DECODED) {
                    if (!fusion_aos_render_background(
                            &backgrounds.entries[index], &tilemap, memory,
                            &rendered[layer_count], error, sizeof error)) {
                        fprintf(stderr,
                                "aria-background-composites: area %zu room %zu layer %u: %s\n",
                                area, room, backgrounds.entries[index].layer,
                                error);
                        fusion_aos_free_background_tilemap(&tilemap);
                        free_aria_rendered_layers(rendered, layer_count);
                        free(memory);
                        fusion_rom_close(&rom);
                        return 1;
                    }
                    layers[layer_count] = (FusionAosCompositeLayer){
                        &backgrounds.entries[index], &rendered[layer_count]
                    };
                    ++layer_count;
                }
                fusion_aos_free_background_tilemap(&tilemap);
            }
            if (layer_count == 0) {
                printf("skipped\t%zu\t%zu\n", area, room);
                ++skipped_count;
                continue;
            }
            FusionAosCompositeBackground composite;
            if (!fusion_aos_composite_backgrounds(
                    layers, layer_count, &composite, error, sizeof error)) {
                fprintf(stderr,
                        "aria-background-composites: area %zu room %zu: %s\n",
                        area, room, error);
                free_aria_rendered_layers(rendered, layer_count);
                free(memory);
                fusion_rom_close(&rom);
                return 1;
            }
            char digest[41];
            if (!fusion_rom_sha1_hex(composite.rgb,
                                     composite.pixel_count * 3, digest)) {
                fprintf(stderr,
                        "aria-background-composites: cannot hash area %zu room %zu\n",
                        area, room);
                fusion_aos_free_composite_background(&composite);
                free_aria_rendered_layers(rendered, layer_count);
                free(memory);
                fusion_rom_close(&rom);
                return 1;
            }
            FusionAosBitmap24 bitmap;
            if (!fusion_aos_serialize_composite_bmp24(
                    &composite, &bitmap, error, sizeof error)) {
                fprintf(stderr,
                        "aria-background-composites: cannot serialize area %zu room %zu: %s\n",
                        area, room, error);
                fusion_aos_free_composite_background(&composite);
                free_aria_rendered_layers(rendered, layer_count);
                free(memory);
                fusion_rom_close(&rom);
                return 1;
            }
            char bitmap_digest[41];
            if (!fusion_rom_sha1_hex(bitmap.data, bitmap.size,
                                     bitmap_digest)) {
                fprintf(stderr,
                        "aria-background-composites: cannot hash bitmap for area %zu room %zu\n",
                        area, room);
                fusion_aos_free_bitmap24(&bitmap);
                fusion_aos_free_composite_background(&composite);
                free_aria_rendered_layers(rendered, layer_count);
                free(memory);
                fusion_rom_close(&rom);
                return 1;
            }
            printf("composite\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%s\t%zu\t%s\n",
                   area, room, composite.width_pixels,
                   composite.height_pixels, composite.pixel_count,
                   layer_count, digest, bitmap.size, bitmap_digest);
            ++composite_count;
            rendered_layer_count += layer_count;
            total_pixels += composite.pixel_count;
            total_bitmap_bytes += bitmap.size;
            fusion_aos_free_bitmap24(&bitmap);
            fusion_aos_free_composite_background(&composite);
            free_aria_rendered_layers(rendered, layer_count);
        }
    }
    printf("summary\t%zu\t%zu\t%zu\t%zu\t%zu\n", composite_count,
           skipped_count, rendered_layer_count, total_pixels,
           total_bitmap_bytes);
    free(memory);
    fusion_rom_close(&rom);
    return 0;
}

static int inspect_aria_collisions(const char *path) {
    FusionRom rom = {0};
    char error[256];
    if (!fusion_rom_open(FUSION_ASSET_GAME_ARIA, path, &rom, error, sizeof error)) {
        fprintf(stderr, "aria-collisions: %s\n", error);
        return 1;
    }
    FusionAosAreaDirectory directory;
    if (!fusion_aos_read_area_directory(&rom, &directory, error, sizeof error)) {
        fprintf(stderr, "aria-collisions: %s\n", error);
        fusion_rom_close(&rom);
        return 1;
    }
    size_t status_counts[4] = {0};
    size_t total_cells = 0;
    for (size_t area = 0; area < FUSION_AOS_AREA_COUNT; ++area) {
        for (size_t room = 0; room < directory.areas[area].room_count; ++room) {
            FusionAosRoomDescriptor descriptor;
            FusionAosBackgroundList backgrounds;
            FusionAosCollisionMap collision;
            if (!fusion_aos_read_room_descriptor(&rom, &directory, area, room,
                                                 &descriptor, error, sizeof error) ||
                !fusion_aos_read_room_backgrounds(&rom, &descriptor, &backgrounds,
                                                  error, sizeof error) ||
                !fusion_aos_read_background_collision(&rom, &backgrounds.entries[0],
                                                      &collision, error,
                                                      sizeof error)) {
                fprintf(stderr, "aria-collisions: area %zu room %zu: %s\n",
                        area, room, error);
                fusion_rom_close(&rom);
                return 1;
            }
            char digest[41] = "-";
            if (collision.status == FUSION_AOS_COLLISION_DECODED &&
                !fusion_rom_sha1_hex(collision.cells, collision.cell_count, digest)) {
                fprintf(stderr, "aria-collisions: cannot hash area %zu room %zu\n",
                        area, room);
                fusion_aos_free_collision_map(&collision);
                fusion_rom_close(&rom);
                return 1;
            }
            ++status_counts[collision.status];
            total_cells += collision.cell_count;
            printf("collision\t%zu\t%zu\t%s\t0x%08X\t%u\t%u\t%u\t0x%08X\t"
                   "0x%08X\t0x%08X\t%u\t%zu\t%zu\t%zu\t%s\n",
                   area, room, collision_status_name(collision.status),
                   collision.metadata_pointer, collision.width_screens,
                   collision.height_screens, collision.flags,
                   collision.blocks_pointer, collision.collision_pointer,
                   collision.map_pointer, collision.maximum_block,
                   collision.width_tiles, collision.height_tiles,
                   collision.cell_count, digest);
            fusion_aos_free_collision_map(&collision);
        }
    }
    printf("summary\t%zu\t%zu\t%zu\t%zu\t%zu\n",
           status_counts[FUSION_AOS_COLLISION_DECODED],
           status_counts[FUSION_AOS_COLLISION_ABSENT],
           status_counts[FUSION_AOS_COLLISION_UNSUPPORTED_AFFINE],
           status_counts[FUSION_AOS_COLLISION_NONE], total_cells);
    fusion_rom_close(&rom);
    return 0;
}

static uint8_t aria_room_flags(const FusionAosMap *map,
                               size_t area, size_t room) {
    uint8_t flags = 0;
    for (size_t index = 0; index < map->count; ++index) {
        const FusionAosMapCell *cell = &map->cells[index];
        if (cell->area != area || cell->room != room) continue;
        if (cell->save) flags |= FUSION_AOS_ROOM_FLAG_SAVE;
        if (cell->warp) flags |= FUSION_AOS_ROOM_FLAG_WARP;
    }
    return flags;
}

static int export_aria_runtime_rooms(const char *path, const char *output) {
    FusionRom rom = {0};
    char error[512];
    static FusionAosExportSummary summary;
    if (!fusion_rom_open(FUSION_ASSET_GAME_ARIA, path, &rom, error, sizeof error) ||
        !fusion_aos_export_runtime_rooms(&rom, output, &summary, error,
                                         sizeof error)) {
        fprintf(stderr, "aria-export-rooms: %s\n", error);
        fusion_rom_close(&rom);
        return 1;
    }
    printf("Aria runtime rooms exported: %zu, skipped: %zu\n", summary.room_count,
           summary.skipped_count);
    for (size_t index = 0; index < summary.skipped_count; ++index)
        printf("  skipped %s\n", summary.skipped[index]);
    fusion_rom_close(&rom);
    return 0;
}

static int export_aria_soma(const char *path, const char *output) {
    FusionRom rom = {0};
    FusionAosSomaExportSummary summary;
    char error[256];
    if (!fusion_rom_open(FUSION_ASSET_GAME_ARIA, path, &rom,
                         error, sizeof error) ||
        !fusion_aos_export_soma(&rom, output, &summary, error, sizeof error)) {
        fprintf(stderr, "aria-export-soma: %s\n", error);
        fusion_rom_close(&rom);
        return 1;
    }
    printf("Soma runtime library: %zu Soma and %zu knife animations, "
           "%zu frames, %zu unique BMPs\n",
           summary.soma_animations, summary.knife_animations,
           summary.frames, summary.unique_bmps);
    printf("Index: %s/runtime_index.tsv\n", output);
    fusion_rom_close(&rom);
    return 0;
}

static int export_aria_weapons(const char *path, const char *metadata,
                               const char *runtime) {
    FusionRom rom = {0};
    FusionAosWeaponExportSummary summary;
    char error[256];
    if (!fusion_rom_open(FUSION_ASSET_GAME_ARIA, path, &rom,
                         error, sizeof error) ||
        !fusion_aos_export_weapons(&rom, metadata, runtime, &summary,
                                   error, sizeof error)) {
        fprintf(stderr, "aria-export-weapons: %s\n", error);
        fusion_rom_close(&rom);
        return 1;
    }
    printf("Aria weapons: %zu weapons and the unarmed record; "
           "%zu weapon sprite sequences; %zu components "
           "outside their tile sheet skipped\n",
           summary.weapons, summary.sequences, summary.outside_components);
    for (size_t index = 0; index < summary.skipped_count; ++index)
        printf("  skipped %s\n", summary.skipped[index]);
    fusion_rom_close(&rom);
    return 0;
}

static int export_aria_objects(const char *path, const char *metadata,
                               const char *runtime) {
    FusionRom rom = {0};
    FusionAosObjectExportSummary summary;
    char error[256];
    if (!fusion_rom_open(FUSION_ASSET_GAME_ARIA, path, &rom,
                         error, sizeof error) ||
        !fusion_aos_export_objects(&rom, metadata, runtime, &summary,
                                   error, sizeof error)) {
        fprintf(stderr, "aria-export-objects: %s\n", error);
        fusion_rom_close(&rom);
        return 1;
    }
    printf("Aria object library: %zu sequences, %zu frames\n",
           summary.sequences, summary.frames);
    fusion_rom_close(&rom);
    return 0;
}

static int inspect_aria_runtime_rooms(const char *path) {
    FusionRom rom = {0};
    char error[256];
    if (!fusion_rom_open(FUSION_ASSET_GAME_ARIA, path, &rom,
                         error, sizeof error)) {
        fprintf(stderr, "aria-runtime-rooms: %s\n", error);
        return 1;
    }
    FusionAosAreaDirectory directory;
    FusionAosMap map;
    if (!fusion_aos_read_area_directory(&rom, &directory,
                                        error, sizeof error) ||
        !fusion_aos_read_map(&rom, &directory, &map,
                             error, sizeof error)) {
        fprintf(stderr, "aria-runtime-rooms: %s\n", error);
        fusion_rom_close(&rom);
        return 1;
    }
    size_t exported_count = 0, skipped_count = 0, total_cells = 0;
    size_t total_transitions = 0, total_entities = 0, total_bytes = 0;
    size_t save_room_count = 0, warp_room_count = 0;
    for (size_t area = 0; area < FUSION_AOS_AREA_COUNT; ++area) {
        for (size_t room = 0; room < directory.areas[area].room_count; ++room) {
            FusionAosRoomDescriptor descriptor;
            FusionAosBackgroundList backgrounds;
            FusionAosCollisionMap collision;
            if (!fusion_aos_read_room_descriptor(&rom, &directory, area, room,
                                                 &descriptor, error, sizeof error) ||
                !fusion_aos_read_room_backgrounds(&rom, &descriptor,
                                                  &backgrounds,
                                                  error, sizeof error) ||
                !fusion_aos_read_background_collision(
                    &rom, &backgrounds.entries[0], &collision,
                    error, sizeof error)) {
                fprintf(stderr,
                        "aria-runtime-rooms: area %zu room %zu: %s\n",
                        area, room, error);
                fusion_rom_close(&rom);
                return 1;
            }
            if (collision.status != FUSION_AOS_COLLISION_DECODED) {
                printf("skipped\t%zu\t%zu\t%s\n", area, room,
                       collision_status_name(collision.status));
                ++skipped_count;
                fusion_aos_free_collision_map(&collision);
                continue;
            }
            FusionAosTransitionList transitions;
            FusionAosEntityList entities;
            if (!fusion_aos_read_room_transitions(
                    &rom, &directory, &descriptor, &transitions,
                    error, sizeof error) ||
                !fusion_aos_read_entity_list(
                    &rom, descriptor.entities_pointer, &entities,
                    error, sizeof error)) {
                fprintf(stderr,
                        "aria-runtime-rooms: area %zu room %zu: %s\n",
                        area, room, error);
                fusion_aos_free_collision_map(&collision);
                fusion_rom_close(&rom);
                return 1;
            }
            uint8_t flags = aria_room_flags(&map, area, room);
            FusionAosRuntimeRoomPayload payload;
            if (!fusion_aos_serialize_runtime_room(
                    area, room, &collision, &transitions, &entities, flags,
                    &payload, error, sizeof error)) {
                fprintf(stderr,
                        "aria-runtime-rooms: cannot serialize area %zu room %zu: %s\n",
                        area, room, error);
                fusion_aos_free_collision_map(&collision);
                fusion_rom_close(&rom);
                return 1;
            }
            char digest[41];
            if (!fusion_rom_sha1_hex(payload.data, payload.size, digest)) {
                fprintf(stderr,
                        "aria-runtime-rooms: cannot hash area %zu room %zu\n",
                        area, room);
                fusion_aos_free_runtime_room_payload(&payload);
                fusion_aos_free_collision_map(&collision);
                fusion_rom_close(&rom);
                return 1;
            }
            printf("room\t%zu\t%zu\t%u\t%u\t%zu\t%zu\t%zu\t%zu\t%zu\t%u\t%zu\t%s\n",
                   area, room, (unsigned)collision.width_screens,
                   (unsigned)collision.height_screens,
                   collision.width_tiles, collision.height_tiles,
                   collision.cell_count, transitions.count, entities.count,
                   (unsigned)flags, payload.size, digest);
            ++exported_count;
            total_cells += collision.cell_count;
            total_transitions += transitions.count;
            total_entities += entities.count;
            total_bytes += payload.size;
            save_room_count +=
                (flags & FUSION_AOS_ROOM_FLAG_SAVE) != 0 ? 1u : 0u;
            warp_room_count +=
                (flags & FUSION_AOS_ROOM_FLAG_WARP) != 0 ? 1u : 0u;
            fusion_aos_free_runtime_room_payload(&payload);
            fusion_aos_free_collision_map(&collision);
        }
    }
    printf("summary\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\n",
           exported_count, skipped_count, total_cells, total_transitions,
           total_entities, total_bytes, save_room_count, warp_room_count);
    fusion_rom_close(&rom);
    return 0;
}

static bool print_aria_load_list(const FusionRom *rom, size_t area, size_t room,
                                 const char *kind, uint32_t pointer,
                                 char *error, size_t error_size) {
    FusionAosLoadList list;
    if (!fusion_aos_read_load_list(rom, pointer, &list, error, error_size)) return false;
    for (size_t index = 0; index < list.count; ++index) {
        const FusionAosLoadEntry *entry = &list.entries[index];
        printf("load\t%zu\t%zu\t%s\t%zu\t0x%08X\t0x%08X\t%u\t%u\t%u\n",
               area, room, kind, index, entry->entry_pointer, entry->resource_pointer,
               entry->parameters[0], entry->parameters[1], entry->parameters[2]);
    }
    return true;
}

static int inspect_aria_loads(const char *path) {
    FusionRom rom = {0};
    char error[256];
    if (!fusion_rom_open(FUSION_ASSET_GAME_ARIA, path, &rom, error, sizeof error)) {
        fprintf(stderr, "aria-loads: %s\n", error);
        return 1;
    }
    FusionAosAreaDirectory directory;
    if (!fusion_aos_read_area_directory(&rom, &directory, error, sizeof error)) {
        fprintf(stderr, "aria-loads: %s\n", error);
        fusion_rom_close(&rom);
        return 1;
    }
    for (size_t area = 0; area < FUSION_AOS_AREA_COUNT; ++area) {
        for (size_t room = 0; room < directory.areas[area].room_count; ++room) {
            FusionAosRoomDescriptor descriptor;
            if (!fusion_aos_read_room_descriptor(&rom, &directory, area, room,
                                                 &descriptor, error, sizeof error) ||
                !print_aria_load_list(&rom, area, room, "graphics",
                                      descriptor.graphics_pointer,
                                      error, sizeof error) ||
                !print_aria_load_list(&rom, area, room, "palette",
                                      descriptor.palette_pointer,
                                      error, sizeof error)) {
                fprintf(stderr, "aria-loads: area %zu room %zu: %s\n",
                        area, room, error);
                fusion_rom_close(&rom);
                return 1;
            }
        }
    }
    fusion_rom_close(&rom);
    return 0;
}

static int inspect_aria_video_memory(const char *path) {
    FusionRom rom = {0};
    char error[256];
    if (!fusion_rom_open(FUSION_ASSET_GAME_ARIA, path, &rom, error, sizeof error)) {
        fprintf(stderr, "aria-video-memory: %s\n", error);
        return 1;
    }
    FusionAosAreaDirectory directory;
    if (!fusion_aos_read_area_directory(&rom, &directory, error, sizeof error)) {
        fprintf(stderr, "aria-video-memory: %s\n", error);
        fusion_rom_close(&rom);
        return 1;
    }
    FusionAosVideoMemory *memory = malloc(sizeof *memory);
    if (!memory) {
        fprintf(stderr, "aria-video-memory: cannot allocate output buffers\n");
        fusion_rom_close(&rom);
        return 1;
    }
    size_t room_count = 0, graphics_count = 0, palette_count = 0;
    for (size_t area = 0; area < FUSION_AOS_AREA_COUNT; ++area) {
        for (size_t room = 0; room < directory.areas[area].room_count; ++room) {
            FusionAosRoomDescriptor descriptor;
            if (!fusion_aos_read_room_descriptor(&rom, &directory, area, room,
                                                 &descriptor, error, sizeof error) ||
                !fusion_aos_load_video_memory(&rom, &descriptor, memory,
                                              error, sizeof error)) {
                fprintf(stderr, "aria-video-memory: area %zu room %zu: %s\n",
                        area, room, error);
                free(memory);
                fusion_rom_close(&rom);
                return 1;
            }
            char vram_digest[41], palette_digest[41];
            if (!fusion_rom_sha1_hex(memory->vram, sizeof memory->vram,
                                     vram_digest) ||
                !fusion_rom_sha1_hex(memory->palette, sizeof memory->palette,
                                     palette_digest)) {
                fprintf(stderr, "aria-video-memory: cannot hash area %zu room %zu\n",
                        area, room);
                free(memory);
                fusion_rom_close(&rom);
                return 1;
            }
            printf("video\t%zu\t%zu\t%zu\t%zu\t%s\t%s\n",
                   area, room, memory->graphics_load_count,
                   memory->palette_load_count, vram_digest, palette_digest);
            ++room_count;
            graphics_count += memory->graphics_load_count;
            palette_count += memory->palette_load_count;
        }
    }
    printf("summary\t%zu\t%zu\t%zu\n", room_count, graphics_count, palette_count);
    free(memory);
    fusion_rom_close(&rom);
    return 0;
}

static int inspect_aria_entities(const char *path) {
    FusionRom rom = {0};
    char error[256];
    if (!fusion_rom_open(FUSION_ASSET_GAME_ARIA, path, &rom, error, sizeof error)) {
        fprintf(stderr, "aria-entities: %s\n", error);
        return 1;
    }
    FusionAosAreaDirectory directory;
    if (!fusion_aos_read_area_directory(&rom, &directory, error, sizeof error)) {
        fprintf(stderr, "aria-entities: %s\n", error);
        fusion_rom_close(&rom);
        return 1;
    }
    for (size_t area = 0; area < FUSION_AOS_AREA_COUNT; ++area) {
        for (size_t room = 0; room < directory.areas[area].room_count; ++room) {
            FusionAosRoomDescriptor descriptor;
            FusionAosEntityList entities;
            if (!fusion_aos_read_room_descriptor(&rom, &directory, area, room,
                                                 &descriptor, error, sizeof error) ||
                !fusion_aos_read_entity_list(&rom, descriptor.entities_pointer,
                                             &entities, error, sizeof error)) {
                fprintf(stderr, "aria-entities: area %zu room %zu: %s\n",
                        area, room, error);
                fusion_rom_close(&rom);
                return 1;
            }
            for (size_t index = 0; index < entities.count; ++index) {
                const FusionAosEntity *entity = &entities.entries[index];
                printf("entity\t%zu\t%zu\t%zu\t0x%08X\t%d\t%d\t%u\t%u\t%u\t%u\t"
                       "%u\t%u\n",
                       area, room, index, entity->entry_pointer, entity->x, entity->y,
                       entity->persistent_index, entity->kind, entity->entity_id,
                       entity->flags, entity->parameters[0], entity->parameters[1]);
            }
        }
    }
    fusion_rom_close(&rom);
    return 0;
}

static int inspect_aria_bosses(const char *path) {
    FusionRom rom = {0};
    char error[256];
    if (!fusion_rom_open(FUSION_ASSET_GAME_ARIA, path, &rom, error, sizeof error)) {
        fprintf(stderr, "aria-bosses: %s\n", error);
        return 1;
    }
    FusionAosAreaDirectory directory;
    if (!fusion_aos_read_area_directory(&rom, &directory, error, sizeof error)) {
        fprintf(stderr, "aria-bosses: %s\n", error);
        fusion_rom_close(&rom);
        return 1;
    }
    for (size_t index = 0; index < FUSION_AOS_BOSS_ROOM_COUNT; ++index) {
        const FusionAosBossSpec *spec = fusion_aos_boss_spec(index);
        FusionAosBossRoom boss;
        if (!spec || !fusion_aos_read_boss_room(&rom, &directory, spec, &boss,
                                                error, sizeof error)) {
            fprintf(stderr, "aria-bosses: entry %zu: %s\n", index, error);
            fusion_rom_close(&rom);
            return 1;
        }
        printf("boss\t%zu\t%s\t%s\t%s\t%u\t%u\t%u\t%u\t0x%08X\t"
               "0x%08X\t%u\t0x%08X\t%u\n",
               index, spec->id, spec->name, spec->phase, spec->area, spec->room,
               boss.map_x, boss.map_y, boss.room_pointer, boss.entity_pointer,
               spec->enemy_id, boss.enemy_table_pointer, boss.health);
    }
    fusion_rom_close(&rom);
    return 0;
}

static int inspect_aria_summary(const char *path) {
    FusionRom rom = {0};
    char error[256];
    if (!fusion_rom_open(FUSION_ASSET_GAME_ARIA, path, &rom, error, sizeof error)) {
        fprintf(stderr, "aria-summary: %s\n", error);
        return 1;
    }
    FusionAosWorldSummary summary;
    if (!fusion_aos_read_world_summary(&rom, &summary, error, sizeof error)) {
        fprintf(stderr, "aria-summary: %s\n", error);
        fusion_rom_close(&rom);
        return 1;
    }
    printf("native_room_count=%zu\n", summary.native_room_count);
    printf("mapped_cells=%zu\n", summary.mapped_cells);
    printf("transition_count=%zu\n", summary.transition_count);
    printf("entity_count=%zu\n", summary.entity_count);
    printf("boss_room_count=%zu\n", summary.boss_room_count);
    printf("savepoint_count=%zu\n", summary.savepoint_count);
    printf("warp_count=%zu\n", summary.warp_count);
    fusion_rom_close(&rom);
    return 0;
}

static int write_aria_catalog(const char *rom_path, const char *output_path,
                              const char *index_path) {
    FusionRom rom = {0};
    char error[256];
    if (!fusion_rom_open(FUSION_ASSET_GAME_ARIA, rom_path, &rom,
                         error, sizeof error) ||
        !fusion_aos_write_world_catalog(&rom, output_path, error, sizeof error) ||
        !fusion_aos_write_room_index(&rom, index_path, error, sizeof error)) {
        fprintf(stderr, "aria-catalog: %s\n", error);
        fusion_rom_close(&rom);
        return 1;
    }
    printf("Aria world catalog -> %s, %s\n", output_path, index_path);
    fusion_rom_close(&rom);
    return 0;
}

static int inspect_mzm_symbols(const char *rom_path, const char *elf_path) {
    FusionRom rom = {0};
    uint8_t *elf = NULL;
    size_t elf_size = 0;
    char error[256];
    if (!fusion_rom_open(FUSION_ASSET_GAME_METROID, rom_path, &rom,
                         error, sizeof error) ||
        !read_file(elf_path, &elf, &elf_size, error, sizeof error)) {
        fprintf(stderr, "mzm-symbols: %s\n", error);
        fusion_rom_close(&rom);
        free(elf);
        return 1;
    }
    FusionMzmRomSymbolTable table;
    if (!fusion_mzm_read_rom_symbols(&rom, elf, elf_size, &table,
                                     error, sizeof error)) {
        fprintf(stderr, "mzm-symbols: %s\n", error);
        fusion_rom_close(&rom);
        free(elf);
        return 1;
    }
    for (size_t index = 0; index < table.count; ++index) {
        const FusionMzmRomSymbol *symbol = &table.entries[index];
        printf("symbol\t%s\t%u\t%u\n", symbol->name,
               symbol->address, symbol->size);
    }
    printf("summary\t%zu\t%zu\n", table.count, table.duplicate_name_count);
    fusion_mzm_free_symbols(&table);
    fusion_rom_close(&rom);
    free(elf);
    return 0;
}

static int write_mzm_rom_bank(const char *rom_path, const char *elf_path,
                              const char *output_directory) {
    FusionRom rom = {0};
    uint8_t *elf = NULL;
    size_t elf_size = 0;
    char error[256];
    if (!fusion_rom_open(FUSION_ASSET_GAME_METROID, rom_path, &rom,
                         error, sizeof error) ||
        !read_file(elf_path, &elf, &elf_size, error, sizeof error)) {
        fprintf(stderr, "mzm-rom-bank: %s\n", error);
        fusion_rom_close(&rom);
        free(elf);
        return 1;
    }
    FusionMzmRomSymbolTable table;
    if (!fusion_mzm_read_rom_symbols(&rom, elf, elf_size, &table,
                                     error, sizeof error)) {
        fprintf(stderr, "mzm-rom-bank: %s\n", error);
        fusion_rom_close(&rom);
        free(elf);
        return 1;
    }
    char bank_path[1024];
    char manifest_path[1024];
    int bank_length = snprintf(bank_path, sizeof bank_path, "%s/rom_bank.bin",
                               output_directory);
    int manifest_length = snprintf(manifest_path, sizeof manifest_path,
                                   "%s/rom_symbols.tsv", output_directory);
    if (bank_length < 0 || (size_t)bank_length >= sizeof bank_path ||
        manifest_length < 0 || (size_t)manifest_length >= sizeof manifest_path ||
        !fusion_mzm_write_rom_bank(&rom, &table, bank_path, manifest_path,
                                   error, sizeof error)) {
        fprintf(stderr, "mzm-rom-bank: %s\n",
                error[0] ? error : "output path is too long");
        fusion_mzm_free_symbols(&table);
        fusion_rom_close(&rom);
        free(elf);
        return 1;
    }
    printf("Zero Mission ROM bank: %zu data symbols, %zu duplicate names -> %s\n",
           table.count, table.duplicate_name_count, output_directory);
    fusion_mzm_free_symbols(&table);
    fusion_rom_close(&rom);
    free(elf);
    return 0;
}

static int inspect_mzm_rooms(const char *rom_path, const char *elf_path) {
    FusionRom rom = {0};
    uint8_t *elf = NULL;
    size_t elf_size = 0;
    char error[256];
    if (!fusion_rom_open(FUSION_ASSET_GAME_METROID, rom_path, &rom,
                         error, sizeof error) ||
        !read_file(elf_path, &elf, &elf_size, error, sizeof error)) {
        fprintf(stderr, "mzm-rooms: %s\n", error);
        fusion_rom_close(&rom);
        free(elf);
        return 1;
    }
    FusionMzmRomSymbolTable symbols = {0};
    FusionMzmRoomDirectory directory;
    if (!fusion_mzm_read_rom_symbols(&rom, elf, elf_size, &symbols,
                                     error, sizeof error) ||
        !fusion_mzm_read_room_directory(&rom, &symbols, &directory,
                                        error, sizeof error)) {
        fprintf(stderr, "mzm-rooms: %s\n", error);
        fusion_mzm_free_symbols(&symbols);
        fusion_rom_close(&rom);
        free(elf);
        return 1;
    }
    for (size_t area = 0; area < FUSION_MZM_ROOM_AREA_COUNT; ++area) {
        const FusionMzmRoomAreaSpec *spec = fusion_mzm_room_area_spec(area);
        printf("area\t%zu\t%s\t%zu\t%u\n", area, spec->id,
               directory.areas[area].room_count,
               directory.areas[area].table_pointer);
        for (size_t room = 0; room < directory.areas[area].room_count; ++room) {
            FusionMzmRoomEntry entry;
            if (!fusion_mzm_read_room_entry(&rom, &directory, area, room,
                                            &entry, error, sizeof error)) {
                fprintf(stderr, "mzm-rooms: %s %zu: %s\n", spec->id, room, error);
                fusion_mzm_free_symbols(&symbols);
                fusion_rom_close(&rom);
                free(elf);
                return 1;
            }
            printf("room\t%s\t%zu\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\n",
                   spec->id, room, entry.entry_pointer, entry.tileset,
                   entry.bg0_property, entry.bg1_property, entry.bg2_property,
                   entry.bg3_property, entry.bg0_pointer, entry.bg1_pointer,
                   entry.bg2_pointer, entry.clip_pointer, entry.bg3_pointer,
                   entry.bg3_scrolling, entry.transparency,
                   entry.default_sprite_pointer, entry.default_spriteset,
                   entry.first_spriteset_event, entry.first_sprite_pointer,
                   entry.first_spriteset, entry.second_spriteset_event,
                   entry.second_sprite_pointer, entry.second_spriteset,
                   entry.map_x, entry.map_y, entry.effect, entry.effect_y,
                   entry.music_track);
        }
    }
    printf("summary\t%zu\n", directory.total_rooms);
    fusion_mzm_free_symbols(&symbols);
    fusion_rom_close(&rom);
    free(elf);
    return 0;
}

static int inspect_mzm_clipdata(const char *rom_path, const char *elf_path) {
    FusionRom rom = {0};
    uint8_t *elf = NULL;
    size_t elf_size = 0;
    char error[256];
    if (!fusion_rom_open(FUSION_ASSET_GAME_METROID, rom_path, &rom,
                         error, sizeof error) ||
        !read_file(elf_path, &elf, &elf_size, error, sizeof error)) {
        fprintf(stderr, "mzm-clipdata: %s\n", error);
        fusion_rom_close(&rom);
        free(elf);
        return 1;
    }
    FusionMzmRomSymbolTable symbols = {0};
    FusionMzmRoomDirectory directory;
    if (!fusion_mzm_read_rom_symbols(&rom, elf, elf_size, &symbols,
                                     error, sizeof error) ||
        !fusion_mzm_read_room_directory(&rom, &symbols, &directory,
                                        error, sizeof error)) {
        fprintf(stderr, "mzm-clipdata: %s\n", error);
        fusion_mzm_free_symbols(&symbols);
        fusion_rom_close(&rom);
        free(elf);
        return 1;
    }
    size_t room_count = 0;
    size_t cell_count = 0;
    uint8_t canonical[FUSION_MZM_ROOM_RLE_CELL_CAPACITY * 2];
    for (size_t area = 0; area < FUSION_MZM_ROOM_AREA_COUNT; ++area) {
        const FusionMzmRoomAreaSpec *spec = fusion_mzm_room_area_spec(area);
        for (size_t room = 0; room < directory.areas[area].room_count; ++room) {
            FusionMzmRoomEntry entry;
            FusionMzmRoomRleMap map;
            if (!fusion_mzm_read_room_entry(&rom, &directory, area, room,
                                            &entry, error, sizeof error) ||
                !fusion_mzm_read_room_rle(&rom, entry.clip_pointer, &map,
                                          error, sizeof error)) {
                fprintf(stderr, "mzm-clipdata: %s %zu: %s\n",
                        spec->id, room, error);
                fusion_mzm_free_symbols(&symbols);
                fusion_rom_close(&rom);
                free(elf);
                return 1;
            }
            for (size_t index = 0; index < map.cell_count; ++index) {
                canonical[index * 2] = (uint8_t)map.cells[index];
                canonical[index * 2 + 1] = (uint8_t)(map.cells[index] >> 8);
            }
            char digest[41];
            if (!fusion_rom_sha1_hex(canonical, map.cell_count * 2, digest)) {
                fprintf(stderr, "mzm-clipdata: cannot hash %s %zu\n",
                        spec->id, room);
                fusion_mzm_free_room_rle(&map);
                fusion_mzm_free_symbols(&symbols);
                fusion_rom_close(&rom);
                free(elf);
                return 1;
            }
            printf("clipdata\t%s\t%zu\t0x%08X\t%u\t%u\t%zu\t%zu\t%s\n",
                   spec->id, room, map.source_pointer, map.width, map.height,
                   map.cell_count, map.encoded_size, digest);
            ++room_count;
            cell_count += map.cell_count;
            fusion_mzm_free_room_rle(&map);
        }
    }
    printf("summary\t%zu\t%zu\n", room_count, cell_count);
    fusion_mzm_free_symbols(&symbols);
    fusion_rom_close(&rom);
    free(elf);
    return 0;
}

static int inspect_mzm_runtime_rooms(const char *rom_path,
                                     const char *elf_path,
                                     bool include_doors,
                                     bool include_hatches,
                                     bool include_metadata,
                                     bool include_sprites) {
    const char *label = include_sprites ? "mzm-runtime-room-sprites" :
        include_metadata ? "mzm-runtime-room-metadata" :
        include_hatches ? "mzm-runtime-room-hatches" :
        include_doors ? "mzm-runtime-room-doors" :
        "mzm-runtime-room-cores";
    FusionRom rom = {0};
    uint8_t *elf = NULL;
    size_t elf_size = 0;
    char error[256];
    if (!fusion_rom_open(FUSION_ASSET_GAME_METROID, rom_path, &rom,
                         error, sizeof error) ||
        !read_file(elf_path, &elf, &elf_size, error, sizeof error)) {
        fprintf(stderr, "%s: %s\n", label, error);
        fusion_rom_close(&rom);
        free(elf);
        return 1;
    }
    FusionMzmRomSymbolTable symbols = {0};
    FusionMzmRoomDirectory directory;
    FusionMzmClipdataTables tables;
    FusionMzmDoorDirectory doors = {0};
    FusionMzmHatchTables hatches = {0};
    static FusionMzmSpritesetTable spritesets;
    if (!fusion_mzm_read_rom_symbols(&rom, elf, elf_size, &symbols,
                                     error, sizeof error) ||
        !fusion_mzm_read_room_directory(&rom, &symbols, &directory,
                                        error, sizeof error) ||
        !fusion_mzm_read_clipdata_tables(&rom, &symbols, &tables,
                                         error, sizeof error) ||
        (include_doors &&
         !fusion_mzm_read_door_directory(&rom, &symbols, &doors,
                                         error, sizeof error)) ||
        (include_hatches &&
         !fusion_mzm_read_hatch_tables(&rom, &symbols, &hatches,
                                       error, sizeof error)) ||
        (include_sprites &&
         !fusion_mzm_read_spritesets(&rom, &symbols, &spritesets,
                                     error, sizeof error))) {
        fprintf(stderr, "%s: %s\n", label, error);
        fusion_mzm_free_hatch_tables(&hatches);
        fusion_mzm_free_door_directory(&doors);
        fusion_mzm_free_symbols(&symbols);
        fusion_rom_close(&rom);
        free(elf);
        return 1;
    }
    size_t room_count = 0, total_cells = 0, total_clipdata = 0;
    size_t total_transitions = 0, total_movements = 0, total_bytes = 0;
    size_t total_doors = 0, total_exits = 0, total_locations = 0;
    size_t total_hatches = 0, total_hatch_locks = 0;
    size_t total_music = 0, total_background_layers = 0;
    size_t total_background_rows = 0, total_text_backgrounds = 0;
    size_t skipped_rooms = 0;
    size_t total_sprites = 0, total_spriteset_members = 0;
    size_t total_sprite_variants = 0, total_variant_sprites = 0;
    size_t total_variant_members = 0;
    for (size_t area = 0; area < FUSION_MZM_ROOM_AREA_COUNT; ++area) {
        const FusionMzmRoomAreaSpec *spec = fusion_mzm_room_area_spec(area);
        for (size_t room = 0; room < directory.areas[area].room_count; ++room) {
            FusionMzmRoomEntry entry;
            FusionMzmRoomRleMap clipdata;
            FusionMzmRoomBackgrounds backgrounds = {0};
            if (!fusion_mzm_read_room_entry(&rom, &directory, area, room,
                                            &entry, error, sizeof error) ||
                !fusion_mzm_read_room_rle(&rom, entry.clip_pointer, &clipdata,
                                          error, sizeof error)) {
                fprintf(stderr, "%s: %s %zu: %s\n",
                        label, spec->id, room, error);
                fusion_mzm_free_hatch_tables(&hatches);
                fusion_mzm_free_door_directory(&doors);
                fusion_mzm_free_symbols(&symbols);
                fusion_rom_close(&rom);
                free(elf);
                return 1;
            }
            if (include_metadata &&
                entry.bg1_property != FUSION_MZM_BG_PROPERTY_RLE_COMPRESSED) {
                ++skipped_rooms;
                fusion_mzm_free_room_rle(&clipdata);
                continue;
            }
            if (include_metadata &&
                !fusion_mzm_read_room_backgrounds(
                    &rom, &entry, &backgrounds, error, sizeof error)) {
                fprintf(stderr, "%s: %s %zu backgrounds: %s\n",
                        label, spec->id, room, error);
                fusion_mzm_free_room_rle(&clipdata);
                fusion_mzm_free_hatch_tables(&hatches);
                fusion_mzm_free_door_directory(&doors);
                fusion_mzm_free_symbols(&symbols);
                fusion_rom_close(&rom);
                free(elf);
                return 1;
            }
            static FusionMzmRoomSprites sprites;
            if (include_sprites &&
                !fusion_mzm_read_room_sprites(&rom, &symbols, &spritesets,
                                              &entry, &sprites,
                                              error, sizeof error)) {
                fprintf(stderr, "%s: %s %zu sprites: %s\n",
                        label, spec->id, room, error);
                fusion_mzm_free_room_rle(&clipdata);
                fusion_mzm_free_room_backgrounds(&backgrounds);
                fusion_mzm_free_hatch_tables(&hatches);
                fusion_mzm_free_door_directory(&doors);
                fusion_mzm_free_symbols(&symbols);
                fusion_rom_close(&rom);
                free(elf);
                return 1;
            }
            FusionMzmRuntimeRoomPayload payload;
            bool serialized = include_sprites
                ? fusion_mzm_serialize_runtime_room_sprites(
                      area, room, &clipdata, &tables, &doors, &hatches,
                      &entry, &backgrounds, &spritesets, &sprites, &payload,
                      error, sizeof error)
                : include_metadata
                ? fusion_mzm_serialize_runtime_room_metadata(
                      area, room, &clipdata, &tables, &doors, &hatches,
                      &entry, &backgrounds, &payload, error, sizeof error)
                : include_hatches
                ? fusion_mzm_serialize_runtime_room_hatches(
                      area, room, &clipdata, &tables, &doors, &hatches,
                      &payload, error, sizeof error)
                : include_doors
                ? fusion_mzm_serialize_runtime_room_doors(
                      area, room, &clipdata, &tables, &doors, &payload,
                      error, sizeof error)
                : fusion_mzm_serialize_runtime_room_core(
                      spec->id, room, &clipdata, &tables, &payload,
                      error, sizeof error);
            if (!serialized) {
                fprintf(stderr,
                        "%s: cannot serialize %s %zu: %s\n",
                        label, spec->id, room, error);
                fusion_mzm_free_room_rle(&clipdata);
                fusion_mzm_free_room_backgrounds(&backgrounds);
                fusion_mzm_free_hatch_tables(&hatches);
                fusion_mzm_free_door_directory(&doors);
                fusion_mzm_free_symbols(&symbols);
                fusion_rom_close(&rom);
                free(elf);
                return 1;
            }
            char digest[41];
            if (!fusion_rom_sha1_hex(payload.data, payload.size, digest)) {
                fprintf(stderr,
                        "%s: cannot hash %s %zu\n", label, spec->id, room);
                fusion_mzm_free_runtime_room_payload(&payload);
                fusion_mzm_free_room_rle(&clipdata);
                fusion_mzm_free_room_backgrounds(&backgrounds);
                fusion_mzm_free_hatch_tables(&hatches);
                fusion_mzm_free_door_directory(&doors);
                fusion_mzm_free_symbols(&symbols);
                fusion_rom_close(&rom);
                free(elf);
                return 1;
            }
            if (include_sprites) {
                printf("room\t%s\t%zu\t%u\t%u\t%zu\t%zu\t%zu\t%zu\t"
                       "%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t"
                       "%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%s\n",
                       spec->id, room, (unsigned)clipdata.width,
                       (unsigned)clipdata.height, clipdata.cell_count,
                       payload.clipdata_count, payload.transition_count,
                       payload.movement_count, payload.door_count,
                       payload.exit_count, payload.location_count,
                       payload.hatch_count, payload.hatch_lock_count,
                       payload.music_count, payload.background_layer_count,
                       payload.background_row_count,
                       payload.text_background_count, payload.sprite_count,
                       payload.spriteset_member_count,
                       payload.sprite_variant_count,
                       payload.variant_sprite_count,
                       payload.variant_member_count, payload.size, digest);
            } else if (include_metadata) {
                printf("room\t%s\t%zu\t%u\t%u\t%zu\t%zu\t%zu\t%zu\t"
                       "%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t"
                       "%zu\t%s\n",
                       spec->id, room, (unsigned)clipdata.width,
                       (unsigned)clipdata.height, clipdata.cell_count,
                       payload.clipdata_count, payload.transition_count,
                       payload.movement_count, payload.door_count,
                       payload.exit_count, payload.location_count,
                       payload.hatch_count, payload.hatch_lock_count,
                       payload.music_count, payload.background_layer_count,
                       payload.background_row_count,
                       payload.text_background_count, payload.size, digest);
            } else if (include_hatches) {
                printf("room\t%s\t%zu\t%u\t%u\t%zu\t%zu\t%zu\t%zu\t"
                       "%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%s\n",
                       spec->id, room, (unsigned)clipdata.width,
                       (unsigned)clipdata.height, clipdata.cell_count,
                       payload.clipdata_count, payload.transition_count,
                       payload.movement_count, payload.door_count,
                       payload.exit_count, payload.location_count,
                       payload.hatch_count, payload.hatch_lock_count,
                       payload.size, digest);
            } else if (include_doors) {
                printf("room\t%s\t%zu\t%u\t%u\t%zu\t%zu\t%zu\t%zu\t"
                       "%zu\t%zu\t%zu\t%zu\t%s\n",
                       spec->id, room, (unsigned)clipdata.width,
                       (unsigned)clipdata.height, clipdata.cell_count,
                       payload.clipdata_count, payload.transition_count,
                       payload.movement_count, payload.door_count,
                       payload.exit_count, payload.location_count,
                       payload.size, digest);
            } else {
                printf("room\t%s\t%zu\t%u\t%u\t%zu\t%zu\t%zu\t%zu\t%zu\t%s\n",
                       spec->id, room, (unsigned)clipdata.width,
                       (unsigned)clipdata.height, clipdata.cell_count,
                       payload.clipdata_count, payload.transition_count,
                       payload.movement_count, payload.size, digest);
            }
            ++room_count;
            total_cells += clipdata.cell_count;
            total_clipdata += payload.clipdata_count;
            total_transitions += payload.transition_count;
            total_movements += payload.movement_count;
            total_doors += payload.door_count;
            total_exits += payload.exit_count;
            total_locations += payload.location_count;
            total_hatches += payload.hatch_count;
            total_hatch_locks += payload.hatch_lock_count;
            total_music += payload.music_count;
            total_background_layers += payload.background_layer_count;
            total_background_rows += payload.background_row_count;
            total_text_backgrounds += payload.text_background_count;
            total_sprites += payload.sprite_count;
            total_spriteset_members += payload.spriteset_member_count;
            total_sprite_variants += payload.sprite_variant_count;
            total_variant_sprites += payload.variant_sprite_count;
            total_variant_members += payload.variant_member_count;
            total_bytes += payload.size;
            fusion_mzm_free_runtime_room_payload(&payload);
            fusion_mzm_free_room_rle(&clipdata);
            fusion_mzm_free_room_backgrounds(&backgrounds);
        }
    }
    if (include_sprites) {
        printf("summary\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t"
               "%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t"
               "%zu\n",
               room_count, skipped_rooms, total_cells, total_clipdata,
               total_transitions, total_movements, total_doors, total_exits,
               total_locations, total_hatches, total_hatch_locks, total_music,
               total_background_layers, total_background_rows,
               total_text_backgrounds, total_sprites, total_spriteset_members,
               total_sprite_variants, total_variant_sprites,
               total_variant_members, total_bytes);
    } else if (include_metadata) {
        printf("summary\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t"
               "%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\n",
               room_count, skipped_rooms, total_cells, total_clipdata,
               total_transitions, total_movements, total_doors, total_exits,
               total_locations, total_hatches, total_hatch_locks, total_music,
               total_background_layers, total_background_rows,
               total_text_backgrounds, total_bytes);
    } else if (include_hatches) {
        printf("summary\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t"
               "%zu\t%zu\t%zu\n",
               room_count, total_cells, total_clipdata, total_transitions,
               total_movements, total_doors, total_exits, total_locations,
               total_hatches, total_hatch_locks, total_bytes);
    } else if (include_doors) {
        printf("summary\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\n",
               room_count, total_cells, total_clipdata, total_transitions,
               total_movements, total_doors, total_exits, total_locations,
               total_bytes);
    } else {
        printf("summary\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\n",
               room_count, total_cells, total_clipdata, total_transitions,
               total_movements, total_bytes);
    }
    fusion_mzm_free_hatch_tables(&hatches);
    fusion_mzm_free_door_directory(&doors);
    fusion_mzm_free_symbols(&symbols);
    fusion_rom_close(&rom);
    free(elf);
    return 0;
}

static const char *mzm_background_status_name(FusionMzmBackgroundStatus status) {
    switch (status) {
        case FUSION_MZM_BACKGROUND_NONE: return "none";
        case FUSION_MZM_BACKGROUND_RLE: return "rle";
        case FUSION_MZM_BACKGROUND_TEXT: return "text";
        case FUSION_MZM_BACKGROUND_UNSUPPORTED: return "unsupported";
    }
    return "invalid";
}

static int inspect_mzm_backgrounds(const char *rom_path, const char *elf_path) {
    FusionRom rom = {0};
    uint8_t *elf = NULL;
    size_t elf_size = 0;
    char error[256];
    if (!fusion_rom_open(FUSION_ASSET_GAME_METROID, rom_path, &rom,
                         error, sizeof error) ||
        !read_file(elf_path, &elf, &elf_size, error, sizeof error)) {
        fprintf(stderr, "mzm-backgrounds: %s\n", error);
        fusion_rom_close(&rom);
        free(elf);
        return 1;
    }
    FusionMzmRomSymbolTable symbols = {0};
    FusionMzmRoomDirectory directory;
    if (!fusion_mzm_read_rom_symbols(&rom, elf, elf_size, &symbols,
                                     error, sizeof error) ||
        !fusion_mzm_read_room_directory(&rom, &symbols, &directory,
                                        error, sizeof error)) {
        fprintf(stderr, "mzm-backgrounds: %s\n", error);
        fusion_mzm_free_symbols(&symbols);
        fusion_rom_close(&rom);
        free(elf);
        return 1;
    }
    size_t status_counts[4] = {0};
    size_t room_count = 0, total_cells = 0;
    uint8_t canonical[FUSION_MZM_ROOM_RLE_CELL_CAPACITY * 2];
    for (size_t area = 0; area < FUSION_MZM_ROOM_AREA_COUNT; ++area) {
        const FusionMzmRoomAreaSpec *spec = fusion_mzm_room_area_spec(area);
        for (size_t room = 0; room < directory.areas[area].room_count; ++room) {
            FusionMzmRoomEntry entry;
            FusionMzmRoomBackgrounds backgrounds = {0};
            if (!fusion_mzm_read_room_entry(&rom, &directory, area, room,
                                            &entry, error, sizeof error) ||
                !fusion_mzm_read_room_backgrounds(&rom, &entry, &backgrounds,
                                                  error, sizeof error)) {
                fprintf(stderr, "mzm-backgrounds: %s %zu: %s\n",
                        spec->id, room, error);
                fusion_mzm_free_symbols(&symbols);
                fusion_rom_close(&rom);
                free(elf);
                return 1;
            }
            for (size_t layer = 0; layer < FUSION_MZM_ROOM_BACKGROUND_COUNT;
                 ++layer) {
                const FusionMzmRoomBackground *background =
                    &backgrounds.layers[layer];
                const uint16_t *cells = NULL;
                size_t cell_count = 0, encoded_size = 0;
                unsigned width = 0, height = 0;
                int size_flag = -1;
                if (background->status == FUSION_MZM_BACKGROUND_RLE) {
                    cells = background->rle.cells;
                    cell_count = background->rle.cell_count;
                    encoded_size = background->rle.encoded_size;
                    width = background->rle.width;
                    height = background->rle.height;
                } else if (background->status == FUSION_MZM_BACKGROUND_TEXT) {
                    cells = background->text.cells;
                    cell_count = background->text.cell_count;
                    width = background->text.width_tiles;
                    height = background->text.height_tiles;
                    size_flag = background->text.size_flag;
                }
                char digest[41] = "-";
                if (cells) {
                    for (size_t index = 0; index < cell_count; ++index) {
                        canonical[index * 2] = (uint8_t)cells[index];
                        canonical[index * 2 + 1] = (uint8_t)(cells[index] >> 8);
                    }
                    if (!fusion_rom_sha1_hex(canonical, cell_count * 2, digest)) {
                        fprintf(stderr,
                                "mzm-backgrounds: cannot hash %s %zu BG%zu\n",
                                spec->id, room, layer);
                        fusion_mzm_free_room_backgrounds(&backgrounds);
                        fusion_mzm_free_symbols(&symbols);
                        fusion_rom_close(&rom);
                        free(elf);
                        return 1;
                    }
                }
                ++status_counts[background->status];
                total_cells += cell_count;
                printf("background\t%s\t%zu\t%zu\t%s\t%u\t0x%08X\t%d\t"
                       "%u\t%u\t%zu\t%zu\t%s\n",
                       spec->id, room, layer,
                       mzm_background_status_name(background->status),
                       background->property, background->source_pointer,
                       size_flag, width, height, cell_count, encoded_size, digest);
            }
            ++room_count;
            fusion_mzm_free_room_backgrounds(&backgrounds);
        }
    }
    printf("summary\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\n",
           room_count, room_count * FUSION_MZM_ROOM_BACKGROUND_COUNT,
           status_counts[FUSION_MZM_BACKGROUND_RLE],
           status_counts[FUSION_MZM_BACKGROUND_TEXT],
           status_counts[FUSION_MZM_BACKGROUND_NONE],
           status_counts[FUSION_MZM_BACKGROUND_UNSUPPORTED], total_cells);
    fusion_mzm_free_symbols(&symbols);
    fusion_rom_close(&rom);
    free(elf);
    return 0;
}

static int inspect_mzm_tilesets(const char *rom_path, const char *elf_path) {
    FusionRom rom = {0};
    uint8_t *elf = NULL;
    size_t elf_size = 0;
    char error[256];
    if (!fusion_rom_open(FUSION_ASSET_GAME_METROID, rom_path, &rom,
                         error, sizeof error) ||
        !read_file(elf_path, &elf, &elf_size, error, sizeof error)) {
        fprintf(stderr, "mzm-tilesets: %s\n", error);
        fusion_rom_close(&rom);
        free(elf);
        return 1;
    }
    FusionMzmRomSymbolTable symbols = {0};
    if (!fusion_mzm_read_rom_symbols(&rom, elf, elf_size, &symbols,
                                     error, sizeof error)) {
        fprintf(stderr, "mzm-tilesets: %s\n", error);
        fusion_rom_close(&rom);
        free(elf);
        return 1;
    }
    FusionMzmVideoMemory *memory = malloc(sizeof *memory);
    if (!memory) {
        fprintf(stderr, "mzm-tilesets: cannot allocate video memory\n");
        fusion_mzm_free_symbols(&symbols);
        fusion_rom_close(&rom);
        free(elf);
        return 1;
    }
    size_t total_graphics = 0, total_background_graphics = 0;
    for (size_t index = 0; index < FUSION_MZM_TILESET_COUNT; ++index) {
        if (!fusion_mzm_load_tileset_video_memory(
                &rom, &symbols, index, memory, error, sizeof error)) {
            fprintf(stderr, "mzm-tilesets: tileset %zu: %s\n", index, error);
            free(memory);
            fusion_mzm_free_symbols(&symbols);
            fusion_rom_close(&rom);
            free(elf);
            return 1;
        }
        char vram_digest[41], palette_digest[41];
        if (!fusion_rom_sha1_hex(memory->vram, sizeof memory->vram,
                                 vram_digest) ||
            !fusion_rom_sha1_hex(memory->palette, sizeof memory->palette,
                                 palette_digest)) {
            fprintf(stderr, "mzm-tilesets: cannot hash tileset %zu\n", index);
            free(memory);
            fusion_mzm_free_symbols(&symbols);
            fusion_rom_close(&rom);
            free(elf);
            return 1;
        }
        printf("tileset\t%zu\t%u\t%u\t%u\t%u\t%u\t%u\t%zu\t%zu\t%zu\t%s\t%s\n",
               index, memory->entry.graphics_pointer,
               memory->entry.palette_pointer,
               memory->entry.background_graphics_pointer,
               memory->entry.tilemap_pointer,
               memory->entry.animated_tileset,
               memory->entry.animated_palette,
               memory->common_graphics_size, memory->graphics_size,
               memory->background_graphics_size,
               vram_digest, palette_digest);
        total_graphics += memory->graphics_size;
        total_background_graphics += memory->background_graphics_size;
    }
    printf("summary\t%u\t%zu\t%zu\n", FUSION_MZM_TILESET_COUNT,
           total_graphics, total_background_graphics);
    free(memory);
    fusion_mzm_free_symbols(&symbols);
    fusion_rom_close(&rom);
    free(elf);
    return 0;
}

/* Prints one hashed BMP32 per tileset metatile sheet, then per text layer
 * (BG0/BG3) of each room with an RLE BG1, as the runtime-room export
 * writes them. */
static bool print_bitmap(const char *prefix,
                         const FusionMzmRenderedBackground *rendered,
                         size_t *total_bytes, char *error, size_t error_size) {
    FusionMzmBitmap bitmap;
    if (!fusion_mzm_serialize_bmp32(rendered, &bitmap, error, error_size))
        return false;
    char digest[41];
    if (!fusion_rom_sha1_hex(bitmap.data, bitmap.size, digest)) {
        fusion_mzm_free_bitmap(&bitmap);
        snprintf(error, error_size, "cannot hash bitmap");
        return false;
    }
    printf("%s\t%zu\t%zu\t%zu\t%s\n", prefix, rendered->width_pixels,
           rendered->height_pixels, bitmap.size, digest);
    *total_bytes += bitmap.size;
    fusion_mzm_free_bitmap(&bitmap);
    return true;
}

static int inspect_mzm_bitmaps(const char *rom_path, const char *elf_path) {
    FusionRom rom = {0};
    uint8_t *elf = NULL;
    size_t elf_size = 0;
    char error[256];
    FusionMzmRomSymbolTable symbols = {0};
    FusionMzmRoomDirectory directory;
    FusionMzmVideoMemory *memory = malloc(sizeof *memory);
    FusionMzmMetatileTable *metatiles = malloc(sizeof *metatiles);
    int status = 1;
    size_t sheets = 0, texts = 0, total_bytes = 0;
    if (!memory || !metatiles) {
        snprintf(error, sizeof error, "cannot allocate tileset buffers");
        goto fail;
    }
    if (!fusion_rom_open(FUSION_ASSET_GAME_METROID, rom_path, &rom,
                         error, sizeof error) ||
        !read_file(elf_path, &elf, &elf_size, error, sizeof error) ||
        !fusion_mzm_read_rom_symbols(&rom, elf, elf_size, &symbols,
                                     error, sizeof error) ||
        !fusion_mzm_read_room_directory(&rom, &symbols, &directory,
                                        error, sizeof error)) {
        goto fail;
    }
    for (size_t index = 0; index < FUSION_MZM_TILESET_COUNT; ++index) {
        FusionMzmRenderedBackground sheet;
        char prefix[32];
        snprintf(prefix, sizeof prefix, "sheet\t%zu", index);
        if (!fusion_mzm_load_tileset_video_memory(&rom, &symbols, index, memory,
                                                  error, sizeof error) ||
            !fusion_mzm_read_tileset_metatiles(&rom, &symbols, index, metatiles,
                                               error, sizeof error) ||
            !fusion_mzm_render_metatile_sheet(metatiles, memory, &sheet,
                                              error, sizeof error)) {
            goto fail;
        }
        bool printed = print_bitmap(prefix, &sheet, &total_bytes,
                                    error, sizeof error);
        fusion_mzm_free_rendered_background(&sheet);
        if (!printed) goto fail;
        ++sheets;
    }
    for (size_t area = 0; area < FUSION_MZM_ROOM_AREA_COUNT; ++area) {
        const FusionMzmRoomAreaSpec *spec = fusion_mzm_room_area_spec(area);
        for (size_t room = 0; room < directory.areas[area].room_count; ++room) {
            FusionMzmRoomEntry entry;
            FusionMzmRoomBackgrounds backgrounds = {0};
            if (!fusion_mzm_read_room_entry(&rom, &directory, area, room,
                                            &entry, error, sizeof error))
                goto fail;
            if (entry.bg1_property != FUSION_MZM_BG_PROPERTY_RLE_COMPRESSED)
                continue;
            if (!fusion_mzm_read_room_backgrounds(&rom, &entry, &backgrounds,
                                                  error, sizeof error) ||
                !fusion_mzm_load_tileset_video_memory(
                    &rom, &symbols, entry.tileset, memory,
                    error, sizeof error)) {
                fusion_mzm_free_room_backgrounds(&backgrounds);
                goto fail;
            }
            for (size_t layer = 0; layer < FUSION_MZM_ROOM_BACKGROUND_COUNT;
                 ++layer) {
                if ((layer != 0 && layer != 3) ||
                    backgrounds.layers[layer].status !=
                        FUSION_MZM_BACKGROUND_TEXT) {
                    continue;
                }
                FusionMzmRenderedBackground text;
                char prefix[48];
                snprintf(prefix, sizeof prefix, "text\t%s\t%zu\t%zu",
                         spec->id, room, layer);
                if (!fusion_mzm_render_text_background(
                        &backgrounds.layers[layer].text, memory, &text,
                        error, sizeof error)) {
                    fusion_mzm_free_room_backgrounds(&backgrounds);
                    goto fail;
                }
                bool printed = print_bitmap(prefix, &text, &total_bytes,
                                            error, sizeof error);
                fusion_mzm_free_rendered_background(&text);
                if (!printed) {
                    fusion_mzm_free_room_backgrounds(&backgrounds);
                    goto fail;
                }
                ++texts;
            }
            fusion_mzm_free_room_backgrounds(&backgrounds);
        }
    }
    printf("summary\t%zu\t%zu\t%zu\n", sheets, texts, total_bytes);
    status = 0;
fail:
    if (status) fprintf(stderr, "mzm-bitmaps: %s\n", error);
    free(memory);
    free(metatiles);
    fusion_mzm_free_symbols(&symbols);
    fusion_rom_close(&rom);
    free(elf);
    return status;
}

/* Prints the size and SHA-1 of the native blocks.tsv and power_bomb.tsv;
 * with DUMP set, writes their bytes to stdout instead. */
static int inspect_mzm_runtime_tables(const char *rom_path,
                                      const char *elf_path,
                                      const char *dump) {
    FusionRom rom = {0};
    uint8_t *elf = NULL;
    size_t elf_size = 0;
    char error[256];
    FusionMzmRomSymbolTable symbols = {0};
    FusionMzmClipdataTables tables;
    FusionMzmTextFile blocks = {0}, windows = {0};
    int status = 1;
    if (!fusion_rom_open(FUSION_ASSET_GAME_METROID, rom_path, &rom,
                         error, sizeof error) ||
        !read_file(elf_path, &elf, &elf_size, error, sizeof error) ||
        !fusion_mzm_read_rom_symbols(&rom, elf, elf_size, &symbols,
                                     error, sizeof error) ||
        !fusion_mzm_read_clipdata_tables(&rom, &symbols, &tables,
                                         error, sizeof error) ||
        !fusion_mzm_serialize_block_tables(&rom, &symbols, &tables, &blocks,
                                           error, sizeof error) ||
        !fusion_mzm_serialize_power_bomb_windows(&rom, &symbols, &windows,
                                                 error, sizeof error)) {
        fprintf(stderr, "mzm-runtime-tables: %s\n", error);
        goto done;
    }
    const struct {
        const char *name;
        const FusionMzmTextFile *file;
    } files[] = {{"blocks.tsv", &blocks}, {"power_bomb.tsv", &windows}};
    for (size_t index = 0; index < 2; ++index) {
        if (dump) {
            if (!strcmp(dump, files[index].name))
                fwrite(files[index].file->data, 1, files[index].file->size,
                       stdout);
            continue;
        }
        char digest[41];
        if (!fusion_rom_sha1_hex(files[index].file->data,
                                 files[index].file->size, digest)) {
            fprintf(stderr, "mzm-runtime-tables: cannot hash %s\n",
                    files[index].name);
            goto done;
        }
        printf("table\t%s\t%zu\t%s\n", files[index].name,
               files[index].file->size, digest);
    }
    status = 0;
done:
    fusion_mzm_free_text_file(&blocks);
    fusion_mzm_free_text_file(&windows);
    fusion_mzm_free_symbols(&symbols);
    fusion_rom_close(&rom);
    free(elf);
    return status;
}

/* Prints each exportable room's background.bmp (the older diagnostic
 * render the runtime export writes): size, coverage and SHA-1. */
static int inspect_mzm_export_backgrounds(const char *rom_path,
                                          const char *elf_path) {
    FusionRom rom = {0};
    uint8_t *elf = NULL;
    size_t elf_size = 0;
    char error[256] = "";
    FusionMzmRomSymbolTable symbols = {0};
    FusionMzmRoomDirectory directory;
    int status = 1;
    size_t rooms = 0, skipped = 0, total_bytes = 0;
    if (!fusion_rom_open(FUSION_ASSET_GAME_METROID, rom_path, &rom,
                         error, sizeof error) ||
        !read_file(elf_path, &elf, &elf_size, error, sizeof error) ||
        !fusion_mzm_read_rom_symbols(&rom, elf, elf_size, &symbols,
                                     error, sizeof error) ||
        !fusion_mzm_read_room_directory(&rom, &symbols, &directory,
                                        error, sizeof error)) {
        goto done;
    }
    for (size_t area = 0; area < FUSION_MZM_ROOM_AREA_COUNT; ++area) {
        const FusionMzmRoomAreaSpec *spec = fusion_mzm_room_area_spec(area);
        for (size_t room = 0; room < directory.areas[area].room_count; ++room) {
            FusionMzmRoomEntry entry;
            FusionMzmRoomBackgrounds backgrounds = {0};
            FusionMzmCompositeBackground composite;
            FusionMzmSerializedBackground serialized;
            bool no_base = false;
            if (!fusion_mzm_read_room_entry(&rom, &directory, area, room,
                                            &entry, error, sizeof error))
                goto done;
            if (entry.bg1_property != FUSION_MZM_BG_PROPERTY_RLE_COMPRESSED) {
                ++skipped;
                continue;
            }
            error[0] = '\0';
            if (!fusion_mzm_read_room_backgrounds(&rom, &entry, &backgrounds,
                                                  error, sizeof error) ||
                !fusion_mzm_render_export_background(
                    &rom, &symbols, &entry, &backgrounds, &composite,
                    &no_base, error, sizeof error)) {
                fusion_mzm_free_room_backgrounds(&backgrounds);
                if (no_base) {
                    printf("skipped\t%s\t%zu\t%s\n", spec->id, room, error);
                    ++skipped;
                    continue;
                }
                goto done;
            }
            fusion_mzm_free_room_backgrounds(&backgrounds);
            bool serialized_ok = fusion_mzm_serialize_background(
                &composite, &serialized, error, sizeof error);
            char digest[41] = "";
            bool hashed = serialized_ok &&
                fusion_rom_sha1_hex(serialized.bitmap, serialized.bitmap_size,
                                    digest);
            if (hashed) {
                printf("background\t%s\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%s\n",
                       spec->id, room, composite.width_pixels,
                       composite.height_pixels, composite.bg1_visible_count,
                       composite.bg2_visible_count, composite.unresolved_count,
                       serialized.bitmap_size, digest);
                total_bytes += serialized.bitmap_size;
                ++rooms;
            }
            if (serialized_ok) fusion_mzm_free_serialized_background(&serialized);
            fusion_mzm_free_composite_background(&composite);
            if (!hashed) {
                if (!error[0]) snprintf(error, sizeof error, "cannot hash background");
                goto done;
            }
        }
    }
    printf("summary\t%zu\t%zu\t%zu\n", rooms, skipped, total_bytes);
    status = 0;
done:
    if (status) fprintf(stderr, "mzm-export-backgrounds: %s\n", error);
    fusion_mzm_free_symbols(&symbols);
    fusion_rom_close(&rom);
    free(elf);
    return status;
}

static int export_mzm_runtime_rooms(const char *rom_path, const char *elf_path,
                                    const char *output) {
    FusionRom rom = {0};
    uint8_t *elf = NULL;
    size_t elf_size = 0;
    char error[512];
    FusionMzmExportSummary summary;
    if (!fusion_rom_open(FUSION_ASSET_GAME_METROID, rom_path, &rom,
                         error, sizeof error) ||
        !read_file(elf_path, &elf, &elf_size, error, sizeof error) ||
        !fusion_mzm_export_runtime_rooms(&rom, elf, elf_size, output, &summary,
                                         error, sizeof error)) {
        fprintf(stderr, "mzm-export-rooms: %s\n", error);
        fusion_rom_close(&rom);
        free(elf);
        return 1;
    }
    printf("Zero Mission runtime rooms exported: %zu, skipped: %zu\n",
           summary.room_count, summary.skipped_count);
    for (size_t index = 0; index < summary.skipped_count; ++index)
        printf("  skipped %s\n", summary.skipped[index]);
    fusion_rom_close(&rom);
    free(elf);
    return 0;
}

static int export_mzm_samus(const char *rom_path, const char *elf_path,
                            const char *samus, const char *projectiles) {
    FusionRom rom = {0};
    uint8_t *elf = NULL;
    size_t elf_size = 0;
    char error[512];
    FusionMzmSamusExportSummary summary;
    if (!fusion_rom_open(FUSION_ASSET_GAME_METROID, rom_path, &rom,
                         error, sizeof error) ||
        !read_file(elf_path, &elf, &elf_size, error, sizeof error) ||
        !fusion_mzm_export_samus(&rom, elf, elf_size, samus, projectiles, &summary,
                                 error, sizeof error)) {
        fprintf(stderr, "mzm-export-samus: %s\n", error);
        fusion_rom_close(&rom);
        free(elf);
        return 1;
    }
    printf("Samus runtime library: %zu sequences, %zu frames, %zu unique BMPs\n",
           summary.samus_sequences, summary.samus_frames, summary.samus_unique_bmps);
    printf("Semantic bindings: %zu\n", summary.bindings);
    printf("Projectile library: %zu sequences, %zu frames, %zu unique BMPs\n",
           summary.projectile_sequences, summary.projectile_frames,
           summary.projectile_unique_bmps);
    fusion_rom_close(&rom);
    free(elf);
    return 0;
}

static int inspect_mzm_metatiles(const char *rom_path, const char *elf_path) {
    FusionRom rom = {0};
    uint8_t *elf = NULL;
    size_t elf_size = 0;
    char error[256];
    if (!fusion_rom_open(FUSION_ASSET_GAME_METROID, rom_path, &rom,
                         error, sizeof error) ||
        !read_file(elf_path, &elf, &elf_size, error, sizeof error)) {
        fprintf(stderr, "mzm-metatiles: %s\n", error);
        fusion_rom_close(&rom);
        free(elf);
        return 1;
    }
    FusionMzmRomSymbolTable symbols = {0};
    if (!fusion_mzm_read_rom_symbols(&rom, elf, elf_size, &symbols,
                                     error, sizeof error)) {
        fprintf(stderr, "mzm-metatiles: %s\n", error);
        fusion_rom_close(&rom);
        free(elf);
        return 1;
    }
    FusionMzmMetatileTable *table = malloc(sizeof *table);
    uint8_t *canonical = malloc(FUSION_MZM_METATILE_WORD_COUNT * 2);
    if (!table || !canonical) {
        fprintf(stderr, "mzm-metatiles: cannot allocate metatile buffers\n");
        free(table);
        free(canonical);
        fusion_mzm_free_symbols(&symbols);
        fusion_rom_close(&rom);
        free(elf);
        return 1;
    }
    size_t total_tileset_words = 0, total_common_words = 0;
    for (size_t index = 0; index < FUSION_MZM_TILESET_COUNT; ++index) {
        if (!fusion_mzm_read_tileset_metatiles(
                &rom, &symbols, index, table, error, sizeof error)) {
            fprintf(stderr, "mzm-metatiles: tileset %zu: %s\n", index, error);
            free(table);
            free(canonical);
            fusion_mzm_free_symbols(&symbols);
            fusion_rom_close(&rom);
            free(elf);
            return 1;
        }
        for (size_t word = 0; word < FUSION_MZM_METATILE_WORD_COUNT; ++word) {
            canonical[word * 2] = (uint8_t)table->cells[word];
            canonical[word * 2 + 1] = (uint8_t)(table->cells[word] >> 8);
        }
        char digest[41];
        if (!fusion_rom_sha1_hex(canonical,
                                 FUSION_MZM_METATILE_WORD_COUNT * 2,
                                 digest)) {
            fprintf(stderr, "mzm-metatiles: cannot hash tileset %zu\n", index);
            free(table);
            free(canonical);
            fusion_mzm_free_symbols(&symbols);
            fusion_rom_close(&rom);
            free(elf);
            return 1;
        }
        printf("metatiles\t%zu\t%u\t%zu\t%zu\t%u\t%s\n",
               index, table->entry.tilemap_pointer,
               table->tileset_word_count, table->common_word_count,
               FUSION_MZM_TILESET_METATILE_COUNT +
                   FUSION_MZM_COMMON_METATILE_COUNT,
               digest);
        total_tileset_words += table->tileset_word_count;
        total_common_words += table->common_word_count;
    }
    printf("summary\t%u\t%zu\t%zu\t%u\n", FUSION_MZM_TILESET_COUNT,
           total_tileset_words, total_common_words,
           FUSION_MZM_TILESET_COUNT *
               (FUSION_MZM_TILESET_METATILE_COUNT +
                FUSION_MZM_COMMON_METATILE_COUNT));
    free(table);
    free(canonical);
    fusion_mzm_free_symbols(&symbols);
    fusion_rom_close(&rom);
    free(elf);
    return 0;
}

static int inspect_mzm_background_renders(const char *rom_path,
                                          const char *elf_path) {
    FusionRom rom = {0};
    uint8_t *elf = NULL;
    size_t elf_size = 0;
    char error[256];
    if (!fusion_rom_open(FUSION_ASSET_GAME_METROID, rom_path, &rom,
                         error, sizeof error) ||
        !read_file(elf_path, &elf, &elf_size, error, sizeof error)) {
        fprintf(stderr, "mzm-background-renders: %s\n", error);
        fusion_rom_close(&rom);
        free(elf);
        return 1;
    }
    FusionMzmRomSymbolTable symbols = {0};
    FusionMzmRoomDirectory directory;
    if (!fusion_mzm_read_rom_symbols(&rom, elf, elf_size, &symbols,
                                     error, sizeof error) ||
        !fusion_mzm_read_room_directory(&rom, &symbols, &directory,
                                        error, sizeof error)) {
        fprintf(stderr, "mzm-background-renders: %s\n", error);
        fusion_mzm_free_symbols(&symbols);
        fusion_rom_close(&rom);
        free(elf);
        return 1;
    }
    FusionMzmVideoMemory *memory = malloc(sizeof *memory);
    FusionMzmMetatileTable *metatiles = malloc(sizeof *metatiles);
    if (!memory || !metatiles) {
        fprintf(stderr,
                "mzm-background-renders: cannot allocate render buffers\n");
        free(memory);
        free(metatiles);
        fusion_mzm_free_symbols(&symbols);
        fusion_rom_close(&rom);
        free(elf);
        return 1;
    }
    size_t rendered_count = 0, rle_count = 0, text_count = 0;
    size_t skipped_count = 0, total_pixels = 0, unresolved_count = 0;
    for (size_t area = 0; area < FUSION_MZM_ROOM_AREA_COUNT; ++area) {
        const FusionMzmRoomAreaSpec *spec = fusion_mzm_room_area_spec(area);
        for (size_t room = 0; room < directory.areas[area].room_count; ++room) {
            FusionMzmRoomEntry entry;
            FusionMzmRoomBackgrounds backgrounds;
            if (!fusion_mzm_read_room_entry(&rom, &directory, area, room,
                                            &entry, error, sizeof error) ||
                !fusion_mzm_read_room_backgrounds(&rom, &entry, &backgrounds,
                                                  error, sizeof error) ||
                !fusion_mzm_load_tileset_video_memory(
                    &rom, &symbols, entry.tileset, memory,
                    error, sizeof error) ||
                !fusion_mzm_read_tileset_metatiles(
                    &rom, &symbols, entry.tileset, metatiles,
                    error, sizeof error)) {
                fprintf(stderr,
                        "mzm-background-renders: %s %zu: %s\n",
                        spec->id, room, error);
                fusion_mzm_free_room_backgrounds(&backgrounds);
                free(memory);
                free(metatiles);
                fusion_mzm_free_symbols(&symbols);
                fusion_rom_close(&rom);
                free(elf);
                return 1;
            }
            for (size_t layer = 0;
                 layer < FUSION_MZM_ROOM_BACKGROUND_COUNT; ++layer) {
                const FusionMzmRoomBackground *background =
                    &backgrounds.layers[layer];
                if (background->status != FUSION_MZM_BACKGROUND_RLE &&
                    background->status != FUSION_MZM_BACKGROUND_TEXT) {
                    ++skipped_count;
                    continue;
                }
                FusionMzmRenderedBackground rendered;
                bool success = background->status == FUSION_MZM_BACKGROUND_RLE
                    ? fusion_mzm_render_rle_background(
                        &background->rle, metatiles, memory, &rendered,
                        error, sizeof error)
                    : fusion_mzm_render_text_background(
                        &background->text, memory, &rendered,
                        error, sizeof error);
                if (!success) {
                    fprintf(stderr,
                            "mzm-background-renders: %s %zu BG%zu: %s\n",
                            spec->id, room, layer, error);
                    fusion_mzm_free_room_backgrounds(&backgrounds);
                    free(memory);
                    free(metatiles);
                    fusion_mzm_free_symbols(&symbols);
                    fusion_rom_close(&rom);
                    free(elf);
                    return 1;
                }
                char digest[41];
                if (!fusion_rom_sha1_hex(rendered.rgba,
                                         rendered.pixel_count * 4, digest)) {
                    fprintf(stderr,
                            "mzm-background-renders: cannot hash %s %zu BG%zu\n",
                            spec->id, room, layer);
                    fusion_mzm_free_rendered_background(&rendered);
                    fusion_mzm_free_room_backgrounds(&backgrounds);
                    free(memory);
                    free(metatiles);
                    fusion_mzm_free_symbols(&symbols);
                    fusion_rom_close(&rom);
                    free(elf);
                    return 1;
                }
                const char *kind = background->status == FUSION_MZM_BACKGROUND_RLE
                    ? "rle" : "text";
                printf("render\t%s\t%zu\t%zu\t%s\t%zu\t%zu\t%zu\t%zu\t%s\n",
                       spec->id, room, layer, kind,
                       rendered.width_pixels, rendered.height_pixels,
                       rendered.pixel_count, rendered.unresolved_count, digest);
                ++rendered_count;
                if (background->status == FUSION_MZM_BACKGROUND_RLE)
                    ++rle_count;
                else
                    ++text_count;
                total_pixels += rendered.pixel_count;
                unresolved_count += rendered.unresolved_count;
                fusion_mzm_free_rendered_background(&rendered);
            }
            fusion_mzm_free_room_backgrounds(&backgrounds);
        }
    }
    printf("summary\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\n",
           rendered_count, rle_count, text_count, skipped_count,
           total_pixels, unresolved_count);
    free(memory);
    free(metatiles);
    fusion_mzm_free_symbols(&symbols);
    fusion_rom_close(&rom);
    free(elf);
    return 0;
}

static bool render_mzm_background(
    const FusionMzmRoomBackground *background,
    const FusionMzmMetatileTable *metatiles,
    const FusionMzmVideoMemory *memory,
    FusionMzmRenderedBackground *rendered,
    char *error, size_t error_size) {
    if (background->status == FUSION_MZM_BACKGROUND_RLE)
        return fusion_mzm_render_rle_background(
            &background->rle, metatiles, memory, rendered,
            error, error_size);
    if (background->status == FUSION_MZM_BACKGROUND_TEXT)
        return fusion_mzm_render_text_background(
            &background->text, memory, rendered, error, error_size);
    return false;
}

static int inspect_mzm_background_composites(const char *rom_path,
                                             const char *elf_path) {
    FusionRom rom = {0};
    uint8_t *elf = NULL;
    size_t elf_size = 0;
    char error[256];
    if (!fusion_rom_open(FUSION_ASSET_GAME_METROID, rom_path, &rom,
                         error, sizeof error) ||
        !read_file(elf_path, &elf, &elf_size, error, sizeof error)) {
        fprintf(stderr, "mzm-background-composites: %s\n", error);
        fusion_rom_close(&rom);
        free(elf);
        return 1;
    }
    FusionMzmRomSymbolTable symbols = {0};
    FusionMzmRoomDirectory directory;
    if (!fusion_mzm_read_rom_symbols(&rom, elf, elf_size, &symbols,
                                     error, sizeof error) ||
        !fusion_mzm_read_room_directory(&rom, &symbols, &directory,
                                        error, sizeof error)) {
        fprintf(stderr, "mzm-background-composites: %s\n", error);
        fusion_mzm_free_symbols(&symbols);
        fusion_rom_close(&rom);
        free(elf);
        return 1;
    }
    FusionMzmVideoMemory *memory = malloc(sizeof *memory);
    FusionMzmMetatileTable *metatiles = malloc(sizeof *metatiles);
    if (!memory || !metatiles) {
        fprintf(stderr,
                "mzm-background-composites: cannot allocate render buffers\n");
        free(memory);
        free(metatiles);
        fusion_mzm_free_symbols(&symbols);
        fusion_rom_close(&rom);
        free(elf);
        return 1;
    }
    size_t selection_count = 0, missing_bg1_count = 0;
    size_t bg12_count = 0, bg1_only_count = 0, mismatch_count = 0;
    size_t total_pixels = 0, total_bg1 = 0, total_bg2 = 0;
    size_t total_unresolved = 0, total_bitmap_bytes = 0;
    size_t total_mask_bytes = 0;
    for (size_t area = 0; area < FUSION_MZM_ROOM_AREA_COUNT; ++area) {
        const FusionMzmRoomAreaSpec *spec = fusion_mzm_room_area_spec(area);
        for (size_t room = 0; room < directory.areas[area].room_count; ++room) {
            FusionMzmRoomEntry entry;
            FusionMzmRoomBackgrounds backgrounds = {0};
            if (!fusion_mzm_read_room_entry(&rom, &directory, area, room,
                                            &entry, error, sizeof error) ||
                !fusion_mzm_read_room_backgrounds(&rom, &entry, &backgrounds,
                                                  error, sizeof error) ||
                !fusion_mzm_load_tileset_video_memory(
                    &rom, &symbols, entry.tileset, memory,
                    error, sizeof error) ||
                !fusion_mzm_read_tileset_metatiles(
                    &rom, &symbols, entry.tileset, metatiles,
                    error, sizeof error)) {
                fprintf(stderr,
                        "mzm-background-composites: %s %zu: %s\n",
                        spec->id, room, error);
                fusion_mzm_free_room_backgrounds(&backgrounds);
                free(memory);
                free(metatiles);
                fusion_mzm_free_symbols(&symbols);
                fusion_rom_close(&rom);
                free(elf);
                return 1;
            }
            const FusionMzmRoomBackground *bg1 = &backgrounds.layers[1];
            const FusionMzmRoomBackground *bg2 = &backgrounds.layers[2];
            bool bg1_supported = bg1->status == FUSION_MZM_BACKGROUND_RLE ||
                                 bg1->status == FUSION_MZM_BACKGROUND_TEXT;
            bool bg2_supported = bg2->status == FUSION_MZM_BACKGROUND_RLE ||
                                 bg2->status == FUSION_MZM_BACKGROUND_TEXT;
            if (!bg1_supported) {
                printf("skipped\t%s\t%zu\tmissing-bg1\t%s\t%s\n",
                       spec->id, room,
                       mzm_background_status_name(bg1->status),
                       mzm_background_status_name(bg2->status));
                ++missing_bg1_count;
                fusion_mzm_free_room_backgrounds(&backgrounds);
                continue;
            }
            FusionMzmRenderedBackground rendered_bg1 = {0};
            FusionMzmRenderedBackground rendered_bg2 = {0};
            if (!render_mzm_background(bg1, metatiles, memory, &rendered_bg1,
                                       error, sizeof error)) {
                fprintf(stderr,
                        "mzm-background-composites: cannot render %s %zu: %s\n",
                        spec->id, room, error);
                fusion_mzm_free_rendered_background(&rendered_bg1);
                fusion_mzm_free_rendered_background(&rendered_bg2);
                fusion_mzm_free_room_backgrounds(&backgrounds);
                free(memory);
                free(metatiles);
                fusion_mzm_free_symbols(&symbols);
                fusion_rom_close(&rom);
                free(elf);
                return 1;
            }
            bool use_bg2 = false;
            if (bg2_supported) {
                if (!render_mzm_background(bg2, metatiles, memory,
                                           &rendered_bg2,
                                           error, sizeof error)) {
                    fprintf(stderr,
                            "mzm-background-composites: cannot render %s %zu BG2: %s\n",
                            spec->id, room, error);
                    fusion_mzm_free_rendered_background(&rendered_bg1);
                    fusion_mzm_free_room_backgrounds(&backgrounds);
                    free(memory);
                    free(metatiles);
                    fusion_mzm_free_symbols(&symbols);
                    fusion_rom_close(&rom);
                    free(elf);
                    return 1;
                }
                if (rendered_bg1.width_pixels == rendered_bg2.width_pixels &&
                    rendered_bg1.height_pixels == rendered_bg2.height_pixels) {
                    use_bg2 = true;
                } else {
                    ++mismatch_count;
                    fusion_mzm_free_rendered_background(&rendered_bg2);
                }
            }
            FusionMzmCompositeBackground composite;
            if (!fusion_mzm_select_bg12(
                    &rendered_bg1, use_bg2 ? &rendered_bg2 : NULL, &composite,
                    error, sizeof error)) {
                fprintf(stderr,
                        "mzm-background-composites: cannot compose %s %zu: %s\n",
                        spec->id, room, error);
                fusion_mzm_free_rendered_background(&rendered_bg1);
                fusion_mzm_free_rendered_background(&rendered_bg2);
                fusion_mzm_free_room_backgrounds(&backgrounds);
                free(memory);
                free(metatiles);
                fusion_mzm_free_symbols(&symbols);
                fusion_rom_close(&rom);
                free(elf);
                return 1;
            }
            FusionMzmSerializedBackground serialized;
            if (!fusion_mzm_serialize_background(
                    &composite, &serialized, error, sizeof error)) {
                fprintf(stderr,
                        "mzm-background-composites: cannot serialize %s %zu: %s\n",
                        spec->id, room, error);
                fusion_mzm_free_composite_background(&composite);
                fusion_mzm_free_rendered_background(&rendered_bg1);
                fusion_mzm_free_rendered_background(&rendered_bg2);
                fusion_mzm_free_room_backgrounds(&backgrounds);
                free(memory);
                free(metatiles);
                fusion_mzm_free_symbols(&symbols);
                fusion_rom_close(&rom);
                free(elf);
                return 1;
            }
            char rgb_digest[41], visibility_digest[41], bitmap_digest[41];
            if (!fusion_rom_sha1_hex(composite.rgb,
                                     composite.pixel_count * 3, rgb_digest) ||
                !fusion_rom_sha1_hex(serialized.visibility,
                                     serialized.visibility_size,
                                     visibility_digest) ||
                !fusion_rom_sha1_hex(serialized.bitmap,
                                     serialized.bitmap_size,
                                     bitmap_digest)) {
                fprintf(stderr,
                        "mzm-background-composites: cannot hash %s %zu\n",
                        spec->id, room);
                fusion_mzm_free_serialized_background(&serialized);
                fusion_mzm_free_composite_background(&composite);
                fusion_mzm_free_rendered_background(&rendered_bg1);
                fusion_mzm_free_rendered_background(&rendered_bg2);
                fusion_mzm_free_room_backgrounds(&backgrounds);
                free(memory);
                free(metatiles);
                fusion_mzm_free_symbols(&symbols);
                fusion_rom_close(&rom);
                free(elf);
                return 1;
            }
            printf("background\t%s\t%zu\t%s\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t"
                   "%s\t%zu\t%s\t%zu\t%s\n",
                   spec->id, room, use_bg2 ? "bg12" : "bg1",
                   composite.width_pixels,
                   composite.height_pixels, composite.pixel_count,
                   composite.bg1_visible_count, composite.bg2_visible_count,
                   composite.unresolved_count, rgb_digest,
                   serialized.bitmap_size, bitmap_digest,
                   serialized.visibility_size, visibility_digest);
            ++selection_count;
            if (use_bg2) ++bg12_count;
            else ++bg1_only_count;
            total_pixels += composite.pixel_count;
            total_bg1 += composite.bg1_visible_count;
            total_bg2 += composite.bg2_visible_count;
            total_unresolved += composite.unresolved_count;
            total_bitmap_bytes += serialized.bitmap_size;
            total_mask_bytes += serialized.visibility_size;
            fusion_mzm_free_serialized_background(&serialized);
            fusion_mzm_free_composite_background(&composite);
            fusion_mzm_free_rendered_background(&rendered_bg1);
            fusion_mzm_free_rendered_background(&rendered_bg2);
            fusion_mzm_free_room_backgrounds(&backgrounds);
        }
    }
    printf("summary\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\n",
           selection_count, missing_bg1_count, bg12_count, bg1_only_count,
           mismatch_count, total_pixels, total_bg1, total_bg2,
           total_unresolved, total_bitmap_bytes, total_mask_bytes);
    free(memory);
    free(metatiles);
    fusion_mzm_free_symbols(&symbols);
    fusion_rom_close(&rom);
    free(elf);
    return 0;
}

int main(int argc, char **argv) {
    bool list = false, verify = false, dry_run = false, assets = false, force = false;
    const char *header_path = NULL;
    const char *manifest_game = NULL;
    const char *manifest_rom_path = NULL;
    const char *manifest_output_path = NULL;
    const char *aria_directory_path = NULL;
    const char *aria_map_path = NULL;
    const char *aria_rooms_path = NULL;
    const char *aria_transitions_path = NULL;
    const char *aria_backgrounds_path = NULL;
    const char *aria_background_tiles_path = NULL;
    const char *aria_background_renders_path = NULL;
    const char *aria_background_composites_path = NULL;
    const char *aria_collisions_path = NULL;
    const char *aria_runtime_rooms_path = NULL;
    const char *aria_loads_path = NULL;
    const char *aria_video_memory_path = NULL;
    const char *aria_entities_path = NULL;
    const char *aria_bosses_path = NULL;
    const char *aria_summary_path = NULL;
    const char *aria_export_rom_path = NULL;
    const char *aria_export_output = NULL;
    const char *aria_soma_rom_path = NULL;
    const char *aria_soma_output = NULL;
    const char *aria_weapons_rom_path = NULL;
    const char *aria_weapons_metadata = NULL;
    const char *aria_weapons_runtime = NULL;
    const char *aria_objects_rom_path = NULL;
    const char *aria_objects_metadata = NULL;
    const char *aria_objects_runtime = NULL;
    const char *aria_catalog_rom_path = NULL;
    const char *aria_catalog_output_path = NULL;
    const char *aria_catalog_index_path = NULL;
    const char *mzm_symbols_rom_path = NULL;
    const char *mzm_symbols_elf_path = NULL;
    const char *mzm_bank_rom_path = NULL;
    const char *mzm_bank_elf_path = NULL;
    const char *mzm_bank_output_path = NULL;
    const char *mzm_rooms_rom_path = NULL;
    const char *mzm_rooms_elf_path = NULL;
    const char *mzm_clip_rom_path = NULL;
    const char *mzm_clip_elf_path = NULL;
    const char *mzm_runtime_core_rom_path = NULL;
    const char *mzm_runtime_core_elf_path = NULL;
    const char *mzm_runtime_door_rom_path = NULL;
    const char *mzm_runtime_door_elf_path = NULL;
    const char *mzm_runtime_hatch_rom_path = NULL;
    const char *mzm_runtime_hatch_elf_path = NULL;
    const char *mzm_runtime_metadata_rom_path = NULL;
    const char *mzm_runtime_sprite_rom_path = NULL;
    const char *mzm_bitmap_rom_path = NULL;
    const char *mzm_table_rom_path = NULL;
    const char *mzm_export_background_rom_path = NULL;
    const char *mzm_export_rom_path = NULL;
    const char *mzm_export_elf_path = NULL;
    const char *mzm_export_output = NULL;
    const char *mzm_samus_rom_path = NULL, *mzm_samus_elf_path = NULL;
    const char *mzm_samus_output = NULL, *mzm_projectile_output = NULL;
    const char *mzm_export_background_elf_path = NULL;
    const char *mzm_table_elf_path = NULL;
    const char *mzm_table_dump = NULL;
    const char *mzm_bitmap_elf_path = NULL;
    const char *mzm_runtime_sprite_elf_path = NULL;
    const char *mzm_runtime_metadata_elf_path = NULL;
    const char *mzm_background_rom_path = NULL;
    const char *mzm_background_elf_path = NULL;
    const char *mzm_tileset_rom_path = NULL;
    const char *mzm_tileset_elf_path = NULL;
    const char *mzm_metatile_rom_path = NULL;
    const char *mzm_metatile_elf_path = NULL;
    const char *mzm_render_rom_path = NULL;
    const char *mzm_render_elf_path = NULL;
    const char *mzm_composite_rom_path = NULL;
    const char *mzm_composite_elf_path = NULL;
    const char *rom_dir = "roms", *aria_path = NULL, *metroid_path = NULL;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--list")) list = true;
        else if (!strcmp(argv[i], "--verify")) verify = true;
        else if (!strcmp(argv[i], "--rom-manifest") && i + 3 < argc) {
            manifest_game = argv[++i];
            manifest_rom_path = argv[++i];
            manifest_output_path = argv[++i];
        }
        else if (!strcmp(argv[i], "--header") && i + 1 < argc) header_path = argv[++i];
        else if (!strcmp(argv[i], "--aria-directory") && i + 1 < argc)
            aria_directory_path = argv[++i];
        else if (!strcmp(argv[i], "--aria-map") && i + 1 < argc)
            aria_map_path = argv[++i];
        else if (!strcmp(argv[i], "--aria-rooms") && i + 1 < argc)
            aria_rooms_path = argv[++i];
        else if (!strcmp(argv[i], "--aria-transitions") && i + 1 < argc)
            aria_transitions_path = argv[++i];
        else if (!strcmp(argv[i], "--aria-backgrounds") && i + 1 < argc)
            aria_backgrounds_path = argv[++i];
        else if (!strcmp(argv[i], "--aria-background-tiles") && i + 1 < argc)
            aria_background_tiles_path = argv[++i];
        else if (!strcmp(argv[i], "--aria-background-renders") && i + 1 < argc)
            aria_background_renders_path = argv[++i];
        else if (!strcmp(argv[i], "--aria-background-composites") &&
                 i + 1 < argc)
            aria_background_composites_path = argv[++i];
        else if (!strcmp(argv[i], "--aria-collisions") && i + 1 < argc)
            aria_collisions_path = argv[++i];
        else if (!strcmp(argv[i], "--aria-runtime-rooms") && i + 1 < argc)
            aria_runtime_rooms_path = argv[++i];
        else if (!strcmp(argv[i], "--aria-loads") && i + 1 < argc)
            aria_loads_path = argv[++i];
        else if (!strcmp(argv[i], "--aria-video-memory") && i + 1 < argc)
            aria_video_memory_path = argv[++i];
        else if (!strcmp(argv[i], "--aria-entities") && i + 1 < argc)
            aria_entities_path = argv[++i];
        else if (!strcmp(argv[i], "--aria-bosses") && i + 1 < argc)
            aria_bosses_path = argv[++i];
        else if (!strcmp(argv[i], "--aria-summary") && i + 1 < argc)
            aria_summary_path = argv[++i];
        else if (!strcmp(argv[i], "--aria-export-rooms") && i + 2 < argc) {
            aria_export_rom_path = argv[++i];
            aria_export_output = argv[++i];
        }
        else if (!strcmp(argv[i], "--aria-export-soma") && i + 2 < argc) {
            aria_soma_rom_path = argv[++i];
            aria_soma_output = argv[++i];
        }
        else if (!strcmp(argv[i], "--aria-export-weapons") && i + 3 < argc) {
            aria_weapons_rom_path = argv[++i];
            aria_weapons_metadata = argv[++i];
            aria_weapons_runtime = argv[++i];
        }
        else if (!strcmp(argv[i], "--aria-export-objects") && i + 3 < argc) {
            aria_objects_rom_path = argv[++i];
            aria_objects_metadata = argv[++i];
            aria_objects_runtime = argv[++i];
        }
        else if (!strcmp(argv[i], "--aria-catalog") && i + 3 < argc) {
            aria_catalog_rom_path = argv[++i];
            aria_catalog_output_path = argv[++i];
            aria_catalog_index_path = argv[++i];
        }
        else if (!strcmp(argv[i], "--mzm-symbols") && i + 2 < argc) {
            mzm_symbols_rom_path = argv[++i];
            mzm_symbols_elf_path = argv[++i];
        }
        else if (!strcmp(argv[i], "--mzm-rom-bank") && i + 3 < argc) {
            mzm_bank_rom_path = argv[++i];
            mzm_bank_elf_path = argv[++i];
            mzm_bank_output_path = argv[++i];
        }
        else if (!strcmp(argv[i], "--mzm-rooms") && i + 2 < argc) {
            mzm_rooms_rom_path = argv[++i];
            mzm_rooms_elf_path = argv[++i];
        }
        else if (!strcmp(argv[i], "--mzm-clipdata") && i + 2 < argc) {
            mzm_clip_rom_path = argv[++i];
            mzm_clip_elf_path = argv[++i];
        }
        else if (!strcmp(argv[i], "--mzm-runtime-room-cores") &&
                 i + 2 < argc) {
            mzm_runtime_core_rom_path = argv[++i];
            mzm_runtime_core_elf_path = argv[++i];
        }
        else if (!strcmp(argv[i], "--mzm-runtime-room-doors") &&
                 i + 2 < argc) {
            mzm_runtime_door_rom_path = argv[++i];
            mzm_runtime_door_elf_path = argv[++i];
        }
        else if (!strcmp(argv[i], "--mzm-runtime-room-hatches") &&
                 i + 2 < argc) {
            mzm_runtime_hatch_rom_path = argv[++i];
            mzm_runtime_hatch_elf_path = argv[++i];
        }
        else if (!strcmp(argv[i], "--mzm-runtime-room-metadata") &&
                 i + 2 < argc) {
            mzm_runtime_metadata_rom_path = argv[++i];
            mzm_runtime_metadata_elf_path = argv[++i];
        }
        else if (!strcmp(argv[i], "--mzm-runtime-room-sprites") &&
                 i + 2 < argc) {
            mzm_runtime_sprite_rom_path = argv[++i];
            mzm_runtime_sprite_elf_path = argv[++i];
        }
        else if (!strcmp(argv[i], "--mzm-export-samus") && i + 4 < argc) {
            mzm_samus_rom_path = argv[++i];
            mzm_samus_elf_path = argv[++i];
            mzm_samus_output = argv[++i];
            mzm_projectile_output = argv[++i];
        }
        else if (!strcmp(argv[i], "--mzm-export-rooms") && i + 3 < argc) {
            mzm_export_rom_path = argv[++i];
            mzm_export_elf_path = argv[++i];
            mzm_export_output = argv[++i];
        }
        else if (!strcmp(argv[i], "--mzm-export-backgrounds") && i + 2 < argc) {
            mzm_export_background_rom_path = argv[++i];
            mzm_export_background_elf_path = argv[++i];
        }
        else if (!strcmp(argv[i], "--mzm-runtime-tables") && i + 2 < argc) {
            mzm_table_rom_path = argv[++i];
            mzm_table_elf_path = argv[++i];
            if (i + 1 < argc && argv[i + 1][0] != '-') mzm_table_dump = argv[++i];
        }
        else if (!strcmp(argv[i], "--mzm-bitmaps") && i + 2 < argc) {
            mzm_bitmap_rom_path = argv[++i];
            mzm_bitmap_elf_path = argv[++i];
        }
        else if (!strcmp(argv[i], "--mzm-backgrounds") && i + 2 < argc) {
            mzm_background_rom_path = argv[++i];
            mzm_background_elf_path = argv[++i];
        }
        else if (!strcmp(argv[i], "--mzm-tilesets") && i + 2 < argc) {
            mzm_tileset_rom_path = argv[++i];
            mzm_tileset_elf_path = argv[++i];
        }
        else if (!strcmp(argv[i], "--mzm-metatiles") && i + 2 < argc) {
            mzm_metatile_rom_path = argv[++i];
            mzm_metatile_elf_path = argv[++i];
        }
        else if (!strcmp(argv[i], "--mzm-background-renders") &&
                 i + 2 < argc) {
            mzm_render_rom_path = argv[++i];
            mzm_render_elf_path = argv[++i];
        }
        else if (!strcmp(argv[i], "--mzm-background-composites") &&
                 i + 2 < argc) {
            mzm_composite_rom_path = argv[++i];
            mzm_composite_elf_path = argv[++i];
        }
        else if (!strcmp(argv[i], "--dry-run")) dry_run = true;
        else if (!strcmp(argv[i], "--assets") || !strcmp(argv[i], "--all")) assets = true;
        else if (!strcmp(argv[i], "--force")) force = true;
        else if (!strcmp(argv[i], "--rom-dir") && i + 1 < argc) rom_dir = argv[++i];
        else if (!strcmp(argv[i], "--aria") && i + 1 < argc) aria_path = argv[++i];
        else if (!strcmp(argv[i], "--metroid") && i + 1 < argc) metroid_path = argv[++i];
        else {
            usage(argv[0]);
            return 2;
        }
    }
    if (list) {
        list_games();
        return 0;
    }
    if (manifest_game)
        return write_rom_manifest(manifest_game, manifest_rom_path,
                                  manifest_output_path);
    if (header_path) return inspect_header(header_path);
    if (aria_directory_path) return inspect_aria_directory(aria_directory_path);
    if (aria_map_path) return inspect_aria_map(aria_map_path);
    if (aria_rooms_path) return inspect_aria_rooms(aria_rooms_path);
    if (aria_transitions_path) return inspect_aria_transitions(aria_transitions_path);
    if (aria_backgrounds_path) return inspect_aria_backgrounds(aria_backgrounds_path);
    if (aria_background_tiles_path)
        return inspect_aria_background_tiles(aria_background_tiles_path);
    if (aria_background_renders_path)
        return inspect_aria_background_renders(aria_background_renders_path);
    if (aria_background_composites_path)
        return inspect_aria_background_composites(
            aria_background_composites_path);
    if (aria_collisions_path) return inspect_aria_collisions(aria_collisions_path);
    if (aria_runtime_rooms_path)
        return inspect_aria_runtime_rooms(aria_runtime_rooms_path);
    if (aria_loads_path) return inspect_aria_loads(aria_loads_path);
    if (aria_video_memory_path)
        return inspect_aria_video_memory(aria_video_memory_path);
    if (aria_entities_path) return inspect_aria_entities(aria_entities_path);
    if (aria_bosses_path) return inspect_aria_bosses(aria_bosses_path);
    if (aria_summary_path) return inspect_aria_summary(aria_summary_path);
    if (aria_export_rom_path)
        return export_aria_runtime_rooms(aria_export_rom_path, aria_export_output);
    if (aria_soma_rom_path)
        return export_aria_soma(aria_soma_rom_path, aria_soma_output);
    if (aria_weapons_rom_path)
        return export_aria_weapons(aria_weapons_rom_path,
                                   aria_weapons_metadata,
                                   aria_weapons_runtime);
    if (aria_objects_rom_path)
        return export_aria_objects(aria_objects_rom_path,
                                   aria_objects_metadata,
                                   aria_objects_runtime);
    if (aria_catalog_rom_path)
        return write_aria_catalog(aria_catalog_rom_path, aria_catalog_output_path,
                                  aria_catalog_index_path);
    if (mzm_symbols_rom_path)
        return inspect_mzm_symbols(mzm_symbols_rom_path, mzm_symbols_elf_path);
    if (mzm_bank_rom_path)
        return write_mzm_rom_bank(mzm_bank_rom_path, mzm_bank_elf_path,
                                  mzm_bank_output_path);
    if (mzm_rooms_rom_path)
        return inspect_mzm_rooms(mzm_rooms_rom_path, mzm_rooms_elf_path);
    if (mzm_clip_rom_path)
        return inspect_mzm_clipdata(mzm_clip_rom_path, mzm_clip_elf_path);
    if (mzm_runtime_core_rom_path)
        return inspect_mzm_runtime_rooms(
            mzm_runtime_core_rom_path, mzm_runtime_core_elf_path,
            false, false, false, false);
    if (mzm_runtime_door_rom_path)
        return inspect_mzm_runtime_rooms(
            mzm_runtime_door_rom_path, mzm_runtime_door_elf_path,
            true, false, false, false);
    if (mzm_runtime_hatch_rom_path)
        return inspect_mzm_runtime_rooms(
            mzm_runtime_hatch_rom_path, mzm_runtime_hatch_elf_path,
            true, true, false, false);
    if (mzm_runtime_metadata_rom_path)
        return inspect_mzm_runtime_rooms(
            mzm_runtime_metadata_rom_path, mzm_runtime_metadata_elf_path,
            true, true, true, false);
    if (mzm_runtime_sprite_rom_path)
        return inspect_mzm_runtime_rooms(
            mzm_runtime_sprite_rom_path, mzm_runtime_sprite_elf_path,
            true, true, true, true);
    if (mzm_samus_rom_path)
        return export_mzm_samus(mzm_samus_rom_path, mzm_samus_elf_path,
                                mzm_samus_output, mzm_projectile_output);
    if (mzm_export_rom_path)
        return export_mzm_runtime_rooms(mzm_export_rom_path, mzm_export_elf_path,
                                        mzm_export_output);
    if (mzm_export_background_rom_path)
        return inspect_mzm_export_backgrounds(mzm_export_background_rom_path,
                                              mzm_export_background_elf_path);
    if (mzm_table_rom_path)
        return inspect_mzm_runtime_tables(mzm_table_rom_path,
                                          mzm_table_elf_path, mzm_table_dump);
    if (mzm_bitmap_rom_path)
        return inspect_mzm_bitmaps(mzm_bitmap_rom_path, mzm_bitmap_elf_path);
    if (mzm_background_rom_path)
        return inspect_mzm_backgrounds(mzm_background_rom_path,
                                       mzm_background_elf_path);
    if (mzm_tileset_rom_path)
        return inspect_mzm_tilesets(mzm_tileset_rom_path,
                                    mzm_tileset_elf_path);
    if (mzm_metatile_rom_path)
        return inspect_mzm_metatiles(mzm_metatile_rom_path,
                                     mzm_metatile_elf_path);
    if (mzm_render_rom_path)
        return inspect_mzm_background_renders(mzm_render_rom_path,
                                              mzm_render_elf_path);
    if (mzm_composite_rom_path)
        return inspect_mzm_background_composites(
            mzm_composite_rom_path, mzm_composite_elf_path);
    if (dry_run || assets) {
        printf("planned: verify metroid and aria ROM fingerprints\n");
        printf("planned: native extraction tasks remain Python-authoritative\n");
        if (force) printf("planned: force invalidation of native asset task cache\n");
        if (dry_run) return 0;
        if (!verify) return 3;
    }
    if (verify) return verify_games(rom_dir, aria_path, metroid_path);
    usage(argv[0]);
    return 2;
}
