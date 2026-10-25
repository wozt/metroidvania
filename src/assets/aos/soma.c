/* SPDX-License-Identifier: GPL-3.0-only */
#include "fusion/assets/aos.h"

#include "export.h"
#include "sprite_library.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    GBA_BASE = 0x08000000,
    SOMA_GRAPHICS = 0x000E11D4,
    SOMA_PALETTE = 0x002097D4,
    SOMA_ANIMATIONS = 0x000E11C4,
    SOMA_PALETTE_BANKS = 0x000E126C,
    KNIFE_GRAPHICS = 0x001AC54C,
    KNIFE_PALETTE = 0x002098B8,
    KNIFE_ANIMATIONS = 0x0022B6C0,
    MAX_ANIMATIONS = 256,
    MAX_FRAMES = 256,
};

static const char *const index_prefix =
    "assets/extracted/aria/sprites/soma/runtime";

typedef struct {
    uint8_t id;
    uint8_t duration;
} AnimationFrame;

typedef struct {
    uint32_t pointer;
    size_t count;
    AnimationFrame frames[MAX_FRAMES];
} Animation;

typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} Text;

typedef struct {
    uint8_t *bmp;
    size_t size;
    int left;
    int top;
    uint8_t record;
    uint8_t duration;
    uint32_t component;
} KnifeFrame;

typedef struct {
    char key[32];
    char reason[128];
} Unresolved;

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

static const uint8_t *slice(const FusionRom *rom, uint32_t offset, size_t size,
                            const char *label, char *error, size_t error_size) {
    if (!rom || rom->game != FUSION_ASSET_GAME_ARIA || !rom->data ||
        offset > rom->size || size > rom->size - offset) {
        char message[160];
        snprintf(message, sizeof message, "truncated %s", label);
        set_error(error, error_size, message);
        return NULL;
    }
    return rom->data + offset;
}

static bool pointer_offset(const FusionRom *rom, uint32_t pointer,
                           uint32_t *offset, const char *label,
                           char *error, size_t error_size) {
    if (pointer < GBA_BASE || (uint64_t)pointer - GBA_BASE >= rom->size) {
        char message[160];
        snprintf(message, sizeof message, "%s pointer outside ROM", label);
        set_error(error, error_size, message);
        return false;
    }
    *offset = pointer - GBA_BASE;
    return true;
}

static uint16_t read_u16(const uint8_t *bytes) {
    return (uint16_t)(bytes[0] | (uint16_t)bytes[1] << 8);
}

