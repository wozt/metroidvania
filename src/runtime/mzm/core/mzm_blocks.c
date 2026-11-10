/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission breakable blocks; comments name the pinned decompilation's
 * routines. */
#include "mzm_blocks.h"
#include "mzm_sfx.h"

#include <stddef.h>

/* ClipBehavior, ClipdataTilemap and the block enums
 * (include/constants/clipdata.h, block.h). */
enum {
    BLOCK_START = 16,               /* CLIP_BEHAVIOR_BLOCK_START */
    TANK_START = 52,                /* CLIP_BEHAVIOR_TANK_START */
    LAST_BLOCK = 63 - BLOCK_START,  /* BEHAVIOR_TO_BLOCK(UNDERWATER_POWER_BOMB_TANK) */
    TOP_LEFT_SHOT_NEVER = 17,
    TOP_RIGHT_SHOT_NEVER = 18,
    BOTTOM_LEFT_SHOT_NEVER = 19,
    BOTTOM_RIGHT_SHOT_NEVER = 20,
    TOP_RIGHT_SHOT_NO = 23,
    BOTTOM_LEFT_SHOT_NO = 24,
    BOTTOM_RIGHT_SHOT_NO = 25,
    BOMB_NEVER = 27,
    BOMB_REFORM = 28,
    MISSILE_NEVER = 29,
    MISSILE_NO = 30,
    SUPER_MISSILE_NEVER = 31,
    SUPER_MISSILE_NO = 32,
    SPEEDBOOST_NO = 33,
    SPEEDBOOST_REFORM = 34,
    CRUMBLE = 35,
    POWER_BOMB_NEVER = 36,
    SCREW_NO = 37,
    VERTICAL_CHAIN1 = 38,
    SLOW_CRUMBLE = 46,
    TILEMAP_FLAG = 1 << 10,
    TILEMAP_AIR = 0,
    TILEMAP_SOLID = 1,
    TILEMAP_SOLID_BREAKING_1 = 2,
    TILEMAP_SOLID_BREAKING_2 = 3,
    TILEMAP_SOLID_BREAKING_3 = 4,
    CLIPDATA_TYPE_DOOR = 11,
    LIFE_NO_NEVER_REFORM = 1,
    LIFE_REFORM = 2,
    LIFE_BOMB_CHAIN = 3,
    LIFE_TANK = 4,
    SUB_REFORM = 0,
    SUB_SQUARE_NO_REFORM = 1,
    SUB_NO_REFORM = 2,
    SUB_SQUARE_NEVER_REFORM = 3,
    SUB_BOMB_CHAIN = 4,
    TYPE_BOMB_BLOCK_NEVER_REFORM = 13,
    CHAIN_VERTICAL4 = 3,
    DAMAGE_BOMB_PISTOL = 1 << 1,
    REVEAL_BOMB_BLOCKS = 1 << 0 | 1 << 2 | 1 << 3,   /* CAA_REVEAL_BOMB_BLOCKS */
    TANK_WEAKNESS = 0x1F,
    SUB_CHAIN_UP = 0,
    SUB_CHAIN_DOWN = 1,
    SUB_CHAIN_LEFT = 2,
    SUB_CHAIN_RIGHT = 3,
};

/* struct ClipdataBlockData */
typedef struct {
    int x, y;
    uint16_t behavior;
    uint16_t block_behavior;
} ClipBlock;

static uint16_t damage(const MzmBlocks *blocks, const MzmBlockEnv *env) {
    return blocks->caa < MZM_CAA_COUNT ? env->caa_damage[blocks->caa] : 0;
}

static void set_block(const MzmBlockEnv *env, uint16_t value, int y, int x) {
    env->set_bg1(env->context, value, y, x);
    env->set_clip(env->context, value, y, x);
}

