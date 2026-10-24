/* SPDX-License-Identifier: GPL-3.0-only */
#include "fusion/assets/mzm.h"

#include <stdio.h>
#include <string.h>

enum { GBA_ROM_BASE = 0x08000000 };

static const FusionMzmRoomAreaSpec area_specs[FUSION_MZM_ROOM_AREA_COUNT] = {
    {"Brinstar", "sBrinstarRoomEntries"},
    {"Kraid", "sKraidRoomEntries"},
    {"Norfair", "sNorfairRoomEntries"},
    {"Ridley", "sRidleyRoomEntries"},
    {"Tourian", "sTourianRoomEntries"},
    {"Crateria", "sCrateriaRoomEntries"},
    {"Chozodia", "sChozodiaRoomEntries"},
    {"Test1", "sTest1RoomEntries"},
    {"Test2", "sTest2RoomEntries"},
    {"Test3", "sTest3RoomEntries"},
    {"Test", "sTestRoomEntries"},
};

static void set_error(char *error, size_t error_size, const char *message) {
    if (!error || error_size == 0) return;
    snprintf(error, error_size, "%s", message);
    error[error_size - 1] = '\0';
}

static uint16_t read_u16(const uint8_t *data) {
    return (uint16_t)data[0] | (uint16_t)data[1] << 8;
}

static uint32_t read_u32(const uint8_t *data) {
    return (uint32_t)data[0] | (uint32_t)data[1] << 8 |
           (uint32_t)data[2] << 16 | (uint32_t)data[3] << 24;
}

static bool pointer_offset(const FusionRom *rom, uint32_t pointer, size_t size,
                           size_t *offset) {
    if (pointer < GBA_ROM_BASE) return false;
    size_t value = (size_t)(pointer - GBA_ROM_BASE);
    if (value > rom->size || size > rom->size - value) return false;
    *offset = value;
    return true;
}

static const FusionMzmRomSymbol *find_symbol(
    const FusionMzmRomSymbolTable *symbols, const char *name) {
    for (size_t index = 0; index < symbols->count; ++index) {
        if (symbols->entries[index].name &&
            !strcmp(symbols->entries[index].name, name))
            return &symbols->entries[index];
    }
    return NULL;
}

const FusionMzmRoomAreaSpec *fusion_mzm_room_area_spec(size_t index) {
    return index < FUSION_MZM_ROOM_AREA_COUNT ? &area_specs[index] : NULL;
}

bool fusion_mzm_read_room_directory(const FusionRom *rom,
                                    const FusionMzmRomSymbolTable *symbols,
                                    FusionMzmRoomDirectory *directory,
                                    char *error, size_t error_size) {
    if (!rom || !rom->data || rom->game != FUSION_ASSET_GAME_METROID ||
        !symbols || (symbols->count && !symbols->entries) || !directory) {
        set_error(error, error_size, "invalid Zero Mission room-directory request");
        return false;
    }
    *directory = (FusionMzmRoomDirectory){0};
    for (size_t area = 0; area < FUSION_MZM_ROOM_AREA_COUNT; ++area) {
        const FusionMzmRomSymbol *symbol =
            find_symbol(symbols, area_specs[area].symbol);
        if (!symbol || symbol->size == 0 ||
            symbol->size % FUSION_MZM_ROOM_ENTRY_SIZE != 0) {
            char message[160];
            snprintf(message, sizeof message,
                     "missing or invalid Zero Mission room table %s",
                     area_specs[area].symbol);
            set_error(error, error_size, message);
            return false;
        }
        size_t room_count = symbol->size / FUSION_MZM_ROOM_ENTRY_SIZE;
        size_t ignored;
        if (room_count > FUSION_MZM_ROOM_CAPACITY ||
            directory->total_rooms > FUSION_MZM_ROOM_CAPACITY - room_count ||
            !pointer_offset(rom, symbol->address, symbol->size, &ignored)) {
            char message[160];
            snprintf(message, sizeof message,
                     "Zero Mission room table %s is outside its safety bounds",
                     area_specs[area].symbol);
            set_error(error, error_size, message);
            return false;
        }
        directory->areas[area] =
            (FusionMzmRoomArea){symbol->address, room_count};
        directory->total_rooms += room_count;
    }
    if (error && error_size > 0) error[0] = '\0';
    return true;
}

bool fusion_mzm_read_room_entry(const FusionRom *rom,
                                const FusionMzmRoomDirectory *directory,
                                size_t area, size_t room,
                                FusionMzmRoomEntry *entry,
                                char *error, size_t error_size) {
    if (!rom || !rom->data || rom->game != FUSION_ASSET_GAME_METROID ||
        !directory || !entry || area >= FUSION_MZM_ROOM_AREA_COUNT ||
        room >= directory->areas[area].room_count) {
        set_error(error, error_size, "invalid Zero Mission room-entry request");
        return false;
    }
    size_t table_offset;
    if (directory->areas[area].room_count > FUSION_MZM_ROOM_CAPACITY ||
        !pointer_offset(rom, directory->areas[area].table_pointer,
                        directory->areas[area].room_count *
                            FUSION_MZM_ROOM_ENTRY_SIZE,
                        &table_offset)) {
        set_error(error, error_size, "Zero Mission room entry is outside the ROM");
        return false;
    }
    size_t offset = table_offset + room * FUSION_MZM_ROOM_ENTRY_SIZE;
    uint32_t pointer = GBA_ROM_BASE + (uint32_t)offset;
    const uint8_t *data = rom->data + offset;
    *entry = (FusionMzmRoomEntry){
        pointer, data[0], data[1], data[2], data[3], data[4],
        read_u32(data + 8), read_u32(data + 12), read_u32(data + 16),
        read_u32(data + 20), read_u32(data + 24), data[28], data[29],
        read_u32(data + 32), data[36], data[37], read_u32(data + 40),
        data[44], data[45], read_u32(data + 48), data[52], data[53],
        data[54], data[55], data[56], read_u16(data + 58)
    };
    const uint32_t resource_pointers[] = {
        entry->bg0_pointer, entry->bg1_pointer, entry->bg2_pointer,
        entry->clip_pointer, entry->bg3_pointer, entry->default_sprite_pointer,
        entry->first_sprite_pointer, entry->second_sprite_pointer
    };
    for (size_t index = 0;
         index < sizeof resource_pointers / sizeof resource_pointers[0]; ++index) {
        size_t ignored;
        if (!pointer_offset(rom, resource_pointers[index], 1, &ignored)) {
            set_error(error, error_size,
                      "Zero Mission room resource pointer is outside the ROM");
            return false;
        }
    }
    if (error && error_size > 0) error[0] = '\0';
    return true;
}
