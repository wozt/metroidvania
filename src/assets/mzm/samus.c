/* SPDX-License-Identifier: GPL-3.0-only */
/* The private Samus and projectile sprite libraries (formerly
 * scripts/mzm_samus_pipeline.py).
 *
 * The Samus selection mirrors SamusUpdateGraphicsOam and SamusUpdatePalette:
 * body animations come from the per-pose or per-selector
 * sSamusAnimPointers_* tables, the arm cannon animation from the matching
 * sArmCannonAnimPointers_* table (or ..._All[pose]), and the arm cannon
 * graphics from sArmCannonGfxPointers_* with the running-right, hanging,
 * zipline and default rules of the source. Varia reuses the Power Suit
 * bodies and Gravity the Full Suit bodies. Each frame is stored once as OBJ
 * palette indices (bank * 16 + color) with the family's default palette as
 * preview, cropped to its visible pixels and placed relative to Samus's
 * position, including the two-pixel downward shift of SamusDraw.
 *
 * The projectile staging mirrors HudGenericLoadCommonSpriteGfx,
 * ProjectileLoadGraphics and ProjectileDraw: every projectile FrameData
 * table in four flip states (Wave and Plasma also with the Ice Beam palette
 * row), then every beam particle for each loaded beam variant. */
#include "fusion/assets/mzm.h"

#include "export.h"
#include "samus_tables.h"
#include "sprite_library.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    ROM_BASE = 0x08000000,
    ROM_LIMIT = 0x0E000000,
    OBJ_VRAM_BYTES = 0x8000,
    ANIMATION_RECORD_BYTES = 16,
    CANNON_RECORD_BYTES = 8,
    CANNON_GFX_BYTES = 64,
    CANNON_UPPER_VRAM = 0x800,
    CANNON_LOWER_VRAM = 0xC00,
    /* SamusDraw adds SUB_PIXEL_TO_PIXEL(EIGHTH_BLOCK_SIZE) to Y. */
    DRAW_Y_OFFSET = 2,
    MAX_FRAMES = 256,
    MAX_OAM = 128,
    PREVIEW_COLORS = 32,
    /* OAM X spans -256..319 and Y -128..191 before any flip; the projectile
     * mirror around the origin extends them to -320..319 and -192..191. */
    SAMUS_CANVAS_WIDTH = 576,
    SAMUS_CANVAS_HEIGHT = 320,
    SAMUS_ORIGIN_X = 256,
    SAMUS_ORIGIN_Y = 128,
    SHOT_CANVAS_WIDTH = 640,
    SHOT_CANVAS_HEIGHT = 384,
    SHOT_ORIGIN_X = 320,
    SHOT_ORIGIN_Y = 192,
    COMMON_GFX_VRAM = 0x800,
    BEAM_BLOCK_BYTES = 512,
    ICE_ROW = 2,
};

static const char *const samus_prefix = "assets/extracted/metroid/sprites/samus/runtime";
static const char *const projectile_prefix =
    "assets/extracted/metroid/sprites/projectiles/runtime";

static const char *const families[] = {"PowerSuit", "FullSuit", "Suitless"};
enum { FAMILY_COUNT = 3 };

/* Visual suit -> body table family and default palette symbol. */
static const struct {
    const char *suit;
    const char *family;
    const char *palette;
} visual_suits[] = {
    {"PowerSuit", "PowerSuit", "sSamusPal_PowerSuit_Default"},
    {"VariaSuit", "PowerSuit", "sSamusPal_VariaSuit_Default"},
    {"FullSuit", "FullSuit", "sSamusPal_FullSuit_Default"},
    {"GravitySuit", "FullSuit", "sSamusPal_GravitySuit_Default"},
    {"Suitless", "Suitless", "sSamusPal_Suitless_Default"},
};
enum { VISUAL_SUIT_COUNT = 5 };

static const char *const acd_selectors[] = {
    "ACD_FORWARD", "ACD_DIAGONALLY_UP", "ACD_DIAGONALLY_DOWN", "ACD_UP", "ACD_DOWN"};

/* Table name -> native pose, only where the cannon graphics rule depends on
 * it or where no dedicated cannon table exists. */
static const char *const table_poses[][2] = {
    {"Running", "SPOSE_RUNNING"},
    {"Running_Speedboosting", "SPOSE_RUNNING"},
    {"Skidding", "SPOSE_SKIDDING"},
    {"ScrewAttacking", "SPOSE_SCREW_ATTACKING"},
    {"AimingWhileHanging", "SPOSE_AIMING_WHILE_HANGING"},
    {"ShootingWhileHanging", "SPOSE_SHOOTING_WHILE_HANGING"},
    {"OnZipline", "SPOSE_ON_ZIPLINE"},
    {"ShootingOnZipline", "SPOSE_SHOOTING_ON_ZIPLINE"},
};

/* Selectors spelled differently in paired tables but with equal values. */
static const char *const selector_aliases[][2] = {
    {"FALSE", "0"}, {"TRUE", "1"},
    {"FORCED_MOVEMENT_CRAWLING_ARM_CANNON_DOWN", "0"},
    {"FORCED_MOVEMENT_CRAWLING_ARM_CANNON_UP", "1"},
};

/* The in-gunship saving animation of each family (SamusUpdateGraphicsOam). */
static const char *const in_ship_animations[] = {
    "sSamusAnim_256484", "sSamusAnim_27f430", "sSamusAnim_2b0bf4"};

static const char *const map_aims[][2] = {
    {"forward", "ACD_FORWARD"},
    {"diagonalup", "ACD_DIAGONALLY_UP"},
    {"diagonaldown", "ACD_DIAGONALLY_DOWN"},
    {"up", "ACD_UP"},
    {"down", "ACD_DOWN"},
};

/* Semantic action -> native per-ACD table used by SamusUpdateGraphicsOam. */
static const char *const aim_actions[][3] = {
    {"idle", "Standing", "loop"},
    {"run", "Running", "loop"},
    {"run_speedboost", "Running_Speedboosting", "loop"},
    {"midair", "MidAir", "loop"},
    {"crouch", "Crouching", "loop"},
    {"fire", "Shooting", "once"},
    {"crouch_fire", "ShootingAndCrouching", "once"},
    {"turn", "TurningAround", "once"},
    {"turn_midair", "TurningAroundMidAir", "once"},
    {"turn_crouch", "TurningAroundAndCrouching", "once"},
    {"landing", "Landing", "once"},
};

/* Semantic action -> exact native (table, selector). */
static const char *const simple_actions[][4] = {
    {"skid", "Skidding", "FALSE", "once"},
    {"spin_start", "pose", "SPOSE_STARTING_SPIN_JUMP", "once"},
    {"spin", "pose", "SPOSE_SPINNING", "loop"},
    {"space_jump", "pose", "SPOSE_SPACE_JUMPING", "loop"},
    {"screw_attack", "ScrewAttacking", "FALSE", "loop"},
    {"screw_attack_space", "ScrewAttacking", "TRUE", "loop"},
    {"wall_jump", "pose", "SPOSE_STARTING_WALL_JUMP", "once"},
    {"morph_start", "pose", "SPOSE_MORPHING", "once"},
    {"morph_ball", "pose", "SPOSE_MORPH_BALL", "loop"},
    {"rolling", "pose", "SPOSE_ROLLING", "loop"},
    {"morph_midair", "pose", "SPOSE_MORPH_BALL_MIDAIR", "loop"},
    {"unmorph", "pose", "SPOSE_UNMORPHING", "once"},
    {"ledge_hang", "pose", "SPOSE_HANGING_ON_LEDGE", "loop"},
    {"ledge_pull_forward", "pose", "SPOSE_PULLING_YOURSELF_FORWARD_FROM_HANGING", "once"},
    {"ledge_pull_up", "pose", "SPOSE_PULLING_YOURSELF_UP_FROM_HANGING", "once"},
    {"hurt", "pose", "SPOSE_GETTING_HURT", "once"},
    {"hurt_morph", "pose", "SPOSE_GETTING_HURT_IN_MORPH_BALL", "loop"},
    {"death", "pose", "SPOSE_DYING", "once"},
    {"shinespark_charge", "pose", "SPOSE_DELAY_BEFORE_SHINESPARKING", "once"},
    {"shinespark", "Shinesparking", "FORCED_MOVEMENT_UPWARDS_SHINESPARK", "loop"},
    {"shinespark_side", "Shinesparking", "FORCED_MOVEMENT_SIDEWARDS_SHINESPARK", "loop"},
    {"shinespark_diagonal", "Shinesparking", "FORCED_MOVEMENT_DIAGONAL_SHINESPARK", "loop"},
    {"shinespark_end", "pose", "SPOSE_DELAY_AFTER_SHINESPARKING", "once"},
    {"ball_spark", "pose", "SPOSE_BALLSPARKING", "loop"},
    {"ballspark_charge", "pose", "SPOSE_DELAY_BEFORE_BALLSPARKING", "loop"},
    {"ball_spark_collision", "pose", "SPOSE_BALLSPARK_COLLISION", "once"},
    {"grabbed_by_chozo_statue", "pose", "SPOSE_GRABBED_BY_CHOZO_STATUE", "loop"},
};

/* These abilities cannot coexist with the suitless part of the campaign, so
 * the native graphics tables have no suitless rows for them. */
static const char *const suitless_unavailable[] = {
    "run_speedboost", "skid", "screw_attack", "screw_attack_space",
    "shinespark", "shinespark_side", "shinespark_diagonal"};

