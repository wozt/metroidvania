/* SPDX-License-Identifier: GPL-3.0-only */
/* Native asset pipeline entry point. Extraction remains Python-authoritative
 * until each C replacement has an equivalence fixture and integration test. */
#include "fusion/assets/aos.h"
#include "fusion/assets/rom.h"
#include "fusion/assets/gba.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void usage(const char *name) {
    fprintf(stderr,
            "Usage: %s --list\n"
            "       %s --verify [--rom-dir DIR] [--aria FILE] [--metroid FILE]\n"
            "       %s --header FILE\n"
            "       %s --aria-directory FILE\n"
            "       %s --aria-map FILE\n"
            "       %s --aria-rooms FILE\n"
            "       %s --dry-run [--force]\n"
            "       %s --assets|--all [--dry-run] [--force]\n",
            name, name, name, name, name, name, name, name);
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

int main(int argc, char **argv) {
    bool list = false, verify = false, dry_run = false, assets = false, force = false;
    const char *header_path = NULL;
    const char *aria_directory_path = NULL;
    const char *aria_map_path = NULL;
    const char *aria_rooms_path = NULL;
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
