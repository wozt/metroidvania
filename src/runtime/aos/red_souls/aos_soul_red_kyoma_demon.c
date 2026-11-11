/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_red_kyoma_demon.h"

AosKyomaDemon aos_kyoma_demon_create(void) {
    return (AosKyomaDemon){.state = 0};
}

bool aos_kyoma_demon_update(AosKyomaDemon *demon) {
    if (demon->state == 0) {
        demon->state = 1;
        demon->wobble = 0x20;
        return true;
    }
    if (demon->state == 1) {
        /* + 0x24 = sub_080009E4(+ 0x0D << 10) is a visual wobble fed to
         * sub_0803E058; only the timer matters here. */
        if (demon->wobble == 0) {
            demon->linger = 0xB4;
            demon->state = 2;
        } else {
            --demon->wobble;
        }
        return true;
    }
    if (demon->linger == 0) return false;
    --demon->linger;
    return true;
}
