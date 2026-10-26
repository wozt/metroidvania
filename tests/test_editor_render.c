/* SPDX-License-Identifier: GPL-3.0-only */
/* The editor's native room renderers and their codecs: LZ77, room RLE,
 * BMP/PNG encoding, annotation rows, Zero Mission metatile rendering, a
 * synthetic Aria room through the real loader, and a real-data smoke test
 * when the private extraction is present. */
#define _POSIX_C_SOURCE 200809L
#include "aria_render.h"
#include "audit.h"
#include "generated.h"
#include "mzm_render.h"
#include "mzm_workspace.h"
#include "world_overview.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

#ifndef FUSION_SOURCE_DIR
#error "FUSION_SOURCE_DIR must identify the repository source directory"
#endif

static void test_lz77(void) {
    /* "ABCABCAB": three literals, then a back-reference of five at distance 3. */
    const uint8_t stream[] = {0x10, 8, 0, 0, 0x10, 'A', 'B', 'C', 0x20, 0x02, 0, 0};
    uint8_t *out = NULL;
    size_t size = 0;
    FebError error = {0};
    assert(feb_lz77(stream, sizeof stream, 64, &out, &size, &error));
    assert(size == 8 && !memcmp(out, "ABCABCAB", 8));
    free(out);
    assert(!feb_lz77(stream, sizeof stream, 4, &out, &size, &error));
    assert(!strcmp(error.message, "invalid LZ77 expanded size"));
    const uint8_t trailing[] = {0x10, 1, 0, 0, 0, 'A', 7};
    assert(!feb_lz77(trailing, sizeof trailing, 64, &out, &size, &error));
    assert(!strcmp(error.message, "unexpected nonzero LZ77 trailing data"));
    const uint8_t backwards[] = {0x10, 4, 0, 0, 0x80, 0x10, 0x00};
    assert(!feb_lz77(backwards, sizeof backwards, 64, &out, &size, &error));
    assert(!strcmp(error.message, "LZ77 invalid backreference"));
    const uint8_t header[] = {0x11, 4, 0, 0, 0};
    assert(!feb_lz77(header, sizeof header, 64, &out, &size, &error));
    assert(!strcmp(error.message, "invalid 0x10 LZ77 header"));
}

static void test_room_rle(void) {
    /* 2x2 blocks: a low pass of one run, a high pass of literals. */
    const uint8_t data[] = {2, 2, 1, 0x84, 7, 0, 2, 0, 4, 1, 2, 3, 4, 0, 0};
    FebBlockMap map = {0};
    FebError error = {0};
    assert(feb_mzm_rle(data, sizeof data, &map, &error));
    assert(map.width == 2 && map.height == 2);
    assert(map.blocks[0] == 0x0107 && map.blocks[3] == 0x0407);
    feb_block_map_free(&map);
    const uint8_t short_pass[] = {2, 2, 1, 0x83, 7, 0, 1, 0x84, 1, 0};
    assert(!feb_mzm_rle(short_pass, sizeof short_pass, &map, &error));
    assert(!strcmp(error.message, "room RLE pass length 3 != 4"));
    const uint8_t overflow[] = {1, 1, 1, 0x85, 7, 0, 1, 0x81, 1, 0};
    assert(!feb_mzm_rle(overflow, sizeof overflow, &map, &error));
    assert(!strcmp(error.message, "room RLE pass overflow"));
    const uint8_t width[] = {1, 1, 3, 0, 0, 0};
    assert(!feb_mzm_rle(width, sizeof width, &map, &error));
    assert(!strcmp(error.message, "invalid room RLE pass width"));
}

