/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_pause_graphics.h"

#include "../../aos/game/aos_obj_rom.h"
#include "../../aos/game/aos_recent_items.h"
#include "../core/aos_rom_graphics.h"
#include "aos_affine_sprite.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    AOS_PAUSE_PALETTE = 0x080E5C20,
    AOS_PAUSE_GRAPHICS = 0x0815E02C,
    AOS_PAUSE_BG1_CONTROL = 0x1D00,
    AOS_PAUSE_BANK_ENGLISH = 0x0827B200,
    AOS_PAUSE_BANK_FRENCH = 0x08277984,
    AOS_PAUSE_BANK_GERMAN = 0x0826C848,
    AOS_PAUSE_BG0_FILL = 0x330D,
    /* sub_08048C74: the hand's sheet per language (the *(gEwramData+0x4D0)
     * value selects 2 -> 0x0827798C, 3 -> 0x0826C840, else 0x082052D8)
     * and sub_08048B0C's frame descriptor (3 -> 0x0827B1F0). The hand's
     * OBJ palette row gets block 1's sixteen colours of 0x0820C2C0 through
     * sub_0803C7B4; the row's upper half is zero in ROM and stays zero in
     * the loaded OBJ palette (verified against an mGBA OBJ palette dump),
     * so indices 8-15 render black rather than borrowing the next block. */
    AOS_CURSOR_SHEETS = 3,
    AOS_CURSOR_PALETTE = 0x0820C2C0 + 4 + 32,
    AOS_PAUSE_WIDTH = 240,
    AOS_PAUSE_HEIGHT = 160,
    AOS_WEAPON_TABLE = 0x08505D3C,
    AOS_ARMOR_TABLE = 0x085063B0,
    AOS_EQUIPMENT_NAME_IDS = 0x08506734,
    AOS_EQUIPMENT_DESCRIPTION_IDS = 0x08506936,
    AOS_ROM_BASE = 0x08000000,
    AOS_TEXT_TILE_FIRST = 0x200,
    AOS_TEXT_TILE_LIMIT = 0x400,
    AOS_PAUSE_HUB_SLEEP = 4,
};

static const uint32_t pause_background_metadata[AOS_PAUSE_BACKGROUND_COUNT] = {
    [AOS_PAUSE_BACKGROUND_HUB] = 0x0815E940,
    [AOS_PAUSE_BACKGROUND_SOUL_SET] = 0x0815E790,
    [AOS_PAUSE_BACKGROUND_EQUIP] = 0x0815E8B0,
    [AOS_PAUSE_BACKGROUND_ITEM_USE] = 0x0815E820,
    [AOS_PAUSE_BACKGROUND_ABILITY] = 0x0815E700,
    [AOS_PAUSE_BACKGROUND_SLEEP] = 0x0815E700,
    [AOS_PAUSE_BACKGROUND_CONFIG] = 0x0815E670,
    [AOS_PAUSE_BACKGROUND_ENEMY_LIST] = 0x0815EA60,
    [AOS_PAUSE_BACKGROUND_ENEMY_DETAIL] = 0x0815E9D0,
};

static void set_error(char *error, size_t error_size, const char *message) {
    if (!error || !error_size) return;
    snprintf(error, error_size, "%s", message);
    error[error_size - 1] = '\0';
}

static void color_rgb(const uint8_t *palette, unsigned index, uint8_t rgb[3]) {
    uint16_t color = (uint16_t)palette[index * 2] |
                     (uint16_t)palette[index * 2 + 1] << 8;
    rgb[0] = (uint8_t)(((color & 31) * 255) / 31);
    rgb[1] = (uint8_t)((((color >> 5) & 31) * 255) / 31);
    rgb[2] = (uint8_t)((((color >> 10) & 31) * 255) / 31);
}

void aos_pause_graphics_free(AosPauseGraphics *graphics) {
    if (!graphics) return;
    for (unsigned i = 0; i < AOS_PAUSE_BACKGROUND_COUNT; ++i)
        fusion_aos_free_rendered_background(&graphics->backgrounds[i]);
    *graphics = (AosPauseGraphics){0};
}

static bool decode_background(const FusionRom *rom,
                              const FusionAosVideoMemory *memory,
                              unsigned index,
                              FusionAosRenderedBackground *rendered,
                              char *error, size_t error_size) {
    FusionAosBackground background = {
        .layer = 1,
        .control = AOS_PAUSE_BG1_CONTROL,
        .metadata_pointer = pause_background_metadata[index],
    };
    FusionAosBackgroundTilemap tilemap = {0};
    bool ok = fusion_aos_read_background_tiles(rom, &background, &tilemap,
                                                error, error_size) &&
              tilemap.status == FUSION_AOS_TILEMAP_DECODED &&
              fusion_aos_render_background(&background, &tilemap, memory,
                                           rendered, error, error_size) &&
              rendered->unresolved_count == 0;
    fusion_aos_free_background_tilemap(&tilemap);
    return ok;
}

bool aos_pause_graphics_init(AosPauseGraphics *graphics, const FusionRom *rom,
                             uint8_t language,
                             char *error, size_t error_size) {
    if (!graphics || !rom || rom->game != FUSION_ASSET_GAME_ARIA ||
        language < 1 || language > 3) {
        set_error(error, error_size, "invalid Aria pause graphics request");
        return false;
    }
    *graphics = (AosPauseGraphics){0};
    graphics->fx_page = 0xFF;
    graphics->fx_category = 0xFF;
    FusionAosVideoMemory *memory = calloc(1, sizeof *memory);
    if (!memory) {
        set_error(error, error_size, "cannot allocate Aria pause video memory");
        return false;
    }
    static const uint32_t language_banks[] = {
        AOS_PAUSE_BANK_ENGLISH, AOS_PAUSE_BANK_FRENCH, AOS_PAUSE_BANK_GERMAN,
    };
    static const uint32_t pause_graphics[] = {
        AOS_PAUSE_GRAPHICS, 0x0827397C, 0x08268838,
    };
    bool loaded = aos_rom_load_palette_resource(
                          rom, AOS_PAUSE_PALETTE, memory->palette,
                          error, error_size) &&
                      aos_rom_load_graphics_resource(
                          rom, pause_graphics[language - 1], memory->vram,
                          sizeof memory->vram, error, error_size) &&
                      aos_rom_load_graphics_resource(
                          rom, language_banks[language - 1],
                          memory->vram + 0xA000,
                          sizeof memory->vram - 0xA000, error, error_size);
    if (!loaded) {
        free(memory);
        return false;
    }
    bool ok = true;
    for (unsigned i = AOS_PAUSE_BACKGROUND_HUB;
         ok && i <= AOS_PAUSE_BACKGROUND_CONFIG; ++i)
        ok = decode_background(rom, memory, i, &graphics->backgrounds[i],
                               error, error_size);
    if (ok) {
        color_rgb(memory->palette, 0, graphics->backdrop_rgb);
        memcpy(graphics->palette, memory->palette, sizeof graphics->palette);
        memcpy(graphics->bg0_base_chars, memory->vram + 0x4000,
               sizeof graphics->bg0_base_chars);
        memcpy(graphics->bg0_chars, graphics->bg0_base_chars,
               sizeof graphics->bg0_chars);
        graphics->language = language;
        graphics->text_ready =
            aos_intro_resources_init(&graphics->text, rom, NULL, 0) &&
            aos_intro_font_init(&graphics->font, rom, NULL, 0);
        /* The native cell renderer: fb buffer plus the four ROM metric
         * tables (char records 0x080E2B4C/0x080E2EC0 stride 4, glyph
         * records 0x0850A136 stride 14 / 0x08509876 stride 10). Each
         * table must cover the full 0x20..0xFF character range. */
        aos_menu_cells_init(&graphics->cells);
        aos_menu_cells_set_fb(&graphics->cells, graphics->cells_fb);
        const uint8_t *rom_end = rom->data + rom->size;
        const uint8_t *t;
        t = rom->data + (size_t)(0x080E2B4C - AOS_ROM_BASE);
        if (t + 0xE0 * 4 <= rom_end)
            graphics->cells_char_a = t;
        t = rom->data + (size_t)(0x0850A136 - AOS_ROM_BASE);
        if (t + 0xE0 * 14 <= rom_end)
            graphics->cells_glyph_a = t;
        t = rom->data + (size_t)(0x080E2EC0 - AOS_ROM_BASE);
        if (t + 0xE0 * 4 <= rom_end)
            graphics->cells_char_b = t;
        t = rom->data + (size_t)(0x08509876 - AOS_ROM_BASE);
        if (t + 0xE0 * 10 <= rom_end)
            graphics->cells_glyph_b = t;
        /* sub_08048C74 loads the hand through sub_0803B800/sub_0803C7B4 and
         * sub_08048B0C frames it through sub_0803B924: sheet and frame
         * descriptor by language, palette block 1 of 0x0820C2C0. */
        static const uint32_t cursor_graphics[AOS_CURSOR_SHEETS] = {
            0x082052D8, 0x0827798C, 0x0826C840,
        };
        static const uint32_t cursor_frames[AOS_CURSOR_SHEETS] = {
            0x08256EF8, 0x08256EF8, 0x0827B1F0,
        };
        const uint8_t *palette = rom->data +
            (size_t)(AOS_CURSOR_PALETTE - AOS_ROM_BASE);
        /* Rows 1-4 of the block must be present: row 1 is the cursor's
         * own palette and rows 2-4 are the swipe hands' tints. */
        if (palette + 4 + 4 * 32 <= rom->data + rom->size &&
            aos_obj_sheet_decode(&graphics->cursor_sheet, rom,
                                  cursor_graphics[language - 1],
                                  cursor_frames[language - 1],
                                  error, error_size)) {
            for (unsigned k = 0; k < 16; ++k)
                graphics->cursor_palette[k] =
                    (uint16_t)(palette[k * 2] | palette[k * 2 + 1] << 8);
            graphics->cursor_ready = true;
            /* The Soul Set init (sub_08048D84's caller) loads rows 2, 3
             * and 4 of the same 0x0820C2C0 block into OBJ palettes 10,
             * 11 and 12 through sub_0803C7B4 — the red, blue and yellow
             * tints sub_08048108 gives its three swipe hands. */
            for (unsigned row = 0; row < 3; ++row)
                for (unsigned k = 0; k < 16; ++k) {
                    const uint8_t *colour = palette + 32 + row * 32 + k * 2;
                    graphics->swipe_palette[row][k] =
                        (uint16_t)(colour[0] | colour[1] << 8);
                }
        }
        /* sub_0801232C's icon sheets: three graphics resources of
         * 16 tiles x 16 (8192 bytes of 4bpp) and the 16 colours of
         * palette bank 2 of 0x082099FC. */
        static const uint32_t icon_sheets[3] = {
            0x081C5E00, 0x081C7E04, 0x081C9E08,
        };
        const uint8_t *icon_palette = rom->data +
            (size_t)(0x082099FC + 4 + 2 * 32 - AOS_ROM_BASE);
        if (icon_palette + 32 <= rom->data + rom->size) {
            bool icons = true;
            for (unsigned s = 0; s < 3 && icons; ++s)
                icons = aos_rom_load_graphics_resource(
                    rom, icon_sheets[s], graphics->icon_tiles[s],
                    sizeof graphics->icon_tiles[s], error, error_size);
            if (icons) {
                for (unsigned k = 0; k < 16; ++k)
                    graphics->icon_palette[k] =
                        (uint16_t)(icon_palette[k * 2] |
                                   icon_palette[k * 2 + 1] << 8);
                graphics->icons_ready = true;
            }
        }

        /* sub_0804C404 replaces the common menu palette/graphics for the
         * Enemy page, then sub_0804C668/sub_0804C968 load the list/detail
         * BG1 metadata. Preserve the two native palette rows restored after
         * the enemy palette load: common row 13 and resource 0x081183F4 row
         * 1 into destination row 15. */
        FusionAosVideoMemory *enemy_memory = calloc(1, sizeof *enemy_memory);
        if (!enemy_memory) {
            set_error(error, error_size,
                      "cannot allocate Aria enemy-menu video memory");
            ok = false;
        } else {
            *enemy_memory = *memory;
            static const uint32_t enemy_graphics[] = {
                0x0815E034, 0x0827799C, 0x0826C858,
            };
            ok = aos_rom_load_palette_resource(
                     rom, 0x0815E0A4, enemy_memory->palette,
                     error, error_size) &&
                 aos_rom_load_palette_range(
                     rom, AOS_PAUSE_PALETTE, 13, 1, 13,
                     enemy_memory->palette, error, error_size) &&
                 aos_rom_load_palette_range(
                     rom, 0x081183F4, 1, 1, 15,
                     enemy_memory->palette, error, error_size) &&
                 aos_rom_load_graphics_resource(
                     rom, enemy_graphics[language - 1], enemy_memory->vram,
                     sizeof enemy_memory->vram, error, error_size) &&
                 decode_background(
                     rom, enemy_memory, AOS_PAUSE_BACKGROUND_ENEMY_LIST,
                     &graphics->backgrounds[AOS_PAUSE_BACKGROUND_ENEMY_LIST],
                     error, error_size) &&
                 decode_background(
                     rom, enemy_memory, AOS_PAUSE_BACKGROUND_ENEMY_DETAIL,
                     &graphics->backgrounds[AOS_PAUSE_BACKGROUND_ENEMY_DETAIL],
                     error, error_size);
            if (ok) {
                memcpy(graphics->enemy_palette, enemy_memory->palette,
                       sizeof graphics->enemy_palette);
                memcpy(graphics->enemy_bg0_base_chars,
                       enemy_memory->vram + 0x4000,
                       sizeof graphics->enemy_bg0_base_chars);
            }
            free(enemy_memory);
        }
    }
    free(memory);
    if (!ok) {
        if (error && error_size && !error[0])
            set_error(error, error_size, "unsupported Aria pause background");
        aos_pause_graphics_free(graphics);
        return false;
    }
    if (error && error_size) error[0] = '\0';
    return true;
}

