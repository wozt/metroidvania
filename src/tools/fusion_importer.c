/* SPDX-License-Identifier: GPL-3.0-only */
/* Native asset pipeline entry point. Extraction remains Python-authoritative
 * until each C replacement has an equivalence fixture and integration test. */
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
            "       %s --header FILE\n"
            "       %s --aria-directory FILE\n"
            "       %s --aria-map FILE\n"
            "       %s --aria-rooms FILE\n"
            "       %s --aria-transitions FILE\n"
            "       %s --aria-backgrounds FILE\n"
            "       %s --aria-collisions FILE\n"
            "       %s --aria-loads FILE\n"
            "       %s --aria-entities FILE\n"
            "       %s --aria-bosses FILE\n"
            "       %s --aria-summary FILE\n"
            "       %s --aria-catalog ROM WORLD_JSON ROOMS_TSV\n"
            "       %s --mzm-symbols ROM ELF\n"
            "       %s --mzm-rom-bank ROM ELF OUTPUT_DIR\n"
            "       %s --mzm-rooms ROM ELF\n"
            "       %s --mzm-clipdata ROM ELF\n"
            "       %s --dry-run [--force]\n"
            "       %s --assets|--all [--dry-run] [--force]\n",
            name, name, name, name, name, name, name, name, name, name, name,
            name, name, name, name, name, name, name, name, name);
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

int main(int argc, char **argv) {
    bool list = false, verify = false, dry_run = false, assets = false, force = false;
    const char *header_path = NULL;
    const char *aria_directory_path = NULL;
    const char *aria_map_path = NULL;
    const char *aria_rooms_path = NULL;
    const char *aria_transitions_path = NULL;
    const char *aria_backgrounds_path = NULL;
    const char *aria_collisions_path = NULL;
    const char *aria_loads_path = NULL;
    const char *aria_entities_path = NULL;
    const char *aria_bosses_path = NULL;
    const char *aria_summary_path = NULL;
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
    const char *rom_dir = "roms", *aria_path = NULL, *metroid_path = NULL;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--list")) list = true;
        else if (!strcmp(argv[i], "--verify")) verify = true;
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
        else if (!strcmp(argv[i], "--aria-collisions") && i + 1 < argc)
            aria_collisions_path = argv[++i];
        else if (!strcmp(argv[i], "--aria-loads") && i + 1 < argc)
            aria_loads_path = argv[++i];
        else if (!strcmp(argv[i], "--aria-entities") && i + 1 < argc)
            aria_entities_path = argv[++i];
        else if (!strcmp(argv[i], "--aria-bosses") && i + 1 < argc)
            aria_bosses_path = argv[++i];
        else if (!strcmp(argv[i], "--aria-summary") && i + 1 < argc)
            aria_summary_path = argv[++i];
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
    if (header_path) return inspect_header(header_path);
    if (aria_directory_path) return inspect_aria_directory(aria_directory_path);
    if (aria_map_path) return inspect_aria_map(aria_map_path);
    if (aria_rooms_path) return inspect_aria_rooms(aria_rooms_path);
    if (aria_transitions_path) return inspect_aria_transitions(aria_transitions_path);
    if (aria_backgrounds_path) return inspect_aria_backgrounds(aria_backgrounds_path);
    if (aria_collisions_path) return inspect_aria_collisions(aria_collisions_path);
    if (aria_loads_path) return inspect_aria_loads(aria_loads_path);
    if (aria_entities_path) return inspect_aria_entities(aria_entities_path);
    if (aria_bosses_path) return inspect_aria_bosses(aria_bosses_path);
    if (aria_summary_path) return inspect_aria_summary(aria_summary_path);
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