/* Projectile beam set -> graphics prefix and sBeamPal row. */
static const struct {
    const char *name;
    const char *graphics;
    unsigned row;
} beam_sets[] = {
    {"NormalBeam", "sNormalBeamGfx", 0},
    {"LongBeam", "sLongBeamGfx", 1},
    {"IceBeam", "sIceBeamGfx", 2},
    {"WaveBeam", "sWaveBeamGfx", 3},
    {"PlasmaBeam", "sPlasmaBeamGfx", 4},
    {"Pistol", "sPistolGfx", 5},
};
enum { BEAM_SET_COUNT = 6 };

static const char *const flip_names[] = {"none", "x", "y", "xy"};

/* The beam graphics ProjectileLoadGraphics loads for the equipped beams: the
 * graphics set and the sBeamPal row (ice recolours wave and plasma). */
static const struct {
    const char *name;
    size_t beam;
    unsigned row;
} beam_variants[] = {
    {"NormalBeam", 0, 0}, {"LongBeam", 1, 1}, {"IceBeam", 2, 2},
    {"WaveBeam", 3, 3}, {"WaveIceBeam", 3, 2}, {"PlasmaBeam", 4, 4},
    {"PlasmaIceBeam", 4, 2}, {"Pistol", 5, 5},
};

static const char *const beam_particles[] = {
    "sParticleChargingBeamBeginOam", "sParticleChargingBeamChargedOam",
    "sParticleShootingBeamHorizontalOam", "sParticleShootingBeamDiagonallyUpOam",
    "sParticleShootingBeamDiagonallyDownOam", "sParticleShootingBeamUpOam",
    "sParticleShootingBeamDownOam", "sParticleBeamTrailingOam",
    "sParticleHittingSomethingWithLongBeamOam",
    "sParticleHittingSomethingWithIceBeamOam",
    "sParticleHittingSomethingWithWaveBeamOam",
    "sParticleHittingSomethingWithPlasmaBeamOam",
    "sParticleMissileTrailOam", "sParticleSuperMissileTrailOam",
    "sParticleChargedLongBeamTrailOam", "sParticleChargedIceBeamTrailOam",
    "sParticleChargedWaveBeamTrailOam", "sParticleChargedPlasmaBeamTrailOam",
    "sParticleChargedFullBeamTrailOam", "sParticleChargedPistolTrailOam"};

/* Particles only drawn with one graphics set (their tiles are blank in the
 * others). */
static const char *const particle_beam_sets[][2] = {
    {"sParticleChargedPlasmaBeamTrailOam", "PlasmaBeam"},
    {"sParticleChargedFullBeamTrailOam", "PlasmaBeam"},
    {"sParticleChargedPistolTrailOam", "Pistol"},
};

/* Symbol name prefixes whose ELF bytes must equal the ROM. */
static const char *const verified_prefixes[] = {
    "sSamusAnim_", "sArmCannonAnim_", "sArmCannonGfx_", "sSamusPal_",
    "sCommonSprites", "sBeamPal", "sNormalBeam", "sChargedNormalBeam", "sLongBeam",
    "sChargedLongBeam", "sIceBeam", "sChargedIceBeam", "sWaveBeam", "sChargedWaveBeam",
    "sPlasmaBeam", "sChargedPlasmaBeam", "sPistol", "sChargedPistol", "sMissile",
    "sSuperMissile", "sBomb", "sPowerBomb", "sParticleChargingBeam",
    "sParticleShootingBeam", "sParticleBeamTrailing",
    "sParticleHittingSomethingWithLongBeam", "sParticleHittingSomethingWithIceBeam",
    "sParticleHittingSomethingWithWaveBeam", "sParticleHittingSomethingWithPlasmaBeam",
    "sParticleMissileTrail", "sParticleSuperMissileTrail",
    "sParticleChargedLongBeamTrail", "sParticleChargedIceBeamTrail",
    "sParticleChargedWaveBeamTrail", "sParticleChargedPlasmaBeamTrail",
    "sParticleChargedFullBeamTrail", "sParticleChargedPistolTrail"};

#define COUNT(array) (sizeof (array) / sizeof (array)[0])

typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} Text;

typedef struct {
    int x, y, w, h;
    unsigned tile, bank;
    bool hflip, vflip;
} OamEntry;

typedef struct {
    const FusionRom *rom;
    const FusionMzmRomSymbol **by_name;
    size_t count;
    char *error;
    size_t error_size;
} Context;

typedef struct {
    size_t family;
    char table[64];
    const char *selector;
    size_t side;
    const char *body;
    const char *cannon;
    const char *pose;
    const char *gfx[2];
    const char *armed[2];
    bool has_gfx, has_armed;
} Variant;

typedef struct {
    char key[160];
    const char *body;
    const char *cannon;
    const char *gfx[2];
    bool has_gfx;
    size_t frames;
    uint8_t parts[MAX_FRAMES];
    int muzzle[MAX_FRAMES][2];
    bool has_muzzle;
    const char *preview;
} SamusSequence;

typedef struct {
    char key[160];
    const char *table;
    const char *beam_set;
    const char *variant;
    const char *flip;
    bool ice;
} ShotSequence;

typedef struct {
    uint8_t vram[OBJ_VRAM_BYTES];
    uint8_t rgb[256][3];
    bool loaded[256];
} ShotStage;

static void set_error(char *error, size_t error_size, const char *format, ...) {
    if (!error || error_size == 0) return;
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(error, error_size, format, arguments);
    va_end(arguments);
    error[error_size - 1] = '\0';
}

static bool text_append(Text *text, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    va_list copy;
    va_copy(copy, arguments);
    int needed = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (needed < 0) {
        va_end(arguments);
        return false;
    }
    size_t total = text->size + (size_t)needed + 1;
    if (total > text->capacity) {
        size_t next = text->capacity ? text->capacity : 4096;
        while (next < total) {
            if (next > SIZE_MAX / 2) {
                va_end(arguments);
                return false;
            }
            next *= 2;
        }
        char *grown = realloc(text->data, next);
        if (!grown) {
            va_end(arguments);
            return false;
        }
        text->data = grown;
        text->capacity = next;
    }
    vsnprintf(text->data + text->size, text->capacity - text->size, format, arguments);
    va_end(arguments);
    text->size += (size_t)needed;
    return true;
}

static uint16_t read_u16(const uint8_t *data) {
    return (uint16_t)(data[0] | (data[1] << 8));
}

static uint32_t read_u32(const uint8_t *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static bool starts_with(const char *text, const char *prefix) {
    return strncmp(text, prefix, strlen(prefix)) == 0;
}

static bool ends_with(const char *text, const char *suffix) {
    size_t length = strlen(text), suffix_length = strlen(suffix);
    return length >= suffix_length && !strcmp(text + length - suffix_length, suffix);
}

static void bgr555(uint16_t value, uint8_t rgb[3]) {
    for (int channel = 0; channel < 3; ++channel)
        rgb[channel] = (uint8_t)(((value >> (5 * channel)) & 31) * 255 / 31);
}

static int compare_names(const void *left, const void *right) {
    const FusionMzmRomSymbol *const *a = left, *const *b = right;
    return strcmp((*a)->name, (*b)->name);
}

static const FusionMzmRomSymbol *symbol(const Context *context, const char *name) {
    size_t low = 0, high = context->count;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        int order = strcmp(context->by_name[middle]->name, name);
        if (!order) return context->by_name[middle];
        if (order < 0) low = middle + 1;
        else high = middle;
    }
    return NULL;
}

/* rom_offset of the former pipeline: a pointer inside the supplied ROM. */
static const uint8_t *slice(const Context *context, uint32_t pointer, size_t size,
                            const char *label) {
    const FusionRom *rom = context->rom;
    if (pointer < ROM_BASE || pointer >= ROM_LIMIT) {
        set_error(context->error, context->error_size,
                  "not a GBA ROM pointer: 0x%08x", (unsigned)pointer);
        return NULL;
    }
    size_t offset = pointer - ROM_BASE;
    if (offset >= rom->size) {
        set_error(context->error, context->error_size,
                  "pointer exceeds supplied ROM size");
        return NULL;
    }
    if (size > rom->size - offset) {
        set_error(context->error, context->error_size, "truncated %s", label);
        return NULL;
    }
    return rom->data + offset;
}

static const FusionMzmRomSymbol *require_symbol(const Context *context,
                                                const char *name) {
    const FusionMzmRomSymbol *found = symbol(context, name);
    if (!found)
        set_error(context->error, context->error_size, "missing ELF symbol %s", name);
    return found;
}

/* The bytes the reference ELF loads at a ROM address, from its allocated
 * PROGBITS sections. */
static const uint8_t *elf_bytes(const uint8_t *elf, size_t elf_size,
                                uint32_t address, uint32_t size) {
    if (elf_size < 52) return NULL;
    size_t table = read_u32(elf + 0x20);
    size_t entry = read_u16(elf + 0x2E), count = read_u16(elf + 0x30);
    if (entry < 40 || table > elf_size || count > (elf_size - table) / entry)
        return NULL;
    for (size_t index = 0; index < count; ++index) {
        const uint8_t *section = elf + table + index * entry;
        uint32_t type = read_u32(section + 4), flags = read_u32(section + 8);
        uint32_t start = read_u32(section + 12);
        size_t offset = read_u32(section + 16), length = read_u32(section + 20);
        if (type != 1 || !(flags & 2) || address < start ||
            (uint64_t)address + size > (uint64_t)start + length ||
            offset > elf_size || length > elf_size - offset)
            continue;
        return elf + offset + (address - start);
    }
    return NULL;
}

