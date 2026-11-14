/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow BG scroll engine state: the gEwramData bgInfo[4]
 * records at +0xA078 and the routines that drive them.
 *
 * Ports of cvaos:
 * - sub_0803FBBC (asm/code/code_08039340.s): the per-layer
 *   scroll-velocity request. Gates on pBgMetadata->unk_0/unk_1 (>1
 *   scrolls that axis, <=1 pins it: x to 0, y to 0x30), clamps the
 *   requested 16.16 velocity to +/-8 px per call and to the map
 *   edge, stores it in xVel/yVel, integrates xPos/yPos by it once,
 *   rebuilds the BG tile row (sub_08001994 lookups into the
 *   gEwramData + 0x13414 buffer + BgCmdBuffer_WriteData) when the
 *   scroll crosses an 8-pixel boundary, then writes the integer
 *   positions into the gDisplayRegisters bgOfs hOfs/vOfs shadow.
 *   Its four helpers sub_0803F3F8/sub_0803F4C8 (x, +/-) and
 *   sub_0803F580/sub_0803F650 (y, +/-) carry the clamp/integrate
 *   math; the tile-row rebuild itself is reported, not applied.
 * - sub_08000F60 (src/code_080009A0.c): adds -bgInfo[bg].xVel /
 *   yVel to the fine position (+0x524/+0x528) of every entityData
 *   record whose +0x59 bit 1 (unk_53D_1) is set — the
 *   screen-locked entity compensation pass.
 * - sub_0800D288 (src/code_0800CB00.c): reads bgInfo[1]'s velocity
 *   and propagates it to BG2/BG3 through sub_0803FBBC, scaled by
 *   each layer's unk_18 mode (parallax factors, unk_14/unk_16
 *   interpolation, direct bgOfs writes, and hblank/affine/scanline
 *   effects that are reported through AosScrollOut::fx instead of
 *   applied). A BG1 unk_18 of 0x1A first calls sub_0800CDAC.
 * - sub_0800ED24 (src/code_0800CB00.c): the gated transaction —
 *   gEwramData +0x427 (unk_60.unk_427) > param0 suppresses it;
 *   otherwise request(1) + entity compensation + apply.
 *
 * Coordinates are 16.16 pixels like the native CameraPart unions.
 * meta_0/meta_1 stand in for pBgMetadata->unk_0/unk_1, the BG map
 * size in 256-pixel units per axis (the same screen counts
 * follow_camera clamps against); unk_14/unk_16 are the per-layer
 * scroll ranges the interpolation modes scale by; unk_18 is the
 * propagation mode. No SDL. */
#ifndef AOS_SCROLL_H
#define AOS_SCROLL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* One gEwramData bgInfo[] record (struct EwramData_unkA078, 0x1C
 * bytes): pBgMetadata collapses to its two scroll-gate bytes. */
typedef struct {
    int32_t x_pos, y_pos;    /* +0x04/+0x08 xPos/yPos, 16.16 whole */
    int32_t x_vel, y_vel;    /* +0x0C/+0x10 xVel/yVel, 16.16 whole */
    uint16_t unk_14, unk_16; /* +0x14/+0x16 scroll ranges */
    uint8_t unk_18;          /* +0x18 propagation mode */
    uint8_t meta_0, meta_1;  /* pBgMetadata->unk_0/unk_1: map size in
                                256-px units per axis; <=1 pins the
                                axis (x to 0, y to 0x30 << 16) */
} AosBgLayer;

typedef struct {
    AosBgLayer bg[4];        /* gEwramData bgInfo[0..3], BG number */
    /* gDisplayRegisters bgOfs[] shadow: the integer scroll offsets
     * sub_0803FBBC writes back and modes 8/9/10/30 adjust. */
    uint16_t disp_h[4], disp_v[4];
} AosScroll;

/* entityData record view for sub_08000F60: +0x59 bit 1
 * (unk_53D_1) flags a screen-locked entity; +0x524/+0x528 are its
 * fine 16.16 positions relative to the scroll origin. */
