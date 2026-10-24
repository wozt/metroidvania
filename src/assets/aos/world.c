/* SPDX-License-Identifier: GPL-3.0-only */
#include "fusion/assets/aos.h"

#include <stdio.h>

enum {
    GBA_ROM_BASE = 0x08000000,
    AOS_MAP_TABLE_OFFSET = 0x116650,
    AOS_AREA_DIRECTORY_OFFSET = 0x50EF08,
    AOS_ROOM_DESCRIPTOR_SIZE = 0x24,
    AOS_TRANSITION_SIZE = 0x10,
    AOS_BACKGROUND_DESCRIPTOR_SIZE = 0x0C,
    AOS_LOAD_ENTRY_SIZE = 0x08,
    AOS_ENTITY_ENTRY_SIZE = 0x0C,
    AOS_ENEMY_TABLE_POINTER = 0x080E9644,
    AOS_ENEMY_TABLE_ENTRY_SIZE = 0x24,
    AOS_ENEMY_HEALTH_OFFSET = 0x0C
};

static const FusionAosBossSpec boss_specs[FUSION_AOS_BOSS_ROOM_COUNT] = {
    {"aria.creaking_skull", "Creaking Skull", "fight", 0, 10, 0x21},
    {"aria.manticore", "Manticore", "fight", 1, 6, 0x36},
    {"aria.great_armor", "Great Armor", "fight", 2, 15, 0x3C},
    {"aria.big_golem", "Big Golem", "fight", 3, 19, 0x45},
    {"aria.headhunter", "Headhunter", "fight", 4, 11, 0x6A},
    {"aria.julius", "Julius Belmont", "fight", 5, 6, 0x6E},
    {"aria.death", "Death", "fight", 6, 7, 0x6B},
    {"aria.legion", "Legion", "fight", 7, 37, 0x6C},
    {"aria.balore", "Balore", "fight", 8, 1, 0x6D},
    {"aria.graham", "Graham Jones", "fight", 9, 4, 0x6F},
    {"aria.chaos", "Chaos", "first_phase", 11, 20, 0x70},
    {"aria.chaos", "Chaos", "second_phase", 11, 22, 0x70},
};

static void set_error(char *error, size_t error_size, const char *message) {
    if (!error || error_size == 0) return;
    snprintf(error, error_size, "%s", message);
    error[error_size - 1] = '\0';
}

static uint32_t read_u32(const uint8_t *data) {
    return (uint32_t)data[0] | (uint32_t)data[1] << 8 |
           (uint32_t)data[2] << 16 | (uint32_t)data[3] << 24;
}

static uint16_t read_u16(const uint8_t *data) {
    return (uint16_t)data[0] | (uint16_t)data[1] << 8;
}

static bool pointer_offset(const FusionRom *rom, uint32_t pointer, size_t size,
                           size_t *offset) {
    if (pointer < GBA_ROM_BASE) return false;
    size_t value = (size_t)(pointer - GBA_ROM_BASE);
    if (value > rom->size || size > rom->size - value) return false;
    *offset = value;
    return true;
}