static void put_tiles(AosPauseGraphics *graphics, int x, int y,
                      uint16_t entry, int count) {
    int cell = y * 32 + x;
    for (int index = 0; index < count; ++index, ++cell)
        if (cell >= 0 && cell < 32 * 32)
            graphics->bg0_map[cell] = (uint16_t)(entry + index);
}

static void put_number(AosPauseGraphics *graphics, int x, int y,
                       int32_t value, int digits, uint16_t base,
                       bool zero_pad) {
    int cell = y * 32 + x;
    if (value < 0) {
        if (cell >= 0 && cell < 32 * 32) graphics->bg0_map[cell] = base;
        return;
    }
    int count = 1;
    do {
        uint16_t entry = (uint16_t)(base + value % 10);
        if (count != 1 && value == 0 && !zero_pad) entry = (uint16_t)(base + 0xD);
        if (cell >= 0 && cell < 32 * 32) graphics->bg0_map[cell] = entry;
        value /= 10;
        --cell;
        ++count;
    } while (count <= digits || value != 0);
}

static void put_large_number(AosPauseGraphics *graphics, int x, int y,
                             uint32_t value, int digits, unsigned palette) {
    uint16_t base = (uint16_t)(0x300 | (palette << 12));
    for (int count = 1; count <= digits || value != 0; ++count, --x) {
        unsigned digit = value % 10;
        value /= 10;
        graphics->bg0_map[y * 32 + x] = (uint16_t)(base + 0x10 + digit);
        graphics->bg0_map[(y + 1) * 32 + x] = (uint16_t)(base + 0x20 + digit);
    }
}

static void put_meter(AosPauseGraphics *graphics, int x, int y,
                      int32_t current, int32_t maximum, unsigned palette) {
    int32_t eighths = maximum > 0 ? current * 100 / maximum * 8 / 100 : 0;
    if (eighths < 0) eighths = 0;
    if (eighths > 56) eighths = 56;
    for (int cell = 0; cell < 7; ++cell) {
        int remaining = eighths - cell * 8;
        uint16_t tile = remaining >= 8 ? 0x330
                        : remaining <= 0 ? 0x338
                                         : (uint16_t)(0x338 - remaining);
        graphics->bg0_map[y * 32 + x + cell] =
            (uint16_t)(tile | palette << 12);
    }
}

static bool bg0_pixel(const AosPauseGraphics *graphics, const uint8_t *palette,
                      unsigned x, unsigned y, uint8_t rgba[4]) {
    uint16_t entry = graphics->bg0_map[(y / 8) * 32 + x / 8];
    unsigned px = x & 7, py = y & 7;
    if (entry & 0x400) px = 7 - px;
    if (entry & 0x800) py = 7 - py;
    size_t offset = (size_t)(entry & 0x3FF) * 32 + py * 4 + px / 2;
    if (offset >= sizeof graphics->bg0_chars) return false;
    unsigned color_index =
        (graphics->bg0_chars[offset] >> ((px & 1u) * 4)) & 0xF;
    if (!color_index) return false;
    size_t color = ((size_t)(entry >> 12) * 16 + color_index) * 2;
    uint16_t bgr = (uint16_t)(palette[color] | palette[color + 1] << 8);
    rgba[0] = (uint8_t)(((bgr & 31) * 255) / 31);
    rgba[1] = (uint8_t)((((bgr >> 5) & 31) * 255) / 31);
    rgba[2] = (uint8_t)((((bgr >> 10) & 31) * 255) / 31);
    rgba[3] = 255;
    return true;
}

static bool valid_output(const AosPauseGraphics *graphics, const uint8_t *rgba,
                         size_t rgba_size) {
    return graphics && rgba &&
           rgba_size >= AOS_PAUSE_WIDTH * AOS_PAUSE_HEIGHT * 4 &&
           graphics->backgrounds[AOS_PAUSE_BACKGROUND_HUB].rgba;
}

static void draw_flourish(AosPauseGraphics *graphics, uint8_t *rgba);
static void draw_swipe(AosPauseGraphics *graphics, uint8_t *rgba);

static void composite(AosPauseGraphics *graphics, unsigned background_index,
                      uint8_t *rgba) {
    const FusionAosRenderedBackground *page =
        &graphics->backgrounds[background_index];
    const uint8_t *palette =
        background_index == AOS_PAUSE_BACKGROUND_ENEMY_LIST ||
                background_index == AOS_PAUSE_BACKGROUND_ENEMY_DETAIL
            ? graphics->enemy_palette
            : graphics->palette;
    if (!page->rgba || page->width_pixels < AOS_PAUSE_WIDTH ||
        page->height_pixels < AOS_PAUSE_HEIGHT)
        return;
    for (unsigned y = 0; y < AOS_PAUSE_HEIGHT; ++y)
        for (unsigned x = 0; x < AOS_PAUSE_WIDTH; ++x) {
            size_t out = ((size_t)y * AOS_PAUSE_WIDTH + x) * 4;
            size_t background =
                ((size_t)y * page->width_pixels + x) * 4;
            memcpy(rgba + out, page->rgba + background, 4);
            bg0_pixel(graphics, palette, x, y, rgba + out);
        }
    draw_swipe(graphics, rgba);
    draw_flourish(graphics, rgba);
}

/* sub_08048C74 creates the hand entity and writes 0x1E directly to +0x65.
 * sub_08048AD8 then positions it at cell_x*8+6, cell_y*8+3, while
 * sub_08048D3C only adds the horizontal wiggle before committing that fixed
 * frame. Frame 0 is a different sprite (the black head/orb that was
 * previously rendered here), not the selection cursor. */
enum { AOS_PAUSE_CURSOR_FRAME = 0x1E };

static void draw_cursor(const AosPauseGraphics *graphics, uint8_t *rgba,
                        int cell_x, int cell_y) {
    if (!graphics->cursor_ready || graphics->cursor_sheet.anim_count == 0)
        return;
    const AosObjSheet *sheet = &graphics->cursor_sheet;
    unsigned frame_index = AOS_PAUSE_CURSOR_FRAME;
    if (frame_index >= sheet->frame_count) return;
    const AosObjFrame *frame = &sheet->frames[frame_index];
    int step = (int)((graphics->cursor_tick >> 1) & 7);
    int wiggle = (graphics->cursor_tick >> 4) & 1 ? 3 - step : step - 3;
    int anchor_x = cell_x * 8 + 6 + wiggle, anchor_y = cell_y * 8 + 3;
    for (unsigned k = 0; k < frame->count; ++k) {
        const AosObjPiece *piece = &frame->pieces[k];
        for (int py = 0; py < piece->height; ++py)
            for (int px = 0; px < piece->width; ++px) {
                int sx = piece->source_x +
                    (piece->hflip ? piece->width - 1 - px : px);
                int sy = piece->source_y +
                    (piece->vflip ? piece->height - 1 - py : py);
                unsigned color =
                    graphics->cursor_sheet.pixels[sy * AOS_OBJ_SHEET_SIZE + sx];
                if (!color || color > 15) continue;
                int x = anchor_x + piece->x + px;
                int y = anchor_y + piece->y + py;
                if (x < 0 || y < 0 || x >= AOS_PAUSE_WIDTH ||
                    y >= AOS_PAUSE_HEIGHT)
                    continue;
                uint16_t bgr = graphics->cursor_palette[color];
                uint8_t *out = rgba + ((size_t)y * AOS_PAUSE_WIDTH + x) * 4;
                out[0] = (uint8_t)(((bgr & 31) * 255) / 31);
                out[1] = (uint8_t)((((bgr >> 5) & 31) * 255) / 31);
                out[2] = (uint8_t)((((bgr >> 10) & 31) * 255) / 31);
                out[3] = 255;
            }
    }
}

/* ---- transient affine hand flourishes ----
 * sub_08048B0C arms entity +0xF34's +0x24 sub-state on every pause page
 * init that uses the press hand — Soul Set (anim arg 0x1F), Equip
 * (0x20), Item Use (0x21), Ability (0x22) and Config (0x24); hub, Sleep
 * and Enemy never call it — and sub_0804875C then runs the 16-update
 * sequence decoded in aos_affine_sprite.h: anchor pixel (0x35,9),
 * +0x0D timer 0..15, sub_0803E058(0, timer<<12, timer<<12, 0, 0, slot)
 * and the +0x59 affine bit cleared afterwards. The same call stores
 * the page's argument into entity +0x65, which sub_0804342C consumes
 * as a FRAME index — the press pages therefore draw dedicated wide
 * hand sprites (frames 0x1F-0x24 of the 0x08256EF8 descriptor), not
 * the live cursor pose; `fx_frame` carries that index.
 *
 * sub_08048108 is the Soul Set L/R transition machine (entity +0xFB8,
 * created by the page init and deleted on exit). sub_080486E0 flips
 * its +0x20 direction sign and bumps +0xA into state 3, which runs
 * 16 frames (+0xD = 0..15) committing the same hand frame THREE times
 * per update — once per sprite with its own 16.16 position (+0x40/
 * +0x44), affine scale (sub_0803E058, angle 0) and OBJ palette row
 * (+0x5A bits -> palettes 10/11/12, the red/blue/yellow rows the page
 * init loads from 0x0820C2C0 rows 2-4). The +0x65 pose is 1 when the
 * entity's own +0x10 category has an equipped soul (the
 * gEwramData+0x1325C+0xD/E/F bytes), else 0. Between slides (state 2)
 * the same three hands still draw every frame at the decoded base
 * pose. sub_0804933C's per-category palette/frame rewrite is native
 * dead code: it runs after sub_08048108's commits and every field it
 * touches (+0x40/+0x44/+0x5A/+0x65) is rewritten by the next update
 * before any draw, so the port does not reproduce it. Retriggers
 * mid-slide (native state 4) restart the 16-frame slide with the new
 * direction, matching the observable result. */
enum {
    AOS_FX_HUB,
    AOS_FX_SOUL_SET,
    AOS_FX_EQUIP,
    AOS_FX_ITEM_USE,
    AOS_FX_ABILITY,
    AOS_FX_SLEEP,
    AOS_FX_CONFIG,
    AOS_FX_ENEMY,
};

static void flourish_page(AosPauseGraphics *graphics, unsigned page) {
    if (graphics->fx_page == page) return;
    graphics->fx_page = (uint8_t)page;
    if (page == AOS_FX_SOUL_SET) {
        /* sub_08048D84's page init recreates entity +0xFB8: state 2,
         * +0xD/+0x2E zeroed, +0x20 = 0x8000 (direction +1) and the
         * entity's own +0x10 category counter back to 0. */
        graphics->fx_category_valid = false;
        graphics->fx_swipe_on = false;
        graphics->fx_swipe_t = 0;
        graphics->fx_swipe_dir = 1;
        graphics->fx_swipe_cat = 0;
    } else {
        /* The swipe entity only exists on the Soul Set page. */
        graphics->fx_swipe_on = false;
    }
    switch (page) {
    case AOS_FX_SOUL_SET:
        graphics->fx_frame = 0x1F;
        break;
    case AOS_FX_EQUIP:
        graphics->fx_frame = 0x20;
        break;
    case AOS_FX_ITEM_USE:
        graphics->fx_frame = 0x21;
        break;
    case AOS_FX_ABILITY:
        graphics->fx_frame = 0x22;
        break;
    case AOS_FX_CONFIG:
        graphics->fx_frame = 0x24;
        break;
    default:
        break;
    }
    switch (page) {
    case AOS_FX_SOUL_SET:
    case AOS_FX_EQUIP:
    case AOS_FX_ITEM_USE:
    case AOS_FX_ABILITY:
    case AOS_FX_CONFIG:
        graphics->fx_timer = 1;
        break;
    default:
        break;
    }
}

static void draw_flourish(AosPauseGraphics *graphics, uint8_t *rgba) {
    bool title_page = graphics->fx_page == AOS_FX_SOUL_SET ||
                      graphics->fx_page == AOS_FX_EQUIP ||
                      graphics->fx_page == AOS_FX_ITEM_USE ||
                      graphics->fx_page == AOS_FX_ABILITY ||
                      graphics->fx_page == AOS_FX_CONFIG;
    if (!title_page || !graphics->cursor_ready ||
        graphics->cursor_sheet.anim_count == 0)
        return;
    const AosObjSheet *sheet = &graphics->cursor_sheet;
    /* sub_08048B0C stores the page argument in entity +0x65, which
     * sub_0804342C resolves as a frame index (out-of-range falls back to
     * frame 0 natively). */
    unsigned frame_index = graphics->fx_frame < sheet->frame_count
                               ? graphics->fx_frame
                               : 0;
    if (frame_index >= sheet->frame_count) {
        if (++graphics->fx_timer > 16) graphics->fx_timer = 0;
        return;
    }
    /* Native +0x0D runs 0..15; the scale is timer<<12 in Q16.16, so the
     * hand grows from invisible to ~0.94 over the 16 updates. */
    unsigned t = graphics->fx_timer ? (unsigned)graphics->fx_timer - 1 : 16;
    int32_t scale = t >= 16 ? 0x10000 : (int32_t)t << 12;
    /* sub_0804875C state 1's fixed anchor: pixel (0x35,9). */
    int16_t params[4];
    aos_affine_matrix(0, scale, scale, false, false, params);
    aos_affine_sprite_draw(sheet, &sheet->frames[frame_index],
                           graphics->cursor_palette, 0x35, 9,
                           params, rgba, AOS_PAUSE_WIDTH, AOS_PAUSE_HEIGHT);
    if (graphics->fx_timer && ++graphics->fx_timer > 16)
        graphics->fx_timer = 0;
}

/* sub_08048108's three-commit slide. Each sprite i gets the decoded
 * 16.16 position (pos40/pos44), Q16.16 scale and palette row; the
 * anchor handed to the blit is the OAM coordinate — x wrapped into
 * the signed 9-bit range, y into 8 bits — which is where the native
 * engine places the sprite origin. `t` is the +0xD counter and the
 * +0x2E ramp reaches +-0x4000 over the 16 frames; `dir` is the +0x20
 * sign (+1 for the fall-through branch, -1 for the blt branch). */
