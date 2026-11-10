/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies.
 *
 * Layout: aos_enemy.c holds the shared framework of cvaos code_08060B98;
 * one file per monster family lives in aos/enemies/ (each file names
 * the monsters and native routines it ports) and compiles as its own
 * translation unit through aos_enemy_internal.h. The ROM sine, the BIOS
 * ArcTan2 and Sqrt and the 16.16 product are in aos_math.
 *
 * The framework: creation (EnemyXCreate per id), the activity window of
 * sub_0806CC20, facing the player (sub_0806BC40), the signed velocity by
 * facing (sub_0806E120), the vertical distance to the player (sub_0806BDEC),
 * the walker and projectile collisions (sub_08069A00, sub_08069770), the
 * generic death sub_0806AEAC and the update dispatch. Facing flag 0x40 of
 * + 0x58 means mirrored: enemy sprites face left by default. Combat: the
 * frame boxes of sub_0806B1FC, the collision pass of sub_0806E314 /
 * sub_080421AC against the player and his weapon, the death flames of
 * sub_0806BE74 / sub_0806AEAC (sub_0806E524) and the blood splash of
 * sub_0806E664 (sub_0806B5DC). No SDL. */
#ifndef AOS_ENEMY_H
#define AOS_ENEMY_H

#include <stdbool.h>
#include <stdint.h>

#include "aos_anim.h"
#include "../math/aos_math.h"
#include "aos_collision.h"
#include "aos_damage_popup.h"
#include "aos_combat.h"
#include "aos_effect.h"
#include "aos_soma.h"
#include "../weapons/aos_weapon.h"

/* Stats of the enemy record copied by sub_0806B04C (from enemies.tsv). */
typedef struct {
    int16_t hp;             /* record + 0xC */
    uint8_t contact;        /* record + 0x13 -> + 0x3C: attack against the player */
    uint8_t defence;        /* record + 0x14 -> + 0x3D */
    uint16_t weak, resist;  /* record + 0x1A, + 0x1C */
    uint16_t experience;    /* record + 0x10, granted by sub_080683BC */
} AosEnemyStats;

#define AOS_ENEMY_MAX_ANIMS 20
#define AOS_ENEMY_MAX_CHILDREN 8
#define AOS_ENEMY_MAX_FRAMES 40
#define AOS_NEMESIS_EFFECT_HISTORY 24

/* The probe table of the walker sub_0806C828 (ROM, e.g. 0x08528708). */
typedef struct {
    int16_t count;          /* wall probes */
    int16_t ceiling, floor; /* y offsets of the ceiling and floor probes */
    int16_t half_width;     /* x offset of the wall probes */
    int16_t wall_y[8];
} AosProbes;

/* Per animation frame: box mode (frame record + 4: 0 none, 1 one box for
 * both, 2 hurtbox then attack box) and boxes. */
typedef struct {
    const AosAnimSet *anims;
    uint8_t modes[AOS_ENEMY_MAX_ANIMS][AOS_ENEMY_MAX_FRAMES];
    AosBox hurt[AOS_ENEMY_MAX_ANIMS][AOS_ENEMY_MAX_FRAMES];
    AosBox attack[AOS_ENEMY_MAX_ANIMS][AOS_ENEMY_MAX_FRAMES];
    uint8_t blink[40];      /* 0x08118CE0 & 1: hidden while dying */
    /* Centre of each frame's last OAM component (sub_0806C48C). */
    int8_t anchors[AOS_ENEMY_MAX_ANIMS][AOS_ENEMY_MAX_FRAMES][2];
    /* The far corner (x + width, y + height) of each frame's last OAM
     * component, mirrored by sub_0806C3F8 with the entity's flips. */
    int8_t edges[AOS_ENEMY_MAX_ANIMS][AOS_ENEMY_MAX_FRAMES][2];
    /* The same for single sprite frames shown by children (+ 0x65 < 64). */
    int8_t static_anchors[64][2];
    /* The death flame's animation (Effect/flame), for a doll piece's glow. */
    const AosAnimSet *flame_anims;
    /* Sprite frame number (+ 0x65) of each animation frame. */
    uint8_t sprites[AOS_ENEMY_MAX_ANIMS][AOS_ENEMY_MAX_FRAMES];
    /* 0x08118D08: the (x, y) screen margins of sub_0806D128(e, n). */
    int16_t margins[7][2];
    AosProbes probes;       /* walker probe table, count 0 when unused */
    /* True only for a locally imported ROM Buer probe row. Allows
     * accurate source-mode motion without changing legacy fixtures. */
    bool buer_native_motion;
    /* Exported locally from the user's ROM (0x081194DC, 8 x s16 X/Y).
     * Not a compiled-in table. Absent in legacy asset exports. */
    int16_t buer_death_offsets[8][2];
    bool buer_death_offsets_valid;
    /* Volley lists (frame, y offset) of a shooter, e.g. 0x08528614. */
    int8_t volleys[4][8][2];
    uint8_t volley_sizes[4];
} AosEnemyKind;

