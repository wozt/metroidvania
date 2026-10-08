#include "core/tile_paint.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    FusionTilemap map = {0};
    unsigned picked = 99;
    strcpy(map.room_id, "metroid:demo:save_01");
    map.world = WORLD_METROID;
    assert(fusion_tilemap_valid(&map));
    assert(fusion_tile_paint(&map, 1, 2, 3, FUSION_PAINT_PENCIL, 2, NULL));
    assert(map.tiles[1][3][2] == 2);
    assert(!fusion_tile_paint(&map, 1, 2, 3, FUSION_PAINT_PENCIL, 2, NULL));
    assert(!fusion_tile_paint(&map, 1, 2, 3, FUSION_PAINT_PICK, 0, &picked));
    assert(picked == 2);
    assert(fusion_tile_stroke(&map, 1, 0, 0, 9, 9, FUSION_PAINT_PENCIL, 3));
    for (int i = 0; i < 10; ++i) assert(map.tiles[1][i][i] == 3);
    assert(fusion_tile_paint(&map, 1, 0, 0, FUSION_PAINT_FILL, 4, NULL));
    assert(map.tiles[1][0][0] == 4);
    assert(map.tiles[1][1][1] == 3); /* Flood does not cross other colors. */
    assert(!fusion_tile_paint(&map, 1, -1, 0, FUSION_PAINT_FILL, 4, NULL));
    assert(!fusion_tile_paint(&map, 3, 0, 0, FUSION_PAINT_PENCIL, 1, NULL));
    assert(!fusion_tile_paint(&map, 1, 0, 0, FUSION_PAINT_PENCIL, 50, NULL));
    assert(fusion_tile_paint(&map, 1, 0, 0, FUSION_PAINT_ERASER, 1, NULL));
    assert(map.tiles[1][0][0] == 0);
    assert(map.tiles[0][0][0] == 0);
    puts("Tile paint pencil/line/fill/eraser/pick bounds passed.");
    return 0;
}
