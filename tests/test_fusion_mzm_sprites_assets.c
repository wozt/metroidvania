/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free tests of the native Zero Mission sprite exporter: the generated
 * decompilation tables, the palette variants, the native VRAM copies and the
 * OAM bounds and mirror rules on synthetic data. */
#include "../src/assets/mzm/sprites.c"

#include <assert.h>
#ifndef _WIN32
#include <dirent.h>
#endif

static unsigned primary_id(const char *name) {
    for (size_t index = 0; index < fusion_mzm_primary_sprite_enum_count; ++index)
        if (!strcmp(fusion_mzm_primary_sprite_enum[index].name, name))
            return fusion_mzm_primary_sprite_enum[index].value;
    assert(!"unknown primary sprite");
    return 0;
}

static void test_generated_tables_match_the_runtime(void) {
    assert(primary_id("UNUSED16") == 16);
    assert(primary_id("ZOOMER_YELLOW") == 18 && primary_id("ZOOMER_RED") == 19);
    assert(primary_id("LARGE_ENERGY_DROP") == 26 && primary_id("SMALL_ENERGY_DROP") == 27);
    assert(primary_id("MISSILE_DROP") == 28 && primary_id("SUPER_MISSILE_DROP") == 29);
    assert(primary_id("POWER_BOMB_DROP") == 30 && primary_id("MULTIPLE_LARGE_ENERGY") == 53);
    assert(primary_id("DESSGEEGA") == 70 && primary_id("DESSGEEGA_AFTER_LONG_BEAM") == 71);
    assert(primary_id("SIDEHOPPER") == 121);
    const FusionMzmSpritePlan *zoomer = find_plan("ZOOMER_RED");
    assert(zoomer && zoomer->row == 1 && !strcmp(zoomer->graphics, "sZoomerGfx"));
    assert(!strcmp(find_plan("ZOOMER_YELLOW")->palette, "sZoomerPal"));
    assert(zoomer->table_count == 9);
    bool on_wall = false;
    for (size_t index = 0; index < zoomer->table_count; ++index)
        if (!strcmp(zoomer->tables[index], "sZoomerOam_OnWall")) on_wall = true;
    assert(on_wall);
    assert(find_plan("SIDEHOPPER") && find_plan("DESSGEEGA") &&
           find_plan("DESSGEEGA_AFTER_LONG_BEAM") && find_plan("MECHA_RIDLEY") &&
           find_plan("AREA_BANNER"));
    assert(fusion_mzm_banner_table_count == 7);
    assert(fusion_mzm_message_enum_count == 39);
}

#ifndef _WIN32
/* Every FrameData table of the generated plan is still defined by the
 * decompilation's sprite data files. */
static void test_plan_tables_exist_in_the_decompilation(void) {
    static char sources[1 << 24];
    size_t used = 0;
    const char *folder = FUSION_SOURCE_DIR "/third_party/mzm/src/data/sprites";
    DIR *directory = opendir(folder);
    assert(directory);
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        size_t length = strlen(entry->d_name);
        if (length < 3 || strcmp(entry->d_name + length - 2, ".c")) continue;
        char path[1024];
        snprintf(path, sizeof path, "%s/%s", folder, entry->d_name);
        FILE *file = fopen(path, "rb");
        assert(file);
        used += fread(sources + used, 1, sizeof sources - 1 - used, file);
        fclose(file);
    }
    closedir(directory);
    sources[used] = '\0';
    for (size_t sprite = 0; sprite < fusion_mzm_sprite_plan_count; ++sprite) {
        const FusionMzmSpritePlan *plan = &fusion_mzm_sprite_plan[sprite];
        for (size_t table = 0; table < plan->table_count; ++table) {
            char needle[160];
            snprintf(needle, sizeof needle, "FrameData %s[", plan->tables[table]);
            assert(strstr(sources, needle));
        }
    }
}
#endif

static void test_frozen_and_stun_rows_shift_the_sprite_rows(void) {
    static Palette palette, common, result;
    for (size_t slot = 128; slot < 256; ++slot) {
        palette.loaded[slot] = true;
        palette.rgb[slot][0] = (uint8_t)slot;
    }
    for (size_t slot = 32; slot < 128; ++slot) {
        common.loaded[slot] = true;
        common.rgb[slot][1] = (uint8_t)slot;
    }
    /* A row 8 part shows OBJ row 7 (frozen) or 6 (stun): common rows. */
    shifted(&palette, &common, 15, &result);
    assert(result.loaded[8 * 16 + 3] && result.rgb[8 * 16 + 3][1] == 7 * 16 + 3);
    /* A row 9 part shows row 8 when frozen. */
    assert(result.rgb[9 * 16][0] == 8 * 16 && result.rgb[9 * 16][1] == 0);
    shifted(&palette, &common, 14, &result);
    assert(result.rgb[8 * 16 + 3][1] == 6 * 16 + 3 && result.rgb[9 * 16][1] == 7 * 16);
    /* Rows below 8 are untouched. */
    assert(!result.loaded[5]);
}