enum {
    AOS_ENEMY_BAT = 0x00,
    AOS_ENEMY_ZOMBIE = 0x01,
    AOS_ENEMY_SKELETON = 0x02,
    AOS_ENEMY_MERMAN = 0x03,
    AOS_ENEMY_AXE_ARMOR = 0x04,
    AOS_ENEMY_SKULL_ARCHER = 0x05,
    AOS_ENEMY_PEEPING_EYE = 0x06,
    AOS_ENEMY_KILLER_FISH = 0x07,
    AOS_ENEMY_BONE_PILLAR = 0x08,
    AOS_ENEMY_BLUE_CROW = 0x09,
    AOS_ENEMY_BUER = 0x0A,
    AOS_ENEMY_WHITE_DRAGON = 0x0B,
    AOS_ENEMY_ZOMBIE_SOLDIER = 0x0C,
    AOS_ENEMY_SKELETON_KNIGHT = 0x0D,
    AOS_ENEMY_GHOST = 0x0E,
    AOS_ENEMY_SIREN = 0x0F,
    AOS_ENEMY_TINY_DEVIL = 0x10,
    AOS_ENEMY_DURGA = 0x11,
    AOS_ENEMY_ROCK_ARMOR = 0x12,
    AOS_ENEMY_GIANT_GHOST = 0x13,
    AOS_ENEMY_WINGED_SKELETON = 0x14,
    AOS_ENEMY_FLEAMAN = 0x18,
    AOS_ENEMY_CATOBLEPAS = 0x1C,
    AOS_ENEMY_GHOST_DANCER = 0x1D,
    AOS_ENEMY_KILLER_DOLL = 0x1F,
    AOS_ENEMY_ZOMBIE_OFFICER = 0x20,
    AOS_ENEMY_NEMESIS = 0x26,
    AOS_ENEMY_VALKYRIE = 0x29,
    AOS_ENEMY_CURLY = 0x2B,
    AOS_ENEMY_ALTAIR = 0x2C,
    AOS_ENEMY_RED_CROW = 0x2D,
    AOS_ENEMY_DEAD_WARRIOR = 0x2F,
    AOS_ENEMY_IMP = 0x31,
    AOS_ENEMY_GORGON = 0x33,
    AOS_ENEMY_GREMLIN = 0x37,
    AOS_ENEMY_HARPY = 0x38,
    AOS_ENEMY_MEDUSA_HEAD = 0x39,
    AOS_ENEMY_LIGHTNING_DOLL = 0x3B,
    AOS_ENEMY_NEEDLES = 0x3F,
    AOS_ENEMY_CAGNAZZO = 0x49,
    AOS_ENEMY_RIPPER = 0x4A,
    AOS_ENEMY_DEAD_CRUSADER = 0x52,
    AOS_ENEMY_GARGOYLE = 0x57,
    AOS_ENEMY_BEAM_SKELETON = 0x59,
    AOS_ENEMY_SKULL_MILLIONE = 0x5B,
    AOS_ENEMY_MIMIC = 0x60,
    AOS_ENEMY_ERINYS = 0x62,
    AOS_ENEMY_LUBICANT = 0x63,
    AOS_ENEMY_MINOTAUR = 0x15,
    AOS_ENEMY_RED_MINOTAUR = 0x58,
    AOS_ENEMY_GIANT_WORM = 0x3E,
    AOS_ENEMY_POISON_WORM = 0x47,
    AOS_ENEMY_BIPHRON = 0x4E,
    AOS_ENEMY_BOMBER_ARMOR = 0x3A,
    AOS_ENEMY_SKY_FISH = 0x51,
    AOS_ENEMY_ECTOPLASM = 0x1B,
    AOS_ENEMY_EVIL_BUTCHER = 0x19,
    AOS_ENEMY_KILLER_MANTLE = 0x55,
    AOS_ENEMY_DRYAD = 0x46,
    AOS_ENEMY_TSUCHINOKO = 0x23,
    AOS_ENEMY_KICKER_SKELETON = 0x53,
    AOS_ENEMY_WAITER_SKELETON = 0x1E,
    AOS_ENEMY_COCKATRICE = 0x2E,
    AOS_ENEMY_CHRONOMAGE = 0x28,
    AOS_ENEMY_MANDRAGORA = 0x4F,
    AOS_ENEMY_DISC_ARMOR = 0x34,
    AOS_ENEMY_ALURA_UNE = 0x4D,
    AOS_ENEMY_FISH_HEAD = 0x41,
    AOS_ENEMY_BASILISK = 0x64,
    AOS_ENEMY_UKOBACK = 0x4C,
    AOS_ENEMY_STUDENT_WITCH = 0x16,
    AOS_ENEMY_WITCH = 0x2A,
    AOS_ENEMY_MUDMAN = 0x56,
    AOS_ENEMY_WEREWOLF = 0x32,
    AOS_ENEMY_WEREJAGUAR = 0x4B,
    AOS_ENEMY_WERETIGER = 0x54,
    AOS_ENEMY_SLIME = 0x44,
    AOS_ENEMY_KYOMA_DEMON = 0x27,
    AOS_ENEMY_BAEL = 0x5E,
    AOS_ENEMY_NIGHTMARE = 0x42,
    AOS_ENEMY_UNE = 0x3D,
    AOS_ENEMY_ARACHNE = 0x17,
    AOS_ENEMY_PERSEPHONE = 0x24,
    AOS_ENEMY_LILITH = 0x25,
    AOS_ENEMY_TRITON = 0x43,
    AOS_ENEMY_SUCCUBUS = 0x5F,
    AOS_ENEMY_DEVIL = 0x30,
    AOS_ENEMY_ARC_DEMON = 0x48,
    AOS_ENEMY_DEMON_LORD = 0x66,
    AOS_ENEMY_FLAME_DEMON = 0x68,
    AOS_ENEMY_QUEZLCOATL = 0x1A,
    AOS_ENEMY_ALASTOR = 0x5A,
    AOS_ENEMY_MAN_EATER = 0x40,
    AOS_ENEMY_GLADIATOR = 0x5D,
};

/* Boss ids live outside the ordinary monster-aspect inventory. */
#define AOS_BOSS_HEADHUNTER 0x6A

/* What an AosEnemy slot holds: the enemy itself or a child entity of its
 * kind (sharing its sprites and record). */
