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
 *   applied). A BG1 unk_18 of 0x1A first calls sub_0800CDAC
 *   (1, 0x450, 0x4A0) — its hblank program is evaluated by
 *   aos_scroll_hblank when no BG2/BG3 emitter claims the buffer.
 *   Native callers: sub_0800ED24 (the gated transaction below),
 *   sub_0800ECA0's room-load tail (sub_0800EBE0 -> ECA0 -> F9EC ->
 *   D288, once per room entry while bgInfo[1]'s velocities are
 *   still 0 — reproduce it by calling aos_scroll_apply once right
 *   after binding the room's layers), and a few camera-mode
 *   updates in code_08036B40.c that the host does not run.
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
 * (the hblank/affine wave phase: sub_0800C8C8/sub_0800CA40
 * increment it once per in-game frame and the room transition
 * clears it; modes 20/21/27/30/36 feed it to the hBlankEffect
 * buffer and BgAffineSet — see the fx bits below). entities is the
 * sub_08000F60 list (native loops the 0xD0 entityData slots);
 * NULL/0 means no flagged entities are bound. */
typedef struct {
    uint32_t unk_33c3;
    bool unk_4cc_0;
    uint32_t unk_4c8;
    AosScrollEntityRef *entities;
    size_t entity_count;
    /* GetRoomFromMapPosition(GetEntityRoomXPositionInteger(player),
     * GetEntityRoomYPositionInteger(player)) — the 6-bit room index
     * sub_0800D288's mode-32 switch keys on (the port's room->number
     * for the room on screen). Pass -1 when unmodelled: it selects
     * the native default arm like any unlisted room index. Only
     * aos_scroll_affine reads it; callers that never evaluate the
     * affine program may leave it 0. */
    int room_index;
} AosScrollWorld;

/* Unported native side effects of sub_0800D288, reported as bits
 * so the caller can log them; the scroll state they accompany is
 * still applied where the native writes it. */
enum {
    AOS_SCROLL_FX_CDAC = 1u << 0,      /* sub_0800CDAC(1,0x450,0x4A0)
                                          armed (hblank program —
                                          consumed by
                                          aos_scroll_hblank) */
    AOS_SCROLL_FX_HBLANK = 1u << 1,    /* hBlankEffect wave buffers */
    AOS_SCROLL_FX_SCANLINE = 1u << 2,  /* sub_0800CB8C / sub_0800CED4 /
                                          sub_0800D0F8 scanline/window FX */
    AOS_SCROLL_FX_CC90 = 1u << 3,      /* sub_0800CC90 */
    AOS_SCROLL_FX_CB00 = 1u << 4,      /* sub_0800CB00 */
    AOS_SCROLL_FX_D000 = 1u << 5,      /* sub_0800D000 */
    AOS_SCROLL_FX_AFFINE = 1u << 6,    /* BgAffineSet on BG2 */
    AOS_SCROLL_FX_BG2_DISABLE = 1u << 7 /* dispCnt &= ~DCNT_BG2 (mode 19) */
};

/* The sub_0800D288 hBlankEffect program for one frame, evaluated
 * for the draw path. The native runs every emitting case into ONE
 * hBlankBuffer + ONE destReg per frame, so the LAST emitter in the
 * bgNum = 2,3 loop order wins outright — earlier programs are
 * overwritten before the hblank ISR ever sees them. `ofs[]` holds
 * the absolute register values the ISR writes per scanline
 * (BGxHOFS for vertical = false, BGxVOFS for true), already
 * including each routine's base-offset fold; target_bg is the BG
 * number (2/3) whose register the program writes — modes 14/20
 * always target BG3 (REG_BG3HOFS) exactly like the native, the
 * scanline modes target their own BG's hOfs/vOfs; target_bg 1 is
 * the sub_0800CDAC fallback (BG1 unk_18 == 0x1A emits before the
 * bgNum = 2,3 loop, so it wins only when no loop emitter exists). affine = true
 * marks mode 18's per-scanline BgAffineSet program (sub_0800D000,
 * writeSize 0x10 into REG_BG2PA): it is the active hblank program
 * but stays unported, so the caller keeps the static draw and no
 * ofs[] table is filled. frame_counter is gEwramData->frameCounter
 * — the global frame count the CB8C/CC90/31 phase terms read
 * (unlike unk_60.unk_4C8, which the room transition clears and
 * which arrives through `world`). target_bg 0 means no mode
 * emitted a program this frame. */
#define AOS_SCROLL_SCANLINES 160
typedef struct {
    int target_bg;      /* 0: no hblank program this frame */
    bool vertical;      /* ofs[] is the vOfs table, else hOfs */
    bool affine;        /* mode 18's per-scanline affine program */
    uint16_t ofs[AOS_SCROLL_SCANLINES];
} AosScrollHBlank;

/* Evaluates the winning hblank program of sub_0800D288 for the
 * draw path (see AosScrollHBlank). Returns nonzero when a program
 * is active. Pure function of the scroll state — call after
 * aos_scroll_apply so the bgOfs shadows the wave routines read as
 * their base are final. */