/* BlockUpdateBrokenBlockAnimation: the BG1 frame of a stage. */
static void broken_animation(const MzmBlockEnv *env, const MzmBrokenBlock *b) {
    uint16_t value = TILEMAP_AIR;
    switch (b->stage) {
    case 2:
    case 12: value = env->reforming[b->type]; break;
    case 3:
    case 11: value = TILEMAP_FLAG | TILEMAP_SOLID; break;
    case 4:
    case 10: value = TILEMAP_FLAG | TILEMAP_SOLID_BREAKING_1; break;
    case 5:
    case 9: value = TILEMAP_FLAG | TILEMAP_SOLID_BREAKING_2; break;
    case 6:
    case 8: value = TILEMAP_FLAG | TILEMAP_SOLID_BREAKING_3; break;
    case 7: value = TILEMAP_FLAG | TILEMAP_AIR; break;
    }
    if (value == TILEMAP_AIR) return;
    env->set_bg1(env->context, value, b->y, b->x);
}

/* BlockBrokenBlockRemoveCollision */
static void remove_collision(const MzmBlockEnv *env, int y, int x) {
    env->set_clip(env->context, 0, y, x);
    env->set_bg1(env->context, 0, y, x);
}

static void reset_broken(MzmBrokenBlock *b) {
    b->broken = false;
    b->stage = 0;
    b->type = 0;
    b->x = 0;
    b->y = 0;
}

bool mzm_blocks_store_broken_reform(MzmBlocks *blocks, const MzmBlockEnv *env, uint8_t type,
                                    int x, int y, bool advance_stage) {
    unsigned result = 0;
    int i;
    for (i = 0; i < MZM_MAX_BROKEN_BLOCKS; ++i) {
        MzmBrokenBlock *b = &blocks->broken[i];
        if (b->x == x && b->y == y) {
            result = b->stage == 0;
            break;
        }
        if (!(result & 0x80) && b->stage == 0) result = (unsigned)i | 0x80;
    }
    if (!result) return false;
    if (result != 1) i = (int)(result & 0x7F);
    MzmBrokenBlock *b = &blocks->broken[i];
    b->broken = true;
    b->type = type;
    b->timer = 0;
    if (result & 0x80) {
        b->x = x;
        b->y = y;
    }
    if (!advance_stage) {
        b->stage = 2;
        remove_collision(env, y, x);
        broken_animation(env, b);
    } else {
        b->stage = 1;
    }
    return true;
}

/* BlockStoreBrokenNonReformBlock */
static void store_broken_non_reform(MzmBlocks *blocks, const MzmBlockEnv *env, int x, int y,
                                    uint8_t type) {
    for (int i = 0; i < MZM_MAX_BROKEN_BLOCKS; ++i) {
        MzmBrokenBlock *b = &blocks->broken[i];
        if (b->stage != 0) continue;
        *b = (MzmBrokenBlock){false, 2, type, 0, x, y};
        broken_animation(env, b);
        return;
    }
    for (int stage = 4; stage != 0; stage >>= 1)
        for (int i = 0; i < MZM_MAX_BROKEN_BLOCKS; ++i) {
            MzmBrokenBlock *b = &blocks->broken[i];
            if (b->broken || b->stage < stage) continue;
            env->set_bg1(env->context, 0, b->y, b->x);
            *b = (MzmBrokenBlock){false, 2, type, 0, x, y};
            broken_animation(env, b);
            return;
        }
}

/* BlockStoreSingleNeverReformBlock: true when the record is full. */
static bool store_never_reform(MzmBlocks *blocks, int x, int y) {
    if (x * y == 0) return false;
    if (blocks->never_reform_count >= MZM_NEVER_REFORM_BLOCKS) return true;
    blocks->never_reform[blocks->never_reform_count][0] = x;
    blocks->never_reform[blocks->never_reform_count][1] = y;
    blocks->never_reform_count++;
    return false;
}