static uint32_t read_u32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 |
           (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

static bool descriptor_count(const FusionRom *rom, uint32_t descriptor,
                             size_t *count, char *error, size_t error_size) {
    const uint8_t *bytes = slice(rom, descriptor, 16, "animation descriptor",
                                 error, error_size);
    if (!bytes) return false;
    *count = read_u16(bytes + 2);
    if (*count > MAX_ANIMATIONS) {
        set_error(error, error_size, "animation count exceeds native limit");
        return false;
    }
    return true;
}

static bool read_animation(const FusionRom *rom, uint32_t descriptor,
                           size_t index, Animation *animation,
                           char *error, size_t error_size) {
    const uint8_t *description =
        slice(rom, descriptor, 16, "animation descriptor", error, error_size);
    if (!description) return false;
    size_t count = read_u16(description + 2);
    uint32_t table = 0;
    if (index >= count ||
        !pointer_offset(rom, read_u32(description + 12), &table,
                        "animation table", error, error_size)) return false;
    const uint8_t *entry = slice(rom, table + (uint32_t)index * 4, 4,
                                 "animation pointer", error, error_size);
    if (!entry || !pointer_offset(rom, read_u32(entry), &animation->pointer,
                                  "animation", error, error_size)) return false;
    const uint8_t *header = slice(rom, animation->pointer, 4,
                                  "animation header", error, error_size);
    if (!header) return false;
    animation->count = read_u16(header);
    if (animation->count < 1 || animation->count > MAX_FRAMES ||
        read_u16(header + 2) != 1) {
        set_error(error, error_size, "unsupported Soma animation encoding");
        return false;
    }
    const uint8_t *frames = slice(rom, animation->pointer + 4,
                                  animation->count * 4, "animation frames",
                                  error, error_size);
    if (!frames) return false;
    for (size_t frame = 0; frame < animation->count; ++frame) {
        const uint8_t *source = frames + frame * 4;
        if (source[2] || source[3]) {
            set_error(error, error_size, "unexpected data in Soma animation frame");
            return false;
        }
        animation->frames[frame] = (AnimationFrame){source[0], source[1]};
    }
    return true;
}

static bool load_palette(const FusionRom *rom, uint32_t descriptor,
                         unsigned bank, const uint8_t **palette,
                         char *error, size_t error_size) {
    const uint8_t *header = slice(rom, descriptor, 4, "Soma palette descriptor",
                                  error, error_size);
    if (!header || header[0] != 0 || header[1] != 4 || header[3] != 0 ||
        bank >= header[2]) {
        set_error(error, error_size, "unexpected Soma palette descriptor");
        return false;
    }
    *palette = slice(rom, descriptor + 4 + bank * 32, 32, "Soma OBJ palette",
                     error, error_size);
    return *palette != NULL;
}

static uint8_t expand5(unsigned value) { return (uint8_t)(value * 255 / 31); }

static void decode_pixel(uint8_t packed, unsigned nibble,
                         const uint8_t *palette, uint8_t output[4]) {
    unsigned index = (packed >> (nibble * 4)) & 15;
    if (!index) {
        memset(output, 0, 4);
        return;
    }
    uint16_t color = read_u16(palette + index * 2);
    output[0] = expand5(color & 31);
    output[1] = expand5((color >> 5) & 31);
    output[2] = expand5((color >> 10) & 31);
    output[3] = 255;
}

static bool body_bitmap(const FusionRom *rom, uint8_t frame_id,
                        const uint8_t *palette, uint8_t **bmp, size_t *bmp_size,
                        int *left, int *top, unsigned *sheet_index,
                        unsigned *quadrant_x, unsigned *quadrant_y,
                        char *error, size_t error_size) {
    const uint8_t *descriptor = slice(rom, SOMA_GRAPHICS, 4,
                                      "Soma graphics descriptor", error, error_size);
    *sheet_index = frame_id / 4;
    if (!descriptor || descriptor[0] != 2 || descriptor[2] != 1 ||
        descriptor[3] != 1 || *sheet_index >= descriptor[1]) {
        set_error(error, error_size, "unexpected Soma graphics descriptor");
        return false;
    }
    const uint8_t *pointer = slice(rom, SOMA_GRAPHICS + 4 + *sheet_index * 4,
                                   4, "Soma graphics sheet pointer",
                                   error, error_size);
    uint32_t sheet = 0;
    if (!pointer || !pointer_offset(rom, read_u32(pointer), &sheet,
                                    "Soma graphics sheet", error, error_size))
        return false;
    const uint8_t *header = slice(rom, sheet, 4, "Soma graphics sheet",
                                  error, error_size);
    const uint8_t *tiles = slice(rom, sheet + 4, 16 * 16 * 32,
                                 "Soma graphics sheet data", error, error_size);
    if (!header || !tiles || header[0] != 0 || header[1] != 4 ||
        header[2] != 16 || header[3] != 16) {
        set_error(error, error_size, "unexpected Soma graphics sheet format");
        return false;
    }
    *quadrant_x = frame_id & 1;
    *quadrant_y = (frame_id >> 1) & 1;
    uint8_t rgba[64 * 64 * 4];
    for (unsigned y = 0; y < 64; ++y) {
        for (unsigned x = 0; x < 64; ++x) {
            unsigned source_x = *quadrant_x * 64 + x;
            unsigned source_y = *quadrant_y * 64 + y;
            size_t tile = (source_y / 8) * 16 + source_x / 8;
            uint8_t packed = tiles[tile * 32 + (source_y & 7) * 4 +
                                   (source_x & 7) / 2];
            decode_pixel(packed, source_x & 1, palette,
                         rgba + (y * 64 + x) * 4);
        }
    }
    return fusion_sprite_bmp32_crop(rgba, 64, 64, 32, 47, bmp, bmp_size,
                                    left, top, error, error_size);
}

static bool knife_bitmap(const FusionRom *rom, uint8_t frame_id,
                         const uint8_t *palette, KnifeFrame *frame,
                         char *error, size_t error_size) {
    const uint8_t *descriptor = slice(rom, KNIFE_ANIMATIONS, 16,
                                      "knife animation descriptor", error, error_size);
    if (!descriptor || frame_id >= read_u16(descriptor)) {
        set_error(error, error_size, "knife frame has no OAM record");
        return false;
    }
    uint32_t records = 0;
    if (!pointer_offset(rom, read_u32(descriptor + 4), &records,
                        "knife OAM records", error, error_size)) return false;
    const uint8_t *record = slice(rom, records + frame_id * 16, 16,
                                  "knife OAM frame record", error, error_size);
    if (!record || record[0] || record[1] || record[2] || record[3] ||
        record[5] != 1 || record[6] || record[7]) {
        set_error(error, error_size, "unsupported knife OAM frame composition");
        return false;
    }
    uint32_t component_offset = 0;
    frame->component = read_u32(record + 12);
    if (!pointer_offset(rom, frame->component, &component_offset,
                        "knife OAM component", error, error_size)) return false;
    const uint8_t *component = slice(rom, component_offset, 12,
                                     "knife OAM component", error, error_size);
    if (!component) return false;
    int x = (int8_t)component[0], y = (int8_t)component[1];
    unsigned source_x = component[4], source_y = component[5];
    unsigned width = component[6], height = component[7];
    if (component[2] || component[3] || source_x % 8 || source_y % 8 ||
        width % 8 || height % 8 || width < 8 || height < 8) {
        set_error(error, error_size, "unsupported knife OAM component");
        return false;
    }
    const uint8_t *graphics = slice(rom, KNIFE_GRAPHICS, 4,
                                    "knife graphics descriptor", error, error_size);
    if (!graphics || graphics[0] != 0 || graphics[1] != 4 ||
        graphics[2] != 16 || graphics[3] != 4 ||
        source_x + width > 128 || source_y + height > 32) {
        set_error(error, error_size, "unexpected knife graphics descriptor");
        return false;
    }
    const uint8_t *tiles = slice(rom, KNIFE_GRAPHICS + 4, 16 * 4 * 32,
                                 "knife graphics sheet", error, error_size);
    if (!tiles) return false;
    uint8_t rgba[128 * 32 * 4];
    memset(rgba, 0, sizeof rgba);
    for (unsigned py = 0; py < height; ++py) {
        for (unsigned px = 0; px < width; ++px) {
            unsigned sx = source_x + px, sy = source_y + py;
            size_t tile = (sy / 8) * 16 + sx / 8;
            uint8_t packed = tiles[tile * 32 + (sy & 7) * 4 + (sx & 7) / 2];
            decode_pixel(packed, sx & 1, palette,
                         rgba + (py * width + px) * 4);
        }
    }
    frame->record = frame_id;
    return fusion_sprite_bmp32_crop(rgba, width, height, -x, -y,
                                    &frame->bmp, &frame->size,
                                    &frame->left, &frame->top,
                                    error, error_size);
}

static const char *observed_label(size_t animation) {
    switch (animation) {
        case 0: return "idle";
        case 1: return "run";
        case 25: return "run_stop";
        case 26: return "run_start";
        default: return NULL;
    }
}

static bool write_body_json(Text *text, size_t index, const Animation *animation,
                            unsigned bank, const uint8_t *frame_ids,
                            const unsigned *sheets, const unsigned *quadrant_x,
                            const unsigned *quadrant_y, bool comma) {
    if (!text_append(text,
            "  \"Soma/animation_%03zu\": {\n"
            "    \"animation_pointer\": \"0x%08x\",\n"
            "    \"frames\": [\n",
            index, animation->pointer + GBA_BASE)) return false;
    for (size_t frame = 0; frame < animation->count; ++frame) {
        if (!text_append(text,
                "      {\n"
                "        \"frame_id\": %u,\n"
                "        \"quadrant\": [\n"
                "          %u,\n"
                "          %u\n"
                "        ],\n"
                "        \"sheet\": %u\n"
                "      }%s\n",
                frame_ids[frame], quadrant_x[frame], quadrant_y[frame],
                sheets[frame], frame + 1 < animation->count ? "," : ""))
            return false;
    }
    const char *label = observed_label(index);
    return text_append(text,
            "    ],\n"
            "    \"native_index\": %zu,\n"
            "    \"observed_label\": %s%s%s,\n"
            "    \"palette_bank\": %u\n"
            "  }%s\n",
            index, label ? "\"" : "", label ? label : "null", label ? "\"" : "",
            bank, comma ? "," : "");
}

static bool write_knife_json(Text *text, size_t index,
                             const Animation *animation,
                             const KnifeFrame *frames, bool comma) {
    if (!text_append(text,
            "  \"Knife/animation_%03zu\": {\n"
            "    \"animation_pointer\": \"0x%08x\",\n"
            "    \"frames\": [\n",
            index, animation->pointer + GBA_BASE)) return false;
    for (size_t frame = 0; frame < animation->count; ++frame) {
        if (!text_append(text,
                "      {\n"
                "        \"component\": \"0x%08x\",\n"
                "        \"record\": %u\n"
                "      }%s\n",
                frames[frame].component, frames[frame].record,
                frame + 1 < animation->count ? "," : "")) return false;
    }
    return text_append(text,
            "    ],\n"
            "    \"native_index\": %zu\n"
            "  }%s\n", index, comma ? "," : "");
}

static bool write_output(const char *output, const char *name, const Text *text,
                         char *error, size_t error_size) {
    char path[FUSION_EXPORT_PATH_CAPACITY];
    return fusion_export_join(path, output, name) &&
           fusion_export_write_file(path, (const uint8_t *)text->data,
                                    text->size, error, error_size);
}

static bool append_manifest(Text *manifest,
                            const FusionSpriteLibrarySummary *library,
                            size_t soma_count, size_t knife_count,
                            const uint8_t *banks, unsigned palette_count,
                            const Unresolved *unresolved, size_t unresolved_count) {
    if (!text_append(manifest,
            "{\n"
            "  \"draw_origin_in_cell\": [\n"
            "    32,\n"
            "    47\n"
            "  ],\n"
            "  \"frames\": %zu,\n"
            "  \"index_schema\": \"metroidvania-sprite-index-v1\",\n"
            "  \"knife_animations\": %zu,\n"
            "  \"note\": \"Private native extraction; source ROM data is never redistributed.\",\n"
            "  \"removed_stale_objects\": %zu,\n"
            "  \"schema\": \"metroidvania-aos-soma-runtime-v1\",\n"
            "  \"sequences\": %zu,\n"
            "  \"soma_animations\": %zu,\n"
            "  \"unidentified\": {\n"
            "    \"animations\": {\n",
            library->frames, knife_count, library->removed_stale_objects,
            library->sequences, soma_count)) return false;
    for (size_t index = 0; index < unresolved_count; ++index) {
        if (!text_append(manifest, "      \"%s\": \"%s\"%s\n",
                         unresolved[index].key, unresolved[index].reason,
                         index + 1 < unresolved_count ? "," : "")) return false;
    }
    if (!text_append(manifest,
            "    },\n"
            "    \"semantic_labels\": \"observed labels cover a subset of animations\",\n"
            "    \"soma_palette_banks\": [")) return false;
    bool first = true;
    for (unsigned bank = 1; bank < palette_count; ++bank) {
        bool used = false;
        for (size_t index = 0; index < soma_count; ++index)
            if (banks[index] == bank) used = true;
        if (used) continue;
        if (!text_append(manifest, "%s\n      %u", first ? "" : ",", bank))
            return false;
        first = false;
    }
    if (!first && !text_append(manifest, "\n    ")) return false;
    return text_append(manifest,
            "],\n"
            "    \"weapons\": \"only the knife descriptors are verified\"\n"
            "  },\n"
            "  \"unique_bmps\": %zu\n"
            "}\n", library->unique_bmps);
}

bool fusion_aos_export_soma(const FusionRom *rom, const char *output,
                            FusionAosSomaExportSummary *summary,
                            char *error, size_t error_size) {
    if (!rom || rom->game != FUSION_ASSET_GAME_ARIA || !output || !summary) {
        set_error(error, error_size, "invalid Soma export request");
        return false;
    }
    *summary = (FusionAosSomaExportSummary){0};
    FusionSpriteLibrary library = {0};
    Text soma_json = {0}, knife_json = {0}, animations = {0}, manifest = {0};
    Unresolved unresolved[MAX_ANIMATIONS];
    size_t unresolved_count = 0;
    bool success = fusion_sprite_library_init(&library, output, index_prefix,
                                              error, error_size);
    size_t soma_count = 0, knife_total = 0, knife_count = 0;
    const uint8_t *banks = NULL;
    if (success) success = descriptor_count(rom, SOMA_ANIMATIONS, &soma_count,
                                            error, error_size);
    if (success) banks = slice(rom, SOMA_PALETTE_BANKS, soma_count,
                               "animation palette table", error, error_size);
    for (size_t index = 0; success && index < soma_count; ++index) {
        Animation animation;
        const uint8_t *palette = NULL;
        unsigned sheets[MAX_FRAMES], qx[MAX_FRAMES], qy[MAX_FRAMES];
        uint8_t frame_ids[MAX_FRAMES];
        char key[32];
        snprintf(key, sizeof key, "Soma/animation_%03u", (unsigned)index);
        success = read_animation(rom, SOMA_ANIMATIONS, index, &animation,
                                 error, error_size) &&
                  load_palette(rom, SOMA_PALETTE, banks[index], &palette,
                               error, error_size) &&
                  fusion_sprite_library_begin(&library, error, error_size);
        for (size_t frame = 0; success && frame < animation.count; ++frame) {
            uint8_t *bmp = NULL;
            size_t bmp_size = 0;
            int left = 0, top = 0;
            frame_ids[frame] = animation.frames[frame].id;
            success = body_bitmap(rom, animation.frames[frame].id, palette,
                                  &bmp, &bmp_size, &left, &top, &sheets[frame],
                                  &qx[frame], &qy[frame], error, error_size) &&
                      fusion_sprite_library_add(
                          &library, key, frame, animation.frames[frame].duration,
                          left, top, bmp, bmp_size, error, error_size);
            free(bmp);
        }
        if (success)
            success = write_body_json(&soma_json, index, &animation, banks[index],
                                      frame_ids, sheets, qx, qy,
                                      index + 1 < soma_count);
    }
    const uint8_t *knife_palette = NULL;
    if (success) success = descriptor_count(rom, KNIFE_ANIMATIONS, &knife_total,
                                            error, error_size) &&
                           load_palette(rom, KNIFE_PALETTE, 0, &knife_palette,
                                        error, error_size);
    for (size_t index = 0; success && index < knife_total; ++index) {
        Animation animation;
        KnifeFrame frames[MAX_FRAMES] = {{0}};
        char local_error[128] = {0};
        success = read_animation(rom, KNIFE_ANIMATIONS, index, &animation,
                                 error, error_size);
        bool supported = success;
        for (size_t frame = 0; supported && frame < animation.count; ++frame) {
            frames[frame].duration = animation.frames[frame].duration;
            supported = knife_bitmap(rom, animation.frames[frame].id,
                                     knife_palette, &frames[frame],
                                     local_error, sizeof local_error);
        }
        if (!success) break;
        if (!supported) {
            snprintf(unresolved[unresolved_count].key,
                     sizeof unresolved[unresolved_count].key,
                     "Knife/animation_%03u", (unsigned)index);
            snprintf(unresolved[unresolved_count].reason,
                     sizeof unresolved[unresolved_count].reason,
                     "%s", local_error);
            ++unresolved_count;
            for (size_t frame = 0; frame < animation.count; ++frame)
                free(frames[frame].bmp);
            continue;
        }
        char key[32];
        snprintf(key, sizeof key, "Knife/animation_%03u", (unsigned)index);
        success = fusion_sprite_library_begin(&library, error, error_size);
        for (size_t frame = 0; success && frame < animation.count; ++frame)
            success = fusion_sprite_library_add(
                &library, key, frame, frames[frame].duration,
                frames[frame].left, frames[frame].top,
                frames[frame].bmp, frames[frame].size, error, error_size);
        if (success)
            success = write_knife_json(&knife_json, index, &animation, frames,
                                       false);
        for (size_t frame = 0; frame < animation.count; ++frame)
            free(frames[frame].bmp);
        if (success) ++knife_count;
    }
    FusionSpriteLibrarySummary library_summary = {0};
    if (success) success = fusion_sprite_library_finish(
        &library, &library_summary, error, error_size);
    if (success && knife_json.size && knife_json.data[knife_json.size - 1] == '\n')
        knife_json.data[--knife_json.size] = '\0';
    if (success)
        success = text_append(&animations, "{\n%s%s%s}\n",
                              knife_json.data ? knife_json.data : "",
                              knife_count && soma_count ? ",\n" : "",
                              soma_json.data ? soma_json.data : "") &&
                  write_output(output, "animations.json", &animations,
                               error, error_size);
    const uint8_t *palette_header = success
        ? slice(rom, SOMA_PALETTE, 4, "Soma palette descriptor",
                error, error_size) : NULL;
    if (success)
        success = palette_header &&
                  append_manifest(&manifest, &library_summary, soma_count,
                                  knife_count, banks, palette_header[2],
                                  unresolved, unresolved_count) &&
                  write_output(output, "manifest.json", &manifest,
                               error, error_size);
    if (success) {
        *summary = (FusionAosSomaExportSummary){
            soma_count, knife_count, library_summary.frames,
            library_summary.unique_bmps,
            library_summary.removed_stale_objects};
    } else if (!error || error[0] == '\0') {
        set_error(error, error_size, "cannot export Soma animation library");
    }
    fusion_sprite_library_free(&library);
    free(soma_json.data); free(knife_json.data); free(animations.data);
    free(manifest.data);
    return success;
}
