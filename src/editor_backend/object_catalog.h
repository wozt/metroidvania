/* SPDX-License-Identifier: GPL-3.0-only */
/* The read-only catalog of decoded native object definitions and private
 * Aria item previews. */
#ifndef FUSION_EDITOR_BACKEND_OBJECT_CATALOG_H
#define FUSION_EDITOR_BACKEND_OBJECT_CATALOG_H

#include "common.h"
#include "fusion/assets/rom.h"

/* _object_records(): the native object records of one world (public name)
 * or of both, sorted as the editor lists them. */
bool feb_object_records(const char *repository, const char *world, FusionJson *records,
                        FebError *error);
/* catalog_options(): the definitions of one role (ENEMY, ITEM, OBJECT) in an
 * internal world (mzm, aria). */
bool feb_catalog_options(const char *repository, const char *world, const char *kind,
                         FusionJson *options, FebError *error);
/* Pure catalog classification used before a record becomes an editor option;
 * NULL means that the record is intentionally excluded. */
const char *feb_catalog_role(const char *world, const char *native_type,
                             const char *category);
/* Append the 27 documented Aria pickup-family choices to an option array. */
bool feb_aria_item_family_options(FusionJson *options, FebError *error);
/* editor_catalog_options(): Aria items come from the ROM's named items (and
 * refresh the private item thumbnails); otherwise catalog_options(). */
bool feb_editor_catalog_options(const char *repository, const char *world, const char *kind,
                                FusionJson *options, FebError *error);
/* item_name(): the ROM name of an Aria item; false when it has none. */
bool feb_aria_item_name(const char *repository, int64_t subtype, int64_t item_id, char *name,
                        size_t size);
/* Buffer-level Aria decoders used by the editor and ROM-free native tests. */
bool feb_aria_item_name_from_rom(const FusionRom *rom, int64_t subtype, int64_t item_id,
                                 char *name, size_t size);
bool feb_aria_named_item_options_from_rom(const FusionRom *rom, FusionJson *options,
                                          FebError *error);
bool feb_aria_icon_page(const FusionRom *rom, uint32_t pointer, uint8_t page[0x2000]);
bool feb_aria_item_thumbnails_from_rom(const char *repository, const FusionRom *rom,
                                       int *written);

/* build_mzm(): the Zero Mission sprite records, appended to ``records``. */
bool feb_mzm_object_records(const char *repository, FusionJson *records, FebError *error);
/* mzm_sprite_kind() without stats: ENEMY, ITEM, DOOR, OBJECT or OTHER. */
const char *feb_mzm_sprite_kind(const char *symbol);

/* load_aria_enemy_names(): the 0x71 enemy names from the ROM constructor
 * table and the pinned cvaos symbols. */
bool feb_aria_enemy_names(const char *repository, char (*names)[96], FebError *error);
/* aria_entity_identity(): the display name and category of a native entity. */
void feb_aria_entity_identity(int64_t kind, int64_t id, char (*enemies)[96], char *name,
                              size_t size, const char **category);
/* Append grouped Aria definitions from an already decoded world catalog. */
bool feb_aria_object_records_from_world(const FusionJson *world, char (*enemies)[96],
                                        FusionJson *records, FebError *error);

#endif /* FUSION_EDITOR_BACKEND_OBJECT_CATALOG_H */
