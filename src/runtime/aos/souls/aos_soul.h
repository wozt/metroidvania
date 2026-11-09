/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SOUL_H
#define AOS_SOUL_H
#include <stdbool.h>
#include <stdint.h>

#include "../../aos_stats.h"

/* One of Soma's red soul records (0x080E1598, metadata/red_souls.tsv). */
typedef struct {
    uint32_t skill;      /* + 0x0: the skill entity's routine, 0 for none */
    uint16_t pose;       /* + 0x4: cast pose 0-3 */
    int16_t mp;          /* + 0x6: MP cost */
    uint8_t maximum;     /* + 0x8: skill entities alive at once */
    uint16_t parameter;  /* + 0xE: given to the skill entity (+ 0x1E) */
    uint16_t power;      /* + 0xA: sub_08021530(5) */
    uint16_t elements;   /* + 0xC: sub_0806B7D8's attack elements */
} AosRedSoulRecord;

enum { AOS_RED_SOUL_RECORDS = 56, AOS_RED_SOUL_SLOTS = 8 };

/* The active red soul entities: activeRedSoulEntities (0x13124),
 * nbrActiveRedSoulEntities (0x131BE) and lastUsedRedSoul (0x131BF). */
typedef struct {
    bool slots[AOS_RED_SOUL_SLOTS];
    uint8_t active;
    uint8_t last_used;
} AosRedSoulCaster;

/* The guards and bookkeeping of sub_08019478 (Soma's red soul use; the
 * 0x131B8 & 0x100 lock and gEwramData + 0x344 & 0x200000 are passed as
 * `locked`): with no skill entity alive the last used soul follows the
 * equipped one; the cast needs that soul, a skill routine, at least its MP
 * cost (Soma may spend all his MP), fewer active entities than the last
 * used record's maximum and a free slot (sub_0802D208). It then spends the
 * MP and takes the slot. Returns the slot, or -1. */
int aos_red_soul_try(AosRedSoulCaster *caster, AosStats *stats,
                     const AosRedSoulRecord records[AOS_RED_SOUL_RECORDS], bool locked);
/* The cast animation of sub_0802D1D8: 0x080E1918[pose * 2 + airborne]. */
uint8_t aos_red_soul_cast_animation(uint16_t pose, bool airborne);
/* A skill entity's end: its slot is freed and the active count drops. */
void aos_red_soul_release(AosRedSoulCaster *caster, int slot);

/* sub_08021530(5) in normal play (gEwramData + 0x4CC bit 6 clear) for the
 * last used soul's record: (STR * power / 10 + INT / 4 + power * 6) / 16,
 * scaled by 8 / 10 when gEwramData + 0xA1 bit 4 is set (`reduced`). */
int32_t aos_red_soul_power(const AosStats *stats, const AosRedSoulRecord *record, bool reduced);
/* sub_08021994's MP regeneration, run with Soma's + 0x22 tick counter
 * before it advances: accessory 0x2C keeps MP full; otherwise one MP every
 * 64 ticks (every 16 with accessory 0x29), at most the maximum. The curse
 * states of unk_13270 are not modelled. */
void aos_stats_mp_regen(AosStats *stats, uint16_t ticks);
/* The status part of sub_08021994, after aos_stats_frame_reset and before
 * the ATK/DEF derivation: stone presses (A, B and the D-pad of `pressed`)
 * wear the stone off (0xB0 on odd counts, 0xAF when it breaks; the dust
 * and debris are not modelled); the poison and curse timers run out;
 * poison quarters STR, CON and INT; curse drains max MP / 128 (at least 1)
 * every 4 frames instead of the MP regeneration of aos_stats_mp_regen,
 * which runs otherwise with `ticks`; accessory 0x2C keeps MP full. */
void aos_stats_status_update(AosStats *stats, uint16_t pressed, uint16_t ticks);
/* func_08021E50: cure the status bits set in `mask` (bits 0-2 =
 * poison, curse, stone) - each matching status_timers entry is zeroed
 * and its status bit cleared. */