bool fusion_aos_read_area_directory(const FusionRom *rom,
                                    FusionAosAreaDirectory *directory,
                                    char *error, size_t error_size) {
    if (!rom || !directory || !rom->data || rom->game != FUSION_ASSET_GAME_ARIA) {
        set_error(error, error_size, "expected an Aria of Sorrow ROM buffer");
        return false;
    }
    const size_t directory_size = FUSION_AOS_AREA_COUNT * sizeof(uint32_t);
    if (rom->size < AOS_AREA_DIRECTORY_OFFSET + directory_size) {
        set_error(error, error_size, "ROM is too short for the Aria area directory");
        return false;
    }

    *directory = (FusionAosAreaDirectory){0};
    uint32_t pointers[FUSION_AOS_AREA_COUNT];
    for (size_t area = 0; area < FUSION_AOS_AREA_COUNT; ++area) {
        pointers[area] = read_u32(rom->data + AOS_AREA_DIRECTORY_OFFSET + area * 4);
    }

    const uint32_t table_end = GBA_ROM_BASE + AOS_AREA_DIRECTORY_OFFSET;
    const uint32_t rom_end = GBA_ROM_BASE + (uint32_t)rom->size;
    for (size_t area = 0; area < FUSION_AOS_AREA_COUNT; ++area) {
        uint32_t start = pointers[area];
        uint32_t end = area + 1 < FUSION_AOS_AREA_COUNT ? pointers[area + 1] : table_end;
        if (start < GBA_ROM_BASE || start >= rom_end || end <= start ||
            end > table_end || (end - start) % sizeof(uint32_t) != 0) {
            char message[128];
            snprintf(message, sizeof message,
                     "invalid room-pointer table bounds for Aria area %zu", area);
            set_error(error, error_size, message);
            return false;
        }
        directory->areas[area].room_table_pointer = start;
        directory->areas[area].room_count = (end - start) / sizeof(uint32_t);
        directory->total_rooms += directory->areas[area].room_count;
    }
    if (error && error_size > 0) error[0] = '\0';
    return true;
}

bool fusion_aos_read_map(const FusionRom *rom,
                         const FusionAosAreaDirectory *directory,
                         FusionAosMap *map, char *error, size_t error_size) {
    const size_t map_size = FUSION_AOS_MAP_CELL_CAPACITY * sizeof(uint16_t);
    if (!rom || !directory || !map || !rom->data ||
        rom->game != FUSION_ASSET_GAME_ARIA) {
        set_error(error, error_size, "expected an Aria of Sorrow ROM buffer and directory");
        return false;
    }
    if (rom->size < AOS_MAP_TABLE_OFFSET + map_size) {
        set_error(error, error_size, "ROM is too short for the Aria map table");
        return false;
    }

    *map = (FusionAosMap){0};
    bool seen_save[FUSION_AOS_AREA_COUNT][64] = {{false}};
    for (size_t y = 0; y < FUSION_AOS_MAP_HEIGHT; ++y) {
        for (size_t x = 0; x < FUSION_AOS_MAP_WIDTH; ++x) {
            size_t index = y * FUSION_AOS_MAP_WIDTH + x;
            uint16_t value = read_u16(rom->data + AOS_MAP_TABLE_OFFSET + index * 2);
            if (value == 0xFFFF) continue;
            size_t area = (value >> 6) & 0xF;
            size_t room = value & 0x3F;
            if (area >= FUSION_AOS_AREA_COUNT ||
                room >= directory->areas[area].room_count) {
                char message[128];
                snprintf(message, sizeof message,
                         "Aria map cell (%zu,%zu) references invalid room %zu:%zu",
                         x, y, area, room);
                set_error(error, error_size, message);
                return false;
            }
            size_t table_offset;
            if (!pointer_offset(rom, directory->areas[area].room_table_pointer,
                                directory->areas[area].room_count * sizeof(uint32_t),
                                &table_offset)) {
                set_error(error, error_size, "Aria room table points outside the ROM");
                return false;
            }
            uint32_t room_pointer = read_u32(rom->data + table_offset + room * 4);
            size_t ignored;
            if (!pointer_offset(rom, room_pointer, 1, &ignored)) {
                set_error(error, error_size, "Aria map cell room pointer is outside the ROM");
                return false;
            }
            bool save = (value & 0x8000) != 0;
            bool warp = (value & 0x4000) != 0;
            if (save && seen_save[area][room]) {
                char message[128];
                snprintf(message, sizeof message,
                         "Aria save room %zu:%zu occupies multiple map cells", area, room);
                set_error(error, error_size, message);
                return false;
            }
            seen_save[area][room] = seen_save[area][room] || save;
            FusionAosMapCell *cell = &map->cells[map->count++];
            *cell = (FusionAosMapCell){(uint8_t)x, (uint8_t)y, (uint8_t)area,
                                       (uint8_t)room, save, warp, room_pointer};
            map->save_count += save ? 1u : 0u;
            map->warp_count += warp ? 1u : 0u;
        }
    }
    if (error && error_size > 0) error[0] = '\0';
    return true;
}

