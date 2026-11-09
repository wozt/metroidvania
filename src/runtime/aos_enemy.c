/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies; comments name the cvaos routine. */
#include "aos_enemy_internal.h"

#include "aos_audio.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>


static bool flag_8e_40;

/* The red soul skills' attack boxes this frame (aos_enemy_set_soul_attacks). */
static AosSoulAttack *soul_attacks;
static int soul_attack_count;

enum { SOUL_ATTACK_CONTACTS = 64 };
static int16_t soul_contact[SOUL_ATTACK_CONTACTS][2];

void aos_enemy_set_soul_attacks(AosSoulAttack *attacks, int count) {
    soul_attacks = attacks;
    soul_attack_count = attacks ? count : 0;
}

void aos_enemy_soul_attack_contact(int index, int *x, int *y) {
    bool ok = index >= 0 && index < SOUL_ATTACK_CONTACTS;
    *x = ok ? soul_contact[index][0] : 0;
    *y = ok ? soul_contact[index][1] : 0;
}

void aos_enemy_set_flag_8e_40(bool set) {
    flag_8e_40 = set;
}

static const AosEffect *burst_source;
static size_t burst_source_count;

void aos_enemy_set_burst_source(const AosEffect *list, size_t count) {
    burst_source = list;
    burst_source_count = count;
}

/* sub_0806D894 (smoke, + 0x18 = 7), sub_0806D930(x, y, 0, 0, p, 1, 7, 5)
 * (dark smoke) and sub_0806D644 (flame art, + 0x18 = 3): EntityCreateInRange(
 * 0x49, 0xCF, sub_08068F78) at (x, y) with (vx, vy). */
static void spawn_particle(uint8_t kind, uint8_t flags, int32_t x, int32_t y, int32_t vx,
                           int32_t vy, AosHitReport *report) {
    if (report->effect_count >= AOS_ENEMY_MAX_EFFECTS) return;
    report->effects[report->effect_count++] = (AosEffect){
        .kind = kind, .x = x, .y = y, .particle = true, .flags = flags, .vx = vx, .vy = vy};
}

/* sub_08045CEC(x, y, animation, row): a drifting effect, NULL when no
 * slot is left (EntityCreateInRange(0x49, 0xCF) failing). */
AosEffect *aos_enemy_spawn_drift(uint8_t kind, int32_t x, int32_t y,
                                 AosHitReport *report) {
    if (report->effect_count >= AOS_ENEMY_MAX_EFFECTS) return NULL;
    AosEffect *e = &report->effects[report->effect_count++];
    *e = (AosEffect){.kind = kind, .x = x, .y = y, .drift = true};
    return e;
}
#define spawn_drift aos_enemy_spawn_drift

/* sub_0802D5EC(n): (random & (n - 1)) - n / 2. */
int32_t aos_enemy_random_centred(uint32_t (*random)(void), uint32_t n) {
    return (int32_t)(random() & (n - 1)) - (int32_t)(n >> 1);
}
#define random_centred aos_enemy_random_centred

/* A unit vector at a random angle: (sin(a + 0x4000), sin(a)) times 1.0
 * (sub_0803E86C). */
static void random_direction(uint32_t (*random)(void), int32_t *vx, int32_t *vy) {
    uint32_t a = random() & 0xFFFF;
    *vx = (int32_t)(((int64_t)aos_sine(a + 0x4000) * 0x10000) >> 16);
    *vy = (int32_t)(((int64_t)aos_sine(a) * 0x10000) >> 16);
}

/* The grenade's burst particles of sub_08092CCC for timer t (already
 * incremented): smoke at a random direction, dark smoke at t = 3, a flame
 * particle at another random direction when t is odd. */
static void grenade_particles(const AosEnemy *g, uint32_t (*random)(void), AosHitReport *report) {
    int32_t vx, vy;
    random_direction(random, &vx, &vy);
    spawn_particle(AOS_EFFECT_SMOKE, 7, g->x, g->y, vx, vy, report);
    if (g->timer == 3) spawn_particle(AOS_EFFECT_DARK_SMOKE, 7, g->x, g->y, 0, 0, report);
    if (g->timer & 1) {
        random_direction(random, &vx, &vy);
        spawn_particle(AOS_EFFECT_FLAME, 3, g->x, g->y, vx, vy, report);
    }
}

/* sub_0806E664(x, y, 1): EntityCreateInRange(0x49, 0xCF, sub_0806B5DC) at
 * integer pixels, mirrored by a random draw made only when created. */
static void spawn_blood(int x, int y, uint32_t (*random)(void), AosHitReport *report) {
    if (report->effect_count >= AOS_ENEMY_MAX_EFFECTS) return;
    AosEffect *blood = &report->effects[report->effect_count++];
    *blood = (AosEffect){.kind = AOS_EFFECT_BLOOD, .x = (int32_t)((uint32_t)x << 16),
                         .y = (int32_t)((uint32_t)y << 16)};
    blood->mirrored = random() & 1;
}

/* sub_0806B7D8 chooses one of the common hit arts from the attack's first
 * relevant element. A weakness uses the intersecting element unchanged; on
 * the ordinary path physical type 1 changes to impact type 0. A fully
 * resisted element also uses type 0. */
static uint8_t hit_effect_type(uint16_t elements, uint16_t weak, uint16_t resist) {
    uint16_t active = elements & 0x3F;
    uint16_t relevant = active & weak;
    bool weakness = relevant != 0;
    if (!weakness) {
        uint16_t resisted = active & resist;
        uint16_t resisted_nonphysical = resisted & (uint16_t)~1u;
        if ((resisted_nonphysical &&
             (active & (uint16_t)~1u) == resisted_nonphysical) ||
            (!resisted_nonphysical && active == resisted))
            return 0;
        relevant = active;
    }
    uint8_t type = 0;
    for (uint8_t bit = 1; bit <= 6; ++bit)
        if (relevant & (1u << (bit - 1))) type = bit;
    return !weakness && type == 1 ? 0 : type;
}

/* 0x08526D0C: animation/palette/sound records consumed by sub_0806B5DC.
 * The arts already shared elsewhere keep their established effect kind. */
static void spawn_hit_effect(uint8_t type, int x, int y, AosHitReport *report) {
    static const uint8_t kinds[7] = {
        AOS_EFFECT_IMPACT, AOS_EFFECT_BLOOD, AOS_EFFECT_EMBER,
        AOS_EFFECT_HIT_ELEMENT_3, AOS_EFFECT_SPARK, AOS_EFFECT_HIT_ELEMENT_5,
        AOS_EFFECT_OPENING_PROJECTILE,
    };
    static const uint16_t sounds[7] = {0xA4, 0x9E, 0xA1, 0xA5, 0xA8, 0xA0, 0xA6};
    if (type >= 7) type = 0;
    aos_audio_play_song(sounds[type]);
    if (type == 1) {
        spawn_blood(x, y, aos_random, report);
        return;
    }
    if (report->effect_count >= AOS_ENEMY_MAX_EFFECTS) return;
    AosEffect *effect = &report->effects[report->effect_count++];
    *effect = (AosEffect){.kind = kinds[type],
                          .x = (int32_t)((uint32_t)x << 16),
                          .y = (int32_t)((uint32_t)y << 16),
                          .mirrored = (aos_random() & 1) != 0};
}

/* The flame spawn of sub_0806BE74 and sub_0806AEAC: EntityCreateInRange(
 * 0x49, 0xCF, sub_0806E524) at a random point of a w x h area (16.16)
 * centred 6 pixels above the entity, mirrored at random (a third random
 * draw, only when the entity was created); priority + 0x5C - 1. A 0 x 0
 * area still draws twice: the ROM's __umodsi3 (0x080DAA00) returns 0 for a
 * zero divisor. */
uint32_t aos_enemy_umod(uint32_t value, uint32_t divisor) {
    return divisor ? value % divisor : 0;
}
#define umod aos_enemy_umod

void aos_enemy_spawn_flame_at(int32_t cx, int32_t cy, uint32_t w, uint32_t h,
                              uint32_t (*random)(void), AosHitReport *report) {
    /* sub_0806D5C0 uses GBA 32-bit ADD/SUB for fixed-point sprite
     * positions. Explicit unsigned operations preserve its modulo-2^32
     * wrap near the signed room boundary without signed overflow in C.
     * Keep the same two RNG calls in X-then-Y order; the mirror RNG
     * draw below is made only if an effect slot was allocated. */
    const uint32_t dx = umod(random(), w);
    const uint32_t dy = umod(random(), h);
    const int32_t x = (int32_t)((uint32_t)cx + dx - (w / 2u));
    const int32_t y = (int32_t)((uint32_t)cy + dy - (h / 2u));
    if (report->effect_count >= AOS_ENEMY_MAX_EFFECTS) return;
    AosEffect *flame = &report->effects[report->effect_count++];
    *flame = (AosEffect){.kind = AOS_EFFECT_FLAME, .x = x, .y = y};
    flame->mirrored = random() & 1;
}
#define spawn_flame_at aos_enemy_spawn_flame_at

static void spawn_flame(const AosEnemy *e, uint32_t w, uint32_t h, uint32_t (*random)(void),
                        AosHitReport *report) {
    spawn_flame_at(e->x, e->y - 0x60000, w, h, random, report);
}

/* sub_0806BE74(e, period, w, h): a flame when the death timer is a multiple
 * of the period, the blink pattern (hidden below 40), then the timer drops
 * and the entity is deleted at 0. Returns true when deleted. */
bool aos_enemy_death_flames(AosEnemy *e, const AosEnemyKind *kind, int period,
                            uint32_t w, uint32_t h,
                            uint32_t (*random)(void), AosHitReport *report) {
    if (e->timer % period == 0) spawn_flame(e, w, h, random, report);
    e->hidden = e->timer <= 0x27 ? kind->blink[e->timer] & 1 : false;
    if ((int8_t)--e->timer <= 0) e->removed = true;
    return e->removed;
}
#define death_flames aos_enemy_death_flames

void aos_enemy_take_effects(AosHitReport *to, const AosHitReport *from) {
    to->effect_count = from->effect_count;
    memcpy(to->effects, from->effects, sizeof from->effects[0] * from->effect_count);
}
#define take_effects aos_enemy_take_effects

static void append_effects(AosHitReport *to, const AosHitReport *from) {
    for (int index = 0;
         index < from->effect_count &&
         to->effect_count < AOS_ENEMY_MAX_EFFECTS;
         ++index)
        to->effects[to->effect_count++] = from->effects[index];
}

/* The whole-pixel distance from `from` to `to`, rounded toward zero. */
int32_t aos_enemy_pixel_delta(int32_t to, int32_t from) {
    return to >= from ? (to - from) >> 16 : -((from - to) >> 16);
}
#define pixel_delta aos_enemy_pixel_delta

/* sub_080694B8: velocity toward (tx, ty) at `speed`; true once the target
 * is within `speed`. The gEwramData + 0xA094 offsets it adds to the entity
 * are also added to the targets of the crow, so they cancel out. */
static bool move_toward(AosEnemy *e, int32_t tx, int32_t ty, int32_t speed) {
    int32_t dx = pixel_delta(tx, e->x), dy = pixel_delta(ty, e->y);
    uint16_t angle = aos_arctan2((int16_t)dx, (int16_t)dy);
    e->vx = aos_fixed_mul(aos_sine(angle + 0x4000u), speed);
    e->vy = aos_fixed_mul(aos_sine(angle), speed);
    return (int32_t)(aos_isqrt((uint32_t)(dx * dx + dy * dy)) << 16) <= speed;
}

/* sub_080695B4: the same polar solution, written to acceleration instead
 * of velocity. */
static bool accelerate_toward(AosEnemy *e, int32_t tx, int32_t ty, int32_t speed) {
    int32_t dx = pixel_delta(tx, e->x), dy = pixel_delta(ty, e->y);
    uint32_t distance = aos_isqrt((uint32_t)(dx * dx + dy * dy));
    if (!distance) {
        e->ax = e->ay = 0;
        return true;
    }
    uint16_t angle = aos_arctan2((int16_t)dx, (int16_t)dy);
    e->ax = aos_fixed_mul(aos_sine(angle + 0x4000u), speed);
    e->ay = aos_fixed_mul(aos_sine(angle), speed);
    return (int32_t)(distance << 16) <= speed;
}

/* sub_0803F2C8: starting an animation also clears + 0x59 bit 0. */
void aos_enemy_play_loop(AosEnemy *enemy, const AosAnimSet *anims,
                         unsigned id, bool loop) {
    aos_anim_start(&enemy->anim, anims, id, loop);
    enemy->anim.id = (uint8_t)id;
    enemy->cycle_done = false;
}
#define play_loop aos_enemy_play_loop

/* sub_0806D128(e, n): the screen position beyond margin entry n. */
bool aos_enemy_outside_margins(const AosEnemyKind *kind, int n, int sx, int sy) {
    int mx = kind->margins[n][0], my = kind->margins[n][1];
    return sx < -mx || sx > mx + 0xF0 || sy < -my || sy > my + 0xA0;
}
#define outside_margins aos_enemy_outside_margins

void aos_enemy_play(AosEnemy *enemy, const AosAnimSet *anims, unsigned id) {
    play_loop(enemy, anims, id, true);
}
#define play aos_enemy_play

/* sub_0803F17C; + 0x59 bit 0 is set when the animation loops or ends. */
void aos_enemy_step_anim(AosEnemy *enemy, const AosAnimSet *anims) {
    bool ended = enemy->anim.flags & AOS_ANIM_ENDED;
    int result = aos_anim_step(&enemy->anim, anims);
    if (result == 4 || (result == 3 && !ended)) enemy->cycle_done = true;
}
#define step_anim aos_enemy_step_anim

/* gEwramData + 0x13168, the entity the enemies aim at (sub_0806CD38 /
 * sub_0806CD78 and the helpers built on them): Soma, or the Killer Doll
 * soul's decoy while it exists. Contact boxes and the routines that read
 * Soma (0x13110) directly keep his position. */
static struct {
    bool active;
    int32_t x, y;
} decoy;

void aos_enemy_set_decoy(bool active, int32_t x, int32_t y) {
    decoy.active = active;
    decoy.x = x;
    decoy.y = y;
}

/* The entity being updated, the target while 0x13168 is 0. */
static bool no_target;
static const AosEnemy *aim_self;

void aos_enemy_set_no_target(bool none) { no_target = none; }

int32_t aos_enemy_target_x(const AosSoma *soma) {
    if (no_target && aim_self) return aim_self->x;
    return decoy.active ? decoy.x : soma->x;
}
#define target_x(...) aos_enemy_target_x(__VA_ARGS__)
int32_t aos_enemy_target_y(const AosSoma *soma) {
    if (no_target && aim_self) return aim_self->y;
    return decoy.active ? decoy.y : soma->y;
}
#define target_y(...) aos_enemy_target_y(__VA_ARGS__)

/* sub_0806BC40 */
void aos_enemy_face_player(AosEnemy *enemy, const AosSoma *soma) {
    if (target_x(soma) == enemy->x) return;
    enemy->mirrored = enemy->x < target_x(soma);
}
#define face_player aos_enemy_face_player

/* sub_0806E120 */
int32_t aos_enemy_toward_facing(const AosEnemy *enemy, int32_t speed) {
    return enemy->mirrored ? speed : -speed;
}
#define toward_facing aos_enemy_toward_facing

/* sub_0806BDEC */
static int16_t player_dy(const AosEnemy *enemy, const AosSoma *soma) {
    return (int16_t)((enemy->y >> 16) - (target_y(soma) >> 16));
}

/* sub_08068AD4 with the player as the entity. */
bool aos_enemy_player_in(const AosSoma *soma, int32_t x, int32_t y,
                         int w, int h) {
    return (uint16_t)((target_x(soma) >> 16) - x) <= w && (uint16_t)((target_y(soma) >> 16) - y) <= h;
}
#define player_in aos_enemy_player_in

#include "aos_enemies/bat.c"

/* sub_0806AF98: the entity's feet onto the floor below or above it. */
void aos_enemy_snap_to_floor(AosEnemy *enemy, const AosCollision *layer) {
    int32_t x = enemy->x >> 16;
    int y = enemy->y >> 16;
    if (aos_floor_depth(layer, x, y, 0, false) < 0) {
        for (int guard = 0; guard < 512 && aos_floor_depth(layer, x, y, 0, false); ++guard) y -= 8;
        y += 7;
    } else {
        for (int guard = 0; guard < 512 && !aos_floor_depth(layer, x, y, 0, false); ++guard) y += 8;
    }
    y += aos_floor_depth(layer, x, y, 0, false);
    enemy->y = (int32_t)((uint32_t)y << 16) | (enemy->y & 0xFFFF);
}
#define snap_to_floor aos_enemy_snap_to_floor

/* sub_0806D104: a slope byte at the position. */
static bool on_slope(const AosCollision *layer, int32_t x, int32_t y) {
    return aos_collision_cell(layer, x, y) & 0xC0;
}

/* The zombie's constructors, in aos_enemies/zombie.c. */
static void zombie_vanish(AosEnemy *zombie);
static void zombie_create(AosEnemy *zombie, const AosSoma *soma, const AosCollision *layer,
                          const AosAnimSet *anims);

static bool is_early_enemy(uint8_t id) {
    return id == AOS_ENEMY_SKELETON || id == AOS_ENEMY_GHOST ||
           id == AOS_ENEMY_WINGED_SKELETON;
}

static void nemesis_helper_step(AosEnemy *n, const AosSoma *soma);
static int cell_kind(const AosCollision *layer, int32_t x, int32_t y);
static void nemesis_appear(AosEnemy *n, const AosSoma *soma);

static bool enemy_create(AosEnemy *enemy, uint8_t id, int32_t x, int32_t y, int16_t param0,
                         int16_t param1, const AosSoma *soma, const AosCollision *layer,
                         const AosEnemyKind *kind, const AosEnemyStats *stats);

bool aos_enemy_create(AosEnemy *enemy, uint8_t id, int32_t x, int32_t y, int16_t param0,
                      int16_t param1, const AosSoma *soma, const AosCollision *layer,
                      const AosEnemyKind *kind, const AosEnemyStats *stats) {
    aim_self = enemy;
    bool created = enemy_create(enemy, id, x, y, param0, param1, soma, layer, kind, stats);
    aim_self = NULL;
    return created;
}

