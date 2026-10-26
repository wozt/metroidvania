/* SPDX-License-Identifier: GPL-3.0-only */
/* The read-only catalog of decoded native object definitions (formerly
 * scripts/object_catalog.py, scripts/aria_item_details.py and
 * scripts/native_sprite_thumbnails.py). */
#ifndef FUSION_EDITOR_BACKEND_OBJECT_CATALOG_H
#define FUSION_EDITOR_BACKEND_OBJECT_CATALOG_H

#include "common.h"

/* _object_records(): the native object records of one world (public name)
 * or of both, sorted as the editor lists them. */
bool feb_object_records(const char *repository, const char *world, FusionJson *records,
                        FebError *error);
/* catalog_options(): the definitions of one role (ENEMY, ITEM, OBJECT) in an
 * internal world (mzm, aria). */
bool feb_catalog_options(const char *repository, const char *world, const char *kind,
                         FusionJson *options, FebError *error);
/* editor_catalog_options(): Aria items come from the ROM's named items (and
 * refresh the private item thumbnails); otherwise catalog_options(). */
bool feb_editor_catalog_options(const char *repository, const char *world, const char *kind,
                                FusionJson *options, FebError *error);
/* item_name(): the ROM name of an Aria item; false when it has none. */
bool feb_aria_item_name(const char *repository, int64_t subtype, int64_t item_id, char *name,
                        size_t size);

#endif /* FUSION_EDITOR_BACKEND_OBJECT_CATALOG_H */