static bool verify_symbols(const Context *context, const uint8_t *elf,
                           size_t elf_size) {
    char mismatched[256] = "";
    size_t mismatches = 0;
    for (size_t index = 0; index < context->count; ++index) {
        const FusionMzmRomSymbol *entry = context->by_name[index];
        bool consumed = false;
        for (size_t prefix = 0; prefix < COUNT(verified_prefixes); ++prefix)
            if (starts_with(entry->name, verified_prefixes[prefix])) consumed = true;
        if (!consumed) continue;
        size_t offset = entry->address - ROM_BASE;
        const uint8_t *image = elf_bytes(elf, elf_size, entry->address, entry->size);
        if (image && !memcmp(image, context->rom->data + offset, entry->size)) continue;
        if (mismatches++ < 5) {
            size_t used = strlen(mismatched);
            snprintf(mismatched + used, sizeof mismatched - used, "%s%s",
                     used ? ", " : "", entry->name);
        }
    }
    if (mismatches) {
        set_error(context->error, context->error_size,
                  "reference ELF does not match the ROM for: %s", mismatched);
        return false;
    }
    return true;
}

static const FusionMzmSamusTable *find_table(const char *name) {
    for (size_t index = 0; index < fusion_mzm_samus_table_count; ++index)
        if (!strcmp(fusion_mzm_samus_tables[index].name, name))
            return &fusion_mzm_samus_tables[index];
    return NULL;
}

static const FusionMzmSamusTableRow *find_row(const FusionMzmSamusTable *table,
                                              const char *selector) {
    if (!table || !selector) return NULL;
    for (size_t index = 0; index < table->count; ++index)
        if (!strcmp(table->rows[index].selector, selector)) return &table->rows[index];
    return NULL;
}

static const char *alias(const char *selector) {
    for (size_t index = 0; index < COUNT(selector_aliases); ++index)
        if (!strcmp(selector_aliases[index][0], selector)) return selector_aliases[index][1];
    return selector;
}

static const char *table_pose(const char *suffix) {
    for (size_t index = 0; index < COUNT(table_poses); ++index)
        if (!strcmp(table_poses[index][0], suffix)) return table_poses[index][1];
    return NULL;
}

static bool pose_is(const char *pose, const char *name) {
    return pose && !strcmp(pose, name);
}

/* The arm cannon graphics of SamusUpdateGraphicsOam; ``armed`` selects the
 * variant used while missiles or super missiles are highlighted. */
static bool cannon_gfx(const char *pose, const char *acd, size_t side, bool armed,
                       const char *result[2]) {
    const char *side_name = side == 0 ? "Right" : "Left";
    const char *armed_part = armed ? "Armed_" : "";
    char family[64];
    if (pose_is(pose, "SPOSE_AIMING_WHILE_HANGING") ||
        pose_is(pose, "SPOSE_SHOOTING_WHILE_HANGING"))
        snprintf(family, sizeof family, "%s_%sHanging", side_name, armed_part);
    else if ((pose_is(pose, "SPOSE_ON_ZIPLINE") ||
              pose_is(pose, "SPOSE_SHOOTING_ON_ZIPLINE")) && side == 0)
        snprintf(family, sizeof family, "Right_%sOnZipline", armed_part);
    else if (pose_is(pose, "SPOSE_RUNNING") && side == 0)
        snprintf(family, sizeof family, "%sStanding", armed_part);
    else
        snprintf(family, sizeof family, "%s_%sDefault", side_name, armed_part);
    static const char *const parts[] = {"Upper", "Lower"};
    for (size_t part = 0; part < 2; ++part) {
        char name[128];
        snprintf(name, sizeof name, "sArmCannonGfxPointers_%s_%s", parts[part], family);
        const FusionMzmSamusTableRow *row = find_row(find_table(name), acd);
        if (!row) return false;
        result[part] = row->symbols[0];
    }
    return true;
}

static int compare_tables(const void *left, const void *right) {
    const FusionMzmSamusTable *const *a = left, *const *b = right;
    return strcmp((*a)->name, (*b)->name);
}

static int compare_rows(const void *left, const void *right) {
    const FusionMzmSamusTableRow *const *a = left, *const *b = right;
    return strcmp((*a)->selector, (*b)->selector);
}

static bool add_variant(Variant **variants, size_t *count, size_t *capacity,
                        const Variant *variant) {
    if (*count == *capacity) {
        size_t next = *capacity ? *capacity * 2 : 256;
        Variant *grown = realloc(*variants, next * sizeof *grown);
        if (!grown) return false;
        *variants = grown;
        *capacity = next;
    }
    (*variants)[(*count)++] = *variant;
    return true;
}

static void fill_gfx(Variant *variant, const char *acd) {
    variant->has_gfx = cannon_gfx(variant->pose, acd, variant->side, false, variant->gfx);
    variant->has_armed = variant->family != 2 &&
        cannon_gfx(variant->pose, acd, variant->side, true, variant->armed);
}

/* Every body, arm cannon and graphics combination, in the order of the
 * former pipeline: sorted tables, sorted selectors, right then left, then
 * the in-gunship animation of each family. */
static bool build_variants(Variant **variants, size_t *count, char *error,
                           size_t error_size) {
    size_t capacity = 0;
    *variants = NULL;
    *count = 0;
    const FusionMzmSamusTable *sorted[512];
    const FusionMzmSamusTableRow *rows[512];
    if (fusion_mzm_samus_table_count > COUNT(sorted)) return false;
    for (size_t family = 0; family < FAMILY_COUNT; ++family) {
        const char *cannon_family = family == 2 ? "Suitless" : "Suit";
        char prefix[64], name[128];
        snprintf(prefix, sizeof prefix, "sSamusAnimPointers_%s", families[family]);
        size_t prefix_length = strlen(prefix), table_count = 0;
        for (size_t index = 0; index < fusion_mzm_samus_table_count; ++index) {
            const char *table = fusion_mzm_samus_tables[index].name;
            if (!strcmp(table, prefix) ||
                (starts_with(table, prefix) && table[prefix_length] == '_'))
                sorted[table_count++] = &fusion_mzm_samus_tables[index];
        }
        qsort(sorted, table_count, sizeof sorted[0], compare_tables);
        snprintf(name, sizeof name, "sArmCannonAnimPointers_%s_All", cannon_family);
        const FusionMzmSamusTable *cannon_all = find_table(name);
        for (size_t index = 0; index < table_count; ++index) {
            const FusionMzmSamusTable *table = sorted[index];
            const char *suffix = table->name[prefix_length]
                ? table->name + prefix_length + 1 : "pose";
            const FusionMzmSamusTable *cannon_table = NULL;
            if (strcmp(suffix, "pose")) {
                snprintf(name, sizeof name, "sArmCannonAnimPointers_%s_%s",
                         cannon_family, suffix);
                cannon_table = find_table(name);
            }
            if (table->count > COUNT(rows)) return false;
            for (size_t row = 0; row < table->count; ++row) rows[row] = &table->rows[row];
            qsort(rows, table->count, sizeof rows[0], compare_rows);
            for (size_t row = 0; row < table->count; ++row) {
                const char *selector = rows[row]->selector;
                const char *pose = !strcmp(suffix, "pose") ? selector : table_pose(suffix);
                /* The cannon graphics index: ACD_NONE - 2 on the Running row. */
                const char *acd = "ACD_FORWARD";
                for (size_t aim = 0; aim < COUNT(acd_selectors); ++aim)
                    if (!strcmp(selector, acd_selectors[aim])) acd = acd_selectors[aim];
                if (!strcmp(selector, "ACD_NONE")) acd = "ACD_UP";
                for (size_t side = 0; side < 2; ++side) {
                    if (side >= rows[row]->count) {
                        set_error(error, error_size, "%s[%s] has no %s entry",
                                  table->name, selector, side ? "left" : "right");
                        return false;
                    }
                    const FusionMzmSamusTableRow *cannon_row = NULL;
                    if (cannon_table) {
                        cannon_row = find_row(cannon_table, selector);
                        for (size_t other = 0; !cannon_row && other < cannon_table->count;
                             ++other) {
                            if (!strcmp(alias(cannon_table->rows[other].selector),
                                        alias(selector)))
                                cannon_row = &cannon_table->rows[other];
                        }
                    } else if (pose) {
                        cannon_row = find_row(cannon_all, pose);
                    }
                    const char *cannon = cannon_row && side < cannon_row->count
                        ? cannon_row->symbols[side] : NULL;
                    if (!cannon) {
                        set_error(error, error_size, "%s[%s] %s: no native arm cannon table",
                                  table->name, selector, side ? "left" : "right");
                        return false;
                    }
                    Variant variant = {.family = family, .selector = selector,
                                       .side = side, .body = rows[row]->symbols[side],
                                       .cannon = cannon, .pose = pose};
                    snprintf(variant.table, sizeof variant.table, "%s", suffix);
                    fill_gfx(&variant, acd);
                    if (!add_variant(variants, count, &capacity, &variant)) return false;
                }
            }
        }
        /* SPOSE_SAVING_LOADING_GAME with lastWallTouchedMidAir (Samus in her
         * gunship) draws one of these tables with the pose's arm cannon. */
        const char *pose = "SPOSE_SAVING_LOADING_GAME";
        const FusionMzmSamusTableRow *cannon_row = find_row(cannon_all, pose);
        for (size_t side = 0; side < 2; ++side) {
            Variant variant = {.family = family, .selector = pose, .side = side,
                               .body = in_ship_animations[family], .pose = pose,
                               .cannon = cannon_row && side < cannon_row->count
                                   ? cannon_row->symbols[side] : NULL};
            snprintf(variant.table, sizeof variant.table, "InShip");
            fill_gfx(&variant, "ACD_FORWARD");
            if (!add_variant(variants, count, &capacity, &variant)) return false;
        }
    }
    return true;
}