/* BlockDestroySingleBreakableBlock and BlockDestroyBombChainBlock */
static bool destroy_single(MzmBlocks *blocks, const MzmBlockEnv *env, const ClipBlock *c) {
    store_broken_non_reform(blocks, env, c->x, c->y, env->behaviors[c->block_behavior].type);
    env->set_clip(env->context, 0, c->y, c->x);
    return true;
}

/* BlockDestroySquareBlock: the four blocks of a 2x2 shot block. */
static bool destroy_square(MzmBlocks *blocks, const MzmBlockEnv *env, ClipBlock *c) {
    uint8_t type = env->behaviors[c->block_behavior].type;
    static const int steps[4][2] = {{0, 0}, {1, 1}, {0, -1}, {-1, 1}};
    for (int i = 0; i < 4; ++i) {
        c->x += steps[i][0];
        c->y += steps[i][1];
        store_broken_non_reform(blocks, env, c->x, c->y, type);
        env->set_clip(env->context, 0, c->y, c->x);
    }
    if (blocks->caa != MZM_CAA_SPEEDBOOSTER)
        mzm_sfx_play_not_playing(MZM_SOUND_SQUARE_BLOCK_DESTROYED);
    return true;
}

/* BlockDestroyNonReformBlock */
static bool destroy_non_reform(MzmBlocks *blocks, const MzmBlockEnv *env, ClipBlock *c) {
    uint8_t sub = env->behaviors[c->block_behavior].sub_type;
    bool full = false;
    switch (sub) {
    case SUB_REFORM:
    case SUB_SQUARE_NO_REFORM:
    case SUB_BOMB_CHAIN: break;
    case SUB_NO_REFORM:
    case SUB_SQUARE_NEVER_REFORM: full = store_never_reform(blocks, c->x, c->y); break;
    default: full = true; break;
    }
    if (full) return false;
    if (sub == SUB_SQUARE_NO_REFORM || sub == SUB_SQUARE_NEVER_REFORM)
        return destroy_square(blocks, env, c);
    return destroy_single(blocks, env, c);
}

/* BlockCheckRevealBombChainBlock */
static bool reveal_bomb_chain(MzmBlocks *blocks, uint8_t type, int x, int y) {
    unsigned spawn = 0;
    for (int i = 0; i < MZM_MAX_BROKEN_BLOCKS; ++i) {
        MzmBrokenBlock *b = &blocks->broken[i];
        if (b->x == x && b->y == y) {
            spawn = 0;
            break;
        }
        if (!(spawn & 0x80) && b->stage == 0) spawn = (unsigned)i | 0x80;
    }
    if (!spawn) return false;
    blocks->broken[spawn & 0x7F] = (MzmBrokenBlock){false, 0, type, 0, x, y};
    return true;
}

/* BlockCheckRevealOrDestroyNonBombBlock */
static bool reveal_or_destroy_non_bomb(MzmBlocks *blocks, const MzmBlockEnv *env,
                                       const ClipBlock *c) {
    uint8_t type = env->behaviors[c->block_behavior].type;
    if (damage(blocks, env) & env->weakness[type]) return true;
    /* The power bomb's owner condition: power bombs are not ported. */
    if (blocks->caa != MZM_CAA_BOMB_PISTOL && blocks->caa != MZM_CAA_POWER_BOMB) return false;
    if (c->behavior != env->reforming[type]) set_block(env, env->reforming[type], c->y, c->x);
    return false;
}

/* BlockCheckRevealOrDestroyBombBlock */
static bool reveal_or_destroy_bomb(MzmBlocks *blocks, const MzmBlockEnv *env, const ClipBlock *c) {
    uint8_t type = env->behaviors[c->block_behavior].type;
    if (damage(blocks, env) & env->weakness[type]) return true;
    if (damage(blocks, env) & REVEAL_BOMB_BLOCKS && c->behavior != env->reforming[type]) {
        bool reveal = true;
        if (type > TYPE_BOMB_BLOCK_NEVER_REFORM)
            reveal = reveal_bomb_chain(blocks, type, c->x, c->y);
        if (!reveal) return false;
        set_block(env, env->reforming[type], c->y, c->x);
    }
    return false;
}

