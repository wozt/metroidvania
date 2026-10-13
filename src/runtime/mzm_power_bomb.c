/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission power bomb explosion; comments name the pinned
 * decompilation's routines. */
#include "mzm_power_bomb.h"
#include "mzm_screen_shake.h"
#include "mzm_sfx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { BLOCK_SIZE = 64 };

bool mzm_power_bomb_start(MzmPowerBomb *pb, uint16_t x, uint16_t y) {
    if (pb->state != MZM_PB_NONE) return false;
    pb->x = x;
    pb->y = y;
    /* PowerBombExplosionBegin (the palette backup is the renderer's). */
    pb->state = MZM_PB_EXPLODING;
    pb->placed = false;
    pb->stage = 0;
    pb->axis = 4;
    pb->hitbox_left = pb->hitbox_right = pb->hitbox_top = pb->hitbox_bottom = 0;
    mzm_screen_shake_start_horizontal(120, 0x80);  /* CONVERT_SECONDS(2.f) */
    mzm_sfx_play(MZM_SOUND_POWER_BOMB_EXPLOSION);
    return true;
}

void mzm_power_bomb_haze(MzmPowerBomb *pb) {
    pb->window_axis = pb->axis;
    if (pb->state == MZM_PB_EXPLODING) {
        /* Haze_PowerBombExpanding: five frames at the largest axis. */
        if (pb->axis >= MZM_PB_MAX_AXIS) {
            if (++pb->stage > 4) pb->state = MZM_PB_IMPLODING;
        } else {
            pb->axis += 3;
            if (pb->axis > MZM_PB_MAX_AXIS) pb->axis = MZM_PB_MAX_AXIS;
        }
    } else if (pb->state == MZM_PB_IMPLODING) {
        /* Haze_PowerBombRetracting: the stage is not reset, so the end
         * comes as soon as the axis is back to 4. */
        if (pb->axis <= 4) {
            if (++pb->stage > 4) {
                pb->state = MZM_PB_ENDING;
                pb->stage = 0;
            }
        } else {
            pb->axis -= 3;
            if (pb->axis < 4) pb->axis = 4;
        }
    }
}

static void apply(const MzmPowerBombRoom *room, int y, int x) {
    /* The native GET_CLIP_BLOCK reads outside the room at its edges; the
     * port skips those blocks. */
    if (x < 0 || y < 0 || x >= room->width || y >= room->height) return;
    if (room->clip(room->context, y, x) != 0) room->apply(room->context, y, x);
}

/* PowerBombExplosion */
static void explosion(MzmPowerBomb *pb, const MzmPowerBombRoom *room, uint8_t frame8) {
    /* Without BUGFIX: a double multiplication by .95, truncated. */
    int32_t vertical = (int32_t)(pb->axis * 4 * 0.95);
    int32_t horizontal = (int32_t)(pb->axis * 8 * 0.95);
    int32_t left = (int16_t)-horizontal, right = (int16_t)horizontal;
    int32_t top = (int16_t)-vertical, bottom = (int16_t)vertical;
    if (pb->x + left < 0) left = (int16_t)-pb->x;
    if (pb->y + top < 0) top = (int16_t)-pb->y;
    if (room->width * BLOCK_SIZE < pb->x + right) right = (int16_t)(room->width * BLOCK_SIZE - pb->x);
    if (room->height * BLOCK_SIZE < pb->y + bottom)
        bottom = (int16_t)(room->height * BLOCK_SIZE - pb->y);
    pb->hitbox_left = (int16_t)left;
    pb->hitbox_right = (int16_t)right;
    pb->hitbox_top = (int16_t)top;
    pb->hitbox_bottom = (int16_t)bottom;
    if (pb->state >= MZM_PB_IMPLODING) return;
    left = (left + pb->x) >> 6;
    right = (right + pb->x) >> 6;
    top = (top + pb->y) >> 6;
    bottom = (bottom + pb->y) >> 6;
    if ((frame8 & 1) == 0) {
        /* The left and right columns, from the centre row outward (the
         * centre block twice). */
        for (int side = 0; side < 2; ++side) {
            int x = side == 0 ? left : right;
            int up = pb->y / BLOCK_SIZE, down = up;
            for (int done = 0; done != 2;) {
                done = 0;
                if (up >= top) apply(room, up--, x);
                else done++;
                if (down <= bottom) apply(room, down++, x);
                else done++;
            }
        }
    } else {
        /* The top and bottom rows, from the centre column outward. */
        for (int side = 0; side < 2; ++side) {
            int y = side == 0 ? top : bottom;
            int right_x = pb->x / BLOCK_SIZE, left_x = right_x;
            for (int done = 0; done != 2;) {
                done = 0;
                if (left_x >= left) apply(room, y, left_x--);
                else done++;
                if (right_x <= right) apply(room, y, right_x++);
                else done++;
            }
        }
    }
}

void mzm_power_bomb_process(MzmPowerBomb *pb, const MzmPowerBombRoom *room, uint8_t frame8) {
    if (pb->state <= MZM_PB_UNK_1) return;
    if (pb->state == MZM_PB_ENDING) {
        /* PowerBombExplosionEnd: the register restore, the blend fade (one
         * frame here, as the runtime has no blend registers), the end. */
        if (pb->stage < 2) {
            pb->stage++;
        } else {
            pb->state = MZM_PB_NONE;
            pb->stage = 0;
        }
        return;
    }
    if (pb->state == MZM_PB_UNK_2) return;
    explosion(pb, room, frame8);
}

/* sHaze_PowerBomb_WindowValuesPointers: 161 tables of (right, left) pairs. */
static int16_t *g_windows[MZM_PB_MAX_AXIS + 2];
static int g_window_rows[MZM_PB_MAX_AXIS + 2];

bool mzm_power_bomb_load_windows(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    static char line[8192];
    int loaded = 0;
    bool ok = true;
    while (ok && fgets(line, sizeof line, f)) {
        int axis, offset;
        if (sscanf(line, "window\t%d\t%n", &axis, &offset) != 1) continue;
        if (axis < 0 || axis > MZM_PB_MAX_AXIS + 1) { ok = false; break; }
        int count = 1;
        for (const char *c = line + offset; *c; ++c) count += *c == ',';
        free(g_windows[axis]);
        g_windows[axis] = malloc(sizeof(int16_t) * (size_t)count);
        if (!g_windows[axis]) { ok = false; break; }
        const char *cursor = line + offset;
        for (int i = 0; i < count; ++i) {
            char *end = NULL;
            g_windows[axis][i] = (int16_t)strtol(cursor, &end, 10);
            if (end == cursor) { ok = false; break; }
            cursor = *end == ',' ? end + 1 : end;
        }
        g_window_rows[axis] = count / 2;
        loaded++;
    }
    fclose(f);
    return ok && loaded == MZM_PB_MAX_AXIS + 2;
}

bool mzm_power_bomb_windows_ready(void) {
    return g_windows[MZM_PB_MAX_AXIS] != NULL;
}

int mzm_power_bomb_window(int axis, int row, int *left, int *right) {
    if (axis < 0 || axis > MZM_PB_MAX_AXIS + 1 || !g_windows[axis] || row < 0 ||
        row >= g_window_rows[axis])
        return 0;
    /* Haze_PowerBomb*: left = x + src[row * 2 + 1] * 2, right = x + src[row * 2] * 2. */
    *left = g_windows[axis][row * 2 + 1] * 2;
    *right = g_windows[axis][row * 2] * 2;
    return 1;
}
