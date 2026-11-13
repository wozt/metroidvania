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

void aos_seq_particle_update(AosSeqParticleEnt *e,
                             bool (*spawn_child)(int16_t, int16_t),
                             int32_t (*sine)(uint32_t)) {
    if (e->state == 0) {
        e->timer = (uint8_t)(e->timer - 1);
        if (e->timer != 0xFF) return;
        e->timer = 0;
        e->spin = 0x10000;
        e->state = 1;
        e->ctr_b = 0;
        e->ctr_c = 0;
        e->flags58 &= (uint8_t)~0x21;
        e->child_spawned = true;
        if (spawn_child &&
            spawn_child(e->spawn_x, e->spawn_y)) {
            /* child keeps the parent's +0x42/+0x46, +0x16 = 0,
             * sub_0803F2C8(e, 1, 0, 0) and sub_0803F17C(e) —
             * engine-side, reported through the callback. */
        }
        return;
    }
    if (e->state != 1) return;
    uint32_t base = (e->field18 & 0x1Fu) << 11;
    int32_t angle = (int32_t)(base + 0x4000);
    e->vx = sine ? (sine((uint32_t)angle) >> 8) << 10 : 0;
    e->vy = sine ? (sine(base) >> 8) << 10 : 0;
    e->x += e->vx;
    e->y += e->vy;
    e->timer = (uint8_t)(e->timer + 1);
    e->anim5c = e->timer > 0x20 ? 0x11 : 3;
    if ((e->timer & 1) == 0) {
        if (e->flags16) {
            e->field18 = (uint16_t)(e->field18 + 1);
            e->spin += 0x200;
        } else {
            e->field18 = (uint16_t)(e->field18 - 1);
            e->spin -= 0x200;
        }
    }
    /* sub_0803E058(spin, 0x12000, 0x12000, 0, {0, nibble}) —
     * draw-side request. */
    if (e->timer > 0x3F) {
        e->pal_restored = true;    /* sub_0803E6BC(+0x58 & 7) */
        e->flags59 |= 0x8;
    }
}
