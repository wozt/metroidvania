/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free tests of the native Samus and projectile composer: the selection
 * rules on the generated decompilation tables, the native sign and mirror
 * rules on synthetic data, and the derived text tables. */
#include "../src/assets/mzm/samus.c"

#include <assert.h>

static const Variant *find_variant(const Variant *variants, size_t count,
                                   size_t family, const char *table,
                                   const char *selector, size_t side) {
    for (size_t index = 0; index < count; ++index) {
        const Variant *variant = &variants[index];
        if (variant->family == family && !strcmp(variant->table, table) &&
            !strcmp(variant->selector, selector) && variant->side == side)
            return variant;
    }
    return NULL;
}

/* The SamusPose enumerators of include/constants/samus.h. */
static size_t native_poses(char poses[][64], size_t capacity) {
    FILE *file = fopen(FUSION_SOURCE_DIR "/third_party/mzm/include/constants/samus.h", "rb");
    assert(file);
    static char text[1 << 20];
    size_t size = fread(text, 1, sizeof text - 1, file);
    fclose(file);
    text[size] = '\0';
    char *cursor = strstr(text, "MAKE_ENUM(u8, SamusPose) {");
    assert(cursor);
    char *end = strstr(cursor, "SPOSE_COUNT");
    assert(end);
    size_t count = 0;
    while ((cursor = strstr(cursor, "SPOSE_")) && cursor < end) {
        size_t length = 0;
        while ((cursor[length] >= 'A' && cursor[length] <= 'Z') ||
               (cursor[length] >= '0' && cursor[length] <= '9') || cursor[length] == '_')
            ++length;
        assert(count < capacity && length < 64);
        memcpy(poses[count], cursor, length);
        poses[count++][length] = '\0';
        cursor += length;
    }
    return count;
}

static void test_variants_follow_the_native_selection(void) {
    Variant *variants = NULL;
    size_t count = 0;
    char error[256] = "";
    assert(build_variants(&variants, &count, error, sizeof error));
    assert(count == 832);

    /* Every real pose has a pose-table variant for each family. */
    static char poses[128][64];
    size_t pose_count = native_poses(poses, 128);
    assert(pose_count == 67);
    for (size_t pose = 0; pose < pose_count; ++pose)
        assert(find_variant(variants, count, 0, "pose", poses[pose], 0));
    size_t exported = 0;
    for (size_t index = 0; index < count; ++index)
        if (variants[index].family == 0 && !strcmp(variants[index].table, "pose") &&
            variants[index].side == 0)
            ++exported;
    assert(exported == pose_count);

    /* Running right uses the Standing cannon graphics, left the default. */
    const Variant *running = find_variant(variants, count, 0, "Running",
                                          "ACD_DIAGONALLY_UP", 0);
    assert(running && !strcmp(running->cannon,
                              "sArmCannonAnim_Suit_Right_DiagonalUp_Running"));
    assert(running->has_gfx &&
           !strcmp(running->gfx[0], "sArmCannonGfx_Upper_DiagonalUp_Standing") &&
           !strcmp(running->gfx[1], "sArmCannonGfx_Lower_DiagonalUp_Standing"));
    assert(running->has_armed &&
           !strcmp(running->armed[0], "sArmCannonGfx_Upper_DiagonalUp_Armed_Standing"));
    running = find_variant(variants, count, 0, "Running", "ACD_DIAGONALLY_UP", 1);
    assert(running && !strcmp(running->gfx[0],
                              "sArmCannonGfx_Upper_DiagonalUp_Left_Default"));
    /* Pose tables pair with ..._All[pose]; non-ACD selectors use forward. */
    const Variant *spinning = find_variant(variants, count, 0, "pose", "SPOSE_SPINNING", 0);
    assert(spinning && !strcmp(spinning->cannon, "sArmCannonAnim_Suit_Right_Spinning"));
    assert(!strcmp(spinning->gfx[0], "sArmCannonGfx_Upper_Forward_Right_Default"));
    const Variant *skidding = find_variant(variants, count, 0, "Skidding", "FALSE", 1);
    assert(skidding && !strcmp(skidding->cannon, "sArmCannonAnim_Suit_Left_Skidding"));
    const Variant *hanging = find_variant(variants, count, 0, "AimingWhileHanging",
                                          "ACD_UP", 0);
    /* The source reuses the zipline graphics for this hanging row. */
    assert(hanging && !strcmp(hanging->gfx[0], "sArmCannonGfx_Upper_Up_Right_OnZipline"));
    hanging = find_variant(variants, count, 0, "AimingWhileHanging", "ACD_UP", 1);
    assert(hanging && !strcmp(hanging->gfx[0], "sArmCannonGfx_Upper_Up_Left_Hanging"));
    /* Differently spelled selectors with equal values still pair. */
    const Variant *crawling = find_variant(variants, count, 2, "CrawlingStopped",
                                           "TRUE", 0);
    assert(crawling && !strcmp(crawling->cannon,
                               "sArmCannonAnim_Suitless_Right_PistolUp_CrawlingStopped"));
    assert(!crawling->has_armed);
    /* The gunship saving animation of each family closes its variants. */
    const Variant *ship = find_variant(variants, count, 1, "InShip",
                                       "SPOSE_SAVING_LOADING_GAME", 1);
    assert(ship && !strcmp(ship->body, "sSamusAnim_27f430"));
    free(variants);
}

