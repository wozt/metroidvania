/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow entity collisions and damage.
 *
 * Ports of cvaos: the player's world rectangles (sub_08042584), the
 * inclusive box overlap (sub_080417A0 / sub_080415A8 + sub_08041688), the
 * recent-hit slots with per-type cooldowns written by the box tests
 * (sub_08041BDC, sub_08041D54, sub_08041864) and checked by the collision
 * pass (sub_080421AC), the cooldown tick (sub_080426B0), the damage an
 * enemy takes (sub_0806B7D8 without the special HP and stun rules) and the
 * damage the player takes (sub_08021654 in normal mode). The player's
 * ATK and DEF are inputs: the new-game stats are not traced. No SDL. */
#ifndef AOS_COMBAT_H
#define AOS_COMBAT_H

#include <stdbool.h>
#include <stdint.h>

/* An entity box: signed x, y and width, height, relative to the entity
 * position while not mirrored. Width or height 1 means disabled. */
typedef struct {
    int8_t x, y;
    uint8_t w, h;
} AosBox;

/* A world rectangle: x1, y1, x2, y2. */
typedef struct {
    int16_t x1, y1, x2, y2;
} AosRect;

/* Collision types (entity + 0x70). */
enum {
    AOS_TYPE_PLAYER = 1,
    AOS_TYPE_WEAPON = 3,
    AOS_TYPE_ENEMY = 8
};

typedef struct {
    uint8_t type;           /* + 0x70 */
    bool attack_off;        /* + 0x72 bit 1 */
    bool hurt_off;          /* + 0x72 bit 2 */
    uint8_t recent[3];      /* + 0x74..0x76: types that hit recently */
    uint8_t cooldown[3];    /* + 0x78..0x7A */
} AosCombat;

/* sub_08042584: x2 = x1 + width (not inclusive like entity boxes). */
AosRect aos_player_rect(AosBox box, int32_t x, int32_t y, bool mirrored, bool vflip);
/* sub_080415A8: an entity box as an inclusive rectangle. */
AosRect aos_entity_rect(AosBox box, int32_t x, int32_t y, bool mirrored, bool vflip);
/* sub_080417A0: a rectangle against an entity box (inclusive). */
bool aos_rect_hits_box(AosRect rect, AosBox box, int32_t x, int32_t y, bool mirrored,
                       bool vflip);
/* The slot check of sub_080421AC: the type is not recent and a slot is
 * free. */
bool aos_combat_can_take(const AosCombat *victim, uint8_t attacker_type);
/* The slot write of the box tests: cooldown + 1 frames. */
void aos_combat_take(AosCombat *victim, uint8_t attacker_type, uint8_t cooldown);
/* sub_080426B0, once per frame after the entity update. */
void aos_combat_tick(AosCombat *combat);
/* Cooldown of an attacker type: 0x080E33F0[type], or for the weapon type
 * the weapon's record + 0x17 (written to gEwramData + 0x1307C). */
uint8_t aos_combat_cooldown(uint8_t attacker_type, uint8_t weapon_interval);
/* sub_0806B7D8: power after element rules, minus half the defence, scaled
 * by (256 - defence) / 256, at least 1. */
int aos_enemy_damage(int power, int defence, uint16_t weak, uint16_t resist,
                     uint16_t attack_elements);
/* sub_08021654, normal mode: (attack * 4 - DEF * 2) / 2, at least 1. */
int aos_player_damage(int attack, int defence);

#endif /* AOS_COMBAT_H */
