/* SPDX-License-Identifier: GPL-3.0-only */
/* The private Zero Mission enemy sprite library and sprite tables (formerly
 * scripts/mzm_sprites.py).
 *
 * Sprite graphics follow SpriteLoadSpriteset and SpriteDraw: the LZ77
 * graphics of sSpritesGraphicsPointers go to OBJ VRAM 0x4000 (graphics slot
 * 0), the palette rows of sSpritesPalettePointers to OBJ rows 8 and up, and
 * each FrameData table of the sprite's data file is drawn relative to the
 * sprite position, mirrored around it for the X and Y flip statuses. Only
 * sprites whose AI is ported are exported, with the palette row their AI
 * selects and the palette variants it applies (frozen and stun rows, flashing
 * rows, damage tints, dynamic palettes). Enemy drops, the water drop and the
 * particles draw from the common sprite graphics
 * (HudGenericLoadCommonSpriteGfx). The message banner is exported once per
 * English message and the area banner once per location text, their text
 * drawn as src/text.c draws it. Frames are cropped to their visible pixels;
 * an empty frame is one black pixel at the origin.
 *
 * metadata/sprites.tsv holds the ROM tables the sprite engine reads: the
 * message banner timings, the random and falling-speed tables, the primary
 * and secondary sprite stats, the signed tables of ported AIs, the haze
 * data, the MultiSpriteData tables and Mecha Ridley's OBJ palette. */
#include "fusion/assets/mzm.h"

#include "export.h"
#include "message_text.h"
#include "sprite_library.h"
#include "sprite_tables.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    ROM_BASE = 0x08000000,
    ROM_LIMIT = 0x0E000000,
    OBJ_VRAM_BYTES = 0x8000,
    SPRITE_VRAM = 0x4000,
    COMMON_GFX_VRAM = 0x800,
    COMMON_PALETTE_ROW = 2,
    MAX_OAM = 128,
    MAX_FRAMES = 512,
    /* OAM X spans -256..318 and Y -128..190; the mirror around the origin
     * extends them to -319..318 and -191..190. */
    CANVAS_WIDTH = 640,
    CANVAS_HEIGHT = 384,
    ORIGIN_X = 320,
    ORIGIN_Y = 192,
    ESCAPE_GATE_VRAM = 0x7800,
    FLASHING_ROWS = 3,
    MECHA_RIDLEY_WEAPON_VRAM = 0x4280,
    RUINS_TEST_DYNAMIC_ROW = 14,
    RUINS_TEST_REFLECTION_ROW = 15,
    LOCATION_TEXTS = 14,        /* LT_BRINSTAR ... LT_STARSHIP */
    LT_SAVE_ROOM = 9,
    LOCATION_BUFFER = 0x800,
};

static const char *const index_prefix = "assets/extracted/metroid/sprites/enemies/runtime";

/* s16 (or small u16) tables an AI reads, written as ``table`` lines. */
static const char *const ai_tables[] = {
    "sSkreeFallingSpeed", "sZeelaEyesFallingFromUpSpeed",
    "sZeelaEyesFallingFromBottomSpeed", "sSpritesFallingCeilingSpeed",
    "sSidehopperHighJumpVelocity", "sSidehopperLowJumpVelocity",
    "sDessgeegaHighJumpYVelocity", "sDessgeegaLowJumpYVelocity", "sReoIdleYMovement",
    "sReoIdleXMovement", "sMellowIdleYVelocity", "sMellowIdleXVelocity",
    "sBaristuteJumpingYVelocity", "sBaristuteFallingYVelocity", "sSpacePirate_2e1070",
    "sImagoAttackingXVelocity", "sImagoDamagedStingerFallingYVelocity",
    "sEscapeShipHoveringYMovement", "sEscapeShipHoveringXMovement",
    "sMechaRidleyFadingPal", "sMechaRidleyGreenGlowPal", "sMechaRidley_8323aaa_Pal",
    "sMechaRidley_8323b42_Pal", "sMechaRidley_8323b4a_Pal", "sMechaRidley_8323b62_Pal",
    "sMellaIdleYMovement", "sMellaIdleXMovement", "sMellaGoingDownYMovement",
    "sMellaGoingUpYMovement", "sMellaMovingXMovement", "sHoltzIdleYVelocity",
    "sSqueeptRisingMovement", "sSqueeptFallingMovement", "sChargeBeamIdleYMovement",
    "sDragonFireballYMovement", "sDragonFireballOamRotation",
    "sSpacePirateWallJumpingVelocity", "sSpacePirate_2e1030",
    "sSpacePirateJumpingVelocity", "sSpacePirate_2e105a", "sDebrisBouncingVelocity",
    "sDebris11YVelocity", "sDebris12YVelocity", "sDebris13YVelocity",
    "sDebris4YVelocity", "sDebris5YVelocity", "sDebris6YVelocity", "sDebris7YVelocity",
    "sDebris8YVelocity", "sGunshipTakingOffYVelocity", "sGunshipFlyingYVelocity",
    "sAtomicIdleYMovement", "sAtomicIdleXMovement", "sPolypProjectileYVelocity",
    "sWorkerRobotSleepingFallingSpeed", "sMetroidSpawningYMovement",
    "sMetroidSpawningXMovement", "sKraidPal", "sRidleyIdleYOffsets",
    "sRidleySpawningSpittingFireballsYOffsets", "sSineTable",
    "sAcidWormSpitCloseYVelocity", "sAcidWormSpitFarYVelocity",
    "sAcidWormSpitMiddleYVelocity", "sAcidWormSwingingMovement",
    "sAcidWormHeadRandomXVelocity", "sDeoremSegmentXVelocity", "sDeoremThornYVelocity",
};

/* u8 [n][2] tables an AI reads, written as ``table`` lines of halfwords
 * (the first byte low, the second high), as the runtime reads them. */
static const char *const byte_pair_tables[] = {"sImagoDynamicPaletteData"};

/* Particle effects and sprite debris drawn from the common sprite graphics
 * (ParticleDraw, SpriteDebrisDraw: no palette row or graphics slot added). */
static const char *const particles[] = {
    "sParticleSpriteSplashWaterSmallOam", "sParticleSpriteSplashWaterBigOam",
    "sParticleSpriteSplashWaterHugeOam", "sParticleSpriteSplashLavaSmallOam",
    "sParticleSpriteSplashLavaBigOam", "sParticleSpriteSplashLavaHugeOam",
    "sParticleSpriteSplashAcidSmallOam", "sParticleSpriteSplashAcidBigOam",
    "sParticleSpriteSplashAcidHugeOam", "sParticleBombOam",
    "sParticleFreezingSpriteWithIceOam", "sParticleFreezingSpriteWithChargedIceBeamOam",
    "sParticleBeamTrailingOam", "sParticleHittingSomethingWithIceBeamOam",
    "sParticleHittingSomethingWithWaveBeamOam",
    "sParticleHittingSomethingWithPlasmaBeamOam", "sParticleMissileTrailOam",
    "sParticleSuperMissileTrailOam", "sParticleChargedLongBeamTrailOam",
    "sParticleChargedIceBeamTrailOam", "sParticleChargedWaveBeamTrailOam",
    "sParticleChargedPlasmaBeamTrailOam", "sParticleChargedFullBeamTrailOam",
    "sParticleSpriteExplosionHugeOam", "sParticleMainBossDeathOam",
    "sParticleSpriteExplosionSmallOam", "sParticleSpriteExplosionMediumOam",
    "sParticleSpriteExplosionBigOam", "sParticleSpriteExplosionSingleThenBigOam",
    "sParticleScrewAttackDestroyedOam", "sParticleShinesparkDestroyedOam",
    "sParticleSpeedboosterDestroyedOam", "sParticleHittingSomethingWithNormalBeamOam",
    "sParticleHittingSomethingWithLongBeamOam", "sParticlePseudoScrewDestroyedOam",
    "sParticleHittingSomethingInvincibleOam", "sParticleHittingSomethingWithMissileOam",
    "sParticleHittingSomethingWithSuperMissileOam", "sParticleSmallDustOam",
    "sParticleMediumDustOam", "sParticleTwoMediumDustOam", "sSpriteDebrisOam",
};

/* MultiSpriteData tables (gSubSpriteData1.pMultiOam) of ported sprites,
 * written as ``multi`` lines: name, frame, timer, then each part's OAM
 * index, y and x offsets. */
