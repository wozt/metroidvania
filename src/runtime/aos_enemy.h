/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies.
 *
 * Shared parts of the enemy framework (cvaos code_08060B98): the activity
 * window of sub_0806CC20, facing the player (sub_0806BC40), the signed
 * velocity by facing (sub_0806E120), the vertical distance to the player
 * (sub_0806BDEC) and the sine of sub_080009E4; the bat (enemy 0x00,
 * EnemyBatCreate / EnemyBatUpdate) and the zombie (enemy 0x01,
 * EnemyZombieCreate / EnemyZombieUpdate, with its spawner records, the
 * walker collision sub_08069A00 and the generic death sub_0806AEAC) and the
 * blue crow (enemy 0x09, EnemyBlueCrowCreate / EnemyBlueCrowUpdate, with the
 * homing velocity of sub_080694B8) and the zombie soldier (enemy 0x0C,
 * EnemyZombieSoldierCreate / EnemyZombieSoldierUpdate) with its grenade
 * (sub_08092BC0 / sub_08092CCC and the projectile collision sub_08069770),
 * and the axe armor (enemy 0x04) with its axe (sub_080B0D5C) and the probe
 * walker sub_0806CAF8 / sub_0806C828, and the skull archer (enemy 0x05) with
 * its arrows (sub_080AF7EC), the Ghost Dancer (enemy 0x1D), the zombie
 * officer (enemy 0x20), Curly (enemy 0x2B) and Durga (enemy 0x11, the same
 * code), the skeleton knight (enemy 0x0D), the dead crusader (enemy 0x52),
 * the Lightning Doll (0x3B) with its burning pieces, the beam skeleton
 * (0x59) with its beam and bones, Skull Millione (0x5B),
 * Cagnazzo (0x49, with its fin) and Lubicant (0x63,
 * with its fin; the same code),
 * the dead warrior (enemy 0x2F),
 * the Fleaman (enemy 0x18), the Mimic (enemy 0x60) with its hidden trigger,
 * the red crow (enemy 0x2D, the blue crow's death), the Valkyrie (enemy 0x29) and Erinys (0x62, the same code), the
 * gargoyle and the Gremlin (0x37, the same code)
 * (enemy 0x57), the Medusa head (enemy 0x39) with its
 * spawner, and Needles (enemy 0x3F) with its pieces
 * (sub_080CB6BC / sub_080CB7C8). Facing flag 0x40 of + 0x58 means
 * mirrored: enemy sprites face left by default. Combat: the frame boxes of
 * sub_0806B1FC, the collision pass of sub_0806E314 / sub_080421AC against
 * the player and his weapon, the bat's hit callback (sub_080AD6E4 /
 * sub_0806E218) and its death (state 3, sub_080AD5B8 / sub_0806BE74),
 * the death flames of sub_0806BE74 / sub_0806AEAC (sub_0806E524), the
 * blood splash of sub_0806E664 (sub_0806B5DC) and the crow's feathers.
 * Hit stun, damage numbers, other particles and drops are NOT ported.
 * No SDL. */
#ifndef AOS_ENEMY_H
#define AOS_ENEMY_H

#include <stdbool.h>
#include <stdint.h>

#include "aos_anim.h"
#include "aos_collision.h"
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
} AosEnemyStats;

#define AOS_ENEMY_MAX_ANIMS 12
#define AOS_ENEMY_MAX_CHILDREN 8
#define AOS_ENEMY_MAX_FRAMES 24

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
    /* The same for single sprite frames shown by children (+ 0x65 < 64). */
    int8_t static_anchors[64][2];
    /* The death flame's animation (Effect/flame), for a doll piece's glow. */
    const AosAnimSet *flame_anims;
    /* Sprite frame number (+ 0x65) of each animation frame. */
    uint8_t sprites[AOS_ENEMY_MAX_ANIMS][AOS_ENEMY_MAX_FRAMES];
    /* 0x08118D08: the (x, y) screen margins of sub_0806D128(e, n). */
    int16_t margins[7][2];
    AosProbes probes;       /* walker probe table, count 0 when unused */
    /* Volley lists (frame, y offset) of a shooter, e.g. 0x08528614. */
    int8_t volleys[4][8][2];
    uint8_t volley_sizes[4];
} AosEnemyKind;

