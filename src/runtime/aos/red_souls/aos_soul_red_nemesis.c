/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_red_nemesis.h"

AosNemesisSoul aos_nemesis_soul_create(void) {
    return (AosNemesisSoul){.state = 1, .timer = AOS_NEMESIS_SOUL_FRAMES};
}

bool aos_nemesis_soul_update(AosNemesisSoul *soul) {
    if (soul->state == 1) {
        if (soul->timer) {
            soul->timer--;
        } else {
            soul->blinking = true;
            soul->blink = AOS_NEMESIS_SOUL_BLINK;
            soul->state = 2;
        }
    }
    if (soul->state == 2) {
        if (soul->blink == 0) return false;
        soul->blink--;
    }
    if (soul->rise <= 0x1D) soul->rise++;
    return true;
}

int aos_nemesis_soul_dy(const AosNemesisSoul *soul) { return -8 - soul->rise; }
