/* Aria of Sorrow special object 7 (sUnk_084F0DF8[7] /
 * sUnk_084F0ED8[7], Object07Create 0x0804F6E4 / Object07Update
 * 0x0804F770): the ambient-mote object — the last special object
 * used by exported rooms (2 occurrences). The parent never calls the
 * shared init and has no sprite of its own; create dispatches on
 * param0 (+ 0x30 s16):
 *  - 0: sub_0804F374 spawns table_085255AC[param1] ambient children
 *    (type 0x49, drawFunc sub_0804F160) AND sub_0804F5C4 spawns the
 *    four anchored children (drawFunc sub_0804F504);
 *  - 1: + 0x10 = 0 (the BG-anim cycler index);
 *  - 2: sub_0804F374 only;
 *  - 3: gEwramData + 0x4C4 = 0, + 0x42C |= 0x800, sub_0803C674;
 *  - 4: param1 != 0 -> sub_08013EEC.
 * Update: param0 == 1 runs sub_0804F690, the background-animation
 * frame cycler over the 8-byte records at 0x08525D9C (the tile DMA
 * itself is not modelled — only the frame index is tracked); every
 * other param0 writes gDisplayRegisters + 0x48 = 0xE40 (a window
 * register the renderer owns).
 *
 * Ambient children (sub_0804F374 spawn, sub_0804F160 update): roll
 * rng & 0xF picks the parallax layer +0x1C {3,0,1,2} and palette
 * hint +0x5C {0x1D,0x19,0x1A,0x1D}; non-layer-3 children get screen
 * anchors (+0x42 = (rng & 0x3F) << 2 - 0x20, +0x46 = (rng & 0x7F) +
 * 1) and velocities +0x50/+0x54 = ((rng & mask) + bias) >> layer with
 * +0x50 negated when +0x58 bit 0x40 holds; layer-3 children anchor
 * relative to the camera ((rng & 0x3F) + 0x8C - cam_x / 4,
 * (rng & 0x3F) + 0x21) with +0x18 = (rng & 0x1F) << 0xC. Update
 * state 1 integrates the fixed-point position (+0x40/+0x44 +=
 * +0x48/+0x4C derived from +0x50/+0x54 through sub_080009E4),
 * compensates the BG scroll by the parallax factor
 * 0x0852559C[layer] * scroll >> 8, then wraps the screen anchor into
 * the visible window re-randomizing on each edge.
 *
 * Anchored children (sub_0804F5C4 spawn, sub_0804F504 update): four
 * children at the 0x085255DC record offsets minus the camera-scaled
 * parallax ((cam * byte) >> 6 per axis) plus the 0x085255C4[param0]
 * u16 pair, +0x50 = 0x80; state 0 attaches the shared mote art,
 * state 1 integrates position and subtracts scroll / 0xC. */
#ifndef AOS_OBJECT07_H
#define AOS_OBJECT07_H
#include <stdbool.h>
#include <stdint.h>

#include "../core/aos_collision.h"
#include "../core/aos_soma.h"

#define AOS_OBJECT07_MAX_AMBIENT 16
#define AOS_OBJECT07_ANCHORED 4

typedef struct {
    uint8_t layer;          /* +0x1C: parallax layer 0-3 (index into 0x0852559C) */
    uint8_t state;          /* +0xA: 0 init, 1 active */
    uint8_t variant;        /* +0x5C palette hint / +0x18 layer-3 timer seed */
    bool alive;
    int32_t fx, fy;         /* +0x40/+0x44 fixed-point position (16.16) */
    int32_t vx, vy;         /* +0x48/+0x4C */
    int32_t speed_x, speed_y; /* +0x50/+0x54 */
    int32_t sx, sy;         /* +0x42/+0x46 screen-pixel anchor */
    uint32_t timer;         /* +0x14: (rng & 0x1F) << 0xC phase */
    uint8_t kind3;          /* +0x1C == 3: camera-relative anchor mode */
} AosObject07Mote;

typedef struct {
    uint8_t state;          /* +0xA: 0 init, 1 drift */
    uint8_t aux;            /* +0x18: record byte 2 */
    bool alive;
    int32_t fx, fy;         /* +0x40/+0x44 fixed-point position (16.16) */
    int32_t vx, vy;         /* +0x48/+0x4C */
} AosObject07Anchor;

typedef struct {
    uint8_t param0;         /* +0x30 s16: create dispatch 0-4 */
    uint8_t param1;         /* +0x32 s16: ambient count table index / case-4 gate */
    uint8_t frame_index;    /* +0x10: BG-anim record index (param0 == 1) */
    uint16_t sub_index;     /* +0x12: frame index inside the record */
    uint8_t win48;          /* gDisplayRegisters + 0x48 = 0xE40 request */
    bool clear_4c4;         /* param0 == 3: gEwramData + 0x4C4 = 0 */
    bool set_42c_800;       /* param0 == 3: + 0x42C |= 0x800 */
    bool call_13eec;        /* param0 == 4 && param1 != 0: sub_08013EEC */
    int ambient_count;      /* spawned motes */
    AosObject07Mote motes[AOS_OBJECT07_MAX_AMBIENT];
    int anchor_count;
    AosObject07Anchor anchors[AOS_OBJECT07_ANCHORED];
} AosObject07;

/* Object07Create; `cam_x`/`cam_y` the BG1 scroll registers feed the
 * layer-3 and anchored-child anchors. `rng` is the native
 * RandomNumberGenerator. */
void aos_object07_create(AosObject07 *o, int param0, int param1,
                         int32_t cam_x, int32_t cam_y,
                         uint32_t (*rng)(void));

/* Object07Update; `cam_x`/`cam_y` the BG1 scroll, `scroll_x`/`scroll_y`
 * the + 0xA094 + 0xC/+ 0x10 fixed-point (16.16) scroll the parallax
 * and the anchored drift use. Anchor/mote draw positions are
 * fx >> 16 / fy >> 16 screen pixels. */
void aos_object07_update(AosObject07 *o, int32_t cam_x, int32_t cam_y,
                         int32_t scroll_x, int32_t scroll_y,
                         uint32_t (*rng)(void));
#endif
