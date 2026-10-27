/* SPDX-License-Identifier: GPL-3.0-only */
#include "fusion/pipeline/rebuild.h"

#include <string.h>

#define COUNT(array) (sizeof(array) / sizeof((array)[0]))

static const char *const checklist_dependencies[] = {"inventory"};
static const char *const aria_asset_dependencies[] = {"aria_rom"};
static const char *const metroid_asset_dependencies[] = {"metroid_rom"};
static const char *const raw_dependencies[] = {"aria_rom", "metroid_rom"};

static const char *const aria_native_inputs[] = {
    "roms/Castlevania - Aria of Sorrow (USA).gba", "build/fusion_importer",
};
static const char *const metroid_native_inputs[] = {
    "roms/Metroid - Zero Mission (USA).gba", "build/fusion_importer",
};
static const char *const metroid_elf_inputs[] = {
    "roms/Metroid - Zero Mission (USA).gba", "third_party/mzm/mzm_us.elf",
    "build/fusion_importer",
};
static const char *const raw_inputs[] = {
    "roms/Metroid - Zero Mission (USA).gba",
    "roms/Castlevania - Aria of Sorrow (USA).gba",
    "third_party/mzm/mzm_us.elf", "third_party/mzm/database.json",
    "third_party/cvaos/database.json", "build/fusion_importer",
};
static const char *const sound_inputs[] = {
    "roms/Metroid - Zero Mission (USA).gba", "third_party/mzm/mzm_us.map",
    "build/fusion_importer",
};
static const char *const inventory_inputs[] = {"build/fusion_inventory"};
static const char *const inventory_requirements[] = {"build/fusion_inventory"};
static const char *const checklist_inputs[] = {
    "build/fusion_checklist", "data/native_parity/annotations.tsv",
};

static const char *const aria_requirements[] = {
    "roms/Castlevania - Aria of Sorrow (USA).gba", "build/fusion_importer",
};
static const char *const metroid_requirements[] = {
    "roms/Metroid - Zero Mission (USA).gba", "build/fusion_importer",
};
static const char *const metroid_elf_requirements[] = {
    "roms/Metroid - Zero Mission (USA).gba", "third_party/mzm/mzm_us.elf",
    "build/fusion_importer",
};
static const char *const raw_requirements[] = {
    "roms/Metroid - Zero Mission (USA).gba",
    "roms/Castlevania - Aria of Sorrow (USA).gba",
    "third_party/mzm/mzm_us.elf", "third_party/mzm/database.json",
    "third_party/cvaos/database.json", "build/fusion_importer",
};
static const char *const sound_requirements[] = {
    "roms/Metroid - Zero Mission (USA).gba", "third_party/mzm/mzm_us.map",
    "build/fusion_importer",
};
static const char *const checklist_requirements[] = {"build/fusion_checklist"};

