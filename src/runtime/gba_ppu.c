/* SPDX-License-Identifier: GPL-3.0-only */
/* GBA PPU scanline renderer (see gba_ppu.h). The layer composition follows
 * GBATEK: each pixel takes the two front-most visible layers (objects in
 * front of backgrounds of the same priority, lower backgrounds in front of
 * higher ones), the window of the pixel selects the visible layers and the
 * color effect, and the effect blends the first target with the second. */
#include "gba_ppu.h"

#include <string.h>

enum { LAYER_OBJ = 4, LAYER_BD = 5, NO_PIXEL = 0xFFFF };

typedef struct {
    uint16_t color[GBA_PPU_MAX_W];  /* NO_PIXEL: transparent */
    uint8_t priority[GBA_PPU_MAX_W];
    uint8_t semi[GBA_PPU_MAX_W];    /* objects: semi-transparent */
} Line;

static uint16_t reg(const GbaPpu *p, uint32_t r) { return p->io[(r & 0x5F) >> 1]; }
static int32_t reg32(const GbaPpu *p, uint32_t r) {
    return (int32_t)((uint32_t)reg(p, r) | (uint32_t)reg(p, r + 2) << 16);
}

void gba_ppu_reset(GbaPpu *ppu) { memset(ppu, 0, sizeof *ppu); }

void gba_ppu_write16(GbaPpu *ppu, uint32_t r, uint16_t value) {
    if ((r & 0xFFFF) < 0x60) ppu->io[(r & 0x5E) >> 1] = value;
}

void gba_ppu_write32(GbaPpu *ppu, uint32_t r, uint32_t value) {
    gba_ppu_write16(ppu, r, (uint16_t)value);
    gba_ppu_write16(ppu, r + 2, (uint16_t)(value >> 16));
}

uint16_t gba_ppu_read16(const GbaPpu *ppu, uint32_t r) {
    return (r & 0xFFFF) < 0x60 ? ppu->io[(r & 0x5E) >> 1] : 0;
}

uint32_t gba_color_argb(uint16_t c) {
    uint32_t r = c & 31, g = c >> 5 & 31, b = c >> 10 & 31;
    r = r << 3 | r >> 2;
    g = g << 3 | g >> 2;
    b = b << 3 | b >> 2;
    return 0xFF000000u | r << 16 | g << 8 | b;
}

static void clear_line(Line *l, int width) {
    for (int x = 0; x < width; ++x) l->color[x] = NO_PIXEL;
    memset(l->semi, 0, (size_t)width);
}

