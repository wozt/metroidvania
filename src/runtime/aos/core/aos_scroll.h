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
 *   math; the tile-row rebuild (20/30 sub_08001994 lookups into
 *   the gEwramData + 0x13414 staging buffer plus one
 *   BgCmdBuffer_WriteData to 0x0600E000 + (bg<<11) + tile offset)
 *   is reported as an AosScrollRebuild DMA descriptor. When a
 *   tile-word source is bound (aos_scroll_tile_source — the host
 *   wires aos/core/aos_tilemap's sub_08001994 port), the
 *   descriptor's `words` carry the real staging-buffer tile
 *   words; the VRAM write itself stays unported (the host has no
 *   BG tilemap memory — the draw path samples a pre-rendered
 *   background.bmp).
 * - sub_0803FC6C (same asm block): the room-transition sibling
 *   of sub_0803FBBC — identical skeleton and x axis, but the y
 *   axis runs sub_0803FA10/sub_0803FAE0: edge constants
 *   +0x14/+0xA00000 (a 0xA0-px screen vs 0xD0), negative y pins
 *   at 0 instead of 0x30 << 16, and a gated y axis pins yPos to
 *   0 instead of 0x300000. Its only native callers are
 *   GameModeIntroCutsceneUpdate stage 3
 *   (sub_0803FC6C(1..3, 0, -0x1C00/-0x3200/-0x1000)) and
 *   GameModeCreditsUpdate stage 3
 *   (sub_0803FC6C(1..3, 0, 0x8400)); the door room transition
 *   sub_080104EC uses plain sub_0803FBBC while the display is
 *   blanked instead.
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

/* Optional tile-word source for the rebuild DMA: bound once per
 * room through aos_scroll_tile_source (the runtime wires
 * aos/core/aos_tilemap's sub_08001994 port). When set, every
 * queued AosScrollRebuild carries the real looked-up tile words
 * in its staging-buffer order; when NULL the descriptor keeps
 * word_count 0. `column` selects the span orientation the way the
 * native helper pair does: true fixes tile_x and steps tile_y
 * upward (the sub_0803F3F8/sub_0803F4C8 x-axis rebuild), false
 * fixes tile_y and steps tile_x upward (sub_0803F580/sub_0803F650
 * and the sub_0803FC6C transition siblings). tile_x/tile_y are the
 * descriptor's lookup origin. Writes at most `count` words to
 * `out` and returns the number written. */
typedef size_t (*AosScrollTileFetch)(void *context, int bg, bool column,
                                     uint16_t tile_x, uint16_t tile_y,
                                     size_t count, uint16_t *out);

typedef struct {
    void *context;
    AosScrollTileFetch fetch;   /* NULL: descriptors stay word-less */
} AosScrollTileSource;

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
    /* Room tilemap hook for the rebuild DMA's sub_08001994 lookups;
     * survives aos_scroll_bind (it is room-scoped, not
     * layer-scoped) but not a whole-struct reset. */
    AosScrollTileSource tile_source;
} AosScroll;

/* entityData record view for sub_08000F60: +0x59 bit 1
 * (unk_53D_1) flags a screen-locked entity; +0x524/+0x528 are its
 * fine 16.16 positions relative to the scroll origin. */
typedef struct {
    bool flagged;            /* entityData +0x59 bit 1 */
    int32_t *fine_x, *fine_y;/* entityData +0x524 / +0x528 */
} AosScrollEntityRef;

/* Native globals sub_0800D288 reads besides bgInfo: the
 * gEwramData unk_60.unk_33C[3] flag word (modes 10/19/21 gates),
 * unk_60.unk_4CC_0 (mode 19's BG2 keep bit) and unk_60.unk_4C8
 * (the hblank/affine wave parameter modes 20/21/27/30/36 feed to
 * the hBlankEffect buffer and BgAffineSet). entities is the
 * sub_08000F60 list (native loops the 0xD0 entityData slots);
 * NULL/0 means no flagged entities are bound. */