int aos_scroll_hblank(const AosScroll *s, const AosScrollWorld *world,
                      uint32_t frame_counter, AosScrollHBlank *out);

/* One BgAffineDstData record — the 16 bytes svcBgAffineSet writes
 * per calculation (BGxPA..PD then BGxPH/PG): the four s16 8.8
 * matrix parameters and the two s32 reference coordinates (28-bit
 * signed, 8 fraction bits — the hardware cuts the s32 store down
 * to 28 bits, which the port reproduces). */
typedef struct {
    int16_t pa, pb, pc, pd; /* BGxPA/PB/PC/PD: per-pixel x and
                               per-scanline gradients, 8.8 */
    int32_t ph, pg;         /* BGxPH/PG: texture coordinate of the
                               scanline's left edge, 8.8 */
} AosScrollAffineSample;

/* The sub_0800D288 affine programs for one frame, evaluated for
 * the draw path. Modes 21/27/32 run BgAffineSet on bg2PA once per
 * frame inside the bgNum = 2,3 loop (the last emitter wins, always
 * targeting BG2); mode 18 instead fills the hBlankEffect buffer
 * with one 16-byte BgAffineDstData record per scanline aimed at
 * REG_BG2PA (sub_0800D000, HBlankEffectSetup writeSize 0x10) — the
 * hblank DMA then rewrites BG2PA..BG2PG every scanline, so a mode
 * 18 on either layer wins over any per-frame affine write.
 * target_bg is always 2 like the native destinations; per_scanline
 * marks the mode-18 program (scan[] holds the 160 records, row y
 * uses record y — the hardware rewrites BG2PA..BG2PG every
 * scanline, so row y samples with record y's full matrix and
 * PH/PG). frame holds the per-frame write for modes 21/27/32
 * (texel at screen (x,y) = ((ph + pa*x + pb*y) >> 8,
 * (pg + pc*x + pd*y) >> 8)). has_frame marks that a 21/27/32
 * emitter produced frame this call — it stays valid even when a
 * mode-18 program also exists, because the per-frame BgAffineSet
 * registers are only overwritten per scanline when the mode-18
 * records actually win the hBlankEffect buffer (aos_scroll_hblank
 * reports that through hblank.affine). Returns nonzero when an
 * affine program is active. */
#define AOS_SCROLL_AFFINE_LINES 160
typedef struct {
    int target_bg;              /* 0: no affine program this frame */
    bool per_scanline;          /* mode 18's hblank program */
    bool has_frame;             /* frame holds a 21/27/32 write */
    AosScrollAffineSample frame;
    AosScrollAffineSample scan[AOS_SCROLL_AFFINE_LINES];
} AosScrollAffine;

/* Evaluates the winning affine program of sub_0800D288 (see
 * AosScrollAffine). Pure function of the scroll state and world —
 * call after aos_scroll_apply like aos_scroll_hblank. */
int aos_scroll_affine(const AosScroll *s, const AosScrollWorld *world,
                      AosScrollAffine *out);

/* Host draw-path helper, not a native routine: the whole-texel
 * coordinate one BgAffineSet sample produces at screen pixel
 * (x,y) — the hardware formula
 *   texel = ((ph + pa*x + pb*y) >> 8, (pg + pc*x + pd*y) >> 8)
 * with the native s32 arithmetic shifts. For mode 18's
 * per-scanline program pass the record for the pixel's row y
 * (scan[y]); for modes 21/27/32 pass `frame`. The caller applies
 * the BGxCNT area-overflow wrap (layer->wrap) or the
 * out-of-bounds transparent sample. */
