/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_skill_valkyrie.h"

AosValkyrie aos_valkyrie_create(int32_t soma_x, int32_t soma_y, bool soma_facing_left) {
    return (AosValkyrie){
        .x = (int32_t)((uint32_t)(int16_t)((soma_x >> 16) + (soma_facing_left ? -0x14 : 0x14))
                       << 16),
        .y = (int32_t)((uint32_t)(int16_t)((soma_y >> 16) - 0x10) << 16),
        .facing_left = soma_facing_left, .state = 1};
}

bool aos_valkyrie_update(AosValkyrie *valkyrie, int cam_x, int cam_y, uint32_t (*random)(void),
                         AosValkyrieSpark *spark) {
    *spark = (AosValkyrieSpark){0};
    if (valkyrie->state == 2) {
        if (valkyrie->timer == 0) return false;
        valkyrie->timer--;
        return true;
    }
    uint8_t counter = ++valkyrie->timer;
    if (counter <= 0x13) return true;
    if ((counter & 3) == 0) {
        int32_t dx = ((counter * 3) << 14) + 0x200000;
        int32_t dy = 0x380000 - ((counter * 31) << 12);
        if (valkyrie->facing_left) dx = -dx;
        spark->spawn = true;
        spark->x = valkyrie->x + dx + (int32_t)(((random() & 0x1F) - 0x10) << 16);
        spark->y = valkyrie->y + dy + (int32_t)(((random() & 0x1F) - 0x10) << 16);
        spark->ay = (int32_t)((random() & 0x1F) << 4) - 0x2000;
        /* sub_08045000's creation draws its two offsets. */
        spark->offset_x = (int32_t)((random() & 0x1F) << 16) - 0x100000;
        spark->offset_y = (int32_t)((random() & 0x1F) << 16) - 0x100000;
    }
    int sx = (valkyrie->x >> 16) - cam_x, sy = (valkyrie->y >> 16) - cam_y;
    valkyrie->hidden = !((uint16_t)(sx + 0x40) <= 0x170 && (uint16_t)(sy + 8) <= 0xB0);
    return true;
}

void aos_valkyrie_animation_ended(AosValkyrie *valkyrie) {
    if (valkyrie->state == 2) return;
    valkyrie->state = 2;
    valkyrie->timer = 10;
    valkyrie->blinking = true;
}