void aos_stats_cure_status(AosStats *stats, uint16_t mask);
/* One of Soma's blue soul records (0x080E192C, metadata/blue_souls.tsv),
 * indexed by the equipped blue soul (0 is none). */
typedef struct {
    uint32_t routine;       /* + 0x0: the guardian entity's routine, 0 for none */
    uint8_t mp_per_second;  /* + 0x4: one MP every 60 / it frames */
    uint8_t flags;          /* + 0x5: AOS_BLUE_SOUL_TOGGLE */
    uint16_t parameter;     /* + 0x6: the entity's + 0x1A */
    uint32_t argument;      /* + 0x8: the entity's + 0x1C */
} AosBlueSoulRecord;

enum { AOS_BLUE_SOUL_RECORDS = 25, AOS_BLUE_SOUL_TOGGLE = 2 };

/* The guardian entity of gEwramData + 0x13144 as its upkeep sees it. */
typedef struct {
    bool active;
    uint8_t soul;           /* + 0x18: the blue soul it was created for */
    uint8_t interval;       /* + 0x20: frames per drained MP */
    uint16_t ticks;         /* + 0x22 */
    uint8_t state;          /* + 0x0A: its routine's state, 0 on the first update */
} AosGuardian;

/* sub_0802E0C4, run at the end of Soma's states 0, 12 and 13: with no
 * guardian alive and the guardian button held (and pressed this frame for
 * a toggled soul), a record with a routine and at least max MP / 128 + 1
 * MP, the guardian starts and Soma's flag 0x200 is set (`soma_flags`). The
 * 0x131B8 & 0x200 lock and the gEwramData + 0x344 & 0x200000 limit to
 * souls 1-3 are passed as `locked`. */
bool aos_guardian_try(AosGuardian *guardian, const AosStats *stats,
                      const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS], uint16_t held,
                      uint16_t pressed, bool locked, uint32_t *soma_flags);
/* sub_0802DF2C (sub_0802DE5C adds the cleanup of aos_guardian_end): one
 * MP every `interval` frames, the soul ending when MP reaches 0; a held
 * soul ends once the button is released, a toggled one when it is pressed
 * again after its first update. The gEwramData + 0x42C bits 0 and 8 are
 * not modelled. Returns true when the guardian must end. */
bool aos_guardian_upkeep(AosGuardian *guardian, AosStats *stats,
                         const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS], uint16_t held,
                         uint16_t pressed);
/* sub_0802DD84: the guardian slot is freed and Soma's flag 0x200 cleared
 * (its child entities, the 0x131D0 palette and the 0x13260 bits 8-13 belong
 * to the souls that use them). */
void aos_guardian_end(AosGuardian *guardian, uint32_t *soma_flags);

/* SkillMedusaHeadUse: while Soma is airborne (+ 0x10 & 0x01008082 == 2) it
 * holds him still: after his update it zeroes his vy and + 0x54 and sets
 * gEwramData + 0x131B8 & 0x20 (`hold_still`), which the end of his next
 * update reads to zero his vx and friction. It keeps his vy (0 when rising)
 * and gravity 0x2800 to give back when it ends. */
typedef struct {
    int32_t vy, gravity;    /* + 0x4C, + 0x54 */
} AosMedusaGuardian;

/* One update after Soma's: sound 0x8C on its first, then the upkeep.
 * Returns false once the guardian has ended (its cleanup done). */
bool aos_medusa_guardian_update(AosMedusaGuardian *medusa, AosGuardian *guardian,
                                AosStats *stats,
                                const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                                uint16_t held, uint16_t pressed, uint32_t *soma_flags,
                                int32_t *soma_vy, int32_t *soma_gravity, bool *hold_still);
/* sub_08021530(6) in normal play: the guardian's power gEwramData + 0x131C2
 * plus power / 8 times INT / 2, scaled by 8 / 10 when `reduced`. */
int32_t aos_guardian_power(const AosStats *stats, uint16_t power, bool reduced);

