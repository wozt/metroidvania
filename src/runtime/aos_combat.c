/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow entity collisions and damage; comments name the cvaos
 * routine. */
#include "aos_combat.h"

/* 0x080E33F0: recent-hit cooldown per attacker type. */
static const uint8_t type_cooldowns[12] = {0, 0, 8, 28, 30, 0, 0, 0, 80, 60, 120, 0};

AosRect aos_player_rect(AosBox box, int32_t x, int32_t y, bool mirrored, bool vflip) {
    AosRect r;
    if (!mirrored) {
        r.x1 = (int16_t)(x + box.x);
        r.x2 = (int16_t)(r.x1 + box.w);
    } else {
        r.x2 = (int16_t)(x - box.x);
        r.x1 = (int16_t)(r.x2 - box.w);
    }
    if (!vflip) {
        r.y1 = (int16_t)(y + box.y);
        r.y2 = (int16_t)(r.y1 + box.h);
    } else {
        r.y2 = (int16_t)(y - box.y);
        r.y1 = (int16_t)(r.y2 - box.h);
    }
    return r;
}

AosRect aos_entity_rect(AosBox box, int32_t x, int32_t y, bool mirrored, bool vflip) {
    AosRect r;
    if (!mirrored) {
        r.x1 = (int16_t)(x + box.x);
        r.x2 = (int16_t)(r.x1 + box.w - 1);
    } else {
        r.x2 = (int16_t)(x - box.x);
        r.x1 = (int16_t)(r.x2 - (box.w - 1));
    }
    if (!vflip) {
        r.y1 = (int16_t)(y + box.y);
        r.y2 = (int16_t)(r.y1 + box.h - 1);
    } else {
        r.y2 = (int16_t)(y - box.y);
        r.y1 = (int16_t)(r.y2 - (box.h - 1));
    }
    return r;
}

bool aos_rect_hits_box(AosRect rect, AosBox box, int32_t x, int32_t y, bool mirrored,
                       bool vflip) {
    if (rect.x1 == rect.x2 || rect.y1 == rect.y2 || box.w == 1 || box.h == 1) return false;
    AosRect b = aos_entity_rect(box, x, y, mirrored, vflip);
    return rect.x1 <= b.x2 && b.x1 <= rect.x2 && rect.y1 <= b.y2 && b.y1 <= rect.y2;
}

bool aos_combat_can_take(const AosCombat *victim, uint8_t attacker_type) {
    if (victim->hurt_off) return false;
    for (int i = 0; i < 3; ++i)
        if (victim->recent[i] == attacker_type) return false;
    return !victim->recent[0] || !victim->recent[1] || !victim->recent[2];
}

void aos_combat_take(AosCombat *victim, uint8_t attacker_type, uint8_t cooldown) {
    for (int i = 0; i < 3; ++i) {
        if (!victim->recent[i]) {
            victim->recent[i] = attacker_type;
            victim->cooldown[i] = (uint8_t)(cooldown + 1);
            return;
        }
    }
}

void aos_combat_tick(AosCombat *combat) {
    for (int i = 0; i < 3; ++i)
        if (combat->cooldown[i] && !--combat->cooldown[i]) combat->recent[i] = 0;
}

uint8_t aos_combat_cooldown(uint8_t attacker_type, uint8_t weapon_interval) {
    if (attacker_type == AOS_TYPE_WEAPON) return weapon_interval;
    return attacker_type < 12 ? type_cooldowns[attacker_type] : 0;
}

int aos_enemy_damage(int power, int defence, uint16_t weak, uint16_t resist,
                     uint16_t attack_elements) {
    uint16_t weakness = weak & attack_elements;
    if (weakness & 0x3F) {
        power = (weakness & 0x3F) == 1 ? power + (power >> 2) : power << 1;
    } else {
        uint16_t elements = attack_elements & 0x3F;
        uint16_t resisted = resist & elements;
        if (elements && ((resisted & 0xFFFE) ? (elements & ~1u) == (resisted & 0xFFFE)
                                             : elements == resisted))
            power -= power >> 1;
    }
    int damage = ((power - (defence >> 1)) * (256 - defence)) >> 8;
    return damage > 0 ? damage : 1;
}

int aos_player_damage(int attack, int defence) {
    int damage = (attack * 4 - defence * 2) >> 1;
    return damage > 0 ? damage : 1;
}
