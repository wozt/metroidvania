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
    ONE_SUB_PIXEL = 1,
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
    SPRITE_POSE_SHINESPARK_DESTROYED = 0x63,
    SPRITE_POSE_SPEEDBOOSTER_DESTROYED = 0x64,
    SPRITE_POSE_SCREW_ATTACK_DESTROYED = 0x65,
    SPRITE_POSE_PSEUDO_SCREW_DESTROYED = 0x66,
    /* SamusSpriteCollision */
    SSC_NONE = 0,
    SSC_SOLID = 1,
    SSC_CAN_STAND_ON_TOP = 2,
    SSC_HURTS_SAMUS = 4,
    SSC_HURTS_SAMUS_A_LOT = 5,
    SSC_HURTS_SAMUS_STOP_DIES_WHEN_HIT = 6,
    SSC_KNOCKS_BACK_SAMUS = 7,
    SSC_HURTS_KNOCKBACK_IF_INVINCIBLE = 10,
    SSC_HURTS_SAMUS_SOLID = 11,
    SSC_MELLOW = 18,
    SSC_SOLID_SIDEWAYS = 12,
    SSC_HURTS_SAMUS_IGNORE_INVINCIBILITY = 15,
    SSC_HURTS_SAMUS_IGNORE_INVINCIBILITY_NO_CONTACT_DAMAGE = 21,
    SSC_ACID_WORM_MOUTH = 22,
    SSC_HURTS_SAMUS_STOP_DIES_WHEN_HIT_NO_KNOCKBACK = 16,
    SSC_SPACE_PIRATE_LASER = 19,
    SSC_SPACE_PIRATE = 20,
    SSC_PARASITE = 23,
    SSC_METROID = 24,
    SSC_HURTS_RIGHT_CAN_STAND_ON_TOP = 25,
    SSC_ZIPLINE = 26,
    SSC_SOLID_CHECK_COLLIDING = 27,
    SSC_RIDLEY_CLAW = 28,
    SSC_MECHA_RIDLEY = 29,
    SSC_CHECK_COLLIDING = 30,
    SPRITE_POSE_STOPPED = 66,
    SSC_SMALL_ENERGY_DROP = 31,
    SSC_LARGE_ENERGY_DROP = 32,
    SSC_MISSILE_DROP = 33,
    SSC_SUPER_MISSILE_DROP = 34,
    SSC_POWER_BOMB_DROP = 35,
    SSC_MULTIPLE_LARGE_ENERGY_DROP = 36,
    SPRITE_DROP_MAX_PROB = 1024,
    /* CONVERT_SECONDS of the combat timers. */
    ISFT_DAMAGED = 17,              /* .25f + 2 * DELTA_TIME */
    ISFT_SCREW_ATTACK = 17,         /* .29f */
    ISFT_SOLID_HIT = 3,             /* .05f */
    IGNORE_SAMUS_AFTER_HIT = 15,    /* .25f */
    NORMAL_BEAM_DAMAGE = 2,
    LONG_BEAM_DAMAGE = 3,
    CHARGED_NORMAL_BEAM_DAMAGE = 8,
    CHARGED_LONG_BEAM_DAMAGE = 12,
    ICE_BEAM_DAMAGE = 3,
    ICE_LONG_BEAM_DAMAGE = 4,
    CHARGED_ICE_BEAM_DAMAGE = 12,
    CHARGED_ICE_LONG_BEAM_DAMAGE = 16,
    CHARGED_PISTOL_DAMAGE = 12,
    SPRITE_FREEZE_TIMER = 240,       /* CONVERT_SECONDS(4.f) */
    MISSILE_DAMAGE = 20,
    SUPER_MISSILE_DAMAGE = 100,
    BOMB_DAMAGE = 8,
    POWER_BOMB_DAMAGE = 50,
    ISFT_POWER_BOMB_STUNNED = 0x80,  /* SPRITE_ISFT_POWER_BOMB_STUNNED */
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
/* SpriteUtilSpriteDeath with its playSound flag (the explosion sounds). */
void ai_sprite_death_full(Ctx *c, int type, uint16_t y, uint16_t x, bool play_sound, uint8_t effect);
/* SpriteSpawnPrimary: the new sprite's slot, or -1. */
int ai_spawn_primary(Ctx *c, uint8_t sprite_id, uint8_t part, uint8_t gfx_slot, uint16_t y,
                     uint16_t x, uint16_t status);
/* SpriteSpawnSecondary: the new sprite's slot, or -1. */
int ai_spawn_secondary(Ctx *c, uint8_t sprite_id, uint8_t part, uint8_t gfx_slot, uint16_t y,
                       uint16_t x, uint16_t status);
MzmSprite *ai_spawn_drop_follower(Ctx *c, uint8_t sprite_id, uint8_t part, uint8_t gfx_slot,
                                  uint8_t ram_slot, uint16_t y, uint16_t x, uint16_t status);