static bool enemy_create(AosEnemy *enemy, uint8_t id, int32_t x, int32_t y, int16_t param0,
                         int16_t param1, const AosSoma *soma, const AosCollision *layer,
                         const AosEnemyKind *kind, const AosEnemyStats *stats) {
    if (id != AOS_ENEMY_BAT && id != AOS_ENEMY_ZOMBIE && !is_early_enemy(id) &&
        id != AOS_ENEMY_BLUE_CROW &&
        id != AOS_ENEMY_MERMAN && id != AOS_ENEMY_SIREN && id != AOS_ENEMY_TINY_DEVIL &&
        id != AOS_ENEMY_CATOBLEPAS && id != AOS_ENEMY_IMP && id != AOS_ENEMY_GORGON &&
        id != AOS_ENEMY_HARPY && id != AOS_ENEMY_RIPPER &&
        id != AOS_ENEMY_ZOMBIE_SOLDIER && id != AOS_ENEMY_AXE_ARMOR && id != AOS_ENEMY_ROCK_ARMOR &&
        id != AOS_ENEMY_KILLER_FISH && id != AOS_ENEMY_BUER && id != AOS_ENEMY_WHITE_DRAGON &&
        id != AOS_ENEMY_GIANT_GHOST && id != AOS_ENEMY_BONE_PILLAR &&
        id != AOS_ENEMY_SKULL_ARCHER && id != AOS_ENEMY_NEEDLES && id != AOS_ENEMY_MEDUSA_HEAD &&
        id != AOS_ENEMY_ZOMBIE_OFFICER && id != AOS_ENEMY_GARGOYLE && id != AOS_ENEMY_GHOST_DANCER &&
        id != AOS_ENEMY_CURLY && id != AOS_ENEMY_DURGA && id != AOS_ENEMY_VALKYRIE &&
        id != AOS_ENEMY_ERINYS && id != AOS_ENEMY_GREMLIN && id != AOS_ENEMY_SKELETON_KNIGHT &&
        id != AOS_ENEMY_DEAD_WARRIOR && id != AOS_ENEMY_FLEAMAN && id != AOS_ENEMY_RED_CROW &&
        id != AOS_ENEMY_MIMIC && id != AOS_ENEMY_DEAD_CRUSADER && id != AOS_ENEMY_CAGNAZZO &&
        id != AOS_ENEMY_SKULL_MILLIONE && id != AOS_ENEMY_LUBICANT &&
        id != AOS_ENEMY_BEAM_SKELETON && id != AOS_ENEMY_LIGHTNING_DOLL &&
        id != AOS_ENEMY_KILLER_DOLL && id != AOS_ENEMY_NEMESIS && id != AOS_ENEMY_PEEPING_EYE &&
        id != AOS_ENEMY_MINOTAUR && id != AOS_ENEMY_RED_MINOTAUR &&
        id != AOS_ENEMY_GIANT_WORM && id != AOS_ENEMY_POISON_WORM &&
        id != AOS_ENEMY_BIPHRON && id != AOS_ENEMY_BOMBER_ARMOR &&
        id != AOS_ENEMY_SKY_FISH && id != AOS_ENEMY_ECTOPLASM &&
        id != AOS_ENEMY_EVIL_BUTCHER && id != AOS_ENEMY_KILLER_MANTLE &&
        id != AOS_ENEMY_DRYAD && id != AOS_ENEMY_TSUCHINOKO &&
        id != AOS_ENEMY_KICKER_SKELETON && id != AOS_ENEMY_WAITER_SKELETON &&
        id != AOS_ENEMY_COCKATRICE && id != AOS_ENEMY_CHRONOMAGE &&
        id != AOS_BOSS_HEADHUNTER)
        return false;
    const AosAnimSet *anims = kind->anims;
    *enemy = (AosEnemy){.id = id, .x = (int32_t)((uint32_t)x << 16), .y = (int32_t)((uint32_t)y << 16),
                        .param0 = param0, .param1 = param1, .static_frame = -1};
    /* sub_0800F1FC: mirrored when the record lies left of the player. */
    enemy->mirrored = enemy->x < target_x(soma);
    /* sub_0806B04C / sub_0806D244: stats and the type 8 collision block. */
    enemy->stats = *stats;
    enemy->hp = stats->hp;
    enemy->combat.type = AOS_TYPE_ENEMY;
    if (id == AOS_BOSS_HEADHUNTER) {
        /* EnemyHeadhunterCreate: the main body starts on sequence 23. The
         * three sub_08083FD0 actors are emitted by its first host update so
         * they can use the current camera origin. */
        play(enemy, anims, 0x17);
        step_anim(enemy, anims);
        enemy->state = 0;
        return true;
    }
    if (is_early_enemy(id)) {
        /* Constructor-table batch. These initial states, animations, boxes,
         * floor offsets and producer forms come directly from the ten native
         * create routines. Their multipart children and attack states remain
         * deliberately outside this first pass. */
        if (id == AOS_ENEMY_GHOST && param1) {
            /* EnemyGhostCreate's room producer: no art or collision, state 3.
             * param0 caps its live children and param1 delays spawn attempts. */
            enemy->state = 3;
            enemy->hidden = true;
            enemy->combat.attack_off = enemy->combat.hurt_off = true;
            return true;
        }
        unsigned animation = id == AOS_ENEMY_WINGED_SKELETON ? 2 : 0;
        bool direct_frame = false;
        if (direct_frame) enemy->static_frame = 0;
        else {
            play(enemy, anims, animation);
            step_anim(enemy, anims);
        }
        if (flag_8e_40) {
            enemy->state = id == AOS_ENEMY_SKELETON ? 5 :
                           id == AOS_ENEMY_WINGED_SKELETON ? 4 :
                           2;
            if (id == AOS_ENEMY_GHOST) enemy->y -= AOS_FIXED(36);
            return true;
        }
        if (id == AOS_ENEMY_WINGED_SKELETON && param1) {
            enemy->state = 3;
            enemy->hidden = true;
            enemy->combat.attack_off = enemy->combat.hurt_off = true;
            return true;
        }
        face_player(enemy, soma);
        switch (id) {
        case AOS_ENEMY_SKELETON:
            /* EnemySkeletonCreate: the floor (sub_0806AF98), + 0x1A bit 0
             * (the head on) and bit 2 (the bone in hand, whose child, like
             * the head's, the first update creates), the patrol bounds
             * x +/- 32 and state 1 (its random draw is not consumed here). */
            snap_to_floor(enemy, layer);
            enemy->skel_flags = 1 | 4;
            enemy->home = (int16_t)(enemy->x >> 16);
            enemy->home_left = enemy->home - 32;
            enemy->home_right = enemy->home + 32;
            enemy->state = 1;
            break;
        case AOS_ENEMY_GHOST:
            enemy->state = 0;
            enemy->combat.attack_off = enemy->combat.hurt_off = true;
            break;
        case AOS_ENEMY_WINGED_SKELETON:
            /* EnemyWingedSkeletonCreate: facing Soma (sub_0806CF2C), it flies
             * at 0.75 + (r & 0x3FFF) toward him, level. */
            enemy->state = 0;
            enemy->mirrored = (enemy->x >> 16) < (target_x(soma) >> 16);
            {
                int32_t r = (int32_t)(aos_random() & 0x3FFF);
                enemy->vx = enemy->mirrored ? r + 0xC000 : (int32_t)0xFFFF4000 - r;
            }
            break;
        default: break;
        }
        return true;
    }
    if (id == AOS_ENEMY_ZOMBIE) {
        zombie_create(enemy, soma, layer, anims);
        return true;
    }
    if (id == AOS_ENEMY_MEDUSA_HEAD) {
        /* EnemyMedusaHeadCreate: animation 0 looping and one step; both
         * boxes are the bytes of 0x0A0AFBFB, (-5, -5, 10, 10); parameter 1
         * bit 0 picks palette bank 1 (a gold head) and bit 1 makes a hidden
         * spawner without boxes (state 3); otherwise state 0, facing the
         * player (sub_0806CF2C), vx 0.75 + (r & 0x3FFF) toward its facing,
         * + 0x54 = 0xC00, + 0x20 = r & 1, + 0x22 = 45 + (r & 0xF) and
         * + 0x21 = + 0x22 / 2. State 4 (inert) under the global
         * 0x8E & 0x40. */
        play(enemy, anims, 0);
        step_anim(enemy, anims);
        if (flag_8e_40) {
            enemy->state = 4;
            return true;
        }
        enemy->own_boxes = true;
        enemy->own_hurt = enemy->own_attack = (AosBox){-5, -5, 10, 10};
        /* sub_08093BF0: a gold head (+ 0x32 bit 0) petrifies (0x100). */
        enemy->contact_elements = (param1 & 1) ? 0x100 : 0;
        if (param1 & 2) {
            enemy->state = 3;
            enemy->hidden = true;
            enemy->combat.attack_off = enemy->combat.hurt_off = true;
            enemy->own_boxes = false;
            return true;
        }
        enemy->state = 0;
        enemy->mirrored = (enemy->x >> 16) < (target_x(soma) >> 16);
        int32_t speed = (int32_t)(aos_random() & 0x3FFF) + 0xC000;
        enemy->vx = enemy->mirrored ? speed : -speed;
        enemy->ay = 0xC00;
        enemy->dropped = aos_random() & 1;                     /* + 0x20 */
        enemy->volley_count = 0;                                /* + 0x21 */
        enemy->shot = (uint8_t)((aos_random() & 0xF) + 0x2D);  /* + 0x22 */
        enemy->volley_count = (int8_t)(enemy->shot / 2);
        return true;
    }
    if (id == AOS_ENEMY_NEEDLES) {
        /* EnemyNeedlesCreate: animation 0 looping, not mirrored, state 1
         * (state 3, inert, under the global 0x8E & 0x40); both boxes are
         * the bytes of 0x1010F8F8, (-8, -8, 16, 16) (sub_08042884 /
         * sub_080428F0); with parameter 0 set it bobs from a random phase
         * (RandomNumberGenerator % 0xFFFF). */
        play(enemy, anims, 0);
        enemy->mirrored = false;
        enemy->state = flag_8e_40 ? 3 : 1;
        if (flag_8e_40) return true;
        enemy->own_boxes = true;
        enemy->own_hurt = enemy->own_attack = (AosBox){-8, -8, 16, 16};
        if (param0) enemy->phase = (uint16_t)(aos_random() % 0xFFFF);
        return true;
    }
    if (id == AOS_ENEMY_SKULL_ARCHER) {
        /* EnemySkullArcherCreate: idle animation, floor snap plus one
         * pixel; a record with parameter 0 patrols (state 2, animation 1),
         * otherwise it stands (state 0). Under the global 0x8E & 0x40,
         * state 4 (animation only, no hit callback, no floor snap). */
        play(enemy, anims, 0);
        step_anim(enemy, anims);
        if (flag_8e_40) {
            enemy->state = 4;
            return true;
        }
        snap_to_floor(enemy, layer);
        enemy->y += 0x10000;
        enemy->state = 0;
        if (param0) {
            enemy->state = 2;
            play(enemy, anims, 1);
        }
        return true;
    }
    if (id == AOS_ENEMY_PEEPING_EYE) {
        /* EnemyPeepingEyeCreate: animation 0 and one step; its parts (made
         * by its first update) after one random draw (r & 1 reverses the
         * segments' angles). State 4, 48 pixels up, under the global
         * 0x8E & 0x40; otherwise the plain contact sub_08088E78, both
         * boxes the bytes of 0x0E0EF9F9 (-7, -7, 14, 14), the floor snap,
         * 8 pixels up and state 0. */
        play(enemy, anims, 0);
        step_anim(enemy, anims);
        enemy->pe_reverse = (aos_random() & 1) != 0;
        if (flag_8e_40) {
            enemy->state = 4;
            enemy->y -= AOS_FIXED(48);
            enemy->combat.attack_off = enemy->combat.hurt_off = true;
            return true;
        }
        enemy->own_boxes = true;
        enemy->own_hurt = enemy->own_attack = (AosBox){-7, -7, 14, 14};
        snap_to_floor(enemy, layer);
        enemy->y -= AOS_FIXED(8);
        enemy->state = 0;
        return true;
    }
    if (id == AOS_ENEMY_BONE_PILLAR) {
        /* sub_080B3BAC (variant 0): + 0x30 clamped to 1-8 (2 under the
         * global 0x8E & 0x40, its skulls inert in state 3); otherwise the
         * floor snap and one pixel down. The unseen controller sits 22
         * pixels below the bottom skull, which rests on that floor. */
        if (flag_8e_40) {
            enemy->param0 = 2;
            enemy->state = 3;
        } else {
            snap_to_floor(enemy, layer);
            enemy->y += 0x10000;
            enemy->state = 0;
        }
        if (enemy->param0 < 1) enemy->param0 = 1;
        if (enemy->param0 > 8) enemy->param0 = 8;
        enemy->y += 0x160000;
        enemy->hidden = true;
        enemy->combat.attack_off = enemy->combat.hurt_off = true;
        enemy->bp_top = -1;
        return true;
    }
    if (id == AOS_ENEMY_GIANT_GHOST) {
        /* EnemyGiantGhostCreate (tiles 0x081CBFA4, palette 0x0820A698,
         * frames 0x08219140): animation 0 looping and one step, its three
         * parts (made by its first update here), state 3 (animation only)
         * under the global 0x8E & 0x40; otherwise contact sub_0807E328
         * (element 0x10, flagging + 0x21), hit sub_0807D88C, state 0. */
        play(enemy, anims, 0);
        step_anim(enemy, anims);
        if (flag_8e_40) {
            enemy->state = 3;
            return true;
        }
        enemy->contact_elements = 0x10;
        enemy->state = 0;
        return true;
    }
    if (id == AOS_ENEMY_WHITE_DRAGON) {
        /* EnemyWhiteDragonCreate (tiles 0x081CBF74, palette 0x0820A580,
         * frames 0x0821631C): direct frame 0, its root at the record, facing
         * right with parameter 0 set (heading 0, else 0x8000), the 100-word
         * swing history, ten segments (made by its first update here);
         * state 4 (inert, swing rate 0x400) under the global 0x8E & 0x40,
         * otherwise contact sub_08077C8C (a knockback), hit sub_080773AC
         * and both boxes (-8, -8, 16, 12) of 0x0C10F8F8. */
        enemy->static_frame = 0;
        enemy->mirrored = param0 != 0;
        enemy->wd_home_x = (int16_t)x;
        enemy->wd_home_y = (int16_t)y;
        enemy->wd_heading = enemy->mirrored ? 0 : 0x8000;
        enemy->own_boxes = true;
        enemy->own_hurt = enemy->own_attack = (AosBox){-8, -8, 16, 12};
        if (flag_8e_40) {
            enemy->state = 4;
            enemy->wd_rate = 0x400;
            enemy->combat.attack_off = enemy->combat.hurt_off = true;
            return true;
        }
        enemy->state = 0;
        return true;
    }
    if (id == AOS_ENEMY_BUER) {
        /* EnemyBuerCreate (tiles 0x081F428C, palette 0x0820BF9C, frames
         * 0x0824FDB0): direct frame 0 in an affine slot, priority 0x14, the
         * spin + 0x18 = 0.5, gravity 0x4000, state 1 (rolling) or, with
         * parameter 0, state 2; its fire (sub_080CBA30) facing the spin.
         * State 4 (inert) under the global 0x8E & 0x40; otherwise contact
         * sub_080CC4E4 (element 1, a knockback), both boxes (-20, -20, 40,
         * 40) of 0x2828ECEC, the floor snap and 24 pixels up. */
        enemy->static_frame = 0;
        enemy->spin = 0x8000;
        enemy->ay = 0x4000;
        enemy->state = param0 ? 2 : 1;
        enemy->buer_fire_mirrored = true;
        enemy->buer_fire_following = true;
        if (flag_8e_40) {
            enemy->state = 4;
            enemy->buer_fire_x = enemy->x;
            enemy->buer_fire_y = enemy->y;
            return true;
        }
        enemy->own_boxes = true;
        enemy->own_hurt = enemy->own_attack = (AosBox){-20, -20, 40, 40};
        enemy->contact_elements = 1;
        snap_to_floor(enemy, layer);
        enemy->y = (int32_t)((uint32_t)enemy->y - (24u << 16));
        enemy->buer_fire_x = enemy->x;
        enemy->buer_fire_y = enemy->y;
        return true;
    }
    if (id == AOS_ENEMY_KILLER_FISH) {
        /* EnemyKillerFishCreate (the Merman's sheet 0x081CBF3C, palette
         * 0x0820A464, frames 0x08212FB4): animation 1 looping, state 5
         * (animation only) under the global 0x8E & 0x40; out of water
         * (sub_0806D0B8 != 3) a fish lying upside down on the floor, 8
         * pixels up (step 0x63, inert); in water + 0x32 fry (made by its
         * first update here). */
        play(enemy, anims, 1);
        step_anim(enemy, anims);
        if (flag_8e_40) {
            enemy->state = 5;
            return true;
        }
        if (cell_kind(layer, enemy->x >> 16, enemy->y >> 16) != 3) {
            snap_to_floor(enemy, layer);
            enemy->step = 0x63;
            enemy->vflip = true;
            enemy->y -= AOS_FIXED(8);
        }
        enemy->state = 0;
        return true;
    }
    if (id == AOS_ENEMY_ROCK_ARMOR) {
        /* EnemyRockArmorCreate: like the Axe Armor's (tiles 0x081E41BC,
         * palette 0x0820B83C bank 1, frames 0x0823FFD8): walk animation,
         * floor snap, then one pixel down; state 0, or state 3 (animation
         * only) under the global 0x8E & 0x40. */
        play(enemy, anims, 0);
        step_anim(enemy, anims);
        if (flag_8e_40) {
            enemy->state = 3;
            return true;
        }
        snap_to_floor(enemy, layer);
        enemy->y += 0x10000;
        enemy->state = 0;
        return true;
    }
    if (id == AOS_ENEMY_BOMBER_ARMOR) {
        /* EnemyBomberArmorCreate (sub_080B2DB8): the Rock Armor's tiles
         * (0x081E41BC), palette 0x0820B83C bank 0 and frames 0x0823EC00;
         * walk animation, affine slot, state 3 under the global 0x8E &
         * 0x40, else the shared contact/hurt callbacks, the box refresh,
         * the sub_0806AF98 floor snap and state 0. The + 0x46 hurtbox
         * nudge and the + 0x59 translucent bit are not ported. */
        play(enemy, anims, 0);
        step_anim(enemy, anims);
        if (flag_8e_40) {
            enemy->state = 3;
            return true;
        }
        snap_to_floor(enemy, layer);
        enemy->state = 0;
        return true;
    }
    if (id == AOS_ENEMY_SKY_FISH) {
        /* EnemySkyFishCreate (sub_080A7C80): sub_080A8194 loads tiles
         * 0x081CC0BC, palette 0x0820B000 bank 0 and frames 0x0822976C;
         * the sub_0803F0E4 animation table 0x08528404 is the kind's, then
         * sub_0803F17C/sub_0806B04C. Under the global 0x8E & 0x40 it is
         * state 2; otherwise the contact callback sub_080A81E0 and the
         * hurt callback sub_080A7FB8, the box refresh sub_0806B1FC, the
         * sub_0806AF98 floor snap, state 0, + 0x18 = 1 (no collisions),
         * + 0x1A = 0x40 + (r & 0x3F), + 0x58 |= 0x20 (hidden) and
         * + 0x19 = r & 1 (the entry side). The RNG call order of + 0x1A
         * and + 0x19 and the variant palette bits are approximations. */
        play_loop(enemy, anims, 0, false);
        step_anim(enemy, anims);
        if (flag_8e_40) {
            enemy->state = 2;
            enemy->step = 1;
            enemy->sf_gate = 1;
            enemy->vx = enemy->mirrored ? 0x1A0000 : (int32_t)0xFFE60000;
            return true;
        }
        snap_to_floor(enemy, layer);
        enemy->state = 0;
        enemy->sf_gate = 1;
        enemy->sf_timer = (int16_t)(0x40 + (aos_random() & 0x3F));
        enemy->sf_variant = (uint8_t)(aos_random() & 1);
        enemy->hidden = true;
        return true;
    }
    if (id == AOS_ENEMY_ECTOPLASM) {
        /* EnemyEctoplasmCreate (sub_080995B8) with sub_080999F0 loading
         * tiles 0x081CC054, palette 0x0820ABE0 bank 0 and frames
         * 0x0822388C; animation 0 looping. Under the global 0x8E & 0x40 it
         * is state 2 parked at its home point; otherwise + 0x5C = 3, the
         * contact sub_08099B30 / hurt sub_080998E8 callbacks (the count
         * variant sub_0806D260 only picks the registration helper), the
         * box refresh, state 0 and + 0x59 |= 2. The home point + 0x14 /
         * + 0x18 is the position plus the native camera offset
         * (gEwramData + 0xA094); stored here in world pixels since the
         * offset cancels in every use. The 0xFFFFFF00 palette merge is
         * not ported. */
        play_loop(enemy, anims, 0, true);
        step_anim(enemy, anims);
        enemy->target_x = enemy->x;
        enemy->target_y = enemy->y;
        if (flag_8e_40) {
            enemy->state = 2;
            return true;
        }
        enemy->combat.attack_off = false;
        enemy->contact_elements = 0x80;   /* sub_08099B30: element 0x80 */
        enemy->state = 0;
        enemy->step = 0;
        return true;
    }
    if (id == AOS_ENEMY_EVIL_BUTCHER) {
        /* EnemyEvilButcherCreate (sub_080BFE1C): the resource load
         * (graphics 0x082052D0, palette 0x0820C27C bank = parameter 1 &
         * 0xFF, frames 0x08256720), + 0x5C = 0x13, animation 0 looping,
         * state 3 under the global 0x8E & 0x40, else the contact
         * sub_0806E1B8 / hurt sub_080BFECC callbacks, the box refresh,
         * the sub_0806AF98 floor snap, + 0x46 += 1 (one pixel down),
         * + 0x59 |= 2 and state 0. */
        play_loop(enemy, anims, 0, true);
        step_anim(enemy, anims);
        if (flag_8e_40) {
            enemy->state = 3;
            return true;
        }
        snap_to_floor(enemy, layer);
        enemy->y += 0x10000;
        enemy->state = 0;
        enemy->step = 0;
        return true;
    }
    if (id == AOS_ENEMY_KILLER_MANTLE) {
        /* EnemyKillerMantleCreate (sub_080BB70C): the resource load
         * (graphics 0x081F421C, palette 0x0820BD04, frames 0x0824AE0C),
         * + 0x5C = 0x13, animation 0 looping, state 4 under the global
         * 0x8E & 0x40; else the contact sub_0806E1E8 (element 1) and
         * hurt sub_080BB7E4 callbacks, the box presets of
         * sub_080428F0 / sub_08042884 (not ported: the exported frame
         * boxes stand in), + 0x72 |= 2, the linked orbit child
         * sub_080BB0F0 — emitted by the first update here since a
         * create cannot return children — and state 0. */
        play_loop(enemy, anims, 0, true);
        step_anim(enemy, anims);
        if (flag_8e_40) {
            enemy->state = 4;
            return true;
        }
        enemy->contact_elements = 1;
        enemy->combat.attack_off = true;
        enemy->state = 0;
        enemy->step = 0;
        return true;
    }
    if (id == AOS_ENEMY_DRYAD) {
        /* EnemyDryadCreate (sub_080AEAAC): the resource load (tiles
         * 0x081E418C, palette 0x0820B768, frames 0x082388AC), animation
         * 0 looping, state 4 under the global 0x8E & 0x40; else the
         * contact sub_080AEB50 (the + 0x3C attack byte through
         * sub_08021654, element 0) and hurt sub_080AE3FC callbacks, the
         * box refresh, the sub_0806AF98 floor snap, + 0x46 += 1, + 0x59
         * |= 2 and state 0. */
        play_loop(enemy, anims, 0, true);
        step_anim(enemy, anims);
        if (flag_8e_40) {
            enemy->state = 4;
            return true;
        }
        snap_to_floor(enemy, layer);
        enemy->y += 0x10000;
        enemy->state = 0;
        enemy->step = 0;
        return true;
    }
    if (id == AOS_ENEMY_TSUCHINOKO) {
        /* EnemyTsuchinokoCreate (sub_080A71CC): sub_080A7ADC loads
         * tiles 0x081CC0B4, palette 0x0820AFDC bank 0 and frames
         * 0x0822952C (a failed load deletes the entity). Animation 0
         * looping, sub_0803F17C, the sub_0806B04C record copy; state 3
         * under the global 0x8E & 0x40, else the contact sub_080A7B28
         * (the + 0x3C attack byte through sub_08021654, element 0) and
         * hurt sub_080A76E0 callbacks through sub_080427B8 /
         * sub_0804277C (the sub_0806D260 count picks the installer),
         * the box refresh, the sub_0806AF98 floor snap, state 0 and
         * + 0x14 = + 0x34 (the HP at create the wounded hops read).
         * RNG & 0x7F > 0x18 leaves it dormant: step 0xC, animation 3
         * once parked on frame 8. */
        play_loop(enemy, anims, 0, true);
        step_anim(enemy, anims);
        if (flag_8e_40) {
            enemy->state = 3;
            return true;
        }
        snap_to_floor(enemy, layer);
        enemy->state = 0;
        enemy->hp_home = enemy->hp;
        if ((aos_random() & 0x7F) > 0x18) {
            enemy->step = 0xC;
            play_loop(enemy, anims, 3, false);
            enemy->anim.frame = 8;
            enemy->anim.tick = 0;
        }
        return true;
    }
    if (id == AOS_ENEMY_KICKER_SKELETON) {
        /* EnemyKickerSkeletonCreate (sub_080C0324): sub_0806E0D0 loads
         * tiles 0x08259214, palette 0x0825C320 and frames 0x0825DA58 (a
         * failed load deletes the entity). sub_0806B04C, + 0x5C = 0x13,
         * animation 0 looping, sub_0803F17C; state 3 under the global
         * 0x8E & 0x40, else the contact sub_080BFF7C (the + 0x3C power,
         * x5/4 and type 3 while + 0x14 & 1) and hurt sub_080C0478
         * callbacks, the box refresh, the sub_0806AF98 floor snap, the
         * + 0x46 -= 0x10 lift (not ported), + 0x59 |= 2 and state 0. */
        play_loop(enemy, anims, 0, true);
        step_anim(enemy, anims);
        if (flag_8e_40) {
            enemy->state = 3;
            return true;
        }
        snap_to_floor(enemy, layer);
        enemy->state = 0;
        return true;
    }
    if (id == AOS_ENEMY_WAITER_SKELETON) {
        /* EnemyWaiterSkeletonCreate (sub_080AF574): sub_0806E0D0 loads
         * tiles 0x081E4194, palette 0x0820BE3C and frames 0x082393E0; on
         * success + 0x20 |= 2 and the sub_080AEC0C companion child (the
         * constructor's child is emitted by the first update here),
         * animation 0 looping and sub_0803F17C; under the global
         * 0x8E & 0x40 state 4, else sub_0806B04C, + 0x5C = 0x13, the
         * sub_0806E1B8 contact and sub_080AF6FC hurt callbacks, the box
         * refresh, the sub_0806AF98 floor snap, + 0x46 += 1, + 0x59 |= 2
         * and state 0. */
        play_loop(enemy, anims, 0, true);
        step_anim(enemy, anims);
        enemy->ws_flags = 2;
        if (flag_8e_40) {
            enemy->state = 4;
            return true;
        }
        snap_to_floor(enemy, layer);
        enemy->y += 0x10000;
        enemy->state = 0;
        return true;
    }
    if (id == AOS_ENEMY_COCKATRICE) {
        /* sub_080BBFF0(e, 0): sub_0806E0D0 loads tiles 0x081E41CC,
         * palette 0x0820B8A4 and frames 0x08242994; + 0x14 the variant
         * (0 for the Cockatrice), sub_0806B04C, + 0x5C = 0x11,
         * animation 0 looping, the sub_0806DFF8(sub_080BB924) shadow
         * child (emitted by the first update here), state 3 under the
         * global 0x8E & 0x40, else the sub_0806E1B8 contact and
         * sub_080BC0B4 hurt callbacks, the sub_0806AF98 floor snap,
         * + 0x46 += 1 and state 0. */
        play_loop(enemy, anims, 0, true);
        step_anim(enemy, anims);
        enemy->variant = 0;
        if (flag_8e_40) {
            enemy->state = 3;
            return true;
        }
        snap_to_floor(enemy, layer);
        enemy->y += 0x10000;
        enemy->state = 0;
        return true;
    }
    if (id == AOS_ENEMY_CHRONOMAGE) {
        /* EnemyChronomageCreate (0x080B712C): sub_0806E0D0 loads tiles
         * 0x081EC1EC, palette 0x0820BBB0 and frames 0x08245CA8;
         * sub_0806B04C, + 0x5C = 0x13, animation 0 looping, the global
         * 0x8E & 0x40 sends it to state 4, else the sub_0806E1B8
         * contact and sub_080B72B8 hurt callbacks, the 0x2008E0FC box
         * preset (exported frame boxes stand in), + 0x72 |= 4 (hurt
         * off until the chase), the sub_0806AF98 floor snap, + 0x46 += 1
         * and + 0x59 |= 2, state 0. */
        play_loop(enemy, anims, 0, true);
        step_anim(enemy, anims);
        if (flag_8e_40) {
            enemy->state = 4;
            return true;
        }
        /* + 0x72 |= 4: hurt off until the chase state clears it. */
        enemy->combat.hurt_off = true;
        snap_to_floor(enemy, layer);
        enemy->y += 0x10000;
        enemy->cm_flags = 2;
        enemy->state = 0;
        return true;
    }
    if (id == AOS_ENEMY_AXE_ARMOR) {
        /* EnemyAxeArmorCreate: walk animation, floor snap, then one pixel
         * down; state 0, or state 3 (animation only, no hit callback, no
         * floor snap) under the global 0x8E & 0x40. */
        play(enemy, anims, 0);
        step_anim(enemy, anims);
        if (flag_8e_40) {
            enemy->state = 3;
            return true;
        }
        snap_to_floor(enemy, layer);
        enemy->y += 0x10000;
        enemy->state = 0;
        return true;
    }
    if (id == AOS_ENEMY_MIMIC) {
        /* EnemyMimicCreate (sub_08099430: tiles 0x081CC04C, palette
         * 0x0820AB7C bank parameter 1, frames 0x08223698): not mirrored;
         * with parameter 0 set a closed chest on the floor (sprite frame 0,
         * the hurtbox of the bytes 0x0E10F2F8 = (-8, -14, 16, 14), no
         * attack box; step 3), otherwise hidden in step 0 with its trigger
         * (created by the first update here). Under the global 0x8E & 0x40
         * state 2: animation 0 once, then animation 1. */
        enemy->mirrored = false;
        if (flag_8e_40) {
            enemy->state = 2;
            play_loop(enemy, anims, 0, false);
            step_anim(enemy, anims);
            return true;
        }
        enemy->state = 0;
        if (param0) {
            enemy->static_frame = 0;
            enemy->own_boxes = true;
            enemy->own_hurt = enemy->own_attack = (AosBox){-8, -14, 16, 14};
            enemy->combat.attack_off = true;
            enemy->step = 3;
            snap_to_floor(enemy, layer);
            return true;
        }
        play(enemy, anims, 0);
        step_anim(enemy, anims);
        enemy->hidden = true;
        enemy->step = 0;
        return true;
    }
    if (id == AOS_ENEMY_MERMAN) {
        /* EnemyMermanCreate: OBJ priority 4 and animation 0. Parameter 1
         * selects the hidden room-spawner form. A room body (parameter 0 is
         * zero) starts at step 10 in its upward water arc; a body produced by
         * the spawner starts at step 0 and waits 60 frames below the surface. */
        play(enemy, anims, 0);
        step_anim(enemy, anims);
        if (flag_8e_40) {
            enemy->state = 4;
            return true;
        }
        if (param1) {
            enemy->state = 3;
            enemy->hidden = true;
            enemy->combat.attack_off = enemy->combat.hurt_off = true;
            return true;
        }
        enemy->state = 0;
        if (!param0) {
            enemy->step = 10;
            enemy->vy = -0x8000;
            enemy->ay = 0x1800;
            face_player(enemy, soma);
        }
        return true;
    }
    if (id == AOS_ENEMY_HARPY || id == AOS_ENEMY_SIREN) {
        /* EnemySirenCreate aliases EnemyHarpyCreate. The two constructors
         * differ only by their palette bank. */
        play(enemy, anims, 0);
        step_anim(enemy, anims);
        enemy->phase = (uint16_t)(aos_random() & 0xFFFF);
        enemy->home = (int16_t)(enemy->y >> 16);
        enemy->state = flag_8e_40 ? 4 : 0;
        enemy->step = flag_8e_40 ? 0 : 10;
        if (!flag_8e_40 && anims && anims->count && anims->anims[0].count) {
            enemy->anim.tick = (uint8_t)(aos_random() & 0xF);
            enemy->anim.frame = (uint8_t)(aos_random() % anims->anims[0].count);
        }
        return true;
    }
    if (id == AOS_ENEMY_GORGON || id == AOS_ENEMY_CATOBLEPAS) {
        /* EnemyCatoblepasCreate aliases EnemyGorgonCreate and selects the
         * second palette bank. Both are grounded patrol enemies. */
        play(enemy, anims, 0);
        step_anim(enemy, anims);
        if (flag_8e_40) {
            enemy->state = 3;
            return true;
        }
        snap_to_floor(enemy, layer);
        enemy->state = 0;
        enemy->home = (int16_t)(enemy->x >> 16);
        enemy->home_left = (int16_t)(enemy->home - 0x40);
        enemy->home_right = (int16_t)(enemy->home + 0x40);
        return true;
    }
    if (id == AOS_ENEMY_TINY_DEVIL || id == AOS_ENEMY_IMP) {
        /* Tiny Devil and Imp share their sheet. Their native constructors
         * choose animation 1 and 0 respectively and use different inert
         * states under the global freeze flag. */
        play(enemy, anims, id == AOS_ENEMY_TINY_DEVIL ? 1 : 0);
        step_anim(enemy, anims);
        enemy->phase = (uint16_t)(aos_random() & 0xFFFF);
        enemy->home = (int16_t)(enemy->y >> 16);
        enemy->state = flag_8e_40 ? (id == AOS_ENEMY_TINY_DEVIL ? 3 : 4) : 1;
        enemy->step = 0;
        return true;
    }
    if (id == AOS_ENEMY_RIPPER) {
        /* EnemyRipperCreate uses the Fleaman sheet with palette bank 1. */
        play(enemy, anims, 0);
        step_anim(enemy, anims);
        if (flag_8e_40) {
            enemy->state = 3;
            return true;
        }
        enemy->state = 1;
        enemy->dropped = ((param0 & param1) & 0xFFFF) == 0xFF;
        if (!enemy->dropped) snap_to_floor(enemy, layer);
        return true;
    }
    if (id == AOS_ENEMY_FLEAMAN) {
        /* EnemyFleamanCreate (sub_080C1E88: tiles 0x081F423C, palette
         * 0x0820BD94, frames 0x0824B92C): animation 0 looping and one step,
         * state 1; + 0x14 = 1 when parameter 0 & parameter 1 is 0xFF (a
         * carried Fleaman, without the floor snap); state 3 (animation
         * only) under the global 0x8E & 0x40. */
        play(enemy, anims, 0);
        step_anim(enemy, anims);
        if (flag_8e_40) {
            enemy->state = 3;
            return true;
        }
        enemy->state = 1;
        enemy->dropped = ((param0 & param1) & 0xFFFF) == 0xFF;
        if (!enemy->dropped) snap_to_floor(enemy, layer);
        return true;
    }
    if (id == AOS_ENEMY_DEAD_WARRIOR) {
        /* EnemyDeadWarriorCreate: animation 0 looping and one step, frame
         * boxes, the floor snap plus one pixel, state 0 with + 0x1C = 50;
         * state 2 (animation only) under the global 0x8E & 0x40. */
        play(enemy, anims, 0);
        step_anim(enemy, anims);
        if (flag_8e_40) {
            enemy->state = 2;
            return true;
        }
        snap_to_floor(enemy, layer);
        enemy->y += 0x10000;
        enemy->state = 0;
        enemy->shot = 50;
        return true;
    }
    if (id == AOS_ENEMY_KILLER_DOLL) {
        /* EnemyKillerDollCreate / sub_080C7C04: the same tiles and palette
         * as the Lightning Doll, frames 0x0824DB0C; animation 0 looping,
         * stats and + 0x5C = 9. State 2 cycles the idle animation under
         * global 0x8E & 0x40. Otherwise sub_080C7C94 snaps it to the floor
         * and seeds + 0x14 to 128..2175, + 0x16 to 32..287 and + 0x18 to
         * 256..767. */
        play(enemy, anims, 0);
        step_anim(enemy, anims);
        if (flag_8e_40) {
            enemy->state = 2;
            return true;
        }
        snap_to_floor(enemy, layer);
        enemy->state = 1;
        enemy->attack_wait = (uint16_t)((aos_random() & 0x7FF) + 0x80);
        enemy->leg = (int16_t)((aos_random() & 0xFF) + 0x20);
        enemy->flight = (uint16_t)((aos_random() & 0x1FF) + 0x100);
        return true;
    }
    if (id == AOS_ENEMY_LIGHTNING_DOLL) {
        /* EnemyLightningDollCreate (sub_080C8A18: tiles 0x081F426C,
         * palette 0x0820BEEC bank 0, frames 0x0824E7F4): animation 0
         * looping and one step, stats, + 0x5C = 9; state 2 (its animation
         * cycle) under the global 0x8E & 0x40; otherwise sub_0806D244
         * (contact sub_080C99E4, hit sub_080C8FBC) and sub_080C9938: no
         * speed, the floor snap, state 1, + 0x14 = (r & 0x1F) + 0x10,
         * + 0x18 = (r & 0x3FF) + 0x200, + 0x16 = 0. */
        play(enemy, anims, 0);
        step_anim(enemy, anims);
        if (flag_8e_40) {
            enemy->state = 2;
            return true;
        }
        snap_to_floor(enemy, layer);
        enemy->state = 1;
        enemy->attack_wait = (uint16_t)((aos_random() & 0x1F) + 0x10);
        enemy->flight = (uint16_t)((aos_random() & 0x3FF) + 0x200);
        enemy->leg = 0;
        return true;
    }
    if (id == AOS_ENEMY_NEMESIS) {
        /* EnemyNemesisCreate / sub_080C6714: direct OAM frame 0 of
         * 0x0824D2D8 (the exporter's one-frame animation 0) in an affine
         * slot at angle 0, never mirrored; state 7 (inert) under the global
         * 0x8E & 0x40. Otherwise the attack box (-4, 4, 8, 8) of 0x080804FC,
         * the hurtbox (-8, -8, 16, 16), + 0x16 = 0, + 0x17 = 9, + 0x24 = 3,
         * the linked sprite (sub_080C76D4: state 1 run once) and state 0's
         * first step. */
        play(enemy, anims, 0);
        step_anim(enemy, anims);
        enemy->mirrored = false;
        enemy->angle = 0;
        if (flag_8e_40) {
            enemy->state = 7;
            return true;
        }
        enemy->own_boxes = true;
        enemy->own_attack = (AosBox){-4, 4, 8, 8};
        enemy->own_hurt = (AosBox){-8, -8, 16, 16};
        enemy->nem_row = 0;
        enemy->nem_index = 9;
        enemy->nem_hits = 3;
        enemy->nem_helper.state = 1;
        nemesis_helper_step(enemy, soma);
        enemy->state = 0;
        enemy->step = 0;
        nemesis_appear(enemy, soma);
        return true;
    }
    if (id == AOS_ENEMY_BEAM_SKELETON) {
        /* EnemyBeamSkeletonCreate (tiles 0x081F427C, palette 0x0820BF54,
         * frames 0x0824F9AC): animation 0 looping and one step; state 4
         * under the global 0x8E & 0x40; otherwise an affine slot, + 0x5C
         * = 0x11, stats, state 0, sub_0806D244 (contact sub_0806E1B8, hit
         * sub_080CB4F0), the floor snap and its patrol: + 0x22 = its room
         * column, + 0x14 / + 0x16 = 64 pixels either side. */
        play(enemy, anims, 0);
        step_anim(enemy, anims);
        if (flag_8e_40) {
            enemy->state = 4;
            return true;
        }
        enemy->state = 0;
        snap_to_floor(enemy, layer);
        enemy->home = (int16_t)(enemy->x >> 16);
        enemy->home_right = (int16_t)(enemy->home + 0x40);
        enemy->home_left = (int16_t)(enemy->home - 0x40);
        return true;
    }
    if (id == AOS_ENEMY_SKULL_MILLIONE || id == AOS_ENEMY_CAGNAZZO || id == AOS_ENEMY_LUBICANT) {
        /* sub_080B7E80 (tiles 0x081F4214, palette 0x0820BCA0, the frames
         * and bank of the variant's record at 0x0852883C): stats, + 0x5C
         * = 4, animation 0 looping and one step, the variant in + 0x14;
         * state 4 (animation only) under the global 0x8E & 0x40, else
         * frame boxes and state 0 (a fin, flags bit 0, is made by the
         * first update). Hit callback sub_080B8004. */
        enemy->variant = id == AOS_ENEMY_SKULL_MILLIONE ? 0 : id == AOS_ENEMY_CAGNAZZO ? 1 : 2;
        play(enemy, anims, 0);
        step_anim(enemy, anims);
        enemy->state = flag_8e_40 ? 4 : 0;
        return true;
    }
    if (id == AOS_ENEMY_DEAD_CRUSADER) {
        /* EnemyDeadCrusaderCreate (sub_0808F8EC: the skeleton knight's
         * tiles 0x081CC00C, palette 0x0820A95C and frames 0x0821FA5C):
         * animation 0 looping and one step, stats, frame boxes, the floor
         * snap, state 0; state 3 (animation only) under the global 0x8E &
         * 0x40. Its hit callback is sub_0808F848. */
        play(enemy, anims, 0);
        step_anim(enemy, anims);
        if (flag_8e_40) {
            enemy->state = 3;
            return true;
        }
        snap_to_floor(enemy, layer);
        enemy->state = 0;
        return true;
    }
    if (id == AOS_ENEMY_SKELETON_KNIGHT) {
        /* EnemySkeletonKnightCreate (sub_0808F120: tiles 0x081CC00C,
         * palette 0x0820A95C, frames 0x0821FA5C): animation 0 looping and
         * one step, frame boxes, the floor snap, state 0; state 3
         * (animation only) under the global 0x8E & 0x40. */
        play(enemy, anims, 0);
        step_anim(enemy, anims);
        if (flag_8e_40) {
            enemy->state = 3;
            return true;
        }
        snap_to_floor(enemy, layer);
        enemy->state = 0;
        return true;
    }
    if (id == AOS_ENEMY_VALKYRIE || id == AOS_ENEMY_ERINYS) {
        /* EnemyValkyrieCreate (sub_0806EB34: tiles 0x081CBF2C, palette
         * 0x0820A3FC bank 0, frames 0x0821285C): animation 0 looping and
         * one step, frame boxes, state 0; state 4 (animation only) under
         * the global 0x8E & 0x40. */
        play(enemy, anims, 0);
        step_anim(enemy, anims);
        enemy->state = flag_8e_40 ? 4 : 0;
        return true;
    }
    if (id == AOS_ENEMY_CURLY || id == AOS_ENEMY_DURGA) {
        /* EnemyCurlyCreate, also EnemyDurgaCreate (sub_08093540: tiles
         * 0x081CC02C, palette 0x0820AA6C bank 1 for Curly, 0 for Durga,
         * frames 0x0822184C): animation 0 looping and
         * one step, frame boxes, the floor snap, state 0 with vy 1.0 and
         * + 0x54 = 0x2800; state 3 (animation only) under the global
         * 0x8E & 0x40. */
        play(enemy, anims, 0);
        step_anim(enemy, anims);
        if (flag_8e_40) {
            enemy->state = 3;
            return true;
        }
        snap_to_floor(enemy, layer);
        enemy->state = 0;
        enemy->vy = 0x10000;
        enemy->ay = 0x2800;
        return true;
    }
    if (id == AOS_ENEMY_GHOST_DANCER) {
        /* EnemyGhostDancerCreate (sub_080C2E74: palette bank = parameter
         * 0): animation 0 looping and one step, state 1, opaque (+ 0x18 =
         * 0); state 5 (animation only) under the global 0x8E & 0x40. */
        play(enemy, anims, 0);
        step_anim(enemy, anims);
        enemy->state = flag_8e_40 ? 5 : 1;
        enemy->phase = 0;
        return true;
    }
    if (id == AOS_ENEMY_GARGOYLE || id == AOS_ENEMY_GREMLIN) {
        /* EnemyGargoyleCreate (sub_08086900 graphics): animation 0 looping
         * and one step, frame boxes, state 0; state 2 (inert) under the
         * global 0x8E & 0x40. */
        play(enemy, anims, 0);
        step_anim(enemy, anims);
        enemy->state = flag_8e_40 ? 2 : 0;
        return true;
    }
    if (id == AOS_ENEMY_ZOMBIE_OFFICER) {
        /* EnemyZombieOfficerCreate (sub_08092680 graphics): walk animation 0
         * and one step, snapped to the floor, state 0; state 3 (inert, no
         * collision block, no snap) under the global 0x8E & 0x40. */
        play(enemy, anims, 0);
        step_anim(enemy, anims);
        if (flag_8e_40) {
            enemy->state = 3;
            return true;
        }
        snap_to_floor(enemy, layer);
        enemy->state = 0;
        return true;
    }
    if (id == AOS_ENEMY_ZOMBIE_SOLDIER) {
        /* EnemyZombieSoldierCreate: walk animation 0, snapped to the floor,
         * state 0; under the global 0x8E & 0x40, state 3 without the
         * collision block or the floor snap. */
        play(enemy, anims, 0);
        step_anim(enemy, anims);
        if (flag_8e_40) {
            enemy->state = 3;
            return true;
        }
        snap_to_floor(enemy, layer);
        enemy->state = 0;
        return true;
    }
    if (id == AOS_ENEMY_BLUE_CROW || id == AOS_ENEMY_RED_CROW) {
        /* EnemyBlueCrowCreate / EnemyRedCrowCreate (palette bank 1):
         * animation 0, state 1, or state 3 without the hit callback under
         * the global 0x8E & 0x40. */
        play(enemy, anims, 0);
        enemy->state = flag_8e_40 ? 3 : 1;
        return true;
    }
    if (id == AOS_ENEMY_GIANT_WORM || id == AOS_ENEMY_POISON_WORM) {
        /* sub_080AD7F0(e, variant): + 0x59 |= 2 | 0x10, a random facing
         * nibble, the sub_080ADB0C floor snap, + 0x14 = itself, four
         * linked segments through sub_0806DFF8 (emitted by the first
         * update, the Headhunter pattern), graphics 0x081E4184, palette
         * 0x0820B724 bank = variant through sub_08068264, frames
         * 0x08237C44, animation 3 (Giant Worm) or 0 (Poison Worm)
         * looping, + 0x21 = variant, + 0x5C = 0x11, state 0 (6 under
         * the global 0x8E & 0x40). The + 0x5A palette merge, the hit
         * callbacks sub_080AE334 / sub_080AE3CC / sub_080AD748 and the
         * sub_0803E058 affine setup are handled by the update. */
        enemy->variant = id == AOS_ENEMY_POISON_WORM ? 1 : 0;
        /* sub_080AE334 / sub_080AE3CC: the contact hit's element word
         * (+ 2 of the sub_08021654 record) — 0 for the Giant Worm,
         * 0x40 (poison) for the Poison Worm. */
        enemy->contact_elements = enemy->variant ? 0x40 : 0;
        enemy->mirrored = (aos_random() & 1) != 0;
        play(enemy, anims, enemy->variant ? 0 : 3);
        step_anim(enemy, anims);
        snap_to_floor(enemy, layer);
        enemy->state = flag_8e_40 ? 6 : 0;
        enemy->step = enemy->substep = 0;
        enemy->timer = 0;
        return true;
    }
    if (id == AOS_ENEMY_MINOTAUR || id == AOS_ENEMY_RED_MINOTAUR) {
        /* sub_080B0AD4 (EnemyMinotaurCreate / EnemyRedMinotaurCreate):
         * tiles 0x081E41A4, palette 0x0820B7B0 bank 0 (brown) or 1 (red,
         * stored in + 0x15), frames 0x0823AE10; + 0x5C = 0x13, animation
         * 1 looping and one step, + 0x18 |= 3, + 0x59 bit 1, state 0;
         * state 4 (animation only) under the global 0x8E & 0x40. The two
         * sub_0806DFF8 helpers (sub_080B02E0 / sub_080B0064), the
         * param0 call sub_08034498 and the hit callbacks sub_080B0CCC /
         * sub_080AFF98 are not ported. */
        play(enemy, anims, 1);
        step_anim(enemy, anims);
        enemy->mino_flags = 3;
        enemy->mino_bank = id == AOS_ENEMY_RED_MINOTAUR ? 1 : 0;
        if (flag_8e_40) {
            enemy->state = 4;
        } else {
            /* sub_0806AF98: the feet onto the floor. */
            snap_to_floor(enemy, layer);
            enemy->state = 0;
        }
        enemy->step = enemy->substep = 0;
        return true;
    }
    if (id == AOS_ENEMY_BIPHRON) {
        /* EnemyBiphronCreate / sub_0809A3D8: graphics 0x081CC05C,
         * palette 0x0820AC04, frames 0x08223FF8; sub_0803F2C8 anim 0
         * looping, sub_0803F17C + sub_0806B04C; the global 0x8E & 0x40
         * selects state 3 (affine only), else the sub_0806D260 param
         * picks sub_080427B8 / sub_0804277C with the sub_0809A424
         * contact (elements 0) and sub_08099F50 hurt callbacks,
         * sub_0806B1FC, the sub_0806AF98 floor snap, state 0. */
        play(enemy, anims, 0);
        step_anim(enemy, anims);
        enemy->contact_elements = 0;
        if (flag_8e_40) {
            enemy->state = 3;
        } else {
            snap_to_floor(enemy, layer);
            enemy->state = 0;
        }
        enemy->step = enemy->substep = 0;
        enemy->timer = 0;
        return true;
    }
    /* EnemyBatCreate: hang from the ceiling above the record; under the
     * global 0x8E & 0x40, state 4 without the ceiling search or the hit
     * callback. */
    play(enemy, anims, 0);
    aos_anim_step(&enemy->anim, anims);
    if (flag_8e_40) {
        enemy->state = 4;
        enemy->substep = enemy->step = 0;
        return true;
    }
    enemy->state = 1;
    for (int guard = 0; guard < 512; ++guard) {
        int depth = aos_ceiling_depth(layer, x, enemy->y >> 16, 0, false);
        if (depth) {
            enemy->y += (int32_t)((uint32_t)depth << 16);
            break;
        }
        enemy->y -= 8 << 16;
    }
    return true;
}


