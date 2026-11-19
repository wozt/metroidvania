#ifndef AOS_BREAKABLE_H
#define AOS_BREAKABLE_H
#include <stdbool.h>
#include <stdint.h>

/* sub_08046080 stores this byte at entity +0x5C before selecting
 * animation 3. The room renderer maps it through sub_0803DD14's
 * band table instead of flattening the prop into the front pass. */
#define AOS_BREAKABLE_DRAW_BAND 0x14

/* sub_0800F1FC case 3: the generic breakable room record. Every
 * kind-3 record creates the same prop through sub_08046080 —
 * sub_08046148(param0 = entity + 0x30) DMAs the variant tiles out of
 * the 0x081C3E00 table into OBJ VRAM (only when the + 0x92 cached
 * param0 changes — the last spawned variant wins natively; the port
 * exports one sprite set per param0 instead), then the shared tile
 * sheet 0x081C15F4 / frames 0x0820ED60 load with animation 3, OBJ
 * palette row 7 and the 0x140CF4FA attack-box preset; + 0x5C =
 * AOS_BREAKABLE_DRAW_BAND
 * and the drawFunc becomes sub_08046100 (animation step plus the
 * + 0x46 six-pixel bounce while gEwramData word 0 bit 1 holds — the
 * runtime's whip-swing flag, approximated by the caller's `swing`).
 *
 * Update sub_08045EE0 runs the sub_08068AD4 probe (-0x10, -0x10,
 * 0x110, 0xC0): off-window sets + 0x58 bit 0x20 and ends the update.
 * State 0 clears + 0x58 bits 0 and 5, then sub_08045F9C (the same
 * attack-box/content dispatcher Object0E uses) decides the hit: on
 * success PlaySong 0xA2, + 0x58 bit 0x20, + 0xD = 0x28 and state 1.
 * State 1 emits the sub_0806D5C0 gore burst every fourth frame while
 * + 0xD exceeds 0x1E, counts the u8 timer down and sets + 0x59 bit 3
 * on the 0xFF wrap.
 *
 * sub_08045F9C's drop dispatch: the selector is the entity + 0x36
 * byte, which no room record or ported create writes — it reads 0,
 * so only case 0 runs: currentMP == maxMP && currentCharacter == 0
 * drops a sub_08043F70 money pickup of variant (RandomNumberGenerator
 * () & 1) + param1 (+ 0x32); otherwise a sub_08043C10 MP heart with
 * big = (param1 != 0) and pop = 0. Selector cases 1 (money of
 * variant param1) and 2 (sub_08044054 item) are unreachable while
 * + 0x36 stays unmodelled; the caller creates the reported pickup
 * through the existing AosPickup paths. */
typedef enum {
    AOS_BREAKABLE_DROP_NONE = 0,
    AOS_BREAKABLE_DROP_HEART,  /* sub_08043C10(x, y, big, 0) */
    AOS_BREAKABLE_DROP_MONEY,  /* sub_08043F70(x, y, (RNG&1)+param1) */
} AosBreakableDrop;

typedef struct {
    uint8_t param0;         /* +0x30 s16: 0x081C3E00 tile-variant index */
    uint8_t param1;         /* +0x32 s16: sub_08045F9C hit/drop parameter */
    uint8_t state;          /* +0xA: 0 idle, 1 breaking */
    uint8_t timer;          /* +0xD: 0x28 on the hit, u8 wrap ends it */
    uint8_t flags58;        /* +0x58: bit 0x20 offscreen mark */
    uint32_t anim_tick;     /* sub_08046100's per-frame step count:
                             * the native drawFunc advances + 0x6F
                             * once per drawn frame against the
                             * animation record's duration bytes,
                             * so the port keeps its own counter
                             * instead of the wall-clock step/4
                             * approximation. */
    bool finished;          /* +0x59 bit 3: the native done mark */
    bool swing_bounce;      /* sub_08046100's + 0x46 bounce, per frame */
    /* Outputs. */
    int song;               /* PlaySong request, -1 none */
    int drop_kind;          /* AosBreakableDrop latched on the hit */
    bool drop_big;          /* sub_08043C10's big-heart flag */
    bool drop_roll;         /* money variant needs (RNG & 1) + param1 */
    int gore_fx;            /* sub_0806D5C0 burst approximated as fx */
} AosBreakable;

/* sub_08046080 create; nothing can fail (the native load-failure
 * path of sub_0806E0D0 does not apply to the DMA setup). */
void aos_breakable_create(AosBreakable *b, int param0, int param1);

/* sub_08045EE0. `near` is the sub_08068AD4 probe result and `hit`
 * the sub_08045F9C match, both approximated by the caller like
 * Object0E's. `swing` mirrors the gEwramData word-0 bit-1 bounce
 * gate of sub_08046100. `mp_full_soma` is sub_08045F9C case 0's
 * money gate: currentMP == maxMP && currentCharacter == 0. */
void aos_breakable_update(AosBreakable *b, bool near, bool hit, bool swing,
                          bool mp_full_soma);
#endif