bool fusion_aos_read_room_descriptor(const FusionRom *rom,
                                     const FusionAosAreaDirectory *directory,
                                     size_t area, size_t room,
                                     FusionAosRoomDescriptor *descriptor,
                                     char *error, size_t error_size) {
    if (!rom || !directory || !descriptor || !rom->data ||
        rom->game != FUSION_ASSET_GAME_ARIA || area >= FUSION_AOS_AREA_COUNT ||
        room >= directory->areas[area].room_count) {
        set_error(error, error_size, "invalid Aria room descriptor request");
        return false;
    }
    size_t table_offset;
    if (!pointer_offset(rom, directory->areas[area].room_table_pointer,
                        directory->areas[area].room_count * sizeof(uint32_t),
                        &table_offset)) {
        set_error(error, error_size, "Aria room table points outside the ROM");
        return false;
    }

    *descriptor = (FusionAosRoomDescriptor){0};
    uint32_t pointer = read_u32(rom->data + table_offset + room * 4);
    descriptor->source_pointer = pointer;
    for (size_t index = 0; index < FUSION_AOS_ROOM_VARIANT_CAPACITY; ++index) {
        for (size_t previous = 0; previous < index; ++previous) {
            if (descriptor->variants[previous].pointer == pointer) {
                set_error(error, error_size, "Aria room variant chain contains a cycle");
                return false;
            }
        }
        size_t offset;
        if (!pointer_offset(rom, pointer, AOS_ROOM_DESCRIPTOR_SIZE, &offset)) {
            set_error(error, error_size, "Aria room descriptor points outside the ROM");
            return false;
        }
        uint16_t selector = read_u16(rom->data + offset + 2);
        descriptor->variants[index] = (FusionAosRoomVariant){pointer, selector};
        descriptor->variant_count = index + 1;
        if (selector == 0xFFFF) break;
        pointer = read_u32(rom->data + offset + 4);
    }
    if (descriptor->variants[descriptor->variant_count - 1].selector != 0xFFFF) {
        set_error(error, error_size, "Aria room variant chain exceeds the safety cap");
        return false;
    }

    size_t offset;
    if (!pointer_offset(rom, pointer, AOS_ROOM_DESCRIPTOR_SIZE, &offset)) {
        set_error(error, error_size, "resolved Aria room descriptor is outside the ROM");
        return false;
    }
    descriptor->resolved_pointer = pointer;
    descriptor->display_control = read_u16(rom->data + offset);
    descriptor->next_pointer = read_u32(rom->data + offset + 4);
    descriptor->backgrounds_pointer = read_u32(rom->data + offset + 8);
    descriptor->graphics_pointer = read_u32(rom->data + offset + 12);
    descriptor->palette_pointer = read_u32(rom->data + offset + 16);
    descriptor->entities_pointer = read_u32(rom->data + offset + 20);
    descriptor->transitions_pointer = read_u32(rom->data + offset + 24);
    descriptor->blend_control = read_u16(rom->data + offset + 30);
    uint16_t coordinates = read_u16(rom->data + offset + 34);
    descriptor->map_x = coordinates & 0x7F;
    descriptor->map_y = (coordinates >> 7) & 0x7F;
    descriptor->coordinate_flag_0 = ((coordinates >> 14) & 1) != 0;
    descriptor->coordinate_flag_1 = ((coordinates >> 15) & 1) != 0;
    if (descriptor->transitions_pointer > pointer ||
        (pointer - descriptor->transitions_pointer) % AOS_TRANSITION_SIZE != 0) {
        set_error(error, error_size, "invalid Aria room transition table bounds");
        return false;
    }
    descriptor->transition_count =
        (pointer - descriptor->transitions_pointer) / AOS_TRANSITION_SIZE;
    size_t transition_offset;
    if (!pointer_offset(rom, descriptor->transitions_pointer,
                        descriptor->transition_count * AOS_TRANSITION_SIZE,
                        &transition_offset)) {
        set_error(error, error_size, "Aria room transition table points outside the ROM");
        return false;
    }
    if (error && error_size > 0) error[0] = '\0';
    return true;
}

