/* SPDX-License-Identifier: GPL-3.0-only */
/* Asset and save-data loaders of the Aria of Sorrow room runtime, extracted
 * from aria_runtime.c (AGENTS.md rule 8): the object/enemy/effect frame
 * index and BMP loader, the metadata TSV record loaders (enemy frames and
 * stats, souls, skill throws, bursts, equipment, item use) and the
 * save-data marshalling they feed. */
#ifndef ARIA_ASSETS_H
#define ARIA_ASSETS_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stddef.h>

#include "aria_runtime_types.h"
#include "aos/core/aos_stats.h"
#include "aos/core/aos_soma.h"
#include "aos/game/aos_save.h"
#include "aos/souls/aos_soul.h"
#include "aos/souls/aos_soul_menu.h"
#include "aos/red_souls/aos_soul_red_throw.h"

/* sprites/objects/runtime/runtime_index.tsv + metadata TSVs: fills the
 * AriaObjects art tables and per-kind kind/stats/drops data. */
bool aria_load_objects(const char *path, AriaObjects *objects, SDL_Renderer *renderer);
void aria_free_objects(AriaObjects *objects);
/* metadata/enemy_frames.tsv, enemies.tsv, item_icons.tsv and money.tsv. */
bool aria_load_enemy_data(AriaObjects *objects);
/* metadata/yellow_souls.tsv, blue_souls.tsv, red_souls.tsv,
 * skill_throws.tsv and bursts.tsv record loaders. */
bool aria_load_yellow_souls(const char *path,
                            AosYellowSoulRecord records[AOS_YELLOW_SOUL_RECORDS]);
bool aria_load_blue_souls(const char *path,
                          AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS]);
bool aria_load_red_souls(const char *path,
                         AosRedSoulRecord records[AOS_RED_SOUL_RECORDS]);
bool aria_load_skill_throws(const char *path, AosThrowRecord records[AOS_THROW_RECORDS]);
bool aria_load_bursts(const char *path);
/* Burst list 0 as loaded by aria_load_bursts (sub_08045A98's source). */
const AosEffect *aria_burst_effects(size_t *count);
/* metadata/equipment.tsv and item_use.tsv record loaders. */
bool aria_load_equipment(const char *path, AosEquipmentTables *tables);
bool aria_load_item_use(const char *path, AosItemUseRecord items[AOS_ITEM_RECORDS]);
/* Save-data marshalling: the soul inventories/abilities block and the
 * runtime -> AosSaveData fill (reads aria_events/aria_pickup_state). */
void aria_souls_from_save(AosStats *stats, const AosSaveData *save);
void aria_runtime_to_save(AosSaveData *save, const AriaRoom *room,
                          const AosSoma *soma, const AosStats *progress);

#endif