/* SkillCagnazzoUse (soul 11, toggled): after the upkeep, its first update
 * sets the guardian power to 12 with no elements, its attack box
 * 0x1414050A (10, 5, 20 x 20, type 6) and sound 0x1AA; every update then
 * places it 16 pixels ahead of Soma (his facing) and 32 above his feet.
 * Returns false once the guardian has ended (its cleanup done). */
typedef struct {
    int32_t x, y;           /* whole pixels */
    bool facing_left;
} AosCagnazzoGuardian;

enum { AOS_CAGNAZZO_POWER = 12 };

/* SkillBlackPantherUse (blue soul 3): an unseen type-6 body (box
 * 0x3030D8F4: -12, -40, 48 x 48) on Soma, power 8 and elements 1 (0x131C2,
 * 0x131C4), striking only while Soma runs (his animation 3, the first frame
 * of a run playing sound 0x9A), and the record's 0x13260 bit (0x400, the
 * fast walk) each update. sub_08042A54(2, 0x3C) is not run. Returns false
 * once the guardian has ended (its cleanup done). */
typedef struct {
    int32_t x, y;           /* whole pixels */
    bool facing_left;
    bool attacking;         /* + 0x72 bit 1 clear */
    bool sounded;           /* + 0x0B */
} AosPantherGuardian;

/* SkillFlyingArmorUse (blue soul 1): a translucent armor on Soma (art
 * 0x0825E08C, looping sequence 0, priority 0xD), facing like him, and the
 * record's 0x13260 bit (0x100, the slow fall) each update. Its sparkles
 * (sub_08045B44 type 8, row 9, every fourth frame) are not ported: their
 * animation record 0x08525194 is not exported. Returns false once the
 * guardian has ended. */
typedef struct {
    int32_t x, y;           /* whole pixels */
    bool facing_left;
} AosFlyingArmorGuardian;

bool aos_flying_armor_guardian_update(AosFlyingArmorGuardian *armor, AosGuardian *guardian,
                                      AosStats *stats,
                                      const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                                      uint16_t held, uint16_t pressed, uint32_t *soma_flags,
                                      int32_t soma_x, int32_t soma_y, bool soma_facing_left);

/* SkillPersephoneUse (blue soul 16, toggled): a translucent guardian on
 * Soma with his facing (art 0x081E0968, looping sequence 0, priority 0xD),
 * guardian power 10 and elements 0 (0x131C2, 0x131C4), striking as type 6
 * with its frame's attack box (sub_0802E1D0), its hit callback
 * sub_08031F28 healing Soma 5 HP for each enemy it strikes. It holds sound
 * 0x9C each update (sub_08013DA8). Below state 3 the upkeep sub_0802DF2C
 * runs first; its end releases 0x9C (sub_08013E18) and starts state 3:
 * blinking (+ 0x0F) for 16 frames, then the cleanup.
 * sub_08042A54(2, 0x14) is not run. Returns false once it has ended. */
typedef struct {
    int32_t x, y;           /* whole pixels */
    bool facing_left;
    bool blinking;          /* + 0x0F */
    uint8_t timer;          /* + 0x0D */
} AosPersephoneGuardian;

enum { AOS_PERSEPHONE_POWER = 10, AOS_PERSEPHONE_ELEMENTS = 0, AOS_PERSEPHONE_LEAVING = 3,
       AOS_PERSEPHONE_HEAL = 5 };

bool aos_persephone_guardian_update(AosPersephoneGuardian *maid, AosGuardian *guardian,
                                    AosStats *stats,
                                    const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                                    uint16_t held, uint16_t pressed, uint32_t *soma_flags,
                                    int32_t soma_x, int32_t soma_y, bool soma_facing_left);