static bool unpack_oam(const Context *context, const uint8_t *raw, OamEntry *entry) {
    static const int sizes[3][4][2] = {
        {{8, 8}, {16, 16}, {32, 32}, {64, 64}},
        {{16, 8}, {32, 8}, {32, 16}, {64, 32}},
        {{8, 16}, {8, 32}, {16, 32}, {32, 64}},
    };
    uint16_t a0 = read_u16(raw), a1 = read_u16(raw + 2), a2 = read_u16(raw + 4);
    if (a0 & 0x0300) {
        set_error(context->error, context->error_size,
                  "affine/double-size or disabled OBJ not supported");
        return false;
    }
    if (a0 & 0x2000) {
        set_error(context->error, context->error_size, "8bpp OBJ not supported");
        return false;
    }
    unsigned shape = (a0 >> 14) & 3, size = (a1 >> 14) & 3;
    if (shape == 3) {
        set_error(context->error, context->error_size, "reserved OBJ shape");
        return false;
    }
    int x = a1 & 511, y = a0 & 255;
    if (x >= 256) x -= 512;
    if (y >= 128) y -= 256;
    *entry = (OamEntry){x, y, sizes[shape][size][0], sizes[shape][size][1],
                        a2 & 1023u, (a2 >> 12) & 15u, (a1 & 0x1000) != 0,
                        (a1 & 0x2000) != 0};
    return true;
}

/* Samus's compact OAM: a header whose low 12 bits count the parts, then
 * three attributes per part. */
static bool samus_oam(const Context *context, uint32_t pointer, uint16_t *header,
                      OamEntry *entries, size_t *count) {
    const uint8_t *data = slice(context, pointer, 2, "OAM header");
    if (!data) return false;
    *header = read_u16(data);
    *count = *header & 0xFFF;
    if (*count > MAX_OAM) {
        set_error(context->error, context->error_size, "excessive OAM part count");
        return false;
    }
    if (!*count) return true;
    const uint8_t *raw = slice(context, pointer + 2, *count * 6, "OAM entries");
    if (!raw) return false;
    for (size_t index = 0; index < *count; ++index)
        if (!unpack_oam(context, raw + index * 6, &entries[index])) return false;
    return true;
}

/* The four independent 4bpp uploads of one body frame. */
static bool stage_body(const Context *context, uint32_t frame_pointer,
                       uint8_t *vram, uint32_t *oam, uint8_t *duration) {
    static const size_t banks[4][2] = {
        {0x000, 0x280}, {0x400, 0x280}, {0x280, 0x180}, {0x680, 0x180}};
    const uint8_t *record = slice(context, frame_pointer, 13, "frame record");
    if (!record) return false;
    uint32_t pointers[2] = {read_u32(record), read_u32(record + 4)};
    *oam = read_u32(record + 8);
    *duration = record[12];
    if (!slice(context, *oam, 0, "OAM")) return false;
    memset(vram, 0, OBJ_VRAM_BYTES);
    for (size_t half = 0; half < 2; ++half) {
        const uint8_t *counts = slice(context, pointers[half], 2, "graphics header");
        if (!counts) return false;
        size_t first = counts[0] * 32u, second = counts[1] * 32u;
        const uint8_t *tiles = slice(context, pointers[half] + 2, first + second,
                                     "graphics data");
        if (!tiles) return false;
        /* Top: shoulders then torso; bottom: legs then lower body. */
        const size_t *slots[2] = {banks[half ? 2 : 0], banks[half ? 3 : 1]};
        size_t sizes[2] = {first, second};
        for (size_t part = 0; part < 2; ++part) {
            if (sizes[part] > slots[part][1]) {
                set_error(context->error, context->error_size,
                          "graphics subset exceeds VRAM slot 0x%zx", slots[part][0]);
                return false;
            }
            memcpy(vram + slots[part][0], tiles + (part ? first : 0), sizes[part]);
        }
    }
    return true;
}

static uint8_t vram_index(const uint8_t *vram, const OamEntry *entry, int ix, int iy,
                          bool *outside) {
    int sx = entry->hflip ? entry->w - 1 - ix : ix;
    int sy = entry->vflip ? entry->h - 1 - iy : iy;
    size_t tile = entry->tile + (size_t)(sy / 8) * 32 + (size_t)(sx / 8);
    size_t offset = tile * 32 + (size_t)(sy % 8) * 4 + (size_t)(sx % 8) / 2;
    if (offset >= OBJ_VRAM_BYTES) {
        *outside = true;
        return 0;
    }
    return (uint8_t)((vram[offset] >> (4 * (sx & 1))) & 15);
}

/* One Samus frame as OBJ palette indices on the canvas. */
static bool render_samus(const Context *context, const Variant *variant, size_t frame,
                         bool armed, uint8_t *canvas, uint8_t *duration,
                         size_t *cannon_parts) {
    static uint8_t vram[OBJ_VRAM_BYTES];
    const FusionMzmRomSymbol *body = require_symbol(context, variant->body);
    uint32_t oam = 0;
    if (!body || !stage_body(context, body->address +
                             (uint32_t)(frame * ANIMATION_RECORD_BYTES),
                             vram, &oam, duration))
        return false;
    OamEntry entries[MAX_OAM * 3], body_entries[MAX_OAM], cannon_entries[MAX_OAM];
    uint16_t header = 0;
    size_t body_count = 0, cannon_count = 0, count = 0;
    if (!samus_oam(context, oam, &header, body_entries, &body_count)) return false;
    memcpy(entries, body_entries, body_count * sizeof *entries);
    count = body_count;
    *cannon_parts = 0;
    const char *const *graphics = armed ? variant->armed : variant->gfx;
    bool has_graphics = armed ? variant->has_armed : variant->has_gfx;
    if (variant->cannon && !pose_is(variant->pose, "SPOSE_DYING")) {
        const FusionMzmRomSymbol *cannon = require_symbol(context, variant->cannon);
        const uint8_t *record = cannon ? slice(context, cannon->address +
            (uint32_t)(frame * CANNON_RECORD_BYTES), CANNON_RECORD_BYTES,
            "cannon record") : NULL;
        if (!record || !samus_oam(context, read_u32(record + 4), &header,
                                  cannon_entries, &cannon_count))
            return false;
        if (cannon_count) {
            if (!has_graphics) {
                set_error(context->error, context->error_size,
                          "arm cannon OAM without native graphics table");
                return false;
            }
            static const uint32_t bases[2] = {CANNON_UPPER_VRAM, CANNON_LOWER_VRAM};
            for (size_t part = 0; part < 2; ++part) {
                const FusionMzmRomSymbol *gfx = require_symbol(context, graphics[part]);
                const uint8_t *data = gfx ? slice(context, gfx->address, CANNON_GFX_BYTES,
                                                  "cannon graphics") : NULL;
                if (!data) return false;
                memcpy(vram + bases[part], data, CANNON_GFX_BYTES);
            }
            /* SamusDraw order: cannon in front, body, cannon behind. */
            count = 0;
            if (header & 0x1000) {
                memcpy(entries, cannon_entries, cannon_count * sizeof *entries);
                count = cannon_count;
            }
            memcpy(entries + count, body_entries, body_count * sizeof *entries);
            count += body_count;
            if (header & 0x2000) {
                memcpy(entries + count, cannon_entries, cannon_count * sizeof *entries);
                count += cannon_count;
            }
            *cannon_parts = cannon_count;
        }
    }
    memset(canvas, 0, SAMUS_CANVAS_WIDTH * SAMUS_CANVAS_HEIGHT);
    for (size_t index = count; index-- > 0;) {  /* OAM slot zero wins. */
        const OamEntry *entry = &entries[index];
        if (entry->bank > 1) {
            set_error(context->error, context->error_size,
                      "OAM uses a palette bank outside Samus's two rows");
            return false;
        }
        for (int iy = 0; iy < entry->h; ++iy) {
            for (int ix = 0; ix < entry->w; ++ix) {
                bool outside = false;
                uint8_t color = vram_index(vram, entry, ix, iy, &outside);
                if (outside) {
                    set_error(context->error, context->error_size,
                              "OBJ tile outside VRAM");
                    return false;
                }
                if (!color) continue;
                canvas[(size_t)(entry->y + iy + SAMUS_ORIGIN_Y) * SAMUS_CANVAS_WIDTH +
                       (size_t)(entry->x + ix + SAMUS_ORIGIN_X)] =
                    (uint8_t)(entry->bank * 16 + color);
            }
        }
    }
    return true;
}

/* Durations of the zero-terminated native body animation. */
static bool body_durations(const Context *context, const char *name,
                           uint8_t *durations, size_t *count) {
    const FusionMzmRomSymbol *body = require_symbol(context, name);
    if (!body) return false;
    if (!body->size || body->size % ANIMATION_RECORD_BYTES) {
        set_error(context->error, context->error_size, "invalid body animation size");
        return false;
    }
    size_t records = body->size / ANIMATION_RECORD_BYTES;
    if (records > MAX_FRAMES) records = MAX_FRAMES;
    *count = 0;
    for (size_t index = 0; index < records; ++index) {
        const uint8_t *record = slice(context, body->address +
            (uint32_t)(index * ANIMATION_RECORD_BYTES), ANIMATION_RECORD_BYTES,
            "body animation");
        if (!record) return false;
        if (!record[12]) break;
        durations[(*count)++] = record[12];
    }
    if (!*count) {
        set_error(context->error, context->error_size, "body animation has no frames");
        return false;
    }
    return true;
}