enum {
    AOS_ENEMY_BAT = 0x00,
    AOS_ENEMY_ZOMBIE = 0x01,
    AOS_ENEMY_AXE_ARMOR = 0x04,
    AOS_ENEMY_SKULL_ARCHER = 0x05,
    AOS_ENEMY_BLUE_CROW = 0x09,
    AOS_ENEMY_ZOMBIE_SOLDIER = 0x0C,
    AOS_ENEMY_SKELETON_KNIGHT = 0x0D,
    AOS_ENEMY_DURGA = 0x11,
    AOS_ENEMY_FLEAMAN = 0x18,
    AOS_ENEMY_GHOST_DANCER = 0x1D,
    AOS_ENEMY_ZOMBIE_OFFICER = 0x20,
    AOS_ENEMY_VALKYRIE = 0x29,
    AOS_ENEMY_CURLY = 0x2B,
    AOS_ENEMY_RED_CROW = 0x2D,
    AOS_ENEMY_DEAD_WARRIOR = 0x2F,
    AOS_ENEMY_DEAD_CRUSADER = 0x52,
    AOS_ENEMY_GREMLIN = 0x37,
    AOS_ENEMY_CAGNAZZO = 0x49,
    AOS_ENEMY_LIGHTNING_DOLL = 0x3B,
    AOS_ENEMY_BEAM_SKELETON = 0x59,
    AOS_ENEMY_SKULL_MILLIONE = 0x5B,
    AOS_ENEMY_LUBICANT = 0x63,
    AOS_ENEMY_MEDUSA_HEAD = 0x39,
    AOS_ENEMY_GARGOYLE = 0x57,
    AOS_ENEMY_MIMIC = 0x60,
    AOS_ENEMY_ERINYS = 0x62,
    AOS_ENEMY_NEEDLES = 0x3F,
};

/* What an AosEnemy slot holds: the enemy itself or a child entity of its
 * kind (sharing its sprites and record). */
enum { AOS_ROLE_ENEMY = 0, AOS_ROLE_GRENADE = 1, AOS_ROLE_AXE = 2, AOS_ROLE_ARROW = 3,
       AOS_ROLE_DEBRIS = 4, AOS_ROLE_FEATHER = 5,
       AOS_ROLE_GIB = 6, AOS_ROLE_DRIP = 7, AOS_ROLE_PUDDLE = 8, AOS_ROLE_NEEDLE_PIECE = 9,
       AOS_ROLE_MIMIC_TRIGGER = 10, AOS_ROLE_FIN = 11, AOS_ROLE_BEAM = 12,
       AOS_ROLE_BEAM_FLASH = 13, AOS_ROLE_DOLL_PIECE = 14 };