static void test_muzzle_offsets_use_native_sign_rules(void) {
    uint8_t data[0x400] = {0};
    data[0x100] = 0x00; data[0x101] = 0x02; data[0x102] = 0x00; data[0x103] = 0x08;
    /* y=0x00E5 (-27 + 1), x=0x01EE (-18). */
    data[0x200] = 0xE5; data[0x201] = 0x00; data[0x202] = 0xEE; data[0x203] = 0x01;
    FusionRom rom = {data, sizeof data, FUSION_ASSET_GAME_METROID};
    FusionMzmRomSymbol cannon = {"cannon", 0x08000100, 8, 0};
    const FusionMzmRomSymbol *by_name[] = {&cannon};
    char error[128] = "";
    Context context = {&rom, by_name, 1, error, sizeof error};
    int x = 0, y = 0;
    assert(muzzle_offset(&context, "cannon", 0, &x, &y));
    assert(x == -18 && y == -26);
    assert(!muzzle_offset(&context, "missing", 0, &x, &y));
    assert(strstr(error, "missing ELF symbol"));
}

static void test_projectiles_mirror_around_their_position(void) {
    uint8_t data[0x400] = {0};
    /* One 8x8 OBJ at x=-6, y=-4, tile 0x80 (OBJ offset 0x1000), bank 2. */
    const uint8_t oam[] = {1, 0, 0xFC, 0, 0xFA, 0x01, 0x80, 0x20};
    memcpy(data + 0x200, oam, sizeof oam);
    FusionRom rom = {data, sizeof data, FUSION_ASSET_GAME_METROID};
    char error[128] = "";
    Context context = {&rom, NULL, 0, error, sizeof error};
    static ShotStage stage;
    static uint8_t canvas[SHOT_CANVAS_WIDTH * SHOT_CANVAS_HEIGHT * 4];
    stage.vram[0x1000] = 0x01;
    stage.loaded[33] = true;
    memcpy(stage.rgb[33], (uint8_t[]){10, 20, 30}, 3);
    static const int expected[4][2] = {{-6, -4}, {5, -4}, {-6, 3}, {5, 3}};
    for (size_t flip = 0; flip < 4; ++flip) {
        assert(render_shot(&context, &stage, 0x08000200, flip, canvas));
        const uint8_t *pixel = canvas +
            ((size_t)(expected[flip][1] + SHOT_ORIGIN_Y) * SHOT_CANVAS_WIDTH +
             (size_t)(expected[flip][0] + SHOT_ORIGIN_X)) * 4;
        assert(pixel[0] == 10 && pixel[1] == 20 && pixel[2] == 30 && pixel[3] == 255);
    }
    stage.loaded[33] = false;
    assert(!render_shot(&context, &stage, 0x08000200, 0, canvas));
    assert(strstr(error, "unloaded palette"));
    assert(beam_set_of("sChargedWaveBeamOam_Diagonal") == 3);
    assert(beam_set_of("sPlasmaBeamOam_Vertical_Wave") == 4);
    assert(beam_set_of("sMissileOam_Vertical") == 0);
}

static void test_tables_and_offsets(void) {
    /* Particles and unused data are not projectile tables. */
    for (size_t index = 0; index < fusion_mzm_projectile_frame_table_count; ++index) {
        assert(!starts_with(fusion_mzm_projectile_frame_tables[index], "sParticle"));
        assert(!strstr(fusion_mzm_projectile_frame_tables[index], "Unused"));
    }
    assert(fusion_mzm_projectile_frame_table_count == 54);

    static SamusSequence sequences[3];
    snprintf(sequences[0].key, sizeof sequences[0].key, "PowerSuit/Standing/ACD_FORWARD/right");
    snprintf(sequences[1].key, sizeof sequences[1].key,
             "PowerSuit/Standing/ACD_FORWARD/right/armed");
    snprintf(sequences[2].key, sizeof sequences[2].key, "PowerSuit/pose/SPOSE_MORPH_BALL/right");
    for (size_t index = 0; index < 2; ++index) {
        sequences[index].frames = 1;
        sequences[index].has_muzzle = true;
        sequences[index].muzzle[0][0] = 18;
        sequences[index].muzzle[0][1] = -26;
    }
    sequences[2].frames = 1;
    const SamusSequence *sorted[] = {&sequences[0], &sequences[1], &sequences[2]};
    Text text = {0};
    assert(write_cannon_offsets(&text, sorted, 3));
    assert(!strcmp(text.data, "schema\tmetroidvania-samus-cannon-offsets-v1\n"
                              "PowerSuit/Standing/ACD_FORWARD/right\t0\t18\t-26\n"));
    free(text.data);
}

int main(void) {
    test_variants_follow_the_native_selection();
    test_muzzle_offsets_use_native_sign_rules();
    test_projectiles_mirror_around_their_position();
    test_tables_and_offsets();
    puts("fusion_mzm_samus_assets tests passed");
    return 0;
}