void ai_particle_set(const Ctx *c, uint16_t y, uint16_t x, uint8_t effect);
void ai_debris_init(const Ctx *c, uint8_t type, uint16_t y, uint16_t x);
bool ai_samus_near_left_right(const Ctx *c, uint16_t y_range, uint16_t x_range);
/* SpriteUtilCheckSamusNearSpriteLeftRight with its NSLR result. */
uint8_t ai_samus_nslr(const Ctx *c, uint16_t y_range, uint16_t x_range);
bool ai_check_event(const Ctx *c, uint8_t event);
/* SpriteUtilRefillEnergy / Missiles / SuperMissiles / PowerBombs: a
 * fraction of what is missing (`divisor`), between 1 and `cap`; false once
 * full. */
bool ai_refill(int *current, int max, int divisor, int cap);
bool ai_samus_above(const Ctx *c, uint16_t y_range, uint16_t x_range);
/* SpriteUtilCheckOutOfRoomEffect / SpriteUtilCheckInRoomEffect (no splash). */
bool ai_left_room_effect(const Ctx *c, uint16_t old_y, uint16_t y);
bool ai_entered_room_effect(const Ctx *c, uint16_t old_y, uint16_t y);
/* gCurrentAffectingClipdata.hazard after a ClipdataProcess at a point. */
uint8_t ai_hazard_at(const Ctx *c, uint16_t y, uint16_t x);
void ai_face_samus(const Ctx *c);
void ai_projectile_collision_self(Ctx *c);
uint8_t ai_face_target_rotation(uint8_t rotation, uint16_t target_y, uint16_t target_x,
                                uint16_t sprite_y, uint16_t sprite_x);
void ai_move_towards_target(Ctx *c, uint16_t target_y, uint16_t target_x, uint8_t y_speed,
                            uint8_t x_speed, uint8_t divisor);
/* SpriteUtilUpdateFreezeTimer */
void ai_update_freeze_timer(Ctx *c);
/* `if (status & SPRITE_STATUS_ONSCREEN) SoundPlay(sound)` and its
 * SoundPlayNotAlreadyPlaying form. */
static inline void ai_sound_onscreen(const MzmSprite *s, uint16_t sound) {
    if (s->status & MZM_SPRITE_STATUS_ONSCREEN) mzm_sfx_play(sound);
}
static inline void ai_sound_onscreen_once(const MzmSprite *s, uint16_t sound) {
    if (s->status & MZM_SPRITE_STATUS_ONSCREEN) mzm_sfx_play_not_playing(sound);
}
bool ai_has_drops(const Ctx *c);
/* Sqrt (BIOS SWI 8): the integer square root. */
uint16_t mzm_bios_sqrt(uint32_t value);

/* mzm_space_pirate.c */
void ai_space_pirate(Ctx *c);
void ai_space_pirate_laser(Ctx *c);
/* mzm_chozodia_security.c: searchlights, security lasers, searchlight eyes. */
void ai_chozodia_security(Ctx *c);
/* mzm_chozo_statue.c: item and hint Chozo statues, their parts and ball. */
void ai_chozo_statue(Ctx *c);
/* mzm_item_sprites.c: the Morph Ball, the Power Grip and the Charge Beam. */
void ai_item_sprite(Ctx *c);
/* mzm_gadget_sprites.c */
void ai_gadget(Ctx *c);
/* Parasites (mzm_parasite.c). */
void ai_parasite(Ctx *c);
/* Atomics and their electricity (mzm_atomic.c). */
void ai_atomic(Ctx *c);
/* Polyps and their projectiles (mzm_polyp.c). */
void ai_polyp(Ctx *c);
/* Tourian's Zebetites, cannons and cannon bullets (mzm_tourian.c). */
void ai_tourian(Ctx *c);
/* Worker Robots (mzm_worker_robot.c). */
void ai_worker_robot(Ctx *c);
/* Metroids and their shells (mzm_metroid.c). */
void ai_metroid(Ctx *c);
/* Kraid, its parts, spikes and nails (mzm_kraid.c). */
void ai_kraid(Ctx *c);
/* Ridley, its parts, tail and fireballs (mzm_ridley.c). */
void ai_ridley(Ctx *c);
/* Mother Brain, its parts, beam, block and glass (mzm_mother_brain.c). */
void ai_mother_brain(Ctx *c);
/* Deorem, its segments, eye and thorns (mzm_deorem.c). */
void ai_deorem(Ctx *c);
/* Acid Worm, its body parts and spit (mzm_acid_worm.c). */
void ai_acid_worm(Ctx *c);
/* The hives, their roots, the Mellows and their swarms (mzm_hive.c). */
void ai_hive(Ctx *c);
/* Kraid and Ridley's Gadoras, eye and beam (mzm_gadora.c). */
void ai_gadora(Ctx *c);
/* The paired Imago larvae, their parts and the animated passage remnant. */
void ai_imago_larva(Ctx *c);
void ai_imago_larva_right_side(Ctx *c);
/* The Imago tunnel trigger and the cocoon actors (mzm_imago_cocoon.c). */
void ai_imago_cocoon(Ctx *c);
/* Mecha Ridley, its synchronized body parts and weapons (mzm_mecha_ridley.c). */
void ai_mecha_ridley(Ctx *c);
/* mzm_gunship.c: the gunship and its parts. */
void ai_gunship(Ctx *c);

#endif /* MZM_SPRITE_AI_H */
