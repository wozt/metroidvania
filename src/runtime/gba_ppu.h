/* SPDX-License-Identifier: GPL-3.0-only */
/* A GBA picture processing unit: the display memories (VRAM, palette RAM,
 * OAM) and registers that the native screens (HUD, menus, cutscenes) write,
 * and a scanline renderer for the tile modes 0 to 2 (text and affine
 * backgrounds, regular and affine objects, the two windows and the object
 * window, alpha blending, brightness and mosaic). The pinned MZM program
 * never selects the unimplemented bitmap modes 3 to 5; a source audit keeps
 * that boundary checked. */
#ifndef GBA_PPU_H
#define GBA_PPU_H

#include <stdbool.h>
#include <stdint.h>

enum {
    GBA_SCREEN_W = 240, GBA_SCREEN_H = 160,
    GBA_PPU_MAX_W = 512, GBA_PPU_MAX_H = 256,
    GBA_VRAM_SIZE = 0x18000, GBA_OBJ_VRAM = 0x10000
};

/* Display register offsets from 0x04000000 (halfword aligned). */
enum {
    GBA_REG_DISPCNT = 0x00, GBA_REG_BG0CNT = 0x08, GBA_REG_BG0HOFS = 0x10,
    GBA_REG_BG0VOFS = 0x12, GBA_REG_BG2PA = 0x20, GBA_REG_BG2PB = 0x22,
    GBA_REG_BG2PC = 0x24, GBA_REG_BG2PD = 0x26, GBA_REG_BG2X = 0x28,
    GBA_REG_BG2Y = 0x2C, GBA_REG_BG3PA = 0x30, GBA_REG_BG3X = 0x38,
    GBA_REG_BG3Y = 0x3C, GBA_REG_WIN0H = 0x40, GBA_REG_WIN1H = 0x42,
    GBA_REG_WIN0V = 0x44, GBA_REG_WIN1V = 0x46, GBA_REG_WININ = 0x48,
    GBA_REG_WINOUT = 0x4A, GBA_REG_MOSAIC = 0x4C, GBA_REG_BLDCNT = 0x50,
    GBA_REG_BLDALPHA = 0x52, GBA_REG_BLDY = 0x54
};

/* DISPCNT bits */
enum {
    GBA_DCNT_OBJ_1D = 1 << 6, GBA_DCNT_BLANK = 1 << 7, GBA_DCNT_BG0 = 1 << 8,
    GBA_DCNT_BG1 = 1 << 9, GBA_DCNT_BG2 = 1 << 10, GBA_DCNT_BG3 = 1 << 11,
    GBA_DCNT_OBJ = 1 << 12, GBA_DCNT_WIN0 = 1 << 13, GBA_DCNT_WIN1 = 1 << 14,
    GBA_DCNT_OBJWIN = 1 << 15
};

typedef struct {
    uint8_t vram[GBA_VRAM_SIZE];
    uint16_t palette[512];          /* BG colors 0-255, OBJ colors 256-511 */
    uint16_t oam[512];              /* 128 entries of four halfwords */
    uint16_t io[0x30];              /* display registers 0x00-0x5F */
    /* WIN0H rewritten by an H-blank handler: with `win0h_per_line`, line y
     * uses win0h_lines[y] instead of the register. */
    bool win0h_per_line;
    uint16_t win0h_lines[GBA_SCREEN_H];
} GbaPpu;

void gba_ppu_reset(GbaPpu *ppu);
void gba_ppu_write16(GbaPpu *ppu, uint32_t reg, uint16_t value);
void gba_ppu_write32(GbaPpu *ppu, uint32_t reg, uint32_t value);
uint16_t gba_ppu_read16(const GbaPpu *ppu, uint32_t reg);

/* The 15-bit color of each pixel (bit 15: no layer drew it, the backdrop
 * shows), for a screen of up to GBA_PPU_MAX_W x GBA_PPU_MAX_H pixels (the
 * native screen is GBA_SCREEN_W x GBA_SCREEN_H; a wider one shows more of
 * the same coordinate space, the windows keeping their 8-bit bounds). */
void gba_ppu_render_line(const GbaPpu *ppu, int y, int width, uint16_t *line);

/* The whole screen as 0xAARRGGBB; with `transparent_backdrop`, the backdrop
 * pixels have alpha 0 (an overlay drawn over another picture). */
void gba_ppu_render(const GbaPpu *ppu, int width, int height, bool transparent_backdrop,
                    uint32_t *pixels, int pitch);

/* 15-bit BGR to 0xFFRRGGBB */
uint32_t gba_color_argb(uint16_t color);

#endif
