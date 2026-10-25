/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FUSION_ASSETS_MZM_SPRITE_TABLES_H
#define FUSION_ASSETS_MZM_SPRITE_TABLES_H

#include <stddef.h>

typedef struct {
    const char *name;           /* without PSPRITE_ */
    unsigned row;               /* the palette row its AI selects */
    const char *graphics;
    const char *palette;
    const char *const *tables;  /* FrameData tables in source order */
    size_t table_count;
} FusionMzmSpritePlan;

typedef struct {
    const char *name;
    unsigned value;
} FusionMzmEnumName;

extern const FusionMzmSpritePlan fusion_mzm_sprite_plan[];
extern const size_t fusion_mzm_sprite_plan_count;
extern const char *const fusion_mzm_banner_tables[];
extern const size_t fusion_mzm_banner_table_count;
/* Sorted by value, the COUNT enumerator excluded. */
extern const FusionMzmEnumName fusion_mzm_primary_sprite_enum[];
extern const size_t fusion_mzm_primary_sprite_enum_count;
extern const FusionMzmEnumName fusion_mzm_secondary_sprite_enum[];
extern const size_t fusion_mzm_secondary_sprite_enum_count;
extern const FusionMzmEnumName fusion_mzm_message_enum[];
extern const size_t fusion_mzm_message_enum_count;
extern const unsigned fusion_mzm_primary_sprite_enum_total;
extern const unsigned fusion_mzm_secondary_sprite_enum_total;

#endif /* FUSION_ASSETS_MZM_SPRITE_TABLES_H */