/* BlockStartBombChain */
static bool start_bomb_chain(MzmBlocks *blocks, uint8_t type, int x, int y) {
    for (int i = MZM_MAX_BOMB_CHAINS - 1; i >= 0; --i) {
        MzmBombChain *chain = &blocks->chains[i];
        if (chain->offset != 0) continue;
        *chain = (MzmBombChain){1, type, true, true, x, y};
        return true;
    }
    return false;
}

/* BlockCheckCcaa */
static bool check_ccaa(MzmBlocks *blocks, const MzmBlockEnv *env, ClipBlock *c) {
    uint16_t block = (uint16_t)(c->behavior - BLOCK_START);
    if (block > LAST_BLOCK) return false;
    c->block_behavior = block;
    const MzmBlockBehavior *behavior = &env->behaviors[block];
    if (blocks->caa == MZM_CAA_SPEEDBOOSTER_ON_GROUND) {
        if (!behavior->speedboost) return false;
        blocks->caa = MZM_CAA_SPEEDBOOSTER;
    } else if (blocks->caa == MZM_CAA_BOMB_CHAIN) {
        if (!behavior->bomb_chain) return false;
    }
    bool destroy = true;
    uint8_t chain_type = 0;
    switch (c->behavior) {
    case TOP_RIGHT_SHOT_NEVER:
    case TOP_RIGHT_SHOT_NO: c->x--; break;
    case BOTTOM_RIGHT_SHOT_NEVER:
    case BOTTOM_RIGHT_SHOT_NO:
        c->x--;
        /* fallthrough */
    case BOTTOM_LEFT_SHOT_NEVER:
    case BOTTOM_LEFT_SHOT_NO: c->y--; break;
    case BOMB_NEVER:
    case BOMB_REFORM: destroy = reveal_or_destroy_bomb(blocks, env, c); break;
    case MISSILE_NEVER:
    case MISSILE_NO:
    case SUPER_MISSILE_NEVER:
    case SUPER_MISSILE_NO:
    case SPEEDBOOST_NO:
    case SPEEDBOOST_REFORM:
    case POWER_BOMB_NEVER:
    case SCREW_NO: destroy = reveal_or_destroy_non_bomb(blocks, env, c); break;
    case CRUMBLE:
    case SLOW_CRUMBLE:
        reveal_or_destroy_non_bomb(blocks, env, c);
        destroy = false;
        break;
    default:
        if (c->behavior >= VERTICAL_CHAIN1 && c->behavior < VERTICAL_CHAIN1 + 8)
            chain_type = (uint8_t)(c->behavior - VERTICAL_CHAIN1);
        break;
    }
    if (!destroy) return false;
    switch (behavior->life_type) {
    case LIFE_NO_NEVER_REFORM: return destroy_non_reform(blocks, env, c);
    case LIFE_REFORM:
        return mzm_blocks_store_broken_reform(blocks, env, behavior->type, c->x, c->y, false);
    case LIFE_BOMB_CHAIN:
        if (!reveal_or_destroy_bomb(blocks, env, c)) return false;
        if (blocks->active_chain_types & env->chain_flag[chain_type]) return false;
        if (start_bomb_chain(blocks, chain_type, c->x, c->y)) {
            blocks->active_chain_types |= env->chain_flag[chain_type];
            /* BlockStartBombChain: the sound of the first active chain. */
            if (blocks->active_chain_types == env->chain_flag[chain_type])
                mzm_sfx_play_not_playing(MZM_SOUND_BOMB_CHAIN);
            return destroy_non_reform(blocks, env, c);
        }
        return false;
    case LIFE_TANK:
        if (damage(blocks, env) & TANK_WEAKNESS) {
            uint16_t revealed = env->tank_revealed[c->behavior - TANK_START];
            if (revealed != 0) {
                set_block(env, revealed, c->y, c->x);
                return true;
            }
        }
        return false;
    default: return false;
    }
}

