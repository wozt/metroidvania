/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission particle effects; comments name the pinned decompilation's
 * routines. */
#include "mzm_effects.h"
#include "mzm_sfx.h"

#include <stddef.h>

enum {
    BLOCK_SIZE = 64,
    PE_CHARGING_BEAM = 59,
    SCREEN_X_BLOCK_PADDING = 2 * BLOCK_SIZE,
    SCREEN_Y_BLOCK_PADDING = 2 * BLOCK_SIZE,
    SCREEN_SIZE_X_SUB_PIXEL = 240 * 4,
    SCREEN_SIZE_Y_SUB_PIXEL = 160 * 4,
};

const char *mzm_particle_table(uint8_t effect) {
    switch (effect) {
    case MZM_PE_BOMB: return "ParticleBombOam";
    case MZM_PE_SPRITE_EXPLOSION_HUGE: return "ParticleSpriteExplosionHugeOam";
    case MZM_PE_SPRITE_EXPLOSION_SMALL: return "ParticleSpriteExplosionSmallOam";
    case MZM_PE_SPRITE_EXPLOSION_MEDIUM: return "ParticleSpriteExplosionMediumOam";
    case MZM_PE_SPRITE_EXPLOSION_BIG: return "ParticleSpriteExplosionBigOam";
    case MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG: return "ParticleSpriteExplosionSingleThenBigOam";
    case MZM_PE_SCREW_ATTACK_DESTROYED: return "ParticleScrewAttackDestroyedOam";
    case MZM_PE_HITTING_SOMETHING_WITH_NORMAL_BEAM:
        return "ParticleHittingSomethingWithNormalBeamOam";
    case MZM_PE_HITTING_SOMETHING_INVINCIBLE: return "ParticleHittingSomethingInvincibleOam";
    case MZM_PE_HITTING_SOMETHING_WITH_MISSILE: return "ParticleHittingSomethingWithMissileOam";
    case MZM_PE_HITTING_SOMETHING_WITH_SUPER_MISSILE:
        return "ParticleHittingSomethingWithSuperMissileOam";
    case MZM_PE_MEDIUM_DUST: return "ParticleMediumDustOam";
    case MZM_PE_TWO_MEDIUM_DUST: return "ParticleTwoMediumDustOam";
    default: return NULL;
    }
}

void mzm_particle_set(MzmEffects *effects, uint16_t y, uint16_t x, uint8_t effect) {
    MzmParticle *p = NULL;
    for (int i = 0; i < MZM_MAX_PARTICLES && !p; ++i)
        if (!effects->particles[i].status) p = &effects->particles[i];
    if (!p) {
        /* The particle with the highest frame counter (the charging beam
         * and escape particles are never replaced). */
        uint8_t highest = 0;
        for (int i = 0; i < MZM_MAX_PARTICLES; ++i) {
            MzmParticle *q = &effects->particles[i];
            uint8_t counter = q->effect < PE_CHARGING_BEAM ? q->frame_counter : 0;
            if (highest < counter) {
                highest = counter;
                p = q;
            }
        }
        if (!p) return;
    }
    p->status = MZM_PARTICLE_EXISTS;
    p->y = y;
    p->x = x;
    p->frame = 0;
    p->adc = 0;
    p->effect = effect;
    p->stage = 0;
    p->frame_counter = 0;
}

static uint8_t timer(const MzmEffectKinds *kinds, uint8_t effect, unsigned frame) {
    if (effect >= MZM_PARTICLE_EFFECTS || frame >= kinds->counts[effect] ||
        frame >= MZM_EFFECT_MAX_FRAMES)
        return 0;
    return kinds->timers[effect][frame];
}

/* ParticleUpdateAnimation: true when the table wrapped. */
static bool update_animation(MzmParticle *p, const MzmEffectKinds *kinds) {
    p->adc++;
    if (timer(kinds, p->effect, p->frame) <= p->adc) {
        p->adc = 0;
        p->frame++;
        if (timer(kinds, p->effect, p->frame) == 0) {
            p->frame = 0;
            return true;
        }
    }
    return false;
}

/* The one-shot effects (ParticleBomb, ParticleSpriteExplosion*, ParticleScrewAttackDestroyed,
 * ParticleHittingSomething*): removed when their table ends; the explosions
 * move to the high OAM priority on their first frame. */