/* The arm cannon position offset of SamusUpdateArmCannonPositionOffset. */
static bool muzzle_offset(const Context *context, const char *name, size_t frame,
                          int *x, int *y) {
    const FusionMzmRomSymbol *cannon = require_symbol(context, name);
    const uint8_t *record = cannon ? slice(context, cannon->address +
        (uint32_t)(frame * CANNON_RECORD_BYTES), CANNON_RECORD_BYTES,
        "cannon record") : NULL;
    const uint8_t *offset = record ? slice(context, read_u32(record), 4,
                                           "cannon offset") : NULL;
    if (!offset) return false;
    int raw_y = read_u16(offset), raw_x = read_u16(offset + 2);
    *y = (raw_y & 0x80 ? raw_y - 0x100 : raw_y) + 1;
    *x = raw_x & 0x100 ? raw_x - 0x200 : raw_x;
    return true;
}

static bool palette_colors(const Context *context, const char *name, uint8_t rgb[][3],
                           size_t count) {
    const FusionMzmRomSymbol *palette = require_symbol(context, name);
    if (!palette) return false;
    if (palette->size < count * 2) {
        set_error(context->error, context->error_size, "palette symbol too small: %s", name);
        return false;
    }
    const uint8_t *raw = slice(context, palette->address, count * 2, name);
    if (!raw) return false;
    for (size_t index = 0; index < count; ++index) bgr555(read_u16(raw + index * 2), rgb[index]);
    return true;
}

static void variant_key(const Variant *variant, const char *suffix, char *key,
                        size_t size) {
    snprintf(key, size, "%s/%s/%s/%s%s", families[variant->family], variant->table,
             variant->selector, variant->side ? "left" : "right", suffix);
}

static bool add_sequence(SamusSequence **sequences, size_t *count, size_t *capacity) {
    if (*count == *capacity) {
        size_t next = *capacity ? *capacity * 2 : 256;
        SamusSequence *grown = realloc(*sequences, next * sizeof *grown);
        if (!grown) return false;
        *sequences = grown;
        *capacity = next;
    }
    memset(&(*sequences)[*count], 0, sizeof **sequences);
    ++*count;
    return true;
}

/* Every variant's frames for both arm cannon graphics into the library. */
static bool compose_samus(const Context *context, FusionSpriteLibrary *library,
                          const Variant *variants, size_t variant_count,
                          SamusSequence **sequences, size_t *sequence_count) {
    static uint8_t canvas[SAMUS_CANVAS_WIDTH * SAMUS_CANVAS_HEIGHT];
    uint8_t previews[FAMILY_COUNT][PREVIEW_COLORS * 3];
    for (size_t family = 0; family < FAMILY_COUNT; ++family) {
        uint8_t rgb[PREVIEW_COLORS][3];
        const char *palette = NULL;
        for (size_t suit = 0; suit < VISUAL_SUIT_COUNT; ++suit)
            if (!strcmp(visual_suits[suit].suit, families[family]))
                palette = visual_suits[suit].palette;
        if (!palette_colors(context, palette, rgb, PREVIEW_COLORS)) return false;
        memcpy(previews[family], rgb, sizeof rgb);
        memset(previews[family], 0, 3);
    }
    size_t capacity = 0;
    for (size_t index = 0; index < variant_count; ++index) {
        const Variant *variant = &variants[index];
        uint8_t durations[MAX_FRAMES];
        size_t frames = 0;
        if (!body_durations(context, variant->body, durations, &frames)) goto failed;
        bool any_cannon = false;
        for (int pass = 0; pass < 2; ++pass) {
            bool armed = pass == 1;
            if (armed && (!variant->has_armed || !any_cannon)) break;
            if (!add_sequence(sequences, sequence_count, &capacity)) {
                set_error(context->error, context->error_size,
                          "cannot allocate the Samus sequences");
                return false;
            }
            SamusSequence *sequence = &(*sequences)[*sequence_count - 1];
            variant_key(variant, armed ? "/armed" : "", sequence->key, sizeof sequence->key);
            sequence->body = variant->body;
            sequence->cannon = variant->cannon;
            sequence->has_gfx = armed ? variant->has_armed : variant->has_gfx;
            sequence->gfx[0] = armed ? variant->armed[0] : variant->gfx[0];
            sequence->gfx[1] = armed ? variant->armed[1] : variant->gfx[1];
            sequence->frames = frames;
            sequence->has_muzzle = variant->cannon != NULL;
            for (size_t family = 0; family < VISUAL_SUIT_COUNT; ++family)
                if (!strcmp(visual_suits[family].suit, families[variant->family]))
                    sequence->preview = visual_suits[family].palette;
            if (!fusion_sprite_library_begin(library, context->error, context->error_size))
                return false;
            for (size_t frame = 0; frame < frames; ++frame) {
                uint8_t duration = 0;
                size_t parts = 0;
                if (!render_samus(context, variant, frame, armed, canvas, &duration, &parts))
                    goto failed;
                if (!pass && parts) any_cannon = true;
                sequence->parts[frame] = (uint8_t)parts;
                if (variant->cannon &&
                    !muzzle_offset(context, variant->cannon, frame,
                                   &sequence->muzzle[frame][0],
                                   &sequence->muzzle[frame][1]))
                    goto failed;
                uint8_t *bmp = NULL;
                size_t bmp_size = 0;
                int left = 0, top = 0;
                bool ok = fusion_sprite_bmp8_crop(
                    canvas, SAMUS_CANVAS_WIDTH, SAMUS_CANVAS_HEIGHT, SAMUS_ORIGIN_X,
                    SAMUS_ORIGIN_Y, previews[variant->family], PREVIEW_COLORS, &bmp,
                    &bmp_size, &left, &top, context->error, context->error_size) &&
                    fusion_sprite_library_add(library, sequence->key, frame, duration,
                                              left, top + DRAW_Y_OFFSET, bmp, bmp_size,
                                              context->error, context->error_size);
                free(bmp);
                if (!ok) goto failed;
            }
        }
        continue;
    failed: {
            char label[192], reason[256];
            variant_key(variant, "", label, sizeof label);
            snprintf(reason, sizeof reason, "%s", context->error ? context->error : "");
            set_error(context->error, context->error_size,
                      "unresolved native Samus variant %s: %s", label, reason);
            return false;
        }
    }
    return true;
}

static int compare_sequences(const void *left, const void *right) {
    const SamusSequence *const *a = left, *const *b = right;
    return strcmp((*a)->key, (*b)->key);
}

static const char *json_string(const char *value, char *buffer, size_t size) {
    if (!value) return "null";
    snprintf(buffer, size, "\"%s\"", value);
    return buffer;
}

static bool write_samus_sequences(Text *text, const SamusSequence **sorted,
                                  size_t count) {
    if (!text_append(text, "{\n")) return false;
    for (size_t index = 0; index < count; ++index) {
        const SamusSequence *sequence = sorted[index];
        char cannon[160];
        if (!text_append(text, "  \"%s\": {\n    \"body\": \"%s\",\n    \"cannon\": %s,\n"
                         "    \"cannon_gfx\": ", sequence->key, sequence->body,
                         json_string(sequence->cannon, cannon, sizeof cannon)))
            return false;
        if (sequence->has_gfx
                ? !text_append(text, "[\n      \"%s\",\n      \"%s\"\n    ],\n",
                               sequence->gfx[0], sequence->gfx[1])
                : !text_append(text, "[],\n"))
            return false;
        if (!text_append(text, "    \"cannon_parts\": [\n")) return false;
        for (size_t frame = 0; frame < sequence->frames; ++frame)
            if (!text_append(text, "      %u%s\n", sequence->parts[frame],
                             frame + 1 < sequence->frames ? "," : ""))
                return false;
        if (!text_append(text, "    ],\n    \"muzzle\": ")) return false;
        if (sequence->has_muzzle) {
            if (!text_append(text, "[\n")) return false;
            for (size_t frame = 0; frame < sequence->frames; ++frame)
                if (!text_append(text, "      [\n        %d,\n        %d\n      ]%s\n",
                                 sequence->muzzle[frame][0], sequence->muzzle[frame][1],
                                 frame + 1 < sequence->frames ? "," : ""))
                    return false;
            if (!text_append(text, "    ],\n")) return false;
        } else if (!text_append(text, "null,\n")) {
            return false;
        }
        if (!text_append(text, "    \"preview_palette\": \"%s\"\n  }%s\n",
                         sequence->preview, index + 1 < count ? "," : ""))
            return false;
    }
    return text_append(text, "}\n");
}

static bool write_cannon_offsets(Text *text, const SamusSequence **sorted, size_t count) {
    if (!text_append(text, "schema\tmetroidvania-samus-cannon-offsets-v1\n")) return false;
    for (size_t index = 0; index < count; ++index) {
        const SamusSequence *sequence = sorted[index];
        if (ends_with(sequence->key, "/armed") || !sequence->has_muzzle) continue;
        for (size_t frame = 0; frame < sequence->frames; ++frame)
            if (!text_append(text, "%s\t%zu\t%d\t%d\n", sequence->key, frame,
                             sequence->muzzle[frame][0], sequence->muzzle[frame][1]))
                return false;
    }
    return true;
}

