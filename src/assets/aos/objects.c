/* SPDX-License-Identifier: GPL-3.0-only */
/* Native export of traced Aria object, enemy and shared-effect sprites. */
#include "fusion/assets/aos.h"

#include "fusion/assets/gba.h"
#include "export.h"
#include "sprite_library.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    GBA_BASE = 0x08000000,
    ENEMY_TABLE = 0x080E9644,
    MONEY_TABLE = 0x085250E4,
    WEAPON_STATS = 0x08505D3C,
    ARMOR_STATS = 0x085063B0,
    RED_SOULS = 0x080E1598,
    SKILL_THROWS = 0x084F12C8,
    HUD_PALETTE = 0x0820C428,
    HUD_TILES = 0x0827B208,
    ENEMY_COUNT = 0x71,
    DEATH_BLINK = 0x08118CE0,
    SCREEN_MARGINS = 0x08118D08,
    TILE_BYTES = 32,
    MAX_ANIMATIONS = 256,
    MAX_FRAMES = 128,
    MAX_STATIC_FRAMES = 16,
    CANVAS_SIZE = 512,
    CANVAS_ORIGIN = 256,
};

static const char *const index_prefix =
    "assets/extracted/aria/sprites/objects/runtime";

typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} Text;

typedef struct {
    const char *name;
    uint32_t graphics;
    uint32_t palette;
    uint8_t bank;
    uint32_t frames;
    uint32_t probes;
    uint32_t volleys;
    uint8_t volley_count;
    uint8_t static_count;
    uint8_t static_frames[MAX_STATIC_FRAMES];
} EntitySpec;

typedef struct {
    const char *name;
    uint32_t graphics;
    uint32_t palette;
    uint8_t bank;
    uint32_t frames;
    uint8_t animation_count;
    uint8_t animations[2];
    int static_frame;
} EffectSpec;

typedef struct {
    int8_t x;
    int8_t y;
    uint8_t source_x;
    uint8_t source_y;
    uint8_t width;
    uint8_t height;
    uint32_t flags;
} Component;

typedef struct {
    uint8_t *data;
    size_t size;
    size_t width_tiles;
} TileSheet;

typedef struct {
    uint8_t *bmp;
    size_t bmp_size;
    int left;
    int top;
    uint8_t mode;
    int8_t hurt_x;
    int8_t hurt_y;
    uint8_t hurt_width;
    uint8_t hurt_height;
    int8_t attack_x;
    int8_t attack_y;
    uint8_t attack_width;
    uint8_t attack_height;
    int anchor_x;
    int anchor_y;
} RenderedFrame;

#define ENTITY(name_, graphics_, palette_, bank_, frames_) \
    {name_, graphics_, palette_, bank_, frames_, 0, 0, 0, 0, {0}}
#define STATIC(name_, graphics_, palette_, bank_, frames_, count_, ...) \
    {name_, graphics_, palette_, bank_, frames_, 0, 0, 0, count_, {__VA_ARGS__}}
#define PROBES(name_, graphics_, palette_, bank_, frames_, probes_) \
    {name_, graphics_, palette_, bank_, frames_, probes_, 0, 0, 0, {0}}

