/* SPDX-License-Identifier: GPL-3.0-only */
/* The Zero Mission save file of the runtime: what SramSaveFile keeps of a
 * game (the room, Samus, gEquipment, the events, the minimap progress, the
 * collected items, the opened hatches and the destroyed never-reform
 * blocks), written as a versioned little-endian file with a checksum. The
 * GBA's SRAM layout (struct SaveFile, its copies and checksums) is not
 * reproduced: the file belongs to this runtime. No SDL dependency. */
#ifndef MZM_SAVE_H
#define MZM_SAVE_H

#include <stdbool.h>
#include <stdint.h>

#include "mzm_hud.h"
#include "mzm_items.h"
#include "mzm_samus.h"
#include "mzm_time_attack.h"

enum {
    MZM_SAVE_ALIAS = 48,
    MZM_SAVE_EVENTS = 32,
    MZM_SAVE_HATCH_AREAS = 10,
    MZM_SAVE_HATCH_DESTINATIONS = 128,
    MZM_SAVE_NEVER_REFORM = 1024,
};

/* A block of a room, by the room's alias. */
typedef struct {
    char alias[MZM_SAVE_ALIAS];
    int32_t x, y;
} MzmSaveRoomBlock;

typedef struct {
    char room[MZM_SAVE_ALIAS];      /* the room's alias ("brinstar_032") */
    int32_t samus_x, samus_y;       /* the runtime's Samus position */
    int8_t facing;
    MzmEquipment equipment;
    uint8_t events[MZM_SAVE_EVENTS];
    MzmMinimapProgress minimap;
    MzmCollectedItems items;
    uint32_t hatches_opened[MZM_SAVE_HATCH_AREAS][8];
    MzmSaveRoomBlock hatch_destinations[MZM_SAVE_HATCH_DESTINATIONS];
    int32_t hatch_destination_count;
    MzmSaveRoomBlock never_reform[MZM_SAVE_NEVER_REFORM];
    int32_t never_reform_count;
    bool escaped_zebes;
    /* Version 2: what the file select screen shows (struct SaveFileInfo):
     * gCurrentArea, gDifficulty, gInGameTimer (gMaxInGameTimerFlag) and
     * the completed game flags. Version 1 files read as Brinstar, normal
     * difficulty and a zero timer. */
    uint8_t area, difficulty;
    struct { uint8_t hours, minutes, seconds, frames; } igt;
    bool max_igt;
    uint8_t completed_game;
    /* Version 3: gInGameTimerAtBosses (zero in older files). */
    struct { uint8_t hours, minutes, seconds, frames; } igt_at_bosses[5];
    /* Version 4: SaveFile.timeAttack (gTimeAttackFlag, a Time Attack run). */
    bool time_attack;
} MzmSaveData;

/* The native FileScreenOptionsUnlocked block is global rather than owned by
 * one of the three game files. The runtime keeps the fields used by the
 * ending. The byte at offset four keeps the native difficulty-clear bits so
 * existing runtime save files remain compatible; the bundled NES game is not
 * part of this project's scope. */
typedef struct {
    uint16_t gallery_images;
    uint16_t fusion_gallery_images;
    uint8_t difficulty_clears;
    uint32_t time_attack;
    /* Version 2: gTimeAttackRecord (SramWrite_TimeAttack); a version 1 file
     * reads it as the empty record, all 0xFF. */
    MzmTimeAttackRecord record;
} MzmUnlockData;

/* The serialized form; `size` receives its length. Returns a malloc'ed
 * buffer (NULL on failure). */
uint8_t *mzm_save_encode(const MzmSaveData *data, size_t *size);
/* Parses a serialized save; false when the magic, version, length or
 * checksum do not match. */
bool mzm_save_decode(const uint8_t *bytes, size_t size, MzmSaveData *data);
/* Writes (through a temporary file renamed over `path`) or reads a save. */
bool mzm_save_write(const char *path, const MzmSaveData *data);
bool mzm_save_read(const char *path, MzmSaveData *data);

/* Versioned, checksummed global unlock file (separate from the game slots). */
bool mzm_unlocks_write(const char *path, const MzmUnlockData *data);
bool mzm_unlocks_read(const char *path, MzmUnlockData *data);

#endif