/* SkillGreatArmorUse (blue soul 8, held): a translucent armor on Soma with
 * his facing (art 0x081E116C, looping sequence 1, priority 0xD). Its
 * + 0x0D counts: every 32nd frame sound 0x94, on the other 16th frames one
 * chance in two of it; every 8th frame a drifting spark (sub_08045CEC
 * animation 16, row 7) up to 8 pixels from Soma's x and 8 to 24 above his
 * feet (`spark`). Each update multiplies the current stat of its + 0x1C
 * (0: STR) by 0x133 / 256 (rounded toward zero), then sub_08021C44.
 * Returns false once the guardian has ended. */
typedef struct {
    int32_t x, y;           /* whole pixels */
    bool facing_left;
    uint8_t timer;          /* + 0x0D */
    bool spark;             /* a spark this update at (spark_x, spark_y) */
    int32_t spark_x, spark_y; /* 16.16 */
} AosGreatArmorGuardian;

bool aos_great_armor_guardian_update(AosGreatArmorGuardian *armor, AosGuardian *guardian,
                                     AosStats *stats,
                                     const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                                     uint16_t held, uint16_t pressed, uint32_t *soma_flags,
                                     int32_t soma_x, int32_t soma_y, bool soma_facing_left,
                                     uint32_t (*random)(void));

/* SkillShadowKnightUse (blue soul 13): a translucent knight on Soma facing
 * against him (+ 0x58 bit 6 his inverted), guardian power 0x23 and
 * elements 1, sound 0x1A6 (sub_08042A54(2, 0x3C) is not run). For its
 * first 32 frames (state 1) it stretches in with an affine scale (+ 0x28
 * by 0x1000, + 0x2C by 0x800 a frame up to 1.0) in sequence 1; then
 * (state 2) whenever Soma's weapon entity exists it swings (sequence 0
 * once, state 3) and back to sequence 1 at the swing's end. It strikes as
 * type 6 with its frame's attack box (sub_0802E1D0). `sequence` is the
 * sequence to start this update (-1 for none). Returns false once ended. */
typedef struct {
    int32_t x, y;           /* whole pixels */
    bool mirrored;          /* + 0x58 bit 6 */
    uint8_t timer;          /* + 0x0D */
    int32_t scale_x, scale_y; /* + 0x28, + 0x2C (0 once normal) */
    int sequence;
} AosShadowKnightGuardian;

enum { AOS_SHADOW_KNIGHT_POWER = 0x23, AOS_SHADOW_KNIGHT_ELEMENTS = 1,
       AOS_BIG_GOLEM_POWER = 0x1E, AOS_BIG_GOLEM_ELEMENTS = 0 };

bool aos_shadow_knight_guardian_update(AosShadowKnightGuardian *knight, AosGuardian *guardian,
                                       AosStats *stats,
                                       const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                                       uint16_t held, uint16_t pressed, uint32_t *soma_flags,
                                       int32_t soma_x, int32_t soma_y, bool soma_facing_left,
                                       bool weapon_out, bool sequence_ended);
/* SkillBigGolemUse (blue soul 12, toggled): the same stretched guardian as
 * the Shadow Knight (power 0x1E, elements 0, sound 0x1A6), facing like
 * Soma, 12 pixels behind him and 4 above his feet, idling in sequence 0
 * and punching (sequence 1 once) whenever his weapon entity exists, sound
 * 0xC5 when its punch enters frame 4 (`frame`, `tick`: its + 0x6E before
 * this update's step). */
bool aos_big_golem_guardian_update(AosShadowKnightGuardian *golem, AosGuardian *guardian,
                                   AosStats *stats,
                                   const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                                   uint16_t held, uint16_t pressed, uint32_t *soma_flags,
                                   int32_t soma_x, int32_t soma_y, bool soma_facing_left,
                                   bool weapon_out, bool sequence_ended, uint8_t frame,
                                   uint8_t tick);

/* SkillAluraUneUse (blue soul 17, held): at full HP it ends before its
 * upkeep (sub_0802DD84). Otherwise a translucent fairy (art 0x081DE154,
 * looping sequence 0, priority 0xD) 16 pixels behind Soma (his facing)
 * and 26 above his feet, growing by an affine scale (+ 0x28 by 0x1000,
 * + 0x2C by 0x2000 a frame up to 1.0); every 17th frame of its + 0x0D it
 * plays 0x140 and heals 10 HP (sub_08021FEC), then at full HP one chance
 * in eight of sound 0x1AC. Returns false once the guardian has ended. */
