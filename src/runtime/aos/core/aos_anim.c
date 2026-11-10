/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow animation timing; comments name the cvaos routine. */
#include "aos_anim.h"

/* sub_0803F2C8 */
bool aos_anim_start(AosAnimState *state, const AosAnimSet *set, unsigned id, bool loop) {
    if (!set || id >= set->count) return false;
    state->flags = (uint8_t)((loop ? AOS_ANIM_LOOP : 0) | AOS_ANIM_TABLE);
    state->id = (uint8_t)id;
    state->frame = 0;
    state->tick = 0;
    return true;
}

/* sub_0803EC34; the "paused" bits 1-2 of + 0x6C are never set here. */
int aos_anim_step(AosAnimState *state, const AosAnimSet *set) {
    if (!set || state->id >= set->count) return 0;
    const AosAnimDef *anim = &set->anims[state->id];
    if (!anim->count) return 0;
    if (state->flags & AOS_ANIM_ENDED) return 3;
    if (state->flags & 6) return 1;
    if (++state->tick < anim->durations[state->frame]) return 1;
    state->tick = 0;
    if (++state->frame < anim->count) return 2;
    if (state->flags & AOS_ANIM_LOOP) {
        state->frame = 0;
        return 4;
    }
    state->frame = (uint8_t)(anim->count - 1);
    state->flags |= AOS_ANIM_ENDED;
    return 3;
}