static const char *const multi_sprite_data[] = {
    "sChozoStatueMultiSpriteData_Standing", "sChozoStatueMultiSpriteData_Sitting",
    "sChozoStatueMultiSpriteData_Seated", "sMotherBrainMultiSpriteData",
    "sRidleyMultiSpriteData_Idle", "sRidleyMultiSpriteData_SpittingFireballs",
    "sRidleyMultiSpriteData_TurningAroundFirstPart",
    "sRidleyMultiSpriteData_TurningAroundSecondPart", "sRidleyTailMultiSpriteData_Idle",
    "sRidleyTailMultiSpriteData_Dying", "sRidleyTailMultiSpriteData_MoveToAttack",
    "sRidleyTailMultiSpriteData_SettingUpDiagonalTailAttack",
    "sRidleyTailMultiSpriteData_ChargingDiagonalTailAttack",
    "sRidleyTailMultiSpriteData_DiagonalTailAttack",
    "sRidleyTailMultiSpriteData_ChargingVerticalTailAttack",
    "sRidleyTailMultiSpriteData_VerticalTailAttack",
    "sRidleyTailMultiSpriteData_LastVerticalAttack", "sRidleyTailMultiSpriteData_BackToIdle",
    "sKraidMultiSpriteData_Rising", "sKraidMultiSpriteData_Standing",
    "sKraidMultiSpriteData_StandingBetweenSteps", "sKraidMultiSpriteData_MovingLeftFeetToRight",
    "sKraidMultiSpriteData_MovingRightFeetToRight", "sKraidMultiSpriteData_MovingLeftFeetToLeft",
    "sKraidMultiSpriteData_MovingRightFeetToLeft", "sKraidMultiSpriteData_Dying1",
    "sKraidMultiSpriteData_Dying2", "sImagoLarvaMultiSpriteData_Attacking",
    "sImagoLarvaMultiSpriteData_Idle", "sImagoLarvaMultiSpriteData_Retreating",
    "sImagoLarvaMultiSpriteData_Dying", "sImagoLarvaMultiSpriteData_Warning",
    "sImagoLarvaMultiSpriteData_TakingDamage", "sImagoCocoonMultiSpriteData_Idle",
    "sImagoCocoonMultiSpriteData_Dying", "sMechaRidleyMultiSpriteData_CrawlingForwardLow",
    "sMechaRidleyMultiSpriteData_StandingLow", "sMechaRidleyMultiSpriteData_StandingMiddle",
    "sMechaRidleyMultiSpriteData_StandingHigh", "sMechaRidleyMultiSpriteData_LowToMiddle",
    "sMechaRidleyMultiSpriteData_MiddleToHigh", "sMechaRidleyMultiSpriteData_HighToMiddle",
    "sMechaRidleyMultiSpriteData_MiddleToLow", "sMechaRidleyMultiSpriteData_LowToHigh",
    "sMechaRidleyMultiSpriteData_HighToLow", "sMechaRidleyMultiSpriteData_CrawlingBackwardsLow",
    "sMechaRidleyMultiSpriteData_OpeningMouthHigh",
    "sMechaRidleyMultiSpriteData_SpittingFireballsHigh",
    "sMechaRidleyMultiSpriteData_ClosingMouthHigh",
    "sMechaRidleyMultiSpriteData_DyingStandingLow",
    "sMechaRidleyMultiSpriteData_DyingStandingMiddle",
    "sMechaRidleyMultiSpriteData_DyingStandingHigh", "sMechaRidleyMultiSpriteData_Dying",
    "sImagoMultiSpriteData_Idle", "sImagoMultiSpriteData_ShootingNeedles",
    "sImagoMultiSpriteData_RechargingNeedles", "sImagoMultiSpriteData_Dying",
    "sTangleVineGerutaMultiSpriteData_Idle", "sCrocomireMultiSpriteData_Idle",
    "sCrocomireMultiSpriteData_WalkingForward", "sCrocomireMultiSpriteData_WalkingBackwards",
    "sCrocomireMultiSpriteData_Screaming", "sCrocomireMultiSpriteData_Dying",
    "sCrocomireMultiSpriteData_Angry", "sUnknownItemChozoStatueMultiSpriteData_Standing",
    "sUnknownItemChozoStatueMultiSpriteData_Sitting",
    "sUnknownItemChozoStatueMultiSpriteData_Seated",
};

/* Extra palette rows a ported AI sets (paletteRow), baked as ``/rowN``. */
static const struct {
    const char *sprite;
    unsigned rows[4];
    size_t count;
} row_variants[] = {
    {"SPACE_PIRATE", {2}, 1}, {"IMAGO", {1, 2, 4, 5}, 4}, {"BARISTUTE", {1, 2, 3}, 3},
    {"ATOMIC", {2, 3}, 2}, {"KRAID", {1, 2, 3}, 3}, {"RIDLEY", {3, 5}, 2},
    {"GADORA_RIDLEY", {1}, 1}, {"IMAGO_LARVA_RIGHT", {1, 2}, 2},
    {"IMAGO_LARVA_LEFT", {1, 2}, 2}, {"IMAGO_COCOON", {1, 2}, 2},
    {"MOTHER_BRAIN", {2, 3, 4}, 3}, {"METROID", {1, 2, 3}, 3}, {"SAVE_PLATFORM", {2}, 1},
    {"SAVE_PLATFORM_CHOZODIA", {2}, 1},
};

/* The tables a sprite's row variants apply to (by prefix; the others keep
 * their rows): only the Atomic's body flashes, not its electricity. */
static const struct {
    const char *sprite;
    const char *prefixes[3];
    size_t count;
} row_variant_tables[] = {
    {"ATOMIC", {"sAtomicOam_"}, 1},
    {"METROID", {"sMetroidOam_Spawning"}, 1},
    {"IMAGO_COCOON", {"sImagoCocoonOam_Idle", "sImagoCocoonOam_2e0bb0",
                      "sImagoCocoonOam_Static"}, 3},
};

/* Enemy drops (EnemyDropInit) and the water drop: common sprite graphics. */
static const struct {
    const char *sprite;
    const char *tables[3];
    size_t count;
} common_sprites[] = {
    {"SMALL_ENERGY_DROP", {"sEnemyDropOam_SmallEnergy"}, 1},
    {"LARGE_ENERGY_DROP", {"sEnemyDropOam_LargeEnergy"}, 1},
    {"MISSILE_DROP", {"sEnemyDropOam_Missile"}, 1},
    {"SUPER_MISSILE_DROP", {"sEnemyDropOam_SuperMissile"}, 1},
    {"POWER_BOMB_DROP", {"sEnemyDropOam_PowerBomb"}, 1},
    {"MULTIPLE_LARGE_ENERGY", {"sEnemyDropOam_MultipleLargeEnergySlot1",
                               "sEnemyDropOam_MultipleLargeEnergySlot2",
                               "sEnemyDropOam_MultipleLargeEnergySlot3"}, 3},
    {"WATER_DROP", {"sWaterDropOam_Spawning", "sWaterDropOam_Falling",
                    "sWaterDropOam_Splashing"}, 3},
};

/* Tables whose parts are also drawn one by one by the affine whole-sprite
 * path (``OamPart`` keys). */
static const char *const affine_whole_part_tables[] = {
    "sSkreeExplosionOam_GoingDown", "sSkreeExplosionOam_GoingUp"};

/* The Ruins Test reflection graphics sets: the actor rewrites its tiles at
 * OBJ VRAM 0x4280 and 0x4680. */
static const struct {
    const char *prefix;
    const char *top;
    const char *bottom;
} reflection_gfx[] = {
    {"tr", "sRuinsTestGfx_SamusReflectionTurningRightTop",
     "sRuinsTestGfx_SamusReflectionTurningRightBottom"},
    {"tl", "sRuinsTestGfx_SamusReflectionTurningLeftTop",
     "sRuinsTestGfx_SamusReflectionTurningLeftBottom"},
    {"u", "sRuinsTestGfx_SamusReflectionSuitlessTop",
     "sRuinsTestGfx_SamusReflectionSuitlessBottom"},
    {"f", "sRuinsTestGfx_SamusReflectionFullSuitTop",
     "sRuinsTestGfx_SamusReflectionFullSuitBottom"},
};

/* MechaRidleyGlowFading's seven copies of sMechaRidleyDestroyedGfx. */
static const unsigned mecha_destroyed_copies[][3] = {
    {0, 0x5580, 128}, {192, 0x5580 + 1024, 192}, {384, 0x5580 + 2048, 192},
    {576, 0x5580 + 3072, 192}, {768, 0x5580 + 4096, 192},
    {1024, 0x5580 + 5120 + 64, 128}, {1216, 0x5580 + 6144 + 64, 128},
};

/* MechaRidleyLoadMissilesGfx / LoadFireballsGfx: from tile 4 * stage
 * (missiles, stages 0-2) or 12 + 4 * stage (fireballs, stages 0-3). */
static const unsigned mecha_weapon_stages[] = {0, 4, 8, 12, 16, 20, 24};

static const char *const flip_names[] = {"none", "x", "y", "xy"};

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
    const FusionMzmRomSymbolTable *symbols;
    char *error;
    size_t error_size;
} Context;

/* An OBJ palette: RGB colors, or OBJ palette indices for an indexed export,
 * and which entries are loaded. */
typedef struct {
    uint8_t rgb[256][3];
    uint8_t index[256];
    bool loaded[256];
    bool indexed;
} Palette;

/* An RGBA canvas that remembers the box of the pixels it holds. */
typedef struct {
    uint8_t *rgba;
    int min_x, min_y, max_x, max_y;
    bool any;
} Canvas;

typedef struct {
    uint32_t pointer;
    uint8_t duration;
} Frame;

typedef struct {
    uint8_t *bmp;
    size_t size;
    unsigned duration;
    int left, top;
} Rendered;

typedef struct {
    Context *context;
    FusionSpriteLibrary *library;
    Text *bounds;
    Canvas *canvas;
    Canvas *other;
    Rendered *rendered;
} Writer;

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

