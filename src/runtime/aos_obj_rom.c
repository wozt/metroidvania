/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria's object sheets from the ROM: see aos_obj_rom.h. */
#include "aos_obj_rom.h"

#include "aos/core/aos_rom_graphics.h"

#include <stdio.h>
#include <string.h>

static void set_error(char *error, size_t error_size, const char *message) {
    if (error && error_size) snprintf(error, error_size, "%s", message);
}

static uint16_t read_u16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t read_u32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static const uint8_t *rom_at(const FusionRom *rom, uint32_t pointer, size_t size) {
    if (pointer < 0x08000000u) return NULL;
    size_t offset = pointer - 0x08000000u;
    if (offset > rom->size || size > rom->size - offset) return NULL;
    return rom->data + offset;
}

/* sub_0803B800 then sub_0803B924: an object resource on its own sheet (the
 * 16 x 16-tile sheet of the graphics header, + 2 and + 3) and its frame
 * descriptor (+ 0: records, + 2: animations, + 4: the 16-byte records,
 * + 12: the animation table). */
bool aos_obj_sheet_decode(AosObjSheet *sheet, const FusionRom *rom, uint32_t graphics,
                          uint32_t frames, char *error, size_t error_size) {
    enum { TILE_BYTES = 32, SHEET_TILES = AOS_OBJ_SHEET_SIZE / 8 };
    uint8_t tiles[SHEET_TILES * SHEET_TILES * TILE_BYTES];
    const uint8_t *header = rom_at(rom, graphics, 4);
    const uint8_t *descriptor = rom_at(rom, frames, 16);
    if (!header || !descriptor || header[1] != 4 || header[2] != SHEET_TILES ||
        header[3] != SHEET_TILES) {
        set_error(error, error_size, "unexpected Aria object sheet");
        return false;
    }
    memset(tiles, 0, sizeof tiles);
    if (!aos_rom_load_graphics_resource(rom, graphics, tiles, sizeof tiles, error, error_size))
        return false;
    for (int y = 0; y < AOS_OBJ_SHEET_SIZE; ++y)
        for (int x = 0; x < AOS_OBJ_SHEET_SIZE; ++x) {
            const uint8_t *tile = tiles + (size_t)((y / 8) * SHEET_TILES + x / 8) * TILE_BYTES;
            sheet->pixels[y * AOS_OBJ_SHEET_SIZE + x] =
                (uint8_t)((tile[(y % 8) * 4 + (x % 8) / 2] >> (4 * (x & 1))) & 0xF);
        }
    uint16_t record_count = read_u16(descriptor), anim_count = read_u16(descriptor + 2);
    if (record_count > AOS_OBJ_MAX_FRAMES || anim_count > AOS_OBJ_MAX_ANIMS) {
        set_error(error, error_size, "Aria object descriptor exceeds its limits");
        return false;
    }
    sheet->frame_count = (uint8_t)record_count;
    for (unsigned f = 0; f < record_count; ++f) {
        const uint8_t *record = rom_at(rom, read_u32(descriptor + 4) + f * 16, 16);
        if (!record || record[5] > AOS_OBJ_MAX_PIECES) {
            set_error(error, error_size, "invalid Aria object frame");
            return false;
        }
        AosObjFrame *frame = &sheet->frames[f];
        frame->count = record[5];
        for (unsigned k = 0; k < frame->count; ++k) {
            const uint8_t *c = rom_at(rom, read_u32(record + 12) + k * 12, 12);
            if (!c || c[4] + c[6] > AOS_OBJ_SHEET_SIZE || c[5] + c[7] > AOS_OBJ_SHEET_SIZE) {
                set_error(error, error_size, "invalid Aria object component");
                return false;
            }
            frame->pieces[k] = (AosObjPiece){(int8_t)c[0], (int8_t)c[1], c[6], c[7], c[4],
                                               c[5], (c[10] & 2) != 0, (c[10] & 1) != 0};
        }
    }
    sheet->anim_count = (uint8_t)anim_count;
    for (unsigned a = 0; a < anim_count; ++a) {
        const uint8_t *entry = rom_at(rom, read_u32(descriptor + 12) + a * 4, 4);
        const uint8_t *anim = entry ? rom_at(rom, read_u32(entry), 4) : NULL;
        uint16_t steps = anim ? read_u16(anim) : 0;
        if (!anim || read_u16(anim + 2) != 1 || steps == 0 || steps > AOS_OBJ_MAX_STEPS ||
            !rom_at(rom, read_u32(entry) + 4, (size_t)steps * 4)) {
            set_error(error, error_size, "unsupported Aria object animation");
            return false;
        }
        sheet->anim_length[a] = (uint8_t)steps;
        for (unsigned k = 0; k < steps; ++k) {
            sheet->anim_frame[a][k] = anim[4 + k * 4];
            sheet->anim_duration[a][k] = anim[5 + k * 4];
        }
    }
    return true;
}
