/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies; comments name the cvaos routine. */
#include "aos_enemy.h"

#include <math.h>


static uint32_t random_state;

uint32_t aos_random(void) {
    return random_state = (random_state >> 8) * 0x3243F6ADu + 0x1B0CB175u;
}

void aos_random_seed(uint32_t seed) {
    random_state = seed;
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

static void play_loop(AosEnemy *enemy, const AosAnimSet *anims, unsigned id, bool loop) {
    aos_anim_start(&enemy->anim, anims, id, loop);
    enemy->anim.id = (uint8_t)id;
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
    int32_t y0 = zombie->y;
    snap_to_floor(zombie, layer);
    zombie->state = 0;
    zombie->mirrored = (zombie->x >> 16) < (soma->x >> 16);
    if (on_slope(layer, zombie->x >> 16, zombie->y >> 16)) zombie_vanish(zombie);
    if (zombie->y <= y0 - 0x80000) zombie_vanish(zombie);
}

bool aos_enemy_create(AosEnemy *enemy, uint8_t id, int32_t x, int32_t y, int16_t param0,
                      int16_t param1, const AosSoma *soma, const AosCollision *layer,
                      const AosEnemyKind *kind, const AosEnemyStats *stats) {
    if (id != AOS_ENEMY_BAT && id != AOS_ENEMY_ZOMBIE && id != AOS_ENEMY_BLUE_CROW) return false;
    const AosAnimSet *anims = kind->anims;
    *enemy = (AosEnemy){.id = id, .x = (int32_t)((uint32_t)x << 16), .y = (int32_t)((uint32_t)y << 16),
                        .param0 = param0, .param1 = param1};
    /* sub_0800F1FC: mirrored when the record lies left of the player. */
    enemy->mirrored = enemy->x < soma->x;
    /* sub_0806B04C / sub_0806D244: stats and the type 8 collision block. */
    enemy->stats = *stats;
    enemy->hp = stats->hp;
    enemy->combat.type = AOS_TYPE_ENEMY;
    if (id == AOS_ENEMY_ZOMBIE) {
        zombie_create(enemy, soma, layer, anims);
        return true;
    }
    if (id == AOS_ENEMY_BLUE_CROW) {
        /* EnemyBlueCrowCreate: animation 0, state 1 (state 3 under the
         * global 0x8E & 0x40 is not ported). */
        play(enemy, anims, 0);
        enemy->state = 1;
        return true;
    }
    /* EnemyBatCreate: hang from the ceiling above the record. */
    play(enemy, anims, 0);
    aos_anim_step(&enemy->anim, anims);
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

/* sub_080AD5B8: the bat falls, blinks (sub_0806BE74) and is deleted. The
 * explosion particles (every 15 frames) and sub_080683BC are not ported. */
static void bat_die(AosEnemy *bat, const AosEnemyKind *kind) {
    if (bat->step == 0) {
        aos_anim_start(&bat->anim, kind->anims, 2, true);
        bat->anim.id = 2;
        bat->anim.frame = 2;
        bat->vy = 0x8000;
        bat->vflip = true;
        bat->timer = 0x40;
        bat->step = 1;
    }
    bat->y += bat->vy;
    bat->hidden = bat->timer <= 0x27 ? kind->blink[bat->timer] & 1 : false;
    if ((int8_t)--bat->timer <= 0) bat->removed = true;
}

/* sub_0806B1FC: the boxes of the current animation frame. */
static void frame_boxes(const AosEnemy *enemy, const AosEnemyKind *kind, AosBox *hurt,
                        AosBox *attack, bool *hurt_on, bool *attack_on) {
    unsigned a = enemy->anim.id, f = enemy->anim.frame;
    *hurt_on = *attack_on = false;
    if (a >= AOS_ENEMY_MAX_ANIMS || f >= AOS_ENEMY_MAX_FRAMES || !kind->modes[a][f]) return;
    *hurt = kind->hurt[a][f];
    *attack = kind->attack[a][f];
    *hurt_on = *attack_on = true;
}

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
    if (attack_on && aos_combat_can_take(&soma->combat, AOS_TYPE_ENEMY)) {
        AosRect player = aos_player_rect(soma->hurtbox, soma->x >> 16, soma->y >> 16,
                                         soma->facing_left, false);
        if (aos_rect_hits_box(player, attack, ex, ey, enemy->mirrored, enemy->vflip)) {
            aos_combat_take(&soma->combat, AOS_TYPE_ENEMY,
                            aos_combat_cooldown(AOS_TYPE_ENEMY, 0));
            report.soma_hit = true;
        }
    }
    /* The weapon's attack box against the enemy's hurtbox (sub_08041864). */
    int wx, wy, ww, wh;
    if (hurt_on && weapon && weapon_frames &&
        aos_combat_can_take(&enemy->combat, AOS_TYPE_WEAPON) &&
        aos_weapon_hitbox(weapon, soma, weapon_frames, &wx, &wy, &ww, &wh)) {
        AosRect target = aos_entity_rect(hurt, ex, ey, enemy->mirrored, enemy->vflip);
        AosRect blade = {(int16_t)wx, (int16_t)wy, (int16_t)(wx + ww - 1), (int16_t)(wy + wh - 1)};
        if (ww > 1 && wh > 1 && blade.x1 <= target.x2 && target.x1 <= blade.x2 &&
            blade.y1 <= target.y2 && target.y1 <= blade.y2) {
            aos_combat_take(&enemy->combat, AOS_TYPE_WEAPON,
                            aos_combat_cooldown(AOS_TYPE_WEAPON, soma->weapon.interval));
            /* sub_0802346C (weapon callback): the kick-hit flag. */
            soma->flags |= AOS_FLAG_KICK_HIT;
            /* sub_0806E218 with sub_08021530(3) = ATK * 16 / 16. */
            report.enemy_hit = true;
            report.enemy_damage = aos_enemy_damage(soma_atk, enemy->stats.defence,
                                                   enemy->stats.weak, enemy->stats.resist,
                                                   soma->weapon.flags);
            enemy->hit_flash = 8;
            enemy->hp = (int16_t)(enemy->hp - report.enemy_damage);
            if (enemy->hp <= 0) {
                report.killed = true;
                if (enemy->id == AOS_ENEMY_BAT) {
                    /* sub_080AD6E4: the bat dies. */
                    enemy->state = 3;
                    enemy->step = enemy->substep = 0;
                } else if (enemy->id == AOS_ENEMY_BLUE_CROW) {
                    /* sub_080CA030: the crow dies. */
                    enemy->state = 2;
                    enemy->step = enemy->substep = 0;
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
    /* The contact callback sub_0806E1B8 -> sub_08021654 (type 0). */
    if (report.soma_hit)
        report.soma_damage = aos_soma_take_hit(soma, enemy->stats.contact, soma_def, enemy->x, 0);
    return report;
}

/* sub_0806AEAC: the generic death, a blink then the deletion (the
 * particles every 4 frames are not ported). */
static void generic_death(AosEnemy *enemy, const AosEnemyKind *kind) {
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

/* sub_0807AD28: rise, walk in bursts, sink. The dust of sub_0807B1CC /
 * sub_0807B33C is not ported. */
static void zombie_walk(AosEnemy *z, const AosSoma *soma, const AosCollision *layer,
                        const AosAnimSet *anims, uint32_t (*random)(void)) {
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
    AosHitReport report = {0};
    if (z->state != 3) {
        /* sub_0806D128(e, 4): out of the screen margins, it vanishes. */
        int sx = (z->x >> 16) - cam_x, sy = (z->y >> 16) - cam_y;
        if (sx < -kind->margin_x || sx > kind->margin_x + 0xF0 || sy < -kind->margin_y ||
            sy > kind->margin_y + 0xA0)
            zombie_vanish(z);
    }
    switch (z->state) {
    case 0: zombie_walk(z, soma, layer, kind->anims, random); break;
    case 1: generic_death(z, kind); break;
    case 3: zombie_spawner(z, soma, cam_x, random, &report); return report;
    default: return report;
    }
    if (z->removed) return report;
    step_anim(z, kind->anims);
    z->attack_off = z->state == 0 && (z->step == 0 || z->step == 3);
    int sx = (z->x >> 16) - cam_x, sy = (z->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
        report = collide(z, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    aos_combat_tick(&z->combat);
    return report;
}

/* sub_0806BF78: the player within `range` of the entity on both axes. */
static bool player_within(const AosEnemy *e, const AosSoma *soma, uint32_t range) {
    int32_t dx = e->x - soma->x, dy = e->y - soma->y;
    return (uint32_t)(dx < 0 ? -dx : dx) <= range && (uint32_t)(dy < 0 ? -dy : dy) <= range;
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

/* sub_080C9D68: animation 3, a fall at 0.5 per frame and the blink of
 * sub_0806BE74 over 64 frames, then the deletion. Sound 0x72, the two
 * feathers of sub_080C9E2C / sub_080C9ED4, the palette bits of + 0x5A, the
 * particles and sub_080683BC are not ported. */
static void crow_die(AosEnemy *c, const AosEnemyKind *kind) {
    if (c->step == 0) {
        play_loop(c, kind->anims, 3, false);
        stop(c);
        c->vy = 0x8000;
        c->timer = 0x40;
        c->step = 1;
        c->substep = 0;
    }
    c->y += c->vy;
    c->hidden = c->timer <= 0x27 ? kind->blink[c->timer] & 1 : false;
    if ((int8_t)--c->timer <= 0) {
        c->removed = true;
        return;
    }
    step_anim(c, kind->anims);
}

/* EnemyBlueCrowUpdate: no activity window and no on-screen test before the
 * collision pass (sub_080421AC). The hit stun of sub_0806AD24 is not
 * ported. */
static AosHitReport crow_update(AosEnemy *c, AosSoma *soma, const AosEnemyKind *kind,
                                const AosWeaponEntity *weapon, const AosWeaponFrames *weapon_frames,
                                int soma_atk, int soma_def) {
    AosHitReport report = {0};
    if (c->state == 2) {
        crow_die(c, kind);
    } else {
        crow_fly(c, soma, kind->anims);
        step_anim(c, kind->anims);
        report = collide(c, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    }
    aos_combat_tick(&c->combat);
    return report;
}

AosHitReport aos_enemy_update(AosEnemy *enemy, AosSoma *soma, const AosCollision *layer,
                              const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                              const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
                              int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0};
    const AosAnimSet *anims = kind->anims;
    if (enemy->removed) return report;
    if (enemy->hit_flash) enemy->hit_flash--;
    if (enemy->id == AOS_ENEMY_ZOMBIE)
        return zombie_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk, soma_def,
                             cam_x, cam_y, random);
    if (enemy->id == AOS_ENEMY_BLUE_CROW)
        return crow_update(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    if (enemy->state == 3) {
        bat_die(enemy, kind);
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
    /* sub_0806E314: on screen, the collision pass. */
    sx = (enemy->x >> 16) - cam_x;
    sy = (enemy->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
        report = collide(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    /* sub_080426B0 after the entity update. */
    aos_combat_tick(&enemy->combat);
    return report;
}