static void write_u32(uint8_t *data, uint32_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

static bool starts_with(const char *text, const char *prefix) {
    return strncmp(text, prefix, strlen(prefix)) == 0;
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

static const FusionMzmRomSymbol *require_symbol(const Context *context, const char *name) {
    const FusionMzmRomSymbol *found = symbol(context, name);
    if (!found)
        set_error(context->error, context->error_size, "reference ELF lacks %s", name);
    return found;
}

/* The first sized symbol at an address (the table entries are address
 * sorted). */
static const FusionMzmRomSymbol *symbol_at(const Context *context, uint32_t address) {
    const FusionMzmRomSymbolTable *table = context->symbols;
    size_t low = 0, high = table->count;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        if (table->entries[middle].address < address) low = middle + 1;
        else high = middle;
    }
    for (; low < table->count && table->entries[low].address == address; ++low)
        if (table->entries[low].size) return &table->entries[low];
    return NULL;
}

static const uint8_t *slice(const Context *context, uint32_t pointer, size_t size,
                            const char *label) {
    const FusionRom *rom = context->rom;
    if (pointer < ROM_BASE || pointer >= ROM_LIMIT) {
        set_error(context->error, context->error_size, "not a GBA ROM pointer: 0x%08x",
                  (unsigned)pointer);
        return NULL;
    }
    size_t offset = pointer - ROM_BASE;
    if (offset >= rom->size) {
        set_error(context->error, context->error_size, "pointer exceeds supplied ROM size");
        return NULL;
    }
    if (size > rom->size - offset) {
        set_error(context->error, context->error_size, "truncated %s", label);
        return NULL;
    }
    return rom->data + offset;
}

/* A symbol's complete bytes. */
static const uint8_t *symbol_data(const Context *context, const char *name, size_t *size) {
    const FusionMzmRomSymbol *found = require_symbol(context, name);
    if (!found) return NULL;
    *size = found->size;
    return slice(context, found->address, found->size, name);
}

/* The bytes the reference ELF loads at a ROM address. */
static const uint8_t *elf_bytes(const uint8_t *elf, size_t elf_size, uint32_t address,
                                uint32_t size) {
    if (elf_size < 52) return NULL;
    size_t table = read_u32(elf + 0x20);
    size_t entry = read_u16(elf + 0x2E), count = read_u16(elf + 0x30);
    if (entry < 40 || table > elf_size || count > (elf_size - table) / entry) return NULL;
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

static bool verify_symbol(const Context *context, const uint8_t *elf, size_t elf_size,
                          const char *name) {
    const FusionMzmRomSymbol *found = require_symbol(context, name);
    if (!found) return false;
    const uint8_t *image = elf_bytes(elf, elf_size, found->address, found->size);
    if (!image || memcmp(image, context->rom->data + (found->address - ROM_BASE),
                         found->size)) {
        set_error(context->error, context->error_size,
                  "reference ELF does not match the ROM for: %s", name);
        return false;
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
    *entry = (OamEntry){x, y, sizes[shape][size][0], sizes[shape][size][1], a2 & 1023u,
                        (a2 >> 12) & 15u, (a1 & 0x1000) != 0, (a1 & 0x2000) != 0};
    return true;
}

/* The non-affine OBJ entries stored by one native frame. */
static bool oam_entries(const Context *context, uint32_t pointer, OamEntry *entries,
                        size_t *count) {
    const uint8_t *data = slice(context, pointer, 2, "projectile OAM");
    if (!data) return false;
    *count = read_u16(data);
    /* At most the 128 hardware OAM entries (the glass tube uses over 32). */
    if (*count < 1 || *count > MAX_OAM) {
        set_error(context->error, context->error_size, "invalid projectile OAM part count");
        return false;
    }
    const uint8_t *raw = slice(context, pointer + 2, *count * 6, "projectile OAM entries");
    if (!raw) return false;
    for (size_t index = 0; index < *count; ++index)
        if (!unpack_oam(context, raw + index * 6, &entries[index])) return false;
    return true;
}

/* (OAM pointer, duration) entries up to the FrameData terminator. */
static bool frame_data(const Context *context, const char *table, Frame *frames,
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
            set_error(context->error, context->error_size, "invalid frame duration in %s",
                      table);
            return false;
        }
        if (*count == MAX_FRAMES) {
            set_error(context->error, context->error_size, "too many frames in %s", table);
            return false;
        }
        frames[(*count)++] = (Frame){pointer, record[4]};
    }
    if (!*count) {
        set_error(context->error, context->error_size, "%s has no frames", table);
        return false;
    }
    return true;
}

/* The full OAM box union after SpriteDraw's global flip, transparent edge
 * pixels included. */
static bool object_bounds(const Context *context, uint32_t pointer, size_t flip,
                          int box[4], size_t *parts) {
    OamEntry entries[MAX_OAM];
    if (!oam_entries(context, pointer, entries, parts)) return false;
    for (size_t index = 0; index < *parts; ++index) {
        const OamEntry *entry = &entries[index];
        int x = flip & 1 ? -entry->x - entry->w : entry->x;
        int y = flip & 2 ? -entry->y - entry->h : entry->y;
        if (!index || x < box[0]) box[0] = x;
        if (!index || y < box[1]) box[1] = y;
        if (!index || x + entry->w > box[2]) box[2] = x + entry->w;
        if (!index || y + entry->h > box[3]) box[3] = y + entry->h;
    }
    box[2] -= box[0];
    box[3] -= box[1];
    return true;
}

static void canvas_clear(Canvas *canvas) {
    if (canvas->any)
        for (int y = canvas->min_y; y <= canvas->max_y; ++y)
            memset(canvas->rgba + ((size_t)y * CANVAS_WIDTH + (size_t)canvas->min_x) * 4, 0,
                   (size_t)(canvas->max_x - canvas->min_x + 1) * 4);
    canvas->any = false;
}

/* Draws OAM entries relative to the sprite position; ProjectileDraw and
 * SpriteDraw mirror the parts around that point for X/Y flips. */
static bool draw_entries(const Context *context, const uint8_t *vram, const Palette *palette,
                         const OamEntry *entries, size_t count, size_t flip,
                         Canvas *canvas) {
    canvas_clear(canvas);
    for (size_t index = count; index-- > 0;) {   /* OAM slot zero wins. */
        const OamEntry *entry = &entries[index];
        for (int iy = 0; iy < entry->h; ++iy) {
            for (int ix = 0; ix < entry->w; ++ix) {
                int sx = entry->hflip ? entry->w - 1 - ix : ix;
                int sy = entry->vflip ? entry->h - 1 - iy : iy;
                size_t tile = entry->tile + (size_t)(sy / 8) * 32 + (size_t)(sx / 8);
                size_t offset = tile * 32 + (size_t)(sy % 8) * 4 + (size_t)(sx % 8) / 2;
                if (offset >= OBJ_VRAM_BYTES) {
                    set_error(context->error, context->error_size,
                              "projectile tile outside OBJ VRAM");
                    return false;
                }
                unsigned color = (vram[offset] >> (4 * (sx & 1))) & 15;
                if (!color) continue;
                size_t slot = entry->bank * 16 + color;
                if (!palette->loaded[slot]) {
                    set_error(context->error, context->error_size,
                              "projectile uses an unloaded palette row");
                    return false;
                }
                int x = entry->x + ix, y = entry->y + iy;
                if (flip & 1) x = -1 - x;
                if (flip & 2) y = -1 - y;
                x += ORIGIN_X;
                y += ORIGIN_Y;
                uint8_t *pixel = canvas->rgba + ((size_t)y * CANVAS_WIDTH + (size_t)x) * 4;
                if (palette->indexed) {
                    pixel[0] = palette->index[slot];
                    pixel[1] = pixel[2] = 0;
                } else {
                    memcpy(pixel, palette->rgb[slot], 3);
                }
                pixel[3] = 255;
                if (!canvas->any || x < canvas->min_x) canvas->min_x = x;
                if (!canvas->any || x > canvas->max_x) canvas->max_x = x;
                if (!canvas->any || y < canvas->min_y) canvas->min_y = y;
                if (!canvas->any || y > canvas->max_y) canvas->max_y = y;
                canvas->any = true;
            }
        }
    }
    return true;
}

static bool render(const Context *context, const uint8_t *vram, const Palette *palette,
                   uint32_t pointer, size_t flip, Canvas *canvas) {
    OamEntry entries[MAX_OAM];
    size_t count = 0;
    return oam_entries(context, pointer, entries, &count) &&
           draw_entries(context, vram, palette, entries, count, flip, canvas);
}

/* One OBJ as the affine whole-sprite mode reads it: the OAM flip bits
 * become matrix-number bits, so the original Y flip is ignored and an X flip
 * is not supported. */
static bool render_affine_part(const Context *context, const uint8_t *vram,
                               const Palette *palette, uint32_t pointer, size_t part,
                               Canvas *canvas, int box[4]) {
    OamEntry entries[MAX_OAM];
    size_t count = 0;
    if (!oam_entries(context, pointer, entries, &count)) return false;
    if (part >= count) {
        set_error(context->error, context->error_size, "OAM part index outside frame");
        return false;
    }
    OamEntry entry = entries[part];
    if (entry.hflip) {
        set_error(context->error, context->error_size,
                  "affine-whole X-flipped OBJ is not supported");
        return false;
    }
    entry.vflip = false;
    box[0] = entry.x;
    box[1] = entry.y;
    box[2] = entry.w;
    box[3] = entry.h;
    return draw_entries(context, vram, palette, &entry, 1, 0, canvas);
}

static bool canvases_equal(const Canvas *left, const Canvas *right) {
    if (left->any != right->any) return false;
    if (!left->any) return true;
    if (left->min_x != right->min_x || left->max_x != right->max_x ||
        left->min_y != right->min_y || left->max_y != right->max_y)
        return false;
    for (int y = left->min_y; y <= left->max_y; ++y) {
        size_t offset = ((size_t)y * CANVAS_WIDTH + (size_t)left->min_x) * 4;
        if (memcmp(left->rgba + offset, right->rgba + offset,
                   (size_t)(left->max_x - left->min_x + 1) * 4))
            return false;
    }
    return true;
}

/* The frame as the exact 32-bit BITMAPV4 of the sprite libraries, or as an
 * 8-bit BMP with ``preview`` as its 256-color palette; an empty frame is one
 * black pixel at the origin. */
static bool canvas_bitmap(const Context *context, const Canvas *canvas,
                          const uint8_t (*preview)[3], Rendered *rendered) {
    int min_x = canvas->min_x, min_y = canvas->min_y;
    int max_x = canvas->max_x, max_y = canvas->max_y;
    bool indexed = preview != NULL && canvas->any;
    if (!canvas->any) {
        min_x = max_x = ORIGIN_X;
        min_y = max_y = ORIGIN_Y;
    }
    size_t width = (size_t)(max_x - min_x + 1), height = (size_t)(max_y - min_y + 1);
    if (width > 512 || height > 512) {
        set_error(context->error, context->error_size, "frame exceeds 512 pixels");
        return false;
    }
    size_t pitch = indexed ? (width + 3) & ~(size_t)3 : width * 4;
    size_t offset = indexed ? 14 + 40 + 1024 : 122;
    rendered->size = offset + pitch * height;
    rendered->bmp = calloc(1, rendered->size);
    if (!rendered->bmp) {
        set_error(context->error, context->error_size, "cannot allocate sprite bitmap");
        return false;
    }
    uint8_t *bmp = rendered->bmp;
    bmp[0] = 'B';
    bmp[1] = 'M';
    write_u32(bmp + 2, (uint32_t)rendered->size);
    write_u32(bmp + 10, (uint32_t)offset);
    write_u32(bmp + 14, indexed ? 40 : 108);
    write_u32(bmp + 18, (uint32_t)width);
    write_u32(bmp + 22, (uint32_t)height);
    bmp[26] = 1;
    bmp[28] = indexed ? 8 : 32;
    write_u32(bmp + 34, (uint32_t)(pitch * height));
    if (indexed) {
        write_u32(bmp + 38, 2835);
        write_u32(bmp + 42, 2835);
        write_u32(bmp + 46, 256);
        for (size_t index = 0; index < 256; ++index) {
            bmp[54 + index * 4] = preview[index][2];
            bmp[55 + index * 4] = preview[index][1];
            bmp[56 + index * 4] = preview[index][0];
        }
    } else {
        bmp[30] = 3;
        write_u32(bmp + 54, 0x00FF0000);
        write_u32(bmp + 58, 0x0000FF00);
        write_u32(bmp + 62, 0x000000FF);
        write_u32(bmp + 66, 0xFF000000);
        write_u32(bmp + 70, 0x73524742);
    }
    for (int y = min_y; y <= max_y; ++y) {
        uint8_t *row = bmp + offset + (size_t)(max_y - y) * pitch;
        for (int x = min_x; x <= max_x; ++x) {
            if (!canvas->any) {
                row[3] = 255;
                continue;
            }
            const uint8_t *pixel = canvas->rgba + ((size_t)y * CANVAS_WIDTH + (size_t)x) * 4;
            if (!pixel[3]) continue;
            if (indexed) {
                row[x - min_x] = pixel[0];
            } else {
                uint8_t *target = row + (size_t)(x - min_x) * 4;
                target[0] = pixel[2];
                target[1] = pixel[1];
                target[2] = pixel[0];
                target[3] = 255;
            }
        }
    }
    rendered->left = min_x - ORIGIN_X;
    rendered->top = min_y - ORIGIN_Y;
    return true;
}

static void free_rendered(Rendered *rendered, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        free(rendered[index].bmp);
        rendered[index].bmp = NULL;
    }
}

