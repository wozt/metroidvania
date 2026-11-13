/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_seq_particles.h"

#include <string.h>

unsigned aos_seq_particles_spawn(const AosSeqParticleRecord recs[9],
                                 int16_t parent_x, int16_t parent_y,
                                 void *(*spawn)(const AosSeqParticleRecord *,
                                                int16_t, int16_t),
                                 int16_t (*rng)(void),
                                 AosSeqParticle out[9]) {
    unsigned count = 0;
    for (unsigned i = 0; i < 9; ++i) {
        const AosSeqParticleRecord *rec = &recs[i];
        void *entity = spawn ? spawn(rec, parent_x, parent_y) : NULL;
        if (!entity) continue;
        AosSeqParticle *p = &out[count++];
        memset(p, 0, sizeof *p);
        p->entity = entity;
        p->callback = 0x08062C18; /* native routine id */
        p->x = (int16_t)(parent_x + rec->xoff);
        p->y = (int16_t)(parent_y + rec->yoff + 0x20);
        p->timer = rec->timer;
        p->field_1f = (uint8_t)rec->word0;
        p->field_18 = rng ? rng() : 0;
        p->field_16 = (uint8_t)(i & 1);
        p->field_17 = (uint8_t)i;
        p->flags58 = 0x20;
    }
    return count;
}

AosSeqParticleSpawn aos_seq_particle_spawn(void *entity, bool has_arg,
                                           int variant_sign) {
    AosSeqParticleSpawn o;
    memset(&o, 0, sizeof o);
    if (!entity) return o;
    o.created = true;
    /* sub_08062B60 also writes +0x2C = 0xE, loads frames
     * 0x08267EE4 animation 0xE through sub_0803B924 and sets the
     * update to sub_0803B9D0 — draw-side, not tracked. */
    if (has_arg) {
        o.scale_1c = 0x12000;
        if (variant_sign < 0) {
            o.deleted = true;   /* EntityDelete path */
            o.created = false;
            return o;
        }
        o.flags58 = (uint8_t)(variant_sign & 0x1F);
        o.flags59 = 0x10;
        o.affine_call = true;   /* sub_0803E058(0x10000, +0x1C, ...) */
    }
    o.anim5c = 0x11;
    return o;
}
