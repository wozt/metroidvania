/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SAVE_H
#define AOS_SAVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AOS_SAVE_SCHEMA "metroidvania-aos-save-v2"
#define AOS_SAVE_SCHEMA_V1 "metroidvania-aos-save-v1"
#define AOS_SAVE_SLOT_COUNT 3

/* Portable host save metadata. This is deliberately separate from the
 * original 0x47C-byte SRAM slot until every native field is understood. */
typedef struct {
    uint8_t slot;
    uint8_t area;
    uint16_t room;
    int16_t spawn_x;
    int16_t spawn_y;
    int16_t hp;
    int16_t max_hp;
    uint32_t moves;
    uint64_t play_frames;
    uint8_t name[8];
    uint8_t character;
    uint32_t challenge_flags;
} AosSaveData;

void aos_save_init_new(AosSaveData *save, unsigned slot);
bool aos_save_load(const char *path, AosSaveData *save, char *error, size_t error_size);
bool aos_save_store(const char *path, const AosSaveData *save, char *error, size_t error_size);

#endif
