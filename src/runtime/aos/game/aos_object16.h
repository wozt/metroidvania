#ifndef AOS_OBJECT16_H
#define AOS_OBJECT16_H
#include <stdbool.h>
#include <stdint.h>

/* Object16Create (0x08052F6C) / Object16Update (0x08052CE4),
 * dispatch index 22: a boss-intro cutscene director with a BG
 * zoom, ambient particles and a closing dialogue. Create loads
 * the 0x0825E084/0x0825E890/0x0825EA70 resources, runs
 * sub_0803CC70 on the + 0x2C byte against 0x08526414, presets
 * + 0x5C = 0x12, + 0x1C = 0xF0000 (BG scale), + 0x24 = 0x40
 * (scroll step) and + 0x14 = 0xC0 (zoom-out counter).
 *
 * Update states: 0 waits for the + 0x42C bit-0 input flag then
 * locks input and runs sub_0800C5A8(0); 1 forces the x-compare
 * direction (0x20/0x10) toward the + 0x13110 entity and enters 2
 * on the sub_08068AD4 probe; 2 locks input, runs
 * sub_080235C8/sub_0802DFF0, plays song 0x157 and enters 3;
 * 3 locks input, derives + 0x1C from sub_080009E4(+ 0x14 << 6)
 * (rounded toward -inf by + 0xF then >> 4, 0x1000 minus),
 * advances + 0x24 by 2 and + 0x20 by + 0x24, plays song 0x1AE
 * when + 0x14 hits 0x40 and fires the white fade pair when it
 * reaches 0x3F post-decrement, then state 4 at <= 0; 4 locks
 * input, clears + 0x42C bit 1, shows the 0x08522ABC record and
 * sets + 0x42C bit 0x8000. Every frame the tail runs
 * BgAffineSet on + 0x1C and spawns a type-0x49 particle
 * (sub_08052C20 callback) on + 0xD parity with random offsets. */
typedef struct {
    uint8_t param;          /* +0x2C: sub_0803CC70 record selector */
    uint8_t state;          /* +0xA */
    uint8_t tick;           /* +0xD: particle spawn parity */
    int32_t counter;        /* +0x14: 0xC0 down to <= 0 */
    uint32_t zoom;          /* +0x1C: BG affine scale */
    uint32_t scroll;        /* +0x24: += 2 per state-3 frame */
    uint32_t scroll_acc;    /* +0x20: += scroll */
    int forced;             /* -1 none, 0 lock, 1 direction word */
    int walk_dir;           /* 0x20/0x10 x-compare result */
    int song;               /* 0x157 / 0x1AE when fired, -1 none */
    bool fade;              /* white fade pair at counter == 0x40 */
    bool dialogue;          /* state-4 0x08522ABC record shown */
    int particles;          /* type-0x49 spawns this frame */
} AosObject16;

void aos_object16_create(AosObject16 *o, int param);

/* `input_flag0` is the + 0x42C bit-0 test, `near` the state-1
 * probe and `soma_left` the x compare against the + 0x13110
 * entity. `sine` is the injected sub_080009E4. */
void aos_object16_update(AosObject16 *o, bool input_flag0, bool near,
                         bool soma_left,
                         int32_t (*sine)(uint32_t phase));
#endif