/* A text background (modes 0 and 1). */
static void text_bg(const GbaPpu *p, int bg, int y, int width, Line *l) {
    uint16_t cnt = reg(p, GBA_REG_BG0CNT + 2u * (unsigned)bg);
    uint16_t mosaic = reg(p, GBA_REG_MOSAIC);
    bool mos = cnt & 0x40;
    int mw = mos ? (mosaic & 15) + 1 : 1, mh = mos ? (mosaic >> 4 & 15) + 1 : 1;
    int size = cnt >> 14;
    int map_w = size & 1 ? 512 : 256, map_h = size & 2 ? 512 : 256;
    uint32_t chars = (uint32_t)(cnt >> 2 & 3) * 0x4000, screen = (uint32_t)(cnt >> 8 & 31) * 0x800;
    bool color256 = cnt & 0x80;
    int sy = ((y - y % mh) + (reg(p, GBA_REG_BG0VOFS + 4u * (unsigned)bg) & 0x1FF)) & (map_h - 1);
    int hofs = reg(p, GBA_REG_BG0HOFS + 4u * (unsigned)bg) & 0x1FF;
    for (int x = 0; x < width; ++x) {
        int sx = ((x - x % mw) + hofs) & (map_w - 1);
        uint32_t block = screen;
        if (sx >= 256) block += 0x800;
        if (sy >= 256) block += map_w == 512 ? 0x1000 : 0x800;
        uint32_t entry_at = block + (uint32_t)(((sy & 255) >> 3) * 32 + ((sx & 255) >> 3)) * 2;
        if (entry_at + 1 >= GBA_OBJ_VRAM) continue;
        uint16_t entry = (uint16_t)(p->vram[entry_at] | p->vram[entry_at + 1] << 8);
        int tx = sx & 7, ty = sy & 7;
        if (entry & 0x400) tx = 7 - tx;
        if (entry & 0x800) ty = 7 - ty;
        uint32_t tile = entry & 0x3FF;
        uint16_t color;
        if (color256) {
            uint32_t at = chars + tile * 64 + (uint32_t)(ty * 8 + tx);
            if (at >= GBA_OBJ_VRAM) continue;
            uint8_t index = p->vram[at];
            if (!index) continue;
            color = p->palette[index];
        } else {
            uint32_t at = chars + tile * 32 + (uint32_t)(ty * 4 + tx / 2);
            if (at >= GBA_OBJ_VRAM) continue;
            uint8_t index = (uint8_t)(tx & 1 ? p->vram[at] >> 4 : p->vram[at] & 15);
            if (!index) continue;
            color = p->palette[(entry >> 12) * 16 + index];
        }
        l->color[x] = color & 0x7FFF;
        l->priority[x] = (uint8_t)(cnt & 3);
    }
}

/* An affine background (BG2 in modes 1 and 2, BG3 in mode 2). */
static void affine_bg(const GbaPpu *p, int bg, int y, int width, Line *l) {
    uint16_t cnt = reg(p, GBA_REG_BG0CNT + 2u * (unsigned)bg);
    uint32_t base = bg == 2 ? GBA_REG_BG2PA : GBA_REG_BG3PA;
    int16_t pa = (int16_t)reg(p, base), pb = (int16_t)reg(p, base + 2);
    int16_t pc = (int16_t)reg(p, base + 4), pd = (int16_t)reg(p, base + 6);
    /* The 28-bit reference points, sign extended. */
    int32_t rx = reg32(p, base + 8) << 4 >> 4, ry = reg32(p, base + 12) << 4 >> 4;
    int size = 128 << (cnt >> 14);
    uint32_t chars = (uint32_t)(cnt >> 2 & 3) * 0x4000, screen = (uint32_t)(cnt >> 8 & 31) * 0x800;
    bool wrap = cnt & 0x2000;
    int32_t px = rx + pb * y, py = ry + pd * y;
    for (int x = 0; x < width; ++x, px += pa, py += pc) {
        int sx = px >> 8, sy = py >> 8;
        if (wrap) {
            sx &= size - 1;
            sy &= size - 1;
        } else if (sx < 0 || sy < 0 || sx >= size || sy >= size) {
            continue;
        }
        uint32_t entry_at = screen + (uint32_t)((sy >> 3) * (size >> 3) + (sx >> 3));
        if (entry_at >= GBA_OBJ_VRAM) continue;
        uint32_t at = chars + (uint32_t)p->vram[entry_at] * 64 + (uint32_t)((sy & 7) * 8 + (sx & 7));
        if (at >= GBA_OBJ_VRAM) continue;
        uint8_t index = p->vram[at];
        if (!index) continue;
        l->color[x] = p->palette[index] & 0x7FFF;
        l->priority[x] = (uint8_t)(cnt & 3);
    }
}

static const uint8_t obj_sizes[3][4][2] = {
    {{8, 8}, {16, 16}, {32, 32}, {64, 64}},
    {{16, 8}, {32, 8}, {32, 16}, {64, 32}},
    {{8, 16}, {8, 32}, {16, 32}, {32, 64}},
};

