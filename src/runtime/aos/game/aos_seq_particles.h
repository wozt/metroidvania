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

/* Runtime fields of the spawned particle for sub_08062C18. */
typedef struct {
    uint8_t state;      /* +0xA */
    uint8_t ctr_b;      /* +0xB */
    uint8_t ctr_c;      /* +0xC */
    uint8_t timer;      /* +0xD */
    uint8_t flags16;    /* +0x16 */
    uint8_t flags58;    /* +0x58 (low 3 bits = palette nibble) */
    uint8_t flags59;    /* +0x59 */
    uint8_t anim5c;     /* +0x5C */
    uint16_t field18;   /* +0x18 direction seed / wobble counter */
    int32_t spin;       /* +0x38 */
    int32_t x, y;       /* +0x40/+0x44 */
    int32_t vx, vy;     /* +0x48/+0x4C */
    int16_t spawn_x;    /* +0x42 */
    int16_t spawn_y;    /* +0x46 */
    bool child_spawned; /* state-0 one-shot ran */
    bool pal_restored;  /* sub_0806BC call on the +0xD > 0x3F exit */
} AosSeqParticleEnt;

/* sub_08062C18: state 0 waits for +0xD to wrap to 0xFF then
 * spawns the sub_08065834 child at +0x42/+0x46 (the spawn is
 * injected through `spawn_child`, which reports the
 * sub_0806E0D0 resource load: nonzero keeps the child); state 1
 * flies the particle on a sine-derived velocity, wobbles +0x18
 * and +0x38 every second tick and deletes through +0x59 bit 3
 * after +0xD passes 0x3F. `sine` is the injected sub_080009E4. */
void aos_seq_particle_update(AosSeqParticleEnt *e,
                             bool (*spawn_child)(int16_t x,
                                                 int16_t y),
                             int32_t (*sine)(uint32_t angle));