void aos_scroll_affine_texel(const AosScrollAffineSample *sm, int x,
                             int y, int32_t *tx, int32_t *ty);

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
 * unk_16 and the pBgMetadata dimension bytes) are per-room data
 * the runtime resolves from the room's background records
 * (sub_0800F9EC's unk_8 table) when a ROM is bound; without one
 * they stay 0 (mode 0, no propagation). */
void aos_scroll_bind(AosScroll *s, int bg, uint8_t meta_0, uint8_t meta_1,
                     uint8_t mode, uint16_t unk_14, uint16_t unk_16,
                     int32_t x_pos, int32_t y_pos);

/* sub_0800ED5C / sub_0800EE54: the per-axis initial scroll
 * position (integer pixels — sub_0803F8A8 stores the u16 result
 * in the position's high half) sub_0800F9EC computes for each
 * layer record at room load. `arg` is the incoming scroll
 * coordinate (unk_60.unk_334/unk_336 — the previous room's
 * bgInfo[1] integer position, y minus 0x30); `meta` is the
 * layer's pBgMetadata->unk_0/unk_1 screen count, `range` its
 * unk_14/unk_16 scroll range and `ref_range` bgInfo[1]'s
 * matching range (the interpolation denominator base). Modes
 * absent from the native switch keep `arg`; a zero
 * interpolation denominator yields 0 like ARM SDIV. The y twin
 * adds the native +0x30 bias (and mode 10's 0xFFD0 pre-bias
 * wrap to 0). */
uint16_t aos_scroll_origin_x(uint8_t mode, uint8_t meta_0,
                             uint16_t range, uint16_t ref_range,
                             uint16_t arg);
uint16_t aos_scroll_origin_y(uint8_t mode, uint8_t meta_1,
                             uint16_t range, uint16_t ref_range,
                             uint16_t arg);

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

/* One source/destination rectangle of a wrapped text-BG blit: the
 * layer bitmap is sampled at (src_x, src_y) for w*h pixels and
 * drawn at (dst_x, dst_y) inside the view. Host draw-path helper,
 * not a native routine: it reproduces the wrap math the runtime's
 * composite background draw already used for BG1 so per-layer
 * draws of BG2/BG3 can share it. */
typedef struct {
    int src_x, src_y;   /* source pixel origin in the layer bitmap */
    int dst_x, dst_y;   /* destination offset inside the view */
    int w, h;
} AosScrollBlit;

/* Wraps the integer bgOfs-style offset (h_ofs, v_ofs) against the
 * layer bitmap size (map_w x map_h pixels) and the view size
 * (view_w x view_h pixels), returning up to 4 blit rectangles:
 * a window crossing the map's right/bottom edge continues at
 * source 0 like the native tilemap wrap. map_w/map_h <= 0 disables
 * wrapping (one full-view blit sampled at the raw offset). The
 * returned count is the number of entries written to `out` (cap 4). */
size_t aos_scroll_bgofs_blits(int32_t h_ofs, int32_t v_ofs,
                              int map_w, int map_h,
                              int view_w, int view_h,
                              AosScrollBlit out[4]);

/* sub_0803DC18 / sub_0803DD14's priority outputs
 * (gUnk_030019D8.unk_A / unk_E). The room load calls
 * sub_0803DD14 once per BG with the background record's
 * field_0 layer byte (BG0's slot gets 0 via sub_0803DD14(0,0)),
 * and sub_0803FD60 ORs unk_A[bg] into BGxCNT bits 0-1 — the
 * hardware BG priority, 0 front .. 3 back. sub_0803DE10 stamps
 * unk_E[band] into each OAM record's attribute-2 priority bits,
 * where `band` is the entity record +0x5C draw band that
 * sub_0803E528 buckets OAM records by (it also emits the
 * records band 0 first, so band order is the OBJ tie-break). */
typedef struct {
    uint8_t bg_priority[4];     /* unk_A[0..3]: BG0..BG3 */
    uint8_t band_priority[32];  /* unk_E[0..0x1F]: +0x5C band */
} AosDepthPriorities;

/* The sub_0803DD14 recompute over the final four layer bytes
 * (BG0..BG3 order): bg_priority[] gets unk_A[], band_priority[]
 * gets unk_E[]. Pure function of `layer` — no state. */
void aos_scroll_depth_priorities(const uint8_t layer[4],
                                 AosDepthPriorities *out);

/* The room record's blend_control (EwramData_unk88.unk_1E — the
 * descriptor +0x1E halfword sub_0800ECA0 copies into
 * gDisplayRegisters.bldCnt on every room entry) decoded for the
 * draw path. sub_0800C778 — inlined into sub_0800ECA0 right after
 * that store and into sub_0800C5D8's new-game init — fixes
 * BLDALPHA at EVA 11 / EVB 10 on every room load, and nothing in
 * normal gameplay rewrites it, so an alpha-effect room blends its
 * first-target BG layer(s) at those coefficients. Returns the
 * BLDCNT-style first-target mask restricted to the BG slots
 * (bits 0-3 = BG0..BG3; OBJ/backdrop first targets are not
 * consumed by the BG draw) when bits 6-7 select alpha blending,
 * else 0. The native also requires the pixel below the first
 * target to be a second-target layer for the blend to fire —
 * the draw path reproduces that through its priority order
 * (layers in front repaint over the blended result), with the
 * backdrop-only edge documented at the consumer. */
uint8_t aos_scroll_blend_first_targets(uint16_t blend_control);

/* The entity record +0x5C (unk_540) create-time draw band of an
 * enemy kind, decoded from the native Enemy*Create bodies
 * (sub_0806B04C's unconditional 0x12 default plus the per-create
 * overrides that land after it). Returns 0xFF for an id outside
 * the native 0x71 enemy records — treat it like any out-of-range
 * band (front pass). Three kinds keep a live +0x5C the draw path
 * reads instead (Kyoma Demon, Altair, Mudman); state-dependent
 * +0x5C rewrites inside enemy updates are not yet tracked, so
 * this is the create-time band. */
uint8_t aos_scroll_enemy_band5c(uint8_t enemy_id);

#endif /* AOS_SCROLL_H */