/* BlockUpdateMakeSolidBlocks: both loops step two entries per iteration
 * (`--i` in the body and `i--` after it), as the native ones do. */
static bool update_make_solid(MzmBlocks *blocks, const MzmBlockEnv *env, bool make_solid, int x,
                              int y) {
    uint16_t key = (uint16_t)(((x & 0xFF) << 8) | (y & 0xFF));
    int i;
    if (!make_solid) {
        for (i = MZM_MAX_MAKE_SOLID_BLOCKS; i > 0; i--) {
            if (blocks->make_solid[--i] == key) {
                blocks->make_solid[i] = 0;
                return true;
            }
        }
        return false;
    }
    for (i = MZM_MAX_MAKE_SOLID_BLOCKS; i > 0; i--) {
        if (blocks->make_solid[--i] == key) {
            i = 0xFF;
            break;
        } else if (blocks->make_solid[i] == 0) {
            break;
        }
    }
    if (i == 0xFF || env->get_clip(env->context, y, x) != 0) return false;
    blocks->make_solid[i] = key;
    return true;
}

bool mzm_block_apply_ccaa(MzmBlocks *blocks, const MzmBlockEnv *env, int y, int x, uint16_t clip) {
    ClipBlock c = {x, y, env->behavior(env->context, clip), 0};
    switch (blocks->caa) {
    case MZM_CAA_BEAM:
    case MZM_CAA_BOMB_PISTOL:
    case MZM_CAA_MISSILE:
    case MZM_CAA_SUPER_MISSILE:
    case MZM_CAA_POWER_BOMB:
        if (env->collision(env->context, clip) == CLIPDATA_TYPE_DOOR && env->check_hatch &&
            env->check_hatch(env->context, x, y, damage(blocks, env)))
            return true;
        return check_ccaa(blocks, env, &c);
    case MZM_CAA_SPEEDBOOSTER:
    case MZM_CAA_SPEEDBOOSTER_ON_GROUND:
        if (!check_ccaa(blocks, env, &c)) return false;
        mzm_sfx_play_not_playing(MZM_SOUND_BLOCK_DESTROYED_WITH_SPEEDBOOSTER);
        return true;
    case MZM_CAA_SCREW_ATTACK:
        if (!check_ccaa(blocks, env, &c)) return false;
        mzm_sfx_play_not_playing(MZM_SOUND_BLOCK_DESTROYED_WITH_SCREW_ATTACK);
        return true;
    case MZM_CAA_BOMB_CHAIN:
    case MZM_CAA_BOMB_CHAIN_UNUSED: return check_ccaa(blocks, env, &c);
    case MZM_CAA_REMOVE_SOLID:
        if (!update_make_solid(blocks, env, false, x, y)) env->set_bg1(env->context, 0, y, x);
        env->set_clip(env->context, 0, y, x);
        return false;
    case MZM_CAA_MAKE_SOLID_GRIPPABLE:
    case MZM_CAA_MAKE_STOP_ENEMY:
    case MZM_CAA_MAKE_NON_POWER_GRIP: {
        bool result = update_make_solid(blocks, env, true, x, y);
        if (result)
            env->set_clip(env->context,
                          env->make_solid_clip[blocks->caa == MZM_CAA_MAKE_SOLID_GRIPPABLE ? 0 :
                                               blocks->caa == MZM_CAA_MAKE_STOP_ENEMY ? 1 : 2],
                          y, x);
        return result;
    }
    default: return false;
    }
}

bool mzm_blocks_affect(MzmBlocks *blocks, const MzmBlockEnv *env, uint8_t caa, int32_t x,
                       int32_t y) {
    if (x < 0 || y < 0) return false;
    int bx = x / 64, by = y / 64;
    if (bx >= env->width || by >= env->height) return false;
    blocks->caa = caa;
    bool result = mzm_block_apply_ccaa(blocks, env, by, bx, env->get_clip(env->context, by, bx));
    blocks->caa = MZM_CAA_NONE;
    return result;
}

