#ifndef AOS_OBJECT18_H
#define AOS_OBJECT18_H
#include <stdbool.h>
#include <stdint.h>

#include "aos_pickup.h"

/* Object18Create (0x080538DC) / Object18Update (0x08053968),
 * dispatch index 24 (0x18): the breakable scatter director.
 * Create loads the 0x0825EAF4 / 0x082663E4 / 0x08266C00 triple
 * (exported as special_24), sets + 0x5C = 0x14, runs
 * sub_0803F2C8(0, 0, 1) and sub_0803F17C, installs the
 * sub_08053B80 hurt callback through sub_0804277C, sets
 * + 0x72 bit 1 and the 0x1820E8F0 attack preset through
 * sub_080428F0, calls sub_0806AF98 and marks + 0x59 bit 1,
 * deleting on load failure.
 *
 * Update opens with the sub_080421AC slot read (approximated
 * by the weapon overlap): its low16 bits 0x1E gate state 0,
 * and a miss on the sub_08068AD4 self probe
 * (-0x20, -0x20, 0x130, 0xE0 — the entity's own screen
 * position inside the rect) only sets + 0x58 bit 0x20.
 * State 0 clears + 0x58 bits 0 and 5, then on the attack
 * gate: param0 (+ 0x30 s16) nonzero takes the armed path —
 * state 2, + 0xD = 0x20, the sub_0805379C orbit ring, song
 * 0xA2 and + 0x58 bit 0x20. Otherwise the
 * gEwramData + 0x13266 flag picks the scatter: clear spawns
 * up to eight sub_08043F70 money pickups of variants i / 2
 * jittered by sub_0802D5EC(0x20) on x and RandomNumber &
 * 0xF below y - 8 (stopping at the first failed create),
 * set drops one sub_08043C10 big heart; both then run the
 * sub_0805379C ring (four type-0x49 children on the parent
 * art), PlaySong 0xA2, + 0x58 bit 0x20, + 0xD = 0x28 and
 * state 1. State 1 emits one sub_0806D5C0 gore burst every
 * fourth frame while + 0xD exceeds 0x1E. State 2 emits one
 * type-0x1B child through EntityCreateInRange(0x1B, 0x48,
 * EntityUpdateNothing, 0x080EA0AC = {sub_080C24AD,
 * sub_080C2559, 0x76, 0x000A0050}) every fourth frame.
 * Both states share the u8 + 0xD countdown; the 0xFF wrap
 * sets + 0x59 bit 3. */
typedef struct {
    int16_t drop_x, drop_y; /* room pixel position */
    uint8_t variant;        /* money kind i / 2, 0-3 */
} AosObject18Drop;

typedef struct {
    uint8_t state;          /* +0xA: 0 idle, 1 gore, 2 child spawn */
    uint8_t timer;          /* +0xD: 0x28 / 0x20 on trigger */
    uint8_t flags58;        /* +0x58: bit 0x20 probe mark, 0x40 facing */
    bool finished;          /* +0x59 bit 3: the native done mark */
    bool armed;             /* +0x30 s16 param0 != 0: state-2 path */
    /* Per-frame outputs, reset by update. */
    int song;               /* PlaySong id, -1 none */
    int ring_fx;            /* sub_0805379C orbit children, up to 4 */
    int gore_fx;            /* sub_0806D5C0 bursts this frame */
    int child_fx;           /* type-0x1B children this frame */
    AosObject18Drop drops[8];
    int drop_count;         /* money pickups to spawn */
    bool heart;             /* sub_08043C10 big heart instead */
} AosObject18;

/* Object18Create. `param0` is the record's + 0x30 s16. */
void aos_object18_create(AosObject18 *o, int param0);

/* Object18Update. `near` is the sub_08068AD4 self probe,
 * `attack` the sub_080421AC low16 & 0x1E gate, `event_flag`
 * the gEwramData + 0x13266 byte (set drops the money scatter
 * for one big heart), `room_x` / `room_y` the entity's
 * integer room pixel position (the + 0x42 / + 0x46 screen
 * coordinates plus the camera cancel out in the scatter
 * math) and `random` the engine RNG. */
void aos_object18_update(AosObject18 *o, bool near, bool attack,
                         bool event_flag, int32_t room_x,
                         int32_t room_y, AosRandom random);
#endif
