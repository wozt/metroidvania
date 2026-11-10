/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow damage-number entities (sub_08042B8C). No SDL. */
#ifndef AOS_DAMAGE_POPUP_H
#define AOS_DAMAGE_POPUP_H

#include <stdbool.h>
#include <stdint.h>

enum {
    AOS_DAMAGE_POPUP_DIGITS = 11,
    AOS_DAMAGE_POPUP_GLYPHS = 12, /* 0-9, minus and plus */
    AOS_DAMAGE_POPUP_MINUS = 10,
    AOS_DAMAGE_POPUP_PLUS = 11,
    AOS_DAMAGE_POPUP_BLANK = 12
};

typedef struct {
    int32_t x, y;       /* + 0x40, + 0x44 */
    int16_t value;
    uint8_t type;       /* 0: damage, 1/2: the other native colour rows */
    uint8_t age;        /* + 0x0D */
    uint8_t glyphs[AOS_DAMAGE_POPUP_DIGITS];
    bool guard;         /* type-0 zero uses still frame 0x58 */
    bool hidden;        /* + 0x58 bit 5 */
    bool active;        /* inverse of + 0x59 bit 3 */
} AosDamagePopup;

/* sub_08042B8C / sub_08042C78. Coordinates are 16.16, as on the entity. */
AosDamagePopup aos_damage_popup_create(int32_t x, int32_t y, int16_t value, uint8_t type);

/* sub_08042DE8: rise by half a pixel, then blink and expire. */
bool aos_damage_popup_update(AosDamagePopup *popup);

#endif /* AOS_DAMAGE_POPUP_H */
