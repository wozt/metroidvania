/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies; comments name the cvaos routine. */
#include "aos_enemy.h"

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
static uint32_t umod(uint32_t value, uint32_t divisor) {
    return divisor ? value % divisor : 0;
}

static void spawn_flame_at(int32_t cx, int32_t cy, uint32_t w, uint32_t h,
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

static void append_effects(AosHitReport *to, const AosHitReport *from) {
    for (int index = 0;
         index < from->effect_count &&
         to->effect_count < AOS_ENEMY_MAX_EFFECTS;
         ++index)
        to->effects[to->effect_count++] = from->effects[index];
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

/* sub_080695B4: the same polar solution, written to acceleration instead
 * of velocity. */
static bool accelerate_toward(AosEnemy *e, int32_t tx, int32_t ty, int32_t speed) {
    int32_t dx = pixel_delta(tx, e->x), dy = pixel_delta(ty, e->y);
    uint32_t distance = isqrt((uint32_t)(dx * dx + dy * dy));
    if (!distance) {
        e->ax = e->ay = 0;
        return true;
    }
    uint16_t angle = aos_arctan2((int16_t)dx, (int16_t)dy);
    e->ax = fixed_mul(aos_sine(angle + 0x4000u), speed);
    e->ay = fixed_mul(aos_sine(angle), speed);
    return (int32_t)(distance << 16) <= speed;
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

static int32_t target_x(const AosSoma *soma) {
    if (no_target && aim_self) return aim_self->x;
    return decoy.active ? decoy.x : soma->x;
}
static int32_t target_y(const AosSoma *soma) {
    if (no_target && aim_self) return aim_self->y;
    return decoy.active ? decoy.y : soma->y;
}

/* sub_0806BC40 */
static void face_player(AosEnemy *enemy, const AosSoma *soma) {
    if (target_x(soma) == enemy->x) return;
    enemy->mirrored = enemy->x < target_x(soma);
}

/* sub_0806E120 */
static int32_t toward_facing(const AosEnemy *enemy, int32_t speed) {
    return enemy->mirrored ? speed : -speed;
}

/* sub_0806BDEC */
static int16_t player_dy(const AosEnemy *enemy, const AosSoma *soma) {
    return (int16_t)((enemy->y >> 16) - (target_y(soma) >> 16));
}

/* sub_08068AD4 with the player as the entity. */
static bool player_in(const AosSoma *soma, int32_t x, int32_t y, int w, int h) {
    return (uint16_t)((target_x(soma) >> 16) - x) <= w && (uint16_t)((target_y(soma) >> 16) - y) <= h;
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
    zombie->mirrored = (zombie->x >> 16) < (target_x(soma) >> 16);
    if (on_slope(layer, zombie->x >> 16, zombie->y >> 16)) zombie_vanish(zombie);
    if (zombie->y <= y0 - 0x80000) zombie_vanish(zombie);
}

/* Constructor and update must agree on validity of ROM-local Buer probes. */

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
        id != AOS_ENEMY_KICKER_SKELETON &&
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

static void skeleton_shatter(AosEnemy *e, const AosSoma *soma, AosHitReport *report);
static void nemesis_struck(AosEnemy *n);
static void peeping_struck(AosEnemy *e, const AosSoma *soma);
static void ectoplasm_struck(AosEnemy *e, const AosSoma *soma, const AosAnimSet *anims);
static void fish_struck(AosEnemy *f, const AosEnemyKind *kind);
static void dragon_struck(AosEnemy *d, uint8_t attacker);
static void skeleton_merge(AosHitReport *report, const AosHitReport *hit);

/* sub_0806E314 / sub_080421AC: the player against the enemy, then the
 * weapon against the enemy, then the contact callbacks. */
static void worm_segment_hurt(AosEnemy *c, AosEnemy *parent, AosSoma *soma,
                              AosHitReport *report);
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
                ectoplasm_struck(enemy, soma, kind->anims);
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
    return report;
}

static void accel_move(AosEnemy *e, int32_t limit);

/* sub_0806AEAC: the generic death, a flame every 4 frames over 16 x 12
 * pixels, the blink, then the deletion. */
static void generic_death(AosEnemy *enemy, const AosEnemyKind *kind, uint32_t (*random)(void),
                          AosHitReport *report) {
    if ((enemy->timer & 3) == 0) spawn_flame(enemy, 0x100000, 0xC0000, random, report);
    enemy->hidden = enemy->timer <= 0x27 ? kind->blink[enemy->timer] & 1 : false;
    if ((int8_t)--enemy->timer <= 0) enemy->removed = true;
}

/* sub_08083860, Headhunter's main-body death. The native body waits at the
 * lower screen edge for its linked heads before step 2; those actors are not
 * reconstructed, so their completed signal is immediate after the landing. */
static AosHitReport headhunter_death(AosEnemy *enemy, const AosEnemyKind *kind, int cam_y) {
    AosHitReport report = {0};
    switch (enemy->step) {
    case 0:
        enemy->vx = enemy->ax = 0;
        enemy->vy = 0x10000;
        enemy->ay = 0x4000;
        enemy->step = 1;
        break;
    case 1:
        accel_move(enemy, 0x60000);
        if ((enemy->y >> 16) - cam_y > 0xBF) {
            enemy->vx = enemy->ax = enemy->vy = enemy->ay = 0;
            enemy->y = AOS_FIXED(cam_y + 0xC0);
            enemy->step = 2;
        }
        break;
    case 2:
        play_loop(enemy, kind->anims, 0x13, false);
        enemy->timer = 0x5A;
        enemy->step = 3;
        /* fall through */
    case 3:
        if ((enemy->timer & 7u) == 0) aos_audio_play_song(0x76);
        if (enemy->timer) {
            enemy->timer--;
        } else {
            enemy->removed = true;
            enemy->step = 4;
            report.special_soul = true;
        }
        break;
    default:
        break;
    }
    return report;
}

/* EnemyHeadhunterCreate's three sub_08083FD0 actors. Native construction
 * places them at screen (240,124), (272,128) and (256,96); their full
 * eight-stage controllers and dedicated OAM resources are a later slice. */
static AosEnemy headhunter_head(AosEnemy *body, unsigned index, int cam_x, int cam_y) {
    static const int16_t screen_x[3] = {240, 272, 256};
    static const int16_t screen_y[3] = {124, 128, 96};
    AosEnemy head = {.id = body->id,
                     .x = AOS_FIXED(cam_x + screen_x[index]),
                     .y = AOS_FIXED(cam_y + screen_y[index]),
                     .role = AOS_ROLE_HEADHUNTER_HEAD,
                     .variant = (uint8_t)index,
                     .parent = body,
                     .static_frame = -1,
                     .combat = {.type = AOS_TYPE_ENEMY, .attack_off = true,
                                .hurt_off = true}};
    head.mirrored = body->mirrored;
    return head;
}

static AosHitReport headhunter_update(AosEnemy *enemy, AosSoma *soma,
                                      const AosEnemyKind *kind,
                                      const AosWeaponEntity *weapon,
                                      const AosWeaponFrames *weapon_frames,
                                      int soma_atk, int soma_def, int cam_x, int cam_y) {
    AosHitReport report = {0};
    if (!enemy->boss_parts_spawned) {
        for (unsigned i = 0; i < 3; ++i)
            report.children[report.child_count++] =
                headhunter_head(enemy, i, cam_x, cam_y);
        enemy->spawned_count = 3;
        enemy->boss_parts_spawned = true;
    }
    /* sub_08081EBC cases 0..2: begin the first head phase, run the
     * 100-frame appearance fade while sequence 24 completes, return to
     * sequence 0, then hold it for 29 frames. */
    switch (enemy->step) {
    case 0:
        enemy->hh_phase = 1;
        enemy->timer = 100;
        enemy->substep = 0;
        play(enemy, kind->anims, 0x18);
        enemy->step = 1;
        break;
    case 1:
        if (enemy->timer) enemy->timer--;
        if (enemy->cycle_done) {
            enemy->cycle_done = false;
            play(enemy, kind->anims, 0);
            enemy->timer = 0;
            enemy->step = 2;
        }
        break;
    case 2:
        if (enemy->timer++ > 27) {
            enemy->step = 3;
            enemy->timer = 60;
        }
        break;
    default:
        break;
    }
    step_anim(enemy, kind->anims);
    AosHitReport hit = collide(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    skeleton_merge(&report, &hit);
    aos_combat_tick(&enemy->combat);
    return report;
}

static void headhunter_head_update(AosEnemy *head, const AosEnemyKind *kind,
                                   uint32_t (*random)(void)) {
    AosEnemy *body = head->parent;
    if (!body || body->removed || body->id != AOS_BOSS_HEADHUNTER) {
        head->removed = true;
        return;
    }
    head->mirrored = body->mirrored;
    /* sub_08083FD0 cases 0-4: wait for this head's phase, make two
     * accelerating arcs above the body, then approach its phase-specific
     * attack perch at one pixel per frame. */
    if (head->step == 0 && body->hh_phase == (uint8_t)(head->variant + 1u)) {
        head->step = 1;
        head->substep = 0;
        head->timer = 0;
        return;
    }
    switch (head->step) {
    case 1:
        head->step = 2;
        /* fall through */
    case 2: {
        unsigned n = 2u - head->substep;
        uint32_t maximum = n << 19, minimum = n << 18;
        uint32_t offset;
        do offset = random() % maximum; while (offset < minimum);
        head->target_x = body->x + (int32_t)(offset - minimum);
        head->target_y = body->y - (int32_t)((n * 5u << 18) + 0x280000u);
        head->step = 3;
        break;
    }
    case 3: {
        (void)accelerate_toward(head, head->target_x, head->target_y, 0x2000);
        head->vy = head->ay * 8;
        head->ay = 0;
        int32_t old_vx = head->vx;
        accel_move(head, 0x40000);
        if (((uint32_t)(old_vx ^ head->vx) >> 31) != 0) {
            if (++head->substep <= 1) head->step = 2;
            else {
                head->step = 4;
                head->substep = 0;
            }
        }
        break;
    }
    case 4: {
        static const int32_t mirrored_x[3] = {0x200000, 0x480000, 0x400000};
        static const int32_t normal_x[3] = {-0x20000, -0x120000, -0x100000};
        static const int32_t y_offset[3] = {-0x340000, -0x2A0000, -0x280000};
        unsigned variant = head->variant < 3 ? head->variant : 0;
        if (head->substep == 0) {
            head->substep = 1;
            head->target_x = body->x +
                             (body->mirrored ? mirrored_x[variant] : normal_x[variant]);
            head->target_y = body->y + y_offset[variant];
        }
        if (move_toward(head, head->target_x, head->target_y, 0x10000)) {
            head->step = 5;
            head->timer = 0;
        } else {
            accel_move(head, 0x40000);
        }
        break;
    }
    case 5:
        head->timer++;
        /* sub_08083C1C emits four phase-specific attack actors at timers
         * 2, 5, 20 and 28. Their dedicated resources are not ported yet. */
        if (head->timer == 90) head->step = 6;
        if (head->timer > 59) {
            head->y += 0x4000;
            if (head->variant)
                head->x += head->mirrored ? -0x2000 : 0x2000;
        }
        break;
    case 6:
        aos_audio_play_song(0x11D);
        if (head->variant == 0) {
            body->step++;
            play(body, kind->anims, 1);
            step_anim(body, kind->anims);
            head->removed = true;
        } else {
            /* Native cases 1 and 2 emit ten shared visual particles here. */
            head->timer = 5;
            head->step = 7;
        }
        break;
    case 7:
        if (head->variant == 1 && head->timer && --head->timer == 0) {
            play(body, kind->anims, 5);
            step_anim(body, kind->anims);
            body->step++;
            body->y -= 0x130000;
            head->removed = true;
        } else if (head->variant == 2 && head->timer && --head->timer == 0) {
            play(body, kind->anims, 9);
            step_anim(body, kind->anims);
            body->step++;
            body->y += 0xB0000;
            head->removed = true;
        }
        break;
    default:
        break;
    }
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
    if (report.killed) {
        /* sub_0807B0DC: sound 0x70, then the gore. */
        aos_audio_play_song(0x70);
        zombie_gore(z, random, &spawned);
    }
    aos_combat_tick(&z->combat);
    report.child_count = spawned.child_count;
    memcpy(report.children, spawned.children, sizeof spawned.children[0] * spawned.child_count);
    /* Keep sub_0806E218's contact art. Zombie's walking particles and
     * death flames are additional effects; an empty secondary report must
     * not erase the native splash from a surviving hit. */
    append_effects(&report, &spawned);
    return report;
}

/* sub_0806BF78: the player within `range` of the entity on both axes. */
static bool player_within(const AosEnemy *e, const AosSoma *soma, uint32_t range) {
    int32_t dx = e->x - target_x(soma), dy = e->y - target_y(soma);
    return (uint32_t)(dx < 0 ? -dx : dx) <= range && (uint32_t)(dy < 0 ? -dy : dy) <= range;
}

static void integrate(AosEnemy *e);
static void stop(AosEnemy *e);

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

/* + 0x18 bits of Nemesis. */
#define NEM_TARGET_LEFT 0x02    /* its target lies left of it */
#define NEM_SOMA_LEFT 0x08      /* Soma lies left of it (state 6) */
#define NEM_NO_COLLIDE 0x20     /* dead: no more collision passes */

/* Sets the linked sprite's state; a new state restarts its steps. */
static void nemesis_helper_set(AosEnemy *n, uint8_t state) {
    if (n->nem_helper.state == state) return;
    n->nem_helper.state = state;
    n->nem_helper.step = 0;
}

static void helper_face(AosNemesisHelper *h, const AosSoma *soma) {
    /* With no target the linked sprite aims at itself. */
    if (no_target) return;
    if (target_x(soma) != h->x) h->mirrored = h->x < target_x(soma);
}

/* sub_080C7784, run after the body as its slot follows the body's: 0
 * hidden; 1 frame 1 on the body; 2 frame 2 nine pixels on its facing side
 * and 37 below, facing away from the target, for ten frames; 3 frame 3 ten
 * pixels behind and 4 above, facing Soma, for 40 frames; 4 a 31-frame
 * flicker (hidden unless + 0x0D % 3 = 0), then 0; 5 idle. */
static void nemesis_helper_step(AosEnemy *n, const AosSoma *soma) {
    AosNemesisHelper *h = &n->nem_helper;
    switch (h->state) {
    case 0: h->hidden = true; break;
    case 1:
        h->hidden = false;
        h->frame = 1;
        h->x = n->x;
        h->y = n->y;
        helper_face(h, soma);
        break;
    case 2:
        if (h->step == 0) {
            h->x = n->x;
            helper_face(h, soma);
            h->mirrored = !h->mirrored;
            h->hidden = false;
            h->frame = 2;
            h->timer = 0;
            h->step = 1;
        }
        if (h->step == 1) {
            h->x = n->x + (h->mirrored ? -AOS_FIXED(9) : AOS_FIXED(9));
            h->y = n->y + 0x250000;
            if (h->timer++ > 9) nemesis_helper_set(n, 4);
        }
        break;
    case 3:
        if (h->step == 0) {
            h->hidden = false;
            h->frame = 3;
            h->timer = 0;
            h->step = 1;
            h->mirrored = !(soma->x < n->x);
        }
        if (h->step == 1) {
            h->x = n->x + (h->mirrored ? AOS_FIXED(10) : -AOS_FIXED(10));
            h->y = n->y - AOS_FIXED(4);
            if (h->timer++ > 0x27) nemesis_helper_set(n, 4);
        }
        break;
    case 4:
        if (h->step == 0) {
            h->timer = 0;
            h->step = 1;
        }
        if (h->step == 1) {
            h->hidden = h->timer % 3 != 0;
            if (h->timer++ > 0x1E) {
                nemesis_helper_set(n, 0);
                h->timer = 0;
            }
        }
        break;
    default: break;
    }
}

/* The child entity of sub_080C76D4 (translucent, + 0x5C + 2) shows the
 * linked sprite its body runs. */
static void nemesis_helper_update(AosEnemy *h) {
    const AosEnemy *parent = h->parent;
    if (!parent || parent->removed) {
        h->removed = true;
        return;
    }
    const AosNemesisHelper *s = &parent->nem_helper;
    h->state = s->state;
    h->x = s->x;
    h->y = s->y;
    h->mirrored = s->mirrored;
    h->hidden = s->hidden;
    h->static_frame = s->frame;
    h->translucent = true;
}

/* The three visual slots made by sub_080C7B54 (sequence indices 9, 12 and
 * 6). sub_08045748 copies the owning Nemesis position, mirror, visibility
 * and current sprite record into each translucent child; their collision
 * callbacks are disabled. sub_080C7AB8 deletes them when the body starts
 * dying (state 0xFF). Keeping the sequence index in `variant` preserves the
 * native identity while the child scratch history selects a delayed direct
 * frame. */
static void nemesis_effect_update(AosEnemy *effect, const AosEnemyKind *kind) {
    const AosEnemy *parent = effect->parent;
    if (!parent || parent->removed || parent->state == 0xFF) {
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

/* sub_080C68AC (state 0): it waits hidden until the target is within 80
 * pixels, flickers in translucent with its linked sprite for 41 frames,
 * waits 21, flickers the linked sprite out for 41 and after 21 more starts
 * its approach. Step 0 runs once at creation. */
static void nemesis_appear(AosEnemy *n, const AosSoma *soma) {
    AosNemesisHelper *h = &n->nem_helper;
    switch (n->step) {
    case 0:
        n->angle = 0x1000;
        n->hidden = h->hidden = true;
        h->state = 5;
        n->timer = 0;
        n->step = 1;
        /* fallthrough */
    case 1:
        if (!player_within(n, soma, 0x500000)) return;
        h->x = n->x + (h->mirrored ? AOS_FIXED(10) : AOS_FIXED(13));
        h->y = n->y + AOS_FIXED(16);
        n->step = 2;
        return;
    case 2:
        n->hidden = h->hidden = n->timer % 3 != 0;
        n->translucent = true;
        if (n->timer++ <= 0x28) return;
        n->hidden = h->hidden = false;
        n->translucent = false;
        break;
    case 3:
        if (n->timer++ <= 0x14) return;
        break;
    case 4:
        h->hidden = n->timer % 3 != 0;
        if (n->timer++ <= 0x28) return;
        h->hidden = true;
        nemesis_helper_set(n, 0);
        break;
    case 5:
        if (n->timer++ <= 0x14) return;
        n->timer = 0;
        n->state = 1;
        n->step = n->substep = 0;
        return;
    default: return;
    }
    n->timer = 0;
    n->step++;
}

/* sub_080C6AA8 (state 1): opaque, the attack box (-8, -8, 16, 16), it tilts
 * to 0x1000 (0xF000 with the target to its right) by 0x180 a frame, then
 * drifts half a pixel a frame on each axis until it is within 72 pixels
 * horizontally and 16 of 48 pixels above the target (state 2), bobbing
 * its tilt by sin(+ 0x1A) / 32; crossing the target restarts the tilt. */
static void nemesis_approach(AosEnemy *n, const AosSoma *soma, uint32_t (*random)(void)) {
    if (player_dx(n, soma) > 0) n->nem_flags |= NEM_TARGET_LEFT;
    else n->nem_flags &= (uint16_t)~NEM_TARGET_LEFT;
    switch (n->step) {
    case 0:
        n->translucent = false;
        n->own_attack = (AosBox){-8, -8, 16, 16};
        n->vx = n->vy = 0x8000;
        if (player_dx(n, soma) > 0) n->vx = -n->vx;
        if (player_dy(n, soma) > 0) n->vy = -n->vy;
        n->phase = 0;
        n->nem_spin = -100;
        n->step = 2;
        return;
    case 1:
    case 2:
        if (turn_angle(&n->angle, 0x180, player_dx(n, soma) > 0 ? 0x1000 : 0xF000)) n->step++;
        return;
    case 3: {
        bool near_x = false, near_y = false;
        n->vx = n->vy = 0x8000;
        if (player_dx(n, soma) > 0) n->vx = -abs(n->vx);
        if (player_dy(n, soma) + 0x20 > 0) n->vy = -abs(n->vy);
        if (abs(player_dx(n, soma)) > 0x48) n->x += n->vx;
        else near_x = true;
        if (abs(player_dy(n, soma) + 0x30) > 0x10) n->y += n->vy;
        else near_y = true;
        if (near_x && near_y && n->state != 2) {
            n->state = 2;
            n->step = n->substep = 0;
        }
        int16_t dx = player_dx(n, soma);
        if ((dx >= 0 && n->vx > 0) || (dx <= 0 && n->vx < 0)) {
            n->step = 0;
            return;
        }
        int32_t s = aos_sine(n->phase);
        if (s < 0) s += 0x1F;
        n->angle = (uint16_t)((s >> 5) + (dx > 0 ? 0x1000 : -0x1000));
        n->phase = (uint16_t)(n->phase + 0x200);
        n->nem_wait = n->nem_wait ? (int16_t)(n->nem_wait - 1) : (int16_t)(random() % 0xB4 + 10);
        return;
    }
    default: return;
    }
}

/* 0x08528A34: four rows of nine attacks (0 dash, 1 swoop, 2 dive). */
static const uint8_t nemesis_attacks[4][9] = {
    {1, 0, 0, 2, 1, 1, 0, 2, 2},
    {0, 1, 2, 1, 0, 1, 2, 1, 0},
    {0, 1, 0, 1, 0, 2, 2, 2, 2},
    {2, 2, 1, 1, 0, 0, 1, 1, 2},
};

static void nemesis_next_attack(AosEnemy *n, uint32_t (*random)(void)) {
    if (n->nem_index == 9) {
        n->nem_row = (uint8_t)(random() & 3);
        n->nem_index = 0;
    }
}

static void nemesis_enter(AosEnemy *n, uint8_t state) {
    if (n->state == state) return;
    n->state = state;
    n->step = n->substep = 0;
}

/* sub_080C6D0C (state 2): the next attack of its row; a dive (2) starts at
 * once, otherwise it slides two pixels a frame away from the target while
 * tilting to 0xD800 (0x2800) by |8 * + 0x1E|, the
 * linked sprite shows frame 2 and, once it is gone, it tilts to 0x9800
 * (0x6800) then 0x2800 (0xD800) by |16 * + 0x1E| and starts the attack. */
static void nemesis_choose(AosEnemy *n, uint32_t (*random)(void)) {
    bool left = n->nem_flags & NEM_TARGET_LEFT;
    if (n->step == 0) {
        nemesis_next_attack(n, random);
        if (nemesis_attacks[n->nem_row][n->nem_index] == 2) {
            nemesis_enter(n, 5);
            n->nem_index++;
        }
    }
    uint16_t target;
    switch (n->step) {
    case 0:
        n->x += left ? 0x20000 : -0x20000;
        if (turn_angle(&n->angle, (uint16_t)abs(n->nem_spin * 8), left ? 0xD800 : 0x2800)) {
            nemesis_helper_set(n, 2);
            n->step++;
        }
        return;
    case 1:
        if (n->nem_helper.state == 0) n->step++;
        return;
    case 2:
    case 3:
        if (n->step == 2) target = left ? 0x9800 : 0x6800;
        else target = left ? 0x2800 : 0xD800;
        if (turn_angle(&n->angle, (uint16_t)abs(n->nem_spin * 16), target)) n->step++;
        return;
    case 4: {
        if (n->nem_helper.state != 0) return;
        nemesis_next_attack(n, random);
        uint8_t attack = nemesis_attacks[n->nem_row][n->nem_index];
        nemesis_enter(n, (uint8_t)(attack + 3));
        n->nem_index++;
        return;
    }
    default: return;
    }
}

/* sub_080C6EF8: both boxes 16 x 8 at the tip its angle points to. */
static void nemesis_tip_box(AosEnemy *n) {
    int16_t a = (int16_t)n->angle;
    int8_t x = (int8_t)(fixed_mul(aos_sine((uint32_t)(a + 0x8000)), 0x100000) >> 16);
    int8_t y = (int8_t)(fixed_mul(aos_sine((uint32_t)(a + 0x4000)), 0x80000) >> 16);
    if (x > 0) x = (int8_t)(x - 16);
    n->own_attack = n->own_hurt = (AosBox){x, y, 16, 8};
}

/* sub_0806D2F8(e, + 0x1C + 0x4000, 1.0): the velocity along its body. */
static void nemesis_heading(AosEnemy *n) {
    uint32_t a = (uint32_t)((int16_t)n->angle + 0x4000);
    n->vx = aos_sine(a + 0x4000);
    n->vy = aos_sine(a);
}

static void nemesis_rest(AosEnemy *n) {
    if (n->timer++ <= 0x28) return;
    n->timer = 0;
    nemesis_enter(n, 1);
    n->nem_passes = 0;
}

/* sub_080C6F64 (state 3): three dashes at three times its heading for 15
 * frames (sound 0x85 at the end), the first two followed by an 8-frame
 * recoil at six times the reverse heading turning by 0x320, then -400 a
 * frame (0x081194D0); 41 frames of rest, then state 1. */
static void nemesis_dash(AosEnemy *n) {
    static const int16_t spins[3] = {0x320, -400, 0};
    nemesis_tip_box(n);
    switch (n->step) {
    case 0:
        n->nem_passes = 3;
        n->step = 1;
        /* fallthrough */
    case 1:
        n->nem_spin = spins[3 - n->nem_passes];
        if (!(n->nem_flags & NEM_TARGET_LEFT)) n->nem_spin = (int16_t)-n->nem_spin;
        nemesis_heading(n);
        n->timer = 0;
        n->step = 2;
        /* fallthrough */
    case 2:
        if (n->timer++ > 0xE) {
            n->timer = 0;
            n->step = 3;
            aos_audio_play_song(0x85);
            return;
        }
        n->x += n->vx * 3;
        n->y += n->vy * 3;
        return;
    case 3:
        if (n->nem_passes == 1) {
            n->step = 4;
            n->timer = 0;
            return;
        }
        if (n->timer++ > 7) {
            n->timer = 0;
            n->step = --n->nem_passes == 0 ? 4 : 1;
            return;
        }
        n->angle = (uint16_t)(n->angle + n->nem_spin);
        nemesis_heading(n);
        n->x -= n->vx * 6;
        n->y -= n->vy * 6;
        return;
    case 4: nemesis_rest(n); return;
    default: return;
    }
}

/* 0x085289FC: the swoop's legs (x, y pixels, angle turned, frames), x
 * mirrored toward the target, ended by 0x7FFF. */
static const int16_t nemesis_swoop[][4] = {
    {48, 8, 0, 24},  {8, -8, 0x3400, 8},  {8, 8, -0x3400, 8},     {-24, -4, 0x3000, 8},
    {32, 0, 0, 8},   {-24, 4, -0x3000, 8}, {0x7FFF, 0, 0, 0},
};

/* sub_080C70F0 (state 4): each leg moves and turns it evenly over its
 * frames, sound 0x85 at each new leg; 41 frames of rest, then state 1. */
static void nemesis_swoop_update(AosEnemy *n) {
    nemesis_tip_box(n);
    switch (n->step) {
    case 0:
        n->nem_passes = 0;
        aos_audio_play_song(0x85);
        n->step = 1;
        /* fallthrough */
    case 1: {
        const int16_t *leg = nemesis_swoop[n->nem_passes];
        bool left = n->nem_flags & NEM_TARGET_LEFT;
        uint8_t frames = (uint8_t)leg[3];
        n->timer = frames;
        n->vx = (int32_t)(left ? -leg[0] : leg[0]) * 0x10000 / frames;
        n->vy = (int32_t)leg[1] * 0x10000 / frames;
        n->nem_spin = (int16_t)(leg[2] / frames);
        if (!left) n->nem_spin = (int16_t)-n->nem_spin;
        n->step = 2;
    }
        /* fallthrough */
    case 2:
        n->x += n->vx;
        n->y += n->vy;
        n->angle = (uint16_t)(n->angle + n->nem_spin);
        if (--n->timer) return;
        n->nem_passes++;
        n->step = nemesis_swoop[n->nem_passes][0] == 0x7FFF ? 5 : 1;
        n->timer = 0;
        aos_audio_play_song(0x85);
        return;
    case 5: nemesis_rest(n); return;
    default: return;
    }
}

/* sub_080C722C (state 5): it spins (0x1000 a frame, sound 0x85 every 16
 * frames) in place for 65 frames, then points at the target and flies at
 * it at two pixels a frame until it reaches its x, then state 1. */
static void nemesis_dive(AosEnemy *n, const AosSoma *soma) {
    nemesis_tip_box(n);
    switch (n->step) {
    case 0:
        n->vx = n->vy = n->ax = n->ay = 0;
        n->nem_spin = 0x1000;
        n->timer = 0;
        n->step = 1;
        /* fallthrough */
    case 1: {
        if ((n->timer & 0xF) == 0) aos_audio_play_song(0x85);
        if (n->timer++ <= 0x40) break;
        n->nem_spin = 0;
        n->angle = (uint16_t)(angle_to_target(n, soma) + 0xC000);
        int16_t a = (int16_t)n->angle;
        n->vx = fixed_mul(aos_sine((uint32_t)(a + 0x8000)), 0x20000);
        n->vy = fixed_mul(aos_sine((uint32_t)(a + 0x4000)), 0x20000);
        int32_t frames = n->vx ? (target_x(soma) - n->x) / n->vx : 0;
        n->timer = (uint8_t)(frames < 0 ? -frames : frames);
        n->step = 2;
        aos_audio_play_song(0x85);
        break;
    }
    case 2:
        if (--n->timer == 0) n->step = 3;
        break;
    default: nemesis_enter(n, 1); break;
    }
    n->x += n->vx;
    n->y += n->vy;
    n->vx += n->ax;
    n->vy += n->ay;
    n->angle = (uint16_t)(n->angle + n->nem_spin);
}

/* sub_080C73F8 (state 6): knocked away from Soma along angle 0xD000 (two
 * pixels a frame across, 1.5 up, falling by 0xF00 a frame) while the
 * linked sprite shows frame 3; once it is gone, state 1, or 0xFF dead. */
static void nemesis_recoil(AosEnemy *n, const AosSoma *soma) {
    switch (n->step) {
    case 0:
        n->timer = 0;
        n->phase = 0;
        if (soma->x < n->x) n->nem_flags |= NEM_SOMA_LEFT;
        else n->nem_flags &= (uint16_t)~NEM_SOMA_LEFT;
        n->step = 1;
        /* fallthrough */
    case 1: {
        int32_t speed = (n->nem_flags & NEM_SOMA_LEFT) ? 0x20000 : -0x20000;
        n->x += (speed >> 8) * (aos_sine(0xD000 + 0x4000) >> 8);
        n->y += (aos_sine(0xD000) >> 8) * (0x18000 >> 8) + (int16_t)n->phase * 0xF00;
        n->phase = (uint16_t)(n->phase + 1);
        if (n->nem_helper.state == 4) n->step++;
        break;
    }
    case 2:
        if (n->nem_helper.state != 0) break;
        if (n->hp <= 0) {
            if (n->state != 0xFF) {
                n->state = 0xFF;
                n->step = n->substep = 0;
            }
        } else {
            nemesis_enter(n, 1);
        }
        break;
    default: break;
    }
    if (n->hp <= 0) n->nem_flags |= NEM_NO_COLLIDE;
}

/* sub_080C750C (state 0xFF): sound 0x72, the trails go, it falls from 0.5
 * by 0x800 a frame, blinking, a flame of a 16 x 16 area every seventh
 * frame, until below screen row 0xAA or after 65 frames. */
static void nemesis_die(AosEnemy *n, int cam_y, uint32_t (*random)(void), AosHitReport *report) {
    if (n->step == 0) {
        aos_audio_play_song(0x72);
        n->vy = 0x8000;
        n->ay = 0x800;
        n->timer = 0;
        n->step = 1;
    }
    if (n->step == 1) {
        n->y += n->vy;
        n->vy += n->ay;
        if ((int16_t)((n->y >> 16) - cam_y) > 0xAA || n->timer++ > 0x40) n->removed = true;
    }
    if (n->timer % 7 == 0) spawn_flame_at(n->x, n->y, 0x100000, 0x100000, random, report);
    n->hidden = n->timer & 1;
}

/* sub_080C7604 (its hit callback, after the damage and the 8-frame flash):
 * three blows in four only count down + 0x24; the fourth, or a lethal one
 * (sound 0x1C3), sends it into its recoil (state 6) with the linked sprite
 * in state 3. Any blow shows it. */
static void nemesis_struck(AosEnemy *n) {
    if (n->nem_hits && n->hp > 0) {
        n->nem_hits--;
    } else {
        if (n->hp <= 0) aos_audio_play_song(0x1C3);
        nemesis_helper_set(n, 3);
        n->state = 6;
        n->step = 0;
        n->nem_hits = 3;
    }
    n->hidden = false;
}

/* EnemyNemesisUpdate: state 7 is inert, 0xFF dies; otherwise it enters the
 * homing list on screen (sub_08021924); alive and flashing from a blow
 * (sub_0806AD24) it only collides, outside state 0; else states 0-6 and
 * the collision pass until dead. Its body is never mirrored; its affine
 * angle (+ 0x1C) turns it. The first update emits its linked sprite and
 * the three trails (native: at creation). */
static AosHitReport nemesis_update(AosEnemy *n, AosSoma *soma, const AosEnemyKind *kind,
                                   const AosWeaponEntity *weapon,
                                   const AosWeaponFrames *weapon_frames, int soma_atk,
                                   int soma_def, int cam_x, int cam_y,
                                   uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (n->state == 7) return report;
    if (!n->helper_spawned) {
        n->helper_spawned = true;
        static const uint8_t sequences[] = {9, 12, 6};
        AosEnemy helper = {0};
        helper.id = n->id;
        helper.role = AOS_ROLE_NEMESIS_HELPER;
        helper.static_frame = n->nem_helper.frame;
        helper.combat.attack_off = helper.combat.hurt_off = true;
        report.children[report.child_count++] = helper;
        for (unsigned i = 0; i < sizeof sequences; ++i) {
            AosEnemy effect = {0};
            effect.id = n->id;
            effect.role = AOS_ROLE_NEMESIS_EFFECT;
            effect.variant = effect.step = sequences[i];
            effect.static_frame = 0;
            effect.x = n->x;
            effect.y = n->y;
            effect.hidden = n->hidden;
            effect.translucent = true;
            effect.combat.attack_off = effect.combat.hurt_off = true;
            report.children[report.child_count++] = effect;
        }
        n->spawned_count = report.child_count;
    }
    if (n->state == 0xFF) {
        nemesis_die(n, cam_y, random, &report);
        nemesis_helper_step(n, soma);
        return report;
    }
    int sx = (n->x >> 16) - cam_x, sy = (n->y >> 16) - cam_y;
    if (!n->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
        report.targetable = true;
    bool pass = true;
    if (n->hp > 0 && n->hit_flash) {
        n->hit_flash--;
        pass = n->state != 0;
    } else {
        switch (n->state) {
        case 0: nemesis_appear(n, soma); break;
        case 1: nemesis_approach(n, soma, random); break;
        case 2: nemesis_choose(n, random); break;
        case 3: nemesis_dash(n); break;
        case 4: nemesis_swoop_update(n); break;
        case 5: nemesis_dive(n, soma); break;
        case 6: nemesis_recoil(n, soma); break;
        default: break;
        }
    }
    if (pass && !(n->nem_flags & NEM_NO_COLLIDE)) {
        AosHitReport hit = collide(n, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        skeleton_merge(&report, &hit);
    }
    aos_combat_tick(&n->combat);
    nemesis_helper_step(n, soma);
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
    int32_t ty = target_y(soma) - 0x2A0000;
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
            int32_t dx = c->x - target_x(soma), dy = c->y - target_y(soma);
            if ((dx < 0 ? -dx : dx) > 0x480000 && (dy < 0 ? -dy : dy) > 0x100000) c->substep = 3;
            break;
        }
        case 3: {
            if (c->anim.id != 1) play(c, anims, 1);
            /* The player's + 0x58 bit 0x40: behind him is to his right. */
            int32_t tx = target_x(soma) + (soma->facing_left ? 0x200000 : -0x200000);
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
    z->mirrored = (z->x >> 16) < (target_x(soma) >> 16);
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
            int32_t vx = (target_x(soma) - (z->x + offset)) / 0x2C;
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
    int32_t dx = (e->x - target_x(soma)) >> 16, dy = (e->y - target_y(soma)) >> 16;
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
    int32_t tx = target_x(soma) + (v->mirrored ? (int32_t)0xFFC00000 : 0x400000);
    if (head_toward(v, tx, target_y(soma) + (int32_t)0xFFD00000, 0x20000)) {
        v->state = 1;
        v->step = 0;
        return;
    }
    int32_t dx = target_x(soma) - v->x, dy = target_y(soma) - v->y;
    if ((dx < 0 ? -dx : dx) <= 0x3BFFFF && v->y < target_y(soma) - 0x200000 &&
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
        v->mirrored = !((v->x >> 16) < (target_x(soma) >> 16));
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
                         const AosEnemyKind *kind, int cam_x, int cam_y,
                         uint32_t (*random)(void), AosHitReport *report) {
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
    case 2: {
        /* Sound 0x7D every 25 frames within sub_08068AD4(e, -64, -64,
         * 0x170, 0x120). */
        int sx = (w->x >> 16) - cam_x, sy = (w->y >> 16) - cam_y;
        if (w->timer % 25 == 0 && (uint16_t)(sx + 0x40) <= 0x170 &&
            (uint16_t)(sy + 0x40) <= 0x120)
            aos_audio_play_song(0x7D);
        w->timer++;
        if (w->walk_timer <= 0x1FFFF) w->walk_timer += 0x4000;
        w->shot++;
        if (w->anim.frame == 1 && w->anim.tick == 1) w->substep++;
        if (w->shot > 0x64) {
            aos_audio_play_song(0x127);
            play_loop(w, kind->anims, 2, false);
            w->step = 3;
            w->timer = 0;
        }
        return;
    }
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
    if ((w->timer & 7) == 0) aos_audio_play_song(0x6E);
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
    case 0: warrior_walk(w, soma, layer, kind, cam_x, cam_y, random, &effects); break;
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
        f->mirrored = (f->x >> 16) < (target_x(soma) >> 16);
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
        if (k->timer == 0x0B) aos_audio_play_song(0x85);
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
        if (player_within(d, soma, 0x3C0000) && (target_x(soma) != d->x || target_y(soma) != d->y)) {
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
            int dx = (int16_t)((d->x >> 16) - (target_x(soma) >> 16));
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
            int16_t dx = (int16_t)((target_x(soma) >> 16) - (d->x >> 16));
            int16_t dy = (int16_t)((target_y(soma) >> 16) - (d->y >> 16) - 0x10);
            uint16_t angle = aos_arctan2(dx, dy);
            d->vx = fixed_mul(aos_sine(angle + 0x14000u), 0x10000);
            d->vy = fixed_mul(aos_sine(angle + 0x10000u), 0x10000);
            int32_t frames = d->vx ? (target_x(soma) - d->x) / d->vx : 0;
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
            int dx = (int16_t)((d->x >> 16) - (target_x(soma) >> 16));
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
    int dx = (int16_t)((d->x >> 16) - (target_x(soma) >> 16));
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
        if (p->x < target_x(soma)) p->vx = -abs(p->vx);
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
                    aos_audio_play_song(0x153);
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
    int dx = (int16_t)((d->x >> 16) - (target_x(soma) >> 16));
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
        int16_t ax = (int16_t)((target_x(soma) >> 16) - (d->x >> 16));
        int16_t ay = (int16_t)((target_y(soma) >> 16) - (d->y >> 16) - 0x10);
        uint16_t angle = aos_arctan2(ax, ay);
        d->vx = fixed_mul(aos_sine(angle + 0x14000u), 0x10000);
        d->vy = fixed_mul(aos_sine(angle + 0x10000u), 0x10000);
        int32_t frames = d->vx ? (target_x(soma) - d->x) / d->vx : 0;
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
        if (player_in_centred(d, soma, 0xF0, 0x80) && (target_x(soma) != d->x || target_y(soma) != d->y)) {
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
            aos_audio_hold(0x75);
            /* fall through */
        case 1:
            if (!d->cycle_done) break;
            if (d->anim.id != 4) play(d, anims, 4);
            d->substep = 2;
            break;
        case 2:
            /* The spark's loop is held again every 16 frames above 0x20. */
            if ((d->timer & 0xF) == 0 && d->timer > 0x20) aos_audio_hold(0x75);
            if (d->timer-- == 0) d->substep = 3;
            break;
        case 3:
            aos_audio_release(0x75);
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
    int dx = (int16_t)((d->x >> 16) - (target_x(soma) >> 16));
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
        aos_audio_release(0x75);
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
    bool right = e->x < target_x(soma);
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
                int dx = (int16_t)((e->x >> 16) - (target_x(soma) >> 16));
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
    int32_t tx = (int32_t)((uint32_t)(target_x(soma) >> 16) << 16);
    int32_t ty = (int32_t)((uint32_t)((target_y(soma) >> 16) - 0x50) << 16);
    tx += target_x(soma) > e->x ? -e->speed : e->speed;
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
            aos_audio_play_song(0x87);
            if (e->variant == 1) aos_audio_play_song(0x1BA);
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
        e->ax = target_x(soma) > e->x ? 0x3000 : -0x3000;
        if (target_y(soma) - 0x680000 > e->y) {
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
        aos_audio_play_song(0x1BC);
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
        if (k->timer == 0x0B) aos_audio_play_song(0x85);
        if (k->timer++ <= 0x1D) return;
        play_loop(k, anims, 4, false);
        k->step = 3;
        k->timer = 0;
        return;
    case 3:
        if (k->timer++ <= 0x13) return;
        break;
    case 5:
        if (k->timer == 0x0B) aos_audio_play_song(0x85);
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
        g->mirrored = (g->x >> 16) < (target_x(soma) >> 16);
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
        dx = target_x(soma) - g->x;
        dy = target_y(soma) - (g->y + 0x400000);
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
        aos_audio_play_song(0x87);
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
        dx = target_x(soma) - g->x;
        dy = target_y(soma) - (g->y + 0x400000);
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
    int side = e->x < target_x(soma) ? 1 : e->x > target_x(soma) ? -1 : 0;
    if (side != (e->mirrored ? 1 : -1)) return false;
    int32_t dx = pixel_delta(target_x(soma), e->x), dy = pixel_delta(target_y(soma), e->y);
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

/* The walker probes of the Rock Armor (0x0852872C) and of its rock
 * (0x08528738), read from the ROM: wall count, ceiling, floor, half width,
 * wall y offsets. */
static const AosProbes rock_armor_probes = {2, -40, -1, 6, {-7, -24}};
static const AosProbes rock_probes = {1, -8, 8, 6, {0}};

/* The walk speed of the Rock Armor for its walk frame (frame % 5 <= 3). */
static int32_t rock_armor_speed(const AosEnemy *a) {
    unsigned r = a->anim.frame % 5;
    return r > 3 ? 0 : 0x2000 + (int32_t)r * 0x4000;
}

/* sub_080B2978: walking with the probe walker (mode 0x14); with a patrol
 * length (parameter 0) it turns around once frame 9 has been shown more
 * than that many updates, otherwise whenever it is not simply on a floor.
 * At the start of frame 9, the player ahead within 99 pixels starts the
 * throw. */
static void rock_armor_walk(AosEnemy *a, const AosSoma *soma, const AosCollision *layer) {
    if (a->step != 0) return;
    if (a->anim.frame == 9 && a->param0) {
        uint8_t shown = a->timer++;
        if (shown > a->param0) {
            a->timer = 0;
            a->mirrored = !a->mirrored;
        }
    }
    int result = ground_walk(a, soma, layer, &rock_armor_probes, rock_armor_speed(a), 0x14);
    if (!a->param0 && (result & 0x1B) != 8) a->mirrored = !a->mirrored;
    if (a->anim.tick == 0 && a->anim.frame == 9 && player_ahead_within(a, soma, 0x63)) {
        a->state = 1;
        a->step = a->substep = 0;
    }
}

/* sub_080B2BD8: animation 1 once; on each update of its frame 7 a rock
 * (sub_080B2798, sound 0x88) with the armor's position, facing, hit points
 * and contact power; when it ends, the player still ahead within 99 pixels
 * throws again, otherwise it walks (animation 0). */
static void rock_armor_throw(AosEnemy *a, const AosSoma *soma, const AosEnemyKind *kind,
                             AosHitReport *report) {
    switch (a->step) {
    case 0:
        play_loop(a, kind->anims, 1, false);
        a->step = 1;
        break;
    case 1:
        if (a->anim.frame == 7) {
            aos_audio_play_song(0x88);
            if (report->child_count < AOS_ENEMY_MAX_CHILDREN) {
                AosEnemy rock = {0};
                rock.id = a->id;
                rock.role = AOS_ROLE_ROCK;
                rock.static_frame = -1;
                rock.x = a->x;
                rock.y = a->y;
                rock.mirrored = a->mirrored;
                rock.hp = a->hp;
                rock.stats = a->stats;
                rock.combat.type = 0x0A;
                rock.combat.attack_off = rock.combat.hurt_off = true;
                report->children[report->child_count++] = rock;
            }
        }
        if (a->anim.flags & AOS_ANIM_ENDED)
            a->step = player_ahead_within(a, soma, 0x63) ? 0 : 2;
        break;
    case 2:
        a->state = a->step = a->substep = 0;
        play(a, kind->anims, 0);
        break;
    default: break;
    }
}

/* sub_080B2CD8: death animation 2 (sound 0x6F) until it ends, then the
 * deletion. + 0x4C (from -40.0, + 0x8000 per frame) is the height of the
 * effects and + 0x48 (from 0, + 0x5000) their phase: every fourth frame a
 * burst (sub_08045CEC animation 15, row 7, vy 0.5, ay -0x2000) at x + 16 *
 * sin(phase + 0x9000) +/- 2.0, and every eighth (sound 0x72) a spark
 * (animation 16, row 6) at x +/- 8 and the height +/- 2 pixels. */
static void rock_armor_die(AosEnemy *a, const AosEnemyKind *kind, uint32_t (*random)(void),
                           AosHitReport *report) {
    if (a->step == 0) {
        play_loop(a, kind->anims, 2, false);
        a->step = 1;
        a->vy = (int32_t)0xFFD80000;
        a->vx = 0;
        aos_audio_play_song(0x6F);
        return;
    }
    a->timer++;
    int32_t height = a->y + a->vy;
    a->vy += 0x8000;
    int32_t phase = a->vx;
    a->vx += 0x5000;
    if ((a->timer & 3) == 0) {
        int32_t offset = aos_sine((uint32_t)(phase + 0x9000)) << 4;
        int32_t x = a->x + offset + random_centred(random, 0x40000);
        AosEffect *burst = spawn_drift(AOS_EFFECT_BURST_15, x, height, report);
        if (burst) {
            burst->vy = 0x8000;
            burst->ay = -0x2000;
        }
    }
    if ((a->timer & 7) == 0) {
        aos_audio_play_song(0x72);
        int32_t x = a->x + (int32_t)((uint32_t)random_centred(random, 16) << 16);
        int32_t y = height + (int32_t)((uint32_t)random_centred(random, 4) << 16);
        spawn_drift(AOS_EFFECT_SPARK, x, y, report);
    }
    if (a->anim.flags & AOS_ANIM_ENDED) a->removed = true;
}

/* EnemyRockArmorUpdate: inside the activity window of sub_0806CC20 (only
 * colliding while flashing from a blow), the state, the collision pass
 * (sub_080421AC, not while dying) and the animation step (sub_0806DF20);
 * state 3 (the global 0x8E & 0x40) only steps its animation. */
static AosHitReport rock_armor_update(AosEnemy *a, AosSoma *soma, const AosCollision *layer,
                                      const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                      const AosWeaponFrames *weapon_frames, int soma_atk,
                                      int soma_def, int cam_x, int cam_y,
                                      uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    int sx = (a->x >> 16) - cam_x, sy = (a->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
    if (a->hp > 0 && a->hit_flash) {
        a->hit_flash--;
        report = collide(a, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        aos_combat_tick(&a->combat);
        return report;
    }
    switch (a->state) {
    case 0: rock_armor_walk(a, soma, layer); break;
    case 1: rock_armor_throw(a, soma, kind, &spawned); break;
    case 2:
        rock_armor_die(a, kind, random, &spawned);
        if (a->removed) {
            take_effects(&report, &spawned);
            return report;
        }
        break;
    default:
        step_anim(a, kind->anims);
        return report;
    }
    if (a->state != 2)
        report = collide(a, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    step_anim(a, kind->anims);
    aos_combat_tick(&a->combat);
    report.child_count = spawned.child_count;
    memcpy(report.children, spawned.children, sizeof spawned.children[0] * spawned.child_count);
    take_effects(&report, &spawned);
    return report;
}

/* sub_080B2798: the collision pass first (its hit callback sub_080B2F5C
 * zeroes its hit points: it bursts). State 0: animation 3, both boxes
 * (-16, -16, 32, 32) (0x2020F0F0), 58 pixels higher, thrown up at 2.0 with
 * gravity 0x2000 and forward at 0.125 + its distance to the player (from
 * the armor's feet) / 64 a frame, contact power + 1 / 4; its contact
 * sub_080B2F94 knocks back. State 1: the probe walker (0x08528738, mode 2)
 * until it meets anything. State 2: sound 0x80, eight pieces within 8
 * pixels, 8 higher, facing at random, and it is gone. Beyond screen margin
 * 3 it is deleted. The global pause of gEwramData + 0x4BE is not ported. */
static AosHitReport rock_update(AosEnemy *r, AosSoma *soma, const AosCollision *layer,
                                const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
                                int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = collide(r, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    aos_combat_tick(&r->combat);
    if (r->state != 0 && r->hp == 0) r->state = 2;
    switch (r->state) {
    case 0: {
        play(r, kind->anims, 3);
        step_anim(r, kind->anims);
        r->combat.attack_off = r->combat.hurt_off = false;
        r->own_boxes = true;
        r->own_hurt = r->own_attack = (AosBox){-16, -16, 32, 32};
        int32_t dx = pixel_delta(target_x(soma), r->x), dy = pixel_delta(target_y(soma), r->y);
        int32_t distance = (int16_t)isqrt((uint32_t)(dx * dx + dy * dy));
        r->speed = -0x2000 - ((distance << 16) >> 6);
        r->y -= 0x3A0000;
        if (r->mirrored) r->speed = -r->speed;
        r->vy = -0x20000;
        r->ay = 0x2000;
        r->stats.contact = (uint8_t)(r->stats.contact + (r->stats.contact >> 2));
        r->state = 1;
    }
        /* fallthrough */
    case 1:
        if (ground_walk(r, soma, layer, &rock_probes, r->speed, 2)) r->state = 2;
        break;
    case 2:
        aos_audio_play_song(0x80);
        for (int i = 0; i < 8 && report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i) {
            AosEnemy piece = {0};
            piece.id = r->id;
            piece.role = AOS_ROLE_ROCK_PIECE;
            piece.static_frame = -1;
            piece.x = r->x + random_centred(random, 0x100000);
            piece.y = r->y + random_centred(random, 0x100000) - 0x80000;
            piece.mirrored = random() & 1;
            piece.combat.attack_off = piece.combat.hurt_off = true;
            report.children[report.child_count++] = piece;
        }
        r->removed = true;
        return report;
    default: break;
    }
    if (outside_margins(kind, 3, (r->x >> 16) - cam_x, (r->y >> 16) - cam_y)) r->removed = true;
    return report;
}

/* sub_080B264C: a piece shows animation 4, 5 or 6 (r & 3, at most 2),
 * flies at (+/- 0.5) - 0.25 away from its facing and up at 1.0 + (r &
 * 0x1FFF) / 0x1000 with gravity 0.25, bounces once at half speed off a
 * solid cell 4 pixels below it, and is gone outside the screen box (-16,
 * -16, 0x110, 0xC0). */
static void rock_piece_update(AosEnemy *p, const AosCollision *layer, const AosEnemyKind *kind,
                              int cam_x, int cam_y, uint32_t (*random)(void)) {
    switch (p->state) {
    case 0: {
        uint32_t size = random() & 3;
        play(p, kind->anims, 4 + (size > 2 ? 2 : size));
        step_anim(p, kind->anims);
        p->vx = random_centred(random, 0x10000) - 0x4000;
        if (p->mirrored) p->vx = -p->vx;
        p->vy = -0x10000 - (int32_t)((random() & 0x1FFF) << 4);
        p->state = 1;
        break;
    }
    case 1:
        if (aos_collision_cell(layer, p->x >> 16, (p->y >> 16) + 4) & 1) {
            p->vy = -(p->vy >> 1);
            p->state = 2;
            break;
        }
        /* fallthrough */
    case 2:
        p->x += p->vx;
        p->y += p->vy;
        p->vy += 0x4000;
        break;
    default: break;
    }
    int sx = (p->x >> 16) - cam_x, sy = (p->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x10) > 0x110 || (uint16_t)(sy + 0x10) > 0xC0) p->removed = true;
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
    int32_t dx = pixel_delta(target_x(soma), a->x), dy = pixel_delta(target_y(soma), a->y);
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
        arrow->x = target_x(soma) + arrow->ax;
        arrow->y = target_y(soma) + arrow->ay;
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
    int px = (target_x(soma) >> 16) - cam_x;
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

/* Skeleton's thrown bone, sub_080C3BD8. The native attack frame 2
 * creates a projectile with its own OAM frame, sound, 8x8 hurt box and a
 * 2x2 contact box. Its loot and experience must never be awarded: only
 * AOS_ROLE_ENEMY is eligible in aria_runtime.c. */
/* 0x0852892C: per body animation, the sub_0806C3F8 component (always the
 * last) and the offset a head or bone in hand adds to it (x mirrored). */
static const int8_t skeleton_hand[7][2] = {
    {-6, 16}, {-6, 16}, {11, -8}, {11, -8}, {11, -8}, {0, 0}, {-6, 16},
};

/* sub_0806C3F8(body, 0): the far corner of the body's current frame's last
 * OAM component, mirrored with its facing, as an integer offset. */
static void skeleton_corner(const AosEnemy *body, const AosEnemyKind *kind, int *dx, int *dy) {
    int anim = body->anim.id < AOS_ENEMY_MAX_ANIMS ? body->anim.id : 0;
    int frame = body->anim.frame < AOS_ENEMY_MAX_FRAMES ? body->anim.frame : 0;
    int ex = kind->edges[anim][frame][0], ey = kind->edges[anim][frame][1];
    *dx = body->mirrored ? -ex : ex;
    *dy = body->vflip ? -ey : ey;
}

/* A child at the body's position plus `dx`, `dy` (whole pixels). */
static void skeleton_place(AosEnemy *child, const AosEnemy *body, int dx, int dy) {
    child->x = (int32_t)((uint32_t)(uint16_t)((body->x >> 16) + dx) << 16) | (child->x & 0xFFFF);
    child->y = (int32_t)((uint32_t)(uint16_t)((body->y >> 16) + dy) << 16) | (child->y & 0xFFFF);
}

/* sub_080C46AC: a child sharing the body's + 0x3C and HP; the body is
 * hidden until sub_0806CC20 shows it again. */
static void skeleton_child(AosEnemy *body, uint8_t role, AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy c = {0};
    c.id = body->id;
    c.role = role;
    c.static_frame = -1;
    c.x = body->x;
    c.y = body->y;
    c.mirrored = body->mirrored;
    c.stats = body->stats;
    c.hp = body->hp;
    c.combat.attack_off = c.combat.hurt_off = true;
    report->children[report->child_count++] = c;
    body->hidden = true;
}

/* sub_080C4148: the head on the body's neck (state 1, at the corner of its
 * last component) or in its hand (state 2, plus the 0x0852892C offset),
 * animation 0x10, or 7 while the body laughs (+ 0x1A bit 5). It goes with
 * the body or once neither head bit is set. */
static void skeleton_head_update(AosEnemy *h, const AosEnemyKind *kind) {
    const AosEnemy *body = h->parent;
    if (!body || body->removed || !(body->skel_flags & 3)) {
        h->removed = true;
        return;
    }
    h->hidden = body->hidden;
    if (h->state == 0) {
        play_loop(h, kind->anims, 0x10, true);
        step_anim(h, kind->anims);
        h->state = 1;
        h->hidden = false;
    }
    int dx, dy;
    skeleton_corner(body, kind, &dx, &dy);
    h->mirrored = body->mirrored;
    if (h->state == 1) {
        skeleton_place(h, body, dx, dy);
        if (!(body->skel_flags & 1)) h->state = 2;
    } else {
        int anim = body->anim.id < 7 ? body->anim.id : 0;
        dx += body->mirrored ? -skeleton_hand[anim][0] : skeleton_hand[anim][0];
        dy += skeleton_hand[anim][1];
        skeleton_place(h, body, dx, dy);
    }
    unsigned anim = (body->skel_flags & 0x20) ? 7 : 0x10;
    if (h->anim.id != anim) play_loop(h, kind->anims, anim, true);
    step_anim(h, kind->anims);
}

/* sub_080C43C4: the bone in the body's hand (animation 8) while + 0x1A
 * bit 2 is set: shown at frame 3 of the body's animation 6 (the pick-up),
 * gone if the body hides while it shows. */
static void skeleton_held_update(AosEnemy *b, const AosEnemyKind *kind) {
    const AosEnemy *body = b->parent;
    if (!body || body->removed || !(body->skel_flags & 4)) {
        b->removed = true;
        return;
    }
    if (!b->hidden && body->hidden) b->hidden = true;
    if (b->state == 0) {
        play_loop(b, kind->anims, 8, true);
        b->state = 1;
        step_anim(b, kind->anims);
        return;
    }
    b->state = 2;
    if (body->anim.id == 6 && body->anim.frame == 3) b->hidden = false;
    if (!b->hidden && body->hidden) {
        b->removed = true;
        return;
    }
    int dx, dy;
    skeleton_corner(body, kind, &dx, &dy);
    int anim = body->anim.id < 7 ? body->anim.id : 0;
    b->mirrored = body->mirrored;
    dx += body->mirrored ? -skeleton_hand[anim][0] : skeleton_hand[anim][0];
    dy += skeleton_hand[anim][1];
    skeleton_place(b, body, dx, dy);
    step_anim(b, kind->anims);
}

/* sub_080C3BD8 states 0-1: the projectile 30 pixels above the body's feet,
 * a bone (frame 0x12, sound 0x88) or its head (+ 0x18, frame 0x1B, sound
 * 0x89), vx +/- 0x8AAA, vy -3.0, turning with the body's affine slot; its
 * boxes are 0x1010F8F8 (hurt) and 0x0202FFFF (attack), type 0xA, one HP. */
static void skeleton_throw(AosEnemy *body, bool head, AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy p = {0};
    p.id = body->id;
    p.role = AOS_ROLE_SKELETON_BONE;
    p.variant = head;
    p.static_frame = head ? 0x1B : 0x12;
    p.mirrored = body->mirrored;
    p.x = (int32_t)((uint32_t)(uint16_t)(body->x >> 16) << 16);
    p.y = (int32_t)((uint32_t)(uint16_t)((body->y >> 16) - 0x1E) << 16);
    p.vx = body->mirrored ? 0x8AAA : (int32_t)0xFFFF7556;
    p.vy = (int32_t)0xFFFD0000;
    p.hp = 1;
    p.state = 2;
    p.spinning = true;
    p.angle = body->skel_spin;
    p.stats = body->stats;
    p.combat.type = AOS_TYPE_ENEMY;
    p.own_boxes = true;
    p.own_hurt = (AosBox){-8, -8, 16, 16};
    p.own_attack = (AosBox){-1, -1, 2, 2};
    aos_audio_play_song(head ? 0x89 : 0x88);
    report->children[report->child_count++] = p;
    body->spawned_count++;
}

/* sub_080C3BD8 states 2-4: it falls (vy += 0x1800) turning with the body
 * and collides; struck (sub_080C3B2C, the weapon) a bone drops (vy 0.25,
 * sound 0x151) and a head flies back twelve times faster (vy negated,
 * 0x15D), both without collisions (state 3). Below screen row 0xAA it is
 * gone (state 4) and the body counts it back. */
static AosHitReport skeleton_bone_update(AosEnemy *p, AosSoma *soma, const AosEnemyKind *kind,
                                         const AosWeaponEntity *weapon,
                                         const AosWeaponFrames *weapon_frames, int soma_atk,
                                         int soma_def, int cam_x, int cam_y) {
    AosHitReport report = {0};
    AosEnemy *body = (AosEnemy *)p->parent;
    if (p->state == 4) {
        if (body && body->spawned_count) body->spawned_count--;
        p->removed = true;
        return report;
    }
    if (p->hit_flash) p->hit_flash--;
    if (body && p->spinning) p->angle = body->skel_spin;
    p->x += p->vx;
    p->y += p->vy;
    p->vy += 0x1800;
    if (p->state != 3) {
        report = collide(p, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        if (report.enemy_hit) {
            /* sub_080C3B2C */
            p->hp = 0;
            p->spinning = false;
            p->hit_flash = 8;
            if (!p->variant) {
                p->timer = 0x32;
                p->vx = p->ax = 0;
                p->vy = 0x4000;
                p->ay = 0x2000;
                aos_audio_play_song(0x151);
            } else {
                int32_t speed = abs(p->vx) * 12;
                p->vx = p->x < target_x(soma) ? -speed : speed;
                p->vy = -p->vy;
                aos_audio_play_song(0x15D);
            }
            p->state = 3;
            report.enemy_hit = false;
            report.killed = false;
        }
        aos_combat_tick(&p->combat);
    }
    if ((int16_t)((p->y >> 16) - cam_y) > 0xAA) p->state = 4;
    (void)cam_x;
    return report;
}

/* sub_080C3E18: the seven pieces of 0x08528952 (x, y offsets, sprite
 * frame, vx * 0x1000, vy * -0x1000 - 0x10, + 0x0D), gravity 0x3000, the
 * first (the head) only while the head is on or in hand; with the player
 * to its right they fly left. */
static const int8_t skeleton_pieces[7][5] = {
    {0, -30, 0x1B, 0x10, 0x40}, {0, -5, 0x17, 0x10, 0x31}, {0, -3, 0x19, 0x11, 0x30},
    {4, -5, 0x15, 0x0C, 0x38}, {4, -2, 0x16, 0x12, 0x33}, {-4, -5, 0x18, 0x18, 0x32},
    {-4, -2, 0x1A, 0x0C, 0x28},
};

static void skeleton_shatter(AosEnemy *e, const AosSoma *soma, AosHitReport *report) {
    e->hidden = true;
    bool right = e->x < target_x(soma);
    for (int i = (e->skel_flags & 3) ? 0 : 1; i < 7 && report->child_count < AOS_ENEMY_MAX_CHILDREN;
         ++i) {
        AosEnemy b = {0};
        b.id = e->id;
        b.role = AOS_ROLE_DEBRIS;
        b.static_frame = (uint8_t)skeleton_pieces[i][2];
        b.mirrored = e->mirrored;
        b.combat.attack_off = b.combat.hurt_off = true;
        int dx = skeleton_pieces[i][0];
        b.vx = (int32_t)(uint8_t)skeleton_pieces[i][3] << 12;
        b.vy = -(skeleton_pieces[i][4] << 12) - 0x10;
        b.ay = 0x3000;
        if (right) {
            b.vx = -abs(b.vx);
            dx = -abs(dx);
        }
        b.x = (int32_t)((uint32_t)((e->x >> 16) + dx) << 16) | (e->x & 0xFFFF);
        b.y = (int32_t)((uint32_t)((e->y >> 16) + skeleton_pieces[i][1]) << 16) | (e->y & 0xFFFF);
        report->children[report->child_count++] = b;
    }
}

/* sub_080C35D0: the patrol between the body's home +/- 32 (turning 40
 * pixels from home) with the 0x08528948 walker at 0.375 (mode 12), turning
 * at walls and room edges; Soma in the 512 x 128 box starts state 1. */
static void skeleton_patrol(AosEnemy *e, const AosSoma *soma, const AosCollision *layer,
                            const AosEnemyKind *kind) {
    if (e->step == 0) {
        if (e->anim.id != 1) play_loop(e, kind->anims, 1, true);
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
    if (player_in_centred(e, soma, 0x200, 0x80)) {
        e->vx = 0;
        e->state = 1;
        e->substep = e->step = 0;
    }
}

/* sub_080C36E8: far from Soma (64 high and 512 wide) it patrols again;
 * otherwise it faces him and walks at 0.5 (backing off within the random
 * box), decaying by 1/16 a frame, for 1 to 8 frames at a time, choosing
 * between the 60-frame pause (state 2), the pick-up (3) and the throw (4). */
static void skeleton_close_in(AosEnemy *e, const AosSoma *soma, const AosCollision *layer,
                              const AosEnemyKind *kind, uint32_t (*random)(void)) {
    switch (e->step) {
    case 0: {
        int dy = abs(player_dy(e, soma));
        int dx = abs((int16_t)((e->x >> 16) - (target_x(soma) >> 16)));
        if (dy > 0x40 && dx > 0x200) {
            e->state = e->substep = e->step = 0;
            return;
        }
        e->rest = (uint16_t)(random() & 0x1F);
        if ((e->skel_flags & 0xA) == 0xA) e->rest = 0x40;
        e->rest = 1;
        e->step = 1;
        e->substep = 0;
    }
        /* fall through */
    case 1: {
        face_player(e, soma);
        e->speed = 0x8000;
        if (!e->mirrored) e->vx = -e->vx;
        if (player_in_centred(e, soma, (int)(random() & 0x3F) + 0x80, 0x80)) e->speed = -e->speed;
        unsigned anim = (e->skel_flags & 8) ? 2 : 1;
        if (e->anim.id != anim) play_loop(e, kind->anims, anim, true);
        if ((e->skel_flags & 0xA) == 0xA) e->vx = 0;
        e->timer = (uint8_t)((random() & 7) + 1);
        e->step = 2;
        e->substep = 0;
    }
        /* fall through */
    case 2:
        if (e->rest) e->rest--;
        if ((e->skel_flags & 6) && (random() & 0xF) == 0) e->skel_flags |= 0x20;
        else e->skel_flags &= (uint8_t)~0x20;
        e->speed -= e->speed >> 4;
        if ((e->skel_flags & 0xA) == 0xA && e->rest == 0) {
            e->state = (random() & 3) ? 4 : 2;
            e->substep = e->step = 0;
            return;
        }
        if (e->timer) {
            e->timer--;
        } else {
            if (e->rest == 0 && player_in_centred(e, soma, 0xC4, 0x80)) {
                e->state = (e->skel_flags & 6) ? 4 : 3;
                e->substep = e->step = 0;
                e->vx = 0;
                return;
            }
            e->patience++;
            if (random() % 0x28 < e->patience) {
                int dx = abs((int16_t)((e->x >> 16) - (target_x(soma) >> 16)));
                if (dx > 0x30) {
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

static void copy_collision_result(AosHitReport *report, const AosHitReport *hit) {
    report->soma_hit = hit->soma_hit;
    report->soma_damage = hit->soma_damage;
    report->enemy_hit = hit->enemy_hit;
    report->enemy_damage = hit->enemy_damage;
    report->has_damage_popup = hit->has_damage_popup;
    report->damage_popup = hit->damage_popup;
    report->killed = hit->killed;
}

/* The collision pass's report with its children (the death pieces). */
static void skeleton_merge(AosHitReport *report, const AosHitReport *hit) {
    copy_collision_result(report, hit);
    report->targetable |= hit->targetable;
    report->wake_spawner |= hit->wake_spawner;
    for (int i = 0; i < hit->child_count && report->child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
        report->children[report->child_count++] = hit->children[i];
    for (int i = 0; i < hit->effect_count && report->effect_count < AOS_ENEMY_MAX_EFFECTS; ++i)
        report->effects[report->effect_count++] = hit->effects[i];
}

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
    int32_t distance = (int32_t)(uint16_t)isqrt((uint32_t)(dx * dx + dy * dy));
    e->vx = distance ? dx * speed / distance : 0;
    e->vy = distance ? dy * speed / distance : 0;
    return (distance << 16) <= speed;
}

/* The target of the swimmers is in water (sub_0806D0B8 == 3), when there
 * is one (gEwramData + 0x13168). */
static bool target_in_water(const AosSoma *soma, const AosCollision *layer) {
    return !no_target && cell_kind(layer, target_x(soma) >> 16, target_y(soma) >> 16) == 3;
}

/* sub_080707BC(e, x, y, frame, vx, vy): a bubble (sub_080708B0) facing at
 * random, priority +/- 1 at random, deleted at once out of water. */
static void fish_bubble(const AosEnemy *e, const AosCollision *layer, int32_t x, int32_t y,
                        int frame, int32_t vx, int32_t vy, uint32_t (*random)(void),
                        AosHitReport *report) {
    bool mirrored = random() & 1;
    (void)random();     /* the priority */
    if (cell_kind(layer, x >> 16, y >> 16) != 3) return;
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy b = {0};
    b.id = e->id;
    b.role = AOS_ROLE_BUBBLE;
    b.static_frame = (int16_t)frame;
    b.x = x;
    b.y = y;
    b.vx = vx;
    b.vy = vy;
    b.mirrored = mirrored;
    b.combat.attack_off = b.combat.hurt_off = true;
    report->children[report->child_count++] = b;
}

/* sub_08070A30(e, x, y, frame): a bursting bubble (sub_0807091C) facing at
 * random, priority - 1. */
static void fish_pop(const AosEnemy *e, int32_t x, int32_t y, int frame,
                     uint32_t (*random)(void), AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy b = {0};
    b.id = e->id;
    b.role = AOS_ROLE_BUBBLE_POP;
    b.static_frame = (int16_t)frame;
    b.x = x;
    b.y = y;
    b.mirrored = random() & 1;
    b.combat.attack_off = b.combat.hurt_off = true;
    report->children[report->child_count++] = b;
}

/* sub_0806FA50 (state 0): it swims at 0.25 toward its facing, bobbing by
 * 0x400 between +/- 0.25, turning after 61 frames at the same x, pushed
 * back under the surface; every 120-239 frames a bubble 16 pixels ahead;
 * its target in water within 80 pixels starts the attack. */
static void fish_swim(AosEnemy *f, const AosSoma *soma, const AosCollision *layer,
                      uint32_t (*random)(void), AosHitReport *report) {
    if (f->step == 0) {
        f->vx = f->mirrored ? 0x4000 : -0x4000;
        f->ax = f->vy = 0;
        f->ay = 0x400;
        f->fish_count = 0;
        f->fish_last_x = f->fish_bubble = 0;
        f->step = 1;
        return;
    }
    if (f->step != 1) return;
    int r = swim_walk(f, layer, 8, -0x70000, 1, 0x40000);
    if (f->vy > 0x4000) f->ay = -0x400;
    else if (f->vy < -0x4000) f->ay = 0x400;
    if (r & 4) f->ay = -0x400;
    if (r & 8) {
        f->vy = 0;
        f->ay = 0x400;
    }
    if (f->fish_last_x == (int16_t)(f->x >> 16)) {
        if (f->fish_count++ > 0x3B) {
            f->mirrored = !f->mirrored;
            f->vx = f->mirrored ? 0x4000 : -0x4000;
            f->fish_count = 0;
            f->fish_last_x = 0;
            f->vy = 0;
            f->ay = 0x400;
        }
    } else {
        f->fish_last_x = (int16_t)(f->x >> 16);
        f->fish_count = 0;
    }
    if (!(r & 0x80)) {
        f->y += (int32_t)((uint32_t)water_below(layer, f->x >> 16, (f->y >> 16) - 7) << 16);
        f->ay = 0x400;
    }
    if (--f->fish_bubble <= 0) {
        fish_bubble(f, layer, f->x + (f->mirrored ? 0x100000 : -0x100000), f->y, 16, 0, -0x4000,
                    random, report);
        f->fish_bubble = (int16_t)(random() % 0x78 + 0x78);
    }
    if (target_in_water(soma, layer) && player_in_range(f, soma, 0x500000)) {
        f->state = 1;
        f->step = 0;
    }
}

/* Under the surface: pushed back into the water, vy 0, ay 0x400. */
static void fish_resurface(AosEnemy *f, const AosCollision *layer, int r) {
    if (r & 0x80) return;
    f->y += (int32_t)((uint32_t)water_below(layer, f->x >> 16, (f->y >> 16) - 7) << 16);
    f->vy = 0;
    f->ay = 0x400;
}

/* sub_0806FCA4 (state 1): it faces its target, stops and picks a point
 * within 4 pixels of it, 16 above; after 40 frames it spits (animation 2):
 * four bubbles 10 pixels ahead at 3.0-4.0 forward, then darts at the point
 * at 2.0 for at most 60 frames and drifts 30 more frames slowing by 1/8. */
static void fish_attack(AosEnemy *f, const AosSoma *soma, const AosCollision *layer,
                        const AosEnemyKind *kind, uint32_t (*random)(void),
                        AosHitReport *report) {
    switch (f->step) {
    case 0:
        soldier_face(f, soma);
        f->vx = f->ax = f->vy = f->ay = 0;
        f->timer = 0;
        f->fish_tx = (int16_t)((target_x(soma) >> 16) + (int32_t)(random() & 7) - 4);
        f->fish_ty = (int16_t)((target_y(soma) >> 16) - 0x10);
        f->step = 1;
        break;
    case 1:
        if (f->timer++ <= 0x27) break;
        f->timer = 0;
        f->step = 2;
        play_loop(f, kind->anims, 2, false);
        break;
    case 2:
        if (f->anim.frame == 0) break;
        f->timer = 0;
        f->step = 3;
        for (int i = 0; i < 4; ++i) {
            int32_t x, vx;
            if (f->mirrored) {
                x = f->x + 0xA0000;
                vx = (int32_t)(random() & 0xFFFF) + 0x30000;
            } else {
                x = f->x - 0xA0000;
                vx = (int32_t)0xFFFD0000 - (int32_t)(random() & 0xFFFF);
            }
            x = x - 0x20000 + (int32_t)(random() & 0x3FFFF);
            int32_t y = f->y - (int32_t)(random() & 0x3FFFF);
            int32_t vy = (i == 0 ? -0x6000 : -0x4000) - (int32_t)(random() & 0x1FFF);
            fish_bubble(f, layer, x, y, i == 0 ? 15 : 16, vx, vy, random, report);
        }
        break;
    case 3: {
        if (aim_at(f, AOS_FIXED(f->fish_tx), AOS_FIXED(f->fish_ty), 0x20000)) {
            f->step = 4;
            f->timer = 0;
            play(f, kind->anims, 1);
        }
        if (f->timer++ > 0x3B) {
            f->step = 4;
            f->timer = 0;
            play(f, kind->anims, 1);
        }
        fish_resurface(f, layer, swim_walk(f, layer, 8, -0x70000, 1, 0x40000));
        break;
    }
    case 4:
        f->vx = f->vx * 7 / 8;
        f->vy = f->vy * 7 / 8;
        fish_resurface(f, layer, swim_walk(f, layer, 8, -0x70000, 1, 0x40000));
        if (f->timer++ > 0x1D) {
            f->state = f->step = 0;
            play(f, kind->anims, 1);
        }
        break;
    default: break;
    }
}

/* sub_0806FF70 (state 2, dead): upside down it floats up (ay -0x320, at
 * most 0.25 a frame) to the surface (sound 0x6C), then bobs there with
 * bursting bubbles around it, blinking after 30 frames, gone after 70. */
static void fish_float(AosEnemy *f, const AosCollision *layer, const AosEnemyKind *kind,
                       uint32_t (*random)(void), AosHitReport *report) {
    switch (f->step) {
    case 0:
        f->vflip = true;
        f->vx = 1;
        f->ax = f->vy = 0;
        f->ay = -0x320;
        f->step = 1;
        break;
    case 1: {
        int r = swim_walk(f, layer, 0, -0x70000, 1, 0x40000);
        if (f->vy < -0x4000) f->vy = -0x4000;
        if ((r & 0x88) == 0x80) break;
        f->y += (int32_t)((uint32_t)water_below(layer, f->x >> 16, (f->y >> 16) - 7) << 16);
        f->vx = f->ax = f->vy = 0;
        f->ay = 0x200;
        f->step = 2;
        f->timer = 0;
        aos_audio_play_song(0x6C);
        break;
    }
    case 2: {
        accel_move(f, 0x20000);
        if (f->vy > 0x2000) f->ay = -0x200;
        else if (f->vy < -0x2000) f->ay = 0x200;
        int32_t x = f->x + (int32_t)(random() & 0x1FFFFF) - 0x100000;
        int32_t y = f->y + (int32_t)(random() & 0xFFFFF) - 0x80000;
        fish_pop(f, x, y, 15, random, report);
        if (f->timer++ > 0x1D) {
            int i = 0x46 - f->timer;
            f->hidden = i >= 0 && i <= 0x27 ? kind->blink[i] & 1 : false;
        }
        if (f->timer > 0x45) f->removed = true;
        break;
    }
    default: break;
    }
}

/* sub_08070168: a fry (sub_08070218) at the fish, animation 0 looping, the
 * collision type 8 with the boxes (-4, -4, 8, 8) of 0x0808FCFC, priority
 * +/- 1 at random and a quarter of the fish's contact power. */
static AosEnemy make_fry(const AosEnemy *f, uint32_t (*random)(void)) {
    AosEnemy fry = {0};
    (void)random();     /* the priority */
    fry.id = f->id;
    fry.role = AOS_ROLE_FRY;
    fry.static_frame = -1;
    fry.x = f->x;
    fry.y = f->y;
    fry.stats = f->stats;
    fry.stats.contact = (uint8_t)(f->stats.contact >> 2);
    fry.hp = 1;
    fry.combat.type = AOS_TYPE_ENEMY;
    fry.own_boxes = true;
    fry.own_hurt = fry.own_attack = (AosBox){-4, -4, 8, 8};
    return fry;
}

/* EnemyKillerFishUpdate: outside screen margin 4 (unless dead) it does
 * nothing; hidden beyond it, in the homing list on screen; alive and
 * flashing from a blow it only collides; state 0 swims, 1 attacks, 2
 * floats up dead, 3 is the generic death (a fish out of water, step 0x63),
 * 5 only animates; then the animation step and the collision pass (not
 * while dying). The first update creates its fry (+ 0x32 of them; native:
 * at creation, in water only). */
static AosHitReport fish_update(AosEnemy *f, AosSoma *soma, const AosCollision *layer,
                                const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                const AosWeaponFrames *weapon_frames, int soma_atk,
                                int soma_def, int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    if (f->state == 5) {
        step_anim(f, kind->anims);
        return report;
    }
    if (!f->helper_spawned) {
        f->helper_spawned = true;
        if (f->step != 0x63)
            for (int i = 0; i < f->param1 && spawned.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
                spawned.children[spawned.child_count++] = make_fry(f, random);
    }
    int sx = (f->x >> 16) - cam_x, sy = (f->y >> 16) - cam_y;
    bool outside = outside_margins(kind, 4, sx, sy);
    if (f->state != 2 && outside) return spawned;
    f->hidden = outside;
    bool dying = f->state == 2 || f->state == 3;
    if (f->hp > 0 && f->hit_flash) {
        f->hit_flash--;
        if (!dying) report = collide(f, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    } else {
        switch (f->state) {
        case 0: fish_swim(f, soma, layer, random, &spawned); break;
        case 1: fish_attack(f, soma, layer, kind, random, &spawned); break;
        case 2: fish_float(f, layer, kind, random, &spawned); break;
        case 3: generic_death(f, kind, random, &spawned); break;
        case 4: f->removed = true; break;
        default: break;
        }
        dying = f->state == 2 || f->state == 3;
        if (!dying) step_anim(f, kind->anims);
        if (!dying && !f->removed)
            report = collide(f, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    }
    aos_combat_tick(&f->combat);
    for (int c = 0; c < spawned.child_count && report.child_count < AOS_ENEMY_MAX_CHILDREN; ++c)
        report.children[report.child_count++] = spawned.children[c];
    take_effects(&report, &spawned);
    return report;
}

/* sub_080700CC (the fish's hit callback, after the damage and the flash):
 * a lethal blow plays animation 1 once and starts the death, 40 frames
 * (state 2 in water, 3 for a fish out of it). */
static void fish_struck(AosEnemy *f, const AosEnemyKind *kind) {
    if (f->hp > 0) return;
    play_loop(f, kind->anims, 1, false);
    step_anim(f, kind->anims);
    f->timer = 0x28;
    f->state = f->step == 0x63 ? 3 : 2;
    f->step = 0;
}

/* A new heading for a fry: v = 2.0 toward (tx, ty) (whole pixels over the
 * BIOS Sqrt distance). Returns the distance. */
static int fry_head(AosEnemy *y, int32_t tx, int32_t ty) {
    int32_t dx = whole(tx) - (y->x >> 16), dy = whole(ty) - (y->y >> 16);
    int distance = (uint16_t)isqrt((uint32_t)(dx * dx + dy * dy));
    y->vx = distance ? (dx << 17) / distance : 0;
    y->vy = distance ? (dy << 17) / distance : 0;
    return distance;
}

/* The fry's move: damped by 1/20, the box walker at 3.0, kept under the
 * surface. */
static void fry_move(AosEnemy *y, const AosCollision *layer) {
    y->vx = y->vx * 19 / 20;
    y->vy = y->vy * 19 / 20;
    if (!(box_walk(y, layer, 0x60000) & 0x80)) {
        y->y += (int32_t)((uint32_t)water_below(layer, y->x >> 16, (y->y >> 16) - 3) << 16);
        y->vy = 0;
    }
}

/* sub_08070218: a fry follows its fish (states 1 and 4: a new point within
 * 8 pixels of it every 20-39 frames, at 2.0 slowed by 1/20); its target in
 * water inside the 160 x 64 box around it, or its fish gone (state 4) or
 * dead, starts a frenzy (state 3): five dashes at points near the target
 * (near itself without one or with its fish gone), back to the fish
 * (state 4) once done or after touching Soma. Struck (sub_08070A08), it
 * bursts into a bubble and blinks out over 40 frames (state 2). Outside
 * screen margin 4 (unless dying) it does nothing. */
static AosHitReport fry_update(AosEnemy *y, AosSoma *soma, const AosCollision *layer,
                               const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                               const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
                               int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0};
    const AosEnemy *fish = y->parent;
    int sx = (y->x >> 16) - cam_x, sy = (y->y >> 16) - cam_y;
    bool outside = outside_margins(kind, 4, sx, sy);
    if (y->state != 2 && outside) return report;
    y->hidden = outside;
    if (fish) {
        if (fish->state == 4) y->fry_parent_gone = true;
        if (fish->hp <= 0 || fish->removed) y->fry_parent_dead = true;
    }
    bool pass = false;
    for (;;) {
        switch (y->state) {
        case 0:
            y->state = 1;
            y->vx = y->vy = y->ax = y->ay = 0;
            y->fish_count = 0;
            y->step = 0;
            /* fallthrough */
        case 1:
        case 4:
            if (y->fry_parent_gone || y->fry_parent_dead) {
                y->state = 3;
                y->step = 0;
                continue;
            }
            if (--y->fish_count <= 0) {
                y->fish_count = (int32_t)(random() % 0x14 + 0x14);
                int32_t fx = fish ? fish->x : y->x, fy = fish ? fish->y : y->y;
                int32_t tx = fx - 0x80000 + (int32_t)(random() & 0xFFFFF);
                int32_t ty = fy - 0x80000 + (int32_t)(random() & 0xFFFFF);
                int distance = fry_head(y, tx, ty);
                if (y->state == 4 && distance <= 9) y->state = 1;
            }
            y->mirrored = y->vx > 0;
            fry_move(y, layer);
            if (y->state != 4 &&
                player_near(soma, (y->x >> 16) - 0x50, (y->y >> 16) - 0x20, 0xA0, 0x40) &&
                target_in_water(soma, layer)) {
                y->state = 3;
                y->step = 0;
            }
            pass = true;
            break;
        case 2:
            y->hidden = y->timer <= 0x27 ? kind->blink[y->timer] & 1 : false;
            if (--y->timer == 0) y->removed = true;
            break;
        case 3:
            if (y->step == 0) {
                y->fry_touched = false;
                y->step = 1;
                y->fish_count = 0;
                y->vx = y->vy = y->ax = y->ay = 0;
                y->timer = 0;
            }
            if (--y->fish_count <= 0) {
                y->timer++;
                y->fish_count = (int32_t)(random() % 0x14 + 0x14);
                int32_t tx = 0, ty = 0;
                if (target_in_water(soma, layer)) {
                    tx = (int32_t)(random() & 0xFFFFF) + target_x(soma) - 0x80000;
                    ty = target_y(soma) - (int32_t)(random() & 0xFFFFF) - 0x100000;
                }
                if (tx == 0 || y->fry_parent_gone) {
                    tx = (int32_t)(random() & 0xFFFFF) + y->x - 0x80000;
                    ty = (int32_t)(random() & 0xFFFFF) + y->y - 0x80000;
                }
                fry_head(y, tx, ty);
            }
            if (y->vx > 0) y->mirrored = true;
            else if (y->vx < 0) y->mirrored = false;
            fry_move(y, layer);
            if (y->timer > 5 || y->fry_touched) y->state = 4;
            pass = true;
            break;
        default: break;
        }
        break;
    }
    if (pass) {
        report = collide(y, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        if (report.soma_hit) y->fry_touched = true;
        aos_combat_tick(&y->combat);
    }
    if (report.enemy_hit) {
        /* sub_08070A08: no damage; a bursting bubble where it was. */
        fish_pop(y, y->x, y->y, 15, random, &report);
        y->state = 2;
        y->timer = 0x28;
        y->step = 0;
        report.killed = false;
    }
    step_anim(y, kind->anims);
    return report;
}

/* sub_080708B0: a bubble keeps 7/8 of its vx, sways by 0.25 every 8 frames
 * and rises (sub_08069B70, head 2 up) until its head leaves the water or
 * it meets the ceiling. */
static void bubble_update(AosEnemy *b, const AosCollision *layer) {
    if (b->state == 0) {
        b->state = 1;
        b->timer = 0;
    }
    b->timer++;
    b->vx = b->vx * 7 / 8;
    if ((b->timer & 7) == 0) b->vx = (b->timer & 8) ? -0x4000 : 0x4000;
    if ((bubble_walk(b, layer, -0x20000, 1) & 0x88) != 0x80) b->removed = true;
}

/* sub_0807091C: a bursting bubble sways by 0.25 every 8 frames rising at
 * 0.25, shows the previous frame from its sixth update and is gone after
 * twelve. */
static void bubble_pop_update(AosEnemy *b) {
    if (b->state == 0) {
        b->state = 1;
        b->timer = 0;
    }
    b->timer++;
    if ((b->timer & 7) == 0) b->vx = (b->timer & 8) ? -0x4000 : 0x4000;
    b->vy = -0x4000;
    b->x += b->vx;
    b->y += b->vy;
    if (b->timer == 6) b->static_frame--;
    if (b->timer > 0xB) b->removed = true;
}


/* The animation position + 0x6E (frame and tick) at its start. */
static bool anim_at_start(const AosEnemy *e) { return e->anim.frame == 0 && e->anim.tick == 0; }

/* sub_08074C24's wing beat: animation 2 restarts as animation 0 once in
 * three, animation 0 at its end loops animation 2 again. */
static void winged_beat(AosEnemy *e, const AosAnimSet *anims, uint32_t (*random)(void)) {
    if (e->anim.id == 2) {
        if (anim_at_start(e) && random() % 3 == 0) {
            play_loop(e, anims, 0, false);
            e->cycle_done = false;
        }
    } else if (e->anim.id == 0 && e->cycle_done) {
        play_loop(e, anims, 2, true);
    }
}

/* sub_08074C24's aim: without + 0x1C, Soma more than 16 pixels off its
 * height but on a diagonal (the horizontal distance + 8 within 31 of the
 * vertical) and ahead lets it look at the point 21 pixels ahead and 9 up:
 * not a wall (sub_0806D0B8), it sets + 0x1C. With + 0x1C, at the start of
 * an animation it attacks (state 1, `step`) while Soma stays ahead, else
 * clears + 0x1C. */
static void winged_aim(AosEnemy *e, const AosSoma *soma, const AosCollision *layer,
                       uint8_t step) {
    int dx = (int16_t)((e->x >> 16) - (target_x(soma) >> 16));
    if (!e->rest) {
        int dy = abs(player_dy(e, soma));
        if (dy <= 0x10) return;
        if (abs(abs(dx) + 8 - dy) > 0x1F) return;
        if (e->mirrored ? dx >= 0 : dx <= 0) return;
        int32_t x = (e->x >> 16) + (e->mirrored ? 0x15 : -0x15);
        if (cell_kind(layer, x, (e->y >> 16) - 9) != 2) e->rest = 1;
        return;
    }
    if (!anim_at_start(e)) return;
    if (e->mirrored ? dx < 0 : dx > 0) {
        e->state = 1;
        e->step = step;
    } else {
        e->rest = 0;
    }
}

/* sub_08074C24 (state 0): it flies level (sub_0806D3AC). Step 0 beats its
 * wings and, Soma on a steep enough line, swoops (animation 3, step 1,
 * then animation 4 looping, step 2); step 2 turns to step 3 once past his
 * x by 16; steps 0, 2 and 3 aim (attack step 10 from steps 0 and 3, 0 from
 * step 2); step 4 only flies. */
static void winged_fly(AosEnemy *e, const AosSoma *soma, const AosCollision *layer,
                       const AosAnimSet *anims, uint32_t (*random)(void)) {
    switch (e->step) {
    case 0:
        e->x += e->vx;
        e->y += e->vy;
        winged_beat(e, anims, random);
        if (anim_at_start(e)) {
            int dx = abs((int16_t)((e->x >> 16) - (target_x(soma) >> 16)));
            if (dx <= abs(player_dy(e, soma)) + 0x20) {
                e->step = 1;
                play_loop(e, anims, 3, false);
                e->cycle_done = false;
                return;
            }
        }
        winged_aim(e, soma, layer, 10);
        return;
    case 1:
        if (e->cycle_done) {
            play_loop(e, anims, 4, true);
            e->step = 2;
        }
        e->x += e->vx;
        e->y += e->vy;
        return;
    case 2:
        e->x += e->vx;
        e->y += e->vy;
        if (anim_at_start(e)) {
            int d = (int16_t)((target_x(soma) >> 16) - (e->x >> 16));
            if (e->vx > 0 ? d < -0x10 : d > 0x10) {
                e->step = 3;
                play_loop(e, anims, 2, true);
                e->cycle_done = false;
                return;
            }
        }
        winged_aim(e, soma, layer, 0);
        return;
    case 3:
        e->x += e->vx;
        e->y += e->vy;
        winged_beat(e, anims, random);
        winged_aim(e, soma, layer, 10);
        return;
    case 4:
        e->x += e->vx;
        e->y += e->vy;
        return;
    default:
        return;
    }
}

/* sub_08075230: the bone 21 pixels ahead and 9 up, at 2.0 forward and 2.0
 * down (animation 8, attack box 0x0404FC00, type 8, a knockback contact). */
static void winged_throw(const AosEnemy *e, AosHitReport *report) {
    aos_audio_play_song(0x85);
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy b = {0};
    b.id = e->id;
    b.role = AOS_ROLE_WINGED_BONE;
    b.static_frame = -1;
    b.mirrored = e->mirrored;
    b.x = (int32_t)((uint32_t)(uint16_t)((e->x >> 16) + (e->mirrored ? 0x15 : -0x15)) << 16);
    b.y = (int32_t)((uint32_t)(uint16_t)((e->y >> 16) - 9) << 16);
    b.vx = e->mirrored ? 0x20000 : (int32_t)0xFFFE0000;
    b.vy = 0x20000;
    b.hp = 1;
    b.stats = e->stats;
    b.combat.type = AOS_TYPE_ENEMY;
    b.combat.hurt_off = true;
    b.own_boxes = true;
    b.own_hurt = b.own_attack = (AosBox){0, -4, 4, 4};
    report->children[report->child_count++] = b;
}

/* sub_08075078 (state 1): it hangs still; from step 0 animation 5 (Soma
 * beyond 80 pixels, sub_0806D044) then 6, past frame 2 the bone; from
 * step 10 animation 7, past frame 2 the bone; each throw plays 0x85, and
 * at the animation's end it flies on (animation 1, state 0 step 4). */
static void winged_attack(AosEnemy *e, const AosSoma *soma, const AosAnimSet *anims,
                          AosHitReport *report) {
    switch (e->step) {
    case 0: {
        int32_t dx = pixel_delta(target_x(soma), e->x), dy = pixel_delta(target_y(soma), e->y);
        bool far = (int16_t)isqrt((uint32_t)(dx * dx + dy * dy)) > 0x50;
        e->step = far ? 1 : 2;
        play_loop(e, anims, far ? 5 : 6, false);
        e->cycle_done = false;
        return;
    }
    case 1:
        if (e->cycle_done) {
            e->step = 2;
            play_loop(e, anims, 6, false);
            e->cycle_done = false;
        }
        return;
    case 2:
    case 11:
        if (e->anim.frame <= 2) return;
        e->step++;
        e->cycle_done = false;
        winged_throw(e, report);
        return;
    case 3:
    case 12:
        if (e->cycle_done) {
            play_loop(e, anims, 1, true);
            e->state = 0;
            e->step = 4;
        }
        return;
    case 10:
        play_loop(e, anims, 7, true);
        e->step = 11;
        return;
    default:
        return;
    }
}

/* sub_0807540C: seven pieces (sprite frames 0x24-0x2A) at its position,
 * vx (r & 0x3FFFF) - 2.0, vy -0.25 - (r & 0xFFFF), gravity 0x1800, for
 * 40-43 frames. */
static void winged_shatter(const AosEnemy *e, uint32_t (*random)(void), AosHitReport *report) {
    for (int i = 0; i < 7; ++i) {
        AosEnemy p = {0};
        p.id = e->id;
        p.role = AOS_ROLE_WINGED_PIECE;
        p.x = e->x;
        p.y = e->y;
        p.vx = (int32_t)(random() & 0x3FFFF) - 0x20000;
        p.vy = -(int32_t)(random() & 0xFFFF) - 0x4000;
        p.ay = 0x1800;
        p.static_frame = (int16_t)(0x24 + i);
        p.mirrored = e->mirrored;
        p.timer = (uint8_t)((random() & 3) + 0x28);
        p.combat.attack_off = p.combat.hurt_off = true;
        if (report->child_count < AOS_ENEMY_MAX_CHILDREN)
            report->children[report->child_count++] = p;
    }
}

/* sub_08075614: a piece keeps 7/8 of its vx (sub_0806D490), moves
 * (sub_0806D3D8, 8.0), blinks below 41 frames (sub_0806D518) and goes at 0. */
static void winged_piece_update(AosEnemy *p, const AosEnemyKind *kind) {
    p->vx = p->vx * 7 / 8;
    p->vy += p->ay;
    if (p->vy > 0x80000) p->vy = 0x80000;
    p->x += p->vx;
    p->y += p->vy;
    if (p->timer <= 0x28) p->hidden = p->timer < 40 && (kind->blink[p->timer] & 1);
    if (--p->timer == 0) p->removed = true;
}

/* sub_08075314: the bone flies (sub_08069770 with floor speed 1.0 and its
 * head 32 up), cycling its attack box through (0, -4), (12, -16) and
 * (24, -28) (4 x 4); beyond the margins of entry 1 it is gone; meeting a
 * wall, ceiling or floor it rests 60 frames without collisions, blinking
 * from 40, then goes. */
static AosHitReport winged_bone_update(AosEnemy *b, AosSoma *soma, const AosCollision *layer,
                                       const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                       const AosWeaponFrames *weapon_frames, int soma_atk,
                                       int soma_def, int cam_x, int cam_y) {
    AosHitReport report = {0};
    if (b->state == 0) {
        static const AosBox boxes[3] = {{0, -4, 4, 4}, {12, -16, 4, 4}, {24, -28, 4, 4}};
        b->timer++;
        b->own_attack = boxes[b->timer % 3];
        int result = projectile_collide(b, layer, 0x10000, (int32_t)0xFFFE0000);
        int sx = (b->x >> 16) - cam_x, sy = (b->y >> 16) - cam_y;
        if (outside_margins(kind, 1, sx, sy)) {
            b->removed = true;
            return report;
        }
        if (result & 7) {
            b->state = 1;
            b->timer = 0x3C;
            b->combat.attack_off = b->combat.hurt_off = true;
            return report;
        }
        report = collide(b, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        aos_combat_tick(&b->combat);
    } else {
        int8_t t = (int8_t)b->timer--;
        if (t <= 0x28) {
            b->hidden = (uint8_t)b->timer < 40 && (kind->blink[(uint8_t)b->timer] & 1);
            if ((int8_t)b->timer <= 0) {
                b->removed = true;
                return report;
            }
        }
    }
    step_anim(b, kind->anims);
    return report;
}

/* EnemyWingedSkeletonUpdate (states 0, 1, 2, 4; 3 is the room producer):
 * beyond the margins of entry 4 it vanishes (state 2); flashing from a
 * blow it only collides; state 2 shatters it at 0 HP and runs the generic
 * death with + 0x0D 1 (the producer's count drops with it). */
static AosHitReport winged_update(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                                  const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                  const AosWeaponFrames *weapon_frames, int soma_atk,
                                  int soma_def, int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0};
    int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
    if (e->state != 2 && e->state != 3) {
        if ((uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0) report.targetable = true;
    }
    if (e->state != 3 && outside_margins(kind, 4, sx, sy)) {
        e->state = 2;
        e->timer = 1;
    }
    if (e->hp > 0 && e->hit_flash) {
        e->hit_flash--;
        if (e->state != 2) {
            AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
            skeleton_merge(&report, &hit);
        }
        aos_combat_tick(&e->combat);
        return report;
    }
    switch (e->state) {
    case 0: winged_fly(e, soma, layer, kind->anims, random); break;
    case 1: winged_attack(e, soma, kind->anims, &report); break;
    case 2:
        e->timer = 1;
        if (e->hp <= 0 && !e->spawned_count) {
            winged_shatter(e, random, &report);
            e->spawned_count = 1;
        }
        generic_death(e, kind, random, &report);
        aos_combat_tick(&e->combat);
        return report;
    case 4:
        step_anim(e, kind->anims);
        return report;
    default:
        return report;
    }
    step_anim(e, kind->anims);
    if (!e->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0) {
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        skeleton_merge(&report, &hit);
    }
    aos_combat_tick(&e->combat);
    return report;
}

/* EnemySkeletonUpdate: the projectile's affine slot turns 0x708 a frame
 * with the facing; state 5 (inert) loops animation 0, 0xFF waits 256
 * frames after the shattering; otherwise, within the activity window
 * (sub_0806CC20, which shows it on screen) and not flashing from a blow,
 * + 0x12 takes a random value and states 0-4 run, then the animation step
 * and the collision pass. The first update creates the head and the bone
 * in hand (EnemySkeletonCreate's children). */
static AosHitReport skeleton_update(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                                    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                    const AosWeaponFrames *weapon_frames, int soma_atk,
                                    int soma_def, int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0};
    e->skel_spin = (uint16_t)(e->skel_spin + (e->mirrored ? 0x708 : -0x708));
    if (e->state == 5) {
        if (e->step == 0) {
            play_loop(e, kind->anims, 0, true);
            e->hidden = false;
            e->step = 1;
        }
        step_anim(e, kind->anims);
        return report;
    }
    if (e->state == 0xFF) {
        if (--e->timer == 0xFF) e->removed = true;
        return report;
    }
    if (!e->helper_spawned) {
        e->helper_spawned = true;
        skeleton_child(e, AOS_ROLE_SKELETON_HEAD, &report);
        skeleton_child(e, AOS_ROLE_SKELETON_HELD, &report);
    }
    int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
    /* sub_0806E4F8(e, -16): the record's + 0x20 is 0. */
    e->hidden = (int16_t)sy < -16;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
    if (e->hit_flash) {
        e->hit_flash--;
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        skeleton_merge(&report, &hit);
        aos_combat_tick(&e->combat);
        return report;
    }
    (void)random();     /* + 0x12 = 40 + r % 80 */
    switch (e->state) {
    case 0: skeleton_patrol(e, soma, layer, kind); break;
    case 1: skeleton_close_in(e, soma, layer, kind, random); break;
    case 2:
        /* sub_080C390C */
        if (e->step == 0) {
            if (e->anim.id != 0) play_loop(e, kind->anims, 0, true);
            e->timer = 0x3C;
            e->step = 1;
        }
        if (--e->timer == 0) {
            e->state = 1;
            e->substep = e->step = 0;
        }
        break;
    case 3:
        /* sub_080C3958: one chance in 8 the head goes to its hand
         * (animation 5), else it picks a bone (animation 6). */
        if (e->step == 0) {
            if ((random() & 7) == 0 && (e->skel_flags & 1)) {
                play_loop(e, kind->anims, 5, false);
                e->skel_flags = (uint8_t)((e->skel_flags & ~1) | 2);
            } else {
                play_loop(e, kind->anims, 6, false);
                skeleton_child(e, AOS_ROLE_SKELETON_HELD, &report);
                e->skel_flags |= 4;
            }
            e->step = 1;
        } else if (e->anim.flags & AOS_ANIM_ENDED) {
            e->state = 1;
            e->substep = e->step = 0;
        }
        break;
    case 4:
        /* sub_080C39F4: animation 3 (+ 0x1A bit 3) or 4; at its frame 2 the
         * head in hand or the bone flies. */
        if (e->step == 0) e->step = 1;
        if (e->step == 1) {
            if (e->skel_flags & 8) {
                play_loop(e, kind->anims, 3, false);
                e->skel_flags &= (uint8_t)~8;
            } else {
                play_loop(e, kind->anims, 4, false);
            }
            e->step = 2;
        }
        if (e->step == 2 && e->anim.frame == 2) {
            bool head = e->skel_flags & 2;
            skeleton_throw(e, head, &report);
            e->skel_flags &= (uint8_t)(head ? ~2 : ~4);
            e->step = 3;
        } else if (e->step == 3 && (e->anim.flags & AOS_ANIM_ENDED)) {
            e->state = 1;
            e->substep = e->step = 0;
        }
        break;
    default:
        break;
    }
    step_anim(e, kind->anims);
    sx = (e->x >> 16) - cam_x;
    sy = (e->y >> 16) - cam_y;
    if (!e->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0) {
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        skeleton_merge(&report, &hit);
    }
    aos_combat_tick(&e->combat);
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
        int dx = (int)((target_x(soma) - e->x) >> 16);
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
                e->walk_timer = target_x(soma);
                e->speed = target_y(soma) - 0x180000;
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
            copy_collision_result(&report, &hit);
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
    child.contact_elements = 0x100;    /* sub_080899CC: the breath petrifies */
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
            /* sub_08089560: sound 0x67 at + 0x0D 0x1E and 0x32. */
            if (e->timer == 0x1E || e->timer == 0x32) aos_audio_play_song(0x67);
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
        int dx = (int)((target_x(soma) - e->x) >> 16);
        int dy = (int)((target_y(soma) - e->y) >> 16);
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
            copy_collision_result(&report, &hit);
        }
    }
    aos_combat_tick(&e->combat);
    return report;
}

static void merge_enemy_hit(AosHitReport *report, AosHitReport hit) {
    copy_collision_result(report, &hit);
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
    orb.x += owner->x < target_x(soma) ? 0x100000 : -0x100000;
    orb.y += 0x80000;
    (void)move_toward(&orb, target_x(soma), target_y(soma) - 0x200000, 0x20000);
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
        helper->x = target_x(soma);
        helper->y = target_y(soma) - 0x280000;
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
        e->walk_timer = target_x(soma) + (e->mirrored ? -side : side);
        e->speed = target_y(soma) + (imp ? -0x2D0000 : -0x460000);
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
        bool player_right = target_x(soma) > e->x;
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
        bool player_right = target_x(soma) > e->x;
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
            e->walk_timer = target_x(soma);
            e->speed = target_y(soma) - 0x200000;
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
        int32_t tx = target_x(soma) + (soma->facing_left ? -0x100000 : 0x100000);
        bool arrived = move_toward(e, tx, target_y(soma) - 0x200000, 0x20000);
        integrate(e);
        if (arrived) {
            report->force_input = true;
            report->forced_held = (e->timer & 1) ? AOS_KEY_ATTACK : 0;
            ++e->timer;
            /* sub_080C4D34 waits until the forced B presses have put Soma
             * outside his neutral states 0, 4 and 8 for 20 frames. */
            if (soma->state != 0 && soma->state != 4 && soma->state != 8 &&
                ++e->patience > 20) {
                e->vx = e->x < target_x(soma) ? -0x10000 : 0x10000;
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
static void ripper_blade(AosEnemy *owner, const AosEnemyKind *kind,
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
    /* sub_080C2B74: native state 3 starts the blade's flight.
     * The host runtime creates the blade directly in its flying state 4. */
    if (b->state == 4 && !b->firing) {
        b->firing = true;
        aos_audio_play_song(0x89);
    }
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
        /* sub_080C2DC4: only contact with Soma plays this sound. */
        if (report.soma_hit) aos_audio_play_song(0x13A);
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
            /* sub_080C2A18: the Ripper's one-shot death sound. */
            aos_audio_play_song(0x72);
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
        int dx = (e->x >> 16) - (target_x(soma) >> 16);
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

/* Peeping Eye. A sequence is (frame, ticks) pairs ending at 0xFF: the
 * opening (0x08527400) and the four looks of 0x08527450 (0 blink, 1 hurt,
 * 2 and 3 looking up and down). */
typedef struct {
    uint8_t frame, ticks;
} PeepingStage;

static const PeepingStage peeping_opening[] = {
    {3, 2}, {2, 2}, {1, 2}, {0, 4}, {1, 2}, {2, 2}, {3, 2}, {2, 2}, {1, 2}, {0, 20}, {0xFF, 0},
};
static const PeepingStage peeping_look_blink[] = {
    {0, 96}, {1, 4}, {2, 4}, {3, 6}, {2, 4}, {1, 4}, {0, 20}, {0xFF, 8},
};
static const PeepingStage peeping_look_hurt[] = {
    {8, 2}, {7, 2}, {6, 2}, {4, 2}, {5, 2}, {0, 2}, {8, 2}, {7, 2}, {6, 2},
    {4, 2}, {5, 2}, {0, 2}, {8, 6}, {7, 20}, {8, 7}, {0, 20}, {0xFF, 10},
};
static const PeepingStage peeping_look_up[] = {{10, 96}, {0, 20}, {0xFF, 9}};
static const PeepingStage peeping_look_down[] = {{9, 96}, {0, 20}, {0xFF, 0}};
static const PeepingStage *const peeping_looks[4] = {
    peeping_look_blink, peeping_look_hurt, peeping_look_up, peeping_look_down,
};

/* The angle from the eye's centre (1 and 23 pixels from it) to the target,
 * in whole pixels (truncated toward zero). */
static uint16_t peeping_aim(const AosEnemy *eye, const AosSoma *soma) {
    return aos_arctan2((int16_t)pixel_delta(target_x(soma), eye->x + 0x10000),
                       (int16_t)pixel_delta(target_y(soma), eye->y + 0x170000));
}

/* The opening's tick: true once it ended (the eye open, + 0x18 bit 1, a
 * blink starting at a random tick r & 0x3F). */
static bool peeping_open(AosEnemy *e, uint32_t (*random)(void)) {
    e->pe_flags |= 1;
    if (++e->pe_tick >= peeping_opening[e->pe_index].ticks) {
        e->pe_tick = 0;
        int8_t last = e->pe_index++;
        if (peeping_opening[e->pe_index].frame == 0xFF) {
            e->pe_flags |= 2;
            e->pe_frame = peeping_opening[last].frame;
            e->pe_index = 0;
            e->pe_seq = 0;
            e->pe_tick = (uint8_t)(random() & 0x3F);
            e->pe_hurt = false;
            return true;
        }
    }
    e->pe_frame = peeping_opening[e->pe_index].frame;
    return false;
}

/* A look's tick; a finished one starts again at a random tick. */
static bool peeping_look_tick(AosEnemy *e, uint32_t (*random)(void)) {
    e->pe_flags |= 1;
    const PeepingStage *look = peeping_looks[e->pe_seq];
    if (++e->pe_tick >= look[e->pe_index].ticks) {
        e->pe_tick = 0;
        if (look[++e->pe_index].frame == 0xFF) {
            e->pe_index = 0;
            e->pe_tick = (uint8_t)(random() & 0x3F);
            return true;
        }
    }
    return false;
}

/* sub_08088168 (state 0). Step 0 waits for the target in (x - 90, y - 100,
 * 180, 200) or a first wound (sub_0806D54C); step 1 opens, faces the
 * target and heads 64 pixels short of it, 64 above; step 2 picks a point
 * 16 to 63 pixels to either side of the target and 32 to 79 above it; step
 * 3 looks (a finished blink looks up or down when the target lies in those
 * quarters, any other look blinks again; a wound starts the hurt look,
 * facing the target) and steers: +- 0x800 per axis beyond 4 pixels, the
 * speeds keeping 31/32, moved by sub_0806D3D8(e, 2.0), back to step 2 once
 * within 4 pixels or after 120 frames. */
static void peeping_watch(AosEnemy *e, const AosSoma *soma, uint32_t (*random)(void)) {
    switch (e->step) {
    case 0:
        if (player_near(soma, (int16_t)((e->x >> 16) - 90), (int16_t)((e->y >> 16) - 100), 180,
                        200) ||
            e->hp < e->stats.hp) {
            e->step = 1;
            e->timer = 0;
            e->pe_tick = 0;
            e->pe_index = 0;
        }
        return;
    case 1:
        if (!peeping_open(e, random)) return;
        soldier_face(e, soma);
        e->target_x = target_x(soma) + (e->mirrored ? -0x400000 : 0x400000);
        e->target_y = target_y(soma) - 0x400000;
        e->vx = e->vy = 0;
        e->step = 3;
        e->timer = 0;
        return;
    case 2: {
        int32_t side = (int32_t)(random() % 0x300000u) + 0x100000;
        int32_t tx = target_x(soma);
        e->target_x = (random() & 1) ? tx + side : tx - side;
        e->target_y = target_y(soma) - (int32_t)(random() % 0x300000u) - 0x200000;
        e->step = 3;
        e->timer = 0;
    }
        /* fall through */
    default:
        break;
    }
    if (peeping_look_tick(e, random)) {
        if (e->pe_seq == 0) {
            uint16_t a = peeping_aim(e, soma);
            if ((uint16_t)(a - 0x2000) <= 0x4000) e->pe_seq = 2;
            else if ((uint16_t)(a - 0xA000) <= 0x4000) e->pe_seq = 3;
        } else {
            e->pe_seq = 0;
        }
    }
    if (e->pe_hurt) {
        e->pe_hurt = false;
        if (e->pe_seq != 1) {
            soldier_face(e, soma);
            e->pe_seq = 1;
            e->pe_index = 0;
            e->pe_tick = 0;
        }
    }
    e->pe_frame = peeping_looks[e->pe_seq][e->pe_index].frame;
    int32_t dx = e->target_x - e->x, dy = e->target_y - e->y;
    e->ax = dx > 0x40000 ? 0x800 : dx < -0x40000 ? -0x800 : 0;
    e->ay = dy > 0x40000 ? 0x800 : dy < -0x40000 ? -0x800 : 0;
    e->vx = e->vx * 31 / 32;                    /* sub_0806D490(e, 32) */
    e->vy = e->vy * 31 / 32;                    /* sub_0806D4A8(e, 32) */
    accel_move(e, 0x20000);
    if ((dx < 0 ? -dx : dx) <= 0x3FFFF && (dy < 0 ? -dy : dy) <= 0x3FFFF) e->step = 2;
    if (e->timer++ > 0x77) e->step = 2;
}

/* sub_08088510 (state 4, under the global 0x8E & 0x40): 31 frames, the
 * opening, then looks in turn (a finished look blinks again). */
static void peeping_showcase(AosEnemy *e, uint32_t (*random)(void)) {
    switch (e->step) {
    case 0:
        if (e->timer++ > 0x1D) e->step = 1;
        return;
    case 1:
        if (peeping_open(e, random)) e->step = 2;
        return;
    case 2:
    case 3:
        if (peeping_look_tick(e, random)) e->pe_seq = 0;
        e->pe_frame = peeping_looks[e->pe_seq][e->pe_index].frame;
        return;
    default:
        return;
    }
}

/* sub_08088638, the hit callback (the damage and flash are collide's): the
 * hurt flag, a 2.0 push away from the attacker (Soma, whose position
 * stands in for the weapon entity) and, at a lethal blow, sound 0x6E and
 * the death (state 2, 45 frames). */
static void peeping_struck(AosEnemy *e, const AosSoma *soma) {
    e->pe_hurt = true;
    uint16_t a = aos_arctan2((int16_t)pixel_delta(e->x, soma->x),
                             (int16_t)pixel_delta(e->y, soma->y));
    e->vx = fixed_mul(aos_sine(a + 0x4000u), 0x20000);
    e->vy = fixed_mul(aos_sine(a), 0x20000);
    if (e->hp <= 0) {
        aos_audio_play_song(0x6E);
        e->timer = 0x2D;
        e->state = 2;
        e->step = e->substep = 0;
    }
}

/* sub_0806D578(x, y, priority): a flame there, mirrored at random. */
static void peeping_flame(int32_t x, int32_t y, uint32_t (*random)(void), AosHitReport *report) {
    if (report->effect_count >= AOS_ENEMY_MAX_EFFECTS) return;
    AosEffect *flame = &report->effects[report->effect_count++];
    *flame = (AosEffect){.kind = AOS_EFFECT_FLAME, .x = x, .y = y};
    flame->mirrored = (random() & 1u) != 0;
}

/* EnemyPeepingEyeCreate's parts, in its order: the eyeball (frame 0xF,
 * EnemyPeepingEyeCreateEyeball), the pupil (0x10, sub_08088D28) and six
 * segments (sub_08088978: frames 0xB, 0xB, 0xC, 0xC, 0xD, 0xE reaching 12,
 * 10, 8, 8, 7 and 3 pixels, angle (k + 1) * 5000 or, with the creation's
 * r & 1, (6 - k) * 5000, death delay 6 (k + 1), hidden). */
static void peeping_parts(const AosEnemy *eye, AosHitReport *report) {
    static const uint8_t frames[6] = {0xB, 0xB, 0xC, 0xC, 0xD, 0xE};
    static const int32_t reach[6] = {0xC0000, 0xA0000, 0x80000, 0x80000, 0x70000, 0x30000};
    for (int i = 0; i < 8 && report->child_count < AOS_ENEMY_MAX_CHILDREN; ++i) {
        AosEnemy c;
        make_static_child(&c, eye, AOS_ROLE_PEEPING_SEGMENT, i == 6 ? 0xF : i == 7 ? 0x10 : 0);
        c.mirrored = false;
        c.combat.attack_off = c.combat.hurt_off = true;
        if (i == 0 || i == 1) {
            c.variant = (uint8_t)(i + 6);
            c.static_frame = i == 0 ? 0xF : 0x10;
        } else {
            int k = i - 2;
            c.variant = (uint8_t)k;
            c.static_frame = frames[k];
            c.angle = (uint16_t)((eye->pe_reverse ? 6 - k : k + 1) * 5000);
            c.timer = (uint8_t)(6 * (k + 1));
            c.pe_radius = 0;
            c.pe_radius_target = reach[k];
            c.hidden = true;
        }
        report->children[report->child_count++] = c;
    }
}

/* EnemyPeepingEyeUpdate: + 0x18 bit 0 cleared; alive (not dying) beyond
 * sub_0806D128(e, 4) a placed eye (parameter 0 == 0) does nothing, and it
 * is hidden there; the flash counts down (sub_0806AE54) outside states 2
 * and 3. State 0 watches, 1 plays animation 0 once, 2 dies (frame 0,
 * hidden, one flame, 45 frames), 3 is removed on its fourth frame, 4 (the
 * global 0x8E & 0x40) only looks. + 0x18 bit 0 shows the frame + 0x19
 * instead of the animation; then the animation step and, outside states 2
 * and 3 for a placed eye, the collision pass. The hit stun of sub_0806D4C0
 * needs the global pause 0x4BE (taken as 0) and the tracking list of
 * sub_08021924 is not ported. */
static AosHitReport peeping_update(AosEnemy *e, AosSoma *soma, const AosEnemyKind *kind,
                                   const AosWeaponEntity *weapon,
                                   const AosWeaponFrames *weapon_frames, int soma_atk,
                                   int soma_def, int cam_x, int cam_y,
                                   uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    if (!e->helper_spawned) {
        e->helper_spawned = true;
        peeping_parts(e, &spawned);
    }
    e->pe_flags &= (uint8_t)~1u;
    bool outside = outside_margins(kind, 4, (e->x >> 16) - cam_x, (e->y >> 16) - cam_y);
    if (e->state != 2 && e->param0 == 0 && outside) return spawned;
    e->hidden = outside;
    if ((uint8_t)(e->state - 2) > 1 && e->hit_flash) e->hit_flash--;
    switch (e->state) {
    case 0: peeping_watch(e, soma, random); break;
    case 1:
        if (e->step == 0) {
            play_loop(e, kind->anims, 0, false);
            e->step = 1;
        }
        break;
    case 2:
        e->pe_flags |= 1;
        e->pe_frame = 0;
        e->hidden = true;
        if (e->substep == 0) {
            e->substep = 1;
            peeping_flame(e->x, e->y, random, &spawned);
        }
        if ((int8_t)--e->timer <= 0) {
            e->state = 3;
            e->timer = 0;
        }
        break;
    case 3:
        e->hidden = true;
        if (e->timer++ > 2) e->removed = true;
        break;
    case 4: peeping_showcase(e, random); break;
    default: break;
    }
    e->static_frame = (e->pe_flags & 1) ? e->pe_frame : -1;
    if (e->static_frame < 0) step_anim(e, kind->anims);
    if (e->state != 4 && (uint8_t)(e->state - 2) > 1 && e->param0 == 0 && !e->removed) {
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        merge_enemy_hit(&report, hit);
    }
    aos_combat_tick(&e->combat);
    for (int c = 0; c < spawned.child_count && report.child_count < AOS_ENEMY_MAX_CHILDREN; ++c)
        report.children[report.child_count++] = spawned.children[c];
    take_effects(&report, &spawned);
    return report;
}

/* The parts. The eyeball (sub_080888D4) sits on the eye with its
 * visibility; the pupil (sub_08088724) also takes its facing and, once the
 * eye is open, moves its offset 0x4000 a frame toward 3 * (cos, sin) of the
 * angle to the target (exactly onto it within 0x3FFF). Both go when the
 * eye dies. A segment (sub_08088A64), unless the eye flashes
 * (sub_0806ADBC), sways once the eye is open (its angle 500 a frame between
 * 0x800 and 0x7800) while its radius grows by a 64th of its reach; it is
 * shown only while the eye is open. It hangs from the previous segment (the
 * eye for the first) at its angle, pulled toward 0x4000 by a quarter
 * (first), as is (second) or pushed away by a quarter (the others); the
 * tip turns by its angle + half its distance from 0x4000. When the eye
 * dies each segment flames after 6 (k + 1) frames and again 4 frames
 * later, then hides until the eye goes. Hidden beyond sub_0806D128(e, 1). */
static void peeping_part_update(AosEnemy *p, const AosEnemyKind *kind, const AosSoma *soma,
                                int cam_x, int cam_y, uint32_t (*random)(void),
                                AosHitReport *report) {
    AosEnemy *eye = (AosEnemy *)p->parent;
    if (!eye || eye->removed || eye->id != AOS_ENEMY_PEEPING_EYE) {
        p->removed = true;
        return;
    }
    if (p->variant >= 6) {
        if (p->state != 0) {
            p->removed = true;
            return;
        }
        p->hidden = eye->hidden;
        if (p->variant == 6) {
            p->x = eye->x;
            p->y = eye->y;
        } else {
            p->mirrored = eye->mirrored;
            if (!flag_8e_40 && (eye->pe_flags & 2)) {
                uint16_t a = peeping_aim(eye, soma);
                int32_t cx = aos_sine(a + 0x4000u) * 3, cy = aos_sine(a) * 3;
                int32_t d = p->pe_off_x - cx;
                p->pe_off_x = (d < 0 ? -d : d) <= 0x3FFF ? cx : p->pe_off_x + (d > 0 ? -0x4000 : 0x4000);
                d = p->pe_off_y - cy;
                p->pe_off_y = (d < 0 ? -d : d) <= 0x3FFF ? cy : p->pe_off_y + (d > 0 ? -0x4000 : 0x4000);
            }
            p->x = eye->x + p->pe_off_x;
            p->y = eye->y + p->pe_off_y;
        }
        if (eye->state == 2) {
            p->state = 1;
            p->removed = true;
        }
        return;
    }
    unsigned k = p->variant;
    eye->pe_seg[k] = p;
    const AosEnemy *prev = k ? eye->pe_seg[k - 1] : NULL;
    if (!prev || prev->removed || prev->parent != eye || prev->role != AOS_ROLE_PEEPING_SEGMENT)
        prev = eye;
    if (!(eye->hp > 0 && eye->hit_flash)) {
        if (p->step == 0) {
            if (eye->pe_flags & 2) p->step = 1;
        } else if (p->step == 1) {
            p->angle = (uint16_t)(p->angle + 500);
            if ((int16_t)p->angle > 0x7800) {
                p->angle = 0x7800;
                p->step = 2;
            }
        } else if (p->step == 2) {
            p->angle = (uint16_t)(p->angle - 500);
            if ((int16_t)p->angle <= 0x7FF) {
                p->angle = 0x800;
                p->step = 1;
            }
        }
        if (eye->pe_flags & 2) {
            p->hidden = false;
            if (p->pe_radius < p->pe_radius_target) {
                p->pe_radius += p->pe_radius_target / 64;
                if (p->pe_radius >= p->pe_radius_target) p->pe_radius = p->pe_radius_target;
            }
        } else {
            p->hidden = true;
        }
    }
    int32_t a = (int16_t)p->angle;
    int32_t pull = (a - 0x4000) / 4;
    int32_t bent = k == 0 ? a - pull : k == 1 ? a : a + pull;
    p->x = prev->x + fixed_mul(aos_sine((uint32_t)bent + 0x4000u), p->pe_radius);
    p->y = prev->y + fixed_mul(aos_sine((uint32_t)bent), p->pe_radius);
    if (k == 5) p->pe_rotation = (uint16_t)(a + (a - 0x4000) / 2);
    if (p->state == 0 && eye->state == 2) {
        p->state = 1;
        p->hidden = false;
    }
    if (p->state == 1) {
        p->hidden = false;
        if (p->timer-- == 0) {
            peeping_flame(p->x, p->y + 0x40000, random, report);
            p->state = 2;
            p->timer = 4;
        }
    }
    if (p->state == 2) {
        p->hidden = false;
        if (p->timer-- == 0) {
            peeping_flame(p->x, p->y + 0x40000, random, report);
            p->state = 3;
            p->hidden = true;
        }
    }
    if (p->state == 3) {
        p->hidden = true;
        if (eye->state == 3) p->removed = true;
    }
    if (!p->hidden && outside_margins(kind, 1, (p->x >> 16) - cam_x, (p->y >> 16) - cam_y))
        p->hidden = true;
}

/* sub_0806E29C(e, w, h): the target inside the w x h box centred on the
 * entity (unsigned 16-bit whole-pixel compares, inclusive). */
static bool buer_player_in_window(const AosEnemy *e, const AosSoma *soma,
                                  uint16_t width, uint16_t height) {
    uint16_t dx = (uint16_t)((target_x(soma) >> 16) - ((e->x >> 16) - (int)(width / 2)));
    uint16_t dy = (uint16_t)((target_y(soma) >> 16) - ((e->y >> 16) - (int)(height / 2)));
    return dx <= width && dy <= height;
}

/* sub_0806DF40(e, dx, dy) / sub_0806B0A8 (dx = dy = 0): the point outside
 * the room's cells (30 x 26 for a single screen, 32 per screen otherwise;
 * negative pixels round toward zero). */
static bool room_outside(const AosEnemy *e, const AosCollision *layer, int dx, int dy) {
    if (!layer || !layer->width_screens || !layer->height_screens) return false;
    int width = layer->width_screens == 1 ? 30 : 32 * layer->width_screens;
    int height = layer->height_screens == 1 ? 26 : 32 * layer->height_screens;
    int x = (int16_t)((e->x >> 16) + dx), y = (int16_t)((e->y >> 16) + dy);
    int cx = (x < 0 ? x + 7 : x) >> 3, cy = (y < 0 ? y + 7 : y) >> 3;
    return cx < 0 || cx >= width || cy < 0 || cy >= height;
}

/* The walker probes 0x08528B64 (exported as `probes buer`). */
static const AosProbes *buer_probes(const AosEnemyKind *kind) {
    return kind && kind->probes.count > 0 && kind->probes.count <= 8 ? &kind->probes : NULL;
}

/* sub_0806CAF8(e, + 0x18, 0x08528B64, mode) then sub_080CC5B4: the move is
 * undone when the point 24 pixels ahead (by the speed's sign) leaves the
 * room. */
static int buer_walk(AosEnemy *b, const AosSoma *soma, const AosCollision *layer,
                     const AosEnemyKind *kind, int32_t speed, int mode) {
    const AosProbes *p = buer_probes(kind);
    int32_t x0 = b->x;
    int r = p ? ground_walk(b, soma, layer, p, speed, mode) : 0;
    if (room_outside(b, layer, b->spin < 0 ? -0x18 : 0x18, 0)) b->x = x0;
    return r;
}

/* The fire's facing (+ 0x58 bit 6 of the linked sprite). */
static void buer_fire_face(AosEnemy *b, bool right) { b->buer_fire_mirrored = right; }

/* sub_080CC20C(e, rolling): the walk (mode 2 rolling, else 6) at the spin
 * + 0x18, then on a downhill slope (0x40) the spin loses 0x333 of its
 * magnitude, uphill (0x20) gains it, otherwise it decays by 0x40 to at
 * least 0x4000; the affine angle + 0x14 turns by spin / 64; a wall, or no
 * floor while not rolling, sends it back at half the spin; the fire faces
 * the spin. */
static void buer_roll(AosEnemy *b, const AosSoma *soma, const AosCollision *layer,
                      const AosEnemyKind *kind, bool rolling) {
    int r = buer_walk(b, soma, layer, kind, b->spin, rolling ? 2 : 6);
    if (r & 0x40) b->spin += b->spin > 0 ? -0x333 : 0x333;
    else if (r & 0x20) b->spin += b->spin > 0 ? 0x333 : -0x333;
    else if (b->spin >= 0) {
        b->spin -= 0x40;
        if (b->spin <= 0x3FFF) b->spin = 0x4000;
    } else {
        b->spin += 0x40;
        if (b->spin > -0x4000) b->spin = -0x4000;
    }
    int32_t s = b->spin;
    b->angle += (uint32_t)((s < 0 ? s + 0x3F : s) >> 6);
    if ((r & 3) || (!rolling && !(r & 8))) b->spin = -s / 2;
    buer_fire_face(b, b->spin >= 0);
}

/* sub_080CC5D8(x, y, animation, flip): a graphics-only fire burst
 * (sub_080CC178: palette 6, the animation once, priority 0x11, still). */
static void buer_burst(const AosEnemy *b, int k, AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy shot = {0};
    shot.id = b->id;
    shot.role = AOS_ROLE_BUER_PROJECTILE;
    shot.static_frame = -1;
    shot.x = AOS_FIXED(b->x >> 16);
    shot.y = AOS_FIXED((int16_t)((b->y >> 16) + 0x18));
    shot.variant = (uint8_t)k;
    shot.mirrored = b->nem_passes != 0;     /* + 0x13: its target to the right */
    shot.combat.attack_off = shot.combat.hurt_off = true;
    report->children[report->child_count++] = shot;
}

/* sub_080CBC64 (state 2, step 1): it takes its target's side (+ 0x13),
 * then for 80 frames spins up by 0xCCC a frame toward it (at most 2.0,
 * which ends the stage at once), the fire facing the spin, turning by
 * 0x400 (0x800 below 51 frames) and, past 50, every third frame two fire
 * bursts (sound 0x84 every ninth); then 32 frames rolling on, spun by the
 * slopes, and again. */
static void buer_attack(AosEnemy *b, const AosSoma *soma, const AosCollision *layer,
                        const AosEnemyKind *kind, AosHitReport *report) {
    switch (b->substep) {
    case 0:
        b->nem_passes = (uint8_t)(player_dx(b, soma) > 0 ? 0 : 1);
        b->timer = 0x50;
        b->substep = 1;
        /* fallthrough */
    case 1: {
        buer_walk(b, soma, layer, kind, b->spin, 6);
        bool right = b->nem_passes != 0;
        if (right) {
            b->spin += 0xCCC;
            if (b->spin > 0x20000) {
                b->spin = 0x20000;
                b->timer = 0;
            }
            if (b->spin > 0) buer_fire_face(b, true);
            b->angle += b->timer > 0x32 ? 0x400u : 0x800u;
        } else {
            b->spin -= 0xCCC;
            if (b->spin < -0x20000) {
                b->spin = -0x20000;
                b->timer = 0;
            }
            if (b->spin < 0) buer_fire_face(b, false);
            b->angle += b->timer > 0x32 ? (uint32_t)-0x400 : (uint32_t)-0x800;
        }
        uint8_t t = b->timer;
        if (t % 3 == 0 && t > 0x32) {
            if (t % 9 == 0) aos_audio_play_song(0x84);
            buer_burst(b, 0, report);
            buer_burst(b, 1, report);
        }
        if (--b->timer == 0xFF) {
            b->substep = 2;
            b->timer = 0x20;
        }
        break;
    }
    case 2: {
        int r = buer_walk(b, soma, layer, kind, b->spin, 6);
        if (r & 0x40) b->spin += b->spin > 0 ? -0x333 : 0x333;
        else if (r & 0x20) b->spin += b->spin > 0 ? 0x333 : -0x333;
        int32_t s = b->spin;
        b->angle += (uint32_t)((s < 0 ? s + 0x3F : s) >> 6);
        if (--b->timer == 0xFF) b->substep = 0;
        break;
    }
    default: break;
    }
}

/* sub_080CBAEC (state 2): step 0 rolls (not rolling mode) and attacks
 * its target inside the 192 x 48 box on the fire's side; every 256 frames
 * an even draw makes it wait (step 2): for 32-63 frames the fire turns
 * round and its target inside the 96 x 48 box starts the attack, then an
 * even draw rolls on with the spin toward the fire's side, an odd one
 * waits again. */
static void buer_idle(AosEnemy *b, const AosSoma *soma, const AosCollision *layer,
                      const AosEnemyKind *kind, uint32_t (*random)(void),
                      AosHitReport *report) {
    switch (b->step) {
    case 0: {
        buer_roll(b, soma, layer, kind, false);
        if (buer_player_in_window(b, soma, 0xC0, 0x30)) {
            int side = b->x < target_x(soma) ? 1 : b->x > target_x(soma) ? -1 : 0;
            if ((!b->buer_fire_mirrored && side == -1) || (b->buer_fire_mirrored && side == 1)) {
                b->step = 1;
                b->substep = 0;
            }
        }
        if (++b->timer == 0xFF) {
            b->timer = 0;
            if (!(random() & 1)) b->step = 2;
            b->substep = 0;
        }
        break;
    }
    case 1: buer_attack(b, soma, layer, kind, report); break;
    case 2:
        if (b->substep == 0) {
            b->timer = (uint8_t)((random() & 0x1F) + 0x20);
            b->substep = 1;
        }
        if (b->substep == 1) {
            b->buer_fire_mirrored = !b->buer_fire_mirrored;
            b->substep = 2;
        }
        if (buer_player_in_window(b, soma, 0x60, 0x30)) {
            b->step = 1;
            b->substep = 0;
        }
        if (--b->timer == 0xFF) {
            if (!(random() & 1)) {
                b->step = b->substep = 0;
                b->timer = 0;
                int32_t magnitude = b->spin < 0 ? -b->spin : b->spin;
                b->spin = b->buer_fire_mirrored ? magnitude : -magnitude;
            } else {
                b->substep = 0;
            }
        }
        break;
    default: break;
    }
}

/* Eight simultaneous sub_0806D578 flames at the ROM's 0x081194DC offsets
 * (exported as `buer_death_offsets`), 4 pixels lower, each mirrored at
 * random. */
static void buer_terminal_flames(const AosEnemy *e, const AosEnemyKind *kind,
                                 uint32_t (*random)(void), AosHitReport *report) {
    if (!kind || !kind->buer_death_offsets_valid) return;
    for (unsigned i = 0; i < 8 && report->effect_count < AOS_ENEMY_MAX_EFFECTS; ++i) {
        AosEffect *flame = &report->effects[report->effect_count++];
        *flame = (AosEffect){
            .kind = AOS_EFFECT_FLAME,
            .x = (int32_t)((uint32_t)e->x + ((uint32_t)kind->buer_death_offsets[i][0] << 16)),
            .y = (int32_t)((uint32_t)e->y + ((uint32_t)kind->buer_death_offsets[i][1] << 16) +
                           0x40000u)};
        flame->mirrored = (random() & 1u) != 0;
    }
}

/* sub_080CBEA4 (state 3, dead): sound 0x6E, no attack; the fire stops
 * following and, for 97 frames, wobbles by a pixel every third frame and
 * moves with the body, flaming every seventh, while the body's spin from
 * 2.0 decays by 0x380 to 0x4000 turning it on the floor walker (speed 0);
 * then the fire goes and for 129 frames the body rolls on with a 32 x 32
 * flame every other frame (0x6E every 20th past 28); then eight flames
 * (sound 0x6E) and it is gone. */
static void buer_die(AosEnemy *b, const AosSoma *soma, const AosCollision *layer,
                     const AosEnemyKind *kind, uint32_t (*random)(void), AosHitReport *report) {
    int32_t x0 = b->x, y0 = b->y;
    switch (b->step) {
    case 0:
        b->combat.attack_off = true;
        aos_audio_play_song(0x6E);
        b->buer_fire_following = false;
        b->timer = 0x60;
        b->step = 1;
        b->spin = b->spin >= 0 ? 0x20000 : -0x20000;
        /* fallthrough */
    case 1:
        if (b->timer % 7 == 0)
            spawn_flame_at(b->buer_fire_x, b->buer_fire_y, 0x100000, 0x100000, random, report);
        if (b->timer % 3 == 0) {
            b->buer_death_linked_wobble = 1;
            b->buer_fire_x += 0x10000;
            b->buer_fire_y += 0x10000;
        } else if (b->buer_death_linked_wobble) {
            b->buer_fire_x -= 0x10000;
            b->buer_fire_y -= 0x10000;
            b->buer_death_linked_wobble = 0;
        }
        if (b->spin >= 0) {
            b->spin -= 0x380;
            if (b->spin <= 0x3FFF) b->spin = 0x4000;
        } else {
            b->spin += 0x380;
            if (b->spin > -0x4000) b->spin = -0x4000;
        }
        {
            int32_t s = b->spin;
            b->angle += (uint32_t)((s < 0 ? s + 0x3F : s) >> 6);
        }
        if (buer_probes(kind)) ground_walk(b, soma, layer, buer_probes(kind), 0, 2);
        b->buer_fire_x += b->x - x0;
        b->buer_fire_y += b->y - y0;
        if (--b->timer == 0xFF) {
            b->buer_fire_gone = true;
            b->timer = 0x80;
            b->step = 2;
        }
        break;
    case 2:
        if ((b->timer & 1) == 0) {
            spawn_flame_at(b->x, b->y, 0x200000, 0x200000, random, report);
            if (b->timer % 20 == 0 && b->timer > 0x1C) aos_audio_play_song(0x6E);
        }
        buer_roll(b, soma, layer, kind, true);
        if (--b->timer == 0xFF) b->step = 3;
        break;
    case 3:
        buer_terminal_flames(b, kind, random, report);
        aos_audio_play_song(0x6E);
        b->removed = true;
        break;
    default: break;
    }
}

/* EnemyBuerUpdate: state 4 (the global 0x8E & 0x40) rises 32 pixels once
 * and spins by 0x100 a frame; state 3 dies; otherwise it enters the homing
 * list, only collides while flashing from a blow, rolls (state 1, until 92
 * pixels below it leave the room: state 2) or idles and attacks (state 2),
 * deleted with its fire once it leaves the room (sub_080CC55C), then the
 * collision pass. The fire (sub_080CBA30) is made by the first update
 * (native: at creation). */
static AosHitReport buer_update(AosEnemy *b, AosSoma *soma, const AosCollision *layer,
                                const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
                                int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    if (!b->helper_spawned) {
        b->helper_spawned = true;
        AosEnemy fire = {0};
        fire.id = b->id;
        fire.role = AOS_ROLE_BUER_FIRE;
        fire.static_frame = 1;
        fire.x = b->x;
        fire.y = b->y;
        fire.mirrored = b->buer_fire_mirrored;
        fire.combat.attack_off = fire.combat.hurt_off = true;
        spawned.children[spawned.child_count++] = fire;
    }
    if (b->state == 4) {
        if (b->step == 0) {
            b->y = (int32_t)((uint32_t)b->y - (32u << 16));
            b->static_frame = 0;
            b->step = 1;
        }
        b->angle += 0x100u;
    } else if (b->state == 3) {
        buer_die(b, soma, layer, kind, random, &spawned);
    } else {
        int sx = (b->x >> 16) - cam_x, sy = (b->y >> 16) - cam_y;
        if (!b->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
            report.targetable = true;
        bool pass = true;
        if (b->hp > 0 && b->hit_flash) {
            b->hit_flash--;
        } else if (room_outside(b, layer, 0, 0)) {
            /* sub_080CC55C: gone with its fire. */
            b->removed = true;
            pass = false;
        } else if (b->state == 1) {
            buer_roll(b, soma, layer, kind, true);
            if (room_outside(b, layer, 0, 0x5C)) {
                b->state = 2;
                b->step = b->substep = 0;
            }
        } else if (b->state == 2) {
            buer_idle(b, soma, layer, kind, random, &spawned);
        }
        if (pass && b->state != 3) {
            AosHitReport hit = collide(b, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
            bool targetable = report.targetable;
            report = hit;
            report.targetable |= targetable;
        }
        aos_combat_tick(&b->combat);
    }
    if (b->buer_fire_following) {
        b->buer_fire_x = b->x;
        b->buer_fire_y = b->y;
    }
    for (int c = 0; c < spawned.child_count && report.child_count < AOS_ENEMY_MAX_CHILDREN; ++c)
        report.children[report.child_count++] = spawned.children[c];
    take_effects(&report, &spawned);
    return report;
}

/* sub_080CC3C8: the fire shows the second frame where its body keeps it,
 * with the body's fire facing; gone with it. */
static void buer_fire_update(AosEnemy *fire) {
    const AosEnemy *b = fire->parent;
    if (!b || b->removed || b->buer_fire_gone) {
        fire->removed = true;
        return;
    }
    fire->x = b->buer_fire_x;
    fire->y = b->buer_fire_y;
    fire->mirrored = b->buer_fire_mirrored;
    fire->hidden = b->hidden;
}

/* sub_080CC178: a fire burst plays its animation once in place. */
static void buer_burst_update(AosEnemy *shot, const AosEnemyKind *kind) {
    if (shot->state == 0) {
        if (!kind->anims || shot->variant >= kind->anims->count) {
            shot->removed = true;
            return;
        }
        play_loop(shot, kind->anims, shot->variant, false);
        shot->state = 1;
    }
    if (aos_anim_step(&shot->anim, kind->anims) == 3) shot->removed = true;
}

/* White Dragon fields: the head's two attack countdowns (+ 0x10 / + 0x12,
 * then its target), its home (the neck's root, + 0x14 + + 0x18 / + 0x16 +
 * + 0x1A in room pixels), the swing amplitude + 0x1C, the wobble phase
 * + 0x1E, the swing angle + 0x48 and its rate + 0x4C, the heading + 0x4A,
 * the neck length + 0x4E, the breath angle + 0x52 and its sweep + 0x50,
 * and the 100 past swing angles (its 0x32-word allocation). */

/* A 16.16 product shifted down 16, rounded toward zero. */
static int32_t trunc16(int32_t v) { return v < 0 ? -((-v) >> 16) : v >> 16; }

/* The angle of a neck point: heading + sin(swing) * amplitude / 4096 +
 * sin(phase) / 16 (both rounded toward zero). */
static int32_t dragon_point_angle(const AosEnemy *d, uint16_t swing) {
    int32_t a = aos_sine(swing) * d->wd_swing;
    a = (a < 0 ? a + 0xFFF : a) >> 12;
    int32_t b = aos_sine(d->wd_phase);
    b = (b < 0 ? b + 0xF : b) >> 4;
    return (int16_t)d->wd_heading + a + b;
}

/* sub_08077080 (the head's draw callback): the head at its neck length
 * from the root at the current swing. */
static void dragon_place_head(AosEnemy *d) {
    int32_t angle = dragon_point_angle(d, d->wd_angle);
    d->x = AOS_FIXED(d->wd_home_x + trunc16(aos_sine((uint32_t)(angle + 0x4000)) * d->wd_radius));
    d->y = AOS_FIXED(d->wd_home_y + trunc16(aos_sine((uint32_t)angle) * d->wd_radius));
}

/* sub_080771C8 (a segment's draw callback): segment i at (9 - i) / 10 of
 * the neck, at the swing of (i + 1) * 8 frames ago (6 while dying). */
static void dragon_place_segment(AosEnemy *s, const AosEnemy *d) {
    int i = s->variant, k = d->state == 2 ? 6 : 8, h = (i + 1) * k;
    uint16_t swing = h < 100 ? d->wd_history[h] : 0;
    int32_t angle = dragon_point_angle(d, swing);
    int32_t dx = aos_sine((uint32_t)(angle + 0x4000)) * d->wd_radius * (9 - i) / 10;
    int32_t dy = aos_sine((uint32_t)angle) * d->wd_radius * (9 - i) / 10;
    s->x = AOS_FIXED(d->wd_home_x + trunc16(dx));
    s->y = AOS_FIXED(d->wd_home_y + trunc16(dy));
}

static void dragon_record(AosEnemy *d) {
    memmove(&d->wd_history[1], &d->wd_history[0], sizeof d->wd_history[0] * 99);
    d->wd_history[0] = d->wd_angle;
}

/* The heading eases by 0x40 toward 0 (facing right) or 0x8000. */
static void dragon_settle_heading(AosEnemy *d) {
    uint32_t h = d->wd_heading;
    if (d->mirrored) {
        if (h <= 0x7FFF) h += 0x10000;
        if (h <= 0xFFFF) {
            h += 0x40;
            if (h > 0x10000) h = 0x10000;
        } else {
            h -= 0x40;
            if (h <= 0x10000) h = 0x10000;
        }
    } else if (h <= 0x7FFF) {
        h += 0x40;
        if (h > 0x8000) h = 0x8000;
    } else {
        h -= 0x40;
        if (h <= 0x8000) h = 0x8000;
    }
    d->wd_heading = (uint16_t)h;
}

/* The wobble phase advances by 0x140 while away from the half turns. */
static bool dragon_wobble(AosEnemy *d) {
    uint16_t p = d->wd_phase;
    if (p > 0x8000 ? p <= 0xFEAF : p <= 0x7EAF) {
        d->wd_phase = (uint16_t)(p + 0x140);
        return true;
    }
    return false;
}

/* sub_080777F4(e, x, y): a flame (sprite frame 9) within 6 pixels of
 * (x, y) on the side the head faces, 3 up to 2 down, flickering its flips
 * for 10 frames (sub_08077BB0). */
static void dragon_flame(const AosEnemy *d, int32_t x, int32_t y, uint32_t (*random)(void),
                         AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy f = {0};
    f.id = d->id;
    f.role = AOS_ROLE_DRAGON_FLAME;
    f.static_frame = 9;
    f.mirrored = random() & 1;
    int32_t dx = (int32_t)(random() % 6);
    f.x = x + AOS_FIXED(d->mirrored ? dx : -dx);
    f.y = y + AOS_FIXED((int32_t)(random() % 6) - 3);
    f.combat.attack_off = f.combat.hurt_off = true;
    report->children[report->child_count++] = f;
}

/* sub_08077A88(e, x, y, frame): a falling piece, 0.25 either way (r &
 * 0x7FFF) - 0.25, gravity 0x1000 (7.0 cap), blinking out over 45 frames. */
static void dragon_piece(const AosEnemy *from, int32_t x, int32_t y, int frame,
                         uint32_t (*random)(void), AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy p = {0};
    p.id = from->id;
    p.role = AOS_ROLE_DRAGON_PIECE;
    p.static_frame = (int16_t)frame;
    p.x = x;
    p.y = y;
    p.mirrored = from->mirrored;
    p.vx = (int32_t)(random() & 0x7FFF) - 0x4000;
    p.ay = 0x1000;
    p.timer = 0x2C;
    p.combat.attack_off = p.combat.hurt_off = true;
    report->children[report->child_count++] = p;
}

/* The target's room position. */
static int dragon_tx(const AosSoma *soma) { return target_x(soma) >> 16; }
static int dragon_ty(const AosSoma *soma) { return target_y(soma) >> 16; }

/* sub_08076824 (state 0, also the inert state 4): the neck eases to 72
 * pixels and swings (rate easing to 0x400, amplitude to 0x80, wobble by
 * 0x140), the heading settles; then two countdowns: the first may start a
 * bite at a target in the 84 x 200 box 40 pixels ahead of its root, the
 * second a breath (fire or fireballs) at one in the 80 x 200 box ahead of
 * its head. */
static void dragon_idle(AosEnemy *d, const AosSoma *soma, uint32_t (*random)(void)) {
    if (d->step != 0) return;
    if (d->wd_radius <= 0x47) d->wd_radius++;
    else if (d->wd_radius > 0x48) d->wd_radius--;
    d->wd_phase = (uint16_t)(d->wd_phase + 0x140);
    if (d->wd_rate <= 0x3FF) d->wd_rate = (uint16_t)(d->wd_rate + 0x10);
    else if (d->wd_rate > 0x400) d->wd_rate = (uint16_t)(d->wd_rate - 0x10);
    d->wd_angle = (uint16_t)(d->wd_angle + d->wd_rate);
    dragon_record(d);
    if (d->wd_swing <= 0x7F) d->wd_swing++;
    else if (d->wd_swing > 0x80) d->wd_swing--;
    dragon_settle_heading(d);
    if (d->state == 4) return;
    int x = d->x >> 16, y = d->y >> 16;
    if (d->wd_cool_a) {
        d->wd_cool_a--;
    } else if ((random() & 0xFF) <= 0x40) {
        int bx = d->mirrored ? d->wd_home_x + 0x28 : d->wd_home_x - 0x7C;
        if (player_near(soma, bx, y - 0x64, 0x54, 0xC8)) {
            d->state = 1;
            d->step = 0;
            d->timer = 0;
            d->wd_cool_a = (int16_t)dragon_tx(soma);
            d->wd_cool_b = (int16_t)(dragon_ty(soma) - 0x14);
            return;
        }
    } else {
        d->wd_cool_a = (int16_t)(random() % 0x1E);
    }
    if (d->wd_cool_b) {
        d->wd_cool_b--;
    } else if ((random() & 0xFF) <= 0x40) {
        if (player_near(soma, d->mirrored ? x : x - 0x50, y - 0x64, 0x50, 0xC8)) {
            d->state = 1;
            d->step = (random() & 1) ? 0x14 : 0x0A;
            d->timer = 0;
            d->wd_cool_a = (int16_t)dragon_tx(soma);
            d->wd_cool_b = (int16_t)(dragon_ty(soma) - 0x14);
            d->wd_aim = aos_arctan2((int16_t)(dragon_tx(soma) - x),
                                    (int16_t)(dragon_ty(soma) - (y + 0x14)));
        }
    } else {
        d->wd_cool_b = (int16_t)(random() % 0x1E + 0x1E);
    }
}

/* Back to state 0 with new countdowns (60-119, 0-59 frames). */
static void dragon_rest(AosEnemy *d, uint32_t (*random)(void)) {
    d->state = d->step = 0;
    d->wd_cool_a = (int16_t)(random() % 0x3C + 0x3C);
    d->wd_cool_b = (int16_t)(random() % 0x3C);
}

/* sub_08077554: a fireball (sub_08077644, sprite frame 5) from the head at
 * 4.0 along the breath angle; contact element 2 with a knockback. */
static void dragon_fireball(const AosEnemy *d, AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy b = {0};
    b.id = d->id;
    b.role = AOS_ROLE_DRAGON_FIREBALL;
    b.static_frame = 5;
    b.x = d->x;
    b.y = d->y;
    b.mirrored = d->mirrored;
    b.vx = fixed_mul(aos_sine((uint32_t)((int16_t)d->wd_aim + 0x4000)), 0x40000);
    b.vy = fixed_mul(aos_sine((uint32_t)(int16_t)d->wd_aim), 0x40000);
    b.stats = d->stats;
    b.hp = 1;
    b.combat.type = AOS_TYPE_ENEMY;
    b.combat.hurt_off = true;
    b.own_boxes = true;
    b.own_hurt = b.own_attack = (AosBox){-4, -4, 8, 8};
    b.contact_elements = 2;
    report->children[report->child_count++] = b;
}

/* sub_08076B34 (state 1). The bite (step 0): the neck draws in to 32
 * pixels, the swing speeds to 0x800 and its amplitude falls, the heading
 * turns by 0x80 toward the target; after 40 frames, once all are settled,
 * it lunges (sound 0x87): the neck grows by 4 to 100 for 40 frames. The
 * breaths (steps 10 / 20): 60 frames of flames at the mouth (sound 0x77,
 * frame 1; step 20 also draws in sparks), one frame opening (frame 2,
 * step 22 choosing its sweep), then 60 frames of a fireball every fourth
 * of the first 30 (0x77), step 22 sweeping by 0x300. */
static void dragon_attack(AosEnemy *d, const AosSoma *soma, uint32_t (*random)(void),
                          AosHitReport *report) {
    switch (d->step) {
    case 0: {
        int busy = 0;
        if (d->wd_radius > 0x20) {
            d->wd_radius--;
            busy = 1;
        }
        if (d->wd_rate <= 0x7FF) {
            d->wd_rate = 0x800;
            busy++;
        }
        d->wd_angle = (uint16_t)(d->wd_angle + d->wd_rate);
        dragon_record(d);
        if (d->wd_swing > 0) {
            d->wd_swing -= 2;
            busy++;
            if (d->wd_swing < 0) d->wd_swing = 0;
        }
        if (dragon_wobble(d)) busy++;
        uint32_t want = aos_arctan2((int16_t)(d->wd_cool_a - d->wd_home_x),
                                    (int16_t)(d->wd_cool_b - d->wd_home_y));
        uint32_t h = d->wd_heading;
        if (d->mirrored) {
            if (h <= 0x7FFF) h += 0x10000;
            if (want <= 0x7FFF) want += 0x10000;
        }
        if (h < want) {
            busy++;
            h += 0x80;
            if (h > want) h = want;
        } else if (h > want) {
            busy++;
            h -= 0x80;
            if (h < want) h = want;
        }
        d->wd_heading = (uint16_t)h;
        if (d->timer <= 0x27) {
            d->timer++;
            break;
        }
        if (busy) break;
        d->step = 1;
        d->timer = 0;
        aos_audio_play_song(0x87);
        break;
    }
    case 1:
        if (d->wd_radius <= 0x63) d->wd_radius += 4;
        if (d->timer++ > 0x27) dragon_rest(d, random);
        break;
    case 10:
    case 20:
        if (d->timer == 0) {
            d->static_frame = 1;
            aos_audio_play_song(0x77);
        }
        if (d->timer <= 0x1D ? (d->timer & 7) == 0 : (d->timer & 3) == 0)
            dragon_flame(d, d->x, d->y, random, report);
        if (d->step == 20 && (d->timer & 7) == 2 &&
            report->child_count < AOS_ENEMY_MAX_CHILDREN) {
            /* sub_080778D4: a spark 16-23 pixels ahead, -16..15 up or
             * down, drawn into the mouth over 8 frames (sub_080779E4). */
            AosEnemy s = {0};
            s.id = d->id;
            s.role = AOS_ROLE_DRAGON_SPARK;
            s.static_frame = 9;
            s.mirrored = random() & 1;
            int32_t dx = (int32_t)(random() & 7) + 0x10;
            s.x = d->x + AOS_FIXED(d->mirrored ? dx : -dx);
            s.y = d->y + AOS_FIXED((int32_t)(random() & 0x1F) - 0x10);
            s.target_x = d->x;
            s.target_y = d->y;
            s.combat.attack_off = s.combat.hurt_off = true;
            report->children[report->child_count++] = s;
        }
        if (d->timer++ > 0x3B) {
            d->step++;
            d->timer = 0;
        }
        break;
    case 11:
    case 21:
        if (d->timer == 0) {
            d->static_frame = 2;
            aos_audio_play_song(0x77);
        }
        if (++d->timer == 0) break;
        d->step++;
        d->timer = 0;
        if (d->step == 0x16) {
            uint32_t now = aos_arctan2((int16_t)(dragon_tx(soma) - (d->x >> 16)),
                                       (int16_t)(dragon_ty(soma) - ((d->y >> 16) + 0x14)));
            uint32_t was = d->wd_aim;
            if (now <= 0x3FFF) now += 0x10000;
            if (was <= 0x3FFF) was += 0x10000;
            d->wd_sweep_up = now > was;
        }
        break;
    case 12:
    case 22: {
        uint8_t t = ++d->timer;
        if ((uint8_t)(t - 1) > 0x3B) {
            dragon_rest(d, random);
            d->static_frame = 0;
            break;
        }
        if (t <= 0x1D && (t & 3) == 1) {
            dragon_fireball(d, report);
            aos_audio_play_song(0x77);
            if (d->step == 0x16)
                d->wd_aim = (uint16_t)(d->wd_aim + (d->wd_sweep_up ? 0x300 : -0x300));
        }
        break;
    }
    default: break;
    }
}

/* sub_08076ED8 (state 3, struck by the weapon while idle): 30 frames of
 * the neck shrinking by 2 to 48 and the swing amplitude growing by 2 to
 * 0x100 (frame 1), then state 0 (frame 0). */
static void dragon_recoil(AosEnemy *d) {
    if (d->wd_radius > 0x30) d->wd_radius -= 2;
    if (d->wd_swing <= 0xFF) d->wd_swing += 2;
    if (d->timer++ > 0x1D) {
        d->state = d->step = d->timer = 0;
        d->static_frame = 0;
    }
}

/* sub_08076F20 (state 2, dead, for 120 frames): the neck eases to 48, the
 * swing amplitude grows to 0x100 and its rate to 0x800, a flame at the head
 * every 4 frames and 0x6E every 16; then (step 1) the head goes (0x6B, two
 * pieces 8 pixels behind it, a flame) and its segments fall apart one by
 * one; gone after 50 frames. */
static void dragon_die(AosEnemy *d, uint32_t (*random)(void), AosHitReport *report) {
    if (d->step == 0) {
        if (d->wd_radius <= 0x2F) d->wd_radius++;
        else if (d->wd_radius > 0x30) d->wd_radius--;
        if (d->wd_swing > 0x100) d->wd_swing--;
        else if (d->wd_swing <= 0xFF) d->wd_swing += 4;
        if (d->wd_rate <= 0x7FF) d->wd_rate = (uint16_t)(d->wd_rate + 0x20);
        d->wd_angle = (uint16_t)(d->wd_angle + d->wd_rate);
        dragon_wobble(d);
        dragon_record(d);
        dragon_settle_heading(d);
        if ((d->timer & 3) == 0) dragon_flame(d, d->x, d->y, random, report);
        if ((d->timer & 0xF) == 0) aos_audio_play_song(0x6E);
        if ((int8_t)--d->timer > 0) return;
        d->hidden = true;
        d->step = 1;
        d->timer = 0;
        int32_t off = d->mirrored ? -0x80000 : 0x80000;
        aos_audio_play_song(0x6B);
        dragon_piece(d, d->x + off, d->y, 10, random, report);
        dragon_piece(d, d->x + off, d->y, 11, random, report);
        dragon_flame(d, d->x, d->y, random, report);
        return;
    }
    if (d->timer++ > 0x31) d->removed = true;
}

/* sub_080773AC (the head's hit callback, after the damage and the flash):
 * dead, sound 0x6E, 120 frames of state 2 (frame 1); struck by the weapon
 * (attacker type 3 or 4) while idle, the recoil (state 3, frame 1). */
static void dragon_struck(AosEnemy *d, uint8_t attacker) {
    if (d->hp <= 0) {
        aos_audio_play_song(0x6E);
        d->timer = 0x78;
        d->state = 2;
        d->step = 0;
        d->static_frame = 1;
    } else if (d->state == 0 && (uint8_t)(attacker - 3) <= 1) {
        d->state = 3;
        d->static_frame = 1;
        d->timer = 0;
    }
}

/* EnemyWhiteDragonUpdate: in the homing list on screen, its flash counted
 * down (not while dying); flashing (outside states 2 and 3) it only
 * collides; state 0 idles, 1 attacks, 2 dies, 3 recoils, 4 (inert) only
 * idles; then the collision pass (not in state 4) and the draw callback
 * placing the head. The ten segments are made by its first update
 * (native: at creation). */
static AosHitReport dragon_update(AosEnemy *d, AosSoma *soma, const AosEnemyKind *kind,
                                  const AosWeaponEntity *weapon,
                                  const AosWeaponFrames *weapon_frames, int soma_atk,
                                  int soma_def, int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    if (!d->helper_spawned) {
        d->helper_spawned = true;
        for (int i = 9; i >= 0 && spawned.child_count < AOS_ENEMY_MAX_CHILDREN; --i) {
            AosEnemy s = {0};
            s.id = d->id;
            s.role = AOS_ROLE_WHITE_DRAGON_PART;
            s.static_frame = -1;
            s.variant = (uint8_t)i;
            s.x = d->x;
            s.y = d->y;
            s.combat.attack_off = s.combat.hurt_off = true;
            play(&s, kind->anims, 1);
            step_anim(&s, kind->anims);
            spawned.children[spawned.child_count++] = s;
        }
        d->spawned_count = spawned.child_count;
    } else if (d->spawned_count < 10 && spawned.child_count < AOS_ENEMY_MAX_CHILDREN) {
        /* The remaining segments, one report later (eight per report). */
        for (int i = 9 - d->spawned_count; i >= 0 && spawned.child_count < AOS_ENEMY_MAX_CHILDREN;
             --i) {
            AosEnemy s = {0};
            s.id = d->id;
            s.role = AOS_ROLE_WHITE_DRAGON_PART;
            s.static_frame = -1;
            s.variant = (uint8_t)i;
            s.x = d->x;
            s.y = d->y;
            s.combat.attack_off = s.combat.hurt_off = true;
            play(&s, kind->anims, 1);
            step_anim(&s, kind->anims);
            spawned.children[spawned.child_count++] = s;
            d->spawned_count++;
        }
    }
    int sx = (d->x >> 16) - cam_x, sy = (d->y >> 16) - cam_y;
    if (d->state != 2) {
        if (!d->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
            report.targetable = true;
        if (d->hit_flash) d->hit_flash--;
    }
    bool hold = d->hit_flash && d->state != 3 && d->state != 2;
    if (!hold) {
        switch (d->state) {
        case 0:
        case 4: dragon_idle(d, soma, random); break;
        case 1: dragon_attack(d, soma, random, &spawned); break;
        case 2: dragon_die(d, random, &spawned); break;
        case 3: dragon_recoil(d); break;
        default: break;
        }
    }
    if (d->state != 4 && !d->removed && !d->defeated) {
        bool targetable = report.targetable;
        AosHitReport hit = collide(d, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        copy_collision_result(&report, &hit);
        report.targetable = targetable;
    }
    aos_combat_tick(&d->combat);
    if (!(d->state == 2 && d->step == 1)) dragon_place_head(d);
    for (int c = 0; c < spawned.child_count && report.child_count < AOS_ENEMY_MAX_CHILDREN; ++c)
        report.children[report.child_count++] = spawned.children[c];
    take_effects(&report, &spawned);
    return report;
}

/* sub_08077458: a segment shares its head's visibility; with the head dead
 * (state 2) segments 0-5 flame every 8 frames for the first (10 - i) * 8,
 * and once the head is gone (step 1) they show again and, every four
 * frames, the next one falls apart into a piece (frame 3); it animates
 * unless the head flashes; sub_080771C8 places it. */
static void dragon_segment_update(AosEnemy *s, const AosEnemyKind *kind,
                                  uint32_t (*random)(void), AosHitReport *report) {
    const AosEnemy *d = s->parent;
    if (!d || d->removed || d->id != AOS_ENEMY_WHITE_DRAGON) {
        s->removed = true;
        return;
    }
    s->hidden = d->hidden;
    int i = s->variant;
    if (d->state == 2 && d->step == 1) {
        s->hidden = false;
        if (i <= 5 && (d->timer & 7) == 0) dragon_flame(d, s->x, s->y, random, report);
        if ((d->timer >> 2) == i + 1) {
            dragon_piece(s, s->x, s->y, 3, random, report);
            s->removed = true;
            return;
        }
    }
    if (d->state == 2 && d->step == 0 && i <= 5 && d->timer < (10 - i) * 8 &&
        (d->timer & 7) == 0)
        dragon_flame(d, s->x, s->y, random, report);
    if (!(d->hp > 0 && d->hit_flash)) step_anim(s, kind->anims);
    dragon_place_segment(s, d);
}

/* sub_08077644: a fireball moves for 6 frames (frame 5), grows for 6
 * (frame 6, box (-5, -5, 10, 10)), slows by 1/6 for 14 (frame 7, box (-8,
 * -8, 16, 16)), fades for 4 (frame 8, the small box) and 4 more (frame 9,
 * no collision); its flips flicker at random. */
static AosHitReport dragon_fireball_update(AosEnemy *b, AosSoma *soma, const AosEnemyKind *kind,
                                           int soma_def, uint32_t (*random)(void)) {
    AosHitReport report = {0};
    b->mirrored = random() & 1;
    b->vflip = random() & 1;
    switch (b->state) {
    case 0:
    case 1:
        b->x += b->vx;
        b->y += b->vy;
        if (b->timer++ > 5) {
            b->timer = 0;
            b->static_frame++;
            b->own_hurt = b->own_attack =
                b->state == 0 ? (AosBox){-5, -5, 10, 10} : (AosBox){-8, -8, 16, 16};
            b->state++;
        }
        break;
    case 2:
        b->vx = b->vx * 5 / 6;
        b->vy = b->vy * 5 / 6;
        b->x += b->vx;
        b->y += b->vy;
        if (b->timer++ > 0xD) {
            b->timer = 0;
            b->static_frame++;
            b->own_hurt = b->own_attack = (AosBox){-5, -5, 10, 10};
            b->state = 3;
        }
        break;
    case 3:
        if (b->timer++ > 3) {
            b->timer = 0;
            b->static_frame++;
            b->state = 4;
        }
        break;
    default:
        if (b->timer++ > 3) b->removed = true;
        return report;
    }
    report = collide(b, soma, kind, NULL, NULL, 0, soma_def);
    aos_combat_tick(&b->combat);
    return report;
}


/* Bone Pillar (EnemyBonePillarCreate / EnemyBonePillarUpdate, sharing
 * sub_080B3BAC / sub_080B3DCC with the Fish Head): an unseen controller
 * and + 0x30 (1-8) skulls stacked 22 pixels apart, facing alternately,
 * each its own enemy (stats, hit points, rewards). */

/* sub_080B3A68: a skull falls (gravity 0x1000) onto the next live skull
 * below it, or the controller, 22 pixels above it. */
static void pillar_settle(AosEnemy *s, const AosEnemy *base, const AosEnemy *below) {
    int floor_y = (below ? (below->y >> 16) : (base->y >> 16)) - 0x16;
    s->y += s->vy;
    if ((int16_t)(s->y >> 16) >= floor_y) {
        s->y = AOS_FIXED(floor_y) | (s->y & 0xFFFF);
        s->vy = 0x1000;
    }
    s->vy += 0x1000;
}

/* sub_080B3954 (state 2): sound 0x6B, three falling pieces (sprite frames
 * 0x17 to 0x15 of 0x081193DC's entry; sub_080B3EE8: 39/40 of vx, gravity
 * 0x2000, gone beyond screen margin 4), then 9 hidden frames. */
static bool pillar_shatter(AosEnemy *s, uint32_t (*random)(void), AosHitReport *report) {
    if (s->step == 0) {
        aos_audio_play_song(0x6B);
        for (int frame = 0x17; frame >= 0x15 && report->child_count < AOS_ENEMY_MAX_CHILDREN;
             --frame) {
            AosEnemy p = {0};
            p.id = s->id;
            p.role = AOS_ROLE_PILLAR_PIECE;
            p.static_frame = (int16_t)frame;
            p.x = AOS_FIXED(s->x >> 16);
            p.y = AOS_FIXED(s->y >> 16);
            p.vx = random_centred(random, 16) << 13;
            p.vy = (random_centred(random, 16) << 12) - 0x30000;
            p.ay = 0x2000;
            p.mirrored = s->mirrored;
            p.combat.attack_off = p.combat.hurt_off = true;
            report->children[report->child_count++] = p;
        }
        s->timer = 8;
        s->step = 1;
        return false;
    }
    s->hidden = true;
    if (s->timer == 0) return true;
    s->timer--;
    return false;
}

/* sub_080B33C8: the fire breath (animation 4 once) 4 pixels ahead and 10
 * up; sound 0x6E; its box (-50, -8, 50, 16) of 0x1032F8CE attacks from its
 * frame 4 (contact sub_080B3EB0: element 2, a knockback); gone when the
 * animation ends or its skull dies. */
static AosEnemy pillar_breath(const AosEnemy *s) {
    AosEnemy b = {0};
    b.id = s->id;
    b.role = AOS_ROLE_PILLAR_BREATH;
    b.static_frame = -1;
    b.x = AOS_FIXED((s->x >> 16) + (s->mirrored ? 4 : -4));
    b.y = AOS_FIXED((s->y >> 16) - 10);
    b.mirrored = s->mirrored;
    b.translucent = true;
    b.stats = s->stats;
    b.hp = 1;
    b.combat.type = AOS_TYPE_ENEMY;
    b.combat.attack_off = b.combat.hurt_off = true;
    b.own_boxes = true;
    b.own_hurt = b.own_attack = (AosBox){-50, -8, 50, 16};
    b.contact_elements = 2;
    return b;
}

/* sub_080B350C: the fireball (animation 5 looping) 8 pixels behind and 12
 * up, at 2.0 ahead; sound 0x7A; one hit point, type 0xA, both boxes
 * (-4, -4, 8, 8), contact element 2 with a knockback; struck it bursts into
 * a flame; every eighth frame a smoke puff; gone beyond screen margin 1. */
static AosEnemy pillar_fireball(const AosEnemy *s) {
    AosEnemy b = {0};
    b.id = s->id;
    b.role = AOS_ROLE_PILLAR_FIREBALL;
    b.static_frame = -1;
    b.mirrored = s->mirrored;
    b.x = AOS_FIXED((s->x >> 16) + (s->mirrored ? 8 : -8));
    b.y = AOS_FIXED((s->y >> 16) - 12);
    b.vx = s->mirrored ? 0x20000 : -0x20000;
    b.stats = s->stats;
    b.hp = 1;
    b.combat.type = 0x0A;
    b.own_boxes = true;
    b.own_hurt = b.own_attack = (AosBox){-4, -4, 8, 8};
    b.contact_elements = 2;
    return b;
}

/* sub_080B32F4: a smoke puff (sprite frame 0x14), translucent, both flips
 * at random, 16-31 frames of the accelerated move (sub_0806D3D8, 8.0). */
static void pillar_smoke(const AosEnemy *from, int32_t x, int32_t y, int32_t vx, int32_t vy,
                         int32_t ax, int32_t ay, uint32_t (*random)(void),
                         AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy p = {0};
    p.id = from->id;
    p.role = AOS_ROLE_PILLAR_SMOKE;
    p.static_frame = 0x14;
    p.x = x;
    p.y = y;
    p.vx = vx;
    p.vy = vy;
    p.ax = ax;
    p.ay = ay;
    p.translucent = true;
    p.mirrored = random() & 1;
    p.vflip = random() & 1;
    p.timer = (uint8_t)((random() & 0xF) + 0x10);
    p.combat.attack_off = p.combat.hurt_off = true;
    report->children[report->child_count++] = p;
}

/* sub_080B36A0 (state 1): it opens its jaw (animation 1); then (animation
 * 3) an even skull breathes fire at frame 2 (three smoke puffs 24, 36 and
 * 48 pixels ahead at its tick 0x28), an odd one spits a fireball; 32
 * frames later, facing away from its target, it turns (animation 2) and
 * rests, else it rests 128 frames (animation 0). */
static void pillar_attack(AosEnemy *s, const AosSoma *soma, const AosAnimSet *anims,
                          uint32_t (*random)(void), AosHitReport *report) {
    switch (s->step) {
    case 0:
        play_loop(s, anims, 1, false);
        s->step = 1;
        break;
    case 1:
        if (!(s->anim.flags & AOS_ANIM_ENDED)) break;
        play_loop(s, anims, 3, false);
        s->step = (s->variant & 1) ? 3 : 2;
        break;
    case 2:
    case 3:
        if (s->anim.frame == 2 && s->anim.tick == 0 &&
            report->child_count < AOS_ENEMY_MAX_CHILDREN) {
            report->children[report->child_count++] =
                s->step == 2 ? pillar_breath(s) : pillar_fireball(s);
            aos_audio_play_song(s->step == 2 ? 0x6E : 0x7A);
        }
        if (s->step == 2) {
            if (s->anim.frame == 2 && s->anim.tick == 0x28) {
                int32_t d = s->mirrored ? 12 : -12;
                for (int k = 0, o = 2 * d; k < 3; ++k, o += d) {
                    int32_t x = AOS_FIXED((s->x >> 16) + o + random_centred(random, 16));
                    int32_t y = AOS_FIXED(random_centred(random, 16) - 0x10 + (s->y >> 16));
                    int32_t vy = random_centred(random, 0x4000) - 0x1000;
                    pillar_smoke(s, x, y, 0, vy, 0, -0x800, random, report);
                }
            }
            s->timer++;
        }
        if (s->anim.flags & AOS_ANIM_ENDED) {
            s->timer = 0x20;
            s->step = 4;
        }
        break;
    case 4: {
        if (s->timer) {
            s->timer--;
            break;
        }
        bool right = s->x <= target_x(soma);
        if (s->mirrored != right) {
            play_loop(s, anims, 2, false);
            s->step = 5;
        } else {
            play(s, anims, 0);
            s->step = 6;
            s->timer = 0x80;
        }
        break;
    }
    case 5:
        if (!(s->anim.flags & AOS_ANIM_ENDED)) break;
        s->mirrored = !s->mirrored;
        s->state = s->step = s->substep = 0;
        play(s, anims, 0);
        break;
    case 6:
        if (s->timer) s->timer--;
        else s->state = s->step = s->substep = 0;
        break;
    default: break;
    }
}

/* sub_080B3AEC: a skull does nothing while its controller is outside its
 * window; it settles, only collides while flashing from a blow, waits
 * (state 0) for its target inside the 160 x 120 box around it, attacks
 * (1), shatters (2) or only animates (3, the global 0x8E & 0x40); then the
 * collision pass (not while shattering) and the animation step. */
static AosHitReport pillar_skull_update(AosEnemy *s, AosSoma *soma, const AosEnemyKind *kind,
                                        const AosWeaponEntity *weapon,
                                        const AosWeaponFrames *weapon_frames, int soma_atk,
                                        int soma_def, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    AosEnemy *base = (AosEnemy *)s->parent;
    if (!base || base->removed) {
        s->removed = true;
        return report;
    }
    if (s->state != 2) {
        /* The controller's slot of this skull (+ 0x18 + i). */
        if (s->variant < 8) base->bp_skull[s->variant] = s;
        if (!base->bp_active) return report;
        const AosEnemy *below = NULL;
        for (int v = s->variant - 1; v >= 0 && !below; --v)
            if (base->bp_skull[v] && base->bp_skull[v]->state != 2) below = base->bp_skull[v];
        pillar_settle(s, base, below);
        if (base->bp_top == s->variant) report.targetable = true;
    } else if (s->variant < 8) {
        base->bp_skull[s->variant] = NULL;
    }
    if (s->hp > 0 && s->hit_flash) {
        s->hit_flash--;
        if (s->state != 2) {
            AosHitReport hit = collide(s, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
            skeleton_merge(&report, &hit);
        }
        aos_combat_tick(&s->combat);
        return report;
    }
    switch (s->state) {
    case 0:
        if (s->step == 0 &&
            player_near(soma, (s->x >> 16) - 0x50, (s->y >> 16) - 0x3C, 0xA0, 0x78)) {
            s->state = 1;
            s->step = s->substep = 0;
        }
        break;
    case 1: pillar_attack(s, soma, kind->anims, random, &spawned); break;
    case 2:
        if (pillar_shatter(s, random, &spawned)) s->removed = true;
        report.child_count = spawned.child_count;
        memcpy(report.children, spawned.children, sizeof spawned.children[0] * spawned.child_count);
        return report;
    case 3:
        step_anim(s, kind->anims);
        return report;
    default: break;
    }
    AosHitReport hit = collide(s, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    bool targetable = report.targetable;
    skeleton_merge(&report, &hit);
    report.targetable = targetable;
    aos_combat_tick(&s->combat);
    step_anim(s, kind->anims);
    for (int c = 0; c < spawned.child_count && report.child_count < AOS_ENEMY_MAX_CHILDREN; ++c)
        report.children[report.child_count++] = spawned.children[c];
    return report;
}

/* sub_080B3DCC: the controller marks itself active (+ 0x15) inside the
 * screen box (-32, -32, 0x130, 32 * + 0x30 + 0xE0); it forgets its
 * shattered skulls, puts the topmost live one in the homing list
 * (sub_08021278), and is deleted with none left. Its skulls (EnemyBone
 * PillarCreate's) are made by its first update. */
static AosHitReport pillar_update(AosEnemy *b, const AosEnemyKind *kind, int cam_x, int cam_y) {
    AosHitReport report = {0};
    if (!b->helper_spawned) {
        b->helper_spawned = true;
        int32_t y = (b->y >> 16) - 0x16;    /* the bottom skull's place */
        for (int i = 1; i <= b->param0 && report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i) {
            AosEnemy s = {0};
            s.id = b->id;
            s.role = AOS_ROLE_BONE_PILLAR_SEGMENT;
            s.static_frame = -1;
            s.variant = (uint8_t)(i - 1);
            s.x = b->x;
            s.y = AOS_FIXED(y - (i - 1) * 0x16);
            s.mirrored = (i & 1) ? b->mirrored : !b->mirrored;
            s.stats = b->stats;
            s.hp = b->stats.hp;
            s.combat.type = AOS_TYPE_ENEMY;
            s.own_boxes = true;
            s.own_hurt = s.own_attack = (AosBox){-14, -20, 28, 20};
            if (b->state == 3) {
                s.state = 3;
                s.combat.attack_off = s.combat.hurt_off = true;
            }
            play(&s, kind->anims, 0);
            step_anim(&s, kind->anims);
            report.children[report.child_count++] = s;
        }
        b->spawned_count = report.child_count;
        return report;
    }
    int sx = (b->x >> 16) - cam_x, sy = (b->y >> 16) - cam_y;
    b->bp_active = (uint16_t)(sx + 0x20) <= 0x130 &&
                   (uint16_t)(sy + 0x20) <= (uint16_t)(b->param0 * 0x20 + 0xE0);
    if (!b->bp_active) return report;
    b->bp_top = -1;
    for (int v = 7; v >= 0 && b->bp_top < 0; --v)
        if (b->bp_skull[v] && b->bp_skull[v]->state != 2) b->bp_top = (int8_t)v;
    if (b->bp_top < 0 && b->bp_seen) b->removed = true;
    if (b->bp_top >= 0) b->bp_seen = true;
    return report;
}

/* sub_0807E228(e, x, y, vx, vy, frame): a ghost wisp (sub_08069134 with
 * + 0x18 = 0, + 0x19 = 3: the common still frame 0x40 or 0x41 on OBJ row 6,
 * gravity 0x2000, both flips at random, 200 frames). */
static void ghost_wisp(int32_t x, int32_t y, int32_t vx, int32_t vy, bool second,
                       AosHitReport *report) {
    if (report->effect_count >= AOS_ENEMY_MAX_EFFECTS) return;
    report->effects[report->effect_count++] = (AosEffect){
        .kind = second ? AOS_EFFECT_ROCK_FRAGMENT_1 : AOS_EFFECT_ROCK_FRAGMENT_0,
        .x = x, .y = y, .vx = vx, .vy = vy, .ay = 0x2000, .wisp = true, .timer = 200};
}

/* The Giant Ghost's speed (+ 0x14) rises by 0x1000 to 0xC000 or falls by
 * 0x800 to 0x1000. */
static void giant_speed_up(AosEnemy *g) {
    if (g->speed <= 0xBFFF) g->speed += 0x1000;
}
static void giant_slow_down(AosEnemy *g) {
    if (g->speed > 0x1000) g->speed -= 0x800;
}

/* sub_080694B8 toward the point `above` pixels over its target, optionally
 * reversed (fleeing), then the move (sub_0806D3AC). */
static bool giant_head(AosEnemy *g, const AosSoma *soma, int32_t above, bool flee) {
    bool there = move_toward(g, target_x(soma), target_y(soma) - above, g->speed);
    if (flee) {
        g->vx = -g->vx;
        g->vy = -g->vy;
    }
    g->x += g->vx;
    g->y += g->vy;
    return there;
}

static void giant_flee_start(AosEnemy *g, const AosAnimSet *anims) {
    g->step = 10;
    g->gg_touched = false;
    play(g, anims, 2);
}

/* sub_0807D33C (state 0): hidden off the screen rows; it faces its target
 * and, one time in three with it in the 160 x 160 box around it, charges
 * (state 1); otherwise it drifts 48 pixels over it (speeding up 20 frames,
 * coasting 20, slowing 30); touching Soma (+ 0x21) it flees, up and away,
 * for as long. */
static void giant_drift(AosEnemy *g, const AosSoma *soma, const AosAnimSet *anims, int cam_y,
                        uint32_t (*random)(void)) {
    g->hidden = (uint16_t)((g->y >> 16) - cam_y + 0x1C) > 0xDC;
    switch (g->step) {
    case 0: {
        soldier_face(g, soma);
        uint32_t pick = random() % 3;
        if (pick == 0 &&
            player_near(soma, (g->x >> 16) - 0x50, (g->y >> 16) - 0x50, 0xA0, 0xA0)) {
            g->state = 1;
            g->step = 0;
            g->speed = 0;
            play_loop(g, anims, 0, false);
        } else {
            g->timer = 0;
            g->step = 1;
            g->gg_touched = false;
            play_loop(g, anims, 3, false);
        }
        break;
    }
    case 1:
        if (g->gg_touched) {
            giant_flee_start(g, anims);
            break;
        }
        giant_speed_up(g);
        giant_head(g, soma, 0x300000, false);
        if (g->timer++ > 0x13) {
            g->step = 2;
            g->timer = 0;
        }
        break;
    case 2:
    case 12:
        if (g->gg_touched) {
            if (g->step == 2) giant_flee_start(g, anims);
            else {
                g->step = 10;
                g->gg_touched = false;
            }
            break;
        }
        g->x += g->vx;
        g->y += g->vy;
        if (g->timer++ > 0x13) {
            g->step++;
            g->timer = 0;
            play(g, anims, 0);
        }
        break;
    case 3:
    case 13:
        if (g->gg_touched) {
            if (g->step == 3) giant_flee_start(g, anims);
            else {
                g->step = 10;
                g->gg_touched = false;
            }
            break;
        }
        giant_slow_down(g);
        giant_head(g, soma, 0x300000, g->step == 13);
        if (g->timer++ > 0x1D) g->step = g->timer = 0;
        break;
    case 10:
        g->speed = 0;
        g->step = 11;
        g->timer = 0;
        /* fallthrough */
    case 11:
        giant_speed_up(g);
        move_toward(g, target_x(soma), target_y(soma) - 0x300000, g->speed);
        g->vx = -g->vx;
        g->vy = -g->speed;
        g->x += g->vx;
        g->y += g->vy;
        if (g->timer++ > 0x13) {
            g->step = 12;
            g->timer = 0;
        }
        break;
    default: break;
    }
}

/* sub_0807D65C (state 1): it backs away from a point 16 pixels over its
 * target (speeding 20 frames, coasting 20, slowing 30), faces it for 20,
 * then (sound 0x87, animation 3) swoops at 2.0 to 32 pixels over where it
 * was, drifts 45 frames slowing by 1/8 (animation 4) and rests 30
 * (animation 0). */
static void giant_charge(AosEnemy *g, const AosSoma *soma, const AosAnimSet *anims) {
    switch (g->step) {
    case 0:
        giant_speed_up(g);
        giant_head(g, soma, 0x100000, true);
        if (g->timer++ > 0x13) {
            g->step++;
            g->timer = 0;
        }
        break;
    case 1:
        g->x += g->vx;
        g->y += g->vy;
        if (g->timer++ > 0x13) {
            g->step++;
            g->timer = 0;
        }
        break;
    case 2:
        giant_slow_down(g);
        giant_head(g, soma, 0x100000, true);
        if (g->timer++ > 0x1D) {
            g->step++;
            g->timer = 0;
        }
        break;
    case 3:
        soldier_face(g, soma);
        if (g->timer++ > 0x13) {
            g->step++;
            g->timer = 0;
            aos_audio_play_song(0x87);
            g->target_x = target_x(soma);
            g->target_y = target_y(soma) - 0x100000;
            play_loop(g, anims, 3, false);
        }
        break;
    case 4:
        if (move_toward(g, g->target_x, g->target_y - 0x200000, 0x20000)) {
            g->step++;
            play(g, anims, 4);
            g->timer = 0;
        }
        g->x += g->vx;
        g->y += g->vy;
        break;
    case 5:
        g->vx = g->vx * 7 / 8;
        g->vy = g->vy * 7 / 8;
        g->x += g->vx;
        g->y += g->vy;
        if (g->timer++ > 0x2C) {
            g->step++;
            play_loop(g, anims, 0, false);
            g->timer = 0;
        }
        break;
    case 6:
        if (g->timer++ > 0x1D) {
            g->state = g->step = g->timer = 0;
            g->gg_touched = false;
        }
        break;
    default: break;
    }
}

/* sub_0807D88C (its hit callback, after the damage and the flash): dead,
 * sound 0x6B, animation 5 once, translucent, 90 frames of state 2 and four
 * wisps rising from it. */
static void giant_struck(AosEnemy *g, const AosEnemyKind *kind, uint32_t (*random)(void),
                         AosHitReport *report) {
    if (g->hp > 0) return;
    aos_audio_play_song(0x6B);
    play_loop(g, kind->anims, 5, false);
    g->timer = 0x5A;
    g->state = 2;
    g->step = 0;
    g->translucent = true;
    for (int i = 0; i < 4; ++i) {
        int32_t x = g->x + (int32_t)(random() & 0xFFFFF) - 0x80000;
        int32_t y = g->y - (int32_t)(random() & 0xFFFFF);
        int32_t vx = (int32_t)(random() & 0x1FFFF) - 0x10000;
        int32_t vy = -0x10000 - (int32_t)(random() & 0x1FFFF);
        ghost_wisp(x, y, vx, vy, random() & 1, report);
    }
}

/* EnemyGiantGhostUpdate: inside the activity window of sub_0806CC20 (only
 * colliding while flashing), state 0 drifts, 1 charges, 2 dies (blinking
 * out over 90 frames, a wisp every eighth while more than 40 are left), 3
 * (the global 0x8E & 0x40) only animates; then the animation step and the
 * collision pass. Its three parts (EnemyGiantGhostCreate's children) are
 * made by its first update. */
static AosHitReport giant_update(AosEnemy *g, AosSoma *soma, const AosEnemyKind *kind,
                                 const AosWeaponEntity *weapon,
                                 const AosWeaponFrames *weapon_frames, int soma_atk,
                                 int soma_def, int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    if (!g->helper_spawned) {
        g->helper_spawned = true;
        static const uint8_t anims_of[3] = {6, 8, 7};
        for (int i = 0; i < 3; ++i) {
            AosEnemy p = {0};
            p.id = g->id;
            p.role = AOS_ROLE_GIANT_GHOST_PART;
            p.static_frame = -1;
            p.variant = (uint8_t)i;
            p.x = g->x;
            p.y = g->y;
            p.mirrored = g->mirrored;
            p.combat.attack_off = p.combat.hurt_off = true;
            play(&p, kind->anims, anims_of[i]);
            step_anim(&p, kind->anims);
            if (i == 0) p.translucent = true;
            if (g->state == 3 && i > 0) {
                p.target_x = p.x - 0x200000;
                p.target_y = p.y - 0x280000;
            }
            spawned.children[spawned.child_count++] = p;
        }
    }
    if (g->state == 3) {
        step_anim(g, kind->anims);
        report.child_count = spawned.child_count;
        memcpy(report.children, spawned.children, sizeof spawned.children[0] * spawned.child_count);
        return report;
    }
    int sx = (g->x >> 16) - cam_x, sy = (g->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) {
        report.child_count = spawned.child_count;
        memcpy(report.children, spawned.children, sizeof spawned.children[0] * spawned.child_count);
        return report;
    }
    bool flashing = g->hp > 0 && g->hit_flash;
    if (flashing) {
        g->hit_flash--;
    } else {
        switch (g->state) {
        case 0: giant_drift(g, soma, kind->anims, cam_y, random); break;
        case 1: giant_charge(g, soma, kind->anims); break;
        case 2: {
            g->hidden = g->timer < 40 ? kind->blink[g->timer] & 1 : false;
            if ((int8_t)--g->timer <= 0) g->removed = true;
            if (g->timer > 0x28 && (g->timer & 7) == 1) {
                int32_t x = g->x + (int32_t)(random() & 0x1FFFFF) - 0x100000;
                int32_t y = g->y - (int32_t)(random() & 0x1FFFFF);
                int32_t vx = (int32_t)(random() & 0x7FFF) - 0x4000;
                ghost_wisp(x, y, vx, 0, random() & 1, &spawned);
            }
            break;
        }
        default: break;
        }
        step_anim(g, kind->anims);
    }
    if (!g->defeated && !g->removed) {
        AosHitReport hit = collide(g, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        copy_collision_result(&report, &hit);
        if (hit.soma_hit) g->gg_touched = true;    /* sub_0807E328 */
        if (hit.killed) giant_struck(g, kind, random, &spawned);
        for (int e = 0; e < hit.effect_count && spawned.effect_count < AOS_ENEMY_MAX_EFFECTS; ++e)
            spawned.effects[spawned.effect_count++] = hit.effects[e];
    }
    aos_combat_tick(&g->combat);
    for (int c = 0; c < spawned.child_count && report.child_count < AOS_ENEMY_MAX_CHILDREN; ++c)
        report.children[report.child_count++] = spawned.children[c];
    take_effects(&report, &spawned);
    return report;
}

/* The parts: 0 (sub_0807D9B8) the translucent aura 4 pixels behind and 18
 * up, pulsing its affine scale (+ 0x48 / + 0x4C = sin(+ 0x1C) / 16 +
 * 0x1B000 / 0x1D000, + 0x1C += 0x400 unless the body flashes), an
 * afterimage (sub_0807E074) every fourth frame; dying with the body, it
 * swells (each scale loses 1 / + 0x0D of itself) over 110 frames. 1
 * (sub_0807DBB4) and 2 (sub_0807DE14), the eyes, look at the target from
 * their centres (1, 1 and 4, 4 pixels from the body), 3 x 2 and 2 x 2
 * pixels out (1 pixel behind); under the global 0x8E & 0x40 they look at a
 * point sweeping 60 pixels diagonally. All share the body's visibility and
 * facing and go when it dies. */
static void giant_part_update(AosEnemy *p, const AosSoma *soma, const AosEnemyKind *kind,
                              uint32_t (*random)(void), AosHitReport *report) {
    const AosEnemy *g = p->parent;
    if (p->state == 0 && (!g || g->removed || g->id != AOS_ENEMY_GIANT_GHOST)) {
        p->removed = true;
        return;
    }
    if (p->variant == 0) {
        if (p->state == 0) {
            p->hidden = g->hidden;
            p->mirrored = g->mirrored;
            if (!(g->hp > 0 && g->hit_flash)) {
                p->phase = (uint16_t)(p->phase + 0x400);
                step_anim(p, kind->anims);
                if ((++p->timer & 3) == 0 && report->child_count < AOS_ENEMY_MAX_CHILDREN) {
                    /* sub_0807E074: a 5-frame translucent afterimage. */
                    int32_t dx = (int32_t)(random() & 0x7FFFF) - 0x40000;
                    int32_t dy = (int32_t)(random() & 0x7FFFF) - 0x40000;
                    AosEnemy a = *p;
                    a.role = AOS_ROLE_GIANT_GHOST_TRAIL;
                    a.parent = NULL;
                    a.x = p->x + dx;
                    a.y = p->y + dy;
                    a.timer = 4;
                    a.state = 0;
                    report->children[report->child_count++] = a;
                }
            }
            p->x = AOS_FIXED((g->x >> 16) + (p->mirrored ? -4 : 4)) | (p->x & 0xFFFF);
            p->y = AOS_FIXED((g->y >> 16) - 0x12) | (p->y & 0xFFFF);
            int32_t s = aos_sine(p->phase);
            s = (s < 0 ? s + 0xF : s) >> 4;
            p->affine_sx = s + 0x1B000;
            p->affine_sy = s + 0x1D000;
            if (g->state == 2) {
                p->state = 1;
                p->step = 0;
                p->timer = 0x6E;
            }
            return;
        }
        if (p->step == 0) p->step = 1;
        p->affine_sx -= p->timer ? p->affine_sx / p->timer : 0;
        p->affine_sy -= p->timer ? p->affine_sy / p->timer : 0;
        step_anim(p, kind->anims);
        if ((int8_t)--p->timer <= 0) p->removed = true;
        return;
    }
    p->hidden = g->hidden;
    p->mirrored = g->mirrored;
    bool first = p->variant == 1;
    int32_t eye = first ? 0x10000 : 0x40000;
    int32_t reach_x = first ? 3 : 2;
    int32_t cx, cy, tx, ty;
    if (g->state == 3) {
        int32_t d = p->gg_scan_back ? -0x10000 : 0x10000;
        p->target_x += d;
        p->target_y += d;
        if ((int8_t)++p->gg_scan > 0x3B) {
            p->gg_scan = 0;
            p->gg_scan_back = !p->gg_scan_back;
        }
        cx = g->x - eye;
        cy = g->y - (first ? 0x1E0000 : 0x240000);
        tx = p->target_x;
        ty = p->target_y;
    } else if (!(g->hp > 0 && g->hit_flash)) {
        cx = g->x + (p->mirrored ? eye : -eye);
        cy = g->y - eye;
        tx = target_x(soma);
        ty = target_y(soma);
    } else {
        cx = cy = tx = ty = 0;
    }
    if (g->state == 3 || !(g->hp > 0 && g->hit_flash)) {
        uint16_t a = aos_arctan2((int16_t)trunc16(tx - cx), (int16_t)trunc16(ty - cy));
        p->vx = aos_sine((uint32_t)a + 0x4000) * reach_x + (p->mirrored ? -0x10000 : 0x10000);
        p->vy = aos_sine(a) * 2;
    }
    p->x = g->x + p->vx;
    p->y = g->y + p->vy;
    if (g->state == 2) p->removed = true;
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
    int32_t dx = target_x(soma) - g->x;
    int32_t dy = target_y(soma) - g->y + g->speed -
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
    int16_t away_x = (int16_t)pixel_delta(g->x, target_x(soma));
    int16_t away_y = (int16_t)pixel_delta(g->y + 0x400000, target_y(soma));
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

/* sub_080AFE08(e, mask): the aggro gate of the Minotaur family. The brown
 * Minotaur (+ 0x15 = 0) always passes; the red one rolls
 * RandomNumberGenerator2 & mask (our aos_random: RNG2 falls back to RNG1
 * while gEwramData + 0x4CC bit 6 is clear), then needs + 0x18 bit 1, the
 * player in the centred 0xA0 x 0x78 box or a wall probe 0x50 px ahead on
 * the facing side (sub_08002058 / sub_0800207C = aos_wall_push_left /
 * aos_wall_push_right; a nonzero push means the charge path is blocked,
 * so the gate passes and it only walks). */
static bool minotaur_aggro(const AosEnemy *e, const AosSoma *soma,
                           const AosCollision *layer, int mask,
                           uint32_t (*random)(void)) {
    if (!e->mino_bank) return true;
    if (random() & (uint32_t)mask) return true;
    if (!(e->mino_flags & 2)) return false;
    if (player_in_centred(e, soma, 0xA0, 0x78)) return true;
    int32_t x = (e->x >> 16) + (e->mirrored ? 0x50 : -0x50);
    int push = e->mirrored ? aos_wall_push_left(layer, x, e->y >> 16)
                           : aos_wall_push_right(layer, x, e->y >> 16);
    return push != 0;
}

/* sub_080B0814, state 0: face the target (+ 0x6F, the animation tick,
 * zero faces it twice), then the aggro gate. The close centred
 * 0xC8 x 0x78 box or any damage (sub_0806D54C: hp < record + 0xC) rolls
 * mask 1; the far 0x118 x 0x78 box undamaged rolls mask 0x3F. A failed
 * roll attacks (state 2); a passed close roll walks (state 1) only while
 * + 0x18 bit 1 is set. */
static void minotaur_idle(AosEnemy *e, const AosSoma *soma, const AosCollision *layer,
                          const AosEnemyKind *kind, uint32_t (*random)(void)) {
    (void)kind;
    face_player(e, soma);
    if (e->anim.tick == 0) face_player(e, soma);
    bool damaged = e->hp < e->stats.hp;
    if (player_in_centred(e, soma, 0xC8, 0x78) || damaged) {
        if (!minotaur_aggro(e, soma, layer, 1, random)) {
            e->state = 2;
            e->step = e->substep = 0;
        } else if (e->mino_flags & 2) {
            e->state = 1;
            e->step = e->substep = 0;
        }
        return;
    }
    if ((player_in_centred(e, soma, 0x118, 0x78) || damaged) &&
        !minotaur_aggro(e, soma, layer, 0x3F, random)) {
        e->state = 2;
        e->step = e->substep = 0;
    }
}

/* sub_080B0898, state 1 chase: animation 2 once, walk animation 3 while
 * the player stays outside the centred 0x64 x 0x50 box (the probe walker
 * sub_0806CAF8 at 0x8000, table 0x085286F8, mode 0xD), then animation 4
 * (sound 0x86 at its frame 3, tick 0) and back to idle. */
static void minotaur_chase(AosEnemy *e, const AosSoma *soma, const AosCollision *layer,
                           const AosEnemyKind *kind, uint32_t (*random)(void)) {
    (void)random;
    switch (e->step) {
    case 0:
        play_loop(e, kind->anims, 2, false);
        e->step = 1;
        break;
    case 1:
        if (e->anim.flags & AOS_ANIM_ENDED) {
            play(e, kind->anims, 3);
            e->step = 2;
        }
        break;
    case 2:
        ground_walk(e, soma, layer, &kind->probes, 0x8000, 0xD);
        if (player_in_centred(e, soma, 0x64, 0x50)) {
            play_loop(e, kind->anims, 4, false);
            e->step = 3;
        }
        break;
    case 3:
        if (e->anim.frame == 3 && e->anim.tick == 0) aos_audio_play_song(0x86);
        if (e->anim.flags & AOS_ANIM_ENDED) {
            play(e, kind->anims, 1);
            e->state = 0;
            e->step = e->substep = 0;
        }
        break;
    default:
        break;
    }
}

/* sub_080B095C, state 2 attack: with + 0x18 bit 1 set it clears it, plays
 * animation 7 once and walks the wind-up; otherwise animation 5 loops
 * straight into the charge. The charge (case 3) decelerates from 3.25 by
 * 1.25 per tick (mode 0xC), switches to animation 6 at tick 0x18, then
 * either waits out animation 4 (bit 1 path, setting bit 0) or counts
 * 0x30 ticks of the current animation before animation 1 and idle. */
static void minotaur_attack(AosEnemy *e, const AosSoma *soma, const AosCollision *layer,
                            const AosEnemyKind *kind, AosHitReport *report) {
    switch (e->step) {
    case 0:
        if (e->mino_flags & 2) {
            e->mino_flags &= ~2u;
            play_loop(e, kind->anims, 7, false);
            e->step = 1;
        } else {
            play(e, kind->anims, 5);
            e->step = 3;
            e->timer = 0;
        }
        break;
    case 1:
        if (e->anim.flags & AOS_ANIM_ENDED) {
            e->step = 2;
            e->timer = 8;
        }
        break;
    case 2:
        if (e->timer) {
            e->timer--;
            break;
        }
        play(e, kind->anims, 5);
        e->step = 3;
        /* sub_080458E4(e, 0x28, 0xC, 1) and (e, 0x28, 6, 0) spawn the
         * charge dust entities; approximated by drift dust on the enemy. */
        spawn_drift(AOS_EFFECT_DUST, e->x, e->y, report);
        spawn_drift(AOS_EFFECT_DUST, e->x, e->y, report);
        break;
    case 3: {
        int32_t speed = 0x34000 - (int32_t)e->timer * 0x1400;
        ground_walk(e, soma, layer, &kind->probes, speed, 0xC);
        e->timer++;
        if (e->timer == 0x18) play_loop(e, kind->anims, 6, false);
        if (e->timer > 0x28) {
            if (!(e->mino_flags & 2)) {
                e->step = 4;
                e->timer = 0x30;
            } else {
                e->step = 5;
                play_loop(e, kind->anims, 4, false);
                e->mino_flags |= 1;
            }
        }
        break;
    }
    case 4:
        if (e->timer) {
            e->timer--;
            break;
        }
        play(e, kind->anims, 1);
        e->state = e->step = e->substep = 0;
        break;
    case 5:
        if (e->anim.frame == 3 && e->anim.tick == 0) aos_audio_play_song(0x86);
        if (e->anim.flags & AOS_ANIM_ENDED) {
            play(e, kind->anims, 1);
            e->state = e->step = e->substep = 0;
        }
        break;
    default:
        break;
    }
}

/* sub_080B0BE8, EnemyMinotaurUpdate shared by ids 0x15 and 0x58: the
 * sleep flag, the tracking list, the hit flash, the state machine, the
 * collision pass and the animation step. State 3 is sub_08079294 (its
 * timed particles differ from generic_death's flames) and state 4 is
 * sub_0803F17C alone (no collision, no extra step). */
static AosHitReport minotaur_update(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                                    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                    const AosWeaponFrames *weapon_frames, int soma_atk,
                                    int soma_def, int cam_x, int cam_y,
                                    uint32_t (*random)(void)) {
    AosHitReport report = {0};
    (void)cam_x;
    int sy = (e->y >> 16) - cam_y;
    /* sub_0806E4F8(e, -16): hidden while above the screen top margin. */
    e->hidden = sy < -16;
    /* Inside the margins of sub_0806D128(e, 6), sub_08021924 tracks it;
     * aos_enemy_update's own targetable covers the list. */
    /* sub_0806AE54: the hit flash counts down, not while dying. */
    if (e->state != 3 && e->hit_flash) e->hit_flash--;
    /* sub_0806D4C0's hit-stop gate is not ported (always false). */
    switch (e->state) {
    case 0:
        /* param0 != 0 with gEwramData + 0x4CC bit 0x40 clear calls the
         * channel claim sub_08013CF0(0x10): not ported. */
        minotaur_idle(e, soma, layer, kind, random);
        break;
    case 1:
        minotaur_chase(e, soma, layer, kind, random);
        break;
    case 2:
        minotaur_attack(e, soma, layer, kind, &report);
        break;
    case 3:
        generic_death(e, kind, random, &report);
        break;
    case 4:
        /* sub_0803F17C alone: animation only, deleted at its end. */
        step_anim(e, kind->anims);
        if (e->cycle_done) e->removed = true;
        return report;
    default:
        break;
    }
    /* sub_080421AC: the collision pass, not while dying. */
    if (e->state != 3) {
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames, soma_atk,
                                   soma_def);
        skeleton_merge(&report, &hit);
    }
    /* + 0x14 = the frame byte, then sub_0806DF20. */
    step_anim(e, kind->anims);
    aos_combat_tick(&e->combat);
    return report;
}

/* ---- Giant Worm / Poison Worm (sub_080AD7F0 family) ---- */

/* sub_0806D5C0(x, y, rx, ry, param): a drifting dust effect
 * (EntityCreateInRange(0x49, 0xCF)) at x + RNG % rx - rx/2,
 * y + RNG % ry - ry/2. The + 0x5C - 2 = 0xF parameter selects a
 * sub-animation the shared effect does not model: AOS_EFFECT_DUST
 * stands for it (the Minotaur approximation). */
static void worm_dust(int32_t x, int32_t y, uint32_t rx, uint32_t ry,
                      AosHitReport *report, uint32_t (*random)(void)) {
    if (report->effect_count >= AOS_ENEMY_MAX_EFFECTS) return;
    int32_t ox = (int32_t)(random() % rx) - (int32_t)(rx >> 1);
    int32_t oy = (int32_t)(random() % ry) - (int32_t)(ry >> 1);
    spawn_drift(AOS_EFFECT_DUST, x + ox, y + oy, report);
}

/* sub_080ADB94(e, dir, r2, r3): the emerge hop step machine on + 0xB.
 * Step 0 launches horizontally (vx +/-0x8000, ax -/+0x300 by dir)
 * when r3 == 0 or r2 > 7; step 1 adds ax to vx until it crosses 0;
 * step 2 waits 0x10 ticks; step 0 relaunches. Position integrates
 * at the end of every call. */
static void worm_emerge(AosEnemy *e, int dir, int r2, int r3) {
    if (e->step == 0) {
        if (r3 == 0 || r2 > 7) {
            if (dir) { e->vx = 0x8000; e->ax = -0x300; }
            else     { e->vx = -0x8000; e->ax = 0x300; }
            e->step = 1;
            e->timer = 0;
        }
    } else if (e->step == 1) {
        e->timer++;
        e->vx += e->ax;
        if ((e->ax > 0 && e->vx >= 0) || (e->ax <= 0 && e->vx <= 0))
            e->step = 2;
    } else if (e->step == 2) {
        if (++e->timer > 0x10) {
            e->step = 0;
            e->timer = 0;
        }
    }
    e->x += e->vx;
    e->y += e->vy;
}

/* sub_080AD9FC(e): settle on the floor and probe the hop side.
 * The floor query sub_08001D94 at (x, y + 0xA) adjusts the y high
 * word by depth + 2; without a floor the owner (+ 0x14: the parent,
 * or the entity itself for the parent whose + 0x14 is itself) waits
 * 4 ticks in + 0x1D. A wall probe 0x18 px ahead also sets it: the
 * parent probes its hop side (dir 0 unmirrored left, dir 1 mirrored
 * right), segment 4 the opposite side by the parent's dir
 * (sub_0800207C / sub_08002058 = aos_wall_push_right /
 * aos_wall_push_left). */
static void worm_settle(AosEnemy *e, const AosCollision *layer) {
    AosEnemy *owner = e->role == AOS_ROLE_WORM_SEGMENT && e->parent ? e->parent : e;
    int x = (int)(e->x >> 16), y = (int)(e->y >> 16);
    int depth = aos_floor_depth(layer, x, y + 0xA, 0, false);
    if (depth) {
        e->y += (int32_t)(depth + 2) << 16;
    } else if (owner->worm_wait == 0) {
        owner->worm_wait = 4;
    }
    int push = 0;
    if (e->worm_index == 0) {
        if (e->worm_dir == 0) {
            if (!e->mirrored) push = aos_wall_push_right(layer, x - 0x18, y - 8);
        } else if (e->mirrored) {
            push = aos_wall_push_left(layer, x + 0x18, y - 8);
        }
    } else if (e->worm_index == 4 && owner != e) {
        if (owner->worm_dir != 0) {
            if (!e->mirrored) push = aos_wall_push_left(layer, x + 0x18, y - 8);
        } else if (e->mirrored) {
            push = aos_wall_push_right(layer, x - 0x18, y - 8);
        }
    }
    if (push && owner->worm_wait == 0) owner->worm_wait = 4;
}

/* sub_080AE008(e): the terrain slope angle under the entity, the
 * + 0x54 affine angle. Two floor scans in 8 px steps down from
 * y + 0xA at x - 8 and x + 8 give the ground heights; ArcTan2 of
 * the (-16, left - right) span plus 0x8000. */
static uint16_t worm_ground_angle(const AosEnemy *e, const AosCollision *layer) {
    int x = (int)(e->x >> 16), y = (int)(e->y >> 16);
    int left_y = y, right_y = y;
    for (int r6 = 0; r6 <= 0x1F; r6 += 8) {
        int d = aos_floor_depth(layer, x - 8, y + 0xA + r6, 0, false);
        if (d) { left_y = y + d + r6; break; }
    }
    int probe_y = y + 0xA;
    for (int r6 = 0; r6 <= 0x1F; r6 += 8, probe_y += 8) {
        int d = aos_floor_depth(layer, x + 8, probe_y, 0, false);
        if (d) { right_y = y + d + r6; break; }
    }
    return (uint16_t)(aos_arctan2(-16, (int16_t)(left_y - right_y)) + 0x8000);
}

/* sub_080AD748(c, hit, hurt): a segment's hurt callback forwards the
 * damage to the parent (+ 0x14): hit flash 8, HP + 0x34 reduced; at
 * HP <= 0 the parent dies in state 3 — PlaySong(0x70), + 0x0D = 0x40,
 * vx toward the player (+/-0x8000 by its x) and vy = -0x30000. The
 * sub_080683BC reward is the parent's own killed report. */
static void worm_segment_hurt(AosEnemy *c, AosEnemy *parent, AosSoma *soma,
                              AosHitReport *report) {
    if (!report->enemy_hit || report->enemy_damage <= 0) return;
    /* The native callback replaces the child's own HP loss. */
    c->hp = (int16_t)(c->hp + report->enemy_damage);
    parent->hit_flash = 8;
    parent->hp = (int16_t)(parent->hp - report->enemy_damage);
    if (parent->hp <= 0 && parent->state != 3) {
        aos_audio_play_song(0x70);
        parent->state = 3;
        parent->timer = 0x40;
        parent->vx = soma->x > parent->x ? 0x8000 : -0x8000;
        parent->vy = -0x30000;
    }
}

/* sub_080AE390 is the cleanup walk over the + 0x20-indexed chain:
 * the room removes dead entities, nothing to port. */

static AosHitReport worm_update(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                                const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                const AosWeaponFrames *weapon_frames, int soma_atk,
                                int soma_def, uint32_t (*random)(void)) {
    AosHitReport report = {0 };
    /* The four linked segments of sub_080AD7F0, emitted on the first
     * update (the native create emits them): each + 0x18 links to the
     * previous entity, + 0x1E its index, + 0x22 index << 12, x =
     * previous x +/- 0xA0000 by the facing bit, y = previous y, then
     * the sub_080ADB0C floor snap. */
    if (!e->boss_parts_spawned) {
        e->boss_parts_spawned = true;
        int32_t offset = e->mirrored ? -0xA0000 : 0xA0000;
        int32_t cx = e->x, cy = e->y;
        for (int i = 1; i <= 4 && report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i) {
            AosEnemy s = {0};
            s.id = e->id;
            s.role = AOS_ROLE_WORM_SEGMENT;
            s.static_frame = -1;
            s.x = cx + offset;
            s.y = cy;
            s.mirrored = e->mirrored;
            s.stats = e->stats;
            s.hp = e->stats.hp;
            s.combat.type = AOS_TYPE_ENEMY;
            s.own_boxes = true;
            s.own_hurt = s.own_attack = (AosBox){-8, -8, 16, 16};
            s.worm_index = (uint8_t)i;
            s.worm_phase = (uint16_t)(i << 12);
            s.variant = e->variant;
            /* The segment's own contact hit keeps the parent's element
             * word (sub_080AE334 / sub_080AE3CC are installed on all
             * five entities by sub_080AD7F0). */
            s.contact_elements = e->contact_elements;
            cx = s.x;
            cy = s.y;
            snap_to_floor(&s, layer);
            report.children[report.child_count++] = s;
        }
    }
    /* sub_0806AD24: alive and flashing (+ 0x2D) — the hit-stop gate
     * sub_0806D4C0 is always false here. A stunned parent only
     * collides (states other than 3), skipping the state machine,
     * the affine setup and the animation commit. */
    if (e->state != 0 && e->hp > 0 && e->hit_flash) {
        e->hit_flash--;
        if (e->state != 3) {
            AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                                       soma_atk, soma_def);
            skeleton_merge(&report, &hit);
        }
        return report;
    }
    /* sub_080AD748 kills the parent through a segment's callback: its
     * state-3 lunge returns before the epilogue, so the death reward
     * report is raised here, once, on the first state-3 update. */
    if (e->state == 3 && e->hp <= 0 && !e->defeated) {
        e->defeated = true;
        report.killed = true;
    }
    switch (e->state) {
    case 0:
        e->state = 0;
        /* fall through */
    case 1:
        /* + 0x1D wall-wait: while set, re-emerge each tick and count
         * down; at 4 the hop direction toggles. Otherwise decide the
         * direction: the sub_0806CFFC box (x - 0xA0, y - 0x3C,
         * 0x140 x 0x78) picks the aim target's side, a miss rolls
         * RandomNumberGenerator & 1. */
        if (e->worm_wait) {
            e->state = 2;
            if (e->worm_wait == 4) e->worm_dir ^= 1;
            e->worm_wait--;
        } else {
            if (!player_near(soma, (e->x >> 16) - 0xA0, (e->y >> 16) - 0x3C,
                             0x140, 0x78)) {
                e->worm_dir = (uint8_t)(random() & 1);
            } else {
                e->worm_dir = target_x(soma) >= e->x ? 1 : 0;
            }
            e->state = 2;
        }
        break;
    case 2:
        worm_emerge(e, e->worm_dir, 0, 0);
        worm_settle(e, layer);
        if (e->step == 0) e->state = 1;
        break;
    case 3:
        /* The death lunge (+ 0x1C tells the segments to attack too):
         * position += velocity, vy += 0x2000, the affine angle turns
         * 0x100 against the facing, dust every 8th tick; the + 0x0D
         * countdown ends in EntityDelete (sub_0803E6BC is cosmetic). */
        e->worm_lunge = 1;
        e->x += e->vx;
        e->y += e->vy;
        e->vy += 0x2000;
        e->angle = (uint32_t)((int32_t)e->angle + (e->mirrored ? -0x100 : 0x100));
        if ((e->timer & 7) == 0)
            worm_dust(e->x, e->y, 0x100000, 0x10000, &report, random);
        if (--e->timer == 0) {
            e->removed = true;
            return report;
        }
        step_anim(e, kind->anims);
        aos_combat_tick(&e->combat);
        return report;
    case 4:
        e->timer = 2;
        e->state = 5;
        /* fall through */
    case 5:
        /* A pure countdown to EntityDelete: no collision, no affine,
         * no animation commit. */
        if (--e->timer == 0) e->removed = true;
        return report;
    case 6:
        /* Inert under the global 0x8E & 0x40: the affine identity and
         * the animation commit only. */
        step_anim(e, kind->anims);
        aos_combat_tick(&e->combat);
        return report;
    default:
        break;
    }
    /* _080AE2E8 epilogue: the collision pass and the terrain angle
     * (state 3 keeps its own), then the affine commit sub_0803F17C
     * (step_anim) — the renderer reads e->angle. */
    if (e->state != 3) {
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        skeleton_merge(&report, &hit);
        e->angle = worm_ground_angle(e, layer);
    }
    step_anim(e, kind->anims);
    aos_combat_tick(&e->combat);
    return report;
}

/* sub_0803F2C8 init of a segment (child state 0): the parent's
 * palette byte and facing bit copy over (the + 0x2C / + 0x5A merges
 * are cosmetic), animation 1 — 2 for segment 4 — looping, + 0x5C =
 * 0x11, the hit callbacks and the 0x1010F8F8 box pair are set at
 * emit, state 1 (4 under the global flag). */
static void worm_segment_init(AosEnemy *c, const AosEnemyKind *kind) {
    play(c, kind->anims, c->worm_index == 4 ? 2 : 1);
    step_anim(c, kind->anims);
    c->state = flag_8e_40 ? 4 : 1;
}

/* The affine tail of sub_080ADC2C (states 1, 2 and 4+): sl = 0x8000
 * unmirrored else 0; the Poison Worm's segments (parent + 0x21) add a
 * sine wobble on the + 0x22 phase (3 * sine << 13, phase += 0x200 —
 * the secondary affine scale is not modelled); the rotation is the
 * direction to the + 0x18 follow target plus sl (sub_08068A70). */
static void worm_segment_affine(AosEnemy *c, AosEnemy *parent) {
    if (parent->variant != 0) c->worm_phase = (uint16_t)(c->worm_phase + 0x200);
    if (!c->follow) return;
    int32_t dx = (int16_t)((c->follow->x >> 16) - (c->x >> 16));
    int32_t dy = (int16_t)((c->follow->y >> 16) - (c->y >> 16));
    uint32_t sl = c->mirrored ? 0 : 0x8000;
    c->angle = (uint32_t)((uint16_t)aos_arctan2((int16_t)dx, (int16_t)dy) + sl);
}

static AosHitReport worm_segment_update(AosEnemy *c, AosSoma *soma,
                                        const AosCollision *layer, const AosEnemyKind *kind,
                                        const AosWeaponEntity *weapon,
                                        const AosWeaponFrames *weapon_frames, int soma_atk,
                                        int soma_def, uint32_t (*random)(void)) {
    AosHitReport report = {0 };
    AosEnemy *parent = c->parent;
    if (!parent || parent->removed) {
        c->removed = true;
        return report;
    }
    /* A live segment under a stunned parent (hit_flash; the hit-stop
     * gate is always false) only collides — the sub_08021278 tracking
     * flag and the sub_080429D0 record are approximated by the regular
     * pass. */
    if (c->state != 0 && parent->state != 3 && parent->hp > 0 && parent->hit_flash) {
        AosHitReport hit = collide(c, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        skeleton_merge(&report, &hit);
        return report;
    }
    switch (c->state) {
    case 0:
        /* The lazy init is skipped when the parent already lunges:
         * the segment deletes at once (the case-3 path). */
        if (parent->worm_lunge) {
            c->removed = true;
            return report;
        }
        worm_segment_init(c, kind);
        /* fall through */
    case 1:
        /* sub_080ADB94(c, parent + 0x1F, follow + 0x0D, 1): the hop is
         * gated by the follow target's timer (> 7), then
         * sub_080AD9FC(c) settles it. */
        worm_emerge(c, parent->worm_dir, c->follow ? c->follow->timer : 0, 1);
        worm_settle(c, layer);
        if (parent->worm_lunge) {
            /* The parent's death lunge: state 2, timer 0x38, the affine
             * angle sl, vx = index << 15 - 0x6000 flipped toward Soma,
             * vy = -0x30000. */
            c->state = 2;
            c->timer = 0x38;
            c->angle = c->mirrored ? 0 : 0x8000;
            c->vx = ((int32_t)c->worm_index << 15) - 0x6000;
            if (soma->x > c->x) c->vx = -c->vx;
            c->vy = -0x30000;
        }
        break;
    case 2:
        c->x += c->vx;
        c->y += c->vy;
        c->vy += 0x2000;
        c->angle = (uint32_t)((int32_t)c->angle + (c->mirrored ? -0x100 : 0x100));
        if ((c->timer & 7) == 0)
            worm_dust(c->x, c->y, 0x100000, 0x100000, &report, random);
        if (c->timer == 0) {
            c->removed = true;
            return report;
        }
        c->timer--;
        break;
    case 3:
        c->removed = true;
        return report;
    default:
        break;
    }
    worm_segment_affine(c, parent);
    if (parent->state != 3) {
        AosHitReport hit = collide(c, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        skeleton_merge(&report, &hit);
    }
    step_anim(c, kind->anims);
    aos_combat_tick(&c->combat);
    return report;
}

/* Biphron (EnemyBiphronCreate 0x08099C5C, EnemyBiphronUpdate
 * 0x0809A450 behind the sub_0806CC20 awake gate — approximated by
 * the usual activity/hit-stun handling): state 0 sub_08099CF0
 * patrols and hovers until the player box + RNG gate, state 1
 * sub_08099ED8 plays the attack and fires the sub_08099FD0
 * projectile at anim frame 4 (song 0x78), state 2 sub_08099DB8
 * is the death burst (song 0x6E), state 3 affine only. */
static AosHitReport biphron_update(AosEnemy *e, AosSoma *soma,
                                   const AosCollision *layer, const AosEnemyKind *kind,
                                   const AosWeaponEntity *weapon,
                                   const AosWeaponFrames *weapon_frames, int soma_atk,
                                   int soma_def, uint32_t (*random)(void)) {
    AosHitReport report = {0};
    switch (e->state) {
    case 0:
        if (e->step == 0) {
            /* sub_0806CF2C facing (+ 0x58 bit 6), vx = +-0x4000 by it,
             * vy = +0x10000, + 0x54 = 0x2800. */
            face_player(e, soma);
            e->vx = e->mirrored ? 0x4000 : -0x4000;
            e->vy = 0x10000;
            e->angle = 0x2800;
            e->step = 1;
        }
        /* sub_08069A00(e, 0x10000, -0x200000, 0x80000, 8): walk_collide
         * is its port; only the floor bit (& 4) feeds the hover
         * timer + 0xD. */
        if (walk_collide(e, layer, 0x10000, -0x200000, 0x80000, 8) & 4) {
            if (++e->timer > 0x3B) {
                e->timer = 0;
                e->step = 0;
                if (player_near(soma, (e->x >> 16) - 0x5A, (e->y >> 16) - 0x64,
                                0xB4, 0xC8) && (random() & 0x7F) <= 0x2F) {
                    e->state = 1;
                    e->step = 0;
                }
            }
        }
        break;
    case 1:
        if (e->step == 0) {
            play_loop(e, kind->anims, 1, false);
            e->step = 1;
        } else {
            /* Every update on anim frame 4 fires one projectile
             * (sub_08099FD0) and requests song 0x78; + 0x59 bit 0
             * (cycle_done) ends the attack back into state 0. */
            if (e->anim.frame == 4) {
                if (report.child_count < AOS_ENEMY_MAX_CHILDREN) {
                    AosEnemy shot = {0};
                    shot.id = e->id;
                    shot.role = AOS_ROLE_BIPHRON_SHOT;
                    shot.static_frame = -1;
                    shot.x = e->x + (e->mirrored ? -0x1A0000 : 0x1A0000);
                    shot.y = e->y;
                    shot.mirrored = e->mirrored;
                    shot.vy = -0x10000;
                    /* The native sub_08042848 physics preset
                     * 0x0E14F2FB is not decoded; a modest forward
                     * drift stands in for it. */
                    shot.vx = e->mirrored ? 0x4000 : -0x4000;
                    shot.stats = e->stats;
                    shot.hp = 1;
                    shot.combat.type = AOS_TYPE_ENEMY;
                    shot.own_boxes = true;
                    shot.own_hurt = (AosBox){-8, -8, 16, 16};
                    shot.own_attack = (AosBox){-6, -6, 12, 12};
                    shot.contact_elements = 2;
                    shot.timer = 0xFF;
                    report.children[report.child_count++] = shot;
                }
                aos_audio_play_song(0x78);
            }
            if (e->cycle_done) {
                play_loop(e, kind->anims, 0, true);
                e->state = 0;
                e->step = 0;
            }
        }
        break;
    case 2:
        /* sub_08099DB8: at anim frame 1 two spark pairs
         * (sub_0806D644, approximated by drift sparks); from frame 6
         * a 0x50-tick burst — a sprite piece + song 0x6E every 16th
         * tick (sub_0809A340 is approximated by a spark), the blink
         * skipped — then the native + 0x59 |= 8 deletes it. */
        if (e->step == 0) {
            if (e->anim.frame == 1) {
                int32_t rx = (int32_t)(random() & 0x7FFFF) - 0x40000;
                spawn_drift(AOS_EFFECT_SPARK, e->x, e->y - 0x6000, &report);
                spawn_drift(AOS_EFFECT_SPARK, e->x + rx - 0x40000, e->y - 0x4000, &report);
                e->step = 1;
            }
        } else if (e->step == 1) {
            if (e->anim.frame == 6) {
                e->step = 2;
                e->timer = 0x50;
            }
        } else {
            if (e->timer > 0x2F && (e->timer & 0xF) == 0) {
                int32_t rx = (int32_t)(random() & 0xFFFFF) - 0x80000;
                spawn_drift(AOS_EFFECT_SPARK,
                            e->x + (e->mirrored ? -0x80000 : 0x80000) + rx - 0x80000,
                            e->y - 0x80000, &report);
                aos_audio_play_song(0x6E);
            }
            if (--e->timer == 0) {
                e->removed = true;
                return report;
            }
        }
        break;
    case 3:
        /* Affine identity only (sub_0803F17C epilogue). */
        step_anim(e, kind->anims);
        aos_combat_tick(&e->combat);
        return report;
    default:
        break;
    }
    /* States 0-2: the sub_0806DF20 + sub_0806E314 epilogue — the
     * collision pass then the animation commit. */
    {
        AosHitReport hit = collide(e, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        skeleton_merge(&report, &hit);
    }
    step_anim(e, kind->anims);
    aos_combat_tick(&e->combat);
    return report;
}

/* The sub_08099FD0 projectile (update sub_0809A29C): it rises,
 * drifts forward, dies on a wall/ceiling probe or off the timers,
 * damages Soma through its elements-2 contact record
 * (sub_0809A310) and leaves a spark trail with song 0x77 every
 * 4th tick (sub_0809A0DC trail approximated). */
static AosHitReport biphron_shot_update(AosEnemy *b, AosSoma *soma,
                                        const AosCollision *layer, const AosEnemyKind *kind,
                                        const AosWeaponEntity *weapon,
                                        const AosWeaponFrames *weapon_frames, int soma_atk,
                                        int soma_def) {
    AosHitReport report = {0};
    if (b->step == 0) {
        /* sub_0803F2C8(e, 4, 0, 0): the shot's own animation. */
        if (kind->anims && kind->anims->count > 4) play_loop(b, kind->anims, 4, false);
        b->step = 1;
    }
    if (b->timer && --b->timer == 0) {
        b->removed = true;
        return report;
    }
    b->x += b->vx;
    b->y += b->vy;
    int32_t x = b->x >> 16, y = (b->y >> 16) - 10;
    bool blocked = false;
    if (b->vx < 0 && aos_wall_push_right(layer, x - 8, y)) blocked = true;
    if (b->vx > 0 && aos_wall_push_left(layer, x + 8, y)) blocked = true;
    if (b->vy < 0) {
        int depth = aos_ceiling_depth(layer, b->x >> 16, b->y >> 16, 0, false);
        if (depth) {
            b->y += (int32_t)((uint32_t)depth << 16);
            blocked = true;
        }
    }
    if (blocked) {
        b->removed = true;
        return report;
    }
    if ((b->timer & 3) == 0) {
        spawn_drift(AOS_EFFECT_SPARK, b->x, b->y, &report);
        aos_audio_play_song(0x77);
    }
    report = collide(b, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    step_anim(b, kind->anims);
    aos_combat_tick(&b->combat);
    return report;
}

/* Bomber Armor (EnemyBomberArmorUpdate 0x080B3040): it shares the Rock
 * Armor's walk (sub_080B2978, probes 0x0852872C) and death (sub_080B2CD8)
 * with it; only the throw (sub_080B2A80) and the bomb it throws
 * (sub_080B243C, with its fuse spark sub_080B2FC4) are its own. */

/* sub_080B2A80: the throw. Substep 0 plays animation 1 once; at frame 7
 * sound 0x88 and the bomb (sub_0806DFF8 with sub_080B243C: the facing,
 * position, hit points and contact power copied). When the animation ends
 * the player still ahead within 99 pixels (sub_0806BBC4 / sub_0806D044)
 * backs it off one step — substep 2 walks backward once at the negated
 * walk speed, then substep 0 replays the throw — otherwise substep 3
 * resets to the walk. */
static void bomber_throw(AosEnemy *a, const AosSoma *soma, const AosCollision *layer,
                         const AosEnemyKind *kind, AosHitReport *report) {
    switch (a->step) {
    case 0:
        play_loop(a, kind->anims, 1, false);
        a->step = 1;
        break;
    case 1:
        if (a->anim.frame == 7) {
            aos_audio_play_song(0x88);
            if (report->child_count < AOS_ENEMY_MAX_CHILDREN) {
                AosEnemy bomb = {0};
                bomb.id = a->id;
                bomb.role = AOS_ROLE_BOMBER_BOMB;
                bomb.static_frame = -1;
                bomb.x = a->x;
                bomb.y = a->y;
                bomb.mirrored = a->mirrored;
                bomb.hp = a->hp;
                bomb.stats = a->stats;
                report->children[report->child_count++] = bomb;
            }
        }
        if (a->anim.flags & AOS_ANIM_ENDED)
            a->step = player_ahead_within(a, soma, 0x63) ? 2 : 3;
        break;
    case 2:
        ground_walk(a, soma, layer, &rock_armor_probes, -rock_armor_speed(a), 0x14);
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

/* sub_080B243C: the bomb. The collision pass first (its hit callback
 * sub_080B2F5C zeroes its hit points). State 0, on the first update:
 * animation 3 looping, contact elements 2 (sub_0804277C with sub_080B2F64
 * / sub_080B2F5C), the boxes of sub_08042884 (0x3030E8E8) and
 * sub_080428F0 (0x1010F8F8), + 0x72 bit 1 (not ported), then the throw —
 * 58 pixels up, vy -2.5 with gravity 0.125 (+ 0x54 = 0x2000) through the
 * probe walker sub_0806CAF8 (0x08528738, mode 2) at the speed of + 0x18:
 * -2.0 - the distance to the player / 64 a frame, negated by the facing;
 * walls halve and reverse it, a ceiling starts the fuse. Every fourth
 * frame a fuse spark (sub_080B2FC4) 8 pixels above. State 2 counts 16
 * frames down, then the burst of sub_08045A98 list 0 and state 3, which
 * plays sound 0x76 and deletes it. Beyond screen margin 3 it is deleted.
 * The global pause of gEwramData + 0x4BE is not ported. */
static void bomber_burst(const AosEnemy *b, AosHitReport *report) {
    for (size_t i = 0; i < burst_source_count &&
                        report->effect_count < AOS_ENEMY_MAX_EFFECTS;
         ++i) {
        AosEffect piece = burst_source[i];
        piece.x = (int32_t)((uint32_t)(int16_t)((b->x >> 16) + (piece.x >> 16)) << 16);
        piece.y = (int32_t)((uint32_t)(int16_t)((b->y >> 16) + (piece.y >> 16)) << 16);
        report->effects[report->effect_count++] = piece;
    }
}

static AosHitReport bomber_bomb_update(AosEnemy *b, AosSoma *soma, const AosCollision *layer,
                                       const AosEnemyKind *kind,
                                       const AosWeaponEntity *weapon,
                                       const AosWeaponFrames *weapon_frames, int soma_atk,
                                       int soma_def, int cam_x, int cam_y,
                                       uint32_t (*random)(void)) {
    (void)random;
    AosHitReport report = collide(b, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    aos_combat_tick(&b->combat);
    switch (b->state) {
    case 0:
        play(b, kind->anims, 3);
        step_anim(b, kind->anims);
        b->contact_elements = 2;
        b->own_boxes = true;
        b->own_hurt = (AosBox){-8, -8, 16, 16};
        b->own_attack = (AosBox){-24, -24, 48, 48};
        {
            int32_t dx = pixel_delta(target_x(soma), b->x);
            int32_t dy = pixel_delta(target_y(soma), b->y);
            int32_t distance = (int16_t)isqrt((uint32_t)(dx * dx + dy * dy));
            b->spin = -0x2000 - ((distance << 16) >> 6);
        }
        b->y -= 0x3A0000;
        if (b->mirrored) b->spin = -b->spin;
        b->vy = (int32_t)0xFFFD8000;
        b->ay = 0x2000;
        b->stats.contact = (uint8_t)(b->stats.contact + (b->stats.contact >> 2));
        b->state = 1;
        /* fallthrough */
    case 1:
        if ((b->timer & 3) == 0 && report.child_count < AOS_ENEMY_MAX_CHILDREN) {
            AosEnemy spark = {0};
            spark.id = b->id;
            spark.role = AOS_ROLE_BOMBER_SPARK;
            spark.static_frame = -1;
            spark.x = b->x;
            spark.y = b->y - 0x80000;
            spark.combat.attack_off = spark.combat.hurt_off = true;
            report.children[report.child_count++] = spark;
        }
        b->timer++;
        {
            int walk = ground_walk(b, soma, layer, &rock_probes, b->spin, 2);
            if (walk & 3) b->spin = -b->spin / 2;
            if (walk & 8) {
                b->state = 2;
                b->timer = 0x10;
            }
        }
        break;
    case 2:
        if (b->timer) {
            b->timer--;
        } else {
            bomber_burst(b, &report);
            b->state = 3;
        }
        break;
    case 3:
        aos_audio_play_song(0x76);
        b->removed = true;
        return report;
    default:
        break;
    }
    if (outside_margins(kind, 3, (b->x >> 16) - cam_x, (b->y >> 16) - cam_y))
        b->removed = true;
    else
        step_anim(b, kind->anims);
    return report;
}

/* sub_080B2FC4: the fuse spark — animation 4 once, then its affine step;
 * gone when the animation ends (the affine slot's return 3). */
static void bomber_spark_update(AosEnemy *s, const AosEnemyKind *kind) {
    if (s->state == 0) {
        play_loop(s, kind->anims, 4, false);
        s->state = 1;
    }
    step_anim(s, kind->anims);
    if (s->anim.flags & AOS_ANIM_ENDED) s->removed = true;
}

/* EnemyBomberArmorUpdate: inside the activity window of sub_0806CC20
 * (only colliding while flashing from a blow), the state, the collision
 * pass (sub_0806E314, skipped while defeated) and the animation step
 * (sub_0806DF20); state 3 (the global 0x8E & 0x40) only steps its
 * animation. */
static AosHitReport bomber_update(AosEnemy *a, AosSoma *soma, const AosCollision *layer,
                                  const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                  const AosWeaponFrames *weapon_frames, int soma_atk,
                                  int soma_def, int cam_x, int cam_y,
                                  uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    int sx = (a->x >> 16) - cam_x, sy = (a->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
    if (a->hp > 0 && a->hit_flash) {
        a->hit_flash--;
        report = collide(a, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        aos_combat_tick(&a->combat);
        return report;
    }
    switch (a->state) {
    case 0:
        rock_armor_walk(a, soma, layer);
        break;
    case 1:
        bomber_throw(a, soma, layer, kind, &spawned);
        break;
    case 2:
        rock_armor_die(a, kind, random, &spawned);
        if (a->removed) {
            take_effects(&report, &spawned);
            return report;
        }
        break;
    default:
        step_anim(a, kind->anims);
        return report;
    }
    if (a->state != 2)
        report = collide(a, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    step_anim(a, kind->anims);
    aos_combat_tick(&a->combat);
    report.child_count = spawned.child_count;
    memcpy(report.children, spawned.children, sizeof spawned.children[0] * spawned.child_count);
    take_effects(&report, &spawned);
    return report;
}

/* Sky Fish state 0 (sub_080A7D58): the entry waits out + 0x1A while hidden,
 * then crosses the top of the screen. Step 0 plants it on the entry side
 * (+ 0x19: -16 or 256 px), a random height in [32, 64), faces the player
 * (sub_0806CF2C) and counts + 0x1A down; at 1 it shows itself and dashes
 * at 26 px/frame toward its facing. Step 1 integrates, spawns the splash
 * entity sub_080A8040 at a random offset every frame and despawns beyond
 * the margin of entry 6 (sub_0806D128). The native step-2 transition sits
 * behind the pause flag 0x4BE (always clear during updates), so the fish
 * keeps step 1; its + 0x18 gate stays 1 and it never collides. */
static void sky_fish_dash(AosEnemy *a, const AosSoma *soma, const AosEnemyKind *kind,
                          int cam_x, int cam_y, uint32_t (*random)(void),
                          AosHitReport *report) {
    if (a->step == 0) {
        a->x = a->sf_variant ? 0x1000000 : (int32_t)0xFFF00000;
        a->y = 0x200000 + (int32_t)(random() & 0x1FFFFF);
        soldier_face(a, soma);
        if (a->sf_timer-- > 1) return;
        a->step = 1;
        a->hidden = false;
        a->vx = a->mirrored ? 0x1A0000 : (int32_t)0xFFE60000;
        return;
    }
    int32_t tx = a->x + (int32_t)(random() & 0xFFFFF) - 0x80000;
    int32_t ty = a->y + (int32_t)(random() & 0xFFFFF) - 0x80000;
    a->x += a->vx;
    a->y += a->vy;
    /* sub_080A8040's splash entity on the shared tiles is approximated by
     * a dust drift at the native random offset. */
    spawn_drift(AOS_EFFECT_DUST, tx, ty, report);
    if (outside_margins(kind, 6, (a->x >> 16) - cam_x, (a->y >> 16) - cam_y))
        a->removed = true;
}

/* Sky Fish state 1 (sub_080A7F08, reached only by the lethal hurt
 * callback): it hangs with ay 0x4000 under the sub_0806D3D8 limit 4.0,
 * blinks on sub_0806D518's timer pattern (+ 0x58 bit 5) and bursts a bubble
 * entity sub_080A80F8 every eighth timer tick at a small random offset;
 * the timer reaching 0 sets + 0x59 bit 3 and it is deleted. */
static void sky_fish_hover(AosEnemy *a, const AosEnemyKind *kind,
                           uint32_t (*random)(void), AosHitReport *report) {
    if (a->step == 0) {
        a->step = 1;
        a->vx = a->vy = a->ax = 0;
        a->ay = 0x4000;
    }
    accel_move(a, 0x40000);
    a->hidden = a->timer <= 0x28 && (kind->blink[a->timer] & 1);
    if ((a->timer & 7) == 0) {
        int32_t bx = a->x + (int32_t)(random() & 0x7FFFF) - 0x40000;
        int32_t by = a->y + (int32_t)(random() & 0xFFFFF) - 0x80000;
        spawn_drift(AOS_EFFECT_DUST, bx, by, report);
    }
    if (--a->timer == 0) a->removed = true;
}

/* EnemySkyFishUpdate (0x080A7BD0): the sub_08068AD4 proximity check only
 * feeds the activity list of sub_08021924 (not ported); sub_0806AD24
 * stuns it while flashing. States 0/1 then step the animation in the
 * common tail, refresh the boxes on a frame change (automatic here) and
 * collide only outside state 1 with the + 0x18 gate clear — which the
 * create never clears, so a live Sky Fish is invulnerable decoration.
 * State 2 (the 0x8E & 0x40 flag path) only steps its animation. */
static AosHitReport sky_fish_update(AosEnemy *enemy, AosSoma *soma,
                                    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                    const AosWeaponFrames *weapon_frames, int soma_atk,
                                    int soma_def, int cam_x, int cam_y,
                                    uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    if (enemy->hp > 0 && enemy->hit_flash) {
        enemy->hit_flash--;
    } else if (enemy->state == 2) {
        step_anim(enemy, kind->anims);
        aos_combat_tick(&enemy->combat);
        return report;
    } else {
        if (enemy->state == 0)
            sky_fish_dash(enemy, soma, kind, cam_x, cam_y, random, &spawned);
        else if (enemy->state == 1)
            sky_fish_hover(enemy, kind, random, &spawned);
        step_anim(enemy, kind->anims);
    }
    if (enemy->state != 1 && enemy->sf_gate == 0)
        report = collide(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    aos_combat_tick(&enemy->combat);
    take_effects(&report, &spawned);
    return report;
}

/* sub_08099690: state 0 of the ectoplasm drift. Step 0 hovers at the home
 * point (+ 0x14/+ 0x18, here in world pixels — the gEwramData + 0xA094
 * camera offsets cancel): x is pinned, y is set to the target plus a
 * 3-pixel sine wobble of phase + 0x1C (+= 0x200), the timer counts and the
 * player box (+ 0x42 - 0x5A, + 0x46 - 0x64, 0xB4 x 0xC8 room pixels)
 * advances to step 1. Step 1 picks a destination around the aim target:
 * x +/- (16 + RNG % 768) pixels by RNG & 1, y = target y - RNG % 768 -
 * 16 or - 32 pixels alternating through + 0x20, then falls into step 2
 * the same frame. Step 2 steers: acceleration 0x400 toward the target
 * beyond 4 pixels on each axis, velocity damped 31/32 (sub_0806D490 /
 * sub_0806D4A8 with 0x20), sub_0806D3D8(e, 1.0), repeated once while
 * sub_080212C8's Soma + 0x10 flags & 0x360 are set; back to step 1 within
 * 0x3FFFF on both axes or after the u8 timer passes 0x77. */
static void ectoplasm_drift(AosEnemy *enemy, const AosSoma *soma, uint32_t (*random)(void)) {
    if (enemy->step == 0) {
        enemy->x = enemy->target_x;
        enemy->y = enemy->target_y +
                   (int32_t)(((int64_t)aos_sine(enemy->walk_timer + 0x4000) * 3));
        enemy->walk_timer += 0x200;
        enemy->timer++;
        if (player_near(soma, (int16_t)((enemy->x >> 16) - 0x5A),
                        (int16_t)((enemy->y >> 16) - 0x64), 0xB4, 0xC8)) {
            enemy->step = 1;
            enemy->timer = 0;
        }
        return;
    }
    if (enemy->step > 2) return;
    if (enemy->step == 1) {
        int32_t ybias;
        if (!enemy->ec_dir) {
            enemy->ec_dir = 1;
            ybias = (int32_t)0xFFF00000;      /* -16 pixels */
        } else {
            enemy->ec_dir = 0;
            ybias = (int32_t)0xFFE00000;      /* -32 pixels */
        }
        int32_t xoff = (int32_t)(umod(random(), 0x3000000) + 0x100000);
        enemy->target_x = target_x(soma) + ((random() & 1) ? xoff : -xoff);
        enemy->target_y = target_y(soma) - (int32_t)umod(random(), 0x3000000) + ybias;
        enemy->step = 2;
        enemy->timer = 0;
    }
    int32_t dx = enemy->target_x - enemy->x, dy = enemy->target_y - enemy->y;
    enemy->ax = dx > 0x40000 ? 0x400 : dx < -0x40000 ? -0x400 : 0;
    enemy->ay = dy > 0x40000 ? 0x400 : dy < -0x40000 ? -0x400 : 0;
    enemy->vx = enemy->vx * 31 / 32;
    enemy->vy = enemy->vy * 31 / 32;
    accel_move(enemy, 0x10000);
    if (soma->flags & 0x360) {
        enemy->vx = enemy->vx * 31 / 32;
        enemy->vy = enemy->vy * 31 / 32;
        accel_move(enemy, 0x10000);
    }
    if ((dx < 0 ? -dx : dx) <= 0x3FFFF && (dy < 0 ? -dy : dy) <= 0x3FFFF)
        enemy->step = 1;
    if (enemy->timer++ > 0x77) enemy->step = 1;
}

/* sub_080998E8: every hit knocks the ectoplasm back from the attacker at
 * 1.0 pixel per frame along the angle to it + 0x4000; a lethal blow marks
 * it defeated (sub_080683BC, done by the caller), plays song 0x6D,
 * switches to animation 1, state 1, step 0, timer 0x28, clears
 * vx/vy/ax and sets ay 0x1000. The + 0x2C/+ 0x5A palette bits are not
 * ported. */
static void ectoplasm_struck(AosEnemy *enemy, const AosSoma *soma,
                             const AosAnimSet *anims) {
    int32_t dx = pixel_delta(enemy->x, soma->x), dy = pixel_delta(enemy->y, soma->y);
    uint16_t angle = aos_arctan2((int16_t)dx, (int16_t)dy);
    enemy->vx = fixed_mul(aos_sine(angle + 0x4000u), 0x10000);
    enemy->vy = fixed_mul(aos_sine(angle), 0x10000);
    if (enemy->hp > 0) return;
    enemy->defeated = true;
    aos_audio_play_song(0x6D);
    play_loop(enemy, anims, 1, true);
    enemy->state = 1;
    enemy->step = 0;
    enemy->timer = 0x28;
    enemy->vx = enemy->vy = enemy->ax = 0;
    enemy->ay = 0x1000;
}

/* EnemyEctoplasmUpdate (sub_080994E0): outside margin 4 it freezes; the
 * sub_08068AD4 activity box only feeds the sub_08021924 target table
 * (not ported). + 0x58 bit 0x20 is the margin flag, sub_0806AE54 the hit
 * flash (skipped in state 1), sub_0806D4C0's global pause 0x4BE is taken
 * as 0. State 0 runs the drift, state 1 sinks with sub_0806AEAC wisps and
 * the blink table then deletes, state 2 parks at home with the wobble and
 * only steps its animation. States 0 and 1 finish with the animation
 * step and the collision pass (skipped once defeated). */
static AosHitReport ectoplasm_update(AosEnemy *enemy, AosSoma *soma,
                                      const AosEnemyKind *kind,
                                      const AosWeaponEntity *weapon,
                                      const AosWeaponFrames *weapon_frames, int soma_atk,
                                      int soma_def, int cam_x, int cam_y,
                                      uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    int sx = (enemy->x >> 16) - cam_x, sy = (enemy->y >> 16) - cam_y;
    if (enemy->state != 1 && outside_margins(kind, 4, sx, sy)) {
        aos_combat_tick(&enemy->combat);
        return report;
    }
    enemy->hidden = outside_margins(kind, 4, sx, sy);
    if (enemy->state != 1 && enemy->hp > 0 && enemy->hit_flash)
        enemy->hit_flash--;
    switch (enemy->state) {
    case 0:
        ectoplasm_drift(enemy, soma, random);
        break;
    case 1:
        accel_move(enemy, 0x40000);
        /* sub_0806AEAC: a wisp (the shared 0x49/0xCF flame spawn) every
         * 4th tick in a 16 x 3 pixel area, the blink table below 0x28,
         * deleted when the u8 timer wraps. */
        if ((enemy->timer & 3) == 0)
            spawn_flame_at(enemy->x, enemy->y - 0xA8000, 0x100000, 0x30000,
                           random, &spawned);
        enemy->hidden = enemy->timer <= 0x27 ? kind->blink[enemy->timer] & 1 : false;
        if ((int8_t)--enemy->timer <= 0) enemy->removed = true;
        break;
    case 2:
        if (enemy->step == 0) {
            enemy->x = enemy->target_x;
            enemy->y = enemy->target_y +
                       (int32_t)(((int64_t)aos_sine(enemy->walk_timer + 0x4000) * 3));
            enemy->walk_timer += 0x200;
            enemy->timer++;
        }
        step_anim(enemy, kind->anims);
        aos_combat_tick(&enemy->combat);
        return report;
    default:
        break;
    }
    step_anim(enemy, kind->anims);
    if (!enemy->defeated)
        report = collide(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    aos_combat_tick(&enemy->combat);
    take_effects(&report, &spawned);
    return report;
}

/* Evil Butcher (EnemyEvilButcherCreate 0x080BFE1C / EnemyEvilButcherUpdate
 * 0x080BFF14 behind the sub_0806CC20 awake gate — approximated by the
 * usual activity/margin handling): state 0 sub_080BFB58 patrols with the
 * probe walker sub_0806CAF8 (table 0x085288FC, mode 0xC) at a speed that
 * follows the animation frame parity and tick, flips its facing after a
 * short obstacle timer and wakes into state 1 when the player enters the
 * 0xA0 x 0x78 box of sub_0806E29C at the start of an animation loop.
 * State 1 sub_080BFBF4 winds up (sub_0806BC40 facing, animation 1 once),
 * throws the cleaver child sub_080BF738 through sub_0806DFF8 at frame 2
 * tick 2 and returns to the patrol when the animation ends. State 2
 * sub_080BFCD8 is the death: song 0x1C1, a gore child sub_080BF990, a
 * still frame 6, then 0x40 frames of gore bursts — every fourth tick a
 * sub_08045CEC sparkle (animation 0xD, row 8) or a sub_080BF6B8 particle —
 * and EntityDelete at the end. State 3 only steps its animation. The
 * hurt callback sub_080BFECC enters state 2 at a lethal blow (the palette
 * bits of + 0x5A are not ported); the contact is the generic
 * sub_0806E1B8 (element 0, damage + 0x3C). The camera offsets of the
 * sub_080020A0 surface probes are normalized into room pixels, the
 * sub_080421AC box query, the + 0x58 fade bits, the + 0x59 flags, the
 * + 0x0F death flag and the sub_0806D4C0 pause are not ported. */
static const AosProbes *butcher_probes(const AosEnemyKind *kind) {
    return kind && kind->probes.count > 0 && kind->probes.count <= 8 ? &kind->probes : NULL;
}

static AosHitReport butcher_update(AosEnemy *e, AosSoma *soma,
                                   const AosCollision *layer, const AosEnemyKind *kind,
                                   const AosWeaponEntity *weapon,
                                   const AosWeaponFrames *weapon_frames, int soma_atk,
                                   int soma_def, int cam_x, int cam_y,
                                   uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
    if (outside_margins(kind, 4, sx, sy)) {
        aos_combat_tick(&e->combat);
        return report;
    }
    e->hidden = outside_margins(kind, 4, sx, sy);
    switch (e->state) {
    case 0:
        if (e->anim.frame == 0 && e->anim.tick == 0) {
            if (buer_player_in_window(e, soma, 0xA0, 0x78)) {
                e->state = 1;
                e->step = e->substep = 0;
            } else {
                uint8_t t = e->timer++;
                if (t > 2) {
                    e->timer = 0;
                    e->mirrored = !e->mirrored;
                }
            }
        }
        if (e->state == 0) {
            const AosProbes *p = butcher_probes(kind);
            int32_t speed = (e->anim.frame & 1) ? 0x3000
                                              : 0xA000 - ((int32_t)e->anim.tick << 0xB);
            int walk = p ? ground_walk(e, soma, layer, p, speed, 0xC) : 0;
            if ((walk & 0x1B) != 8 && e->timer <= 2) e->timer = 3;
        }
        break;
    case 1:
        switch (e->step) {
        case 0:
            face_player(e, soma);
            play_loop(e, kind->anims, 1, false);
            e->step = 1;
            break;
        case 1:
            if (e->anim.frame == 2 && e->anim.tick == 2 &&
                spawned.child_count < AOS_ENEMY_MAX_CHILDREN) {
                /* sub_0806DFF8(e, sub_080BF738): the cleaver inherits the
                 * facing, the palette bank (+ 0x32) and the contact stats,
                 * placed on the pixel position 24 px up and 16 px to the
                 * facing side (the native + 0x42 / + 0x46 whole-pixel
                 * copies lose the fraction). */
                AosEnemy c = {0};
                c.id = e->id;
                c.role = AOS_ROLE_BUTCHER_CLEAVER;
                c.static_frame = -1;
                c.param1 = e->param1;
                c.stats = e->stats;
                c.x = (int32_t)(((int32_t)(e->x >> 16) + (e->mirrored ? 0x10 : -0x10)) << 16);
                c.y = (int32_t)(((e->y >> 16) - 0x18) << 16);
                c.mirrored = e->mirrored;
                spawned.children[spawned.child_count++] = c;
            }
            if (e->anim.flags & AOS_ANIM_ENDED) e->step = 3;
            break;
        case 3:
            e->state = e->step = e->substep = 0;
            play_loop(e, kind->anims, 0, true);
            break;
        default:
            break;
        }
        break;
    case 2:
        if (e->step == 0) {
            aos_audio_play_song(0x1C1);
            if (spawned.child_count < AOS_ENEMY_MAX_CHILDREN) {
                /* EntityCreateInRange(0x49, 0xCF, sub_080BF990): the gore
                 * 18 px to the facing side (negated when mirrored) plus or
                 * minus 1 px, 32 px up, palette bank copied. */
                AosEnemy g = {0};
                g.id = e->id;
                g.role = AOS_ROLE_BUTCHER_GORE;
                g.static_frame = -1;
                g.param1 = e->param1;
                g.x = e->x + (e->mirrored ? -0x120000 : 0x120000) +
                     random_centred(random, 0x20000);
                g.y = e->y + random_centred(random, 0x20000) - 0x200000;
                spawned.children[spawned.child_count++] = g;
            }
            e->static_frame = 6;
            e->step = 1;
            e->timer = 0x40;
        } else {
            if ((e->timer & 3) == 0) {
                if (random() & 1) {
                    /* sub_08045CEC(x + r, y + r - 16 px, 0xD, 8): the
                     * centred draws are +/- 8 px in x and -32..0 px in y
                     * (2D5EC(0x100000) and 2D5EC(0x200000) - 0x100000). */
                    spawn_drift(AOS_EFFECT_DEATH_SPARKLE,
                                e->x + random_centred(random, 0x100000),
                                e->y + random_centred(random, 0x200000) - 0x100000,
                                &spawned);
                } else if (spawned.child_count < AOS_ENEMY_MAX_CHILDREN) {
                    /* EntityCreateInRange(0x49, 0xCF, sub_080BF6B8): a
                     * particle with the same scatter and a life of
                     * (r & 7) + 8 frames. */
                    AosEnemy p = {0};
                    p.id = e->id;
                    p.role = AOS_ROLE_BUTCHER_PARTICLE;
                    p.static_frame = -1;
                    p.param1 = e->param1;
                    p.x = e->x + random_centred(random, 0x100000);
                    p.y = e->y + random_centred(random, 0x200000) - 0x100000;
                    p.timer = (uint8_t)((random() & 7) + 8);
                    spawned.children[spawned.child_count++] = p;
                }
            }
            if (e->timer == 0) {
                e->removed = true;
            } else {
                if (e->timer == 0x10) e->helper_spawned = true;   /* + 0xF */
                e->timer--;
            }
        }
        break;
    case 3:
        step_anim(e, kind->anims);
        aos_combat_tick(&e->combat);
        for (int i = 0; i < spawned.child_count &&
                        report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
            report.children[report.child_count++] = spawned.children[i];
        append_effects(&report, &spawned);
        return report;
    default:
        break;
    }
    /* sub_0806E314 runs in states 0-2 (the defeated butcher keeps its
     * contact); sub_0806DF20 steps the animation everywhere but state 2. */
    report = collide(e, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    if (e->state != 2) step_anim(e, kind->anims);
    aos_combat_tick(&e->combat);
    for (int i = 0; i < spawned.child_count &&
                    report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
        report.children[report.child_count++] = spawned.children[i];
    append_effects(&report, &spawned);
    return report;
}

/* sub_0806E350(c, parent, dx, dy): the child's whole-pixel position
 * becomes the parent's plus the offset — dx negated by the parent's
 * facing — keeping the child's own fraction. */
static void mantle_follow(AosEnemy *c, const AosEnemy *p, int dx, int dy) {
    int ex = (int)(p->x >> 16) + (p->mirrored ? -dx : dx);
    int ey = (int)(p->y >> 16) + dy;
    c->x = (c->x & 0xFFFF) | ((int32_t)ex << 16);
    c->y = (c->y & 0xFFFF) | ((int32_t)ey << 16);
}

/* sub_0806BBC4: the direction of the target from the entity (+1 to its
 * right, -1 to its left). The decoy-aware target_x approximates the
 * native target record; the target-facing variant sub_0806E3B0 reads
 * Soma's + 0x58 bit 0x40 (facing_left). */
static int mantle_player_dir(const AosEnemy *e, const AosSoma *soma) {
    return target_x(soma) > e->x ? 1 : -1;
}

/* sub_080BB2D4: the rocking patrol of state 1. Step 0 plays animation 1
 * once and clears the motion fields; step 1 waits for its end, plays
 * animation 2 and seeds + 0x14 = (random & 0xFF) + 0x100. Step 2 rocks:
 * + 0x18 counts up, + 0x54 = sine(+ 0x18 << 0xB) / 0x30 and the walk
 * speed is + 0x14 times sine(+ 0x18 * 0x300) >> 8, negated by + 0xC.
 * While Soma attacks (+ 0x10 & 0x360) within 0x4F pixels and both the
 * effective facing and the target facing point at him, + 0x18 = 0x30 and
 * + 0xD = 1 (the dash). The probe walker 0x085288B4 (mode 0) moves it;
 * past + 0x18 = 0x2F the 0xA0 x 0x46 box of sub_0806E29C starts the
 * attack (state 2) unless + 0xD is set, otherwise the walk turns around:
 * + 0xC flips, animation 2 or 7, a fresh speed with a bonus toward the
 * player and another one while + 0xD holds, + 0x18 = 0. */
static void mantle_patrol(AosEnemy *e, const AosSoma *soma, const AosCollision *layer,
                          const AosEnemyKind *kind, uint32_t (*random)(void)) {
    const AosAnimSet *anims = kind->anims;
    switch (e->step) {
    case 0:
        play_loop(e, anims, 1, false);
        e->vy = e->ay = 0;
        e->substep = 0;
        e->spin = 0;
        e->step = 1;
        break;
    case 1:
        if (!(e->anim.flags & AOS_ANIM_ENDED)) break;
        play_loop(e, anims, 2, false);
        e->step = 2;
        e->mantle_speed = (int32_t)(random() & 0xFF) + 0x100;
        break;
    case 2: {
        e->spin = (int16_t)(e->spin + 1) & 0xFFFF;
        int16_t phase = (int16_t)e->spin;
        e->ay = aos_sine((uint32_t)(phase << 0xB)) / 0x30;
        int32_t v = aos_sine((uint32_t)((int32_t)phase * 3 << 8));
        v = (v + (v < 0 ? 0xFF : 0)) >> 8;
        int32_t speed = e->mantle_speed * v;
        if (e->substep) speed = -speed;
        e->timer = 0;
        if (soma->flags & 0x360) {
            int32_t dx = (int16_t)(target_x(soma) >> 16) - (int16_t)(e->x >> 16);
            if (dx >= -0x4F && dx <= 0x4F) {
                int dir_eff = e->substep ^ (e->mirrored ? 1 : 0);
                int pdir = mantle_player_dir(e, soma);
                int tface = soma->facing_left ? 1 : 0;
                bool facing_ok = dir_eff == 0 ? pdir == -1 : pdir == 1;
                bool target_ok = tface == 0 ? pdir == -1 : pdir == 1;
                if (facing_ok && target_ok) {
                    e->spin = 0x30;
                    e->timer = 1;
                }
            }
        }
        if (kind->probes.count)
            ground_walk(e, soma, layer, &kind->probes, speed, 0);
        if ((int16_t)e->spin > 0x2F) {
            if (player_in_centred(e, soma, 0xA0, 0x46) && e->timer == 0) {
                e->state = 2;
                e->substep = e->step = 0;
            } else {
                e->substep ^= 1;
                play_loop(e, anims, e->substep ? 7 : 2, false);
                int dir_eff = e->substep ^ (e->mirrored ? 1 : 0);
                e->mantle_speed = (int32_t)(random() & 0xFF) + 0x100;
                int pdir = mantle_player_dir(e, soma);
                if ((dir_eff == 0) == (pdir == -1))
                    e->mantle_speed += 0x100 + (int32_t)(random() & 0x3F);
                if (e->timer) e->mantle_speed += 0x100;
                e->spin = 0;
            }
        }
        break;
    }
    default:
        break;
    }
}

/* sub_080BB518: the dash attack of state 2. Step 0 faces the player,
 * plays animation 3 once and sets + 0x1A bit 0 (the orbit blade's go).
 * Step 1 waits for its end, then plays animation 4, hops with
 * + 0x4C = sub_0802D5EC(0x2000) - 0x1A000 under + 0x54 = 0x1000,
 * seeds + 0x14 = 0x48000 + ((random & 0x1F) << 8), clears + 0x72 bit 1
 * (the contact is live for the dash) and plays song 0x89. Steps 2 and
 * 3 brake + 0x14 by 0x2000 then 0x1000 a frame, walk with the same
 * probes, and at animation 4's end leave the afterimage child
 * sub_080BB054, set + 0x72 bit 1 again, clear + 0x1A bit 1 and play
 * animation 5; at its end back to state 1 step 0. */
static void mantle_attack(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                          const AosEnemyKind *kind, uint32_t (*random)(void),
                          AosHitReport *spawned) {
    const AosAnimSet *anims = kind->anims;
    switch (e->step) {
    case 0:
        face_player(e, soma);
        play_loop(e, anims, 3, false);
        e->producer_timer |= 1;
        e->step = 1;
        break;
    case 1:
        if (!(e->anim.flags & AOS_ANIM_ENDED)) break;
        e->step = 2;
        play_loop(e, anims, 4, false);
        e->vy = random_centred(random, 0x2000) + (int32_t)0xFFFE6000;
        e->ay = 0x1000;
        e->mantle_speed = 0x48000 + ((int32_t)(random() & 0x1F) << 8);
        e->combat.attack_off = false;
        aos_audio_play_song(0x89);
        break;
    case 2:
        if (e->mantle_speed > 0) e->mantle_speed -= 0x2000;
        if (kind->probes.count)
            ground_walk(e, soma, layer, &kind->probes, e->mantle_speed, 0);
        if (!(e->anim.flags & AOS_ANIM_ENDED)) break;
        if (spawned->child_count < AOS_ENEMY_MAX_CHILDREN) {
            /* sub_0806DFF8(e, sub_080BB054): the afterimage follows the
             * parent from its own update. */
            AosEnemy g = {0};
            g.id = e->id;
            g.role = AOS_ROLE_MANTLE_GHOST;
            g.static_frame = -1;
            g.x = e->x;
            g.y = e->y;
            spawned->children[spawned->child_count++] = g;
        }
        e->combat.attack_off = true;
        e->producer_timer &= ~2;
        e->step = 3;
        play_loop(e, anims, 5, false);
        break;
    case 3:
        if (e->mantle_speed > 0) e->mantle_speed -= 0x1000;
        if (kind->probes.count)
            ground_walk(e, soma, layer, &kind->probes, e->mantle_speed, 0);
        if (e->anim.flags & AOS_ANIM_ENDED) {
            e->state = 1;
            e->substep = e->step = 0;
        }
        break;
    default:
        break;
    }
}

/* sub_080BB664: the death of state 3. Step 0 plays animation 6 once and
 * song 0x6D; step 1 counts + 0xD and every fourth tick emits the gore
 * drift of sub_08045CEC(x, y, 0x16, 8): a unit direction at a random
 * angle, acceleration vx / 0x18 and vy / 0x18, vflip set. Returns true
 * when the animation ends (the entity is deleted). */
static bool mantle_death(AosEnemy *e, const AosEnemyKind *kind,
                         uint32_t (*random)(void), AosHitReport *spawned) {
    if (e->step == 0) {
        play_loop(e, kind->anims, 6, false);
        e->step = 1;
        aos_audio_play_song(0x6D);
    }
    e->timer++;
    if ((e->timer & 3) == 0) {
        int32_t vx, vy;
        random_direction(random, &vx, &vy);
        AosEffect *g = spawn_drift(AOS_EFFECT_HIT_ELEMENT_5, e->x, e->y, spawned);
        if (g) {
            g->vx = vx;
            g->vy = vy;
            g->ax = vx / 0x18;
            g->ay = vy / 0x18;
            g->vflip = true;
        }
    }
    return (e->anim.flags & AOS_ANIM_ENDED) != 0;
}

/* sub_080BB054: the afterimage of the dash end. State 0 plays animation
 * 8 once (+ 0x5C = 0x14 and the + 0x5A merge are not ported); state 1
 * follows the parent 12 px to its back (negated by the facing) and 4 px
 * down, deleting when the animation ends. */
static void mantle_ghost_update(AosEnemy *g, const AosEnemy *parent,
                                const AosAnimSet *anims) {
    if (g->state == 0) {
        play_loop(g, anims, 8, false);
        g->state = 1;
    }
    if (!parent) return;
    mantle_follow(g, parent, -0xC, 4);
    step_anim(g, anims);
    if (g->anim.flags & AOS_ANIM_ENDED) g->removed = true;
}

/* sub_080BB0F0: the orbiting blade linked to its parent (+ 0x14). The
 * parent's state 3 ends it in state 4 (the + 0x4BE pause and the + 0x2D
 * hit flash freeze states 1-3 — only the flash is modelled). State 0
 * draws sub_0803E654 and deletes on a negative result (~half); else it
 * keeps still frame 0x12, hidden. States 0 and 1 fall through into the
 * next like the native jump table: state 1 waits for the parent's
 * + 0x1A bit 0 (set by the first attack), resets + 0x1C and + 0x1A =
 * 0xD000 and unhides; state 2 grows + 0x1C by 0x1000 a frame while the
 * bit holds, spins + 0x1A by 0x300 and orbits 8 px to the facing side —
 * on a cleared bit it counts + 0xD down from 8 into state 3, which
 * shrinks by 0x2000, spins by 0x200 and returns to state 1 hidden.
 * States 1-3 end with the affine sub_0803E058 (angle = + 0x1A negated
 * by the child's facing, scale = + 0x1C): mapped to `angle` and
 * `walk_timer` for the renderer; the scale itself is not drawn. */
static void mantle_orbit_update(AosEnemy *o, const AosEnemy *parent,
                                uint32_t (*random)(void)) {
    if (!parent) {
        o->removed = true;
        return;
    }
    if (parent->state == 3) o->state = 4;
    if (o->state != 0 && parent->hit_flash) return;
    if (o->state == 0) {
        if (random() & 0x8000) {
            o->removed = true;
            return;
        }
        /* The low 5 bits of + 0x58 (the fade) and the + 0x59 / + 0x5C
         * bookkeeping are not ported. */
        o->static_frame = 0x12;
        o->hidden = true;
        o->state = 1;
    }
    if (o->state == 1) {
        if (!(parent->producer_timer & 1)) return;
        o->walk_timer = 0;
        o->producer_timer = 0xD000;
        o->hidden = false;
        o->state = 2;
    }
    if (o->state == 2) {
        if (parent->producer_timer & 1) {
            if (o->walk_timer <= 0xFFFF) o->walk_timer += 0x1000;
            o->producer_timer = (uint16_t)(o->producer_timer + 0x300);
            mantle_follow(o, parent, 8, 0);
        } else {
            o->timer = 8;
            o->state = 3;
        }
    } else if (o->state == 3) {
        if (o->walk_timer > 0) o->walk_timer -= 0x2000;
        o->producer_timer = (uint16_t)(o->producer_timer + 0x200);
        mantle_follow(o, parent, 8, 0);
        if (o->timer) {
            o->timer--;
        } else {
            o->state = 1;
            o->hidden = true;
        }
    } else if (o->state == 4) {
        /* sub_0803E6BC fade then EntityDelete, same call. */
        o->removed = true;
        return;
    }
    /* The affine tail of states 1-3 (and the > 4 default): the angle is
     * + 0x1A sign-extended, negated by the child's + 0x58 bit 0x40. */
    uint16_t a = (uint16_t)(int16_t)o->producer_timer;
    o->angle = o->mirrored ? (uint32_t)(uint16_t)(-a) : a;
}

/* EnemyKillerMantleUpdate (sub_080BB824): inside the activity window
 * of sub_0806CC20, state 0 waits for the player in the 0xDC x 0xA0 box
 * of sub_0806E29C, state 1 patrols, state 2 attacks, state 3 dies
 * (deleted when sub_080BB664 ends) and state 4 only steps the
 * animation. States 0-3 then run the collision pass sub_0806E314 and
 * the animation step sub_0803F17C; state 4 skips both. */
static AosHitReport killer_mantle_update(AosEnemy *e, AosSoma *soma,
                                         const AosCollision *layer,
                                         const AosEnemyKind *kind,
                                         const AosWeaponEntity *weapon,
                                         const AosWeaponFrames *weapon_frames,
                                         int soma_atk, int soma_def, int cam_x,
                                         int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
    if (outside_margins(kind, 4, sx, sy)) {
        aos_combat_tick(&e->combat);
        return report;
    }
    e->hidden = outside_margins(kind, 4, sx, sy);
    /* + 0x2D counts down in the shared update path (sub_0806E314),
     * including the defeated mantle — the orbit child reads it. */
    if (e->hit_flash) e->hit_flash--;
    /* sub_0806DFF8(e, sub_080BB0F0) in the create: the orbit child is
     * emitted once, here, since a create cannot return children. */
    if (!e->boss_parts_spawned && spawned.child_count < AOS_ENEMY_MAX_CHILDREN) {
        AosEnemy o = {0};
        o.id = e->id;
        o.role = AOS_ROLE_MANTLE_ORBIT;
        o.static_frame = -1;
        o.x = e->x;
        o.y = e->y;
        spawned.children[spawned.child_count++] = o;
        e->boss_parts_spawned = true;
    }
    switch (e->state) {
    case 0:
        if (player_in_centred(e, soma, 0xDC, 0xA0)) {
            e->state = 1;
            e->substep = e->step = 0;
        }
        break;
    case 1:
        mantle_patrol(e, soma, layer, kind, random);
        break;
    case 2:
        mantle_attack(e, soma, layer, kind, random, &spawned);
        break;
    case 3:
        if (mantle_death(e, kind, random, &spawned)) e->removed = true;
        break;
    case 4:
        step_anim(e, kind->anims);
        aos_combat_tick(&e->combat);
        for (int i = 0; i < spawned.child_count &&
                        report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
            report.children[report.child_count++] = spawned.children[i];
        append_effects(&report, &spawned);
        return report;
    default:
        break;
    }
    if (!e->removed) {
        report = collide(e, soma, kind, weapon, weapon_frames, soma_atk,
                         soma_def);
        step_anim(e, kind->anims);
    }
    aos_combat_tick(&e->combat);
    for (int i = 0; i < spawned.child_count &&
                    report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
        report.children[report.child_count++] = spawned.children[i];
    append_effects(&report, &spawned);
    return report;
}

/* sub_080BF738: the thrown cleaver. State 0 (first update) shows still
 * frame 8 with the (-8, -4, 16, 8) boxes of sub_08042884 / sub_080428F0
 * (constant 0x0810FCF8), flies at +/- 0x28000 (2.5 px/frame) by the
 * facing, spins at -+0x1234, plays song 0x89, hp 1, contact element 1
 * (sub_0806E1E8) and the hurt callback sub_080BFF0C (any hit zeroes the
 * hit points — handled by the collide role list). State 1 accelerates at
 * 8.0, despawns beyond margin 3, dies into a 4 x 4 px flame of
 * sub_0806D5C0 when struck, and sticks on a solid cell 6 px ahead (the
 * sub_080020A0 probe): song 0x13A, hp 0, vx negated and halved plus a
 * random 0..0x7FFF, vy random - 0x18000, ay 0x2000, state 2. State 2
 * spins the affine angle (+ 0x18 += + 0x1A) while moving; state 3 and
 * the margin check delete it (the + 0x58 fade is cosmetic). */
static AosHitReport butcher_cleaver_update(AosEnemy *c, AosSoma *soma,
                                           const AosCollision *layer,
                                           const AosEnemyKind *kind,
                                           const AosWeaponEntity *weapon,
                                           const AosWeaponFrames *weapon_frames,
                                           int soma_atk, int soma_def, int cam_x,
                                           int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = collide(c, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
    aos_combat_tick(&c->combat);
    switch (c->state) {
    case 0:
        c->static_frame = 8;
        c->vx = c->mirrored ? 0x28000 : (int32_t)0xFFFD8000;
        c->vy = 0;
        c->contact_elements = 1;
        c->own_boxes = true;
        c->own_hurt = c->own_attack = (AosBox){-8, -4, 16, 8};
        c->hp = 1;
        c->producer_timer = c->mirrored ? (uint16_t)-0x1234 : 0x1234;
        aos_audio_play_song(0x89);
        c->state = 1;
        break;
    case 1:
        accel_move(c, 0x80000);
        if (outside_margins(kind, 3, (c->x >> 16) - cam_x, (c->y >> 16) - cam_y))
            c->state = 3;
        if (c->hp == 0) {
            c->state = 3;
            spawn_flame_at(c->x, c->y, 0x40000, 0x40000, random, &report);
        } else if (aos_collision_cell(layer,
                                      (int16_t)((c->x >> 16) + (c->mirrored ? 6 : -6)),
                                      (int16_t)(c->y >> 16)) & 1) {
            aos_audio_play_song(0x13A);
            c->hp = 0;
            (void)random();              /* the + 0x58 fade draw */
            int32_t half = (c->vx + (c->vx < 0 ? 1 : 0)) >> 1;
            c->vx = -(half + (int32_t)(random() & 0x7FFF));
            c->vy = random_centred(random, 0x80000) - 0x18000;
            c->ay = 0x2000;
            c->state = 2;
        }
        break;
    case 2:
        accel_move(c, 0x80000);
        c->angle = (c->angle + c->producer_timer) & 0xFFFF;
        if (outside_margins(kind, 3, (c->x >> 16) - cam_x, (c->y >> 16) - cam_y))
            c->state = 3;
        break;
    case 3:
        c->removed = true;
        return report;
    default:
        break;
    }
    return report;
}

/* sub_080BF990: a gore chunk. State 0 (first update) keeps the butcher's
 * still frame 8, deletes itself on the sub_0803E654 sign draw (about
 * half), fades in + 0x58 bits (cosmetic) and falls with ay 0x2000 from
 * angle 0xC000. State 1 accelerates at 8.0 and sticks on a solid cell
 * 4 px below (the sub_080020A0 probe): song 0x13A, spin
 * (r & 0x7FF) + 0xC00, vx 0x8000 + (r & 0x3FFF) and vy random, both
 * negated together on a second draw, state 2. State 2 spins and moves;
 * state 3 deletes it. It never collides. */
static AosHitReport butcher_gore_update(AosEnemy *g, AosSoma *soma,
                                        const AosCollision *layer,
                                        const AosEnemyKind *kind,
                                        const AosWeaponEntity *weapon,
                                        const AosWeaponFrames *weapon_frames,
                                        int soma_atk, int soma_def, int cam_x,
                                        int cam_y, uint32_t (*random)(void)) {
    (void)soma; (void)weapon; (void)weapon_frames; (void)soma_atk;
    (void)soma_def;
    AosHitReport report = {0};
    switch (g->state) {
    case 0:
        if ((int16_t)(random() & 0xFFFF) < 0) {
            g->removed = true;
            return report;
        }
        g->fade = (uint8_t)(random() & 0x1F);      /* + 0x58 low bits */
        g->static_frame = 8;
        g->vy = 0;
        g->ay = 0x2000;
        g->angle = 0xC000;
        g->state = 1;
        break;
    case 1:
        accel_move(g, 0x80000);
        if (outside_margins(kind, 3, (g->x >> 16) - cam_x, (g->y >> 16) - cam_y))
            g->state = 3;
        if (aos_collision_cell(layer, (int16_t)(g->x >> 16),
                               (int16_t)((g->y >> 16) + 4)) & 1) {
            aos_audio_play_song(0x13A);
            g->producer_timer = (uint16_t)((random() & 0x7FF) + 0xC00);
            g->vx = 0x8000 + (int32_t)(random() & 0x3FFF);
            if (random() & 1) {
                g->producer_timer = (uint16_t)-g->producer_timer;
                g->vx = -g->vx;
            }
            g->vy = (int32_t)(random() & 0xFFFF) - 0xA8000;
            g->state = 2;
        }
        break;
    case 2:
        accel_move(g, 0x80000);
        g->angle = (g->angle + g->producer_timer) & 0xFFFF;
        if (outside_margins(kind, 3, (g->x >> 16) - cam_x, (g->y >> 16) - cam_y))
            g->state = 3;
        break;
    case 3:
        g->removed = true;
        return report;
    default:
        break;
    }
    return report;
}

/* sub_080BF6B8: a death particle — still frame 9, then its + 0xD life
 * counts down (8..15 frames set by the spawner); gone when it wraps. */
static void butcher_particle_update(AosEnemy *p, const AosEnemyKind *kind) {
    (void)kind;
    if (p->state == 0) {
        p->static_frame = 9;
        p->state = 1;
    }
    if (p->timer == 0) p->removed = true;
    else p->timer--;
}

/* Dryad (EnemyDryadCreate 0x080AEAAC, EnemyDryadUpdate 0x080AEB8C): a
 * stationary spitter. State 0 sub_080AE4BC notices the player in the
 * (+ 0x42 - 0x6E, + 0x46 - 0x28) 0xDC x 0x50 box of sub_0806CFFC and on
 * RNG & 3 == 0 enters state 1 with animation 1 once. State 1
 * sub_080AE554 waits the animation end, then loops animation 2 and rolls
 * RNG & 7 == 0 while on screen (sub_08068AD4 margins; off screen it only
 * sleeps through + 0x59 bit 3 and a ledger clear — approximated by the
 * margin early-out) into animation 3 with timer 0x12; the last three
 * ticks play song 0x89 and call sub_080AE96C. State 2 waits the
 * animation end back to state 0 (no decoded writer enters it). State 3
 * sub_080AE650 is the death: timer 0x40, song 0x6E and a sub_0806D5C0
 * burst every eighth tick, the timer to zero deletes it; its affine
 * angle is + 0xD << 0xA while + 0x59 & 0x10 lasts (one turn). State 4
 * only animates. The hurt sub_080AE3FC deals the generic damage, +
 * 0x2D = 8, and on a lethal blow plays animation 5 (state 1, step != 0)
 * or 4, then state 3, timer 0x28, steps cleared. */

/* sub_080AE96C: the seed it spits — entity type 0x1B, role 0x48, update
 * sub_080AE7E4. Spawned 10..-5 px to the facing side and 11..26 px up
 * (RNG & 0xF offsets, the x flipped by the facing bit), its + 0x3C
 * attack byte is three quarters of the parent's. The native velocity is
 * distance-scaled (sub_0806CD38 / 64 corrections); approximated here as
 * a lob toward the player. */
static void dryad_spit(AosEnemy *e, AosSoma *soma, AosHitReport *report,
                       uint32_t (*random)(void)) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy seed = {0};
    seed.id = e->id;
    seed.role = AOS_ROLE_DRYAD_SEED;
    seed.static_frame = -1;
    seed.stats = e->stats;
    seed.stats.contact = (uint8_t)(e->stats.contact - (e->stats.contact >> 2));
    seed.contact_elements = 0;
    seed.mirrored = e->mirrored;
    int dx = (int)(random() & 0xF) - 10;
    if (e->mirrored) dx = -dx;
    seed.x = e->x + ((int32_t)dx << 16);
    seed.y = e->y + (((int32_t)(random() & 0xF) - 26) << 16);
    int32_t dist = target_x(soma) - seed.x;
    if (dist < 0) dist = -dist;
    int32_t dir = target_x(soma) > seed.x ? 1 : -1;
    seed.vx = dir * (0x8000 + (dist >> 6));
    seed.vy = -0x4000 - (int32_t)(random() & 0xF) * 0x200;
    report->children[report->child_count++] = seed;
}

static AosHitReport dryad_update(AosEnemy *e, AosSoma *soma,
                                 const AosCollision *layer,
                                 const AosEnemyKind *kind,
                                 const AosWeaponEntity *weapon,
                                 const AosWeaponFrames *weapon_frames,
                                 int soma_atk, int soma_def, int cam_x,
                                 int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    (void)layer;   /* the Dryad never moves after its create snap */
    int sx = (e->x >> 16) - cam_x, sy = (e->y >> 16) - cam_y;
    if (outside_margins(kind, 4, sx, sy)) {
        aos_combat_tick(&e->combat);
        return report;
    }
    /* + 0x2D counts down in the shared update path. */
    if (e->hit_flash) e->hit_flash--;
    switch (e->state) {
    case 0:
        /* sub_080AE4BC: the facing write of sub_0806CF2C into + 0x58
         * bit 6 every tick; step 0 runs the notice box, step 1 the
         * RNG & 3 wake. */
        face_player(e, soma);
        if (e->step == 0) {
            if (player_in(soma, (int16_t)((e->x >> 16) - 0x6E),
                          (int16_t)((e->y >> 16) - 0x28), 0xDC, 0x50))
                e->step = 1;
        } else if ((random() & 3) == 0) {
            e->state = 1;
            e->step = e->substep = 0;
            play_loop(e, kind->anims, 1, false);
        }
        break;
    case 1:
        if (e->step == 0) {
            if (e->anim.flags & AOS_ANIM_ENDED) {
                play_loop(e, kind->anims, 2, true);
                e->step = 1;
            }
        } else if (e->step == 1) {
            if (!outside_margins(kind, 4, sx, sy)) {
                if ((random() & 7) == 0) {
                    play_loop(e, kind->anims, 3, false);
                    e->timer = 0x12;
                    e->step = 2;
                }
            } else {
                /* Off screen: + 0x59 |= 8 sleeps it and the gEwramData
                 * ledger bit clears; approximated by hidden. */
                e->hidden = true;
            }
        } else {
            /* step 2: the last three ticks of the 0x12 timer spit. */
            if (e->timer && --e->timer <= 2) {
                aos_audio_play_song(0x89);
                dryad_spit(e, soma, &spawned, random);
            }
            if (e->anim.flags & AOS_ANIM_ENDED) {
                play_loop(e, kind->anims, 2, true);
                e->step = 1;
            }
        }
        break;
    case 2:
        if (e->anim.flags & AOS_ANIM_ENDED) {
            e->state = 0;
            e->step = e->substep = 0;
        }
        break;
    case 3: {
        /* sub_080AE650: + 0xB step 0 arms the 0x40 timer and, when the
         * sub_0803E654 draw's sign bit is clear, owes the affine turn
         * (+ 0x59 bit 0x10) and rewrites the + 0x58 facing nibble with
         * rand & 0x1F (cosmetic, not ported). Step 1 plays song 0x6E
         * and the sub_0806D5C0 drift burst while the timer is a
         * multiple of eight, marks + 0xF at 0x10 (the fade ledger,
         * not ported), counts down and deletes at zero. The shared tail
         * turns the sprite by timer << 0xA while the turn flag lasts
         * and runs the sub_0806D518 blink palette merge (not ported). */
        if (e->step == 0) {
            e->timer = 0x40;
            e->step = 1;
            if ((int32_t)(random() << 16) >= 0) e->dryad_flags |= 0x10;
        } else if (e->step == 1) {
            if ((e->timer & 7) == 0) {
                aos_audio_play_song(0x6E);
                for (int i = 0; i < 4; ++i) {
                    int32_t vx, vy;
                    random_direction(random, &vx, &vy);
                    AosEffect *g = spawn_drift(AOS_EFFECT_HIT_ELEMENT_5,
                                               e->x, e->y - 0x100000, &spawned);
                    if (g) {
                        g->vx = vx;
                        g->vy = vy;
                        g->ax = vx / 0x18;
                        g->ay = vy / 0x18;
                    }
                }
            }
            if (e->timer && --e->timer == 0) {
                e->removed = true;
                e->hidden = true;
                e->dryad_flags &= ~0x10;
            }
        }
        if (e->dryad_flags & 0x10) e->angle = (uint32_t)e->timer << 10;
        break;
    }
    case 4:
        step_anim(e, kind->anims);
        aos_combat_tick(&e->combat);
        for (int i = 0; i < spawned.child_count &&
                        report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
            report.children[report.child_count++] = spawned.children[i];
        append_effects(&report, &spawned);
        return report;
    default:
        break;
    }
    if (!e->removed) {
        report = collide(e, soma, kind, weapon, weapon_frames, soma_atk,
                         soma_def);
        step_anim(e, kind->anims);
    }
    aos_combat_tick(&e->combat);
    for (int i = 0; i < spawned.child_count &&
                    report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
        report.children[report.child_count++] = spawned.children[i];
    append_effects(&report, &spawned);
    return report;
}

/* The seed's landed tail shared by states 2 and 3 (sub_080AE8E8): x
 * tracks the player's, y the sub_0801CFB0 floor probe (snap_to_floor),
 * state 3 emits the sub_08021654 contact record (element 0, the + 0x3C
 * attack byte) every sixteenth tick and the timer deletes it. */
static void dryad_seed_landed(AosEnemy *s, AosSoma *soma,
                              const AosCollision *layer, int soma_def,
                              AosHitReport *report) {
    s->x = (s->x & 0xFFFF) | ((int32_t)(target_x(soma) >> 16) << 16);
    snap_to_floor(s, layer);
    /* The record is positional: it only reaches a player next to the
     * seed (the native hitbox extent is not traced — 16 px stands in). */
    if (s->state == 3 && (s->timer & 0xF) == 0 &&
        player_in(soma, (int16_t)((s->x >> 16) - 8),
                  (int16_t)((s->y >> 16) - 8), 16, 16)) {
        int dmg = aos_soma_take_elemental_hit(soma, s->stats.contact, soma_def,
                                              s->x, 0, s->contact_elements);
        if (dmg > 0) {
            report->soma_hit = true;
            report->soma_damage += dmg;
        }
    }
    if (s->timer && --s->timer == 0) s->removed = true;
}

/* sub_080AE7E4: state 0 (first update) registers the (-2, -2, 4, 4)
 * boxes of the 0x0404FEFE preset on both slots, animation 8 loop and
 * falls into state 1; state 1 integrates the velocity with 0x1000
 * gravity and the sub_080AE78C landing callback moves it to state 2
 * (timer 0xB4) — approximated by the floor probe; state 2 walks the
 * 0x08528604 table {6, 7, 0xFF} on the animation end then state 3
 * (timer 0x78); states 2 and 3 share the landed tail; sub_0806D128(1)
 * deletes it off screen. */
/* Tsuchinoko (EnemyTsuchinokoCreate 0x080A71CC, EnemyTsuchinokoUpdate
 * 0x080A7B54): a hopping snake. State 0 sub_080A7294 is a 13-step jump
 * table on + 0xB: step 0 faces the player and wakes into step 1 (anim 1
 * once) when the (+ 0x42 - 0x5A, + 0x46 - 0x64) 0xB4 x 0xC8 box of
 * sub_0806CFFC holds him or sub_0806D54C reports damage; step 1 at anim
 * frame 2 tick 0 picks the facing — the near 0x78 box inverts it, the
 * far 0xC8 box keeps it, outside both faces him — then hops (vy -1.5,
 * ay 0x2800, vx +/-1.0); step 2 sub_08069A00 walks it with bounce 1.0,
 * and on the ground bit rolls the landed tree: two RNG & 0x7F <= 0xF
 * gates to the state 1 attack or the step 0xA burrow sequence, then the
 * + 0x14 / + 0x34 HP thresholds pick a re-hop or the burrow. Steps 0xA
 * to 0xC play anims 2 and 3 into the dormant rock spitter, which dies
 * when its animation ends. State 1 sub_080A7534 rears (anim 2) and at
 * frame 3 tick 0 spits the fast lob of sub_080A776C with song 0x85,
 * then re-enters the hop at step 1. State 2 sub_080A75B0 is the death:
 * two sub_0806DD64 gore chunks, a 0x3C timer, the sub_0806D518 blink
 * and delete. State 3 only animates. The hurt sub_080A76E0 deals the
 * generic damage, + 0x2D = 8, and on a lethal blow plays the
 * sub_0803F0E4 death table 0x085283FC (a custom frame sequence not
 * ported — the last animation keeps running), song 0x70, timer 0x3C,
 * state 2. */

/* sub_080A776C: the fast spit — entity type 0x1B, role 0x48, update
 * sub_080A79D8, animation 4 looping. It spawns at the parent's pixel
 * position 0x1A pixels up, inherits the facing bit, flies at +/-3.0
 * with vy -1.0 under ay 0x1800 and copies the + 0x37 / + 0x2C / + 0x3C
 * bytes (the contact damage base). Its sub_080A785C installs the
 * 0x0808FCFC box preset with the sub_080A7A54 contact (the + 0x3C
 * damage byte, element 0x40, then + 0x59 |= 8) and the sub_080A7A90
 * hurt (any hit deletes it). */
static void tsuchinoko_spit(AosEnemy *e, AosHitReport *report) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy s = {0};
    s.id = e->id;
    s.role = AOS_ROLE_TSUCHINOKO_SPIT;
    s.static_frame = -1;
    s.stats = e->stats;
    s.contact_elements = 0x40;
    s.mirrored = e->mirrored;
    s.x = (e->x >> 16) << 16;
    s.y = ((e->y >> 16) - 0x1A) << 16;
    s.vx = e->mirrored ? 0x30000 : (int32_t)0xFFFD0000;
    s.vy = (int32_t)0xFFFF0000;
    s.ay = 0x1800;
    report->children[report->child_count++] = s;
}

/* sub_080A78A4: the dormant burrower's rock — entity type 0x1B, role
 * 0x48, update sub_080A7AAC, still frame 0x12 (+ 0x65), + 0x72 |= 6.
 * Random mirrored and vflip bits, the parent's pixel position, vx
 * (RNG & 0x1FFFF) - 1.0 and vy -1.0 - (RNG & 0xFFFF) under ay 0x1800.
 * No contact or hurt callbacks: it is harmless and cannot be struck. */
static void tsuchinoko_rock(AosEnemy *e, AosHitReport *report,
                            uint32_t (*random)(void)) {
    if (report->child_count >= AOS_ENEMY_MAX_CHILDREN) return;
    AosEnemy s = {0};
    s.id = e->id;
    s.role = AOS_ROLE_TSUCHINOKO_ROCK;
    s.static_frame = 0x12;
    s.stats = e->stats;
    s.mirrored = (random() & 1) != 0;
    s.vflip = (random() & 1) != 0;
    s.x = (e->x >> 16) << 16;
    s.y = (e->y >> 16) << 16;
    s.vx = (int32_t)(random() & 0x1FFFF) - 0x10000;
    s.vy = (int32_t)0xFFFF0000 - (int32_t)(random() & 0xFFFF);
    s.ay = 0x1800;
    s.combat.attack_off = s.combat.hurt_off = true;
    report->children[report->child_count++] = s;
}

/* sub_080A7294: state 0, the hop state machine. */
static void tsuchinoko_ground(AosEnemy *e, AosSoma *soma,
                              const AosCollision *layer,
                              const AosEnemyKind *kind, AosHitReport *spawned,
                              uint32_t (*random)(void)) {
    const AosAnimSet *anims = kind->anims;
    int px = (int16_t)(e->x >> 16), py = (int16_t)(e->y >> 16);
    switch (e->step) {
    case 0:
        /* The facing write of sub_0806CF2C into + 0x58 bit 6; the wake
         * is the notice box or the damage flag of sub_0806D54C. */
        face_player(e, soma);
        if (player_in(soma, px - 0x5A, py - 0x64, 0xB4, 0xC8) ||
            e->hp < e->stats.hp) {
            e->step = 1;
            play_loop(e, anims, 1, false);
        }
        break;
    case 1:
        if (e->anim.frame == 2 && e->anim.tick == 0) {
            /* Near box: face away; far box hit: keep the facing;
             * outside both: face the player. */
            if (player_in(soma, px - 0x3C, py - 0x64, 0x78, 0xC8)) {
                e->mirrored = !(px < (int16_t)(target_x(soma) >> 16));
            } else if (!player_in(soma, px - 0x64, py - 0x64, 0xC8, 0xC8)) {
                face_player(e, soma);
            }
            e->step = 2;
            e->vy = (int32_t)0xFFFE8000;
            e->ay = 0x2800;
            e->vx = e->mirrored ? 0x10000 : (int32_t)0xFFFF0000;
        }
        break;
    case 2: {
        /* sub_08069A00: walk_collide with bounce 1.0, no head offset,
         * speed cap 8.0 and half width 8; only the ground bit matters. */
        int result = walk_collide(e, layer, 0x10000, 0, 0x80000, 8);
        if (!(result & 4)) break;
        if ((random() & 0x7F) <= 0xF) {
            e->state = 1;
            e->step = 0;
            break;
        }
        if ((random() & 0x7F) <= 0xF) {
            e->step = 0xA;
            break;
        }
        bool wounded = e->hp_home / 2 >= e->hp;
        if (wounded && (random() & 0x7F) <= 0x1F) {
            e->step = 0xA;
            break;
        }
        /* _080A7468: the quarter-HP gate, then the last RNG draw. */
        if (e->hp_home / 4 < e->hp || (random() & 0x7F) > 0x3F) {
            play_loop(e, anims, 1, false);
            e->step = 1;
        } else {
            e->step = 0xA;
        }
        break;
    }
    case 0xA:
        play_loop(e, anims, 2, false);
        face_player(e, soma);
        e->step = 0xB;
        break;
    case 0xB:
        if (e->anim.frame == 3 && e->anim.tick == 0x14) {
            play_loop(e, anims, 3, false);
            e->step = 0xC;
        }
        break;
    case 0xC:
        /* The dormant spitter: past frame 1 its timer counts and every
         * sixteenth tick launches a rock with song 0x152; the
         * animation end deletes it (+ 0x59 |= 8). */
        if (e->anim.frame > 1) {
            uint8_t old = e->timer;
            e->timer = (uint8_t)(old + 1);
            if ((old & 0xF) == 0) {
                tsuchinoko_rock(e, spawned, random);
                aos_audio_play_song(0x152);
            }
        }
        if (e->anim.flags & AOS_ANIM_ENDED) e->removed = true;
        break;
    default:
        break;
    }
}

/* sub_080A7534: state 1, the rear-up attack. */
static void tsuchinoko_attack(AosEnemy *e, AosSoma *soma,
                              const AosEnemyKind *kind, AosHitReport *spawned,
                              uint32_t (*random)(void)) {
    (void)random;
    const AosAnimSet *anims = kind->anims;
    if (e->step == 0) {
        play_loop(e, anims, 2, false);
        face_player(e, soma);
        e->step = 1;
    } else {
        if (e->anim.frame == 3 && e->anim.tick == 0) {
            tsuchinoko_spit(e, spawned);
            aos_audio_play_song(0x85);
        }
        if (e->anim.flags & AOS_ANIM_ENDED) {
            /* Back to the hop launch: state 0, step 1 (the entry step),
             * animation 1 once. */
            e->state = 0;
            e->step = 1;
            play_loop(e, anims, 1, false);
        }
    }
}

/* sub_080A75B0: state 2, the death — vx = vy = 0, ay 0x1000, a gore
 * chunk of sub_0806DD64 at timer 0 and again at 0x34, the sub_0806D3D8
 * move capped at 0.5, the sub_0806D518 blink (not ported) and the
 * delete at timer 0. */
static void tsuchinoko_die(AosEnemy *e, AosHitReport *spawned,
                           uint32_t (*random)(void)) {
    if (e->step == 0) {
        e->vx = e->vy = 0;
        e->ay = 0x1000;
        int32_t ox = (int32_t)(random() & 0x7FFFF) - 0x40000;
        int32_t oy = (int32_t)(random() & 0x7FFFF) - 0x40000;
        int32_t vx = (int32_t)(random() & 0x1FFFF) - 0x10000;
        int32_t vy = (int32_t)(random() & 0x1FFFF) - 0x10000;
        spawn_particle(AOS_EFFECT_BLOOD, 0x11, e->x + ox, e->y + oy,
                       vx, vy, spawned);
        e->timer = 0x3C;
        e->step = 1;
    } else {
        if (e->timer == 0x34) {
            int32_t ox = (int32_t)(random() & 0x7FFFF) - 0x40000;
            int32_t oy = (int32_t)(random() & 0x7FFFF) - 0x40000;
            int32_t vx = (int32_t)(random() & 0x1FFFF) - 0x10000;
            int32_t vy = (int32_t)(random() & 0x1FFFF) - 0x10000;
            spawn_particle(AOS_EFFECT_BLOOD, 0x11, e->x + ox, e->y + oy,
                           vx, vy, spawned);
        }
        accel_move(e, 0x8000);
        if (e->timer && --e->timer == 0) e->removed = true;
    }
}

static AosHitReport tsuchinoko_update(AosEnemy *e, AosSoma *soma,
                                      const AosCollision *layer,
                                      const AosEnemyKind *kind,
                                      const AosWeaponEntity *weapon,
                                      const AosWeaponFrames *weapon_frames,
                                      int soma_atk, int soma_def, int cam_x,
                                      int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    (void)cam_x; (void)cam_y;
    /* sub_0806AE54: + 0x2D counts down outside the death state. */
    if (e->state != 2 && e->hit_flash) e->hit_flash--;
    switch (e->state) {
    case 0:
        tsuchinoko_ground(e, soma, layer, kind, &spawned, random);
        break;
    case 1:
        tsuchinoko_attack(e, soma, kind, &spawned, random);
        break;
    case 2:
        tsuchinoko_die(e, &spawned, random);
        break;
    case 3:
        /* Animation only (sub_0803F17C), no collision pass. */
        step_anim(e, kind->anims);
        aos_combat_tick(&e->combat);
        for (int i = 0; i < spawned.child_count &&
                        report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
            report.children[report.child_count++] = spawned.children[i];
        append_effects(&report, &spawned);
        return report;
    default:
        break;
    }
    if (!e->removed) {
        report = collide(e, soma, kind, weapon, weapon_frames, soma_atk,
                         soma_def);
        step_anim(e, kind->anims);
    }
    aos_combat_tick(&e->combat);
    for (int i = 0; i < spawned.child_count &&
                    report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
        report.children[report.child_count++] = spawned.children[i];
    append_effects(&report, &spawned);
    return report;
}

/* sub_080A79D8 (the spit) and sub_080A7AAC (the rock): state 0 moves
 * with sub_0806D3D8 capped at 4.0 and deletes off screen through
 * sub_0806D128(1). Only the spit refreshes its animation and runs the
 * collision pass — its sub_080A7A54 contact deals the + 0x3C damage at
 * element 0x40 and deletes it; its sub_080A7A90 hurt deletes it on any
 * hit (handled in collide). The rock has no callbacks at all. */
static AosHitReport tsuchinoko_child_update(AosEnemy *s, AosSoma *soma,
                                            const AosCollision *layer,
                                            const AosEnemyKind *kind,
                                            const AosWeaponEntity *weapon,
                                            const AosWeaponFrames *weapon_frames,
                                            int soma_atk, int soma_def,
                                            int cam_x, int cam_y,
                                            uint32_t (*random)(void)) {
    (void)layer; (void)random;
    AosHitReport report = {0};
    if (s->state == 0) {
        accel_move(s, 0x40000);
        int sx = (s->x >> 16) - cam_x, sy = (s->y >> 16) - cam_y;
        if (outside_margins(kind, 1, sx, sy)) {
            s->removed = true;
            return report;
        }
    }
    if (s->role == AOS_ROLE_TSUCHINOKO_SPIT) {
        step_anim(s, kind->anims);
        report = collide(s, soma, kind, weapon, weapon_frames, soma_atk,
                         soma_def);
        if (report.soma_hit) s->removed = true;
    }
    aos_combat_tick(&s->combat);
    return report;
}

static AosHitReport dryad_seed_update(AosEnemy *s, AosSoma *soma,
                                      const AosCollision *layer,
                                      const AosEnemyKind *kind,
                                      const AosWeaponEntity *weapon,
                                      const AosWeaponFrames *weapon_frames,
                                      int soma_atk, int soma_def, int cam_x,
                                      int cam_y, uint32_t (*random)(void)) {
    (void)random;
    AosHitReport report = {0}, landed = {0};
    int sx = (s->x >> 16) - cam_x, sy = (s->y >> 16) - cam_y;
    if (outside_margins(kind, 4, sx, sy)) {
        s->removed = true;
        return report;
    }
    if (s->state == 0) {
        s->own_boxes = true;
        s->own_hurt = s->own_attack = (AosBox){-2, -2, 4, 4};
        play_loop(s, kind->anims, 8, true);
        s->state = 1;
    }
    switch (s->state) {
    case 1:
        s->x += s->vx;
        s->y += s->vy;
        s->vy += 0x1000;
        if (s->vy > 0 &&
            aos_floor_depth(layer, s->x >> 16, (s->y >> 16) + 4, 0, false) < 0) {
            snap_to_floor(s, layer);
            s->vx = s->vy = 0;
            s->state = 2;
            s->timer = 0xB4;
            /* The landed mine damages through the periodic record, not
             * its box. */
            s->combat.attack_off = true;
        }
        break;
    case 2:
        /* Table 0x08528604 {6, 7, 0xFF}: animation 6 then 7 once each,
         * then state 3 with timer 0x78. */
        if (s->step == 0) {
            play_loop(s, kind->anims, 6, false);
            s->step = 1;
        } else if (s->step == 1) {
            if (s->anim.flags & AOS_ANIM_ENDED) {
                play_loop(s, kind->anims, 7, false);
                s->step = 2;
            }
        } else if (s->anim.flags & AOS_ANIM_ENDED) {
            s->state = 3;
            s->timer = 0x78;
        }
        dryad_seed_landed(s, soma, layer, soma_def, &landed);
        break;
    case 3:
        dryad_seed_landed(s, soma, layer, soma_def, &landed);
        break;
    default:
        break;
    }
    if (!s->removed) {
        report = collide(s, soma, kind, weapon, weapon_frames, soma_atk,
                         soma_def);
        step_anim(s, kind->anims);
    }
    report.soma_hit = report.soma_hit || landed.soma_hit;
    report.soma_damage += landed.soma_damage;
    aos_combat_tick(&s->combat);
    return report;
}

/* The Kicker Skeleton's attack machine (sub_080BFFF0): + 0x0B is its
 * substate, + 0x18 the walker speed (`kicker_speed`), + 0x14 the contact
 * flags (`kicker_flags`: bit 0 the strike's x5/4 contact, bit 1 cleared on
 * landing), + 0x20 the death direction (`away`), + 0x0D the gore counter
 * (`timer`). The two sub_080458E4 strike zones (0x30, 0xC, 0x11) and
 * (0x30, 6, 0x10) are approximated by the exported frame attack boxes;
 * the sub_08021248 script slot of a type-3 contact is not ported. */
static int kicker_walk(AosEnemy *e, AosSoma *soma, const AosCollision *layer,
                       const AosEnemyKind *kind) {
    return ground_walk(e, soma, layer, &kind->probes, e->kicker_speed, 0);
}

static void kicker_idle(AosEnemy *e, const AosEnemyKind *kind) {
    play_loop(e, kind->anims, 0, true);
    e->state = 0;
    e->step = e->substep = 0;
}

/* sub_080C0250: the death — song 0x6B and seven sub_0806C5AC pieces
 * (AOS_ROLE_DEBRIS) flung from its position, then the entity deletes
 * itself the same frame. */
static void kicker_shatter(AosEnemy *e, const AosEnemyKind *kind,
                           AosHitReport *report, uint32_t (*random)(void)) {
    aos_audio_play_song(0x6B);
    for (int i = 0; i < 7 && report->child_count < AOS_ENEMY_MAX_CHILDREN; ++i) {
        AosEnemy d = {0};
        d.id = e->id;
        d.role = AOS_ROLE_DEBRIS;
        d.static_frame = -1;
        d.x = e->x;
        d.y = e->y;
        d.mirrored = e->mirrored;
        d.combat.attack_off = d.combat.hurt_off = true;
        d.vx = (((int32_t)(random() & 7) << 14) + 0x8000) * e->away;
        d.vy = ((int32_t)((random() & 0xF) - 8) << 12) - 0x28000;
        d.ay = 0x2000;
        if (e->anim.id < AOS_ENEMY_MAX_ANIMS &&
            e->anim.frame < AOS_ENEMY_MAX_FRAMES &&
            kind->sprites[e->anim.id][e->anim.frame])
            d.static_frame = kind->sprites[e->anim.id][e->anim.frame];
        report->children[report->child_count++] = d;
    }
    e->removed = true;
}

static AosHitReport kicker_skeleton_update(AosEnemy *e, AosSoma *soma,
                                           const AosCollision *layer,
                                           const AosEnemyKind *kind,
                                           const AosWeaponEntity *weapon,
                                           const AosWeaponFrames *weapon_frames,
                                           int soma_atk, int soma_def,
                                           int cam_x, int cam_y,
                                           uint32_t (*random)(void)) {
    (void)cam_x; (void)cam_y;
    AosHitReport report = {0}, spawned = {0};
    switch (e->state) {
    case 0:
        /* Idle: only while + 0x0B is 0, a 1/64 fidget animation, then the
         * attack on the player in the 0xC8 x 0x50 centred box or on any
         * damage. */
        if (e->step == 0) {
            if (!(random() & 0x3F))
                play_loop(e, kind->anims, random() & 1, true);
            if (player_in_centred(e, soma, 0xC8, 0x50) || e->hp < e->stats.hp) {
                e->state = 1;
                e->step = e->substep = 0;
            }
        }
        break;
    case 1:
        /* sub_080BFFF0's jump table on + 0x0B; case 0 falls through into
         * case 1, facing and playing animation 3 in the same call. */
        switch (e->step) {
        case 0:
            face_player(e, soma);
            play_loop(e, kind->anims, 3, false);
            e->step = 2;
            break;
        case 1:
            play_loop(e, kind->anims, 3, false);
            e->step = 2;
            break;
        case 2:
            if (!(e->anim.flags & AOS_ANIM_ENDED)) break;
            play_loop(e, kind->anims, 4, false);
            e->kicker_speed = 0x8000;
            e->vy = -0x30000;
            e->ay = 0x1000;
            e->step = 3;
            break;
        case 3: {
            int result = kicker_walk(e, soma, layer, kind);
            if (result & 4) {
                e->step = 7;
            } else if (e->vy >= -0x18000) {
                face_player(e, soma);
                play_loop(e, kind->anims, 6, false);
                e->step = 4;
            }
            break;
        }
        case 4:
            kicker_walk(e, soma, layer, kind);
            if (e->anim.frame != 5) break;
            /* The two sub_080458E4 strike zones (0x30, 0xC, 0x11) and
             * (0x30, 6, 0x10) are approximated by the frame attack boxes. */
            play_loop(e, kind->anims, 5, false);
            e->kicker_speed = 0x28000;
            e->vy = 0x30000;
            e->kicker_flags |= 1;
            aos_audio_play_song(0x87);
            e->step = 5;
            break;
        case 5: {
            int result = kicker_walk(e, soma, layer, kind);
            if (!(result & 8)) break;
            play_loop(e, kind->anims, 3, false);
            e->kicker_flags &= ~2u;
            aos_audio_play_song(0x127);
            e->step = 6;
            break;
        }
        case 6: {
            int result = kicker_walk(e, soma, layer, kind);
            e->kicker_speed = e->kicker_speed * 0x1F / 0x20;
            if (e->kicker_speed <= 0x7FFF) {
                kicker_idle(e, kind);
                break;
            }
            if (!(result & 8)) break;
            if (result & 3) e->kicker_speed = 0;
            if (e->kicker_speed <= 0xBFFF) break;
            e->timer++;
            if (e->timer & 3) break;
            /* The skid's gore: sub_08045CEC animation 6, row 4 or 5. */
            AosEffect *gore = spawn_drift(AOS_EFFECT_BLOOD,
                e->x + ((int32_t)random_centred(random, 0x10) << 16),
                e->y + 0xC0000 + ((int32_t)random_centred(random, 4) << 16),
                &spawned);
            if (gore) {
                gore->vx = e->mirrored ? -0x8000 : 0x8000;
                gore->ay = -(int32_t)(random() & 0x3FF);
            }
            break;
        }
        case 7: {
            int result = kicker_walk(e, soma, layer, kind);
            if (result & 8) kicker_idle(e, kind);
            break;
        }
        default:
            break;
        }
        break;
    case 2:
        kicker_shatter(e, kind, &spawned, random);
        break;
    case 3:
        /* Flag state: sub_0803F17C only — animation, no collision. */
        step_anim(e, kind->anims);
        aos_combat_tick(&e->combat);
        for (int i = 0; i < spawned.child_count &&
                        report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
            report.children[report.child_count++] = spawned.children[i];
        append_effects(&report, &spawned);
        return report;
    default:
        break;
    }
    if (!e->removed) {
        report = collide(e, soma, kind, weapon, weapon_frames, soma_atk,
                         soma_def);
        step_anim(e, kind->anims);
    }
    aos_combat_tick(&e->combat);
    for (int i = 0; i < spawned.child_count &&
                    report.child_count < AOS_ENEMY_MAX_CHILDREN; ++i)
        report.children[report.child_count++] = spawned.children[i];
    append_effects(&report, &spawned);
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
        return enemy_ghost_update(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def,
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
        return sky_fish_update(enemy, soma, kind, weapon, weapon_frames,
                               soma_atk, soma_def, cam_x, cam_y, random);
    if (enemy->id == AOS_ENEMY_ECTOPLASM && enemy->role == AOS_ROLE_ENEMY)
        return ectoplasm_update(enemy, soma, kind, weapon, weapon_frames,
                                soma_atk, soma_def, cam_x, cam_y, random);
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
