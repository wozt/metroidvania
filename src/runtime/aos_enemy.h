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
#include "aos_math.h"
#include "aos_collision.h"
#include "aos_damage_popup.h"
#include "aos_combat.h"
#include "aos_effect.h"
#include "aos_soma.h"
#include "aos_weapon.h"

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
       AOS_ROLE_CHRONO_WISP = 68 };

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