static void swipe_hand(const AosPauseGraphics *graphics,
                       const AosObjFrame *frame, const uint16_t palette[16],
                       int32_t x16, int32_t y16, int32_t scale,
                       uint8_t *rgba) {
    /* The native coordinate is the 16.16 sum truncated toward zero,
     * then wrapped: x into the signed 9-bit OAM field, y into 8 bits. */
    int xi = x16 >= 0 ? (int)(x16 >> 16) : -(int)((-x16) >> 16);
    int yi = y16 >= 0 ? (int)(y16 >> 16) : -(int)((-y16) >> 16);
    int anchor_x = xi & 0x1FF;
    if (anchor_x & 0x100) anchor_x -= 0x200;
    int anchor_y = yi & 0xFF;
    int16_t params[4];
    aos_affine_matrix(0, scale, scale, false, false, params);
    aos_affine_sprite_draw(&graphics->cursor_sheet, frame, palette,
                           anchor_x, anchor_y, params, rgba,
                           AOS_PAUSE_WIDTH, AOS_PAUSE_HEIGHT);
}

static void draw_swipe(AosPauseGraphics *graphics, uint8_t *rgba) {
    if (graphics->fx_page != AOS_FX_SOUL_SET || !graphics->cursor_ready ||
        graphics->cursor_sheet.frame_count < 2)
        return;
    const AosObjSheet *sheet = &graphics->cursor_sheet;
    /* +0x65: frame 1 when the entity's +0x10 category has an equipped
     * soul, else frame 0 (fx_swipe_pose snapshots the native flag). */
    const AosObjFrame *frame =
        &sheet->frames[graphics->fx_swipe_pose ? 1 : 0];
    int dir = graphics->fx_swipe_dir;
    int32_t t = graphics->fx_swipe_on ? graphics->fx_swipe_t : 0;
    /* +0x2E ramps +-0x400 per state-3 frame, clamped at +-0x4000 (the
     * native 0x3FFF0000 bound); t <= 15 keeps it inside the ramp. */
    int32_t ramp = 0;
    if (graphics->fx_swipe_on) {
        int32_t step = (int32_t)(t + 1) * 0x400;
        if (step > 0x4000) step = 0x4000;
        ramp = dir * step;
    }
    int32_t scale_a = !graphics->fx_swipe_on ? 0xC000
                    : dir > 0 ? 0x10000 - 2 * ramp
                              : 0x10000 + ramp;
    int32_t scale_b = 0xC000 + ramp;
    int32_t scale_c = !graphics->fx_swipe_on ? 0x8000
                    : dir > 0 ? 0xC000 + ramp
                              : 0x8000 - 2 * ramp;
    int32_t ax, ay, bx, by, cx, cy;
    if (dir > 0) {
        ax = 0x54000 + 0xA000 * t;  ay = 0x200000 + 0x1C0000 * t;
        bx = 0x0B0000 + 0xA000 * t; by = 0x300000 - 0x10000 * t;
        cx = 0x7C0000 - 0x14000 * t; cy = 0x3C00000 - 0xC000 * t;
    } else {
        ax = 0x54000 - 0xA000 * t;  ay = 0x200000 + 0x10000 * t;
        bx = 0x0B0000 + 0x14000 * t; by = 0x300000 + 0xC000 * t;
        cx = 0x7C0000 - 0xA000 * t; cy = 0x3C00000 - 0x1C0000 * t;
    }
    /* Commit order A/B/C carries OBJ palettes 10/11/12 — the red, blue
     * and yellow rows. */
    swipe_hand(graphics, frame, graphics->swipe_palette[0],
               ax, ay, scale_a, rgba);
    swipe_hand(graphics, frame, graphics->swipe_palette[1],
               bx, by, scale_b, rgba);
    swipe_hand(graphics, frame, graphics->swipe_palette[2],
               cx, cy, scale_c, rgba);
}

/* One rendered frame of the swipe machine: the +0xD counter advances
 * after the draw and the +0x10 category steps +-1 (clamped 0..2) when
 * the slide ends, exactly like the native state-3 tail. */
static void swipe_advance(AosPauseGraphics *graphics) {
    if (!graphics->fx_swipe_on) return;
    if (++graphics->fx_swipe_t > 15) {
        graphics->fx_swipe_on = false;
        graphics->fx_swipe_t = 0;
        int cat = (int)graphics->fx_swipe_cat + graphics->fx_swipe_dir;
        if (cat < 0) cat = 0;
        if (cat > 2) cat = 2;
        graphics->fx_swipe_cat = (uint8_t)cat;
    }
}

/* Pause-menu OBJ-layer audit (Patch 1165). Every sub_08048804/
 * sub_080488FC spawn call in the page state machines was traced:
 *  - Item Use: sub_0804B6C4/sub_0804B808 each rebuild exactly ONE icon
 *    entity at pixel anchor (0x1C,0x94); sub_0804B494 emits names and
 *    quantities only, so native shows no per-slot icons. Ported below.
 *  - Equip: sub_0804A95C -> sub_0804A0B4 spawns the three equipped
 *    icons through sub_080488FC and sub_0804A00C/sub_0804AAF0 rebuild
 *    the selected preview through sub_08048804. Ported below.
 *  - Soul Set, Ability, Config, Sleep, hub and the Enemy list/detail:
 *    no icon spawn calls at all — their content is BG0 tile/text plus
 *    the hand cursor; the enemy detail's drop/soul grids are BG0
 *    strips (sub_0804CDF4/sub_0804D768 -> sub_08046DD4) and its
 *    description/soul band is sub_0804D1DC -> sub_08047330 text.
 *  - Title/frame strips on every page come from the decoded BG0
 *    tilemap, not OBJ.
 *  - The hand is a single entity family: sub_08048AD8 anchors it,
 *    sub_08048B0C/sub_08048C74 pick the per-language sheet, and the
 *    entity+0x65 argument selects within one animation (no per-page
 *    frames); sub_08048BC4's flag is the icon-overlay hand-hide bit,
 *    not a flip. Patch 1166 ports the Enemy portrait entity's
 *    per-enemy update: sub_0804CA18 stores the record's +4 update
 *    (e.g. 0x080AD645) and the port retains the created probe and
 *    re-runs aos_enemy_step_anim once per pause frame in
 *    aria_menus_debug.c, so animated enemies cycle frames instead of
 *    keeping the create frame. Patch 1171 ports the two transient
 *    affine flourishes through aos_affine_sprite.c (the decoded
 *    sub_0803E058/sub_0803E968 Q8.8 matrix): sub_0804875C's 16-step
 *    press sequence — armed by sub_08048B0C on page entry (Soul Set
 *    0x1F, Equip 0x20, Item Use 0x21, Ability 0x22, Config 0x24; hub,
 *    Sleep and Enemy never call it), anchored at pixel (0x35,9),
 *    timer 0..15 driving sub_0803E058(0, timer<<12, timer<<12, 0, 0,
 *    slot) then the +0x59 affine bit cleared — and a scoped stand-in
 *    for the Soul Set L/R transition hand: sub_08048108's three-sprite
 *    slide machine collapses to one hand running the same scale ramp
 *    while drifting toward the newly selected category label, fired
 *    when render_soul_set observes the category delta that native
 *    input bits 0x40/0x80 feed into sub_080486E0. Patch (this change)
 *    replaces that stand-in with the decoded machine: all three
 *    commits draw every Soul Set frame — idle included — with the
 *    native 16.16 positions, per-sprite Q16.16 scales, OBJ palettes
 *    10/11/12 and the +0x65 = 0/1 equipped-soul pose pick; the press
 *    flourish now uses the native +0x65 page frames (0x1F-0x24)
 *    instead of the live cursor pose. sub_0804933C's per-category
 *    palette/animation swap is documented native dead code (every
 *    field it writes is overwritten by the next sub_08048108 commit
 *    before any draw). Remaining approximations: the swipe keeps only
 *    the visible essence of the native state machine — the +0x14/
 *    +0x18/+0x1C accumulators, the +0x38 retrigger flag and state 4's
 *    exact re-arm timing are folded into an immediate restart — and
 *    piece-level OAM coordinate wrap is approximated by wrapping only
 *    the sprite anchor. */

/* sub_08048804/sub_080488FC's icon entity: icon n is the 16x16 block
 * (n-1)%64 of sheet (n-1)/64 (eight icons per row of 16 px), sampled
 * from the 4bpp sheets loaded at init and painted with palette bank 2
 * of 0x082099FC — the same extraction the asset exporter uses for
 * Icon/i<n>. The caller passes the on-screen top-left pixel. */
static void draw_icon(const AosPauseGraphics *graphics, uint8_t *rgba,
                      unsigned icon, int x0, int y0) {
    if (!graphics->icons_ready || icon < 1 || icon > 192) return;
    const uint8_t *sheet = graphics->icon_tiles[(icon - 1) / 64];
    unsigned slot = (icon - 1) % 64;
    unsigned origin_x = (slot % 8) * 16, origin_y = (slot / 8) * 16;
    for (unsigned py = 0; py < 16; ++py)
        for (unsigned px = 0; px < 16; ++px) {
            unsigned sx = origin_x + px, sy = origin_y + py;
            const uint8_t *tile = sheet +
                ((size_t)(sy / 8) * 16 + sx / 8) * 32;
            uint8_t packed = tile[(sy % 8) * 4 + (sx % 8) / 2];
            unsigned color_index = (packed >> (4 * (sx & 1))) & 0xF;
            if (!color_index) continue;
            int x = x0 + (int)px, y = y0 + (int)py;
            if (x < 0 || y < 0 || x >= AOS_PAUSE_WIDTH ||
                y >= AOS_PAUSE_HEIGHT)
                continue;
            uint16_t bgr = graphics->icon_palette[color_index];
            uint8_t *out = rgba + ((size_t)y * AOS_PAUSE_WIDTH + x) * 4;
            out[0] = (uint8_t)(((bgr & 31) * 255) / 31);
            out[1] = (uint8_t)((((bgr >> 5) & 31) * 255) / 31);
            out[2] = (uint8_t)((((bgr >> 10) & 31) * 255) / 31);
            out[3] = 255;
        }
}

static void window_fill(AosPauseGraphics *graphics, unsigned column,
                        unsigned row, uint16_t tile_base, unsigned dim_a,
                        unsigned dim_b);
static bool place_text_id(AosPauseGraphics *graphics, uint16_t id, bool small,
                          unsigned x, unsigned y, unsigned width,
                          unsigned palette, unsigned *next_tile);
static bool place_text_cells(AosPauseGraphics *graphics, uint16_t id,
                             bool small, unsigned tile_x, unsigned tile_y,
                             uint16_t col, uint16_t row, uint16_t w,
                             uint16_t h, unsigned palette,
                             unsigned *next_tile);
static bool cells_begin(AosPauseGraphics *graphics, bool small,
                        uint16_t col, uint16_t row, uint16_t w,
                        uint16_t h);
static bool cells_print_id(AosPauseGraphics *graphics, uint16_t id,
                           bool small, uint16_t cur_col, uint16_t cur_row);
static bool cells_commit(AosPauseGraphics *graphics, unsigned tile_x,
                         unsigned tile_y, uint16_t col, uint16_t row,
                         uint16_t w, uint16_t h, unsigned palette,
                         unsigned *next_tile);

static void reset_bg0(AosPauseGraphics *graphics, bool enemy) {
    ++graphics->cursor_tick;
    memcpy(graphics->bg0_chars,
           enemy ? graphics->enemy_bg0_base_chars
                 : graphics->bg0_base_chars,
           sizeof graphics->bg0_chars);
    for (size_t index = 0; index < 32 * 32; ++index)
        graphics->bg0_map[index] = AOS_PAUSE_BG0_FILL;
}

static void clear_bg0_tiles(AosPauseGraphics *graphics,
                            unsigned first, unsigned count) {
    if (first >= 0x400 || count > 0x400 - first) return;
    memset(graphics->bg0_chars + (size_t)first * 32, 0,
           (size_t)count * 32);
}

