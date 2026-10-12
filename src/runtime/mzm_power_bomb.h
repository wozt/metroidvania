/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission power bomb explosion (src/power_bomb_explosion.c and the
 * power bomb parts of src/haze.c): gCurrentPowerBomb, its expanding and
 * retracting semi-minor axis, the explosion hitbox, the CAA_POWER_BOMB sweep
 * of its edges and the window spans (sHaze_PowerBomb_WindowValuesPointers)
 * the renderer uses for the explosion's look. No SDL dependency. */
#ifndef MZM_POWER_BOMB_H
#define MZM_POWER_BOMB_H

#include <stdbool.h>
#include <stdint.h>

/* PowerBombState */
enum {
    MZM_PB_NONE, MZM_PB_UNK_1, MZM_PB_UNK_2, MZM_PB_EXPLODING, MZM_PB_IMPLODING, MZM_PB_ENDING
};
/* 53 * 3: the largest semi-minor axis (screen pixels). */
#define MZM_PB_MAX_AXIS 159

typedef struct {
    uint8_t state;                  /* animationState */
    uint8_t stage;
    bool placed;                    /* powerBombPlaced */
    uint16_t x, y;                  /* subpixels */
    int16_t axis;                   /* semiMinorAxis */
    /* The axis of this frame's window (the haze draws before resizing). */
    int16_t window_axis;
    int16_t hitbox_top, hitbox_bottom, hitbox_left, hitbox_right;
} MzmPowerBomb;

/* PowerBombExplosionStart (and PowerBombExplosionBegin): the explosion
 * sound plays; false while an explosion is running. */
bool mzm_power_bomb_start(MzmPowerBomb *pb, uint16_t x, uint16_t y);
/* The power bomb cases of HazeProcess (Haze_PowerBombExpanding /
 * Retracting): grow or shrink the axis by 3 and switch state. */
void mzm_power_bomb_haze(MzmPowerBomb *pb);
/* PowerBombExplosionProcess: the hitbox, and while expanding the
 * BlockApplyCcaa(CAA_POWER_BOMB) sweep of the hitbox's left and right
 * columns (even frames) or top and bottom rows (odd frames) through
 * `apply(context, block_y, block_x)` for non-air blocks. The room is
 * `width` x `height` blocks; `clip` returns a block's Clipdata. */
typedef struct {
    void *context;
    int width, height;
    uint16_t (*clip)(void *context, int block_y, int block_x);
    void (*apply)(void *context, int block_y, int block_x);
} MzmPowerBombRoom;
void mzm_power_bomb_process(MzmPowerBomb *pb, const MzmPowerBombRoom *room, uint8_t frame8);

/* The window rows of sHaze_PowerBomb_WindowValuesPointers[axis]: for
 * each of the 2 * axis + 1 rows from the top, the left and right offsets in
 * screen pixels from the centre. Tables come from power_bomb.tsv. */
bool mzm_power_bomb_load_windows(const char *path);
bool mzm_power_bomb_windows_ready(void);
int mzm_power_bomb_window(int axis, int row, int *left, int *right);

#endif /* MZM_POWER_BOMB_H */
