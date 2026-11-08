/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_pause_graphics.h"

#include "aos_obj_rom.h"
#include "aos_rom_graphics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    AOS_PAUSE_PALETTE = 0x080E5C20,
    AOS_PAUSE_GRAPHICS = 0x0815E02C,
    AOS_PAUSE_BG1_METADATA = 0x0815E940,
    AOS_PAUSE_BG1_CONTROL = 0x1D00,
    AOS_PAUSE_BANK_ENGLISH = 0x0827B200,
    AOS_PAUSE_BANK_FRENCH = 0x08277984,
    AOS_PAUSE_BANK_GERMAN = 0x0826C848,
    AOS_PAUSE_BG0_FILL = 0x330D,
    /* sub_08048C74: the hand's sheet per language (the *(gEwramData+0x4D0)
     * value selects 2 -> 0x0827798C, 3 -> 0x0826C840, else 0x082052D8)
     * and sub_08048B0C's frame descriptor (3 -> 0x0827B1F0). The hand's
     * OBJ palette row gets block 1's first eight colours of 0x0820C2C0
     * through sub_0803C7B4; indices 8-15 keep the row's upper half, taken
     * here from the following block. */
    AOS_CURSOR_SHEETS = 3,
    AOS_CURSOR_PALETTE = 0x0820C2C0 + 4 + 32,
    AOS_CURSOR_PALETTE_UPPER = 0x0820C2C0 + 4 + 64,
    AOS_PAUSE_WIDTH = 240,
    AOS_PAUSE_HEIGHT = 160,
    AOS_WEAPON_RECORDS = 0x08505D3C,
    AOS_ARMOR_RECORDS = 0x085063B0,
    AOS_EQUIPMENT_NAME_IDS = 0x08506734,
    AOS_EQUIPMENT_DESCRIPTION_IDS = 0x08506936,
    AOS_ROM_BASE = 0x08000000,
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
    fusion_aos_free_rendered_background(&graphics->background);
    *graphics = (AosPauseGraphics){0};
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
    FusionAosVideoMemory *memory = calloc(1, sizeof *memory);
    if (!memory) {
        set_error(error, error_size, "cannot allocate Aria pause video memory");
        return false;
    }
    static const uint32_t language_banks[] = {
        AOS_PAUSE_BANK_ENGLISH, AOS_PAUSE_BANK_FRENCH, AOS_PAUSE_BANK_GERMAN,
    };
    bool loaded = aos_rom_load_palette_resource(
                          rom, AOS_PAUSE_PALETTE, memory->palette,
                          error, error_size) &&
                      aos_rom_load_graphics_resource(
                          rom, AOS_PAUSE_GRAPHICS, memory->vram,
                          sizeof memory->vram, error, error_size) &&
                      aos_rom_load_graphics_resource(
                          rom, language_banks[language - 1],
                          memory->vram + 0xA000,
                          sizeof memory->vram - 0xA000, error, error_size);
    if (!loaded) {
        free(memory);
        return false;
    }
    FusionAosBackground background = {
        .layer = 1,
        .control = AOS_PAUSE_BG1_CONTROL,
        .metadata_pointer = AOS_PAUSE_BG1_METADATA,
    };
    FusionAosBackgroundTilemap tilemap = {0};
    bool ok = fusion_aos_read_background_tiles(rom, &background, &tilemap,
                                                error, error_size) &&
              tilemap.status == FUSION_AOS_TILEMAP_DECODED &&
              fusion_aos_render_background(&background, &tilemap, memory,
                                           &graphics->background,
                                           error, error_size) &&
              graphics->background.unresolved_count == 0;
    fusion_aos_free_background_tilemap(&tilemap);
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
        const uint8_t *palette_upper = rom->data +
            (size_t)(AOS_CURSOR_PALETTE_UPPER - AOS_ROM_BASE);
        if (palette + 16 <= rom->data + rom->size &&
            palette_upper + 16 <= rom->data + rom->size &&
            aos_obj_sheet_decode(&graphics->cursor_sheet, rom,
                                  cursor_graphics[language - 1],
                                  cursor_frames[language - 1],
                                  error, error_size)) {
            for (unsigned k = 0; k < 8; ++k) {
                graphics->cursor_palette[k] =
                    (uint16_t)(palette[k * 2] | palette[k * 2 + 1] << 8);
                graphics->cursor_palette[k + 8] =
                    (uint16_t)(palette_upper[k * 2] |
                               palette_upper[k * 2 + 1] << 8);
            }
            graphics->cursor_ready = true;
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

static bool bg0_pixel(const AosPauseGraphics *graphics, unsigned x, unsigned y,
                      uint8_t rgba[4]) {
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
    uint16_t bgr = (uint16_t)(graphics->palette[color] |
                              graphics->palette[color + 1] << 8);
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
           graphics->background.rgba &&
           graphics->background.width_pixels >= AOS_PAUSE_WIDTH &&
           graphics->background.height_pixels >= AOS_PAUSE_HEIGHT;
}

static void composite(const AosPauseGraphics *graphics, uint8_t *rgba) {
    for (unsigned y = 0; y < AOS_PAUSE_HEIGHT; ++y)
        for (unsigned x = 0; x < AOS_PAUSE_WIDTH; ++x) {
            size_t out = ((size_t)y * AOS_PAUSE_WIDTH + x) * 4;
            size_t background =
                ((size_t)y * graphics->background.width_pixels + x) * 4;
            memcpy(rgba + out, graphics->background.rgba + background, 4);
            bg0_pixel(graphics, x, y, rgba + out);
        }
}

static void composite_on_backdrop(const AosPauseGraphics *graphics,
                                  uint8_t *rgba) {
    for (unsigned y = 0; y < AOS_PAUSE_HEIGHT; ++y)
        for (unsigned x = 0; x < AOS_PAUSE_WIDTH; ++x) {
            size_t out = ((size_t)y * AOS_PAUSE_WIDTH + x) * 4;
            memcpy(rgba + out, graphics->backdrop_rgb, 3);
            rgba[out + 3] = 255;
            bg0_pixel(graphics, x, y, rgba + out);
        }
}

/* sub_08048AD8 positions the hand entity at cell_x*8+6, cell_y*8+3 and its
 * frame pieces hang off that anchor (the first record's -8,-8 offset makes
 * the 16 x 16 hand span anchor-8..anchor+8). `record` opens the pointing
 * strip: the idle cycle walks records 2-8 at four ticks a step and
 * sub_08048018 adds its +-3px horizontal wiggle to the anchor's X from the
 * shared frame counter. */
static void draw_cursor(const AosPauseGraphics *graphics, uint8_t *rgba,
                        int cell_x, int cell_y, unsigned record) {
    if (!graphics->cursor_ready || record + 6 >= graphics->cursor_sheet.frame_count)
        return;
    unsigned frame_index = record + (graphics->cursor_tick / 4) % 7;
    const AosObjFrame *frame = &graphics->cursor_sheet.frames[frame_index];
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

static void reset_bg0(AosPauseGraphics *graphics) {
    ++graphics->cursor_tick;
    memcpy(graphics->bg0_chars, graphics->bg0_base_chars,
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
    reset_bg0(graphics);

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

    /* sub_08047C4C's tail: sub_0804728C(0x362 + selected) prints the
     * selected entry's description into the bottom message window
     * through the native cell engine. The native caller runs
     * sub_08040748(0,0,0x1a,3) + sub_08040FE0 then sub_0804728C, so
     * the text window is cols 0-25 (6px units) x rows 0-2 (12px
     * units) and the cursor starts at the window home; sub_08046BC8
     * restores it on exit. The cell canvas origin is tile (0,15) =
     * pixel (0,120), so window pixels map to screen y 120-156. */
    unsigned next_tile = 0;
    if (!place_text_cells(graphics, (uint16_t)(0x362 + selected), false,
                          0, 15, 0, 0, 26, 3, 3, &next_tile))
        return false;

    composite(graphics, rgba);
    /* sub_08047818's hub placement: sub_08048AD8(1, selected + 0xC). */
    draw_cursor(graphics, rgba, 1, 12 + (int)selected, 2);
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
    if (*next_tile + tile_width * tile_height > 0x200u) return false;

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
            unsigned tile = (*next_tile)++;
            size_t source = ((size_t)row * 32 + column) * 32;
            unsigned map_x = destination_x + column;
            unsigned map_y = destination_y + row;
            uint8_t *destination =
                graphics->bg0_chars + (size_t)tile * 32;
            if (map_x < 32 && map_y < 32) {
                uint16_t previous = graphics->bg0_map[map_y * 32 + map_x];
                memcpy(destination,
                       graphics->bg0_chars + (size_t)(previous & 0x3FF) * 32,
                       32);
                const uint8_t *glyph = graphics->text_tiles.data + source;
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
    static const uint16_t bases[] = {0x1E3, 0x21B, 0x233};
    static const uint16_t none[] = {0x37D, 0x37E, 0x37F};
    return soul ? (uint16_t)(bases[category] + soul) : none[category];
}

/* Cell-engine text path: run a localized entry through the native
 * software cell renderer, then commit each non-empty framebuffer cell
 * as a BG0 tile. `tile_x`/`tile_y` are the BG0 map origin of the cell
 * canvas; `col`/`row`/`w`/`h` are the context-A window arguments fed
 * to sub_08040748 (6px columns, 12px rows). Only cells containing
 * glyph pixels consume tiles from `next_tile` (cap 0x200 like
 * place_text); empty cells keep the existing map entry. */
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

/* Commits framebuffer cells [cx0,cx1] x [cy0,cy1] as BG0 tiles whose
 * map position is (tile_x + cx, tile_y + cy). Shared by the context-A
 * and context-B commit paths. */
static bool cells_commit_range(AosPauseGraphics *graphics,
                               unsigned tile_x, unsigned tile_y,
                               unsigned cx0, unsigned cy0, unsigned cx1,
                               unsigned cy1, unsigned palette,
                               unsigned *next_tile) {
    if (!graphics || !next_tile) return false;
    for (unsigned cy = cy0; cy <= cy1 && cy < 32; ++cy)
        for (unsigned cx = cx0; cx <= cx1 && cx < 32; ++cx) {
            const uint32_t *cell =
                graphics->cells_fb + (size_t)cy * AOS_MENU_CELLS_ROW_WORDS +
                cx * 8;
            bool empty = true;
            for (unsigned word = 0; word < 8; ++word)
                if (cell[word]) { empty = false; break; }
            if (empty) continue;
            unsigned map_x = tile_x + cx, map_y = tile_y + cy;
            if (map_x >= 32 || map_y >= 32 || *next_tile >= 0x200)
                return false;
            unsigned tile = (*next_tile)++;
            uint8_t tile_data[32];
            if (!aos_menu_cells_cell_tile(&graphics->cells,
                                          (uint16_t)cx, (uint16_t)cy,
                                          tile_data))
                return false;
            uint16_t previous = graphics->bg0_map[map_y * 32 + map_x];
            uint8_t *destination = graphics->bg0_chars + (size_t)tile * 32;
            memcpy(destination,
                   graphics->bg0_chars + (size_t)(previous & 0x3FF) * 32,
                   32);
            for (unsigned byte = 0; byte < 32; ++byte) {
                uint8_t low = tile_data[byte] & 0x0F;
                uint8_t high = tile_data[byte] & 0xF0;
                if (low) destination[byte] =
                    (uint8_t)((destination[byte] & 0xF0) | low);
                if (high) destination[byte] =
                    (uint8_t)((destination[byte] & 0x0F) | high);
            }
            graphics->bg0_map[map_y * 32 + map_x] =
                (uint16_t)(tile | palette << 12);
        }
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
    /* The window covers canvas pixels [col*6, (col+w-1)*6) x
     * [row*12, (row+h-1)*12); cells are the 8px framebuffer grid. */
    unsigned px_x0 = col * 6, px_x1 = (unsigned)(col + w - 1) * 6 + 6;
    unsigned px_y0 = row * 12, px_y1 = (unsigned)(row + h - 1) * 12 + 12;
    unsigned cx0 = px_x0 / 8, cx1 = (px_x1 - 1) / 8;
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
    return cells_begin(graphics, small, col, row, w, h) &&
           cells_print_id(graphics, id, small, col, row) &&
           cells_commit(graphics, tile_x, tile_y, col, row, w, h,
                        palette, next_tile);
}

static bool place_text_id(AosPauseGraphics *graphics, uint16_t id, bool small,
                          unsigned x, unsigned y, unsigned width,
                          unsigned palette, unsigned *next_tile) {
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
    reset_bg0(graphics);
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

    unsigned next_tile = 0;
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

    unsigned available[AOS_PAUSE_SOUL_RECORDS];
    unsigned available_count = 1;
    available[0] = 0;
    for (unsigned soul = 1; soul < soul_counts[values->category]; ++soul)
        if (values->counts[values->category][soul - 1])
            available[available_count++] = soul;
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
    if (!place_text_id(graphics,
                       soul_description_id(values->category, selected_soul),
                       false, 56, 120, 174, 3, &next_tile))
        return false;

    composite_on_backdrop(graphics, rgba);
    /* sub_08048EA4: column 2 or 0x10 on the flat slot's bit 0, row
     * 0xA + slot/2 where slot = selected - top in the eight-entry list. */
    draw_cursor(graphics, rgba, (selected - top) & 1 ? 0x10 : 2,
                0xA + (int)(selected - top) / 2, 2);
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
    uint32_t records = slot == 0 ? AOS_WEAPON_RECORDS : AOS_ARMOR_RECORDS;
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
    uint32_t records = slot == 0 ? AOS_WEAPON_RECORDS : AOS_ARMOR_RECORDS;
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
    reset_bg0(graphics);
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

    unsigned next_tile = 0;
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
    if (!place_text_id(graphics, description, false, 40, 128, 192,
                       3, &next_tile))
        return false;

    composite_on_backdrop(graphics, rgba);
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
                            0xB + (int)(entry / 2), 2);
                break;
            }
    } else {
        draw_cursor(graphics, rgba, 2, 5 + 2 * (int)values->slot, 2);
    }
    return true;
}

bool aos_pause_graphics_render_config(AosPauseGraphics *graphics,
                                      const uint8_t config[AOS_BUTTON_ACTIONS],
                                      uint8_t cursor,
                                      uint8_t *rgba, size_t rgba_size) {
    if (!config || cursor >= AOS_BUTTON_ACTIONS ||
        !valid_output(graphics, rgba, rgba_size)) return false;
    reset_bg0(graphics);
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
    static const struct {
        uint8_t ch;
        int8_t bump_sub, bump_px;
    } buttons[] = {
        {0x80, 7, 7}, {0x81, 8, 8}, {0x82, 8, 8}, {0x83, 8, 8},
        {0x86, 7, 7}, {0x87, 7, 6}, {0x84, 7, 7}, {0x85, 7, 7},
    };
    unsigned next_tile = 0;
    if (!cells_begin_b(graphics, 0, 5, 0x1E, 1))
        return false;
    for (size_t i = 0; i < sizeof buttons / sizeof buttons[0]; ++i) {
        graphics->cells.b_sx =
            (uint16_t)(graphics->cells.b_sx + buttons[i].bump_sub);
        graphics->cells.b_px =
            (uint16_t)(graphics->cells.b_px + buttons[i].bump_px);
        if (!cells_print_glyph_b(graphics, buttons[i].ch))
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
    if (!place_text_id(graphics, 0x352, false, 40, 128, 192, 3,
                       &next_tile))
        return false;
    composite(graphics, rgba);
    /* sub_0804C3AC: sub_08048AD8(5, cursor*2 + 4) beside the rows. */
    draw_cursor(graphics, rgba, 5, 4 + 2 * (int)cursor, 2);
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
    reset_bg0(graphics);
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
    unsigned next_tile = 0;
    for (unsigned i = 0; i < AOS_PAUSE_ABILITY_COUNT; ++i) {
        if (!place_text_id(graphics,
                           values->owned[i] ? (uint16_t)(0x156 + i) : 0x34F,
                           true, (i & 1u) ? 128u : 0u,
                           (12u + i / 2u) * 8u, 78, 3, &next_tile))
            return false;
    }
    composite(graphics, rgba);
    /* sub_0804BA18's placement: column 2 or 0x10 on cursor bit 0, row
     * 4 + 2*(cursor>>1) — the hand sits left of the selected strip. */
    draw_cursor(graphics, rgba,
                values->cursor & 1 ? 0x10 : 2,
                4 + 2 * (int)(values->cursor >> 1), 2);
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
    reset_bg0(graphics);
    clear_bg0_tiles(graphics, 0x380, 0x80);
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
    unsigned next_tile = 0;
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
        !place_text_id(graphics, (uint16_t)(0x15C + values->selected),
                       false, 40, 128, 192, 3, &next_tile))
        return false;
    composite(graphics, rgba);
    /* sub_08048804 anchors the icon entity at pixel (0x1C, 0x94); its
     * frame's (-8, -16) piece offset puts the 16x16 icon's top-left at
     * (20, 132). */
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
                                0xB + slot / 2, 2);
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
    reset_bg0(graphics);
    clear_bg0_tiles(graphics, 0x380, 0x80);
    unsigned next_tile = 0;
    for (unsigned slot = 0; slot < AOS_PAUSE_ENEMY_PAGE; ++slot) {
        unsigned enemy = values->top + slot;
        if (enemy >= AOS_PAUSE_ENEMY_RECORDS) break;
        /* sub_0804D2C8 intro loop: ten-tile row strips at columns 6/0x14,
         * rows 5+2*(slot/2), entries 0xF320+slot*0x10 (palette 15). */
        put_tiles(graphics, (int)(slot & 1u ? 0x14 : 6),
                  (int)(5 + (slot / 2) * 2),
                  (uint16_t)(0xF320 + slot * 0x10), 10);
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
        bool found = enemy_defeated(values, enemy);
        uint16_t id = found ? (uint16_t)(0x25D + enemy) : 0x34E;
        if (!place_text_id(graphics, id, true, slot & 1u ? 126u : 0u,
                           (8u + slot / 2u) * 8u, 110,
                           found ? 15 : 14, &next_tile))
            return false;
    }
    composite(graphics, rgba);
    /* sub_0804C668/sub_0804C770: column 2 or 0x10 on the slot's bit 0,
     * row 5 + slot/2 where slot = cursor - top on the ten-entry page. */
    if (!values->detail && values->cursor >= values->top &&
        values->cursor - values->top < AOS_PAUSE_ENEMY_PAGE)
        draw_cursor(graphics, rgba,
                    (values->cursor - values->top) & 1 ? 0x10 : 2,
                    5 + (int)((values->cursor - values->top) / 2), 2);
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
    reset_bg0(graphics);
    clear_bg0_tiles(graphics, 0x380, 0x80);
    unsigned next_tile = 0;
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
     * name through the context-B window at cell (0x15,8). */
    put_dashes(graphics, 2, 0xC, 0x14);
    put_tiles(graphics, 2, 0xC, 0x1310, 0xC);
    bool found = enemy_defeated(values, enemy);
    if (!place_text_id(graphics,
                       found ? (uint16_t)(0x25D + enemy) : 0x34E,
                       true, 126, 64, 110, 3, &next_tile))
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
    /* sub_0804D1DC tail: owned markers at column 0x1C rows 0xE/0x10 —
     * 0x134E bottom in soul view off, 0x134D top in soul view on,
     * dashes on the other row; unowned or absent souls dash both. */
    bool has_soul = values->soul_type || values->soul_index;
    bool owned = has_soul && values->soul_owned;
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
    composite(graphics, rgba);
    return true;
}