static void test_bitmaps(void) {
    const uint8_t rgb[] = {10, 20, 30, 40, 50, 60};  /* 1x2: top, bottom */
    uint8_t *bmp = NULL;
    size_t size = 0;
    FebError error = {0};
    assert(feb_bmp24(1, 2, rgb, &bmp, &size, &error));
    assert(size == 54 + 8 && bmp[0] == 'B' && bmp[1] == 'M' && bmp[10] == 54);
    /* Bottom-up rows of BGR pixels, each padded to four bytes. */
    assert(bmp[54] == 60 && bmp[55] == 50 && bmp[56] == 40);
    assert(bmp[58] == 30 && bmp[59] == 20 && bmp[60] == 10);
    free(bmp);
    assert(!feb_bmp24(0, 2, rgb, &bmp, &size, &error));
    assert(!strcmp(error.message, "invalid RGB bitmap"));

    uint8_t pixels[3 * 2 * 4];
    for (size_t index = 0; index < sizeof pixels; ++index) pixels[index] = (uint8_t)index;
    uint8_t *png = NULL;
    assert(feb_png_rgba(3, 2, pixels, &png, &size));
    assert(!memcmp(png, "\x89PNG\r\n\x1a\n", 8) && !memcmp(png + 12, "IHDR", 4));
    assert(png[19] == 3 && png[23] == 2 && png[24] == 8 && png[25] == 6);
    uint32_t length = (uint32_t)png[33] << 24 | (uint32_t)png[34] << 16 | png[35] << 8 | png[36];
    assert(!memcmp(png + 37, "IDAT", 4));
    uint8_t raw[2 * (1 + 12)];
    uLongf raw_size = sizeof raw;
    assert(uncompress(raw, &raw_size, png + 41, length) == Z_OK && raw_size == sizeof raw);
    assert(raw[0] == 0 && raw[1] == 0 && raw[13] == 0 && raw[14] == 12);
    free(png);
}

static void test_annotation_rows(void) {
    char long_label[400];
    memset(long_label, 'x', sizeof long_label - 1);
    long_label[sizeof long_label - 1] = '\0';
    const FebAnnotation rows[] = {
        {"ENEMY", 0, 16, 32, 16, 16, "native", "PSPRITE_ZOOMER", "Zoomer", "a\tb\nc"},
        {"DOOR", 3, 0, 0, 16, 64, "native", "3", long_label, NULL}};
    char *text = NULL;
    size_t size = 0;
    FebError error = {0};
    assert(feb_annotations_serialize(rows, 2, &text, &size, &error));
    assert(!strncmp(text, "# MV_ROOM_ANNOTATIONS_1\n# kind|index|", 37));
    assert(strstr(text, "ENEMY|0|16|32|16|16|native|PSPRITE_ZOOMER|Zoomer|a b c\n"));
    /* Every field is capped at 300 code points. */
    const char *door = strstr(text, "DOOR|3|");
    assert(door && strlen(door) == strlen("DOOR|3|0|0|16|64|native|3|") + 300 + 2);
    free(text);
}

static void test_metatile_rendering(void) {
    /* One 4bpp tile whose left half is index 1 and right half index 0. */
    uint8_t gfx[32];
    for (int row = 0; row < 8; ++row) {
        gfx[row * 4] = 0x11;
        gfx[row * 4 + 1] = 0x11;
        gfx[row * 4 + 2] = 0;
        gfx[row * 4 + 3] = 0;
    }
    uint8_t palette[64] = {0};
    palette[(16 + 1) * 2] = 0x1F;  /* source row 1 color 1: red, BG row 3 */
    uint16_t table[2][4] = {
        {3u << 12 | 192, 3u << 12 | 0x400 | 192, 2u << 12 | 192, 3u << 12 | 193},
        {3u << 12 | 192, 3u << 12 | 192, 3u << 12 | 192, 3u << 12 | 192}};
    FebMzmTileset tileset = {gfx, sizeof gfx, palette, sizeof palette, table, 2};
    FebBlockMap bg1 = {1, 1, (uint16_t[]){0}};
    int64_t base = 0, coverage = 0;
    FebError error = {0};
    assert(feb_mzm_graphic_base(&tileset, &bg1, &base, &coverage, &error));
    assert(base == 192 && coverage == 3);
    uint8_t *rgb = NULL, visibility[16 * 16] = {0};
    int64_t unresolved = 0, painted = 0;
    const uint16_t blocks[] = {0, 5};
    assert(feb_mzm_render_layer(2, 1, blocks, &tileset, base, &rgb, visibility, &unresolved,
                                &painted, &error));
    /* Cell 2 uses a common palette row and cell 3 an absent tile; block 5 is
     * outside the table: none of them is invented. */
    assert(unresolved == 3 && painted == 128);
    assert(rgb[0] == 255 && rgb[1] == 0 && visibility[0] == 1 && visibility[4] == 0);
    /* The horizontally flipped cell draws its opaque half on the right. */
    assert(visibility[15] == 1 && visibility[8] == 0);
    free(rgb);
}

