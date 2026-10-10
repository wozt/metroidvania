/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies; comments name the cvaos routine. */
#include "aos_enemy.h"

#include <math.h>

enum { BAT_ID = 0x00 };

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

static void play(AosEnemy *enemy, const AosAnimSet *anims, unsigned id) {
    aos_anim_start(&enemy->anim, anims, id, true);
    enemy->anim.id = (uint8_t)id;
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
        /* + 0x59 bit 0: the animation looped (sub_0803EC34 result 4). */
        if (bat->anim.flags & 0x80) {
            bat->anim.flags &= ~0x80;
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
        aos_anim_step(&bat->anim, anims);
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

bool aos_enemy_create(AosEnemy *enemy, uint8_t id, int32_t x, int32_t y, const AosSoma *soma,
                      const AosCollision *layer, const AosEnemyKind *kind,
                      const AosEnemyStats *stats) {
    if (id != BAT_ID) return false;
    const AosAnimSet *anims = kind->anims;
    *enemy = (AosEnemy){.id = id, .x = (int32_t)((uint32_t)x << 16), .y = (int32_t)((uint32_t)y << 16)};
    /* sub_0800F1FC: mirrored when the record lies left of the player. */
    enemy->mirrored = enemy->x < soma->x;
    /* sub_0806B04C / sub_0806D244: stats and the type 8 collision block. */
    enemy->stats = *stats;
    enemy->hp = stats->hp;
    enemy->combat.type = AOS_TYPE_ENEMY;
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
                /* sub_080AD6E4: the bat dies. */
                report.killed = true;
                enemy->state = 3;
                enemy->step = enemy->substep = 0;
            }
        }
    }
    /* The contact callback sub_0806E1B8 -> sub_08021654. */
    if (report.soma_hit) report.soma_damage = aos_player_damage(enemy->stats.contact, soma_def);
    return report;
}

AosHitReport aos_enemy_update(AosEnemy *enemy, AosSoma *soma, const AosEnemyKind *kind,
                              const AosWeaponEntity *weapon, const AosWeaponFrames *weapon_frames,
                              int soma_atk, int soma_def, int cam_x, int cam_y,
                              uint32_t (*random)(void)) {
    AosHitReport report = {0};
    const AosAnimSet *anims = kind->anims;
    if (enemy->removed) return report;
    if (enemy->hit_flash) enemy->hit_flash--;
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
    if (aos_anim_step(&enemy->anim, anims) == 4) enemy->anim.flags |= 0x80;
    /* sub_0806E314: on screen, the collision pass. */
    sx = (enemy->x >> 16) - cam_x;
    sy = (enemy->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
        report = collide(enemy, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    /* sub_080426B0 after the entity update. */
    aos_combat_tick(&enemy->combat);
    return report;
}
