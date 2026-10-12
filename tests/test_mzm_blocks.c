/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free tests of the Zero Mission breakable blocks. */
#include "mzm_blocks.h"
#include "mzm_sfx.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { W = 24, H = 16, FLAG = 0x400 };
/* Behaviors of the raw values used here (ClipBehavior). */
enum {
    SHOT_NEVER = 16, TOP_LEFT_NEVER = 17, TOP_RIGHT_NEVER = 18, BOTTOM_LEFT_NEVER = 19,
    BOTTOM_RIGHT_NEVER = 20, SHOT_NO = 21, SHOT_REFORM = 26, CHAIN_H1 = 42,
    HIDDEN_MISSILE_TANK = 53,
};
static uint16_t clip[H][W], bg1[H][W];
static int hatch_checks;
static bool samus_inside;

static uint16_t get_clip(void *c, int y, int x) {
    (void)c;
    return x >= 0 && y >= 0 && x < W && y < H ? clip[y][x] : 0;
}
/* Raw values are the behaviors themselves here; FLAG values are air. */
static uint16_t behavior(void *c, uint16_t raw) {
    (void)c;
    return raw & FLAG ? 0 : raw;
}
static uint8_t collision(void *c, uint16_t raw) {
    (void)c;
    return raw == 11 ? 11 : raw ? 1 : 0;
}
static void set_clip(void *c, uint16_t raw, int y, int x) {
    (void)c;
    if (x >= 0 && y >= 0 && x < W && y < H) clip[y][x] = raw;
}
static void set_bg1(void *c, uint16_t value, int y, int x) {
    (void)c;
    if (x >= 0 && y >= 0 && x < W && y < H) bg1[y][x] = value;
}
static bool check_hatch(void *c, int x, int y, unsigned damage) {
    (void)c;
    (void)x;
    (void)y;
    (void)damage;
    ++hatch_checks;
    return true;
}
static bool samus_in(void *c, int x, int y) {
    (void)c;
    (void)x;
    (void)y;
    return samus_inside;
}

static void set_behavior(MzmBlockEnv *env, int behavior, uint8_t life, uint8_t sub, uint8_t type,
                         uint8_t chain) {
    env->behaviors[behavior - 16] = (MzmBlockBehavior){life, sub, type, 0, chain};
}

