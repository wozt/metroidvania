/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies; comments name the cvaos routine. */
#include "aos_enemy.h"

#include "aos_audio.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>


static uint32_t random_state;

uint32_t aos_random(void) {
    return random_state = (random_state >> 8) * 0x3243F6ADu + 0x1B0CB175u;
}

void aos_random_seed(uint32_t seed) {
    random_state = seed;
}

static bool flag_8e_40;

/* The red soul skills' attack boxes this frame (aos_enemy_set_soul_attacks). */
static AosSoulAttack *soul_attacks;
static int soul_attack_count;

void aos_enemy_set_soul_attacks(AosSoulAttack *attacks, int count) {
    soul_attacks = attacks;
    soul_attack_count = attacks ? count : 0;
}

void aos_enemy_set_flag_8e_40(bool set) {
    flag_8e_40 = set;
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
static AosEffect *spawn_drift(uint8_t kind, int32_t x, int32_t y, AosHitReport *report) {
    if (report->effect_count >= AOS_ENEMY_MAX_EFFECTS) return NULL;
    AosEffect *e = &report->effects[report->effect_count++];
    *e = (AosEffect){.kind = kind, .x = x, .y = y, .drift = true};
    return e;
}

/* sub_0802D5EC(n): (random & (n - 1)) - n / 2. */
static int32_t random_centred(uint32_t (*random)(void), uint32_t n) {
    return (int32_t)(random() & (n - 1)) - (int32_t)(n >> 1);
}

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

/* The flame spawn of sub_0806BE74 and sub_0806AEAC: EntityCreateInRange(
 * 0x49, 0xCF, sub_0806E524) at a random point of a w x h area (16.16)
 * centred 6 pixels above the entity, mirrored at random (a third random
 * draw, only when the entity was created); priority + 0x5C - 1. A 0 x 0
 * area still draws twice: the ROM's __umodsi3 (0x080DAA00) returns 0 for a
 * zero divisor. */
static uint32_t umod(uint32_t value, uint32_t divisor) {
    return divisor ? value % divisor : 0;
}

static void spawn_flame_at(int32_t cx, int32_t cy, uint32_t w, uint32_t h,
                           uint32_t (*random)(void), AosHitReport *report) {
    int32_t x = cx + (int32_t)umod(random(), w) - (int32_t)(w / 2);
    int32_t y = cy + (int32_t)umod(random(), h) - (int32_t)(h / 2);
    if (report->effect_count >= AOS_ENEMY_MAX_EFFECTS) return;
    AosEffect *flame = &report->effects[report->effect_count++];
    *flame = (AosEffect){.kind = AOS_EFFECT_FLAME, .x = x, .y = y};
    flame->mirrored = random() & 1;
}

static void spawn_flame(const AosEnemy *e, uint32_t w, uint32_t h, uint32_t (*random)(void),
                        AosHitReport *report) {
    spawn_flame_at(e->x, e->y - 0x60000, w, h, random, report);
}

/* sub_0806BE74(e, period, w, h): a flame when the death timer is a multiple
 * of the period, the blink pattern (hidden below 40), then the timer drops
 * and the entity is deleted at 0. Returns true when deleted. */
static bool death_flames(AosEnemy *e, const AosEnemyKind *kind, int period, uint32_t w,
                         uint32_t h, uint32_t (*random)(void), AosHitReport *report) {
    if (e->timer % period == 0) spawn_flame(e, w, h, random, report);
    e->hidden = e->timer <= 0x27 ? kind->blink[e->timer] & 1 : false;
    if ((int8_t)--e->timer <= 0) e->removed = true;
    return e->removed;
}

static void take_effects(AosHitReport *to, const AosHitReport *from) {
    to->effect_count = from->effect_count;
    memcpy(to->effects, from->effects, sizeof from->effects[0] * from->effect_count);
}

/* sSineTable: every entry is floor(sin(i * pi / 2048) * 65536), which was
 * checked against all 1024 entries of the original table. */
static uint16_t sine_table[0x400];

static void build_sine(void) {
    if (sine_table[1]) return;
    for (int i = 0; i < 0x400; ++i)
        sine_table[i] = (uint16_t)floor(sin(i * 3.14159265358979323846 / 2048.0) * 65536.0);
}

int32_t aos_sine(uint32_t angle) {
    build_sine();
    uint32_t a = (angle + (0x80 >> 4)) & 0xFFFF;
    int32_t sign = 1;
    if (a & 0x8000) {
        a -= 0x8000;
        sign = -1;
    }
    if (a & 0x4000) {
        if (a & 0xFFFFBFF0u) return sign * (int32_t)sine_table[(0x800F - a) >> 4];
        return sign * 0x10000;
    }
    return sign * (int32_t)sine_table[a >> 4];
}

/* ArcTan2 (BIOS call 0x0A). The polynomial is the BIOS one as reimplemented
 * by mGBA's high-level BIOS; it is not checked against the BIOS itself. */
static int32_t arctan(int32_t i) {
    int32_t a = -((i * i) >> 14);
    int32_t b = ((0xA9 * a) >> 14) + 0x390;
    b = ((b * a) >> 14) + 0x91C;
    b = ((b * a) >> 14) + 0xFB6;
    b = ((b * a) >> 14) + 0x16AA;
    b = ((b * a) >> 14) + 0x2081;
    b = ((b * a) >> 14) + 0x3651;
    b = ((b * a) >> 14) + 0xA2F9;
    return (int16_t)((i * b) >> 16);
}

uint16_t aos_arctan2(int16_t x16, int16_t y16) {
    int32_t x = x16, y = y16, r;
    if (!y) return x >= 0 ? 0 : 0x8000;
    if (!x) return y >= 0 ? 0x4000 : 0xC000;
    if (y >= 0) {
        if (x >= 0 && x >= y) r = arctan(y * 0x4000 / x);
        else if (x < 0 && -x >= y) r = arctan(y * 0x4000 / x) + 0x8000;
        else r = 0x4000 - arctan(x * 0x4000 / y);
    } else {
        if (x <= 0 && -x > -y) r = arctan(y * 0x4000 / x) + 0x8000;
        else if (x > 0 && x >= -y) r = arctan(y * 0x4000 / x) + 0x10000;
        else r = 0xC000 - arctan(x * 0x4000 / y);
    }
    return (uint16_t)r;
}

/* Sqrt (BIOS call 0x08): the integer square root, rounded down. */
static uint32_t isqrt(uint32_t x) {
    uint32_t r = 0;
    for (uint32_t bit = 1u << 30; bit; bit >>= 2) {
        if (x >= r + bit) {
            x -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
    }
    return r;
}

/* sub_0803E86C: a 16.16 product from the operands' top 24 bits, rounded
 * toward zero to a multiple of 0x100. */
static int32_t fixed_mul(int32_t a, int32_t b) {
    if (!a || !b) return 0;
    if (a == 0x10000) return b;
    if (b == 0x10000) return a;
    int32_t a8 = a < 0 ? -((-a) >> 8) : a >> 8;
    int32_t b8 = b < 0 ? -((-b) >> 8) : b >> 8;
    int32_t p = a8 * b8;
    if (p < 0) p += 0xFF;
    return (int32_t)((uint32_t)(p >> 8) << 8);
}

/* The whole-pixel distance from `from` to `to`, rounded toward zero. */
static int32_t pixel_delta(int32_t to, int32_t from) {
    return to >= from ? (to - from) >> 16 : -((from - to) >> 16);
}

/* sub_080694B8: velocity toward (tx, ty) at `speed`; true once the target
 * is within `speed`. The gEwramData + 0xA094 offsets it adds to the entity
 * are also added to the targets of the crow, so they cancel out. */
static bool move_toward(AosEnemy *e, int32_t tx, int32_t ty, int32_t speed) {
    int32_t dx = pixel_delta(tx, e->x), dy = pixel_delta(ty, e->y);
    uint16_t angle = aos_arctan2((int16_t)dx, (int16_t)dy);
    e->vx = fixed_mul(aos_sine(angle + 0x4000u), speed);
    e->vy = fixed_mul(aos_sine(angle), speed);
    return (int32_t)(isqrt((uint32_t)(dx * dx + dy * dy)) << 16) <= speed;
}

/* sub_0803F2C8: starting an animation also clears + 0x59 bit 0. */
static void play_loop(AosEnemy *enemy, const AosAnimSet *anims, unsigned id, bool loop) {
    aos_anim_start(&enemy->anim, anims, id, loop);
    enemy->anim.id = (uint8_t)id;
    enemy->cycle_done = false;
}

/* sub_0806D128(e, n): the screen position beyond margin entry n. */
static bool outside_margins(const AosEnemyKind *kind, int n, int sx, int sy) {
    int mx = kind->margins[n][0], my = kind->margins[n][1];
    return sx < -mx || sx > mx + 0xF0 || sy < -my || sy > my + 0xA0;
}

static void play(AosEnemy *enemy, const AosAnimSet *anims, unsigned id) {
    play_loop(enemy, anims, id, true);
}

/* sub_0803F17C; + 0x59 bit 0 is set when the animation loops or ends. */
static void step_anim(AosEnemy *enemy, const AosAnimSet *anims) {
    bool ended = enemy->anim.flags & AOS_ANIM_ENDED;
    int result = aos_anim_step(&enemy->anim, anims);
    if (result == 4 || (result == 3 && !ended)) enemy->cycle_done = true;
}

/* sub_0806BC40 */
static void face_player(AosEnemy *enemy, const AosSoma *soma) {
    if (soma->x == enemy->x) return;
    enemy->mirrored = enemy->x < soma->x;
}

/* sub_0806E120 */
static int32_t toward_facing(const AosEnemy *enemy, int32_t speed) {
    return enemy->mirrored ? speed : -speed;
}

/* sub_0806BDEC */
static int16_t player_dy(const AosEnemy *enemy, const AosSoma *soma) {
    return (int16_t)((enemy->y >> 16) - (soma->y >> 16));
}

/* sub_08068AD4 with the player as the entity. */
static bool player_in(const AosSoma *soma, int32_t x, int32_t y, int w, int h) {
    return (uint16_t)((soma->x >> 16) - x) <= w && (uint16_t)((soma->y >> 16) - y) <= h;
}

/* sub_080AD700: the player in a w x h box centred on X, below the bat. */
static bool bat_sees(const AosEnemy *bat, const AosSoma *soma, int w, int h) {
    return player_in(soma, (bat->x >> 16) - w / 2, bat->y >> 16, w, h);
}

/* sub_080AD364: hanging. */
static void bat_hang(AosEnemy *bat, const AosSoma *soma, const AosAnimSet *anims,
                     uint32_t (*random)(void)) {
    face_player(bat, soma);
    switch (bat->step) {
    case 0:
        if (bat->anim.id != 0) play(bat, anims, 0);
        bat->state = 1;
        bat->substep = bat->step = 0;
        /* fallthrough */
    case 1:
        if (bat_sees(bat, soma, 0xE0, 0xA0)) {
            if (bat_sees(bat, soma, 0xC0, 0x70)) {
                bat->state = 2;
                bat->substep = bat->step = 0;
            } else if (bat->anim.id != 1) {
                play(bat, anims, 1);
            }
        } else if (!(random() & 0x7F)) {
            bat->step = 2;
            bat->substep = 0;
        } else if (bat->anim.id != 0) {
            play(bat, anims, 0);
        }
        break;
    case 2:
        if (bat->substep == 0) {
            if (bat->anim.id != 1) play(bat, anims, 1);
            bat->substep = 1;
        }
        /* + 0x59 bit 0: the animation looped. */
        if (bat->cycle_done) {
            bat->cycle_done = false;
            bat->step = 1;
            bat->substep = 0;
        }
        break;
    default:
        break;
    }
}

/* sub_080AD44C: swoop. */
static void bat_swoop(AosEnemy *bat, const AosSoma *soma, const AosAnimSet *anims, int cam_x) {
    switch (bat->step) {
    case 0:
        if (bat->substep == 0) {
            if (bat->anim.id != 2) play(bat, anims, 2);
            bat->vx = 0x8000;
            bat->vy = 0x2000;
            face_player(bat, soma);
            bat->vx = -toward_facing(bat, bat->vx);
            bat->timer = 0x20;
            bat->substep = 1;
        }
        if (--bat->timer == 0xFF) {
            bat->step = 1;
            bat->substep = 0;
        }
        break;
    case 1:
        if (bat->substep == 0) {
            bat->vy = 0x6000;
            bat->vx = toward_facing(bat, 0x6000);
            bat->substep = 1;
        }
        step_anim(bat, anims);
        if (player_dy(bat, soma) >= -0x27 && player_dy(bat, soma) <= 0x27) {
            bat->step = 2;
            bat->substep = 0;
        }
        break;
    case 2:
        if (bat->substep == 0) {
            bat->vx = toward_facing(bat, 0xC000);
            bat->vy = 0x200;
            bat->ax = toward_facing(bat, 0x50);
            bat->ay = 0x25;
            bat->phase = 0;
            bat->substep = 1;
        }
        bat->y += aos_sine(bat->phase) / 4;
        bat->phase = (uint16_t)(bat->phase + 0x200);
        {
            /* Leaving the screen: the bounds are read from + 0x4A, the
             * integer part of vx (+ 0x59 |= 8 deletes the entity). */
            int screen_x = (bat->x >> 16) - cam_x;
            int vx_pixels = (int16_t)(bat->vx >> 16);
            if (screen_x < vx_pixels - 0x28 || screen_x > vx_pixels + 0xF0) bat->removed = true;
        }
        break;
    default:
        break;
    }
    bat->x += bat->vx;
    bat->y += bat->vy;
    bat->vx += bat->ax;
    bat->vy += bat->ay;
}

/* sub_0806AF98: the entity's feet onto the floor below or above it. */
static void snap_to_floor(AosEnemy *enemy, const AosCollision *layer) {
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

/* sub_0806D104: a slope byte at the position. */
static bool on_slope(const AosCollision *layer, int32_t x, int32_t y) {
    return aos_collision_cell(layer, x, y) & 0xC0;
}

/* The removal of a zombie: state 1 with a one-frame death (HP 0). */
static void zombie_vanish(AosEnemy *zombie) {
    zombie->state = 1;
    zombie->timer = 1;
    zombie->hp = 0;
}

/* EnemyZombieCreate (the facing of sub_0806CF2C; sub_0807B404 graphics). */
static void zombie_create(AosEnemy *zombie, const AosSoma *soma, const AosCollision *layer,
                          const AosAnimSet *anims) {
    if (zombie->param1) {
        /* A spawner: hidden, no boxes, state 3. */
        zombie->state = 3;
        zombie->hidden = true;
        zombie->combat.attack_off = zombie->combat.hurt_off = true;
        snap_to_floor(zombie, layer);
        return;
    }
    play_loop(zombie, anims, 0, false);
    step_anim(zombie, anims);
    if (flag_8e_40) {
        /* State 2 (sub_0807B034) without the collision block, the floor
         * snap or the facing. */
        play_loop(zombie, anims, 0, false);
        zombie->state = 2;
        return;
    }
    int32_t y0 = zombie->y;
    snap_to_floor(zombie, layer);
    zombie->state = 0;
    zombie->mirrored = (zombie->x >> 16) < (soma->x >> 16);
    if (on_slope(layer, zombie->x >> 16, zombie->y >> 16)) zombie_vanish(zombie);
    if (zombie->y <= y0 - 0x80000) zombie_vanish(zombie);
}

static bool is_early_enemy(uint8_t id) {
    return id == AOS_ENEMY_SKELETON || id == AOS_ENEMY_PEEPING_EYE ||
           id == AOS_ENEMY_KILLER_FISH || id == AOS_ENEMY_BONE_PILLAR ||
           id == AOS_ENEMY_BUER || id == AOS_ENEMY_WHITE_DRAGON || id == AOS_ENEMY_GHOST ||
           id == AOS_ENEMY_ROCK_ARMOR || id == AOS_ENEMY_GIANT_GHOST ||
           id == AOS_ENEMY_WINGED_SKELETON;
}

bool aos_enemy_create(AosEnemy *enemy, uint8_t id, int32_t x, int32_t y, int16_t param0,
                      int16_t param1, const AosSoma *soma, const AosCollision *layer,
                      const AosEnemyKind *kind, const AosEnemyStats *stats) {
    if (id != AOS_ENEMY_BAT && id != AOS_ENEMY_ZOMBIE && !is_early_enemy(id) &&
        id != AOS_ENEMY_BLUE_CROW &&
        id != AOS_ENEMY_MERMAN && id != AOS_ENEMY_SIREN && id != AOS_ENEMY_TINY_DEVIL &&
        id != AOS_ENEMY_CATOBLEPAS && id != AOS_ENEMY_IMP && id != AOS_ENEMY_GORGON &&
        id != AOS_ENEMY_HARPY && id != AOS_ENEMY_RIPPER &&
        id != AOS_ENEMY_ZOMBIE_SOLDIER && id != AOS_ENEMY_AXE_ARMOR &&
        id != AOS_ENEMY_SKULL_ARCHER && id != AOS_ENEMY_NEEDLES && id != AOS_ENEMY_MEDUSA_HEAD &&
        id != AOS_ENEMY_ZOMBIE_OFFICER && id != AOS_ENEMY_GARGOYLE && id != AOS_ENEMY_GHOST_DANCER &&
        id != AOS_ENEMY_CURLY && id != AOS_ENEMY_DURGA && id != AOS_ENEMY_VALKYRIE &&
        id != AOS_ENEMY_ERINYS && id != AOS_ENEMY_GREMLIN && id != AOS_ENEMY_SKELETON_KNIGHT &&
        id != AOS_ENEMY_DEAD_WARRIOR && id != AOS_ENEMY_FLEAMAN && id != AOS_ENEMY_RED_CROW &&
        id != AOS_ENEMY_MIMIC && id != AOS_ENEMY_DEAD_CRUSADER && id != AOS_ENEMY_CAGNAZZO &&
        id != AOS_ENEMY_SKULL_MILLIONE && id != AOS_ENEMY_LUBICANT &&
        id != AOS_ENEMY_BEAM_SKELETON && id != AOS_ENEMY_LIGHTNING_DOLL &&
        id != AOS_ENEMY_KILLER_DOLL && id != AOS_ENEMY_NEMESIS)
        return false;
    const AosAnimSet *anims = kind->anims;
    *enemy = (AosEnemy){.id = id, .x = (int32_t)((uint32_t)x << 16), .y = (int32_t)((uint32_t)y << 16),
                        .param0 = param0, .param1 = param1, .static_frame = -1};
    /* sub_0800F1FC: mirrored when the record lies left of the player. */
    enemy->mirrored = enemy->x < soma->x;
    /* sub_0806B04C / sub_0806D244: stats and the type 8 collision block. */
    enemy->stats = *stats;
    enemy->hp = stats->hp;
    enemy->combat.type = AOS_TYPE_ENEMY;
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
        unsigned animation = id == AOS_ENEMY_KILLER_FISH ? 1 :
                             id == AOS_ENEMY_WINGED_SKELETON ? 2 : 0;
        bool direct_frame = id == AOS_ENEMY_BUER || id == AOS_ENEMY_WHITE_DRAGON;
        if (direct_frame) enemy->static_frame = 0;
        else {
            play(enemy, anims, animation);
            step_anim(enemy, anims);
        }
        if (flag_8e_40) {
            enemy->state = id == AOS_ENEMY_SKELETON || id == AOS_ENEMY_KILLER_FISH ? 5 :
                           id == AOS_ENEMY_BUER || id == AOS_ENEMY_WINGED_SKELETON ? 4 :
                           id == AOS_ENEMY_GIANT_GHOST || id == AOS_ENEMY_ROCK_ARMOR ? 3 : 2;
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
            enemy->state = 1;
            snap_to_floor(enemy, layer);
            enemy->home = (int16_t)(enemy->x >> 16);
            enemy->home_left = enemy->home - 32;
            enemy->home_right = enemy->home + 32;
            enemy->vx = toward_facing(enemy, 0x8000);
            break;
        case AOS_ENEMY_PEEPING_EYE:
            enemy->own_boxes = true;
            enemy->own_hurt = enemy->own_attack = (AosBox){-7, -7, 14, 14};
            snap_to_floor(enemy, layer);
            enemy->y -= AOS_FIXED(8);
            enemy->home = (int16_t)(enemy->y >> 16);
            break;
        case AOS_ENEMY_KILLER_FISH:
            enemy->vx = toward_facing(enemy, 0xC000);
            enemy->home = (int16_t)(enemy->y >> 16);
            break;
        case AOS_ENEMY_BONE_PILLAR:
            if (enemy->param0 < 1) enemy->param0 = 1;
            if (enemy->param0 > 8) enemy->param0 = 8;
            snap_to_floor(enemy, layer);
            /* EnemyBonePillarCreate builds its linked tower upward in
             * 22-pixel increments, leaving this original record at the top. */
            enemy->y -= AOS_FIXED(enemy->param0 * 22);
            break;
        case AOS_ENEMY_BUER:
            enemy->state = param0 ? 2 : 1;
            enemy->own_boxes = true;
            enemy->own_hurt = enemy->own_attack = (AosBox){-20, -20, 40, 40};
            snap_to_floor(enemy, layer);
            enemy->y -= AOS_FIXED(24);
            enemy->home = (int16_t)(enemy->y >> 16);
            enemy->home_left = (int16_t)((enemy->x >> 16) - 64);
            enemy->home_right = (int16_t)((enemy->x >> 16) + 64);
            enemy->vx = toward_facing(enemy, 0x18000);
            break;
        case AOS_ENEMY_WHITE_DRAGON:
            enemy->own_boxes = true;
            enemy->own_hurt = enemy->own_attack = (AosBox){-8, -8, 16, 12};
            enemy->mirrored = param0 != 0;
            enemy->home = (int16_t)(enemy->y >> 16);
            break;
        case AOS_ENEMY_GHOST:
            enemy->state = 0;
            enemy->combat.attack_off = enemy->combat.hurt_off = true;
            break;
        case AOS_ENEMY_ROCK_ARMOR:
            enemy->state = 0;
            snap_to_floor(enemy, layer);
            enemy->y += AOS_FIXED(1);
            enemy->home = (int16_t)(enemy->x >> 16);
            enemy->home_left = enemy->home - 48;
            enemy->home_right = enemy->home + 48;
            enemy->vx = toward_facing(enemy, 0x6000);
            break;
        case AOS_ENEMY_GIANT_GHOST:
            enemy->state = 0;
            break;
        case AOS_ENEMY_WINGED_SKELETON:
            enemy->state = 0;
            enemy->home = (int16_t)(enemy->y >> 16);
            enemy->vx = toward_facing(enemy, (int32_t)(0xC000 + (aos_random() & 0x3FFF)));
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
        if (param1 & 2) {
            enemy->state = 3;
            enemy->hidden = true;
            enemy->combat.attack_off = enemy->combat.hurt_off = true;
            enemy->own_boxes = false;
            return true;
        }
        enemy->state = 0;
        enemy->mirrored = (enemy->x >> 16) < (soma->x >> 16);
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
        /* EnemyNemesisCreate / sub_080C6714: four direct OAM frames from
         * 0x0824D2D8, with no animation table. The native state machine
         * selects those records directly while orbiting and charging at
         * Soma; the exporter exposes each record as a one-frame animation.
         * Nemesis starts in state 0, with the global 0x8E & 0x40 path using
         * the same idle frame without combat. */
        play(enemy, anims, 0);
        step_anim(enemy, anims);
        enemy->state = flag_8e_40 ? 7 : 0;
        enemy->phase = (uint16_t)(aos_random() & 0xFFFF);
        enemy->attack_wait = (uint16_t)((aos_random() & 0x7F) + 0x40);
        enemy->flight = (uint16_t)((aos_random() & 0x7F) + 0x40);
        enemy->leg = 0;
        enemy->own_boxes = false;
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

/* sub_080AD5B8: the bat plays sound 0x72, falls, blinks with a flame every
 * 15 frames (sub_0806BE74 over 8 x 6 pixels) and is deleted. The palette
 * bits of + 0x5A are not ported; sub_080683BC is the caller's reward. */
static void bat_die(AosEnemy *bat, const AosEnemyKind *kind, uint32_t (*random)(void),
                    AosHitReport *report) {
    if (bat->step == 0) {
        aos_audio_play_song(0x72);
        aos_anim_start(&bat->anim, kind->anims, 2, true);
        bat->anim.id = 2;
        bat->anim.frame = 2;
        bat->vy = 0x8000;
        bat->vflip = true;
        bat->timer = 0x40;
        bat->step = 1;
    }
    bat->y += bat->vy;
    death_flames(bat, kind, 15, 0x80000, 0x60000, random, report);
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

/* sub_0806E314 / sub_080421AC: the player against the enemy, then the
 * weapon against the enemy, then the contact callbacks. */
static AosHitReport collide(AosEnemy *enemy, AosSoma *soma, const AosEnemyKind *kind,
                            const AosWeaponEntity *weapon, const AosWeaponFrames *weapon_frames,
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
        }
    }
    for (int i = 0; hurt_on && !struck && i < soul_attack_count; ++i) {
        AosSoulAttack *attack = &soul_attacks[i];
        if (!aos_combat_can_take(&enemy->combat, AOS_TYPE_SOUL)) break;
        if (attack->rect.x1 <= target.x2 && target.x1 <= attack->rect.x2 &&
            attack->rect.y1 <= target.y2 && target.y1 <= attack->rect.y2) {
            aos_combat_take(&enemy->combat, AOS_TYPE_SOUL, aos_combat_cooldown(AOS_TYPE_SOUL, 0));
            struck = true;
            attacker = AOS_TYPE_SOUL;
            attack->struck = true;
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
            if (enemy->role == AOS_ROLE_AXE || enemy->role == AOS_ROLE_ARROW) {
                /* sub_080B13BC / sub_080AFD8C: a struck axe or arrow loses
                 * its single hit point. */
                enemy->hp = 0;
                goto contact;
            }
            /* sub_080B1370: the axe armor turns to face the player when hit. */
            if (enemy->id == AOS_ENEMY_AXE_ARMOR) face_player(enemy, soma);
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
            if (enemy->id == AOS_ENEMY_DEAD_CRUSADER && enemy->guarding)
                report.enemy_damage = 0;
            enemy->hit_flash = 8;
            enemy->hp = (int16_t)(enemy->hp - report.enemy_damage);
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
            if (enemy->hp <= 0) {
                report.killed = true;
                /* sub_080683BC marks it defeated (+ 0x3E bit 1):
                 * sub_0806E314 then skips its collisions. Its experience
                 * reward is applied by the runtime (aos_stats). */
                enemy->defeated = true;
                if (enemy->id == AOS_ENEMY_BAT) {
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
                } else if (enemy->id == AOS_ENEMY_AXE_ARMOR) {
                    /* sub_080B1370: state 2, step 0 (the palette bits of
                     * + 0x5A are not ported). */
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
                     * they are gone. */
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
                    play_loop(enemy, kind->anims, 2, false);
                    enemy->translucent = true;
                    enemy->state = 3;
                    enemy->step = 1;
                } else if (enemy->id == AOS_ENEMY_GARGOYLE || enemy->id == AOS_ENEMY_GREMLIN) {
                    /* sub_08086878: state 1, 40 frames, falling with
                     * + 0x54 = 0x1000 (sound 0x6E). */
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
                    play_loop(enemy, kind->anims, 3, false);
                    enemy->timer = 0x28;
                    enemy->state = 2;
                    enemy->step = 0;
                } else if (enemy->id == AOS_ENEMY_NEEDLES) {
                    /* sub_080CB930: it shatters (state 2). */
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
                    /* sub_080896D8: a 60-frame death whose first update
                     * emits six native sprite fragments. */
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
        report.soma_damage = aos_soma_take_hit(soma, enemy->stats.contact, soma_def, enemy->x, 1);
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
        if ((enemy->id == AOS_ENEMY_VALKYRIE || enemy->id == AOS_ENEMY_ERINYS) &&
            !(enemy->state == 1 && enemy->anim.id == 1 && (uint8_t)(enemy->anim.frame - 4) <= 2))
            contact >>= 1;
        /* sub_080994B0: the Mimic's contact is a knockback (type 1). */
        report.soma_damage = aos_soma_take_hit(soma, contact, soma_def, enemy->x,
                                               enemy->id == AOS_ENEMY_MIMIC ? 1 : 0);
    }
    return report;
}

/* sub_0806AEAC: the generic death, a flame every 4 frames over 16 x 12
 * pixels, the blink, then the deletion. */
static void generic_death(AosEnemy *enemy, const AosEnemyKind *kind, uint32_t (*random)(void),
                          AosHitReport *report) {
    if ((enemy->timer & 3) == 0) spawn_flame(enemy, 0x100000, 0xC0000, random, report);
    enemy->hidden = enemy->timer <= 0x27 ? kind->blink[enemy->timer] & 1 : false;
    if ((int8_t)--enemy->timer <= 0) enemy->removed = true;
}

/* sub_08069A00 with the zombie's arguments: X then Y motion with the walls
 * at x +/- 8, y - 10, the ceiling at y - 32 and the floor; returns 1 / 2
 * for a wall on the right / left, 8 for a ceiling, 4 for a landing. */
static int walk_collide(AosEnemy *e, const AosCollision *layer, int32_t bounce_vy,
                        int32_t head, int32_t max_speed, int half_width) {
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

/* sub_0806CFFC: the player inside a box given in room pixels. */
static bool player_near(const AosSoma *soma, int32_t x, int32_t y, int w, int h) {
    return player_in(soma, x, y, w, h);
}

/* sub_08092FCC(e, dx, dy, vx, vy, frame): a gib (EntityCreateInRange(0x49,
 * 0xCF, sub_08093078)) showing a sprite frame of the soldier, at its
 * position + (dx, dy), mirrored like it, falling with ay 0x1800; it keeps
 * the soldier's y (+ 0x1C) and is deleted once lower. Priority - 1. */
static AosEnemy make_gib(const AosEnemy *z, int32_t dx, int32_t dy, int32_t vx, int32_t vy,
                         int16_t frame) {
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

/* A child of the zombie showing one of its sprite frames, mirrored like it. */
static AosEnemy zombie_child(const AosEnemy *z, uint8_t role, int16_t frame, int32_t x,
                             int32_t y) {
    AosEnemy c = {0};
    c.id = z->id;
    c.role = role;
    c.static_frame = frame;
    c.x = x;
    c.y = y;
    c.mirrored = z->mirrored;
    c.combat.attack_off = c.combat.hurt_off = true;
    return c;
}

/* sub_0807B1CC: a drip (sub_0806DFF8(sub_0807B310)), sprite frame 0xD,
 * falling with ay 0xA00 (sub_0806D384 / sub_0806D3CC, clamped to 8.0) for
 * 31 updates (+ 0x0D from 30). */
static void zombie_drip(const AosEnemy *z, int32_t x, int32_t y, AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy d = zombie_child(z, AOS_ROLE_DRIP, 0xD, x, y);
    d.timer = 0x1E;
    d.ay = 0xA00;
    report->children[report->child_count++] = d;
}

/* sub_0807B33C: a puddle at the zombie's feet, sprite frame 0, behind it
 * (priority + 1), for 121 updates (+ 0x1C from 120). */
static void zombie_puddle(const AosEnemy *z, AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy p = zombie_child(z, AOS_ROLE_PUDDLE, 0, z->x, z->y);
    p.walk_timer = 0x78;
    report->children[report->child_count++] = p;
}

/* sub_0807B310 / sub_0807B3C8 */
static void drip_update(AosEnemy *d) {
    d->vy += d->ay;
    if (d->vy > 0x80000) d->vy = 0x80000;
    d->y += d->vy;
    if ((int8_t)d->timer-- <= 0) d->removed = true;
}

static void puddle_update(AosEnemy *p) {
    if (p->walk_timer-- <= 0) p->removed = true;
}

/* sub_0807B0DC at 0 HP: two pieces of sub_0807B258 (the gib motion of
 * sub_0806D3D8 with ay 0x1800, sprite frame 0xD, deleted below the
 * zombie's y) at x - 4 + (r & 0x7FFFF), y - 26 + (r & 0x3FFFF); each
 * created piece draws vx = (r & 0x1FFFF) - 1.0 and vy = -1.0 - (r & 0xFFFF). */
static void zombie_gore(const AosEnemy *z, uint32_t (*random)(void), AosHitReport *report) {
    for (int i = 0; i < 2; ++i) {
        int32_t x = (int32_t)(random() & 0x7FFFF) - 0x40000 + z->x;
        int32_t y = (int32_t)(random() & 0x3FFFF) - 0x1A0000 + z->y;
        if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) continue;
        int32_t vx = (int32_t)(random() & 0x1FFFF) - 0x10000;
        int32_t vy = -0x10000 - (int32_t)(random() & 0xFFFF);
        report->children[report->child_count++] = make_gib(z, x - z->x, y - z->y, vx, vy, 0xD);
    }
}

/* The drip offsets of sub_0807AD28 (walking) and sub_0807B034 (state 2):
 * walk animation 4 at (+/- 9, -24), animation 1 at (-9 / +7, -22) in the
 * walk, (-7 / +7, -22) in state 2; the first value is unmirrored. */
static void zombie_drip_at_frame(const AosEnemy *z, bool walking, AosHitReport *report) {
    int32_t dx, dy;
    if (z->anim.id == 4) {
        dx = z->mirrored ? (int32_t)0xFFF70000 : 0x90000;
        dy = (int32_t)0xFFE80000;
    } else {
        dx = z->mirrored ? 0x70000 : (walking ? (int32_t)0xFFF70000 : (int32_t)0xFFF90000);
        dy = (int32_t)0xFFEA0000;
    }
    zombie_drip(z, z->x + dx, z->y + dy, report);
}

/* sub_0807B034 (state 2, under the global 0x8E & 0x40): once the rise
 * ends, walk animation 1 or 4 at random in place, a drip at the start of
 * every cycle; no collision pass. */
static void zombie_idle(AosEnemy *z, const AosAnimSet *anims, uint32_t (*random)(void),
                        AosHitReport *report) {
    if (z->step == 0) {
        if (!z->cycle_done) return;
        z->step = 1;
        play(z, anims, (random() & 1) ? 1 : 4);
        return;
    }
    if (z->anim.frame == 0 && z->anim.tick == 0) zombie_drip_at_frame(z, false, report);
}

static void zombie_walk(AosEnemy *z, const AosSoma *soma, const AosCollision *layer,
                        const AosAnimSet *anims, uint32_t (*random)(void), AosHitReport *report) {
    switch (z->step) {
    case 0:
        if (!z->cycle_done) return;
        z->cycle_done = false;
        z->step = 1;
        play(z, anims, (random() & 1) ? 1 : 4);
        z->vx = z->mirrored ? 0x20000 : (int32_t)0xFFFE0000;
        z->vy = 0x10000;
        z->ay = 0x2800;
        z->walk_timer = (int32_t)(random() & 0x1FF) + 600;
        return;
    case 1: {
        if (z->anim.tick == 0) {
            z->vx = z->mirrored ? 0x20000 : (int32_t)0xFFFE0000;
            z->vy = 0x10000;
            z->ay = 0x2800;
        }
        z->vx = z->vx * 3 / 4;                  /* sub_0806D490(e, 4) */
        int32_t x0 = z->x, y0 = z->y;
        bool stop = walk_collide(z, layer, 0x10000, (int32_t)0xFFE00000, 0x80000, 8) & 3;
        if (!stop) stop = on_slope(layer, z->x >> 16, z->y >> 16);
        if (!stop) stop = z->walk_timer-- <= 0;
        if (stop) {
            z->step = 2;
            z->anim.flags |= 2;                 /* + 0x6C bit 1 pauses it */
            z->x = x0;
            z->y = y0;
            z->timer = 0;
            return;
        }
        /* At the start of each cycle a drip, at frames 0 and 2 a puddle
         * (once each, re-armed on frames 1 and 3). */
        if (z->anim.frame == 0 && !z->dropped) {
            z->dropped = true;
            zombie_drip_at_frame(z, true, report);
        }
        if ((z->anim.frame == 0 || z->anim.frame == 2) && !z->puddled) {
            z->puddled = true;
            zombie_puddle(z, report);
        }
        if (z->anim.frame == 1 || z->anim.frame == 3) z->dropped = z->puddled = false;
        /* Near the player (80 x 64 in front), the animation runs faster. */
        int32_t sx = z->x >> 16, sy = (z->y >> 16) - 0x20;
        bool near = z->mirrored ? player_near(soma, sx, sy, 0x50, 0x40)
                                : player_near(soma, sx - 0x50, sy, 0x50, 0x40);
        if (near && !(z->walk_timer & 1)) z->anim.tick++;
        return;
    }
    case 2:
        z->anim.flags |= 2;
        if (z->timer++ > 0x3B) {
            play_loop(z, anims, 2, false);
            z->timer = 0;
            z->step = 3;
        }
        return;
    case 3:
        if (z->cycle_done) {
            z->cycle_done = false;
            zombie_vanish(z);
        }
        return;
    default:
        return;
    }
}

/* EnemyZombieUpdate state 3: a spawner creates a zombie one frame in 32 at
 * a random screen X at least 32 pixels from the player, up to param0. */
static void zombie_spawner(AosEnemy *spawner, const AosSoma *soma, int cam_x,
                           uint32_t (*random)(void), AosHitReport *report) {
    if (random() & 0x1F) return;
    if (spawner->spawned_count >= spawner->param0) return;
    int32_t x;
    int guard = 0;
    do {
        x = (int32_t)(random() % 0xF0) + cam_x;
        int32_t dx = soma->x - (x << 16);
        if ((dx < 0 ? -dx : dx) > 0x200000) break;
    } while (++guard < 64);
    spawner->spawned_count++;
    report->spawn = true;
    report->spawn_x = x;
    report->spawn_y = spawner->y >> 16;
}

static AosHitReport zombie_update(AosEnemy *z, AosSoma *soma, const AosCollision *layer,
                                  const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                  const AosWeaponFrames *weapon_frames, int soma_atk,
                                  int soma_def, int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    if (z->state != 3) {
        /* sub_0806D128(e, 4): out of the screen margins, it vanishes. */
        int sx = (z->x >> 16) - cam_x, sy = (z->y >> 16) - cam_y;
        if (outside_margins(kind, 4, sx, sy))
            zombie_vanish(z);
    }
    /* sub_0806AD24: alive and flashing (+ 0x2D), sub_0806AE54 counts down
     * and only the collision pass runs (not while dying). */
    if (z->hp > 0 && z->hit_flash) {
        z->hit_flash--;
        if (z->state != 1) report = collide(z, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        aos_combat_tick(&z->combat);
        return report;
    }
    switch (z->state) {
    case 0: zombie_walk(z, soma, layer, kind->anims, random, &spawned); break;
    case 1: generic_death(z, kind, random, &spawned); break;
    case 2:
        zombie_idle(z, kind->anims, random, &spawned);
        step_anim(z, kind->anims);
        return spawned;
    case 3: zombie_spawner(z, soma, cam_x, random, &report); return report;
    default: return report;
    }
    if (z->removed) return spawned;
    step_anim(z, kind->anims);
    z->attack_off = z->state == 0 && (z->step == 0 || z->step == 3);
    int sx = (z->x >> 16) - cam_x, sy = (z->y >> 16) - cam_y;
    if (!z->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
        report = collide(z, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    if (report.killed) zombie_gore(z, random, &spawned);
    aos_combat_tick(&z->combat);
    report.child_count = spawned.child_count;
    memcpy(report.children, spawned.children, sizeof spawned.children[0] * spawned.child_count);
    take_effects(&report, &spawned);
    return report;
}

/* sub_0806BF78: the player within `range` of the entity on both axes. */
static bool player_within(const AosEnemy *e, const AosSoma *soma, uint32_t range) {
    int32_t dx = e->x - soma->x, dy = e->y - soma->y;
    return (uint32_t)(dx < 0 ? -dx : dx) <= range && (uint32_t)(dy < 0 ? -dy : dy) <= range;
}

static void integrate(AosEnemy *e);
static void stop(AosEnemy *e);

/* The linked Nemesis sprite created by sub_080C76D4/sub_080C7784. The native
 * callback is driven by a small state machine and reads the parent slot on
 * every frame for position and lifecycle. The offsets and frame selections
 * are the native fixed-point values. */
static void nemesis_helper_update(AosEnemy *h) {
    const AosEnemy *parent = h->parent;
    if (!parent || parent->removed) {
        h->removed = true;
        return;
    }
    if (parent->defeated) {
        h->hidden = true;
        h->removed = true;
        return;
    }
    switch (h->state) {
    case 0:
        h->hidden = true;
        break;
    case 1:
        h->hidden = false;
        h->static_frame = 1;
        h->x = parent->x;
        h->y = parent->y;
        h->mirrored = parent->mirrored;
        break;
    case 2:
        h->hidden = false;
        h->static_frame = 2;
        if (h->step == 0) {
            h->x = parent->x;
            h->mirrored = !parent->mirrored;
            h->timer = 0;
            h->step = 1;
        }
        h->x = parent->x + (h->mirrored ? -AOS_FIXED(9) : AOS_FIXED(9));
        h->y = parent->y + AOS_FIXED(37);
        if (h->timer++ >= 10) {
            h->state = 4;
            h->step = 0;
            h->timer = 0;
        }
        break;
    case 3:
        h->hidden = false;
        h->static_frame = 3;
        if (h->step == 0) {
            h->timer = 0;
            h->step = 1;
            h->mirrored = parent->mirrored;
        }
        h->x = parent->x + (h->mirrored ? -AOS_FIXED(10) : AOS_FIXED(10));
        h->y = parent->y - AOS_FIXED(4);
        if (h->timer++ >= 40) {
            h->state = 4;
            h->step = 0;
            h->timer = 0;
        }
        break;
    case 4:
        h->hidden = false;
        h->static_frame = 3;
        /* sub_080C7784: mirror is cleared on timer % 3 == 0 and set on the
         * other two ticks of each three-frame flicker group. */
        h->mirrored = (h->timer % 3) != 0;
        if (h->timer++ >= 31) {
            h->state = 0;
            h->step = 0;
            h->timer = 0;
            h->hidden = true;
        }
        break;
    default:
        /* State 5 is the native no-op state used while the parent is idle. */
        h->hidden = true;
        break;
    }
}

/* The three visual slots made by sub_080C7B54 (sequence indices 9, 12 and
 * 6). sub_08045748 copies the owning Nemesis position, mirror and current
 * sprite record into each translucent child; their collision callbacks are
 * disabled. Keeping the sequence index in `variant` preserves the native
 * identity while the child scratch history selects a delayed direct frame. */
static void nemesis_effect_update(AosEnemy *effect, const AosEnemyKind *kind) {
    const AosEnemy *parent = effect->parent;
    if (!parent || parent->removed || parent->defeated) {
        effect->removed = true;
        return;
    }
    /* sub_08045748 starts a newly allocated child in state 0, promotes it to
     * state 2 on the first callback and advances +0x0D every callback. */
    if (effect->state == 0) effect->state = 2;
    effect->timer = (uint8_t)(effect->timer + 1);
    effect->hidden = parent->hidden;
    effect->vflip = parent->vflip;
    effect->translucent = true;

    /* The child owns a zero-filled 0x31-word scratch allocation.  The native
     * callback copies records backwards so record 0 remains the newest sample
     * and records 1..23 are the preceding frames.  This is the source of the
     * Nemesis motion trail; it is not a separate animation descriptor. */
    for (int i = AOS_NEMESIS_EFFECT_HISTORY - 1; i > 0; --i) {
        effect->nemesis_history_x[i] = effect->nemesis_history_x[i - 1];
        effect->nemesis_history_y[i] = effect->nemesis_history_y[i - 1];
        effect->nemesis_history_frame[i] = effect->nemesis_history_frame[i - 1];
        effect->nemesis_history_mirror[i] = effect->nemesis_history_mirror[i - 1];
    }
    effect->nemesis_history_x[0] = (int16_t)(parent->x >> 16);
    effect->nemesis_history_y[0] = (int16_t)(parent->y >> 16);
    uint8_t frame = parent->anim.frame;
    if (kind && parent->anim.id < AOS_ENEMY_MAX_ANIMS && parent->anim.frame < AOS_ENEMY_MAX_FRAMES)
        frame = kind->sprites[parent->anim.id][parent->anim.frame];
    effect->nemesis_history_frame[0] = frame;
    effect->nemesis_history_mirror[0] = parent->mirrored ? 1 : 0;

    int index = effect->variant;
    if (index < 0 || index >= AOS_NEMESIS_EFFECT_HISTORY) index = 0;
    effect->x = AOS_FIXED(effect->nemesis_history_x[index]);
    effect->y = AOS_FIXED(effect->nemesis_history_y[index]);
    effect->mirrored = effect->nemesis_history_mirror[index] != 0;
    effect->static_frame = effect->nemesis_history_frame[index];
}

/* EnemyNemesisUpdate (sub_080C67CC and states 0..6): the original uses a
 * compact four-frame descriptor and several linked helper entities. The
 * observable body routine keeps the idle hover, homing approach, short
 * charge and recovery loop, while emitting the linked helper on its first
 * active update. The three technical effects created by sub_080C7B54 remain
 * a separate parity gap. */
static AosHitReport nemesis_update(AosEnemy *n, AosSoma *soma, const AosEnemyKind *kind,
                                   const AosWeaponEntity *weapon,
                                   const AosWeaponFrames *weapon_frames, int soma_atk,
                                   int soma_def, uint32_t (*random)(void)) {
    AosHitReport report = {0};
    bool emit_helper = !n->helper_spawned && n->state != 7;
    bool emit_effects = !n->technical_effects_spawned && n->state != 7;
    if (n->state == 7) {
        if (n->anim.id != 0) play(n, kind->anims, 0);
        step_anim(n, kind->anims);
        return report;
    }
    if (n->defeated) {
        generic_death(n, kind, random, &report);
        aos_combat_tick(&n->combat);
        return report;
    }
    if (n->hit_flash) n->hit_flash--;
    if (n->attack_wait) n->attack_wait--;
    switch (n->state) {
    case 0: {
        if (n->anim.id != 0) play(n, kind->anims, 0);
        n->phase = (uint16_t)(n->phase + 0x300);
        n->y += aos_sine(n->phase) / 8;
        face_player(n, soma);
        if (player_within(n, soma, 0xC00000) && !n->attack_wait) {
            n->state = 1;
            n->step = 0;
            n->timer = 0x20;
        }
        break;
    }
    case 1:
        if (n->anim.id != 1) play(n, kind->anims, 1);
        if (n->step == 0) {
            n->timer = 0x20;
            n->step = 1;
        }
        face_player(n, soma);
        (void)move_toward(n, soma->x, soma->y - 0x180000, 0x8000);
        integrate(n);
        if (n->timer == 0 || --n->timer == 0 || player_within(n, soma, 0x180000)) {
            n->state = 2;
            n->step = 0;
        }
        break;
    case 2:
        if (n->anim.id != 2) play(n, kind->anims, 2);
        if (n->step == 0) {
            face_player(n, soma);
            (void)move_toward(n, soma->x, soma->y, 0x18000);
            n->timer = 0x18;
            n->step = 1;
        }
        integrate(n);
        if (n->timer && --n->timer == 0) {
            n->state = 3;
            n->step = 0;
        }
        break;
    case 3:
        if (n->anim.id != 3) play(n, kind->anims, 3);
        stop(n);
        if (n->step == 0) {
            n->timer = 0x1C;
            n->step = 1;
        }
        if (n->timer && --n->timer == 0) {
            n->state = 0;
            n->step = 0;
            n->attack_wait = 0x40;
        }
        break;
    default:
        n->state = 0;
        n->step = 0;
        break;
    }
    step_anim(n, kind->anims);
    if (!n->defeated && player_within(n, soma, 0x1000000))
        report = collide(n, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    aos_combat_tick(&n->combat);
    if (emit_helper && report.child_count < AOS_ENEMY_MAX_CHILDREN) {
        AosEnemy helper = {0};
        helper.id = n->id;
        helper.role = AOS_ROLE_NEMESIS_HELPER;
        helper.state = 1;
        helper.static_frame = 1;
        helper.anim.id = 0;
        helper.x = n->x;
        helper.y = n->y;
        helper.mirrored = n->mirrored;
        helper.combat.attack_off = true;
        helper.combat.hurt_off = true;
        report.children[report.child_count++] = helper;
        n->helper_spawned = true;
        n->spawned_count = 1;
    }
    if (emit_effects) {
        static const uint8_t sequences[] = {9, 12, 6};
        for (unsigned i = 0; i < sizeof sequences &&
                              report.child_count < AOS_ENEMY_MAX_CHILDREN;
             ++i) {
            AosEnemy effect = {0};
            effect.id = n->id;
            effect.role = AOS_ROLE_NEMESIS_EFFECT;
            effect.variant = sequences[i];
            effect.step = sequences[i];
            effect.state = 0;
            effect.timer = 0;
            effect.static_frame = n->anim.frame < 4 ? n->anim.frame : 0;
            effect.anim.id = 0;
            effect.x = n->x;
            effect.y = n->y;
            effect.mirrored = n->mirrored;
            effect.hidden = n->hidden;
            effect.translucent = true;
            effect.combat.attack_off = true;
            effect.combat.hurt_off = true;
            report.children[report.child_count++] = effect;
        }
        if (report.child_count >= 4) {
            n->technical_effects_spawned = true;
            n->spawned_count = report.child_count;
        }
    }
    return report;
}

static void integrate(AosEnemy *e) {
    e->x += e->vx;
    e->y += e->vy;
    e->vx += e->ax;
    e->vy += e->ay;
}

static void stop(AosEnemy *e) {
    e->vx = e->vy = e->ax = e->ay = 0;
}

/* sub_080C9AF4: perch until the player comes within 60 pixels (sound
 * 0x8B), then keep returning to a point 42 pixels above him and 32 pixels
 * behind him whenever he is more than 72 pixels away horizontally and 16
 * vertically. Step 2 (a glide from the current velocity) is not entered by
 * any code read and is not ported. */
static void crow_fly(AosEnemy *c, const AosSoma *soma, const AosAnimSet *anims) {
    int32_t ty = soma->y - 0x2A0000;
    switch (c->step) {
    case 0:
        c->phase = 0;
        face_player(c, soma);
        if (c->anim.id != 0) play(c, anims, 0);
        c->step = 1;
        c->substep = c->timer = 0;
        /* fallthrough */
    case 1:
        if (player_within(c, soma, 0x3C0000)) {
            c->step = 3;
            c->substep = 0;
            aos_audio_play_song(0x8B);
        }
        break;
    case 3:
        face_player(c, soma);
        switch (c->substep) {
        case 0:
            c->phase = (uint8_t)(c->phase + 1);
            if (c->anim.id != 2) play(c, anims, 2);
            stop(c);
            c->timer = 0x20;
            c->substep = 1;
            /* fallthrough */
        case 1:
            if (--c->timer == 0xFF) c->substep = 2;
            break;
        case 2: {
            int32_t dx = c->x - soma->x, dy = c->y - soma->y;
            if ((dx < 0 ? -dx : dx) > 0x480000 && (dy < 0 ? -dy : dy) > 0x100000) c->substep = 3;
            break;
        }
        case 3: {
            if (c->anim.id != 1) play(c, anims, 1);
            /* The player's + 0x58 bit 0x40: behind him is to his right. */
            int32_t tx = soma->x + (soma->facing_left ? 0x200000 : -0x200000);
            if (move_toward(c, tx, ty, 0x1A000)) {
                if (c->anim.id != 2) play(c, anims, 2);
                stop(c);
                c->substep = 2;
            }
            break;
        }
        default:
            break;
        }
        break;
    default:
        break;
    }
    integrate(c);
}

/* sub_080C9E2C: two feathers (EntityCreateInRange(0x1B, 0x48,
 * sub_080C9ED4)), sprite frame 8 of the crow, at (x + 8, y + 4) and
 * (x - 8, y - 4) in whole pixels, parameter 0 = 0 / 1. */
static void crow_feathers(const AosEnemy *c, AosHitReport *report) {
    for (int i = 0; i < 2 && report->child_count < AOS_ENEMY_MAX_CHILDREN; ++i) {
        AosEnemy f = {0};
        f.id = c->id;
        f.role = AOS_ROLE_FEATHER;
        f.static_frame = 8;
        f.param0 = (int16_t)i;
        f.x = (int32_t)((uint32_t)((c->x >> 16) + (i ? -8 : 8)) << 16);
        f.y = (int32_t)((uint32_t)((c->y >> 16) + (i ? -4 : 4)) << 16);
        f.combat.attack_off = f.combat.hurt_off = true;
        report->children[report->child_count++] = f;
    }
}

/* sub_080C9ED4: a feather drifts at 0.25 to the side (parameter 1:
 * mirrored, to the left) and 0.125 down, its X speed decreasing by 0x300
 * per frame; after 49 frames (state 1) and 25 more (state 2) it is
 * deleted; it blinks on the timer's bit 0. The palette bits set at the end
 * of state 1 (+ 0x5A = 2) are not ported. */
static void feather_update(AosEnemy *f) {
    switch (f->state) {
    case 0:
        f->vx = 0x4000;
        f->vy = 0x2000;
        f->ax = -0x300;
        if (f->param0) {
            f->vx = -f->vx;
            f->ax = -f->ax;
            f->mirrored = true;
        }
        f->timer = 0x30;
        f->state = 1;
        /* fall through */
    case 1:
        if (--f->timer == 0xFF) {
            f->timer = 0x18;
            f->state = 2;
        }
        break;
    case 2:
        if (--f->timer == 0xFF) f->removed = true;
        break;
    default:
        break;
    }
    f->x += f->vx;
    f->y += f->vy;
    f->vx += f->ax;
    f->vy += f->ay;
    f->hidden = f->timer & 1;
}

/* sub_080C9D68: the feathers, animation 3, a blood splash of
 * sub_0806E664 at the crow, then a fall at 0.5 per frame with a splash
 * 4 pixels lower every 16 frames and sub_0806BE74 over 64 frames (a flame
 * every 6 frames over 8 x 8 pixels), then the deletion. Sound 0x72, the
 * palette bits of + 0x5A and sub_080683BC are not ported. */
static void crow_die(AosEnemy *c, const AosEnemyKind *kind, uint32_t (*random)(void),
                     AosHitReport *report) {
    if (c->step == 0) {
        /* sub_080C9D68 / sub_080CA308 step 0: sound 0x72. */
        aos_audio_play_song(0x72);
        crow_feathers(c, report);
        if (c->anim.id != 3) play_loop(c, kind->anims, 3, false);
        stop(c);
        c->vy = 0x8000;
        c->timer = 0x40;
        c->step = 1;
        c->substep = 0;
        spawn_blood(c->x >> 16, c->y >> 16, random, report);
    }
    c->y += c->vy;
    if ((c->timer & 0xF) == 0) spawn_blood(c->x >> 16, (c->y >> 16) + 4, random, report);
    if (death_flames(c, kind, 6, 0x80000, 0x80000, random, report)) return;
    step_anim(c, kind->anims);
}

/* sub_080CA0D4: the red crow perches facing the player until he is within
 * 120 pixels on both axes (sub_0806BF78); then it hovers (animation 2,
 * still) for 128 frames, turning to him (odd rounds, + 0x14) or away (even
 * ones) halfway, and swoops (animation 1, sound 0x8B): vx 0.5 toward its
 * facing with + 0x50 0x1000 that way, vy 2.0 with + 0x54 -0x1000, for 64
 * frames, then hovers again; after an odd round other than the first,
 * with the player beyond 120 pixels, it plays animation 4 once and
 * perches again. It always moves by its velocity, then accelerates. */
static void red_crow_fly(AosEnemy *c, const AosSoma *soma, const AosAnimSet *anims) {
    switch (c->step) {
    case 0:
        c->volley_count = 0;
        face_player(c, soma);
        if (c->anim.id != 0) play(c, anims, 0);
        c->step = 1;
        c->substep = 0;
        c->timer = 0;
        /* fall through */
    case 1:
        if (player_within(c, soma, 0x780000)) {
            c->step = 3;
            c->substep = 0;
        }
        break;
    case 2:
        if (c->substep == 0) {
            if (c->anim.id != 1) play(c, anims, 1);
            c->vx = 0x8000;
            c->vy = 0x20000;
            c->ax = 0x1000;
            c->ay = -0x1000;
            if (!c->mirrored) {
                c->vx = -0x8000;
                c->ax = -0x1000;
            }
            c->timer = (uint8_t)((c->vy / c->ay < 0 ? -(c->vy / c->ay) : c->vy / c->ay) * 2);
            c->substep = 1;
            aos_audio_play_song(0x8B);
        }
        if (--c->timer == 0xFF) {
            c->step = 3;
            c->substep = 0;
        }
        break;
    case 3:
        if (c->substep == 0) {
            c->volley_count++;
            if (c->anim.id != 2) play(c, anims, 2);
            c->vx = c->vy = c->ax = c->ay = 0;
            c->timer = 0x80;
            c->substep = 1;
        }
        if (c->timer == 0x40) {
            if (c->volley_count & 1) face_player(c, soma);
            else c->mirrored = !c->mirrored;
        }
        if (--c->timer == 0xFF) {
            c->step = 2;
            c->substep = 0;
            if ((c->volley_count & 1) && c->volley_count != 1 &&
                !player_within(c, soma, 0x780000)) {
                c->step = 4;
                c->cycle_done = false;
            }
        }
        break;
    case 4:
        if (c->substep == 0) {
            if (c->cycle_done) c->substep = 1;
        } else if (c->substep == 1) {
            if (c->anim.id != 4) play_loop(c, anims, 4, false);
            c->substep = 2;
            if (c->cycle_done) c->step = c->substep = 0;
        } else if (c->cycle_done) {
            c->step = c->substep = 0;
        }
        break;
    default:
        break;
    }
    c->x += c->vx;
    c->y += c->vy;
    c->vx += c->ax;
    c->vy += c->ay;
}

/* EnemyBlueCrowUpdate: no activity window and no on-screen test before the
 * collision pass (sub_080421AC). sub_0806AD24 stuns it while alive: with
 * + 0x2D (set to 8 by a hit) non-zero, sub_0806AE54 decrements it and the
 * flight and the animation step are skipped; the palette flash it writes,
 * the global pause 0x4BE (taken as 0), the + 0x38 bit 0x100 stop and the
 * tracking list of sub_08021924 are not ported. */
static AosHitReport crow_update(AosEnemy *c, AosSoma *soma, const AosEnemyKind *kind,
                                const AosWeaponEntity *weapon, const AosWeaponFrames *weapon_frames,
                                int soma_atk, int soma_def, uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (c->state == 3) {
        /* sub_080C9FFC: animation 0 (unless playing), stepped each frame;
         * no collision pass. */
        if (c->step == 0) {
            if (c->anim.id != 0) play(c, kind->anims, 0);
            c->step = 1;
        }
        step_anim(c, kind->anims);
        return report;
    }
    if (c->state == 2) {
        crow_die(c, kind, random, &report);
    } else if (c->hp > 0 && c->hit_flash) {
        c->hit_flash--;
        report = collide(c, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    } else {
        if (c->id == AOS_ENEMY_RED_CROW) red_crow_fly(c, soma, kind->anims);
        else crow_fly(c, soma, kind->anims);
        step_anim(c, kind->anims);
        report = collide(c, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    }
    aos_combat_tick(&c->combat);
    return report;
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

/* sub_08092BC0: a grenade at the soldier's position plus (dx, dy), thrown
 * at (vx, vy) with gravity 0x1800; it shows sprite frame 18, carries the
 * soldier's contact power and has a 4 x 4 box for both roles. */
static AosEnemy make_grenade(const AosEnemy *soldier, int32_t dx, int32_t dy, int32_t vx,
                             int32_t vy) {
    AosEnemy g = {0};
    g.id = soldier->id;
    g.role = AOS_ROLE_GRENADE;
    g.static_frame = 18;
    g.x = soldier->x + dx;
    g.y = soldier->y + dy;
    g.vx = vx;
    g.vy = vy;
    g.ay = 0x1800;
    g.mirrored = soldier->mirrored;
    g.stats = soldier->stats;
    g.hp = 1;
    g.combat.type = AOS_TYPE_ENEMY;
    g.own_boxes = true;
    g.own_hurt = g.own_attack = (AosBox){-2, -2, 4, 4};
    return g;
}

/* sub_08092CCC: flight (a floor hit bounces at a third of the speed, sound
 * 0x11B; a second floor hit or a ceiling makes it explode, sound 0x76),
 * the harmless burst of state 1 (7 frames) and the explosion of state 2,
 * whose attack box grows to 2 + t / 2 pixels around it until t = 14, hits
 * until t = 18 and ends at t = 24. The burst and the explosion's first 8
 * frames create the particles of grenade_particles. */
static AosHitReport grenade_update(AosEnemy *g, AosSoma *soma, const AosCollision *layer,
                                   const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                   const AosWeaponFrames *weapon_frames, int soma_atk,
                                   int soma_def, int cam_x, int cam_y,
                                   uint32_t (*random)(void)) {
    AosHitReport report = {0}, particles = {0};
    switch (g->state) {
    case 0: {
        int flags = projectile_collide(g, layer, g->vy / 3, (int32_t)0xFFFC0000);
        bool explode = false;
        if (flags & 4) {
            g->vy = -g->vy;
            explode = ++g->timer > 1;
            if (!explode) aos_audio_play_song(0x11B);
        }
        if (explode || (flags & 8)) {
            g->state = 2;
            aos_audio_play_song(0x76);
            break;
        }
        if (outside_margins(kind, 2, (g->x >> 16) - cam_x, (g->y >> 16) - cam_y)) {
            g->removed = true;
            return report;
        }
        report = collide(g, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        break;
    }
    case 1:
        g->hidden = true;
        ++g->timer;
        grenade_particles(g, random, &report);
        if (g->timer > 6) g->removed = true;
        break;
    case 2:
        g->hidden = true;
        ++g->timer;
        if (g->timer <= 8) grenade_particles(g, random, &particles);
        if (g->timer <= 14) {
            int r = 2 + g->timer / 2;
            g->own_attack = (AosBox){(int8_t)-r, (int8_t)-r, (uint8_t)(2 * r), (uint8_t)(2 * r)};
            g->combat.hurt_off = true;
        }
        if (g->timer > 0x17) {
            g->removed = true;
            return report;
        }
        if (g->timer <= 0x12) report = collide(g, soma, kind, weapon, weapon_frames, soma_atk,
                                               soma_def);
        else g->combat.attack_off = true;
        take_effects(&report, &particles);
        break;
    default:
        break;
    }
    aos_combat_tick(&g->combat);
    return report;
}

/* sub_0806CF2C: facing the player, mirrored when he is to the right. */
static void soldier_face(AosEnemy *z, const AosSoma *soma) {
    z->mirrored = (z->x >> 16) < (soma->x >> 16);
}

/* sub_08092A28 steps 1 / 0xB: back to walking. */
static void soldier_walk_again(AosEnemy *z, const AosAnimSet *anims) {
    play(z, anims, 0);
    z->state = z->step = 0;
    z->vx = 0;
    z->vy = 0x10000;
    z->ay = 0x2800;
}

/* sub_080928FC: at the start of walk frames 1 and 5 the soldier faces the
 * player, then attacks him at close range (an 80 x 70 box ahead, one
 * chance in two: animation 1), or throws a grenade (one chance in four,
 * a 220 x 70 box: animation 2), or steps at 0.25 toward its facing; frames
 * 0 and 4 stop it. It moves with the walker collision of the zombie. */
static void soldier_walk(AosEnemy *z, const AosSoma *soma, const AosCollision *layer,
                         const AosAnimSet *anims, uint32_t (*random)(void)) {
    if (z->step != 0) return;
    uint8_t frame = z->anim.frame;
    bool frame_start = z->anim.tick == 0;
    if ((frame == 1 || frame == 5) && frame_start) {
        soldier_face(z, soma);
        int32_t x = z->x >> 16, y = z->y >> 16;
        if (player_near(soma, (int16_t)(x - 0x28), (int16_t)(y - 0x40), 0x50, 0x46) &&
            (random() & 0x7F) <= 0x3F) {
            z->state = 1;
            z->step = 0;
            play_loop(z, anims, 1, false);
            return;
        }
        if ((random() & 0x7F) <= 0x1F &&
            player_near(soma, (int16_t)(x - 0x6E), (int16_t)(y - 0x40), 0xDC, 0x46)) {
            z->state = 1;
            z->step = 0xA;
            play_loop(z, anims, 2, false);
            return;
        }
        z->vx = z->mirrored ? 0x4000 : -0x4000;
        z->vy = 0x10000;
        z->ay = 0x2800;
    }
    if ((frame == 0 || frame == 4) && frame_start) z->vx = 0;
    walk_collide(z, layer, 0x10000, (int32_t)0xFFE00000, 0x80000, 8);
}

/* sub_08092A28: the close attack (sound 0x85 at frame 2) and the throw: at
 * the start of frame 10 a grenade leaves 16 pixels ahead and 37 up, at
 * vy -2.0 and vx = (player x - its x) / 44 frames, at most 1.25. */
static void soldier_attack(AosEnemy *z, const AosSoma *soma, const AosAnimSet *anims,
                           AosHitReport *report) {
    switch (z->step) {
    case 0:
        z->step = 1;
        break;
    case 1:
        if (z->anim.frame == 2 && z->anim.tick == 0) aos_audio_play_song(0x85);
        if (z->cycle_done) soldier_walk_again(z, anims);
        break;
    case 0xA:
        soldier_face(z, soma);
        if (z->anim.frame == 10 && z->anim.tick == 0) {
            int32_t offset = z->mirrored ? 0x100000 : (int32_t)0xFFF00000;
            int32_t vx = (soma->x - (z->x + offset)) / 0x2C;
            if (z->mirrored ? vx > 0x13FFF : vx <= (int32_t)0xFFFEC000)
                vx = z->mirrored ? 0x14000 : (int32_t)0xFFFEC000;
            report->children[report->child_count++] =
                make_grenade(z, offset, (int32_t)0xFFDB0000, vx, (int32_t)0xFFFE0000);
            z->step = 0xB;
        }
        break;
    case 0xB:
        if (z->cycle_done) soldier_walk_again(z, anims);
        break;
    default:
        break;
    }
}

/* The X speed of a gib: 0.5 to 1.0, backward when mirrored. */
static int32_t gib_speed(const AosEnemy *z, uint32_t (*random)(void)) {
    int32_t r = (int32_t)(random() & 0x7FFF);
    return z->mirrored ? -0x8000 - r : r + 0x8000;
}

/* EnemyZombieSoldierUpdate state 2 at timer 0x28: two gibs, frames 0x13
 * (dx -8..8, dy -1..-17, vy -1.5..-3) and 0x14 (dy 0..-24), the random
 * draws in the order dx, dy, vx, vy. */
static void soldier_gibs(const AosEnemy *z, uint32_t (*random)(void), AosHitReport *report) {
    for (int i = 0; i < 2; ++i) {
        int32_t dx = (int32_t)(random() & 0xFFFFF) - 0x80000;
        int32_t dy = i == 0 ? -0x10000 - (int32_t)(random() & 0xFFFFF)
                            : -(int32_t)(random() % 0x180000u);
        int32_t vx = gib_speed(z, random);
        int32_t vy = -(int32_t)(random() % 0x18000u) - 0x18000;
        if (report->child_count < AOS_ENEMY_MAX_CHILDREN)
            report->children[report->child_count++] =
                make_gib(z, dx, dy, vx, vy, (int16_t)(i == 0 ? 0x13 : 0x14));
    }
}

/* sub_08093078: sub_0806D3D8(e, 8.0) (velocity, clamped, then position),
 * deleted below the soldier's y. */
static void gib_update(AosEnemy *g) {
    g->vx += g->ax;
    if (g->ax >= 0 ? g->vx > 0x80000 : g->vx < -0x80000) g->vx = g->ax >= 0 ? 0x80000 : -0x80000;
    g->vy += g->ay;
    if (g->ay >= 0 ? g->vy > 0x80000 : g->vy < -0x80000) g->vy = g->ay >= 0 ? 0x80000 : -0x80000;
    g->x += g->vx;
    g->y += g->vy;
    if (g->y > g->walk_timer) g->removed = true;
}

/* EnemyZombieSoldierUpdate: within the activity window of sub_0806CC20,
 * state 0 walks, 1 attacks, 2 dies (gibs on its first frame, then the
 * generic death), then the animation step and the on-screen collision
 * pass; state 3 only steps its animation. */
static AosHitReport soldier_update(AosEnemy *z, AosSoma *soma, const AosCollision *layer,
                                   const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                   const AosWeaponFrames *weapon_frames, int soma_atk,
                                   int soma_def, int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    int sx = (z->x >> 16) - cam_x, sy = (z->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
    switch (z->state) {
    case 0: soldier_walk(z, soma, layer, kind->anims, random); break;
    case 1: soldier_attack(z, soma, kind->anims, &spawned); break;
    case 2:
        if (z->timer == 0x28) soldier_gibs(z, random, &spawned);
        generic_death(z, kind, random, &spawned);
        break;
    case 3:
        step_anim(z, kind->anims);
        return report;
    default: break;
    }
    if (!z->removed) {
        step_anim(z, kind->anims);
        sx = (z->x >> 16) - cam_x;
        sy = (z->y >> 16) - cam_y;
        if (!z->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
            report = collide(z, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        aos_combat_tick(&z->combat);
    }
    report.child_count = spawned.child_count;
    memcpy(report.children, spawned.children, sizeof spawned.children[0] * spawned.child_count);
    take_effects(&report, &spawned);
    return report;
}

/* sub_0806D3D8(e, limit): velocity plus acceleration, clamped toward the
 * acceleration's sign, then the move. */
static void accel_move(AosEnemy *e, int32_t limit) {
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
    int32_t dx = (e->x - soma->x) >> 16, dy = (e->y - soma->y) >> 16;
    return (uint32_t)isqrt((uint32_t)(dx * dx + dy * dy)) << 16 <= range;
}

/* sub_080696B0(e, tx, ty, speed): v = delta * speed / distance (whole
 * pixels), then the move; true once the distance is within `speed`. A
 * zero distance (a division by zero natively) leaves it still. */
static bool head_toward(AosEnemy *e, int32_t tx, int32_t ty, int32_t speed) {
    int32_t dx = pixel_delta(tx, e->x), dy = pixel_delta(ty, e->y);
    int32_t distance = (int32_t)isqrt((uint32_t)(dx * dx + dy * dy));
    e->vx = distance ? dx * speed / distance : 0;
    e->vy = distance ? dy * speed / distance : 0;
    e->x += e->vx;
    e->y += e->vy;
    return (distance << 16) <= speed;
}

/* sub_0806E7A8: step 0 faces the player and waits until he is within 120
 * pixels or it is hurt; step 1 heads at 2.0 for the point 64 pixels in
 * front of him and 48 above, attacking (state 1) when it gets there or
 * when it is within 60 pixels of him on both axes and more than 32 above. */
static void valkyrie_hover(AosEnemy *v, const AosSoma *soma) {
    soldier_face(v, soma);
    if (v->step == 0) {
        if (player_in_range(v, soma, 0x780000) || v->hp < v->stats.hp) v->step = 1;
        return;
    }
    int32_t tx = soma->x + (v->mirrored ? (int32_t)0xFFC00000 : 0x400000);
    if (head_toward(v, tx, soma->y + (int32_t)0xFFD00000, 0x20000)) {
        v->state = 1;
        v->step = 0;
        return;
    }
    int32_t dx = soma->x - v->x, dy = soma->y - v->y;
    if ((dx < 0 ? -dx : dx) <= 0x3BFFFF && v->y < soma->y - 0x200000 &&
        (dy < 0 ? -dy : dy) <= 0x3BFFFF) {
        v->state = 1;
        v->step = 0;
    }
}

/* sub_0806E8AC: animation 1 once; it slows (5/6) and holds frame 2 for 16
 * frames, then (frame 3, sound 0x87) charges at 3.0 forward and 2.5 down,
 * pulled up by 0x4000 from frame 8, plays animation 0 when the attack
 * ends, slows by 31/32 for 20 frames (or until the player leaves 120
 * pixels), then hovers back (7/8) for 48 frames to state 0, step 1. All
 * moves go through sub_0806D3D8(e, 3.0). */
static void valkyrie_attack(AosEnemy *v, const AosSoma *soma, const AosAnimSet *anims) {
    switch (v->step) {
    case 0:
        play_loop(v, anims, 1, false);
        v->step = 1;
        v->ax = v->ay = 0;
        v->timer = 0;
        /* fall through */
    case 1:
        v->vx = v->vx * 5 / 6;
        v->vy = v->vy * 5 / 6;
        accel_move(v, 0x30000);
        if (v->anim.frame != 2) return;
        if (v->timer++ > 0xF) {
            v->anim.tick = 0x64;
            v->step = 2;
        } else {
            v->anim.tick = 0;
        }
        return;
    case 2:
        v->vx = v->vx * 5 / 6;
        v->vy = v->vy * 5 / 6;
        accel_move(v, 0x30000);
        if (v->anim.frame <= 2) return;
        v->step = 3;
        aos_audio_play_song(0x87);
        v->vx = v->mirrored ? 0x30000 : -0x30000;
        v->vy = 0x28000;
        v->ax = v->ay = 0;
        /* fall through */
    case 3:
        accel_move(v, 0x30000);
        if (v->anim.frame <= 7) return;
        v->step = 4;
        v->ay = -0x4000;
        return;
    case 4:
        accel_move(v, 0x30000);
        if (!v->cycle_done) return;
        v->step = 5;
        play(v, anims, 0);
        v->walk_timer = 0;
        return;
    case 5:
        accel_move(v, 0x30000);
        v->vx = v->vx * 31 / 32;
        if (!player_in_range(v, soma, 0x780000)) {
            v->step = 6;
            v->walk_timer = 0;
            v->ay = 0;
        }
        if (++v->walk_timer > 0x13) {
            v->step++;
            v->walk_timer = 0;
            v->ay = 0;
        }
        return;
    case 6:
        soldier_face(v, soma);
        v->vx = v->vx * 7 / 8;
        v->vy = v->vy * 7 / 8;
        accel_move(v, 0x30000);
        if (++v->walk_timer > 0x2F) {
            v->state = 0;
            v->step = 1;
        }
        return;
    default:
        return;
    }
}

/* sub_0806EA8C: the recoil, 3.0 up and away from the player, slowing by
 * 11/12, facing him again below -1.25 and back to state 0 above -0.125. */
static void valkyrie_recoil(AosEnemy *v, const AosSoma *soma) {
    if (v->step == 0) {
        v->mirrored = !((v->x >> 16) < (soma->x >> 16));
        v->vx = v->mirrored ? 0x30000 : -0x30000;
        v->vy = -0x30000;
        v->step = 1;
        return;
    }
    v->vx = v->vx * 11 / 12;
    v->vy = v->vy * 11 / 12;
    v->x += v->vx;
    v->y += v->vy;
    if (v->vy > (int32_t)0xFFFEC000) soldier_face(v, soma);
    if (v->vy > (int32_t)0xFFFFE000) v->state = v->step = 0;
}

/* EnemyValkyrieUpdate (no activity window): sub_0806AD24 stuns it; state
 * 0 hovers, 1 attacks, 2 dies (9/10 of vx, falling, the generic death), 3
 * recoils, 4 only steps its animation; then the animation step and the
 * collision pass. */
static AosHitReport valkyrie_update(AosEnemy *v, AosSoma *soma, const AosEnemyKind *kind,
                                    const AosWeaponEntity *weapon,
                                    const AosWeaponFrames *weapon_frames, int soma_atk,
                                    int soma_def, int cam_x, int cam_y,
                                    uint32_t (*random)(void)) {
    AosHitReport report = {0}, effects = {0};
    if (v->hp > 0 && v->hit_flash) {
        v->hit_flash--;
        if (v->state != 2) report = collide(v, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        aos_combat_tick(&v->combat);
        return report;
    }
    switch (v->state) {
    case 0: valkyrie_hover(v, soma); break;
    case 1: valkyrie_attack(v, soma, kind->anims); break;
    case 2:
        v->vx = v->vx * 9 / 10;
        accel_move(v, 0x30000);
        generic_death(v, kind, random, &effects);
        if (v->removed) return effects;
        break;
    case 3: valkyrie_recoil(v, soma); break;
    case 4:
        step_anim(v, kind->anims);
        return report;
    default:
        break;
    }
    step_anim(v, kind->anims);
    int sx = (v->x >> 16) - cam_x, sy = (v->y >> 16) - cam_y;
    if (!v->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
        report = collide(v, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    aos_combat_tick(&v->combat);
    take_effects(&report, &effects);
    return report;
}

/* sub_0806AC5C: a walking speed halved (slope type 1) or cut to 2/3 (type
 * 2) when climbing the slope under the entity (the cell at its feet, or
 * one pixel lower when empty; bit 2 the slope's direction). */
static int32_t slope_speed(const AosEnemy *e, const AosCollision *layer, int32_t speed) {
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

/* sub_0806B1A0: no floor 8 pixels below an empty cell ahead (a drop), or a
 * wall ahead that is not a slope. */
static bool blocked_ahead(const AosCollision *layer, int32_t x, int32_t y) {
    if (!aos_collision_cell(layer, x, y)) {
        if (!aos_collision_cell(layer, x, y + 8)) return true;
        return aos_collision_cell(layer, x, y - 7) != 0;
    }
    uint8_t above = aos_collision_cell(layer, x, y - 7);
    return above && !(above & 0xC0);
}

/* The room width in cell columns as sub_0806D188 / sub_0806B120 count it
 * (30 for a one-screen room). */
static int room_columns(const AosCollision *layer) {
    return layer->width_cells == 32 ? 0x1E : layer->width_cells;
}

/* sub_0806D188(e, 1): the entity's cell column outside [1, width - 1]
 * (a one-screen room counts 30 columns). */
static bool at_room_edge(const AosEnemy *e, const AosCollision *layer) {
    int32_t column = (e->x >> 16) >> 3;
    int width = room_columns(layer);
    return column < 1 || column > width - 1;
}

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

static int ground_walk(AosEnemy *e, const AosSoma *soma, const AosCollision *layer,
                       const AosProbes *p, int32_t speed, int mode);

/* sub_0806E29C(e, w, h): the player in the w x h box centred on it. */
static bool player_in_centred(const AosEnemy *e, const AosSoma *soma, int w, int h) {
    return player_in(soma, (int16_t)((e->x >> 16) - w / 2), (int16_t)((e->y >> 16) - h / 2), w, h);
}

/* sub_080B72F8: every frame the probe walker sub_0806CAF8 at + 0x18
 * (table 0x08528830, mode 4); step 1 waits for the player in the centred
 * 100 x 60 box at walk frame 1, tick 1, then charges (animation 1, step
 * 2); steps 1 and 2 raise + 0x18 by 0x4000 up to 2.0 (sound 0x7D every 25
 * frames on screen) and count + 0x1C: past 100 (sound 0x127) it brakes
 * (animation 2, step 3), losing 0x1000 per frame, raising dust every 4
 * frames (sub_08045CEC animation 4 or 5, row 6, vx 0.5 backward), then
 * turns around and walks again when the animation ends. */
static void warrior_walk(AosEnemy *w, const AosSoma *soma, const AosCollision *layer,
                         const AosEnemyKind *kind, uint32_t (*random)(void),
                         AosHitReport *report) {
    ground_walk(w, soma, layer, &kind->probes, w->walk_timer, 4);
    switch (w->step) {
    case 0:
        w->walk_timer = 0;
        w->substep = 0;
        w->step = 1;
        /* fall through */
    case 1:
        if (player_in_centred(w, soma, 0x64, 0x3C) && w->anim.frame == 0 && w->anim.tick == 1) {
            play(w, kind->anims, 1);
            w->step = 2;
        }
        /* fall through */
    case 2:
        w->timer++;
        if (w->walk_timer <= 0x1FFFF) w->walk_timer += 0x4000;
        w->shot++;
        if (w->anim.frame == 1 && w->anim.tick == 1) w->substep++;
        if (w->shot > 0x64) {
            play_loop(w, kind->anims, 2, false);
            w->step = 3;
            w->timer = 0;
        }
        return;
    case 3:
        if (w->walk_timer > 0) w->walk_timer -= 0x1000;
        if ((++w->timer & 3) == 0) {
            uint8_t art = (random() & 1) ? AOS_EFFECT_SMOKE : AOS_EFFECT_DUST;
            AosEffect *dust = spawn_drift(art, w->x, w->y, report);
            if (dust) dust->vx = w->mirrored ? -0x8000 : 0x8000;
        }
        if (w->anim.flags & AOS_ANIM_ENDED) {
            w->shot = 0;
            play(w, kind->anims, 0);
            w->step = 0;
            w->mirrored = !w->mirrored;
        }
        return;
    default:
        return;
    }
}

/* sub_080B747C: animation 3, thrown back at 2.5 and up at 2.5 with gravity
 * 0x2000 (sub_0806D3D8(e, 8.0)), flashing for 40 frames (the palette bits
 * are not ported), sound 0x6E every 8 frames and a fire of sub_08045CEC
 * (animation 1, row 7) every 4 frames within 8 pixels of it, 32 higher;
 * deleted beyond sub_08068AD4(e, -128, -64, 0x1F0, 0x120) or when the
 * animation ended within 128 frames. Returns true when deleted. */
static bool warrior_die(AosEnemy *w, const AosEnemyKind *kind, int cam_x, int cam_y,
                        uint32_t (*random)(void), AosHitReport *report) {
    if (w->step == 0) {
        play_loop(w, kind->anims, 3, false);
        w->step = 1;
        w->vx = w->mirrored ? 0x28000 : -0x28000;
        w->vy = -0x28000;
        w->ax = 0;
        w->ay = 0x2000;
        w->timer = 0;
    }
    w->timer++;
    if ((w->timer & 3) == 0) {
        int32_t x = w->x + (int32_t)((uint32_t)random_centred(random, 16) << 16);
        int32_t y = w->y + (int32_t)((uint32_t)(random_centred(random, 16) - 0x20) << 16);
        spawn_drift(AOS_EFFECT_EMBER, x, y, report);
    }
    accel_move(w, 0x80000);
    int sx = (w->x >> 16) - cam_x, sy = (w->y >> 16) - cam_y;
    bool on_screen = (uint16_t)(sx + 0x80) <= 0x1F0 && (uint16_t)(sy + 0x40) <= 0x120;
    return !on_screen || ((w->anim.flags & AOS_ANIM_ENDED) && w->timer <= 0x80);
}

/* EnemyDeadWarriorUpdate: inside the activity window, hidden above screen
 * Y -16 (sub_0806E4F8); state 0 walks, 1 dies, 2 only steps its
 * animation; then the collision pass before the animation step. */
static AosHitReport warrior_update(AosEnemy *w, AosSoma *soma, const AosCollision *layer,
                                   const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                   const AosWeaponFrames *weapon_frames, int soma_atk,
                                   int soma_def, int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, effects = {0};
    int sx = (w->x >> 16) - cam_x, sy = (w->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
    w->hidden = sy < -16;
    switch (w->state) {
    case 0: warrior_walk(w, soma, layer, kind, random, &effects); break;
    case 1:
        if (warrior_die(w, kind, cam_x, cam_y, random, &effects)) {
            w->removed = true;
            return effects;
        }
        break;
    case 2:
        step_anim(w, kind->anims);
        return report;
    default:
        break;
    }
    sx = (w->x >> 16) - cam_x;
    sy = (w->y >> 16) - cam_y;
    if (!w->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
        report = collide(w, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    step_anim(w, kind->anims);
    aos_combat_tick(&w->combat);
    take_effects(&report, &effects);
    return report;
}

/* sub_080698A8(e, floor_vy, head, limit): sub_08069770 with its own limit,
 * moving each axis only when its speed or acceleration is set; the ceiling
 * only while rising, the floor only while falling (subpixel cleared, vy =
 * floor_vy). */
static int fall_collide(AosEnemy *e, const AosCollision *layer, int32_t floor_vy, int32_t head,
                        int32_t limit) {
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

/* sub_080992C8: the Mimic's trigger, a hidden entity at its position with
 * the hurtbox of the bytes 0x100CF8FA (-6, -8, 12, 16), no attack box;
 * its common sprite (animation 3, row 7) and its draw routine sub_08046100
 * are not drawn. */
static AosEnemy mimic_trigger(const AosEnemy *m) {
    AosEnemy t = {0};
    t.id = m->id;
    t.role = AOS_ROLE_MIMIC_TRIGGER;
    t.static_frame = -1;
    t.x = m->x;
    t.y = m->y;
    t.hidden = true;
    t.hp = 1;
    t.combat.type = AOS_TYPE_ENEMY;
    t.combat.attack_off = true;
    t.own_boxes = true;
    t.own_hurt = t.own_attack = (AosBox){-6, -8, 12, 16};
    return t;
}

/* sub_0809937C: the trigger collides until struck; then, hidden, it sends
 * flames of sub_0806D5C0 (8 x 8) every 4 frames while its timer is above
 * 30 and is deleted after 41 frames. */
static AosHitReport trigger_update(AosEnemy *t, AosSoma *soma, const AosEnemyKind *kind,
                                   const AosWeaponEntity *weapon,
                                   const AosWeaponFrames *weapon_frames, int soma_atk,
                                   int soma_def, uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (t->state == 0) {
        report = collide(t, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        aos_combat_tick(&t->combat);
        return report;
    }
    t->hidden = true;
    if (t->timer > 0x1E && (t->timer & 3) == 0)
        spawn_flame_at(t->x, t->y, 0x80000, 0x80000, random, &report);
    if (--t->timer == 0xFF) t->removed = true;
    return report;
}

/* sub_080990D8: step 0 waits hidden without collisions (its trigger made
 * on the first update); woken (step 1) it shows itself and falls as a
 * closed chest (step 2: sprite frame 0, sub_080698A8(e, 1.0, -16, 4.0)
 * with + 0x54 0x2000) until it lands; step 3 sits closed until the player
 * is in (x - 16, y - 20, 32, 20), then opens (animation 0 once, step 4)
 * and plays animation 1 (step 5), hidden beyond margin entry 4. Returns
 * whether its collisions are off this frame (+ 0x18 bit 0). */
static bool mimic_act(AosEnemy *m, const AosSoma *soma, const AosCollision *layer,
                      const AosEnemyKind *kind, int cam_x, int cam_y, AosHitReport *report) {
    switch (m->step) {
    case 0:
        if (!m->dropped) {
            m->dropped = true;
            if (report->child_count < AOS_ENEMY_MAX_CHILDREN)
                report->children[report->child_count++] = mimic_trigger(m);
        }
        m->hidden = true;
        return true;
    case 1:
        m->step = 2;
        m->hidden = false;
        m->ay = 0x2000;
        /* fall through */
    case 2:
        m->static_frame = 0;
        if (fall_collide(m, layer, 0x10000, (int32_t)0xFFF00000, 0x40000) & 4) {
            m->step = 3;
            m->own_boxes = true;
            m->own_hurt = m->own_attack = (AosBox){-8, -14, 16, 14};
            m->combat.attack_off = true;
            m->combat.hurt_off = false;
        }
        return true;
    case 3:
        m->static_frame = 0;
        if (player_in(soma, (int16_t)((m->x >> 16) - 0x10), (int16_t)((m->y >> 16) - 0x14), 0x20,
                      0x14)) {
            m->static_frame = -1;
            m->own_boxes = false;
            m->combat.attack_off = false;
            play_loop(m, kind->anims, 0, false);
            m->step = 4;
        }
        return false;
    case 4:
        if (m->cycle_done) {
            play(m, kind->anims, 1);
            m->step = 5;
        }
        return false;
    case 5:
        m->hidden = outside_margins(kind, 4, (m->x >> 16) - cam_x, (m->y >> 16) - cam_y);
        return false;
    default:
        return false;
    }
}

/* EnemyMimicUpdate: inside the activity window, state 0 acts, 1 runs the
 * generic death, 2 (inert) plays animation 1 once animation 0 ends; then
 * the animation step (a closed chest holds sprite frame 0) and, unless
 * dying or with collisions off, the collision pass. */
static AosHitReport mimic_update(AosEnemy *m, AosSoma *soma, const AosCollision *layer,
                                 const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                 const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
                                 int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    int sx = (m->x >> 16) - cam_x, sy = (m->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
    bool quiet = false;
    switch (m->state) {
    case 0: quiet = mimic_act(m, soma, layer, kind, cam_x, cam_y, &spawned); break;
    case 1:
        generic_death(m, kind, random, &spawned);
        if (m->removed) return spawned;
        break;
    case 2:
        if (m->step == 0 && m->cycle_done) {
            play(m, kind->anims, 1);
            m->step = 1;
        }
        step_anim(m, kind->anims);
        return report;
    default:
        break;
    }
    if (m->static_frame < 0) step_anim(m, kind->anims);
    if (m->state != 1 && !quiet)
        report = collide(m, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    aos_combat_tick(&m->combat);
    report.child_count = spawned.child_count;
    memcpy(report.children, spawned.children, sizeof spawned.children[0] * spawned.child_count);
    take_effects(&report, &spawned);
    return report;
}

/* Sets the Fleaman's animation frame by hand, paused (+ 0x6E, + 0x6F, + 0x6C
 * bit 1). */
static void flea_frame(AosEnemy *f, uint8_t frame) {
    f->anim.frame = frame;
    f->anim.tick = 0;
    f->anim.flags |= 2;
}

/* sub_080C1FBC: step 0 sets animation 0 (paused) and vx 0.5 toward its
 * facing, then step 1 (a carried Fleaman, waiting for whatever carries it:
 * the check of its slot record never passes) or 4. Step 2 falls (animation
 * 3, frame 3) until its cell is solid, snaps to the floor and rests 16
 * frames (step 3). Step 4 (on the ground) moves, faces the player and
 * jumps by r % 30: below 10 a hop (0.8125, -3.0, 0xA000, animation 0,
 * step 6), 10 a long jump (1.625, -3.0, 0x2800), 11 to 13 a high one
 * (0.8125, -5.0, 0x3C00), otherwise straight up (-3.0, 0x5000, frame 1);
 * the last three play animation 3 (step 5), paused. In the air it falls
 * (step 5 sets frames 1 to 3 by vy), is pushed out of ceilings
 * (sub_08001C1C at y - 10) and walls (8 pixels ahead, 8 up) and lands on a
 * solid cell (step 4, frame 0). */
static void flea_hop(AosEnemy *f, const AosSoma *soma, const AosCollision *layer,
                     const AosAnimSet *anims, uint32_t (*random)(void)) {
    switch (f->step) {
    case 0:
        play(f, anims, 0);
        f->anim.flags |= 2;
        f->vx = f->mirrored ? 0x8000 : -0x8000;
        f->step = f->dropped ? 1 : 4;
        f->vy = f->ay = 0x2000;
        return;
    case 1:
        f->vy = f->ay = 0x2000;
        return;
    case 2:
        if (f->anim.id != 3) {
            play(f, anims, 3);
            flea_frame(f, 3);
        }
        f->x += f->vx;
        f->y += f->vy;
        f->vy += f->ay;
        if (!aos_collision_cell(layer, f->x >> 16, f->y >> 16)) return;
        snap_to_floor(f, layer);
        f->step = 3;
        f->timer = 0;
        return;
    case 3:
        flea_frame(f, 0);
        if (f->timer++ <= 0xF) return;
        f->step = 4;
        play(f, anims, 0);
        return;
    case 4: {
        f->x += f->vx;
        f->mirrored = (f->x >> 16) < (soma->x >> 16);
        uint32_t pick = random() % 0x1E;
        if (pick <= 9) {
            f->vx = 0xD000;
            f->vy = -0x30000;
            f->ay = 0xA000;
            play(f, anims, 0);
            f->step = 6;
        } else {
            if (pick == 10) {
                f->vx = 0x1A000;
                f->vy = -0x30000;
                f->ay = 0x2800;
            } else if (pick <= 13) {
                f->vx = 0xD000;
                f->vy = -0x50000;
                f->ay = 0x3C00;
            } else {
                f->vx = 0;
                f->vy = -0x30000;
                f->ay = 0x5000;
            }
            play(f, anims, 3);
            if (pick > 13) flea_frame(f, 1);
            f->anim.flags |= 2;
            f->step = 5;
        }
        if (!f->mirrored) f->vx = -f->vx;
        return;
    }
    case 5:
    case 6: {
        f->x += f->vx;
        f->y += f->vy;
        f->vy += f->ay;
        if (f->step == 5) {
            if (-f->ay <= f->vy && f->vy <= f->ay) flea_frame(f, 2);
            else if (f->anim.frame == 0) flea_frame(f, 1);
            else if (f->anim.frame == 2) flea_frame(f, 3);
        }
        int32_t x = f->x >> 16, y = f->y >> 16;
        int depth = aos_ceiling_depth(layer, x, y - 10, 0, false);
        if (depth) f->y += (int32_t)((uint32_t)depth << 16);
        if (f->vx > 0) {
            int push = aos_wall_push_left(layer, x + 8, y - 8);
            if (push) f->x += (int32_t)((uint32_t)push << 16);
        }
        if (f->vx < 0) {
            int push = aos_wall_push_right(layer, x - 8, y - 8);
            if (push) f->x += (int32_t)((uint32_t)push << 16);
        }
        if (aos_collision_cell(layer, f->x >> 16, f->y >> 16)) {
            if (f->step == 5) {
                f->anim.frame = 0;
                f->anim.tick = 0;
            }
            f->step = 4;
        }
        return;
    }
    default:
        return;
    }
}

/* EnemyFleamanUpdate: state 3 steps animation 0; otherwise sub_0806AE54
 * counts + 0x2D down and while it is set only the collision pass runs
 * (not while dying); state 1 hops, 0xFF dies (sub_080C2388: sound 0x72,
 * animation 2, then sub_0806BE74(e, 15, 8.0, 6.0) from timer 50), then
 * the animation step and, unless dying, the collision pass. Contact
 * sub_080C242C: type 0. */
static AosHitReport flea_update(AosEnemy *f, AosSoma *soma, const AosCollision *layer,
                                const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
                                uint32_t (*random)(void)) {
    AosHitReport report = {0}, effects = {0};
    if (f->state == 3) {
        step_anim(f, kind->anims);
        return report;
    }
    if (f->state != 0xFF && f->hit_flash) f->hit_flash--;
    if (f->hit_flash && f->state != 0xFF) {
        report = collide(f, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        aos_combat_tick(&f->combat);
        return report;
    }
    if (f->state == 1) {
        flea_hop(f, soma, layer, kind->anims, random);
    } else if (f->state == 0xFF) {
        if (f->step == 0) {
            aos_audio_play_song(0x72);
            play(f, kind->anims, 2);
            f->timer = 0x32;
            f->vx = f->ax = f->vy = 0;
            f->ay = 0x2000;
            f->step = 2;
        }
        if (death_flames(f, kind, 15, 0x80000, 0x60000, random, &effects)) return effects;
    }
    step_anim(f, kind->anims);
    if (f->state != 0xFF) report = collide(f, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    aos_combat_tick(&f->combat);
    take_effects(&report, &effects);
    return report;
}

/* The skeleton knight's turn to the player, then walking (or backing)
 * at the given pace with vy 1.0 and + 0x54 = 0x2800. */
static void knight_turn(AosEnemy *k, const AosSoma *soma, uint8_t step) {
    soldier_face(k, soma);
    k->step = step;
    k->timer = 0;
    k->vy = 0x10000;
}

/* sub_0808ECF8: step 0 waits for the player in (x - 90, y - 48, 180, 96)
 * or a wound, then walks (animation 1); step 1 advances at 0.5, facing him
 * again every 60 frames; on the floor, with him in (x - 40, y - 40, 80,
 * 80) it attacks (r & 0x7F <= 0x40) or backs away (step 2), at the room's
 * edge (sub_0806B120(e, 1)) it backs away, and beyond the screen margins it
 * waits again. Step 2 backs away at 0.5 and advances again once he leaves
 * (x - 60, y - 40, 120, 80), after 60 frames, at a wall or the room's edge,
 * or attacks him within reach after 60 frames. Moves use sub_08069A00(e,
 * 1.0, -32, 8.0, 8). */
static void knight_walk(AosEnemy *k, const AosSoma *soma, const AosCollision *layer,
                        const AosEnemyKind *kind, int cam_x, int cam_y, uint32_t (*random)(void)) {
    int32_t x = k->x >> 16, y = k->y >> 16;
    if (k->step == 0) {
        if (!player_near(soma, (int16_t)(x - 0x5A), (int16_t)(y - 0x30), 0xB4, 0x60) &&
            k->hp >= k->stats.hp)
            return;
        k->step = 1;
        k->timer = 0;
        play(k, kind->anims, 1);
        k->vy = 0x10000;
        k->ay = 0x2800;
        return;
    }
    if (k->timer++ > 0x3B) {
        k->timer = 0;
        soldier_face(k, soma);
    }
    bool forward = k->step == 1;
    k->vx = (k->mirrored == forward) ? 0x8000 : -0x8000;
    int result = walk_collide(k, layer, 0x10000, (int32_t)0xFFE00000, 0x80000, 8);
    k->ay = 0x2800;
    if (!(result & 4)) return;
    x = k->x >> 16;
    y = k->y >> 16;
    bool close = player_near(soma, (int16_t)(x - 0x28), (int16_t)(y - 0x28), 0x50, 0x50);
    if (forward) {
        if (close) {
            if ((random() & 0x7F) <= 0x40) {
                k->state = 1;
                k->step = 0;
                k->timer = 0;
                return;
            }
            knight_turn(k, soma, 2);
        } else if (clamp_to_room(k, layer, 1)) {
            knight_turn(k, soma, 2);
        } else if (outside_margins(kind, 4, x - cam_x, y - cam_y)) {
            play(k, kind->anims, 0);
            k->step = 0;
        }
        return;
    }
    if (!player_near(soma, (int16_t)(x - 0x3C), (int16_t)(y - 0x28), 0x78, 0x50) ||
        k->timer > 0x3B) {
        knight_turn(k, soma, 1);
    } else if (clamp_to_room(k, layer, 1) || (result & 3)) {
        knight_turn(k, soma, 1);
    } else if (close && k->timer > 0x3B && (random() & 0x7F) <= 0x40) {
        k->state = 1;
        k->step = 0;
        k->timer = 0;
    }
}

/* sub_0808EFD8: it faces the player and plays animation 2 for 45 frames,
 * animation 3 for 31 (sound 0x85 at its 12th), animation 4 for 21, then
 * walks again (animation 1, step 1). */
static void knight_attack(AosEnemy *k, const AosSoma *soma, const AosAnimSet *anims) {
    switch (k->step) {
    case 0:
        soldier_face(k, soma);
        play_loop(k, anims, 2, false);
        k->step = 1;
        k->timer = 0;
        return;
    case 1:
        if (k->timer++ <= 0x2B) return;
        play_loop(k, anims, 3, false);
        k->step = 2;
        k->timer = 0;
        return;
    case 2:
        if (k->timer++ <= 0x1D) return;
        play_loop(k, anims, 4, false);
        k->step = 3;
        k->timer = 0;
        return;
    case 3:
        if (k->timer++ <= 0x13) return;
        play(k, anims, 1);
        k->state = 0;
        k->step = 1;
        k->timer = 0;
        k->vy = 0x10000;
        k->ay = 0x2800;
        return;
    default:
        return;
    }
}

/* EnemySkeletonKnightUpdate: inside the activity window, state 0 walks, 1
 * attacks, 2 runs the generic death, 3 only steps its animation; then the
 * animation step and the collision pass. Contact sub_0808F16C: type 0. */
static AosHitReport knight_update(AosEnemy *k, AosSoma *soma, const AosCollision *layer,
                                  const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                  const AosWeaponFrames *weapon_frames, int soma_atk,
                                  int soma_def, int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, effects = {0};
    int sx = (k->x >> 16) - cam_x, sy = (k->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
    switch (k->state) {
    case 0: knight_walk(k, soma, layer, kind, cam_x, cam_y, random); break;
    case 1: knight_attack(k, soma, kind->anims); break;
    case 2:
        generic_death(k, kind, random, &effects);
        if (k->removed) return effects;
        break;
    case 3:
        step_anim(k, kind->anims);
        return report;
    default:
        break;
    }
    step_anim(k, kind->anims);
    sx = (k->x >> 16) - cam_x;
    sy = (k->y >> 16) - cam_y;
    if (!k->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
        report = collide(k, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    aos_combat_tick(&k->combat);
    take_effects(&report, &effects);
    return report;
}

/* sub_080C86A0: Killer Doll flight stays inside the room. Crossing a room
 * edge reflects that speed; a wall, floor or ceiling zeros it. Horizontal
 * contacts also turn the sprite. */
static void killer_bounds(AosEnemy *d, const AosCollision *layer) {
    int32_t x = d->x >> 16, y = d->y >> 16;
    int32_t column = x < 0 ? (x + 7) >> 3 : x >> 3;
    if (column < 0 || column >= room_columns(layer)) {
        d->vx = -d->vx;
        d->x += d->vx * 2;
        d->mirrored = !d->mirrored;
    } else {
        int push = d->vx >= 0 ? aos_wall_push_left(layer, x + 8, y - 16)
                              : aos_wall_push_right(layer, x - 8, y - 16);
        if (push) {
            d->x += (int32_t)((uint32_t)push << 16);
            d->vx = 0;
            d->mirrored = !d->mirrored;
        }
    }
    x = d->x >> 16;
    int32_t row = y < 0 ? (y + 7) >> 3 : y >> 3;
    int rows = layer->height_cells == 32 ? 0x1A : layer->height_cells;
    if (row < 0 || row >= rows) {
        d->vy = -d->vy;
        d->y += d->vy * 2;
        return;
    }
    int depth = d->vy >= 0 ? aos_floor_depth(layer, x, y + 1, 0, false)
                           : aos_ceiling_depth(layer, x, y - 32, 0, false);
    if (depth) {
        d->y += (int32_t)((uint32_t)depth << 16);
        d->vy = 0;
    }
}

/* sub_080C7CE4: + 0x14, + 0x16 and + 0x18 count down. The doll wakes
 * within 60 pixels on both axes, rises through animations 1 and 2, then enters
 * flight. A flight leg is either 1.0 horizontally away from the player
 * (one chance in four) or 0.53125 diagonally toward him. It periodically
 * pauses in animation 3, attacks toward his upper body in animation 5,
 * and lands in animation 6 once its flight timer ends. */
static void killer_act(AosEnemy *d, const AosSoma *soma, const AosCollision *layer,
                       const AosAnimSet *anims, uint32_t (*random)(void)) {
    if ((int16_t)d->attack_wait > 0) d->attack_wait--;
    if (d->leg > 0) d->leg--;
    if ((int16_t)d->flight > 0) d->flight--;
    switch (d->step) {
    case 0:
        if (d->anim.id != 0) play(d, anims, 0);
        if (player_within(d, soma, 0x3C0000) && (soma->x != d->x || soma->y != d->y)) {
            if (d->anim.id != 1) play_loop(d, anims, 1, false);
            d->timer = 0;
            d->step = 1;
        }
        break;
    case 1:
        if (d->cycle_done) {
            if (d->anim.id != 2) play_loop(d, anims, 2, false);
            d->vy = -0x4000;
            d->timer = 0;
            d->step = 2;
        }
        break;
    case 2:
        if (d->timer++ <= 0x20) break;
        if (d->anim.id != 4) play(d, anims, 4);
        d->vy = 0;
        d->timer = 0;
        d->step = 4;
        d->substep = 0;
        break;
    case 3:
        if (d->substep == 0) {
            d->vx = d->vy = d->ax = d->ay = 0;
            int dx = (int16_t)((d->x >> 16) - (soma->x >> 16));
            if ((random() & 3) == 0) {
                face_player(d, soma);
                d->mirrored = !d->mirrored;
                d->vx = 0x10000;
                if (dx < 0) d->vx = -d->vx;
            } else {
                d->vx = d->vy = 0x8800;
                if (dx > 0) d->vx = -d->vx;
                if (player_dy(d, soma) > 0) d->vy = -d->vy;
            }
            if (d->anim.id != 4) play(d, anims, 4);
            d->substep = 1;
            break;
        }
        if (!d->mirrored && d->vx > 0) d->mirrored = true;
        if (d->mirrored && d->vx < 0) d->mirrored = false;
        killer_bounds(d, layer);
        break;
    case 4:
        if (d->substep != 0) break;
        d->vx = d->vy = d->ax = d->ay = 0;
        if (d->anim.id != 3) play_loop(d, anims, 3, false);
        if (!d->cycle_done) break;
        d->step = 3;
        d->attack_wait = (uint16_t)(d->attack_wait + 0x20);
        d->flight = (uint16_t)(d->flight + 0x20);
        d->leg = (int16_t)((random() & 0xFF) + 0x20);
        face_player(d, soma);
        break;
    case 5:
        switch (d->substep) {
        case 0: {
            if (d->anim.id != 5) play_loop(d, anims, 5, false);
            int16_t dx = (int16_t)((soma->x >> 16) - (d->x >> 16));
            int16_t dy = (int16_t)((soma->y >> 16) - (d->y >> 16) - 0x10);
            uint16_t angle = aos_arctan2(dx, dy);
            d->vx = fixed_mul(aos_sine(angle + 0x14000u), 0x10000);
            d->vy = fixed_mul(aos_sine(angle + 0x10000u), 0x10000);
            int32_t frames = d->vx ? (soma->x - d->x) / d->vx : 0;
            d->timer = (uint8_t)(frames < 0 ? -frames : frames);
            d->substep = 1;
            break;
        }
        case 1: {
            uint8_t before = d->timer--;
            if (before == 0) d->substep = 2;
            break;
        }
        case 2: {
            int dx = (int16_t)((d->x >> 16) - (soma->x >> 16));
            bool ahead = d->mirrored ? dx < 0 : dx > 0;
            d->step = ahead ? 3 : 4;
            d->substep = 0;
            d->attack_wait = (uint16_t)((random() & 0x7FF) + 0x80);
            d->leg = (int16_t)(d->leg + 0x20);
            d->flight = (uint16_t)(d->flight + 0x20);
            break;
        }
        default:
            break;
        }
        break;
    case 6:
        if (d->substep == 2) {
            if (d->timer-- == 0) {
                d->attack_wait = (uint16_t)(d->attack_wait + 0x20);
                d->leg = (int16_t)(d->leg + 0x20);
                d->flight = (uint16_t)((random() & 0x3FF) + 0x200);
                if (d->anim.id != 1) play_loop(d, anims, 1, false);
                d->step = 1;
                d->substep = 0;
            }
            break;
        }
        if (d->substep == 0) d->vx = d->vy = d->ax = d->ay = 0;
        if (!aos_collision_cell(layer, d->x >> 16, d->y >> 16)) {
            d->y += 0x10000;
            break;
        }
        if (d->anim.id != 6) play_loop(d, anims, 6, false);
        d->timer = (uint8_t)((random() & 0x3F) + 0x40);
        d->substep++;
        break;
    default:
        break;
    }
    d->x += d->vx;
    d->y += d->vy;
    d->vx += d->ax;
    d->vy += d->ay;
    if (d->step != 3) return;
    if ((int16_t)d->flight == 0) {
        d->step = 6;
        d->substep = 0;
    }
    if (d->leg == 0) {
        d->step = 4;
        d->substep = 0;
    }
    if ((int16_t)d->attack_wait != 0) return;
    int dx = (int16_t)((d->x >> 16) - (soma->x >> 16));
    bool ahead = d->mirrored ? dx < 0 : dx > 0;
    if (ahead && player_within(d, soma, 0x280000)) {
        d->step = 5;
        d->substep = 0;
    }
}

/* sub_080C8914: the inert global-flag state only loops animation 0. */
static void killer_cycle(AosEnemy *d, const AosAnimSet *anims) {
    if (d->step == 0) {
        play(d, anims, 0);
        d->step = 1;
    }
    step_anim(d, anims);
}

/* EnemyKillerDollUpdate: the active AI, animation step and collision pass;
 * its hit callback sub_080C81AC creates the five pieces through
 * sub_080C8968 and waits in state 0xFF until they leave. */
static AosHitReport killer_update(AosEnemy *d, AosSoma *soma, const AosCollision *layer,
                                  const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                  const AosWeaponFrames *weapon_frames, int soma_atk,
                                  int soma_def, uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (d->state == 2) {
        killer_cycle(d, kind->anims);
        return report;
    }
    if (d->hp > 0 && d->hit_flash && d->state != 0xFF) {
        d->hit_flash--;
        report = collide(d, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        aos_combat_tick(&d->combat);
        return report;
    }
    if (d->state == 1) killer_act(d, soma, layer, kind->anims, random);
    if (d->state == 0xFF) {
        if (!d->spawned_count) d->removed = true;
        return report;
    }
    step_anim(d, kind->anims);
    report = collide(d, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    aos_combat_tick(&d->combat);
    return report;
}

/* The Killer Doll's (0x08528AC4) and Lightning Doll's (0x08528AE4)
 * pieces, 6 bytes each: x and y offsets (the y one places the glow),
 * sprite frame, vx / 0x1000, -vy / 0x1000, timer. */
static const int8_t killer_doll_pieces[5][6] = {
    {4, -8, 13, 8, 24, 60}, {4, 0, 14, 4, 24, -128}, {4, 8, 15, 0, 24, 46},
    {4, 8, 16, -4, 24, 86}, {4, 0, 17, -8, 24, 80},
};
static const int8_t lightning_doll_pieces[5][6] = {
    {4, -8, 20, 8, 24, 60}, {4, 0, 21, 4, 24, -128}, {4, 8, 22, 0, 24, 46},
    {4, 8, 23, -4, 24, 86}, {4, 0, 24, -8, 24, 80},
};

/* sub_080C9A24: five pieces of sub_080C9284 (+ 0x1D the index), set up on
 * their first update. */
static void doll_shatter(AosEnemy *e, AosHitReport *report) {
    for (int i = 0; i < 5 && report->child_count < AOS_ENEMY_MAX_CHILDREN; ++i) {
        AosEnemy p = {0};
        p.id = e->id;
        p.role = AOS_ROLE_DOLL_PIECE;
        p.variant = (uint8_t)i;
        p.static_frame = -1;
        p.x = e->x;
        p.y = e->y;
        p.mirrored = e->mirrored;
        p.hidden = true;
        p.combat.attack_off = p.combat.hurt_off = true;
        report->children[report->child_count++] = p;
        e->spawned_count++;
    }
}

/* sub_080C9284: a piece's first update shows its sprite frame at the
 * doll's position with vx, vy = -vy and + 0x54 0x1800 (vx pointing left
 * with the player to the doll's right) and lights its glow (sub_080C9058:
 * the death flame's animation looping 16 pixels above it plus the y
 * offset, flipped at random). Then it flies: at timer 0x30 a flame of
 * sub_0806D5C0 and a dust particle of sub_0806DA6C / sub_0806D7F0
 * (animation 4, row 6, vy -0.25, + 0x18 = 4) at its position (both with
 * a 0 x 0 area: two RandomNumberGenerator draws each, % 0 being 0), x +=
 * vx, y += vy, vy += 0x1800; piece 1 alone bounces on floors (y - 15,
 * vy = -vy / 2, stopped at 0x5FFF or less; sound 0x153), meets ceilings
 * (y - 32) and walls (x +/- 8, y - 16, vx = 0). It blinks on odd timers
 * up to 0x3F and stops, hidden, when its timer runs out or it falls below
 * screen row 0xC8; it is deleted once its glow has played a cycle. */
static void doll_piece_update(AosEnemy *p, const AosSoma *soma, const AosCollision *layer,
                              const AosEnemyKind *kind, int cam_y, uint32_t (*random)(void),
                              AosHitReport *report) {
    const int8_t *r = p->id == AOS_ENEMY_KILLER_DOLL ? killer_doll_pieces[p->variant]
                                                      : lightning_doll_pieces[p->variant];
    switch (p->state) {
    case 0:
        p->static_frame = (uint8_t)r[2];
        p->hidden = false;
        p->vx = r[3] * 0x1000;
        p->vy = -(r[4] * 0x1000);
        p->ay = 0x1800;
        p->timer = (uint8_t)r[5];
        if (p->x < soma->x) p->vx = -abs(p->vx);
        p->glow = (AosEffect){.kind = AOS_EFFECT_FLAME, .x = p->x, .y = p->y - 0x100000};
        p->glow.mirrored = random() & 1;
        if (kind->flame_anims && aos_anim_start(&p->glow.anim, kind->flame_anims, 0, true))
            p->glow.started = true;
        p->state = 1;
        break;
    case 1: {
        if ((p->timer & 0xF) == 0 && p->timer <= 0x3F && p->timer > 0x20) {
            spawn_flame_at(p->x + r[0], p->y + r[1], 0, 0, random, report);
            (void)random();
            (void)random();
            if (report->effect_count < AOS_ENEMY_MAX_EFFECTS)
                report->effects[report->effect_count++] = (AosEffect){
                    .kind = AOS_EFFECT_DUST, .x = p->x + r[0], .y = p->y + r[1],
                    .particle = true, .flags = 4, .vy = -0x4000};
        }
        p->x += p->vx;
        p->y += p->vy;
        p->vy += p->ay;
        if (p->variant == 1) {
            int32_t x = p->x >> 16, y = p->y >> 16;
            if (p->vy >= 0) {
                int depth = aos_floor_depth(layer, x, y - 15, 0, false);
                if (depth) {
                    p->y += (int32_t)((uint32_t)depth << 16);
                    p->vy = -p->vy / 2;
                    if (abs(p->vy) <= 0x5FFF) p->vx = p->vy = p->ax = p->ay = 0;
                }
            } else {
                int depth = aos_ceiling_depth(layer, x, y - 32, 0, false);
                if (depth) p->y += (int32_t)((uint32_t)depth << 16);
            }
            y = p->y >> 16;
            int push = 0;
            if (p->vx > 0) push = aos_wall_push_left(layer, x + 8, y - 16);
            else if (p->vx < 0) push = aos_wall_push_right(layer, x - 8, y - 16);
            if (push) {
                p->x += (int32_t)((uint32_t)push << 16);
                p->vx = 0;
            }
        }
        if ((p->timer & 1) && p->timer <= 0x3F) p->hidden = !p->hidden;
        bool done = --p->timer == 0;
        if ((int16_t)((p->y >> 16) - cam_y) > 0xC8) done = true;
        if (done) {
            p->vx = p->vy = 0;
            p->state = 2;
            p->hidden = true;
        }
        break;
    }
    case 2:
        if (p->glow.cycle_done) {
            p->removed = true;
            return;
        }
        break;
    default:
        break;
    }
    /* The glow, updated after its piece. */
    if (p->glow.started) {
        p->glow.x = p->x;
        p->glow.y = (int32_t)((uint32_t)((p->y >> 16) - 16 + r[1]) << 16) | (p->y & 0xFFFF);
        bool ended = p->glow.anim.flags & AOS_ANIM_ENDED;
        int result = aos_anim_step(&p->glow.anim, kind->flame_anims);
        if (result == 4 || (result == 3 && !ended)) p->glow.cycle_done = true;
    }
}

/* sub_080C8B3C step 2: a new drift (r % 6): 0 to 2 at 0.5 diagonally
 * toward the player, 3 turned over at 0.25 away from him, both for (r &
 * 0xFF) + 0x80 frames; 4 and 5 at 1.0 straight at his position 16 pixels
 * higher (sub_0806BAC8) until it reaches his x. */
static void doll_new_drift(AosEnemy *d, const AosSoma *soma, uint32_t (*random)(void)) {
    face_player(d, soma);
    d->vx = d->vy = d->ax = d->ay = 0;
    unsigned pattern = random() % 6;
    int dx = (int16_t)((d->x >> 16) - (soma->x >> 16));
    if (pattern <= 2) {
        d->vx = d->vy = 0x8000;
        if (dx > 0) d->vx = -d->vx;
        if (player_dy(d, soma) > 0) d->vy = -d->vy;
        d->leg = (int16_t)((random() & 0xFF) + 0x80);
    } else if (pattern == 3) {
        d->mirrored = !d->mirrored;
        d->vx = 0x4000;
        if (dx < 0) d->vx = -d->vx;
        d->leg = (int16_t)((random() & 0xFF) + 0x80);
    } else {
        int16_t ax = (int16_t)((soma->x >> 16) - (d->x >> 16));
        int16_t ay = (int16_t)((soma->y >> 16) - (d->y >> 16) - 0x10);
        uint16_t angle = aos_arctan2(ax, ay);
        d->vx = fixed_mul(aos_sine(angle + 0x14000u), 0x10000);
        d->vy = fixed_mul(aos_sine(angle + 0x10000u), 0x10000);
        int32_t frames = d->vx ? (soma->x - d->x) / d->vx : 0;
        d->leg = (int16_t)(frames < 0 ? -frames : frames);
    }
}

/* sub_080C9788: beyond the room's columns (30 for one screen) it turns
 * back (vx negated and applied twice); otherwise the wall ahead (x +/- 8,
 * y - 16) pushes it back and turns it. Beyond its rows (26 for one screen)
 * vy turns back the same way; otherwise the floor (y + 1) or the ceiling
 * (y - 32) pushes it back and turns vy. */
static void doll_bounds(AosEnemy *d, const AosCollision *layer) {
    int32_t x = d->x >> 16, y = d->y >> 16;
    int32_t column = x < 0 ? (x + 7) >> 3 : x >> 3;
    if (column < 0 || column >= room_columns(layer)) {
        d->vx = -d->vx;
        d->x += d->vx * 2;
        d->mirrored = !d->mirrored;
    } else {
        int push = d->vx >= 0 ? aos_wall_push_left(layer, x + 8, y - 16)
                              : aos_wall_push_right(layer, x - 8, y - 16);
        if (push) {
            d->x += (int32_t)((uint32_t)push << 16);
            d->vx = -d->vx;
            d->mirrored = !d->mirrored;
        }
    }
    x = d->x >> 16;
    int32_t row = y < 0 ? (y + 7) >> 3 : y >> 3;
    int rows = layer->height_cells == 32 ? 0x1A : layer->height_cells;
    if (row < 0 || row >= rows) {
        d->vy = -d->vy;
        d->y += d->vy * 2;
        return;
    }
    int depth = d->vy >= 0 ? aos_floor_depth(layer, x, y + 1, 0, false)
                           : aos_ceiling_depth(layer, x, y - 32, 0, false);
    if (depth) {
        d->y += (int32_t)((uint32_t)depth << 16);
        d->vy = -d->vy;
    }
}

/* sub_080C8B3C: + 0x14 (attack wait) and + 0x18 (flight) count down.
 * Step 0 rests (animation 0) until the player is in its 0xF0 x 0x80 box,
 * then rises (animation 1 once, step 1) and flies (step 2, + 0x5C 3):
 * drifts of doll_new_drift, facing its vx, bounded by doll_bounds. Step 3
 * charges (animation 3 once, sound 0x75) then sparks (animation 4 looping)
 * for (r & 0xFF) + 0x80 frames, then flies again with + 0x14 = (r & 0x1F)
 * + 0x10 and + 0x18 += 0x20. Step 4 (+ 0x5C 9) sinks a pixel a frame to a
 * cell, plays animation 5 for (r & 0x3F) + 0x40 frames (drawn twice) and
 * rises again with + 0x18 = (r & 0x3FF) + 0x200. Each frame it moves by
 * its speed and adds its accelerations; flying, it lands once + 0x18 runs
 * out, and with + 0x14 out and the player ahead it attacks within 20
 * pixels (sub_0806BF78), else one frame in 1024. */
static void doll_act(AosEnemy *d, const AosSoma *soma, const AosCollision *layer,
                     const AosAnimSet *anims, uint32_t (*random)(void)) {
    if (d->attack_wait) d->attack_wait--;
    if (d->flight) d->flight--;
    switch (d->step) {
    case 0:
        if (d->anim.id != 0) play(d, anims, 0);
        if (player_in_centred(d, soma, 0xF0, 0x80) && (soma->x != d->x || soma->y != d->y)) {
            if (d->anim.id != 1) play_loop(d, anims, 1, false);
            d->step = 1;
            d->substep = 0;
        }
        break;
    case 1:
        if (d->cycle_done) {
            d->timer = 0;
            d->step = 2;
            d->substep = 0;
        }
        break;
    case 2:
        if (d->substep == 0) {
            doll_new_drift(d, soma, random);
            if (d->anim.id != 2) play(d, anims, 2);
            d->substep = 1;
            break;
        }
        if (!d->mirrored && d->vx > 0) d->mirrored = true;
        if (d->mirrored && d->vx < 0) d->mirrored = false;
        if (d->leg-- <= 0) d->substep = 0;
        doll_bounds(d, layer);
        break;
    case 3:
        switch (d->substep) {
        case 0:
            if (d->anim.id != 3) play_loop(d, anims, 3, false);
            d->vx = d->vy = d->ax = d->ay = 0;
            d->timer = (uint8_t)((random() & 0xFF) + 0x80);
            d->substep = 1;
            /* fall through */
        case 1:
            if (!d->cycle_done) break;
            if (d->anim.id != 4) play(d, anims, 4);
            d->substep = 2;
            break;
        case 2:
            if (d->timer-- == 0) d->substep = 3;
            break;
        case 3:
            d->step = 2;
            d->substep = 0;
            d->attack_wait = (uint16_t)((random() & 0x1F) + 0x10);
            d->flight = (uint16_t)(d->flight + 0x20);
            break;
        default:
            break;
        }
        break;
    case 4:
        if (d->substep == 2) {
            if (--d->timer == 0xFF) {
                d->flight = (uint16_t)((random() & 0x3FF) + 0x200);
                if (d->anim.id != 1) play_loop(d, anims, 1, false);
                d->step = 1;
                d->substep = 0;
            }
            break;
        }
        if (d->substep == 0) d->vx = d->vy = d->ax = d->ay = 0;
        if (!aos_collision_cell(layer, d->x >> 16, d->y >> 16)) {
            d->y += 0x10000;
            break;
        }
        if (d->anim.id != 5) play_loop(d, anims, 5, false);
        d->timer = (uint8_t)((random() & 0x3F) + 0x40);
        d->substep++;
        break;
    default:
        break;
    }
    d->x += d->vx;
    d->y += d->vy;
    d->vx += d->ax;
    d->vy += d->ay;
    if (d->step != 2) return;
    if (d->flight == 0) {
        d->step = 4;
        d->substep = 0;
    }
    if (d->attack_wait) return;
    int dx = (int16_t)((d->x >> 16) - (soma->x >> 16));
    if (d->mirrored ? dx >= 0 : dx <= 0) return;
    if (player_within(d, soma, 0x140000) || (random() & 0x3FF) == 0) {
        d->step = 3;
        d->substep = 0;
    }
}

/* sub_080C9630: under the global 0x8E & 0x40 it cycles its animations: 0
 * to its end (or 17 frames), 1 to its end, 2 for 65 frames, 3 to its end,
 * 4 for 129 frames, 5 for 49 frames. */
static void doll_cycle(AosEnemy *d, const AosAnimSet *anims) {
    static const uint8_t loops[6] = {0, 0, 1, 0, 1, 0};
    static const uint8_t limits[6] = {0x10, 0xFF, 0x40, 0xFF, 0x80, 0x30};
    uint8_t s = d->step;
    if (s > 5) return;
    if (d->anim.id != s) play_loop(d, anims, s, loops[s]);
    bool next;
    if (s == 1 || s == 3) next = d->cycle_done;
    else if (s == 0) next = d->cycle_done || d->timer++ > limits[0];
    else next = d->timer++ > limits[s];
    if (next) {
        d->timer = 0;
        d->step = s == 5 ? 0 : (uint8_t)(s + 1);
    }
    step_anim(d, anims);
}

/* EnemyLightningDollUpdate: state 2 cycles its animations; otherwise, alive
 * and flashing from a blow (sub_0806AD24, the sound 0x75 off) it only
 * collides; state 1 acts, 0xFF waits for its pieces. Then the animation
 * step: on a new sprite frame animation 4 sets the attack box (-14, -32,
 * 30, 28) of sub_08042884 (the hurtbox kept), any other the frame's boxes;
 * then the collision pass (sub_080421AC: no on-screen test). Its contact
 * callback sub_080C99E4, during animations 3 and 4, holds a spark (common
 * animation 16, row 6: sub_080C9108 / sub_080C9198) on the player. The
 * tracking list of sub_08021924 and the sound loop 0x75 are not ported. */
static AosHitReport doll_update(AosEnemy *d, AosSoma *soma, const AosCollision *layer,
                                const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                const AosWeaponFrames *weapon_frames, int soma_atk,
                                int soma_def, uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (d->state == 2) {
        doll_cycle(d, kind->anims);
        return report;
    }
    if (d->hp > 0 && d->hit_flash && d->state != 0xFF) {
        d->hit_flash--;
        report = collide(d, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        aos_combat_tick(&d->combat);
        return report;
    }
    if (d->state == 1) doll_act(d, soma, layer, kind->anims, random);
    if (d->state == 0xFF) {
        if (!d->spawned_count) d->removed = true;
        return report;
    }
    AosBox hurt, attack;
    bool hurt_on, attack_on;
    frame_boxes(d, kind, &hurt, &attack, &hurt_on, &attack_on);
    uint8_t before = d->anim.id < AOS_ENEMY_MAX_ANIMS && d->anim.frame < AOS_ENEMY_MAX_FRAMES
                         ? kind->sprites[d->anim.id][d->anim.frame] : 0;
    step_anim(d, kind->anims);
    uint8_t after = d->anim.id < AOS_ENEMY_MAX_ANIMS && d->anim.frame < AOS_ENEMY_MAX_FRAMES
                        ? kind->sprites[d->anim.id][d->anim.frame] : 0;
    if (before != after) {
        if (d->anim.id == 4) {
            d->own_boxes = true;
            d->own_hurt = hurt;
            d->own_attack = (AosBox){-14, -32, 30, 28};
        } else {
            d->own_boxes = false;
        }
    }
    report = collide(d, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    if (report.soma_hit && (d->anim.id == 3 || d->anim.id == 4) &&
        report.effect_count < AOS_ENEMY_MAX_EFFECTS) {
        AosEffect spark = {.kind = AOS_EFFECT_SPARK, .follow = true, .x = soma->x,
                           .y = soma->y - 0x100000};
        spark.mirrored = random() & 1;
        report.effects[report.effect_count++] = spark;
    }
    aos_combat_tick(&d->combat);
    return report;
}

/* The beam skeleton's walker probes at 0x08528B04 are the kind's. Its
 * bones (0x08528B17, 6 bytes each): x and y offsets, sprite frame, vx *
 * 0x1000, vy * -0x1000 (minus 0x10); the last byte (+ 0x0D) is unused by
 * their update sub_0806C5AC. */
static const int8_t beam_bones[7][5] = {
    {0, -8, 0x1E, 20, 50}, {0, -6, 0x1B, 16, 49}, {0, -4, 0x18, 17, 48},
    {4, -6, 0x1A, 12, 56}, {4, -3, 0x19, 18, 51}, {-4, -6, 0x1D, 24, 50},
    {-4, -3, 0x1C, 12, 40},
};

/* sub_080CB1B4: seven bones of sub_0806C5AC at its position (+ 0x5C and
 * facing its own), + 0x54 0x3000; with the player to its right
 * (sub_0806BBC4 > 0) their x offsets and vx point left. */
static void beam_shatter(AosEnemy *e, const AosSoma *soma, AosHitReport *report) {
    bool right = e->x < soma->x;
    for (int i = 0; i < 7 && report->child_count < AOS_ENEMY_MAX_CHILDREN; ++i) {
        AosEnemy b = {0};
        b.id = e->id;
        b.role = AOS_ROLE_DEBRIS;
        b.static_frame = (uint8_t)beam_bones[i][2];
        b.mirrored = e->mirrored;
        b.combat.attack_off = b.combat.hurt_off = true;
        int dx = beam_bones[i][0];
        b.vx = (int32_t)(uint8_t)beam_bones[i][3] << 12;
        b.vy = -(beam_bones[i][4] << 12) - 0x10;
        b.ay = 0x3000;
        if (right) {
            b.vx = -abs(b.vx);
            dx = -abs(dx);
        }
        b.x = (int32_t)((uint32_t)((e->x >> 16) + dx) << 16) | (e->x & 0xFFFF);
        b.y = (int32_t)((uint32_t)((e->y >> 16) + beam_bones[i][1]) << 16) | (e->y & 0xFFFF);
        report->children[report->child_count++] = b;
    }
}

/* sub_080CA770: it patrols at 0.375 (the probe walker, mode 12), turning
 * 40 pixels from its home column or past the patrol bounds, at a wall or
 * the room's edge; with the player in the 0xC0 x 0x80 box centred on it,
 * it stops and closes in (state 1). */
static void beam_patrol(AosEnemy *e, const AosSoma *soma, const AosCollision *layer,
                        const AosEnemyKind *kind) {
    if (e->step == 0) {
        if (e->anim.id != 1) play(e, kind->anims, 1);
        e->step = 1;
        return;
    }
    int16_t x = (int16_t)(e->x >> 16);
    int16_t d = (int16_t)(e->home - x);
    if (e->mirrored) {
        if (e->home_right < x || d < -0x28) e->mirrored = false;
    } else if (e->home_left > x || d > 0x28) {
        e->mirrored = true;
    }
    bool turn = ground_walk(e, soma, layer, &kind->probes, 0x6000, 12) & 3;
    if (clamp_to_room(e, layer, 1)) turn = true;
    if (turn) e->mirrored = !e->mirrored;
    if (player_in_centred(e, soma, 0xC0, 0x80)) {
        e->vx = 0;
        e->state = 1;
        e->substep = e->step = 0;
    }
}

/* sub_080CA884: with the player more than 32 pixels above or below it
 * patrols again; otherwise it waits r & 0x1F frames before it may fire
 * (+ 0x1C) and steps 20 frames at a random pace ((r & 0x3FFF) + 0x8000,
 * braked by 1/16 a frame) toward the player, or away when he is in the
 * ((r & 0x7F) + 0x40) x 0x80 box centred on it. Then, the wait over and
 * the player in its 100 x 40 box, it fires (state 3); otherwise its +
 * 0x19 grows, and r % 40 below it with the player more than 48 pixels
 * away clears it and may pause (r % 50 <= 14: state 2), or halves it near
 * him, before another step. */
static void beam_close_in(AosEnemy *e, const AosSoma *soma, const AosCollision *layer,
                          const AosEnemyKind *kind, uint32_t (*random)(void)) {
    switch (e->step) {
    case 0: {
        int dy = player_dy(e, soma);
        if (abs(dy) > 0x20) {
            e->state = e->substep = e->step = 0;
            return;
        }
        e->rest = (uint16_t)(random() & 0x1F);
        e->step = 1;
        e->substep = 0;
    }
        /* fall through */
    case 1:
        face_player(e, soma);
        e->timer = 0x14;
        e->speed = (int32_t)(random() & 0x3FFF) + 0x8000;
        if (player_in_centred(e, soma, (int)(random() & 0x7F) + 0x40, 0x80)) e->speed = -e->speed;
        if (e->anim.id != 1) play(e, kind->anims, 1);
        e->step = 2;
        e->substep = 0;
        /* fall through */
    case 2:
        if (e->rest) e->rest--;
        e->speed = e->speed > 0 ? e->speed - (e->speed >> 4) : e->speed + (-e->speed >> 4);
        if (e->timer) {
            e->timer--;
        } else {
            if (e->rest == 0 && player_in_centred(e, soma, 0x64, 0x28)) {
                e->state = 3;
                e->substep = e->step = 0;
                return;
            }
            e->patience++;
            if (random() % 0x28 < e->patience) {
                int dx = (int16_t)((e->x >> 16) - (soma->x >> 16));
                if (abs(dx) > 0x30) {
                    e->patience = 0;
                    if (random() % 0x32 <= 0x0E) {
                        e->state = 2;
                        e->substep = e->step = 0;
                    }
                    return;
                }
                e->patience >>= 1;
            }
            e->step--;
            e->substep = 0;
        }
        ground_walk(e, soma, layer, &kind->probes, e->speed, 12);
        return;
    default:
        return;
    }
}

/* sub_080CA9E0: it stands (animation 0) for 60 frames, then closes in. */
static void beam_pause(AosEnemy *e, const AosAnimSet *anims) {
    if (e->step == 0) {
        if (e->anim.id != 0) play(e, anims, 0);
        e->timer = 0x3C;
        e->step = 1;
    }
    if (--e->timer == 0) {
        e->state = 1;
        e->substep = e->step = 0;
    }
}

/* sub_080CAC34: the muzzle flash (sub_080CB0D0: frame 0x15, translucent)
 * and the beam (sub_080CAE74: frame 0x16, + 0x5C 9, its own box (-4, -6,
 * 8, 12) for both, 3/2 of the skeleton's contact attack) 11 pixels ahead
 * of it and 17 above, sharing its affine slot. */
static void beam_fire(const AosEnemy *e, AosHitReport *report) {
    for (int i = 0; i < 2 && report->child_count < AOS_ENEMY_MAX_CHILDREN; ++i) {
        AosEnemy c = {0};
        c.id = e->id;
        c.role = i == 0 ? AOS_ROLE_BEAM_FLASH : AOS_ROLE_BEAM;
        c.static_frame = i == 0 ? 0x15 : 0x16;
        c.mirrored = e->mirrored;
        c.x = e->x + (e->mirrored ? 0xB0000 : -0xB0000);
        c.y = e->y - 0x110000;
        c.hp = 1;
        c.scaled = true;
        c.scale = e->scale;
        c.stats = e->stats;
        c.stats.contact = (uint8_t)(e->stats.contact * 3 >> 1);
        if (i == 0) {
            c.translucent = true;
            c.combat.attack_off = c.combat.hurt_off = true;
        } else {
            c.combat.type = AOS_TYPE_ENEMY;
            c.combat.hurt_off = true;
            c.own_boxes = true;
            c.own_hurt = c.own_attack = (AosBox){-4, -6, 8, 12};
        }
        report->children[report->child_count++] = c;
    }
}

/* sub_080CAA2C: animation 2 to its end; animation 3 for 33 frames (its
 * slot's vertical scale + 0x20 at 0), then + 0x1A bit 0; animation 4 at
 * full scale until sprite frame 0x13 fires the beam; animation 5 for 43
 * frames, the scale then shrinking by 0x400 a frame to 0; 9 more frames
 * with the beam gone (+ 0x1A cleared), then it closes in again. */
static void beam_attack(AosEnemy *e, const AosEnemyKind *kind, AosHitReport *report) {
    const AosAnimSet *anims = kind->anims;
    switch (e->step) {
    case 0:
        if (e->anim.id != 2) play_loop(e, anims, 2, false);
        if (e->cycle_done) {
            e->step = 1;
            e->substep = 0;
        }
        return;
    case 1:
        if (e->substep == 0) {
            if (e->anim.id != 3) play(e, anims, 3);
            e->timer = 0x20;
            e->substep = 1;
            e->scale = 0;
        }
        if (--e->timer == 0xFF) {
            e->firing = true;
            e->step = 2;
            e->substep = 0;
        }
        return;
    case 2:
        e->scale = 0xFFFF;
        if (e->anim.id != 4) play_loop(e, anims, 4, false);
        if (e->anim.id < AOS_ENEMY_MAX_ANIMS && e->anim.frame < AOS_ENEMY_MAX_FRAMES &&
            kind->sprites[e->anim.id][e->anim.frame] == 0x13) {
            beam_fire(e, report);
            e->step = 3;
            e->substep = 0;
        }
        return;
    case 3:
        if (e->substep == 0) {
            if (e->anim.id != 5) play(e, anims, 5);
            e->timer = 0x2A;
            e->substep = 1;
        }
        if (e->substep == 1) {
            e->scale = e->scale + 0x2000 > 0xFFFF ? 0xFFFF : (uint16_t)(e->scale + 0x2000);
            if (--e->timer == 0xFF) e->substep = 2;
            return;
        }
        e->scale = e->scale < 0x400 ? 0 : (uint16_t)(e->scale - 0x400);
        if (e->scale == 0) {
            e->timer = 8;
            e->step = 4;
            e->substep = 0;
        }
        return;
    case 4:
        e->firing = false;
        if (--e->timer == 0xFF) {
            e->state = 1;
            e->substep = e->step = 0;
        }
        return;
    default:
        return;
    }
}

/* sub_080CAE74 / sub_080CB0D0: the beam collides while its owner's scale
 * is above 0x400; it waits at the eye one frame, then two more once the
 * scale is full, and shoots at 12.0 (frame 0x17 after its first frame),
 * back to the eye beyond 92 pixels of the owner. Drawn with the owner's
 * vertical scale unless full, it stops hurting (+ 0x72 bit 1) once the
 * scale shrinks. The flash only follows the scale. Both are deleted with
 * + 0x1A clear or the owner hidden. The sound 0x66 loop is not ported. */
static AosHitReport beam_update(AosEnemy *b, AosSoma *soma, const AosEnemyKind *kind,
                                const AosWeaponEntity *weapon,
                                const AosWeaponFrames *weapon_frames, int soma_atk,
                                int soma_def) {
    AosHitReport report = {0};
    const AosEnemy *o = b->parent;
    if (!o) return report;
    if (b->role == AOS_ROLE_BEAM) {
        if (o->scale > 0x400)
            report = collide(b, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        switch (b->state) {
        case 0:
            b->x = o->x + (b->mirrored ? 0xB0000 : -0xB0000);
            b->static_frame = 0x16;
            b->vx = b->mirrored ? 0xC0000 : -0xC0000;
            b->state = 1;
            /* fall through */
        case 1:
            if (--b->timer == 0xFF) {
                b->timer = 2;
                b->state = 2;
            }
            break;
        case 2:
            if (o->scale > 0xFFFE && --b->timer == 0xFF) {
                b->state = 3;
                b->timer = 1;
            }
            break;
        case 3:
            if (--b->timer == 0xFF && b->static_frame == 0x16) b->static_frame = 0x17;
            b->x += b->vx;
            b->vx += b->ax;
            if (abs((int16_t)((b->x >> 16) - (o->x >> 16))) > 0x5C) {
                b->x = o->x + (b->mirrored ? 0xB0000 : -0xB0000);
                b->static_frame = 0x16;
                b->vx = b->mirrored ? 0xC0000 : -0xC0000;
                b->timer = 1;
            }
            break;
        default:
            break;
        }
        if (o->scale != 0xFFFF) b->combat.attack_off = true;
        aos_combat_tick(&b->combat);
    }
    b->scale = o->scale;
    b->scaled = o->scale != 0xFFFF;
    if (!o->firing || o->hidden || o->removed) b->removed = true;
    return report;
}

/* EnemyBeamSkeletonUpdate: state 4 plays animation 0; 0xFF (killed) is
 * deleted; 3 fires without the activity window; otherwise, inside it (and
 * after a RandomNumberGenerator draw for + 0x1E, which nothing reads), 0
 * patrols, 1 closes in, 2 pauses. Then the animation step and the
 * collision pass (sub_0806E314). */
static AosHitReport beam_skeleton_update(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                                         const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                         const AosWeaponFrames *weapon_frames, int soma_atk,
                                         int soma_def, int cam_x, int cam_y,
                                         uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    if (e->state == 4) {
        if (e->step == 0) {
            play(e, kind->anims, 0);
            e->step = 1;
        }
        step_anim(e, kind->anims);
        return report;
    }
    if (e->state == 0xFF) {
        e->hidden = true;
        e->removed = true;
        return report;
    }
    if (e->state == 3) {
        beam_attack(e, kind, &spawned);
    } else {
        int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
        if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
        (void)random();
        switch (e->state) {
        case 0: beam_patrol(e, soma, layer, kind); break;
        case 1: beam_close_in(e, soma, layer, kind, random); break;
        case 2: beam_pause(e, kind->anims); break;
        default: break;
        }
    }
    step_anim(e, kind->anims);
    int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
    if (!e->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
        report = collide(e, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    aos_combat_tick(&e->combat);
    for (int i = 0; i < spawned.child_count && report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
        report.children[report.child_count++] = spawned.children[i];
    return report;
}

/* The records at 0x0852883C (20 bytes) of Skull Millione, Cagnazzo and
 * Lubicant: flags (+ 5: bit 0 a fin, bit 1 the hover attack), the lead on
 * the player's speed (+ 6), the idle wait (+ 7), the chase's top speed
 * (+ 0xC) and the dive's speed (+ 0x10). */
typedef struct {
    uint8_t flags;
    int8_t lead;
    uint8_t wait;
    int32_t top_speed, dive;
} SkullRecord;

static const SkullRecord skull_records[3] = {
    {0, 4, 0x20, 0x100000, 0x50000},
    {3, 2, 0x20, 0x140000, 0x40000},
    {1, 8, 0, 0x180000, 0x40000},
};

/* sub_080B7A34: it bobs (+ 0x18 from 0x8000, vy = sin(+ 0x18 * 0x5B0) /
 * 2) for the record's wait, then, with the player in the 0xDC x 0xA0 box
 * centred 40 pixels below it, faces him and chases (state 1); a fin
 * without the hover attack plays animation 3. */
static void skull_idle(AosEnemy *e, const AosSoma *soma, const AosAnimSet *anims) {
    const SkullRecord *r = &skull_records[e->variant];
    if (e->step == 0) {
        e->step = 1;
        e->phase = 0x8000;
        e->timer = r->wait;
    }
    e->phase++;
    int32_t s = aos_sine((uint32_t)(int16_t)e->phase * 0x5B0u & 0xFFFF);
    e->vy = s / 2;
    e->y += e->vy;
    if (e->timer) {
        e->timer--;
        return;
    }
    e->y += 0x280000;
    if (player_in_centred(e, soma, 0xDC, 0xA0)) {
        face_player(e, soma);
        e->state = 1;
        e->substep = e->step = 0;
        if ((r->flags & 1) && !(r->flags & 2)) play(e, anims, 3);
    }
    e->y -= 0x280000;
}

/* sub_080B7AEC: from rest its speed (+ 0x20) gains 0x2000 a frame up to
 * the record's top; it heads (sub_080694B8) for 80 pixels above the
 * player, the speed short of him, led by his vx times the record's lead,
 * facing him at each frame start, and dives (state 2) on arrival. */
static void skull_chase(AosEnemy *e, const AosSoma *soma) {
    const SkullRecord *r = &skull_records[e->variant];
    if (e->step == 0) {
        e->speed = 0;
        e->step = 1;
    }
    if (e->anim.tick == 0) face_player(e, soma);
    if (e->speed < r->top_speed) e->speed += 0x2000;
    int32_t tx = (int32_t)((uint32_t)(soma->x >> 16) << 16);
    int32_t ty = (int32_t)((uint32_t)((soma->y >> 16) - 0x50) << 16);
    tx += soma->x > e->x ? -e->speed : e->speed;
    tx += soma->vx * r->lead;
    bool arrived = move_toward(e, tx, ty, e->speed);
    e->x += e->vx;
    e->y += e->vy;
    if (arrived) {
        e->state = 2;
        e->substep = e->step = 0;
    }
}

/* sub_080B7BC8: the dive. A hover-attack variant first circles the player
 * for 64 frames (animation 3: + 0x50 +/- 0x3000 toward him up to 2.5, +
 * 0x54 0x1800 down while above y - 104, else -0x3000 with vy at most
 * -0x1000, up to 0.5). Then animation 1 once rising (vy -2.0, + 0x54
 * 0x1000 up to 0, vx braked by -vx / 16 for 32 frames); at animation frame
 * 4 it plunges (the record's dive speed, + 0x54 -0x2000 down to -3.0;
 * sound 0x87, and 0x1BA for Cagnazzo) until the animation ends, then rises
 * until vy turns negative and bobs again (state 0, animation 0). */
static void skull_dive(AosEnemy *e, const AosSoma *soma, const AosAnimSet *anims) {
    const SkullRecord *r = &skull_records[e->variant];
    switch (e->step) {
    case 0:
        if (r->flags & 2) {
            play(e, anims, 3);
            e->timer = 0x40;
            e->step = 5;
            return;
        }
        /* fall through */
    case 1:
        play_loop(e, anims, 1, false);
        e->vy = -0x20000;
        e->ay = 0x1000;
        e->ax = -e->vx / 16;
        e->step = 2;
        e->timer = 0x20;
        /* fall through */
    case 2:
        e->x += e->vx;
        e->y += e->vy;
        if (e->timer) {
            e->timer--;
            e->vx += e->ax;
        }
        e->vy += e->ay;
        if (e->vy > 0) e->vy = 0;
        if (e->anim.frame == 4) {
            e->step = 3;
            e->vy = r->dive;
            e->ay = -0x2000;
        }
        return;
    case 3:
        e->vy += e->ay;
        if (e->vy < -0x30000) e->vy = -0x30000;
        e->y += e->vy;
        if (e->anim.flags & AOS_ANIM_ENDED) {
            e->ay = -0x2000;
            e->step = 4;
        }
        return;
    case 4:
        e->y += e->vy;
        e->vy += e->ay;
        if (e->vy >= 0) return;
        e->vy = 0;
        e->state = e->substep = e->step = 0;
        play(e, anims, 0);
        return;
    case 5:
        if (e->anim.frame == 0 && e->anim.tick == 0) face_player(e, soma);
        e->ax = soma->x > e->x ? 0x3000 : -0x3000;
        if (soma->y - 0x680000 > e->y) {
            e->ay = 0x1800;
        } else {
            if (e->vy > -0x1000) e->vy = -0x1000;
            e->ay = -0x3000;
        }
        /* sub_0806D430(e, 2.5), sub_0806D460(e, 0.5) */
        e->vx += e->ax;
        if (e->ax >= 0 ? e->vx > 0x28000 : e->vx < -0x28000)
            e->vx = e->ax >= 0 ? 0x28000 : -0x28000;
        e->x += e->vx;
        e->vy += e->ay;
        if (e->ay >= 0 ? e->vy > 0x8000 : e->vy < -0x8000) e->vy = e->ay >= 0 ? 0x8000 : -0x8000;
        e->y += e->vy;
        if (e->timer) e->timer--;
        else e->step = 1;
        return;
    default:
        return;
    }
}

/* sub_080B7D90: animation 2 once (sound 0x1BC) and 32 frames with a blood
 * splash of sub_08045CEC (animation 13, row 8) at x + (r & 7) - 4, y +
 * (r & 15) - 24 every 16, flipped at random; then it sinks (vy -0.5, +
 * 0x54 0x1000 up to 8.0) through the generic death of 50 frames. Beyond
 * the window (-128, -64, 0x1F0, 0x120) of the screen it is deleted. */
static void skull_die(AosEnemy *e, const AosEnemyKind *kind, int cam_x, int cam_y,
                      uint32_t (*random)(void), AosHitReport *report) {
    switch (e->step) {
    case 0:
        play_loop(e, kind->anims, 2, false);
        e->step = 1;
        e->timer = 0x20;
        /* fall through */
    case 1:
        if ((e->timer & 0xF) == 0) {
            int32_t x = e->x + (random_centred(random, 8) << 16);
            int32_t y = e->y + ((random_centred(random, 0x10) - 0x10) << 16);
            AosEffect *blood = spawn_drift(AOS_EFFECT_BLOOD, x, y, report);
            if (blood) blood->mirrored = random() & 1;
        }
        if (e->timer) {
            e->timer--;
            break;
        }
        e->vy = -0x8000;
        e->ay = 0x1000;
        e->step = 2;
        e->timer = 0x32;
        break;
    case 2:
        e->vy += e->ay;
        if (e->vy > 0x80000) e->vy = 0x80000;
        e->y += e->vy;
        generic_death(e, kind, random, report);
        break;
    default:
        break;
    }
    int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) e->removed = true;
}

/* sub_080B7988: the fin of Cagnazzo and Lubicant, an affine sprite (frame
 * 0xD) of their sheet. */
static AosEnemy skull_fin(const AosEnemy *owner) {
    AosEnemy f = {0};
    f.id = owner->id;
    f.role = AOS_ROLE_FIN;
    f.static_frame = 0xD;
    f.x = owner->x;
    f.y = owner->y;
    f.hidden = true;
    f.hp = 1;
    return f;
}

/* sub_080B7780: hidden until its owner plays animation 3, then held 12
 * pixels ahead of it and 6 above, turning by -0x1200 a frame (mirrored
 * +0x1200). Once the owner dies (state 3) it is flung off (vx (r & 63) -
 * 32 / 16, vy -3.5 + ((r & 15) - 8) / 16, + 0x54 0x2000 up to 8.0),
 * turning by 0xB00 (mirrored -0xB00), and deleted beyond margin entry 4. */
static void fin_update(AosEnemy *f, const AosEnemyKind *kind, int cam_x, int cam_y,
                       uint32_t (*random)(void)) {
    const AosEnemy *o = f->parent;
    if (f->state <= 1 && o && o->state == 3) f->state = 2;
    if (f->state <= 1 && o) {
        if (f->state == 0) {
            if (o->anim.id != 3) return;
            f->angle = 0;
            f->hidden = false;
            f->state = 1;
        }
        if (o->anim.id != 3) {
            f->hidden = true;
            f->state = 0;
            return;
        }
        f->mirrored = o->mirrored;
        f->x = (int32_t)((uint32_t)((o->x >> 16) + (f->mirrored ? 12 : -12)) << 16);
        f->y = (int32_t)((uint32_t)((o->y >> 16) - 6) << 16);
        f->angle = (uint16_t)(f->angle + (f->mirrored ? 0x1200 : -0x1200));
        return;
    }
    if (f->state == 2) {
        f->hidden = false;
        if (o) {
            f->mirrored = o->mirrored;
            f->x = (int32_t)((uint32_t)((o->x >> 16) + (f->mirrored ? 12 : -12)) << 16);
            f->y = (int32_t)((uint32_t)((o->y >> 16) - 6) << 16);
        }
        f->vy = (random_centred(random, 0x10) << 12) - 0x38000;
        f->ay = 0x2000;
        f->vx = random_centred(random, 0x40) << 12;
        f->ax = 0;
        f->angle = 0;
        f->state = 3;
    }
    f->angle = (uint16_t)(f->angle + (f->mirrored ? -0xB00 : 0xB00));
    /* sub_0806D3D8(e, 8.0) */
    f->vx += f->ax;
    f->vy += f->ay;
    if (f->vy > 0x80000) f->vy = 0x80000;
    f->x += f->vx;
    f->y += f->vy;
    if (outside_margins(kind, 4, (f->x >> 16) - cam_x, (f->y >> 16) - cam_y)) f->removed = true;
}

/* sub_080B7F40: hidden beyond margin entry 4 (no activity window); while
 * alive and flashing from a blow (sub_0806AD24) it only collides. Then
 * state 0 bobs, 1 chases, 2 dives, 3 dies, 4 only steps its animation;
 * outside the death the collision pass (sub_080421AC: no on-screen test)
 * and two animation steps (sub_0806DF20, then sub_0803F17C). The tracking
 * list of sub_08021924 and the routine sub_08021654 of its draw callback
 * sub_080B76E4 are not ported. */
static AosHitReport skull_update(AosEnemy *e, AosSoma *soma, const AosEnemyKind *kind,
                                 const AosWeaponEntity *weapon,
                                 const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
                                 int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    const AosAnimSet *anims = kind->anims;
    e->hidden = outside_margins(kind, 4, (e->x >> 16) - cam_x, (e->y >> 16) - cam_y);
    if (!e->dropped) {
        e->dropped = true;
        if ((skull_records[e->variant].flags & 1) && e->state != 4)
            spawned.children[spawned.child_count++] = skull_fin(e);
    }
    if (e->hp > 0 && e->hit_flash) {
        e->hit_flash--;
        if (e->state != 3) report = collide(e, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        aos_combat_tick(&e->combat);
    } else {
        switch (e->state) {
        case 0: skull_idle(e, soma, anims); break;
        case 1: skull_chase(e, soma); break;
        case 2: skull_dive(e, soma, anims); break;
        case 3: skull_die(e, kind, cam_x, cam_y, random, &spawned); break;
        default: break;
        }
        if (!e->removed) {
            if (e->state != 3 && e->state != 4) {
                report = collide(e, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
                step_anim(e, anims);
            }
            step_anim(e, anims);
        }
        aos_combat_tick(&e->combat);
    }
    report.child_count = spawned.child_count;
    memcpy(report.children, spawned.children, sizeof spawned.children[0] * spawned.child_count);
    take_effects(&report, &spawned);
    return report;
}

/* The dead crusader's guard: Soma's + 0x10 flags & 0x360 (sub_080212C8:
 * attacking) while it stands on the floor raise its shield (animation 7
 * once, step 5, + 0x21 set, no speed). */
static bool crusader_guard(AosEnemy *k, const AosSoma *soma, const AosAnimSet *anims) {
    if (!(soma->flags & 0x360u)) return false;
    play_loop(k, anims, 7, false);
    k->step = 5;
    k->timer = 0;
    k->guarding = true;
    k->vx = k->ax = 0;
    k->vy = 0x10000;
    k->ay = 0x2800;
    return true;
}

/* The dead crusader walking on (animation 1 kept) toward its facing. */
static void crusader_turn(AosEnemy *k, const AosSoma *soma, uint8_t step) {
    soldier_face(k, soma);
    k->step = step;
    k->timer = 0;
    k->vy = 0x10000;
    k->ay = 0x2800;
}

/* sub_0808F364: the skeleton knight's walk at 0.75 with a guard. Step 0
 * waits for the player in (x - 90, y - 48, 180, 96) or a wound, then walks
 * (animation 1); step 1 advances, facing him again every 60 frames; on the
 * floor it guards while he attacks, attacks him in (x - 40, y - 40, 80, 80)
 * (r & 0x7F <= 0x40) or else backs away (step 2), backs away at the room's
 * edge (sub_0806B120(e, 1)) and waits again beyond the screen margins.
 * Step 2 backs away and advances again once he leaves (x - 60, y - 40,
 * 120, 80), after 60 frames, at a wall or the room's edge. Step 5 guards,
 * facing him, for 60 frames on the floor, then walks on. Moves use
 * sub_08069A00(e, 1.0, -32, 8.0, 8). */
static void crusader_walk(AosEnemy *k, const AosSoma *soma, const AosCollision *layer,
                          const AosEnemyKind *kind, int cam_x, int cam_y,
                          uint32_t (*random)(void)) {
    int32_t x = k->x >> 16, y = k->y >> 16;
    switch (k->step) {
    case 0:
        if (!player_near(soma, (int16_t)(x - 0x5A), (int16_t)(y - 0x30), 0xB4, 0x60) &&
            k->hp >= k->stats.hp)
            return;
        k->step = 1;
        play(k, kind->anims, 1);
        k->vy = 0x10000;
        k->ay = 0x2800;
        return;
    case 1:
    case 2: {
        if (k->timer++ > 0x3B) {
            k->timer = 0;
            soldier_face(k, soma);
        }
        bool forward = k->step == 1;
        k->vx = (k->mirrored == forward) ? 0xC000 : -0xC000;
        int result = walk_collide(k, layer, 0x10000, (int32_t)0xFFE00000, 0x80000, 8);
        if (!(result & 4) || crusader_guard(k, soma, kind->anims)) return;
        x = k->x >> 16;
        y = k->y >> 16;
        bool close = player_near(soma, (int16_t)(x - 0x28), (int16_t)(y - 0x28), 0x50, 0x50);
        if (forward) {
            if (close) {
                if ((random() & 0x7F) <= 0x40) {
                    k->state = 1;
                    k->step = 0;
                    k->timer = 0;
                    return;
                }
                k->step = 2;
                k->timer = 0;
                k->vy = 0x10000;
                k->ay = 0x2800;
            } else if (clamp_to_room(k, layer, 1)) {
                k->step = 2;
                k->timer = 0;
                k->vy = 0x10000;
                k->ay = 0x2800;
            } else if (outside_margins(kind, 4, x - cam_x, y - cam_y)) {
                play(k, kind->anims, 0);
                k->step = 0;
            }
            return;
        }
        if (!player_near(soma, (int16_t)(x - 0x3C), (int16_t)(y - 0x28), 0x78, 0x50) ||
            k->timer > 0x3B || clamp_to_room(k, layer, 1) || (result & 3))
            crusader_turn(k, soma, 1);
        return;
    }
    case 5: {
        soldier_face(k, soma);
        k->guarding = true;
        int result = walk_collide(k, layer, 0x10000, (int32_t)0xFFE00000, 0x80000, 8);
        if (!(result & 4) || k->timer++ <= 0x3B) return;
        play(k, kind->anims, 1);
        k->step = 1;
        k->timer = 0;
        k->vy = 0x10000;
        k->ay = 0x2800;
        return;
    }
    default:
        return;
    }
}

/* sub_0808F6E8: it faces the player and raises its sword (animation 2) for
 * 38 frames, then (animation 3) one chance in two (r & 0x7F <= 0x3F) to
 * thrust (step 2: sound 0x85 at the 12th frame, 31 frames, animation 4 for
 * 21) or else to slash (step 5: sound 0x85 at the 12th frame, 13 frames,
 * then animation 6 three times 13 frames); then it walks on (animation 1,
 * state 0, step 1). */
static void crusader_attack(AosEnemy *k, const AosSoma *soma, const AosAnimSet *anims,
                            uint32_t (*random)(void)) {
    switch (k->step) {
    case 0:
        soldier_face(k, soma);
        play_loop(k, anims, 2, false);
        k->step = 1;
        k->timer = 0;
        return;
    case 1:
        if (k->timer++ <= 0x25) return;
        play_loop(k, anims, 3, false);
        k->timer = 0;
        k->step = (random() & 0x7F) <= 0x3F ? 2 : 5;
        return;
    case 2:
        if (k->timer++ <= 0x1D) return;
        play_loop(k, anims, 4, false);
        k->step = 3;
        k->timer = 0;
        return;
    case 3:
        if (k->timer++ <= 0x13) return;
        break;
    case 5:
        if (k->timer++ <= 0x0B) return;
        play_loop(k, anims, 6, false);
        k->step = 6;
        k->substep = 0;
        k->timer = 0;
        return;
    case 6:
        if (k->timer++ <= 0x0B) return;
        if (k->substep++ <= 2) {
            play_loop(k, anims, 6, false);
            k->timer = 0;
            return;
        }
        break;
    default:
        return;
    }
    play(k, anims, 1);
    k->state = 0;
    k->step = 1;
    k->timer = 0;
    k->vy = 0x10000;
    k->ay = 0x2800;
}

/* EnemyDeadCrusaderUpdate: + 0x21 cleared each frame; alive and beyond
 * the margins of sub_0806D128(e, 4) it does nothing. Then state 0 walks,
 * 1 attacks, 2 runs the generic death (hidden beyond the margins), 3 only
 * steps its animation; then the animation step and the collision pass.
 * The hit stun of sub_0806D4C0 needs the global pause 0x4BE (taken as 0)
 * and the tracking list of sub_08021924 is not ported. */
static AosHitReport crusader_update(AosEnemy *k, AosSoma *soma, const AosCollision *layer,
                                    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                    const AosWeaponFrames *weapon_frames, int soma_atk,
                                    int soma_def, int cam_x, int cam_y,
                                    uint32_t (*random)(void)) {
    AosHitReport report = {0}, effects = {0};
    k->guarding = false;
    bool outside = outside_margins(kind, 4, (k->x >> 16) - cam_x, (k->y >> 16) - cam_y);
    if (k->state != 2 && outside) return report;
    k->hidden = outside;
    switch (k->state) {
    case 0: crusader_walk(k, soma, layer, kind, cam_x, cam_y, random); break;
    case 1: crusader_attack(k, soma, kind->anims, random); break;
    case 2:
        generic_death(k, kind, random, &effects);
        if (k->removed) return effects;
        break;
    case 3:
        step_anim(k, kind->anims);
        return report;
    default:
        break;
    }
    step_anim(k, kind->anims);
    int sx = (k->x >> 16) - cam_x, sy = (k->y >> 16) - cam_y;
    if (!k->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
        report = collide(k, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    aos_combat_tick(&k->combat);
    take_effects(&report, &effects);
    return report;
}

/* Curly's walk speed (0.5) toward its facing, slowed uphill. */
static void curly_pace(AosEnemy *c, const AosCollision *layer) {
    c->vx = slope_speed(c, layer, c->mirrored ? 0x8000 : -0x8000);
}

/* sub_08093218: step 0 (unhurt) paces, turning at walls, drops and the
 * room's edges; step 1 (hurt, sub_0806D54C) faces the player at every
 * frame start. Outside walk frames 4 to 7 each frame start sets the pace
 * and on frame 1, with the player in (x - 50, y - 64, 100, 70), three
 * chances in four (r & 0x7F <= 0x5F) start the attack (animation 1); frames
 * 4 to 7 stand still. It moves with sub_08069A00(e, 1.0, -32, 8.0, 8). */
static void curly_walk(AosEnemy *c, const AosSoma *soma, const AosCollision *layer,
                       const AosAnimSet *anims, uint32_t (*random)(void)) {
    if (c->step == 0 && c->hp < c->stats.hp) {
        c->step = 1;
        return;
    }
    uint8_t frame = c->anim.frame;
    bool start = c->anim.tick == 0;
    if ((uint8_t)(frame - 4) > 3) {
        if (start) {
            if (c->step == 1) soldier_face(c, soma);
            curly_pace(c, layer);
            int32_t x = c->x >> 16, y = c->y >> 16;
            if (frame == 1 &&
                player_near(soma, (int16_t)(x - 0x32), (int16_t)(y - 0x40), 0x64, 0x46) &&
                (random() & 0x7F) <= 0x5F) {
                c->state = 1;
                c->step = 0;
                c->timer = 0;
                play_loop(c, anims, 1, false);
                return;
            }
        }
    } else if (start) {
        c->vx = 0;
    }
    /* A step: sound 0x85 at the start of walk frame 4. */
    if (frame == 4 && start) aos_audio_play_song(0x85);
    int result = walk_collide(c, layer, 0x10000, (int32_t)0xFFE00000, 0x80000, 8);
    if (c->step != 0 || !(result & 4)) return;
    int32_t ahead = (c->x >> 16) + (c->mirrored ? 1 : -1);
    if ((result & 3) || blocked_ahead(layer, ahead, c->y >> 16) || at_room_edge(c, layer)) {
        c->mirrored = !c->mirrored;
        curly_pace(c, layer);
    }
}

/* sub_08093460: a lunge at 1.0 for 8 frames, then still; back to the walk
 * when animation 1 ends (sound 0x85 at frames 4, 7, 10 and 13). */
static void curly_attack(AosEnemy *c, const AosCollision *layer, const AosAnimSet *anims) {
    if (c->step != 0) return;
    if (c->timer <= 7) {
        c->timer++;
        c->vx = slope_speed(c, layer, c->mirrored ? 0x10000 : -0x10000);
        c->vy = 0x10000;
    } else {
        c->vx = 0;
    }
    c->ay = 0x2800;
    walk_collide(c, layer, 0x10000, (int32_t)0xFFE00000, 0x80000, 8);
    if (c->cycle_done) {
        play(c, anims, 0);
        c->state = c->step = 0;
        curly_pace(c, layer);
        c->vy = 0x10000;
        c->ay = 0x2800;
    }
    /* Sound 0x85 at the start of attack frames 4, 7, 10 and 13. */
    if (c->state == 1 && c->anim.tick == 0 &&
        (c->anim.frame == 4 || c->anim.frame == 7 || c->anim.frame == 10 || c->anim.frame == 13))
        aos_audio_play_song(0x85);
}

/* EnemyCurlyUpdate: inside the activity window, state 0 walks, 1 attacks,
 * 2 runs the generic death, 3 only steps its animation; then the
 * animation step and the collision pass. Contact sub_08093674: type 0,
 * element 0. */
static AosHitReport curly_update(AosEnemy *c, AosSoma *soma, const AosCollision *layer,
                                 const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                 const AosWeaponFrames *weapon_frames, int soma_atk,
                                 int soma_def, int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, effects = {0};
    int sx = (c->x >> 16) - cam_x, sy = (c->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
    switch (c->state) {
    case 0: curly_walk(c, soma, layer, kind->anims, random); break;
    case 1: curly_attack(c, layer, kind->anims); break;
    case 2:
        generic_death(c, kind, random, &effects);
        if (c->removed) return effects;
        break;
    case 3:
        step_anim(c, kind->anims);
        return report;
    default:
        break;
    }
    step_anim(c, kind->anims);
    sx = (c->x >> 16) - cam_x;
    sy = (c->y >> 16) - cam_y;
    if (!c->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
        report = collide(c, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    aos_combat_tick(&c->combat);
    take_effects(&report, &effects);
    return report;
}

/* sub_080C3190: step 0 plays animation 0, stops and faces the player;
 * step 1 dances a figure of eight: vx = 2 sin(+ 0x14 + 0x4000), x += vx,
 * y += sin(4 * + 0x14) + vy, + 0x14 += 0x80; when vx is 0 the animation
 * switches between 0 and 1; every half turn (+ 0x14 % 0x8000 == 0) with
 * + 0x18 even, + 0x18 advances (the fade starts). */
static void ghost_dance(AosEnemy *g, const AosSoma *soma, const AosAnimSet *anims) {
    if (g->step == 0) {
        play(g, anims, 0);
        g->vx = g->vy = 0;
        g->mirrored = (g->x >> 16) < (soma->x >> 16);
        g->phase = 0;
        g->timer = 0;
        g->step = 1;
        return;
    }
    g->vx = aos_sine((uint32_t)g->phase + 0x4000) * 2;
    g->x += g->vx;
    g->y += aos_sine((uint32_t)g->phase * 4) + g->vy;
    g->phase = (uint16_t)(g->phase + 0x80);
    if (g->vx == 0) play(g, anims, (unsigned)(1 - g->anim.id));
    if (g->phase % 0x8000 == 0 && !(g->fade & 1)) {
        g->fade++;
        g->fade_timer = 0;
    }
}

/* EnemyGhostDancerUpdate's fade (+ 0x18): 0 opaque, 1 alternating
 * semi-transparent / opaque frames for 33 frames, 2 semi-transparent, 3
 * alternating back, then 0. */
static void ghost_fade(AosEnemy *g) {
    switch (g->fade) {
    case 0: g->translucent = false; break;
    case 1:
    case 3:
        g->translucent = (g->fade == 1) == !(g->fade_timer & 1);
        if ((int8_t)g->fade_timer++ > 0x20) {
            g->fade = g->fade == 1 ? 2 : 0;
            g->fade_timer = 0;
        }
        break;
    case 2: g->translucent = true; break;
    default: break;
    }
}

/* EnemyGhostDancerUpdate: state 5 steps its animation; otherwise it is
 * hidden beyond the margins of sub_0806D128(e, 4), fades (not while dying),
 * is stunned by sub_0806AD24, dances (states 1 and 2) or dies (state 3:
 * deleted when its animation ends), then the animation step and, except
 * while dying, the collision pass (sub_080421AC: no on-screen test).
 * Contact sub_080C339C: type 0, element 0. */
static AosHitReport ghost_dancer_update(AosEnemy *g, AosSoma *soma, const AosEnemyKind *kind,
                                        const AosWeaponEntity *weapon,
                                        const AosWeaponFrames *weapon_frames, int soma_atk,
                                        int soma_def, int cam_x, int cam_y) {
    AosHitReport report = {0};
    if (g->state == 5) {
        step_anim(g, kind->anims);
        return report;
    }
    g->hidden = outside_margins(kind, 4, (g->x >> 16) - cam_x, (g->y >> 16) - cam_y);
    if (g->state != 3) ghost_fade(g);
    if (g->hp > 0 && g->hit_flash) {
        g->hit_flash--;
        if (g->state != 3) report = collide(g, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        aos_combat_tick(&g->combat);
        return report;
    }
    if (g->state == 1 || g->state == 2) {
        ghost_dance(g, soma, kind->anims);
    } else if (g->state == 3 && g->cycle_done) {
        g->removed = true;
        return report;
    }
    step_anim(g, kind->anims);
    if (g->state != 3) report = collide(g, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    aos_combat_tick(&g->combat);
    return report;
}

/* The gargoyle's pursuit of one axis: beyond 8 pixels the speed moves
 * toward the target by + 0x50, up to + 0x54. */
static void gargoyle_chase(int32_t *v, int32_t delta, int32_t a, int32_t max) {
    if (delta < -0x80000) {
        *v -= a;
        if (*v <= -max) *v = -max;
    } else if (delta > 0x80000) {
        *v += a;
        if (*v >= max) *v = max;
    }
}

/* sub_08086534: step 0 faces the player at 0.5; step 1 chases the point
 * 64 pixels above him (+ 0x50 = 0x1000 up to 1.0 per axis) until, once its
 * timer runs out, he is within 8 pixels vertically and 96 horizontally:
 * animation 3; step 2 brakes (9/10 per frame) for 21 frames, then plays
 * animation 1 and dives (sound 0x87) with accelerations (+/- 0xC00,
 * 0x1800) clamped to 2.0 for 25 frames; step 4 doubles the X acceleration,
 * keeps 15/16 of vy for 61 frames (animation 2 one frame in 256 in steps 3
 * and 4); step 5 climbs (0x1000 up to 1.5) until level with or below that
 * point, then chases again after 20 frames. */
static void gargoyle_fly(AosEnemy *g, const AosSoma *soma, const AosAnimSet *anims,
                         uint32_t (*random)(void)) {
    int32_t dx, dy;
    switch (g->step) {
    case 0:
        soldier_face(g, soma);
        g->vx = g->mirrored ? 0x8000 : -0x8000;
        g->vy = 0;
        g->ax = 0x1000;
        g->ay = 0x10000;
        g->timer = 0;
        g->step = 1;
        /* fall through */
    case 1:
        dx = soma->x - g->x;
        dy = soma->y - (g->y + 0x400000);
        gargoyle_chase(&g->vx, dx, g->ax, g->ay);
        gargoyle_chase(&g->vy, dy, g->ax, g->ay);
        g->mirrored = g->vx >= 0;
        g->x += g->vx;
        g->y += g->vy;
        if ((int8_t)--g->timer > 0) return;
        g->timer = 0;
        if ((uint32_t)(dy + 0x7FFFF) > 0xFFFFE) return;
        if ((dx < 0 ? -dx : dx) > 0x5FFFFF) return;
        g->step = 2;
        play_loop(g, anims, 3, false);
        return;
    case 2:
        soldier_face(g, soma);
        g->vx = g->vx * 9 / 10;
        g->vy = g->vy * 9 / 10;
        g->x += g->vx;
        g->y += g->vy;
        if (g->timer++ <= 0x13) return;
        g->step = 3;
        g->timer = 0;
        play_loop(g, anims, 1, false);
        g->ax = g->mirrored ? 0xC00 : -0xC00;
        g->ay = 0x1800;
        return;
    case 3:
        if (!(random() & 0xFF)) play_loop(g, anims, 2, false);
        accel_move(g, 0x20000);
        if (g->timer++ <= 0x17) return;
        g->step = 4;
        g->timer = 0;
        g->ax *= 2;
        return;
    case 4:
        if (!(random() & 0xFF)) play_loop(g, anims, 2, false);
        /* sub_0806D430(e, 2.0), sub_0806D4A8(e, 16), sub_0806D3CC */
        g->vx += g->ax;
        if (g->ax >= 0 ? g->vx > 0x20000 : g->vx < -0x20000) g->vx = g->ax >= 0 ? 0x20000 : -0x20000;
        g->x += g->vx;
        g->vy = g->vy * 15 / 16;
        g->y += g->vy;
        if (g->timer++ <= 0x3B) return;
        g->step = 5;
        g->timer = 0;
        g->ax = 0x1000;
        g->ay = 0x18000;
        play(g, anims, 0);
        return;
    case 5:
        dx = soma->x - g->x;
        dy = soma->y - (g->y + 0x400000);
        gargoyle_chase(&g->vx, dx, g->ax, g->ay);
        if (dy < 0) {
            g->vy -= g->ax;
            if (g->vy <= -g->ay) g->vy = -g->ay;
        } else {
            g->step = 1;
            g->timer = 0x14;
            g->ax = 0x1000;
            g->ay = 0x10000;
        }
        g->mirrored = g->vx >= 0;
        g->x += g->vx;
        g->y += g->vy;
        return;
    default:
        return;
    }
}

/* EnemyGargoyleUpdate: inside the activity window, state 0 flies, state 1
 * dies (9/10 of vx, a fall with 0x1000 clamped to 3.0, the generic death),
 * state 2 only steps its animation; then the animation step and the
 * collision pass. Contact sub_08086964: type 0, element 0. */
static AosHitReport gargoyle_update(AosEnemy *g, AosSoma *soma, const AosEnemyKind *kind,
                                    const AosWeaponEntity *weapon,
                                    const AosWeaponFrames *weapon_frames, int soma_atk,
                                    int soma_def, int cam_x, int cam_y,
                                    uint32_t (*random)(void)) {
    AosHitReport report = {0}, effects = {0};
    int sx = (g->x >> 16) - cam_x, sy = (g->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
    switch (g->state) {
    case 0: gargoyle_fly(g, soma, kind->anims, random); break;
    case 1:
        g->vx = g->vx * 9 / 10;
        accel_move(g, 0x30000);
        generic_death(g, kind, random, &effects);
        if (g->removed) return effects;
        break;
    case 2:
        step_anim(g, kind->anims);
        return report;
    default:
        break;
    }
    step_anim(g, kind->anims);
    sx = (g->x >> 16) - cam_x;
    sy = (g->y >> 16) - cam_y;
    if (!g->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
        report = collide(g, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    aos_combat_tick(&g->combat);
    take_effects(&report, &effects);
    return report;
}

/* The officer's walk speed toward its facing (0.625). */
static void officer_step(AosEnemy *z) {
    z->vx = z->mirrored ? 0xA000 : -0xA000;
    z->vy = 0x10000;
    z->ay = 0x2800;
}

/* sub_080923E4: at the start of walk frames 3 and 7 the officer faces the
 * player and attacks him within the 100 x 70 box centred on it
 * (sub_0806CFFC(x - 50, y - 64, 100, 70)), or steps at 0.625; frames 2 and
 * 6 stop it; it moves with the zombie's walker collision, half width 10. */
static void officer_walk(AosEnemy *z, const AosSoma *soma, const AosCollision *layer) {
    if (z->step != 0) return;
    uint8_t frame = z->anim.frame;
    bool frame_start = z->anim.tick == 0;
    if ((frame == 3 || frame == 7) && frame_start) {
        soldier_face(z, soma);
        int32_t x = z->x >> 16, y = z->y >> 16;
        if (player_near(soma, (int16_t)(x - 0x32), (int16_t)(y - 0x40), 0x64, 0x46)) {
            z->state = 1;
            z->step = 0;
            return;
        }
        officer_step(z);
    }
    if ((frame == 2 || frame == 6) && frame_start) z->vx = 0;
    walk_collide(z, layer, 0x10000, (int32_t)0xFFE00000, 0x80000, 10);
}

/* sub_080924AC: animation 1 once (sound 0x85 at frame 4), then back to the
 * walk. */
static void officer_attack(AosEnemy *z, const AosAnimSet *anims) {
    if (z->step == 0) {
        play_loop(z, anims, 1, false);
        z->step = 1;
        return;
    }
    if (z->anim.frame == 4 && z->anim.tick == 0) aos_audio_play_song(0x85);
    if (z->cycle_done) {
        play(z, anims, 0);
        z->state = z->step = 0;
        officer_step(z);
    }
}

/* EnemyZombieOfficerUpdate state 2 at timer 0x28: gibs 0x10 and 0x11 as
 * the soldier's, then 0x0F at (-6 / +6 mirrored, -13) with vy -1.0 -
 * (r & 0xFFFF) (sub_080925B4, graphics 0x081CC01C / 0x08220F84). */
static void officer_gibs(const AosEnemy *z, uint32_t (*random)(void), AosHitReport *report) {
    for (int i = 0; i < 3; ++i) {
        int32_t dx, dy;
        if (i < 2) {
            dx = (int32_t)(random() & 0xFFFFF) - 0x80000;
            dy = i == 0 ? -0x10000 - (int32_t)(random() & 0xFFFFF)
                        : -(int32_t)(random() % 0x180000u);
        } else {
            dx = z->mirrored ? 0x60000 : -0x60000;
            dy = (int32_t)0xFFF30000;
        }
        int32_t vx = gib_speed(z, random);
        int32_t vy = i < 2 ? -(int32_t)(random() % 0x18000u) - 0x18000
                           : -0x10000 - (int32_t)(random() & 0xFFFF);
        if (report->child_count < AOS_ENEMY_MAX_CHILDREN)
            report->children[report->child_count++] =
                make_gib(z, dx, dy, vx, vy, (int16_t)(i == 0 ? 0x10 : i == 1 ? 0x11 : 0x0F));
    }
}

/* EnemyZombieOfficerUpdate: within the activity window, state 0 walks, 1
 * attacks, 2 dies (gibs, then the generic death), then the animation step
 * and the on-screen collision pass; state 3 steps its animation. The
 * contact callback sub_080926CC is type 0, element 0. */
static AosHitReport officer_update(AosEnemy *z, AosSoma *soma, const AosCollision *layer,
                                   const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                   const AosWeaponFrames *weapon_frames, int soma_atk,
                                   int soma_def, int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    int sx = (z->x >> 16) - cam_x, sy = (z->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
    switch (z->state) {
    case 0: officer_walk(z, soma, layer); break;
    case 1: officer_attack(z, kind->anims); break;
    case 2:
        if (z->timer == 0x28) officer_gibs(z, random, &spawned);
        generic_death(z, kind, random, &spawned);
        break;
    case 3:
        step_anim(z, kind->anims);
        return report;
    default: break;
    }
    if (!z->removed) {
        step_anim(z, kind->anims);
        sx = (z->x >> 16) - cam_x;
        sy = (z->y >> 16) - cam_y;
        if (!z->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
            report = collide(z, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        aos_combat_tick(&z->combat);
    }
    report.child_count = spawned.child_count;
    memcpy(report.children, spawned.children, sizeof spawned.children[0] * spawned.child_count);
    take_effects(&report, &spawned);
    return report;
}

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
static int ground_walk(AosEnemy *e, const AosSoma *soma, const AosCollision *layer,
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
    int side = e->x < soma->x ? 1 : e->x > soma->x ? -1 : 0;
    if (side != (e->mirrored ? 1 : -1)) return false;
    int32_t dx = pixel_delta(soma->x, e->x), dy = pixel_delta(soma->y, e->y);
    return (int16_t)isqrt((uint32_t)(dx * dx + dy * dy)) <= range;
}

/* The walk speed of the axe armor for its walk frame (frame % 9 <= 6). */
static int32_t armor_speed(const AosEnemy *a) {
    unsigned r = a->anim.frame % 9;
    return r > 6 ? 0 : 0x1000 + (int32_t)r * 0x2800;
}

/* sub_080B0F1C: walking with the probe walker (mode 0x14: no slopes, no
 * ledges); without a patrol length (parameter 0) it turns around whenever
 * it is not simply on a floor, otherwise after that many steps (frame 17).
 * At the start of frame 17, the player ahead within 99 pixels starts an
 * attack. */
static void armor_walk(AosEnemy *a, const AosSoma *soma, const AosCollision *layer,
                       const AosEnemyKind *kind) {
    if (a->step != 0) return;
    bool frame17 = a->anim.frame == 0x11 && a->anim.tick == 0;
    if (frame17 && a->param0) {
        uint8_t steps = a->timer++;
        if (steps > a->param0) {
            a->timer = 0;
            a->mirrored = !a->mirrored;
        }
    }
    int result = ground_walk(a, soma, layer, &kind->probes, armor_speed(a), 0x14);
    if (!a->param0 && (result & 0x1B) != 8) a->mirrored = !a->mirrored;
    if (frame17 && player_ahead_within(a, soma, 0x63)) {
        a->state = 1;
        a->step = a->substep = 0;
    }
}

/* sub_080B0D5C setup: an axe 24 pixels ahead and 16 up, with the armor's
 * facing, contact power and throw height. */
static AosEnemy make_axe(const AosEnemy *armor) {
    AosEnemy axe = {0};
    axe.id = armor->id;
    axe.role = AOS_ROLE_AXE;
    axe.static_frame = -1;
    axe.x = armor->x + (armor->mirrored ? 0x180000 : (int32_t)0xFFE80000);
    axe.y = armor->y - 0x100000;
    axe.mirrored = armor->mirrored;
    axe.substep = armor->substep;
    axe.stats = armor->stats;
    axe.combat.type = 0x0A;
    return axe;
}

/* sub_080B1030: a high or low throw at random (animation 1 or 2); at frame
 * 12, tick 2, the axe leaves; when the animation ends, with the player still
 * ahead within 99 pixels it backs away playing the walk once, otherwise it
 * walks again. */
static void armor_attack(AosEnemy *a, const AosSoma *soma, const AosCollision *layer,
                         const AosEnemyKind *kind, uint32_t (*random)(void),
                         AosHitReport *report) {
    switch (a->step) {
    case 0:
        if (random() & 1) {
            play_loop(a, kind->anims, 1, false);
            a->substep = 0;
        } else {
            play_loop(a, kind->anims, 2, false);
            a->substep = 1;
        }
        a->step = 1;
        break;
    case 1:
        if (a->anim.frame == 12 && a->anim.tick == 2) {
            report->children[report->child_count++] = make_axe(a);
        }
        if (a->anim.flags & AOS_ANIM_ENDED) {
            if (player_ahead_within(a, soma, 0x63)) {
                play_loop(a, kind->anims, 0, false);
                a->step = 2;
            } else {
                a->step = 3;
            }
        }
        break;
    case 2:
        ground_walk(a, soma, layer, &kind->probes, -armor_speed(a), 0x14);
        if (a->anim.flags & AOS_ANIM_ENDED) a->step = 0;
        break;
    case 3:
        a->state = a->step = a->substep = 0;
        play(a, kind->anims, 0);
        break;
    default:
        break;
    }
}

/* sub_080B0D5C: the first update starts animation 3 (sound 0x86) and the
 * flight: vx 2.5 toward the throw, decelerated by 0x800 per frame (it comes
 * back), a spin of 0x800 per frame (a high throw starts 20 pixels higher
 * and spins the other way), one hit point and a 16 x 16 box (type 0xA).
 * Later updates run the collision pass first: struck, it vanishes with a
 * flame of sub_0806D5C0 within 8 x 8 pixels. It is deleted beyond screen
 * margin 4. The global pause of gEwramData + 0x4BE is not ported. */
static AosHitReport axe_update(AosEnemy *axe, AosSoma *soma, const AosEnemyKind *kind,
                               const AosWeaponEntity *weapon,
                               const AosWeaponFrames *weapon_frames, int soma_atk,
                               int soma_def, int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (axe->state != 0) {
        report = collide(axe, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        if (axe->hp <= 0) {
            aos_audio_play_song(0x86);
            /* sub_0806D5C0(x, y, 8.0, 8.0): a flame where it was struck. */
            spawn_flame_at(axe->x, axe->y, 0x80000, 0x80000, random, &report);
            axe->removed = true;
            aos_combat_tick(&axe->combat);
            return report;
        }
    }
    if (axe->state == 0) {
        play_loop(axe, kind->anims, 3, false);
        step_anim(axe, kind->anims);
        axe->vx = (int32_t)0xFFFD8000;
        axe->ax = 0x800;
        axe->spin = 0x800;
        if (axe->substep == 1) {
            axe->y -= 0x140000;
            axe->spin = -axe->spin;
        }
        if (axe->mirrored) {
            axe->vx = -axe->vx;
            axe->ax = -axe->ax;
            axe->spin = -axe->spin;
        }
        axe->state = 1;
        axe->hp = 1;
        axe->own_boxes = true;
        axe->own_hurt = axe->own_attack = (AosBox){-8, -8, 16, 16};
    } else {
        /* sub_0806D430(e, 2.5) */
        axe->vx += axe->ax;
        if (axe->ax >= 0 ? axe->vx > 0x28000 : axe->vx < -0x28000)
            axe->vx = axe->ax >= 0 ? 0x28000 : -0x28000;
        axe->x += axe->vx;
    }
    axe->angle += (uint32_t)axe->spin;
    if (outside_margins(kind, 4, (axe->x >> 16) - cam_x, (axe->y >> 16) - cam_y))
        axe->removed = true;
    aos_combat_tick(&axe->combat);
    return report;
}

/* sub_080B11DC: death animation 4 (sound 0x6F) until it ends, then the
 * deletion. + 0x4C (from -40.0, + 0x12000 per frame) is the height of the
 * effects and + 0x48 (from 0, + 0x5000) their phase: during the first 32
 * frames, on even frames a fire (sub_08045CEC animation 1, row 8, vy 0.5,
 * ay -0x1000) at x + 16 * sin(phase + 0x4800) +/- 2.0, and every 8 frames
 * (sound 0x6E) a spark (animation 16, row 6) at x +/- 8 and the height
 * +/- 2 pixels. */
static void armor_die(AosEnemy *a, const AosEnemyKind *kind, uint32_t (*random)(void),
                      AosHitReport *report) {
    if (a->step == 0) {
        aos_audio_play_song(0x6F);
        play_loop(a, kind->anims, 4, false);
        a->step = 1;
        a->vy = (int32_t)0xFFD80000;
        a->vx = 0;
        return;
    }
    a->timer++;
    int32_t height = a->y + a->vy;
    a->vy += 0x12000;
    int32_t phase = a->vx;
    a->vx += 0x5000;
    if (a->anim.flags & AOS_ANIM_ENDED) {
        a->removed = true;
        return;
    }
    if (a->timer > 0x20) return;
    if ((a->timer & 1) == 0) {
        int32_t offset = aos_sine((uint32_t)(phase + 0x4800)) << 4;
        int32_t x = a->x + offset + random_centred(random, 0x40000);
        AosEffect *fire = spawn_drift(AOS_EFFECT_FIRE, x, height, report);
        if (fire) {
            fire->vy = 0x8000;
            fire->ay = -0x1000;
        }
    }
    if ((a->timer & 7) == 0) {
        aos_audio_play_song(0x6E);
        int32_t x = a->x + (int32_t)((uint32_t)random_centred(random, 16) << 16);
        int32_t y = height + (int32_t)((uint32_t)random_centred(random, 4) << 16);
        spawn_drift(AOS_EFFECT_SPARK, x, y, report);
    }
}

/* EnemyAxeArmorUpdate: inside the activity window, the state, then the
 * collision pass (sub_0806E314) before the animation step. */
static AosHitReport armor_update(AosEnemy *a, AosSoma *soma, const AosCollision *layer,
                                 const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                 const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
                                 int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    int sx = (a->x >> 16) - cam_x, sy = (a->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
    switch (a->state) {
    case 0: armor_walk(a, soma, layer, kind); break;
    case 1: armor_attack(a, soma, layer, kind, random, &spawned); break;
    case 2:
        armor_die(a, kind, random, &spawned);
        if (a->removed) {
            take_effects(&report, &spawned);
            return report;
        }
        break;
    default:
        step_anim(a, kind->anims);
        return report;
    }
    sx = (a->x >> 16) - cam_x;
    sy = (a->y >> 16) - cam_y;
    if (!a->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
        report = collide(a, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    step_anim(a, kind->anims);
    aos_combat_tick(&a->combat);
    report.child_count = spawned.child_count;
    memcpy(report.children, spawned.children, sizeof spawned.children[0] * spawned.child_count);
    take_effects(&report, &spawned);
    return report;
}

/* sub_080AF8D0 / sub_080AFA9C: the next volley, in the order 0, 0, 1, 1, 2
 * (+ 0x14 counts 0..4); it plays animation 2 + volley once. */
static void archer_volley(AosEnemy *a, const AosAnimSet *anims) {
    int volley = a->volley_count / 2;
    if (++a->volley_count > 4) a->volley_count = 0;
    a->state = 1;
    a->step = a->substep = 0;
    a->volley = (uint8_t)volley;
    a->shot = 0;
    play_loop(a, anims, (unsigned)volley + 2, false);
}

/* sub_080AF934 step 0: an arrow (sub_080AF7EC) at the archer's position
 * plus the entry's y offset, with its facing and contact power, one hit
 * point, sprite frame 25 and a 2 x 2 box 18 pixels ahead and 24 up for both
 * roles (collision type 0xA). Sound 0x85. */
static AosEnemy make_arrow(const AosEnemy *archer, int8_t y_offset) {
    AosEnemy arrow = {0};
    arrow.id = archer->id;
    arrow.role = AOS_ROLE_ARROW;
    arrow.static_frame = 25;
    arrow.x = archer->x;
    arrow.y = archer->y + (int32_t)((uint32_t)(int32_t)y_offset << 16);
    arrow.mirrored = archer->mirrored;
    arrow.stats = archer->stats;
    arrow.hp = 1;
    arrow.combat.type = 0x0A;
    arrow.own_boxes = true;
    arrow.own_hurt = arrow.own_attack = (AosBox){-18, -24, 2, 2};
    return arrow;
}

/* sub_080AF8D0: standing, it faces the player and shoots when he enters
 * the 240 x 35 box centred on it (sub_0806E29C). */
static void archer_stand(AosEnemy *a, const AosSoma *soma, const AosAnimSet *anims) {
    face_player(a, soma);
    if (a->step == 0 && player_in(soma, (int16_t)((a->x >> 16) - 0x78),
                                  (int16_t)((a->y >> 16) - 0x11), 0xF0, 0x23))
        archer_volley(a, anims);
}

/* sub_080AF934: an arrow at the start of each frame its volley lists, then
 * a 32-frame pause after the animation, then standing or patrolling again. */
static void archer_shoot(AosEnemy *a, const AosEnemyKind *kind, AosHitReport *report) {
    if (a->step == 0) {
        unsigned v = a->volley < 4 ? a->volley : 0;
        if (a->shot < kind->volley_sizes[v] && a->shot < 8 &&
            kind->volleys[v][a->shot][0] == (int8_t)a->anim.frame && a->anim.tick == 0) {
            /* The arrow entity is created, then sound 0x85. */
            report->children[report->child_count++] = make_arrow(a, kind->volleys[v][a->shot][1]);
            aos_audio_play_song(0x85);
            a->shot++;
        }
        if (a->anim.flags & AOS_ANIM_ENDED) {
            a->step = 1;
            a->timer = 0x20;
        }
        return;
    }
    if (a->timer) {
        a->timer--;
        return;
    }
    a->step = a->substep = 0;
    if (!a->param0) {
        a->state = 0;
        play(a, kind->anims, 0);
    } else {
        a->state = 2;
        play(a, kind->anims, 1);
    }
}

/* sub_080AFA9C: patrolling at 0.25 (turning every 129 frames, no ledges,
 * slowed on slopes), backing away at 0.75 facing the player once he is
 * ahead within 79 pixels, and shooting when he is farther than 99. */
static void archer_patrol(AosEnemy *a, const AosSoma *soma, const AosCollision *layer,
                          const AosEnemyKind *kind) {
    if (a->step == 0) {
        if (a->timer++ > 0x80) {
            a->timer = 0;
            a->mirrored = !a->mirrored;
        }
        ground_walk(a, soma, layer, &kind->probes, 0x4000, 0xC);
        if (player_ahead_within(a, soma, 0x4F)) a->step = 1;
        return;
    }
    ground_walk(a, soma, layer, &kind->probes, (int32_t)0xFFFF4000, 0xD);
    int32_t dx = pixel_delta(soma->x, a->x), dy = pixel_delta(soma->y, a->y);
    if ((int16_t)isqrt((uint32_t)(dx * dx + dy * dy)) > 0x63) archer_volley(a, kind->anims);
}

/* sub_080AF7EC: flying at 3.0 toward its facing with the collision pass;
 * stuck to Soma after a hit (state 1, 30 frames; the + 0x0F change at 10
 * frames left is not traced); struck (no hit point), it vanishes (state 2)
 * with a flame of sub_0806D5C0 within 4 x 4 pixels of (x + 8 * vx,
 * y - 22). Deleted beyond screen margin 4. The global pause 0x4BE is not
 * ported. */
static AosHitReport arrow_update(AosEnemy *arrow, AosSoma *soma, const AosEnemyKind *kind,
                                 const AosWeaponEntity *weapon,
                                 const AosWeaponFrames *weapon_frames, int soma_atk,
                                 int soma_def, int cam_x, int cam_y,
                                 uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (arrow->state != 2 && arrow->hp <= 0) arrow->state = 2;
    if (outside_margins(kind, 4, (arrow->x >> 16) - cam_x, (arrow->y >> 16) - cam_y)) {
        arrow->removed = true;
        return report;
    }
    switch (arrow->state) {
    case 0:
        arrow->vx = arrow->mirrored ? 0x30000 : -0x30000;
        arrow->x += arrow->vx;
        report = collide(arrow, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        break;
    case 1:
        arrow->x = soma->x + arrow->ax;
        arrow->y = soma->y + arrow->ay;
        if (arrow->timer) arrow->timer--;
        else arrow->removed = true;
        break;
    default:
        spawn_flame_at(arrow->x + arrow->vx * 8, arrow->y - 0x160000, 0x40000, 0x40000, random,
                       &report);
        arrow->removed = true;
        return report;
    }
    aos_combat_tick(&arrow->combat);
    return report;
}

/* A debris piece (sub_0806C5AC) of its parent's kind, playing `anim`. */
static AosEnemy make_debris(const AosEnemy *parent, const AosAnimSet *anims, unsigned anim) {
    AosEnemy d = {0};
    d.id = parent->id;
    d.role = AOS_ROLE_DEBRIS;
    d.static_frame = -1;
    d.x = parent->x;
    d.y = parent->y;
    d.mirrored = parent->mirrored;
    d.combat.attack_off = d.combat.hurt_off = true;
    play_loop(&d, anims, anim, true);
    step_anim(&d, anims);
    return d;
}

/* sub_0806C5AC: a piece keeps 79/80 of vx, adds its accelerations (both
 * clamped to 8.0) and moves; it vanishes when the centre of its sprite's
 * last OAM component, 4 pixels lower (sub_0806C48C), is in a solid cell
 * (sub_080020A0), playing sound 0x162 and leaving a dust puff of sub_08045CEC
 * (animation 4, row 6) at that centre: vy = -0.25 +/- 0x800 with ay = vy / 32,
 * vx = +/- 0x1000 - the piece's vx with ax = -vx / 64 (the random draws are
 * made only when the puff is created; its + 0x0F = 1 is not modelled). It
 * also vanishes when it leaves the screen widened by 32 pixels. It never
 * collides. */
static void debris_update(AosEnemy *d, const AosCollision *layer, const AosEnemyKind *kind,
                          int cam_x, int cam_y, uint32_t (*random)(void), AosHitReport *report) {
    d->vx = d->vx * 79 / 80 + d->ax;
    if (d->ax >= 0 ? d->vx > 0x80000 : d->vx < -0x80000) d->vx = d->ax >= 0 ? 0x80000 : -0x80000;
    d->vy += d->ay;
    if (d->ay >= 0 ? d->vy > 0x80000 : d->vy < -0x80000) d->vy = d->ay >= 0 ? 0x80000 : -0x80000;
    d->x += d->vx;
    d->y += d->vy;
    int ax = 0, ay = 0;
    if (d->static_frame >= 0 && d->static_frame < 64) {
        ax = kind->static_anchors[d->static_frame][0];
        ay = kind->static_anchors[d->static_frame][1];
    } else if (d->anim.id < AOS_ENEMY_MAX_ANIMS && d->anim.frame < AOS_ENEMY_MAX_FRAMES) {
        ax = kind->anchors[d->anim.id][d->anim.frame][0];
        ay = kind->anchors[d->anim.id][d->anim.frame][1];
    }
    if (d->mirrored) ax = -ax;
    if (d->vflip) ay = -ay;
    int32_t px = (d->x >> 16) + ax, py = (d->y >> 16) + ay + 4;
    if (aos_collision_cell(layer, px, py) & 1) {
        AosEffect *dust = spawn_drift(AOS_EFFECT_DUST, d->x + (int32_t)((uint32_t)ax << 16),
                                      d->y + (int32_t)((uint32_t)ay << 16), report);
        if (dust) {
            dust->vy = random_centred(random, 0x1000) - 0x4000;
            dust->ay = dust->vy / 32;
            dust->vx = random_centred(random, 0x2000) - d->vx;
            dust->ax = -dust->vx / 64;
        }
        aos_audio_play_song(0x162);
        d->removed = true;
        return;
    }
    int sx = (d->x >> 16) - cam_x, sy = (d->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x20) > 0x130 || (uint16_t)(sy + 0x20) > 0xE0) d->removed = true;
}

/* sub_080AFB9C: sound 0x6B, then seven pieces (animations 5 to 11) fly away from the
 * attacker at 0.5 to 2.4, rising at 0.5 to 3.5 with gravity 0x2000. */
static void archer_shatter(const AosEnemy *a, const AosEnemyKind *kind, uint32_t (*random)(void),
                           AosHitReport *report) {
    aos_audio_play_song(0x6B);
    for (unsigned anim = 5; anim <= 11 && report->child_count < AOS_ENEMY_MAX_CHILDREN; ++anim) {
        AosEnemy d = make_debris(a, kind->anims, anim);
        d.vx = (int32_t)(((random() & 0xF) << 13) + 0x8000) * a->away;
        d.vy = (int32_t)((random() & 0x1F) << 12) + (int32_t)0xFFFC8000;
        d.ay = 0x2000;
        d.timer = (uint8_t)((random() & 0xF) + 0x28);
        report->children[report->child_count++] = d;
    }
}

/* EnemySkullArcherUpdate: inside the activity window, the state, then the
 * collision pass before the animation step; state 3 shatters it into its
 * pieces (sound 0x6B) and deletes it at once. */
static AosHitReport archer_update(AosEnemy *a, AosSoma *soma, const AosCollision *layer,
                                  const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                  const AosWeaponFrames *weapon_frames, int soma_atk,
                                  int soma_def, int cam_x, int cam_y,
                                  uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    int sx = (a->x >> 16) - cam_x, sy = (a->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
    switch (a->state) {
    case 0: archer_stand(a, soma, kind->anims); break;
    case 1: archer_shoot(a, kind, &spawned); break;
    case 2: archer_patrol(a, soma, layer, kind); break;
    case 3:
        archer_shatter(a, kind, random, &report);
        a->removed = true;
        return report;
    default:
        step_anim(a, kind->anims);
        return report;
    }
    sx = (a->x >> 16) - cam_x;
    sy = (a->y >> 16) - cam_y;
    if (!a->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
        report = collide(a, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    step_anim(a, kind->anims);
    aos_combat_tick(&a->combat);
    report.child_count = spawned.child_count;
    memcpy(report.children, spawned.children, sizeof spawned.children[0] * spawned.child_count);
    return report;
}

/* sub_08093A3C: vy swings by + 0x54 (subtracted while + 0x20 is set),
 * + 0x20 flips every + 0x22 frames (+ 0x21 counts), then the move
 * (sub_0806D3AC); rising it plays animation 2, falling animation 0 or 1
 * at random. */
static void medusa_fly(AosEnemy *m, const AosAnimSet *anims, uint32_t (*random)(void)) {
    if (m->step != 0) return;
    m->vy += m->dropped ? -m->ay : m->ay;
    if (++m->volley_count >= (int8_t)m->shot) {
        m->volley_count = 0;
        m->dropped = !m->dropped;
    }
    m->x += m->vx;
    m->y += m->vy;
    if (m->vy >= 0) {
        if (m->anim.id == 2) play(m, anims, (random() & 1) ? 0 : 1);
    } else if (m->anim.id != 2) {
        play(m, anims, 2);
    }
}

/* EnemyMedusaHeadUpdate state 3: a spawner creates a head one frame in 32,
 * while fewer than parameter 0 exist (+ 0x18), at screen X 256 when Soma's
 * screen X is at most 0x67, or at most 0x88 facing right (sub_0806CEC0),
 * else at -16, and screen Y 64 + (r & 0x3F); the head takes parameter 1
 * bit 0 (gold). */
static void medusa_spawner(AosEnemy *spawner, const AosSoma *soma, int cam_x, int cam_y,
                           uint32_t (*random)(void), AosHitReport *report) {
    if (random() & 0x1F) return;
    if (spawner->spawned_count >= spawner->param0) return;
    spawner->spawned_count++;
    int px = (soma->x >> 16) - cam_x;
    int sx = px <= 0x67 || (px <= 0x88 && !soma->facing_left) ? 256 : -16;
    report->spawn = true;
    report->spawn_x = cam_x + sx;
    report->spawn_y = cam_y + (int32_t)(random() & 0x3F) + 0x40;
    report->spawn_param1 = (int16_t)(spawner->param1 & 1);
}

/* EnemyMedusaHeadUpdate: outside the margins of sub_0806D128(e, 4) a head
 * dies at once (state 2, timer 1); sub_0806AD24 stuns it; state 0 flies,
 * 2 is the generic death, 3 the spawner, 4 inert; then the animation step
 * and the collision pass (sub_0806E314). The contact callback sub_08093BF0
 * uses element 0x100 for gold heads (its petrification is not ported) and
 * the tracking list of sub_08021924 is not. */
static AosHitReport medusa_update(AosEnemy *m, AosSoma *soma, const AosEnemyKind *kind,
                                  const AosWeaponEntity *weapon,
                                  const AosWeaponFrames *weapon_frames, int soma_atk,
                                  int soma_def, int cam_x, int cam_y,
                                  uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    if (m->state != 3 &&
        outside_margins(kind, 4, (m->x >> 16) - cam_x, (m->y >> 16) - cam_y)) {
        m->state = 2;
        m->timer = 1;
    }
    if (m->hp > 0 && m->hit_flash) {
        m->hit_flash--;
        if (m->state != 2) report = collide(m, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        aos_combat_tick(&m->combat);
        return report;
    }
    switch (m->state) {
    case 0: medusa_fly(m, kind->anims, random); break;
    case 2:
        generic_death(m, kind, random, &spawned);
        if (m->removed) return spawned;
        break;
    case 3:
        medusa_spawner(m, soma, cam_x, cam_y, random, &report);
        return report;
    case 4:
        step_anim(m, kind->anims);
        return report;
    default:
        break;
    }
    step_anim(m, kind->anims);
    int sx = (m->x >> 16) - cam_x, sy = (m->y >> 16) - cam_y;
    if (!m->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
        report = collide(m, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    aos_combat_tick(&m->combat);
    take_effects(&report, &spawned);
    return report;
}

/* sub_080CB5E0: the animation loops with a pause of 0 to 127 frames
 * (RandomNumberGenerator & 0x7F) after every cycle; with parameter 0 set
 * it bobs: y += sin(phase + 0x4000) / 4 (rounded toward zero), phase +=
 * 0x400. */
static void needles_idle(AosEnemy *n, const AosAnimSet *anims, uint32_t (*random)(void)) {
    if (n->step == 0) {
        if (n->anim.id != 0) play(n, anims, 0);
        n->cycle_done = true;
        n->step = 1;
        n->substep = 0;
    }
    if (n->substep == 0) {
        if (n->cycle_done) {
            n->substep = 1;
            n->timer = (uint8_t)(random() & 0x7F);
            n->cycle_done = false;
            n->anim.flags |= 2;
        }
    } else if (--n->timer == 0xFF) {
        n->substep = 0;
        n->anim.flags &= (uint8_t)~2;
    }
    if (!n->param0) return;
    int32_t s = aos_sine((uint32_t)n->phase + 0x4000);
    int32_t dy = (s < 0 ? s + 3 : s) >> 2;
    n->phase = (uint16_t)(n->phase + 0x400);
    n->x += n->vx;
    n->y += dy;
    n->vx += n->ax;
    n->vy = dy + n->ay;
}

/* sub_080CB6BC: five pieces from 0x08528B44 (s8 dx, s8 dy, sprite frame,
 * s8 vx >> 12, s8 vy >> 12 upward, frames), mirrored with the body. */
static void needles_shatter(const AosEnemy *n, AosHitReport *report) {
    static const int8_t pieces[5][6] = {
        {-4, -4, 5, -8, 48, 60}, {0, 0, 5, 8, 24, 30}, {0, 0, 4, -8, 40, 38},
        {0, 0, 3, 8, 40, 48}, {0, 0, 2, 0, 48, 48},
    };
    for (int i = 0; i < 5 && report->child_count < AOS_ENEMY_MAX_CHILDREN; ++i) {
        AosEnemy p = {0};
        p.id = n->id;
        p.role = AOS_ROLE_NEEDLE_PIECE;
        p.static_frame = pieces[i][2];
        p.mirrored = n->mirrored;
        int dx = pieces[i][0];
        p.vx = pieces[i][3] * 0x1000;
        if (p.mirrored) {
            p.vx = -p.vx;
            dx = -dx;
        }
        p.x = n->x + (int32_t)((uint32_t)dx << 16);
        p.y = n->y + (int32_t)((uint32_t)pieces[i][1] << 16);
        p.vy = -(pieces[i][4] * 0x1000);
        p.timer = (uint8_t)pieces[i][5];
        p.combat.attack_off = p.combat.hurt_off = true;
        report->children[report->child_count++] = p;
    }
}

/* sub_080CB7C8: a piece is deleted outside sub_08068AD4(e, -128, -64,
 * 0x1F0, 0x120), when its timer runs out or below screen Y 0xAA; otherwise
 * it moves and falls (vy += 0x3000). */
static void needle_piece_update(AosEnemy *p, int cam_x, int cam_y) {
    int sx = (p->x >> 16) - cam_x, sy = (p->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120 || --p->timer == 0xFF ||
        sy > 0xAA) {
        p->removed = true;
        return;
    }
    p->x += p->vx;
    p->y += p->vy;
    p->vy += 0x3000;
}

/* EnemyNeedlesUpdate: state 3 steps the animation; state 2 (sub_080CB894)
 * hides it, throws its pieces (sound 0x70; sub_080683BC is not ported) and
 * deletes it once they are gone (+ 0x13); otherwise, inside the activity
 * window, sub_080CB5E0, the animation step and the collision pass. */
static AosHitReport needles_update(AosEnemy *n, AosSoma *soma, const AosEnemyKind *kind,
                                   const AosWeaponEntity *weapon,
                                   const AosWeaponFrames *weapon_frames, int soma_atk,
                                   int soma_def, int cam_x, int cam_y,
                                   uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (n->state == 3) {
        step_anim(n, kind->anims);
        return report;
    }
    if (n->state == 2) {
        if (n->step == 0) {
            /* sub_080CB894 step 0: sound 0x70. */
            aos_audio_play_song(0x70);
            n->hidden = true;
            needles_shatter(n, &report);
            n->spawned_count = report.child_count;
            n->step = 1;
        }
        if (n->spawned_count == 0) n->removed = true;
        return report;
    }
    int sx = (n->x >> 16) - cam_x, sy = (n->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
    needles_idle(n, kind->anims, random);
    step_anim(n, kind->anims);
    sx = (n->x >> 16) - cam_x;
    sy = (n->y >> 16) - cam_y;
    if (!n->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
        report = collide(n, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    aos_combat_tick(&n->combat);
    return report;
}

static void make_static_child(AosEnemy *child, const AosEnemy *owner, uint8_t role,
                              int frame) {
    *child = (AosEnemy){.id = owner->id,
                        .x = owner->x,
                        .y = owner->y,
                        .role = role,
                        .static_frame = (int16_t)frame,
                        .mirrored = owner->mirrored,
                        .stats = owner->stats};
    child->combat.type = AOS_TYPE_ENEMY;
}

/* The Peeping Eye creates two eye overlays and six linked tendrils. The
 * native tendril updater uses a polar chain; the compact representation keeps
 * the same parent lifetime and frame order while the attack choreography is
 * still being reconstructed. */
static void peeping_eye_children(const AosEnemy *owner, AosHitReport *report) {
    static const int frames[] = {0xF, 0x10, 0xB, 0xB, 0xC, 0xC, 0xD, 0xE};
    for (unsigned i = 0; i < sizeof frames / sizeof frames[0] &&
                         report->child_count < AOS_ENEMY_MAX_CHILDREN; ++i) {
        AosEnemy child;
        make_static_child(&child, owner, AOS_ROLE_PEEPING_SEGMENT, frames[i]);
        child.variant = (uint8_t)i;
        child.hp = owner->hp;
        child.angle = owner->angle + i * 0x1800u;
        child.speed = 0x50000 + (int32_t)i * 0x18000;
        child.x += fixed_mul(aos_sine(child.angle + 0x4000u), child.speed);
        child.y += fixed_mul(aos_sine(child.angle), child.speed);
        report->children[report->child_count++] = child;
    }
}

typedef struct {
    uint8_t extension;
    uint8_t duration;
} PeepingEyeIntroStage;

/* sub_08088168's initial state walks this traced, terminating sequence before
 * choosing a movement target.  The extension value is consumed by the linked
 * tendril presentation below; the later random target and ceiling-probe
 * branches are still deliberately outside this pass. */
static const PeepingEyeIntroStage peeping_eye_intro[] = {
    {3, 2}, {2, 2}, {1, 2}, {0, 4}, {1, 2},
    {2, 2}, {3, 2}, {2, 2}, {1, 2}, {0, 20},
};

static int32_t peeping_eye_radius(const AosEnemy *owner, unsigned i) {
    int32_t radius = 0x50000 + (int32_t)i * 0x18000;
    if (owner->step == 1 && owner->substep < sizeof peeping_eye_intro / sizeof peeping_eye_intro[0])
        return radius + radius * peeping_eye_intro[owner->substep].extension / 3;
    return radius;
}

/* EnemyBonePillarCreate creates one linked animation-0 record for each
 * clamped parameter unit, then leaves the original record at the tower's
 * top. The linked records are visual only in this pass: their native per-part
 * collision and hit callbacks still need their own reconstruction. */
static void bone_pillar_children(const AosEnemy *owner, const AosAnimSet *anims,
                                 AosHitReport *report) {
    for (int i = 0; i < owner->param0 && report->child_count < AOS_ENEMY_MAX_CHILDREN; ++i) {
        AosEnemy child = {0};
        child.id = owner->id;
        child.role = AOS_ROLE_BONE_PILLAR_SEGMENT;
        child.static_frame = -1;
        child.variant = (uint8_t)i;
        child.x = owner->x;
        child.y = owner->y + AOS_FIXED((owner->param0 - i) * 22);
        child.mirrored = owner->mirrored;
        child.stats = owner->stats;
        child.hp = owner->hp;
        child.combat.type = AOS_TYPE_ENEMY;
        child.combat.attack_off = child.combat.hurt_off = true;
        play_loop(&child, anims, 0, true);
        step_anim(&child, anims);
        report->children[report->child_count++] = child;
    }
}

/* sub_080CBA30 creates Buer's second direct frame as a linked component.
 * The native helper follows the parent while it remains in its active state;
 * its independent fire collision and later states are intentionally left for
 * the dedicated attack pass. */
static void buer_fire_child(const AosEnemy *owner, AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy child;
    make_static_child(&child, owner, AOS_ROLE_BUER_FIRE, 1);
    child.combat.attack_off = child.combat.hurt_off = true;
    report->children[report->child_count++] = child;
}

/* White Dragon's nine body records all use animation 1 and retain the native
 * owner link. The full ROM routine bends the chain with affine state; this
 * first pass keeps the nine animated parts visibly attached to the head. */
static void white_dragon_children(const AosEnemy *owner, const AosAnimSet *anims,
                                  AosHitReport *report, unsigned first, unsigned count) {
    for (unsigned i = first; i < first + count && i < 9 &&
                         report->child_count < AOS_ENEMY_MAX_CHILDREN; ++i) {
        AosEnemy child = {0};
        child.id = owner->id;
        child.role = AOS_ROLE_WHITE_DRAGON_PART;
        child.static_frame = -1;
        child.variant = (uint8_t)i;
        child.angle = owner->phase + i * 0x1000u;
        /* sub_080771C8 assigns the middle body record the shared affine
         * matrix at unit scale; the other records remain regular OAM. */
        child.scaled = i == 5;
        child.scale = child.scaled ? 0xFFFF : 0;
        child.x = owner->x + (owner->mirrored ? 1 : -1) * (int32_t)((i + 1) * 0x80000);
        child.y = owner->y + (int32_t)(aos_sine(child.angle) / 8);
        child.mirrored = owner->mirrored;
        child.stats = owner->stats;
        child.hp = owner->hp;
        child.combat.type = AOS_TYPE_ENEMY;
        play_loop(&child, anims, 1, true);
        step_anim(&child, anims);
        report->children[report->child_count++] = child;
    }
}

static AosHitReport linked_early_child_update(AosEnemy *child, const AosEnemyKind *kind) {
    AosHitReport report = {0};
    const AosEnemy *owner = child->parent;
    if (!owner || owner->removed || owner->defeated) {
        child->removed = true;
        return report;
    }
    child->mirrored = owner->mirrored;
    child->hidden = owner->hidden;
    if (child->role == AOS_ROLE_PEEPING_SEGMENT) {
        unsigned i = child->variant;
        /* The native child stores its own polar angle/radius. The parent
         * target angle supplies the attack direction while phase keeps the
         * chain moving between target updates. */
        uint16_t angle = (uint16_t)(owner->angle + owner->phase + i * 0x1800u);
        int32_t radius = peeping_eye_radius(owner, i);
        child->angle = angle;
        child->x = owner->x + fixed_mul(aos_sine(angle + 0x4000), radius);
        child->y = owner->y + fixed_mul(aos_sine(angle), radius);
        return report;
    }
    if (child->role == AOS_ROLE_BONE_PILLAR_SEGMENT) {
        child->x = owner->x;
        child->y = owner->y + AOS_FIXED((owner->param0 - child->variant) * 22);
        step_anim(child, kind->anims);
        return report;
    }
    if (child->role == AOS_ROLE_BUER_FIRE) {
        child->x = owner->x;
        child->y = owner->y;
        return report;
    }
    unsigned i = child->variant;
    uint16_t bend = (uint16_t)(owner->phase + i * 0x1000u);
    int32_t spacing = (int32_t)((i + 1) * 0x80000);
    int32_t curve = 0x20000 + (int32_t)i * 0x8000;
    int32_t side = owner->mirrored ? 1 : -1;
    child->angle = (uint16_t)(bend + (owner->mirrored ? 0x8000u : 0));
    child->scaled = i == 5;
    child->scale = child->scaled ? 0xFFFF : 0;
    child->x = owner->x + side * (spacing + fixed_mul(aos_sine(bend + 0x4000u), curve));
    child->y = owner->y + fixed_mul(aos_sine(bend), curve);
    step_anim(child, kind->anims);
    return report;
}

/* sub_08075EDC: the five Harpy feathers (frames 12 down to 8), and
 * sub_08076054: the Siren's three notes (frames 14, 13, 14). */
static void harpy_shot(AosEnemy *owner, int variant, AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    static const uint16_t harpy_angles[2][5] = {
        {0x4000, 0x5000, 0x6000, 0x7000, 0x8000},
        {0x4000, 0x3000, 0x2000, 0x1000, 0x0000},
    };
    static const uint16_t siren_angles[2][3] = {
        {0x438E, 0x6000, 0x7C72},
        {0x3C72, 0x2000, 0x038E},
    };
    bool siren = owner->id == AOS_ENEMY_SIREN;
    int count = siren ? 3 : 5;
    if (variant < 0 || variant >= count) return;
    AosEnemy child;
    make_static_child(&child, owner, siren ? AOS_ROLE_SIREN_SHOT : AOS_ROLE_HARPY_SHOT,
                      siren ? (variant == 1 ? 13 : 14) : 12 - variant);
    child.variant = (uint8_t)variant;
    child.angle = siren ? siren_angles[owner->mirrored][variant]
                        : harpy_angles[owner->mirrored][variant];
    int32_t speed = siren ? 0x8000 : 0xC000;
    child.vx = fixed_mul(aos_sine(child.angle + 0x4000u), speed);
    child.vy = fixed_mul(aos_sine(child.angle), speed);
    child.x += owner->mirrored ? 0x10000 : -0x10000;
    child.y += 0x20000;
    child.own_boxes = true;
    child.own_hurt = child.own_attack = (AosBox){-3, -3, 6, 6};
    report->children[report->child_count++] = child;
}

static AosHitReport harpy_shot_update(AosEnemy *shot, AosSoma *soma,
                                      const AosEnemyKind *kind,
                                      const AosWeaponEntity *weapon,
                                      const AosWeaponFrames *weapon_frames, int soma_atk,
                                      int soma_def, int cam_x, int cam_y) {
    AosHitReport report = {0};
    if (shot->role == AOS_ROLE_SIREN_SHOT) {
        /* sub_080761B8: bend for 16 frames in one direction, then for 16
         * in the other, repeating the angular wave. */
        if ((shot->timer++ & 0x1F) < 16)
            shot->angle += 0x400;
        else
            shot->angle -= 0x400;
        shot->vx = fixed_mul(aos_sine(shot->angle + 0x4000u), 0x8000);
        shot->vy = fixed_mul(aos_sine(shot->angle), 0x8000);
    }
    integrate(shot);
    int sx = (shot->x >> 16) - cam_x, sy = (shot->y >> 16) - cam_y;
    if (sx < -32 || sx > 272 || sy < -32 || sy > 192) {
        shot->removed = true;
        return report;
    }
    report = collide(shot, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    if (report.enemy_hit) shot->removed = true;
    aos_combat_tick(&shot->combat);
    return report;
}

static void falling_fragment(AosEnemy *owner, int frame, AosHitReport *report,
                             uint32_t (*random)(void)) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy child;
    make_static_child(&child, owner, AOS_ROLE_GORGON_DEBRIS, frame);
    child.vx = (int32_t)(random() & 0xFFFF) - 0x8000;
    child.vy = frame == 8 ? (int32_t)(random() & 0x3FFFF) - 0x20000 : 0;
    child.ay = 0x1400;
    child.timer = (uint8_t)((random() & 0xF) + 0x28);
    child.combat.attack_off = child.combat.hurt_off = true;
    report->children[report->child_count++] = child;
}

static void falling_fragment_update(AosEnemy *piece, int cam_x, int cam_y) {
    integrate(piece);
    int sx = (piece->x >> 16) - cam_x, sy = (piece->y >> 16) - cam_y;
    piece->hidden = piece->timer < 12 && (piece->timer & 1);
    if (--piece->timer == 0 || sx < -32 || sx > 272 || sy > 192) piece->removed = true;
}

static AosHitReport harpy_update(AosEnemy *e, AosSoma *soma, const AosEnemyKind *kind,
                                 const AosWeaponEntity *weapon,
                                 const AosWeaponFrames *weapon_frames, int soma_atk,
                                 int soma_def, int cam_x, int cam_y,
                                 uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (e->state == 4) {
        step_anim(e, kind->anims);
        return report;
    }
    int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
    if (e->state == 2) {
        if (e->step == 0) {
            for (int i = 0; i < 4; ++i) falling_fragment(e, 8, &report, random);
            face_player(e, soma);
            e->vx = e->mirrored ? -0x10000 : 0x10000;
            e->vy = -0x20000;
            e->ay = 0x2000;
            e->step = 1;
        }
        integrate(e);
        generic_death(e, kind, random, &report);
        aos_combat_tick(&e->combat);
        return report;
    }
    if (e->state == 3) {
        if (e->step == 0) {
            face_player(e, soma);
            e->mirrored = !e->mirrored;
            e->vx = toward_facing(e, 0xC000);
            e->vy = -0x30000;
            e->ay = 0x2000;
            e->timer = 0;
            e->step = 1;
        }
        integrate(e);
        if (++e->timer >= 24) {
            stop(e);
            play(e, kind->anims, 0);
            e->state = 0;
            e->step = 10;
            e->timer = 0;
        }
    } else if (e->state == 0) {
        face_player(e, soma);
        /* sub_080756FC plays the shared Harpy/Siren wing sound while its
         * idle flight advances. This controller's hover is source-adapted,
         * so connect it to the corresponding animation boundary. */
        if (e->anim.frame == 1 && e->anim.tick == 0) aos_audio_play_song(0x8B);
        e->y += aos_sine(e->phase) / 32;
        e->phase = (uint16_t)(e->phase + 0x180);
        int dx = (int)((soma->x - e->x) >> 16);
        if (dx < 0) dx = -dx;
        if (++e->timer >= 30 && dx <= 0x59) {
            stop(e);
            e->state = 1;
            e->step = 0;
            e->timer = 0;
        }
    } else if (e->state == 1) {
        face_player(e, soma);
        if (e->step == 0) {
            if (random() & 1) {
                /* sub_0807594C steps 0..3: fly to Soma's X and 24 pixels
                 * above him, then brake back into the idle state. */
                play_loop(e, kind->anims, 2, false);
                aos_audio_play_song(0x87);
                e->walk_timer = soma->x;
                e->speed = soma->y - 0x180000;
                e->step = 1;
            } else {
                play(e, kind->anims, 1);
                e->step = 10;
            }
            e->timer = 0;
        } else if (e->step == 1) {
            if (move_toward(e, e->walk_timer, e->speed, 0xC000)) {
                e->step = 2;
                e->timer = 0;
            }
            integrate(e);
        } else if (e->step == 2) {
            e->vx -= e->vx / 8;
            e->vy -= e->vy / 8;
            integrate(e);
            int32_t avx = e->vx < 0 ? -e->vx : e->vx;
            int32_t avy = e->vy < 0 ? -e->vy : e->vy;
            if (++e->timer > 60 || (avx <= 0x7FFF && avy <= 0x7FFF)) e->step = 3;
        } else if (e->step == 3) {
            e->vx -= e->vx / 4;
            e->vy -= e->vy / 4;
            integrate(e);
            int32_t avx = e->vx < 0 ? -e->vx : e->vx;
            int32_t avy = e->vy < 0 ? -e->vy : e->vy;
            if (avx <= 0x7FFF && avy <= 0x7FFF) {
                stop(e);
                play(e, kind->anims, 0);
                e->state = 0;
                e->step = 10;
                e->timer = 0;
            }
        } else if (e->step == 10 && ++e->timer > 3) {
            for (int i = 0; i < 3; ++i)
                harpy_shot(e, e->id == AOS_ENEMY_SIREN ? i : i * 2, &report);
            aos_audio_play_song(0x89);
            e->step = 11;
            e->timer = 0;
        } else if (e->step == 11 && ++e->timer > 7) {
            if (e->id == AOS_ENEMY_HARPY) {
                harpy_shot(e, 1, &report);
                harpy_shot(e, 3, &report);
                aos_audio_play_song(0x89);
            }
            e->step = 12;
            e->timer = 0;
        } else if (e->step == 12 && ++e->timer > 3) {
            play(e, kind->anims, 0);
            e->state = 0;
            e->step = 10;
            e->timer = 0;
        }
    }
    step_anim(e, kind->anims);
    sx = (e->x >> 16) - cam_x;
    sy = (e->y >> 16) - cam_y;
    if (!e->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0) {
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        if (!report.child_count) report = hit;
        else {
            report.soma_hit = hit.soma_hit;
            report.soma_damage = hit.soma_damage;
            report.enemy_hit = hit.enemy_hit;
            report.enemy_damage = hit.enemy_damage;
            report.killed = hit.killed;
        }
    }
    aos_combat_tick(&e->combat);
    return report;
}

static void gorgon_breath(AosEnemy *owner, AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy child;
    make_static_child(&child, owner, AOS_ROLE_GORGON_BREATH, 25);
    child.x += owner->mirrored ? 0x2A0000 : -0x2A0000;
    child.y -= 0x120000;
    child.angle = owner->phase;
    child.vx = fixed_mul(aos_sine(child.angle + 0x4000u), 0xC000);
    child.vy = fixed_mul(aos_sine(child.angle), 0xC000);
    child.own_boxes = true;
    child.own_hurt = child.own_attack = (AosBox){-7, -7, 14, 14};
    report->children[report->child_count++] = child;
    owner->phase = (uint16_t)(owner->phase + 0x400);
}

static AosHitReport gorgon_breath_update(AosEnemy *breath, AosSoma *soma,
                                         const AosEnemyKind *kind,
                                         const AosWeaponEntity *weapon,
                                         const AosWeaponFrames *weapon_frames, int soma_atk,
                                         int soma_def, int cam_x, int cam_y) {
    AosHitReport report = {0};
    integrate(breath);
    if (++breath->timer == 25) breath->static_frame = 26;
    int sx = (breath->x >> 16) - cam_x, sy = (breath->y >> 16) - cam_y;
    if (breath->timer > 40 || sx < -32 || sx > 272 || sy < -32 || sy > 192) {
        breath->removed = true;
        return report;
    }
    if (breath->timer & 1)
        report = collide(breath, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    if (report.enemy_hit) breath->removed = true;
    aos_combat_tick(&breath->combat);
    return report;
}

static AosHitReport gorgon_update(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                                  const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                  const AosWeaponFrames *weapon_frames, int soma_atk,
                                  int soma_def, int cam_x, int cam_y,
                                  uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (e->state == 3) {
        step_anim(e, kind->anims);
        return report;
    }
    int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
    if (e->state == 2) {
        if (e->step == 0) {
            for (int frame = 17; frame <= 22; ++frame)
                falling_fragment(e, frame, &report, random);
            e->step = 1;
        }
        generic_death(e, kind, random, &report);
        aos_combat_tick(&e->combat);
        return report;
    }
    if (e->state == 1) {
        if (e->step == 0) {
            play_loop(e, kind->anims, 2, false);
            e->step = 1;
            e->timer = 0;
            e->phase = e->mirrored ? 0x4000 : 0xC000;
        } else if (e->step == 1) {
            if (e->cycle_done || ++e->timer >= 48) {
                e->step = 2;
                e->timer = 0;
            }
        } else if (e->step == 2) {
            if (e->timer >= 30 && e->timer <= 70 && !(e->timer & 1))
                gorgon_breath(e, &report);
            if (++e->timer > 76) {
                play(e, kind->anims, 0);
                e->state = 0;
                e->step = 1;
                e->timer = 0;
            }
        }
    } else {
        if (e->step == 0) {
            snap_to_floor(e, layer);
            e->step = 1;
            face_player(e, soma);
        }
        if (!e->vx) e->vx = toward_facing(e, 0x8000);
        if (walk_collide(e, layer, 0x10000, (int32_t)0xFFE00000, 0x8000, 12) & 3) {
            e->mirrored = !e->mirrored;
            e->vx = toward_facing(e, 0x8000);
        }
        int dx = (int)((soma->x - e->x) >> 16);
        int dy = (int)((soma->y - e->y) >> 16);
        if (dx < 0) dx = -dx;
        if (dy < 0) dy = -dy;
        if (++e->timer >= 45 && dx <= 0x50 && dy <= 0x18 && random() % 100 <= 50) {
            stop(e);
            face_player(e, soma);
            e->state = 1;
            e->step = 0;
            e->timer = 0;
        }
    }
    step_anim(e, kind->anims);
    sx = (e->x >> 16) - cam_x;
    sy = (e->y >> 16) - cam_y;
    if (!e->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0) {
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        if (!report.child_count) report = hit;
        else {
            report.soma_hit = hit.soma_hit;
            report.soma_damage = hit.soma_damage;
            report.enemy_hit = hit.enemy_hit;
            report.enemy_damage = hit.enemy_damage;
            report.killed = hit.killed;
        }
    }
    aos_combat_tick(&e->combat);
    return report;
}

static void merge_enemy_hit(AosHitReport *report, AosHitReport hit) {
    report->soma_hit = hit.soma_hit;
    report->soma_damage = hit.soma_damage;
    report->enemy_hit = hit.enemy_hit;
    report->enemy_damage = hit.enemy_damage;
    report->killed = hit.killed;
}

/* sub_080C5898: the Tiny Devil holds an aimed animation-4 orb beside its
 * body. The orb cycles animation 5 a short random number of times, changes
 * to animation 6 and then travels for 60 frames. */
static void tiny_devil_orb(AosEnemy *owner, const AosSoma *soma,
                           const AosEnemyKind *kind, AosHitReport *report,
                           uint32_t (*random)(void)) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy orb;
    make_static_child(&orb, owner, AOS_ROLE_TINY_DEVIL_ORB, -1);
    orb.x += owner->x < soma->x ? 0x100000 : -0x100000;
    orb.y += 0x80000;
    (void)move_toward(&orb, soma->x, soma->y - 0x200000, 0x20000);
    orb.state = 0;
    orb.variant = (uint8_t)((random() % 5) + 1);
    orb.hp = 1;
    orb.own_boxes = true;
    orb.own_hurt = orb.own_attack = (AosBox){-5, -5, 10, 10};
    play_loop(&orb, kind->anims, 4, false);
    report->children[report->child_count++] = orb;
}

static AosHitReport tiny_devil_orb_update(AosEnemy *orb, AosSoma *soma,
                                           const AosEnemyKind *kind,
                                           const AosWeaponEntity *weapon,
                                           const AosWeaponFrames *weapon_frames, int soma_atk,
                                           int soma_def, int cam_x, int cam_y) {
    AosHitReport report = {0};
    if (orb->state == 0) {
        step_anim(orb, kind->anims);
        if (orb->cycle_done) {
            play(orb, kind->anims, 5);
            orb->state = 1;
            orb->phase = 0;
        }
    } else if (orb->state == 1) {
        step_anim(orb, kind->anims);
        if (orb->cycle_done) {
            orb->cycle_done = false;
            if (++orb->phase > orb->variant) {
                /* sub_080C5A44: animation 6, then sound 0x89. */
                play_loop(orb, kind->anims, 6, false);
                aos_audio_play_song(0x89);
                orb->state = 2;
                orb->timer = 0;
                orb->scaled = true;
            }
        }
    } else {
        integrate(orb);
        step_anim(orb, kind->anims);
        int sx = (orb->x >> 16) - cam_x, sy = (orb->y >> 16) - cam_y;
        if (++orb->timer > 60 || sx < -32 || sx > 272 || sy < -32 || sy > 192) {
            orb->removed = true;
            return report;
        }
    }
    report = collide(orb, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    if (report.enemy_hit) orb->removed = true;
    aos_combat_tick(&orb->combat);
    return report;
}

/* sub_080C5010: Imp owns a hidden technical child for its entire lifetime.
 * State 3 makes it visible at Soma's position, 40 pixels above his feet. */
static void imp_helper(AosEnemy *owner, const AosEnemyKind *kind, AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy helper;
    make_static_child(&helper, owner, AOS_ROLE_IMP_HELPER, -1);
    helper.parent = owner;
    helper.hidden = true;
    helper.combat.attack_off = helper.combat.hurt_off = true;
    play(&helper, kind->anims, 3);
    report->children[report->child_count++] = helper;
    owner->helper_spawned = true;
}

static void imp_helper_update(AosEnemy *helper, const AosSoma *soma,
                              const AosEnemyKind *kind) {
    if (!helper->parent || helper->parent->removed || helper->parent->defeated) {
        helper->removed = true;
        return;
    }
    bool active = helper->parent->state == 3 && helper->parent->step <= 2;
    if (active && helper->hidden) play(helper, kind->anims, 3);
    helper->hidden = !active;
    if (active) {
        helper->x = soma->x;
        helper->y = soma->y - 0x280000;
        step_anim(helper, kind->anims);
    }
}

/* sub_080C50EC: Imp's death creates direct sprite frame 0x15, falling at
 * one and a half times the body's initial death velocity. */
static void imp_death_child(AosEnemy *owner, AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy child;
    make_static_child(&child, owner, AOS_ROLE_IMP_DEATH, 0x15);
    child.y += 0x100000;
    child.vy = owner->vy + owner->vy / 2;
    child.timer = 0x32;
    child.combat.attack_off = child.combat.hurt_off = true;
    report->children[report->child_count++] = child;
}

static void imp_death_child_update(AosEnemy *child, const AosEnemyKind *kind,
                                   int cam_y) {
    child->y += child->vy;
    child->hidden = kind->blink[child->timer] & 1;
    if (--child->timer == 0 || (child->y >> 16) - cam_y > 0xAA) child->removed = true;
}

static void devil_idle(AosEnemy *e, const AosSoma *soma, const AosEnemyKind *kind,
                       int cam_x, int cam_y, uint32_t (*random)(void)) {
    bool imp = e->id == AOS_ENEMY_IMP;
    face_player(e, soma);
    if (e->step == 0) {
        if (!e->rest) e->rest = (uint16_t)(100 + random() % (imp ? 360 : 500));
        int32_t side = imp ? 0x460000 : 0x190000;
        e->walk_timer = soma->x + (e->mirrored ? -side : side);
        e->speed = soma->y + (imp ? -0x2D0000 : -0x460000);
        e->timer = 0;
        e->step = 1;
    }
    bool arrived = move_toward(e, e->walk_timer, e->speed, imp ? 0xA000 : 0xC000);
    integrate(e);
    if (arrived || ++e->timer >= 60) e->step = 0;
    if (e->rest) --e->rest;
    int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
    if (!e->rest && !outside_margins(kind, 4, sx, sy)) {
        stop(e);
        e->state = 2;
        e->step = 0;
        e->timer = 0;
    }
}

static void tiny_devil_attack(AosEnemy *e, const AosSoma *soma,
                              const AosEnemyKind *kind, int cam_x, int cam_y,
                              AosHitReport *report, uint32_t (*random)(void)) {
    if (e->step == 0) {
        bool player_right = soma->x > e->x;
        e->vx = player_right ? 0x20000 : -0x20000;
        e->ax = player_right ? -0x800 : 0x800;
        e->vy = -0x10000;
        e->ay = 0x800;
        e->timer = 0;
        e->step = 1;
    } else if (e->step == 1) {
        integrate(e);
        int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
        if (sx < 8 || sx > 0xE8 || sy < 8 || sy > 0x98) {
            e->state = 1;
            e->step = 0;
            e->rest = 0;
        } else if (++e->timer > 30) {
            play(e, kind->anims, 2);
            tiny_devil_orb(e, soma, kind, report, random);
            stop(e);
            e->timer = 0;
            e->step = 2;
        }
    } else if (++e->timer > 80) {
        e->state = 1;
        e->step = 0;
        e->rest = 0;
    } else if (e->timer == 40) {
        play(e, kind->anims, 1);
    }
}

static void imp_approach(AosEnemy *e, const AosSoma *soma) {
    if (e->step == 0) {
        bool player_right = soma->x > e->x;
        e->vx = player_right ? 0x20000 : -0x20000;
        e->ax = player_right ? -0x800 : 0x800;
        e->vy = 0x20000;
        e->ay = -0x800;
        e->timer = 0;
        e->step = 1;
    } else if (e->step == 1) {
        integrate(e);
        if (++e->timer > 30) {
            stop(e);
            e->walk_timer = soma->x;
            e->speed = soma->y - 0x200000;
            (void)move_toward(e, e->walk_timer, e->speed, 0x4000);
            e->timer = 0;
            e->step = 2;
        }
    } else if (e->step == 2) {
        integrate(e);
        if (++e->timer > 80) {
            e->ax = e->vx > 0 ? -0x800 : 0x800;
            e->ay = e->vy > 0 ? -0x800 : 0x800;
            e->step = 3;
        }
    } else {
        int32_t old_vx = e->vx, old_vy = e->vy;
        integrate(e);
        if ((old_vx <= 0 && e->vx >= 0) || (old_vx >= 0 && e->vx <= 0))
            e->vx = e->ax = 0;
        if ((old_vy <= 0 && e->vy >= 0) || (old_vy >= 0 && e->vy <= 0))
            e->vy = e->ay = 0;
        if (!e->vx && !e->vy) {
            e->state = 3;
            e->step = 0;
            e->timer = 0;
        }
    }
}

static void imp_control(AosEnemy *e, AosSoma *soma, AosHitReport *report) {
    if (e->step == 0) {
        stop(e);
        e->timer = 0;
        e->patience = 0;
        e->step = 1;
    } else if (e->step == 1) {
        int32_t tx = soma->x + (soma->facing_left ? -0x100000 : 0x100000);
        bool arrived = move_toward(e, tx, soma->y - 0x200000, 0x20000);
        integrate(e);
        if (arrived) {
            report->force_input = true;
            report->forced_held = (e->timer & 1) ? AOS_KEY_ATTACK : 0;
            ++e->timer;
            /* sub_080C4D34 waits until the forced B presses have put Soma
             * outside his neutral states 0, 4 and 8 for 20 frames. */
            if (soma->state != 0 && soma->state != 4 && soma->state != 8 &&
                ++e->patience > 20) {
                e->vx = e->x < soma->x ? -0x10000 : 0x10000;
                e->vy = -0x10000;
                e->timer = 0;
                e->step = 2;
            }
        }
    } else if (e->step == 2) {
        integrate(e);
        if (++e->timer > 40) e->step = 3;
    } else {
        stop(e);
        e->state = 1;
        e->step = 0;
        e->rest = 0;
    }
}

static AosHitReport devil_update(AosEnemy *e, AosSoma *soma, const AosEnemyKind *kind,
                                 const AosWeaponEntity *weapon,
                                 const AosWeaponFrames *weapon_frames, int soma_atk,
                                 int soma_def, int cam_x, int cam_y,
                                 uint32_t (*random)(void)) {
    AosHitReport report = {0};
    bool imp = e->id == AOS_ENEMY_IMP;
    if (imp && !e->helper_spawned) imp_helper(e, kind, &report);
    if ((imp && e->state == 4) || (!imp && e->state == 3)) {
        step_anim(e, kind->anims);
        return report;
    }
    if (e->state == 0xFF) {
        if (e->step == 0) {
            /* sub_080C580C / sub_080C4ED4 step 0: sound 0x72. */
            aos_audio_play_song(0x72);
            if (imp) imp_death_child(e, &report);
            e->step = 1;
        }
        e->y += e->vy;
        generic_death(e, kind, random, &report);
        aos_combat_tick(&e->combat);
        return report;
    }
    int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x10) > 0x110 || (uint16_t)sy > 0xB0) return report;
    if (e->state == 1)
        devil_idle(e, soma, kind, cam_x, cam_y, random);
    else if (!imp && e->state == 2)
        tiny_devil_attack(e, soma, kind, cam_x, cam_y, &report, random);
    else if (imp && e->state == 2)
        imp_approach(e, soma);
    else if (imp && e->state == 3)
        imp_control(e, soma, &report);
    step_anim(e, kind->anims);
    AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    merge_enemy_hit(&report, hit);
    aos_combat_tick(&e->combat);
    return report;
}

/* sub_080C2ABC / sub_080C2B74: Ripper's short-range blade child. It is
 * placed twelve pixels toward the body's facing, accelerates horizontally,
 * and falls away after a weapon or player collision. */
static void ripper_blade(const AosEnemy *owner, const AosEnemyKind *kind,
                         AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy blade = {0};
    blade.id = owner->id;
    blade.role = AOS_ROLE_RIPPER_BLADE;
    blade.parent = owner;
    blade.x = owner->x + (owner->mirrored ? AOS_FIXED(12) : -AOS_FIXED(12));
    blade.y = owner->y - AOS_FIXED(12);
    blade.mirrored = owner->mirrored;
    blade.vx = owner->mirrored ? 0x30000 : -0x30000;
    blade.ax = owner->mirrored ? 0x50 : -0x50;
    blade.state = 4;
    blade.static_frame = -1;
    blade.stats = owner->stats;
    blade.hp = owner->hp;
    blade.combat.type = AOS_TYPE_ENEMY;
    blade.own_boxes = true;
    blade.own_hurt = blade.own_attack = (AosBox){-8, -8, 16, 16};
    play(&blade, kind->anims, 1);
    report->children[report->child_count++] = blade;
}

static AosHitReport ripper_blade_update(AosEnemy *b, AosSoma *soma,
                                        const AosEnemyKind *kind,
                                        const AosWeaponEntity *weapon,
                                        const AosWeaponFrames *weapon_frames, int soma_atk,
                                        int soma_def, int cam_x, int cam_y) {
    AosHitReport report = {0};
    if (b->state == 4) {
        b->vx += b->ax;
        b->x += b->vx;
    } else {
        b->vy += b->ay;
        b->y += b->vy;
    }
    step_anim(b, kind->anims);
    int sx = (b->x >> 16) - cam_x, sy = (b->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) {
        b->removed = true;
        return report;
    }
    report = collide(b, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    if (report.enemy_hit || report.soma_hit) {
        b->state = 6;
        b->vx = b->ax = 0;
        b->vy = 0x8000;
        b->ay = 0x1000;
        b->combat.attack_off = b->combat.hurt_off = true;
    }
    aos_combat_tick(&b->combat);
    return report;
}

/* sub_080C25F8 and sub_080C2A18. */
static AosHitReport ripper_update(AosEnemy *e, AosSoma *soma,
                                  const AosCollision *layer, const AosEnemyKind *kind,
                                  const AosWeaponEntity *weapon,
                                  const AosWeaponFrames *weapon_frames, int soma_atk,
                                  int soma_def, int cam_x, int cam_y,
                                  uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (e->state == 3) {
        step_anim(e, kind->anims);
        return report;
    }
    if (e->state == 0xFF) {
        if (e->step == 0) {
            play(e, kind->anims, 2);
            e->timer = 50;
            e->vx = e->ax = e->vy = 0;
            e->ay = 0x2000;
            e->step = 1;
        }
        e->vy += e->ay;
        e->y += e->vy;
        generic_death(e, kind, random, &report);
        aos_combat_tick(&e->combat);
        return report;
    }
    int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;

    if (e->step == 0) {
        play(e, kind->anims, 0);
        e->phase = 0;
        e->vx = toward_facing(e, 0x8000);
        e->step = e->dropped ? 1 : 4;
    }
    if (e->step == 1) {
        e->vy = e->ay = 0x2000;
        e->step = 2;
    } else if (e->step == 2) {
        if (e->phase != 3) {
            play(e, kind->anims, 3);
            e->phase = 3;
        }
        if (walk_collide(e, layer, 0, -AOS_FIXED(10), 0, 8) & 4) {
            snap_to_floor(e, layer);
            e->timer = 0;
            e->step = 3;
        }
    } else if (e->step == 3) {
        if (++e->timer > 15) {
            e->step = 4;
            e->phase = 0;
            play(e, kind->anims, 0);
        }
    } else if (e->step == 4) {
        e->x += e->vx;
        face_player(e, soma);
        uint32_t jump = random() % 6;
        int32_t speed = jump == 3 ? 0x1A000 : 0xD000;
        e->vx = toward_facing(e, speed);
        e->vy = -0x30000;
        if (jump == 3) e->ay = 0x2800;
        else if (jump == 4) e->ay = 0x1400;
        else e->ay = 0xA000;
        if (jump == 3 || jump == 4) {
            play(e, kind->anims, 3);
            e->phase = 3;
            e->firing = false;
            e->step = 5;
        } else {
            play(e, kind->anims, 0);
            e->phase = 0;
            e->step = 6;
        }
        int dx = (e->x >> 16) - (soma->x >> 16);
        if (dx >= -63 && dx <= 63 && (e->x >> 16) > 0 && (e->x >> 16) < 239)
            e->vx = -e->vx;
    } else if (e->step == 5 || e->step == 6) {
        if (e->step == 5 && e->phase == 3 && !e->firing && e->vy >= -e->ay && e->vy <= e->ay) {
            play_loop(e, kind->anims, 1, false);
            e->phase = 1;
            e->firing = true;
        }
        if (e->step == 5 && e->phase == 1 && (e->cycle_done || e->anim.frame > 0)) {
            ripper_blade(e, kind, &report);
            play(e, kind->anims, 3);
            e->phase = 3;
        }
        int collision = walk_collide(e, layer, 0, -AOS_FIXED(10), 0, 8);
        if (collision) {
            if (collision & 4) snap_to_floor(e, layer);
            e->step = 4;
            e->phase = 0;
            e->firing = false;
            play(e, kind->anims, 0);
        }
    }

    step_anim(e, kind->anims);
    sx = (e->x >> 16) - cam_x;
    sy = (e->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0) {
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        merge_enemy_hit(&report, hit);
    }
    aos_combat_tick(&e->combat);
    return report;
}

/* sub_08071648 / sub_08071764: the projectile emitted after Merman's
 * animation-3 wind-up. */
static void merman_shot(const AosEnemy *owner, const AosEnemyKind *kind,
                        AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy shot = {0};
    shot.id = owner->id;
    shot.role = AOS_ROLE_MERMAN_SHOT;
    shot.x = owner->x + (owner->mirrored ? AOS_FIXED(5) : -AOS_FIXED(5));
    shot.y = owner->y - AOS_FIXED(24);
    shot.vx = owner->mirrored ? 0x20000 : -0x20000;
    shot.mirrored = owner->mirrored;
    shot.static_frame = -1;
    shot.stats = owner->stats;
    shot.hp = owner->hp;
    shot.combat.type = AOS_TYPE_ENEMY;
    play(&shot, kind->anims, 6);
    report->children[report->child_count++] = shot;
}

static AosHitReport merman_shot_update(AosEnemy *shot, AosSoma *soma,
                                       const AosEnemyKind *kind,
                                       const AosWeaponEntity *weapon,
                                       const AosWeaponFrames *weapon_frames, int soma_atk,
                                       int soma_def, int cam_x, int cam_y,
                                       uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (shot->state == 2) {
        generic_death(shot, kind, random, &report);
        return report;
    }
    shot->x += shot->vx;
    step_anim(shot, kind->anims);
    int sx = (shot->x >> 16) - cam_x, sy = (shot->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x10) > 0x110 || (uint16_t)sy > 0xB0) {
        shot->removed = true;
        return report;
    }
    report = collide(shot, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    if ((report.enemy_hit || report.soma_hit) && !shot->removed) {
        shot->state = 2;
        shot->timer = 40;
        shot->combat.attack_off = shot->combat.hurt_off = true;
    }
    aos_combat_tick(&shot->combat);
    return report;
}

/* EnemyMermanUpdate, sub_08070E68 and sub_080714A4. The collision layer
 * does not encode the separate room-water height, so the native surface
 * crossings are represented by their exact timed phases around the spawn
 * Y while the solid probes remain native BG1 probes. */
static AosHitReport merman_update(AosEnemy *e, AosSoma *soma,
                                  const AosCollision *layer, const AosEnemyKind *kind,
                                  const AosWeaponEntity *weapon,
                                  const AosWeaponFrames *weapon_frames, int soma_atk,
                                  int soma_def, int cam_x, int cam_y,
                                  uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (e->state == 4) {
        step_anim(e, kind->anims);
        return report;
    }
    if (e->state == 2) {
        e->vy += e->ay;
        e->y += e->vy;
        generic_death(e, kind, random, &report);
        aos_combat_tick(&e->combat);
        return report;
    }
    if (e->state == 3) {
        if ((random() & 7) == 0 && e->spawned_count < 3 && e->param0 == 1) {
            report.spawn = true;
            report.spawn_x = cam_x + 0x90 + (int32_t)(random() & 0xFF);
            report.spawn_y = (e->y >> 16) + 0x20;
            report.spawn_param1 = 0;
            e->spawned_count++;
            e->timer = 60;
        }
        return report;
    }

    int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
    if (e->state == 1) {
        if (e->step == 0) {
            play_loop(e, kind->anims, 3, false);
            e->step = 1;
        } else if (e->step == 1 && e->cycle_done) {
            /* sub_080714A4: sound 0x7A, then the shot. */
            aos_audio_play_song(0x7A);
            merman_shot(e, kind, &report);
            e->step = 2;
        } else if (e->step == 2 && e->cycle_done) {
            e->state = 0;
            e->step = e->param0 ? 0 : 6;
            e->timer = 0;
            e->vy = -0x40000;
            e->ay = 0x1800;
            e->vx = toward_facing(e, 0x8000);
            play(e, kind->anims, 2);
        }
    } else if (e->step == 0) {
        if (++e->timer > 60) {
            e->timer = 0;
            e->step = 1;
            e->vy = -0x8000;
        }
    } else if (e->step == 1) {
        e->y += e->vy;
        e->vy -= 0x8000;
        if (++e->timer >= 8) {
            e->step = 2;
            e->timer = 0;
        }
    } else if (e->step == 2) {
        face_player(e, soma);
        e->vx = toward_facing(e, 0x8000);
        e->vy = -0x30000;
        e->ay = 0x1800;
        play(e, kind->anims, 2);
        e->step = 6;
    } else if (e->step == 4) {
        if (++e->timer > 32) {
            e->step = 2;
            e->timer = 0;
        }
    } else { /* Steps 5, 6, 9 and 10 are the native ballistic phases. */
        int collision = walk_collide(e, layer, 0x10000, (int32_t)0xFFE00000, 0, 8);
        if (collision & 4) {
            if (++e->timer > 60) {
                e->timer = 0;
                if ((random() & 3) == 0) {
                    e->state = 1;
                    e->step = 0;
                    stop(e);
                } else {
                    face_player(e, soma);
                    e->vx = toward_facing(e, 0x8000);
                    e->vy = -0x30000;
                    e->ay = 0x1800;
                }
            }
        }
    }

    step_anim(e, kind->anims);
    sx = (e->x >> 16) - cam_x;
    sy = (e->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0) {
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        merge_enemy_hit(&report, hit);
    }
    aos_combat_tick(&e->combat);
    return report;
}

static void peeping_eye_controller(AosEnemy *e, const AosSoma *soma,
                                   uint32_t (*random)(void)) {
    int32_t dx = pixel_delta(soma->x, e->x), dy = pixel_delta(soma->y, e->y);
    e->angle = aos_arctan2((int16_t)dx, (int16_t)dy);
    /* sub_08088168 begins its attack when Soma enters the 180 by 200
     * window centred 90 pixels left and 100 pixels above the eye. */
    if (e->step == 0 &&
        player_in(soma, (e->x >> 16) - 90, (e->y >> 16) - 100, 180, 200)) {
        e->step = 1;
        e->timer = 0;
        e->substep = 0;
    }
    if (e->step == 1) {
        e->phase = (uint16_t)(e->phase + 0x300);
        const PeepingEyeIntroStage *stage = &peeping_eye_intro[e->substep];
        if (++e->timer >= stage->duration) {
            e->timer = 0;
            if (++e->substep == sizeof peeping_eye_intro / sizeof peeping_eye_intro[0]) {
                e->substep = 0;
                /* State 2 below picks the next random target. The following
                 * state-3 visual-variant table remains outside this compact
                 * motion pass. */
                e->step = 2;
            }
        }
    } else if (e->step == 2) {
        /* Native state 2 chooses its next point 64..112 pixels left or
         * right and 32..80 pixels upward, then immediately enters the
         * steering state. */
        int32_t horizontal = AOS_FIXED(64) + (int32_t)(random() % AOS_FIXED(48));
        e->target_x = e->x + ((random() & 1) ? horizontal : -horizontal);
        e->target_y = e->y - AOS_FIXED(32) - (int32_t)(random() % AOS_FIXED(48));
        e->vx = e->vy = 0;
        e->phase = (uint16_t)(e->phase + 0x200);
        e->timer = (uint8_t)(random() & 0x3F);
        e->step = 3;
    } else if (e->step == 3) {
        /* sub_08088168 damps both axes by 31/32, accelerates only while the
         * target is more than four pixels away, and caps the result at two
         * pixels per frame. Its state-3 visual variant table is still open. */
        int32_t dx = e->target_x - e->x;
        int32_t dy = e->target_y - e->y;
        e->ax = dx > AOS_FIXED(4) ? 0x8000 : dx < -AOS_FIXED(4) ? -0x800 : 0;
        e->ay = dy > AOS_FIXED(4) ? 0x8000 : dy < -AOS_FIXED(4) ? -0x800 : 0;
        e->vx = e->vx * 31 / 32 + e->ax;
        e->vy = e->vy * 31 / 32 + e->ay;
        if (e->vx > 0x20000) e->vx = 0x20000;
        if (e->vx < -0x20000) e->vx = -0x20000;
        if (e->vy > 0x20000) e->vy = 0x20000;
        if (e->vy < -0x20000) e->vy = -0x20000;
        e->x += e->vx;
        e->y += e->vy;
        e->phase = (uint16_t)(e->phase + 0x200);
        if ((dx < 0 ? -dx : dx) <= AOS_FIXED(4) &&
            (dy < 0 ? -dy : dy) <= AOS_FIXED(4)) {
            e->step = 2;
        } else if (e->timer++ >= 120) {
            e->step = 2;
        }
    } else {
        e->phase = (uint16_t)(e->phase + 0x180);
    }
    if (e->step != 3) e->y = AOS_FIXED(e->home) + aos_sine(e->phase) / 32;
    face_player(e, soma);
}

static void white_dragon_controller(AosEnemy *e, const AosSoma *soma) {
    face_player(e, soma);
    e->phase = (uint16_t)(e->phase + 0x100);
    e->y = AOS_FIXED(e->home) + aos_sine(e->phase) / 32;
    e->x += toward_facing(e, 0x2000);
}

static void buer_controller(AosEnemy *e, const AosSoma *soma) {
    face_player(e, soma);
    e->x += e->vx;
    if ((e->x >> 16) <= e->home_left || (e->x >> 16) >= e->home_right) {
        e->mirrored = !e->mirrored;
        e->vx = toward_facing(e, 0x18000);
    }
    e->phase = (uint16_t)(e->phase + 0x400);
    e->y = AOS_FIXED(e->home) - abs(aos_sine(e->phase) / 8);
}

static void rock_armor_controller(AosEnemy *e, const AosSoma *soma) {
    face_player(e, soma);
    e->x += e->vx;
    if ((e->x >> 16) <= e->home_left || (e->x >> 16) >= e->home_right) {
        e->mirrored = !e->mirrored;
        e->vx = toward_facing(e, 0x6000);
    }
    e->phase = (uint16_t)(e->phase + 0x180);
    e->y = AOS_FIXED(e->home) + aos_sine(e->phase) / 24;
}

static void giant_ghost_controller(AosEnemy *e, const AosSoma *soma) {
    move_toward(e, soma->x, soma->y - AOS_FIXED(24), 0x4000);
    e->phase = (uint16_t)(e->phase + 0x180);
    e->x += e->vx;
    e->y += e->vy + aos_sine(e->phase) / 32;
    face_player(e, soma);
}

static void winged_skeleton_controller(AosEnemy *e, const AosSoma *soma,
                                       const AosEnemyKind *kind, int cam_x, int cam_y) {
    (void)soma;
    e->x += e->vx;
    e->phase = (uint16_t)(e->phase + 0x280);
    e->y = AOS_FIXED(e->home) + aos_sine(e->phase) / 12;
    if (outside_margins(kind, 0, (e->x >> 16) - cam_x, (e->y >> 16) - cam_y)) {
        e->mirrored = !e->mirrored;
        e->vx = toward_facing(e, 0xC000);
    }
}

/* sub_08089E64: after animation 0 has appeared, animation 1 attacks while
 * the Ghost accelerates toward Soma at a random height. Near that target it
 * receives a two-pixel impulse away from a point 64 pixels below Soma. */
static void ghost_controller(AosEnemy *g, const AosSoma *soma, const AosEnemyKind *kind,
                             uint32_t (*random)(void)) {
    if (g->step == 0) {
        if (!g->cycle_done) return;
        g->step = 1;
        play(g, kind->anims, 1);
        g->combat.attack_off = g->combat.hurt_off = false;
        g->speed = -(int32_t)(random() & 0xFFFFF);
        return;
    }

    face_player(g, soma);
    int32_t dx = soma->x - g->x;
    int32_t dy = soma->y - g->y + g->speed -
                 ((dx < 0 ? -dx : dx) <= 0x5FFFFF ? 0x100000 : 0x200000);
    g->ax = dx > 0x40000 ? 0x400 : dx < -0x40000 ? -0x400 : 0;
    g->ay = dy > 0x40000 ? 0x400 : dy < -0x40000 ? -0x400 : 0;
    g->vx = g->vx * 31 / 32;
    g->vy = g->vy * 31 / 32;
    g->vx += g->ax;
    g->vy += g->ay;
    if (g->ax >= 0 ? g->vx > 0x400000 : g->vx < -0x400000)
        g->vx = g->ax >= 0 ? 0x400000 : -0x400000;
    if (g->ay >= 0 ? g->vy > 0x400000 : g->vy < -0x400000)
        g->vy = g->ay >= 0 ? 0x400000 : -0x400000;
    g->x += g->vx;
    g->y += g->vy;

    if ((dx < 0 ? -dx : dx) > 0x3FFFF || (dy < 0 ? -dy : dy) > 0x3FFFF) return;
    int16_t away_x = (int16_t)pixel_delta(g->x, soma->x);
    int16_t away_y = (int16_t)pixel_delta(g->y + 0x400000, soma->y);
    uint16_t angle = aos_arctan2(away_x, away_y);
    angle = (uint16_t)(angle - 0x1000 + (random() & 0x1FFF));
    g->vx = fixed_mul(aos_sine(angle + 0x4000u), 0x20000);
    g->vy = fixed_mul(aos_sine(angle), 0x20000);
}

/* EnemyGhostUpdate: the normal attack/death states, the event-state variant
 * and state 3's hidden room producer. The producer uses raw 0..239 and
 * 16..191 room coordinates exactly as the native routine does. */
static AosHitReport enemy_ghost_update(AosEnemy *g, AosSoma *soma,
                                       const AosEnemyKind *kind,
                                       const AosWeaponEntity *weapon,
                                       const AosWeaponFrames *weapon_frames, int soma_atk,
                                       int soma_def, int cam_x, int cam_y,
                                       uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (g->state == 3) {
        if (g->producer_timer++ < (uint16_t)g->param1 || (random() & 0x1F)) return report;
        g->producer_timer = 0;
        if (g->spawned_count >= g->param0) return report;
        int x = 0, y = 0;
        bool found_x = false, found_y = false;
        for (int attempt = 0; attempt < 256 && !found_x; ++attempt) {
            x = (int)(random() % 0xF0);
            int32_t distance = soma->x - AOS_FIXED(x);
            found_x = (distance < 0 ? -distance : distance) > 0x300000;
        }
        for (int attempt = 0; attempt < 256 && !found_y; ++attempt) {
            y = (int)(random() % 0xB0) + 0x10;
            int32_t distance = soma->y - AOS_FIXED(y);
            found_y = (distance < 0 ? -distance : distance) > 0x300000;
        }
        if (!found_x || !found_y) return report;
        g->spawned_count++;
        report.spawn = true;
        report.spawn_x = x;
        report.spawn_y = y;
        return report;
    }
    if (g->defeated || g->state == 1) {
        generic_death(g, kind, random, &report);
        aos_combat_tick(&g->combat);
        return report;
    }
    if (g->state == 2) {
        if (g->step == 0 && g->cycle_done) {
            g->step = 1;
            play(g, kind->anims, 1);
        }
        step_anim(g, kind->anims);
        return report;
    }

    int sx = (g->x >> 16) - cam_x, sy = (g->y >> 16) - cam_y;
    if (outside_margins(kind, 4, sx, sy)) return report;
    g->hidden = false;
    ghost_controller(g, soma, kind, random);
    step_anim(g, kind->anims);
    sx = (g->x >> 16) - cam_x;
    sy = (g->y >> 16) - cam_y;
    g->hidden = outside_margins(kind, 4, sx, sy);
    if ((uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0) {
        AosHitReport hit = collide(g, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        merge_enemy_hit(&report, hit);
    }
    aos_combat_tick(&g->combat);
    return report;
}

static bool early_enemy_inert(const AosEnemy *e) {
    if ((e->id == AOS_ENEMY_SKELETON || e->id == AOS_ENEMY_KILLER_FISH) && e->state == 5)
        return true;
    if ((e->id == AOS_ENEMY_BUER || e->id == AOS_ENEMY_WINGED_SKELETON) && e->state == 4)
        return true;
    if ((e->id == AOS_ENEMY_ROCK_ARMOR || e->id == AOS_ENEMY_GIANT_GHOST) && e->state == 3)
        return true;
    return (e->id == AOS_ENEMY_PEEPING_EYE || e->id == AOS_ENEMY_BONE_PILLAR ||
            e->id == AOS_ENEMY_WHITE_DRAGON || e->id == AOS_ENEMY_GHOST) && e->state == 2;
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

    if (e->id == AOS_ENEMY_PEEPING_EYE && !e->helper_spawned) {
        peeping_eye_children(e, &report);
        e->helper_spawned = true;
    } else if (e->id == AOS_ENEMY_BONE_PILLAR && !e->helper_spawned) {
        bone_pillar_children(e, kind->anims, &report);
        e->helper_spawned = true;
    } else if (e->id == AOS_ENEMY_BUER && !e->helper_spawned) {
        buer_fire_child(e, &report);
        e->helper_spawned = true;
    } else if (e->id == AOS_ENEMY_WHITE_DRAGON && e->spawned_count < 9) {
        unsigned remaining = (unsigned)(9 - e->spawned_count);
        unsigned emit = remaining > AOS_ENEMY_MAX_CHILDREN ? AOS_ENEMY_MAX_CHILDREN : remaining;
        white_dragon_children(e, kind->anims, &report, e->spawned_count, emit);
        e->spawned_count = (uint8_t)(e->spawned_count + report.child_count);
    }

    switch (e->id) {
    case AOS_ENEMY_SKELETON:
        e->x += e->vx;
        if ((e->x >> 16) <= e->home_left || (e->x >> 16) >= e->home_right) {
            e->mirrored = !e->mirrored;
            e->vx = toward_facing(e, 0x8000);
        }
        break;
    case AOS_ENEMY_ROCK_ARMOR:
        rock_armor_controller(e, soma);
        break;
    case AOS_ENEMY_PEEPING_EYE:
        peeping_eye_controller(e, soma, random);
        break;
    case AOS_ENEMY_KILLER_FISH:
        e->x += e->vx;
        e->phase += 0x300;
        e->y = AOS_FIXED(e->home) + aos_sine(e->phase) / 16;
        if (outside_margins(kind, 0, (e->x >> 16) - cam_x, (e->y >> 16) - cam_y)) {
            e->mirrored = !e->mirrored;
            e->vx = toward_facing(e, 0xC000);
        }
        break;
    case AOS_ENEMY_BONE_PILLAR:
        face_player(e, soma);
        break;
    case AOS_ENEMY_BUER:
        buer_controller(e, soma);
        break;
    case AOS_ENEMY_WHITE_DRAGON:
        white_dragon_controller(e, soma);
        break;
    case AOS_ENEMY_GIANT_GHOST:
        giant_ghost_controller(e, soma);
        break;
    case AOS_ENEMY_WINGED_SKELETON:
        winged_skeleton_controller(e, soma, kind, cam_x, cam_y);
        break;
    default: break;
    }
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
    AosHitReport report = enemy_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk,
                                       soma_def, cam_x, cam_y, random);
    int sx = (enemy->x >> 16) - cam_x, sy = (enemy->y >> 16) - cam_y;
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
    if (enemy->role == AOS_ROLE_PEEPING_SEGMENT ||
        enemy->role == AOS_ROLE_BONE_PILLAR_SEGMENT ||
        enemy->role == AOS_ROLE_BUER_FIRE ||
        enemy->role == AOS_ROLE_WHITE_DRAGON_PART)
        return linked_early_child_update(enemy, kind);
    if (enemy->id == AOS_ENEMY_GHOST && enemy->role == AOS_ROLE_ENEMY)
        return enemy_ghost_update(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def,
                                  cam_x, cam_y, random);
    if (is_early_enemy(enemy->id) && enemy->role == AOS_ROLE_ENEMY)
        return early_enemy_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk,
                                  soma_def, cam_x, cam_y, random);
    if (enemy->role == AOS_ROLE_HARPY_SHOT || enemy->role == AOS_ROLE_SIREN_SHOT)
        return harpy_shot_update(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def,
                                 cam_x, cam_y);
    if (enemy->role == AOS_ROLE_GORGON_BREATH)
        return gorgon_breath_update(enemy, soma, kind, weapon, weapon_frames, soma_atk,
                                    soma_def, cam_x, cam_y);
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
        return gorgon_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk, soma_def,
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
        return trigger_update(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def,
                              random);
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
                              random);
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
        return mimic_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk, soma_def,
                            cam_x, cam_y, random);
    if (enemy->id == AOS_ENEMY_FLEAMAN)
        return flea_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk, soma_def,
                           random);
    if (enemy->id == AOS_ENEMY_MEDUSA_HEAD && enemy->role == AOS_ROLE_ENEMY)
        return medusa_update(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def, cam_x,
                             cam_y, random);
    if (enemy->id == AOS_ENEMY_GHOST_DANCER)
        return ghost_dancer_update(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def,
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
        needle_piece_update(enemy, cam_x, cam_y);
        return report;
    }
    if (enemy->id == AOS_ENEMY_NEEDLES)
        return needles_update(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def, cam_x,
                              cam_y, random);
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
        return warrior_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk,
                              soma_def, cam_x, cam_y, random);
    if (enemy->id == AOS_ENEMY_DEAD_CRUSADER)
        return crusader_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk,
                               soma_def, cam_x, cam_y, random);
    if (enemy->id == AOS_ENEMY_SKELETON_KNIGHT)
        return knight_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk, soma_def,
                             cam_x, cam_y, random);
    if (enemy->id == AOS_ENEMY_CURLY || enemy->id == AOS_ENEMY_DURGA)
        return curly_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk, soma_def,
                            cam_x, cam_y, random);
    if (enemy->id == AOS_ENEMY_GARGOYLE || enemy->id == AOS_ENEMY_GREMLIN)
        return gargoyle_update(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def,
                               cam_x, cam_y, random);
    if (enemy->id == AOS_ENEMY_ZOMBIE_OFFICER)
        return officer_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk, soma_def,
                              cam_x, cam_y, random);
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