enum { AOS_ROLE_ENEMY = 0, AOS_ROLE_GRENADE = 1, AOS_ROLE_AXE = 2, AOS_ROLE_ARROW = 3,
       AOS_ROLE_DEBRIS = 4, AOS_ROLE_FEATHER = 5,
       AOS_ROLE_GIB = 6, AOS_ROLE_DRIP = 7, AOS_ROLE_PUDDLE = 8, AOS_ROLE_NEEDLE_PIECE = 9,
       AOS_ROLE_MIMIC_TRIGGER = 10, AOS_ROLE_FIN = 11, AOS_ROLE_BEAM = 12,
       AOS_ROLE_BEAM_FLASH = 13, AOS_ROLE_DOLL_PIECE = 14,
       AOS_ROLE_NEMESIS_HELPER = 15, AOS_ROLE_NEMESIS_EFFECT = 16,
       AOS_ROLE_HARPY_SHOT = 17, AOS_ROLE_SIREN_SHOT = 18,
       AOS_ROLE_GORGON_BREATH = 19, AOS_ROLE_GORGON_DEBRIS = 20,
       AOS_ROLE_TINY_DEVIL_ORB = 21, AOS_ROLE_IMP_HELPER = 22,
       AOS_ROLE_IMP_DEATH = 23, AOS_ROLE_MERMAN_SHOT = 24,
       AOS_ROLE_RIPPER_BLADE = 25, AOS_ROLE_PEEPING_SEGMENT = 26,
       AOS_ROLE_WHITE_DRAGON_PART = 27, AOS_ROLE_BONE_PILLAR_SEGMENT = 28,
       AOS_ROLE_BUER_FIRE = 29, AOS_ROLE_SKELETON_BONE = 30,
       AOS_ROLE_BUER_PROJECTILE = 31, AOS_ROLE_SKELETON_HEAD = 32,
       AOS_ROLE_SKELETON_HELD = 33, AOS_ROLE_WINGED_BONE = 34,
       AOS_ROLE_WINGED_PIECE = 35, AOS_ROLE_ROCK = 36,
       AOS_ROLE_ROCK_PIECE = 37, AOS_ROLE_FRY = 38, AOS_ROLE_BUBBLE = 39,
       AOS_ROLE_BUBBLE_POP = 40, AOS_ROLE_DRAGON_FIREBALL = 41, AOS_ROLE_DRAGON_FLAME = 42,
       AOS_ROLE_DRAGON_SPARK = 43, AOS_ROLE_DRAGON_PIECE = 44,
       AOS_ROLE_GIANT_GHOST_PART = 45, AOS_ROLE_GIANT_GHOST_TRAIL = 46,
       AOS_ROLE_PILLAR_PIECE = 47, AOS_ROLE_PILLAR_BREATH = 48, AOS_ROLE_PILLAR_FIREBALL = 49,
       AOS_ROLE_PILLAR_SMOKE = 50, AOS_ROLE_HEADHUNTER_HEAD = 51,
       AOS_ROLE_WORM_SEGMENT = 52, AOS_ROLE_BIPHRON_SHOT = 53,
       AOS_ROLE_BOMBER_BOMB = 54, AOS_ROLE_BOMBER_SPARK = 55,
       AOS_ROLE_BUTCHER_CLEAVER = 56, AOS_ROLE_BUTCHER_GORE = 57,
       AOS_ROLE_BUTCHER_PARTICLE = 58,
       AOS_ROLE_MANTLE_ORBIT = 59, AOS_ROLE_MANTLE_GHOST = 60,
       AOS_ROLE_DRYAD_SEED = 61, AOS_ROLE_TSUCHINOKO_SPIT = 62,
       AOS_ROLE_TSUCHINOKO_ROCK = 63,
       AOS_ROLE_WAITER_COMPANION = 64, AOS_ROLE_WAITER_TRAY = 65,
       AOS_ROLE_COCKATRICE_SHADOW = 66, AOS_ROLE_COCKATRICE_ROCK = 67,
       AOS_ROLE_CHRONO_WISP = 68, AOS_ROLE_MANDRAGORA_BODY = 69,
       AOS_ROLE_MANDRAGORA_TAIL = 70, AOS_ROLE_MANDRAGORA_SEED = 71,
       AOS_ROLE_DISC_BLADE = 72, AOS_ROLE_DISC_SHARD = 73,
       AOS_ROLE_DISC_GLOW = 74, AOS_ROLE_ALURA_SCYTHE = 75,
       AOS_ROLE_ALURA_BLADE = 76, AOS_ROLE_UKOBACK_CREATURE = 77,
       AOS_ROLE_UKOBACK_LID = 78, AOS_ROLE_STUDENT_WITCH_BOLT = 79,
       AOS_ROLE_WITCH_COMPANION = 80, AOS_ROLE_WITCH_ORBIT = 81,
       AOS_ROLE_WITCH_BOLT = 82, AOS_ROLE_WITCH_SPARK = 83,
       AOS_ROLE_WITCH_TRAIL = 84, AOS_ROLE_MUDMAN_BUBBLE = 85,
       AOS_ROLE_MUDMAN_ARM = 86, AOS_ROLE_WEREWOLF_SHADOW = 87,
       AOS_ROLE_ALTAIR_BLADE = 88, AOS_ROLE_BAEL_COMPANION = 89,
       AOS_ROLE_UNE_DEBRIS = 90,
       AOS_ROLE_ARACHNE_LEG = 91, AOS_ROLE_ARACHNE_WEB = 92,
       AOS_ROLE_PERSEPHONE_STRIKE = 93, AOS_ROLE_LILITH_BOLT = 94,
       AOS_ROLE_TRITON_HEAD = 95, AOS_ROLE_TRITON_BUBBLE = 96,
       AOS_ROLE_TRITON_JET = 97, AOS_ROLE_TRITON_SPLASH = 98,
       AOS_ROLE_DEVIL_IMP = 99, AOS_ROLE_DEVIL_FIRE = 100,
       AOS_ROLE_DEVIL_ORB = 101, AOS_ROLE_QUEZLCOATL_SEGMENT = 102,
       AOS_ROLE_QUEZLCOATL_GORE = 103, AOS_ROLE_ALASTOR_BODY = 104,
       AOS_ROLE_ALASTOR_SHOT = 105, AOS_ROLE_MAN_LEVEL = 106,
       AOS_ROLE_MAN_VOLLEY = 107, AOS_ROLE_MAN_SHOT = 108,
       AOS_ROLE_MAN_DEATHP = 109, AOS_ROLE_GLADIATOR_PART0 = 110,
       AOS_ROLE_GLADIATOR_PART1 = 111, AOS_ROLE_GLADIATOR_PART2 = 112,
       AOS_ROLE_GLADIATOR_SHOT = 113, AOS_ROLE_GLADIATOR_TRAIL = 114,
       AOS_ROLE_GLADIATOR_GORE = 115 };

/* The linked Nemesis sprite (sub_080C7784): state, step, + 0x0D, direct
 * frame (+ 0x65), facing and visibility (+ 0x58), room position. Its slot
 * follows the body's, so the body runs it at the end of its own update and
 * the child entity only shows it. */
typedef struct {
    uint8_t state, step, timer, frame;
    bool mirrored, hidden;
    int32_t x, y;
} AosNemesisHelper;

