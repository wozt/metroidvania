/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria's title screen: see aos_title_screen.h. */
#include "aos_title_screen.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "aos_game_mode.h"
#include "aos_input.h"
#include "aos/math/aos_math.h"

enum {
    DCNT_MODE_1 = 1, DCNT_BG0 = 1 << 8, DCNT_BG1 = 1 << 9, DCNT_BG2 = 1 << 10,
    DCNT_OBJ = 1 << 12, DCNT_WINOBJ = 1 << 15,
    BLD_FIRST_ALL = 0x3F, BLD_ALPHA = 0x40, BLD_DECREASE = 0xC0, BLD_SECOND_ALL = 0x3F00,
    BLD_BG1_FIRST = 0x02, BLD_BG2_SECOND = 0x400, BLD_OBJ_SECOND = 0x1000,
    MUS_27 = 27, SONG_MUTE = 0x1010,
    GLOBAL = 16,
};

/* BgAffineSet (aos_title_graphics.h). */
/* The BIOS sine table: sin(i * 2 pi / 256) * 0x4000, rounded. */
static int32_t bios_sin(uint8_t index) {
    return (int32_t)lround(sin((double)index * 6.283185307179586 / 256.0) * 16384.0);
}

AosTitleAffine aos_title_affine(int16_t scale, uint16_t angle) {
    uint8_t theta = (uint8_t)(angle >> 8);
    int32_t s = bios_sin(theta), c = bios_sin((uint8_t)(theta + 64));
    AosTitleAffine a;
    a.pa = (int16_t)((scale * c) >> 14);
    a.pb = (int16_t)(-(scale * s) >> 14);
    a.pc = (int16_t)((scale * s) >> 14);
    a.pd = (int16_t)((scale * c) >> 14);
    a.x = 0x8000 - (a.pa * 120 + a.pb * 80);
    a.y = 0x8000 - (a.pc * 120 + a.pd * 80);
    return a;
}

uint8_t aos_title_affine_index(const AosTitleGraphics *graphics, const AosTitleAffine *affine,
                               int sx, int sy) {
    int32_t tx = (affine->x + affine->pa * sx + affine->pb * sy) >> 8;
    int32_t ty = (affine->y + affine->pc * sx + affine->pd * sy) >> 8;
    if (tx < 0 || ty < 0 || tx >= AOS_TITLE_LAYER_SIZE || ty >= AOS_TITLE_LAYER_SIZE) return 0;
    return graphics->affine[(size_t)ty * AOS_TITLE_LAYER_SIZE + (size_t)tx];
}

static uint16_t row_color(const uint8_t *resource, unsigned row, unsigned index) {
    const uint8_t *p = resource + 4 + row * 0x20 + index * 2;
    return (uint16_t)(p[0] | p[1] << 8);
}

/* sub_0803C918 (BG) / sub_0803C7B4 (OBJ): `count` rows of a palette
 * resource (+ 2: its row count) from `from` to `to`, clamped to 16 rows. */
static void copy_rows(AosTitlePalette *palette, const uint8_t *resource, unsigned from,
                      unsigned count, unsigned to) {
    if (!resource) return;
    unsigned n = count > 16 ? 16 : count;
    if (to + n > 16) n = 16 - to;
    if (from + n > resource[2]) n = resource[2] > from ? resource[2] - from : 0;
    for (unsigned r = 0; r < n; ++r)
        for (unsigned i = 0; i < 16; ++i)
            palette->source[(to + r) * 16 + i] = row_color(resource, from + r, i);
}

/* sub_0803E8BC(a, b << 16): the 16.16 quotient. */
static int32_t fx_step(int32_t from, int32_t to, int32_t frames) {
    return frames ? (int32_t)(((int64_t)(to - from) << 16) / ((int64_t)frames << 16)) : 0;
}

/* sub_0803D2D0(row, resource, resource_row, from, to, frames). */
static void fade_row(AosTitlePalette *palette, unsigned row, const uint8_t *resource,
                     unsigned resource_row, int32_t from, int32_t to, int32_t frames) {
    AosTitlePaletteFx *fx = &palette->fx;
    uint32_t bit = 1u << row;
    if (!resource) return;
    fx->enabled |= bit;
    fx->on |= bit;
    fx->running |= bit;
    for (unsigned i = 0; i < 16; ++i) fx->target[row][i] = row_color(resource, resource_row, i);
    fx->weight[row] = from;
    fx->end[row] = to;
    fx->step[row] = fx_step(from, to, frames);
}

/* sub_0803D18C (BG) / sub_0803CDF0 (OBJ): the whole palette toward a
 * colour, its own weight going from `from` to `to`. */
static void fade_color(AosTitlePalette *palette, uint8_t r, uint8_t g, uint8_t b, int32_t from,
                       int32_t to, int32_t frames) {
    AosTitlePaletteFx *fx = &palette->fx;
    fx->enabled |= 1u << GLOBAL;
    fx->on |= 1u << GLOBAL;
    fx->running |= 1u << GLOBAL;
    fx->color[0] = r;
    fx->color[1] = g;
    fx->color[2] = b;
    fx->weight[GLOBAL] = from;
    fx->end[GLOBAL] = to;
    fx->step[GLOBAL] = fx_step(from, to, frames);
}