bool aos_pause_graphics_render_hub(AosPauseGraphics *graphics,
                                   const AosPauseHubValues *values,
                                   uint8_t selected, uint8_t *rgba,
                                   size_t rgba_size) {
    if (!values || selected >= 7 ||
        !valid_output(graphics, rgba, rgba_size))
        return false;
    flourish_page(graphics, AOS_FX_HUB);
    reset_bg0(graphics, false);

    /* sub_08047C4C: seven six-tile labels, palette 3 selected and palette 7
     * otherwise. The tile sequence is the language bank at VRAM 0xA000. */
    for (unsigned entry = 0; entry < 7; ++entry)
        put_tiles(graphics, 2, 12 + (int)entry,
                  (uint16_t)((0x340 + entry * 0x10) |
                             (entry == selected ? 3 : 7) << 12), 6);

    put_number(graphics, 14, 2, values->hp, 5, 0x3300, false);
    put_number(graphics, 14, 3, values->mp, 5, 0x3300, false);
    put_number(graphics, 19, 2, values->max_hp, 5, 0x3300, false);
    put_number(graphics, 19, 3, values->max_mp, 5, 0x3300, false);
    put_meter(graphics, 21, 2, values->hp, values->max_hp, 3);
    put_meter(graphics, 21, 3, values->mp, values->max_mp, 4);
    for (int stat = 0; stat < 4; ++stat)
        put_number(graphics, 14, 5 + stat, values->stats[stat], 3, 0x3300, false);
    put_number(graphics, 24, 5, values->attack, 3, 0x3300, false);
    put_number(graphics, 24, 6, values->defence, 3, 0x3300, false);
    uint32_t seconds = values->play_frames / 60;
    put_number(graphics, 28, 8, (int32_t)(seconds % 60), 2, 0x3300, true);
    put_number(graphics, 25, 8, (int32_t)(seconds / 60 % 60), 2, 0x3300, true);
    put_number(graphics, 22, 8, (int32_t)(seconds / 3600), 2, 0x3300, true);
    put_large_number(graphics, 5, 9, values->level, 2, 3);
    put_number(graphics, 26, 10, (int32_t)values->experience, 6, 0x3300, false);
    put_number(graphics, 25, 12, (int32_t)values->next_experience, 8, 0x3300, false);
    put_number(graphics, 25, 13, (int32_t)values->gold, 8, 0x3300, false);
    /* sub_080471E4: normal/poison/curse/stone status is a four-tile word. */
    unsigned status_variant = values->status & 1 ? 2
                              : values->status & 2 ? 3
                              : values->status & 4 ? 1 : 0;
    put_tiles(graphics, 12, 9, (uint16_t)(0x330A + status_variant * 0x10), 4);

    /* sub_08047520 builds the 19x5-cell description frame at tilemap
     * column 10, row 15 before sub_08047C4C prints text 0x362+selected.
     * This is deliberately separate from the label rows at columns 2..7. */
    window_fill(graphics, 10, 15, 0x3200, 0x1A, 3);
    /* sub_08047C4C's tail: sub_0804728C(0x362 + selected) prints the
     * selected entry's description into the bottom message window
     * through the native cell engine. The native caller runs
     * sub_08040748(0,0,0x1a,3) + sub_08040FE0 then sub_0804728C, so
     * the text window is cols 0-25 (6px units) x rows 0-2 (12px
     * units) and the cursor starts at the window home; sub_08046BC8
     * restores it on exit. The cell canvas is mapped at tile (10,15),
     * matching sub_08047520's frame rather than the label column. */
    unsigned next_tile = AOS_TEXT_TILE_FIRST;
    if (!place_text_cells(graphics, (uint16_t)(0x362 + selected), false,
                          10, 15, 0, 0, 26, 3, 3, &next_tile))
        return false;

    composite(graphics, AOS_PAUSE_BACKGROUND_HUB, rgba);
    /* sub_08047818's hub placement: sub_08048AD8(1, selected + 0xC). */
    draw_cursor(graphics, rgba, 1, 12 + (int)selected);
    return true;
}

bool aos_pause_graphics_render_sleep_hub(
    AosPauseGraphics *graphics, const AosPauseHubValues *values,
    bool can_sleep, uint8_t choice, unsigned sleep_result,
    uint8_t *rgba, size_t rgba_size) {
    if (!values || choice > 1 ||
        !aos_pause_graphics_render_hub(graphics, values, AOS_PAUSE_HUB_SLEEP,
                                       rgba, rgba_size))
        return false;
    uint16_t message = can_sleep ? 0x36A : 0x361;
    if (sleep_result == AOS_PAUSE_SLEEP_RESULT_SAVED)
        message = 0x36B;
    else if (sleep_result == AOS_PAUSE_SLEEP_RESULT_FAILED)
        message = 0x386;
    unsigned next_tile = AOS_TEXT_TILE_FIRST;
    if (!cells_begin(graphics, false, 0, 0, 0x1A, 3) ||
        !cells_print_id(graphics, message, false, 0, 0))
        return false;
    if (can_sleep && sleep_result == AOS_PAUSE_SLEEP_RESULT_NONE &&
        (!cells_print_id(graphics, 0x34B, false, 4, 1) ||
         !cells_print_id(graphics, 0x34D, false, 12, 1)))
        return false;
    if (!cells_commit(graphics, 10, 15, 0, 0, 0x1A, 3, 3,
                      &next_tile))
        return false;
    composite(graphics, AOS_PAUSE_BACKGROUND_HUB, rgba);
    draw_cursor(graphics, rgba, 1, 12 + AOS_PAUSE_HUB_SLEEP);
    if (can_sleep && sleep_result == AOS_PAUSE_SLEEP_RESULT_NONE)
        draw_cursor(graphics, rgba, choice ? 18 : 12, 17);
    return true;
}

static bool read_text(AosPauseGraphics *graphics, uint16_t id,
                      AosNativeText *text) {
    return aos_intro_resources_read_text(&graphics->text, graphics->language,
                                         id, text, NULL, 0);
}

static bool place_text(AosPauseGraphics *graphics, const AosNativeText *text,
                       bool small, unsigned x, unsigned y,
                       unsigned width, unsigned palette,
                       unsigned *next_tile) {
    if (!graphics || !text || !next_tile || !text->line_count || x >= 240 ||
        y >= 160 || !width)
        return false;
    size_t line_count = text->line_count;
    if (line_count > (small ? 1u : 2u)) line_count = small ? 1u : 2u;
    unsigned offset_x = x & 7u, offset_y = y & 7u;
    unsigned line_height = small ? 8u : 12u;
    unsigned height = offset_y + (unsigned)line_count * line_height;
    unsigned tile_width = (offset_x + width + 7u) / 8u;
    unsigned tile_height = (height + 7u) / 8u;
    if (*next_tile < AOS_TEXT_TILE_FIRST ||
        *next_tile + tile_width * tile_height > AOS_TEXT_TILE_LIMIT)
        return false;

    aos_intro_text_tiles_clear(&graphics->text_tiles);
    for (size_t line = 0; line < line_count; ++line) {
        bool rendered = small
            ? aos_intro_font_render_small_span(
                  &graphics->font, &graphics->text_tiles, offset_x,
                  offset_y + (unsigned)line * line_height, width, 1,
                  text->glyphs[line], text->glyph_counts[line])
            : aos_intro_font_render_span(
                  &graphics->font, &graphics->text_tiles, offset_x,
                  offset_y + (unsigned)line * line_height, width, 1,
                  text->glyphs[line], text->glyph_counts[line]);
        if (!rendered) return false;
    }

    unsigned destination_x = x / 8u, destination_y = y / 8u;
    for (unsigned row = 0; row < tile_height; ++row)
        for (unsigned column = 0; column < tile_width; ++column) {
            size_t source = ((size_t)row * 32 + column) * 32;
            const uint8_t *glyph = graphics->text_tiles.data + source;
            /* Like the native windowed cell commit, only cells holding
             * glyph pixels consume a tile: empty span cells keep the
             * existing map entry (and its palette) instead of being
             * rewritten as blank text tiles over strips and frames. */
            bool empty = true;
            for (unsigned byte = 0; byte < 32; ++byte)
                if (glyph[byte]) { empty = false; break; }
            if (empty) continue;
            unsigned tile = (*next_tile)++;
            unsigned map_x = destination_x + column;
            unsigned map_y = destination_y + row;
            uint8_t *destination =
                graphics->bg0_chars + (size_t)tile * 32;
            if (map_x < 32 && map_y < 32) {
                uint16_t previous = graphics->bg0_map[map_y * 32 + map_x];
                memcpy(destination,
                       graphics->bg0_chars + (size_t)(previous & 0x3FF) * 32,
                       32);
                for (unsigned byte = 0; byte < 32; ++byte) {
                    uint8_t low = glyph[byte] & 0x0F;
                    uint8_t high = glyph[byte] & 0xF0;
                    if (low) destination[byte] =
                        (uint8_t)((destination[byte] & 0xF0) | low);
                    if (high) destination[byte] =
                        (uint8_t)((destination[byte] & 0x0F) | high);
                }
                graphics->bg0_map[map_y * 32 + map_x] =
                    (uint16_t)(tile | palette << 12);
            }
        }
    return true;
}

static uint16_t soul_name_id(unsigned category, unsigned soul) {
    static const uint16_t bases[] = {0xE2, 0x11A, 0x132};
    return soul ? (uint16_t)(bases[category] + soul) : 0x34F;
}

static uint16_t soul_description_id(unsigned category, unsigned soul) {
    /* sub_08047330's bases minus one: 0x1E3/0x21B/0x233 for categories
     * 0-2 and 0x256 for category 3 (enemy 0x53 drops a type-3 soul). */
    static const uint16_t bases[] = {0x1E3, 0x21B, 0x233, 0x256};
    static const uint16_t none[] = {0x37D, 0x37E, 0x37F, 0x37F};
    return soul ? (uint16_t)(bases[category & 3] + soul) : none[category & 3];
}

/* Cell-engine text path: run a localized entry through the native
 * software cell renderer, then commit each non-empty framebuffer cell
 * as a BG0 tile. `tile_x`/`tile_y` are the BG0 map origin of the cell
 * canvas; `col`/`row`/`w`/`h` are the context-A window arguments fed
 * to sub_08040748 (6px columns, 12px rows). Only cells containing
 * glyph pixels consume tiles from `next_tile` in native's 0x200..0x2FF
 * scratch character region; empty cells keep the existing map entry. */
static bool cells_begin(AosPauseGraphics *graphics, bool small,
                        uint16_t col, uint16_t row, uint16_t w,
                        uint16_t h) {
    if (!graphics) return false;
    bool ready = small ? (graphics->cells_char_b && graphics->cells_glyph_b)
                       : (graphics->cells_char_a && graphics->cells_glyph_a);
    if (!ready) return false;
    memset(graphics->cells_fb, 0, sizeof graphics->cells_fb);
    aos_menu_cells_set_attr(&graphics->cells, 1);
    aos_menu_cells_set_ink(&graphics->cells, 0);
    aos_menu_cells_window_a(&graphics->cells, col, row, w, h);
    return true;
}

static bool cells_print_id(AosPauseGraphics *graphics, uint16_t id,
                           bool small, uint16_t cur_col, uint16_t cur_row) {
    const uint8_t *entry = NULL;
    size_t entry_size = 0;
    if (!aos_intro_resources_entry(&graphics->text, graphics->language,
                                   id, &entry, &entry_size))
        return false;
    aos_menu_cells_cursor_a(&graphics->cells, cur_col, cur_row);
    if (small)
        aos_menu_cells_run_b(&graphics->cells, entry, 0,
                             graphics->cells_char_b, 4,
                             graphics->cells_glyph_b, 10);
    else
        aos_menu_cells_run_a(&graphics->cells, entry, 0,
                             graphics->cells_char_a, 4,
                             graphics->cells_glyph_a, 14);
    return true;
}

/* Native sub_080412DC points the software renderer directly at VRAM
 * 0x06008000. A framebuffer cell (cx,cy) is therefore character tile
 * 0x200 + cy*32 + cx; page strip/window tilemaps already reference those
 * fixed characters. Copying cells there, without rewriting the screen map,
 * is what makes the same text canvas appear at each page's native anchor. */
static bool cells_commit_range(AosPauseGraphics *graphics,
                               unsigned tile_x, unsigned tile_y,
                               unsigned cx0, unsigned cy0, unsigned cx1,
                               unsigned cy1, unsigned palette,
                               unsigned *next_tile) {
    if (!graphics || !next_tile) return false;
    (void)tile_x;
    (void)tile_y;
    (void)palette;
    for (unsigned cy = cy0; cy <= cy1 && cy < 32; ++cy)
        for (unsigned cx = cx0; cx <= cx1 && cx < 32; ++cx) {
            unsigned tile = AOS_TEXT_TILE_FIRST + cy * 32 + cx;
            if (tile >= AOS_TEXT_TILE_LIMIT)
                return false;
            uint8_t tile_data[32];
            if (!aos_menu_cells_cell_tile(&graphics->cells,
                                          (uint16_t)cx, (uint16_t)cy,
                                          tile_data))
                return false;
            memcpy(graphics->bg0_chars + (size_t)tile * 32,
                   tile_data, sizeof tile_data);
        }
    *next_tile = AOS_TEXT_TILE_FIRST;
    return true;
}

/* Context-B begin: sub_08040970 window + sub_08041204 rect fill,
 * leaving the cursor at the window origin like the native pages. */
static bool cells_begin_b(AosPauseGraphics *graphics, uint16_t x,
                          uint16_t y, uint16_t w, uint16_t h) {
    if (!graphics) return false;
    if (!(graphics->cells_char_b && graphics->cells_glyph_b))
        return false;
    memset(graphics->cells_fb, 0, sizeof graphics->cells_fb);
    aos_menu_cells_set_attr(&graphics->cells, 1);
    aos_menu_cells_set_ink(&graphics->cells, 0);
    aos_menu_cells_cursor_b(&graphics->cells, 0, 0);
    aos_menu_cells_window_b(&graphics->cells, x, y, w, h);
    aos_menu_cells_icon_fill_rect(&graphics->cells);
    return true;
}

/* One context-B glyph through the native printer (sub_08040C60). */
static bool cells_print_glyph_b(AosPauseGraphics *graphics, uint16_t ch) {
    if (!graphics || ch < 0x20 || ch >= 0x100) return false;
    unsigned index = ch - 0x20;
    const uint8_t *ci = graphics->cells_char_b + index * 4;
    const uint8_t *gl = graphics->cells_glyph_b + index * 10;
    return aos_menu_cells_print_b(&graphics->cells, ch, ci, gl) == -1;
}

static bool cells_commit(AosPauseGraphics *graphics, unsigned tile_x,
                         unsigned tile_y, uint16_t col, uint16_t row,
                         uint16_t w, uint16_t h, unsigned palette,
                         unsigned *next_tile) {
    if (!graphics || !next_tile || tile_x >= 32 || tile_y >= 32)
        return false;
    /* The window covers canvas pixels [col*6, (col+w-2)*6+6) x
     * [row*12, (row+h-1)*12+12): sub_08040748 stores x1 = col+w-2 and
     * y1 = row+h-1 and sub_08040FE0 iterates the home..x1 columns and
     * home..y1 rows, so context-A commits span w-1 text columns but h
     * text rows. Cells are the 8px framebuffer grid. */
    unsigned px_x0 = col * 6;
    int px_x1 = (int)(col + w - 2) * 6 + 6;
    unsigned px_y0 = row * 12, px_y1 = (unsigned)(row + h - 1) * 12 + 12;
    if (px_x1 <= (int)px_x0) return true;
    unsigned cx0 = px_x0 / 8, cx1 = (unsigned)(px_x1 - 1) / 8;
    unsigned cy0 = px_y0 / 8, cy1 = (px_y1 - 1) / 8;
    return cells_commit_range(graphics, tile_x, tile_y, cx0, cy0, cx1,
                              cy1, palette, next_tile);
}