static bool write_palettes(const Context *context, Text *text) {
    if (!text_append(text, "schema\tmetroidvania-samus-palettes-v1\n")) return false;
    for (size_t index = 0; index < context->count; ++index) {
        const FusionMzmRomSymbol *entry = context->by_name[index];
        const char *name = entry->name;
        if (!starts_with(name, "sSamusPal_") || ends_with(name, "Pointers")) continue;
        /* sSamusPal_<letters>_<word characters>. */
        const char *suit = name + strlen("sSamusPal_"), *cursor = suit;
        while ((*cursor >= 'A' && *cursor <= 'Z') || (*cursor >= 'a' && *cursor <= 'z'))
            ++cursor;
        if (cursor == suit || *cursor != '_' || !cursor[1]) continue;
        const char *kind = cursor + 1;
        bool word = true;
        for (const char *c = kind; *c; ++c)
            if (!((*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') ||
                  (*c >= '0' && *c <= '9') || *c == '_'))
                word = false;
        if (!word) continue;
        if (entry->size % 32) {
            set_error(context->error, context->error_size,
                      "palette array is not whole rows: %s", name);
            return false;
        }
        const uint8_t *raw = slice(context, entry->address, entry->size, name);
        if (!raw) return false;
        for (size_t row = 0; row < entry->size / 32; ++row) {
            if (!text_append(text, "%.*s\t%s\t%zu", (int)(cursor - suit), suit, kind, row))
                return false;
            for (size_t color = 0; color < 16; ++color) {
                uint8_t rgb[3];
                bgr555(read_u16(raw + (row * 16 + color) * 2), rgb);
                if (!text_append(text, "\t%02x%02x%02x", rgb[0], rgb[1], rgb[2]))
                    return false;
            }
            if (!text_append(text, "\n")) return false;
        }
    }
    return true;
}

static bool has_key(const SamusSequence **sorted, size_t count, const char *key) {
    size_t low = 0, high = count;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        int order = strcmp(sorted[middle]->key, key);
        if (!order) return true;
        if (order < 0) low = middle + 1;
        else high = middle;
    }
    return false;
}

static bool is_suitless_unavailable(size_t suit, const char *action) {
    if (strcmp(visual_suits[suit].suit, "Suitless")) return false;
    for (size_t index = 0; index < COUNT(suitless_unavailable); ++index)
        if (!strcmp(suitless_unavailable[index], action)) return true;
    return false;
}

/* The semantic animation registry: gameplay states to exact native keys. */
static bool write_animation_map(Text *map, Text *missing, Text *unavailable,
                                size_t *rows, const SamusSequence **sorted, size_t count) {
    *rows = 0;
    if (!text_append(map, "schema\tmetroidvania-samus-animation-map-v1\n"
                          "action\tsuit\tfacing\taim\tmode\tkey\n"))
        return false;
    for (size_t suit = 0; suit < VISUAL_SUIT_COUNT; ++suit) {
        const char *family = visual_suits[suit].family;
        for (size_t side = 0; side < 2; ++side) {
            const char *facing = side ? "right" : "left";
            for (size_t action = 0; action < COUNT(aim_actions); ++action) {
                for (size_t aim = 0; aim < COUNT(map_aims); ++aim) {
                    char key[192];
                    snprintf(key, sizeof key, "%s/%s/%s/%s", family,
                             aim_actions[action][1], map_aims[aim][1], facing);
                    if (has_key(sorted, count, key)) {
                        if (!text_append(map, "%s\t%s\t%s\t%s\t%s\t%s\n",
                                         aim_actions[action][0], visual_suits[suit].suit,
                                         facing, map_aims[aim][0], aim_actions[action][2],
                                         key))
                            return false;
                        ++*rows;
                    } else if (aim < 3) {
                        Text *list = is_suitless_unavailable(suit, aim_actions[action][0])
                            ? unavailable : missing;
                        if (!text_append(list, "%s\n      \"%s/%s/%s/%s\"",
                                         list->size ? "," : "", aim_actions[action][0],
                                         visual_suits[suit].suit, facing, map_aims[aim][0]))
                            return false;
                    }
                }
            }
            for (size_t action = 0; action < COUNT(simple_actions); ++action) {
                char key[192];
                snprintf(key, sizeof key, "%s/%s/%s/%s", family, simple_actions[action][1],
                         simple_actions[action][2], facing);
                if (has_key(sorted, count, key)) {
                    if (!text_append(map, "%s\t%s\t%s\tnone\t%s\t%s\n",
                                     simple_actions[action][0], visual_suits[suit].suit,
                                     facing, simple_actions[action][3], key))
                        return false;
                    ++*rows;
                } else {
                    Text *list = is_suitless_unavailable(suit, simple_actions[action][0])
                        ? unavailable : missing;
                    if (!text_append(list, "%s\n      \"%s/%s/%s/none\"",
                                     list->size ? "," : "", simple_actions[action][0],
                                     visual_suits[suit].suit, facing))
                        return false;
                }
            }
        }
    }
    return true;
}

static bool write_output(const char *output, const char *name, const Text *text,
                         char *error, size_t error_size) {
    char path[FUSION_EXPORT_PATH_CAPACITY];
    if (!fusion_export_join(path, output, name)) {
        set_error(error, error_size, "sprite-library path is too long");
        return false;
    }
    return fusion_export_write_file(path, (const uint8_t *)(text->data ? text->data : ""),
                                    text->size, error, error_size);
}

static size_t beam_set_of(const char *table) {
    /* Missiles and bombs use the common graphics with the Power Beam row. */
    const char *name = table + 1;
    if (starts_with(name, "Charged")) name += strlen("Charged");
    for (size_t beam = 0; beam < BEAM_SET_COUNT; ++beam)
        if (starts_with(name, beam_sets[beam].name)) return beam;
    return 0;
}

/* OBJ VRAM and palette of one beam set; ``row`` is its sBeamPal row. */
static bool stage_shots(const Context *context, size_t beam, unsigned row,
                        ShotStage *stage) {
    memset(stage, 0, sizeof *stage);
    const FusionMzmRomSymbol *common = require_symbol(context, "sCommonSpritesGfx");
    const uint8_t *data = common ? slice(context, common->address, common->size,
                                         "common sprite graphics") : NULL;
    if (!data) return false;
    if (common->size > OBJ_VRAM_BYTES - COMMON_GFX_VRAM) {
        set_error(context->error, context->error_size, "common sprite graphics overflow VRAM");
        return false;
    }
    memcpy(stage->vram + COMMON_GFX_VRAM, data, common->size);
    static const char *const parts[] = {"Top", "Bottom", "Charged_Top", "Charged_Bottom"};
    static const uint32_t bases[] = {0x1000, 0x1400, 0x1800, 0x1C00};
    for (size_t part = 0; part < 4; ++part) {
        char name[96];
        snprintf(name, sizeof name, "%s_%s", beam_sets[beam].graphics, parts[part]);
        size_t length = beam == 5 && part == 0 ? BEAM_BLOCK_BYTES / 2 : BEAM_BLOCK_BYTES;
        const FusionMzmRomSymbol *gfx = require_symbol(context, name);
        data = gfx ? slice(context, gfx->address, length, name) : NULL;
        if (!data) return false;
        memcpy(stage->vram + bases[part], data, length);
    }
    const FusionMzmRomSymbol *palette = require_symbol(context, "sCommonSpritesPal");
    data = palette ? slice(context, palette->address, palette->size,
                           "common sprite palette") : NULL;
    if (!data) return false;
    if (palette->size / 2 > 256 - 32) {
        set_error(context->error, context->error_size, "common sprite palette overflow");
        return false;
    }
    for (size_t index = 0; index < palette->size / 2; ++index) {
        bgr555(read_u16(data + index * 2), stage->rgb[32 + index]);
        stage->loaded[32 + index] = true;
    }
    const FusionMzmRomSymbol *beams = require_symbol(context, "sBeamPal");
    data = beams ? slice(context, beams->address, beams->size, "beam palette") : NULL;
    if (!data) return false;
    if ((row * 16 + 6) * 2 > beams->size) {
        set_error(context->error, context->error_size, "truncated beam palette");
        return false;
    }
    for (size_t index = 0; index < 6; ++index) {
        bgr555(read_u16(data + (row * 16 + index) * 2), stage->rgb[32 + index]);
        stage->loaded[32 + index] = true;
    }
    return true;
}

typedef struct {
    uint32_t pointer;
    uint8_t duration;
} ShotFrame;

/* (OAM pointer, duration) entries up to the FrameData terminator. */
static bool shot_frames(const Context *context, const char *table, ShotFrame *frames,
                        size_t *count) {
    const FusionMzmRomSymbol *entry = require_symbol(context, table);
    if (!entry) return false;
    *count = 0;
    for (size_t index = 0; index < entry->size / 8; ++index) {
        const uint8_t *record = slice(context, entry->address + (uint32_t)(index * 8), 5,
                                      table);
        if (!record) return false;
        uint32_t pointer = read_u32(record);
        if (!pointer && !record[4]) break;
        if (!record[4]) {
            set_error(context->error, context->error_size,
                      "invalid frame duration in %s", table);
            return false;
        }
        if (*count == MAX_FRAMES) {
            set_error(context->error, context->error_size, "too many frames in %s", table);
            return false;
        }
        frames[(*count)++] = (ShotFrame){pointer, record[4]};
    }
    if (!*count) {
        set_error(context->error, context->error_size, "%s has no frames", table);
        return false;
    }
    return true;
}