/* Renders every frame of a table into the writer's frame buffer. */
static bool render_frames(Writer *writer, const uint8_t *vram, const Palette *palette,
                          const Frame *frames, size_t count, size_t flip,
                          const uint8_t (*preview)[3]) {
    for (size_t index = 0; index < count; ++index) {
        writer->rendered[index] = (Rendered){.duration = frames[index].duration};
        if (!render(writer->context, vram, palette, frames[index].pointer, flip,
                    writer->canvas) ||
            !canvas_bitmap(writer->context, writer->canvas, preview,
                           &writer->rendered[index])) {
            free_rendered(writer->rendered, index + 1);
            return false;
        }
    }
    return true;
}

static bool add_rendered(Writer *writer, const char *key, size_t count) {
    bool ok = fusion_sprite_library_begin(writer->library, writer->context->error,
                                          writer->context->error_size);
    for (size_t index = 0; ok && index < count; ++index) {
        const Rendered *frame = &writer->rendered[index];
        ok = fusion_sprite_library_add(writer->library, key, index, frame->duration,
                                       frame->left, frame->top, frame->bmp, frame->size,
                                       writer->context->error, writer->context->error_size);
    }
    free_rendered(writer->rendered, count);
    return ok;
}

static bool add_bounds(Writer *writer, const char *key, const Frame *frames, size_t count,
                       size_t flip) {
    for (size_t index = 0; index < count; ++index) {
        int box[4] = {0};
        size_t parts = 0;
        if (!object_bounds(writer->context, frames[index].pointer, flip, box, &parts) ||
            !text_append(writer->bounds, "%s\t%zu\t%d\t%d\t%d\t%d\t%zu\n", key, index,
                         box[0], box[1], box[2], box[3], parts))
            return false;
    }
    return true;
}

/* A plain table: every frame, then its library key. */
static bool emit(Writer *writer, const char *key, const uint8_t *vram,
                 const Palette *palette, const Frame *frames, size_t count, size_t flip) {
    return render_frames(writer, vram, palette, frames, count, flip, NULL) &&
           add_rendered(writer, key, count);
}

static bool decompress(const Context *context, const char *name, uint8_t *vram) {
    size_t size = 0;
    const uint8_t *data = symbol_data(context, name, &size);
    if (!data) return false;
    uint32_t expanded = fusion_gba_uncompressed_size(data, size);
    size_t written = 0;
    if (!expanded || expanded > OBJ_VRAM_BYTES - SPRITE_VRAM) {
        set_error(context->error, context->error_size, "%s overflows OBJ VRAM", name);
        return false;
    }
    if (!fusion_gba_lz77_uncompress(data, size, vram + SPRITE_VRAM, expanded, &written) ||
        written != expanded) {
        set_error(context->error, context->error_size, "invalid LZ77 graphics %s", name);
        return false;
    }
    return true;
}

static void set_color(Palette *palette, size_t slot, uint16_t value) {
    bgr555(value, palette->rgb[slot]);
    palette->loaded[slot] = true;
}

/* OBJ VRAM with the sprite in graphics slot 0, and the OBJ palette with its
 * palette row ``row`` at row 8 (SpriteDraw adds the row). */
static bool stage(const Context *context, const char *graphics, const char *palette_name,
                  unsigned row, uint8_t *vram, Palette *palette) {
    if (vram) {
        memset(vram, 0, OBJ_VRAM_BYTES);
        if (!decompress(context, graphics, vram)) return false;
    }
    size_t size = 0;
    const uint8_t *colors = symbol_data(context, palette_name, &size);
    if (!colors) return false;
    memset(palette, 0, sizeof *palette);
    size_t rows = size / 32;
    for (size_t index = 0; index + row < rows; ++index) {
        if (8 + index > 15) {
            set_error(context->error, context->error_size, "%s overflows the OBJ palette",
                      palette_name);
            return false;
        }
        for (size_t color = 0; color < 16; ++color)
            set_color(palette, (8 + index) * 16 + color,
                      read_u16(colors + ((row + index) * 16 + color) * 2));
    }
    return true;
}

/* OBJ VRAM and palette as HudGenericLoadCommonSpriteGfx and the room load
 * leave them for the common sprites. */
static bool stage_common(const Context *context, uint8_t *vram, Palette *palette) {
    size_t size = 0;
    const uint8_t *data = symbol_data(context, "sCommonSpritesGfx", &size);
    if (!data) return false;
    if (size > OBJ_VRAM_BYTES - COMMON_GFX_VRAM) {
        set_error(context->error, context->error_size, "common sprite graphics overflow VRAM");
        return false;
    }
    memset(vram, 0, OBJ_VRAM_BYTES);
    memcpy(vram + COMMON_GFX_VRAM, data, size);
    data = symbol_data(context, "sCommonSpritesPal", &size);
    if (!data) return false;
    if (size / 2 > 256 - COMMON_PALETTE_ROW * 16) {
        set_error(context->error, context->error_size, "common sprite palette overflow");
        return false;
    }
    memset(palette, 0, sizeof *palette);
    for (size_t index = 0; index < size / 2; ++index)
        set_color(palette, COMMON_PALETTE_ROW * 16 + index, read_u16(data + index * 2));
    return true;
}

/* Copies ``count`` colors of a symbol, from color ``first``, to ``slot``. */
static bool copy_colors(const Context *context, const char *name, size_t first,
                        size_t count, Palette *palette, size_t slot) {
    size_t size = 0;
    const uint8_t *data = symbol_data(context, name, &size);
    if (!data) return false;
    if ((first + count) * 2 > size || slot + count > 256) {
        set_error(context->error, context->error_size, "truncated palette %s", name);
        return false;
    }
    for (size_t index = 0; index < count; ++index)
        set_color(palette, slot + index, read_u16(data + (first + index) * 2));
    return true;
}

/* Copies a symbol's bytes into OBJ VRAM. */
static bool copy_vram(const Context *context, const char *name, size_t source,
                      size_t length, uint8_t *vram, size_t target) {
    size_t size = 0;
    const uint8_t *data = symbol_data(context, name, &size);
    if (!data) return false;
    if (length == SIZE_MAX) length = size - source;
    if (source > size || length > size - source || target + length > OBJ_VRAM_BYTES) {
        set_error(context->error, context->error_size, "%s overflows OBJ VRAM", name);
        return false;
    }
    memcpy(vram + target, data + source, length);
    return true;
}

typedef struct {
    char suffix[16];
    Palette palette;
} Variant;

/* The frozen and stun looks: SpriteDraw adds the sprite's paletteRow to each
 * part's row, and ProjectileFreezeSprite / SPRITE_GET_STUN_PALETTE set it so
 * a part of row b shows OBJ row (b + 15) or (b + 14) mod 16. */
static void shifted(const Palette *palette, const Palette *common, unsigned shift,
                    Palette *result) {
    *result = *palette;
    for (size_t bank = 8; bank < 16; ++bank) {
        size_t source = (bank + shift) % 16;
        for (size_t color = 0; color < 16; ++color) {
            size_t from = source * 16 + color, to = bank * 16 + color;
            const Palette *origin = common->loaded[from] ? common : palette;
            memcpy(result->rgb[to], origin->rgb[from], 3);
            result->loaded[to] = origin->loaded[from];
        }
    }
}

static size_t row_variant(const char *sprite) {
    for (size_t index = 0; index < COUNT(row_variants); ++index)
        if (!strcmp(row_variants[index].sprite, sprite)) return index;
    return SIZE_MAX;
}

static bool row_variant_applies(const char *sprite, const char *table) {
    for (size_t index = 0; index < COUNT(row_variant_tables); ++index) {
        if (strcmp(row_variant_tables[index].sprite, sprite)) continue;
        for (size_t prefix = 0; prefix < row_variant_tables[index].count; ++prefix)
            if (starts_with(table, row_variant_tables[index].prefixes[prefix])) return true;
        return false;
    }
    return true;
}

static bool is_affine_part_table(const char *table) {
    for (size_t index = 0; index < COUNT(affine_whole_part_tables); ++index)
        if (!strcmp(affine_whole_part_tables[index], table)) return true;
    return false;
}

