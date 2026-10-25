/* SPDX-License-Identifier: GPL-3.0-only */
/* Native export of Aria's weapon records, attack frames and blade sprites. */
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
    WEAPON_TABLE = 0x08505D3C,
    UNARMED_RECORD = 0x084F1270,
    POSTURE_TABLES = 0x084F1238,
    WEAPON_TILES = 0x084F10C0,
    WEAPON_FRAMES = 0x084F117C,
    WEAPON_PALETTE = 0x082098B8,
    WEAPON_RECORD_SIZE = 0x1C,
    MAX_WEAPONS = 256,
    MAX_ROWS = MAX_WEAPONS + 1,
    MAX_FRAMES = 64,
    TILE_BYTES = 32,
    CANVAS_SIZE = 512,
    CANVAS_ORIGIN = 256,
};

static const char *const index_prefix =
    "assets/extracted/aria/sprites/weapons/runtime";

typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} Text;

typedef struct {
    uint8_t item;
    uint8_t class_id;
    uint16_t flags;
    uint8_t tiles;
    uint8_t frames;
    uint8_t animation;
    uint8_t bank;
    uint8_t variant;
    uint8_t interval;
} WeaponRecord;

typedef struct {
    char name[8];
    WeaponRecord record;
    uint8_t postures[5];
} WeaponRow;

typedef struct {
    uint8_t *bmp;
    size_t bmp_size;
    int left;
    int top;
    uint8_t frame;
    uint8_t duration;
    bool hit;
    int8_t hit_x;
    int8_t hit_y;
    uint8_t hit_width;
    uint8_t hit_height;
    size_t outside;
} WeaponFrame;

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