/* A synthetic Aria ROM: one 4bpp background through the real loader. */
enum { ROM_SIZE = 0x10000 };

static void put16(uint8_t *data, size_t offset, unsigned value) {
    data[offset] = (uint8_t)value;
    data[offset + 1] = (uint8_t)(value >> 8);
}

static void put32(uint8_t *data, size_t offset, uint32_t value) {
    put16(data, offset, value & 0xFFFF);
    put16(data, offset + 2, value >> 16);
}

static FusionJson parse(const char *text) {
    FusionJson value;
    char error[128];
    assert(fusion_json_parse(text, strlen(text), &value, error, sizeof error));
    return value;
}

static void synthetic_rom(uint8_t *rom, bool compressed_graphics) {
    memset(rom, 0, ROM_SIZE);
    if (compressed_graphics) {
        /* Encoding 1: an LZ77 stream whose expansion starts with a zero word. */
        rom[0x100] = 1;
        put32(rom, 0x104, 0x08000400);
        size_t at = 0x400;
        rom[at++] = 0x10;
        rom[at++] = 0x04;
        rom[at++] = 0x08;
        rom[at++] = 0;
        /* 0x804 bytes: 4 zero header bytes, tile 0 zero, tile 1 all 0x11. */
        size_t produced = 0;
        while (produced < 0x804) {
            uint8_t flags = 0;
            rom[at++] = flags;
            for (int bit = 0; bit < 8 && produced < 0x804; ++bit, ++produced)
                rom[at++] = produced >= 4 + 32 && produced < 4 + 64 ? 0x11 : 0;
        }
    } else {
        rom[0x100] = 0;
        memset(rom + 0x104 + 32, 0x11, 32);
    }
    /* Palette: final row index 1, row 0 color 1 red. */
    rom[0x1000] = 0;
    rom[0x1002] = 1;
    put16(rom, 0x1004 + 2, 0x001F);
    /* Metadata: 1x1 screens, no flags, blocks, collision and map. */
    rom[0x2000] = 1;
    rom[0x2001] = 1;
    put32(rom, 0x2004, 0x08002100);
    put32(rom, 0x2008, 0x08002200);
    put32(rom, 0x200C, 0x08002300);
    for (int cell = 0; cell < 16; ++cell) put16(rom, 0x2100 + cell * 2, 1);
    memset(rom + 0x2200, 2, 16);
    for (int entry = 0; entry < 64; ++entry) put16(rom, 0x2300 + entry * 2, 1);
}

static const char room_template[] =
    "{\"format\":\"MV_AOS_WORLD_2\",\"source_revision\":\"synthetic\",\"rooms\":[{"
    "\"engine_area\":0,\"area\":\"Castle Corridor\",\"room\":0,\"source_pointer\":\"0x08000000\","
    "\"map_x\":1,\"map_y\":2,\"entities\":[],\"transitions\":[],"
    "\"graphics_loads\":[{\"entry_pointer\":\"0x08000010\",\"resource_pointer\":\"0x08000100\","
    "\"parameters\":[0,0,1]}],"
    "\"palette_loads\":[{\"entry_pointer\":\"0x08000020\",\"resource_pointer\":\"0x08001000\","
    "\"parameters\":[0,%d,%d]}],"
    "\"backgrounds\":[{\"layer\":1,\"metadata_pointer\":%s,\"control\":%d,\"field_0\":0},"
    "{\"layer\":2,\"metadata_pointer\":%s,\"control\":0,\"field_0\":1},"
    "{\"layer\":3,\"metadata_pointer\":null,\"control\":0,\"field_0\":2}]}]}";

