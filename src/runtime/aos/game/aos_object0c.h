#ifndef AOS_OBJECT0C_H
#define AOS_OBJECT0C_H
#include <stdbool.h>
#include <stdint.h>

/* Object0C (sUnk_084F0DF8[12] / sUnk_084F0ED8[12]): the room-region
 * trigger. Create claims the shared gEwramData + 0x1316C slot through
 * sub_08022088 (the same slot Object01 uses) with descriptor
 * 0x08118520 and sets + 0x59 bit 2; the claim result is ignored.
 * Update walks the 0x085261FC[param0] record list — 0x10-byte records
 * {u16 type, u16 pad, u32 param, s16 x, s16 y, u16 w, u16 h} terminated
 * by type 3 — and calls the 0x08526208[type] callback whenever the
 * player's room position (BG1 scroll integer + entity room coordinate,
 * truncated to u16) sits inside the record rectangle. When + 0x59 bit 2
 * is clear the update sets bit 3 and releases the slot through
 * sub_080220B4; nothing in the port clears bit 2, matching the native
 * paths that leave the claim held.
 *
 * Callbacks:
 *  - type 0 (sub_08050694): when Soma + 0x10 bit 30 is set,
 *    entity + 0x48 = record param (read back by sub_080506EC).
 *  - type 1 (sub_080506C0): when the bit is set, Soma + 0x10 |=
 *    0x4000000 (AOS_FLAG_HEAVY — the region applies heavy physics).
 *  - type 2 (sub_08050510): gated by gEwramData + 0x13260 bit 0x200 OR
 *    the same Soma bit; sets Soma + 0x10 |= 0x4000000, forces entity
 *    + 0x1E to 1 when nonzero, arms the + 0x131D4/+ 0x131D6 camera
 *    snapshot at Soma + 0x40/+ 0x44 + 0x80000, then writes the pan
 *    target + 0x131E0/+ 0x131E4 through sub_08021248 (only while the
 *    snapshot mode reads 3, which the callback itself just set) and
 *    entity + 0x48 = -+0x30000 by record param == 0. */
typedef struct {
    uint8_t param0;         /* +0x30 s16: region list index 0-2 */
    uint8_t flags59;        /* +0x59: bit 2 claim-held mark, bit 3 released */
    bool claimed;           /* sub_08022088 took the + 0x1316C slot */
    uint32_t value48;       /* +0x48: type-0 record param / type-2 dir */
    /* Type-2 outputs, the gEwramData + 0x131D4.. snapshot block. */
    bool snapshot_armed;    /* +0x131D6 = 1 and + 0x131D4 = 3 */
    int32_t snapshot_x;     /* +0x131D8 = Soma + 0x40 + 0x80000 */
    int32_t snapshot_y;     /* +0x131DC = Soma + 0x44 */
    int32_t pan_x, pan_y;   /* +0x131E0/+0x131E4 through sub_08021248 */
    bool soma_1e_forced;    /* entity + 0x1E was nonzero, forced to 1 */
    bool heavy_set;         /* a type-1 record set AOS_FLAG_HEAVY */
} AosObject0c;

/* Object0CCreate: attempts the + 0x1316C claim (result ignored like
 * the native code) and marks + 0x59 bit 2. */
void aos_object0c_create(AosObject0c *o, int param0);

/* Object0CUpdate. `soma_x`/`soma_y` are the player's room pixel
 * position truncated to u16 like GetEntityRoom*PositionInteger;
 * `soma_flag30` is entity + 0x10 bit 30 (the callback gate);
 * `global_13260_200` is gEwramData + 0x13260 bit 0x200; `soma_fx`/
 * `soma_fy` are Soma + 0x40/+ 0x44 fixed-point for the type-2
 * snapshot; `soma_1e` is the entity + 0x1E byte. */
void aos_object0c_update(AosObject0c *o, uint16_t soma_x, uint16_t soma_y,
                         bool soma_flag30, bool global_13260_200,
                         int32_t soma_fx, int32_t soma_fy, uint8_t soma_1e);
#endif