/* The objects of a line, and the object window mask. */
static void objects(const GbaPpu *p, int y, int width, Line *l, uint8_t *objwin) {
    uint16_t dispcnt = reg(p, GBA_REG_DISPCNT);
    uint16_t mosaic = reg(p, GBA_REG_MOSAIC);
    int mode = dispcnt & 7;
    for (int i = 0; i < 128; ++i) {
        const uint16_t *o = &p->oam[i * 4];
        uint16_t a0 = o[0], a1 = o[1], a2 = o[2];
        bool affine = a0 & 0x100;
        if (!affine && a0 & 0x200) continue;           /* disabled */
        int shape = a0 >> 14;
        if (shape == 3) continue;
        int kind = a0 >> 10 & 3;
        if (kind == 3) continue;
        int w = obj_sizes[shape][a1 >> 14][0], h = obj_sizes[shape][a1 >> 14][1];
        int bw = w, bh = h;                             /* bounding box */
        if (affine && a0 & 0x200) {
            bw *= 2;
            bh *= 2;
        }
        int oy = a0 & 0xFF, ox = a1 & 0x1FF;
        if (oy + bh > 256) oy -= 256;
        if (ox + bw > GBA_PPU_MAX_W) ox -= GBA_PPU_MAX_W;
        int line_y = y;
        if (a0 & 0x1000) line_y -= line_y % ((mosaic >> 12 & 15) + 1);
        int ry = line_y - oy;
        if (ry < 0 || ry >= bh || y - oy < 0 || y - oy >= bh) continue;
        bool color256 = a0 & 0x2000;
        uint32_t tile = a2 & 0x3FF;
        if (mode >= 3 && tile < 512) continue;
        int priority = a2 >> 10 & 3;
        int pal = a2 >> 12;
        int row_units = dispcnt & GBA_DCNT_OBJ_1D ? (w / 8) * (color256 ? 2 : 1) : 32;
        int16_t pa = 256, pb = 0, pc = 0, pd = 256;
        if (affine) {
            int g = (a1 >> 9 & 31) * 16;
            pa = (int16_t)p->oam[g + 3];
            pb = (int16_t)p->oam[g + 7];
            pc = (int16_t)p->oam[g + 11];
            pd = (int16_t)p->oam[g + 15];
        }
        int mw = a0 & 0x1000 ? (mosaic >> 8 & 15) + 1 : 1;
        for (int bx = 0; bx < bw; ++bx) {
            int x = ox + bx;
            if (x < 0 || x >= width) continue;
            int mx = bx - (x % mw);
            if (mx < 0) mx = bx;
            int tx, ty;
            if (affine) {
                int cx = mx - bw / 2, cy = ry - bh / 2;
                tx = ((pa * cx + pb * cy) >> 8) + w / 2;
                ty = ((pc * cx + pd * cy) >> 8) + h / 2;
                if (tx < 0 || ty < 0 || tx >= w || ty >= h) continue;
            } else {
                tx = a1 & 0x1000 ? w - 1 - mx : mx;
                ty = a1 & 0x2000 ? h - 1 - ry : ry;
            }
            uint32_t unit = tile + (uint32_t)((ty >> 3) * row_units) +
                            (uint32_t)((tx >> 3) * (color256 ? 2 : 1));
            uint32_t at = GBA_OBJ_VRAM + (unit & 0x3FF) * 32;
            uint8_t index;
            if (color256) {
                index = p->vram[at + (uint32_t)((ty & 7) * 8 + (tx & 7))];
            } else {
                uint8_t b = p->vram[at + (uint32_t)((ty & 7) * 4 + (tx & 7) / 2)];
                index = (uint8_t)(tx & 1 ? b >> 4 : b & 15);
            }
            if (!index) continue;
            if (kind == 2) {
                objwin[x] = 1;
                continue;
            }
            if (l->color[x] != NO_PIXEL && l->priority[x] <= priority) continue;
            l->color[x] = p->palette[256 + (color256 ? index : pal * 16 + index)] & 0x7FFF;
            l->priority[x] = (uint8_t)priority;
            l->semi[x] = kind == 1;
        }
    }
}