static bool render_synthetic(uint8_t *rom, int source_row, int rows, const char *bg1,
                             int control, const char *bg2, FusionJson *result, FebError *error) {
    char text[sizeof room_template + 128];
    snprintf(text, sizeof text, room_template, source_row, rows, bg1, control, bg2);
    FusionJson world = parse(text);
    FusionRom loaded = {rom, ROM_SIZE, FUSION_ASSET_GAME_ARIA};
    bool ok = feb_aria_render_loaded("/nonexistent", &loaded, &world, 0, 0, false, result, error);
    fusion_json_free(&world);
    return ok;
}

static void test_synthetic_aria_room(void) {
    uint8_t *rom = malloc(ROM_SIZE);
    assert(rom);
    for (int compressed = 0; compressed < 2; ++compressed) {
        synthetic_rom(rom, compressed);
        FusionJson result;
        FebError error = {0};
        assert(render_synthetic(rom, 0, 2, "\"0x08002000\"", 0, "null", &result, &error));
        assert(fusion_json_member(&result, "width")->integer == 256);
        assert(fusion_json_member(&result, "height")->integer == 256);
        assert(fusion_json_member(&result, "collision_path")->kind == FUSION_JSON_STRING);
        assert(!fusion_json_member(&result, "outputs_written")->boolean);
        const FusionJson *layers = fusion_json_member(&result, "layers");
        assert(layers->count == 3);
        const FusionJson *first = &layers->items[0];
        assert(!strcmp(fusion_json_member(first, "status")->string, "DECODED_TEXT_BACKGROUND"));
        assert(fusion_json_member(first, "unresolved_tiles_or_pixels")->integer == 0);
        assert(!strcmp(fusion_json_member(&layers->items[1], "status")->string, "ABSENT"));
        const FusionJson *loads = fusion_json_member(&result, "loads");
        const FusionJson *resource = fusion_json_member(&loads->items[0], "resource");
        assert(fusion_json_member(resource, "encoding")->integer == compressed);
        fusion_json_free(&result);
    }
    /* The palette header stores the final valid row index: rows 0..1 only. */
    FusionJson result;
    FebError error = {0};
    assert(render_synthetic(rom, 1, 1, "\"0x08002000\"", 0, "null", &result, &error));
    fusion_json_free(&result);
    assert(!render_synthetic(rom, 1, 2, "\"0x08002000\"", 0, "null", &result, &error));
    assert(!strcmp(error.message, "palette load exceeds bounds at 0x08000020"));
    assert(!render_synthetic(rom, 0, 2, "\"0x08002000\"", 0x80, "null", &result, &error));
    assert(!strcmp(error.message, "8 bpp text backgrounds are not supported"));
    /* Without a text BG1 the composite uses the first available layer. */
    assert(render_synthetic(rom, 0, 2, "null", 0, "\"0x08002000\"", &result, &error));
    assert(fusion_json_member(&result, "width")->integer == 256);
    assert(fusion_json_member(&result, "collision_path")->kind == FUSION_JSON_NULL);
    fusion_json_free(&result);
    assert(!render_synthetic(rom, 0, 2, "null", 0, "null", &result, &error));
    assert(!strcmp(error.message, "room has no supported text background layers"));
    rom[0x2000] = 17;
    assert(!render_synthetic(rom, 0, 2, "\"0x08002000\"", 0, "null", &result, &error));
    assert(!strcmp(error.message, "invalid background dimensions at 0x08002000"));
    free(rom);
}

