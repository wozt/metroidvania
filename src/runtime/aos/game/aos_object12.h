#ifndef AOS_OBJECT12_H
#define AOS_OBJECT12_H
#include <stdbool.h>
#include <stdint.h>

/* Object12Create (0x0805140C) / Object12Update (0x0805158C), the
 * dispatch pair sUnk_084F0DF8[18] / sUnk_084F0ED8[18]: the
 * param-driven animated sprite. param0 indexes the four 24-byte
 * records at 0x085263A8 (gfx@0, palette@4, frames@8, bank@0xC,
 * anim@0xD, + 0x5C value@0xF, flags@0x10, update callback@0x14 —
 * the exported special_18_v0-v3 variants). Records 2 and 3 share
 * the same art and differ only in the flag bits.
 *
 * Update gates on the record's bit-3 probe flag (a sub_08068AD4
 * miss skips everything, animation included), then runs the record
 * callback when present:
 *  - record 0, sub_0805133C: a five-state machine playing the
 *    scripted sequences 0x0811853C / 0x08118544 / 0x08118558
 *    through sub_0803F0E4 with random timers and re-arms on the
 *    entity + 0x6C bit-2 hit flag.
 *  - records 2/3, sub_080514D8 -> sub_0805153C: a two-state drift
 *    that banks the camera scroll delta into + 0x40/+ 0x48 and a
 *    palette-cycle stepper driven by the + 0x5A bits.
 * Records without a callback and without flag bit 2 only run the
 * shared sub_0803F17C animation step. */
typedef struct {
    uint8_t variant;        /* param0 clamped to the four records */
    uint8_t state;          /* callback state (+ 0xA) */
    uint8_t timer;          /* callback + 0xD countdown */
    uint8_t seq;            /* last sub_0803F0E4 sequence, 0xFF none */
    uint8_t flags;          /* record + 0x10 flags */
    uint8_t palette_step;   /* sub_0805153C cycle approximation */
    bool animated;          /* !(flags & 4): the sub_0803F17C step runs */
} AosObject12;

/* Object12Create; `deleted` reports the native load failure. */
void aos_object12_create(AosObject12 *o, int param0, int param1,
                         bool *deleted);

/* Object12Update. `near` is the bit-3 sub_08068AD4 probe result,
 * `hit` the + 0x6C bit-2 flag and `param1_is_one` the + 0x32 == 1
 * gate of the records-2/3 callback. `rng` feeds the native
 * RandomNumberGenerator timers. */
void aos_object12_update(AosObject12 *o, bool near, bool hit,
                         bool param1_is_one, uint32_t (*rng)(void));
#endif
