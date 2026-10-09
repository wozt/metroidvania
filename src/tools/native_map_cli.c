/* SPDX-License-Identifier: GPL-3.0-only */
/* Internal headless adapter around the exact native-map core used by GTK. */
#include "core/native_map.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *option(int argc, char **argv, const char *name)
{
    size_t length = strlen(name);
    for (int index = 1; index < argc; ++index) {
        if (!strncmp(argv[index], name, length) && argv[index][length] == '=')
            return argv[index] + length + 1;
    }
    return NULL;
}

static bool number(const char *text, unsigned maximum, unsigned *result)
{
    char *end = NULL;
    unsigned long value;
    if (!text || !*text || text[0] == '-') return false;
    errno = 0;
    value = strtoul(text, &end, 10);
    if (errno || !end || *end || value > maximum) return false;
    *result = (unsigned)value;
    return true;
}

int main(int argc, char **argv)
{
    const char *command = option(argc, argv, "--command");
    const char *input = option(argc, argv, "--input");
    const char *output = option(argc, argv, "--output");
    const char *layer_text = option(argc, argv, "--layer");
    const char *dry_text = option(argc, argv, "--dry-run");
    NativeMap *map = calloc(1, sizeof(*map));
    char error[192] = {0};
    unsigned layer = 0, x = 0, y = 0, tile = 0;
    bool dry_run = dry_text && !strcmp(dry_text, "true");
    int result = 4;

    if (!map) {
        fputs("native map allocation failed\n", stderr);
        return 4;
    }
    if (!command || !input || !native_map_load(map, input, error, sizeof(error))) {
        fprintf(stderr, "%s\n", *error ? error : "invalid native map command/input");
        goto done;
    }
    if (!strcmp(command, "inspect")) {
        printf("MAP\t%s\t%u\t%u\t%u\t%u\t%u\t%u\n", map->room_id,
               map->tile_count, map->width[0], map->height[0],
               map->width[1], map->height[1], map->tileset);
        result = 0;
        goto done;
    }
    if (!layer_text || (strcmp(layer_text, "bg1") && strcmp(layer_text, "bg2"))) {
        fputs("invalid native map layer\n", stderr);
        goto done;
    }
    layer = !strcmp(layer_text, "bg2") ? 1u : 0u;
    if (!number(option(argc, argv, "--x"), NATIVE_MAX_DIMENSION - 1, &x) ||
        !number(option(argc, argv, "--y"), NATIVE_MAX_DIMENSION - 1, &y) ||
        x >= map->width[layer] || y >= map->height[layer]) {
        fputs("invalid native map layer or coordinates\n", stderr);
        goto done;
    }
    if (!strcmp(command, "get")) {
        unsigned picked = 0;
        (void)native_map_edit(map, layer, (int)x, (int)y, 3, 0, &picked);
        printf("VALUE\t%u\n", picked);
        result = 0;
        goto done;
    }
    if ((!strcmp(command, "set") || !strcmp(command, "fill")) &&
        number(option(argc, argv, "--tile"), NATIVE_MAX_TILES - 1, &tile) &&
        tile < map->tile_count) {
        bool changed = native_map_edit(map, layer, (int)x, (int)y,
                                       !strcmp(command, "fill") ? 2 : 0,
                                       tile, NULL);
        if (dry_text && strcmp(dry_text, "true") && strcmp(dry_text, "false")) {
            fputs("dry-run must be true or false\n", stderr);
            goto done;
        }
        if (!dry_run && changed) {
            if (!output || !native_map_save(map, output, error, sizeof(error))) {
                fprintf(stderr, "%s\n", *error ? error : "native map output required");
                goto done;
            }
        }
        printf("CHANGED\t%u\n", changed ? 1u : 0u);
        result = 0;
        goto done;
    }
    fputs("unknown or invalid native map operation\n", stderr);
done:
    free(map);
    return result;
}