/* write_generated(): plain components only, never through a symlink. */
static void test_generated_writer(void) {
    char root[] = "/tmp/fusion-generated-XXXXXX", path[4096];
    assert(mkdtemp(root));
    snprintf(path, sizeof path, "%s/assets", root);
    assert(!mkdir(path, 0777));
    snprintf(path, sizeof path, "%s/assets/extracted", root);
    assert(!mkdir(path, 0777));
    char target[4096], link[4096];
    snprintf(target, sizeof target, "%s/elsewhere", root);
    snprintf(link, sizeof link, "%s/assets/extracted/metroid", root);
    assert(!symlink(target, link));
    FebError error = {0};
    assert(!feb_write_generated(root, "metroid/raw/data/block.bin", (const uint8_t *)"secret", 6,
                                NULL, &error));
    assert(strstr(error.message, "symlink extraction destination refused"));
    struct stat status;
    assert(stat(target, &status) != 0);
    static const char *const unsafe[] = {"../escape.bin", "/tmp/leak.bin", "folder/../x", "a\\b",
                                         "bad name.bin"};
    for (size_t index = 0; index < sizeof unsafe / sizeof *unsafe; ++index)
        assert(!feb_generated_path(root, unsafe[index], path, &error));
    assert(feb_generated_path(root, "room/data_123.bin", path, &error));
    bool written = false;
    assert(feb_write_generated(root, "room/data_123.bin", (const uint8_t *)"abc", 3, &written,
                               &error) && written);
    assert(feb_write_generated(root, "room/data_123.bin", (const uint8_t *)"abc", 3, &written,
                               &error) && !written);
    char command[4200];
    snprintf(command, sizeof command, "rm -rf '%s'", root);
    assert(system(command) == 0);
}

static void test_world_overview_arguments(void) {
    char *budget[] = {"fusion_world_overview", "--world", "mzm", "--budget", "40", NULL};
    assert(feb_world_overview_main(5, budget, "/nonexistent") == 2);
    char *area[] = {"fusion_world_overview", "--world=aria", "--area=12", NULL};
    assert(feb_world_overview_main(3, area, "/nonexistent") == 2);
    char *world[] = {"fusion_world_overview", "--world", "moon", NULL};
    assert(feb_world_overview_main(3, world, "/nonexistent") == 2);
    char *missing[] = {"fusion_world_overview", NULL};
    assert(feb_world_overview_main(1, missing, "/nonexistent") == 2);
}

static bool exists(const char *relative) {
    char path[4096];
    struct stat status;
    snprintf(path, sizeof path, "%s/%s", FUSION_SOURCE_DIR, relative);
    return stat(path, &status) == 0;
}

/* The private extraction, when present: no file is written. */
static void test_real_rooms(void) {
    if (!exists("assets/extracted/metroid/raw/data/tilesets") ||
        !exists("roms/Castlevania - Aria of Sorrow (USA).gba")) {
        puts("real-data room checks skipped: private extraction absent");
        return;
    }
    FusionJson result;
    FebError error = {0};
    assert(feb_mzm_decode_room(FUSION_SOURCE_DIR, "brinstar", 5, false, &result, &error));
    assert(!strcmp(fusion_json_member(&result, "room_id")->string, "mzm:brinstar:005"));
    assert(fusion_json_member(&result, "tileset")->integer == 20);
    assert(!strcmp(fusion_json_member(fusion_json_member(&result, "composite"), "status")->string,
                   "PARTIAL_BG1_OVER_BG2_DIAGNOSTIC"));
    fusion_json_free(&result);
    assert(!feb_mzm_decode_room(FUSION_SOURCE_DIR, "Brinstar", 999, false, &result, &error));
    assert(!strcmp(error.message, "unknown Zero Mission room Brinstar:999"));
    assert(feb_aria_render_room(FUSION_SOURCE_DIR, 1, 3, false, &result, &error));
    assert(!strcmp(fusion_json_member(&result, "area")->string, "Chapel"));
    assert(fusion_json_member(&result, "transition_count")->integer == 2);
    fusion_json_free(&result);
    FusionJson area = fusion_json_string("9");
    assert(feb_audit_world(FUSION_SOURCE_DIR, "zero_mission", &area, 1, &result, &error));
    const FusionJson *summary = fusion_json_member(&result, "summary");
    assert(fusion_json_member(&result, "room_count")->integer ==
           fusion_json_member(summary, "partial")->integer +
               fusion_json_member(summary, "unsupported")->integer +
               fusion_json_member(summary, "error")->integer +
               fusion_json_member(summary, "success")->integer);
    fusion_json_free(&result);
    fusion_json_free(&area);
}

int main(void) {
    test_lz77();
    test_room_rle();
    test_bitmaps();
    test_annotation_rows();
    test_metatile_rendering();
    test_synthetic_aria_room();
    test_generated_writer();
    test_world_overview_arguments();
    test_real_rooms();
    puts("editor render contract: ok");
    return 0;
}