/* One projectile frame relative to its position; ProjectileDraw mirrors the
 * parts around that point for X/Y flips. */
static bool render_shot(const Context *context, const ShotStage *stage, uint32_t pointer,
                        size_t flip, uint8_t *canvas) {
    const uint8_t *data = slice(context, pointer, 2, "projectile OAM");
    if (!data) return false;
    size_t count = read_u16(data);
    /* At most the 128 hardware OAM entries (the glass tube uses over 32). */
    if (count < 1 || count > MAX_OAM) {
        set_error(context->error, context->error_size, "invalid projectile OAM part count");
        return false;
    }
    const uint8_t *raw = slice(context, pointer + 2, count * 6, "projectile OAM entries");
    if (!raw) return false;
    OamEntry entries[MAX_OAM];
    for (size_t index = 0; index < count; ++index)
        if (!unpack_oam(context, raw + index * 6, &entries[index])) return false;
    bool flip_x = flip & 1, flip_y = flip & 2;
    memset(canvas, 0, SHOT_CANVAS_WIDTH * SHOT_CANVAS_HEIGHT * 4);
    for (size_t index = count; index-- > 0;) {
        const OamEntry *entry = &entries[index];
        for (int iy = 0; iy < entry->h; ++iy) {
            for (int ix = 0; ix < entry->w; ++ix) {
                bool outside = false;
                uint8_t color = vram_index(stage->vram, entry, ix, iy, &outside);
                if (outside) {
                    set_error(context->error, context->error_size,
                              "projectile tile outside OBJ VRAM");
                    return false;
                }
                if (!color) continue;
                size_t slot = entry->bank * 16 + color;
                if (!stage->loaded[slot]) {
                    set_error(context->error, context->error_size,
                              "projectile uses an unloaded palette row");
                    return false;
                }
                int x = entry->x + ix, y = entry->y + iy;
                if (flip_x) x = -1 - x;
                if (flip_y) y = -1 - y;
                uint8_t *pixel = canvas + ((size_t)(y + SHOT_ORIGIN_Y) * SHOT_CANVAS_WIDTH +
                                           (size_t)(x + SHOT_ORIGIN_X)) * 4;
                memcpy(pixel, stage->rgb[slot], 3);
                pixel[3] = 255;
            }
        }
    }
    return true;
}

static bool add_shot(ShotSequence **sequences, size_t *count, size_t *capacity,
                     const ShotSequence *sequence) {
    if (*count == *capacity) {
        size_t next = *capacity ? *capacity * 2 : 256;
        ShotSequence *grown = realloc(*sequences, next * sizeof *grown);
        if (!grown) return false;
        *sequences = grown;
        *capacity = next;
    }
    (*sequences)[(*count)++] = *sequence;
    return true;
}

static bool emit_shot(const Context *context, FusionSpriteLibrary *library,
                      const ShotStage *stage, const ShotFrame *frames, size_t count,
                      size_t flip, const char *key, uint8_t *canvas) {
    if (!fusion_sprite_library_begin(library, context->error, context->error_size))
        return false;
    for (size_t frame = 0; frame < count; ++frame) {
        uint8_t *bmp = NULL;
        size_t bmp_size = 0;
        int left = 0, top = 0;
        bool ok = render_shot(context, stage, frames[frame].pointer, flip, canvas) &&
            fusion_sprite_bmp32_crop(canvas, SHOT_CANVAS_WIDTH, SHOT_CANVAS_HEIGHT,
                                     SHOT_ORIGIN_X, SHOT_ORIGIN_Y, &bmp, &bmp_size,
                                     &left, &top, context->error, context->error_size) &&
            fusion_sprite_library_add(library, key, frame, frames[frame].duration, left,
                                      top, bmp, bmp_size, context->error,
                                      context->error_size);
        free(bmp);
        if (!ok) return false;
    }
    return true;
}

static int compare_shots(const void *left, const void *right) {
    const ShotSequence *const *a = left, *const *b = right;
    return strcmp((*a)->key, (*b)->key);
}

static bool write_shot_sequences(Text *text, ShotSequence *sequences, size_t count) {
    const ShotSequence **sorted = malloc((count ? count : 1) * sizeof *sorted);
    if (!sorted) return false;
    for (size_t index = 0; index < count; ++index) sorted[index] = &sequences[index];
    qsort(sorted, count, sizeof *sorted, compare_shots);
    bool ok = text_append(text, "{\n");
    for (size_t index = 0; ok && index < count; ++index) {
        const ShotSequence *sequence = sorted[index];
        const char *comma = index + 1 < count ? "," : "";
        if (sequence->variant)
            ok = text_append(text, "  \"%s\": {\n    \"flip\": \"%s\",\n"
                             "    \"table\": \"%s\",\n    \"variant\": \"%s\"\n  }%s\n",
                             sequence->key, sequence->flip, sequence->table,
                             sequence->variant, comma);
        else if (sequence->ice)
            ok = text_append(text, "  \"%s\": {\n    \"beam_set\": \"%s\",\n"
                             "    \"flip\": \"%s\",\n    \"palette_row\": %d,\n"
                             "    \"table\": \"%s\"\n  }%s\n", sequence->key,
                             sequence->beam_set, sequence->flip, ICE_ROW, sequence->table,
                             comma);
        else
            ok = text_append(text, "  \"%s\": {\n    \"beam_set\": \"%s\",\n"
                             "    \"flip\": \"%s\",\n    \"table\": \"%s\"\n  }%s\n",
                             sequence->key, sequence->beam_set, sequence->flip,
                             sequence->table, comma);
    }
    free(sorted);
    return ok && text_append(text, "}\n");
}

/* The projectile library: every FrameData table, then the beam particles. */
static bool produce_projectiles(const Context *context, const char *output,
                                FusionSpriteLibrarySummary *summary) {
    FusionSpriteLibrary library = {0};
    ShotSequence *sequences = NULL;
    size_t count = 0, capacity = 0;
    ShotStage *stages = calloc(BEAM_SET_COUNT + 2, sizeof *stages);
    ShotStage *particle_stage = malloc(sizeof *particle_stage);
    uint8_t *canvas = malloc(SHOT_CANVAS_WIDTH * SHOT_CANVAS_HEIGHT * 4);
    bool staged[BEAM_SET_COUNT + 2] = {false};
    Text text = {0};
    bool ok = stages && particle_stage && canvas &&
        fusion_sprite_library_init(&library, output, projectile_prefix,
                                   context->error, context->error_size);
    if (!stages || !particle_stage || !canvas)
        set_error(context->error, context->error_size, "cannot allocate projectile staging");
    for (size_t table = 0; ok && table < fusion_mzm_projectile_frame_table_count; ++table) {
        const char *name = fusion_mzm_projectile_frame_tables[table];
        size_t beam = beam_set_of(name);
        ShotFrame frames[MAX_FRAMES];
        size_t frame_count = 0;
        ok = (staged[beam] || (staged[beam] = stage_shots(context, beam,
                                                          beam_sets[beam].row,
                                                          &stages[beam]))) &&
             shot_frames(context, name, frames, &frame_count);
        /* The Ice Beam recolours the Wave and Plasma graphics. */
        size_t ice = beam == 3 ? BEAM_SET_COUNT : beam == 4 ? BEAM_SET_COUNT + 1 : 0;
        for (size_t pass = 0; ok && pass < (ice ? 2u : 1u); ++pass) {
            size_t slot = pass ? ice : beam;
            if (pass && !staged[slot])
                ok = staged[slot] = stage_shots(context, beam, ICE_ROW, &stages[slot]);
            for (size_t flip = 0; ok && flip < 4; ++flip) {
                ShotSequence sequence = {.table = name, .beam_set = beam_sets[beam].name,
                                         .flip = flip_names[flip], .ice = pass == 1};
                snprintf(sequence.key, sizeof sequence.key, "Projectile/%s/%s%s",
                         name + 1, flip_names[flip], pass ? "/ice" : "");
                ok = emit_shot(context, &library, &stages[slot], frames, frame_count,
                               flip, sequence.key, canvas) &&
                     add_shot(&sequences, &count, &capacity, &sequence);
            }
        }
        if (!ok) {
            char reason[256];
            snprintf(reason, sizeof reason, "%s", context->error);
            set_error(context->error, context->error_size,
                      "unresolved projectile table %s: %s", name, reason);
        }
    }
    for (size_t variant = 0; ok && variant < COUNT(beam_variants); ++variant) {
        size_t beam = beam_variants[variant].beam;
        ok = stage_shots(context, beam, beam_variants[variant].row, particle_stage);
        for (size_t table = 0; ok && table < COUNT(beam_particles); ++table) {
            const char *name = beam_particles[table];
            if (beam == 5 && (!strcmp(name, "sParticleChargedIceBeamTrailOam") ||
                              !strcmp(name, "sParticleChargedWaveBeamTrailOam")))
                continue;
            const char *only = NULL;
            for (size_t index = 0; index < COUNT(particle_beam_sets); ++index)
                if (!strcmp(particle_beam_sets[index][0], name))
                    only = particle_beam_sets[index][1];
            if (only && strcmp(only, beam_sets[beam].name)) continue;
            ShotFrame frames[MAX_FRAMES];
            size_t frame_count = 0;
            ok = shot_frames(context, name, frames, &frame_count);
            for (size_t flip = 0; ok && flip < 2; ++flip) {
                ShotSequence sequence = {.table = name, .variant = beam_variants[variant].name,
                                         .flip = flip_names[flip]};
                snprintf(sequence.key, sizeof sequence.key, "Particle/%s/%s/%s", name + 1,
                         beam_variants[variant].name, flip_names[flip]);
                ok = emit_shot(context, &library, particle_stage, frames, frame_count,
                               flip, sequence.key, canvas) &&
                     add_shot(&sequences, &count, &capacity, &sequence);
            }
            if (!ok) {
                char reason[256];
                snprintf(reason, sizeof reason, "%s", context->error);
                set_error(context->error, context->error_size,
                          "unresolved beam particle %s/%s: %s", name,
                          beam_variants[variant].name, reason);
            }
        }
    }
    if (ok) ok = fusion_sprite_library_finish(&library, summary, context->error,
                                              context->error_size);
    if (ok) {
        ok = write_shot_sequences(&text, sequences, count);
        if (!ok) set_error(context->error, context->error_size,
                           "cannot serialize projectile sequences");
    }
    if (ok) ok = write_output(output, "sequences.json", &text, context->error,
                              context->error_size);
    fusion_sprite_library_free(&library);
    free(sequences);
    free(stages);
    free(particle_stage);
    free(canvas);
    free(text.data);
    return ok;
}