/* The OBJ palette variants of one ported sprite, in export order. */
static bool build_variants(const Context *context, const FusionMzmSpritePlan *plan,
                           const Palette *palette, const Palette *common, Variant *variants,
                           size_t *count, size_t *flips) {
    const char *sprite = plan->name;
    bool chozo = starts_with(sprite, "CHOZO_STATUE");
    bool statue = chozo || !strcmp(sprite, "KRAID_STATUE") || !strcmp(sprite, "RIDLEY_STATUE");
    *count = 0;
    variants[(*count)++] = (Variant){"", *palette};
    *flips = statue ? 2 : 4;
    if (!statue) {
        snprintf(variants[*count].suffix, sizeof variants[0].suffix, "/frozen");
        shifted(palette, common, 15, &variants[(*count)++].palette);
        snprintf(variants[*count].suffix, sizeof variants[0].suffix, "/stun");
        shifted(palette, common, 14, &variants[(*count)++].palette);
    } else if (chozo) {
        /* sChozoStatueFlashingPaletteRows: palette rows 1 to 3. */
        for (unsigned flash = 1; flash <= 3; ++flash) {
            snprintf(variants[*count].suffix, sizeof variants[0].suffix, "/row%u", flash);
            if (!stage(context, plan->graphics, plan->palette, plan->row + flash, NULL,
                       &variants[(*count)++].palette))
                return false;
        }
    }
    size_t rows = row_variant(sprite);
    for (size_t index = 0; rows != SIZE_MAX && index < row_variants[rows].count; ++index) {
        unsigned flash = row_variants[rows].rows[index];
        snprintf(variants[*count].suffix, sizeof variants[0].suffix, "/row%u", flash);
        if (!stage(context, plan->graphics, plan->palette, plan->row + flash, NULL,
                   &variants[(*count)++].palette))
            return false;
    }
    if (!strcmp(sprite, "GUNSHIP")) {
        /* GunshipEntranceFlashingAnim copies sGunshipFlashingPal[0x38 + 16e]
         * (8 colors) to OBJ row 9 colors 8-15 and GunshipFlickerFlames
         * sGunshipFlashingPal[16f] to OBJ row 10; every combination, each
         * row also untouched. The gunship never flips. */
        *count = 0;
        for (int entrance = -1; entrance < 5; ++entrance) {
            for (int flames = -1; flames < 3; ++flames) {
                Variant *variant = &variants[(*count)++];
                variant->palette = *palette;
                char suffix[16] = "";
                if (entrance >= 0) {
                    if (!copy_colors(context, "sGunshipFlashingPal", 0x38 + 16 * (size_t)entrance,
                                     8, &variant->palette, 9 * 16 + 8))
                        return false;
                    snprintf(suffix, sizeof suffix, "e%d", entrance);
                }
                if (flames >= 0) {
                    if (!copy_colors(context, "sGunshipFlashingPal", 16 * (size_t)flames, 16,
                                     &variant->palette, 10 * 16))
                        return false;
                    size_t used = strlen(suffix);
                    snprintf(suffix + used, sizeof suffix - used, "f%d", flames);
                }
                snprintf(variant->suffix, sizeof variant->suffix, "%s%s",
                         suffix[0] ? "/" : "", suffix);
            }
        }
        *flips = 1;
    }
    bool gate = !strcmp(sprite, "ESCAPE_GATE1");
    if (gate || !strcmp(sprite, "ESCAPE_SHIP")) {
        /* The gate's sEscapeGateFlashingPal rows 0 to 2 replace its last OBJ
         * row; the ship's tail copies sEscapeShipFlashingPal rows 0 to 2 to
         * its row 1. Never frozen, stunned or flipped. */
        size_t row = gate ? 15 : 9;
        Palette base = variants[0].palette;
        if (gate && !copy_colors(context, "sEscapeGateAndTimerPal", 0, 16, &base, row * 16))
            return false;
        *count = 0;
        variants[(*count)++] = (Variant){"", base};
        for (size_t index = 0; index < FLASHING_ROWS; ++index) {
            Variant *variant = &variants[(*count)++];
            snprintf(variant->suffix, sizeof variant->suffix, "/f%zu", index);
            variant->palette = base;
            if (!copy_colors(context, gate ? "sEscapeGateFlashingPal" : "sEscapeShipFlashingPal",
                             16 * index, 16, &variant->palette, row * 16))
                return false;
        }
        *flips = 1;
    }
    if (!strcmp(sprite, "METROID")) {
        /* MetroidSamusGrabbed copies 8 colors of a sMetroidPal_SamusGrabbed
         * row to OBJ row 12. */
        const FusionMzmRomSymbol *grabbed = require_symbol(context, "sMetroidPal_SamusGrabbed");
        if (!grabbed) return false;
        for (size_t index = 0; index < grabbed->size / 32; ++index) {
            Variant *variant = &variants[(*count)++];
            snprintf(variant->suffix, sizeof variant->suffix, "/g%zu", index);
            variant->palette = *palette;
            if (!copy_colors(context, "sMetroidPal_SamusGrabbed", 16 * index, 8,
                             &variant->palette, 12 * 16))
                return false;
        }
    }
    if (!strcmp(sprite, "RUINS_TEST")) {
        /* RUINS_TEST_TRANSFER_DYNAMIC_PAL: the first 8 colors of OBJ row 14
         * replaced by sRuinsTestPal_SymbolShot_FrameN; the reflections' row
         * 15 rewritten with the 15 rows of sRuinsTestPal_SamusReflection. */
        for (size_t index = 1; index <= 6; ++index) {
            char name[64];
            snprintf(name, sizeof name, "sRuinsTestPal_SymbolShot_Frame%zu", index);
            Variant *variant = &variants[(*count)++];
            snprintf(variant->suffix, sizeof variant->suffix, "/s%zu", index);
            variant->palette = *palette;
            if (!copy_colors(context, name, 0, 8, &variant->palette,
                             RUINS_TEST_DYNAMIC_ROW * 16))
                return false;
        }
        for (size_t index = 0; index < 15; ++index) {
            Variant *variant = &variants[(*count)++];
            snprintf(variant->suffix, sizeof variant->suffix, "/r%zu", index + 1);
            variant->palette = *palette;
            if (!copy_colors(context, "sRuinsTestPal_SamusReflection", 16 * index, 16,
                             &variant->palette, RUINS_TEST_REFLECTION_ROW * 16))
                return false;
        }
    }
    return true;
}

static bool export_affine_parts(Writer *writer, const char *sprite, const char *table,
                                const char *suffix, const uint8_t *vram,
                                const Palette *palette, const Frame *frames, size_t count) {
    for (size_t frame = 0; frame < count; ++frame) {
        OamEntry entries[MAX_OAM];
        size_t parts = 0;
        if (!oam_entries(writer->context, frames[frame].pointer, entries, &parts))
            return false;
        for (size_t part = 0; part < parts; ++part) {
            int box[4];
            char key[256];
            snprintf(key, sizeof key, "OamPart/%s/%s/none%s/frame%zu/part%zu", sprite,
                     table + 1, suffix, frame, part);
            writer->rendered[0] = (Rendered){.duration = frames[frame].duration};
            if (!render_affine_part(writer->context, vram, palette, frames[frame].pointer,
                                    part, writer->canvas, box) ||
                !canvas_bitmap(writer->context, writer->canvas, NULL, &writer->rendered[0]) ||
                !add_rendered(writer, key, 1) ||
                !text_append(writer->bounds, "%s\t0\t%d\t%d\t%d\t%d\t1\n", key, box[0],
                             box[1], box[2], box[3]))
                return false;
        }
    }
    return true;
}

/* The Ruins Test reflection: every graphics set with the palette rows it can
 * use (normal reflection rows for turning and suitless, the full-suit rows
 * for the final reflection). */
static bool export_reflection(Writer *writer, const char *table, const uint8_t *vram,
                              const Palette *palette, const Frame *frames, size_t count,
                              size_t flips) {
    static uint8_t staged[OBJ_VRAM_BYTES];
    for (size_t set = 0; set < COUNT(reflection_gfx); ++set) {
        memcpy(staged, vram, OBJ_VRAM_BYTES);
        if (!copy_vram(writer->context, reflection_gfx[set].top, 0, SIZE_MAX, staged, 0x4280) ||
            !copy_vram(writer->context, reflection_gfx[set].bottom, 0, SIZE_MAX, staged,
                       0x4680))
            return false;
        bool full = !strcmp(reflection_gfx[set].prefix, "f");
        size_t palettes = full ? 15 : 16;
        for (size_t index = 0; index < palettes; ++index) {
            Palette colors = *palette;
            if ((full || index) &&
                !copy_colors(writer->context,
                             full ? "sRuinsTestPal_SamusReflectionFullSuit"
                                  : "sRuinsTestPal_SamusReflection",
                             16 * (full ? index : index - 1), 16, &colors,
                             RUINS_TEST_REFLECTION_ROW * 16))
                return false;
            for (size_t flip = 0; flip < flips; ++flip) {
                char key[256];
                snprintf(key, sizeof key, "Sprite/RUINS_TEST/%s/%s/%s%zu", table + 1,
                         flip_names[flip], reflection_gfx[set].prefix, index);
                if (!emit(writer, key, staged, &colors, frames, count, flip) ||
                    !add_bounds(writer, key, frames, count, flip))
                    return false;
            }
        }
    }
    return true;
}

/* Whether every frame draws the same pixels with both palettes. */
static bool same_look(Writer *writer, const uint8_t *vram, const Palette *left,
                      const Palette *right, const Frame *frames, size_t count, size_t flip,
                      bool *same) {
    *same = true;
    for (size_t index = 0; index < count && *same; ++index) {
        if (!render(writer->context, vram, left, frames[index].pointer, flip, writer->canvas) ||
            !render(writer->context, vram, right, frames[index].pointer, flip, writer->other))
            return false;
        *same = canvases_equal(writer->canvas, writer->other);
    }
    return true;
}

