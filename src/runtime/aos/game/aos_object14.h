#ifndef AOS_OBJECT14_H
#define AOS_OBJECT14_H
#include <stdbool.h>
#include <stdint.h>

/* Object14Create (0x08052870) / Object14Update (0x080528C4),
 * dispatch index 20 (0x14): the param0-selected moving
 * actor/probe object.
 *
 * Create runs the sub_0806E0D0 sprite attach — tiles
 * 0x081CBEA4, palette 0x0820A0DC, frames 0x08210B60, bank 0 —
 * deletes on load failure, then sets + 0x58 = (+ 0x58 & ~0x41)
 * | 0x20 and + 0x59 |= 2. Update dispatches through the
 * 0x08526408 function table indexed by the + 0x30 parameter:
 * [0] sub_08051E60, [1] sub_08052340, [2] sub_0805262C (other
 * values read past the table natively; the port no-ops them).
 *
 * Variant 0 (sub_08051E60) is gated every frame on the
 * gEwramData + 0x4BE s16: nonzero registers the entity through
 * sub_08021278(0) once + 0xA passes 1 and skips the machine and
 * the player-distance tail entirely. The + 0xA machine:
 *  - state 0 runs sub_0803E654(1) (palette allocator; -1 ->
 *    EntityDelete), stores slot & 0x1F into + 0x58, + 0x5C =
 *    0xC, + 0x59 |= 0x10, installs the sub_080527E4 child
 *    callback at + 4, snapshots the room position into
 *    + 0x18/+ 0x1A and + 0x1C/+ 0x1E, sets + 0x48 = 0x8000,
 *    the sub_0804277C(8) hurt slot, the 0x103000E8 preset and
 *    + 0x72 |= 6, then falls into state 1.
 *  - state 1 zeroes + 0x65, adds 0x400 to + 0x24; past 0xFFFF
 *    it sets + 0x65 = 2, + 0x24 = 0x10000 and -> 2.
 *  - state 2 runs sub_08051BF8(ent, 2) — position integration,
 *    the + 0x21278(7) register, two sub_08001E58 tile probes
 *    and the ArcTan2/sub_080686CC + 0x20 steer — and advances
 *    to 3 with + 0x54 = 0x2000 only when the probe chain
 *    returns 0.
 *  - state 3 runs sub_08051BF8(ent, 1); a nonzero return with
 *    + 0x4C above 0x40000 -> 4, otherwise bounces to 2 with
 *    + 0x4C/+ 0x54 cleared.
 *  - state 4 probes the (-0x10, 0, 0x110, 0xB0) screen box;
 *    a hit fires sub_08051CC4 (six type-0x49 children) and
 *    PlaySong 0xAC, then resets + 0x24/+ 0x20/+ 0x4C/+ 0x54
 *    and re-anchors + 0x42/+ 0x46 to home - A094 -> 1.
 *  - every ungated frame the shared tail measures the
 *    sub_0806E3F8 player distance (clamped to 0xC0000, sign
 *    flipped when the player is left), scales it by the
 *    + 0x20 sine pair with the 0xA/0xC range factor into the
 *    sub_080428F0 preset and runs sub_0803E058.
 *
 * Variant 1 (sub_08052340) is the same skeleton with the
 * sub_08022088 room-slot claim, the 0x102EFEE8 preset, the
 * sub_08052208 probe/steer (mode 0, + 8 room-y bias and a
 * two-step vertical rescan), the + 0x14 collision-bit0 gate
 * driving the sub_08013D60(0x136)/sub_08013E18 song pair, the
 * + 0x48 velocity ramp through states 2-3 and the
 * sub_080020D8 point test that ends in sub_08051CC4 + PlaySong
 * 0xAC + sub_0803E6BC(+ 0x58 & 7) + EntityDelete.
 *
 * Variant 2 (sub_0805262C) probes the (-0x80, -0x40, 0x1F0,
 * 0x120) screen box every frame: a hit registers through
 * sub_08021278(3) and, when the + 0x13190 link is self, writes
 * the + 0x40 low half into the player's + 0x40 through
 * sub_0802138C. The + 0xA machine rolls two
 * RandomNumberGenerator values at state 0 (+ 0x24 = 0x8000 on
 * odd, + 0x26 = (roll & 0x3F) + 0x40), integrates + 0x24 +=
 * + 0x26 and + 0x40 += sub_080009E4(+ 0x24) / 3 in state 1,
 * then in state 2 aims a lead point (A094 + screen +
 * u16(+ 0x48 >> 16), +/- 0x18 by + 0x48 sign) through the
 * sub_080020A0/sub_08001F3C wall pair — a confirmed wall halves
 * and flips + 0x48 — before the sub_0806D490(0x50)/
 * sub_0806D3D8(0x800000) drag and the |+ 0x48| <= 0x7FF
 * restart into state 1.
 *
 * sub_080527E4 (the + 4 child callback) re-anchors + 0x42/+ 0x46
 * from + 0x1C/+ 0x1E minus A094, runs the sub_0803B9D0 draw
 * call and refreshes + 0x1C/+ 0x1E from the room position;
 * sub_08052834 is the type-0x49 child's own physics callback
 * and stays unported with the children.
 *
 * Approximations: GetEntityRoomX/YPositionInteger is modelled
 * as + 0x40/+ 0x44 >> 16 whole room pixels (the A094 screen
 * offset stays a separate input), the sub_08001E58 tile
 * distance probe, the sub_080020D8 point test and the
 * sub_080020A0/sub_08001F3C wall pair are injected through the
 * probe callback, the + 0x14 collision bits arrive through
 * env->collide14, sub_0803E654 is an injected palette slot,
 * EntityCreateInRange children collapse to a count of six,
 * sub_0803E058 becomes counted affine calls with the raw
 * arguments exposed, and there is no emulator comparison. */