static bool write_manifest(Text *text, const FusionSpriteLibrarySummary *samus,
                           const FusionSpriteLibrarySummary *shots, size_t variant_count,
                           size_t sequence_count, size_t frame_count, size_t rows,
                           const Text *missing, const Text *unavailable,
                           const size_t family_counts[FAMILY_COUNT]) {
    if (!text_append(text, "{\n  \"animation_map\": {\n    \"actions\": %zu,\n"
                     "    \"graphics_by_suit\": {\n"
                     "      \"FullSuit\": \"FullSuit\",\n"
                     "      \"GravitySuit\": \"FullSuit\",\n"
                     "      \"PowerSuit\": \"PowerSuit\",\n"
                     "      \"Suitless\": \"Suitless\",\n"
                     "      \"VariaSuit\": \"PowerSuit\"\n"
                     "    },\n    \"missing\": ",
                     COUNT(aim_actions) + COUNT(simple_actions)))
        return false;
    if (missing->size ? !text_append(text, "[%s\n    ],\n", missing->data)
                      : !text_append(text, "[],\n"))
        return false;
    if (!text_append(text, "    \"palette_by_suit\": {\n"
                     "      \"FullSuit\": \"sSamusPal_FullSuit_Default\",\n"
                     "      \"GravitySuit\": \"sSamusPal_GravitySuit_Default\",\n"
                     "      \"PowerSuit\": \"sSamusPal_PowerSuit_Default\",\n"
                     "      \"Suitless\": \"sSamusPal_Suitless_Default\",\n"
                     "      \"VariaSuit\": \"sSamusPal_VariaSuit_Default\"\n"
                     "    },\n    \"rows\": %zu,\n"
                     "    \"schema\": \"metroidvania-samus-animation-map-report-v2\",\n"
                     "    \"unavailable\": ", rows))
        return false;
    if (unavailable->size ? !text_append(text, "[%s\n    ]\n", unavailable->data)
                          : !text_append(text, "[]\n"))
        return false;
    return text_append(text,
        "  },\n  \"composition\": {\n    \"frames\": %zu,\n    \"sequences\": %zu,\n"
        "    \"table_problems\": [],\n    \"variants\": %zu\n  },\n"
        "  \"families\": {\n    \"FullSuit\": %zu,\n    \"PowerSuit\": %zu,\n"
        "    \"Suitless\": %zu\n  },\n"
        "  \"frames\": %zu,\n"
        "  \"index_schema\": \"metroidvania-sprite-index-v1\",\n"
        "  \"note\": \"Private native extraction; source ROM data is never redistributed.\",\n"
        "  \"object_store\": \"sha256\",\n"
        "  \"projectiles\": {\n    \"frames\": %zu,\n"
        "    \"index_schema\": \"metroidvania-sprite-index-v1\",\n"
        "    \"removed_stale_objects\": %zu,\n    \"sequences\": %zu,\n"
        "    \"tables\": %zu,\n    \"unique_bmps\": %zu\n  },\n"
        "  \"removed_stale_objects\": %zu,\n"
        "  \"schema\": \"metroidvania-mzm-samus-runtime-v3\",\n"
        "  \"sequences\": %zu,\n  \"unique_bmps\": %zu\n}\n",
        frame_count, sequence_count, variant_count, family_counts[1], family_counts[0],
        family_counts[2], samus->frames, shots->frames, shots->removed_stale_objects,
        shots->sequences, fusion_mzm_projectile_frame_table_count, shots->unique_bmps,
        samus->removed_stale_objects, samus->sequences, samus->unique_bmps);
}

bool fusion_mzm_export_samus(const FusionRom *rom, const uint8_t *elf, size_t elf_size,
                             const char *samus_output, const char *projectile_output,
                             FusionMzmSamusExportSummary *summary,
                             char *error, size_t error_size) {
    if (summary) *summary = (FusionMzmSamusExportSummary){0};
    if (!rom || rom->game != FUSION_ASSET_GAME_METROID || !rom->data || !elf ||
        !samus_output || !projectile_output || !summary) {
        set_error(error, error_size, "invalid Samus export request");
        return false;
    }
    if (error && error_size) error[0] = '\0';
    FusionMzmRomSymbolTable symbols = {0};
    Context context = {rom, NULL, 0, error, error_size};
    Variant *variants = NULL;
    SamusSequence *sequences = NULL;
    const SamusSequence **sorted = NULL;
    FusionSpriteLibrary library = {0};
    FusionSpriteLibrarySummary samus = {0}, shots = {0};
    Text map = {0}, missing = {0}, unavailable = {0}, offsets = {0}, palettes = {0},
         json = {0}, manifest = {0};
    size_t variant_count = 0, sequence_count = 0, rows = 0;
    bool ok = fusion_mzm_read_rom_symbols(rom, elf, elf_size, &symbols, error, error_size);
    if (ok) {
        context.by_name = malloc((symbols.count ? symbols.count : 1) *
                                 sizeof *context.by_name);
        ok = context.by_name != NULL;
        if (!ok) set_error(error, error_size, "cannot index the ELF symbols");
    }
    if (ok) {
        for (size_t index = 0; index < symbols.count; ++index)
            context.by_name[index] = &symbols.entries[index];
        context.count = symbols.count;
        qsort(context.by_name, context.count, sizeof *context.by_name, compare_names);
        ok = verify_symbols(&context, elf, elf_size) &&
             build_variants(&variants, &variant_count, error, error_size);
        if (!ok && error && !error[0]) set_error(error, error_size, "cannot list Samus variants");
    }
    if (ok) ok = fusion_sprite_library_init(&library, samus_output, samus_prefix,
                                            error, error_size) &&
                 compose_samus(&context, &library, variants, variant_count, &sequences,
                               &sequence_count) &&
                 fusion_sprite_library_finish(&library, &samus, error, error_size);
    size_t frame_count = 0, family_counts[FAMILY_COUNT] = {0};
    if (ok) {
        sorted = malloc((sequence_count ? sequence_count : 1) * sizeof *sorted);
        ok = sorted != NULL;
        if (!ok) set_error(error, error_size, "cannot sort the Samus sequences");
    }
    if (ok) {
        for (size_t index = 0; index < sequence_count; ++index) {
            sorted[index] = &sequences[index];
            frame_count += sequences[index].frames;
            for (size_t family = 0; family < FAMILY_COUNT; ++family) {
                size_t length = strlen(families[family]);
                if (!strncmp(sequences[index].key, families[family], length) &&
                    sequences[index].key[length] == '/')
                    ++family_counts[family];
            }
        }
        qsort(sorted, sequence_count, sizeof *sorted, compare_sequences);
        ok = write_animation_map(&map, &missing, &unavailable, &rows, sorted,
                                 sequence_count) &&
             write_cannon_offsets(&offsets, sorted, sequence_count) &&
             write_samus_sequences(&json, sorted, sequence_count);
        if (!ok) set_error(error, error_size, "cannot serialize the Samus metadata");
    }
    if (ok) ok = write_palettes(&context, &palettes) &&
                 write_output(samus_output, "animation_map.tsv", &map, error, error_size) &&
                 write_output(samus_output, "cannon_offsets.tsv", &offsets, error,
                              error_size) &&
                 write_output(samus_output, "palettes.tsv", &palettes, error, error_size) &&
                 write_output(samus_output, "sequences.json", &json, error, error_size) &&
                 produce_projectiles(&context, projectile_output, &shots);
    if (ok) {
        ok = write_manifest(&manifest, &samus, &shots, variant_count, sequence_count,
                            frame_count, rows, &missing, &unavailable, family_counts);
        if (!ok) set_error(error, error_size, "cannot serialize the Samus manifest");
    }
    if (ok) ok = write_output(samus_output, "manifest.json", &manifest, error, error_size);
    if (ok) {
        *summary = (FusionMzmSamusExportSummary){
            samus.sequences, samus.frames, samus.unique_bmps, rows,
            shots.sequences, shots.frames, shots.unique_bmps};
        if (error && error_size) error[0] = '\0';
    } else if (error && error_size && !error[0]) {
        set_error(error, error_size, "cannot export the Samus libraries");
    }
    fusion_sprite_library_free(&library);
    fusion_mzm_free_symbols(&symbols);
    free(context.by_name);
    free(variants);
    free(sequences);
    free(sorted);
    free(map.data); free(missing.data); free(unavailable.data); free(offsets.data);
    free(palettes.data); free(json.data); free(manifest.data);
    return ok;
}
