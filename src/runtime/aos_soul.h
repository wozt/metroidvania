/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SOUL_H
#define AOS_SOUL_H
#include <stdbool.h>
#include <stdint.h>

#include "aos_stats.h"

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
 * - record 36 doubles ATK (+ 0x1C 0) or DEF.
 * Giant Worm and Headhunter, and the 0x131B8 & 0x400 and 0x344 limits,
 * are not ported. */
void aos_yellow_soul_update(AosYellowSoul *soul, AosStats *stats,
                            const AosYellowSoulRecord records[AOS_YELLOW_SOUL_RECORDS],
                            int16_t hp, int16_t max_hp);
/* sub_08021654's Mimic rule (0x13260 bit 0x40000): damage * 5 gold,
 * at most 999999 (sub_08044F80). */
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
#endif
