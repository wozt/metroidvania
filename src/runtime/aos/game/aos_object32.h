#ifndef AOS_OBJECT32_H
#define AOS_OBJECT32_H
#include <stdbool.h>
#include <stdint.h>

/* Object32Create (0x08059494) / Object32Update (0x080597DC) /
 * sub_08059580, dispatch index 50 (0x32): the stationary
 * cinematic sequence director.
 *
 * Create memcpy's the two 4-byte collision boxes at
 * 0x08118684, runs the sub_08059864 sprite attach — tiles
 * 0x081CBF0C through sub_0803B800, palette
 * sub_08068264(0x0820A350, 1), frames 0x08211F08,
 * drawFunc sub_0803B9D0 — deletes on load failure, installs
 * the sub_0804277C(8) hurt slot and the param1-indexed box
 * through sub_080428B4/sub_08042848, sets + 0x72 |= 6,
 * + 0x65 = (param1 == 0 ? 0 : 7) — the child anim selector —
 * + 0x5C = 0x12, + 0xA = 0 and the shared origin fields
 * + 0x18 = + 0x44 + A094[8], + 0x1C = param0 << 16,
 * + 0x10/+ 0x14 = pos + A094[4]/[8], + 0x24/+ 0x28 screen
 * mirrors, + 0x59 &= ~3 and + 0x20 = (+ 0x18 > + 0x1C).
 *
 * Update runs sub_08059580 while + 0xA == 0, then checks the
 * player entity (gEwramData + 0x13110) within 0x1FFFFF in
 * both axes and calls sub_08021278(1) on proximity, then
 * the sub_080421AC slot read.
 *
 * The + 0xB machine is a five-step cinematic:
 *  - state 0 waits for the + 0x13190 linked entity to be
 *    self, then -> 1 with PlaySong 0x153.
 *  - state 1 counts + 0xD past 9 -> 2, + 0x65++, no song.
 *  - state 2 counts past 0x13 -> 3, + 0x65++, PlaySong 0xA8.
 *  - state 3 counts past 9 -> 4, + 0x65++, PlaySong 0x150.
 *  - state 4 counts past 5, sets + 0x59 bit 3 and fires five
 *    sub_080596E0 type-0x49 children — slots (5,1), (6,2),
 *    (4,3) always plus (4,0) and (5,4) when param1 == 0 —
 *    then stays.
 * The tail rewrites + 0x40/+ 0x44 from + 0x10/+ 0x14 minus
 * the A094 origin every frame; the origin fields never
 * integrate, so the object is stationary.
 *
 * Approximations: the sub_080596E0 children become counted
 * spawn outputs (their per-slot offsets, velocities, RNG
 * bits and update sub_08059840 are not modelled), the
 * + 0x13190/+ 0x13110 entity pointers are input flags, the
 * collision boxes and hurt slot stay under special_sprites
 * and there is no emulator comparison. */
typedef struct {
    uint8_t state;          /* +0xB: 0 linked-wait, 1-4 steps */
    uint8_t timer;          /* +0xD: step countdown */
    uint8_t anim_sel;       /* +0x65: 0/7 init, ++ per step */
    uint8_t flags59;        /* +0x59: bit 3 finale done */
    int32_t pos_x;          /* +0x10: world origin x */
    int32_t pos_y;          /* +0x14: world origin y */
    int32_t world_x;        /* +0x40: screen-mirror write */
    int32_t world_y;        /* +0x44 */
    int16_t param0;         /* +0x30: unused threshold src */
    int16_t param1;         /* +0x32: box selector, gates the
                             * two extra children at == 0 */
    bool deleted;           /* sprite load failure */
    /* Per-frame outputs, reset by update. */
    int song;               /* PlaySong id, -1 none */
    bool deactivate;        /* sub_08021278 proximity request */
    unsigned child_count;   /* sub_080596E0 children fired */
} AosObject32;

/* Object32Create. `param0`/`param1` are the + 0x30/+ 0x32
 * selectors, `room_x`/`room_y` the A094 + 4 / + 8 origin,
 * `ent_x`/`ent_y` the entity + 0x40/+ 0x44 and `sprite_ok`
 * the sub_08059864 load result. */
void aos_object32_create(AosObject32 *o, int16_t param0,
                         int16_t param1, int32_t room_x,
                         int32_t room_y, int32_t ent_x,
                         int32_t ent_y, bool sprite_ok,
                         bool *deleted);

/* Object32Update while + 0xA == 0. `linked_self` is the
 * + 0x13190 == entity check and `player_near` the + 0x13110
 * player within 0x1FFFFF on both axes. */
void aos_object32_update(AosObject32 *o, bool linked_self,
                         bool player_near);

#endif