static bool resolve_room_pointer(const FusionRom *rom,
                                 const FusionAosAreaDirectory *directory,
                                 uint32_t target, size_t *target_area,
                                 size_t *target_room) {
    size_t matches = 0;
    for (size_t area = 0; area < FUSION_AOS_AREA_COUNT; ++area) {
        size_t table_offset;
        if (!pointer_offset(rom, directory->areas[area].room_table_pointer,
                            directory->areas[area].room_count * sizeof(uint32_t),
                            &table_offset)) {
            return false;
        }
        for (size_t room = 0; room < directory->areas[area].room_count; ++room) {
            if (read_u32(rom->data + table_offset + room * 4) == target) {
                *target_area = area;
                *target_room = room;
                ++matches;
            }
        }
    }
    return matches == 1;
}

bool fusion_aos_read_room_transitions(const FusionRom *rom,
                                      const FusionAosAreaDirectory *directory,
                                      const FusionAosRoomDescriptor *descriptor,
                                      FusionAosTransitionList *transitions,
                                      char *error, size_t error_size) {
    if (!rom || !directory || !descriptor || !transitions || !rom->data ||
        rom->game != FUSION_ASSET_GAME_ARIA) {
        set_error(error, error_size, "invalid Aria room transition request");
        return false;
    }
    if (descriptor->transition_count > FUSION_AOS_ROOM_TRANSITION_CAPACITY) {
        set_error(error, error_size, "Aria room transition count exceeds the safety cap");
        return false;
    }
    size_t offset;
    if (!pointer_offset(rom, descriptor->transitions_pointer,
                        descriptor->transition_count * AOS_TRANSITION_SIZE, &offset)) {
        set_error(error, error_size, "Aria room transition table points outside the ROM");
        return false;
    }

    *transitions = (FusionAosTransitionList){0};
    for (size_t index = 0; index < descriptor->transition_count; ++index) {
        const uint8_t *data = rom->data + offset + index * AOS_TRANSITION_SIZE;
        FusionAosTransition *transition = &transitions->entries[index];
        transition->entry_pointer =
            GBA_ROM_BASE + (uint32_t)(offset + index * AOS_TRANSITION_SIZE);
        transition->target_pointer = read_u32(data);
        transition->source_screen_x = (int8_t)data[4];
        transition->source_screen_y = (int8_t)data[5];
        transition->field_6 = read_u16(data + 6);
        transition->field_8 = read_u16(data + 8);
        transition->load_x = read_u16(data + 10);
        transition->load_y = read_u16(data + 12);
        transition->field_e = read_u16(data + 14);
        size_t target_offset;
        if (!pointer_offset(rom, transition->target_pointer,
                            AOS_ROOM_DESCRIPTOR_SIZE, &target_offset) ||
            !resolve_room_pointer(rom, directory, transition->target_pointer,
                                  &transition->target_area,
                                  &transition->target_room)) {
            set_error(error, error_size,
                      "Aria transition targets an unknown or ambiguous room");
            return false;
        }
        transitions->count = index + 1;
    }
    if (error && error_size > 0) error[0] = '\0';
    return true;
}