typedef struct AosEnemy {
    uint8_t id;             /* + 0x36 */
    int32_t x, y;           /* + 0x40, + 0x44 (room pixels, 16.16) */
    int32_t vx, vy;         /* + 0x48, + 0x4C */
    int32_t ax, ay;         /* + 0x50, + 0x54 */
    uint8_t state;          /* + 0x0A */
    uint8_t step;           /* + 0x0B */
    uint8_t substep;        /* + 0x0C */
    uint8_t timer;          /* + 0x0D */
    uint16_t phase;         /* generic visual phase; NOT Buer's affine +0x14 */
    bool mirrored;          /* + 0x58 bit 0x40 */
    bool removed;           /* + 0x59 bit 8: deleted by the framework */
    bool vflip;             /* + 0x58 bit 0x80 */
    bool hidden;            /* + 0x58 bit 0x20 */
    int16_t hp;             /* + 0x34 */
    int16_t param0, param1; /* + 0x30, + 0x32: record parameters */
    int32_t walk_timer;     /* + 0x1C (a gib: its parent's y at spawn) */
    uint8_t spawned_count;  /* + 0x18 of a spawner */
    uint16_t producer_timer; /* + 0x1A of a Ghost producer */
    bool attack_off;        /* + 0x72 bit 1 */
    bool cycle_done;        /* + 0x59 bit 0: the animation looped or ended */
    uint8_t hit_flash;      /* + 0x2D */
    uint8_t role;           /* AOS_ROLE_* */
    bool defeated;          /* + 0x3E bit 1, set at death by sub_080683BC */
    int16_t static_frame;   /* sprite frame of a child (+ 0x65), or -1 */
    bool own_boxes;         /* boxes set by code, not by the animation frame */
    uint8_t ground;         /* + 0x3F: floor cell byte under the walker */
    uint32_t angle;         /* +0x14: affine angle of a child or Buer */
    int32_t spin;           /* +0x18: signed angular speed, including Buer */
    int32_t mantle_speed;   /* + 0x14 of the Killer Mantle: the speed of its
                             * rocking walk and of its dash attack */
    uint16_t kicker_flags;  /* + 0x14 of the Kicker Skeleton: bit 0 the
                             * strike's contact, bit 1 cleared on landing */
    int32_t kicker_speed;   /* + 0x18 of the Kicker Skeleton: the speed of
                             * its hop, slide and skid */
    int8_t volley_count;    /* + 0x14 of a shooter: volleys fired, 0..4 */
    uint8_t volley, shot;   /* current volley list and entry (+ 0x1C) */
    int8_t away;            /* + 0x20 at death: +1 / -1, away from the attacker */
    bool dropped, puddled;  /* + 0x14, + 0x15 of a walking zombie */
    bool guarding;          /* + 0x21 of the dead crusader: blows deal 0 */
    uint8_t variant;        /* + 0x14 of a Skull Millione, Cagnazzo or Lubicant */
    int32_t speed;          /* + 0x20 of their chase */
    struct AosEnemy *parent;  /* a linked actor's owner, set by the caller */
    /* A compact target location for controllers that have an explicit native
     * random destination rather than following Soma directly. */
    int32_t target_x, target_y;
    bool helper_spawned;    /* the Nemesis body has emitted its linked helper */
    bool boss_parts_spawned; /* a multipart boss emitted its constructor children */
    uint8_t hh_phase;       /* Headhunter body +0x18: active head, 1..3 */
    bool buer_fire_mirrored; /* linked second sprite's +0x58 bit 0x40, separate from parent */
    /* Death step-one linked-fire displacement: +1 X/Y on every third
     * tick, then -1 on the following tick; carried into death step two.
     * This tracks the source child position without retaining room pointers. */
    uint8_t buer_death_linked_wobble;
    /* Buer's fire sprite (sub_080CC3C8): its position, kept by the body,
     * on the body while it follows it (its + 0x0A = 0), gone in death
     * stage two. */
    /* White Dragon (EnemyWhiteDragonCreate): + 0x10 / + 0x12 (countdowns,
     * then the attack target), its root (room pixels), + 0x1C (swing
     * amplitude), + 0x1E (wobble phase), + 0x48 (swing angle), + 0x4A
     * (heading), + 0x4C (swing rate), + 0x4E (neck length), + 0x50 (breath
     * sweep up), + 0x52 (breath angle) and the 100 past swing angles. */
    int16_t wd_cool_a, wd_cool_b, wd_home_x, wd_home_y, wd_swing, wd_radius;
    uint16_t wd_phase, wd_angle, wd_heading, wd_rate, wd_aim;
    bool wd_sweep_up;
    uint16_t wd_history[100];
    /* Giant Ghost: + 0x21 (it touched Soma: it flees); a part's affine
     * scale (+ 0x48 / + 0x4C, 0x10000 = unit, larger draws smaller) and an
     * eye's inert sweep (+ 0x22 / + 0x23). */
    /* Bone Pillar's controller: + 0x15 (inside its window), its skulls'
     * slots (+ 0x18 + i, each registered by its skull), the topmost live
     * one (the homing target) and whether any was seen. */
    bool bp_active, bp_seen;
    int8_t bp_top;
    struct AosEnemy *bp_skull[8];
    /* Peeping Eye: + 0x18 (bit 0 a direct frame + 0x19, bit 1 open), its
     * frame sequence (+ 0x14), index (+ 0x1A), tick (+ 0x1B) and hurt flag
     * (+ 0x15); a segment's radius (+ 0x14) toward + 0x18, the pupil's
     * offset (+ 0x48 / + 0x4C) and the eye's segments (each registered by
     * its segment). */
    uint8_t pe_flags, pe_frame, pe_seq, pe_tick;
    int8_t pe_index;
    bool pe_hurt, pe_reverse;      /* + 0x15; the creation's r & 1 (segment angles) */
    uint16_t pe_rotation;           /* the tip segment's affine angle */
    int32_t pe_radius, pe_radius_target, pe_off_x, pe_off_y;
    struct AosEnemy *pe_seg[6];
    bool gg_touched, gg_scan_back;
    uint8_t gg_scan;
    int32_t affine_sx, affine_sy;
    int32_t buer_fire_x, buer_fire_y;
    bool buer_fire_following, buer_fire_gone;
    bool technical_effects_spawned; /* the three Nemesis visual effect slots */
    int16_t home, home_right, home_left;  /* + 0x22, + 0x14, + 0x16 of a beam skeleton */
    uint16_t rest;          /* + 0x1C of a beam skeleton: frames before it may fire */
    uint8_t patience;       /* + 0x19 */
    bool firing;            /* + 0x1A bit 0: its beam lives */
    uint16_t scale;         /* + 0x20: the vertical scale of its affine slot */
    bool scaled;            /* + 0x59 bit 0x10: drawn with the owner's scale */
    uint16_t attack_wait, flight;   /* + 0x14, + 0x18 of either Doll */
    int16_t leg;            /* + 0x16: drift / rest timer of either Doll */
    AosEffect glow;         /* a doll piece's flame (sub_080C9058) */
    bool translucent;       /* + 0x5A bits 1-2 = 2: semi-transparent OBJ */
    /* The temporary 24-record OAM history allocated by sub_080458E4.  The
     * native effect callback shifts records 0..22 to 1..23, writes the
     * owner's current integer position/frame at record 0, then selects the
     * child sequence index (9, 12 or 6). */
    int16_t nemesis_history_x[AOS_NEMESIS_EFFECT_HISTORY];
    int16_t nemesis_history_y[AOS_NEMESIS_EFFECT_HISTORY];
    uint8_t nemesis_history_frame[AOS_NEMESIS_EFFECT_HISTORY];
    uint8_t nemesis_history_mirror[AOS_NEMESIS_EFFECT_HISTORY];
    uint8_t fade, fade_timer;   /* + 0x18, + 0x19 of a Ghost Dancer */
    AosBox own_hurt, own_attack;
    AosEnemyStats stats;
    uint16_t contact_elements;  /* the + 2 of its contact callback's hit (sub_08021654) */
    /* The Skeleton: + 0x1A (bit 0 head on its neck, 1 head in hand, 2 bone
     * in hand, 3 the alternate throw, 5 the head's laugh), + 0x1E (the
     * affine angle its projectile turns by), and a projectile's affine use
     * (+ 0x59 bit 4) and head (+ 0x18). */
    /* Killer Fish: + 0x14 (frames at one x; a fry's retarget countdown),
     * + 0x18 (that x), + 0x1A (bubble countdown), + 0x1C / + 0x1E (the
     * attack point); a fry's + 0x18 (its fish gone), + 0x19 (its fish dead)
     * and + 0x1C (it touched Soma). */
    int32_t fish_count;
    int16_t fish_last_x, fish_bubble, fish_tx, fish_ty;
    bool fry_parent_gone, fry_parent_dead, fry_touched;
    uint8_t skel_flags;
    uint16_t skel_spin;
    bool spinning;
    /* Nemesis: + 0x13 (passes left), + 0x16 / + 0x17 (attack row and
     * index of 0x08528A34), + 0x18 (bit 1 right of its target, bit 3 right
     * of Soma, bit 5 no collisions), + 0x1E (angular speed), + 0x24 (blows
     * before a reaction); + 0x1A is `phase`, + 0x1C `angle`. */
    uint8_t nem_passes, nem_row, nem_index, nem_hits;
    uint16_t nem_flags;
    int16_t nem_spin, nem_wait;
    AosNemesisHelper nem_helper;
    /* Minotaur/RedMinotaur (sub_080B0BE8): + 0x18 flags (bit 0 set at
     * create and after a charge, bit 1 the wind-up attack still owed),
     * + 0x15 the palette bank (0 brown, 1 red; the aggro gate reads it). */
    uint32_t mino_flags;
    uint8_t mino_bank;
    /* Giant/Poison Worm (sub_080AD7F0 create, sub_080AE0E0 parent,
     * sub_080ADC2C segment): + 0x1C the lunge flag children read,
     * + 0x1D the wall-wait countdown, + 0x1E the segment index (1..4,
     * 0 on the parent), + 0x1F the hop direction, + 0x22 the sine phase.
     * `variant` is + 0x21, `angle` is + 0x54, `ax` is + 0x50,
     * `parent` is + 0x14 and `follow` is + 0x18 (the segment ahead). */
    uint8_t worm_wait, worm_dir, worm_index, worm_lunge;
    uint16_t worm_phase;
    struct AosEnemy *follow;
    /* Sky Fish (sub_080A7C80 create, sub_080A7D58 dash, sub_080A7F08
     * hover/death): + 0x18 s8 the collision gate (0 collides), + 0x19 the
     * entry side (0 left, 1 right), + 0x1A the entry countdown (0x40 +
     * (r & 0x3F)). */
    int8_t sf_gate;
    uint8_t sf_variant;
    int16_t sf_timer;
    /* Ectoplasm (sub_08099690): + 0x20 the target-pick direction flag.
     * `target_x`/`target_y` are + 0x14/+ 0x18 (the drift destination,
     * stored here in world pixels — the native camera offset cancels),
     * `walk_timer` is the + 0x1C sine phase. */
    uint8_t ec_dir;
    /* Dryad death (sub_080AE650): + 0x59 bit 0x10 — the affine turn is
     * owed. Set on the arming tick when the RNG gate passes, cleared
     * when the timer deletes it. */
    uint8_t dryad_flags;
    /* Tsuchinoko (sub_080A71CC create): + 0x14, its HP at create — the
     * wounded-hop thresholds of sub_080A7294 step 2 read it. */
    int16_t hp_home;
    /* Waiter Skeleton: + 0x20 flags on the body (bit 0 the powered tray,
     * bit 1 the held companion's visibility, bit 2 the tray's throw
     * window, bit 3 its landed contact); + 0x14 animation index and
     * + 0x52 spawn offset of the tray child. */
    uint32_t ws_flags;
    int16_t ws_anim;
    int16_t ws_offset;
    /* Cockatrice (sub_080BBFF0 create, sub_080BB924 shadow,
     * sub_080BBAC4 rock): + 0x18 flags on the body (bit 0 the corpse
     * mark), + 0x1A the shadow's drift phase / the rock's age and
     * + 0x20 its orbit parameter. */
    uint32_t ck_flags;
    int16_t ck_phase;
    uint16_t ck_orbit;
    /* Chronomage (sub_080B712C create, sub_080B6C3C wisp): + 0x59 bits
     * 1 (linked flag), 3 (margin-out mark) and 4 (wisp palette group). */
    uint8_t cm_flags;
    /* Mandragora (EnemyMandragoraCreate 0x080CEF34; head update
     * 0x080CF99C, body 0x080CF0E4, tail 0x080CF3BC, seed 0x080CF910):
     * + 0x13 the death-seed volley index, + 0x20 the entity sensor byte
     * the collision pass writes, + 0xF the tail's sub-state, + 0x59
     * bits 1-4 (linked, tail-active, fade marks, translucent) and + 0x5A
     * the draw-flip byte. */
    uint8_t mg_seeds, mg_sensors, mg_sub, mg_flags, mg_draw;
    /* The body's and the tail's slots, each registered by the child
     * through its `parent` link (the tail reaches the head through
     * `parent->parent`). */
    struct AosEnemy *mg_body, *mg_tail;
    /* DiscArmor (EnemyDiscArmorCreate 0x080B2228, update 0x080B22D4;
     * disc sub_080B1678, shard sub_080B1428, glow sub_080B14B4): the
     * armor's +0x20/+0x22 orbit pixel offsets, +0x6E walk/shot counter,
     * +0x6F aggro mark and +0x18/+0x1C orbit draw offset; the disc's
     * +0x20 radius, +0x24 return speed, +0x28 momentum, +0x2C/+0x30
     * launch, +0x1C heading, +0x42/+0x46 shard offsets, +0x54 spin and
     * +0x18 glow child; +0x59 flag bits and the +0x5A draw byte. */
    int16_t da_orbit_x, da_orbit_y, da_tx, da_ty;
    uint16_t da_frame;
    uint8_t da_mark, da_lflags, da_draw;
    uint16_t da_flags;
    int32_t da_ox, da_oy, da_radius, da_speed, da_momentum, da_launch,
            da_lvy, da_heading, da_spin;
    struct AosEnemy *da_glow;
    /* Alura Une (EnemyAluraUneCreate 0x0807C0A0, update 0x0807CF9C;
     * scythe sub_0807C9A4/sub_0807CAD4, orbit blade sub_0807CC9C/
     * sub_0807CDA8): + 0x20 the aggro box already seen, + 0x21 the hurt
     * reaction owed, + 0x6D the once-only 0x6A hurt song and the
     * previous animation frame for the state-1 blade spawns; a blade's
     * + 0x18 facing-flip counter and + 0x20 (u16) flight life; + 0x72
     * bit 2 the launch mark, + 0x73 bit 1 the landed mark and + 0x59
     * bits 1/3 the blade's launch/expiry flags. */
    uint8_t au_seen, au_react, au_hurted, au_prev_frame, au_blade_flags;
    /* Ukoback (sub_080C5CE8 hop, sub_080C5F10 shatter, creature
     * sub_080C610C, lid sub_080C61F4): + 0x16 the hop countdown,
     * + 0x22 the peek count the drifting creature decrements; the
     * + 0x42/+ 0x46/+ 0x4A/+ 0x4E probe offsets are approximated by
     * the room-edge bounce below. A creature's + 0x14/+ 0x18 drift
     * offsets live in target_x/target_y and its + 0x1C lifetime in
     * walk_timer. */
    int16_t uk_timer;
    uint8_t uk_peek;
    int16_t uk_death_y;      /* the shatter's sink reference (+0x46 approx) */
    uint8_t sw_probe;        /* Student Witch: last sub_0806CAF8 result
                              * byte (+ 0x6C), its bit 2 the ceiling mark */
    /* Witch (EnemyWitchCreate 0x080B9D90 and its children): + 0x14 the
     * flags byte — bit 0 the invulnerable attack window read by
     * sub_080B9F2C, bit 1 the companion's wait, bit 2 the tall aggro box
     * (`variant`); + 0x18 the state-1 hover tick and the bolt's lock
     * timer (`spin`); + 0x10 the orbit spark's shrinking radius and the
     * bolt's speed ramp (`speed`); + 0x14 the orbit/bolt spin angle
     * (`angle`, low 16 bits signed) and + 0x16 its spin rate (`w_spin`);
     * the orbit spark's + 0x50/+ 0x54 orbit centre (`ax`/`ay`, world
     * 16.16, approximating the native screen-space base) and + 0x48 its
     * radius velocity (`vx`); the bolt's + 0x1C/+ 0x20 homing target
     * (`target_x`/`target_y`); + 0xF the companion's and the trail
     * sparkle's active mark (`w_active`). */
    uint16_t w_spin;
    bool w_active;
    /* Mudman (EnemyMudmanCreate 0x08072670): + 0x5C the spawned-child
     * counter the bubble/arm copies read (md_spawns) and the + 0x59
     * gameplay bits 2-3 the death sequence sets (md_flags, raw bits
     * 4/8); the + 0x32 aggro direction memory is param1 and the arm's
     * + 0x18/+ 0x1C follow anchor lives in target_x/target_y. */
    uint8_t md_spawns, md_flags;
    /* Werejaguar/Weretiger (the shared sub_08078880 family): + 0x14 the
     * pounce-active mark the hurt clears, + 0x15 the attack element
     * (1 slash, 2/3 the dash), + 0x16 the Werejaguar chain latch and
     * + 0x17 the repeat-attack mark read at the pounce landing. */
    uint8_t were_attack, were_element, were_chain, were_flag;
    /* Werewolf (id 0x32): + 0x10 the attack land/mark word the state-1
     * machine reads and clears, and the + 0x59 bit-1 mark the attack
     * tail clears (value 2). */
    int32_t ww_mark;
    uint8_t ww_bit;
    /* Slime (EnemySlimeCreate 0x080C11C0): + 0x11 the flags byte —
     * bit 0 the hurt squash mark read by sub_080C0A1C, bit 1 its
     * follow-through, bit 2 the airborne mark of the state-1 launch
     * and the state-2 spin; + 0x12 the ground rest countdown, + 0x13
     * the hop direction index of 0x08528922, + 0x14 the signed hop
     * power, + 0x18 the ground charge (the affine height), + 0x19
     * and + 0x1B the attack marks, + 0x1C the affine width and the
     * state-2 spin countdown, + 0x20 the hop angle (also the sprite
     * rotation of sub_0803E058), + 0x22 the angle-approach step and
     * + 0x24..+ 0x2A the predicted landing box of sub_080C06A8. */
    uint8_t sl_flags, sl_rest, sl_dir, sl_mark19, sl_mark1b, sl_coll;
    int32_t sl_power, sl_charge, sl_width;
    uint16_t sl_angle, sl_astep;
    int16_t sl_box[4];
    /* Kyoma Demon (sub_080A02AC / sub_080A04B0 / sub_080A0790): the
     * + 0x14/+ 0x16 spawn-window centre and + 0x1C/+ 0x20 the spawn
     * screen position latched on the first update (the native stores
     * them at create against the room scroll; the camera is only
     * known here), + 0x42/+ 0x46 the spawn slot pixels, + 0x18 the
     * alert flag, + 0x5C the draw byte (0x1D idle, 0x11 attack),
     * + 0x2C the last attack element low nibble, + 0x59 bit 1 the
     * attack-anim lock and + 0x5A bits 3-6 the hurt draw group
     * (bits 1-2 stay `translucent`). + 0x6D/+ 0x6E are the shared
     * animation id/frame fields. */
    int16_t kd_sx, kd_sy, kd_ox, kd_oy;
    int32_t kd_tx, kd_ty;
    uint8_t kd_alert, kd_5c, kd_atk, kd_f59, kd_f5a;
    bool kd_origin;
    uint16_t au_life;
    /* Altair (EnemyAltairCreate 0x080C1490 / sub_080C1694 /
     * sub_080C1898 / sub_080C1940 / sub_080C1CDC): + 0x42/+ 0x46/+ 0x4A
     * the pixel window fields the state-1 tail despawn check reads
     * (+ 0x4A latched 0 — its native writer was not traced), + 0x1C and
     * + 0x1D the state-1 marks, + 0x1E the s16 spawned-copy count,
     * + 0x5C the draw byte (3 at create, 0x30 in the dash), + 0x6F an
     * unread animation byte kept 0, and the blade child's + 0x14/+ 0x16
     * angular speed and + 0x24 phase with `al_linked` marking the
     * persistent partner blade of func_080C1A64 versus the free bolts
     * of func_080C1D38. + 0x20/+ 0x10 are the shared `parent` field and
     * + 0x6D/+ 0x6E the shared animation id/frame. */
    int16_t al_wx, al_wy, al_ww;
    uint8_t al_mark1c, al_mark1d, al_draw5c, al_f6f, al_linked;
    int16_t al_copies;
    uint16_t al_angle, al_avel;
    /* Gladiator (EnemyGladiatorCreate-equivalent func_080CC624 /
     * func_080CD2D0): + 0x18 the walk-speed accumulator the slope bits
     * of sub_080CD144 grow and brake (`spin`), + 0x14 the affine
     * rotation angle (`angle`), + 0xF the death phase flag, + 0x14/
     * + 0x15 the death pixel-drift bytes (gl_drift_*), + 0x11 the
     * instance index whose low bits seed the + 0x58 tint (ba_tint)
     * and the companion's + 0x11 swap. */
    uint8_t ba_flag, ba_tint, ba_companion;
    int8_t ba_drift_x, ba_drift_y;
    /* Nightmare (EnemyNightmareCreate 0x080B839C, update 0x080B84B4;
     * drift sub_080B80BC, death sub_080B8240): + 0x14/+ 0x15 the two
     * trail slots sub_080458E4 registers and sub_080B8240 releases
     * (marks only — the native OAM-history effect slots are not
     * ported), + 0x18 the ramped drift speed, + 0x6C the
     * sub_0806CAF8 probe byte (bit 2 the turn mark). + 0x1C is
     * walk_timer, + 0xD timer, + 0xB step. */
    uint8_t nm_wisp_a, nm_wisp_b, nm_probe;
    int32_t nm_speed;
    /* Une (EnemyUneCreate 0x0807427C / EnemyUneUpdate 0x08074130;
     * state0 sub_080743B4, state1 sub_080748C0, hurt sub_0807473C,
     * debris sub_080747B4/sub_08074838): + 0x18 the root spawner's
     * live-descendant count (spawned_count) and each child's root
     * link (parent), + 0x1C the create-spawned child's short-fuse
     * mark, + 0x20 the split-cycle wait and + 0x5C-1 the debris draw
     * byte (cosmetic, kept). + 0x54 is the projectile-motion gravity
     * carried in `ay`; + 0x2C the palette bank is folded into the
     * shared draw. */
    uint8_t une_child, une_draw;
    int32_t une_wait;
    /* Arachne (EnemyArachneCreate 0x080799FC, EnemyArachneUpdate
     * 0x080798F0): `spin` is + 0x18 the charge speed accumulator,
     * `walk_timer` is + 0x1C its acceleration, `angle` is + 0x14 the
     * web launch angle (or the AIM phase while winding up), + 0x20
     * the leg index, + 0x21 the attack mode, + 0x22 the cooldown,
     * + 0x2C the palette byte and + 0x48 of a leg its vertical
     * drop. `ar_*` carry the leg index, attack mode, cooldown,
     * palette byte and the leg's vertical drop. */
    uint8_t ar_leg, ar_mode, ar_cool, ar_pal;
    int32_t ar_drop;
    /* Persephone (EnemyPersephoneCreate 0x0807B5D0, EnemyPersephoneUpdate
     * 0x0807B47C): `pr_hit` is + 0x20 (a strike already connected);
     * + 0x6C is the shared anim.flags, + 0x6E the shared anim.frame. */
    uint8_t pr_hit;
    /* Lilith (EnemyLilithCreate 0x08094818, EnemyLilithUpdate 0x08094750):
     * `li_approach` + 0x18, `li_flag` + 0x19, `li_breath` + 0x1A (the
     * contact callback's mark), `li_inactive` + 0x1B (intangible until
     * the wake), `li_pal` + 0x2C, `li_tint` + 0x5A; `li_bolt` is the
     * breath child's + 0x1C speed/phase accumulator. */
    uint8_t li_approach, li_flag, li_breath, li_inactive, li_pal, li_tint;
    int32_t li_bolt;
    /* Succubus (EnemySuccubusCreate 0x08093D24, EnemySuccubusUpdate
     * 0x08093C34): `su_gate` + 0x18 (set by the hurt pass), `su_try`
     * + 0x19, `su_touch` + 0x1A, `su_inactive` + 0x1B, `su_dash` + 0x1C,
     * `su_chain` + 0x1D, `su_age` + 0x1E, `su_volley` + 0x1F, `su_emit`
     * + 0x20, `su_aggro` + 0x21, `su_pal` + 0x2C, `su_tint` + 0x5A. */
    uint8_t su_gate, su_try, su_touch, su_inactive, su_dash, su_chain;
    uint8_t su_age, su_volley, su_emit, su_aggro, su_pal, su_tint;
    /* Triton (EnemyTritonCreate 0x08086AD4): `tr_launch` is the head's
     * + 0x18 (the state-1 attack's whirlpool launch flag), `tr_returned`
     * mirrors the head child's + 0x18, `tr_saved` is the head's + 0x19
     * (the saved state-0 step), the head child's `tr_angle` is its + 0x20
     * whirlpool angle and `tr_angle_target` its + 0x50 turn target, and
     * the jet's `tr_jvy` is its + 0x14 rise speed; the bubble homes with
     * the shared + 0x48/+ 0x4C/+ 0x50/+ 0x54 velocity/accel fields. */
    uint8_t tr_launch, tr_returned, tr_saved;
    uint32_t tr_angle;
    int32_t tr_angle_target;
    int32_t tr_jvy;
    /* Devil family (EnemyDevilCreate 0x0808FA30 / update 0x0808F964):
     * the + 0x1C container's eight 0x2C-byte orbit records folded into
     * the parent — ox/oy position, osc scale (+ 8), oat/oac the orbit
     * angle target/current (+ 0x10/+ 0x14), oan the 0x80000000 anchor
     * flag (+ 0x18), oai the anchor record index (+ 0x1C), osx/osy the
     * saved position (+ 0x20/+ 0x24) and orad the orbit radius
     * (+ 0x28).  dv_tx/dv_ty is the state-0/1 target (+ 0x10/+ 0x14 of
     * the native actor), dv_dist the + 0x18 approach ramp, dv_ax16 /
     * dv_ay16 the + 0x42/+ 0x46 home anchor, dv_form the + 0x20
     * formation index, dv_cyc the + 0xC sub-cycle, dv_pal the + 0x2C
     * palette/kind index, dv_f27 the + 0x27 anchor-side flag and
     * dv_c26 the + 0x26 attack-enable byte.  On an imp child dv_slot
     * is its container slot (+ 0x20), dv_dvx/dv_dvy its dive velocity
     * and dv_ang its launch angle; on a fireball dv_life is + 0xD and
     * dv_kind the inherited + 0x36 variant byte. */
    struct {
        int32_t ox[8], oy[8], osc[8];
        int32_t oat[8], oac[8];
        int32_t oan[8], oai[8];
        int32_t osx[8], osy[8];
        int32_t orad[8];
    } dv;
    int32_t dv_tx, dv_ty, dv_dist;
    int32_t dv_ax16, dv_ay16;
    uint8_t dv_form, dv_cyc, dv_pal, dv_f27, dv_c26;
    int32_t dv_slot, dv_dvx, dv_dvy, dv_ang, dv_life, dv_kind;
    /* Quezlcoatl (EnemyQuezlcoatlCreate 0x08077DF4, head update
     * 0x08077FC0/0x08078178, segment sub_08078318): `qz_trail` is the
     * + 0x20 container's movement-history buffer — 2-byte (dx, dy)
     * pixel-delta entries, index 0 newest, shifted up every recorded
     * frame; `substep` counts the segments already emitted (the native
     * create spawns all fifteen at once, here they go out 8 + 7 over
     * the first two updates).  On a segment `qz_depth` is its + 0x1C
     * trail index (0xE down to 0), `qz_seg` its + 0x5C number (1..15),
     * `qz_tx`/`qz_ty` the + 0x4A/+ 0x4E pixel-anchor deltas and
     * `qz_pal`/`qz_tint` the + 0x2C/+ 0x5A palette bytes. */
    int8_t qz_trail[240];
    int8_t qz_depth;
    uint8_t qz_seg, qz_pal, qz_tint;
    int16_t qz_tx, qz_ty;
    /* Alastor (EnemyAlastorCreate 0x080CD544 / EnemyAlastorUpdate
     * 0x080CD644): `ala_angle` is + 0x1A (the spin angle the sine/cosine
     * motion of states 5-7 reads), `ala_rot` the + 0x1C draw rotation
     * (sub_080CDD9C's oriented box angle), `ala_probe` the + 0x1E probe
     * distance, `ala_hits` the + 0x24 three-hit counter, `ala_row`/`ala_col`
     * the + 0x16/+ 0x17 pattern-table indices into 0x08528C2C, `ala_vol`
     * the + 0x13 volley record into 0x08528BF4.  The partner body
     * (sub_080CEB58) mirrors its + 0xA/+ 0xB/+ 0xC/+ 0xD machine into
     * `ala_pstate`/`ala_pstep`/`ala_psub`/`ala_ptmr` on the head and reads
     * them back through `parent`; `ala_plife` is a shot's + 0xD
     * lifetime and `ala_pslot` its + 0x11 parent link. */
    uint16_t ala_angle;
    int16_t ala_rot, ala_probe;
    uint8_t ala_hits, ala_row, ala_col, ala_vol;
    uint8_t ala_pstate, ala_pstep, ala_psub, ala_ptmr, ala_plife, ala_pslot;
    /* Man Eater (EnemyManEaterCreate 0x080BAC3C / update 0x080BAD68):
     * `me_fan` is the + 0xB fan-volley index (0..2), `me_ang` the + 0x18
     * shot angle (the parent's spin or a bolt's fan angle), `me_bolt`
     * the + 0x17 slot a level child or bolt answers to, `me_life` the
     * + 0x20 volley lifetime and `me_seen` the + 0xF half-death flag.
     * The three level children (sub_080BAAC0), the volley entities
     * (sub_080BA788), their six bolts (sub_080BA37C) and the death
     * particles (sub_080BABD4) are persistent children of roles
     * AOS_ROLE_MAN_*. */
    uint8_t me_fan, me_bolt;
    int16_t me_ang;
    int32_t me_life;
    uint8_t me_seen;
    /* Gladiator (EnemyGladiatorCreate 0x080A07FC / update 0x080A1D64):
     * `gl_part` is the + 0x18 child index (0 shield arm, 1 sword arm,
     * 2 flail ball), `gl_atk` the + 0x6D attack phase of the sword
     * arm, `gl_atk2` its + 0x6F one-shot gate, `gl_ang`/`gl_ang2` the
     * + 0x1C/+ 0x1E spin pair of the flail and the shot's + 0x1E
     * angle, `gl_ox`/`gl_oy` the 16-bit attach offsets (+ 0x42/+ 0x46)
     * and `gl_life` the + 0x48 parent-frame memory. The three limb
     * children (sub_080A0DEC/sub_080A1060/sub_080A13D0), the spinning
     * shot and its trail (sub_080A15CC/sub_080A18C0 through
     * sub_080A19C4), the death chunks (sub_080A1A50/sub_080A1B98) are
     * persistent children of roles AOS_ROLE_GLADIATOR_*. */
    uint8_t gl_part, gl_atk, gl_atk2;
    int16_t gl_ang, gl_ang2, gl_ox, gl_oy;
    int32_t gl_life;
    AosCombat combat;
    AosAnimState anim;
} AosEnemy;