typedef struct AosEnemy {
    uint8_t id;             /* + 0x36 */
    int32_t x, y;           /* + 0x40, + 0x44 (room pixels, 16.16) */
    int32_t vx, vy;         /* + 0x48, + 0x4C */
    int32_t ax, ay;         /* + 0x50, + 0x54 */
    uint8_t state;          /* + 0x0A */
    uint8_t step;           /* + 0x0B */
    uint8_t substep;        /* + 0x0C */
    uint8_t timer;          /* + 0x0D */
    uint16_t phase;         /* + 0x14 */
    bool mirrored;          /* + 0x58 bit 0x40 */
    bool removed;           /* + 0x59 bit 8: deleted by the framework */
    bool vflip;             /* + 0x58 bit 0x80 */
    bool hidden;            /* + 0x58 bit 0x20 */
    int16_t hp;             /* + 0x34 */
    int16_t param0, param1; /* + 0x30, + 0x32: record parameters */
    int32_t walk_timer;     /* + 0x1C (a gib: its parent's y at spawn) */
    uint8_t spawned_count;  /* + 0x18 of a spawner */
    bool attack_off;        /* + 0x72 bit 1 */
    bool cycle_done;        /* + 0x59 bit 0: the animation looped or ended */
    uint8_t hit_flash;      /* + 0x2D */
    uint8_t role;           /* AOS_ROLE_* */
    bool defeated;          /* + 0x3E bit 1, set at death by sub_080683BC */
    int16_t static_frame;   /* sprite frame of a child (+ 0x65), or -1 */
    bool own_boxes;         /* boxes set by code, not by the animation frame */
    uint8_t ground;         /* + 0x3F: floor cell byte under the walker */
    uint32_t angle;         /* + 0x14 of a spinning child (affine rotation) */
    int32_t spin;           /* + 0x18: angle step per frame */
    int8_t volley_count;    /* + 0x14 of a shooter: volleys fired, 0..4 */
    uint8_t volley, shot;   /* current volley list and entry (+ 0x1C) */
    int8_t away;            /* + 0x20 at death: +1 / -1, away from the attacker */
    bool dropped, puddled;  /* + 0x14, + 0x15 of a walking zombie */
    bool guarding;          /* + 0x21 of the dead crusader: blows deal 0 */
    uint8_t variant;        /* + 0x14 of a Skull Millione, Cagnazzo or Lubicant */
    int32_t speed;          /* + 0x20 of their chase */
    const struct AosEnemy *parent;  /* a fin's or beam's owner, set by the caller */
    int16_t home, home_right, home_left;  /* + 0x22, + 0x14, + 0x16 of a beam skeleton */
    uint16_t rest;          /* + 0x1C of a beam skeleton: frames before it may fire */
    uint8_t patience;       /* + 0x19 */
    bool firing;            /* + 0x1A bit 0: its beam lives */
    uint16_t scale;         /* + 0x20: the vertical scale of its affine slot */
    bool scaled;            /* + 0x59 bit 0x10: drawn with the owner's scale */
    uint16_t attack_wait, flight;   /* + 0x14, + 0x18 of a Lightning Doll */
    int16_t leg;            /* + 0x16: frames of its current drift */
    AosEffect glow;         /* a doll piece's flame (sub_080C9058) */
    bool translucent;       /* + 0x5A bits 1-2 = 2: semi-transparent OBJ */
    uint8_t fade, fade_timer;   /* + 0x18, + 0x19 of a Ghost Dancer */
    AosBox own_hurt, own_attack;
    AosEnemyStats stats;
    AosCombat combat;
    AosAnimState anim;
} AosEnemy;

#define AOS_ENEMY_MAX_EFFECTS 4  /* at most 3 per frame (sub_08092CCC) */

/* What one collision pass did. */
typedef struct {
    bool soma_hit;          /* the enemy's attack hit Soma */
    int soma_damage;
    bool enemy_hit;         /* the weapon hit the enemy */
    int enemy_damage;
    bool killed;
    bool spawn;             /* a zombie spawner created a zombie here */
    int32_t spawn_x, spawn_y;   /* room pixels */
    int16_t spawn_param1;       /* + 0x32 of the spawned enemy */
    uint8_t child_count;    /* child entities to add to the room */
    AosEnemy children[AOS_ENEMY_MAX_CHILDREN];
    bool wake_spawner;      /* a Mimic trigger was struck (sub_080993EC) */
    uint8_t effect_count;   /* shared effects to add */
    AosEffect effects[AOS_ENEMY_MAX_EFFECTS];
} AosHitReport;

/* RandomNumberGenerator: r = (r >> 8) * 0x3243F6AD + 0x1B0CB175. The seed
 * the game starts from is not traced; aos_random_seed sets it. */
uint32_t aos_random(void);
void aos_random_seed(uint32_t seed);
/* sub_080009E4: sine of a 16-bit angle (0x10000 per turn), 16.16. */
int32_t aos_sine(uint32_t angle);
/* ArcTan2 (BIOS call 0x0A): the angle of (x, y), 0x10000 per turn. */
uint16_t aos_arctan2(int16_t x, int16_t y);
/* gEwramData + 0x8E bit 0x40, read by EnemyBatCreate (a bat created while
 * it is set stays in state 4, inert). It is set by sub_0804C404 (reached
 * through the table 0x085254D8) and cleared by sub_0804CC14; its meaning is
 * not traced. Cleared by default. */
void aos_enemy_set_flag_8e_40(bool set);
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