static void process(MzmParticle *p, const MzmEffectKinds *kinds) {
    p->frame_counter++;
    if (!mzm_particle_table(p->effect) || update_animation(p, kinds)) {
        p->status = 0;
        return;
    }
    if (p->stage == 0) {
        p->stage++;
        if (p->effect <= MZM_PE_SCREW_ATTACK_DESTROYED) p->status |= MZM_PARTICLE_HIGH_OAM_PRIORITY;
        /* ParticleBomb */
        if (p->effect == MZM_PE_BOMB) mzm_sfx_play(MZM_SOUND_BOMB_EXPLOSION);
        /* The impact sounds of ParticleHittingSomething*. */
        switch (p->effect) {
        case MZM_PE_HITTING_SOMETHING_WITH_NORMAL_BEAM:
            mzm_sfx_play(MZM_SOUND_BEAM_IMPACT);
            break;
        case MZM_PE_HITTING_SOMETHING_INVINCIBLE:
            mzm_sfx_play(MZM_SOUND_INVINCIBLE_IMPACT);
            break;
        case MZM_PE_HITTING_SOMETHING_WITH_MISSILE:
            mzm_sfx_stop(MZM_SOUND_MISSILE_THRUST);
            mzm_sfx_play(MZM_SOUND_MISSILE_EXPLOSION);
            break;
        case MZM_PE_HITTING_SOMETHING_WITH_SUPER_MISSILE:
            mzm_sfx_stop(MZM_SOUND_SUPER_MISSILE_THRUST);
            mzm_sfx_play(MZM_SOUND_SUPER_MISSILE_EXPLOSION);
            break;
        default:
            break;
        }
    }
}

/* ParticleCheckOnScreen (no ported particle lives off screen). */
static void check_on_screen(MzmParticle *p, uint16_t camera_x, uint16_t camera_y) {
    uint16_t base_y = (uint16_t)(camera_y + (SCREEN_SIZE_Y_SUB_PIXEL - SCREEN_Y_BLOCK_PADDING));
    uint16_t y = (uint16_t)(p->y + (SCREEN_SIZE_Y_SUB_PIXEL - SCREEN_Y_BLOCK_PADDING));
    uint16_t bottom = (uint16_t)(base_y - SCREEN_Y_BLOCK_PADDING);
    uint16_t top = (uint16_t)(base_y + SCREEN_SIZE_Y_SUB_PIXEL + SCREEN_Y_BLOCK_PADDING);
    uint16_t base_x = (uint16_t)(camera_x + (SCREEN_SIZE_X_SUB_PIXEL - SCREEN_X_BLOCK_PADDING -
                                             5 * BLOCK_SIZE));
    uint16_t x = (uint16_t)(p->x + (SCREEN_SIZE_X_SUB_PIXEL - SCREEN_X_BLOCK_PADDING -
                                    5 * BLOCK_SIZE));
    uint16_t left = (uint16_t)(base_x - SCREEN_X_BLOCK_PADDING);
    uint16_t right = (uint16_t)(base_x + SCREEN_SIZE_X_SUB_PIXEL + SCREEN_X_BLOCK_PADDING);
    if (left < x && x < right && bottom < y && y < top) p->status |= MZM_PARTICLE_ONSCREEN;
    else p->status = 0;
}

void mzm_particles_process(MzmEffects *effects, const MzmEffectKinds *kinds,
                           uint16_t camera_x, uint16_t camera_y) {
    for (int pass = 0; pass < 2; ++pass) {
        uint8_t wanted = pass == 0 ? MZM_PARTICLE_EXISTS | MZM_PARTICLE_HIGH_OAM_PRIORITY
                                   : MZM_PARTICLE_EXISTS;
        for (int i = 0; i < MZM_MAX_PARTICLES; ++i) {
            MzmParticle *p = &effects->particles[i];
            if ((p->status & (MZM_PARTICLE_EXISTS | MZM_PARTICLE_HIGH_OAM_PRIORITY)) != wanted)
                continue;
            process(p, kinds);
            if (p->status & MZM_PARTICLE_EXISTS) check_on_screen(p, camera_x, camera_y);
        }
    }
}