static uint32_t read_u32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 |
           (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

static bool table_pointer(const FusionRom *rom, uint32_t table, size_t index,
                          uint32_t *pointer, char *error, size_t error_size) {
    const uint8_t *entry = slice(rom, table + (uint32_t)index * 4, 4,
                                 "pointer table", error, error_size);
    if (!entry) return false;
    *pointer = read_u32(entry);
    return true;
}

static bool read_weapon_record(const FusionRom *rom, uint32_t pointer,
                               WeaponRecord *record,
                               char *error, size_t error_size) {
    const uint8_t *bytes = slice(rom, pointer, WEAPON_RECORD_SIZE,
                                 "weapon record", error, error_size);
    if (!bytes) return false;
    *record = (WeaponRecord){
        bytes[0], bytes[8], read_u16(bytes + 0x10), bytes[0x12], bytes[0x13],
        bytes[0x14], bytes[0x15], bytes[0x16], bytes[0x17]};
    return true;
}

static bool read_weapon_rows(const FusionRom *rom, WeaponRow rows[MAX_ROWS],
                             size_t *count, char *error, size_t error_size) {
    uint32_t posture[5];
    const uint8_t *tables = slice(rom, POSTURE_TABLES, 20, "posture tables",
                                  error, error_size);
    if (!tables || !read_weapon_record(rom, UNARMED_RECORD, &rows[0].record,
                                       error, error_size)) return false;
    for (size_t index = 0; index < 5; ++index) posture[index] = read_u32(tables + index * 4);
    snprintf(rows[0].name, sizeof rows[0].name, "none");
    *count = 1;
    for (size_t index = 0; index < MAX_WEAPONS; ++index) {
        WeaponRecord record;
        if (!read_weapon_record(rom, WEAPON_TABLE + (uint32_t)index * WEAPON_RECORD_SIZE,
                                &record, error, error_size)) return false;
        if (record.item != 0x20 + index || record.class_id > 5 || record.variant > 2)
            break;
        WeaponRow *row = &rows[(*count)++];
        row->record = record;
        snprintf(row->name, sizeof row->name, "%zu", index);
    }
    for (size_t row_index = 0; row_index < *count; ++row_index) {
        WeaponRow *row = &rows[row_index];
        for (size_t index = 0; index < 5; ++index) {
            size_t item = index <= 2
                ? (size_t)row->record.class_id * 3 + row->record.variant
                : row->record.class_id;
            const uint8_t *value = slice(rom, posture[index] + (uint32_t)item, 1,
                                         "posture table", error, error_size);
            if (!value) return false;
            row->postures[index] = *value;
        }
    }
    return true;
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
        set_error(error, error_size, "cannot allocate weapon tile sheet");
        return false;
    }
    if (header[0] == 0) {
        const uint8_t *tiles = slice(rom, descriptor + 4, sheet->size,
                                     "object tiles", error, error_size);
        if (tiles) memcpy(sheet->data, tiles, sheet->size);
        if (!tiles) goto failed;
        return true;
    }
    if (header[0] == 1) {
        const uint8_t *entry = slice(rom, descriptor + 4, 4, "tile pointer",
                                     error, error_size);
        uint32_t compressed_offset = 0;
        if (!entry || !pointer_offset(rom, read_u32(entry), 4, &compressed_offset,
                                      "tile data", error, error_size)) goto failed;
        uint32_t expanded = fusion_gba_uncompressed_size(
            rom->data + compressed_offset, rom->size - compressed_offset);
        if (expanded < sheet->size) {
            set_error(error, error_size,
                      "compressed object tiles are shorter than their sheet");
            goto failed;
        }
        uint8_t *decoded = malloc(expanded);
        size_t written = 0;
        if (!decoded || !fusion_gba_lz77_uncompress(
                rom->data + compressed_offset, rom->size - compressed_offset,
                decoded, expanded, &written) || written < sheet->size) {
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
        snprintf(message, sizeof message, "unsupported object graphics encoding %u",
                 header[0]);
        set_error(error, error_size, message);
    }
failed:
    free(sheet->data);
    *sheet = (TileSheet){0};
    return false;
}

static bool read_components(const FusionRom *rom, uint32_t descriptor,
                            uint16_t record_count, uint32_t records,
                            uint8_t frame, Component components[256],
                            size_t *count, const uint8_t **frame_record,
                            char *error, size_t error_size) {
    if (frame >= record_count) {
        set_error(error, error_size, "object frame outside its frame list");
        return false;
    }
    (void)descriptor;
    *frame_record = slice(rom, records + (uint32_t)frame * 16, 16,
                          "object frame record", error, error_size);
    if (!*frame_record) return false;
    *count = (*frame_record)[5];
    uint32_t component_pointer = read_u32(*frame_record + 12);
    for (size_t index = 0; index < *count; ++index) {
        const uint8_t *source = slice(rom, component_pointer + (uint32_t)index * 12,
                                      12, "OAM component", error, error_size);
        if (!source) return false;
        uint16_t reserved = read_u16(source + 2);
        if (reserved || source[4] % 8 || source[5] % 8 ||
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

static bool render_bitmap(const FusionRom *rom, uint32_t descriptor,
                          uint16_t record_count, uint32_t records,
                          uint8_t frame, const TileSheet *sheet,
                          const uint8_t *palette, WeaponFrame *output,
                          char *error, size_t error_size) {
    Component components[256];
    size_t component_count = 0;
    const uint8_t *record = NULL;
    if (!read_components(rom, descriptor, record_count, records, frame,
                         components, &component_count, &record,
                         error, error_size)) return false;
    if (record[4]) {
        const uint8_t *box = slice(rom, read_u32(record + 8), 4,
                                   "weapon hitbox", error, error_size);
        if (!box) return false;
        output->hit = true;
        output->hit_x = (int8_t)box[0]; output->hit_y = (int8_t)box[1];
        output->hit_width = box[2]; output->hit_height = box[3];
    }
    uint8_t *rgba = calloc((size_t)CANVAS_SIZE * CANVAS_SIZE, 4);
    if (!rgba) {
        set_error(error, error_size, "cannot allocate weapon sprite canvas");
        return false;
    }
    size_t sheet_height = sheet->size / (sheet->width_tiles * TILE_BYTES) * 8;
    for (size_t order = component_count; order > 0; --order) {
        const Component *component = &components[order - 1];
        if (((component->flags >> 16) & 0xFF) >= 4) {
            free(rgba);
            set_error(error, error_size, "unsupported OAM component attributes");
            return false;
        }
        if ((size_t)component->source_x + component->width > sheet->width_tiles * 8 ||
            (size_t)component->source_y + component->height > sheet_height) {
            ++output->outside;
            continue;
        }
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
                    set_error(error, error_size, "weapon frame exceeds 512 pixels");
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
    for (size_t index = 3; index < (size_t)CANVAS_SIZE * CANVAS_SIZE * 4; index += 4)
        if (rgba[index]) { visible = true; break; }
    if (!visible) {
        free(rgba);
        rgba = calloc(4, 1);
        if (!rgba) {
            set_error(error, error_size, "cannot allocate empty weapon frame");
            return false;
        }
        rgba[3] = 255;
        bool result = fusion_sprite_bmp32_crop(rgba, 1, 1, 0, 0,
                                               &output->bmp, &output->bmp_size,
                                               &output->left, &output->top,
                                               error, error_size);
        free(rgba);
        return result;
    }
    bool result = fusion_sprite_bmp32_crop(
        rgba, CANVAS_SIZE, CANVAS_SIZE, CANVAS_ORIGIN, CANVAS_ORIGIN,
        &output->bmp, &output->bmp_size, &output->left, &output->top,
        error, error_size);
    free(rgba);
    return result;
}

static void free_frames(WeaponFrame frames[MAX_FRAMES], size_t count) {
    for (size_t index = 0; index < count; ++index) free(frames[index].bmp);
}

static bool read_animation(const FusionRom *rom, const WeaponRecord *weapon,
                           WeaponFrame frames[MAX_FRAMES], size_t *frame_count,
                           char *error, size_t error_size) {
    uint32_t descriptor = 0;
    if (!table_pointer(rom, WEAPON_FRAMES, weapon->frames, &descriptor,
                       error, error_size)) return false;
    const uint8_t *description = slice(rom, descriptor, 16, "weapon descriptor",
                                       error, error_size);
    if (!description) return false;
    uint16_t record_count = read_u16(description);
    uint16_t animation_count = read_u16(description + 2);
    uint32_t records = read_u32(description + 4);
    if (weapon->animation >= animation_count) {
        set_error(error, error_size, "weapon animation outside its descriptor");
        return false;
    }
    uint32_t animation = 0;
    if (!table_pointer(rom, read_u32(description + 12), weapon->animation,
                       &animation, error, error_size)) return false;
    const uint8_t *header = slice(rom, animation, 4, "weapon animation",
                                  error, error_size);
    if (!header) return false;
    *frame_count = read_u16(header);
    if (read_u16(header + 2) != 1 || *frame_count == 0 ||
        *frame_count > MAX_FRAMES) {
        set_error(error, error_size, "unsupported weapon animation encoding");
        return false;
    }
    uint32_t graphics = 0;
    TileSheet sheet;
    if (!table_pointer(rom, WEAPON_TILES, weapon->tiles, &graphics,
                       error, error_size) ||
        !load_tile_sheet(rom, graphics, &sheet, error, error_size)) return false;
    const uint8_t *palette = slice(
        rom, WEAPON_PALETTE + 4 + (uint32_t)weapon->bank * 32, 32,
        "weapon palette", error, error_size);
    bool success = palette != NULL;
    for (size_t index = 0; success && index < *frame_count; ++index) {
        const uint8_t *entry = slice(rom, animation + 4 + (uint32_t)index * 4, 2,
                                     "weapon frame", error, error_size);
        if (!entry) { success = false; break; }
        if (entry[0] >= record_count || entry[1] == 0) {
            set_error(error, error_size, "invalid weapon animation frame");
            success = false;
            break;
        }
        frames[index].frame = entry[0];
        frames[index].duration = entry[1];
        success = render_bitmap(rom, descriptor, record_count, records, entry[0],
                                &sheet, palette, &frames[index],
                                error, error_size);
    }
    free(sheet.data);
    if (!success) free_frames(frames, *frame_count);
    return success;
}

static bool write_text_file(const char *directory, const char *name,
                            const Text *text, char *error, size_t error_size) {
    char path[FUSION_EXPORT_PATH_CAPACITY];
    if (!fusion_export_join(path, directory, name)) {
        set_error(error, error_size, "weapon metadata path is too long");
        return false;
    }
    return fusion_export_write_file(path, (const uint8_t *)text->data, text->size,
                                    error, error_size);
}

bool fusion_aos_export_weapons(const FusionRom *rom, const char *metadata_output,
                               const char *runtime_output,
                               FusionAosWeaponExportSummary *summary,
                               char *error, size_t error_size) {
    if (!rom || rom->game != FUSION_ASSET_GAME_ARIA || !metadata_output ||
        !runtime_output || !summary) {
        set_error(error, error_size, "invalid Aria weapon export request");
        return false;
    }
    *summary = (FusionAosWeaponExportSummary){0};
    WeaponRow rows[MAX_ROWS];
    size_t row_count = 0;
    Text weapons = {0}, frame_rows = {0};
    FusionSpriteLibrary library = {0};
    bool success = read_weapon_rows(rom, rows, &row_count, error, error_size) &&
        text_append(&weapons,
            "schema\tmetroidvania-aos-weapons-v2\n"
            "# weapon\titem\tclass\tvariant\tflags\tstand\tcrouch\tair\trecover\t"
            "crouch_recover\tinterval\n");
    for (size_t index = 0; success && index < row_count; ++index) {
        const WeaponRow *row = &rows[index];
        success = text_append(&weapons,
            "%s\t0x%02x\t%u\t%u\t0x%04x\t%u\t%u\t%u\t%u\t%u\t%u\n",
            row->name, row->record.item, row->record.class_id,
            row->record.variant, row->record.flags, row->postures[0],
            row->postures[1], row->postures[2], row->postures[3],
            row->postures[4], row->record.interval);
    }
    if (success) success = fusion_export_ensure_directory(metadata_output, error,
                                                           error_size) &&
                           fusion_export_ensure_directory(runtime_output, error,
                                                           error_size) &&
                           write_text_file(metadata_output, "weapons.tsv", &weapons,
                                           error, error_size) &&
                           text_append(&frame_rows,
                               "schema\tmetroidvania-aos-weapon-frames-v1\n"
                               "# weapon\tindex\tframe\tticks\thit\tx\ty\twidth\theight\n") &&
                           fusion_sprite_library_init(&library, runtime_output,
                                                      index_prefix, error,
                                                      error_size);
    for (size_t row_index = 0; success && row_index < row_count; ++row_index) {
        WeaponFrame frames[MAX_FRAMES] = {{0}};
        size_t frame_count = 0;
        char local_error[160] = {0};
        if (!read_animation(rom, &rows[row_index].record, frames, &frame_count,
                            local_error, sizeof local_error)) {
            if (summary->skipped_count >= FUSION_AOS_EXPORT_SKIP_CAPACITY) {
                set_error(error, error_size, "too many skipped Aria weapon animations");
                success = false;
                break;
            }
            snprintf(summary->skipped[summary->skipped_count],
                     sizeof summary->skipped[summary->skipped_count], "%s: %.148s",
                     rows[row_index].name, local_error);
            ++summary->skipped_count;
            continue;
        }
        success = fusion_sprite_library_begin(&library, error, error_size);
        char key[32];
        snprintf(key, sizeof key, "Weapon/%s", rows[row_index].name);
        for (size_t index = 0; success && index < frame_count; ++index) {
            WeaponFrame *frame = &frames[index];
            success = text_append(&frame_rows,
                "%s\t%zu\t%u\t%u\t%u\t%d\t%d\t%u\t%u\n",
                rows[row_index].name, index, frame->frame, frame->duration,
                frame->hit ? 1u : 0u, frame->hit ? frame->hit_x : 0,
                frame->hit ? frame->hit_y : 0,
                frame->hit ? frame->hit_width : 0,
                frame->hit ? frame->hit_height : 0) &&
                fusion_sprite_library_add(&library, key, index, frame->duration,
                                          frame->left, frame->top,
                                          frame->bmp, frame->bmp_size,
                                          error, error_size);
            summary->outside_components += frame->outside;
        }
        free_frames(frames, frame_count);
    }
    FusionSpriteLibrarySummary library_summary = {0};
    if (success) success = write_text_file(metadata_output, "weapon_frames.tsv",
                                           &frame_rows, error, error_size) &&
                           fusion_sprite_library_finish(&library, &library_summary,
                                                        error, error_size);
    if (success) {
        summary->weapons = row_count - 1;
        summary->sequences = library_summary.sequences;
        summary->frames = library_summary.frames;
        summary->unique_bmps = library_summary.unique_bmps;
        summary->removed_stale_objects = library_summary.removed_stale_objects;
    } else if (!error || error[0] == '\0') {
        set_error(error, error_size, "cannot export Aria weapon library");
    }
    fusion_sprite_library_free(&library);
    free(weapons.data);
    free(frame_rows.data);
    return success;
}
