/* SPDX-License-Identifier: GPL-3.0-only */
/* Object20 sequence particle helpers from code_08060B98.s:
 * sub_08062B60 spawns one type-0x1B particle entity
 * (EntityCreateInRange, graphics 0x08260390, frames
 * 0x08267EE4, animation 0xE) and sub_08062D8C loops it over the
 * nine 0x085268B8 records (stride 0xC) seeding each child's
 * callback, offset position, timer, random word and index
 * fields. The entity itself is not instantiated here; the spawn
 * is injected. */
#ifndef AOS_SEQ_PARTICLES_H
#define AOS_SEQ_PARTICLES_H

#include <stdbool.h>
#include <stdint.h>

/* One 0x085268B8 record (stride 0xC). */
typedef struct {
    uint32_t word0;     /* low byte -> child +0x1F */
    int16_t xoff;       /* +4, added to parent +0x20 -> child +0x42 */
    int16_t yoff;       /* +6, +0x20, added to parent +0x22 -> +0x46 */
    uint8_t timer;      /* +8 -> child +0xD */
} AosSeqParticleRecord;

/* Fields sub_08062D8C writes on each spawned child. */
typedef struct {
    void *entity;           /* sub_08062B60 result, NULL skipped */
    uint32_t callback;      /* +0 = sub_08062C18 */
    int16_t x, y;           /* +0x42/+0x46 */
    uint8_t timer;          /* +0xD */
    uint8_t field_1f;       /* +0x1F = record word0 & 0xFF */
    int16_t field_18;       /* +0x18 = RandomNumberGenerator() */
    uint8_t field_16;       /* +0x16 = index & 1 */
    uint8_t field_17;       /* +0x17 = index */
    uint8_t flags58;        /* +0x58 |= 0x20 */
} AosSeqParticle;

/* sub_08062D8C: for i in 0..8 spawn through `spawn`(record,
 * parent +0x20/+0x22) and fill `out[i]`; entries whose spawn
 * returned NULL are skipped (count returned). `rng` is the
 * injected RandomNumberGenerator. */
unsigned aos_seq_particles_spawn(const AosSeqParticleRecord recs[9],
                                 int16_t parent_x, int16_t parent_y,
                                 void *(*spawn)(const AosSeqParticleRecord *,
                                                int16_t, int16_t),
                                 int16_t (*rng)(void),
                                 AosSeqParticle out[9]);

/* sub_08062B60: returns the fields it writes on the spawned
 * type-0x1B entity, or NULL-fields when `entity` is NULL or the
 * graphics load failed. `variant_ok` is the sub_0803E654(1)
 * result (negative deletes the entity in the ROM). */
typedef struct {
    bool created;       /* EntityCreateInRange returned non-NULL */
    uint32_t scale_1c;  /* +0x1C = 0x12000 when arg nonzero */
    uint8_t flags58;    /* +0x58 transformed bits */
    uint8_t flags59;    /* +0x59 |= 0x10 */
    uint8_t anim5c;     /* +0x5C = 0x11 */
    bool affine_call;   /* sub_0803E058 issued */
    bool deleted;       /* EntityDelete path taken */
} AosSeqParticleSpawn;

AosSeqParticleSpawn aos_seq_particle_spawn(void *entity, bool has_arg,
                                           int variant_sign);

#endif