static const char *const inventory_outputs[] = {"data/native_parity/inventory.json"};
static const char *const checklist_outputs[] = {
    "docs/NATIVE_PARITY_CHECKLIST.md", "data/native_parity/aos_unassigned_routines.tsv",
};
static const char *const aria_rom_outputs[] = {
    "assets/extracted/shared/manifests/rom_aria.json",
};
static const char *const metroid_rom_outputs[] = {
    "assets/extracted/shared/manifests/rom_metroid.json",
};
static const char *const raw_outputs[] = {
    "assets/extracted/shared/manifests/import_catalog.json",
    "assets/extracted/rooms/metroid/rooms.tsv",
};
static const char *const aria_world_outputs[] = {
    "assets/extracted/rooms/aria/world.json", "assets/extracted/rooms/aria/rooms.tsv",
};
static const char *const aria_room_outputs[] = {
    "assets/extracted/aria/rooms/runtime/export_index.tsv",
};
static const char *const aria_soma_outputs[] = {
    "assets/extracted/aria/sprites/soma/runtime/runtime_index.tsv",
};
static const char *const aria_object_outputs[] = {
    "assets/extracted/aria/sprites/objects/runtime/runtime_index.tsv",
    "assets/extracted/aria/metadata/enemy_frames.tsv",
    "assets/extracted/aria/metadata/enemies.tsv",
};
static const char *const aria_weapon_outputs[] = {
    "assets/extracted/aria/sprites/weapons/runtime/runtime_index.tsv",
    "assets/extracted/aria/metadata/weapons.tsv",
    "assets/extracted/aria/metadata/weapon_frames.tsv",
};
static const char *const metroid_samus_outputs[] = {
    "assets/extracted/metroid/sprites/samus/runtime/runtime_index.tsv",
    "assets/extracted/metroid/sprites/projectiles/runtime/runtime_index.tsv",
    "assets/extracted/metroid/sprites/samus/runtime/animation_map.tsv",
};
static const char *const metroid_sprite_outputs[] = {
    "assets/extracted/metroid/sprites/enemies/runtime/runtime_index.tsv",
    "assets/extracted/metroid/metadata/sprites.tsv",
};
static const char *const metroid_room_outputs[] = {
    "assets/extracted/metroid/rooms/runtime/export_index.tsv",
};
static const char *const sound_outputs[] = {
    "assets/extracted/metroid/sound/runtime/sound_bank.bin",
    "assets/extracted/metroid/sound/runtime/sound_tables.tsv",
};
static const char *const rom_bank_outputs[] = {
    "assets/extracted/metroid/rom/rom_bank.bin",
    "assets/extracted/metroid/rom/rom_symbols.tsv",
};

#define PATHS(input_array, output_array) \
    .inputs = input_array, .input_count = COUNT(input_array), \
    .outputs = output_array, .output_count = COUNT(output_array)
#define REQUIRES(array) .requirements = array, .requirement_count = COUNT(array)
#define DEPENDS(array) .dependencies = array, .dependency_count = COUNT(array)