bool fusion_aos_read_room_backgrounds(const FusionRom *rom,
                                      const FusionAosRoomDescriptor *descriptor,
                                      FusionAosBackgroundList *backgrounds,
                                      char *error, size_t error_size) {
    if (!rom || !descriptor || !backgrounds || !rom->data ||
        rom->game != FUSION_ASSET_GAME_ARIA) {
        set_error(error, error_size, "invalid Aria room background request");
        return false;
    }
    size_t offset;
    if (!pointer_offset(rom, descriptor->backgrounds_pointer,
                        FUSION_AOS_BACKGROUND_COUNT * AOS_BACKGROUND_DESCRIPTOR_SIZE,
                        &offset)) {
        set_error(error, error_size, "Aria background descriptors point outside the ROM");
        return false;
    }

    *backgrounds = (FusionAosBackgroundList){0};
    for (size_t index = 0; index < FUSION_AOS_BACKGROUND_COUNT; ++index) {
        size_t entry_offset = offset + index * AOS_BACKGROUND_DESCRIPTOR_SIZE;
        const uint8_t *data = rom->data + entry_offset;
        FusionAosBackground *background = &backgrounds->entries[index];
        background->descriptor_pointer = GBA_ROM_BASE + (uint32_t)entry_offset;
        background->layer = (uint8_t)(index + 1);
        background->field_0 = data[0];
        background->field_1 = data[1];
        background->control = read_u16(data + 2);
        background->field_4 = read_u16(data + 4);
        background->field_6 = read_u16(data + 6);
        background->metadata_pointer = read_u32(data + 8);
        if (background->metadata_pointer != 0) {
            size_t metadata_offset;
            if (!pointer_offset(rom, background->metadata_pointer, 2, &metadata_offset)) {
                set_error(error, error_size,
                          "Aria background dimension metadata points outside the ROM");
                return false;
            }
            background->has_dimensions = true;
            background->width_screens = rom->data[metadata_offset];
            background->height_screens = rom->data[metadata_offset + 1];
        }
    }
    if (error && error_size > 0) error[0] = '\0';
    return true;
}

bool fusion_aos_read_load_list(const FusionRom *rom, uint32_t pointer,
                               FusionAosLoadList *list,
                               char *error, size_t error_size) {
    if (!rom || !list || !rom->data || rom->game != FUSION_ASSET_GAME_ARIA) {
        set_error(error, error_size, "invalid Aria loading-list request");
        return false;
    }
    size_t offset;
    if (!pointer_offset(rom, pointer, AOS_LOAD_ENTRY_SIZE, &offset)) {
        set_error(error, error_size, "Aria loading list points outside the ROM");
        return false;
    }

    *list = (FusionAosLoadList){0};
    for (size_t index = 0; index < FUSION_AOS_LOAD_ENTRY_CAPACITY; ++index) {
        size_t entry_offset = offset + index * AOS_LOAD_ENTRY_SIZE;
        if (entry_offset > rom->size || AOS_LOAD_ENTRY_SIZE > rom->size - entry_offset) {
            set_error(error, error_size, "unterminated Aria loading list");
            return false;
        }
        const uint8_t *data = rom->data + entry_offset;
        uint32_t resource_pointer = read_u32(data);
        if (resource_pointer == 0) {
            if (error && error_size > 0) error[0] = '\0';
            return true;
        }
        size_t resource_offset;
        if (!pointer_offset(rom, resource_pointer, 1, &resource_offset)) {
            set_error(error, error_size, "Aria loading-list resource points outside the ROM");
            return false;
        }
        FusionAosLoadEntry *entry = &list->entries[index];
        entry->entry_pointer = GBA_ROM_BASE + (uint32_t)entry_offset;
        entry->resource_pointer = resource_pointer;
        entry->parameters[0] = data[4];
        entry->parameters[1] = data[5];
        entry->parameters[2] = data[6];
        list->count = index + 1;
    }
    set_error(error, error_size, "Aria loading list exceeds the safety cap");
    return false;
}