/* Context-B variant: window bounds are 6px columns / 8px rows, so the
 * covered cells come from b_x0*6 .. b_x1*6+6 and b_y0*8 .. b_y1*8+8. */
static bool cells_commit_b(AosPauseGraphics *graphics, unsigned tile_x,
                           unsigned tile_y, unsigned palette,
                           unsigned *next_tile) {
    if (!graphics || !next_tile) return false;
    unsigned cx0 = graphics->cells.b_x0 * 6 / 8;
    unsigned cx1 = (graphics->cells.b_x1 * 6 + 6 - 1) / 8;
    unsigned cy0 = graphics->cells.b_y0;
    unsigned cy1 = graphics->cells.b_y1;
    return cells_commit_range(graphics, tile_x, tile_y, cx0, cy0, cx1,
                              cy1, palette, next_tile);
}

static bool place_text_cells(AosPauseGraphics *graphics, uint16_t id,
                             bool small, unsigned tile_x, unsigned tile_y,
                             uint16_t col, uint16_t row, uint16_t w,
                             uint16_t h, unsigned palette,
                             unsigned *next_tile) {
    if (!cells_begin(graphics, small, col, row, w, h)) return false;
    /* Context-A page descriptions keep sub_08047520's native pixel
     * attribute 1. `palette` belongs to the BG0 tilemap entries already
     * installed by window_fill; it is not the glyph's 4bpp pixel value. */
    return cells_print_id(graphics, id, small, col, row) &&
           cells_commit(graphics, tile_x, tile_y, col, row, w, h,
                        palette, next_tile);
}

static bool place_text_id(AosPauseGraphics *graphics, uint16_t id, bool small,
                          unsigned x, unsigned y, unsigned width,
                          unsigned palette, unsigned *next_tile) {
    if (small) {
        const uint8_t *entry = NULL;
        size_t entry_size = 0;
        uint16_t col = (uint16_t)(x / 6), row = (uint16_t)(y / 8);
        uint16_t w = (uint16_t)((width + 5) / 6 + 1);
        if (!aos_intro_resources_entry(&graphics->text, graphics->language,
                                       id, &entry, &entry_size) ||
            !cells_begin_b(graphics, col, row, w, 1))
            return false;
        aos_menu_cells_set_attr(&graphics->cells, (uint16_t)palette);
        /* sub_0804090C accepts exact pixel X, unlike the six-pixel window
         * origin (notably the right column uses x=128 with window col 21). */
        graphics->cells.b_col = (uint16_t)(x / 6);
        graphics->cells.b_sx = (uint16_t)(x % 6);
        graphics->cells.b_px = (uint16_t)x;
        graphics->cells.b_row = row;
        graphics->cells.b_py = (uint16_t)(row * 8);
        aos_menu_cells_run_b(&graphics->cells, entry, 0,
                             graphics->cells_char_b, 4,
                             graphics->cells_glyph_b, 10);
        return cells_commit_b(graphics, 0, 0, palette, next_tile);
    }
    AosNativeText text;
    return read_text(graphics, id, &text) &&
           place_text(graphics, &text, small, x, y, width, palette, next_tile);
}

bool aos_pause_graphics_render_soul_set(
    AosPauseGraphics *graphics, const AosPauseSoulSetValues *values,
    uint8_t *rgba, size_t rgba_size) {
    static const unsigned soul_counts[] = {56, 25, 36};
    static const unsigned selected_palettes[] = {8, 9, 6};
    if (!values || values->category >= AOS_PAUSE_SOUL_FAMILIES ||
        !graphics || !graphics->text_ready ||
        !valid_output(graphics, rgba, rgba_size))
        return false;
    flourish_page(graphics, AOS_FX_SOUL_SET);
    /* sub_080486E0 category watch: the native L/R input bits 0x40/0x80
     * pass direction -1/+1 into the Soul Set transition machine
     * (sub_08048108, entity +0xFB8), flipping its +0x20 sign and
     * bumping +0xA into the 16-frame state-3 slide. The port observes
     * the resulting category delta on this page's values instead of
     * raw input; a mid-slide delta restarts the slide with the new
     * direction, matching the native state-4 retrigger's observable
     * result. */
    if (graphics->fx_category_valid &&
        values->category != graphics->fx_category) {
        graphics->fx_swipe_dir =
            values->category > graphics->fx_category ? 1 : -1;
        graphics->fx_swipe_t = 0;
        graphics->fx_swipe_on = true;
    }
    graphics->fx_category = values->category;
    graphics->fx_category_valid = true;
    /* The +0x65 pose pick reads the entity's own +0x10 category (the
     * native gEwramData+0x1325C+0xD/E/F equipped-soul bytes), which
     * lags the menu category by one slide. */
    graphics->fx_swipe_pose =
        graphics->fx_swipe_cat < AOS_PAUSE_SOUL_FAMILIES &&
        values->equipped[graphics->fx_swipe_cat] != 0;
    reset_bg0(graphics, false);
    /* sub_08041204 clears the language-bank placeholders before the small
     * font renderer rewrites their character data. */
    clear_bg0_tiles(graphics, 0x380, 0x80);

    put_number(graphics, 22, 2, values->attack, 3, 0x3300, false);
    put_number(graphics, 22, 3, values->defence, 3, 0x3300, false);
    for (int stat = 0; stat < 4; ++stat)
        put_number(graphics, 22, 4 + stat, values->stats[stat], 3, 0x3300, false);
    unsigned status_variant = values->status & 1 ? 2
                              : values->status & 2 ? 3
                              : values->status & 4 ? 1 : 0;
    put_tiles(graphics, 21, 8, (uint16_t)(0x330A + status_variant * 0x10), 4);

    /* sub_08048D84: sub_080472B0(7,0xF,0x3200,0x1E,2) fills the
     * description window rect at tilemap (7,15), 22x3 tiles, and sets
     * the context-A window (0,0,0x1E,2) whose home maps to pixel
     * (56,120) — 29 cols * 6px = 174 px wide. */
    window_fill(graphics, 7, 15, 0x3200, 0x1E, 2);
    /* Category label strips at (6, 3+2*cat) then eight soul strips at
     * columns 3/0x11 rows 10+entry/2 (0x3380+entry*0x10), matching the
     * native 46DD4 loop. */
    for (unsigned category = 0; category < AOS_PAUSE_SOUL_FAMILIES; ++category) {
        unsigned palette = category == values->category
                               ? selected_palettes[category]
                               : 3;
        put_tiles(graphics, 6, 3 + (int)category * 2,
                  (uint16_t)((0x350 + category * 0x10) | (palette << 12)), 10);
    }
    for (unsigned entry = 0; entry < 8; ++entry)
        put_tiles(graphics, entry & 1u ? 17 : 3,
                  10 + (int)(entry / 2),
                  (uint16_t)(0x3380 + entry * 0x10), 10);

    /* sub_080472B0's tail sets the context-A window (0,0,0x1E,2) after
     * filling the strip at tilemap (7,15); the description then prints
     * through sub_0804728C -> sub_08046BC8 on that window. The cell
     * canvas origin is the strip's top-left tile (7,15): the covered
     * cells land exactly on tiles 7..28 x 15..17, the strip itself. */
    unsigned next_tile = AOS_TEXT_TILE_FIRST;
    for (unsigned category = 0; category < AOS_PAUSE_SOUL_FAMILIES; ++category) {
        unsigned x = category == 0 ? 126 : category == 1 ? 0 : 126;
        unsigned y = category == 0 ? 80 : 88;
        unsigned palette = category == values->category
                               ? selected_palettes[category]
                               : 3;
        if (!place_text_id(graphics,
                           soul_name_id(category, values->equipped[category]),
                           true, x, y, 84, palette, &next_tile))
            return false;
    }

    /* sub_080494F8 orders the list: the category's recent-soul cache
     * (sub_08049E64, four ids per family) leads with its owned entries in
     * acquisition order, then the remaining owned souls by index. */
    unsigned available[AOS_PAUSE_SOUL_RECORDS];
    unsigned available_count = 1;
    available[0] = 0;
    const uint8_t *recent = values->recent_souls
                                ? values->recent_souls->soul[values->category]
                                : NULL;
    if (recent)
        for (unsigned slot = 0; slot < AOS_PICKUP_RECENT_ITEMS; ++slot) {
            unsigned soul = recent[slot];
            if (soul == AOS_EQUIPMENT_NONE ||
                soul >= soul_counts[values->category] ||
                !values->counts[values->category][soul - 1])
                continue;
            bool duplicate = false;
            for (unsigned index = 1; index < available_count; ++index)
                if (available[index] == soul) duplicate = true;
            if (!duplicate) available[available_count++] = soul;
        }
    for (unsigned soul = 1; soul < soul_counts[values->category]; ++soul) {
        if (!values->counts[values->category][soul - 1]) continue;
        bool listed = false;
        if (recent)
            for (unsigned slot = 0; slot < AOS_PICKUP_RECENT_ITEMS; ++slot)
                if (recent[slot] == soul) listed = true;
        if (!listed) available[available_count++] = soul;
    }
    unsigned selected_soul = values->cursor[values->category];
    unsigned selected = 0;
    for (unsigned index = 0; index < available_count; ++index)
        if (available[index] == selected_soul) selected = index;
    unsigned top = selected > 3 ? selected - 3 : 0;
    if (top + 8 > available_count)
        top = available_count > 8 ? available_count - 8 : 0;
    for (unsigned entry = 0; entry < 8 && top + entry < available_count; ++entry) {
        unsigned soul = available[top + entry];
        unsigned x = entry & 1u ? 126 : 0;
        unsigned y = (12 + entry / 2) * 8;
        unsigned palette = soul == selected_soul
                               ? selected_palettes[values->category]
                               : 3;
        if (!place_text_id(graphics,
                           soul_name_id(values->category, soul), true,
                           x, y, 84, palette, &next_tile))
            return false;
        put_number(graphics, entry & 1u ? 27 : 13,
                   10 + (int)(entry / 2),
                   soul ? values->counts[values->category][soul - 1] : 0,
                   1, 0x3300, false);
    }
    /* Same native path as the hub tail: sub_0804728C on the
     * (0,0,0x1E,2) context-A window sub_080472B0 configured, with the
     * cell canvas rooted at the strip's tilemap origin (7,15). */
    if (!place_text_cells(graphics,
                          soul_description_id(values->category, selected_soul),
                          false, 7, 15, 0, 0, 0x1E, 2, 3, &next_tile))
        return false;

    composite(graphics, AOS_PAUSE_BACKGROUND_SOUL_SET, rgba);
    swipe_advance(graphics);
    /* sub_08048EA4: column 2 or 0x10 on the flat slot's bit 0, row
     * 0xA + slot/2 where slot = selected - top in the eight-entry list. */
    draw_cursor(graphics, rgba, (selected - top) & 1 ? 0x10 : 2,
                0xA + (int)(selected - top) / 2);
    return true;
}

static bool rom_u16(const AosPauseGraphics *graphics, uint32_t address,
                    uint16_t *value) {
    if (!graphics || !graphics->text.rom || !graphics->text.rom->data ||
        !value || address < AOS_ROM_BASE)
        return false;
    size_t offset = (size_t)(address - AOS_ROM_BASE);
    if (offset + 2 > graphics->text.rom->size) return false;
    const uint8_t *bytes = graphics->text.rom->data + offset;
    *value = (uint16_t)(bytes[0] | bytes[1] << 8);
    return true;
}

static bool equipment_text_ids(const AosPauseGraphics *graphics,
                               unsigned slot, unsigned item,
                               uint16_t *name, uint16_t *description) {
    if (slot >= AOS_PAUSE_EQUIP_SLOTS || !name || !description) return false;
    if (item == AOS_PAUSE_EQUIP_NONE) {
        *name = 0x34F;
        *description = 0x351;
        return true;
    }
    if ((slot == 0 && item >= 59) || (slot != 0 && item >= 45)) return false;
    uint32_t records = slot == 0 ? AOS_WEAPON_TABLE : AOS_ARMOR_TABLE;
    unsigned record_size = slot == 0 ? 28 : 20;
    uint16_t text_index;
    return rom_u16(graphics, records + item * record_size, &text_index) &&
           rom_u16(graphics, AOS_EQUIPMENT_NAME_IDS + text_index * 2, name) &&
           rom_u16(graphics,
                   AOS_EQUIPMENT_DESCRIPTION_IDS + text_index * 2,
                   description);
}

/* The icon id a weapon/armor record shows: byte +0x2 of its stats
 * record (sub_08044E94 kind 3/4 -> the record itself, sub_0801232C
 * reads +0x2). Slot 0 is the weapon table, slots 1-2 armor. */
static bool equipment_icon_id(const AosPauseGraphics *graphics,
                              unsigned slot, unsigned item,
                              unsigned *icon) {
    if (slot >= AOS_PAUSE_EQUIP_SLOTS || !icon ||
        item == AOS_PAUSE_EQUIP_NONE ||
        (slot == 0 && item >= 59) || (slot != 0 && item >= 45))
        return false;
    const FusionRom *rom = graphics->text.rom;
    uint32_t records = slot == 0 ? AOS_WEAPON_TABLE : AOS_ARMOR_TABLE;
    unsigned stride = slot == 0 ? 28 : 20;
    size_t offset = (size_t)(records + item * stride + 2 - AOS_ROM_BASE);
    if (!rom || !rom->data || offset + 1 > rom->size) return false;
    *icon = rom->data[offset];
    return true;
}

static void put_comparison(AosPauseGraphics *graphics, int y,
                           int16_t current, int16_t preview) {
    unsigned kind = preview == current ? 0 : preview > current ? 1 : 2;
    static const uint16_t tiles[] = {0x34C, 0x34A, 0x34B};
    static const unsigned palettes[] = {3, 9, 8};
    put_tiles(graphics, 24, y,
              (uint16_t)(tiles[kind] | palettes[kind] << 12), 2);
    put_number(graphics, 27, y, preview, 3, 0x3300, false);
}

