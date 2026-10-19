/* SPDX-License-Identifier: GPL-3.0-only */
/* The shared non-gameplay screen routines (see mzm_screen.h). */
#include "mzm_screen.h"

#include <stdlib.h>
#include <string.h>

#include "mzm_rom.h"

enum { OAM_ID_CHANGED_FLAG = 2 };

uint32_t mzm_sym(const char *name) { return mzm_rom_symbol(name, NULL); }

bool mzm_screen_init(MzmScreen *s, GbaPpu *ppu) {
    memset(s, 0, sizeof *s);
    s->ppu = ppu;
    s->ewram = calloc(MZM_EWRAM_SIZE, 1);
    return s->ewram != NULL;
}

void mzm_screen_free(MzmScreen *s) {
    free(s->ewram);
    s->ewram = NULL;
}

void mzm_screen_input(MzmScreen *s, uint16_t held) {
    s->changed_input = (uint16_t)(held & ~s->button_input);
    s->button_input = held;
}

void mzm_clear_gfx_ram(MzmScreen *s) {
    memset(s->ppu->vram, 0, sizeof s->ppu->vram);
    memset(s->ppu->oam, 0xFF, sizeof s->ppu->oam);
    memset(s->ppu->palette, 0, sizeof s->ppu->palette);
}

void mzm_reset_free_oam(MzmScreen *s) {
    for (int i = s->next_oam_slot; i < MZM_OAM_BUFFER; ++i) {
        s->oam[i * 4] = 0xFF;
        s->oam[i * 4 + 1] = 0xFF;
        s->oam[i * 4 + 2] = 0;
    }
}

void mzm_screen_copy_oam(MzmScreen *s) {
    memcpy(s->ppu->oam, s->oam, sizeof s->ppu->oam);
}

void mzm_menu_oam_set_id(MzmMenuOam *oam, uint8_t oam_id) {
    oam->oam_id = oam_id;
    oam->exists = oam_id ? OAM_ID_CHANGED_FLAG : 0;
}

MzmMenuOam mzm_menu_oam_from_rom(uint32_t address) {
    MzmMenuOam o = {0};
    const uint8_t *b = mzm_rom_ptr(address, 16);
    if (!b) return o;
    o.y = (int16_t)(b[0] | b[1] << 8);
    o.x = (int16_t)(b[2] | b[3] << 8);
    o.unk_4 = b[4];
    o.unk_5 = b[5];
    o.unk_6 = b[6];
    o.unk_7 = b[7];
    o.animation_duration_counter = b[8];
    o.current_animation_frame = b[9];
    o.oam_id = b[10];
    /* The u16 bit-field shares the halfword of oamId (byte 0xB) and the u8
     * one is byte 0xC. */
    o.priority = b[11] & 3;
    o.obj_mode = b[11] >> 2 & 3;
    o.ended = b[11] >> 4 & 1;
    o.not_drawn = b[11] >> 5 & 1;
    o.exists = b[11] >> 6 & 3;
    o.bound_background = b[12] & 15;
    o.rotation_scaling = b[12] >> 4 & 1;
    o.unk_e = (int16_t)(b[14] | b[15] << 8);
    return o;
}

/* struct OamArray: the FrameData pointer and the pre-action (8 bytes);
 * struct FrameData: the OAM frame pointer and its timer (8 bytes). */
static uint32_t frames_of(uint32_t oam_array, uint8_t id) { return mzm_rom_u32(oam_array + id * 8u); }
static uint8_t pre_action(uint32_t oam_array, uint8_t id) { return mzm_rom_u8(oam_array + id * 8u + 4); }
static uint8_t frame_timer(uint32_t frames, uint8_t frame) { return mzm_rom_u8(frames + frame * 8u + 4); }
static uint32_t frame_oam(uint32_t frames, uint8_t frame) { return mzm_rom_u32(frames + frame * 8u); }