#define AOS_ENEMY_MAX_EFFECTS 32  /* a bomb's sub_08045A98 burst: 18 pieces */

/* What one collision pass did. */
typedef struct {
    bool soma_hit;          /* the enemy's attack hit Soma */
    int soma_damage;
    bool enemy_hit;         /* the weapon hit the enemy */
    int enemy_damage;
    bool has_damage_popup; /* sub_08042B8C, including the zero-damage guard art */
    AosDamagePopup damage_popup;
    bool killed;
    bool special_soul;     /* this boss death reached sub_080D7308 */
    bool spawn;             /* a zombie spawner created a zombie here */
    int32_t spawn_x, spawn_y;   /* room pixels */
    int16_t spawn_param1;       /* + 0x32 of the spawned enemy */
    uint8_t child_count;    /* child entities to add to the room */
    AosEnemy children[AOS_ENEMY_MAX_CHILDREN];
    bool wake_spawner;      /* a Mimic trigger was struck (sub_080993EC) */
    bool force_input;       /* SetPlayerInput(1, forced_held) */
    uint16_t forced_held;
    uint8_t effect_count;   /* shared effects to add */
    AosEffect effects[AOS_ENEMY_MAX_EFFECTS];
    /* sub_0806CC20's sub_08021924: an active, unrewarded enemy within the
     * screen box (-16, 0, 0x110, 0xB0) enters the homing list this frame. */
    bool targetable;
} AosHitReport;

