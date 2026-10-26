/* SPDX-License-Identifier: GPL-3.0-only */
#include "fusion/pipeline/rebuild.h"

#include <string.h>

#define COUNT(array) (sizeof(array) / sizeof((array)[0]))

static const char *const checklist_dependencies[] = {"inventory"};
static const char *const aria_asset_dependencies[] = {"aria_rom"};
static const char *const metroid_asset_dependencies[] = {"metroid_rom"};
static const char *const raw_dependencies[] = {"aria_rom", "metroid_rom"};

static const char *const aria_rom_requirements[] = {
    "roms/Castlevania - Aria of Sorrow (USA).gba",
};
static const char *const metroid_rom_requirements[] = {
    "roms/Metroid - Zero Mission (USA).gba",
};
static const char *const metroid_elf_requirements[] = {
    "roms/Metroid - Zero Mission (USA).gba",
    "third_party/mzm/mzm_us.elf",
};
static const char *const raw_requirements[] = {
    "roms/Metroid - Zero Mission (USA).gba",
    "roms/Castlevania - Aria of Sorrow (USA).gba",
    "third_party/mzm/mzm_us.elf",
    "third_party/mzm/database.json",
    "third_party/cvaos/database.json",
};
static const char *const sound_requirements[] = {
    "roms/Metroid - Zero Mission (USA).gba",
    "third_party/mzm/mzm_us.map",
};

static const FusionRebuildTask tasks[] = {
    {"inventory", 2, FUSION_REBUILD_GROUP_NATIVE,
     FUSION_REBUILD_BACKEND_PYTHON_TEMPORARY, NULL, 0, NULL, 0},
    {"checklist", 4, FUSION_REBUILD_GROUP_NATIVE,
     FUSION_REBUILD_BACKEND_PYTHON_TEMPORARY,
     checklist_dependencies, COUNT(checklist_dependencies), NULL, 0},
    {"aria_rom", 2, FUSION_REBUILD_GROUP_ASSETS, FUSION_REBUILD_BACKEND_C,
     NULL, 0, aria_rom_requirements, COUNT(aria_rom_requirements)},
    {"metroid_rom", 2, FUSION_REBUILD_GROUP_ASSETS, FUSION_REBUILD_BACKEND_C,
     NULL, 0, metroid_rom_requirements, COUNT(metroid_rom_requirements)},
    {"raw_import", 2, FUSION_REBUILD_GROUP_ASSETS, FUSION_REBUILD_BACKEND_C,
     raw_dependencies, COUNT(raw_dependencies), raw_requirements, COUNT(raw_requirements)},
    {"aria_world", 2, FUSION_REBUILD_GROUP_ASSETS, FUSION_REBUILD_BACKEND_C,
     aria_asset_dependencies, COUNT(aria_asset_dependencies),
     aria_rom_requirements, COUNT(aria_rom_requirements)},
    {"aria_rooms", 2, FUSION_REBUILD_GROUP_ASSETS, FUSION_REBUILD_BACKEND_C,
     aria_asset_dependencies, COUNT(aria_asset_dependencies),
     aria_rom_requirements, COUNT(aria_rom_requirements)},
    {"aria_soma", 2, FUSION_REBUILD_GROUP_ASSETS, FUSION_REBUILD_BACKEND_C,
     aria_asset_dependencies, COUNT(aria_asset_dependencies),
     aria_rom_requirements, COUNT(aria_rom_requirements)},
    {"aria_objects", 2, FUSION_REBUILD_GROUP_ASSETS, FUSION_REBUILD_BACKEND_C,
     aria_asset_dependencies, COUNT(aria_asset_dependencies),
     aria_rom_requirements, COUNT(aria_rom_requirements)},
    {"aria_weapons", 2, FUSION_REBUILD_GROUP_ASSETS, FUSION_REBUILD_BACKEND_C,
     aria_asset_dependencies, COUNT(aria_asset_dependencies),
     aria_rom_requirements, COUNT(aria_rom_requirements)},
    {"metroid_samus", 2, FUSION_REBUILD_GROUP_ASSETS, FUSION_REBUILD_BACKEND_C,
     metroid_asset_dependencies, COUNT(metroid_asset_dependencies),
     metroid_elf_requirements, COUNT(metroid_elf_requirements)},
    {"metroid_sprites", 2, FUSION_REBUILD_GROUP_ASSETS, FUSION_REBUILD_BACKEND_C,
     metroid_asset_dependencies, COUNT(metroid_asset_dependencies),
     metroid_elf_requirements, COUNT(metroid_elf_requirements)},
    {"metroid_rooms", 2, FUSION_REBUILD_GROUP_ASSETS, FUSION_REBUILD_BACKEND_C,
     metroid_asset_dependencies, COUNT(metroid_asset_dependencies),
     metroid_elf_requirements, COUNT(metroid_elf_requirements)},
    {"metroid_sound", 1, FUSION_REBUILD_GROUP_ASSETS,
     FUSION_REBUILD_BACKEND_PYTHON_TEMPORARY,
     metroid_asset_dependencies, COUNT(metroid_asset_dependencies),
     sound_requirements, COUNT(sound_requirements)},
    {"metroid_rom_bank", 2, FUSION_REBUILD_GROUP_ASSETS, FUSION_REBUILD_BACKEND_C,
     metroid_asset_dependencies, COUNT(metroid_asset_dependencies),
     metroid_elf_requirements, COUNT(metroid_elf_requirements)},
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