static const EntitySpec entities[] = {
    ENTITY("bat", 0x081F422C, 0x0820BD4C, 0, 0x0824B2C4),
    STATIC("zombie", 0x081CBF8C, 0x0820A62C, 0, 0x082178B8, 2, 0, 13),
    ENTITY("skeleton", 0x081F424C, 0x0820BE3C, 0, 0x0824C910),
    ENTITY("merman", 0x081CBF3C, 0x0820A464, 0, 0x08212FB4),
    STATIC("peeping_eye", 0x081CBFEC, 0x0820A86C, 0, 0x0821D7B4, 6,
           11, 12, 13, 14, 15, 16),
    ENTITY("killer_fish", 0x081CBF3C, 0x0820A464, 0, 0x08212FB4),
    ENTITY("bone_pillar", 0x081E41D4, 0x0820B8E8, 0, 0x08242F8C),
    STATIC("blue_crow", 0x081F4274, 0x0820BF10, 0, 0x0824EC04, 1, 8),
    STATIC("buer", 0x081F428C, 0x0820BF9C, 0, 0x0824FDB0, 2, 0, 1),
    STATIC("white_dragon", 0x081CBF74, 0x0820A580, 0, 0x0821631C, 1, 0),
    ENTITY("ghost", 0x081CBFFC, 0x0820A8D4, 0, 0x0821EE0C),
    ENTITY("rock_armor", 0x081E41BC, 0x0820B83C, 1, 0x0823FFD8),
    ENTITY("giant_ghost", 0x081CBFA4, 0x0820A698, 0, 0x08219140),
    ENTITY("winged_skeleton", 0x081CBF64, 0x0820A518, 0, 0x08215CBC),
    STATIC("red_crow", 0x081F4274, 0x0820BF10, 1, 0x0824EC04, 1, 8),
    PROBES("axe_armor", 0x081E41AC, 0x0820B7F4, 0, 0x0823C290, 0x08528708),
    {"skull_archer", 0x081E419C, 0x0820B78C, 0, 0x0823A52C,
     0x08528620, 0x08528614, 3, 1, {25}},
    ENTITY("fleaman", 0x081F423C, 0x0820BD94, 0, 0x0824B92C),
    ENTITY("ripper", 0x081F423C, 0x0820BD94, 1, 0x0824B92C),
    STATIC("harpy", 0x081CBF6C, 0x0820A53C, 0, 0x08216060, 7,
           8, 9, 10, 11, 12, 13, 14),
    STATIC("siren", 0x081CBF6C, 0x0820A53C, 1, 0x08216060, 7,
           8, 9, 10, 11, 12, 13, 14),
    STATIC("gorgon", 0x081CBFF4, 0x0820A890, 0, 0x0821ECE8, 10,
           17, 18, 19, 20, 21, 22, 23, 24, 25, 26),
    STATIC("catoblepas", 0x081CBFF4, 0x0820A890, 1, 0x0821ECE8, 10,
           17, 18, 19, 20, 21, 22, 23, 24, 25, 26),
    ENTITY("tiny_devil", 0x081F4254, 0x0820BE60, 1, 0x0824CF0C),
    STATIC("imp", 0x081F4254, 0x0820BE60, 0, 0x0824CF0C, 1, 21),
    PROBES("dead_warrior", 0x081EC1F4, 0x0820BBD4, 0, 0x08246F00, 0x08528830),
    ENTITY("skeleton_knight", 0x081CC00C, 0x0820A95C, 0, 0x0821FA5C),
    ENTITY("skull_millione", 0x081F4214, 0x0820BCA0, 2, 0x0824A8E0),
    STATIC("cagnazzo", 0x081F4214, 0x0820BCA0, 0, 0x0824A334, 1, 13),
    STATIC("lubicant", 0x081F4214, 0x0820BCA0, 1, 0x0824A334, 1, 13),
    STATIC("killer_doll", 0x081F426C, 0x0820BEEC, 0, 0x0824DB0C, 5,
           13, 14, 15, 16, 17),
    STATIC("lightning_doll", 0x081F426C, 0x0820BEEC, 0, 0x0824E7F4, 5,
           20, 21, 22, 23, 24),
    STATIC("nemesis", 0x081F4264, 0x0820BEC8, 0, 0x0824D2D8, 4,
           0, 1, 2, 3),
    {"beam_skeleton", 0x081F427C, 0x0820BF54, 0, 0x0824F9AC,
     0x08528B04, 0, 0, 10, {21, 22, 23, 24, 25, 26, 27, 28, 29, 30}},
    ENTITY("dead_crusader", 0x081CC00C, 0x0820A95C, 1, 0x0821FA5C),
    ENTITY("valkyrie", 0x081CBF2C, 0x0820A3FC, 0, 0x0821285C),
    STATIC("mimic", 0x081CC04C, 0x0820AB7C, 0, 0x08223698, 1, 0),
    ENTITY("erinys", 0x081CBF2C, 0x0820A3FC, 1, 0x0821285C),
    ENTITY("durga", 0x081CC02C, 0x0820AA6C, 0, 0x0822184C),
    ENTITY("curly", 0x081CC02C, 0x0820AA6C, 1, 0x0822184C),
    ENTITY("ghost_dancer", 0x081F4244, 0x0820BDD8, 0, 0x0824BC84),
    ENTITY("ghost_dancer_bank1", 0x081F4244, 0x0820BDD8, 1, 0x0824BC84),
    ENTITY("ghost_dancer_bank2", 0x081F4244, 0x0820BDD8, 2, 0x0824BC84),
    ENTITY("gargoyle", 0x081CBFDC, 0x0820A804, 0, 0x0821C688),
    ENTITY("gremlin", 0x081CBFDC, 0x0820A804, 1, 0x0821C688),
    STATIC("zombie_officer", 0x081CC01C, 0x0820AA24, 0, 0x08220F84, 3,
           15, 16, 17),
    ENTITY("medusa_head", 0x081CC034, 0x0820AAB0, 0, 0x08221978),
    ENTITY("medusa_head_bank1", 0x081CC034, 0x0820AAB0, 1, 0x08221978),
    STATIC("needles", 0x081F4284, 0x0820BF78, 0, 0x0824FB10, 4,
           2, 3, 4, 5),
    STATIC("zombie_soldier", 0x081CC024, 0x0820AA48, 0, 0x082215C4, 3,
           18, 19, 20),
    /* SkillBatUse: entry 3 of the skill resources 0x080E13BC (tiles,
     * palette row 0, frames), exported with its per-frame attack boxes. */
    ENTITY("skill_bat", 0x081CD8D0, 0x0820B0B0, 0, 0x082311F8),
    /* SkillSkullArcherUse and its arrow sub_08028BC4: entry 18 of 0x080E13BC
     * (the bow plays sequence 0, the arrow shows frame 1). */
    STATIC("skill_archer", 0x081D510C, 0x0820B2EC, 0, 0x08233BCC, 1, 1),
    /* Resource 0 of 0x080E13BC, shared by several souls: the Merman's spout
     * and drops (sequences 0, 6), the Winged Skeleton's spear (still frame
     * 0), the Siren's notes (2-3) and the Axe Armor's axe (6). */
    STATIC("skill_shared", 0x081CC0C4, 0x0820B044, 0, 0x082308E8, 4, 0, 2, 3, 6),
    /* Resource 24 of 0x080E13BC: the Harpy soul's dagger (still frame 1) and
     * its trail (3-5), the Durga soul's sword (2). */
    STATIC("skill_blades", 0x081D8124, 0x0820B3C4, 0, 0x082347BC, 5, 1, 2, 3, 4, 5),
    /* Resource 21 of 0x080E13BC: the Tiny Devil soul's fire (sequence 0,
     * then 16 still frames, one per direction). */
    STATIC("skill_devil", 0x081D6918, 0x0820B358, 0, 0x0823445C, 16, 0, 1, 2, 3, 4, 5, 6, 7, 8,
           9, 10, 11, 12, 13, 14, 15),
    /* Resource 31 of 0x080E13BC: the Blue Crow soul's crow (sequence 2). */
    ENTITY("skill_crow", 0x081DB940, 0x0820B4C0, 0, 0x08235F04),
    /* Resource 20 of 0x080E13BC: the Fleaman soul's flea (still frames 0-2,
     * sequence 1, the walker probes 0x084F13BE). */
    {"skill_flea", 0x081D6114, 0x0820B334, 0, 0x0823415C, 0x084F13BE, 0, 0, 3, {0, 1, 2}},
    /* Resource 7 of 0x080E13BC: the Skeleton soul's bone. */
    /* Its still frames 0 (the bone) and 3 (the grenade of parameter 5), with
     * the walker probes 0x084F1388 of the bouncing throws (flag 4). */
    {"skill_bone", 0x081CF8E0, 0x0820B140, 0, 0x08231740, 0x084F1388, 0, 0, 2, {0, 3}},
};

static const EffectSpec effects[] = {
    {"flame", 0x081C15F4, 0x082099FC, 3, 0x0820ED60, 1, {0}, -1},
    {"blood", 0x081C15F4, 0x082099FC, 4, 0x0820ED60, 1, {13}, -1},
    {"smoke", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 1, {5}, -1},
    {"dark_smoke", 0x081C15F4, 0x082099FC, 1, 0x0820ED60, 1, {5}, -1},
    {"fire", 0x081C15F4, 0x082099FC, 4, 0x0820ED60, 1, {1}, -1},
    {"spark", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 1, {16}, -1},
    {"dust", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 1, {4}, -1},
    {"impact", 0x081C15F4, 0x082099FC, 4, 0x0820ED60, 1, {7}, -1},
    {"ember", 0x081C15F4, 0x082099FC, 3, 0x0820ED60, 1, {1}, -1},
    {"casing", 0x081C15F4, 0x082099FC, 4, 0x0820ED60, 0, {0}, 53},
    /* The arts of burst list 0 of sub_08045A98 not shared with the above. */
    {"burst_1", 0x081C15F4, 0x082099FC, 3, 0x0820ED60, 1, {1}, -1},
    {"burst_12", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 1, {12}, -1},
    {"burst_15", 0x081C15F4, 0x082099FC, 3, 0x0820ED60, 1, {15}, -1},
    /* Pickups. sub_0803B924's attribute bits are the OBJ palette row,
     * which sub_0800C5D8 fills from bank sUnk_084F0DD8[row] of 0x082099FC
     * (rows 6, 7, 8, 9 -> banks 2, 3, 4, 6). Small coins (money table rows
     * 0-3: rows 6, 7, 9, 8) loop sequence 21 (sub_0803F2C8), bags (rows 4-6:
     * rows 6, 7, 8) show frame 33, hearts (row 7) frames 40 (small) and 39
     * (big), souls (rows of 0x080E1CD0: 7, 6, 8, 9) loop sequence 6. */
    {"coin0", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 1, {21}, -1},
    {"coin1", 0x081C15F4, 0x082099FC, 3, 0x0820ED60, 1, {21}, -1},
    {"coin2", 0x081C15F4, 0x082099FC, 6, 0x0820ED60, 1, {21}, -1},
    {"coin3", 0x081C15F4, 0x082099FC, 4, 0x0820ED60, 1, {21}, -1},
    {"bag4", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 0, {0}, 33},
    {"bag5", 0x081C15F4, 0x082099FC, 3, 0x0820ED60, 0, {0}, 33},
    {"bag6", 0x081C15F4, 0x082099FC, 4, 0x0820ED60, 0, {0}, 33},
    {"heart_small", 0x081C15F4, 0x082099FC, 3, 0x0820ED60, 0, {0}, 40},
    {"heart_big", 0x081C15F4, 0x082099FC, 3, 0x0820ED60, 0, {0}, 39},
    {"soul0", 0x081C15F4, 0x082099FC, 3, 0x0820ED60, 1, {6}, -1},
    {"soul1", 0x081C15F4, 0x082099FC, 2, 0x0820ED60, 1, {6}, -1},
    {"soul2", 0x081C15F4, 0x082099FC, 4, 0x0820ED60, 1, {6}, -1},
    {"soul3", 0x081C15F4, 0x082099FC, 6, 0x0820ED60, 1, {6}, -1},
};