/* sub_0806B1FC: the boxes of the current animation frame. */
static void frame_boxes(const AosEnemy *enemy, const AosEnemyKind *kind, AosBox *hurt,
                        AosBox *attack, bool *hurt_on, bool *attack_on) {
    unsigned a = enemy->anim.id, f = enemy->anim.frame;
    *hurt_on = *attack_on = false;
    if (enemy->own_boxes) {
        *hurt = enemy->own_hurt;
        *attack = enemy->own_attack;
        *hurt_on = *attack_on = true;
        return;
    }
    if (a >= AOS_ENEMY_MAX_ANIMS || f >= AOS_ENEMY_MAX_FRAMES || !kind->modes[a][f]) return;
    *hurt = kind->hurt[a][f];
    *attack = kind->attack[a][f];
    *hurt_on = *attack_on = true;
}

static void beam_shatter(AosEnemy *e, const AosSoma *soma, AosHitReport *report);
static void doll_shatter(AosEnemy *e, AosHitReport *report);

static void skeleton_shatter(AosEnemy *e, const AosSoma *soma, AosHitReport *report);
static void nemesis_struck(AosEnemy *n);
static void peeping_struck(AosEnemy *e, const AosSoma *soma);
static void fish_struck(AosEnemy *f, const AosEnemyKind *kind);
static void dragon_struck(AosEnemy *d, uint8_t attacker);
static void skeleton_merge(AosHitReport *report, const AosHitReport *hit);