bool mzm_blocks_samus_destruct(MzmBlocks *blocks, const MzmBlockEnv *env, int32_t x, int32_t y,
                               int action) {
    if (x < 0 || y < 0) return false;
    int bx = x / 64, by = y / 64;
    if (bx >= env->width || by >= env->height) return false;
    switch (action) {
    case MZM_DESTRUCT_SPEED: blocks->caa = MZM_CAA_SPEEDBOOSTER; break;
    case MZM_DESTRUCT_SCREW:
    case MZM_DESTRUCT_SPEED_SCREW: blocks->caa = MZM_CAA_SCREW_ATTACK; break;
    case MZM_DESTRUCT_SPEED_ON_GROUND: blocks->caa = MZM_CAA_SPEEDBOOSTER_ON_GROUND; break;
    default: return false;
    }
    uint16_t clip = env->get_clip(env->context, by, bx);
    bool result = false;            /* BUGFIX: natively undefined on air */
    if (clip != 0) {
        result = mzm_block_apply_ccaa(blocks, env, by, bx, clip);
        if (!result && action == MZM_DESTRUCT_SPEED_SCREW) {
            blocks->caa = MZM_CAA_SPEEDBOOSTER;
            mzm_block_apply_ccaa(blocks, env, by, bx, clip);
        }
    }
    blocks->caa = MZM_CAA_NONE;
    return result;
}

/* sBrokenBlocksTimers[type][stage]; stage 13 reads past the row into the
 * next type's first timer, as the native table access does. */
static uint16_t broken_timer(const MzmBlockEnv *env, uint8_t type, uint8_t stage) {
    unsigned index = (unsigned)type * 13u + stage;
    return index < MZM_BLOCK_TYPES * 13u ? env->timers[index / 13][index % 13] : 0;
}

void mzm_blocks_update_broken(MzmBlocks *blocks, const MzmBlockEnv *env) {
    for (int i = 0; i < MZM_MAX_BROKEN_BLOCKS; ++i) {
        MzmBrokenBlock *b = &blocks->broken[i];
        if (b->stage == 0) continue;
        b->timer++;
        if (b->broken) {
            if (b->timer < broken_timer(env, b->type, b->stage)) continue;
            bool update_stage = false;
            b->timer = 0;
            if (b->stage >= 13) {
                if (env->samus_in_block && env->samus_in_block(env->context, b->x, b->y)) {
                    b->stage = 2;
                } else {
                    env->set_clip(env->context, env->reforming[b->type], b->y, b->x);
                    reset_broken(b);
                }
            } else if (b->stage == 7) {
                if (env->samus_in_block && env->samus_in_block(env->context, b->x, b->y))
                    b->timer = (uint16_t)(broken_timer(env, b->type, b->stage) / 2);
                else
                    update_stage = true;
            } else {
                if (b->stage == 1) remove_collision(env, b->y, b->x);
                update_stage = true;
            }
            if (update_stage) {
                b->stage++;
                broken_animation(env, b);
            }
        } else if (b->timer >= broken_timer(env, b->type, b->stage)) {
            b->timer = 0;
            b->stage++;
            broken_animation(env, b);
            if (b->stage > 6) reset_broken(b);
        }
    }
}

/* BlockCheckStartNewSubBombChain */
static void start_sub_chain(MzmBlocks *blocks, const MzmBlockEnv *env, int request, int x, int y) {
    blocks->caa = MZM_CAA_BOMB_CHAIN;
    uint16_t clip = env->get_clip(env->context, y, x);
    if (clip != 0) mzm_block_apply_ccaa(blocks, env, y, x, clip);
    for (int i = 0; i < 2; ++i) {
        int yy = y + env->sub_chain[request][i * 2 + 1];
        int xx = x + env->sub_chain[request][i * 2 + 0];
        if (xx < 0 || yy < 0 || xx >= env->width || yy >= env->height) continue;
        clip = env->get_clip(env->context, yy, xx);
        if (clip != 0) mzm_block_apply_ccaa(blocks, env, yy, xx, clip);
    }
    blocks->caa = MZM_CAA_NONE;
}

