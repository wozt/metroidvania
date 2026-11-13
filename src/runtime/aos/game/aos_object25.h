#ifndef AOS_OBJECT25_H
#define AOS_OBJECT25_H
#include <stdbool.h>
#include <stdint.h>

#include "aos_pickup.h"

#include "../core/aos_collision.h"

/* Object25Create (0x08057AA0) / Object25Update (0x080579A4),
 * dispatch index 37 (0x25): the Arena soul carrier — a floating
 * mover that sheds counted child copies and dissolves at a
 * param-selected world bound.
 *
 * Create first gates on gEwramData + 0x4CC bits 0-1 and the
 * + 0x37E event word bit 0x20: with (flags4cc & 3) == 0 and the
 * event bit set the entity deletes. param1 (+ 0x32) == 1 takes
 * the invisible spawner path — + 0xA = 1, + 0x58 bit 0x20,
 * + 0xD = 0x78, no sprite load, no health. Every other variant
 * runs sub_08057DB8 (tiles 0x081CC07C, palette 0x0820AC4C bank
 * 0, frames 0x08225A08, drawFunc sub_0803B9D0 — exported as
 * special_37), clears + 0x58 bits 0 and 6, sets + 0x1A bits
 * 1-2, + 0x3E bit 0, + 0x3C = 0x28, applies animation table
 * 0x08526550, steps sub_0803F17C, seeds + 0x34 = 0xC8 health,
 * + 0x5C = 0x14 and + 0xA = 0, deleting on load failure.
 * param1 == 2 additionally enters substate 1 immediately with
 * animation table 0x08526564, + 0x48 = 0x4000, + 0x4C = 0x80000
 * and + 0x5C = 0x13.
 *
 * Update dispatches on + 0xA: state 1 is the spawner — the u8
 * + 0xD counts up and each time the OLD value exceeds 0x77 it
 * resets to 0 and a RandomNumberGenerator() & 1 gate (spawn on
 * even) plus a + 0x18 <= 9 live-child cap decides one
 * EntityCreateInRange(0x1B, 0x48, EntityUpdateNothing) child.
 * The child's world position is a (random % count)-th entry of
 * the 0x085265F8 / 0x08526608 table pair indexed by + 0x30
 * (six/ four/ two/ two 16.16 positions per variant, minus the
 * A094 room origin), it inherits + 0x30 and the parent's + 0x36
 * byte, gets + 0x59 bit 1, + 0x37 = + 0x32 = 0 (the gate's zero
 * result, not a fresh roll), the parent's draw callback and
 * vtable, runs Object25Create and links + 0x1C back to the
 * parent — which the child's finish decrements through + 0x18.
 * State 0 runs sub_08057BAC, the + 0xB machine: substate 0 waits
 * for the + 0x59 bit-0 engine-active mark, then loads table
 * 0x08526564 with + 0x48 = 0x4000 and + 0x4C = 0x80000 and
 * decrements + 0x5C. Substate 1 re-arms + 0x48 = 0x4000 each
 * frame, integrates + 0x40 += + 0x48 once and clears it (a
 * 0.25 px/frame right drift), then — param0 != 2 always,
 * param0 == 2 only while pos_x + A094[4] <= 0xD00000 — sets
 * + 0x4C = 0x80000 and runs the sub_0806D288 -> sub_080698A8
 * physics (16.16 y integration with tile collision,
 * approximated by the floor probe). sub_0806AE54 ticks the + 0x2D
 * invulnerability blink into the + 0x5A flash bits. The bound
 * check compares pos_x + A094[4] against 0x040FFFFF for
 * param0 0-1 and 0x00FFFFFF for param0 2-3 (no check outside
 * 0-3); crossing enters substate 10 with + 0xD = 1. Substate
 * 10 emits one sub_0806D5C0 particle burst every fourth frame
 * at (pos_x, pos_y - 0x60000) with ranges 0x100000 / 0xC0000
 * and type + 0x5C - 1, derives + 0x58 bit 5 from the
 * sub_0806D518(+ 0xD) phase table, counts + 0xD down and on the
 * 0xFF wrap sets + 0x59 bit 3 and decrements the parent's
 * + 0x18 when linked. Every frame ends with sub_0803F17C and,
 * in substate 1, the sub_080421AC attack-slot read.
 *
 * sub_08057D38 is the hurt callback: sub_08021530 on the attack
 * record feeds sub_0806B7D8 for the damage, sub_08042B8C spawns
 * the damage-number entity, + 0x2D = 8 marks the invulnerability
 * window, + 0x34 loses the damage and at <= 0 the + 0x5A flash
 * bits, + 0xD = 0x28, + 0xB = 10 and animation table
 * 0x08526580 (mode 2) run the dissolve. The runtime exposes it
 * as aos_object25_hurt with the weapon-overlap gate outside. */