/* sub_0806E314 / sub_080421AC: the player against the enemy, then the
 * weapon against the enemy, then the contact callbacks. */
static void worm_segment_hurt(AosEnemy *c, AosEnemy *parent, AosSoma *soma,
                              AosHitReport *report);
AosHitReport aos_enemy_collide(AosEnemy *enemy, AosSoma *soma,
                               const AosEnemyKind *kind,
                               const AosWeaponEntity *weapon,
                               const AosWeaponFrames *weapon_frames,
                               int soma_atk, int soma_def) {
    AosHitReport report = {0};
    AosBox hurt, attack;
    bool hurt_on, attack_on;
    frame_boxes(enemy, kind, &hurt, &attack, &hurt_on, &attack_on);
    int32_t ex = enemy->x >> 16, ey = enemy->y >> 16;
    if (enemy->combat.attack_off || enemy->attack_off) attack_on = false;
    if (enemy->combat.hurt_off) hurt_on = false;
    /* Enemy attack box against the player's hurt rectangle (sub_08041D54). */
    if (attack_on && aos_combat_can_take(&soma->combat, enemy->combat.type)) {
        AosRect player = aos_player_rect(soma->hurtbox, soma->x >> 16, soma->y >> 16,
                                         soma->facing_left, false);
        if (aos_rect_hits_box(player, attack, ex, ey, enemy->mirrored, enemy->vflip)) {
            /* The recent-hit slot uses the attacker's own type (+ 0x70):
             * an axe (type 0xA) is tracked apart from enemy bodies. */
            aos_combat_take(&soma->combat, enemy->combat.type,
                            aos_combat_cooldown(enemy->combat.type, 0));
            report.soma_hit = true;
        }
    }
    /* The weapon's attack box against the enemy's hurtbox (sub_08041864),
     * then the red soul skills' boxes (type 5); both reach sub_0806E218. */
    int wx, wy, ww, wh;
    bool struck = false;
    uint8_t attacker = 0;
    int power = 0;
    uint16_t elements = 0;
    int contact_x = ex, contact_y = ey;
    AosRect target = aos_entity_rect(hurt, ex, ey, enemy->mirrored, enemy->vflip);
    if (hurt_on && weapon && weapon_frames &&
        aos_combat_can_take(&enemy->combat, AOS_TYPE_WEAPON) &&
        aos_weapon_hitbox(weapon, soma, weapon_frames, &wx, &wy, &ww, &wh)) {
        AosRect blade = {(int16_t)wx, (int16_t)wy, (int16_t)(wx + ww - 1), (int16_t)(wy + wh - 1)};
        if (ww > 1 && wh > 1 && blade.x1 <= target.x2 && target.x1 <= blade.x2 &&
            blade.y1 <= target.y2 && target.y1 <= blade.y2) {
            aos_combat_take(&enemy->combat, AOS_TYPE_WEAPON,
                            aos_combat_cooldown(AOS_TYPE_WEAPON, soma->weapon.interval));
            /* sub_0802346C (weapon callback): the kick-hit flag. */
            soma->flags |= AOS_FLAG_KICK_HIT;
            struck = true;
            attacker = AOS_TYPE_WEAPON;
            /* sub_08021530(3) = ATK * 16 / 16. */
            power = soma_atk;
            elements = soma->weapon.flags;
            {
                int x1 = blade.x1 > target.x1 ? blade.x1 : target.x1;
                int x2 = blade.x2 < target.x2 ? blade.x2 : target.x2;
                int y1 = blade.y1 > target.y1 ? blade.y1 : target.y1;
                int y2 = blade.y2 < target.y2 ? blade.y2 : target.y2;
                contact_x = (x1 + x2) / 2;
                contact_y = (y1 + y2) / 2;
            }
        }
    }
    for (int i = 0; hurt_on && !struck && i < soul_attack_count; ++i) {
        AosSoulAttack *attack = &soul_attacks[i];
        uint8_t type = attack->type ? attack->type : AOS_TYPE_SOUL;
        if (!aos_combat_can_take(&enemy->combat, type)) continue;
        if (attack->rect.x1 <= target.x2 && target.x1 <= attack->rect.x2 &&
            attack->rect.y1 <= target.y2 && target.y1 <= attack->rect.y2) {
            aos_combat_take(&enemy->combat, type, aos_combat_cooldown(type, 0));
            struck = true;
            attacker = type;
            attack->struck = true;
            attack->target = enemy;
            {
                int16_t x1 = attack->rect.x1 > target.x1 ? attack->rect.x1 : target.x1;
                int16_t x2 = attack->rect.x2 < target.x2 ? attack->rect.x2 : target.x2;
                int16_t y1 = attack->rect.y1 > target.y1 ? attack->rect.y1 : target.y1;
                int16_t y2 = attack->rect.y2 < target.y2 ? attack->rect.y2 : target.y2;
                if (i < SOUL_ATTACK_CONTACTS) {
                    soul_contact[i][0] = (int16_t)((x1 + x2) / 2);
                    soul_contact[i][1] = (int16_t)((y1 + y2) / 2);
                }
                contact_x = (x1 + x2) / 2;
                contact_y = (y1 + y2) / 2;
            }
            power = attack->power;
            elements = attack->elements;
        }
    }
    {
        if (struck) {
            report.enemy_hit = true;
            if (enemy->role == AOS_ROLE_MIMIC_TRIGGER) {
                /* sub_080993EC: the trigger bursts (state 1, 40 frames)
                 * and wakes its Mimic. */
                enemy->state = 1;
                enemy->step = 0;
                enemy->timer = 0x28;
                report.wake_spawner = true;
                goto contact;
            }
            if (enemy->role == AOS_ROLE_DRYAD_SEED ||
                enemy->role == AOS_ROLE_TSUCHINOKO_SPIT) {
                /* sub_080AEB80 / sub_080A7A90: any hit deletes the
                 * projectile. */
                enemy->removed = true;
                goto contact;
            }
            if (enemy->role == AOS_ROLE_AXE || enemy->role == AOS_ROLE_ARROW ||
                enemy->role == AOS_ROLE_ROCK || enemy->role == AOS_ROLE_FRY ||
                enemy->role == AOS_ROLE_PILLAR_FIREBALL ||
                enemy->role == AOS_ROLE_BOMBER_BOMB ||
                enemy->role == AOS_ROLE_BUTCHER_CLEAVER) {
                /* sub_080B13BC / sub_080AFD8C: a struck axe or arrow loses
                 * its single hit point. */
                enemy->hp = 0;
                goto contact;
            }
            /* sub_080B1370 / sub_080B2F10: the armors turn to face the
             * player when hit. */
            if (enemy->id == AOS_ENEMY_AXE_ARMOR || enemy->id == AOS_ENEMY_ROCK_ARMOR ||
                enemy->id == AOS_ENEMY_BOMBER_ARMOR)
                face_player(enemy, soma);
            if (enemy->role == AOS_ROLE_GRENADE) {
                /* sub_080930E0: a struck grenade bursts harmlessly (sound
                 * 0x76). */
                enemy->state = 1;
                enemy->step = 0;
                enemy->timer = 0;
                goto contact;
            }
            /* sub_0806E218: sub_08021530 for the attacker type. */
            report.enemy_damage = aos_enemy_damage(power, enemy->stats.defence,
                                                   enemy->stats.weak, enemy->stats.resist,
                                                   elements);
            /* sub_0808F848: a guarding dead crusader takes no damage
             * (sound 0x69). */
            if (enemy->id == AOS_ENEMY_DEAD_CRUSADER && enemy->guarding) {
                report.enemy_damage = 0;
                aos_audio_play_song(0x69);
            }
            report.has_damage_popup = true;
            report.damage_popup = aos_damage_popup_create(enemy->x, enemy->y,
                                                          (int16_t)report.enemy_damage, 0);
            if (report.enemy_damage > 0)
                spawn_hit_effect(hit_effect_type(elements, enemy->stats.weak,
                                                  enemy->stats.resist),
                                 contact_x, contact_y, &report);
            enemy->hit_flash = 8;
            enemy->hp = (int16_t)(enemy->hp - report.enemy_damage);
            if (enemy->id == AOS_ENEMY_ECTOPLASM && enemy->role == AOS_ROLE_ENEMY) {
                /* sub_080998E8: knockback on every hit, the death drift
                 * on a lethal one. */
                if (enemy->hp <= 0) report.killed = enemy->defeated = true;
                aos_enemy_ectoplasm_struck(enemy, soma, kind->anims);
                goto contact;
            }
            if (enemy->role == AOS_ROLE_WORM_SEGMENT && enemy->parent &&
                report.enemy_damage > 0) {
                /* sub_080AD748: the segment's hurt callback forwards the
                 * damage to the parent (+ 0x14) — the segment itself
                 * takes none and cannot die. */
                worm_segment_hurt(enemy, enemy->parent, soma, &report);
                enemy->hit_flash = 0;
                goto contact;
            }
            if (enemy->hp > 0 && attacker == AOS_TYPE_WEAPON &&
                (enemy->id == AOS_ENEMY_HARPY || enemy->id == AOS_ENEMY_SIREN)) {
                /* sub_08075CAC: a surviving weapon hit throws the flyer
                 * into its short recoil state. */
                enemy->state = 3;
                enemy->step = enemy->substep = 0;
                enemy->timer = 0;
            }
            if (enemy->hp > 0 && attacker == AOS_TYPE_WEAPON && enemy->id == AOS_ENEMY_VALKYRIE &&
                enemy->state == 1) {
                /* sub_0806EBDC: struck by a weapon (attacker type 3 or 4)
                 * during its attack, it recoils (state 3). */
                play(enemy, kind->anims, 0);
                enemy->state = 3;
                enemy->step = 0;
            }
            if (enemy->hp > 0 && enemy->id == AOS_ENEMY_BUER &&
                enemy->role == AOS_ROLE_ENEMY && enemy->state == 2 && enemy->step == 0) {
                /* sub_080CC514: a surviving blow while it rolls in state 2
                 * (the halfword + 0x0A == 2) starts the attack. */
                enemy->step = 1;
                enemy->substep = 0;
            }
            if (enemy->id == AOS_ENEMY_WHITE_DRAGON && enemy->role == AOS_ROLE_ENEMY) {
                if (enemy->hp <= 0) report.killed = enemy->defeated = true;
                dragon_struck(enemy, attacker);
                goto contact;
            }
            if (enemy->id == AOS_ENEMY_PEEPING_EYE && enemy->role == AOS_ROLE_ENEMY) {
                /* sub_08088638: sub_080683BC at a lethal blow, then its
                 * own reaction. */
                if (enemy->hp <= 0) report.killed = enemy->defeated = true;
                peeping_struck(enemy, soma);
                goto contact;
            }
            if (enemy->id == AOS_ENEMY_NEMESIS && enemy->role == AOS_ROLE_ENEMY) {
                /* sub_080C7604: sub_080683BC at a lethal blow, then its
                 * own reaction. */
                if (enemy->hp <= 0) report.killed = enemy->defeated = true;
                nemesis_struck(enemy);
                goto contact;
            }
            /* The Waiter Skeleton tray's + 0x34 reaching 0 enters its
             * burst state on its next update (sub_080AED28's preamble) —
             * it is not a kill. */
            if (enemy->role == AOS_ROLE_WAITER_TRAY) goto contact;
            if (enemy->hp <= 0) {
                report.killed = true;
                /* sub_080683BC marks it defeated (+ 0x3E bit 1):
                 * sub_0806E314 then skips its collisions. Its experience
                 * reward is applied by the runtime (aos_stats). */
                enemy->defeated = true;
                if (enemy->id == AOS_ENEMY_SKELETON &&
                    enemy->role == AOS_ROLE_ENEMY) {
                    /* sub_080C4624: sound 0x6B, then sub_080C3AB4: the
                     * seven pieces (sub_080C3E18), the reward (the host's)
                     * and 256 hidden frames in state 0xFF. */
                    aos_audio_play_song(0x6B);
                    skeleton_shatter(enemy, soma, &report);
                    enemy->state = 0xFF;
                    enemy->step = 1;
                    enemy->timer = 0xFF;
                } else if (enemy->id == AOS_ENEMY_BUER &&
                           enemy->role == AOS_ROLE_ENEMY) {
                    /* sub_080CC514 marks Buer defeated and dispatches
                     * sub_080CBEA4 in state 3. Its first tick supplies
                     * the sound and death-stage timing, not the callback. */
                    enemy->state = 3;
                    enemy->step = 0;
                    enemy->substep = 0;
                    enemy->timer = 0;
                } else if (enemy->id == AOS_ENEMY_WINGED_SKELETON &&
                           enemy->role == AOS_ROLE_ENEMY) {
                    /* sub_080751B4: the GBA requests 0x6B and enters
                     * state 2 with a 40-frame death timer. */
                    aos_audio_play_song(0x6B);
                    enemy->state = 2;
                    enemy->step = 0;
                    enemy->timer = 0x28;
                } else if (enemy->id == AOS_ENEMY_BAT) {
                    /* sub_080AD6E4: the bat dies. */
                    enemy->state = 3;
                    enemy->step = enemy->substep = 0;
                } else if (enemy->id == AOS_ENEMY_SKULL_ARCHER) {
                    /* sub_080AFD3C: state 3 shatters it on the next update,
                     * its pieces flung away from the attacker (+ 0x20; the
                     * weapon entity follows Soma, whose position stands in
                     * for it). */
                    enemy->away = enemy->x < soma->x ? -1 : 1;
                    enemy->state = 3;
                    enemy->step = enemy->substep = 0;
                } else if (enemy->id == AOS_ENEMY_KILLER_FISH &&
                           enemy->role == AOS_ROLE_ENEMY) {
                    fish_struck(enemy, kind);
                } else if (enemy->id == AOS_ENEMY_AXE_ARMOR ||
                           enemy->id == AOS_ENEMY_ROCK_ARMOR ||
                           enemy->id == AOS_ENEMY_BOMBER_ARMOR) {
                    /* sub_080B1370 / sub_080B2F10: state 2, step 0 (the
                     * palette bits of + 0x5A are not ported). */
                    enemy->state = 2;
                    enemy->step = enemy->substep = 0;
                } else if (enemy->id == AOS_ENEMY_ZOMBIE_SOLDIER) {
                    /* sub_08092B38: sound 0x70, death animation 3 and the
                     * generic death in state 2. */
                    aos_audio_play_song(0x70);
                    play_loop(enemy, kind->anims, 3, false);
                    enemy->timer = 0x28;
                    enemy->state = 2;
                    enemy->step = 0;
                } else if (enemy->id == AOS_ENEMY_MIMIC) {
                    /* sub_0809922C (sound 0x72): animation 3 when struck
                     * closed (step 3), else 2; 40 frames of the generic
                     * death (state 1). */
                    aos_audio_play_song(0x72);
                    play_loop(enemy, kind->anims, enemy->step == 3 ? 3 : 2, false);
                    enemy->static_frame = -1;
                    enemy->own_boxes = false;
                    enemy->timer = 0x28;
                    enemy->state = 1;
                    enemy->step = 0;
                } else if (enemy->id == AOS_ENEMY_MERMAN &&
                           enemy->role == AOS_ROLE_ENEMY) {
                    /* sub_080715B4: sound 0x6E, animation 1, 40-frame
                     * falling death. */
                    aos_audio_play_song(0x6E);
                    play_loop(enemy, kind->anims, 1, false);
                    enemy->timer = 0x28;
                    enemy->state = 2;
                    enemy->step = 0;
                    enemy->vx = enemy->ax = enemy->vy = 0;
                    enemy->ay = 0x2000;
                } else if (enemy->id == AOS_ENEMY_BIPHRON &&
                           enemy->role == AOS_ROLE_ENEMY) {
                    /* sub_08099F50: sub_080683BC at the lethal blow, then
                     * animation 3 one-shot, state 2, step and timer 0
                     * (the + 0x5A palette merge is not ported). */
                    play_loop(enemy, kind->anims, 3, false);
                    enemy->state = 2;
                    enemy->step = enemy->substep = 0;
                    enemy->timer = 0;
                } else if (enemy->id == AOS_ENEMY_SKY_FISH &&
                           enemy->role == AOS_ROLE_ENEMY) {
                    /* sub_080A7FB8: at the lethal blow the palette merge
                     * is skipped, the death animation of table 0x0852840C
                     * plays (approximated by animation 1), + 0x0D = 0x40,
                     * state 1, step 0. */
                    play_loop(enemy, kind->anims, 1, false);
                    enemy->timer = 0x40;
                    enemy->state = 1;
                    enemy->step = 0;
                } else if (enemy->id == AOS_ENEMY_GIANT_GHOST &&
                           enemy->role == AOS_ROLE_ENEMY) {
                    /* sub_0807D88C: its update finishes the death. */
                } else if (enemy->role == AOS_ROLE_BONE_PILLAR_SEGMENT) {
                    /* sub_080B3E70: state 2 (the palette bits are not
                     * ported). */
                    enemy->state = 2;
                    enemy->step = enemy->substep = 0;
                } else if (enemy->id == AOS_ENEMY_GHOST) {
                    /* sub_0808A0C8: a lethal hit plays 0x6E. The native
                     * palette fade and producer accounting remain open. */
                    aos_audio_play_song(0x6E);
                    play_loop(enemy, kind->anims, 1, false);
                    enemy->timer = 8;
                    enemy->state = 1;
                    enemy->step = 0;
                } else if (enemy->id == AOS_ENEMY_RIPPER &&
                           enemy->role == AOS_ROLE_ENEMY) {
                    /* sub_080C2D70 -> sub_080C2A18: state 0xFF. */
                    enemy->state = 0xFF;
                    enemy->step = 0;
                } else if (enemy->id == AOS_ENEMY_FLEAMAN) {
                    /* sub_080C2458 -> sub_080C2388: state 0xFF. */
                    enemy->state = 0xFF;
                    enemy->step = 0;
                } else if (enemy->id == AOS_ENEMY_DEAD_WARRIOR) {
                    /* sub_080B7644: state 1 (the palette bits are not
                     * ported). */
                    enemy->state = 1;
                    enemy->step = enemy->substep = 0;
                } else if (enemy->id == AOS_ENEMY_KILLER_DOLL ||
                           enemy->id == AOS_ENEMY_LIGHTNING_DOLL) {
                    /* sub_080C81AC / sub_080C8FBC (sound 0x6E): hidden,
                     * five pieces counted in + 0x1C, state 0xFF until
                     * they are gone; the Lightning Doll first ends its
                     * spark loop 0x75. */
                    if (enemy->id == AOS_ENEMY_LIGHTNING_DOLL) aos_audio_release(0x75);
                    aos_audio_play_song(0x6E);
                    enemy->state = 0xFF;
                    enemy->step = 1;
                    enemy->hidden = true;
                    doll_shatter(enemy, &report);
                    if (!enemy->spawned_count) enemy->removed = true;
                } else if (enemy->id == AOS_ENEMY_BEAM_SKELETON) {
                    /* sub_080CB4F0 (sound 0x6B) -> sub_080CB144: hidden,
                     * its seven bones thrown (sub_080CB1B4), deleted on its
                     * next update (state 0xFF). */
                    enemy->state = 0xFF;
                    enemy->step = 1;
                    enemy->substep = 0;
                    enemy->timer = 0x32;
                    enemy->hidden = true;
                    beam_shatter(enemy, soma, &report);
                    aos_audio_play_song(0x6B);
                } else if (enemy->id == AOS_ENEMY_SKULL_MILLIONE ||
                           enemy->id == AOS_ENEMY_CAGNAZZO || enemy->id == AOS_ENEMY_LUBICANT) {
                    /* sub_080B8004: state 3 (the palette bits are not
                     * ported). */
                    enemy->state = 3;
                    enemy->step = enemy->substep = 0;
                } else if (enemy->id == AOS_ENEMY_SKELETON_KNIGHT ||
                           enemy->id == AOS_ENEMY_DEAD_CRUSADER) {
                    /* sub_0808F0A4 / sub_0808F848: 40 frames of the
                     * generic death in state 2 (sound 0x72). */
                    aos_audio_play_song(0x72);
                    enemy->timer = 0x28;
                    enemy->state = 2;
                    enemy->step = 0;
                } else if (enemy->id == AOS_ENEMY_VALKYRIE || enemy->id == AOS_ENEMY_ERINYS) {
                    /* sub_0806EBDC: sound 0x1CD, death animation 2, 40
                     * frames of state 2 falling with + 0x54 = 0x1000. */
                    aos_audio_play_song(0x1CD);
                    play_loop(enemy, kind->anims, 2, false);
                    enemy->timer = 0x28;
                    enemy->state = 2;
                    enemy->ax = 0;
                    enemy->ay = 0x1000;
                } else if (enemy->id == AOS_ENEMY_CURLY || enemy->id == AOS_ENEMY_DURGA) {
                    /* sub_080935E8: sound 0x1C3, death animation 2 and the
                     * generic death in state 2. */
                    aos_audio_play_song(0x1C3);
                    play_loop(enemy, kind->anims, 2, false);
                    enemy->timer = 0x28;
                    enemy->state = 2;
                    enemy->step = 0;
                } else if (enemy->id == AOS_ENEMY_GHOST_DANCER) {
                    /* sub_080C32EC (sound 0x6D) -> sub_080C327C: animation
                     * 2 once, semi-transparent, state 3; deleted when the
                     * animation ends. */
                    aos_audio_play_song(0x6D);
                    play_loop(enemy, kind->anims, 2, false);
                    enemy->translucent = true;
                    enemy->state = 3;
                    enemy->step = 1;
                } else if (enemy->id == AOS_ENEMY_GARGOYLE || enemy->id == AOS_ENEMY_GREMLIN) {
                    /* sub_08086878: state 1, 40 frames, falling with
                     * + 0x54 = 0x1000 (sound 0x6E). */
                    aos_audio_play_song(0x6E);
                    enemy->timer = 0x28;
                    enemy->state = 1;
                    enemy->step = 0;
                    enemy->ax = 0;
                    enemy->ay = 0x1000;
                } else if (enemy->id == AOS_ENEMY_ZOMBIE_OFFICER) {
                    /* sub_0809252C: sound 0x70, death animation 2 and the
                     * generic death in state 2. */
                    aos_audio_play_song(0x70);
                    play_loop(enemy, kind->anims, 2, false);
                    enemy->timer = 0x28;
                    enemy->state = 2;
                    enemy->step = 0;
                } else if (enemy->id == AOS_ENEMY_MEDUSA_HEAD) {
                    /* sub_08093AE0: death animation 3, 40 frames of the
                     * generic death in state 2 (sound 0x72). */
                    aos_audio_play_song(0x72);
                    play_loop(enemy, kind->anims, 3, false);
                    enemy->timer = 0x28;
                    enemy->state = 2;
                    enemy->step = 0;
                } else if (enemy->id == AOS_ENEMY_NEEDLES) {
                    /* sub_080CB930: it shatters (state 2). */
                    enemy->state = 2;
                    enemy->step = enemy->substep = 0;
                } else if (enemy->id == AOS_ENEMY_EVIL_BUTCHER &&
                           enemy->role == AOS_ROLE_ENEMY) {
                    /* sub_080BFECC: sub_080683BC at the lethal blow (done
                     * by the caller), then state 2, steps 0 — the + 0x5A
                     * palette bits are not ported. */
                    enemy->state = 2;
                    enemy->step = enemy->substep = 0;
                } else if (enemy->id == AOS_ENEMY_KILLER_MANTLE &&
                           enemy->role == AOS_ROLE_ENEMY) {
                    /* sub_080BB7E4: sub_080683BC at the lethal blow (done
                     * by the caller) and the + 0x5A palette merge (not
                     * ported), then state 3, step and direction 0. */
                    enemy->state = 3;
                    enemy->step = enemy->substep = 0;
                } else if (enemy->id == AOS_ENEMY_DRYAD &&
                           enemy->role == AOS_ROLE_ENEMY) {
                    /* sub_080AE3FC: lethal -> animation 5 when state 1
                     * with step != 0 else animation 4, state 3, timer
                     * 0x28, steps cleared. */
                    play_loop(enemy, kind->anims,
                              enemy->state == 1 && enemy->step ? 5 : 4, false);
                    enemy->timer = 0x28;
                    enemy->state = 3;
                    enemy->step = enemy->substep = 0;
                } else if (enemy->id == AOS_ENEMY_TSUCHINOKO &&
                           enemy->role == AOS_ROLE_ENEMY) {
                    /* sub_080A76E0: sub_080683BC at the lethal blow (done
                     * by the caller), song 0x70, the sub_0803F0E4 death
                     * table 0x085283FC (a custom frame sequence not
                     * ported — the current animation keeps running),
                     * the + 0x5A palette merge (not ported), timer 0x3C,
                     * state 2, step 0. */
                    aos_audio_play_song(0x70);
                    enemy->timer = 0x3C;
                    enemy->state = 2;
                    enemy->step = enemy->substep = 0;
                } else if (enemy->id == AOS_ENEMY_KICKER_SKELETON &&
                           enemy->role == AOS_ROLE_ENEMY) {
                    /* sub_080C0478: only a lethal blow (sub_0806E218's
                     * HP <= 0) reacts — sub_080683BC (done by the
                     * caller), + 0x20 the side away from the attacker,
                     * the + 0x5A palette merge (not ported), state 2. */
                    enemy->away = enemy->x >= soma->x ? 1 : -1;
                    enemy->state = 2;
                    enemy->step = enemy->substep = 0;
                } else if (enemy->id == AOS_ENEMY_WAITER_SKELETON &&
                           enemy->role == AOS_ROLE_ENEMY) {
                    /* sub_080AF6FC: sub_0806E218's damage and hit timer,
                     * sub_080683BC at the lethal blow (done by the
                     * caller), the + 0x5A palette merge (not ported),
                     * timer 0x28, state 3, steps cleared. */
                    enemy->timer = 0x28;
                    enemy->state = 3;
                    enemy->step = enemy->substep = 0;
                } else if (enemy->id == AOS_ENEMY_CHRONOMAGE &&
                           enemy->role == AOS_ROLE_ENEMY && enemy->state != 3) {
                    /* sub_080B72B8: sub_0806E218's damage and hit timer,
                     * sub_080683BC at the lethal blow (done by the
                     * caller), the + 0x5A palette merge from + 0x2C
                     * (not ported), state 3, steps cleared. */
                    enemy->state = 3;
                    enemy->step = enemy->substep = 0;
                } else if (enemy->id == AOS_ENEMY_COCKATRICE &&
                           enemy->role == AOS_ROLE_ENEMY && enemy->state != 2) {
                    /* sub_080BC0B4: sub_0806E218's damage and hit timer,
                     * sub_080683BC at the lethal blow (done by the
                     * caller), the + 0x5A palette merge (not ported),
                     * state 2 with its steps cleared. */
                    enemy->state = 2;
                    enemy->step = enemy->substep = 0;
                } else if (enemy->id == AOS_ENEMY_BLUE_CROW || enemy->id == AOS_ENEMY_RED_CROW) {
                    /* sub_080CA030: the crow dies. */
                    enemy->state = 2;
                    enemy->step = enemy->substep = 0;
                } else if (enemy->id == AOS_ENEMY_HARPY || enemy->id == AOS_ENEMY_SIREN) {
                    /* sub_08075CAC: sound 0x1C3, animation 3 and a 40-frame
                     * falling death, with four fragments emitted by its
                     * updater. */
                    aos_audio_play_song(0x1C3);
                    play_loop(enemy, kind->anims, 3, false);
                    enemy->timer = 0x28;
                    enemy->state = 2;
                    enemy->step = enemy->substep = 0;
                } else if (enemy->id == AOS_ENEMY_GORGON ||
                           enemy->id == AOS_ENEMY_CATOBLEPAS) {
                    /* sub_080896D8: sound 0x71 and a 60-frame death whose
                     * first update emits six native sprite fragments. */
                    aos_audio_play_song(0x71);
                    enemy->timer = 0x3C;
                    enemy->state = 2;
                    enemy->step = enemy->substep = 0;
                } else if (enemy->id == AOS_ENEMY_TINY_DEVIL ||
                           enemy->id == AOS_ENEMY_IMP) {
                    /* sub_080C580C / sub_080C4ED4: both bodies enter the
                     * 50-frame falling death state. Imp additionally emits
                     * its direct-frame child from the state updater. */
                    enemy->timer = 0x32;
                    enemy->state = 0xFF;
                    enemy->step = enemy->substep = 0;
                    enemy->vx = enemy->ax = enemy->ay = 0;
                    enemy->vy = 0x8000;
                } else if (enemy->id == AOS_BOSS_HEADHUNTER) {
                    /* sub_08083AC4: the main body enters sub_08083860. */
                    aos_audio_play_song(0x1C0);
                    enemy->state = 2;
                    enemy->step = enemy->substep = 0;
                    enemy->timer = 0;
                } else {
                    /* sub_0807B0DC: death animation 3, 40 frames (sound 0x70;
                     * sub_080683BC and the gore of sub_0807B258 are not
                     * ported). */
                    play_loop(enemy, kind->anims, 3, false);
                    enemy->timer = 0x28;
                    enemy->state = 1;
                    enemy->step = 0;
                }
            }
        }
    }
contact:
    if (report.soma_hit && enemy->role == AOS_ROLE_ARROW) {
        /* sub_080AF78C: a knockback (type 1); the arrow stops colliding and
         * sticks to Soma for 30 frames at its offset (+ 0x50 / + 0x54). */
        report.soma_damage = aos_soma_take_hit(soma, enemy->stats.contact, soma_def, enemy->x, 1);
        enemy->combat.attack_off = enemy->combat.hurt_off = true;
        enemy->ax = enemy->x - soma->x;
        enemy->ay = enemy->y - soma->y;
        enemy->state = 1;
        enemy->timer = 0x1E;
        return report;
    }
    if (report.soma_hit && enemy->role == AOS_ROLE_GRENADE) {
        /* sub_08093098: element 2, always a knockback (type 1); a flying
         * grenade then bursts (sound 0x76). */
        report.soma_damage = aos_soma_take_elemental_hit(soma, enemy->stats.contact, soma_def,
                                                         enemy->x, 1, 2);
        if (enemy->state == 0) {
            enemy->state = 1;
            enemy->timer = 0;
        }
    } else if (report.soma_hit) {
        /* The contact callbacks sub_0806E1B8, the zombie soldier's
         * sub_0809314C and the axe's sub_0806E1E8 (element 1) ->
         * sub_08021654 (type 0). */
        /* sub_0806ECA8: the Valkyrie's contact is halved outside frames 4
         * to 6 of its attack animation (state 1, animation 1). */
        int contact = enemy->stats.contact;
        /* sub_080BFF7C: while + 0x14 & 1 (the strike) the power is
         * + 0x3C * 5 / 4 and the hit is type 3 (the sub_08021248 script
         * slot is not ported). */
        if (enemy->id == AOS_ENEMY_KICKER_SKELETON && (enemy->kicker_flags & 1))
            contact = contact + contact / 4;
        if ((enemy->id == AOS_ENEMY_VALKYRIE || enemy->id == AOS_ENEMY_ERINYS) &&
            !(enemy->state == 1 && enemy->anim.id == 1 && (uint8_t)(enemy->anim.frame - 4) <= 2))
            contact >>= 1;
        /* sub_080994B0 (the Mimic) and sub_08093BF0 (a Medusa Head) are
         * knockbacks (type 1); the elements are the callback's
         * (contact_elements). */
        bool knockback = enemy->id == AOS_ENEMY_MIMIC || enemy->role == AOS_ROLE_ROCK ||
                         (enemy->id == AOS_ENEMY_BUER && enemy->role == AOS_ROLE_ENEMY) ||
                         enemy->role == AOS_ROLE_FRY || enemy->role == AOS_ROLE_DRAGON_FIREBALL ||
                         enemy->role == AOS_ROLE_PILLAR_BREATH ||
                         enemy->role == AOS_ROLE_PILLAR_FIREBALL ||
                         (enemy->id == AOS_ENEMY_WHITE_DRAGON && enemy->role == AOS_ROLE_ENEMY) ||
                         (enemy->id == AOS_ENEMY_MEDUSA_HEAD && enemy->role == AOS_ROLE_ENEMY);
        report.soma_damage = aos_soma_take_elemental_hit(soma, contact, soma_def, enemy->x,
                                                         knockback ? 1 : 0,
                                                         enemy->contact_elements);
    }
    if (report.soma_hit && enemy->role == AOS_ROLE_WAITER_TRAY) {
        /* sub_080AF740 / sub_080AF784: the type-0 contact sets the
         * parent's + 0x20 bit 3 (the palette bytes match — both share
         * the kind) and zeroes + 0x34: it bursts on its next update. */
        if (enemy->parent) enemy->parent->ws_flags |= 8;
        enemy->hp = 0;
    }
    return report;
}
#define collide aos_enemy_collide