typedef struct {
    int32_t x, y;           /* whole pixels */
    bool facing_left;
    uint8_t timer;          /* + 0x0D */
    int32_t scale_x, scale_y; /* + 0x28, + 0x2C */
} AosAluraUneGuardian;

bool aos_alura_une_guardian_update(AosAluraUneGuardian *fairy, AosGuardian *guardian,
                                   AosStats *stats,
                                   const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                                   uint16_t held, uint16_t pressed, uint32_t *soma_flags,
                                   int32_t soma_x, int32_t soma_y, bool soma_facing_left,
                                   int16_t *hp, int16_t max_hp, uint32_t (*random)(void));

enum { AOS_PANTHER_POWER = 8, AOS_PANTHER_ELEMENTS = 1, AOS_PANTHER_RUN_ANIM = 3 };

bool aos_panther_guardian_update(AosPantherGuardian *panther, AosGuardian *guardian,
                                 AosStats *stats,
                                 const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                                 uint16_t held, uint16_t pressed, uint32_t *soma_flags,
                                 int32_t soma_x, int32_t soma_y, bool soma_facing_left,
                                 uint8_t soma_anim);

bool aos_cagnazzo_guardian_update(AosCagnazzoGuardian *cagnazzo, AosGuardian *guardian,
                                  AosStats *stats,
                                  const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                                  uint16_t held, uint16_t pressed, uint32_t *soma_flags,
                                  int32_t soma_x, int32_t soma_y, bool soma_facing_left);
/* SkillBuerUse (soul 4, toggled): an unseen parent over Soma (his x, his
 * feet plus a height that eases toward -20, or -8 while he crouches, by
 * 0.25 a frame from -16) whose radius grows to 28, and two flames of
 * sub_0802FDE4 at angles 0 and 0x8000 turning by 0x200 a frame (-0x200
 * when Soma faces left) at that radius. Each flame attacks with box
 * 0x1414F6F6 (-10, -10, 20 x 20, type 6) at power 12 with elements 2.
 * Ended (by the upkeep, which then stops), the radius shrinks by 2 a
 * frame and at 0 the soul is cleaned up. The flames' timed type-6
 * sparkles every fourth frame (sub_08045B44(x + random 2.0, y - 4, 0,
 * -1/16, 6, 7), vy -1.0) are not ported. */
enum { AOS_BUER_FLAMES = 2, AOS_BUER_POWER = 12, AOS_BUER_ELEMENTS = 2 };

typedef struct {
    int32_t x, y;           /* + 0x40, + 0x44 (16.16) */
    uint16_t angle;         /* + 0x2C */
    int16_t speed;          /* + 0x2E */
    uint8_t timer;          /* + 0x0D */
} AosBuerFlame;

typedef struct {
    int32_t x, y;           /* + 0x40, + 0x44 (16.16, whole parts written) */
    int32_t height;         /* + 0x4C: its integer half is added to Soma's y */
    int32_t radius;         /* + 0x30 */
    AosBuerFlame flames[AOS_BUER_FLAMES];
} AosBuerGuardian;

/* One update of the parent, then of its flames (created by its first
 * update: sounds 0x1A5 and 0x92). Returns false once the soul has ended
 * and been cleaned up (the flames are gone without their update). */
bool aos_buer_guardian_update(AosBuerGuardian *buer, AosGuardian *guardian, AosStats *stats,
                              const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                              uint16_t held, uint16_t pressed, uint32_t *soma_flags,
                              int32_t soma_x, int32_t soma_y, bool soma_facing_left);
/* One of Soma's yellow soul records (0x080E1B08, metadata/yellow_souls.tsv),
 * indexed by the equipped yellow soul (0 is none). */