#undef ENTITY
#undef STATIC
#undef PROBES

static void set_error(char *error, size_t error_size, const char *message) {
    if (!error || error_size == 0) return;
    snprintf(error, error_size, "%s", message);
    error[error_size - 1] = '\0';
}

static bool text_append(Text *text, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    va_list copy;
    va_copy(copy, arguments);
    int needed = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (needed < 0) {
        va_end(arguments);
        return false;
    }
    size_t total = text->size + (size_t)needed + 1;
    if (total > text->capacity) {
        size_t next = text->capacity ? text->capacity : 4096;
        while (next < total) {
            if (next > SIZE_MAX / 2) {
                va_end(arguments);
                return false;
            }
            next *= 2;
        }
        char *grown = realloc(text->data, next);
        if (!grown) {
            va_end(arguments);
            return false;
        }
        text->data = grown;
        text->capacity = next;
    }
    vsnprintf(text->data + text->size, text->capacity - text->size,
              format, arguments);
    va_end(arguments);
    text->size += (size_t)needed;
    return true;
}

static bool pointer_offset(const FusionRom *rom, uint32_t pointer, size_t size,
                           uint32_t *offset, const char *label,
                           char *error, size_t error_size) {
    if (!rom || !rom->data || pointer < GBA_BASE ||
        (uint64_t)pointer - GBA_BASE > rom->size ||
        size > rom->size - ((size_t)pointer - GBA_BASE)) {
        char message[160];
        snprintf(message, sizeof message, "truncated %s", label);
        set_error(error, error_size, message);
        return false;
    }
    *offset = pointer - GBA_BASE;
    return true;
}

static const uint8_t *slice(const FusionRom *rom, uint32_t pointer, size_t size,
                            const char *label, char *error, size_t error_size) {
    uint32_t offset = 0;
    if (!pointer_offset(rom, pointer, size, &offset, label, error, error_size))
        return NULL;
    return rom->data + offset;
}

static uint16_t read_u16(const uint8_t *bytes) {
    return (uint16_t)(bytes[0] | (uint16_t)bytes[1] << 8);
}

static int16_t read_s16(const uint8_t *bytes) {
    return (int16_t)read_u16(bytes);
}