/* One direction of a bomb chain: false when it stops. */
static bool chain_step(MzmBlocks *blocks, const MzmBlockEnv *env, ClipBlock *c, int request) {
    uint16_t clip = env->get_clip(env->context, c->y, c->x);
    if (c->behavior == env->behavior(env->context, clip)) {
        ClipBlock copy = *c;
        return destroy_non_reform(blocks, env, &copy);
    }
    start_sub_chain(blocks, env, request, c->x, c->y);
    return false;
}

void mzm_blocks_process_bomb_chains(MzmBlocks *blocks, const MzmBlockEnv *env, uint8_t frame8) {
    MzmBombChain *chain = &blocks->chains[frame8 & (MZM_MAX_BOMB_CHAINS - 1)];
    if (chain->offset == 0) return;
    bool horizontal = chain->type > CHAIN_VERTICAL4;
    ClipBlock c = {0, 0, env->chain_behavior[chain->type & 7], 0};
    c.block_behavior = (uint16_t)(c.behavior - BLOCK_START);
    if (!horizontal) {
        c.x = chain->x;
        if (chain->up_or_left) {
            c.y = (uint8_t)(chain->y - chain->offset);
            if (c.y <= 1) chain->up_or_left = false;
            else if (!chain_step(blocks, env, &c, SUB_CHAIN_UP)) chain->up_or_left = false;
        }
        if (chain->down_or_right) {
            c.y = (uint8_t)(chain->y + chain->offset);
            if (c.y >= env->height - 2) chain->down_or_right = false;
            else if (!chain_step(blocks, env, &c, SUB_CHAIN_DOWN)) chain->down_or_right = false;
        }
    } else {
        c.y = chain->y;
        if (chain->up_or_left) {
            c.x = (uint8_t)(chain->x - chain->offset);
            if (c.x <= 1) chain->up_or_left = false;
            else if (!chain_step(blocks, env, &c, SUB_CHAIN_LEFT)) chain->up_or_left = false;
        }
        if (chain->down_or_right) {
            c.x = (uint8_t)(chain->x + chain->offset);
            if (c.x >= env->width - 2) chain->down_or_right = false;
            else if (!chain_step(blocks, env, &c, SUB_CHAIN_RIGHT)) chain->down_or_right = false;
        }
    }
    if (chain->up_or_left || chain->down_or_right) {
        chain->offset++;
    } else {
        chain->offset = 0;
        blocks->active_chain_types &= (uint8_t)~env->chain_flag[chain->type & 7];
        /* CONVERT_SECONDS(1.f / 6) once no chain is active. */
        if (blocks->active_chain_types == 0) mzm_sfx_fade(MZM_SOUND_BOMB_CHAIN, 10);
    }
}

void mzm_blocks_remove_never_reform(const MzmBlockEnv *env, int x, int y) {
    uint16_t behavior = env->behavior(env->context, env->get_clip(env->context, y, x));
    env->set_clip(env->context, 0, y, x);
    env->set_bg1(env->context, 0, y, x);
    if (behavior == TOP_LEFT_SHOT_NEVER) {
        env->set_clip(env->context, 0, y, x + 1);
        env->set_bg1(env->context, 0, y, x + 1);
        env->set_clip(env->context, 0, y + 1, x);
        env->set_bg1(env->context, 0, y + 1, x);
        env->set_clip(env->context, 0, y + 1, x + 1);
        env->set_bg1(env->context, 0, y + 1, x + 1);
    }
}