void aos_enemy_accel_move(AosEnemy *e, int32_t limit);
#define accel_move aos_enemy_accel_move

/* sub_0806AEAC: the generic death, a flame every 4 frames over 16 x 12
 * pixels, the blink, then the deletion. */
void aos_enemy_generic_death(AosEnemy *enemy, const AosEnemyKind *kind,
                             uint32_t (*random)(void),
                             AosHitReport *report) {
    if ((enemy->timer & 3) == 0) spawn_flame(enemy, 0x100000, 0xC0000, random, report);
    enemy->hidden = enemy->timer <= 0x27 ? kind->blink[enemy->timer] & 1 : false;
    if ((int8_t)--enemy->timer <= 0) enemy->removed = true;
}
#define generic_death aos_enemy_generic_death

/* sub_08069A00 with the zombie's arguments: X then Y motion with the walls
 * at x +/- 8, y - 10, the ceiling at y - 32 and the floor; returns 1 / 2
 * for a wall on the right / left, 8 for a ceiling, 4 for a landing. */
int aos_enemy_walk_collide(AosEnemy *e, const AosCollision *layer,
                           int32_t bounce_vy, int32_t head,
                           int32_t max_speed, int half_width) {
    int result = 0;
    if (e->vx || e->ax) {
        /* sub_0806D430 */
        e->vx += e->ax;
        if (max_speed) {
            if (e->ax >= 0 ? e->vx > max_speed : e->vx < -max_speed)
                e->vx = e->ax >= 0 ? max_speed : -max_speed;
        }
        e->x += e->vx;
        int32_t x = e->x >> 16, y = (e->y >> 16) - 10;
        if (e->vx < 0) {
            int push = aos_wall_push_right(layer, x - half_width, y);
            if (push) {
                e->x += (int32_t)((uint32_t)push << 16);
                result = 2;
            }
        }
        if (e->vx > 0) {
            int push = aos_wall_push_left(layer, x + half_width, y);
            if (push) {
                e->x += (int32_t)((uint32_t)push << 16);
                result |= 1;
            }
        }
    }
    if (e->vy || e->ay) {
        /* sub_0806D460 */
        e->vy += e->ay;
        if (max_speed) {
            if (e->ay >= 0 ? e->vy > max_speed : e->vy < -max_speed)
                e->vy = e->ay >= 0 ? max_speed : -max_speed;
        }
        e->y += e->vy;
        int32_t x = e->x >> 16;
        if (e->vy < 0) {
            int32_t head_y = e->y + head;
            int depth = aos_ceiling_depth(layer, x, head_y < 0 ? -((-head_y) >> 16) : head_y >> 16,
                                          0, false);
            if (depth) {
                e->y += (int32_t)((uint32_t)depth << 16);
                result |= 8;
            }
        }
        if (e->vy > 0) {
            int depth = aos_floor_depth(layer, x, e->y >> 16, 0, false);
            if (depth) {
                e->y = (int32_t)(((uint32_t)((e->y >> 16) + depth)) << 16);
                e->vy = bounce_vy;
                result |= 4;
            }
        }
    }
    return result;
}
#define walk_collide aos_enemy_walk_collide