static uint32_t read_u32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 |
           (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

static bool load_tile_sheet(const FusionRom *rom, uint32_t descriptor,
                            TileSheet *sheet, char *error, size_t error_size) {
    *sheet = (TileSheet){0};
    const uint8_t *header = slice(rom, descriptor, 4, "graphics descriptor",
                                  error, error_size);
    if (!header || header[1] != 4 || header[2] == 0 || header[3] == 0) {
        set_error(error, error_size, "unsupported object graphics descriptor");
        return false;
    }
    sheet->width_tiles = header[2];
    sheet->size = (size_t)header[2] * header[3] * TILE_BYTES;
    sheet->data = malloc(sheet->size);
    if (!sheet->data) {
        set_error(error, error_size, "cannot allocate object tile sheet");
        return false;
    }
    if (header[0] == 0) {
        const uint8_t *tiles = slice(rom, descriptor + 4, sheet->size,
                                     "object tiles", error, error_size);
        if (!tiles) goto failed;
        memcpy(sheet->data, tiles, sheet->size);
        return true;
    }
    if (header[0] == 1) {
        const uint8_t *entry = slice(rom, descriptor + 4, 4, "tile pointer",
                                     error, error_size);
        uint32_t offset = 0;
        if (!entry || !pointer_offset(rom, read_u32(entry), 4, &offset,
                                      "tile data", error, error_size)) goto failed;
        uint32_t expanded = fusion_gba_uncompressed_size(
            rom->data + offset, rom->size - offset);
        if (expanded < sheet->size) {
            set_error(error, error_size,
                      "compressed object tiles are shorter than their sheet");
            goto failed;
        }
        uint8_t *decoded = malloc(expanded);
        size_t written = 0;
        if (!decoded || !fusion_gba_lz77_uncompress(
                rom->data + offset, rom->size - offset, decoded, expanded,
                &written) || written < sheet->size) {
            free(decoded);
            set_error(error, error_size, "invalid compressed object tiles");
            goto failed;
        }
        memcpy(sheet->data, decoded, sheet->size);
        free(decoded);
        return true;
    }
    {
        char message[96];
        snprintf(message, sizeof message,
                 "unsupported object graphics encoding %u", header[0]);
        set_error(error, error_size, message);
    }
failed:
    free(sheet->data);
    *sheet = (TileSheet){0};
    return false;
}

static const uint8_t *load_palette(const FusionRom *rom, uint32_t descriptor,
                                   unsigned bank, char *error,
                                   size_t error_size) {
    const uint8_t *header = slice(rom, descriptor, 4, "palette descriptor",
                                  error, error_size);
    if (!header || header[0] != 0 || bank >= header[2]) {
        set_error(error, error_size, "unsupported object palette bank");
        return NULL;
    }
    return slice(rom, descriptor + 4 + bank * 32, 32, "object palette",
                 error, error_size);
}

static bool read_components(const FusionRom *rom, uint32_t descriptor,
                            uint8_t frame, Component components[256],
                            size_t *count, const uint8_t **record,
                            char *error, size_t error_size) {
    const uint8_t *header = slice(rom, descriptor, 8, "frame descriptor",
                                  error, error_size);
    if (!header) return false;
    uint16_t record_count = read_u16(header);
    uint32_t records = read_u32(header + 4);
    if (frame >= record_count) {
        set_error(error, error_size, "object frame outside its frame list");
        return false;
    }
    *record = slice(rom, records + (uint32_t)frame * 16, 16,
                    "object frame record", error, error_size);
    if (!*record) return false;
    *count = (*record)[5];
    uint32_t pointer = read_u32(*record + 12);
    for (size_t index = 0; index < *count; ++index) {
        const uint8_t *source = slice(rom, pointer + (uint32_t)index * 12, 12,
                                      "OAM component", error, error_size);
        if (!source) return false;
        if (read_u16(source + 2) || source[4] % 8 || source[5] % 8 ||
            source[6] % 8 || source[7] % 8) {
            set_error(error, error_size, "unsupported object OAM component");
            return false;
        }
        components[index] = (Component){
            (int8_t)source[0], (int8_t)source[1], source[4], source[5],
            source[6], source[7], read_u32(source + 8)};
    }
    return true;
}

static uint8_t expand5(unsigned value) { return (uint8_t)(value * 255 / 31); }

static bool render_frame(const FusionRom *rom, uint32_t descriptor,
                         const TileSheet *sheet, const uint8_t *palette,
                         uint8_t frame, RenderedFrame *rendered,
                         char *error, size_t error_size) {
    *rendered = (RenderedFrame){0};
    Component components[256];
    size_t count = 0;
    const uint8_t *record = NULL;
    if (!read_components(rom, descriptor, frame, components, &count, &record,
                         error, error_size)) return false;
    if (count) {
        const Component *last = &components[count - 1];
        rendered->anchor_x = last->x + last->width / 2;
        rendered->anchor_y = last->y + last->height / 2;
    }
    rendered->mode = record[4];
    if (rendered->mode) {
        const uint8_t *box = slice(rom, read_u32(record + 8),
                                   rendered->mode == 2 ? 8 : 4,
                                   "frame box", error, error_size);
        if (!box) return false;
        rendered->hurt_x = (int8_t)box[0];
        rendered->hurt_y = (int8_t)box[1];
        rendered->hurt_width = box[2];
        rendered->hurt_height = box[3];
        const uint8_t *attack = rendered->mode == 2 ? box + 4 : box;
        rendered->attack_x = (int8_t)attack[0];
        rendered->attack_y = (int8_t)attack[1];
        rendered->attack_width = attack[2];
        rendered->attack_height = attack[3];
    }
    uint8_t *rgba = calloc((size_t)CANVAS_SIZE * CANVAS_SIZE, 4);
    if (!rgba) {
        set_error(error, error_size, "cannot allocate object sprite canvas");
        return false;
    }
    size_t sheet_height = sheet->size / (sheet->width_tiles * TILE_BYTES) * 8;
    for (size_t order = count; order > 0; --order) {
        const Component *component = &components[order - 1];
        if (((component->flags >> 16) & 0xFF) >= 4) {
            free(rgba);
            set_error(error, error_size, "unsupported OAM component attributes");
            return false;
        }
        if ((size_t)component->source_x + component->width >
                sheet->width_tiles * 8 ||
            (size_t)component->source_y + component->height > sheet_height)
            continue;
        bool vertical = (component->flags & (1u << 16)) != 0;
        bool horizontal = (component->flags & (1u << 17)) != 0;
        for (size_t y = 0; y < component->height; ++y) {
            for (size_t x = 0; x < component->width; ++x) {
                size_t source_x = component->source_x +
                    (horizontal ? component->width - 1 - x : x);
                size_t source_y = component->source_y +
                    (vertical ? component->height - 1 - y : y);
                size_t tile = source_y / 8 * sheet->width_tiles + source_x / 8;
                uint8_t packed = sheet->data[tile * TILE_BYTES +
                    source_y % 8 * 4 + source_x % 8 / 2];
                unsigned color_index = (packed >> (4 * (source_x & 1))) & 15;
                if (!color_index) continue;
                int canvas_x = CANVAS_ORIGIN + component->x + (int)x;
                int canvas_y = CANVAS_ORIGIN + component->y + (int)y;
                if (canvas_x < 0 || canvas_x >= CANVAS_SIZE ||
                    canvas_y < 0 || canvas_y >= CANVAS_SIZE) {
                    free(rgba);
                    set_error(error, error_size, "object frame exceeds 512 pixels");
                    return false;
                }
                uint16_t color = read_u16(palette + color_index * 2);
                uint8_t *pixel = rgba +
                    ((size_t)canvas_y * CANVAS_SIZE + (size_t)canvas_x) * 4;
                pixel[0] = expand5(color & 31);
                pixel[1] = expand5((color >> 5) & 31);
                pixel[2] = expand5((color >> 10) & 31);
                pixel[3] = 255;
            }
        }
    }
    bool visible = false;
    for (size_t index = 3; index < (size_t)CANVAS_SIZE * CANVAS_SIZE * 4;
         index += 4) {
        if (rgba[index]) { visible = true; break; }
    }
    if (!visible) {
        free(rgba);
        rgba = calloc(4, 1);
        if (!rgba) {
            set_error(error, error_size, "cannot allocate empty object frame");
            return false;
        }
        rgba[3] = 255;
        bool success = fusion_sprite_bmp32_crop(
            rgba, 1, 1, 0, 0, &rendered->bmp, &rendered->bmp_size,
            &rendered->left, &rendered->top, error, error_size);
        free(rgba);
        return success;
    }
    bool success = fusion_sprite_bmp32_crop(
        rgba, CANVAS_SIZE, CANVAS_SIZE, CANVAS_ORIGIN, CANVAS_ORIGIN,
        &rendered->bmp, &rendered->bmp_size, &rendered->left, &rendered->top,
        error, error_size);
    free(rgba);
    return success;
}

static bool begin_and_add(FusionSpriteLibrary *library, const char *key,
                          size_t index, unsigned ticks,
                          const RenderedFrame *frame,
                          bool begin, char *error, size_t error_size) {
    return (!begin || fusion_sprite_library_begin(library, error, error_size)) &&
           fusion_sprite_library_add(library, key, index, ticks,
                                     frame->left, frame->top,
                                     frame->bmp, frame->bmp_size,
                                     error, error_size);
}

static bool append_frame_row(Text *rows, const char *name, size_t animation,
                             size_t index, uint8_t frame, uint8_t duration,
                             const RenderedFrame *rendered) {
    return text_append(rows,
        "%s\t%zu\t%zu\t%u\t%u\t%u\t%d\t%d\t%u\t%u\t%d\t%d\t%u\t%u\n",
        name, animation, index, frame, duration, rendered->mode,
        rendered->hurt_x, rendered->hurt_y,
        rendered->hurt_width, rendered->hurt_height,
        rendered->attack_x, rendered->attack_y,
        rendered->attack_width, rendered->attack_height);
}

static bool export_animation(const FusionRom *rom,
                             FusionSpriteLibrary *library,
                             const char *key_prefix, const char *name,
                             uint32_t descriptor, const TileSheet *sheet,
                             const uint8_t *palette, size_t animation_number,
                             uint32_t animation_pointer, uint8_t direct_frame,
                             bool direct, bool metadata, Text *rows,
                             char *error, size_t error_size) {
    const uint8_t *descriptor_bytes = slice(
        rom, descriptor, 16, "entity descriptor", error, error_size);
    if (!descriptor_bytes) return false;
    uint16_t record_count = read_u16(descriptor_bytes);
    size_t count = 1;
    const uint8_t *animation = NULL;
    if (!direct) {
        animation = slice(rom, animation_pointer, 4, "animation header",
                          error, error_size);
        if (!animation) return false;
        count = read_u16(animation);
        if (read_u16(animation + 2) != 1 || count == 0 || count > MAX_FRAMES) {
            set_error(error, error_size,
                      "unsupported entity animation encoding");
            return false;
        }
    }
    int anchors_x[MAX_FRAMES], anchors_y[MAX_FRAMES];
    char key[96];
    int written = snprintf(key, sizeof key, "%s/%s/anim_%zu",
                           key_prefix, name, animation_number);
    if (written < 0 || (size_t)written >= sizeof key) {
        set_error(error, error_size, "object sprite key is too long");
        return false;
    }
    for (size_t index = 0; index < count; ++index) {
        uint8_t frame = direct_frame, duration = 1;
        if (!direct) {
            const uint8_t *entry = slice(
                rom, animation_pointer + 4 + (uint32_t)index * 4, 2,
                "animation frame", error, error_size);
            if (!entry) return false;
            frame = entry[0]; duration = entry[1];
            if (frame >= record_count || duration == 0) {
                set_error(error, error_size, "invalid entity animation frame");
                return false;
            }
        }
        RenderedFrame rendered;
        if (!render_frame(rom, descriptor, sheet, palette, frame, &rendered,
                          error, error_size)) return false;
        anchors_x[index] = rendered.anchor_x;
        anchors_y[index] = rendered.anchor_y;
        bool success = (!metadata || append_frame_row(
                            rows, name, animation_number, index,
                            frame, duration, &rendered)) &&
                       begin_and_add(library, key, index, duration, &rendered,
                                     index == 0, error, error_size);
        free(rendered.bmp);
        if (!success) return false;
    }
    if (metadata) {
        for (size_t index = 0; index < count; ++index) {
            if (!text_append(rows, "anchor\t%s\t%zu\t%zu\t%d\t%d\n",
                             name, animation_number, index,
                             anchors_x[index], anchors_y[index])) return false;
        }
    }
    return true;
}

static bool append_probes(const FusionRom *rom, const EntitySpec *entity,
                          Text *rows, char *error, size_t error_size) {
    if (!entity->probes) return true;
    const uint8_t *count_bytes = slice(rom, entity->probes, 2, "probe table",
                                       error, error_size);
    if (!count_bytes) return false;
    int count = read_s16(count_bytes);
    if (count < 1 || count > 8) {
        char message[128];
        snprintf(message, sizeof message, "%s: implausible probe count %d",
                 entity->name, count);
        set_error(error, error_size, message);
        return false;
    }
    const uint8_t *table = slice(rom, entity->probes, 8 + (size_t)count * 2,
                                 "probe table", error, error_size);
    if (!table || !text_append(rows, "probes\t%s", entity->name)) return false;
    for (int index = 0; index < 4 + count; ++index)
        if (!text_append(rows, "\t%d", read_s16(table + index * 2))) return false;
    return text_append(rows, "\n");
}

static bool append_volleys(const FusionRom *rom, const EntitySpec *entity,
                           Text *rows, char *error, size_t error_size) {
    for (size_t number = 0; number < entity->volley_count; ++number) {
        const uint8_t *entry = slice(
            rom, entity->volleys + (uint32_t)number * 4, 4, "volley",
            error, error_size);
        if (!entry || !text_append(rows, "volley\t%s\t%zu",
                                   entity->name, number)) return false;
        uint32_t pointer = read_u32(entry);
        for (size_t index = 0; index < 8; ++index) {
            const uint8_t *shot = slice(
                rom, pointer + (uint32_t)index * 4, 2, "volley entry",
                error, error_size);
            if (!shot) return false;
            int frame = (int8_t)shot[0];
            if (frame < 0) break;
            if (!text_append(rows, "\t%d\t%d", frame, (int8_t)shot[1]))
                return false;
        }
        if (!text_append(rows, "\n")) return false;
    }
    return true;
}

static bool export_entity(const FusionRom *rom, FusionSpriteLibrary *library,
                          const EntitySpec *entity, Text *rows,
                          char *error, size_t error_size) {
    TileSheet sheet;
    if (!load_tile_sheet(rom, entity->graphics, &sheet, error, error_size))
        return false;
    const uint8_t *palette = load_palette(rom, entity->palette, entity->bank,
                                          error, error_size);
    const uint8_t *descriptor = palette
        ? slice(rom, entity->frames, 16, "entity descriptor", error, error_size)
        : NULL;
    bool success = descriptor != NULL;
    uint16_t record_count = success ? read_u16(descriptor) : 0;
    uint16_t animation_count = success ? read_u16(descriptor + 2) : 0;
    uint32_t table = success ? read_u32(descriptor + 12) : 0;
    if (animation_count > MAX_ANIMATIONS) {
        set_error(error, error_size, "entity animation count exceeds native limit");
        success = false;
    }
    size_t sequence_count = animation_count ? animation_count : record_count;
    for (size_t number = 0; success && number < sequence_count; ++number) {
        uint32_t pointer = 0;
        if (animation_count) {
            const uint8_t *entry = slice(
                rom, table + (uint32_t)number * 4, 4, "animation",
                error, error_size);
            if (!entry) { success = false; break; }
            pointer = read_u32(entry);
        }
        success = export_animation(
            rom, library, "Enemy", entity->name, entity->frames, &sheet,
            palette, number, pointer, (uint8_t)number, animation_count == 0,
            true, rows, error, error_size);
    }
    if (success) success = append_probes(rom, entity, rows, error, error_size) &&
                           append_volleys(rom, entity, rows, error, error_size);
    for (size_t index = 0; success && index < entity->static_count; ++index) {
        uint8_t frame_id = entity->static_frames[index];
        RenderedFrame rendered;
        char key[96];
        snprintf(key, sizeof key, "Enemy/%s/frame_%u", entity->name, frame_id);
        success = render_frame(rom, entity->frames, &sheet, palette, frame_id,
                               &rendered, error, error_size);
        if (!success) break;
        success = begin_and_add(library, key, 0, 1, &rendered, true,
                                error, error_size) &&
                  text_append(rows, "static_anchor\t%s\t%u\t%d\t%d\n",
                              entity->name, frame_id,
                              rendered.anchor_x, rendered.anchor_y) &&
                  /* The frame record's attack box (+ 8), which sub_08042848
                   * installs for entities that use their still frame's. */
                  text_append(rows, "static_attack\t%s\t%u\t%d\t%d\t%u\t%u\n",
                              entity->name, frame_id, rendered.attack_x,
                              rendered.attack_y, rendered.attack_width,
                              rendered.attack_height);
        free(rendered.bmp);
    }
    free(sheet.data);
    return success;
}

static bool export_effect(const FusionRom *rom, FusionSpriteLibrary *library,
                          const EffectSpec *effect,
                          char *error, size_t error_size) {
    TileSheet sheet;
    if (!load_tile_sheet(rom, effect->graphics, &sheet, error, error_size))
        return false;
    const uint8_t *palette = load_palette(rom, effect->palette, effect->bank,
                                          error, error_size);
    bool success = palette != NULL;
    if (success && effect->static_frame >= 0) {
        RenderedFrame rendered;
        char key[96];
        snprintf(key, sizeof key, "Effect/%s/anim_0", effect->name);
        success = render_frame(rom, effect->frames, &sheet, palette,
                               (uint8_t)effect->static_frame, &rendered,
                               error, error_size);
        if (success) {
            success = begin_and_add(library, key, 0, 1, &rendered, true,
                                    error, error_size);
            free(rendered.bmp);
        }
    }
    const uint8_t *descriptor = success && effect->static_frame < 0
        ? slice(rom, effect->frames, 16, "entity descriptor", error, error_size)
        : NULL;
    uint16_t animation_count = descriptor ? read_u16(descriptor + 2) : 0;
    uint32_t table = descriptor ? read_u32(descriptor + 12) : 0;
    for (size_t index = 0;
         success && effect->static_frame < 0 && index < effect->animation_count;
         ++index) {
        unsigned number = effect->animations[index];
        if (number >= animation_count) {
            set_error(error, error_size, "entity animation outside its descriptor");
            success = false;
            break;
        }
        const uint8_t *entry = slice(rom, table + number * 4, 4, "animation",
                                      error, error_size);
        if (!entry) { success = false; break; }
        success = export_animation(
            rom, library, "Effect", effect->name, effect->frames, &sheet,
            palette, number, read_u32(entry), 0, false, false, NULL,
            error, error_size);
    }
    free(sheet.data);
    return success;
}

/* The item icons of sub_0801232C: icon n (a record's + 0x2) is the 16 x 16
 * block (n - 1) % 64 of sheet sUnk_084F1060[(n - 1) / 64] (0x081C5E00,
 * 0x081C7E04, 0x081C9E08; eight per row), shown by the one-component frames
 * of 0x080E10F0 at (-8, -16) on OBJ row + 0x3 (6 for every record), which
 * sub_0800C5D8 fills with bank 2 of 0x082099FC. Each icon a record uses is
 * exported as Icon/i<n>/anim_0. */
static bool export_icons(const FusionRom *rom, FusionSpriteLibrary *library, char *error,
                         size_t error_size) {
    static const uint32_t sheets[3] = {0x081C5E00, 0x081C7E04, 0x081C9E08};
    static const struct { uint32_t table, stride, count; } tables[3] = {
        {0x08505B3C, 16, 32}, {WEAPON_STATS, 28, 59}, {ARMOR_STATS, 20, 45}};
    bool used[193] = {false};
    for (size_t table = 0; table < 3; ++table)
        for (uint32_t index = 0; index < tables[table].count; ++index) {
            const uint8_t *record = slice(rom, tables[table].table + index * tables[table].stride,
                                          4, "item record", error, error_size);
            if (!record) return false;
            if (record[3] != 6) {
                set_error(error, error_size, "item icon outside OBJ palette row 6");
                return false;
            }
            if (record[2] >= 1 && record[2] <= 192) used[record[2]] = true;
        }
    const uint8_t *palette = load_palette(rom, 0x082099FC, 2, error, error_size);
    if (!palette) return false;
    TileSheet sheet[3] = {{0}};
    bool success = true;
    for (size_t s = 0; success && s < 3; ++s)
        success = load_tile_sheet(rom, sheets[s], &sheet[s], error, error_size);
    for (unsigned icon = 1; success && icon <= 192; ++icon) {
        if (!used[icon]) continue;
        const TileSheet *source = &sheet[(icon - 1) / 64];
        unsigned slot = (icon - 1) % 64, origin_x = (slot % 8) * 16, origin_y = (slot / 8) * 16;
        uint8_t rgba[16 * 16 * 4] = {0};
        bool visible = false;
        for (unsigned y = 0; y < 16; ++y)
            for (unsigned x = 0; x < 16; ++x) {
                size_t sx = origin_x + x, sy = origin_y + y;
                size_t tile = sy / 8 * source->width_tiles + sx / 8;
                if ((tile + 1) * TILE_BYTES > source->size) continue;
                uint8_t packed = source->data[tile * TILE_BYTES + sy % 8 * 4 + sx % 8 / 2];
                unsigned color_index = (packed >> (4 * (sx & 1))) & 15;
                if (!color_index) continue;
                uint16_t color = read_u16(palette + color_index * 2);
                uint8_t *pixel = rgba + (y * 16 + x) * 4;
                pixel[0] = expand5(color & 31);
                pixel[1] = expand5((color >> 5) & 31);
                pixel[2] = expand5((color >> 10) & 31);
                pixel[3] = 255;
                visible = true;
            }
        if (!visible) continue;
        RenderedFrame rendered = {0};
        char key[48];
        snprintf(key, sizeof key, "Icon/i%u/anim_0", icon);
        success = fusion_sprite_bmp32_crop(rgba, 16, 16, 8, 16, &rendered.bmp,
                                           &rendered.bmp_size, &rendered.left, &rendered.top,
                                           error, error_size) &&
                  begin_and_add(library, key, 0, 1, &rendered, true, error, error_size);
        free(rendered.bmp);
    }
    for (size_t s = 0; s < 3; ++s) free(sheet[s].data);
    return success;
}

/* The icon each item, weapon and armor record shows (+ 0x2). */
static bool append_item_icons(const FusionRom *rom, Text *icons, char *error,
                              size_t error_size) {
    static const struct { const char *kind; uint32_t table, stride, count; } tables[3] = {
        {"item", 0x08505B3C, 16, 32}, {"weapon", WEAPON_STATS, 28, 59},
        {"armor", ARMOR_STATS, 20, 45}};
    if (!text_append(icons, "schema\tmetroidvania-aos-item-icons-v1\n"
                            "# kind\tindex\ticon\n")) return false;
    for (size_t table = 0; table < 3; ++table)
        for (uint32_t index = 0; index < tables[table].count; ++index) {
            const uint8_t *record = slice(rom, tables[table].table + index * tables[table].stride,
                                          4, "item record", error, error_size);
            if (!record || !text_append(icons, "%s\t%u\t%u\n", tables[table].kind, index,
                                        record[2]))
                return false;
        }
    return true;
}

static bool export_doors(const FusionRom *rom, FusionSpriteLibrary *library,
                         char *error, size_t error_size) {
    static const uint8_t frames[] = {0, 5};
    static const uint32_t scripts[] = {0x08525564, 0x08525574};
    TileSheet sheet;
    if (!load_tile_sheet(rom, 0x081CBE0C, &sheet, error, error_size))
        return false;
    bool success = true;
    for (size_t style = 0; success && style < 2; ++style) {
        const uint8_t *header = slice(rom, scripts[style], 2,
                                      "palette script", error, error_size);
        size_t count = header ? read_u16(header) : 0;
        if (!header || count == 0 || count > 64) {
            set_error(error, error_size, "invalid palette script length");
            success = false;
            break;
        }
        char key[40];
        snprintf(key, sizeof key, "WoodenDoor/style_%zu", style);
        for (size_t index = 0; success && index < count; ++index) {
            const uint8_t *entry = slice(
                rom, scripts[style] + 4 + (uint32_t)index * 4, 2,
                "palette script entries", error, error_size);
            const uint8_t *palette = entry
                ? load_palette(rom, 0x08209AE0, entry[0], error, error_size)
                : NULL;
            RenderedFrame rendered;
            success = palette && render_frame(
                rom, 0x0820F160, &sheet, palette, frames[style], &rendered,
                error, error_size);
            if (!success) break;
            success = begin_and_add(library, key, index, entry[1], &rendered,
                                    index == 0, error, error_size);
            free(rendered.bmp);
        }
    }
    free(sheet.data);
    return success;
}

static bool append_enemy_stats(const FusionRom *rom, Text *stats,
                               char *error, size_t error_size) {
    if (!text_append(stats,
            "schema\tmetroidvania-aos-enemy-stats-v3\n"
            "# id\thp\tfield_e\tcontact\tdefence\tweak\tresist\texperience"
            "\tcommon_item\trare_item\tsoul_rarity\tcommon_rate\trare_rate"
            "\tsoul_type\tsoul_index\n")) return false;
    for (size_t enemy = 0; enemy < ENEMY_COUNT; ++enemy) {
        const uint8_t *record = slice(
            rom, ENEMY_TABLE + (uint32_t)enemy * 0x24, 0x24,
            "enemy record", error, error_size);
        if (!record || !text_append(
                stats, "%zu\t%u\t%u\t%u\t%u\t0x%04x\t0x%04x\t%u"
                "\t0x%04x\t0x%04x\t%u\t%u\t%u\t%u\t%u\n",
                enemy, read_u16(record + 0xC), read_u16(record + 0xE),
                record[0x13], record[0x14], read_u16(record + 0x1A),
                read_u16(record + 0x1C), read_u16(record + 0x10),
                read_u16(record + 0x8), read_u16(record + 0xA), record[0x12],
                record[0x15], record[0x16], record[0x17], record[0x18])) return false;
    }
    return true;
}

/* The money pickup table of sub_08043F70 (0x085250E4, seven 12-byte
 * records): value, message, the bytes at + 0x8 (attribute bits of
 * sub_0803B924) and + 0x9 (a sequence or frame number, not traced) and the
 * lift flag. */
static bool append_money(const FusionRom *rom, Text *money, char *error, size_t error_size) {
    if (!text_append(money,
            "schema\tmetroidvania-aos-money-v1\n"
            "# variant\tvalue\ttext\tfield_8\tfield_9\tlifted\n")) return false;
    for (uint32_t variant = 0; variant < 7; ++variant) {
        const uint8_t *record = slice(rom, MONEY_TABLE + variant * 12, 12, "money record",
                                      error, error_size);
        if (!record || !text_append(money, "%u\t%u\t%u\t%u\t%u\t%u\n", variant,
                                    read_u32(record), read_u32(record + 4), record[8],
                                    record[9], read_u16(record + 10)))
            return false;
    }
    return true;
}

/* The equipment records sub_0804ACD4 reads: weapons (0x08505D3C, 59
 * records of 28 bytes) and armor/accessories (0x085063B0, 45 records of 20
 * bytes): ATK (+ 0xA), DEF (+ 0xB), STR, CON, INT, LCK (+ 0xC-0xF, signed)
 * and the armor table's resistances (+ 0x10). */
static bool append_equipment(const FusionRom *rom, Text *equipment, char *error,
                             size_t error_size) {
    if (!text_append(equipment,
            "schema\tmetroidvania-aos-equipment-v1\n"
            "# kind\tindex\tatk\tdef\tstr\tcon\tint\tlck\tresist\n")) return false;
    static const struct { const char *kind; uint32_t table, stride, count; } tables[] = {
        {"weapon", WEAPON_STATS, 28, 59}, {"armor", ARMOR_STATS, 20, 45}};
    for (size_t table = 0; table < 2; ++table)
        for (uint32_t index = 0; index < tables[table].count; ++index) {
            const uint8_t *record = slice(rom, tables[table].table + index * tables[table].stride,
                                          tables[table].stride, "equipment record", error,
                                          error_size);
            if (!record || !text_append(equipment, "%s\t%u\t%u\t%u\t%d\t%d\t%d\t%d\t0x%04x\n",
                                        tables[table].kind, index, record[0xA], record[0xB],
                                        (int8_t)record[0xC], (int8_t)record[0xD],
                                        (int8_t)record[0xE], (int8_t)record[0xF],
                                        table ? read_u16(record + 0x10) : 0))
                return false;
        }
    return true;
}

/* Soma's red soul records (0x080E1598, 56 x 16 bytes, one per count
 * nibble of a 0x1C-byte red soul inventory group; record 0 is none):
 * whether a skill entity exists (+ 0x0), the cast pose (+ 0x4), the MP cost
 * (+ 0x6), the most active at once (+ 0x8), the power of sub_08021530(5)
 * (+ 0xA), the parameter given to the skill entity (+ 0xE) and the attack
 * elements of sub_0806B7D8 (+ 0xC). */
static bool append_red_souls(const FusionRom *rom, Text *souls, char *error,
                             size_t error_size) {
    if (!text_append(souls,
            "schema\tmetroidvania-aos-red-souls-v2\n"
            "# soul\tskill\tpose\tmp\tmaximum\tpower\tparameter\telements\n")) return false;
    for (uint32_t soul = 0; soul < 56; ++soul) {
        const uint8_t *record = slice(rom, RED_SOULS + soul * 16, 16, "red soul record",
                                      error, error_size);
        if (!record || !text_append(souls, "%u\t0x%08x\t%u\t%d\t%u\t%u\t%u\t0x%04x\n", soul,
                                    read_u32(record), read_u16(record + 4),
                                    (int16_t)read_u16(record + 6), record[8],
                                    read_u16(record + 0xA), read_u16(record + 0xE),
                                    read_u16(record + 0xC)))
            return false;
    }
    return true;
}

/* The thrown-object records of the shared Skeleton skill routine
 * (0x084F12C8, 8 x 24 bytes, indexed by the red soul's parameter): launch
 * speeds (+ 0x0, + 0x4), gravity (+ 0x8), graphics record of 0x080E13BC
 * (+ 0xC), still frame (+ 0xD), sequence (+ 0xE), palette row (+ 0xF),
 * song (+ 0x10), flags (+ 0x12), spin (+ 0x14) and + 0x16. */
/* Burst list 0 of sub_08045A98 (0x08525498[0], 24-byte entries ended by a
 * row byte 0xFF): animation (+ 0x0), OBJ row (+ 0x1), priority (+ 0x2),
 * delay (+ 0x3), x and y offsets (+ 0x4, + 0x6, s16), vx, vy, ax, ay
 * (+ 0x8 to + 0x14). */
static bool append_bursts(const FusionRom *rom, Text *text, char *error, size_t error_size) {
    const uint8_t *head = slice(rom, 0x08525498, 4, "burst lists", error, error_size);
    if (!head || !text_append(text, "schema\tmetroidvania-aos-bursts-v1\n"
                                    "# list\tanimation\trow\tpriority\tdelay\tdx\tdy\tvx\tvy"
                                    "\tax\tay\n"))
        return false;
    uint32_t pointer = read_u32(head);
    for (unsigned index = 0; index < 64; ++index) {
        const uint8_t *entry = slice(rom, pointer + index * 24, 24, "burst entry", error,
                                     error_size);
        if (!entry) return false;
        if (entry[1] == 0xFF) return true;
        if (!text_append(text, "0\t%u\t%u\t%u\t%u\t%d\t%d\t%d\t%d\t%d\t%d\n", entry[0],
                         entry[1], entry[2], entry[3], (int16_t)read_u16(entry + 4),
                         (int16_t)read_u16(entry + 6), (int32_t)read_u32(entry + 8),
                         (int32_t)read_u32(entry + 12), (int32_t)read_u32(entry + 16),
                         (int32_t)read_u32(entry + 20)))
            return false;
    }
    set_error(error, error_size, "unterminated burst list");
    return false;
}

static bool append_skill_throws(const FusionRom *rom, Text *throws, char *error,
                                size_t error_size) {
    if (!text_append(throws,
            "schema\tmetroidvania-aos-skill-throws-v1\n"
            "# parameter\tvx\tvy\tgravity\tresource\tframe\tsequence\tpalette\tsong\tflags"
            "\tspin\tfield_16\n")) return false;
    for (uint32_t index = 0; index < 8; ++index) {
        const uint8_t *record = slice(rom, SKILL_THROWS + index * 24, 24, "skill throw record",
                                      error, error_size);
        if (!record || !text_append(throws,
                                    "%u\t%d\t%d\t%d\t%u\t%u\t%u\t%u\t0x%04x\t0x%04x\t0x%04x"
                                    "\t0x%04x\n",
                                    index, (int32_t)read_u32(record),
                                    (int32_t)read_u32(record + 4), (int32_t)read_u32(record + 8),
                                    record[0xC], record[0xD], record[0xE], record[0xF],
                                    read_u16(record + 0x10), read_u16(record + 0x12),
                                    read_u16(record + 0x14), read_u16(record + 0x16)))
            return false;
    }
    return true;
}

/* The in-game HUD resources of sub_0800DA50: palette rows 0-1 of
 * 0x0820C428 (rows 13-14 of BG palette memory) and the 16 x 16 tiles of
 * 0x0827B208 (BG tiles 0x200-0x2FF), as one private binary: 64 palette
 * bytes then 0x2000 tile bytes. */
static bool write_hud(const FusionRom *rom, const char *metadata_output, char *error,
                      size_t error_size) {
    const uint8_t *palette = slice(rom, HUD_PALETTE, 4 + 2 * 32, "HUD palette", error,
                                   error_size);
    if (!palette || palette[0] != 0 || palette[2] < 2) {
        if (palette) set_error(error, error_size, "unsupported HUD palette resource");
        return false;
    }
    TileSheet sheet;
    if (!load_tile_sheet(rom, HUD_TILES, &sheet, error, error_size)) return false;
    bool ok = sheet.size == 0x2000;
    uint8_t *bytes = ok ? malloc(64 + sheet.size) : NULL;
    if (bytes) {
        memcpy(bytes, palette + 4, 64);
        memcpy(bytes + 64, sheet.data, sheet.size);
        char path[FUSION_EXPORT_PATH_CAPACITY];
        ok = fusion_export_join(path, metadata_output, "hud.bin") &&
             fusion_export_write_file(path, bytes, 64 + sheet.size, error, error_size);
    } else {
        if (ok) set_error(error, error_size, "cannot allocate the HUD resources");
        else set_error(error, error_size, "unexpected HUD tile sheet size");
        ok = false;
    }
    free(bytes);
    free(sheet.data);
    return ok;
}

static bool append_global_metadata(const FusionRom *rom, Text *rows,
                                   char *error, size_t error_size) {
    const uint8_t *blink = slice(rom, DEATH_BLINK, 40, "death blink pattern",
                                 error, error_size);
    if (!blink || !text_append(rows, "blink\t")) return false;
    for (size_t index = 0; index < 40; ++index)
        if (!text_append(rows, "%u", blink[index] & 1)) return false;
    if (!text_append(rows, "\nmargins")) return false;
    const uint8_t *margins = slice(rom, SCREEN_MARGINS, 28, "screen margins",
                                   error, error_size);
    if (!margins) return false;
    for (size_t index = 0; index < 14; ++index)
        if (!text_append(rows, "\t%d", read_s16(margins + index * 2))) return false;
    return text_append(rows, "\n");
}

static bool write_text_file(const char *directory, const char *name,
                            const Text *text, char *error, size_t error_size) {
    char path[FUSION_EXPORT_PATH_CAPACITY];
    if (!fusion_export_join(path, directory, name)) {
        set_error(error, error_size, "object metadata path is too long");
        return false;
    }
    return fusion_export_write_file(path, (const uint8_t *)text->data,
                                    text->size, error, error_size);
}

bool fusion_aos_export_objects(const FusionRom *rom,
                               const char *metadata_output,
                               const char *runtime_output,
                               FusionAosObjectExportSummary *summary,
                               char *error, size_t error_size) {
    if (!rom || rom->game != FUSION_ASSET_GAME_ARIA || !metadata_output ||
        !runtime_output || !summary) {
        set_error(error, error_size, "invalid Aria object export request");
        return false;
    }
    *summary = (FusionAosObjectExportSummary){0};
    Text rows = {0}, stats = {0}, money = {0}, equipment = {0}, red_souls = {0}, skill_throws = {0}, bursts = {0},
         item_icons = {0};
    FusionSpriteLibrary library = {0};
    bool success = fusion_export_ensure_directory(metadata_output, error,
                                                   error_size) &&
        fusion_export_ensure_directory(runtime_output, error, error_size) &&
        fusion_sprite_library_init(&library, runtime_output, index_prefix,
                                   error, error_size) &&
        export_doors(rom, &library, error, error_size) &&
        text_append(&rows,
            "schema\tmetroidvania-aos-enemy-frames-v2\n"
            "# enemy\tanimation\tindex\tframe\tticks\tmode\thurt x\ty\twidth\theight"
            "\tattack x\ty\twidth\theight\n");
    for (size_t index = 0;
         success && index < sizeof entities / sizeof entities[0]; ++index)
        success = export_entity(rom, &library, &entities[index], &rows,
                                error, error_size);
    for (size_t index = 0;
         success && index < sizeof effects / sizeof effects[0]; ++index)
        success = export_effect(rom, &library, &effects[index],
                                error, error_size);
    if (success) success = export_icons(rom, &library, error, error_size);
    if (success) success = append_global_metadata(rom, &rows, error, error_size) &&
                           append_enemy_stats(rom, &stats, error, error_size) &&
                           write_text_file(metadata_output, "enemy_frames.tsv",
                                           &rows, error, error_size) &&
                           write_text_file(metadata_output, "enemies.tsv",
                                           &stats, error, error_size) &&
                           append_money(rom, &money, error, error_size) &&
                           write_text_file(metadata_output, "money.tsv",
                                           &money, error, error_size) &&
                           append_equipment(rom, &equipment, error, error_size) &&
                           write_text_file(metadata_output, "equipment.tsv",
                                           &equipment, error, error_size) &&
                           append_red_souls(rom, &red_souls, error, error_size) &&
                           write_text_file(metadata_output, "red_souls.tsv",
                                           &red_souls, error, error_size) &&
                           append_skill_throws(rom, &skill_throws, error, error_size) &&
                           write_text_file(metadata_output, "skill_throws.tsv",
                                           &skill_throws, error, error_size) &&
                           append_bursts(rom, &bursts, error, error_size) &&
                           write_text_file(metadata_output, "bursts.tsv",
                                           &bursts, error, error_size) &&
                           write_hud(rom, metadata_output, error, error_size) &&
                           append_item_icons(rom, &item_icons, error, error_size) &&
                           write_text_file(metadata_output, "item_icons.tsv",
                                           &item_icons, error, error_size);
    FusionSpriteLibrarySummary library_summary = {0};
    if (success) success = fusion_sprite_library_finish(
        &library, &library_summary, error, error_size);
    if (success) {
        summary->sequences = library_summary.sequences;
        summary->frames = library_summary.frames;
        summary->unique_bmps = library_summary.unique_bmps;
        summary->removed_stale_objects = library_summary.removed_stale_objects;
    } else if (!error || error[0] == '\0') {
        set_error(error, error_size, "cannot export Aria object library");
    }
    fusion_sprite_library_free(&library);
    free(rows.data);
    free(stats.data);
    free(money.data);
    free(equipment.data);
    free(red_souls.data);
    free(skill_throws.data);
    free(bursts.data);
    free(item_icons.data);
    return success;
}

bool fusion_aos_render_object_frame(const FusionRom *rom, uint32_t graphics, uint32_t palette,
                                    uint8_t bank, uint32_t frames, uint8_t frame,
                                    uint8_t **bmp, size_t *bmp_size, int *left, int *top,
                                    char *error, size_t error_size) {
    if (!rom || !bmp || !bmp_size || !left || !top) {
        set_error(error, error_size, "invalid Aria object frame request");
        return false;
    }
    TileSheet sheet;
    if (!load_tile_sheet(rom, graphics, &sheet, error, error_size)) return false;
    const uint8_t *colors = load_palette(rom, palette, bank, error, error_size);
    RenderedFrame rendered = {0};
    bool success = colors && render_frame(rom, frames, &sheet, colors, frame, &rendered,
                                          error, error_size);
    free(sheet.data);
    if (!success) return false;
    *bmp = rendered.bmp;
    *bmp_size = rendered.bmp_size;
    *left = rendered.left;
    *top = rendered.top;
    return true;
}