bool fusion_aos_read_entity_list(const FusionRom *rom, uint32_t pointer,
                                 FusionAosEntityList *list,
                                 char *error, size_t error_size) {
    if (!rom || !list || !rom->data || rom->game != FUSION_ASSET_GAME_ARIA) {
        set_error(error, error_size, "invalid Aria entity-list request");
        return false;
    }
    *list = (FusionAosEntityList){0};
    if (pointer == 0) {
        if (error && error_size > 0) error[0] = '\0';
        return true;
    }
    size_t offset;
    if (!pointer_offset(rom, pointer, AOS_ENTITY_ENTRY_SIZE, &offset)) {
        set_error(error, error_size, "Aria entity list points outside the ROM");
        return false;
    }
    for (size_t index = 0; index < FUSION_AOS_ENTITY_CAPACITY; ++index) {
        size_t entry_offset = offset + index * AOS_ENTITY_ENTRY_SIZE;
        if (entry_offset > rom->size || AOS_ENTITY_ENTRY_SIZE > rom->size - entry_offset) {
            set_error(error, error_size, "unterminated Aria entity list");
            return false;
        }
        const uint8_t *data = rom->data + entry_offset;
        int16_t x = (int16_t)read_u16(data);
        if (x == INT16_MAX) {
            if (error && error_size > 0) error[0] = '\0';
            return true;
        }
        FusionAosEntity *entity = &list->entries[index];
        entity->entry_pointer = GBA_ROM_BASE + (uint32_t)entry_offset;
        entity->x = x;
        entity->y = (int16_t)read_u16(data + 2);
        entity->persistent_index = data[4];
        entity->kind = data[5];
        entity->entity_id = data[6];
        entity->flags = data[7];
        entity->parameters[0] = read_u16(data + 8);
        entity->parameters[1] = read_u16(data + 10);
        list->count = index + 1;
    }
    set_error(error, error_size, "Aria entity list exceeds the safety cap");
    return false;
}

const FusionAosBossSpec *fusion_aos_boss_spec(size_t index) {
    return index < FUSION_AOS_BOSS_ROOM_COUNT ? &boss_specs[index] : NULL;
}

bool fusion_aos_read_boss_room(const FusionRom *rom,
                               const FusionAosAreaDirectory *directory,
                               const FusionAosBossSpec *spec,
                               FusionAosBossRoom *boss,
                               char *error, size_t error_size) {
    if (!rom || !directory || !spec || !boss || !rom->data ||
        rom->game != FUSION_ASSET_GAME_ARIA ||
        spec->area >= FUSION_AOS_AREA_COUNT ||
        spec->room >= directory->areas[spec->area].room_count) {
        set_error(error, error_size, "invalid Aria boss-room request");
        return false;
    }
    FusionAosRoomDescriptor descriptor;
    FusionAosEntityList entities;
    if (!fusion_aos_read_room_descriptor(rom, directory, spec->area, spec->room,
                                         &descriptor, error, error_size) ||
        !fusion_aos_read_entity_list(rom, descriptor.entities_pointer,
                                     &entities, error, error_size)) {
        return false;
    }
    const FusionAosEntity *match = NULL;
    for (size_t index = 0; index < entities.count; ++index) {
        const FusionAosEntity *entity = &entities.entries[index];
        if (entity->kind == 1 && entity->entity_id == spec->enemy_id) {
            if (match) {
                set_error(error, error_size, "Aria boss entity is ambiguous in its room");
                return false;
            }
            match = entity;
        }
    }
    if (!match) {
        set_error(error, error_size, "Aria boss entity is missing from its room");
        return false;
    }
    uint32_t enemy_table_pointer =
        AOS_ENEMY_TABLE_POINTER + spec->enemy_id * AOS_ENEMY_TABLE_ENTRY_SIZE;
    size_t enemy_offset;
    if (!pointer_offset(rom, enemy_table_pointer,
                        AOS_ENEMY_TABLE_ENTRY_SIZE, &enemy_offset)) {
        set_error(error, error_size, "Aria enemy table entry points outside the ROM");
        return false;
    }
    *boss = (FusionAosBossRoom){spec, descriptor.map_x, descriptor.map_y,
                                descriptor.source_pointer, match->entry_pointer,
                                enemy_table_pointer,
                                read_u16(rom->data + enemy_offset + AOS_ENEMY_HEALTH_OFFSET)};
    if (error && error_size > 0) error[0] = '\0';
    return true;
}
