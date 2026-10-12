/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission particle effects and sprite debris (no SDL).
 *
 * Particles follow src/particle.c: ParticleSet (the first free slot, or the
 * oldest particle), ParticleUpdateAnimation and ParticleCheckOnScreen, for
 * the one-shot effects the ported sprites and projectiles set: sprite
 * explosions, the screw attack kill, projectile impacts and dust. Their sounds and
 * the super missile screen shake are not ported. Sprite debris
 * (src/sprite_debris.c) is stored here and processed by mzm_sprites, which
 * owns the collision helpers it needs. Frame durations come from the OAM
 * tables exported by scripts/mzm_sprites.py. */
#ifndef MZM_EFFECTS_H
#define MZM_EFFECTS_H

#include <stdbool.h>
#include <stdint.h>

#define MZM_MAX_PARTICLES 16
#define MZM_MAX_SPRITE_DEBRIS 8
#define MZM_PARTICLE_EFFECTS 64
#define MZM_EFFECT_MAX_FRAMES 32

/* ParticleEffectId values of the ported effects. */
enum {
    MZM_PE_BOMB = 19,
    MZM_PE_SPRITE_EXPLOSION_HUGE = 30,
    MZM_PE_SPRITE_EXPLOSION_SMALL = 31,
    MZM_PE_SPRITE_EXPLOSION_MEDIUM = 32,
    MZM_PE_SPRITE_EXPLOSION_BIG = 33,
    MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG = 34,
    MZM_PE_SCREW_ATTACK_DESTROYED = 35,
    MZM_PE_SPEEDBOOSTER_DESTROYED = 38,
    MZM_PE_HITTING_SOMETHING_WITH_NORMAL_BEAM = 42,
    MZM_PE_HITTING_SOMETHING_INVINCIBLE = 47,
    MZM_PE_HITTING_SOMETHING_WITH_MISSILE = 48,
    MZM_PE_HITTING_SOMETHING_WITH_SUPER_MISSILE = 49,
    MZM_PE_MEDIUM_DUST = 53,
    MZM_PE_TWO_MEDIUM_DUST = 54,
};

/* ParticleStatus */
enum {
    MZM_PARTICLE_EXISTS = 1 << 0,
    MZM_PARTICLE_ONSCREEN = 1 << 1,
    MZM_PARTICLE_HIGH_OAM_PRIORITY = 1 << 2,
};

typedef struct {
    uint8_t status;
    uint8_t adc;                    /* animationDurationCounter */
    uint8_t effect;                 /* ParticleEffectId */
    uint8_t stage;
    uint8_t frame_counter;
    uint16_t frame;                 /* currentAnimationFrame */
    uint16_t y, x;                  /* subpixels */
} MzmParticle;

typedef struct {
    bool exists;
    uint8_t adc;
    uint8_t type;                   /* debrisType (0x80: bouncing) */
    uint8_t frame_counter;
    uint8_t offset;                 /* arrayOffset */
    uint16_t frame;
    uint16_t y, x;
} MzmSpriteDebris;

typedef struct {
    MzmParticle particles[MZM_MAX_PARTICLES];
    MzmSpriteDebris debris[MZM_MAX_SPRITE_DEBRIS];
} MzmEffects;

/* Frame durations of each effect's OAM table (by ParticleEffectId) and of
 * sSpriteDebrisOam; a zero count means the effect is not available. */
typedef struct {
    uint8_t timers[MZM_PARTICLE_EFFECTS][MZM_EFFECT_MAX_FRAMES];
    uint8_t counts[MZM_PARTICLE_EFFECTS];
    uint8_t debris_timers[MZM_EFFECT_MAX_FRAMES];
    uint8_t debris_count;
} MzmEffectKinds;

/* The OAM table name of a ported effect (without the leading `s`), or NULL. */
const char *mzm_particle_table(uint8_t effect);

/* ParticleSet */
void mzm_particle_set(MzmEffects *effects, uint16_t y, uint16_t x, uint8_t effect);
/* ParticleProcessAll during gameplay: the high OAM priority particles, then
 * the others; each runs its effect, then ParticleCheckOnScreen against the
 * camera (subpixels). */
void mzm_particles_process(MzmEffects *effects, const MzmEffectKinds *kinds,
                           uint16_t camera_x, uint16_t camera_y);

#endif /* MZM_EFFECTS_H */