typedef struct {
    int32_t pos_x, pos_y;   /* +0x40/+0x44: 16.16 world position */
    int32_t vel_x, vel_y;   /* +0x48/+0x4C: 16.16 velocities */
    uint8_t state;          /* +0xA: 0 mover machine, 1 spawner */
    uint8_t substate;       /* +0xB: 0 wait, 1 move, 10 dissolve */
    uint8_t timer;          /* +0xD: spawn gate / dissolve count */
    int16_t param0;         /* +0x30 s16: table index + bound variant */
    int16_t param1;         /* +0x32 s16: create variant */
    uint8_t flags58;        /* +0x58: bit 5 phase mark, bit 0x20 */
    uint8_t flags59;        /* +0x59: bit 0 active in, bit 3 done */
    uint8_t blink;          /* +0x2D: hurt invulnerability counter */
    uint8_t flash;          /* +0x5A: hit-flash bits */
    int16_t hp;             /* +0x34: 0xC8 on the sprite path */
    uint8_t kind;           /* +0x5C - 1: particle type base */
    uint8_t field36;        /* +0x36: damage-table byte, parent copy */
    uint16_t children;      /* +0x18: live child count, cap 10 */
    bool linked;            /* +0x1C: spawned by a parent Object25 */
    bool deleted;           /* EntityDelete ran (create gate) */
    /* Per-frame outputs, reset by update. */
    uint8_t anim_table;     /* current sub_0803F0E4 table 0/1/2 */
    int anim_seq;           /* table applied this frame, -1 none */
    int particle_fx;        /* sub_0806D5C0 bursts this frame */
    int damage;             /* sub_08042B8C value, -1 none */
    bool spawn_pending;     /* one state-1 child this frame */
    int32_t spawn_x, spawn_y;   /* child +0x40/+0x44, 16.16 */
    uint8_t spawn_param0;   /* child +0x30: the parent's */
    bool finished;          /* +0x59 bit 3: the native done mark */
} AosObject25;

/* Object25Create. `pos_x` / `pos_y` are the record position in
 * 16.16 (e->x / e->y << 16), `flags4cc` the gEwramData + 0x4CC
 * byte (bits 0-1) and `flags37e` the + 0x37E event word. The
 * sprite load is represented by the special_37 asset; `deleted`
 * reports the native gate/load-failure paths. */
void aos_object25_create(AosObject25 *o, int16_t param0,
                         int16_t param1, int32_t pos_x, int32_t pos_y,
                         uint8_t flags4cc, uint16_t flags37e,
                         bool *deleted);

/* Object25Update while + 0xA dispatches. `active` is the + 0x59
 * bit-0 engine-active approximation, `layer` the room collision
 * for the sub_080698A8 floor probe (NULL skips it), `room_ox` /
 * `room_oy` the A094 + 4 / + 8 16.16 room origin (0 in the
 * runtime) and `random` the engine RNG. */
void aos_object25_update(AosObject25 *o, bool active,
                         const AosCollision *layer,
                         int32_t room_ox, int32_t room_oy,
                         AosRandom random);

/* sub_08057D38: one hurt application. `damage` is the
 * sub_0806B7D8 result (the runtime passes the attack power);
 * ignored while the + 0x2D invulnerability window is armed or
 * the entity has no health (the param1 == 1 spawner path). */
void aos_object25_hurt(AosObject25 *o, int damage);
#endif
