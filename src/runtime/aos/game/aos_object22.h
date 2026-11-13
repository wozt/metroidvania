#ifndef AOS_OBJECT22_H
#define AOS_OBJECT22_H
#include <stdbool.h>
#include <stdint.h>

/* Object22Create (0x08056828) / Object22Update (0x08056F14) /
 * sub_080568F4, dispatch index 34 (0x22): the scripted
 * elevator/sequence director.
 *
 * Create runs the sub_08057034 sprite attach — tiles
 * 0x081CBE8C through sub_0803B800, palette
 * sub_08068264(0x08209FD8, 2), frames 0x082103F4,
 * drawFunc sub_0803B9D0 — deletes on load failure, installs
 * the sub_0804277C(8) hurt slot and the 0x0C20FFF0 collision
 * box through sub_080428B4/sub_08042848, sets + 0x72 |= 6,
 * sub_0803F2C8(3, 0, 1), derives the + 0x3C camera byte from
 * gEwramData + 0x131FE/+ 0x1327E, + 0x5C = 0x12, + 0xA = 0,
 * + 0x42 = 0x40 - A094[6], + 0x46 = 0x9D - A094[0xA] and
 * + 0x54 = 0x2800.
 *
 * Update calls sub_08021278(1) while + 0x23 bit 0 is set,
 * runs sub_080568F4 while + 0xA == 0, then sub_0803F17C and
 * the sub_080421AC slot read.
 *
 * The + 0xB machine (sub_080568F4) visits up to six stops
 * indexed by the s8 + 0x20, each stop pairing a bound from
 * the 0x0852649C u16 table {64,192,352,512,672,832} with a
 * 3-byte record from the 0x085264BC pointer table into the
 * 0x085264A8 stream ([count, dir/mask bytes...] where dir
 * codes 0x10/0x20/0x40/0x80 pick the sub_08056F50 projectile
 * velocity pair). A linked entity at gEwramData + 0x13190
 * gates the movement: while it is not this entity the + 0x46
 * anchor uses 0x9D - A094[0xA] and the + 0x1C timer runs to
 * 0x3B before falling into the finale; while it is this
 * entity the anchor uses 0x9E - A094[0xA] and + 0x1C resets.
 *  - state 0: anchor + 0xD count to 0x3B -> state 1 with
 *    + 0x20 = 1, + 0x48 = 0x40000.
 *  - state 1: sub_0806D3C0 x integration; linked!=self runs
 *    the + 0x1C timeout into the finale (state 4, + 0xD =
 *    0x14); linked==self re-anchors and clamps + 0x42 to the
 *    stop bound -> state 2 (+ 0xC = + 0xD = 0).
 *  - state 2: substate + 0xC 0 delays 0x1E frames, 1 fires
 *    one sub_08056F50 volley per record dir byte at the
 *    bound-derived position with the 0x08118628 lifetime
 *    byte and PlaySong 0x15B, then + 0xC = 2. Substate 2
 *    counts + 0xD against lifetime + 0x1E; entity flag bits
 *    (+ 0x1E word) matching the record mask byte set + 0x22
 *    (1 = volley hit -> PlaySong 0x15A and immediate next
 *    volley, duration byte otherwise) or the 0xF0 mask
 *    interrupts; when + 0xD reaches lifetime + 0x1E the
 *    volley index + 0x21 advances while + 0x22 == 1, else
 *    the sequence drops to the finale state 4.
 *  - state 3: advance + 0x20; past 5 -> state 10, else back
 *    to state 1 with + 0x48 = 0x40000.
 *  - state 4: first frame spawns the sub_08056D6C carrier
 *    child (type 0x49, parent-linked through + 0x18, update
 *    sub_08056E74) and PlaySong 0x15C; + 0xD counts 0x28
 *    down -> state 5 with + 0x23 bit 0.
 *  - state 5: sub_0806A224 heavy physics (approximated), the
 *    sub_0806D518 phase table drives + 0x58 bit 5 and + 0xD
 *    expiry sets + 0x59 bit 3.
 *  - state 10: sets gEwramData + 0x348 bit 15 -> state 11.
 *  - states 6-9 and 11 are empty.
 *
 * Approximations: the sub_0806A224 state-5 physics collapses
 * to the shared gravity model, the sub_08056F50 projectiles
 * and the sub_08056D6C carrier child become per-frame spawn
 * outputs (their entities, parent link and hurt callback are
 * not modelled), the + 0x13190 linked-entity check is an
 * input flag, sub_08021278 becomes a done output and there
 * is no emulator comparison. */