static bool export_sprite(Writer *writer, const FusionMzmSpritePlan *plan,
                          const Palette *common, Variant *variants) {
    Context *context = writer->context;
    static uint8_t vram[OBJ_VRAM_BYTES];
    Palette palette;
    size_t variant_count = 0, flips = 0;
    if (!stage(context, plan->graphics, plan->palette, plan->row, vram, &palette) ||
        !build_variants(context, plan, &palette, common, variants, &variant_count, &flips))
        return false;
    if (!strcmp(plan->name, "ESCAPE_GATE1") &&
        !copy_vram(context, "sEscapeGateAndTimerGfx", 0, SIZE_MAX, vram, ESCAPE_GATE_VRAM))
        return false;
    bool tint_overflow = !strcmp(plan->name, "IMAGO");
    bool ruins = !strcmp(plan->name, "RUINS_TEST");
    for (size_t table = 0; table < plan->table_count; ++table) {
        const char *name = plan->tables[table];
        Frame frames[MAX_FRAMES];
        size_t count = 0;
        if (!frame_data(context, name, frames, &count)) return false;
        for (size_t index = 0; index < variant_count; ++index) {
            const Variant *variant = &variants[index];
            if (starts_with(variant->suffix, "/g") && strcmp(name, "sMetroidOam_SamusGrabbed"))
                continue;
            if (starts_with(variant->suffix, "/row") && !row_variant_applies(plan->name, name))
                continue;
            for (size_t flip = 0; flip < flips; ++flip) {
                if (!render_frames(writer, vram, &variant->palette, frames, count, flip, NULL)) {
                    /* A damage tint shifting a part past the sprite's eight
                     * rows reads another sprite's OBJ row natively; that
                     * table keeps its untinted look. */
                    if (tint_overflow && starts_with(variant->suffix, "/row")) continue;
                    char reason[256];
                    snprintf(reason, sizeof reason, "%s", context->error);
                    set_error(context->error, context->error_size, "%s/%s%s/%s: %s",
                              plan->name, name, variant->suffix, flip_names[flip], reason);
                    return false;
                }
                if (ruins && (starts_with(variant->suffix, "/s") ||
                              starts_with(variant->suffix, "/r"))) {
                    bool same = false;
                    if (!same_look(writer, vram, &variant->palette, &palette, frames, count,
                                   flip, &same)) {
                        free_rendered(writer->rendered, count);
                        return false;
                    }
                    if (same) {
                        /* This table never reads the dynamic colors. */
                        free_rendered(writer->rendered, count);
                        continue;
                    }
                }
                char key[256];
                snprintf(key, sizeof key, "Sprite/%s/%s/%s%s", plan->name, name + 1,
                         flip_names[flip], variant->suffix);
                if (!add_rendered(writer, key, count) ||
                    !add_bounds(writer, key, frames, count, flip))
                    return false;
                if (!flip && is_affine_part_table(name) &&
                    !export_affine_parts(writer, plan->name, name, variant->suffix, vram,
                                         &variant->palette, frames, count))
                    return false;
            }
        }
        if (ruins && !strcmp(name, "sRuinsTestSamusReflectionOam") &&
            !export_reflection(writer, name, vram, &palette, frames, count, flips))
            return false;
    }
    return true;
}

/* The OBJ palette indices a part of bank b reads with ``shift`` added to
 * its row (SpriteDraw's paletteRow, 4 bits). */
static void index_palette(const bool loaded[256], unsigned shift, Palette *palette) {
    memset(palette, 0, sizeof *palette);
    palette->indexed = true;
    for (size_t bank = 8; bank < 16; ++bank) {
        size_t target = (bank + shift) % 16;
        for (size_t color = 0; color < 16; ++color) {
            if (!loaded[target * 16 + color]) continue;
            palette->index[bank * 16 + color] = (uint8_t)(target * 16 + color);
            palette->loaded[bank * 16 + color] = true;
        }
    }
}

/* Mecha Ridley's frames as OBJ palette indices: the base look, the damage
 * tints (rows 2-4), the stun and frozen rows, the destroyed graphics (/d,
 * alone or tinted) and the weapon graphics stages (/wN). A variant reading a
 * row that is not loaded is skipped (natively another sprite's palette). */
static bool export_mecha_ridley(Writer *writer, const FusionMzmSpritePlan *plan,
                                const uint8_t *vram, const bool loaded[256],
                                const uint8_t (*preview)[3]) {
    Context *context = writer->context;
    static uint8_t destroyed[OBJ_VRAM_BYTES], weapons[OBJ_VRAM_BYTES];
    memcpy(destroyed, vram, OBJ_VRAM_BYTES);
    for (size_t copy = 0; copy < COUNT(mecha_destroyed_copies); ++copy)
        if (!copy_vram(context, "sMechaRidleyDestroyedGfx", mecha_destroyed_copies[copy][0],
                       mecha_destroyed_copies[copy][2], destroyed,
                       mecha_destroyed_copies[copy][1]))
            return false;
    static const struct {
        const char *suffix;
        unsigned shift;
    } shifts[] = {{"", 0}, {"/row2", 2}, {"/row3", 3}, {"/row4", 4}, {"/frozen", 15},
                  {"/stun", 14}};
    for (size_t table = 0; table < plan->table_count; ++table) {
        const char *name = plan->tables[table];
        Frame frames[MAX_FRAMES];
        size_t count = 0;
        if (!frame_data(context, name, frames, &count)) return false;
        bool weapon = !strcmp(name, "sMechaRidleyMissileOam") ||
                      !strcmp(name, "sMechaRidleyFireballOam");
        size_t stages = COUNT(shifts) + 4 + (weapon ? COUNT(mecha_weapon_stages) : 0);
        for (size_t stage_index = 0; stage_index < stages; ++stage_index) {
            char suffix[24];
            const uint8_t *staged = vram;
            unsigned shift = 0;
            if (stage_index < COUNT(shifts)) {
                snprintf(suffix, sizeof suffix, "%s", shifts[stage_index].suffix);
                shift = shifts[stage_index].shift;
            } else if (stage_index < COUNT(shifts) + 4) {
                size_t tint = stage_index - COUNT(shifts);
                snprintf(suffix, sizeof suffix, "/d%s", shifts[tint].suffix);
                shift = shifts[tint].shift;
                staged = destroyed;
            } else {
                size_t weapon_stage = stage_index - COUNT(shifts) - 4;
                snprintf(suffix, sizeof suffix, "/w%zu", weapon_stage);
                memcpy(weapons, vram, OBJ_VRAM_BYTES);
                for (size_t row = 0; row < 4; ++row)
                    if (!copy_vram(context, "sMechaRidleyWeaponsGfx",
                                   row * 1024 + mecha_weapon_stages[weapon_stage] * 32, 128,
                                   weapons, MECHA_RIDLEY_WEAPON_VRAM + row * 1024))
                        return false;
                staged = weapons;
            }
            Palette indices;
            index_palette(loaded, shift, &indices);
            if (!render_frames(writer, staged, &indices, frames, count, 0, preview)) {
                if (suffix[0]) continue;
                return false;
            }
            char key[256];
            snprintf(key, sizeof key, "Sprite/MECHA_RIDLEY/%s/none%s", name + 1, suffix);
            if (!add_rendered(writer, key, count) || !add_bounds(writer, key, frames, count, 0))
                return false;
        }
    }
    return true;
}

static bool read_text(const Context *context, uint32_t pointer, uint16_t *text,
                      size_t *length) {
    *length = 0;
    for (;;) {
        const uint8_t *data = slice(context, pointer + (uint32_t)(*length * 2), 2,
                                    "message text");
        if (!data) return false;
        uint16_t value = read_u16(data);
        text[(*length)++] = value;
        if ((value & 0xFF00) == 0xFF00) return true;
        if (*length >= FUSION_MZM_TEXT_CAPACITY - 1) {
            set_error(context->error, context->error_size, "unterminated message text");
            return false;
        }
    }
}

/* The English text pointer of entry ``index`` of a language table. */
static bool english_text(const Context *context, const char *table, size_t index,
                         uint32_t *pointer) {
    const FusionMzmRomSymbol *found = require_symbol(context, table);
    const uint8_t *data = found ? slice(context, found->address +
                                        4 * FUSION_MZM_LANGUAGE_ENGLISH, 4, table) : NULL;
    const uint8_t *entry = data ? slice(context, read_u32(data) + (uint32_t)(4 * index), 4,
                                        table) : NULL;
    if (!entry) return false;
    *pointer = read_u32(entry);
    return true;
}

static bool load_font(const Context *context, FusionMzmFont *font) {
    size_t size = 0;
    font->gfx = symbol_data(context, "sCharactersGfx", &size);
    font->gfx_size = size;
    font->widths = font->gfx ? symbol_data(context, "sCharacterWidths", &size) : NULL;
    font->width_count = size;
    return font->widths != NULL;
}

static const FusionMzmSpritePlan *find_plan(const char *name) {
    for (size_t index = 0; index < fusion_mzm_sprite_plan_count; ++index)
        if (!strcmp(fusion_mzm_sprite_plan[index].name, name))
            return &fusion_mzm_sprite_plan[index];
    return NULL;
}

/* The message banner with each message's text (TextProcessMessageBanner
 * over SpriteLoadGfx's tiles), and the area banner with each location text
 * (TextDrawLocation: the text in mask mode over 0x800 bytes of 0xFF, its two
 * tile rows copied to the graphics slot). */
