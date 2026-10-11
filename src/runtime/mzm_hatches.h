/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission hatches (no SDL): src/connection.c (ConnectionUpdateHatches,
 * ConnectionUpdateHatchAnimation, ConnectionHatchFlashingAnimation,
 * ConnectionHatchStartLockAnimation, ConnectionOverrideOpenedHatch) and
 * BgClipCheckOpeningHatch (src/bg_clip.c). A hatch changes four BG1 blocks
 * and their Clipdata through the environment's setters; the ROM tables
 * (sHatchesTilemapValues, sHatchesAnimationDurationCounter, sHatchBehaviors)
 * come from the room export. Sounds are not played. */
#ifndef MZM_HATCHES_H
#define MZM_HATCHES_H

#include <stdbool.h>
#include <stdint.h>

#define MZM_MAX_HATCHES 16          /* MAX_AMOUNT_OF_HATCHES */
#define MZM_HATCH_VERTICAL_SIZE 4

/* HatchType */
enum {
    MZM_HATCH_NONE, MZM_HATCH_UNUSED, MZM_HATCH_NORMAL, MZM_HATCH_MISSILE,
    MZM_HATCH_SUPER_MISSILE, MZM_HATCH_POWER_BOMB, MZM_HATCH_LOCKED,
    MZM_HATCH_LOCKED_NAVIGATION, MZM_HATCH_COUNT,
};

/* HatchState */
enum { MZM_HATCH_CLOSED, MZM_HATCH_OPENING, MZM_HATCH_OPENED, MZM_HATCH_CLOSING };

/* HatchOpeningAction */
enum { MZM_HATCH_NOT_OPENING, MZM_HATCH_ACTION_OPENING, MZM_HATCH_ACTION_LOCKED };

typedef struct {
    bool exists, facing_right;
    uint8_t type, state, hits, hit_timer, flashing_timer, frame, adc;
    int x, y;                       /* the hatch's top block */
    int source_door;
} MzmHatch;

typedef struct {
    uint16_t tilemap_values[MZM_HATCH_COUNT];   /* sHatchesTilemapValues */
    uint8_t durations[6];                       /* sHatchesAnimationDurationCounter */
    uint8_t weakness[MZM_HATCH_COUNT];          /* sHatchBehaviors (damage bits) */
    uint8_t health[MZM_HATCH_COUNT];
    bool unlocking, navigation_unlocking;       /* gHatchesState */
    void *context;
    /* BgClipSetBg1BlockValue and BgClipSetClipdataBlockValue. */
    void (*set_block)(void *context, uint16_t value, int y, int x);
} MzmHatchEnv;

/* ConnectionUpdateHatchAnimation */
void mzm_hatch_update_animation(MzmHatch *hatch, const MzmHatchEnv *env);
/* ConnectionHatchStartLockAnimation */
void mzm_hatch_start_lock_animation(MzmHatch *hatch, const MzmHatchEnv *env, uint8_t state);
/* ConnectionOverrideOpenedHatch */
void mzm_hatch_override(MzmHatch *hatch, const MzmHatchEnv *env, uint8_t type);
/* BgClipCheckOpeningHatch for a block hit by a clipdata-affecting action of
 * the given damage bits; `opened` receives the hatch that starts opening
 * (NULL when none). */
int mzm_hatch_check_opening(MzmHatch *hatches, int count, const MzmHatchEnv *env, int x, int y,
                            unsigned damage, MzmHatch **opened);
/* ConnectionUpdateHatches */
void mzm_hatches_update(MzmHatch *hatches, int count, const MzmHatchEnv *env);

#endif /* MZM_HATCHES_H */