typedef struct {
    uint32_t routine;       /* + 0x0: the soul entity's routine, 0 for none */
    uint16_t refresh;       /* + 0x4: read by sub_08031FC4 */
    uint16_t parameter;     /* + 0x6: the entity's + 0x1A */
    uint32_t argument;      /* + 0x8: the entity's + 0x1C */
} AosYellowSoulRecord;

enum {
    AOS_YELLOW_SOUL_RECORDS = 37,
    AOS_YELLOW_FLAGS_MASK = 0x07FFC000, /* the 0x13260 bits a change clears */
    AOS_YELLOW_FLAG_ROUTINE = 0x08032205,
    AOS_YELLOW_STAT_ROUTINE = 0x0803226D,
    AOS_YELLOW_RESIST_ROUTINE = 0x080322A1,
    AOS_YELLOW_ZOMBIE_ROUTINE = 0x08032311,
    AOS_YELLOW_LUBICANT_ROUTINE = 0x08032361,
    AOS_YELLOW_WOODEN_GOLEM_ROUTINE = 0x080322E9,
    AOS_YELLOW_BASILISK_ROUTINE = 0x08032155,
    AOS_YELLOW_DOUBLE_ROUTINE = 0x0803222D,
    AOS_YELLOW_GIANT_WORM_ROUTINE = 0x080320D9,
    AOS_YELLOW_HEADHUNTER_ROUTINE = 0x080323DD,
};

/* The yellow soul entity of gEwramData + 0x13164. */
typedef struct {
    bool active;
    uint8_t soul;           /* the equipped yellow soul it runs */
    uint8_t timer;          /* + 0x0D */
} AosYellowSoul;

/* Whether the record's routine is one aos_yellow_soul_update runs. */
bool aos_yellow_soul_ported(uint32_t routine);
/* One frame of the yellow soul, after Soma's update has run
 * aos_stats_frame_reset: an equip change (sub_08031FC4 / sub_080321C4)
 * clears the 0x13260 bits 14-26 and the entity; sub_08031F3C then creates
 * it for a record with a routine. Its routine then runs:
 * - the flag souls (Undine ... Arc Demon) OR + 0x1C into 0x13260;
 * - the stat souls (Skeleton Knight ... Gremlin) add + 0x1C to the current
 *   stat + 0x1A, then sub_08021C44;
 * - Poison Worm, Ectoplasm and Gargoyle OR + 0x1C into the resistances
 *   (with a status set, sub_08021E50(+ 0x1C >> 6) is not modelled);
 * - Zombie, while poisoned (unk_13270 bit 0), adds 4.5 times STR, CON and
 *   INT to each (v += 4v + v / 2), then sub_08021C44;
 * - Lubicant adds STR and INT times the lost HP fraction and an eighth of
 *   that for CON, then sub_08021C44;
 * - Wooden Golem gives one MP every 32 frames;
 * - Basilisk adds DEF / 4 and takes ATK / 2 away;
 * - record 36 doubles ATK (+ 0x1C 0) or DEF;
 * - Headhunter (SkillHeadhunterUse) adds 1 + totalNbrSoulsCollected / 16
 *   (at most 32 + 1) to STR, CON and INT, then sub_08021C44;
 * - Giant Worm's heal is aos_yellow_soul_regen.
 * The 0x131B8 & 0x400 and 0x344 limits are not ported. */
void aos_yellow_soul_update(AosYellowSoul *soul, AosStats *stats,
                            const AosYellowSoulRecord records[AOS_YELLOW_SOUL_RECORDS],
                            int16_t hp, int16_t max_hp);
/* SkillGiantWormUse (soul 10), part of the yellow soul entity's update: with
 * HP below the maximum, Soma still (+ 0x48 = 0, + 0x10 & 0x360 clear) and
 * gEwramData + 0x42C bit 0 clear (`held`: a door walking him through), its
 * + 0x0D counts and every 61st frame heals 5 HP (sub_08021FEC; the number
 * popup sub_08042A7C is not drawn). */