int main(void) {
    static MzmBlockEnv env;
    static MzmBlocks blocks;
    memset(&env, 0, sizeof env);
    const uint16_t damage[MZM_CAA_COUNT] = {0, 0, 0, 0, 0, 0, 0, 1, 2, 4, 8, 16, 32, 64, 128, 4096};
    memcpy(env.caa_damage, damage, sizeof damage);
    set_behavior(&env, SHOT_NEVER, 1, 2, 0, 0);
    for (int b = TOP_LEFT_NEVER; b <= BOTTOM_RIGHT_NEVER; ++b) set_behavior(&env, b, 1, 3, 0, 0);
    set_behavior(&env, SHOT_NO, 1, 0, 0, 0);
    set_behavior(&env, SHOT_REFORM, 2, 0, 1, 0);
    set_behavior(&env, CHAIN_H1, 3, 4, 18, 1);
    set_behavior(&env, HIDDEN_MISSILE_TANK, 4, 0, 0, 0);
    env.weakness[1] = 0xFFFF;
    env.weakness[18] = 4096 | 16 | 2;
    for (int t = 0; t < MZM_BLOCK_TYPES; ++t) env.reforming[t] = (uint16_t)(FLAG + 5 + t);
    env.reforming[0] = FLAG;
    const uint8_t none_timers[13] = {0, 0, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0};
    const uint8_t shot_timers[13] = {0, 0, 4, 4, 4, 4, 4, 255, 4, 4, 4, 4, 4};
    memcpy(env.timers[0], none_timers, 13);
    memcpy(env.timers[1], shot_timers, 13);
    env.tank_revealed[HIDDEN_MISSILE_TANK - 52] = FLAG | 0x1D;
    env.chain_flag[4] = 16;
    env.chain_behavior[4] = CHAIN_H1;
    const int8_t sub[4][4] = {{1, 1, -1, 1}, {1, -1, -1, -1}, {1, -1, 1, 1}, {-1, -1, -1, 1}};
    memcpy(env.sub_chain, sub, sizeof sub);
    env.width = W;
    env.height = H;
    env.get_clip = get_clip;
    env.behavior = behavior;
    env.collision = collision;
    env.set_clip = set_clip;
    env.set_bg1 = set_bg1;
    env.check_hatch = check_hatch;
    env.samus_in_block = samus_in;

    /* A beam breaks a shot block that does not reform: the Clipdata goes,
     * BG1 plays the breaking frames and ends on the common air block. */
    clip[5][5] = SHOT_NO;
    assert(mzm_blocks_affect(&blocks, &env, MZM_CAA_BEAM, 5 * 64 + 10, 5 * 64 + 10));
    assert(clip[5][5] == 0 && bg1[5][5] == FLAG && blocks.caa == MZM_CAA_NONE);
    for (int i = 0; i < 5; ++i) mzm_blocks_update_broken(&blocks, &env);
    assert(bg1[5][5] == (FLAG | 1));        /* stage 3: the solid frame */
    for (int i = 0; i < 40; ++i) mzm_blocks_update_broken(&blocks, &env);
    assert(bg1[5][5] == FLAG && blocks.broken[0].stage == 0);

    /* A reforming shot block comes back after its 255-frame wait, later
     * while Samus stands in it. */
    clip[6][6] = SHOT_REFORM;
    assert(mzm_blocks_affect(&blocks, &env, MZM_CAA_MISSILE, 6 * 64, 6 * 64));
    assert(clip[6][6] == 0 && bg1[6][6] == env.reforming[1]);
    int frames = 0;
    while (clip[6][6] == 0 && frames < 1000) {
        mzm_blocks_update_broken(&blocks, &env);
        ++frames;
    }
    /* A stage advances when its timer reaches the duration: stages 2-6 (4
     * frames each), 7 (255), 8-12 (4 each), then the row overrun reads the
     * next type's first timer (0): the reform on the next frame. */
    assert(clip[6][6] == env.reforming[1] && frames == 5 * 4 + 255 + 5 * 4 + 1);
    clip[6][6] = SHOT_REFORM;
    assert(mzm_blocks_affect(&blocks, &env, MZM_CAA_BEAM, 6 * 64, 6 * 64));
    samus_inside = true;
    for (int i = 0; i < 5 * 4 + 255 + 10; ++i) mzm_blocks_update_broken(&blocks, &env);
    assert(clip[6][6] == 0);                /* held at stage 7 */
    samus_inside = false;

    /* A 2x2 never-reform block hit on its top right: all four go, and the
     * top left is remembered. */
    clip[8][10] = TOP_LEFT_NEVER;
    clip[8][11] = TOP_RIGHT_NEVER;
    clip[9][10] = BOTTOM_LEFT_NEVER;
    clip[9][11] = BOTTOM_RIGHT_NEVER;
    mzm_sfx_clear();
    assert(mzm_blocks_affect(&blocks, &env, MZM_CAA_BEAM, 11 * 64, 8 * 64));
    assert(!clip[8][10] && !clip[8][11] && !clip[9][10] && !clip[9][11]);
    assert(mzm_sfx_requested(MZM_SFX_PLAY_NOT_PLAYING, MZM_SOUND_SQUARE_BLOCK_DESTROYED));
    assert(blocks.never_reform_count == 1 && blocks.never_reform[0][0] == 10 &&
           blocks.never_reform[0][1] == 8);
    /* BlockRemoveNeverReformSingleBlock clears a 2x2 from its top left. */
    clip[2][2] = TOP_LEFT_NEVER;
    clip[2][3] = clip[3][2] = clip[3][3] = 1;
    mzm_blocks_remove_never_reform(&env, 2, 2);
    assert(!clip[2][2] && !clip[2][3] && !clip[3][2] && !clip[3][3]);

    /* A missile reveals a hidden missile tank. */
    clip[3][12] = HIDDEN_MISSILE_TANK;
    assert(mzm_blocks_affect(&blocks, &env, MZM_CAA_MISSILE, 12 * 64, 3 * 64));
    assert(clip[3][12] == (FLAG | 0x1D) && bg1[3][12] == (FLAG | 0x1D));

    /* Door Clipdata goes to the hatches first. */
    clip[4][1] = 11;
    assert(mzm_blocks_affect(&blocks, &env, MZM_CAA_BEAM, 64, 4 * 64) && hatch_checks == 1);

    /* A pistol shot on a horizontal bomb chain destroys it and the chain
     * runs both ways, one chain step per four frames. */
    for (int x = 14; x <= 19; ++x) clip[12][x] = CHAIN_H1;
    mzm_sfx_clear();
    assert(mzm_blocks_affect(&blocks, &env, MZM_CAA_BOMB_PISTOL, 16 * 64, 12 * 64));
    assert(clip[12][16] == 0 && blocks.active_chain_types == 16);
    assert(mzm_sfx_requested(MZM_SFX_PLAY_NOT_PLAYING, MZM_SOUND_BOMB_CHAIN));
    for (int frame = 0; frame < 64; ++frame)
        mzm_blocks_process_bomb_chains(&blocks, &env, (uint8_t)frame);
    for (int x = 14; x <= 19; ++x) assert(clip[12][x] == 0);
    assert(blocks.active_chain_types == 0);
    assert(mzm_sfx_requested(MZM_SFX_FADE, MZM_SOUND_BOMB_CHAIN));

    /* The screw attack breaks a reforming shot block (weak to every
     * action). */
    clip[1][20] = SHOT_REFORM;
    mzm_sfx_clear();
    assert(mzm_blocks_samus_destruct(&blocks, &env, 20 * 64, 64, MZM_DESTRUCT_SCREW));
    assert(clip[1][20] == 0);
    assert(mzm_sfx_requested(MZM_SFX_PLAY_NOT_PLAYING,
                             MZM_SOUND_BLOCK_DESTROYED_WITH_SCREW_ATTACK));
    /* BlockUpdateMakeSolidBlocks: CAA_MAKE_NON_POWER_GRIP stores an air
     * block (at the odd entries its double step reaches) and writes its
     * value; again, nothing changes; CAA_REMOVE_SOLID then empties the
     * Clipdata but keeps the BG1 block of a block it had made solid. */
    env.make_solid_clip[0] = 0x401;
    env.make_solid_clip[2] = 0x40A;
    clip[14][2] = 0;
    bg1[14][2] = 77;
    assert(mzm_blocks_affect(&blocks, &env, MZM_CAA_MAKE_NON_POWER_GRIP, 2 * 64, 14 * 64));
    assert(clip[14][2] == 0x40A && blocks.make_solid[63] == ((2 << 8) | 14));
    assert(!mzm_blocks_affect(&blocks, &env, MZM_CAA_MAKE_SOLID_GRIPPABLE, 2 * 64, 14 * 64));
    clip[14][3] = 0;
    assert(mzm_blocks_affect(&blocks, &env, MZM_CAA_MAKE_SOLID_GRIPPABLE, 3 * 64, 14 * 64));
    assert(clip[14][3] == 0x401 && blocks.make_solid[61] == ((3 << 8) | 14));
    mzm_blocks_affect(&blocks, &env, MZM_CAA_REMOVE_SOLID, 2 * 64, 14 * 64);
    assert(clip[14][2] == 0 && bg1[14][2] == 77 && blocks.make_solid[63] == 0);
    /* A block it did not make solid loses its BG1 block too. */
    clip[14][4] = 0x10;
    bg1[14][4] = 78;
    mzm_blocks_affect(&blocks, &env, MZM_CAA_REMOVE_SOLID, 4 * 64, 14 * 64);
    assert(clip[14][4] == 0 && bg1[14][4] == 0);
    puts("mzm_blocks: ok");
    return 0;
}
