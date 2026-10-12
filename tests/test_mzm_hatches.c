/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free tests of the Zero Mission hatch state machine. */
#include "mzm_hatches.h"
#include "mzm_sfx.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint16_t column[64][4];      /* written values by y, x 0..3 */
static int writes;

static void set_block(void *context, uint16_t value, int y, int x) {
    (void)context;
    assert(y >= 0 && y < 64 && x >= 0 && x < 4);
    column[y][x] = value;
    ++writes;
}

int main(void) {
    MzmHatchEnv env;
    memset(&env, 0, sizeof env);
    for (int i = 0; i < MZM_HATCH_COUNT; ++i) {
        env.tilemap_values[i] = (uint16_t)(0x400 + 0x80 + 2 * i);
        env.weakness[i] = 1;
    }
    env.weakness[MZM_HATCH_MISSILE] = 4 | 8;
    for (int i = 0; i < 5; ++i) env.durations[i] = 3;
    env.durations[5] = 255;
    env.set_block = set_block;
    MzmHatch hatches[2] = {
        {.exists = true, .type = MZM_HATCH_NORMAL, .x = 1, .y = 10, .facing_right = false},
        {.exists = true, .type = MZM_HATCH_MISSILE, .x = 2, .y = 20, .facing_right = true},
    };

    /* A beam misses the missile hatch and opens the normal one. */
    MzmHatch *opened = NULL;
    assert(mzm_hatch_check_opening(hatches, 2, &env, 2, 21, 1, &opened) == MZM_HATCH_NOT_OPENING);
    assert(mzm_hatch_check_opening(hatches, 2, &env, 1, 13, 1, &opened) ==
           MZM_HATCH_ACTION_OPENING);
    assert(opened == &hatches[0] && hatches[0].state == MZM_HATCH_OPENING);
    /* Frame 0 starts the animation; then each frame waits 3 + 1 ticks and
     * writes CLIPDATA_TILEMAP_FLAG | 0x11 + (frame - 1) down the column. */
    mzm_sfx_clear();
    mzm_hatches_update(hatches, 2, &env);
    assert(writes == 0 && hatches[0].frame == 1);
    assert(mzm_sfx_count() == 1 && mzm_sfx_get(0).sound == MZM_SOUND_DOOR_OPENING);
    for (int i = 0; i < 4; ++i) mzm_hatches_update(hatches, 2, &env);
    assert(writes == 4 && column[10][1] == 0x411 && column[11][1] == 0x421 &&
           column[13][1] == 0x441);
    for (int i = 0; i < 12; ++i) mzm_hatches_update(hatches, 2, &env);
    assert(column[10][1] == 0x414 && hatches[0].state == MZM_HATCH_OPENED);

    /* A super missile opens a missile hatch at once (facing right: 0x416). */
    writes = 0;
    assert(mzm_hatch_check_opening(hatches, 2, &env, 2, 20, 8, &opened) ==
           MZM_HATCH_ACTION_OPENING);
    assert(opened == &hatches[1]);
    for (int i = 0; i < 5; ++i) mzm_hatches_update(hatches, 2, &env);
    assert(column[20][2] == 0x416);

    /* Closing behind Samus: frames 3, 2, 1 (+0x40 for a real hatch), then
     * the closed hatch value of the type, plus one when facing right. */
    MzmHatch door = {.exists = true, .type = MZM_HATCH_NORMAL, .x = 3, .y = 30,
                     .facing_right = true};
    mzm_hatch_start_lock_animation(&door, &env, MZM_HATCH_CLOSING);
    assert(column[30][3] == 0x416 + 3 + 0x40);
    mzm_sfx_clear();
    mzm_hatches_update(&door, 1, &env);
    assert(mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_DOOR_CLOSING));
    for (int i = 0; i < 16; ++i) mzm_hatches_update(&door, 1, &env);
    assert(door.state == MZM_HATCH_CLOSED &&
           column[30][3] == env.tilemap_values[MZM_HATCH_NORMAL] + 1);

    /* A hatch opened before comes back as a normal closed hatch. */
    MzmHatch kept = {.exists = true, .type = MZM_HATCH_MISSILE, .x = 0, .y = 40};
    mzm_hatch_override(&kept, &env, MZM_HATCH_NORMAL);
    assert(kept.type == MZM_HATCH_NORMAL && kept.state == MZM_HATCH_CLOSED &&
           column[40][0] == env.tilemap_values[MZM_HATCH_NORMAL] &&
           column[43][0] == env.tilemap_values[MZM_HATCH_NORMAL] + 48);

    /* A locked hatch stops the shot and stays shut. */
    MzmHatch locked = {.exists = true, .type = MZM_HATCH_LOCKED, .x = 0, .y = 50};
    assert(mzm_hatch_check_opening(&locked, 1, &env, 0, 51, 1, &opened) ==
           MZM_HATCH_ACTION_LOCKED);
    assert(locked.state == MZM_HATCH_CLOSED && locked.hits == 0 && opened == NULL);
    puts("mzm_hatches: ok");
    return 0;
}