void aos_yellow_soul_regen(AosYellowSoul *soul, const AosStats *stats,
                           const AosYellowSoulRecord records[AOS_YELLOW_SOUL_RECORDS],
                           int16_t *hp, int16_t max_hp, int32_t soma_vx, uint32_t soma_flags,
                           bool held);
/* sub_08021654's yellow soul rules after a hit of `damage`: Mimic
 * (0x13260 bit 0x40000) gives damage * 5 gold, at most 999999
 * (sub_08044F80); Arc Demon (bit 0x80000) gives damage * 2 MP (at least 1,
 * up to the maximum, sub_08022028). */
void aos_yellow_soul_hit(AosStats *stats, int damage);
/* SkillCatoblepasUse (soul 9, held, 15 MP per second) and its gas clouds
 * sub_08030D6C (guardian slots 1-7 of sub_0802DD00). */
enum { AOS_CATOBLEPAS_GASES = 7, AOS_CATOBLEPAS_POWER = 10, AOS_CATOBLEPAS_ELEMENTS = 0x100,
       AOS_CATOBLEPAS_POSE = 0x35 };

typedef struct {
    bool alive;
    bool facing_left;       /* + 0x58 bit 6 */
    bool started;           /* its first update (the animation starts) */
    int32_t x, y;           /* + 0x40, + 0x44 (16.16) */
    int32_t vx, vy, ax, ay; /* + 0x48, + 0x4C, + 0x50, + 0x54 */
    uint8_t timer;          /* + 0x0D */
} AosCatoblepasGas;

typedef struct {
    int32_t x, y;           /* whole pixels: Soma's x +/- 16, his y - 28 */
    bool facing_left;
    uint8_t timer;          /* + 0x0D: frames to the next cloud */
    bool sound;             /* + 0x0B: 0x8E with every other cloud */
    int16_t cycle;          /* + 0x26: index into 0x080E1A64 */
    uint8_t step;           /* + 0x0B of the Bone Pillar soul: 0-4 */
    AosCatoblepasGas gases[AOS_CATOBLEPAS_GASES];
} AosCatoblepasGuardian;

/* One update of the parent, then of its clouds. After the upkeep it needs
 * Soma grounded and still (+ 0x10 & 0x1001A2 == 0x100000), else it ends;
 * it sets 0x131B8 & 0x820 (`hold_still`; bit 0x800 is not modelled). Its
 * first update asks for Soma's pose (`pose`: state 10, animation 0x35;
 * sub_08042A54(2, 0x1E) is not run) with power 10 and elements 0x100.
 * Every update it sits 16 pixels ahead of Soma and 28 above his feet, and
 * every fifth frame it breathes a cloud (0x8E with every other): 1.5
 * forward plus Soma's vx, 0x400 more each frame up to 8.0, vy 0.5 plus
 * 0, -0x2000, 0x2000 in turn, gravity 0x200, for 26 frames, each a
 * -10, -10, 20 x 20 type-6 attack. Returns false once the soul has ended
 * (its clouds gone). */
bool aos_catoblepas_guardian_update(AosCatoblepasGuardian *catoblepas, AosGuardian *guardian,
                                    AosStats *stats,
                                    const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                                    uint16_t held, uint16_t pressed, uint32_t *soma_flags,
                                    int32_t soma_x, int32_t soma_y, bool soma_facing_left,
                                    int32_t soma_vx, bool *hold_still, bool *pose);
/* SkillBonePillarUse (soul 10, held, 30 MP per second): the same as the
 * Catoblepas soul (the same grounded-and-still rule, hold, pose and place)
 * with its own art, power 20 and elements 2 (sub_08042A54(2, 0xF) is not
 * run), its fires (sub_0802E638: animation 0 once, box 0x1414F6F6, 26
 * frames) at 1.5 forward plus Soma's vx, 0x400 more each frame, vy 1.25
 * plus 0, -0x2000, 0x2000 in turn and gravity -0x1800, sound 0x91 with the
 * first of every five fires and, at random, the third. */
