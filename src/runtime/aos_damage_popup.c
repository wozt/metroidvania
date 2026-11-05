/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_damage_popup.h"

static void encode_value(AosDamagePopup *popup) {
    for (int i = 0; i < AOS_DAMAGE_POPUP_DIGITS; ++i)
        popup->glyphs[i] = AOS_DAMAGE_POPUP_BLANK;
    if (!popup->value) return;

    int32_t signed_value = popup->value;
    uint32_t value;
    if (signed_value < 0) {
        popup->glyphs[0] = AOS_DAMAGE_POPUP_MINUS;
        value = (uint32_t)(-signed_value);
    } else {
        if (popup->type == 1) popup->glyphs[0] = AOS_DAMAGE_POPUP_PLUS;
        value = (uint32_t)signed_value;
    }

    uint32_t divisor = 1000000000u;
    bool started = false;
    for (int i = 1; i < AOS_DAMAGE_POPUP_DIGITS; ++i) {
        uint8_t digit = (uint8_t)(value / divisor);
        value %= divisor;
        if (digit || started || i == AOS_DAMAGE_POPUP_DIGITS - 1) {
            popup->glyphs[i] = digit;
            started = true;
        }
        divisor /= 10;
    }
}

AosDamagePopup aos_damage_popup_create(int32_t x, int32_t y, int16_t value, uint8_t type) {
    AosDamagePopup popup = {
        .x = x,
        .y = y - (type == 1 || type == 2 ? 0x80000 : 0),
        .value = value,
        .type = type,
        .guard = type == 0 && value == 0,
        .active = true,
    };
    encode_value(&popup);
    return popup;
}

bool aos_damage_popup_update(AosDamagePopup *popup) {
    if (!popup || !popup->active) return false;
    if (popup->type > 2) return true;
    popup->y -= 0x8000;
    uint8_t blink_at = popup->type == 0 ? 20 : 30;
    uint8_t expires_at = popup->type == 0 ? 28 : 40;
    if (popup->age > blink_at) popup->hidden = (popup->age & 1) != 0;
    if (++popup->age == expires_at) popup->active = false;
    return popup->active;
}