void mzm_menu_oam_process(MzmScreen *s, int length, MzmMenuOam *o, uint32_t oam_array) {
    int next = s->next_oam_slot, current = next;
    for (int i = 0; i < length; ++i, ++o) {
        if (o->oam_id == 0 || !o->exists) continue;
        if (o->exists == OAM_ID_CHANGED_FLAG) {
            o->exists = 1;
            o->animation_duration_counter = 0;
            o->current_animation_frame = 0;
        }
        o->ended = false;
        uint32_t frames = frames_of(oam_array, o->oam_id);
        if (o->animation_duration_counter >= frame_timer(frames, o->current_animation_frame)) {
            uint8_t part;
            switch (pre_action(oam_array, o->oam_id)) {
            case MZM_OAM_PRE_NONE:
                break;
            case MZM_OAM_PRE_CHANGE_FRAME:
                o->animation_duration_counter = 0;
                o->current_animation_frame++;
                if (frame_timer(frames, o->current_animation_frame) == 0) o->current_animation_frame = 0;
                break;
            case MZM_OAM_PRE_RESET_FRAME:
                o->animation_duration_counter = 0;
                break;
            case MZM_OAM_PRE_LOOP_ON_LAST_FRAME:
                part = frame_timer(frames, o->current_animation_frame);
                o->current_animation_frame++;
                if (frame_timer(frames, o->current_animation_frame) == 0) {
                    o->current_animation_frame--;
                    o->animation_duration_counter = (uint8_t)(part - 1);
                    o->ended = true;
                } else {
                    o->animation_duration_counter = 0;
                }
                break;
            case MZM_OAM_PRE_KILL_AFTER_END:
                o->animation_duration_counter = 0;
                o->current_animation_frame++;
                if (frame_timer(frames, o->current_animation_frame) == 0) {
                    o->oam_id = 0;
                    o->current_animation_frame = 0;
                    o->exists = 0;
                    continue;
                }
                break;
            case MZM_OAM_PRE_INCREMENT_ID_AFTER_END:
                o->animation_duration_counter = 0;
                o->current_animation_frame++;
                if (frame_timer(frames, o->current_animation_frame) == 0) {
                    o->oam_id++;
                    o->current_animation_frame = 0;
                    frames = frames_of(oam_array, o->oam_id);
                }
                break;
            case MZM_OAM_PRE_INCREMENT_ID_LOCK:
                o->animation_duration_counter = 0;
                o->current_animation_frame++;
                if (frame_timer(frames, o->current_animation_frame) == 0) {
                    o->oam_id++;
                    o->current_animation_frame--;
                    frames = frames_of(oam_array, o->oam_id);
                }
                break;
            case MZM_OAM_PRE_DECREMENT_ID_AFTER_END:
                o->animation_duration_counter = 0;
                o->current_animation_frame++;
                if (frame_timer(frames, o->current_animation_frame) == 0) {
                    o->oam_id--;
                    o->current_animation_frame = 0;
                    frames = frames_of(oam_array, o->oam_id);
                }
                break;
            case MZM_OAM_PRE_PLAY_BACKWARDS:
                o->animation_duration_counter = 0;
                if (o->current_animation_frame == 0) {
                    o->oam_id = 0;
                    o->current_animation_frame = 0;
                    o->exists = 0;
                    continue;
                }
                o->current_animation_frame--;
                break;
            case MZM_OAM_PRE_PLAY_BACKWARDS_DECREMENT_ID:
                o->animation_duration_counter = 0;
                if (o->current_animation_frame != 0) {
                    o->current_animation_frame--;
                } else {
                    o->oam_id--;
                    o->current_animation_frame = 0;
                    frames = frames_of(oam_array, o->oam_id);
                }
                break;
            default:
                break;
            }
        }
        o->animation_duration_counter++;
        if (o->not_drawn) continue;
        int32_t x = 0, y = 0;
        if (o->bound_background < 4) {
            x = (int16_t)s->bg_hofs[o->bound_background];
            y = (int16_t)s->bg_vofs[o->bound_background];
        }
        /* SUB_PIXEL_TO_PIXEL_ is DIV_SHIFT: an arithmetic shift */
        y = (int16_t)((o->y >> 2) - (y >> 2) + s->oam_y_offset);
        x = (int16_t)((o->x >> 2) - (x >> 2) + s->oam_x_offset);
        uint32_t src = frame_oam(frames, o->current_animation_frame);
        uint16_t count = mzm_rom_u16(src);
        if (next + count > 0x7F) break;
        next += count;
        src += 2;
        for (; current < next; ++current, src += 6) {
            uint16_t *dst = &s->oam[current * 4];
            uint16_t a0 = mzm_rom_u16(src), a1 = mzm_rom_u16(src + 2), a2 = mzm_rom_u16(src + 4);
            a0 = (uint16_t)((a0 & 0xFF00) | ((a0 + y) & 0xFF));
            a1 = (uint16_t)((a1 & 0xFE00) | ((a1 + x) & 0x1FF));
            a2 = (uint16_t)((a2 & ~(3 << 10)) | (o->priority & 3) << 10);
            a0 = (uint16_t)((a0 & ~(3 << 10)) | (o->obj_mode & 3) << 10);
            dst[0] = a0;
            dst[1] = a1;
            dst[2] = a2;
        }
    }
    s->next_oam_slot = (uint8_t)next;
}