bool aos_pause_graphics_render_equip(AosPauseGraphics *graphics,
                                     const AosPauseEquipValues *values,
                                     uint8_t *rgba, size_t rgba_size) {
    if (!values || values->slot >= AOS_PAUSE_EQUIP_SLOTS ||
        values->item_count > AOS_PAUSE_EQUIP_VISIBLE ||
        !graphics || !graphics->text_ready ||
        !valid_output(graphics, rgba, rgba_size))
        return false;
    flourish_page(graphics, AOS_FX_EQUIP);
    reset_bg0(graphics, false);
    clear_bg0_tiles(graphics, 0x340, 0x80);

    put_number(graphics, 22, 2, values->current[0], 3, 0x3300, false);
    put_number(graphics, 22, 3, values->current[1], 3, 0x3300, false);
    for (int stat = 0; stat < 4; ++stat)
        put_number(graphics, 22, 5 + stat, values->current[2 + stat],
                   3, 0x3300, false);
    if (values->preview_active) {
        put_comparison(graphics, 2, values->current[0], values->preview[0]);
        put_comparison(graphics, 3, values->current[1], values->preview[1]);
        for (int stat = 0; stat < 4; ++stat)
            put_comparison(graphics, 5 + stat, values->current[2 + stat],
                           values->preview[2 + stat]);
    }

    put_tiles(graphics, 2, 3, (uint16_t)(0x33D0 + values->slot * 0x10), 11);
    put_tiles(graphics, 3, 5, 0x3310, 9);
    put_tiles(graphics, 3, 7, 0x3320, 9);
    put_tiles(graphics, 3, 9, 0x3330, 9);
    for (unsigned entry = 0; entry < AOS_PAUSE_EQUIP_VISIBLE; ++entry)
        put_tiles(graphics, entry & 1u ? 17 : 3,
                  11 + (int)(entry / 2),
                  (uint16_t)(0x3340 + entry * 0x10), 9);
    for (unsigned row = 0; row < 3; ++row)
        put_tiles(graphics, 5, 16 + (int)row,
                  (uint16_t)(0x3200 + row * 0x20), 24);

    unsigned next_tile = AOS_TEXT_TILE_FIRST;
    static const unsigned equipped_x[] = {126, 0, 126};
    static const unsigned equipped_y[] = {64, 72, 72};
    for (unsigned slot = 0; slot < AOS_PAUSE_EQUIP_SLOTS; ++slot) {
        uint16_t name, description;
        if (!equipment_text_ids(graphics, slot, values->equipped[slot],
                                &name, &description) ||
            !place_text_id(graphics, name, true, equipped_x[slot],
                           equipped_y[slot], 84,
                           slot == values->slot ? 9 : 3, &next_tile))
            return false;
    }

    for (unsigned entry = 0; entry < values->item_count; ++entry) {
        uint16_t name, description;
        unsigned item = values->items[entry];
        if (!equipment_text_ids(graphics, values->slot, item,
                                &name, &description))
            return false;
        unsigned x = entry & 1u ? 126 : 0;
        unsigned y = (10 + entry / 2) * 8;
        if (!place_text_id(graphics, name, true, x, y, 78,
                           item == values->selected_item ? 9 : 3,
                           &next_tile))
            return false;
        put_number(graphics, entry & 1u ? 27 : 13,
                   11 + (int)(entry / 2), values->quantities[entry],
                   1, 0x3300, false);
    }

    uint16_t name, description;
    if (!equipment_text_ids(graphics, values->slot, values->selected_item,
                            &name, &description))
        return false;
    if (values->selected_item == AOS_PAUSE_EQUIP_NONE && values->preview_active)
        description = 0x350;
    if (!place_text_cells(graphics, description, false, 5, 16,
                          0, 0, 0x20, 2, 3, &next_tile))
        return false;

    composite(graphics, AOS_PAUSE_BACKGROUND_EQUIP, rgba);
    /* sub_0804A00C draws the description popup's icon entity at the
     * same (0x1C,0x94) anchor: the selected list item while the list
     * is open, otherwise the slot's equipped record. */
    {
        unsigned icon_item = values->selecting ? values->selected_item
                                               : values->equipped[values->slot];
        unsigned icon;
        if (equipment_icon_id(graphics, values->slot, icon_item, &icon))
            draw_icon(graphics, rgba, icon, 20, 132);
    }
    /* sub_0804A0B4 spawns the three equipped icons through
     * sub_080488FC at anchor (0x70, 0x32+16*slot); the frame's
     * (-8,-16) offset puts each 16x16 icon's top-left at
     * (104, 34+16*slot). Slot 0 is the weapon (kind 3), slots 1-2
     * armor (kind 4); AOS_PAUSE_EQUIP_NONE hides the entity. */
    for (unsigned slot = 0; slot < AOS_PAUSE_EQUIP_SLOTS; ++slot) {
        unsigned icon;
        if (equipment_icon_id(graphics, slot, values->equipped[slot],
                              &icon))
            draw_icon(graphics, rgba, icon, 104, 34 + 16 * (int)slot);
    }
    /* sub_0804AAF0 holds the hand at cell (2, 5 + 2*slot) on the equipped
     * rows; sub_0804AFD4 moves it to the flat item slot
     * ((entry&1) ? 0x10 : 2, 0xB + entry/2) while the list is open. */
    if (values->selecting) {
        for (unsigned entry = 0; entry < values->item_count; ++entry)
            if (values->items[entry] == values->selected_item) {
                draw_cursor(graphics, rgba, entry & 1 ? 0x10 : 2,
                            0xB + (int)(entry / 2));
                break;
            }
    } else {
        draw_cursor(graphics, rgba, 2, 5 + 2 * (int)values->slot);
    }
    return true;
}

bool aos_pause_graphics_render_config(AosPauseGraphics *graphics,
                                      const uint8_t config[AOS_BUTTON_ACTIONS],
                                      uint8_t cursor,
                                      uint8_t *rgba, size_t rgba_size) {
    if (!config || cursor >= AOS_BUTTON_ACTIONS ||
        !valid_output(graphics, rgba, rgba_size)) return false;
    flourish_page(graphics, AOS_FX_CONFIG);
    reset_bg0(graphics, false);
    /* sub_0804BF8C init: three 24-tile description strips at column 5,
     * rows 15-17 (0x3200+i*0x20), the context-A window (0,0,0x20,2)
     * and the hint text 0x352 via sub_0804728C. */
    for (unsigned i = 0; i < 3; ++i)
        put_tiles(graphics, 5, (int)(15 + i),
                  (uint16_t)(0x3200 + i * 0x20), 24);
    /* sub_0804C264 head: context-B window (0,5,0x1E,1) cleared by
     * sub_08041204 then eight controller glyphs 0x80-0x87 printed with
     * manual +6..8 px cursor bumps (the native code adds to
     * +0x115B6/+0x115B8 before each sub_08040C60 call). */
    unsigned next_tile = AOS_TEXT_TILE_FIRST;
    if (!cells_begin_b(graphics, 0, 5, 0x1E, 1))
        return false;
    static const uint8_t pairs[4][2] = {
        {0x80, 0x81}, {0x82, 0x83}, {0x86, 0x87}, {0x84, 0x85},
    };
    static const uint8_t gap_before[4] = {7, 15, 14, 13};
    for (unsigned pair = 0; pair < 4; ++pair) {
        graphics->cells.b_sx =
            (uint16_t)(graphics->cells.b_sx + gap_before[pair]);
        graphics->cells.b_px =
            (uint16_t)(graphics->cells.b_px + gap_before[pair]);
        if (!cells_print_glyph_b(graphics, pairs[pair][0]) ||
            !cells_print_glyph_b(graphics, pairs[pair][1]))
            return false;
    }
    if (!cells_commit_b(graphics, 0, 0, 3, &next_tile))
        return false;
    /* sub_0804C264: four rows at tile rows 4,6,8,10. The action label is a
     * baked seven- or six-tile strip at entry 0x3359+row*0x10 (row 1 is six
     * tiles wide natively); the assigned button name is the three-tile
     * strip at 0x32A0+index*3 in column 0x12. Row order follows table
     * 0x08525510 {0,1,3,2} = attack, jump, guardian, ability. */
    static const uint8_t row_actions[AOS_BUTTON_ACTIONS] = {0, 1, 3, 2};
    static const unsigned label_tiles[AOS_BUTTON_ACTIONS] = {7, 6, 7, 7};
    for (unsigned row = 0; row < AOS_BUTTON_ACTIONS; ++row) {
        unsigned y = 4 + row * 2;
        put_tiles(graphics, 6, (int)y,
                  (uint16_t)(0x3359 + row * 0x10), label_tiles[row]);
        unsigned index = config[row_actions[row]];
        if (index > 3) index = 0;
        put_tiles(graphics, 0x12, (int)y,
                  (uint16_t)(0x32A0 + index * 3), 3);
    }
    /* sub_0804BF8C's sub_0804728C(0x352): "Press SELECT to restore /
     * the default settings." in the bottom window at (40,128). */
    if (!place_text_cells(graphics, 0x352, false, 5, 15,
                          0, 0, 0x20, 2, 3, &next_tile))
        return false;
    composite(graphics, AOS_PAUSE_BACKGROUND_CONFIG, rgba);
    /* sub_0804C3AC: sub_08048AD8(5, cursor*2 + 4) beside the rows. */
    draw_cursor(graphics, rgba, 5, 4 + 2 * (int)cursor);
    return true;
}

/* sub_080472B0: fills a tilemap rectangle with consecutive tile indices.
 * Native column/row counts derive from the last two arguments as
 * (arg3 * 6) >> 3 and (arg4 * 12) >> 3; tile (i, j) is arg2 + i * 32 + j. */
static void window_fill(AosPauseGraphics *graphics, unsigned column,
                        unsigned row, uint16_t tile_base, unsigned dim_a,
                        unsigned dim_b) {
    unsigned columns = (dim_a * 3 * 2) >> 3;
    unsigned rows = (dim_b * 3 * 4) >> 3;
    for (unsigned i = 0; i < rows; ++i)
        for (unsigned j = 0; j < columns; ++j) {
            size_t cell = (size_t)(row + i) * 32 + column + j;
            if (cell < 32 * 32)
                graphics->bg0_map[cell] =
                    (uint16_t)(tile_base + i * 32 + j);
        }
}

bool aos_pause_graphics_render_ability(AosPauseGraphics *graphics,
                                       const AosPauseAbilityValues *values,
                                       uint8_t *rgba, size_t rgba_size) {
    if (!values || values->cursor >= AOS_PAUSE_ABILITY_COUNT ||
        !graphics || !graphics->text_ready ||
        !valid_output(graphics, rgba, rgba_size))
        return false;
    flourish_page(graphics, AOS_FX_ABILITY);
    reset_bg0(graphics, false);
    /* sub_08041204-style placeholder clear shared with the soul pages. */
    clear_bg0_tiles(graphics, 0x380, 0x80);
    /* sub_0804BA18 init: sub_080472B0(6,0xD,0x3200,0x1F,4) draws the
     * bottom message window frame at tilemap (6,13), 23x6 tiles, then
     * sets the context-A window (0,0,0x1F,4). The strip loop runs
     * byte14>>1 = 3 iterations: per row a ten-tile strip at column 3
     * (0x3380+i*0x20) and one at column 0x11 (0x3390+i*0x20), rows
     * 4+2i — three rows for six abilities, two columns. */
    window_fill(graphics, 6, 0xD, 0x3200, 0x1F, 4);
    for (unsigned i = 0; i < AOS_PAUSE_ABILITY_COUNT / 2; ++i) {
        put_tiles(graphics, 3, (int)(4 + i * 2),
                  (uint16_t)(0x3380 + i * 0x20), 10);
        put_tiles(graphics, 0x11, (int)(4 + i * 2),
                  (uint16_t)(0x3390 + i * 0x20), 10);
    }
    /* sub_0804BB40's per-frame loop: each owned ability gets a two-tile
     * ON/OFF marker. Column is 0xD for even indices, 0x1B for odd; row is
     * 4 + 2*(i>>1). sub_08032AB8 returns the ability's mask bit, so
     * (state >> i) << 4 adds 0x10 to 0xB30E when enabled (0xB31E). */
    for (unsigned i = 0; i < AOS_PAUSE_ABILITY_COUNT; ++i) {
        if (!values->owned[i]) continue;
        put_tiles(graphics, (int)(i & 1 ? 0x1B : 0xD),
                  (int)(4 + (i >> 1) * 2),
                  (uint16_t)(values->enabled[i] ? 0xB31E : 0xB30E), 2);
    }
    /* Names print through the context-B cursor: window column 0 or 0x15
     * and row 0xC + i/2; odd slots take the sub_0804090C pixel cursor at
     * x=0x80 (128 px), even slots stay at the window origin x=0. Rows
     * are 8px cells so text lands at (0|128, (12+i/2)*8). */
    unsigned next_tile = AOS_TEXT_TILE_FIRST;
    for (unsigned i = 0; i < AOS_PAUSE_ABILITY_COUNT; ++i) {
        if (!place_text_id(graphics,
                           values->owned[i] ? (uint16_t)(0x156 + i) : 0x34F,
                           true, (i & 1u) ? 128u : 0u,
                           (12u + i / 2u) * 8u, 78, 3, &next_tile))
            return false;
    }
    /* sub_0804BCA0 on entry and sub_0804BB40 after each update print the
     * selected entry's description through the context-A cell engine:
     * text 0x257+cursor while SoulInventory_GetSoulTotal(3,cursor)
     * reports the ability owned, else 0x34F "---". The native window is
     * sub_08040748(0,0,0x1F,4) set up by sub_080472B0's tail; the cell
     * canvas is rooted at the strip's tilemap origin (6,13), so the
     * covered cells land on tiles 6..28 x 13..18 — exactly the frame
     * sub_080472B0(6,0xD,0x3200,0x1F,4) filled. */
    if (!place_text_cells(graphics,
                          values->owned[values->cursor]
                              ? (uint16_t)(0x257 + values->cursor) : 0x34F,
                          false, 6, 0xD, 0, 0, 0x1F, 4, 3, &next_tile))
        return false;
    composite(graphics, AOS_PAUSE_BACKGROUND_ABILITY, rgba);
    /* sub_0804BA18's placement: column 2 or 0x10 on cursor bit 0, row
     * 4 + 2*(cursor>>1) — the hand sits left of the selected strip. */
    draw_cursor(graphics, rgba,
                values->cursor & 1 ? 0x10 : 2,
                4 + 2 * (int)(values->cursor >> 1));
    return true;
}