/* sub_0806CFFC: the player inside a box given in room pixels. */
bool aos_enemy_player_near(const AosSoma *soma, int32_t x, int32_t y,
                           int w, int h) {
    return player_in(soma, x, y, w, h);
}
#define player_near aos_enemy_player_near

/* sub_08092FCC(e, dx, dy, vx, vy, frame): a gib (EntityCreateInRange(0x49,
 * 0xCF, sub_08093078)) showing a sprite frame of the soldier, at its
 * position + (dx, dy), mirrored like it, falling with ay 0x1800; it keeps
 * the soldier's y (+ 0x1C) and is deleted once lower. Priority - 1. */
AosEnemy aos_enemy_make_gib(const AosEnemy *z, int32_t dx, int32_t dy,
                            int32_t vx, int32_t vy, int16_t frame) {
    AosEnemy g = {0};
    g.id = z->id;
    g.role = AOS_ROLE_GIB;
    g.static_frame = frame;
    g.x = z->x + dx;
    g.y = z->y + dy;
    g.walk_timer = z->y;
    g.mirrored = z->mirrored;
    g.ay = 0x1800;
    g.vx = vx;
    g.vy = vy;
    g.combat.attack_off = g.combat.hurt_off = true;
    return g;
}
#define make_gib aos_enemy_make_gib

/* sub_0806BF78: the player within `range` of the entity on both axes. */
static bool player_within(const AosEnemy *e, const AosSoma *soma, uint32_t range) {
    int32_t dx = e->x - target_x(soma), dy = e->y - target_y(soma);
    return (uint32_t)(dx < 0 ? -dx : dx) <= range && (uint32_t)(dy < 0 ? -dy : dy) <= range;
}

void aos_enemy_integrate(AosEnemy *e);
void aos_enemy_stop(AosEnemy *e);
#define integrate aos_enemy_integrate
#define stop aos_enemy_stop

/* sub_0806BD64: (s16)(integer x - the target's integer x). */
static int16_t player_dx(const AosEnemy *enemy, const AosSoma *soma) {
    return (int16_t)((enemy->x >> 16) - (target_x(soma) >> 16));
}

/* sub_0806BAC8: the angle from the entity to its target, whole pixels. */
static uint16_t angle_to_target(const AosEnemy *e, const AosSoma *soma) {
    return aos_arctan2((int16_t)((target_x(soma) >> 16) - (e->x >> 16)),
                       (int16_t)((target_y(soma) >> 16) - (e->y >> 16)));
}

/* sub_080686CC: the 16-bit angle turned by `step` the short way toward
 * `target`; true once it is there (the last step snaps onto it). */
static int turn_side(uint16_t a, uint16_t target) {
    if (a == target) return 0;
    return (uint16_t)(a - target) > (uint16_t)(target - a) ? 1 : -1;
}

static bool turn_angle(uint32_t *angle, uint16_t step, uint16_t target) {
    uint16_t a = (uint16_t)*angle;
    int side = turn_side(a, target);
    if (side == 0) return true;
    a = (uint16_t)(side > 0 ? a + step : a - step);
    *angle = a;
    if (turn_side(a, target) == side) return false;
    *angle = target;
    return true;
}

void aos_enemy_integrate(AosEnemy *e) {
    e->x += e->vx;
    e->y += e->vy;
    e->vx += e->ax;
    e->vy += e->ay;
}

void aos_enemy_stop(AosEnemy *e) {
    e->vx = e->vy = e->ax = e->ay = 0;
}

/* sub_08069770: a projectile's motion with the point probes at its position:
 * X (walls on either side, by the sign of vx), then Y (the ceiling at
 * y + head and the floor, which sets vy to floor_vy). Returns 1 / 2 for a
 * wall on the right / left, 8 for a ceiling, 4 for the floor. */
static int projectile_collide(AosEnemy *e, const AosCollision *layer, int32_t floor_vy,
                              int32_t head) {
    int result = 0;
    /* sub_0806D430(e, 8.0) */
    e->vx += e->ax;
    if (e->ax >= 0 ? e->vx > 0x80000 : e->vx < -0x80000) e->vx = e->ax >= 0 ? 0x80000 : -0x80000;
    e->x += e->vx;
    if (e->vx <= 0) {
        int push = aos_wall_push_right(layer, e->x >> 16, e->y >> 16);
        if (push) {
            e->x += (int32_t)((uint32_t)push << 16);
            result = 2;
        }
    }
    if (e->vx >= 0) {
        int push = aos_wall_push_left(layer, e->x >> 16, e->y >> 16);
        if (push) {
            e->x += (int32_t)((uint32_t)push << 16);
            result |= 1;
        }
    }
    /* sub_0806D460(e, 8.0) */
    e->vy += e->ay;
    if (e->ay >= 0 ? e->vy > 0x80000 : e->vy < -0x80000) e->vy = e->ay >= 0 ? 0x80000 : -0x80000;
    e->y += e->vy;
    int32_t head_y = e->y + head;
    int depth = aos_ceiling_depth(layer, e->x >> 16,
                                  head_y < 0 ? -((-head_y) >> 16) : head_y >> 16, 0, false);
    if (depth) {
        e->y += (int32_t)((uint32_t)depth << 16);
        result |= 8;
    }
    depth = aos_floor_depth(layer, e->x >> 16, e->y >> 16, 0, false);
    if (depth) {
        e->y += (int32_t)((uint32_t)depth << 16);
        e->vy = floor_vy;
        result |= 4;
    }
    return result;
}

/* sub_0806D3D8(e, limit): velocity plus acceleration, clamped toward the
 * acceleration's sign, then the move. */
void aos_enemy_accel_move(AosEnemy *e, int32_t limit) {
    e->vx += e->ax;
    if (e->ax >= 0 ? e->vx > limit : e->vx < -limit) e->vx = e->ax >= 0 ? limit : -limit;
    e->vy += e->ay;
    if (e->ay >= 0 ? e->vy > limit : e->vy < -limit) e->vy = e->ay >= 0 ? limit : -limit;
    e->x += e->vx;
    e->y += e->vy;
}

/* sub_080689F8 / sub_0806CEF8(e, range): the player's distance (whole
 * pixels, through Sqrt) within `range` (16.16). */
static bool player_in_range(const AosEnemy *e, const AosSoma *soma, uint32_t range) {
    int32_t dx = (e->x - target_x(soma)) >> 16, dy = (e->y - target_y(soma)) >> 16;
    return (uint32_t)aos_isqrt((uint32_t)(dx * dx + dy * dy)) << 16 <= range;
}

/* sub_080696B0(e, tx, ty, speed): v = delta * speed / distance (whole
 * pixels), then the move; true once the distance is within `speed`. A
 * zero distance (a division by zero natively) leaves it still. */
static bool head_toward(AosEnemy *e, int32_t tx, int32_t ty, int32_t speed) {
    int32_t dx = pixel_delta(tx, e->x), dy = pixel_delta(ty, e->y);
    int32_t distance = (int32_t)aos_isqrt((uint32_t)(dx * dx + dy * dy));
    e->vx = distance ? dx * speed / distance : 0;
    e->vy = distance ? dy * speed / distance : 0;
    e->x += e->vx;
    e->y += e->vy;
    return (distance << 16) <= speed;
}

/* sub_0806AC5C: a walking speed halved (slope type 1) or cut to 2/3 (type
 * 2) when climbing the slope under the entity (the cell at its feet, or
 * one pixel lower when empty; bit 2 the slope's direction). */
int32_t aos_enemy_slope_speed(const AosEnemy *e, const AosCollision *layer,
                              int32_t speed) {
    int32_t x = e->x >> 16, y = e->y >> 16;
    uint8_t cell = aos_collision_cell(layer, x, y);
    if (!cell) cell = aos_collision_cell(layer, x, y + 1);
    int type = (cell & 0xC0) >> 6;
    if (!type) return speed;
    bool uphill = e->mirrored ? (cell & 4) != 0 : (cell & 4) == 0;
    if (!uphill) return speed;
    if (type == 1) return speed / 2;
    if (type == 2) return speed * 2 / 3;
    return speed;
}
#define slope_speed aos_enemy_slope_speed

/* sub_0806B1A0: no floor 8 pixels below an empty cell ahead (a drop), or a
 * wall ahead that is not a slope. */
bool aos_enemy_blocked_ahead(const AosCollision *layer, int32_t x, int32_t y) {
    if (!aos_collision_cell(layer, x, y)) {
        if (!aos_collision_cell(layer, x, y + 8)) return true;
        return aos_collision_cell(layer, x, y - 7) != 0;
    }
    uint8_t above = aos_collision_cell(layer, x, y - 7);
    return above && !(above & 0xC0);
}
#define blocked_ahead aos_enemy_blocked_ahead

/* The room width in cell columns as sub_0806D188 / sub_0806B120 count it
 * (30 for a one-screen room). */
static int room_columns(const AosCollision *layer) {
    return layer->width_cells == 32 ? 0x1E : layer->width_cells;
}

/* sub_0806D188(e, 1): the entity's cell column outside [1, width - 1]
 * (a one-screen room counts 30 columns). */
bool aos_enemy_at_room_edge(const AosEnemy *e, const AosCollision *layer) {
    int32_t column = (e->x >> 16) >> 3;
    int width = room_columns(layer);
    return column < 1 || column > width - 1;
}
#define at_room_edge aos_enemy_at_room_edge

/* sub_0806B120(e, margin): an entity beyond `margin` columns of either
 * room edge is put back on that column boundary (whole pixel); true when
 * it was. */
static bool clamp_to_room(AosEnemy *e, const AosCollision *layer, int margin) {
    int32_t column = (e->x >> 16) >> 3;
    int width = room_columns(layer);
    if (column < margin) {
        e->x = (int32_t)((uint32_t)(margin * 8) << 16);
        return true;
    }
    if (column > width - margin) {
        e->x = (int32_t)((uint32_t)((width - margin + 1) * 8) << 16);
        return true;
    }
    return false;
}

int aos_enemy_ground_walk(AosEnemy *e, const AosSoma *soma, const AosCollision *layer,
                          const AosProbes *p, int32_t speed, int mode);
#define ground_walk aos_enemy_ground_walk

/* sub_0806E29C(e, w, h): the player in the w x h box centred on it. */
bool aos_enemy_player_in_centred(const AosEnemy *e, const AosSoma *soma, int w, int h) {
    return player_in(soma, (int16_t)((e->x >> 16) - w / 2), (int16_t)((e->y >> 16) - h / 2), w, h);
}
#define player_in_centred aos_enemy_player_in_centred

/* sub_080698A8(e, floor_vy, head, limit): sub_08069770 with its own limit,
 * moving each axis only when its speed or acceleration is set; the ceiling
 * only while rising, the floor only while falling (subpixel cleared, vy =
 * floor_vy). */
int aos_enemy_fall_collide(AosEnemy *e, const AosCollision *layer,
                           int32_t floor_vy, int32_t head, int32_t limit) {
    int result = 0;
    if (e->vx || e->ax) {
        e->vx += e->ax;
        if (e->ax >= 0 ? e->vx > limit : e->vx < -limit) e->vx = e->ax >= 0 ? limit : -limit;
        e->x += e->vx;
        if (e->vx < 0) {
            int push = aos_wall_push_right(layer, e->x >> 16, e->y >> 16);
            if (push) {
                e->x += (int32_t)((uint32_t)push << 16);
                result = 2;
            }
        }
        if (e->vx > 0) {
            int push = aos_wall_push_left(layer, e->x >> 16, e->y >> 16);
            if (push) {
                e->x += (int32_t)((uint32_t)push << 16);
                result |= 1;
            }
        }
    }
    if (!e->vy && !e->ay) return result;
    e->vy += e->ay;
    if (e->ay >= 0 ? e->vy > limit : e->vy < -limit) e->vy = e->ay >= 0 ? limit : -limit;
    e->y += e->vy;
    if (e->vy < 0) {
        int32_t head_y = e->y + head;
        int depth = aos_ceiling_depth(layer, e->x >> 16,
                                      head_y < 0 ? -((-head_y) >> 16) : head_y >> 16, 0, false);
        if (depth) {
            e->y += (int32_t)((uint32_t)depth << 16);
            result |= 8;
        }
    }
    if (e->vy > 0) {
        int depth = aos_floor_depth(layer, e->x >> 16, e->y >> 16, 0, false);
        if (depth) {
            e->y = (int32_t)((uint32_t)((e->y >> 16) + depth) << 16);
            e->vy = floor_vy;
            result |= 4;
        }
    }
    return result;
}
#define fall_collide aos_enemy_fall_collide

/* sub_0806C828: the probe walker. X first: vx += ax (clamped to 8.0), then
 * the wall probes at the position before the move, half_width to the side
 * of the motion, at each wall_y offset (the first push wins: 1 on the left,
 * 2 on the right). Then Y: vy += ay (clamped), and the ceiling probe when
 * rising (4, vy = 0) or the floor probe (8, vy = 0), which, while vy has no
 * integer part, looks `snap` pixels lower. A floor hit records the cell
 * byte (+ 0x3F); on a slope byte it adds 0x10 and 0x40 when moving down the
 * slope (0x20 otherwise); with mode bit 0 a second floor probe at the
 * unmoved height also reports a slope (0x10). Mode bit 1 (one-way
 * platforms) is not ported. */
static int probe_walk(AosEnemy *e, const AosCollision *layer, const AosProbes *p, int snap,
                      int mode) {
    int result = 0;
    int32_t px = e->x >> 16, py = e->y >> 16;
    e->vx += e->ax;
    if (e->ax >= 0 ? e->vx > 0x80000 : e->vx < -0x80000) e->vx = e->ax >= 0 ? 0x80000 : -0x80000;
    e->x += e->vx;
    for (int i = 0; i < p->count && i < 8 && e->vx; ++i) {
        int push = e->vx < 0 ? aos_wall_push_right(layer, px - p->half_width, py + p->wall_y[i])
                             : aos_wall_push_left(layer, px + p->half_width, py + p->wall_y[i]);
        if (push) {
            e->x += (int32_t)((uint32_t)push << 16);
            result |= e->vx < 0 ? 1 : 2;
            break;
        }
    }
    e->vy += e->ay;
    if (e->ay >= 0 ? e->vy > 0x80000 : e->vy < -0x80000) e->vy = e->ay >= 0 ? 0x80000 : -0x80000;
    e->y += e->vy;
    px = e->x >> 16;
    py = e->y >> 16;
    e->ground = 0;
    if (e->vy < 0) {
        int depth = aos_ceiling_depth(layer, px, py + p->ceiling, 0, false);
        if (depth) {
            e->y += (int32_t)((uint32_t)depth << 16);
            e->vy = 0;
            result |= 4;
        }
        return result;
    }
    int extra = (int16_t)(e->vy >> 16) == 0 ? snap : 0;
    uint8_t raw = aos_collision_cell(layer, px, py + p->floor + extra);
    int depth = aos_floor_depth(layer, px, py + p->floor + extra, 0, false);
    (void)mode;
    if (!depth) return result;
    e->y += (int32_t)((uint32_t)(depth + extra) << 16);
    e->vy = 0;
    e->ground = raw;
    result |= 8;
    if (raw & 0xC0) {
        result |= 0x10;
        result |= ((e->vx > 0 && (raw & 4)) || (e->vx < 0 && !(raw & 4))) ? 0x40 : 0x20;
    }
    if ((mode & 1) && aos_floor_depth(layer, px, py + p->floor, 0, false) &&
        (aos_collision_cell(layer, px, py + p->floor) & 0xC0))
        result |= 0x10;
    return result;
}

/* sub_0806CAF8: a walker step at `speed` toward the facing (mode bit 1:
 * the raw sign), facing the player first at the start of frame 0 (bit 0),
 * slowed on slopes (bit 3); the step is undone on a wall (keeping the
 * vertical move on a floor), on a slope with bit 4, and without a floor
 * with bit 2. Returns the probe walker's flags. */
int aos_enemy_ground_walk(AosEnemy *e, const AosSoma *soma, const AosCollision *layer,
                          const AosProbes *p, int32_t speed, int mode) {
    int32_t x0 = e->x, y0 = e->y;
    if ((mode & 1) && e->anim.frame == 0 && e->anim.tick == 0) face_player(e, soma);
    e->vx = (!(mode & 2) && !e->mirrored) ? -speed : speed;
    if ((mode & 8) && (e->ground & 0xC0) &&
        ((e->vx > 0 && (e->ground & 4)) || (e->vx < 0 && !(e->ground & 4)))) {
        int steep = e->ground >> 6;
        if (steep == 1) e->vx /= 2;
        else if (steep == 2) e->vx = e->vx / 3 * 2;
    }
    int result = probe_walk(e, layer, p, 4, ((mode & 0x10) ? 1 : 0) | ((mode & 0x20) ? 2 : 0));
    if (result & 3) {
        e->x = x0;
        if (!(result & 8)) e->y = y0;
    } else if ((mode & 0x10) && (result & 0x10)) {
        e->x = x0;
        e->y = y0;
    }
    if ((mode & 4) && !(result & 8)) {
        e->x = x0;
        e->y = y0;
    }
    return result;
}

int aos_enemy_probe_walk(AosEnemy *e, const AosCollision *layer, const AosProbes *p,
                         int32_t speed, int mode) {
    return ground_walk(e, NULL, layer, p, speed, mode & ~1);
}

/* sub_0806BBC4 and sub_0806D044: the player ahead (by the facing) within
 * `range` pixels (the BIOS Sqrt of the squared whole-pixel distance). */
static bool player_ahead_within(const AosEnemy *e, const AosSoma *soma, int range) {
    int side = e->x < target_x(soma) ? 1 : e->x > target_x(soma) ? -1 : 0;
    if (side != (e->mirrored ? 1 : -1)) return false;
    int32_t dx = pixel_delta(target_x(soma), e->x), dy = pixel_delta(target_y(soma), e->y);
    return (int16_t)aos_isqrt((uint32_t)(dx * dx + dy * dy)) <= range;
}