void mzm_apply_fading_color(uint8_t type, uint8_t color, const uint16_t *src, uint16_t *dst,
                            uint16_t mask) {
    for (int i = 0; i < 16; ++i, src += 16, dst += 16) {
        if (!(mask >> i & 1)) continue;
        for (int j = 0; j < 16; ++j) {
            int r = src[j] & 31, g = src[j] >> 5 & 31, b = src[j] >> 10 & 31;
            switch (type) {
            case MZM_FADING_IN:
                r = (r * color) >> 5 & 31;
                g = (g * color) >> 5 & 31;
                b = (b * color) >> 5 & 31;
                break;
            case MZM_FADING_FLASH:
                r = (31 - (((31 - r) * color) >> 5)) & 31;
                g = (31 - (((31 - g) * color) >> 5)) & 31;
                b = (31 - (((31 - b) * color) >> 5)) & 31;
                break;
            case MZM_FADING_OUT:
                r = (r - ((r * color) >> 5)) & 31;
                g = (g - ((g * color) >> 5)) & 31;
                b = (b - ((b * color) >> 5)) & 31;
                break;
            default:
                r = (r + ((color * (31 - r)) >> 5)) & 31;
                g = (g + ((color * (31 - g)) >> 5)) & 31;
                b = (b + ((color * (31 - b)) >> 5)) & 31;
                break;
            }
            dst[j] = (uint16_t)(r | g << 5 | b << 10);
        }
    }
}

bool mzm_lz77_from_rom(uint32_t address, uint8_t *dst, size_t dst_size) {
    uint32_t available;
    const uint8_t *src = mzm_rom_span(address, &available);
    return src && gba_lz77_uncompress(src, available, dst, dst_size) >= 0;
}

void mzm_copy_from_rom(void *dst, uint32_t address, size_t size) {
    const uint8_t *src = mzm_rom_ptr(address, (uint32_t)size);
    if (src) memcpy(dst, src, size);
}

/* ProcessComplexOam and CalculateOamPart4's helpers */
enum { COMPLEX_OAM_BLOCK = 64, COMPLEX_OAM_PI_2 = 64 };

static uint16_t *complex_oam_attributes(MzmScreen *s, int slot) { return &s->oam[slot * 4]; }

static int16_t affine_sine(uint16_t angle) {
    return (int16_t)mzm_rom_u16(mzm_sym("sSineTable") + (uint32_t)angle * 2u);
}

static int16_t fixed_multiply(int16_t a, int16_t b) {
    return (int16_t)(((int32_t)a * b) >> 8);
}