/* sub_0803D270 / sub_0803D3A0: one effect off. */
static void fx_off(AosTitlePalette *palette, unsigned index) {
    AosTitlePaletteFx *fx = &palette->fx;
    uint32_t bit = 1u << index;
    fx->enabled &= ~bit;
    fx->on &= ~bit;
    fx->running &= ~bit;
    fx->weight[index] = fx->end[index] = 0x10000;
    fx->step[index] = 0;
    if (index == GLOBAL) memset(fx->color, 0, sizeof fx->color);
}

/* sub_0803BD5C then sub_0803BBA4 / sub_0803BA88 (in sub_0803BE04). */
static void apply_palette(AosTitlePalette *palette) {
    AosTitlePaletteFx *fx = &palette->fx;
    if (!fx->enabled) {
        memcpy(palette->shown, palette->source, sizeof palette->shown);
        return;
    }
    uint32_t live = fx->enabled & fx->on & fx->running;
    for (unsigned k = 0; k <= GLOBAL; ++k) {
        uint32_t bit = 1u << k;
        if (!(live & bit)) continue;
        fx->weight[k] += fx->step[k];
        if (fx->step[k] > 0 ? fx->weight[k] > fx->end[k] : fx->weight[k] < fx->end[k]) {
            fx->weight[k] = fx->end[k];
            fx->running &= ~bit;
        }
    }
    bool global = fx->enabled & (1u << GLOBAL);
    uint32_t gw = (uint32_t)fx->weight[GLOBAL], gi = 0x10000 - gw;
    uint32_t base[3] = {fx->color[0] * gi, (fx->color[1] * gi) << 5, (fx->color[2] * gi) << 10};
    for (unsigned row = 0; row < 16; ++row)
        for (unsigned i = 0; i < 16; ++i) {
            uint16_t c = palette->source[row * 16 + i];
            uint32_t ch[3] = {c & 0x1Fu, c & (0x1Fu << 5), c & (0x1Fu << 10)};
            if (fx->enabled & (1u << row)) {
                uint16_t t = fx->target[row][i];
                uint32_t tc[3] = {t & 0x1Fu, t & (0x1Fu << 5), t & (0x1Fu << 10)};
                uint32_t w = (uint32_t)fx->weight[row], wi = 0x10000 - w;
                for (int k = 0; k < 3; ++k) ch[k] = (uint32_t)(((uint64_t)ch[k] * w + (uint64_t)tc[k] * wi) >> 16);
            }
            if (global)
                for (int k = 0; k < 3; ++k) ch[k] = (uint32_t)(((uint64_t)ch[k] * gw + base[k]) >> 16);
            palette->shown[row * 16 + i] =
                (uint16_t)((ch[0] & 0x1F) | (ch[1] & (0x1Fu << 5)) | (ch[2] & (0x1Fu << 10)));
        }
}

static int32_t round_shift(int32_t v, int shift) {
    if (v < 0) v += (1 << shift) - 1;
    return v >> shift;
}

static void set_affine(AosTitleScreen *t, int32_t scale) {
    t->affine = aos_title_affine((int16_t)scale, (uint16_t)(t->frame * 0x10));
}

/* The wobble of stages 6-8: sine(phase) / 4096 - (progress - 0x118). */
static void wobble(AosTitleScreen *t) {
    t->phase = (uint16_t)(t->phase + 0x40);
    set_affine(t, round_shift(aos_sine(t->phase & 0x7FFF), 12) - (t->progress - 0x118));
}

/* TitleScreen_sub_08002F44: BG1HOFS of the 120 first lines. */
static void set_wave(AosTitleScreen *t, int32_t amplitude) {
    int32_t angle = (int32_t)(t->frame << 10);
    for (int line = 0; line < AOS_TITLE_WAVE_LINES; ++line) {
        int32_t v = amplitude * (aos_sine((uint32_t)angle) >> 8);
        int16_t off = (int16_t)(v < 0 ? -(-v >> 16) : v >> 16);
        t->wave[line] = (line & 1) ? (int16_t)-off : off;
        angle += 0x1000;
    }
}

/* TitleScreen_sub_08002FF8: every 49th frame BG row 0 becomes the next row
 * of 0x080E622C (rows 0, 5-15), then fades toward the following one over
 * 48 frames. */