static const FusionRebuildTask tasks[] = {
    {.name = "inventory", .version = 3, .group = FUSION_REBUILD_GROUP_NATIVE,
     REQUIRES(inventory_requirements),
     PATHS(inventory_inputs, inventory_outputs),
     .path_kind = FUSION_REBUILD_PATHS_INVENTORY},
    {.name = "checklist", .version = 5, .group = FUSION_REBUILD_GROUP_NATIVE,
     REQUIRES(checklist_requirements),
     DEPENDS(checklist_dependencies), PATHS(checklist_inputs, checklist_outputs),
     .path_kind = FUSION_REBUILD_PATHS_CHECKLIST},
    {.name = "aria_rom", .version = 2, .group = FUSION_REBUILD_GROUP_ASSETS,
     REQUIRES(aria_requirements), PATHS(aria_native_inputs, aria_rom_outputs)},
    {.name = "metroid_rom", .version = 2, .group = FUSION_REBUILD_GROUP_ASSETS,
     REQUIRES(metroid_requirements), PATHS(metroid_native_inputs, metroid_rom_outputs)},
    {.name = "raw_import", .version = 2, .group = FUSION_REBUILD_GROUP_ASSETS,
     DEPENDS(raw_dependencies),
     REQUIRES(raw_requirements), PATHS(raw_inputs, raw_outputs),
     .stamp_games = FUSION_REBUILD_STAMP_METROID | FUSION_REBUILD_STAMP_ARIA},
    {.name = "aria_world", .version = 2, .group = FUSION_REBUILD_GROUP_ASSETS,
     DEPENDS(aria_asset_dependencies),
     REQUIRES(aria_requirements), PATHS(aria_native_inputs, aria_world_outputs),
     .stamp_games = FUSION_REBUILD_STAMP_ARIA},
    {.name = "aria_rooms", .version = 2, .group = FUSION_REBUILD_GROUP_ASSETS,
     DEPENDS(aria_asset_dependencies),
     REQUIRES(aria_requirements), PATHS(aria_native_inputs, aria_room_outputs),
     .validation = FUSION_REBUILD_VALIDATE_EXPORT_INDEX,
     .stamp_games = FUSION_REBUILD_STAMP_ARIA},
    {.name = "aria_soma", .version = 2, .group = FUSION_REBUILD_GROUP_ASSETS,
     DEPENDS(aria_asset_dependencies),
     REQUIRES(aria_requirements), PATHS(aria_native_inputs, aria_soma_outputs),
     .validation = FUSION_REBUILD_VALIDATE_SPRITE_INDEX,
     .stamp_games = FUSION_REBUILD_STAMP_ARIA},
    {.name = "aria_objects", .version = 6, .group = FUSION_REBUILD_GROUP_ASSETS,
     DEPENDS(aria_asset_dependencies),
     REQUIRES(aria_requirements), PATHS(aria_native_inputs, aria_object_outputs),
     .validation = FUSION_REBUILD_VALIDATE_SPRITE_INDEX,
     .stamp_games = FUSION_REBUILD_STAMP_ARIA},
    {.name = "aria_weapons", .version = 2, .group = FUSION_REBUILD_GROUP_ASSETS,
     DEPENDS(aria_asset_dependencies),
     REQUIRES(aria_requirements), PATHS(aria_native_inputs, aria_weapon_outputs),
     .validation = FUSION_REBUILD_VALIDATE_SPRITE_INDEX,
     .stamp_games = FUSION_REBUILD_STAMP_ARIA},
    {.name = "metroid_samus", .version = 2, .group = FUSION_REBUILD_GROUP_ASSETS,
     DEPENDS(metroid_asset_dependencies),
     REQUIRES(metroid_elf_requirements), PATHS(metroid_elf_inputs, metroid_samus_outputs),
     .validation = FUSION_REBUILD_VALIDATE_SPRITE_INDEX,
     .stamp_games = FUSION_REBUILD_STAMP_METROID},
    {.name = "metroid_sprites", .version = 2, .group = FUSION_REBUILD_GROUP_ASSETS,
     DEPENDS(metroid_asset_dependencies),
     REQUIRES(metroid_elf_requirements), PATHS(metroid_elf_inputs, metroid_sprite_outputs),
     .validation = FUSION_REBUILD_VALIDATE_SPRITE_INDEX,
     .stamp_games = FUSION_REBUILD_STAMP_METROID},
    {.name = "metroid_rooms", .version = 2, .group = FUSION_REBUILD_GROUP_ASSETS,
     DEPENDS(metroid_asset_dependencies),
     REQUIRES(metroid_elf_requirements), PATHS(metroid_elf_inputs, metroid_room_outputs),
     .validation = FUSION_REBUILD_VALIDATE_EXPORT_INDEX,
     .stamp_games = FUSION_REBUILD_STAMP_METROID},
    {.name = "metroid_sound", .version = 2, .group = FUSION_REBUILD_GROUP_ASSETS,
     DEPENDS(metroid_asset_dependencies), REQUIRES(sound_requirements),
     PATHS(sound_inputs, sound_outputs), .stamp_games = FUSION_REBUILD_STAMP_METROID},
    {.name = "metroid_rom_bank", .version = 2, .group = FUSION_REBUILD_GROUP_ASSETS,
     DEPENDS(metroid_asset_dependencies),
     REQUIRES(metroid_elf_requirements), PATHS(metroid_elf_inputs, rom_bank_outputs),
     .stamp_games = FUSION_REBUILD_STAMP_METROID},
};

const FusionRebuildTask *fusion_rebuild_tasks(size_t *count) {
    if (count) *count = COUNT(tasks);
    return tasks;
}

const FusionRebuildTask *fusion_rebuild_find_task(const char *name) {
    if (!name) return NULL;
    for (size_t index = 0; index < COUNT(tasks); ++index)
        if (!strcmp(tasks[index].name, name)) return &tasks[index];
    return NULL;
}

const char *fusion_rebuild_group_name(FusionRebuildGroup group) {
    return group == FUSION_REBUILD_GROUP_ASSETS ? "assets" : "native";
}
