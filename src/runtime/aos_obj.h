/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria's objects as the OAM builders draw them (sub_0804311C, and the
 * affine sub_0804342C): a 16 x 16-tile sheet with its frame records and
 * animations (aos_obj_rom decodes them from the ROM), the sprites of an OAM
 * list, and the OBJ layer of a 240 x 160 screen they make. No SDL, no ROM
 * access here. */
#ifndef AOS_OBJ_H
#define AOS_OBJ_H

#include <stdbool.h>
#include <stdint.h>

enum { AOS_OBJ_SHEET_SIZE = 128, AOS_OBJ_MAX_FRAMES = 64, AOS_OBJ_MAX_PIECES = 9,
       AOS_OBJ_MAX_ANIMS = 13, AOS_OBJ_MAX_STEPS = 8, AOS_OBJ_SCREEN_W = 240, AOS_OBJ_SCREEN_H = 160 };

/* One OAM component of a frame record (12 bytes): its offset from the
 * entity, its size and its place on the sheet in pixels, and its flips
 * (+ 0x0A bit 1: horizontal, bit 0: vertical). */
typedef struct {
    int8_t x, y;
    uint8_t width, height, source_x, source_y;
    bool hflip, vflip;
} AosObjPiece;

typedef struct {
    uint8_t count;   /* record + 5; the first piece is drawn on top */
    AosObjPiece pieces[AOS_OBJ_MAX_PIECES];
} AosObjFrame;

/* A tile sheet as 128 x 128 colour indices (0-15, 0 transparent), its
 * frame records and its animations (encoding 1: frame, duration). */
typedef struct {
    uint8_t pixels[AOS_OBJ_SHEET_SIZE * AOS_OBJ_SHEET_SIZE];
    uint8_t frame_count;
    AosObjFrame frames[AOS_OBJ_MAX_FRAMES];
    uint8_t anim_count;
    uint8_t anim_length[AOS_OBJ_MAX_ANIMS];
    uint8_t anim_frame[AOS_OBJ_MAX_ANIMS][AOS_OBJ_MAX_STEPS];
    uint8_t anim_duration[AOS_OBJ_MAX_ANIMS][AOS_OBJ_MAX_STEPS];
} AosObjSheet;

/* One sprite of an OAM list: a frame record of a sheet (an index into the
 * caller's sheets), its OBJ palette row, its priority, its OBJ mode (0
 * normal, 1 semi-transparent, 2 OBJ window), its position and, when
 * affine, the 8.8 matrix of sub_0803E058 mapping screen offsets to frame
 * offsets (double size). */
typedef struct {
    uint8_t sheet, record, palette_row, priority, mode;
    bool affine;
    int16_t pa, pb, pc, pd;
    int16_t x, y;
} AosObjSprite;

/* The OBJ layer of a screen: per pixel the OBJ palette index of the
 * winning sprite (0: none; the lowest priority, then the first in the OAM),
 * its priority, whether it is semi-transparent, and the OBJ window. */
typedef struct {
    uint8_t index[AOS_OBJ_SCREEN_W * AOS_OBJ_SCREEN_H];
    uint8_t priority[AOS_OBJ_SCREEN_W * AOS_OBJ_SCREEN_H];
    bool semi[AOS_OBJ_SCREEN_W * AOS_OBJ_SCREEN_H];
    bool window[AOS_OBJ_SCREEN_W * AOS_OBJ_SCREEN_H];
} AosObjLayer;

void aos_obj_layer_clear(AosObjLayer *layer);
/* One sprite into the layer, after the sprites drawn before it in the OAM
 * (the double-size box of each affine piece is not clipped). */
void aos_obj_layer_draw(AosObjLayer *layer, const AosObjSheet *sheets, const AosObjSprite *sprite);

#endif