static bool export_banners(Writer *writer, const FusionMzmFont *font,
                           uint8_t (*banners)[OBJ_VRAM_BYTES]) {
    Context *context = writer->context;
    Palette palette;
    if (!stage(context, "sMessageBannerGfx", "sMessageBannerPal", 0, NULL, &palette))
        return false;
    for (size_t message = 0; message < fusion_mzm_message_enum_count; ++message) {
        for (size_t table = 0; table < fusion_mzm_banner_table_count; ++table) {
            Frame frames[MAX_FRAMES];
            size_t count = 0;
            char key[256];
            snprintf(key, sizeof key, "Banner/%s/%s/none", fusion_mzm_message_enum[message].name,
                     fusion_mzm_banner_tables[table] + 1);
            if (!frame_data(context, fusion_mzm_banner_tables[table], frames, &count) ||
                !emit(writer, key, banners[message], &palette, frames, count, 0))
                return false;
        }
    }
    const FusionMzmSpritePlan *plan = find_plan("AREA_BANNER");
    if (!plan) {
        set_error(context->error, context->error_size, "the area banner is not ported");
        return false;
    }
    static uint8_t vram[OBJ_VRAM_BYTES], buffer[LOCATION_BUFFER];
    static uint16_t text[FUSION_MZM_TEXT_CAPACITY];
    for (size_t location = 0; location < LOCATION_TEXTS; ++location) {
        const char *palette_name = location >= LT_SAVE_ROOM ? plan->palette
                                                            : "sAreaBannerLocationTextPal";
        uint32_t pointer = 0;
        size_t length = 0;
        if (!stage(context, plan->graphics, palette_name, 0, vram, &palette) ||
            !english_text(context, "sLocationTextPointers", location, &pointer) ||
            !read_text(context, pointer, text, &length))
            return false;
        memset(buffer, 0xFF, sizeof buffer);
        FusionMzmTextVram target = {buffer, sizeof buffer};
        if (!fusion_mzm_draw_location(&target, font, text, length)) {
            set_error(context->error, context->error_size, "cannot draw location text %zu",
                      location);
            return false;
        }
        memcpy(vram + SPRITE_VRAM, buffer, 0x380);
        memcpy(vram + SPRITE_VRAM + 0x400, buffer + 0x400, 0x380);
        for (size_t table = 0; table < plan->table_count; ++table) {
            Frame frames[MAX_FRAMES];
            size_t count = 0;
            char key[256];
            snprintf(key, sizeof key, "Sprite/AREA_BANNER/%s/none/lt%zu", plan->tables[table] + 1,
                     location);
            if (!frame_data(context, plan->tables[table], frames, &count) ||
                !emit(writer, key, vram, &palette, frames, count, 0))
                return false;
        }
    }
    return true;
}

static bool export_common(Writer *writer) {
    Context *context = writer->context;
    static uint8_t vram[OBJ_VRAM_BYTES];
    Palette palette;
    if (!stage_common(context, vram, &palette)) return false;
    for (size_t sprite = 0; sprite < COUNT(common_sprites); ++sprite) {
        for (size_t table = 0; table < common_sprites[sprite].count; ++table) {
            const char *name = common_sprites[sprite].tables[table];
            Frame frames[MAX_FRAMES];
            size_t count = 0;
            char key[256];
            snprintf(key, sizeof key, "Sprite/%s/%s/none", common_sprites[sprite].sprite,
                     name + 1);
            if (!frame_data(context, name, frames, &count) ||
                !emit(writer, key, vram, &palette, frames, count, 0))
                return false;
        }
    }
    for (size_t table = 0; table < COUNT(particles); ++table) {
        Frame frames[MAX_FRAMES];
        size_t count = 0;
        char key[256];
        snprintf(key, sizeof key, "Particle/%s/none", particles[table] + 1);
        if (!frame_data(context, particles[table], frames, &count) ||
            !emit(writer, key, vram, &palette, frames, count, 0))
            return false;
    }
    return true;
}

/* A symbol as unsigned bytes, signed or unsigned halfwords. */
static bool append_words(const Context *context, Text *text, const char *name, int width,
                         bool is_signed) {
    size_t size = 0;
    const uint8_t *data = symbol_data(context, name, &size);
    if (!data) return false;
    if (size % (size_t)width) {
        set_error(context->error, context->error_size, "%s is not whole values", name);
        return false;
    }
    for (size_t offset = 0; offset < size; offset += (size_t)width) {
        long value = width == 1 ? data[offset] : read_u16(data + offset);
        if (is_signed && width == 2 && value >= 0x8000) value -= 0x10000;
        if (!text_append(text, "\t%ld", value)) return false;
    }
    return true;
}

static bool append_stats(const Context *context, Text *text, const char *tag,
                         const char *table, const FusionMzmEnumName *names, size_t count,
                         unsigned total, size_t *columns) {
    size_t size = 0;
    const uint8_t *data = symbol_data(context, table, &size);
    if (!data) return false;
    size_t values = size / 2;
    if (!*columns) *columns = total ? values / total : 0;
    if (!*columns || *columns * total != values) {
        set_error(context->error, context->error_size, "%s does not match the %s count",
                  table, tag[0] == 's' && tag[1] == 's' ? "SSPRITE" : "PSPRITE");
        return false;
    }
    for (size_t index = 0; index < count; ++index) {
        if (!text_append(text, "%s\t%s\t%u", tag, names[index].name, names[index].value))
            return false;
        for (size_t column = 0; names[index].value < total && column < *columns; ++column)
            if (!text_append(text, "\t%u", read_u16(data + (names[index].value * *columns +
                                                             column) * 2)))
                return false;
        if (!text_append(text, "\n")) return false;
    }
    return true;
}

static bool append_multi(const Context *context, Text *text, const char *name) {
    const FusionMzmRomSymbol *table = require_symbol(context, name);
    if (!table) return false;
    for (size_t frame = 0; frame < table->size / 8; ++frame) {
        const uint8_t *record = slice(context, table->address + (uint32_t)(frame * 8), 8, name);
        if (!record) return false;
        uint32_t pointer = read_u32(record);
        if (!pointer) break;
        const FusionMzmRomSymbol *parts = symbol_at(context, pointer);
        const uint8_t *data = parts ? slice(context, pointer, parts->size, name) : NULL;
        if (!data) {
            if (!parts) set_error(context->error, context->error_size,
                                  "no MultiSpriteData parts at 0x%08x", (unsigned)pointer);
            return false;
        }
        if (!text_append(text, "multi\t%s\t%zu\t%u", name, frame, record[4])) return false;
        for (size_t part = 0; part < parts->size / 6; ++part) {
            const uint8_t *values = data + part * 6;
            if (!text_append(text, "\t%d,%d,%d", (int16_t)read_u16(values),
                             (int16_t)read_u16(values + 2), (int16_t)read_u16(values + 4)))
                return false;
        }
        if (!text_append(text, "\n")) return false;
    }
    return true;
}

/* metadata/sprites.tsv. ``mecha`` receives Mecha Ridley's OBJ palette. */
static bool write_tables(const Context *context, Text *text, const unsigned *calls,
                         int32_t mecha[256]) {
    if (!text_append(text, "schema\tmetroidvania-mzm-sprite-tables-v1\n")) return false;
    for (size_t index = 0; index < fusion_mzm_message_enum_count; ++index)
        if (!text_append(text, "message\t%u\t%s\t%u\n", fusion_mzm_message_enum[index].value,
                         fusion_mzm_message_enum[index].name, calls[index]))
            return false;
    size_t columns = 0;
    bool ok = text_append(text, "random") &&
              append_words(context, text, "sSpriteRandomNumberTable", 1, false) &&
              text_append(text, "\ngeneric_random") &&
              append_words(context, text, "sRandomNumberTable", 1, false) &&
              text_append(text, "\nfalling") &&
              append_words(context, text, "sSpritesFallingSpeed", 2, true) &&
              text_append(text, "\n") &&
              append_stats(context, text, "stats", "sPrimarySpriteStats",
                           fusion_mzm_primary_sprite_enum, fusion_mzm_primary_sprite_enum_count,
                           fusion_mzm_primary_sprite_enum_total, &columns) &&
              append_stats(context, text, "sstats", "sSecondarySpriteStats",
                           fusion_mzm_secondary_sprite_enum,
                           fusion_mzm_secondary_sprite_enum_count,
                           fusion_mzm_secondary_sprite_enum_total, &columns);
    for (size_t index = 0; ok && index < COUNT(ai_tables); ++index)
        ok = text_append(text, "table\t%s", ai_tables[index]) &&
             append_words(context, text, ai_tables[index], 2, true) && text_append(text, "\n");
    for (size_t index = 0; ok && index < COUNT(byte_pair_tables); ++index) {
        size_t size = 0;
        const uint8_t *data = symbol_data(context, byte_pair_tables[index], &size);
        ok = data && text_append(text, "table\t%s", byte_pair_tables[index]);
        for (size_t offset = 0; ok && offset + 1 < size; offset += 2)
            ok = text_append(text, "\t%u", data[offset] | data[offset + 1] << 8);
        ok = ok && text_append(text, "\n");
    }
    /* sHazeData[EFFECT_HAZE_COUNT][4]: haze value, damage effect, water
     * movement and blending kind of each room effect. */
    ok = ok && text_append(text, "haze") && append_words(context, text, "sHazeData", 1, false) &&
         text_append(text, "\n");
    for (size_t index = 0; ok && index < COUNT(multi_sprite_data); ++index)
        ok = append_multi(context, text, multi_sprite_data[index]);
    if (!ok) return false;
    /* Mecha Ridley's OBJ palette (BGR555, -1: not loaded) for its indexed
     * frames: the common rows and its own rows 8-15. */
    for (size_t index = 0; index < 256; ++index) mecha[index] = -1;
    size_t size = 0;
    const uint8_t *data = symbol_data(context, "sCommonSpritesPal", &size);
    if (!data) return false;
    for (size_t index = 0; index < size / 2 && index < 256 - 32; ++index)
        mecha[COMMON_PALETTE_ROW * 16 + index] = read_u16(data + index * 2);
    const FusionMzmSpritePlan *plan = find_plan("MECHA_RIDLEY");
    data = plan ? symbol_data(context, plan->palette, &size) : NULL;
    if (!data) {
        if (!plan) set_error(context->error, context->error_size, "Mecha Ridley is not ported");
        return false;
    }
    for (size_t index = 0; index < size / 2 && index < 128; ++index)
        mecha[128 + index] = read_u16(data + index * 2);
    if (!text_append(text, "objpal\tMECHA_RIDLEY")) return false;
    for (size_t index = 0; index < 256; ++index)
        if (!text_append(text, "\t%ld", (long)mecha[index])) return false;
    return text_append(text, "\n");
}

static bool add_name(const char ***names, size_t *count, size_t *capacity, const char *name) {
    if (*count == *capacity) {
        size_t next = *capacity ? *capacity * 2 : 256;
        const char **grown = realloc(*names, next * sizeof *grown);
        if (!grown) return false;
        *names = grown;
        *capacity = next;
    }
    (*names)[(*count)++] = name;
    return true;
}

