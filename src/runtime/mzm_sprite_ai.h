/* SPDX-License-Identifier: GPL-3.0-only */
/* Internal interface of the Zero Mission sprite engine for the AI files:
 * native units and constants, the state SpriteUpdate shares with an AI, and
 * the sprite utilities they call (named after their native routines in
 * mzm_sprites.c). Not part of the public sprite API. */
#ifndef MZM_SPRITE_AI_H
#define MZM_SPRITE_AI_H

#include "mzm_sfx.h"
#include "mzm_sprites.h"

/* Native units (include/types.h). */
enum {
    BLOCK_SIZE = 64,
    HALF_BLOCK_SIZE = 32,
    QUARTER_BLOCK_SIZE = 16,
    EIGHTH_BLOCK_SIZE = 8,
    THREE_QUARTER_BLOCK_SIZE = 48,
    PIXEL_SIZE = 4,
    SUB_PIXEL_POSITION_FLAG = 0x3F,
    BLOCK_POSITION_FLAG = 0xFFC0,
    SHORT_MAX = 32767,
    SCREEN_SIZE_X_SUB_PIXEL = 240 * 4,
    SCREEN_SIZE_Y_SUB_PIXEL = 160 * 4,
    HALF_SCREEN_BLOCKS_SUB_PIXEL = 8 * BLOCK_SIZE,  /* CEIL(SCREEN_SIZE_X_BLOCKS / 2) */
    /* SpriteUtilIsSpriteStunned on easy and normal: CONVERT_SECONDS(.15f). */
    STUN_THRESHOLD = 9,
    SPRITE_POSE_UNINITIALIZED = 0,
    SPRITE_POSE_DESTROYED = 0x62,
    SPRITE_POSE_SCREW_ATTACK_DESTROYED = 0x65,
    /* SamusSpriteCollision */
    SSC_NONE = 0,
    SSC_HURTS_SAMUS = 4,
    SSC_HURTS_SAMUS_STOP_DIES_WHEN_HIT = 6,
    SSC_SPACE_PIRATE_LASER = 19,
    SSC_SPACE_PIRATE = 20,
    SSC_CHECK_COLLIDING = 30,
    SPRITE_POSE_STOPPED = 66,
    SSC_SMALL_ENERGY_DROP = 31,
    SSC_LARGE_ENERGY_DROP = 32,
    SSC_MISSILE_DROP = 33,
    SSC_SUPER_MISSILE_DROP = 34,
    SSC_POWER_BOMB_DROP = 35,
    SPRITE_DROP_MAX_PROB = 1024,
    /* CONVERT_SECONDS of the combat timers. */
    ISFT_DAMAGED = 17,              /* .25f + 2 * DELTA_TIME */
    ISFT_SCREW_ATTACK = 17,         /* .29f */
    ISFT_SOLID_HIT = 3,             /* .05f */
    IGNORE_SAMUS_AFTER_HIT = 15,    /* .25f */
    NORMAL_BEAM_DAMAGE = 2,
    MISSILE_DAMAGE = 20,
    SUPER_MISSILE_DAMAGE = 100,
    BOMB_DAMAGE = 8,
};

/* The state SpriteUpdate shares with an AI (gCurrentSprite, gSpriteRng,
 * gPreviousVerticalCollisionCheck, gFrameCounter8Bit). */
typedef struct MzmAiCtx {
    MzmSprite *s;
    const MzmSpriteFrame *f;
    const MzmSpriteWorld *world;
    const MzmSpriteTables *tables;
    const MzmSpriteKind *kind;
    uint8_t rng;
    uint8_t vertical;
    uint8_t frame8;
} Ctx;

/* NearLeftRight */
enum { NSLR_OUT_OF_RANGE = 0, NSLR_LEFT = 4, NSLR_RIGHT = 8 };

/* SpriteDeathType */
enum { DEATH_NORMAL, DEATH_RESPAWNING, DEATH_NO_DEATH_OR_RESPAWNING_ALREADY_HAS_DROP };

uint8_t ai_collision_at(const Ctx *c, uint16_t y, uint16_t x);
/* SpriteUtilGetCollisionAtPosition for an actor (a clipdata-affecting
 * action at or above CAA_UNUSED makes ClipdataProcess use the non-sprite
 * actor). */
uint8_t ai_collision_at_actor(const Ctx *c, uint16_t y, uint16_t x, int actor);
/* gCurrentAffectingClipdata.movement after ClipdataProcess at a point. */
uint8_t ai_movement_at(const Ctx *c, uint16_t y, uint16_t x);
/* The frame duration of a table entry (0 past its end). */
uint8_t ai_timer(const Ctx *c, unsigned anim, unsigned frame);
/* SpriteUtilAlignYPositionOnSlopeAtOrigin */
void ai_align_on_slope(Ctx *c);
uint16_t ai_check_vertical(Ctx *c, uint16_t y, uint16_t x);
uint16_t ai_check_vertical_slopes(Ctx *c, uint16_t y, uint16_t x);
bool ai_animation_ending(const Ctx *c, unsigned extra);
void ai_set_anim(MzmSprite *s, uint8_t anim);
void ai_random_x_direction(Ctx *c);
void ai_sprite_death(Ctx *c, uint16_t y, uint16_t x, uint8_t effect);
void ai_sprite_death_typed(Ctx *c, int type, uint16_t y, uint16_t x, uint8_t effect);
void ai_spawn_secondary(Ctx *c, uint8_t sprite_id, uint8_t part, uint8_t gfx_slot, uint16_t y,
                        uint16_t x, uint16_t status);
MzmSprite *ai_spawn_drop_follower(Ctx *c, uint8_t sprite_id, uint8_t part, uint8_t gfx_slot,
                                  uint8_t ram_slot, uint16_t y, uint16_t x, uint16_t status);
void ai_particle_set(const Ctx *c, uint16_t y, uint16_t x, uint8_t effect);
void ai_debris_init(const Ctx *c, uint8_t type, uint16_t y, uint16_t x);
bool ai_samus_near_left_right(const Ctx *c, uint16_t y_range, uint16_t x_range);
/* SpriteUtilCheckSamusNearSpriteLeftRight with its NSLR result. */
uint8_t ai_samus_nslr(const Ctx *c, uint16_t y_range, uint16_t x_range);
bool ai_check_event(const Ctx *c, uint8_t event);
bool ai_samus_above(const Ctx *c, uint16_t y_range, uint16_t x_range);
void ai_face_samus(const Ctx *c);
/* `if (status & SPRITE_STATUS_ONSCREEN) SoundPlay(sound)` and its
 * SoundPlayNotAlreadyPlaying form. */
static inline void ai_sound_onscreen(const MzmSprite *s, uint16_t sound) {
    if (s->status & MZM_SPRITE_STATUS_ONSCREEN) mzm_sfx_play(sound);
}
static inline void ai_sound_onscreen_once(const MzmSprite *s, uint16_t sound) {
    if (s->status & MZM_SPRITE_STATUS_ONSCREEN) mzm_sfx_play_not_playing(sound);
}
bool ai_has_drops(const Ctx *c);

/* mzm_space_pirate.c */
void ai_space_pirate(Ctx *c);
void ai_space_pirate_laser(Ctx *c);
/* mzm_chozodia_security.c: searchlights, security lasers, searchlight eyes. */
void ai_chozodia_security(Ctx *c);

#endif /* MZM_SPRITE_AI_H */