bool aos_pause_graphics_render_item_use(
    AosPauseGraphics *graphics, const AosPauseItemUseValues *values,
    uint8_t *rgba, size_t rgba_size) {
    if (!values ||
        (values->selected != 0xFF &&
         values->selected >= AOS_PAUSE_ITEM_USE_RECORDS) ||
        !graphics || !graphics->text_ready ||
        !valid_output(graphics, rgba, rgba_size))
        return false;
    flourish_page(graphics, AOS_FX_ITEM_USE);
    reset_bg0(graphics, false);
    clear_bg0_tiles(graphics, 0x380, 0x80);
    /* sub_0804B6C4 calls sub_080463A8(10,4): current HP/MP end at
     * columns 10, max values at 15, and the seven-tile meters begin at
     * 17. sub_080471E4(20,7) writes the four-tile status word. */
    put_number(graphics, 10, 4, values->hp, 4, 0x3300, false);
    put_number(graphics, 10, 5, values->mp, 4, 0x3300, false);
    put_number(graphics, 15, 4, values->max_hp, 4, 0x3300, false);
    put_number(graphics, 15, 5, values->max_mp, 4, 0x3300, false);
    put_meter(graphics, 17, 4, values->hp, values->max_hp, 3);
    put_meter(graphics, 17, 5, values->mp, values->max_mp, 4);
    unsigned status_variant = values->status & 1 ? 2
                              : values->status & 2 ? 3
                              : values->status & 4 ? 1 : 0;
    put_tiles(graphics, 20, 7,
              (uint16_t)(0x330A + status_variant * 0x10), 4);
    /* sub_0804B6C4: four slot rows of strips at columns 2 and 0x11, tile
     * rows 11-14 (entries 0x3380/0x3390+i*0x20), then three 24-tile
     * description bars at column 5, rows 16-18 (0x3200+i*0x20). */
    for (unsigned i = 0; i < 4; ++i) {
        put_tiles(graphics, 2, (int)(11 + i),
                  (uint16_t)(0x3380 + i * 0x20), 10);
        put_tiles(graphics, 0x11, (int)(11 + i),
                  (uint16_t)(0x3390 + i * 0x20), 10);
    }
    for (unsigned i = 0; i < 3; ++i)
        put_tiles(graphics, 5, (int)(16 + i),
                  (uint16_t)(0x3200 + i * 0x20), 24);
    /* sub_0804B494: owned items in slot order fill the 4x2 grid; names
     * are text 0x5B+item at context-B cells column 0/0x15 row 0xC+j/2,
     * quantities are 0x3300 digits ending at column 0xD/0x1C row
     * 0xB+j/2, and unused slots get two 0x330D dashes (sub_08047098). */
    uint8_t owned[AOS_PAUSE_ITEM_USE_RECORDS];
    unsigned owned_count = 0;
    for (unsigned item = 0; item < AOS_PAUSE_ITEM_USE_RECORDS; ++item)
        if (values->counts[item]) owned[owned_count++] = (uint8_t)item;
    unsigned next_tile = AOS_TEXT_TILE_FIRST;
    for (unsigned slot = 0; slot < AOS_PAUSE_ITEM_USE_VISIBLE; ++slot) {
        unsigned ordinal = values->top + slot;
        bool filled = ordinal < owned_count;
        uint8_t item = filled ? owned[ordinal] : 0;
        unsigned x = slot & 1u ? 126u : 0u;
        unsigned y = (12u + slot / 2u) * 8u;
        if (filled &&
            !place_text_id(graphics, (uint16_t)(0x5B + item), true, x, y,
                           78, 3, &next_tile))
            return false;
        unsigned quantity_x = slot & 1u ? 0x1Cu : 0xDu;
        unsigned quantity_y = 0xBu + slot / 2u;
        if (filled)
            put_number(graphics, (int)quantity_x, (int)quantity_y,
                       values->counts[item], 1, 0x3300, false);
        else {
            /* sub_08047098 writes the same 0x330D tile to both cells,
             * walking left from the quantity column. */
            size_t dash = (size_t)quantity_y * 32 + quantity_x;
            if (dash < 32 * 32) graphics->bg0_map[dash] = 0x330D;
            if (dash >= 1 && dash - 1 < 32 * 32)
                graphics->bg0_map[dash - 1] = 0x330D;
        }
    }
    /* The selected item's description (text 0x15C+item via table
     * 0x08506936) prints through sub_0804728C into the context-A
     * window sub_08040748(0,0,0x20,2): 32 cols * 6px = 192 px wide,
     * home cell maps onto the description strips at tilemap (5,16) =
     * pixel (40,128) — same anchor as the Equip page. */
    if (values->selected < AOS_PAUSE_ITEM_USE_RECORDS &&
        values->counts[values->selected] &&
        !place_text_cells(graphics,
                          (uint16_t)(0x15C + values->selected), false,
                          5, 16, 0, 0, 0x20, 2, 3, &next_tile))
        return false;
    composite(graphics, AOS_PAUSE_BACKGROUND_ITEM_USE, rgba);
    /* sub_0804B6C4 (init) and sub_0804B808 (update) each spawn exactly
     * ONE sub_08048804 icon entity at pixel anchor (0x1C, 0x94), rebuilt
     * only when the selection changes. Its frame's (-8, -16) piece
     * offset puts the 16x16 icon's top-left at (20, 132). The native
     * renderer sub_0804B494 fills the 4x2 grid with names and quantity
     * digits only — no per-slot icon entities exist — so the port draws
     * just the selected item's icon, matching native rather than
     * inventing per-slot icons the game does not show. */
    if (values->selected < AOS_PAUSE_ITEM_USE_RECORDS &&
        values->counts[values->selected])
        draw_icon(graphics, rgba, values->icons[values->selected], 20, 132);
    /* sub_0804B808's placement: column 1 or 0x10 on the flat slot's bit 0,
     * row 0xB + slot/2 where slot is the selected owned ordinal minus the
     * scroll top. */
    if (values->selected != 0xFF)
        for (unsigned o = 0; o < owned_count; ++o)
            if (owned[o] == values->selected) {
                int slot = (int)o - (int)values->top;
                if (slot >= 0 && slot < (int)AOS_PAUSE_ITEM_USE_VISIBLE)
                    draw_cursor(graphics, rgba, slot & 1 ? 0x10 : 1,
                                0xB + slot / 2);
                break;
            }
    return true;
}

/* sub_08046FFC: right-aligned single-row digits ending at column x on
 * tile row y. Entries are (palette << 12) | 0x300 | digit; exhausted
 * high digits become base | 0xD unless show_zeros is set. Writes at
 * least `digits` tiles and keeps going while the value has more. */
static void put_bignum(AosPauseGraphics *graphics, int x, int y,
                       int32_t value, int digits, int palette,
                       int show_zeros) {
    if (!graphics || value < 0 || digits <= 0 || y < 0 || y >= 32)
        return;
    uint16_t base = (uint16_t)(((palette & 0xF) << 12) | 0x300);
    uint32_t v = (uint32_t)value;
    int col = x;
    for (int i = 0;; ++i) {
        if (col < 0 || y * 32 + col >= (int)(sizeof graphics->bg0_map / 2))
            return;
        unsigned d = v % 10;
        uint16_t entry = (i > 0 && v == 0 && !show_zeros)
                             ? (uint16_t)(base | 0xD)
                             : (uint16_t)(base | d);
        graphics->bg0_map[y * 32 + col--] = entry;
        v /= 10;
        if (i + 1 >= digits && v == 0) break;
    }
}

/* sub_08047128: fills n cells starting at (x, y) with tile 0x330D. */
static void put_dashes(AosPauseGraphics *graphics, int x, int y, int n) {
    for (int i = 0; i < n && x + i < 32; ++i)
        graphics->bg0_map[y * 32 + x + i] = 0x330D;
}

static bool enemy_defeated(const AosPauseEnemyValues *values, unsigned enemy) {
    return enemy < AOS_PAUSE_ENEMY_RECORDS &&
           (values->defeated[enemy >> 5] & (1u << (enemy & 31))) != 0;
}

bool aos_pause_graphics_render_enemy(AosPauseGraphics *graphics,
                                     const AosPauseEnemyValues *values,
                                     uint8_t *rgba, size_t rgba_size) {
    if (!values || values->top >= AOS_PAUSE_ENEMY_RECORDS ||
        values->cursor >= AOS_PAUSE_ENEMY_RECORDS ||
        !graphics || !graphics->text_ready ||
        !valid_output(graphics, rgba, rgba_size))
        return false;
    flourish_page(graphics, AOS_FX_ENEMY);
    reset_bg0(graphics, true);
    clear_bg0_tiles(graphics, 0x380, 0x80);
    unsigned next_tile = AOS_TEXT_TILE_FIRST;
    for (unsigned slot = 0; slot < AOS_PAUSE_ENEMY_PAGE; ++slot) {
        unsigned enemy = values->top + slot;
        if (enemy >= AOS_PAUSE_ENEMY_RECORDS) break;
        bool found = enemy_defeated(values, enemy);
        /* sub_0804D2C8 intro loop: ten-tile row strips at columns 6/0x14,
         * rows 5+2*(slot/2), entries 0xF320+slot*0x10 (palette 15). */
        put_tiles(graphics, (int)(slot & 1u ? 0x14 : 6),
                  (int)(5 + (slot / 2) * 2),
                  (uint16_t)(0xF320 + slot * 0x10), 10);
        /* +0xC phase 1: names/numbers/dashes appear one frame after
         * the strips (phase 0 draws the strips alone). */
        if (values->intro_phase < 1) continue;
        /* sub_0804D2C8: separator dash then the enemy number through
         * sub_08046FFC — 2 digits palette 1 with zeros shown up to 99,
         * 3 digits without leading-zero blanks above. */
        int row = (int)(5 + (slot / 2) * 2);
        int num_x = (int)(slot & 1u ? 0x12 : 4);
        put_dashes(graphics, slot & 1u ? 0x10 : 2, row, 1);
        unsigned number = enemy + 1;
        if (number > 99)
            put_bignum(graphics, num_x, row, (int32_t)number, 3, 1, 0);
        else
            put_bignum(graphics, num_x, row, (int32_t)number, 2, 1, 1);
        /* Names: palette 15 when defeated, 14 with 0x34E otherwise
         * (sub_080412F0 calls in the loop). */
        uint16_t id = found ? (uint16_t)(0x25D + enemy) : 0x34E;
        if (!place_text_id(graphics, id, true, slot & 1u ? 126u : 0u,
                           (8u + slot / 2u) * 8u, 110,
                           found ? 15 : 14, &next_tile))
            return false;
    }
    /* sub_0804C668 header/footer strips drawn at page enter:
     * 0xF3D0 x14 title strip at (0xB,3), 0xF3E0 x4 and the three
     * single tiles 0x13E4/0x13E8/0xF3EA along row 0xF. */
    put_tiles(graphics, 0xB, 3, 0xF3D0, 0xE);
    put_tiles(graphics, 0x11, 0xF, 0xF3E0, 4);
    put_tiles(graphics, 0x15, 0xF, 0x13E4, 1);
    put_tiles(graphics, 0x19, 0xF, 0x13E8, 1);
    put_tiles(graphics, 0x1B, 0xF, 0xF3EA, 1);
    /* sub_0804D2C8 footer, drawn once +0xC reaches phase 2 (steady
     * state): page numbers via sub_08046F74 at (0xD,0x11)/(0x10,0x11)
     * with the 0x139B separator, corner singles
     * 0x174C/0x134C/0xF3EC-0xF3EF, "ENEMY DATA" (0x3B1) and "SOUL"
     * (0x3B2) labels through context-B windows at pixel (126,112)
     * and (0,120), and the controller glyphs 0x84-0x87. */
    if (values->intro_phase >= 2) {
        unsigned total_pages = (AOS_PAUSE_ENEMY_RECORDS - 1) / 10 + 1;
        unsigned page = values->top / 10 + 1;
        put_bignum(graphics, 0xD, 0x11, (int32_t)page, 2, 1, 1);
        put_tiles(graphics, 0xE, 0x11, 0x139B, 1);
        put_bignum(graphics, 0x10, 0x11, (int32_t)total_pages, 2, 1, 1);
        put_tiles(graphics, 2, 0x11, 0x174C, 1);
        put_tiles(graphics, 0x1C, 0x11, 0x134C, 1);
        put_tiles(graphics, 3, 0x11, 0xF3EC, 1);
        put_tiles(graphics, 4, 0x11, 0xF3ED, 1);
        put_tiles(graphics, 0x1A, 0x11, 0xF3EE, 1);
        put_tiles(graphics, 0x1B, 0x11, 0xF3EF, 1);
        if (!place_text_id(graphics, 0x3B1, true, 126, 112, 108, 3,
                           &next_tile) ||
            !place_text_id(graphics, 0x3B2, true, 0, 120, 96, 3,
                           &next_tile))
            return false;
        /* sub_0804D2C8 button hints: context-B window (0,0xF,0x29,1),
         * pixel cursor (0x60,0xF) = cell (16,0xF), glyphs 0x84/0x85 then a
         * manual +9 px bump (native +0x115B6/+0x115B8) before 0x86/0x87. */
        if (!cells_begin_b(graphics, 0, 0xF, 0x29, 1))
            return false;
        aos_menu_cells_cursor_b(&graphics->cells, 16, 0xF);
        if (!cells_print_glyph_b(graphics, 0x84) ||
            !cells_print_glyph_b(graphics, 0x85))
            return false;
        graphics->cells.b_sx = (uint16_t)(graphics->cells.b_sx + 9);
        graphics->cells.b_px = (uint16_t)(graphics->cells.b_px + 9);
        if (!cells_print_glyph_b(graphics, 0x86) ||
            !cells_print_glyph_b(graphics, 0x87))
            return false;
        if (!cells_commit_b(graphics, 0, 0, 3, &next_tile))
            return false;
    }
    composite(graphics, AOS_PAUSE_BACKGROUND_ENEMY_LIST, rgba);
    /* sub_0804C668/sub_0804C770: column 2 or 0x10 on the slot's bit 0,
     * row 5 + slot/2 where slot = cursor - top on the ten-entry page. */
    if (!values->detail && values->cursor >= values->top &&
        values->cursor - values->top < AOS_PAUSE_ENEMY_PAGE)
        draw_cursor(graphics, rgba,
                    (values->cursor - values->top) & 1 ? 0x10 : 2,
                    5 + (int)((values->cursor - values->top) / 2));
    return true;
}

