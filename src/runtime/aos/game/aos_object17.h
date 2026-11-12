#ifndef AOS_OBJECT17_H
#define AOS_OBJECT17_H
#include <stdbool.h>
#include <stdint.h>

/* Object17Create (0x0805305C) / Object17Update (0x080532E8),
 * dispatch index 23 (0x17): the vertical pan director. Create
 * picks the + 0xB floor band (0..2) from the dumped 0x08118590
 * s16 table {0x80, 0x180, 0x280} — the first entry whose
 * value + 0x20 is not below the focus entity's integer room Y —
 * places the entity at + 0x42 = 0x78 - A094[6] and
 * + 0x46 = table[+ 0xB] - A094[0xA], registers the
 * gEwramData + 0x1316C singleton slot through sub_08022088
 * (callback table 0x08118598), sets + 0x59 bit 2 and spawns
 * nineteen type-0x49 children counted here: one sub_0805366C
 * anchor, two sub_08052FDC twins and a 2 x 8 sub_0805360C grid
 * over the 0x081185B0 x / 0x081185B4 y offset tables.
 *
 * Update is a four-state machine on + 0xA behind the + 0x59
 * bit-2 gate (clear: set bit 3, clear the singleton slot through
 * sub_080220B4 and skip the frame). State 0 runs the
 * sub_08021304 gate, the sub_08068AD4 probe
 * (focus.x - 0x20, focus.y - 0x20, 0x40, 0x10) and the
 * gEwramData + 0x1C flags: the + 0x18 dwell needs the probe,
 * a clear gate and flags & 0xC0 for more than two frames, then
 * flags & 0x80 pans down (variant++, + 0xC = 0, + 0xD = 0x20)
 * and flags & 0x40 pans up (variant--, + 0xC = 1, + 0xD = 0x40),
 * both entering state 1 with SetPlayerInput(1, 0). State 1
 * locks input, counts + 0xD down then plays song 0x116
 * (sub_08013D60) into state 2. State 2 locks input, advances
 * + 0x44 (16.16 pan position) by + 0x4C, ramps + 0x4C by
 * +/- 0x1000 capped at 0x2FFFF / -0x30000 on the OLD value and
 * compares (pos44 + speed + A094[8]) >> 16 against
 * table[+ 0xB]; on arrival it stops 0x116, plays song 0x117,
 * fires sub_08011FC0(0, 0x10, 0, 7), rewrites
 * + 0x44 = (table[+ 0xB] - A094[0xA]) << 16, clears + 0x4C and
 * enters state 3 with + 0xD = 0x40. State 3 counts + 0xD down
 * without locking input, then returns to state 0. Every frame
 * ends with sub_0803F2C8(ent, 0, 0, 1) and sub_0803F17C. */
typedef struct {
    uint8_t variant;        /* +0xB: floor band 0..2 */
    uint8_t dir;            /* +0xC: state-2 ramp direction */
    uint8_t state;          /* +0xA: 0 wait, 1 hold, 2 pan, 3 rest */
    uint8_t dwell;          /* +0x18: probe dwell counter */
    uint8_t timer;          /* +0xD: state 1/3 countdown */
    int32_t speed;          /* +0x4C: pan velocity */
    int32_t pos;            /* +0x44: 16.16 pan position */
    uint16_t pos_x;         /* +0x42 */
    uint16_t pos_y;         /* +0x46 */
    uint8_t flags59;        /* +0x59 bits 1-3 */
    int child_count;        /* nineteen type-0x49 children */
    bool deleted;           /* EntityDelete ran (sprite load fail) */
    /* Per-frame outputs. */
    bool locked;            /* SetPlayerInput(1, 0) this frame */
    int song;               /* sub_08013D60/PlaySong id, -1 none */
    int song_stop;          /* sub_08013E18 id, -1 none */
    bool fade;              /* sub_08011FC0(0, 0x10, 0, 7) fired */
    bool slot_clear;        /* sub_080220B4 cleared + 0x1316C */
} AosObject17;

/* Object17Create. `player_y` is the focus entity's integer room
 * Y (GetEntityRoomYPositionInteger) and `room_ox`/`room_oy` the
 * A094 + 6 / + 0xA u16 fields. */
void aos_object17_create(AosObject17 *o, int player_y,
                         uint16_t room_ox, uint16_t room_oy);

/* Object17Update. `gate_blocked` is sub_08021304() != 0,
 * `near` the sub_08068AD4 probe result, `flags` the
 * gEwramData + 0x1C u16 (bits 0xC0 gate the dwell, 0x80/0x40
 * pick the pan direction). */
void aos_object17_update(AosObject17 *o, bool gate_blocked,
                         bool near, uint16_t flags,
                         int32_t room_y16, uint16_t room_oy);
#endif
