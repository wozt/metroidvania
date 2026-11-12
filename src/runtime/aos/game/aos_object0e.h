#ifndef AOS_OBJECT0E_H
#define AOS_OBJECT0E_H
#include <stdbool.h>
#include <stdint.h>

/* Object0E (sUnk_084F0DF8[14] / sUnk_084F0ED8[14]): the multi-variant
 * breakable prop (pots/barrels). Create indexes the 0x085262BC[param0]
 * 12-byte config record — byte 0 picks the 0x08526214 resource triple
 * (tiles/palette/frames, exported as special_14_v00-v11), byte 2 the
 * animation, bytes 3-7 the debris/content parameters and +8 the
 * 0x428F0 attack-box preset — loads through sub_0806E0D0, sets
 * + 0x5C = 0x14, installs the sub_08050BA8 frame callback and + 0x59
 * bit 1, deleting on load failure.
 *
 * Update runs the sub_08068AD4 probe (-0x20, -0x30, 0x120, 0x110):
 * a miss sets + 0x58 bit 0x20 and ends the update. State 0 clears
 * + 0x58 bits 0 and 5, then sub_08045F9C with the 0x08526370[param1]
 * byte pair decides the hit: on success sub_08050A3C releases the
 * config's content children (type 0x49 entities sharing the prop
 * art — approximated here by `content_fx`), PlaySong 0xA2, + 0x58
 * bit 0x20, + 0xD = 0x28 and state 1. State 1 emits the sub_0806D5C0
 * gore burst every fourth frame while + 0xD exceeds 0x1E, counts the
 * u8 timer down and sets + 0x59 bit 3 on the 0xFF wrap.
 * sub_08050D98 is the hurt callback storing the x-compare direction
 * in + 0x48. */
typedef struct {
    uint8_t param0;         /* +0x30 s16: config record index */
    uint8_t param1;         /* +0x32 s16: hit-parameter table index */
    uint8_t variant;        /* cfg byte 0: resource table index */
    uint8_t state;          /* +0xA: 0 idle, 1 breaking */
    uint8_t timer;          /* +0xD: 0x28 on the hit, u8 wrap ends it */
    uint8_t flags58;        /* +0x58: bit 0x20 offscreen mark */
    bool finished;          /* +0x59 bit 3: the native done mark */
    int32_t fx, fy;         /* +0x40/+0x44 fixed-point position */
    /* Outputs. */
    int song;               /* PlaySong request, -1 none */
    int content_fx;         /* sub_08050A3C children approximated as fx */
    int gore_fx;            /* sub_0806D5C0 burst approximated as fx */
} AosObject0e;

/* Object0ECreate; `deleted` reports the native load-failure path. */
void aos_object0e_create(AosObject0e *o, int param0, int param1,
                         bool *deleted);

/* Object0EUpdate. `near` is the sub_08068AD4 probe result, `hit` the
 * sub_08045F9C match and `fx`/`fy` the entity fixed-point position. */
void aos_object0e_update(AosObject0e *o, bool near, bool hit,
                         int32_t fx, int32_t fy);
#endif