/* Every ELF symbol the export reads must hold the ROM's bytes. */
static bool verify_inputs(const Context *context, const uint8_t *elf, size_t elf_size) {
    static const char *const fixed[] = {
        "sSpriteRandomNumberTable", "sSpritesFallingSpeed", "sPrimarySpriteStats",
        "sRandomNumberTable", "sMechaRidleyWeaponsGfx", "sMechaRidleyDestroyedGfx",
        "sRuinsTestPal_SymbolShot_Frame1", "sRuinsTestPal_SymbolShot_Frame2",
        "sRuinsTestPal_SymbolShot_Frame3", "sRuinsTestPal_SymbolShot_Frame4",
        "sRuinsTestPal_SymbolShot_Frame5", "sRuinsTestPal_SymbolShot_Frame6",
        "sRuinsTestPal_SamusReflection", "sRuinsTestPal_SamusReflectionFullSuit",
        "sEscapeGateAndTimerGfx", "sEscapeGateAndTimerPal", "sEscapeGateFlashingPal",
        "sEscapeShipFlashingPal", "sSecondarySpriteStats", "sHazeData",
        "sGunshipFlashingPal", "sMetroidPal_SamusGrabbed", "sLocationTextPointers",
        "sAreaBannerLocationTextPal", "sCommonSpritesGfx", "sCommonSpritesPal",
        "sMessageBannerGfx", "sMessageBannerPal", "sMessageTextPointers", "sCharactersGfx",
        "sCharacterWidths",
    };
    const char **names = NULL;
    size_t count = 0, capacity = 0;
    bool ok = true;
    for (size_t index = 0; ok && index < COUNT(fixed); ++index)
        ok = add_name(&names, &count, &capacity, fixed[index]);
    for (size_t sprite = 0; ok && sprite < fusion_mzm_sprite_plan_count; ++sprite) {
        const FusionMzmSpritePlan *plan = &fusion_mzm_sprite_plan[sprite];
        ok = add_name(&names, &count, &capacity, plan->graphics) &&
             add_name(&names, &count, &capacity, plan->palette);
        for (size_t table = 0; ok && table < plan->table_count; ++table)
            ok = add_name(&names, &count, &capacity, plan->tables[table]);
    }
    for (size_t set = 0; ok && set < COUNT(reflection_gfx); ++set)
        ok = add_name(&names, &count, &capacity, reflection_gfx[set].top) &&
             add_name(&names, &count, &capacity, reflection_gfx[set].bottom);
    for (size_t index = 0; ok && index < COUNT(ai_tables); ++index)
        ok = add_name(&names, &count, &capacity, ai_tables[index]);
    for (size_t index = 0; ok && index < COUNT(byte_pair_tables); ++index)
        ok = add_name(&names, &count, &capacity, byte_pair_tables[index]);
    for (size_t index = 0; ok && index < COUNT(multi_sprite_data); ++index)
        ok = add_name(&names, &count, &capacity, multi_sprite_data[index]);
    for (size_t sprite = 0; ok && sprite < COUNT(common_sprites); ++sprite)
        for (size_t table = 0; ok && table < common_sprites[sprite].count; ++table)
            ok = add_name(&names, &count, &capacity, common_sprites[sprite].tables[table]);
    for (size_t index = 0; ok && index < COUNT(particles); ++index)
        ok = add_name(&names, &count, &capacity, particles[index]);
    for (size_t index = 0; ok && index < fusion_mzm_banner_table_count; ++index)
        ok = add_name(&names, &count, &capacity, fusion_mzm_banner_tables[index]);
    if (!ok) set_error(context->error, context->error_size, "cannot list the sprite inputs");
    for (size_t index = 0; ok && index < count; ++index)
        ok = verify_symbol(context, elf, elf_size, names[index]);
    free(names);
    return ok;
}

static bool write_output(const char *output, const char *name, const Text *text,
                         char *error, size_t error_size) {
    char path[FUSION_EXPORT_PATH_CAPACITY];
    if (!fusion_export_join(path, output, name)) {
        set_error(error, error_size, "sprite output path is too long");
        return false;
    }
    return fusion_export_write_file(path, (const uint8_t *)(text->data ? text->data : ""),
                                    text->size, error, error_size);
}

bool fusion_mzm_export_sprites(const FusionRom *rom, const uint8_t *elf, size_t elf_size,
                               const char *runtime_output, const char *metadata_output,
                               FusionMzmSpriteExportSummary *summary,
                               char *error, size_t error_size) {
    if (summary) *summary = (FusionMzmSpriteExportSummary){0};
    if (!rom || rom->game != FUSION_ASSET_GAME_METROID || !rom->data || !elf ||
        !runtime_output || !metadata_output || !summary) {
        set_error(error, error_size, "invalid sprite export request");
        return false;
    }
    if (error && error_size) error[0] = '\0';
    FusionMzmRomSymbolTable symbols = {0};
    Context context = {rom, NULL, 0, &symbols, error, error_size};
    FusionSpriteLibrary library = {0};
    FusionSpriteLibrarySummary totals = {0};
    Text tables = {0}, bounds = {0};
    Canvas canvas = {0}, other = {0};
    Rendered *rendered = calloc(MAX_FRAMES, sizeof *rendered);
    Variant *variants = calloc(64, sizeof *variants);
    uint8_t (*banners)[OBJ_VRAM_BYTES] =
        calloc(fusion_mzm_message_enum_count ? fusion_mzm_message_enum_count : 1,
               sizeof *banners);
    unsigned *calls = calloc(fusion_mzm_message_enum_count + 1, sizeof *calls);
    static uint16_t text[FUSION_MZM_TEXT_CAPACITY];
    static uint8_t vram[OBJ_VRAM_BYTES];
    canvas.rgba = calloc((size_t)CANVAS_WIDTH * CANVAS_HEIGHT, 4);
    other.rgba = calloc((size_t)CANVAS_WIDTH * CANVAS_HEIGHT, 4);
    bool ok = rendered && variants && banners && calls && canvas.rgba && other.rgba;
    if (!ok) set_error(error, error_size, "cannot allocate the sprite export");
    ok = ok && fusion_mzm_read_rom_symbols(rom, elf, elf_size, &symbols, error, error_size);
    if (ok) {
        context.by_name = malloc((symbols.count ? symbols.count : 1) * sizeof *context.by_name);
        ok = context.by_name != NULL;
        if (!ok) set_error(error, error_size, "cannot index the ELF symbols");
    }
    if (ok) {
        for (size_t index = 0; index < symbols.count; ++index)
            context.by_name[index] = &symbols.entries[index];
        context.count = symbols.count;
        qsort(context.by_name, context.count, sizeof *context.by_name, compare_names);
        ok = verify_inputs(&context, elf, elf_size);
    }
    /* The message banner timings and graphics with each English message. */
    FusionMzmFont font = {0};
    Palette banner_palette;
    ok = ok && load_font(&context, &font) &&
         stage(&context, "sMessageBannerGfx", "sMessageBannerPal", 0, vram, &banner_palette);
    for (size_t message = 0; ok && message < fusion_mzm_message_enum_count; ++message) {
        uint32_t pointer = 0;
        size_t length = 0;
        memcpy(banners[message], vram, OBJ_VRAM_BYTES);
        FusionMzmTextVram target = {banners[message], OBJ_VRAM_BYTES};
        ok = english_text(&context, "sMessageTextPointers",
                          fusion_mzm_message_enum[message].value, &pointer) &&
             read_text(&context, pointer, text, &length);
        if (ok && !fusion_mzm_render_message(&target, &font, text, length, SPRITE_VRAM,
                                             &calls[message])) {
            set_error(error, error_size, "message banner text %s never finished",
                      fusion_mzm_message_enum[message].name);
            ok = false;
        }
    }
    int32_t mecha[256];
    if (ok) {
        ok = write_tables(&context, &tables, calls, mecha);
        if (!ok && error && !error[0]) set_error(error, error_size, "cannot serialize sprites.tsv");
    }
    ok = ok && fusion_export_ensure_directory(metadata_output, error, error_size) &&
         write_output(metadata_output, "sprites.tsv", &tables, error, error_size) &&
         fusion_sprite_library_init(&library, runtime_output, index_prefix, error, error_size) &&
         text_append(&bounds, "schema\tmetroidvania-oam-bounds-v1\n");
    Writer writer = {&context, &library, &bounds, &canvas, &other, rendered};
    Palette common;
    static uint8_t common_vram[OBJ_VRAM_BYTES];
    ok = ok && stage_common(&context, common_vram, &common);
    for (size_t sprite = 0; ok && sprite < fusion_mzm_sprite_plan_count; ++sprite) {
        const FusionMzmSpritePlan *plan = &fusion_mzm_sprite_plan[sprite];
        if (!strcmp(plan->name, "MECHA_RIDLEY")) {
            bool loaded[256];
            uint8_t preview[256][3];
            for (size_t index = 0; index < 256; ++index) {
                loaded[index] = mecha[index] >= 0;
                memset(preview[index], 0, 3);
                if (loaded[index]) bgr555((uint16_t)mecha[index], preview[index]);
            }
            Palette unused;
            ok = stage(&context, plan->graphics, plan->palette, plan->row, vram, &unused) &&
                 export_mecha_ridley(&writer, plan, vram, loaded,
                                     (const uint8_t (*)[3])preview);
        } else {
            ok = export_sprite(&writer, plan, &common, variants);
        }
    }
    ok = ok && export_banners(&writer, &font, banners) && export_common(&writer) &&
         write_output(runtime_output, "oam_bounds.tsv", &bounds, error, error_size) &&
         fusion_sprite_library_finish(&library, &totals, error, error_size);
    if (ok) {
        *summary = (FusionMzmSpriteExportSummary){totals.sequences, totals.frames,
                                                  totals.unique_bmps};
        if (error && error_size) error[0] = '\0';
    } else if (error && error_size && !error[0]) {
        set_error(error, error_size, "cannot export the Zero Mission sprites");
    }
    fusion_sprite_library_free(&library);
    fusion_mzm_free_symbols(&symbols);
    free(context.by_name);
    free(tables.data);
    free(bounds.data);
    free(canvas.rgba);
    free(other.rgba);
    free(rendered);
    free(variants);
    free(banners);
    free(calls);
    return ok;
}
