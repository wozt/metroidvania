/* SPDX-License-Identifier: GPL-3.0-only */
/* Minimal affine sprite path for the pause-menu transient hands.
 *
 * Matrix decode (third_party/cvaos):
 *  - sub_0803E058(angle, scale_x, scale_y, flip_h, flip_v, slot) fills one
 *    of the 32 OBJ affine parameter slots (gUnk_03001458). `angle` is a
 *    Q16.16 fraction of a full turn (0x10000 = 360 degrees) fed to
 *    sub_080009E4's sine table; `scale_x`/`scale_y` are Q16.16 (0x10000 =
 *    1.0). sub_0803E968 builds the forward 2x2
 *      [ sx*cos  -sx*sin ]
 *      [ sy*sin   sy*cos ]
 *    in Q16.16, then sub_0803E058 stores the four s16 8.8 hardware
 *    parameters (unk_20..unk_26, copied to the OAM affine slot by
 *    sub_0803DF70) that map *screen* offsets back to *frame* offsets:
 *      pa = cos/sx, pb = sin/sx, pc = -sin/sy, pd = cos/sy
 *    computed through sub_0803E86C (Q16.16 multiply) and sub_0803E8FC
 *    (Q16.16 reciprocal, 0x7FFFFF sentinel for scales <= 0x400). The
 *    native anisotropic case keeps sx on the sine terms of both rows —
 *    a quirk this port reproduces exactly.
 *  - sub_0804875C (press flourish, entity +0xF34): state +0x24 == 1
 *    anchors the draw position at pixel (0x35,9), clears the +0x0D
 *    timer and arms +0x59 bit 4; state 2 then for timer <= 0xF calls
 *    sub_0803E058(0, timer<<12, timer<<12, 0, 0, slot=entity+0x2C) —
 *    angle 0, uniform scale ramping 0 -> 0xF000 (0 -> ~0.94) — and on
 *    the next update clears +0x59 bits 0x11, ending the affine pass.
 *  - sub_08048108/sub_080486E0 (Soul Set L/R transition hand, entity
 *    +0xFB8): a three-sprite slide machine; only its scale pulses use
 *    sub_0803E058, again with angle 0.
 *
 * Both target animations therefore only ever need angle = 0, but the
 * matrix helper implements the full decoded routine so rotated/scaled
 * sprites stay available. The blit uses the same inverse mapping as
 * aos_obj_layer_draw (screen offset -> frame offset), which is how the
 * GBA affine parameters are consumed.
 */
#ifndef AOS_AFFINE_SPRITE_H
#define AOS_AFFINE_SPRITE_H

#include <stdbool.h>
#include <stdint.h>

#include "../game/aos_obj.h"

/* sub_0803E058's matrix in the four s16 8.8 parameters the hardware
 * consumes: fx = (pa*dx + pb*dy) >> 8, fy = (pc*dx + pd*dy) >> 8 with
 * (dx,dy) the screen offset from the sprite anchor. `angle` is Q16.16
 * fraction of a turn, `scale_x`/`scale_y` Q16.16 (0x10000 = 1.0);
 * `flip_h`/`flip_v` are the routine's param_3/param_4. */
void aos_affine_matrix(int32_t angle, int32_t scale_x, int32_t scale_y,
                       bool flip_h, bool flip_v, int16_t params[4]);

/* Draw `frame` of `sheet` into `rgba` (width*height*4 bytes, top-left
 * origin) anchored at pixel (anchor_x, anchor_y): each output pixel's
 * frame coordinate comes from the affine inverse map above, then the
 * frame's pieces are sampled like aos_obj_layer_draw. `palette` holds
 * the 16 BGR555 colours of the sprite's OBJ palette row. Pixels outside
 * the buffer are clipped; transparent frame texels leave `rgba`
 * untouched. */
void aos_affine_sprite_draw(const AosObjSheet *sheet, const AosObjFrame *frame,
                            const uint16_t palette[16], int anchor_x,
                            int anchor_y, const int16_t params[4],
                            uint8_t *rgba, int width, int height);

#endif /* AOS_AFFINE_SPRITE_H */