typedef struct {
    bool flagged;            /* entityData +0x59 bit 1 */
    int32_t *fine_x, *fine_y;/* entityData +0x524 / +0x528 */
} AosScrollEntityRef;

/* Native globals sub_0800D288 reads besides bgInfo: the
 * gEwramData unk_60.unk_33C[3] flag word (modes 10/19/21 gates)
 * and unk_60.unk_4CC_0 (mode 19's BG2 keep bit). entities is the
 * sub_08000F60 list (native loops the 0xD0 entityData slots);
 * NULL/0 means no flagged entities are bound. */
typedef struct {
    uint32_t unk_33c3;
    bool unk_4cc_0;
    AosScrollEntityRef *entities;
    size_t entity_count;
} AosScrollWorld;

/* Unported native side effects of sub_0800D288, reported as bits
 * so the caller can log them; the scroll state they accompany is
 * still applied where the native writes it. */
enum {
    AOS_SCROLL_FX_CDAC = 1u << 0,      /* sub_0800CDAC(1,0x450,0x4A0) */
    AOS_SCROLL_FX_HBLANK = 1u << 1,    /* hBlankEffect wave buffers */
    AOS_SCROLL_FX_SCANLINE = 1u << 2,  /* sub_0800CB8C / sub_0800CED4 /
                                          sub_0800D0F8 scanline/window FX */
    AOS_SCROLL_FX_CC90 = 1u << 3,      /* sub_0800CC90 */
    AOS_SCROLL_FX_CB00 = 1u << 4,      /* sub_0800CB00 */
    AOS_SCROLL_FX_D000 = 1u << 5,      /* sub_0800D000 */
    AOS_SCROLL_FX_AFFINE = 1u << 6,    /* BgAffineSet on BG2 */
    AOS_SCROLL_FX_BG2_DISABLE = 1u << 7 /* dispCnt &= ~DCNT_BG2 (mode 19) */
};

typedef struct {
    uint32_t fx;               /* AOS_SCROLL_FX_* bits */
    int requests;              /* sub_0803FBBC calls that ran */
    size_t entities_shifted;   /* sub_08000F60 flagged records moved */
    bool map_rebuild_x, map_rebuild_y; /* BG tile-row rewrite queued:
                                          sub_08001994 lookups +
                                          BgCmdBuffer_WriteData */
} AosScrollOut;

/* sub_0803F8A8's binding half: attaches the layer's scroll gates
 * and mode, sets the integer position (16.16 whole here) and
 * clears the velocities. BG2/BG3 layer records (unk_18/unk_14/
 * unk_16) are per-room data the runtime does not extract yet;
 * leaving them 0 keeps mode 0 (no propagation). */
void aos_scroll_bind(AosScroll *s, int bg, uint8_t meta_0, uint8_t meta_1,
                     uint8_t mode, uint16_t unk_14, uint16_t unk_16,
                     int32_t x_pos, int32_t y_pos);

/* sub_0803FBBC(bg, vx, vy): the scroll-velocity request. */
void aos_scroll_request(AosScroll *s, int bg, int32_t vx, int32_t vy,
                        AosScrollOut *out);

/* sub_08000F60(bg): shifts every flagged entity by
 * -bgInfo[bg].xVel/-yVel; returns the shifted count. */
size_t aos_scroll_shift_entities(const AosScroll *s, int bg,
                                 AosScrollEntityRef *ents, size_t count);

/* sub_0800D288: propagates bgInfo[1]'s velocity to BG2/BG3. */
void aos_scroll_apply(AosScroll *s, const AosScrollWorld *world,
                      AosScrollOut *out);

/* sub_0800ED24(param0, vx, vy): the gated transaction — returns 0
 * when gEwramData +0x427 (gate_byte) exceeds param0, else runs the
 * three callees on layer 1 and returns 1. `world` may be NULL. */
int aos_scroll_transaction(AosScroll *s, uint8_t gate_byte, int param0,
                           int32_t vx, int32_t vy,
                           const AosScrollWorld *world, AosScrollOut *out);

#endif /* AOS_SCROLL_H */
