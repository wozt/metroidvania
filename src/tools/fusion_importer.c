/* SPDX-License-Identifier: GPL-3.0-only */
/* Native asset pipeline entry point. Extraction remains Python-authoritative
 * until each C replacement has an equivalence fixture and integration test. */
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
            "       %s --dry-run [--force]\n"
            "       %s --assets|--all [--dry-run] [--force]\n",
            name, name, name, name, name);
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
            printf("%s: valid ROM (%s)\n", spec->id, spec->sha1);
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

int main(int argc, char **argv) {
    bool list = false, verify = false, dry_run = false, assets = false, force = false;
    const char *header_path = NULL;
    const char *rom_dir = "roms", *aria_path = NULL, *metroid_path = NULL;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--list")) list = true;
        else if (!strcmp(argv[i], "--verify")) verify = true;
        else if (!strcmp(argv[i], "--header") && i + 1 < argc) header_path = argv[++i];
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
