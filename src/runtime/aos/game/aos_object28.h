#ifndef AOS_OBJECT28_H
#define AOS_OBJECT28_H
#include "aos_object27.h"

/* Object28Create (0x08057FA8) / Object28Update (0x080581B4),
 * dispatch index 40 (0x28): the Object27 orb's sibling. The
 * + 0xB four-state machine in sub_08058094 is instruction-for-
 * instruction the Object27 machine — same + 0x48 ramp
 * (0xC000 -> 0x40000 by 0x800), same state-2 + 0xD = 0x1E hold,
 * same state-3 bleed to 0xC000 — over the identical chandelier
 * resources. The only native differences are the create-time
 * sub_0803F2C8 preset argument (4 instead of 2), the hurt
 * callback wrapper sub_08058228 and the state-0 gate reading
 * the + 0x14 entity's + 0x1A s16 instead of + 0x18; all three
 * collapse to the same approximations Object27 already
 * documents, so this module delegates to it. */
typedef AosObject27 AosObject28;

void aos_object28_create(AosObject28 *o, uint8_t variant);
void aos_object28_update(AosObject28 *o, bool target_far);
#endif