bool aos_pause_graphics_render_sleep(AosPauseGraphics *graphics,
                                     bool can_sleep, uint8_t *rgba,
                                     size_t rgba_size) {
    if (!valid_output(graphics, rgba, rgba_size)) return false;
    reset_bg0(graphics);
    unsigned next_tile = 0;
    /* sub_0804BD38/sub_0804BE0C run on the same pause text layer as the
     * hub: sub_0804728C prints the question/refusal through
     * sub_08046BC8, then sub_0804066C(4,1) + sub_08041434(0x34B) and
     * sub_0804066C(0xC,1) + sub_08041434(0x34D) print Yes/No through
     * sub_08041318 — all context A inside the shared
     * sub_08040748(0,0,0x1a,3) window on the cell canvas at tile
     * (0,15). The selection arrow stays OBJ and unrendered. */
    if (!cells_begin(graphics, false, 0, 0, 26, 3))
        return false;
    if (!can_sleep) {
        if (!cells_print_id(graphics, 0x361, false, 0, 0) ||
            !cells_commit(graphics, 0, 15, 0, 0, 26, 3, 3, &next_tile))
            return false;
    } else {
        if (!cells_print_id(graphics, 0x36A, false, 0, 0) ||
            !cells_print_id(graphics, 0x34B, false, 4, 1) ||
            !cells_print_id(graphics, 0x34D, false, 0xC, 1) ||
            !cells_commit(graphics, 0, 15, 0, 0, 26, 3, 3, &next_tile))
            return false;
    }
    composite(graphics, rgba);
    if (can_sleep) draw_cursor(graphics, rgba, 1, 16, 2);
    return true;
}