/* RandomNumberGenerator: r = (r >> 8) * 0x3243F6AD + 0x1B0CB175. The seed
 * the game starts from is not traced; aos_random_seed sets it. */
uint32_t aos_random(void);
void aos_random_seed(uint32_t seed);
uint32_t aos_random_get(void);
/* aos_sine and aos_arctan2: aos_math.h (included above). */
/* gEwramData + 0x8E bit 0x40, read by EnemyBatCreate (a bat created while
 * it is set stays in state 4, inert). It is set by sub_0804C404 (reached
 * through the table 0x085254D8) and cleared by sub_0804CC14; its meaning is
 * not traced. Cleared by default. */
void aos_enemy_set_flag_8e_40(bool set);
/* metadata/bursts.tsv list 0, installed by the host once loaded: the
 * sub_08045A98 burst a Bomber Armor bomb sets off (NULL: none). */
void aos_enemy_set_burst_source(const AosEffect *list, size_t count);
/* A red soul skill's attack this frame: its world rectangle (inclusive),
 * the power of sub_08021530(5) and the elements of its record (+ 0xC);
 * `struck` and `target` are set when an enemy took it (the skill's hit
 * callback, as sub_0802DB84 for the arrow and sub_0802D9BC for Ripper). */
typedef struct {
    AosRect rect;
    int power;
    uint16_t elements;
    bool struck;
    AosEnemy *target;
    uint8_t type;           /* + 0x70: AOS_TYPE_SOUL when 0, 6 for a guardian */
} AosSoulAttack;