typedef struct {
    uint8_t state;          /* +0xA: per-variant machine state */
    uint8_t cnt_c;          /* +0xC: variant 1 frame counter */
    uint8_t timer_d;        /* +0xD: state-2 release countdown */
    uint16_t flags14;       /* +0x14: collision flags, cleared
                             * by the variant 1 tail each frame */
    int16_t home_x;         /* +0x18: home room pos x */
    int16_t home_y;         /* +0x1A */
    int16_t room_x;         /* +0x1C: live room pos (child cb) */
    int16_t room_y;         /* +0x1E */
    int16_t angle;          /* +0x20: steered heading, sine src */
    int32_t f24;            /* +0x24: v0/v1 spin, v2 u16 angle */
    int16_t f26;            /* +0x26: v2 angular speed */
    int32_t pos_x;          /* +0x40: 16.16 fine pos */
    int32_t pos_y;          /* +0x44 */
    int16_t scr_x;          /* +0x42: screen pos */
    int16_t scr_y;          /* +0x46 */
    uint16_t probe_off;     /* +0x4E: v1 probe y offset src */
    int32_t vel_x;          /* +0x48 */
    int32_t vel_y;          /* +0x4C */
    int32_t acc_x;          /* +0x50: sub_0806D3D8 x accel */
    int32_t acc_y;          /* +0x54 */
    uint8_t flags58;        /* +0x58: palette/variant bits */
    uint8_t flags59;        /* +0x59 */
    uint8_t aux_5c;         /* +0x5C: 0xC on state-0 init */
    uint8_t sel_65;         /* +0x65: anim selector */
    uint8_t flags72;        /* +0x72 */
    int16_t param0;         /* +0x30: variant selector 0-2 */
    bool deleted;           /* EntityDelete ran */
    /* Per-frame outputs, reset by update. */
    int song;               /* PlaySong / sub_08013D60 id, -1 none */
    int song_stop;          /* sub_08013E18 id, -1 none */
    unsigned register_calls;/* sub_08021278 count */
    uint8_t register_arg;   /* last sub_08021278 channel */
    unsigned hurt_calls;    /* sub_0804277C count */
    unsigned claim_calls;   /* sub_08022088 count */
    unsigned release_calls; /* sub_080220B4 count */
    unsigned cb_installs;   /* + 4 callback installs (sub_080527E4) */
    unsigned affine_calls;  /* sub_0803E058 count */
    int32_t affine_a;       /* last sub_0803E058 r0 (angle) */
    int32_t affine_b;       /* last sub_0803E058 r1/r2 (scale) */
    uint32_t preset_arg;    /* last sub_080428F0 arg */
    unsigned child_count;   /* sub_08051CC4 children fired */
    unsigned palette_frees; /* sub_0803E6BC count */
    uint8_t palette_free_arg;/* last sub_0803E6BC nibble */
    unsigned draw_calls;    /* sub_0803B9D0 count (child cb) */
    bool player_snap;       /* sub_0802138C fired this frame */
    int16_t player_snap_x;  /* value written to player + 0x40 */
} AosObject14;

/* Probe kinds for the injected engine queries. TILE returns the
 * sub_08001E58 s16 hit distance (0 = clear) with `c` = mode;
 * POINT/WALL_A/WALL_B return nonzero on hit. */
enum {
    AOS_OBJ14_PROBE_TILE = 0,
    AOS_OBJ14_PROBE_POINT = 1,
    AOS_OBJ14_PROBE_WALL_A = 2,
    AOS_OBJ14_PROBE_WALL_B = 3
};
typedef int (*AosObject14Probe)(void *ctx, int kind,
                                int32_t a, int32_t b, int32_t c);

typedef struct {
    int16_t gate_4be;       /* gEwramData + 0x4BE s16 (v0 gate) */
    uint16_t collide14;     /* external + 0x14 writers (v1 bit0) */
    bool linked_self;       /* + 0x13190 == entity (v2) */
    int32_t player_x;       /* player + 0x40 fine pos */
    int32_t player_y;       /* player + 0x44 fine pos */
    uint16_t room_ox;       /* A094 + 6 screen origin x */
    uint16_t room_oy;       /* A094 + 0xA screen origin y */
    uint32_t rng_a;         /* first RandomNumberGenerator roll */
    uint32_t rng_b;         /* second roll */
    int palette_slot;       /* sub_0803E654(1): -1 = allocator full */
    AosObject14Probe probe; /* engine tile/wall queries */
    void *probe_ctx;
} AosObject14Env;

/* Object14Create. `param0` is the + 0x30 variant selector,
 * `pos_x`/`pos_y` the entity + 0x40/+ 0x44 fine positions and
 * `sprite_ok` the sub_0806E0D0 load result. */
void aos_object14_create(AosObject14 *o, int16_t param0,
                         int32_t pos_x, int32_t pos_y,
                         bool sprite_ok, bool *deleted);

/* Object14Update dispatch. Returns false when the entity
 * deleted itself this frame. */
bool aos_object14_update(AosObject14 *o, const AosObject14Env *env);

/* sub_080527E4, the + 4 child callback: re-anchors + 0x42/+ 0x46
 * from + 0x1C/+ 0x1E minus the A094 origin, counts the
 * sub_0803B9D0 draw call and refreshes + 0x1C/+ 0x1E. */
void aos_object14_child_update(AosObject14 *o,
                               uint16_t room_ox, uint16_t room_oy);

#endif
