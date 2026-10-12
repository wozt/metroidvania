/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free regression tests for the Zero Mission power bomb explosion. */
#include "mzm_power_bomb.h"
#include "mzm_sfx.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { W = 40, H = 30 };
static uint16_t grid[H][W];
static int applied[H][W];
static int calls;

static uint16_t get_clip(void *context, int y, int x) {
    (void)context;
    return grid[y][x];
}

static void apply(void *context, int y, int x) {
    (void)context;
    applied[y][x]++;
    calls++;
}

int main(void) {
    MzmPowerBombRoom room = {NULL, W, H, get_clip, apply};
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) grid[y][x] = 1;
    MzmPowerBomb pb;
    memset(&pb, 0, sizeof pb);
    mzm_sfx_clear();
    /* PowerBombExplosionBegin at block (20, 15). */
    assert(mzm_power_bomb_start(&pb, 20 * 64 + 32, 15 * 64 + 32));
    assert(pb.state == MZM_PB_EXPLODING && pb.axis == 4);
    assert(mzm_sfx_requested(MZM_SFX_PLAY, MZM_SOUND_POWER_BOMB_EXPLOSION));
    assert(!mzm_power_bomb_start(&pb, 0, 0));

    /* One frame: the haze grows the axis to 7, then the hitbox is
     * axis * 4 * .95 by axis * 8 * .95 subpixels. */
    mzm_power_bomb_haze(&pb);
    assert(pb.axis == 7 && pb.window_axis == 4);
    mzm_power_bomb_process(&pb, &room, 0);
    assert(pb.hitbox_top == -26 && pb.hitbox_bottom == 26 && pb.hitbox_left == -53 &&
           pb.hitbox_right == 53);
    /* Even frame: the left and right columns (blocks 19 and 21) from the
     * centre row, which is applied twice (the hitbox spans one row). */
    assert(applied[15][19] == 2 && applied[15][21] == 2 && applied[14][19] == 0 &&
           calls == 4 && applied[15][20] == 0);
    memset(applied, 0, sizeof applied);
    calls = 0;
    /* Odd frame: the top and bottom rows. */
    mzm_power_bomb_haze(&pb);
    mzm_power_bomb_process(&pb, &room, 1);
    assert(pb.axis == 10 && applied[14][20] == 2 && applied[16][20] == 2 &&
           applied[14][19] == 1 && applied[16][21] == 1 && calls == 8);
    /* Air blocks are skipped. */
    memset(applied, 0, sizeof applied);
    calls = 0;
    grid[14][20] = 0;
    mzm_power_bomb_process(&pb, &room, 1);
    assert(applied[14][20] == 0 && applied[16][20] == 2);

    /* Expanding to 159 (3 a frame), five more frames, then imploding with
     * no block sweep, back to 4, then the end. */
    int frames = 2;
    while (pb.state == MZM_PB_EXPLODING) {
        mzm_power_bomb_haze(&pb);
        mzm_power_bomb_process(&pb, &room, (uint8_t)frames);
        frames++;
    }
    assert(pb.axis == MZM_PB_MAX_AXIS && frames == 2 + 50 + 5);
    /* The hitbox is clamped to the room. */
    assert(pb.x + pb.hitbox_right <= W * 64 && pb.y + pb.hitbox_bottom <= H * 64 &&
           pb.x + pb.hitbox_left >= 0 && pb.y + pb.hitbox_top >= 0);
    memset(applied, 0, sizeof applied);
    calls = 0;
    int implode = 0;
    while (pb.state == MZM_PB_IMPLODING) {
        mzm_power_bomb_haze(&pb);
        mzm_power_bomb_process(&pb, &room, (uint8_t)implode);
        implode++;
    }
    assert(calls == 0 && pb.axis == 4 && pb.state == MZM_PB_ENDING);
    /* 52 frames back to 4; the stage is not reset after expanding, so the
     * next frame ends it, and PowerBombExplosionEnd starts that frame. */
    assert(implode == 53);
    mzm_power_bomb_process(&pb, &room, 0);
    assert(pb.state == MZM_PB_ENDING);
    mzm_power_bomb_process(&pb, &room, 0);
    assert(pb.state == MZM_PB_NONE);
    assert(mzm_power_bomb_start(&pb, 64, 64));

    /* The window table loader. */
    const char *path = "mzm_power_bomb_test.tsv";
    FILE *f = fopen(path, "w");
    assert(f);
    fputs("schema\tmetroidvania-mzm-power-bomb-v1\n", f);
    for (int axis = 0; axis <= MZM_PB_MAX_AXIS + 1; ++axis) {
        int size = axis < 4 ? 4 : axis;
        fprintf(f, "window\t%d\t", axis);
        for (int row = 0; row < 2 * size + 1; ++row)
            fprintf(f, row ? ",%d,%d" : "%d,%d", row, -row);
        fputc('\n', f);
    }
    fclose(f);
    assert(mzm_power_bomb_load_windows(path) && mzm_power_bomb_windows_ready());
    int left = 0, right = 0;
    assert(mzm_power_bomb_window(10, 3, &left, &right) && left == -6 && right == 6);
    assert(!mzm_power_bomb_window(10, 21, &left, &right));
    remove(path);
    puts("mzm_power_bomb: ok");
    return 0;
}