/* gEwramData + 0x13168: while `active`, the enemies aim at (x, y) (the
 * Killer Doll soul's decoy) instead of Soma; contact, knockback and the
 * routines that read Soma (0x13110) directly keep his position. */
void aos_enemy_set_decoy(bool active, int32_t x, int32_t y);
/* gEwramData + 0x13168 = 0 (the Nemesis soul): sub_0806CD38 / sub_0806CD78
 * and the helpers built on them then return the asking entity's own
 * position, so the enemies lose their target. */
void aos_enemy_set_no_target(bool none);
/* The soul attacks the next enemy updates test after the weapon (type 5
 * collisions); NULL for none. */
void aos_enemy_set_soul_attacks(AosSoulAttack *attacks, int count);
/* sub_0806B658: the centre of the overlap of soul attack `index` (as set
 * by aos_enemy_set_soul_attacks) with the last hurtbox that took it. */
void aos_enemy_soul_attack_contact(int index, int *x, int *y);
/* sub_0806CAF8 for another entity carried in an AosEnemy (x, y, vx, vy,
 * ax, ay, mirrored, ground): one walker step at `speed` toward its facing
 * with the probe table; mode bit 0 (face the player) is not available
 * here. Returns the probe walker's flags (1 / 2 walls, 8 floor). */
int aos_enemy_probe_walk(AosEnemy *e, const AosCollision *layer, const AosProbes *p,
                         int32_t speed, int mode);
/* Creates enemy `id` at a record position; false for unported enemies. */
bool aos_enemy_create(AosEnemy *enemy, uint8_t id, int32_t x, int32_t y, int16_t param0,
                      int16_t param1, const AosSoma *soma, const AosCollision *layer,
                      const AosEnemyKind *kind, const AosEnemyStats *stats);
/* One update; cam_x / cam_y place the activity window. random() returns
 * the next RandomNumberGenerator value. The collision pass runs inside, as
 * in the game: soma_atk / soma_def are the player's current ATK and DEF
 * (new-game values are not traced); the weapon may be NULL. */
AosHitReport aos_enemy_update(AosEnemy *enemy, AosSoma *soma, const AosCollision *layer,
                              const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                              const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
                              int cam_x, int cam_y, uint32_t (*random)(void));

#endif /* AOS_ENEMY_H */
