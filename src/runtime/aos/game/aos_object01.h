/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_OBJECT01_H
#define AOS_OBJECT01_H
#include <stdbool.h>
#include <stdint.h>

/* Aria of Sorrow special object 1 (sUnk_084F0DF8[1] /
 * sUnk_084F0ED8[1], Object01Create 0x08033254 through the 0x08033C84
 * thunk / Object01Update 0x08033C90): a conditional multi-part static
 * decoration. The create claims the gEwramData + 0x1316C slot through
 * sub_08022088 (first come, one per room in the port), then spawns
 * sUnk_080E1DA8.unk_4[param0] child sprites — type 0x49 entities with
 * drawFunc sub_0803B9D0, tiles 0x081CBE14, palette 0x08209C64 and
 * frames 0x0820F18C — at the record's u16 coordinate pairs minus the
 * BG1 origin (screen pixels; the port keeps them as room pixels, the
 * same approximation aos_door uses). param0 > 1 or a busy slot leaves
 * unk_53D_2 clear, so the first update runs sub_08033C18: the children
 * are marked and sub_080220B4 releases the slot unconditionally — even
 * a failed object's update releases a claim it never held, a native
 * quirk the port keeps. */

typedef struct {
    bool claimed;           /* sub_08022088 returned 1 and children exist */
    bool deleted;           /* unk_53D_3: the failed path ran */
    uint8_t child_count;
    int16_t child_x[3], child_y[3]; /* room pixels (the table coordinates) */
    int16_t param0;         /* + 0x514 record field, clamped to 0..1 */
} AosObject01;

/* Room transition: the port scopes the + 0x1316C claim to the room
 * (the native slot outlives the entities' delete marks). */
void aos_object01_reset(void);

/* Object01Create. param0 outside 0..1 or a held slot claims nothing;
 * the first update then deletes the entity. */
void aos_object01_create(AosObject01 *o, int16_t param0);

/* Object01Update (sub_08033C18 on the unclaimed path only). */
void aos_object01_update(AosObject01 *o);
#endif
