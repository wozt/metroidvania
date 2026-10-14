/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission tank pickups (no SDL): the tank part of
 * BgClipCheckTouchingTransitionOrTank (src/bg_clip.c), BgClipFinishCollectingTank,
 * BgClipSetItemAsCollected and BgClipRemoveCollectedTanks. The tables
 * (sTankBehaviors, sStartingHealthAmmo, sNumberOfTanksPerArea's game total,
 * sTankIncreaseAmount[DIFF_NORMAL]) come from the room export. */
#ifndef MZM_ITEMS_H
#define MZM_ITEMS_H

#include "mzm_blocks.h"
#include "mzm_samus.h"

#include <stdbool.h>
#include <stdint.h>

#define MZM_TANK_BEHAVIOR_START 52          /* CLIP_BEHAVIOR_TANK_START */
#define MZM_MAX_ITEMS_PER_AREA 64           /* MAX_AMOUNT_OF_ITEMS_PER_AREA */
#define MZM_MAX_AREAS 8                     /* MAX_AMOUNT_OF_AREAS */
/* SAMUS_ITEM_PMT: CONVERT_SECONDS(16.f) + ONE_THIRD_SECOND * 2 */
#define MZM_SAMUS_ITEM_PMT 1000

/* ItemType */
enum {
    MZM_ITEM_TYPE_NONE, MZM_ITEM_TYPE_MISSILE, MZM_ITEM_TYPE_ENERGY,
    MZM_ITEM_TYPE_SUPER_MISSILE, MZM_ITEM_TYPE_POWER_BOMB,
    MZM_ITEM_TYPE_ABILITY = 1 << 7
};

/* TankList columns */
enum { MZM_TANK_ENERGY, MZM_TANK_MISSILE, MZM_TANK_SUPER_MISSILE, MZM_TANK_POWER_BOMB };

typedef struct {
    /* sTankBehaviors by BEHAVIOR_TO_TANK: itemType, underwater, messageId
     * (-1: MESSAGE_NONE). */
    uint8_t item_type[MZM_TANK_BEHAVIORS];
    bool underwater[MZM_TANK_BEHAVIORS];
    int16_t message[MZM_TANK_BEHAVIORS];
    /* sStartingHealthAmmo, the game total of sNumberOfTanksPerArea and
     * sTankIncreaseAmount[DIFF_NORMAL]: energy, missile, super missile,
     * power bomb. */
    int start[4], total[4], increase[4];
    /* sTankIncreaseAmount by difficulty (DIFF_EASY, _NORMAL, _HARD), when
     * exported; mzm_tank_select_difficulty copies one into `increase`. */
    int difficulty_increase[3][4];
    bool has_difficulty[3];
    /* CLIPDATA_TILEMAP_AIR and CLIPDATA_TILEMAP_FLAG | CLIPDATA_TILEMAP_WATER. */
    uint16_t air_clip, water_clip;
} MzmTankTables;

/* sTankIncreaseAmount[gDifficulty] for the next tanks (the normal row
 * stays without the difficulty's row). */
void mzm_tank_select_difficulty(MzmTankTables *tables, int difficulty);

/* gCollectingTank and gLastTankCollected */
typedef struct {
    bool collecting;
    uint16_t behavior;
    int x, y;                   /* blocks */
} MzmTankCollection;

/* The touch probes of BgClipCheckTouchingTransitionOrTank: the clip
 * behaviors at (right, center), (left, center), (center, bottom),
 * (center, top) and their block positions. */
typedef struct {
    uint16_t behaviors[4];
    int x[4], y[4];
} MzmTouchProbes;

/* The tank loop of BgClipCheckTouchingTransitionOrTank, last probe first:
 * a revealed tank starts the collection, raises the equipment (within the
 * game total; the first missile, super missile or power bomb tank uses the
 * next message) and returns the message the banner shows, or -1. A tank
 * is only taken on an explored minimap tile (MinimapCheckIsTileExplored,
 * `explored` at the probe's block; NULL: every tile is explored). */
int mzm_tank_touch(const MzmTankTables *tables, MzmTankCollection *collection,
                   const MzmTouchProbes *probes, MzmEquipment *equipment,
                   bool (*explored)(void *context, int x, int y), void *context);

/* gItemsCollected: the room, item type and block of every collected item,
 * by area. */
typedef struct {
    struct { uint8_t room, type, x, y; } items[MZM_MAX_AREAS][MZM_MAX_ITEMS_PER_AREA];
    int count[MZM_MAX_AREAS];
} MzmCollectedItems;

/* BgClipSetItemAsCollected */
void mzm_items_set_collected(MzmCollectedItems *items, uint8_t area, uint8_t room, int x, int y,
                             uint8_t type);

/* The block a tank leaves (BgClipFinishCollectingTank and
 * BgClipRemoveCollectedTanks): water for the underwater tank behaviors. */
uint16_t mzm_tank_cleared_clip(const MzmTankTables *tables, uint16_t behavior);
/* The item type of a tank behavior (ITEM_TYPE_NONE for any other). */
uint8_t mzm_tank_item_type(const MzmTankTables *tables, uint16_t behavior);

#endif /* MZM_ITEMS_H */
