/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission breakable blocks (no SDL): src/block.c (BlockApplyCcaa,
 * BlockCheckCcaa and the destruction, reveal, reform and bomb chain
 * routines, BlockUpdateBrokenBlocks, BlockProcessBombChains,
 * BlockSamusApplyScrewSpeedboosterDamageToEnvironment) on a room the
 * environment exposes block by block (raw Clipdata values, BG1 values).
 * The tables (sBlockBehaviors, sBlockWeaknesses, sReformingBlocksTilemapValue,
 * sBrokenBlocksTimers, sClipdataAffectingActionDamageTypes, sTankBehaviors,
 * sBombChainReverseData, sSubBombChainPositionOffset) come from the room
 * export. Sounds are not played. */
#ifndef MZM_BLOCKS_H
#define MZM_BLOCKS_H

#include <stdbool.h>
#include <stdint.h>

#define MZM_BLOCK_TYPES 22          /* BLOCK_TYPE_COUNT */
#define MZM_BLOCK_BEHAVIORS 48      /* BEHAVIOR_BLOCK_AMOUNT */
#define MZM_TANK_BEHAVIORS 12       /* BEHAVIOR_TANK_AMOUNT */
#define MZM_CAA_COUNT 16
#define MZM_MAX_BROKEN_BLOCKS 48    /* MAX_AMOUNT_OF_BROKEN_BLOCKS */
#define MZM_MAX_BOMB_CHAINS 4       /* MAX_AMOUNT_OF_BOMB_CHAINS */
#define MZM_NEVER_REFORM_BLOCKS 128
#define MZM_MAX_MAKE_SOLID_BLOCKS 64    /* MAX_AMOUNT_OF_MAKE_SOLID_BLOCKS */

/* ClipdataAffectingAction */
enum {
    MZM_CAA_NONE, MZM_CAA_REMOVE_SOLID, MZM_CAA_MAKE_SOLID_GRIPPABLE, MZM_CAA_MAKE_STOP_ENEMY,
    MZM_CAA_MAKE_NON_POWER_GRIP, MZM_CAA_BOMB_CHAIN_UNUSED, MZM_CAA_UNUSED, MZM_CAA_BEAM,
    MZM_CAA_BOMB_PISTOL, MZM_CAA_MISSILE, MZM_CAA_SUPER_MISSILE, MZM_CAA_POWER_BOMB,
    MZM_CAA_SPEEDBOOSTER, MZM_CAA_SPEEDBOOSTER_ON_GROUND, MZM_CAA_SCREW_ATTACK,
    MZM_CAA_BOMB_CHAIN,
};

/* DestructingAction */
enum {
    MZM_DESTRUCT_NONE, MZM_DESTRUCT_SPEED, MZM_DESTRUCT_SCREW, MZM_DESTRUCT_SPEED_SCREW,
    MZM_DESTRUCT_SPEED_ON_GROUND,
};

typedef struct {
    uint8_t life_type, sub_type, type, speedboost, bomb_chain;
} MzmBlockBehavior;

typedef struct {
    bool broken;
    uint8_t stage, type;
    uint16_t timer;
    int x, y;                       /* u8 natively */
} MzmBrokenBlock;

typedef struct {
    uint8_t offset, type;
    bool up_or_left, down_or_right;  /* flipped / unk */
    int x, y;
} MzmBombChain;

typedef struct {
    /* ROM tables. */
    uint16_t caa_damage[MZM_CAA_COUNT];
    MzmBlockBehavior behaviors[MZM_BLOCK_BEHAVIORS];
    uint16_t weakness[MZM_BLOCK_TYPES];
    uint16_t reforming[MZM_BLOCK_TYPES];
    uint8_t timers[MZM_BLOCK_TYPES][13];
    uint16_t tank_revealed[MZM_TANK_BEHAVIORS];
    uint8_t chain_flag[8];
    uint16_t chain_behavior[8];
    int8_t sub_chain[4][4];
    /* The Clipdata values CAA_MAKE_SOLID_GRIPPABLE, CAA_MAKE_STOP_ENEMY and
     * CAA_MAKE_NON_POWER_GRIP write. */
    uint16_t make_solid_clip[3];
    /* The room. */
    void *context;
    int width, height;              /* in blocks */
    uint16_t (*get_clip)(void *context, int y, int x);
    /* The behavior (pClipBehaviors) and collision type of a raw value. */
    uint16_t (*behavior)(void *context, uint16_t clip);
    uint8_t (*collision)(void *context, uint16_t clip);
    void (*set_clip)(void *context, uint16_t clip, int y, int x);
    void (*set_bg1)(void *context, uint16_t value, int y, int x);
    /* BgClipCheckOpeningHatch: true unless HATCH_OPENING_ACTION_NOT_OPENING. */
    bool (*check_hatch)(void *context, int x, int y, unsigned damage);
    /* BlockCheckSamusInReformingBlock */
    bool (*samus_in_block)(void *context, int x, int y);
} MzmBlockEnv;

typedef struct {
    MzmBrokenBlock broken[MZM_MAX_BROKEN_BLOCKS];
    MzmBombChain chains[MZM_MAX_BOMB_CHAINS];
    uint8_t active_chain_types;     /* gActiveBombChainTypes */
    uint8_t caa;                    /* gCurrentClipdataAffectingAction */
    /* Never-reform blocks destroyed in the current room (gNeverReformBlocks
     * entries of the room), as block positions. */
    int never_reform[MZM_NEVER_REFORM_BLOCKS][2];
    int never_reform_count;
    /* gMakeSolidBlocks: C_16_2_8(x, y) of the blocks a clipdata-affecting
     * action made solid. */
    uint16_t make_solid[MZM_MAX_MAKE_SOLID_BLOCKS];
} MzmBlocks;

/* BlockApplyCcaa with `blocks->caa` set: true when something changed. */
bool mzm_block_apply_ccaa(MzmBlocks *blocks, const MzmBlockEnv *env, int y, int x, uint16_t clip);
/* A clipdata-affecting action on the block at a subpixel point (the
 * ClipdataProcess path of projectiles): true when it changed something. */
bool mzm_blocks_affect(MzmBlocks *blocks, const MzmBlockEnv *env, uint8_t caa, int32_t x,
                       int32_t y);
/* BlockSamusApplyScrewSpeedboosterDamageToEnvironment */
bool mzm_blocks_samus_destruct(MzmBlocks *blocks, const MzmBlockEnv *env, int32_t x, int32_t y,
                               int action);
/* BlockStoreBrokenReformBlock (crumble blocks start at stage 1 when
 * `advance_stage`). */
bool mzm_blocks_store_broken_reform(MzmBlocks *blocks, const MzmBlockEnv *env, uint8_t type,
                                    int x, int y, bool advance_stage);
/* BlockUpdateBrokenBlocks */
void mzm_blocks_update_broken(MzmBlocks *blocks, const MzmBlockEnv *env);
/* BlockProcessBombChains (one chain per frame, by gFrameCounter8Bit). */
void mzm_blocks_process_bomb_chains(MzmBlocks *blocks, const MzmBlockEnv *env, uint8_t frame8);
/* BlockRemoveNeverReformSingleBlock (room load). */
void mzm_blocks_remove_never_reform(const MzmBlockEnv *env, int x, int y);

#endif /* MZM_BLOCKS_H */