typedef struct {
    uint32_t unk_33c3;
    bool unk_4cc_0;
    uint32_t unk_4c8;
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

/* The tile-row rebuild DMA sub_0803FBBC queues through
 * BgCmdBuffer_WriteData when a scroll request crosses an 8-px
 * tile boundary: 20 (x scroll) or 30 (y scroll) halfword lookups
 * through sub_08001994 fill the gEwramData +0x13414 staging
 * buffer, then one BgCmdBuffer_WriteData streams it to the BG
 * tilemap in VRAM. The tile IDs themselves come from the room
 * tilemap (sub_08001800's pBgMetadata->unk_C block lookup plus
 * the per-BG tile arrays); when the scroll state has a bound
 * AosScrollTileSource the descriptor's `words` hold the real
 * looked-up staging buffer, otherwise word_count stays 0. The
 * VRAM write itself stays unported either way: the host has no
 * BG tilemap memory for BgCmdBuffer_WriteData to land in. */
typedef struct {
    bool queued;             /* a rebuild was armed this call */
    int bg;                  /* bgInfo[] index the DMA targets */
    uint32_t vram_addr;      /* 0x0600E000 + (bg<<11)
                                + (tile_x&0x1F)<<1
                                + (tile_y&0x1F)<<6 */
    uint8_t bg_cmd;          /* BgCmdBuffer_WriteData arg:
                                0x80 for the x-axis rebuilds
                                (sub_0803F3F8/sub_0803F4C8),
                                0x00 for the y-axis ones
                                (sub_0803F580/sub_0803F650) */
    uint8_t length;          /* 0x15 (x) / 0x1F (y) */
    uint16_t tile_x, tile_y; /* sub_08001994 lookup origin:
                                the fixed coordinate and the
                                start of the iterated one */
    uint16_t count;          /* 0x15 (x) / 0x1F (y) lookups */
    /* The staging-buffer words the native sub_08001994 loop would
     * fill before BgCmdBuffer_WriteData streams them: `count`
     * entries in lookup order (tile_y stepping for the x rebuild,
     * tile_x for the y rebuild), palette-XORed like the native
     * result. word_count is 0 when no tile source is bound or the
     * source declined the span. */
    uint16_t words[0x1F];
    uint16_t word_count;
} AosScrollRebuild;

typedef struct {
    uint32_t fx;               /* AOS_SCROLL_FX_* bits */
    int requests;              /* sub_0803FBBC calls that ran */
    size_t entities_shifted;   /* sub_08000F60 flagged records moved */
    bool map_rebuild_x, map_rebuild_y; /* BG tile-row rewrite queued:
                                          sub_08001994 lookups +
                                          BgCmdBuffer_WriteData */
    AosScrollRebuild rebuild_x, rebuild_y; /* the queued DMA
                                          descriptors */
    uint32_t unk_4c8_seen;     /* last unk_60.unk_4C8 value a mode
                                  consumed (hblank/affine cases
                                  20/21/27/30/36) */
} AosScrollOut;

/* sub_0803F8A8's binding half: attaches the layer's scroll gates
 * and mode, sets the integer position (16.16 whole here) and
 * clears the velocities. BG2/BG3 layer records (unk_18/unk_14/
 * unk_16) are per-room data the runtime does not extract yet;
 * leaving them 0 keeps mode 0 (no propagation). */
void aos_scroll_bind(AosScroll *s, int bg, uint8_t meta_0, uint8_t meta_1,
                     uint8_t mode, uint16_t unk_14, uint16_t unk_16,
                     int32_t x_pos, int32_t y_pos);

/* Binds (fetch != NULL) or clears (fetch == NULL) the tile-word
 * source consulted when a scroll request queues a rebuild DMA.
 * Room-scoped: aos_scroll_bind does not touch it. */
void aos_scroll_tile_source(AosScroll *s, AosScrollTileFetch fetch,
                            void *context);

/* sub_0803FBBC(bg, vx, vy): the scroll-velocity request. */
void aos_scroll_request(AosScroll *s, int bg, int32_t vx, int32_t vy,
                        AosScrollOut *out);

/* sub_0803FC6C(bg, vx, vy): the transition sibling — same gates
 * and x axis, but the y helpers carry the intro/credits edge
 * constants and pin y at 0. Callers: GameModeIntroCutsceneUpdate
 * stage 3 and GameModeCreditsUpdate stage 3 (not the door path). */
void aos_scroll_transition_request(AosScroll *s, int bg, int32_t vx,
                                   int32_t vy, AosScrollOut *out);

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
