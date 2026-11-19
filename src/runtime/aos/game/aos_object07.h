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
 * 1) and velocities +0x50/+0x54 = ((rng & mask) + bias) >> layer;
 * layer-3 children anchor relative to the camera ((rng & 0x3F) +
 * 0x8C - cam_x / 4, (rng & 0x3F) + 0x21) with +0x18 = (rng & 0x1F)
 * << 0xC. Every child also gets +0x14 = (rng & 0x1F) << 0xC. The
 * native +0x58 bit-0x40 negate of +0x50 is dead code — the entity
 * is zeroed by EntityCreateInRange and nothing sets +0x58 before
 * the check. Update state 1 advances +0x14 += 0x800 (+0x18 +=
 * 0xA00 for layer 3) and derives +0x48/+0x4C through the shared
 * sub_080009E4 sine: non-layer-3 keeps +0x48 = +0x50 and computes
 * +0x4C = trunc16(sine(+0x14) * +0x54), layer 3 runs both axes at
 * >> 0xC (x on the +0x14 phase + 0x4000, y on +0x18). It then
 * integrates +0x40/+0x44 += +0x48/+0x4C, compensates the BG scroll
 * by the parallax factor 0x0852559C[layer] * scroll >> 8, and wraps
 * the screen anchor (the s16 integer halves +0x42/+0x46 of
 * +0x40/+0x44): x > 0x110 or x < -0x20 reseeds +0x46 and teleports
 * +0x42 to the opposite edge, and only when x is in range does a
 * y > 0xC0 / y < -0x20 wrap reseed +0x42 = rng & 0xBF.
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
    uint8_t variant;        /* +0x5C native OAM draw band */
    bool alive;
    int32_t fx, fy;         /* +0x40/+0x44 fixed-point position (16.16) */
    int32_t vx, vy;         /* +0x48/+0x4C */
    int32_t speed_x, speed_y; /* +0x50/+0x54 */
    int32_t sx, sy;         /* +0x42/+0x46 screen-pixel anchor */
    uint32_t timer;         /* +0x14: (rng & 0x1F) << 0xC phase, += 0x800/frame */
    uint32_t phase2;        /* +0x18: layer-3 only, (rng & 0x1F) << 0xC, += 0xA00/frame */
    uint8_t kind3;          /* +0x1C == 3: camera-relative anchor mode */
    uint8_t anim_frame;     /* +0x6E: current sub_0803F17C frame */
    uint8_t anim_tick;      /* +0x6F: ticks spent on that frame */
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
    uint8_t frame_index;    /* +0x10: frame position inside the
                               param1-selected BG-anim record */
    uint16_t sub_index;     /* +0x12: tick count inside that frame */
    uint8_t pal_bank;       /* param0 == 1: palette RAM row the record's
                               arg field patches (sub_0803C918 dest) */
    uint32_t pal_serial;    /* param0 == 1: bumped on every record-frame
                               advance; the draw side re-applies the
                               palette chunk only when it changes */
    uint8_t win48;          /* gDisplayRegisters + 0x48 = 0xE40 request */
    bool clear_4c4;         /* param0 == 3: gEwramData + 0x4C4 = 0 */
    bool set_42c_800;       /* param0 == 3: + 0x42C |= 0x800 */
    bool call_13eec;        /* param0 == 4 && param1 != 0: sub_08013EEC */
    int32_t last_cam_x;     /* previous BG1 integer scroll, for +0xA0A0 */
    int32_t last_cam_y;     /* previous BG1 integer scroll, for +0xA0A4 */
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

/* Object07Update; `cam_x`/`cam_y` are the current BG1 integer scroll.
 * The object retains the previous pair and derives the host-equivalent
 * 16.16 velocities from their per-frame delta; native children read
 * those velocities at gEwramData + 0xA0A0/+0xA0A4. */
void aos_object07_update(AosObject07 *o, int32_t cam_x, int32_t cam_y,
                         uint32_t (*rng)(void));

/* The palette RAM row a param0 == 1 record patches, for room-bind
 * scans that run before Object07Create (0x08525D9C[param1].arg). */
uint8_t aos_object07_anim_bank(int param1);
#endif