static void test_index_palette_shifts_rows_and_skips_unloaded_ones(void) {
    bool loaded[256] = {false};
    for (size_t slot = 128; slot < 256; ++slot) loaded[slot] = true;
    static Palette indices;
    index_palette(loaded, 0, &indices);
    assert(indices.indexed && indices.loaded[8 * 16 + 5] && indices.index[8 * 16 + 5] == 8 * 16 + 5);
    assert(!indices.loaded[5]);
    index_palette(loaded, 4, &indices);
    assert(indices.index[8 * 16 + 1] == 12 * 16 + 1);
    /* Bank 12 + 4 wraps to row 0, which Mecha Ridley does not load. */
    assert(!indices.loaded[12 * 16 + 1]);
}

static void test_bounds_follow_global_flips_and_affine_parts_ignore_y_flip(void) {
    uint8_t data[0x100] = {0};
    /* Two parts: 8x8 at (-5, -4) and 16x16 at (3, 6) with the Y flip set. */
    const uint8_t oam[] = {2, 0, 0xFC, 0x00, 0xFB, 0x01, 0x00, 0x00,
                           0x06, 0x00, 0x03, 0x60, 0x00, 0x00};
    memcpy(data + 0x10, oam, sizeof oam);
    FusionRom rom = {data, sizeof data, FUSION_ASSET_GAME_METROID};
    char error[128] = "";
    Context context = {&rom, NULL, 0, NULL, error, sizeof error};
    int box[4];
    size_t parts = 0;
    assert(object_bounds(&context, 0x08000010, 0, box, &parts));
    assert(parts == 2 && box[0] == -5 && box[1] == -4 && box[2] == 24 && box[3] == 26);
    assert(object_bounds(&context, 0x08000010, 1, box, &parts));
    assert(box[0] == -19 && box[1] == -4 && box[2] == 24 && box[3] == 26);
    assert(object_bounds(&context, 0x08000010, 2, box, &parts));
    assert(box[0] == -5 && box[1] == -22);

    static uint8_t vram[OBJ_VRAM_BYTES];
    vram[0] = 0x01;                 /* tile 0, pixel (0, 0) -> index 1 */
    static Palette palette;
    palette.loaded[1] = true;
    static uint8_t pixels[CANVAS_WIDTH * CANVAS_HEIGHT * 4];
    Canvas canvas = {pixels, 0, 0, 0, 0, false};
    assert(render_affine_part(&context, vram, &palette, 0x08000010, 1, &canvas, box));
    assert(box[0] == 3 && box[1] == 6 && box[2] == 16 && box[3] == 16);
    /* The Y flip is ignored: the pixel stays at the part's top-left. */
    assert(canvas.any && canvas.min_x == ORIGIN_X + 3 && canvas.min_y == ORIGIN_Y + 6);
    assert(!render_affine_part(&context, vram, &palette, 0x08000010, 2, &canvas, box));
    /* An empty frame is one black pixel at the origin. */
    canvas_clear(&canvas);
    Rendered rendered = {0};
    assert(canvas_bitmap(&context, &canvas, NULL, &rendered));
    assert(rendered.size == 126 && rendered.left == 0 && rendered.top == 0 &&
           rendered.bmp[125] == 255 && rendered.bmp[122] == 0);
    free(rendered.bmp);
}

static void test_native_vram_copies(void) {
    /* Mecha Ridley's destroyed graphics and weapon stages, and the Ruins
     * Test reflection ranges, stay inside OBJ VRAM. */
    for (size_t copy = 0; copy < COUNT(mecha_destroyed_copies); ++copy)
        assert(mecha_destroyed_copies[copy][1] + mecha_destroyed_copies[copy][2] <=
               OBJ_VRAM_BYTES);
    assert(mecha_destroyed_copies[6][0] == 1216 &&
           mecha_destroyed_copies[6][1] == 0x5580 + 6144 + 64);
    assert(mecha_weapon_stages[6] == 24);
    assert(MECHA_RIDLEY_WEAPON_VRAM + 3 * 1024 + 128 <= OBJ_VRAM_BYTES);
    assert(row_variant_applies("ATOMIC", "sAtomicOam_Idle"));
    assert(!row_variant_applies("ATOMIC", "sAtomicElectricityOam_Charging"));
    assert(row_variant_applies("KRAID", "sKraidOam_Anything"));
}

int main(void) {
    test_generated_tables_match_the_runtime();
#ifndef _WIN32
    test_plan_tables_exist_in_the_decompilation();
#endif
    test_frozen_and_stun_rows_shift_the_sprite_rows();
    test_index_palette_shifts_rows_and_skips_unloaded_ones();
    test_bounds_follow_global_flips_and_affine_parts_ignore_y_flip();
    test_native_vram_copies();
    puts("fusion_mzm_sprites_assets tests passed");
    return 0;
}