void aos_enemy_copy_collision_result(AosHitReport *report,
                                     const AosHitReport *hit) {
    report->soma_hit = hit->soma_hit;
    report->soma_damage = hit->soma_damage;
    report->enemy_hit = hit->enemy_hit;
    report->enemy_damage = hit->enemy_damage;
    report->has_damage_popup = hit->has_damage_popup;
    report->damage_popup = hit->damage_popup;
    report->killed = hit->killed;
}
#define copy_collision_result aos_enemy_copy_collision_result

/* sub_0806D0B8(x, y): 2 (3 with bit 3) in a solid cell, else 0 above a
 * cell with bits 0-1 (a floor) and 1 in the air. */
static int cell_kind(const AosCollision *layer, int32_t x, int32_t y) {
    uint8_t c = aos_collision_cell(layer, x, y);
    if (c) return (c & 8) ? 3 : 2;
    return (aos_collision_cell(layer, x, y + 1) & 3) ? 0 : 1;
}

/* The whole-pixel part of a 16.16 value, rounded toward zero (the native
 * absolute-value shifts). */
static int32_t whole(int32_t v) { return v < 0 ? -((-v) >> 16) : v >> 16; }

/* sub_0806E5FC(x, y): how many pixels (0-7) below (x, y) the water starts,
 * skipping solid cells; 8 when it does not. */
static int water_below(const AosCollision *layer, int x, int y) {
    for (int d = 0; d < 8; ++d)
        if (cell_kind(layer, x, y + d) == 3) return d;
    return 8;
}

static void clamped_add(int32_t *v, int32_t a, int32_t limit) {
    *v += a;
    if (a >= 0 ? *v > limit : *v < -limit) *v = a >= 0 ? limit : -limit;
}

/* The water flags of the swimmers: 0x80 with the head (y + head) in water,
 * 0x40 with the feet. */
static int water_flags(const AosEnemy *e, const AosCollision *layer, int32_t head) {
    int x = e->x >> 16, result = 0;
    if (cell_kind(layer, x, whole(e->y + head)) == 3) result |= 0x80;
    if (cell_kind(layer, x, e->y >> 16) == 3) result |= 0x40;
    return result;
}

/* sub_08069CF4(e, half width, head, mode, limit): the x move (sub_0806D430)
 * with a wall `half` pixels ahead (2 left, 1 right) and a ceiling push at
 * that point, the y move (sub_0806D460), with mode a ceiling at y + head
 * (8), falling a floor (4), then the water flags. */
static int swim_walk(AosEnemy *e, const AosCollision *layer, int half, int32_t head, int mode,
                     int32_t limit) {
    int result = 0;
    clamped_add(&e->vx, e->ax, limit);
    e->x += e->vx;
    int x = e->x >> 16, y = e->y >> 16, px;
    if (e->vx <= 0) {
        int push = aos_wall_push_right(layer, x - half, y);
        if (push) {
            result |= 2;
            e->x += (int32_t)((uint32_t)push << 16);
        }
        px = (e->x >> 16) - half;
    } else {
        int push = aos_wall_push_left(layer, x + half, y);
        if (push) {
            result |= 1;
            e->x += (int32_t)((uint32_t)push << 16);
        }
        px = (e->x >> 16) + half;
    }
    int depth = aos_ceiling_depth(layer, px, e->y >> 16, 0, false);
    if (depth) e->y += (int32_t)((uint32_t)depth << 16);
    clamped_add(&e->vy, e->ay, limit);
    e->y += e->vy;
    if (mode) {
        depth = aos_ceiling_depth(layer, e->x >> 16, whole(e->y + head), 0, false);
        if (depth) {
            result |= 8;
            e->y += (int32_t)((uint32_t)depth << 16);
        }
    }
    if (e->vy > 0) {
        depth = aos_floor_depth(layer, e->x >> 16, e->y >> 16, 0, false);
        if (depth) {
            result |= 4;
            e->y += (int32_t)((uint32_t)depth << 16);
        }
    }
    return result | water_flags(e, layer, head);
}

/* sub_08069B70(e, -, head, mode): the same with an 8.0 limit, the walls at
 * the entity itself and the floor tested whatever its vy. */
static int bubble_walk(AosEnemy *e, const AosCollision *layer, int32_t head, int mode) {
    int result = 0;
    clamped_add(&e->vx, e->ax, 0x80000);
    e->x += e->vx;
    int x = e->x >> 16, y = e->y >> 16;
    if (e->vx < 0) {
        int push = aos_wall_push_right(layer, x, y);
        if (push) {
            result |= 2;
            e->x += (int32_t)((uint32_t)push << 16);
        }
    } else if (e->vx > 0) {
        int push = aos_wall_push_left(layer, x, y);
        if (push) {
            result |= 1;
            e->x += (int32_t)((uint32_t)push << 16);
        }
    }
    clamped_add(&e->vy, e->ay, 0x80000);
    e->y += e->vy;
    if (mode) {
        int depth = aos_ceiling_depth(layer, e->x >> 16, whole(e->y + head), 0, false);
        if (depth) {
            result |= 8;
            e->y += (int32_t)((uint32_t)depth << 16);
        }
    }
    int depth = aos_floor_depth(layer, e->x >> 16, e->y >> 16, 0, false);
    if (depth) {
        result |= 4;
        e->y += (int32_t)((uint32_t)depth << 16);
    }
    return result | water_flags(e, layer, head);
}

/* sub_0806A224(e, 0, -4, -4, 8, 8, limit), the fry's box walker: walls at
 * the box's left or right edge (both corners), ceiling or floor at its top
 * or bottom edge (both corners), the water flag 0x80 for its top-left
 * corner. The corner-specific flags 0x100-0x800 and 0x10-0x40 are not
 * modelled (the fry reads only 0x80). */
static int box_walk(AosEnemy *e, const AosCollision *layer, int32_t limit) {
    enum { BX = -4, BY = -4, BW = 8, BH = 8 };
    int result = 0;
    clamped_add(&e->vx, e->ax, limit);
    e->x += e->vx;
    for (int corner = 0; corner < 2; ++corner) {
        int x = e->x >> 16, y = (e->y >> 16) + BY + corner * BH;
        int push = e->vx <= 0 ? aos_wall_push_right(layer, x + BX, y)
                              : aos_wall_push_left(layer, x + BX + BW, y);
        if (push) {
            e->x += (int32_t)((uint32_t)push << 16);
            result |= e->vx <= 0 ? 2 : 1;
        }
    }
    clamped_add(&e->vy, e->ay, limit);
    e->y += e->vy;
    for (int corner = 0; corner < 2; ++corner) {
        int x = (e->x >> 16) + BX + corner * BW, y = e->y >> 16;
        if (e->vy <= 0) {
            int depth = aos_ceiling_depth(layer, x, y + BY, 0, false);
            if (depth) {
                e->y += (int32_t)((uint32_t)depth << 16);
                result |= 8;
            }
        } else {
            int depth = aos_floor_depth(layer, x, y + BY + BH, 0, false);
            if (depth) {
                e->y += (int32_t)((uint32_t)depth << 16);
                result |= 4;
            }
        }
    }
    if (cell_kind(layer, (e->x >> 16) + BX, (e->y >> 16) + BY) == 3) result |= 0x80;
    return result;
}

/* sub_080693E4(e, tx, ty, speed): the velocity toward (tx, ty) at `speed`
 * (whole-pixel deltas over the BIOS Sqrt distance); true once within it. */
static bool aim_at(AosEnemy *e, int32_t tx, int32_t ty, int32_t speed) {
    int32_t dx = whole(tx - e->x), dy = whole(ty - e->y);
    int32_t distance = (int32_t)(uint16_t)aos_isqrt((uint32_t)(dx * dx + dy * dy));
    e->vx = distance ? dx * speed / distance : 0;
    e->vy = distance ? dy * speed / distance : 0;
    return (distance << 16) <= speed;
}

/* The target of the swimmers is in water (sub_0806D0B8 == 3), when there
 * is one (gEwramData + 0x13168). */
static bool target_in_water(const AosSoma *soma, const AosCollision *layer) {
    return !no_target && cell_kind(layer, target_x(soma) >> 16, target_y(soma) >> 16) == 3;
}

void aos_enemy_merge_hit(AosHitReport *report, AosHitReport hit) {
    copy_collision_result(report, &hit);
}
#define merge_enemy_hit aos_enemy_merge_hit

/* The monsters, one file each in aos_enemies/ (included here: they share
 * this file's static framework and helpers in one translation unit). */
#include "aos_enemies/headhunter.c"
#include "aos_enemies/zombie.c"
#include "aos_enemies/nemesis.c"
#include "aos_enemies/crow.c"
#include "aos_enemies/zombie_soldier.c"
#include "aos_enemies/valkyrie.c"
#include "aos_enemies/skeleton_knight.c"
#include "aos_enemies/killer_doll.c"
#include "aos_enemies/lightning_doll.c"
#include "aos_enemies/beam_skeleton.c"
#include "aos_enemies/skull_millione.c"
#include "aos_enemies/dead_crusader.c"
#include "aos_enemies/gargoyle.c"
#include "aos_enemies/axe_armor.c"
#include "aos_enemies/rock_armor.c"
#include "aos_enemies/skull_archer.c"
#include "aos_enemies/skeleton.c"
#include "aos_enemies/killer_fish.c"
#include "aos_enemies/winged_skeleton.c"
#include "aos_enemies/harpy.c"
#include "aos_enemies/tiny_devil.c"
#include "aos_enemies/ripper.c"
#include "aos_enemies/merman.c"
#include "aos_enemies/peeping_eye.c"
#include "aos_enemies/buer.c"
#include "aos_enemies/white_dragon.c"
#include "aos_enemies/bone_pillar.c"
#include "aos_enemies/giant_ghost.c"
#include "aos_enemies/minotaur.c"
#include "aos_enemies/worm.c"
#include "aos_enemies/biphron.c"
#include "aos_enemies/bomber_armor.c"
#include "aos_enemies/evil_butcher.c"
#include "aos_enemies/killer_mantle.c"
#include "aos_enemies/dryad.c"
#include "aos_enemies/tsuchinoko.c"
#include "aos_enemies/kicker_skeleton.c"
#include "aos_enemies/waiter_skeleton.c"
#include "aos_enemies/cockatrice.c"
#include "aos_enemies/chronomage.c"

static bool early_enemy_inert(const AosEnemy *e) {
    if (e->id == AOS_ENEMY_SKELETON && e->state == 5)
        return true;
    if (e->id == AOS_ENEMY_WINGED_SKELETON && e->state == 4)
        return true;
    return e->id == AOS_ENEMY_GHOST && e->state == 2;
}

/* First observable loops for the remaining constructor-table enemies. The
 * Ghost has its complete specialized path above. This stops at the boundary
 * documented in the parity rows: linked multipart bodies and enemy-specific
 * attacks are not treated as ported by this shared path. */
static AosHitReport early_enemy_update(AosEnemy *e, AosSoma *soma,
                                       const AosCollision *layer, const AosEnemyKind *kind,
                                       const AosWeaponEntity *weapon,
                                       const AosWeaponFrames *weapon_frames, int soma_atk,
                                       int soma_def, int cam_x, int cam_y,
                                       uint32_t (*random)(void)) {
    AosHitReport report = {0};
    (void)layer;
    if (e->id == AOS_ENEMY_WINGED_SKELETON && e->state == 3) {
        /* EnemyWingedSkeletonUpdate state 3: one-in-32 spawn attempt,
         * capped by the record's parameter 0. Spawned children remain
         * owned by this room producer, which decrements on their removal. */
        if ((random() & 31) == 0 && e->param0 > 0 &&
            e->spawned_count < e->param0) {
            int sx = (target_x(soma) >> 16) - cam_x;
            int sy = (int)(soma->vy >> 16);
            report.spawn_x = cam_x + (sx <= 31 ? 256 : sx > 208 ? -16 :
                                      ((random() & 1) ? 256 : -16));
            report.spawn_y = cam_y + (sy < 0 ? (int)(random() % 0x50) - 0x40 :
                                      sy > 0 ? (int)(random() & 0x3F) + 0x90 :
                                               (int)(random() & 0x3F) + 0x30);
            report.spawn_param1 = 0;
            report.spawn = true;
            ++e->spawned_count;
        }
        return report;
    }
    if (e->defeated) {
        generic_death(e, kind, random, &report);
        aos_combat_tick(&e->combat);
        return report;
    }
    if (early_enemy_inert(e)) {
        if (e->static_frame < 0) step_anim(e, kind->anims);
        return report;
    }
    if (e->hidden) return report;
    int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;

    if (e->static_frame < 0) step_anim(e, kind->anims);
    sx = (e->x >> 16) - cam_x;
    sy = (e->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0) {
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        merge_enemy_hit(&report, hit);
    }
    aos_combat_tick(&e->combat);
    return report;
}

static AosHitReport enemy_update(AosEnemy *enemy, AosSoma *soma, const AosCollision *layer,
                                 const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                 const AosWeaponFrames *weapon_frames, int soma_atk,
                                 int soma_def, int cam_x, int cam_y, uint32_t (*random)(void));

/* The enemies register themselves in the activity check most of them run
 * (sub_0806CC20); here every active enemy that is not rewarded does, which
 * approximates the native call sites. */
AosHitReport aos_enemy_update(AosEnemy *enemy, AosSoma *soma, const AosCollision *layer,
                              const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                              const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
                              int cam_x, int cam_y, uint32_t (*random)(void)) {
    aim_self = enemy;
    AosHitReport report = enemy_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk,
                                       soma_def, cam_x, cam_y, random);
    aim_self = NULL;
    int sx = (enemy->x >> 16) - cam_x, sy = (enemy->y >> 16) - cam_y;
    /* sub_080B3DCC puts only the topmost live Bone Pillar skull in the
     * homing list (sub_08021278), whatever its screen position. */
    if (enemy->role == AOS_ROLE_BONE_PILLAR_SEGMENT) return report;
    report.targetable = enemy->role == AOS_ROLE_ENEMY && !enemy->removed && !enemy->defeated &&
                        !enemy->hidden && enemy->hp > 0 && (uint16_t)(sx + 0x10) <= 0x110 &&
                        (uint16_t)sy <= 0xB0;
    return report;
}