static int16_t fixed_inverse(int16_t value) {
    return (int16_t)(65536 / value);
}

void mzm_process_complex_oam(MzmScreen *s, int slot, int16_t x_position, int16_t y_position,
                             uint16_t rotation, int16_t scaling, uint16_t matrix) {
    uint16_t *a = complex_oam_attributes(s, slot);
    int32_t x_origin = (int16_t)((uint16_t)x_position + COMPLEX_OAM_BLOCK);
    int32_t y_origin = (int16_t)((uint16_t)y_position + COMPLEX_OAM_BLOCK);
    int shape = a[0] >> 14, size = a[1] >> 14;
    uint32_t radius = mzm_sym("sSpriteXHalfRadius") + (uint32_t)(shape * 4 + size);
    uint8_t x_half = mzm_rom_u8(radius);
    uint8_t y_half = mzm_rom_u8(mzm_sym("sSpriteYHalfRadius") +
                                (uint32_t)(shape * 4 + size));
    int32_t x = (int16_t)(((a[1] & 0x1FF) + x_origin) & 511);
    int32_t y = (int16_t)(((a[0] & 0xFF) + y_origin) & 255);
    int32_t local_x = (int16_t)(x - x_origin + x_half);
    int32_t local_y = (int16_t)(y - y_origin + y_half);
    x += (int16_t)(((local_x * scaling) >> 8) - local_x);
    y += (int16_t)(((local_y * scaling) >> 8) - local_y);
    int32_t u = (int16_t)(x - x_origin + x_half);
    int32_t v = (int16_t)(y - y_origin + y_half);
    int32_t cosine = affine_sine((uint16_t)(rotation + COMPLEX_OAM_PI_2));
    int32_t sine = affine_sine(rotation);
    x = (int16_t)((u * cosine - v * sine) >> 8);
    y = (int16_t)((u * sine + v * cosine) >> 8);
    a[0] = (uint16_t)((a[0] & ~(3 << 8)) | 3 << 8);
    x -= x_half * 2;
    y -= y_half * 2;
    a[1] = (uint16_t)((a[1] & 0xFE00) | ((x + x_origin - COMPLEX_OAM_BLOCK) & 0x1FF));
    a[0] = (uint16_t)((a[0] & 0xFF00) | ((y + y_origin - COMPLEX_OAM_BLOCK) & 0xFF));
    bool x_flip = (a[1] & 1 << 12) != 0;
    bool y_flip = (a[1] & 1 << 13) != 0;
    uint16_t number = (uint16_t)(matrix +
        (x_flip ? (y_flip ? 3 : 1) : (y_flip ? 2 : 0)));
    a[1] = (uint16_t)((a[1] & ~(0x1F << 9)) | (number & 0x1F) << 9);
}

void mzm_calculate_oam_part4(MzmScreen *s, uint16_t rotation, int16_t scaling, int slot) {
    int16_t cosine = affine_sine((uint16_t)(rotation + COMPLEX_OAM_PI_2));
    int16_t sine = affine_sine(rotation);
    int16_t inverse = fixed_inverse(scaling);
    int16_t negative_inverse = fixed_inverse((int16_t)-scaling);
    int16_t dy1 = fixed_multiply(cosine, inverse);
    int16_t dmy1 = fixed_multiply(sine, inverse);
    int16_t dx1 = fixed_multiply((int16_t)-sine, inverse);
    int16_t dmx1 = fixed_multiply(cosine, negative_inverse);
    int16_t dy2 = fixed_multiply(sine, negative_inverse);
    int16_t dmy2 = fixed_multiply((int16_t)-sine, negative_inverse);
    const int16_t parameters[16] = {
        dy1, dmy1, dx1, dy1, dmx1, dy2, dx1, dy1,
        dy1, dmy1, dmy2, dmx1, dmx1, dy2, dmy2, dmx1,
    };
    for (int i = 0; i < 16; ++i)
        complex_oam_attributes(s, slot + i)[3] = (uint16_t)parameters[i];
}