static bool drop_text_id(const AosPauseEnemyValues *values, unsigned enemy,
                         uint16_t item, int which, uint16_t *id) {
    if (item == 0) {
        *id = 0x34F;
        return true;
    }
    const uint32_t *bits = values->dropped[which];
    if ((bits[enemy >> 5] & (1u << (enemy & 31))) == 0) {
        *id = 0xA;
        return true;
    }
    *id = (uint16_t)(0x5A + item);
    return true;
}

/* sub_0804D768 + the CDF4 icon loops: element icons for the set bits
 * of a record mask, packed row-major from (x0, y0) five per row, then
 * 0x330D dashes fill the remaining cells. Entries come from the
 * 0x0852551C table (palette 13). */
static void element_grid(AosPauseGraphics *graphics, uint16_t mask,
                         int x0, int y0) {
    static const uint16_t icons[8] = {0xD36F, 0xD39F, 0xD3AF, 0xD3BF,
                                      0xD3CF, 0xD3DF, 0xD3FF, 0xD3FE};
    int x = x0, y = y0, clear = 0;
    for (unsigned bit = 0; bit < 8; ++bit) {
        if (mask & (1u << bit)) {
            graphics->bg0_map[y * 32 + x] = icons[bit];
            if (++x - x0 == 5) { x = x0; ++y; }
        } else {
            ++clear;
        }
    }
    for (int i = 0; i < clear; ++i) {
        graphics->bg0_map[y * 32 + x] = 0x330D;
        if (++x - x0 == 5) { x = x0; ++y; }
    }
}

bool aos_pause_graphics_render_enemy_detail(AosPauseGraphics *graphics,
                                            const AosPauseEnemyValues *values,
                                            uint8_t *rgba, size_t rgba_size) {
    if (!values || values->cursor >= AOS_PAUSE_ENEMY_RECORDS ||
        !graphics || !graphics->text_ready ||
        !valid_output(graphics, rgba, rgba_size))
        return false;
    unsigned enemy = values->cursor;
    flourish_page(graphics, AOS_FX_ENEMY);
    reset_bg0(graphics, true);
    clear_bg0_tiles(graphics, 0x380, 0x80);
    /* sub_0804C968 header strips: 0x1320 x10 at (0x12,8) and
     * 0x1330 x10 at (0x12,9). */
    put_tiles(graphics, 0x12, 8, 0x1320, 0xA);
    put_tiles(graphics, 0x12, 9, 0x1330, 0xA);
    unsigned next_tile = AOS_TEXT_TILE_FIRST;
    /* sub_0804CDF4 header numbers. */
    put_dashes(graphics, 6, 1, 1);
    unsigned number = enemy + 1;
    put_bignum(graphics, 8, 1, (int32_t)number,
               number > 99 ? 3 : 2, 1, 1);
    put_bignum(graphics, 0x14, 2, values->hp, 4, 1, 0);
    put_bignum(graphics, 0x1B, 0xB, (int32_t)values->experience, 8, 1, 0);
    /* Record +0x1C (resist) icons rows 4+, +0x1A (weak) rows 6+,
     * column 0x15, five per row. */
    element_grid(graphics, values->resist, 0x15, 4);
    element_grid(graphics, values->weak, 0x15, 6);
    /* sub_0804CFE8: common drop at window (0,9), rare at (0x15,9)
     * -> pixel rows 72; sub_0804090C(0x80,9) is the context-B pixel
     * cursor setter forcing the rare name to x=128, not an icon. */
    uint16_t drop_id = 0;
    if (!drop_text_id(values, enemy, values->common_item, 0, &drop_id) ||
        !place_text_id(graphics, drop_id, true, 0, 72, 110, 3, &next_tile))
        return false;
    if (!drop_text_id(values, enemy, values->rare_item, 1, &drop_id) ||
        !place_text_id(graphics, drop_id, true, 128, 72, 110, 3,
                       &next_tile))
        return false;
    /* sub_0804D0D0: dash fill then the 0x1310 underline and the enemy
     * name through the context-B window at cell (0x15,8). The native
     * x is 0xB0 - width/2 (func_0804CD80 measures the small-font
     * pixel width), centering the name at pixel 176 on row 64. */
    put_dashes(graphics, 2, 0xC, 0x14);
    put_tiles(graphics, 2, 0xC, 0x1310, 0xC);
    bool found = enemy_defeated(values, enemy);
    AosNativeText name_text;
    if (!read_text(graphics, found ? (uint16_t)(0x25D + enemy) : 0x34E,
                   &name_text) ||
        !name_text.line_count)
        return false;
    unsigned name_width =
        name_text.glyph_counts[0]
            ? aos_intro_font_measure_small(&graphics->font,
                                           name_text.glyphs[0],
                                           name_text.glyph_counts[0])
            : 0;
    unsigned name_x = name_width > 0xB0 ? 0 : 0xB0 - name_width / 2;
    if (!place_text_id(graphics,
                       found ? (uint16_t)(0x25D + enemy) : 0x34E,
                       true, name_x, 64, 110, 3, &next_tile))
        return false;
    /* Soul-type icon tile on the underline, only in soul view while
     * the soul exists and is owned (r6 gate in sub_0804D0D0). */
    if (values->soul_view && values->soul_owned && values->soul_index) {
        static const uint16_t soul_icon[4] = {0xD3FD, 0xD3FC, 0xD3FB,
                                              0xD3A8};
        graphics->bg0_map[0xC * 32 + 2] =
            soul_icon[values->soul_type & 3];
    }
    /* sub_0804D1DC: three soul-bar strips rows 14-16, cols 3-27. */
    put_tiles(graphics, 3, 0xE, 0x1200, 0x19);
    put_tiles(graphics, 3, 0xF, 0x1240, 0x19);
    put_tiles(graphics, 3, 0x10, 0x1280, 0x19);
    bool has_soul = values->soul_type || values->soul_index;
    bool owned = has_soul && values->soul_owned;
    /* sub_0804CDF4 tail: the soul icon at column 0x18 rows 1-2 —
     * 0x399/0x3A9 bars without a soul, dashes when unowned, and the
     * 0xCC0/0xCD0 strips when owned. */
    if (!has_soul) {
        put_dashes(graphics, 0x18, 2, 3);
        put_tiles(graphics, 0x19, 1, 0x399, 2);
        put_tiles(graphics, 0x19, 2, 0x3A9, 2);
    } else if (!owned) {
        put_dashes(graphics, 0x18, 1, 3);
        put_dashes(graphics, 0x18, 2, 3);
    } else {
        put_tiles(graphics, 0x18, 1, 0xCC0, 3);
        put_tiles(graphics, 0x18, 2, 0xCD0, 3);
    }
    /* sub_0804D1DC tail: the shared bottom context-A window alternates
     * content — enemy description (0x080EA628 entry + 0x71 = 0x2CE+enemy,
     * window 0,0,0x22,2) in normal view, soul description
     * (sub_08047330, window 0,0,0x1E,2, "---" 0x34F without a soul) in
     * soul view, but only while the soul is owned (r5 gate demotes to
     * the enemy text). Cell home pixel (56,120), palette 3, like the
     * soul-set description window. */
    if (values->soul_view && owned) {
        if (!place_text_cells(
                graphics,
                values->soul_index
                    ? soul_description_id(values->soul_type,
                                          values->soul_index)
                    : 0x34F,
                false, 3, 14, 0, 0, 0x1E, 2, 3, &next_tile))
            return false;
    } else if (!place_text_cells(
                   graphics, found ? (uint16_t)(0x2CE + enemy) : 0x34E,
                   false, 3, 14, 0, 0, 0x22, 2, 3, &next_tile)) {
        return false;
    }
    /* sub_0804D1DC tail: owned markers at column 0x1C rows 0xE/0x10 —
     * 0x134E bottom in soul view off, 0x134D top in soul view on,
     * dashes on the other row; unowned or absent souls dash both.
     * Native order draws them after the description window so they
     * stay visible over the text. */
    if (owned && !values->soul_view) {
        graphics->bg0_map[0x10 * 32 + 0x1C] = 0x134E;
        put_dashes(graphics, 0x1C, 0xE, 1);
    } else if (owned && values->soul_view) {
        graphics->bg0_map[0xE * 32 + 0x1C] = 0x134D;
        put_dashes(graphics, 0x1C, 0x10, 1);
    } else {
        put_dashes(graphics, 0x1C, 0xE, 1);
        put_dashes(graphics, 0x1C, 0x10, 1);
    }
    composite(graphics, AOS_PAUSE_BACKGROUND_ENEMY_DETAIL, rgba);
    return true;
}

bool aos_pause_graphics_render_sleep(AosPauseGraphics *graphics,
                                     bool can_sleep, uint8_t choice,
                                     unsigned sleep_result,
                                     uint8_t *rgba, size_t rgba_size) {
    if (choice > 1 || !valid_output(graphics, rgba, rgba_size)) return false;
    flourish_page(graphics, AOS_FX_SLEEP);
    reset_bg0(graphics, false);
    unsigned next_tile = AOS_TEXT_TILE_FIRST;
    /* sub_0804BD38/sub_0804BE0C draw three separate bands, not one shared
     * bottom window. Band 1 is the question/refusal: sub_0804728C ->
     * sub_08046BC8 prints 0x36A ("Pause the game?") or 0x361 ("Cannot
     * save until you have / saved once in a save room.") through the
     * small-font cell path into the narrow framed box at pixels
     * x64-131, y61-82 (native capture pgb_2/pgb_6). The ~11-column
     * window wraps each display line, so the message occupies two or
     * three rows; it is placed at pixel (68,64). The cell engine is
     * used (not place_text_id) because only it wraps the string at the
     * window edge the way sub_08046BC8 does natively. */
    if (!cells_begin(graphics, false, 0, 0, 0x1A, 3))
        return false;
    if (!cells_print_id(graphics, can_sleep ? 0x36A : 0x361,
                        false, 0, 0) ||
        !cells_commit(graphics, 10, 15, 0, 0, 0x1A, 3, 3, &next_tile))
        return false;
    if (can_sleep) {
        /* Band 2 is the framed Yes/No prompt: sub_0804066C(4,1) +
         * sub_08041434(0x34B) and sub_0804066C(0xC,1) +
         * sub_08041434(0x34D) print the two labels through the native
         * small-font printer. The native window wraps the second
         * cursor onto the next row, so the labels stack vertically
         * inside the parallelogram frame — native capture shows "Yes"
         * at y98-102 and "No" at y106-110, both left aligned near
         * x108. place_text_id uses the intro small-font span (not the
         * cells_char_b path the question band uses); the two native
         * fonts are close enough that the labels land within a few
         * pixels of the capture. */
        if (!place_text_id(graphics, 0x34B, true, 106, 96, 48, 3,
                           &next_tile) ||
            !place_text_id(graphics, 0x34D, true, 106, 104, 48, 3,
                           &next_tile))
            return false;
    }
    /* Band 3 is the shared bottom description window every pause page
     * uses: sub_08040748(0,0,0x1a,3) + sub_08040FE0 commits the cell
     * canvas at tile (0,15), so window pixels map to screen y120-156.
     * sub_0804BD38 prints nothing there on entry — the native captures
     * (pgb_2 available, pgb_6 unavailable) show the band empty — and
     * only sub_0804BE0C writes a result message after a save attempt:
     * 0x36B "Game was saved/successfully." on success, 0x386 "Save
     * failed." on failure, through sub_0804728C -> sub_08046BC8 into
     * the same window. The outcome arrives as sleep_result — the
     * AOS_PAUSE_SLEEP_RESULT_* value the caller fills from the
     * AOS_SLEEP_RESULT_* state owned by aos_soul_menu.h (identical
     * 0/1/2 codes) — and stays visible until the page is re-entered. */
    if (sleep_result == AOS_PAUSE_SLEEP_RESULT_SAVED ||
        sleep_result == AOS_PAUSE_SLEEP_RESULT_FAILED) {
        if (!place_text_cells(graphics,
                              sleep_result == AOS_PAUSE_SLEEP_RESULT_SAVED
                                  ? 0x36B : 0x386,
                              false, 0, 15, 0, 0, 0x1A, 3, 3, &next_tile))
            return false;
    }
    (void)next_tile;
    composite(graphics, AOS_PAUSE_BACKGROUND_SLEEP, rgba);
    /* sub_0804BD38 initializes the hand at (0x95,0x89), the native
     * default No choice. sub_0804BE0C then writes x = 0x65 for Yes or
     * 0x95 for No while y stays 0x89. draw_cursor adds its decoded bob
     * around the sub_08048AD8 anchor; cells (12,17)/(18,17) resolve to
     * x 102/150 and y 139, within that native wiggle range. */
    if (can_sleep) draw_cursor(graphics, rgba, choice ? 18 : 12, 17);
    return true;
}
