/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FUSION_EDITOR_BACKEND_CATALOG_H
#define FUSION_EDITOR_BACKEND_CATALOG_H

#include "common.h"

/* _native_catalog(): the original rooms of a world. */
bool feb_native_catalog(const char *repository, const char *world, FusionJson *rooms,
                        FebError *error);
/* _filter_native_rooms(): optionally the rooms of one area (an index, or a
 * Zero Mission area name in any case). */
bool feb_filter_native_rooms(const char *repository, const char *world,
                             const FusionJson *area, FusionJson *rooms, FebError *error);

#endif /* FUSION_EDITOR_BACKEND_CATALOG_H */
