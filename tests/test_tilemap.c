/* SPDX-License-Identifier: GPL-3.0-only */
#include "core/tilemap.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    FusionTilemap m = {0}, copy = {0}, unchanged = {0};
    char error[160] = {0};
    uint8_t color[4];
    FILE *file;
    const char *tmp = "tilemap-test-output.mvroom";
    assert(fusion_tilemap_load(&m, "data/maps/metroid_demo_save.mvroom", error, sizeof(error)));
    assert(!strcmp(m.room_id, "metroid:demo:save_01"));
    assert(m.world == WORLD_METROID);
    assert(m.tiles[FUSION_LAYER_TERRAIN][16][4] != 0);
    assert(fusion_tilemap_valid(&m));
    assert(fusion_tile_color(WORLD_METROID, FUSION_LAYER_BACK, 0, color) && color[3] == 0);
    assert(fusion_tile_color(WORLD_CASTLEVANIA, FUSION_LAYER_FRONT, 4, color) && color[3] == 255);
    assert(!fusion_tile_color(WORLD_METROID, 4, 1, color));
    assert(!fusion_tile_color(WORLD_METROID, 1, 5, color));
    assert(fusion_tilemap_save(&m, tmp, error, sizeof(error)));
    assert(fusion_tilemap_load(&copy, tmp, error, sizeof(error)));
    assert(memcmp(&m, &copy, sizeof(m)) == 0);
    unchanged = copy;
    file = fopen(tmp, "a");
    assert(file);
    assert(fputs("EXTRA\n", file) >= 0);
    assert(fclose(file) == 0);
    assert(!fusion_tilemap_load(&copy, tmp, error, sizeof(error)));
    assert(memcmp(&copy, &unchanged, sizeof(copy)) == 0);
    remove(tmp);
    m.tiles[0][0][0] = 5;
    assert(!fusion_tilemap_valid(&m));
    assert(!fusion_tilemap_save(&m, tmp, error, sizeof(error)));
    assert(fusion_tilemap_load(&copy, "data/maps/aria_demo_save.mvroom", error, sizeof(error)));
    assert(copy.world == WORLD_CASTLEVANIA);
    puts("Tilemap load/save, strict parsing, palette and invalid-data tests passed.");
    return 0;
}