enum { AOS_PILLAR_SOUL_POWER = 0x14, AOS_PILLAR_SOUL_ELEMENTS = 2 };

bool aos_pillar_guardian_update(AosCatoblepasGuardian *pillar, AosGuardian *guardian,
                                AosStats *stats,
                                const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                                uint16_t held, uint16_t pressed, uint32_t *soma_flags,
                                int32_t soma_x, int32_t soma_y, bool soma_facing_left,
                                int32_t soma_vx, bool *hold_still, bool *pose,
                                uint32_t (*random)(void));
/* Skill_Devil_Manticore_Curly_Use (souls 18, 19 and 20, held, 30 MP per
 * second, + 0x1C the variant 0 Devil, 1 Manticore, 2 Curly): it turns Soma
 * into its state 9 and, once he is transformed (+ 0x0B 3), attacks around
 * him with the power, elements and start sound of 0x080E1A74[variant]
 * while that entry's child follows him: Curly's sub_0802EE40 (its sprite,
 * sound 0x93 every eighth frame) or SkillDevilManticoreSparksUpdate (sound
 * 0x90 every eighth frame and a spark sub_0802ECF4 every fourth, from its
 * first: `spawn_spark`). */
enum { AOS_CURLY_POWER = 0x32, AOS_CURLY_ELEMENTS = 1, AOS_CURLY_VARIANT = 2,
       AOS_TRANSFORM_FLAGS = 0x3F00, AOS_TRANSFORM_VARIANTS = 3 };

/* 0x080E1A74: the child routine (sparks or the Curly sprite), the guardian
 * power (0x131C2) and elements (0x131C4) and the start sound. */
typedef struct {
    bool sparks;
    uint16_t power, elements, sound;
} AosTransformVariant;
extern const AosTransformVariant aos_transform_variants[AOS_TRANSFORM_VARIANTS];

typedef struct {
    int32_t x, y;           /* Soma's (+ 0x42, + 0x46 copied) */
    bool facing_left;
    uint8_t timer;          /* + 0x0D */
    bool attacking;         /* type 6 with box 0x4430C0E8 (-24, -64, 48 x 68) */
    bool child;             /* the variant's child exists */
    uint8_t child_timer;    /* its + 0x0D */
    uint8_t variant;        /* + 0x1C */
    bool spawn_spark;       /* the sparks child made a spark this update */
} AosCurlyGuardian;

/* What the soul changes on Soma (gEwramData + 0x13110). */
typedef struct {
    uint32_t *flags;            /* + 0x10 */
    uint32_t *abilities;        /* gEwramData + 0x13260 */
    uint8_t *state, *step;      /* + 0x0A, + 0x0B */
    uint8_t *variant;           /* + 0x27 */
    int32_t *friction, *gravity; /* + 0x50, + 0x54 */
} AosTransformTarget;

/* One update after Soma's: the upkeep (its end, or Soma airborne,
 * attacking, casting or with flag 0x80 or 0x8000, ends the soul and clears
 * 0x13260 bits 8-13); at Soma's position and facing; the first update
 * stops his friction and gravity, clears his attack flag and starts his
 * transformation with the record's variant; once he is transformed the
 * child appears and the attack starts with sound 0x1B0 (sub_08042A54(2,
 * 0xF) and the sub_0803D18C palette blend are not run). Each update puts
 * Soma in state 9 with 0x13260 bit 0x1000. Returns false once ended. */
bool aos_curly_guardian_update(AosCurlyGuardian *curly, AosGuardian *guardian, AosStats *stats,
                               const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                               uint16_t held, uint16_t pressed, int32_t soma_x, int32_t soma_y,
                               bool soma_facing_left, AosTransformTarget soma);
/* sub_0802DFF0 / sub_0802DD84 for the transformation: the guardian and its
 * child are gone and 0x13260 bits 8-13 cleared. */
void aos_curly_guardian_end(AosCurlyGuardian *curly, AosGuardian *guardian, uint32_t *soma_flags,
                            uint32_t *abilities);
#endif