static AosHitReport enemy_update(AosEnemy *enemy, AosSoma *soma, const AosCollision *layer,
                                 const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                 const AosWeaponFrames *weapon_frames, int soma_atk,
                                 int soma_def, int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0};
    const AosAnimSet *anims = kind->anims;
    if (enemy->removed) return report;
    if (enemy->role == AOS_ROLE_WORM_SEGMENT)
        return worm_segment_update(enemy, soma, layer, kind, weapon, weapon_frames,
                                   soma_atk, soma_def, random);
    if ((enemy->id == AOS_ENEMY_GIANT_WORM || enemy->id == AOS_ENEMY_POISON_WORM) &&
        enemy->role == AOS_ROLE_ENEMY)
        return worm_update(enemy, soma, layer, kind, weapon, weapon_frames,
                           soma_atk, soma_def, random);
    if (enemy->role == AOS_ROLE_BIPHRON_SHOT)
        return biphron_shot_update(enemy, soma, layer, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
    if (enemy->role == AOS_ROLE_DRYAD_SEED)
        return dryad_seed_update(enemy, soma, layer, kind, weapon, weapon_frames,
                                 soma_atk, soma_def, cam_x, cam_y, random);
    if (enemy->role == AOS_ROLE_TSUCHINOKO_SPIT ||
        enemy->role == AOS_ROLE_TSUCHINOKO_ROCK)
        return tsuchinoko_child_update(enemy, soma, layer, kind, weapon,
                                       weapon_frames, soma_atk, soma_def,
                                       cam_x, cam_y, random);
    if (enemy->id == AOS_ENEMY_TSUCHINOKO && enemy->role == AOS_ROLE_ENEMY)
        return tsuchinoko_update(enemy, soma, layer, kind, weapon,
                                 weapon_frames, soma_atk, soma_def, cam_x,
                                 cam_y, random);
    if (enemy->id == AOS_ENEMY_KICKER_SKELETON && enemy->role == AOS_ROLE_ENEMY)
        return kicker_skeleton_update(enemy, soma, layer, kind, weapon,
                                      weapon_frames, soma_atk, soma_def,
                                      cam_x, cam_y, random);
    if (enemy->role == AOS_ROLE_WAITER_COMPANION) {
        waiter_companion_update(enemy, kind);
        return report;
    }
    if (enemy->role == AOS_ROLE_WAITER_TRAY)
        return waiter_tray_update(enemy, soma, layer, kind, weapon,
                                  weapon_frames, soma_atk, soma_def, cam_x,
                                  cam_y, random);
    if (enemy->id == AOS_ENEMY_WAITER_SKELETON && enemy->role == AOS_ROLE_ENEMY)
        return waiter_update(enemy, soma, layer, kind, weapon, weapon_frames,
                             soma_atk, soma_def, cam_x, cam_y, random);
    if (enemy->role == AOS_ROLE_COCKATRICE_SHADOW) {
        cockatrice_shadow_update(enemy, kind);
        return report;
    }
    if (enemy->role == AOS_ROLE_COCKATRICE_ROCK)
        return cockatrice_rock_update(enemy, soma, layer, kind, weapon,
                                      weapon_frames, soma_atk, soma_def,
                                      cam_x, cam_y, random);
    if (enemy->id == AOS_ENEMY_COCKATRICE && enemy->role == AOS_ROLE_ENEMY)
        return cockatrice_update(enemy, soma, layer, kind, weapon,
                                 weapon_frames, soma_atk, soma_def, cam_x,
                                 cam_y, random);
    if (enemy->role == AOS_ROLE_CHRONO_WISP) {
        chrono_wisp_update(enemy, kind, random);
        return report;
    }
    if (enemy->id == AOS_ENEMY_CHRONOMAGE && enemy->role == AOS_ROLE_ENEMY)
        return chronomage_update(enemy, soma, layer, kind, weapon,
                                 weapon_frames, soma_atk, soma_def, cam_x,
                                 cam_y, random);
    if (enemy->id == AOS_ENEMY_DRYAD && enemy->role == AOS_ROLE_ENEMY)
        return dryad_update(enemy, soma, layer, kind, weapon, weapon_frames,
                            soma_atk, soma_def, cam_x, cam_y, random);
    if (enemy->id == AOS_ENEMY_BIPHRON && enemy->role == AOS_ROLE_ENEMY)
        return biphron_update(enemy, soma, layer, kind, weapon, weapon_frames,
                              soma_atk, soma_def, random);
    if (enemy->role == AOS_ROLE_HEADHUNTER_HEAD) {
        headhunter_head_update(enemy, kind, random);
        return report;
    }
    if (enemy->id == AOS_BOSS_HEADHUNTER && enemy->role == AOS_ROLE_ENEMY) {
        if (enemy->state == 2) return headhunter_death(enemy, kind, cam_y);
        return headhunter_update(enemy, soma, kind, weapon, weapon_frames,
                                 soma_atk, soma_def, cam_x, cam_y);
    }
    if (enemy->role == AOS_ROLE_SKELETON_BONE)
        return skeleton_bone_update(enemy, soma, kind, weapon, weapon_frames,
                                    soma_atk, soma_def, cam_x, cam_y);
    if (enemy->role == AOS_ROLE_SKELETON_HEAD) {
        skeleton_head_update(enemy, kind);
        return report;
    }
    if (enemy->role == AOS_ROLE_SKELETON_HELD) {
        skeleton_held_update(enemy, kind);
        return report;
    }
    if (enemy->role == AOS_ROLE_WINGED_BONE)
        return winged_bone_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk,
                                  soma_def, cam_x, cam_y);
    if (enemy->role == AOS_ROLE_WINGED_PIECE) {
        winged_piece_update(enemy, kind);
        return report;
    }
    if (enemy->id == AOS_ENEMY_WINGED_SKELETON && enemy->role == AOS_ROLE_ENEMY &&
        enemy->state != 3)
        return winged_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk, soma_def,
                             cam_x, cam_y, random);
    if (enemy->id == AOS_ENEMY_SKELETON && enemy->role == AOS_ROLE_ENEMY)
        return skeleton_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk,
                               soma_def, cam_x, cam_y, random);
    if (enemy->role == AOS_ROLE_GIANT_GHOST_PART) {
        giant_part_update(enemy, soma, kind, random, &report);
        return report;
    }
    if (enemy->role == AOS_ROLE_GIANT_GHOST_TRAIL) {
        /* sub_0807E1B0: off the screen rows hidden, animated, 5 frames. */
        enemy->hidden = (uint16_t)((enemy->y >> 16) - cam_y + 0x1C) > 0xDC;
        step_anim(enemy, kind->anims);
        if ((int8_t)enemy->timer-- <= 0) enemy->removed = true;
        return report;
    }
    if (enemy->id == AOS_ENEMY_GIANT_GHOST && enemy->role == AOS_ROLE_ENEMY)
        return giant_update(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def, cam_x,
                            cam_y, random);
    if (enemy->role == AOS_ROLE_WHITE_DRAGON_PART) {
        dragon_segment_update(enemy, kind, random, &report);
        return report;
    }
    if (enemy->role == AOS_ROLE_DRAGON_FIREBALL)
        return dragon_fireball_update(enemy, soma, kind, soma_def, random);
    if (enemy->role == AOS_ROLE_DRAGON_FLAME) {
        /* sub_08077BB0: flips at random for 10 frames. */
        enemy->mirrored = random() & 1;
        enemy->vflip = random() & 1;
        if (enemy->timer++ > 8) enemy->removed = true;
        return report;
    }
    if (enemy->role == AOS_ROLE_DRAGON_SPARK) {
        /* sub_080779E4: flips at random, drawn into the mouth by 1 / (8 -
         * + 0x0D) of the way a frame, gone after 9 frames. */
        enemy->mirrored = random() & 1;
        enemy->vflip = random() & 1;
        int left = 8 - enemy->timer;
        if (left) {
            enemy->x += (enemy->target_x - enemy->x) / left;
            enemy->y += (enemy->target_y - enemy->y) / left;
        }
        if (enemy->timer++ > 8) enemy->removed = true;
        return report;
    }
    if (enemy->role == AOS_ROLE_DRAGON_PIECE) {
        /* sub_08077BFC: falls (sub_0806D3D8, 7.0), blinking below 40. */
        accel_move(enemy, 0x70000);
        enemy->hidden = enemy->timer < 40 ? kind->blink[enemy->timer] & 1 : false;
        if ((int8_t)enemy->timer-- <= 0) enemy->removed = true;
        return report;
    }
    if (enemy->id == AOS_ENEMY_WHITE_DRAGON && enemy->role == AOS_ROLE_ENEMY)
        return dragon_update(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def, cam_x,
                             cam_y, random);
    if (enemy->role == AOS_ROLE_BUER_PROJECTILE) {
        buer_burst_update(enemy, kind);
        return report;
    }
    if (enemy->role == AOS_ROLE_BUER_FIRE) {
        buer_fire_update(enemy);
        return report;
    }
    if (enemy->id == AOS_ENEMY_BUER && enemy->role == AOS_ROLE_ENEMY)
        return buer_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk, soma_def,
                           cam_x, cam_y, random);
    if (enemy->role == AOS_ROLE_BONE_PILLAR_SEGMENT)
        return pillar_skull_update(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def,
                                   random);
    if (enemy->id == AOS_ENEMY_BONE_PILLAR && enemy->role == AOS_ROLE_ENEMY)
        return pillar_update(enemy, kind, cam_x, cam_y);
    if (enemy->role == AOS_ROLE_PILLAR_PIECE) {
        /* sub_080B3EE8. */
        enemy->vx = enemy->vx * 39 / 40;
        accel_move(enemy, 0x80000);
        if (outside_margins(kind, 4, (enemy->x >> 16) - cam_x, (enemy->y >> 16) - cam_y))
            enemy->removed = true;
        return report;
    }
    if (enemy->role == AOS_ROLE_PILLAR_SMOKE) {
        accel_move(enemy, 0x80000);
        if (enemy->timer) enemy->timer--;
        else enemy->removed = true;
        return report;
    }
    if (enemy->role == AOS_ROLE_PILLAR_BREATH) {
        /* sub_080B33C8 state 1: the attack from frame 4; gone when the
         * animation ends or its skull shatters. */
        const AosEnemy *skull = enemy->parent;
        if (enemy->state == 0) {
            play_loop(enemy, kind->anims, 4, false);
            enemy->state = 1;
        }
        if (enemy->anim.tick == 0 && enemy->anim.frame == 4) enemy->combat.attack_off = false;
        report = collide(enemy, soma, kind, NULL, NULL, 0, soma_def);
        aos_combat_tick(&enemy->combat);
        if (aos_anim_step(&enemy->anim, kind->anims) == 3 || !skull || skull->removed ||
            skull->state == 2)
            enemy->removed = true;
        return report;
    }
    if (enemy->role == AOS_ROLE_PILLAR_FIREBALL) {
        /* sub_080B350C state 1. */
        if (enemy->state == 0) {
            play(enemy, kind->anims, 5);
            enemy->state = 1;
        }
        if ((++enemy->timer & 7) == 0) {
            int32_t x = AOS_FIXED((enemy->x >> 16) + random_centred(random, 4));
            int32_t y = AOS_FIXED((enemy->y >> 16) + random_centred(random, 8));
            int32_t v = enemy->vx + random_centred(random, 0x8000);
            v = -v;
            pillar_smoke(enemy, x, y, (v < 0 ? v + 3 : v) >> 2, -0x1000, -0x100, 0, random,
                         &report);
        }
        accel_move(enemy, 0x80000);
        AosHitReport hit = collide(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        skeleton_merge(&report, &hit);
        aos_combat_tick(&enemy->combat);
        step_anim(enemy, kind->anims);
        if (enemy->hp <= 0) {
            spawn_flame_at(enemy->x, enemy->y, 0, 0, random, &report);
            enemy->removed = true;
        }
        if (outside_margins(kind, 1, (enemy->x >> 16) - cam_x, (enemy->y >> 16) - cam_y))
            enemy->removed = true;
        return report;
    }
    if (enemy->role == AOS_ROLE_PEEPING_SEGMENT) {
        peeping_part_update(enemy, kind, soma, cam_x, cam_y, random, &report);
        return report;
    }
    if (enemy->id == AOS_ENEMY_PEEPING_EYE && enemy->role == AOS_ROLE_ENEMY)
        return peeping_update(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def, cam_x,
                              cam_y, random);
    if (enemy->id == AOS_ENEMY_GHOST && enemy->role == AOS_ROLE_ENEMY)
        return aos_enemy_ghost_update(enemy, soma, kind, weapon,
                                      weapon_frames, soma_atk, soma_def,
                                      cam_x, cam_y, random);
    if ((enemy->id == AOS_ENEMY_MINOTAUR || enemy->id == AOS_ENEMY_RED_MINOTAUR) &&
        enemy->role == AOS_ROLE_ENEMY)
        return minotaur_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk,
                               soma_def, cam_x, cam_y, random);
    if (is_early_enemy(enemy->id) && enemy->role == AOS_ROLE_ENEMY)
        return early_enemy_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk,
                                  soma_def, cam_x, cam_y, random);
    if (enemy->role == AOS_ROLE_HARPY_SHOT || enemy->role == AOS_ROLE_SIREN_SHOT)
        return harpy_shot_update(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def,
                                 cam_x, cam_y);
    if (enemy->role == AOS_ROLE_GORGON_BREATH)
        return aos_enemy_gorgon_breath_update(
            enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def,
            cam_x, cam_y);
    if (enemy->role == AOS_ROLE_GORGON_DEBRIS) {
        falling_fragment_update(enemy, cam_x, cam_y);
        return report;
    }
    if (enemy->role == AOS_ROLE_TINY_DEVIL_ORB)
        return tiny_devil_orb_update(enemy, soma, kind, weapon, weapon_frames, soma_atk,
                                     soma_def, cam_x, cam_y);
    if (enemy->role == AOS_ROLE_IMP_HELPER) {
        imp_helper_update(enemy, soma, kind);
        return report;
    }
    if (enemy->role == AOS_ROLE_IMP_DEATH) {
        imp_death_child_update(enemy, kind, cam_y);
        return report;
    }
    if (enemy->role == AOS_ROLE_MERMAN_SHOT)
        return merman_shot_update(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def,
                                  cam_x, cam_y, random);
    if (enemy->role == AOS_ROLE_RIPPER_BLADE)
        return ripper_blade_update(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def,
                                   cam_x, cam_y);
    if ((enemy->id == AOS_ENEMY_HARPY || enemy->id == AOS_ENEMY_SIREN) &&
        enemy->role == AOS_ROLE_ENEMY)
        return harpy_update(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def, cam_x,
                            cam_y, random);
    if ((enemy->id == AOS_ENEMY_GORGON || enemy->id == AOS_ENEMY_CATOBLEPAS) &&
        enemy->role == AOS_ROLE_ENEMY)
        return aos_enemy_gorgon_update(enemy, soma, layer, kind, weapon,
                                       weapon_frames, soma_atk, soma_def,
                                       cam_x, cam_y, random);
    if ((enemy->id == AOS_ENEMY_TINY_DEVIL || enemy->id == AOS_ENEMY_IMP) &&
        enemy->role == AOS_ROLE_ENEMY)
        return devil_update(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def, cam_x,
                            cam_y, random);
    if (enemy->id == AOS_ENEMY_MERMAN && enemy->role == AOS_ROLE_ENEMY)
        return merman_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk, soma_def,
                             cam_x, cam_y, random);
    if (enemy->id == AOS_ENEMY_RIPPER && enemy->role == AOS_ROLE_ENEMY)
        return ripper_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk, soma_def,
                             cam_x, cam_y, random);
    if ((enemy->id == AOS_ENEMY_BLUE_CROW || enemy->id == AOS_ENEMY_RED_CROW) &&
        enemy->role == AOS_ROLE_ENEMY)
        return crow_update(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def, random);
    if (enemy->role == AOS_ROLE_MIMIC_TRIGGER)
        return aos_enemy_mimic_trigger_update(enemy, soma, kind, weapon,
                                              weapon_frames, soma_atk,
                                              soma_def, random);
    if (enemy->role == AOS_ROLE_DOLL_PIECE) {
        doll_piece_update(enemy, soma, layer, kind, cam_y, random, &report);
        return report;
    }
    if (enemy->role == AOS_ROLE_NEMESIS_HELPER) {
        nemesis_helper_update(enemy);
        return report;
    }
    if (enemy->role == AOS_ROLE_NEMESIS_EFFECT) {
        nemesis_effect_update(enemy, kind);
        return report;
    }
    if (enemy->id == AOS_ENEMY_KILLER_DOLL && enemy->role == AOS_ROLE_ENEMY)
        return killer_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk, soma_def,
                             random);
    if (enemy->id == AOS_ENEMY_NEMESIS && enemy->role == AOS_ROLE_ENEMY)
        return nemesis_update(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def,
                              cam_x, cam_y, random);
    if (enemy->id == AOS_ENEMY_LIGHTNING_DOLL && enemy->role == AOS_ROLE_ENEMY)
        return doll_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk, soma_def,
                           random);
    if (enemy->role == AOS_ROLE_BEAM || enemy->role == AOS_ROLE_BEAM_FLASH)
        return beam_update(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    if (enemy->id == AOS_ENEMY_BEAM_SKELETON && enemy->role == AOS_ROLE_ENEMY)
        return beam_skeleton_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk,
                                    soma_def, cam_x, cam_y, random);
    if (enemy->role == AOS_ROLE_FIN) {
        fin_update(enemy, kind, cam_x, cam_y, random);
        return report;
    }
    if ((enemy->id == AOS_ENEMY_SKULL_MILLIONE || enemy->id == AOS_ENEMY_CAGNAZZO ||
         enemy->id == AOS_ENEMY_LUBICANT) && enemy->role == AOS_ROLE_ENEMY)
        return skull_update(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def, cam_x,
                            cam_y, random);
    if (enemy->id == AOS_ENEMY_MIMIC)
        return aos_enemy_mimic_update(enemy, soma, layer, kind, weapon,
                                      weapon_frames, soma_atk, soma_def,
                                      cam_x, cam_y, random);
    if (enemy->id == AOS_ENEMY_FLEAMAN)
        return aos_enemy_fleaman_update(enemy, soma, layer, kind, weapon,
                                        weapon_frames, soma_atk, soma_def,
                                        random);
    if (enemy->id == AOS_ENEMY_MEDUSA_HEAD && enemy->role == AOS_ROLE_ENEMY)
        return aos_enemy_medusa_head_update(enemy, soma, kind, weapon,
                                            weapon_frames, soma_atk, soma_def,
                                            cam_x, cam_y, random);
    if (enemy->id == AOS_ENEMY_GHOST_DANCER)
        return aos_enemy_ghost_dancer_update(enemy, soma, kind, weapon,
                                             weapon_frames, soma_atk, soma_def,
                                             cam_x, cam_y);
    if (enemy->id == AOS_ENEMY_ZOMBIE && enemy->role == AOS_ROLE_ENEMY)
        return zombie_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk, soma_def,
                             cam_x, cam_y, random);
    if (enemy->hit_flash) enemy->hit_flash--;
    if (enemy->role == AOS_ROLE_DRIP) {
        drip_update(enemy);
        return report;
    }
    if (enemy->role == AOS_ROLE_PUDDLE) {
        puddle_update(enemy);
        return report;
    }
    if (enemy->role == AOS_ROLE_FEATHER) {
        feather_update(enemy);
        return report;
    }
    if (enemy->role == AOS_ROLE_GIB) {
        gib_update(enemy);
        return report;
    }
    if (enemy->role == AOS_ROLE_NEEDLE_PIECE) {
        aos_enemy_needle_piece_update(enemy, cam_x, cam_y);
        return report;
    }
    if (enemy->id == AOS_ENEMY_NEEDLES)
        return aos_enemy_needles_update(enemy, soma, kind, weapon,
                                        weapon_frames, soma_atk, soma_def,
                                        cam_x, cam_y, random);
    if (enemy->role == AOS_ROLE_DEBRIS) {
        debris_update(enemy, layer, kind, cam_x, cam_y, random, &report);
        if (!enemy->removed) step_anim(enemy, anims);
        return report;
    }
    if (enemy->role == AOS_ROLE_ARROW)
        return arrow_update(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def, cam_x,
                            cam_y, random);
    if (enemy->id == AOS_ENEMY_SKULL_ARCHER)
        return archer_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk, soma_def,
                             cam_x, cam_y, random);
    if (enemy->role == AOS_ROLE_AXE)
        return axe_update(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def, cam_x,
                          cam_y, random);
    if (enemy->role == AOS_ROLE_FRY)
        return fry_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk, soma_def,
                          cam_x, cam_y, random);
    if (enemy->role == AOS_ROLE_BUBBLE) {
        bubble_update(enemy, layer);
        return report;
    }
    if (enemy->role == AOS_ROLE_BUBBLE_POP) {
        bubble_pop_update(enemy);
        return report;
    }
    if (enemy->id == AOS_ENEMY_KILLER_FISH && enemy->role == AOS_ROLE_ENEMY)
        return fish_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk, soma_def,
                           cam_x, cam_y, random);
    if (enemy->role == AOS_ROLE_ROCK)
        return rock_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk, soma_def,
                           cam_x, cam_y, random);
    if (enemy->role == AOS_ROLE_ROCK_PIECE) {
        rock_piece_update(enemy, layer, kind, cam_x, cam_y, random);
        return report;
    }
    if (enemy->role == AOS_ROLE_BUTCHER_CLEAVER)
        return butcher_cleaver_update(enemy, soma, layer, kind, weapon, weapon_frames,
                                      soma_atk, soma_def, cam_x, cam_y, random);
    if (enemy->role == AOS_ROLE_BUTCHER_GORE)
        return butcher_gore_update(enemy, soma, layer, kind, weapon, weapon_frames,
                                   soma_atk, soma_def, cam_x, cam_y, random);
    if (enemy->role == AOS_ROLE_BUTCHER_PARTICLE) {
        butcher_particle_update(enemy, kind);
        return report;
    }
    if (enemy->id == AOS_ENEMY_EVIL_BUTCHER && enemy->role == AOS_ROLE_ENEMY)
        return butcher_update(enemy, soma, layer, kind, weapon, weapon_frames,
                              soma_atk, soma_def, cam_x, cam_y, random);
    if (enemy->role == AOS_ROLE_MANTLE_ORBIT) {
        mantle_orbit_update(enemy, enemy->parent, random);
        return report;
    }
    if (enemy->role == AOS_ROLE_MANTLE_GHOST) {
        mantle_ghost_update(enemy, enemy->parent, kind->anims);
        return report;
    }
    if (enemy->id == AOS_ENEMY_KILLER_MANTLE && enemy->role == AOS_ROLE_ENEMY)
        return killer_mantle_update(enemy, soma, layer, kind, weapon,
                                    weapon_frames, soma_atk, soma_def, cam_x,
                                    cam_y, random);
    if (enemy->role == AOS_ROLE_BOMBER_BOMB)
        return bomber_bomb_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk,
                                  soma_def, cam_x, cam_y, random);
    if (enemy->role == AOS_ROLE_BOMBER_SPARK) {
        bomber_spark_update(enemy, kind);
        return report;
    }
    if (enemy->id == AOS_ENEMY_BOMBER_ARMOR && enemy->role == AOS_ROLE_ENEMY)
        return bomber_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk,
                             soma_def, cam_x, cam_y, random);
    if (enemy->id == AOS_ENEMY_SKY_FISH && enemy->role == AOS_ROLE_ENEMY)
        return aos_enemy_sky_fish_update(enemy, soma, kind, weapon,
                                         weapon_frames, soma_atk, soma_def,
                                         cam_x, cam_y, random);
    if (enemy->id == AOS_ENEMY_ECTOPLASM && enemy->role == AOS_ROLE_ENEMY)
        return aos_enemy_ectoplasm_update(enemy, soma, kind, weapon,
                                          weapon_frames, soma_atk, soma_def,
                                          cam_x, cam_y, random);
    if (enemy->id == AOS_ENEMY_ROCK_ARMOR && enemy->role == AOS_ROLE_ENEMY)
        return rock_armor_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk,
                                 soma_def, cam_x, cam_y, random);
    if (enemy->id == AOS_ENEMY_AXE_ARMOR)
        return armor_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk, soma_def,
                            cam_x, cam_y, random);
    if (enemy->role == AOS_ROLE_GRENADE)
        return grenade_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk, soma_def,
                              cam_x, cam_y, random);
    if ((enemy->id == AOS_ENEMY_VALKYRIE || enemy->id == AOS_ENEMY_ERINYS) &&
        enemy->role == AOS_ROLE_ENEMY)
        return valkyrie_update(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def,
                               cam_x, cam_y, random);
    if (enemy->id == AOS_ENEMY_DEAD_WARRIOR)
        return aos_enemy_dead_warrior_update(enemy, soma, layer, kind, weapon,
                                             weapon_frames, soma_atk, soma_def,
                                             cam_x, cam_y, random);
    if (enemy->id == AOS_ENEMY_DEAD_CRUSADER)
        return crusader_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk,
                               soma_def, cam_x, cam_y, random);
    if (enemy->id == AOS_ENEMY_SKELETON_KNIGHT)
        return knight_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk, soma_def,
                             cam_x, cam_y, random);
    if (enemy->id == AOS_ENEMY_CURLY || enemy->id == AOS_ENEMY_DURGA)
        return aos_enemy_curly_update(enemy, soma, layer, kind, weapon,
                                      weapon_frames, soma_atk, soma_def,
                                      cam_x, cam_y, random);
    if (enemy->id == AOS_ENEMY_GARGOYLE || enemy->id == AOS_ENEMY_GREMLIN)
        return gargoyle_update(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def,
                               cam_x, cam_y, random);
    if (enemy->id == AOS_ENEMY_ZOMBIE_OFFICER)
        return aos_enemy_zombie_officer_update(
            enemy, soma, layer, kind, weapon, weapon_frames, soma_atk,
            soma_def, cam_x, cam_y, random);
    if (enemy->id == AOS_ENEMY_ZOMBIE_SOLDIER)
        return soldier_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk, soma_def,
                              cam_x, cam_y, random);
    if (enemy->state == 4) {
        /* sub_080AD6AC: animation 1 (unless playing), stepped each frame;
         * no activity window and no collision pass. */
        if (enemy->step == 0) {
            if (enemy->anim.id != 1) play(enemy, anims, 1);
            enemy->step = 1;
            enemy->substep = 0;
        }
        step_anim(enemy, anims);
        return report;
    }
    if (enemy->state == 3) {
        bat_die(enemy, kind, random, &report);
        aos_combat_tick(&enemy->combat);
        return report;
    }
    /* sub_0806CC20: the AI only runs within the activity window. */
    int sx = (enemy->x >> 16) - cam_x, sy = (enemy->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
    switch (enemy->state) {
    case 1: bat_hang(enemy, soma, anims, random); break;
    case 2: bat_swoop(enemy, soma, anims, cam_x); break;
    default:
        break;
    }
    /* sub_0806DF20: the animation step. */
    step_anim(enemy, anims);
    /* sub_0806E314: on screen and not defeated, the collision pass. */
    sx = (enemy->x >> 16) - cam_x;
    sy = (enemy->y >> 16) - cam_y;
    if (!enemy->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
        report = collide(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    /* sub_080426B0 after the entity update. */
    aos_combat_tick(&enemy->combat);
    return report;
}