typedef struct {
    uint8_t state;          /* +0xB: 0 arm, 1 move, 2 volley,
                             * 3 advance, 4 finale-arm,
                             * 5 finale, 10 end, 11 done */
    uint8_t substate;       /* +0xC: state-2 phase 0/1/2,
                             * state-4 spawn latch */
    uint8_t timer;          /* +0xD: per-state countdown */
    int8_t stop;            /* +0x20: current stop 1-6 */
    int8_t volley;          /* +0x21: volley index in record */
    uint8_t volley_flag;    /* +0x22: 1 hit-continue, else
                             * the record lifetime byte */
    uint8_t flags23;        /* +0x23: bit 0 finale-done mark */
    uint8_t aux_1c;         /* +0x1C: linked-mismatch timer */
    int32_t vel_x;          /* +0x48 */
    int32_t vel_y;          /* +0x4C */
    int32_t aux_50;         /* +0x50 */
    int32_t accel_y;        /* +0x54 */
    int32_t pos_y;          /* +0x44: 16.16 */
    int32_t pos_x;          /* +0x40: 16.16 */
    int16_t bound_px;       /* +0x42: stop bound, screen px */
    int16_t anchor_py;      /* +0x46: 0x9D/0x9E - room y */
    int32_t room_x6;        /* A094 + 6: bound probe offset */
    int32_t room_yA;        /* A094 + 0xA: anchor offset */
    int32_t room_ox;        /* A094 + 4: shot x origin */
    int32_t room_oy;        /* A094 + 8: shot y origin */
    uint8_t byte58;         /* +0x58: phase-table bit 5 */
    uint8_t flags59;        /* +0x59: bit 3 finale done */
    uint8_t cam_byte;       /* +0x3C */
    bool deleted;           /* sprite load failure */
    /* Per-frame outputs, reset by update. */
    int song;               /* PlaySong id, -1 none */
    bool deactivate;        /* sub_08021278 request */
    bool set_global_348;    /* state-10 gEwramData + 0x348 bit */
    bool spawn_projectile;  /* sub_08056F50 volley fired */
    int32_t shot_x, shot_y; /* 16.16 launch position */
    uint8_t shot_life;      /* record lifetime byte */
    int shot_dir;           /* record dir code 0x10-0x80 */
    bool spawn_carrier;     /* sub_08056D6C child fired */
} AosObject22;

/* ROM data dumped from the (USA) image:
 *   0x0852649C u16 stop bounds {64,192,352,512,672,832}
 *   0x085264BC ptr[5] -> 0x085264A8 records
 *   0x085264A8 stream: 02 20 10 | 02 40 80 | 03 10 80 40 |
 *     03 40 80 04 | 40 10 80 20 — [count, dir/mask bytes]
 *   0x08118628 u8 lifetimes {60,54,46,38,30} */
#define AOS_OBJ22_STOP_COUNT 6

/* Object22Create. `cam_byte` is the + 0x3C approximation,
 * `room_x`/`room_y` the A094 + 6 / + 0xA fields,
 * `room_ox`/`room_oy` the A094 + 4 / + 8 shot origins and
 * `sprite_ok` the sub_08057034 load result. */
void aos_object22_create(AosObject22 *o, int cam_byte,
                         uint16_t room_x, uint16_t room_y,
                         int32_t room_ox, int32_t room_oy,
                         bool sprite_ok, bool *deleted);

/* Object22Update while + 0xA == 0.
 * `linked_self` is the gEwramData + 0x13190 == entity check,
 * `hit_flags` the entity + 0x1E flag word the volley masks
 * test and `rng` is unused (kept for symmetry). */
void aos_object22_update(AosObject22 *o, bool linked_self,
                         uint16_t hit_flags);

#endif