/* A window span: [lo, hi), wrapping around when lo > hi. */
static bool in_range(int v, int lo, int hi) {
    return lo <= hi ? v >= lo && v < hi : v >= lo || v < hi;
}

static uint16_t blend(uint16_t a, uint16_t b, int eva, int evb) {
    int r = ((a & 31) * eva + (b & 31) * evb) >> 4;
    int g = ((a >> 5 & 31) * eva + (b >> 5 & 31) * evb) >> 4;
    int bl = ((a >> 10 & 31) * eva + (b >> 10 & 31) * evb) >> 4;
    if (r > 31) r = 31;
    if (g > 31) g = 31;
    if (bl > 31) bl = 31;
    return (uint16_t)(r | g << 5 | bl << 10);
}

static uint16_t brightness(uint16_t c, int evy, bool up) {
    int ch[3] = {c & 31, c >> 5 & 31, c >> 10 & 31};
    for (int i = 0; i < 3; ++i) ch[i] = up ? ch[i] + (((31 - ch[i]) * evy) >> 4) : ch[i] - ((ch[i] * evy) >> 4);
    return (uint16_t)(ch[0] | ch[1] << 5 | ch[2] << 10);
}

void gba_ppu_render_line(const GbaPpu *p, int y, int width, uint16_t *out) {
    if (width > GBA_PPU_MAX_W) width = GBA_PPU_MAX_W;
    uint16_t dispcnt = reg(p, GBA_REG_DISPCNT);
    if (dispcnt & GBA_DCNT_BLANK) {
        for (int x = 0; x < width; ++x) out[x] = 0x7FFF;
        return;
    }
    static Line lines[5];
    static uint8_t objwin[GBA_PPU_MAX_W];
    int mode = dispcnt & 7;
    bool present[5] = {false};
    for (int bg = 0; bg < 4; ++bg) {
        if (!(dispcnt & (GBA_DCNT_BG0 << bg))) continue;
        bool text = mode == 0 || (mode == 1 && bg < 2);
        bool aff = (mode == 1 && bg == 2) || (mode == 2 && bg >= 2);
        if (!text && !aff) continue;
        clear_line(&lines[bg], width);
        if (text) text_bg(p, bg, y, width, &lines[bg]);
        else affine_bg(p, bg, y, width, &lines[bg]);
        present[bg] = true;
    }
    memset(objwin, 0, (size_t)width);
    if (dispcnt & (GBA_DCNT_OBJ | GBA_DCNT_OBJWIN)) {
        clear_line(&lines[LAYER_OBJ], width);
        objects(p, y, width, &lines[LAYER_OBJ], objwin);
        present[LAYER_OBJ] = (dispcnt & GBA_DCNT_OBJ) != 0;
    }
    /* The backgrounds from front to back. */
    int order[4], count = 0;
    for (int pr = 0; pr < 4; ++pr)
        for (int bg = 0; bg < 4; ++bg)
            if (present[bg] && (reg(p, GBA_REG_BG0CNT + 2u * (unsigned)bg) & 3) == pr) order[count++] = bg;
    bool windows = dispcnt & (GBA_DCNT_WIN0 | GBA_DCNT_WIN1 | GBA_DCNT_OBJWIN);
    uint16_t winin = reg(p, GBA_REG_WININ), winout = reg(p, GBA_REG_WINOUT);
    bool in_win[2] = {false, false};
    for (int w = 0; w < 2; ++w) {
        if (!(dispcnt & (GBA_DCNT_WIN0 << w))) continue;
        uint16_t v = reg(p, GBA_REG_WIN0V + 2u * (unsigned)w);
        in_win[w] = in_range(y, v >> 8, v & 0xFF);
    }
    uint16_t bldcnt = reg(p, GBA_REG_BLDCNT), bldalpha = reg(p, GBA_REG_BLDALPHA);
    int eva = bldalpha & 31, evb = bldalpha >> 8 & 31, evy = reg(p, GBA_REG_BLDY) & 31;
    if (eva > 16) eva = 16;
    if (evb > 16) evb = 16;
    if (evy > 16) evy = 16;
    int effect = bldcnt >> 6 & 3;
    uint16_t backdrop = p->palette[0] & 0x7FFF;
    for (int x = 0; x < width; ++x) {
        uint8_t enabled = 0x3F;
        if (windows) {
            enabled = (uint8_t)(winout & 0x3F);
            if (dispcnt & GBA_DCNT_OBJWIN && objwin[x]) enabled = (uint8_t)(winout >> 8 & 0x3F);
            if (in_win[1]) {
                uint16_t h = reg(p, GBA_REG_WIN1H);
                if (in_range(x, h >> 8, h & 0xFF)) enabled = (uint8_t)(winin >> 8 & 0x3F);
            }
            if (in_win[0]) {
                uint16_t h = reg(p, GBA_REG_WIN0H);
                if (in_range(x, h >> 8, h & 0xFF)) enabled = (uint8_t)(winin & 0x3F);
            }
        }
        /* The two front-most layers. */
        int top[2] = {LAYER_BD, LAYER_BD};
        uint16_t color[2] = {backdrop, backdrop};
        int found = 0;
        bool obj = present[LAYER_OBJ] && enabled & 0x10 && lines[LAYER_OBJ].color[x] != NO_PIXEL;
        int obj_pr = obj ? lines[LAYER_OBJ].priority[x] : 4;
        bool obj_used = false;
        for (int i = 0; i <= count && found < 2; ++i) {
            int bg = i < count ? order[i] : -1;
            int bg_pr = bg >= 0 ? reg(p, GBA_REG_BG0CNT + 2u * (unsigned)bg) & 3 : 4;
            if (obj && !obj_used && obj_pr <= bg_pr) {
                obj_used = true;
                top[found] = LAYER_OBJ;
                color[found++] = lines[LAYER_OBJ].color[x];
                if (found == 2) break;
            }
            if (bg < 0) break;
            if (!(enabled & (1 << bg)) || lines[bg].color[x] == NO_PIXEL) continue;
            top[found] = bg;
            color[found++] = lines[bg].color[x];
        }
        uint16_t c = color[0];
        bool first = bldcnt & (1 << top[0]);
        bool second = top[1] != top[0] && bldcnt & (1 << (8 + top[1]));
        if (top[0] == LAYER_OBJ && lines[LAYER_OBJ].semi[x] && second) {
            c = blend(color[0], color[1], eva, evb);
        } else if (enabled & 0x20 && first) {
            if (effect == 1 && second) c = blend(color[0], color[1], eva, evb);
            else if (effect == 2) c = brightness(c, evy, true);
            else if (effect == 3) c = brightness(c, evy, false);
        }
        out[x] = (uint16_t)(c | (top[0] == LAYER_BD ? 0x8000 : 0));
    }
}

void gba_ppu_render(const GbaPpu *ppu, int width, int height, bool transparent_backdrop,
                    uint32_t *pixels, int pitch) {
    uint16_t line[GBA_PPU_MAX_W];
    if (width > GBA_PPU_MAX_W) width = GBA_PPU_MAX_W;
    if (height > GBA_PPU_MAX_H) height = GBA_PPU_MAX_H;
    for (int y = 0; y < height; ++y) {
        gba_ppu_render_line(ppu, y, width, line);
        uint32_t *row = (uint32_t *)((uint8_t *)pixels + (size_t)y * (size_t)pitch);
        for (int x = 0; x < width; ++x)
            row[x] = transparent_backdrop && line[x] & 0x8000 ? 0 : gba_color_argb(line[x] & 0x7FFF);
    }
}
