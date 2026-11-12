#ifndef AOS_OBJECT0D_H
#define AOS_OBJECT0D_H
#include <stdbool.h>
#include <stdint.h>

/* Object0D (sUnk_084F0DF8[13] / sUnk_084F0ED8[13]): the swinging
 * object — a rotating prop (blade/pendulum) drawn through the
 * sub_0803E058 affine path. Create loads tiles 0x081CBE74, palette
 * 0x08209F54 and frames 0x0821017C through sub_0806E0D0 (the shared
 * enemy-style loader), picks a palette variant through
 * sub_0803E654, sets + 0x5C = 0x16, + 0x59 bit 1, + 0x72 bit 1 and
 * the sub_080428F0 preset 0x122006F0; a failed load deletes the
 * entity. The art is exported as special_13.
 *
 * Update: + 0x46 above 0xA8 sets + 0x58 bit 0x20 (the low-screen
 * mark), otherwise bits 0 and 5 clear. sub_08021278 registers the
 * entity into the gEwramData + 0x131B4 slot list every update —
 * reported as `registered`; no runtime consumer reads it yet.
 * While + 0xB is clear and the entity is the + 0x13190 focus, or on
 * an attack result with bits 0x1A, the swing re-arms: PlaySong 0x112,
 * + 0x1C = -+0x100 by the Soma/entity + 0x40 x compare, + 0x20 =
 * 0x100. + 0xB re-arms only through the focus path and clears when
 * the focus moves away.
 *
 * The swing integrates every update: + 0x18 > 0 steps + 0x1C down
 * 0x10 and accumulates, < 0 steps it up, == 0 takes + 0x1C; a sign
 * crossing drops + 0x20 by 0x20 while nonzero; + 0x1C clamps to
 * -+0x20 and + 0x18 to -+0x1000. The affine draw consumes + 0x18
 * and the + 0x58 low-5 flip bits — approximated by the static
 * special_13 animation in the runtime. */

typedef struct {
    uint8_t param0, param1;
    uint8_t flags58;        /* +0x58: bit 0x20 low-screen mark */
    bool unkB;              /* +0xB: focus-init done flag */
    bool registered;        /* sub_08021278 ran this update */
    int32_t swing;          /* +0x1C: swing step, clamped -+0x20 */
    int32_t angle;          /* +0x18: integrated angle, clamped -+0x1000 */
    int32_t speed;          /* +0x20: swing amplitude, -0x20 per crossing */
    int song;               /* PlaySong request, -1 none */
} AosObject0d;

/* Object0DCreate: the resource load is covered by the exported
 * special_13 art; `deleted` reports the native load-failure path. */
void aos_object0d_create(AosObject0d *o, int param0, int param1,
                         bool *deleted);

/* Object0DUpdate. `screen_y` is + 0x46, `focused` is the + 0x13190
 * pointer compare, `attacked` the sub_080421AC bits-0x1A match and
 * `soma_left` the Soma + 0x40 < entity + 0x40 compare for the swing
 * direction. */
void aos_object0d_update(AosObject0d *o, int16_t screen_y, bool focused,
                         bool attacked, bool soma_left);
#endif
