/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies; comments name the cvaos routine. */
#include "aos_enemy.h"

#include <math.h>
#include <string.h>


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
    if (id != AOS_ENEMY_BAT && id != AOS_ENEMY_ZOMBIE && id != AOS_ENEMY_BLUE_CROW &&
        id != AOS_ENEMY_ZOMBIE_SOLDIER && id != AOS_ENEMY_AXE_ARMOR &&
        id != AOS_ENEMY_SKULL_ARCHER)
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
    if (id == AOS_ENEMY_ZOMBIE) {
        zombie_create(enemy, soma, layer, anims);
        return true;
    }
    if (id == AOS_ENEMY_SKULL_ARCHER) {
        /* EnemySkullArcherCreate: idle animation, floor snap plus one
         * pixel; a record with parameter 0 patrols (state 2, animation 1),
         * otherwise it stands (state 0). State 4 is not ported. */
        play(enemy, anims, 0);
        step_anim(enemy, anims);
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
         * down; state 0 (state 3 under the global 0x8E & 0x40 is not
         * ported). */
        play(enemy, anims, 0);
        step_anim(enemy, anims);
        snap_to_floor(enemy, layer);
        enemy->y += 0x10000;
        enemy->state = 0;
        return true;
    }
    if (id == AOS_ENEMY_ZOMBIE_SOLDIER) {
        /* EnemyZombieSoldierCreate: walk animation 0, snapped to the floor,
         * state 0 (state 3 under the global 0x8E & 0x40 is not ported). */
        play(enemy, anims, 0);
        step_anim(enemy, anims);
        snap_to_floor(enemy, layer);
        enemy->state = 0;
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
            report.enemy_hit = true;
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
            /* sub_0806E218 with sub_08021530(3) = ATK * 16 / 16. */
            report.enemy_damage = aos_enemy_damage(soma_atk, enemy->stats.defence,
                                                   enemy->stats.weak, enemy->stats.resist,
                                                   soma->weapon.flags);
            enemy->hit_flash = 8;
            enemy->hp = (int16_t)(enemy->hp - report.enemy_damage);
            if (enemy->hp <= 0) {
                report.killed = true;
                /* sub_080683BC (rewards, not ported) marks it defeated
                 * (+ 0x3E bit 1): sub_0806E314 then skips its collisions. */
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
                    /* sub_08092B38: death animation 3 and the generic death
                     * in state 2 (sound 0x70; sub_080683BC is not ported). */
                    play_loop(enemy, kind->anims, 3, false);
                    enemy->timer = 0x28;
                    enemy->state = 2;
                    enemy->step = 0;
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
        report.soma_damage = aos_soma_take_hit(soma, enemy->stats.contact, soma_def, enemy->x, 0);
    }
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
        if (outside_margins(kind, 4, sx, sy))
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
    if (!z->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
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
 * until t = 18 and ends at t = 24. The explosion particles of
 * sub_0806D894 / sub_0806D644 / sub_0806D930 are not ported, so nothing is
 * drawn while it bursts. */
static AosHitReport grenade_update(AosEnemy *g, AosSoma *soma, const AosCollision *layer,
                                   const AosEnemyKind *kind, const AosWeaponEntity *weapon,
                                   const AosWeaponFrames *weapon_frames, int soma_atk,
                                   int soma_def, int cam_x, int cam_y) {
    AosHitReport report = {0};
    switch (g->state) {
    case 0: {
        int flags = projectile_collide(g, layer, g->vy / 3, (int32_t)0xFFFC0000);
        bool explode = false;
        if (flags & 4) {
            g->vy = -g->vy;
            explode = ++g->timer > 1;
        }
        if (explode || (flags & 8)) {
            g->state = 2;
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
        if (++g->timer > 6) g->removed = true;
        break;
    case 2:
        g->hidden = true;
        ++g->timer;
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

/* EnemyZombieSoldierUpdate: within the activity window of sub_0806CC20,
 * state 0 walks, 1 attacks, 2 dies (the gibs of sub_08092FCC are not
 * ported), then the animation step and the on-screen collision pass. */
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
    case 2: generic_death(z, kind); break;
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
 * Later updates run the collision pass first: struck, it vanishes (the
 * effect of sub_0806D5C0 is not ported). It is deleted beyond screen margin
 * 4. The global pause of gEwramData + 0x4BE is not ported. */
static AosHitReport axe_update(AosEnemy *axe, AosSoma *soma, const AosEnemyKind *kind,
                               const AosWeaponEntity *weapon,
                               const AosWeaponFrames *weapon_frames, int soma_atk,
                               int soma_def, int cam_x, int cam_y) {
    AosHitReport report = {0};
    if (axe->state != 0) {
        report = collide(axe, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        if (axe->hp <= 0) {
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
 * deletion; the explosions of sub_08045CEC are not ported. */
static void armor_die(AosEnemy *a, const AosEnemyKind *kind) {
    if (a->step == 0) {
        play_loop(a, kind->anims, 4, false);
        a->step = 1;
        return;
    }
    a->timer++;
    if (a->anim.flags & AOS_ANIM_ENDED) a->removed = true;
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
        armor_die(a, kind);
        if (a->removed) return report;
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
            report->children[report->child_count++] = make_arrow(a, kind->volleys[v][a->shot][1]);
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
 * frames left is not traced); struck (no hit point), it vanishes (state 2;
 * the effect of sub_0806D5C0 is not ported). Deleted beyond screen margin
 * 4. The global pause 0x4BE is not ported. */
static AosHitReport arrow_update(AosEnemy *arrow, AosSoma *soma, const AosEnemyKind *kind,
                                 const AosWeaponEntity *weapon,
                                 const AosWeaponFrames *weapon_frames, int soma_atk,
                                 int soma_def, int cam_x, int cam_y) {
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
 * (sub_080020A0; sound 0x162, the effect of sub_08045CEC is not ported), or
 * when it leaves the screen widened by 32 pixels. It never collides. */
static void debris_update(AosEnemy *d, const AosCollision *layer, const AosEnemyKind *kind,
                          int cam_x, int cam_y) {
    d->vx = d->vx * 79 / 80 + d->ax;
    if (d->ax >= 0 ? d->vx > 0x80000 : d->vx < -0x80000) d->vx = d->ax >= 0 ? 0x80000 : -0x80000;
    d->vy += d->ay;
    if (d->ay >= 0 ? d->vy > 0x80000 : d->vy < -0x80000) d->vy = d->ay >= 0 ? 0x80000 : -0x80000;
    d->x += d->vx;
    d->y += d->vy;
    int ax = 0, ay = 0;
    if (d->anim.id < AOS_ENEMY_MAX_ANIMS && d->anim.frame < AOS_ENEMY_MAX_FRAMES) {
        ax = kind->anchors[d->anim.id][d->anim.frame][0];
        ay = kind->anchors[d->anim.id][d->anim.frame][1];
    }
    if (d->mirrored) ax = -ax;
    if (d->vflip) ay = -ay;
    int32_t px = (d->x >> 16) + ax, py = (d->y >> 16) + ay + 4;
    if (aos_collision_cell(layer, px, py) & 1) {
        d->removed = true;
        return;
    }
    int sx = (d->x >> 16) - cam_x, sy = (d->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x20) > 0x130 || (uint16_t)(sy + 0x20) > 0xE0) d->removed = true;
}

/* sub_080AFB9C: seven pieces (animations 5 to 11) fly away from the
 * attacker at 0.5 to 2.4, rising at 0.5 to 3.5 with gravity 0x2000. */
static void archer_shatter(const AosEnemy *a, const AosEnemyKind *kind, uint32_t (*random)(void),
                           AosHitReport *report) {
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
    if (enemy->role == AOS_ROLE_DEBRIS) {
        debris_update(enemy, layer, kind, cam_x, cam_y);
        if (!enemy->removed) step_anim(enemy, anims);
        return report;
    }
    if (enemy->role == AOS_ROLE_ARROW)
        return arrow_update(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def, cam_x,
                            cam_y);
    if (enemy->id == AOS_ENEMY_SKULL_ARCHER)
        return archer_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk, soma_def,
                             cam_x, cam_y, random);
    if (enemy->role == AOS_ROLE_AXE)
        return axe_update(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def, cam_x,
                          cam_y);
    if (enemy->id == AOS_ENEMY_AXE_ARMOR)
        return armor_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk, soma_def,
                            cam_x, cam_y, random);
    if (enemy->role == AOS_ROLE_GRENADE)
        return grenade_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk, soma_def,
                              cam_x, cam_y);
    if (enemy->id == AOS_ENEMY_ZOMBIE_SOLDIER)
        return soldier_update(enemy, soma, layer, kind, weapon, weapon_frames, soma_atk, soma_def,
                              cam_x, cam_y, random);
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
    /* sub_0806E314: on screen and not defeated, the collision pass. */
    sx = (enemy->x >> 16) - cam_x;
    sy = (enemy->y >> 16) - cam_y;
    if (!enemy->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
        report = collide(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    /* sub_080426B0 after the entity update. */
    aos_combat_tick(&enemy->combat);
    return report;
}