static const uint8_t cycle_rows[12] = {0, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

static void palette_cycle(AosTitleScreen *t) {
    if (t->cycle_timer++ <= 0x30) return;
    t->cycle_timer = 0;
    copy_rows(&t->bg, t->cycle_rows, cycle_rows[t->cycle_index], 1, 0);
    if (++t->cycle_index >= 12) t->cycle_index = 0;
    fade_row(&t->bg, 0, t->cycle_rows, cycle_rows[t->cycle_index], 0x10000, 0, 0x30);
}

/* sub_08004004: entity 0x12's OBJ rows 4-7 sweep through 0x08209308, row 4
 * from its row 3, then rows 5 and 6, then row 7; its end (+ 0x4FC bit 1)
 * lets stage 4 go on. */
static void e12_update(AosTitleScreen *t) {
    if (t->e12_hidden) t->e12_state = 0x63;
    const uint8_t *src = t->obj_palette_rows;
    switch (t->e12_state) {
    case 0:
        if (!(t->e12_flags & 1)) break;
        t->e12_state = 1;
        /* fall through */
    case 1: case 2: case 3:
        if (t->e12_wait == 0) {
            if (t->e12_state == 1) {
                copy_rows(&t->obj, src, t->e12_row + 3, 1, 4);
            } else if (t->e12_state == 2) {
                copy_rows(&t->obj, src, t->e12_row + 0x12, 1, 5);
                copy_rows(&t->obj, src, t->e12_row + 3, 1, 6);
            } else {
                copy_rows(&t->obj, src, t->e12_row + 0x12, 1, 7);
            }
            t->e12_row++;
            t->e12_wait = 1;
            if (t->e12_row > 0xE) {
                t->e12_row = 0;
                t->e12_wait = 0;
                if (t->e12_state == 3) t->e12_flags |= 2;
                t->e12_state++;
                break;
            }
        }
        t->e12_wait--;
        break;
    case 4:
        if (t->e12_wait) {
            t->e12_wait--;
            break;
        }
        for (unsigned row = 4; row <= 7; ++row) copy_rows(&t->obj, src, 3, 1, row);
        t->e12_state = 5;
        break;
    default:
        break;
    }
}

/* RandomNumberGenerator. */
static uint32_t title_random(AosTitleScreen *t) {
    return t->random = (t->random >> 8) * 0x3243F6ADu + 0x1B0CB175u;
}

/* sub_0802D5EC: (random & (n - 1)) - n / 2. */
static int32_t random_centered(AosTitleScreen *t, int32_t n) {
    return (int32_t)(title_random(t) & (uint32_t)(n - 1)) - n / 2;
}

/* The animations of a sheet as aos_anim sees them. */
static const AosObjSheet *sheet_of(const AosTitleScreen *t, unsigned sheet) {
    return t->graphics ? &t->graphics->sheets[sheet] : NULL;
}

static AosAnimSet anim_set(const AosObjSheet *sheet, AosAnimDef defs[AOS_OBJ_MAX_ANIMS]) {
    for (unsigned k = 0; k < sheet->anim_count && k < AOS_OBJ_MAX_ANIMS; ++k)
        defs[k] = (AosAnimDef){sheet->anim_length[k], sheet->anim_duration[k]};
    return (AosAnimSet){defs, sheet->anim_count};
}

/* sub_0803F2C8. */
static void anim_start(const AosTitleScreen *t, unsigned sheet, AosAnimState *state, unsigned id,
                       bool loop) {
    const AosObjSheet *s = sheet_of(t, sheet);
    memset(state, 0, sizeof *state);
    if (!s) return;
    AosAnimDef defs[AOS_OBJ_MAX_ANIMS];
    AosAnimSet set = anim_set(s, defs);
    aos_anim_start(state, &set, id, loop);
}

/* sub_0803F17C / sub_0803EC34: + 0x549 takes the current step's frame,
 * then the step's timer advances. */
static void anim_step(const AosTitleScreen *t, unsigned sheet, AosAnimState *state,
                      uint8_t *record) {
    const AosObjSheet *s = sheet_of(t, sheet);
    if (!s || state->id >= s->anim_count || !s->anim_length[state->id]) return;
    unsigned length = s->anim_length[state->id];
    *record = s->anim_frame[state->id][state->frame < length ? state->frame : length - 1];
    AosAnimDef defs[AOS_OBJ_MAX_ANIMS];
    AosAnimSet set = anim_set(s, defs);
    aos_anim_step(state, &set);
}

/* sub_08003CF8, then sub_08003DBC's 34 bats (sub_08003A9C), from the seed
 * 0x1967 of sub_08004160. */
static void create_objects(AosTitleScreen *t) {
    t->random = 0x1967;
    AosTitleSwoop *w = &t->swoop;
    memset(w, 0, sizeof *w);
    w->active = true;
    anim_start(t, AOS_TITLE_SHEET_BATS, &w->anim, 4, true);
    anim_step(t, AOS_TITLE_SHEET_BATS, &w->anim, &w->record);
    w->hidden = true;
    w->angle = 0x9800;
    w->radius = 0xC00000;
    for (int i = 0; i < AOS_TITLE_BATS; ++i) {
        AosTitleBat *b = &t->bats[i];
        memset(b, 0, sizeof *b);
        b->active = true;
        b->x = i * 0x3C3C3 + 0x380000 + (int32_t)((uint32_t)random_centered(t, 0x10) << 16);
        b->y = (int32_t)((uint32_t)(uint16_t)(random_centered(t, 0x20) + 0x60) << 16);
        anim_start(t, AOS_TITLE_SHEET_BATS, &b->anim, 0, true);
        anim_step(t, AOS_TITLE_SHEET_BATS, &b->anim, &b->record);
        b->anim.frame = (uint8_t)(title_random(t) & 3);
        b->anim.tick = (uint8_t)(title_random(t) & 3);
        b->semi = true;
        b->hidden = true;
        b->ax = (int32_t)((title_random(t) & 0xFF) * 0x10) + 0x400;
        b->ay = (int32_t)((title_random(t) & 0xFF) * 0x10) + 0x400;
        uint16_t a = aos_arctan2((int16_t)((int16_t)(b->x >> 16) - 0x78),
                                   (int16_t)((int16_t)(b->y >> 16) - 0x60));
        b->vx = aos_sine(a + 0x4000u);
        b->vy = aos_sine(a);
        b->x += b->vx * 32;
        b->y += b->vy * 32;
        b->vx = round_shift(-b->vx, 4);
        b->vy = round_shift(-b->vy, 4);
        b->phase_x = (uint16_t)((title_random(t) & 0xFF) << 8);
        b->phase_y = (uint16_t)(b->phase_x + 0x8000 + ((title_random(t) & 0x1F) << 4));
        b->speed_x = (uint16_t)((title_random(t) & 0xFF) + 0x800);
        b->speed_y = (uint16_t)((title_random(t) & 0xFF) + 0xA00);
    }
}

/* sub_08003F08(kind, size, spin). */
static void create_sparkle(AosTitleScreen *t, int index, uint8_t kind, int32_t size,
                           uint16_t spin) {
    AosTitleSparkle *k = &t->sparkles[index];
    memset(k, 0, sizeof *k);
    k->active = true;
    anim_start(t, AOS_TITLE_SHEET_DISC, &k->anim, 4, false);
    anim_step(t, AOS_TITLE_SHEET_DISC, &k->anim, &k->record);
    k->kind = kind;
    k->size = size;
    k->spin = spin;
    k->x = 0x4E << 16;
    k->y = 0x26 << 16;
}

/* sub_08000EA8(1): the objects of + 0x547 == 1 are deleted. */
static void delete_objects(AosTitleScreen *t) {
    for (int i = 0; i < AOS_TITLE_BATS; ++i) t->bats[i].active = false;
    t->swoop.active = false;
    for (int i = 0; i < AOS_TITLE_SPARKLES; ++i) t->sparkles[i].active = false;
}

/* sub_08003C18, sub_080039D4 and sub_08003E64, in slot order. */
static void objects_update(AosTitleScreen *t) {
    AosTitleSwoop *w = &t->swoop;
    if (w->active && (w->state != 0 || (t->e13_flags & 2))) {
        uint16_t angle = w->angle;
        if (w->state == 0) {
            w->hidden = false;
            w->state = 1;
        }
        if (w->radius != 0) {
            w->angle = (uint16_t)(w->angle - 0xA0);
            int32_t r = w->radius;
            w->radius = (r - 0x8000) - round_shift(r, 6);
            if (w->radius <= 0xFFF) w->radius = 0;
        }
        w->scale = w->radius / 160 + 0x10000;
        w->x = (aos_sine(angle + 0x4000u) >> 8) * (w->radius >> 8) + 0x7A0000;
        w->y = (aos_sine(angle) >> 8) * (w->radius >> 8) + 0x1A0000;
        anim_step(t, AOS_TITLE_SHEET_BATS, &w->anim, &w->record);
    }
    for (int i = 0; i < AOS_TITLE_BATS; ++i) {
        AosTitleBat *b = &t->bats[i];
        if (!b->active) continue;
        if (b->state == 0) {
            if (!(t->e13_flags & 1)) continue;
            b->state = 1;
            b->hidden = false;
        }
        if (b->semi && (t->e13_flags & 4)) b->semi = false;
        b->phase_x = (uint16_t)(b->phase_x + b->speed_x);
        b->phase_y = (uint16_t)(b->phase_y + b->speed_y);
        b->x += round_shift(b->ax * aos_sine(b->phase_x + 0x4000u), 12);
        int32_t y = b->y + round_shift(b->ay * aos_sine(b->phase_y), 12);
        b->x += b->vx;
        b->y = y + b->vy;
        anim_step(t, AOS_TITLE_SHEET_BATS, &b->anim, &b->record);
    }
    for (int i = 0; i < AOS_TITLE_SPARKLES; ++i) {
        AosTitleSparkle *k = &t->sparkles[i];
        if (!k->active) continue;
        if (k->state == 0) {
            k->state = 1;
            k->scale = 0x20000;
            k->timer = 0x8000;
        }
        k->scale = k->size << 12;
        if (k->kind == 0) {
            int32_t v = 16 - (aos_sine((uint32_t)k->timer) >> 12);
            t->bldalpha = (uint16_t)((v << 8) | (16 - v));
        }
        k->angle = (uint16_t)(k->angle + k->spin);
        anim_step(t, AOS_TITLE_SHEET_DISC, &k->anim, &k->record);
        if (k->timer > 0) k->timer -= 0x180;
        else k->active = false;   /* + 0x53D bit 3: deleted after its update */
    }
}

/* sub_0803E8FC: 1 / x in 16.16. */
static int32_t reciprocal(int32_t x) {
    if (x == 0x10000) return 0x10000;
    int32_t d = x < 0 ? -((-x) >> 8) : x >> 8;
    return (0x10000 / d) * 256;
}

static int16_t toward_zero_8(int32_t v) {
    return (int16_t)(v < 0 ? -((-v) >> 8) : v >> 8);
}

/* sub_0803E058(angle, sx, sy, 0, 0, slot). */
static void sprite_affine(AosTitleSprite *sprite, uint16_t angle, int32_t sx, int32_t sy) {
    int32_t sine = aos_sine(angle), cosine = aos_sine(angle + 0x4000u);
    int32_t rx = sx > 0x400 ? reciprocal(sx) : 0x7FFFFF;
    int32_t ry = sy > 0x400 ? reciprocal(sy) : 0x7FFFFF;
    sprite->affine = true;
    sprite->pa = toward_zero_8(aos_fixed_mul(cosine, rx));
    sprite->pb = toward_zero_8(aos_fixed_mul(sine, rx));
    sprite->pc = toward_zero_8(aos_fixed_mul(-sine, ry));
    sprite->pd = toward_zero_8(aos_fixed_mul(cosine, ry));
}

/* The layers of sub_0803DD14 (stage 1): BG0 at 7, BG1 at 6, BG2 at 0x16,
 * BG3 at 0x1F (sub_0803DC18's default). An object layer up to 6 gets
 * priority 0, 7 priority 1, up to 0x16 priority 2, the others 3
 * (sub_0803DE10). */
static uint8_t layer_priority(unsigned layer) {
    return (uint8_t)(layer <= 6 ? 0 : layer == 7 ? 1 : layer <= 0x16 ? 2 : 3);
}

typedef struct {
    AosTitleSprite *sprites;
    uint8_t layers[AOS_TITLE_MAX_SPRITES];
    int count;
} SpriteList;

static int16_t pixel_of(int32_t v) {
    return (int16_t)(v < 0 ? -((-v) >> 16) : v >> 16);
}

/* sub_0803AC40: the culling box of the entity's position, then the OAM
 * list of its layer (sub_0803E528 appends). */
static AosTitleSprite *emit(SpriteList *list, unsigned layer, unsigned sheet, uint8_t record,
                            uint8_t palette_row, uint8_t mode, int32_t x, int32_t y) {
    int16_t hx = (int16_t)((uint32_t)x >> 16), hy = (int16_t)((uint32_t)y >> 16);
    if (hx < -0x40 || hx > 0x130 || hy < -0x20 || hy > 0xE0) return NULL;
    if (list->count >= AOS_TITLE_MAX_SPRITES) return NULL;
    AosTitleSprite *sprite = &list->sprites[list->count];
    *sprite = (AosTitleSprite){(uint8_t)sheet, record, palette_row, layer_priority(layer), mode,
                               false, 0x100, 0, 0, 0x100, pixel_of(x), pixel_of(y)};
    list->layers[list->count++] = (uint8_t)layer;
    return sprite;
}

/* sUnk_084F0CEC: the four quarters of entity 0x12's disc (frames 0-3,
 * rows 4-7) and of entity 0x15's ring (frames 0x0C-0x0F, row 0x0B). */
static const struct { uint8_t frame, row; int8_t x, y; } quarters[8] = {
    {0x0, 0x4, -32, -32}, {0x1, 0x5, 32, -32}, {0x2, 0x6, -32, 32}, {0x3, 0x7, 32, 32},
    {0xC, 0xB, -32, -32}, {0xD, 0xB, 32, -32}, {0xE, 0xB, -32, 32}, {0xF, 0xB, 32, 32},
};

int aos_title_screen_sprites(const AosTitleScreen *t,
                             AosTitleSprite sprites[AOS_TITLE_MAX_SPRITES]) {
    if (!(t->dispcnt & DCNT_OBJ) || t->stage < 2) return 0;
    SpriteList list = {sprites, {0}, 0};
    /* Entity 0x12 (sub_080046D0): its OBJ mode is + 0x53E bits 1-2. */
    if (!t->e12_hidden)
        for (int k = 0; k < 4; ++k)
            emit(&list, 0x11, AOS_TITLE_SHEET_DISC, quarters[k].frame, quarters[k].row,
                 t->e12_blend, t->e12_x + quarters[k].x * 0x10000,
                 t->e12_y + quarters[k].y * 0x10000);
    /* Entity 0x14 (sub_0800427C): rows 9 pixels apart from 0x8C - 4 * count,
     * sliding apart left and right; the selected entry on row 0x0B. */
    uint8_t entries[3];
    int n = aos_title_screen_entries(t, entries);
    for (int k = 0; k < n; ++k) {
        int32_t slide = ((t->first + k) & 1) ? -t->slide : t->slide;
        emit(&list, 0x11, AOS_TITLE_SHEET_MENU, entries[k], k == t->cursor ? 0xB : 0xC, 0,
             0x780000 + slide, (0x8C - 4 * n + 9 * k) * 0x10000);
    }
    /* Entity 0x15 (sub_080044DC): the ring around the moon, the copyright. */
    if (t->copyright_shown) {
        for (int k = 4; k < 8; ++k)
            emit(&list, 0x12, AOS_TITLE_SHEET_BATS, quarters[k].frame, quarters[k].row, 0,
                 (120 + quarters[k].x) * 0x10000, (80 + quarters[k].y) * 0x10000);
        emit(&list, 0x12, AOS_TITLE_SHEET_BATS, 0xB, 0xB, 0, 120 * 0x10000, 160 * 0x10000);
    }
    const AosTitleSwoop *w = &t->swoop;
    if (w->active && !w->hidden) {
        AosTitleSprite *s = emit(&list, 6, AOS_TITLE_SHEET_BATS, w->record, 0xC, 0, w->x, w->y);
        if (s) sprite_affine(s, 0, w->scale, w->scale);
    }
    for (int i = 0; i < AOS_TITLE_BATS; ++i) {
        const AosTitleBat *b = &t->bats[i];
        if (b->active && !b->hidden)
            emit(&list, 7, AOS_TITLE_SHEET_BATS, b->record, 0xD, b->semi ? 1 : 0, b->x, b->y);
    }
    for (int i = 0; i < AOS_TITLE_SPARKLES; ++i) {
        const AosTitleSparkle *k = &t->sparkles[i];
        if (!k->active) continue;
        AosTitleSprite *s = emit(&list, 6, AOS_TITLE_SHEET_DISC, k->record, 0xB, 1, k->x, k->y);
        if (s) sprite_affine(s, k->angle, k->scale, k->scale);
    }
    /* sub_0803DE10: layer 0 first, each layer in drawing order. */
    for (int i = 1; i < list.count; ++i)
        for (int j = i; j > 0 && list.layers[j - 1] > list.layers[j]; --j) {
            AosTitleSprite s = sprites[j];
            sprites[j] = sprites[j - 1];
            sprites[j - 1] = s;
            uint8_t l = list.layers[j];
            list.layers[j] = list.layers[j - 1];
            list.layers[j - 1] = l;
        }
    return list.count;
}

/* The menu's entries (sub_080043CC's counts) without SOUL TRADE. */
int aos_title_screen_entries(const AosTitleScreen *t, uint8_t entries[3]) {
    if (!t->menu_shown) return 0;
    if (t->first == AOS_TITLE_ENTRY_BOSS_RUSH) {
        entries[0] = AOS_TITLE_ENTRY_BOSS_RUSH;
        entries[1] = AOS_TITLE_ENTRY_SOUND_TEST;
        return 2;
    }
    entries[0] = AOS_TITLE_ENTRY_GAME_START;
    if (!t->unlocked) return 1;
    entries[1] = AOS_TITLE_ENTRY_EXTRA;
    return 2;
}

static uint8_t selected_entry(const AosTitleScreen *t) {
    uint8_t entries[3];
    int n = aos_title_screen_entries(t, entries);
    return n ? entries[t->cursor < n ? t->cursor : 0] : AOS_TITLE_ENTRY_GAME_START;
}

static int32_t entry_mode(uint8_t entry) {
    static const int32_t modes[5] = {AOS_MODE_MAIN_MENU, AOS_MODE_SOUL_TRADE_MENU,
                                     AOS_MODE_KONAMI_LOGO, AOS_MODE_BOSS_RUSH_MENU,
                                     AOS_MODE_SOUND_TEST_MENU};
    return modes[entry < 5 ? entry : 0];
}

/* sub_08004648: the extra entry opens (or B closes) the submenu, sliding. */
static void menu_toggle(AosTitleScreen *t) {
    int n;
    uint8_t entries[3];
    n = aos_title_screen_entries(t, entries);
    if (n && !t->chosen && !t->sliding) {
        t->slide = 0;
        t->slide_stage = 1;
        t->sliding = true;
    }
}

/* sub_0800432C. */
static void menu_update(AosTitleScreen *t, uint16_t repeated) {
    if (!t->menu_shown || t->chosen) return;
    if (t->slide_stage == 1) {
        t->slide += 0x180000;
        if (t->slide > 0x9FFFFF) {
            t->first = t->first == 0 ? AOS_TITLE_ENTRY_BOSS_RUSH : 0;
            t->cursor = 0;
            t->slide_stage = 2;
        }
    } else if (t->slide_stage == 2) {
        t->slide -= 0x100000;
        if (t->slide <= 0) {
            t->slide = 0;
            t->slide_stage = 0;
            t->sliding = false;
        }
    }
    uint8_t entries[3];
    int n = aos_title_screen_entries(t, entries);
    if (n <= 0) return;
    if (repeated & AOS_KEY_DOWN) {
        t->cursor = (int8_t)((t->cursor + 1) % n);
    } else if (repeated & AOS_KEY_UP) {
        if (--t->cursor < 0) t->cursor = (int8_t)(n - 1);
    }
}

/* The end of stage 7 and the START shortcut: the castle (BG0), the menu
 * and copyright, the white flash of the backgrounds and the objects' tint. */
static void reveal(AosTitleScreen *t, uint8_t tint) {
    delete_objects(t);
    t->dispcnt |= DCNT_BG0;
    t->e12_hidden = true;
    t->e12_blend = 0;
    t->menu_shown = true;
    t->first = 0;
    t->cursor = 0;
    t->copyright_shown = true;
    fade_color(&t->bg, 0x1F, 0x1F, 0x1F, 0, 0x10000, 0x20);
    fade_color(&t->obj, 0x1F, tint, tint, 0x2000, 0x10000, 0x20);
}

void aos_title_screen_init(AosTitleScreen *title, const AosTitleGraphics *graphics,
                           const uint8_t *obj_palette_rows, const uint8_t *cycle_rows_resource) {
    memset(title, 0, sizeof *title);
    title->graphics = graphics;
    title->obj_palette_rows = obj_palette_rows;
    title->cycle_rows = cycle_rows_resource;
    title->song = -1;
}

int32_t aos_title_screen_update(AosTitleScreen *t, uint16_t newly, uint16_t repeated) {
    int32_t mode = AOS_MODE_SAME;
    t->song = -1;
    switch (t->stage) {
    case 0: {
        /* A new entry: the title's entities start afresh. */
        AosTitleScreen fresh;
        aos_title_screen_init(&fresh, t->graphics, t->obj_palette_rows, t->cycle_rows);
        fresh.unlocked = t->unlocked;
        fresh.frame = t->frame;
        *t = fresh;
    }
        t->dispcnt = 0;
        t->bldcnt = BLD_FIRST_ALL | BLD_DECREASE;
        t->bldy = 16;
        t->bldalpha = 16 << 8;
        t->counter = 0;
        t->timed_out = false;
        t->stage = 1;
        break;
    case 1:
        t->song = MUS_27;
        memset(&t->bg, 0, sizeof t->bg);
        memset(&t->obj, 0, sizeof t->obj);
        fx_off(&t->bg, GLOBAL);
        t->dispcnt = DCNT_WINOBJ | DCNT_OBJ | DCNT_BG2 | DCNT_MODE_1;
        if (t->graphics) memcpy(t->bg.source, t->graphics->palette, sizeof t->bg.source);
        /* sub_08004160: entity 0x12 and its OBJ rows. */
        copy_rows(&t->obj, t->obj_palette_rows, 3, 1, 4);
        copy_rows(&t->obj, t->obj_palette_rows, 1, 1, 0xC);
        copy_rows(&t->obj, t->obj_palette_rows, 0, 1, 0xD);
        copy_rows(&t->obj, t->obj_palette_rows, 2, 1, 0xB);
        for (unsigned row = 5; row <= 7; ++row) copy_rows(&t->obj, t->obj_palette_rows, 3, 1, row);
        t->motion = 0x200000;
        t->reveal = 0x5000;
        t->e12_x = 0x980000;
        t->e12_y = 0x700000;
        set_affine(t, 0x108 - round_shift(t->motion, 15));
        create_objects(t);
        t->stage = 2;
        /* fall through */
    case 2:
        palette_cycle(t);
        t->bldy = (uint8_t)(16 - (t->counter >> 1));
        if (t->bldy > 0) {
            t->counter++;
        } else {
            t->bldcnt = BLD_SECOND_ALL | BLD_ALPHA;
            t->stage = 3;
        }
        break;
    case 3:
        palette_cycle(t);
        if (t->motion != 0) {
            t->motion -= 0x2000;
            t->e12_x -= 0x2000;
            t->e12_y -= 0x2000;
            if (t->motion == 0x40000) {
                t->bldy = 16;
                create_sparkle(t, 0, 0, 0x18, 0xEF);
                create_sparkle(t, 1, 1, 0x20, 0x89);
            }
            if (t->motion < 0x30000) {
                t->e12_flags |= 1;
                t->stage = 4;
            }
            set_affine(t, 0x108 - round_shift(t->motion, 15));
        }
        break;
    case 4:
        palette_cycle(t);
        if (t->motion != 0) {
            t->motion -= 0x2000;
            t->e12_x -= 0x2000;
            t->e12_y -= 0x2000;
            set_affine(t, 0x108 - round_shift(t->motion, 15));
        }
        if (!(t->e12_flags & 2)) break;
        t->stage = 5;
        /* fall through */
    case 5:
        palette_cycle(t);
        set_affine(t, 0x118 - t->progress);
        if (t->e12_flags & 2) t->e12_blend = 2;
        if (t->progress < 0x40) {
            t->progress++;
        } else {
            t->e13_timer = 0x88;
            t->bldalpha = 16 << 8;
            t->e13_flags |= 1;
            t->stage = 6;
        }
        break;
    case 6:
        palette_cycle(t);
        wobble(t);
        if (t->e13_timer != 0) {
            int32_t v = --t->e13_timer;
            t->bldalpha = (uint16_t)((round_shift(v, 3) << 8) | (16 - round_shift(v, 3)));
        } else {
            t->e13_flags |= 4;
            t->bldalpha = 16 << 8;
            t->bldcnt = BLD_SECOND_ALL | BLD_ALPHA | BLD_BG1_FIRST;
            t->dispcnt |= DCNT_BG1;
            t->stage = 7;
        }
        break;
    case 7:
        palette_cycle(t);
        wobble(t);
        if (t->reveal != 0) {
            if (!(t->e13_flags & 2) && t->reveal <= 0x37F) t->e13_flags |= 2;
            int32_t v = t->reveal;
            t->reveal = (v - 2) - ((round_shift(v, 8) << 8) / 80);
            if (t->reveal < 0xC0) t->reveal = 0;
            int32_t eva = t->reveal / 1280;
            t->bldalpha = (uint16_t)((eva << 8) | (16 - eva));
            set_wave(t, t->reveal);
        } else {
            reveal(t, 0x12);
            t->counter = 0x258;
            t->stage = 8;
        }
        break;
    case 8: {
        t->bldy = 16;
        palette_cycle(t);
        wobble(t);
        if (repeated & (AOS_KEY_DOWN | AOS_KEY_UP)) t->counter = 10 * 60;
        if (--t->counter == 0) t->timed_out = true;
        if (newly & (AOS_KEY_START | AOS_KEY_A)) {
            if (entry_mode(selected_entry(t)) == AOS_MODE_KONAMI_LOGO) {
                menu_toggle(t);
                break;
            }
        } else if ((newly & AOS_KEY_B) && t->first != 0) {
            menu_toggle(t);
            break;
        }
        if (!t->sliding && (t->timed_out || (newly & (AOS_KEY_START | AOS_KEY_A)))) {
            t->song = SONG_MUTE;
            t->chosen = true;
            t->stage = 9;
            t->counter = 0;
            t->bldcnt = BLD_FIRST_ALL | BLD_DECREASE;
            t->bldy = 0;
        }
        break;
    }
    case 9:
        palette_cycle(t);
        t->bldy = (uint8_t)(t->counter >> 1);
        if (t->bldy == 16) {
            mode = t->timed_out ? AOS_MODE_INTRO_CUTSCENE : entry_mode(selected_entry(t));
            t->stage = 0;
            return mode;
        }
        t->counter++;
        break;
    default:
        break;
    }
    if (t->stage >= 2 && t->stage <= 7 && (newly & (AOS_KEY_START | AOS_KEY_A))) {
        t->bldcnt = BLD_OBJ_SECOND | BLD_BG2_SECOND | BLD_ALPHA | BLD_BG1_FIRST;
        t->dispcnt |= DCNT_BG1 | DCNT_BG0;
        t->e12_x = 0x780000;
        t->e12_y = 0x500000;
        t->motion = 0;
        t->progress = 0x40;
        t->bldalpha = 16;
        set_wave(t, 0);
        reveal(t, 0x17);
        t->counter = 10 * 60;
        t->stage = 8;
    }
    /* sub_08000F60 / sub_08000B64: the entities, then (V-blank) the
     * palettes. */
    e12_update(t);
    menu_update(t, repeated);
    objects_update(t);
    apply_palette(&t->bg);
    apply_palette(&t->obj);
    t->frame++;
    return mode;
}

/* The GBA colour math of BLDCNT on two 15-bit colours. */
static uint16_t blend(uint16_t top, uint16_t below, int eva, int evb) {
    int r = ((top & 31) * eva + (below & 31) * evb) >> 4;
    int g = (((top >> 5) & 31) * eva + ((below >> 5) & 31) * evb) >> 4;
    int b = (((top >> 10) & 31) * eva + ((below >> 10) & 31) * evb) >> 4;
    if (r > 31) r = 31;
    if (g > 31) g = 31;
    if (b > 31) b = 31;
    return (uint16_t)(r | g << 5 | b << 10);
}

static uint16_t brightness(uint16_t c, int evy, bool increase) {
    int ch[3] = {c & 31, (c >> 5) & 31, (c >> 10) & 31};
    for (int k = 0; k < 3; ++k)
        ch[k] = increase ? ch[k] + (((31 - ch[k]) * evy) >> 4) : ch[k] - ((ch[k] * evy) >> 4);
    return (uint16_t)(ch[0] | ch[1] << 5 | ch[2] << 10);
}

void aos_title_screen_render(const AosTitleScreen *t, uint8_t rgba[AOS_TITLE_W * AOS_TITLE_H * 4]) {
    const AosTitleGraphics *g = t->graphics;
    int eva = t->bldalpha & 0x1F, evb = (t->bldalpha >> 8) & 0x1F, evy = t->bldy;
    if (eva > 16) eva = 16;
    if (evb > 16) evb = 16;
    if (evy > 16) evy = 16;
    int effect = (t->bldcnt >> 6) & 3;
    static AosObjLayer obj;
    aos_obj_layer_clear(&obj);
    if (g) {
        AosTitleSprite sprites[AOS_TITLE_MAX_SPRITES];
        int count = aos_title_screen_sprites(t, sprites);
        for (int k = 0; k < count; ++k) aos_obj_layer_draw(&obj, g->sheets, &sprites[k]);
    }
    for (int y = 0; y < AOS_TITLE_H; ++y)
        for (int x = 0; x < AOS_TITLE_W; ++x) {
            size_t at = (size_t)y * AOS_TITLE_W + (size_t)x;
            /* sub_08004160's windows: inside the OBJ window (WINOUT high
             * byte) everything but BG2, outside (WINOUT low byte) all. */
            bool bg2 = !((t->dispcnt & DCNT_WINOBJ) && obj.window[at]);
            uint8_t bgi[3] = {0, 0, 0};
            if (g && (t->dispcnt & DCNT_BG0)) bgi[0] = g->text[0][y * AOS_TITLE_LAYER_SIZE + x];
            if (g && (t->dispcnt & DCNT_BG1)) {
                int sx = x + (y < AOS_TITLE_WAVE_LINES ? t->wave[y] : 0);
                bgi[1] = g->text[1][y * AOS_TITLE_LAYER_SIZE + (sx & (AOS_TITLE_LAYER_SIZE - 1))];
            }
            if (g && bg2 && (t->dispcnt & DCNT_BG2))
                bgi[2] = aos_title_affine_index(g, &t->affine, x, y);
            bool has_obj = obj.index[at] != 0;
            /* Front to back by priority, an object before a background of
             * its priority. sub_0803FD60 ranks the backgrounds by their
             * layers (7, 6, 0x16): BG1 has priority 0, BG0 1, BG2 2. */
            static const int bg_of_priority[3] = {1, 0, 2};
            uint16_t colors[2] = {t->bg.shown[0], t->bg.shown[0]};
            int layers[2] = {5, 5};
            int found = 0;
            for (int prio = 0; prio < 4 && found < 2; ++prio) {
                if (has_obj && obj.priority[at] == prio) {
                    colors[found] = t->obj.shown[obj.index[at]];
                    layers[found++] = 4;
                }
                if (prio < 3 && found < 2 && bgi[bg_of_priority[prio]]) {
                    colors[found] = t->bg.shown[bgi[bg_of_priority[prio]]];
                    layers[found++] = bg_of_priority[prio];
                }
            }
            uint16_t c = colors[0];
            bool first = (t->bldcnt >> layers[0]) & 1;
            bool second = (t->bldcnt >> (8 + layers[1])) & 1;
            if (layers[0] == 4 && obj.semi[at] && second)
                c = blend(colors[0], colors[1], eva, evb);
            else if (effect == 1 && first && second)
                c = blend(colors[0], colors[1], eva, evb);
            else if ((effect == 2 || effect == 3) && first)
                c = brightness(c, evy, effect == 2);
            uint8_t *out = rgba + at * 4;
            out[0] = (uint8_t)(((c & 31) * 255) / 31);
            out[1] = (uint8_t)((((c >> 5) & 31) * 255) / 31);
            out[2] = (uint8_t)((((c >> 10) & 31) * 255) / 31);
            out[3] = 255;
        }
}
