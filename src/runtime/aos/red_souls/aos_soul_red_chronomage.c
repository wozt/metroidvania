/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_red_chronomage.h"

AosChronomage aos_chronomage_create(uint16_t field) {
    return (AosChronomage){.state = 0, .timer = 0, .charge = 0,
                           .charge2 = 0, .field = field,
                           .tick = false, .release = false};
}

bool aos_chronomage_update(AosChronomage *chronomage) {
    chronomage->tick = false;
    chronomage->release = false;
    if (chronomage->state == 0) {
        chronomage->state = 1;
    } else if (chronomage->state == 1) {
        /* + 0x20 += 0x1000 and + 0x24 += 0x2000, both capped at 0x10000;
         * + 0x28 mirrors + 0x20 for the global publish in the tail. */
        if (chronomage->charge <= 0x10000 - 0x1000)
            chronomage->charge += 0x1000;
        else
            chronomage->charge = 0x10000;
        if (chronomage->charge2 <= 0x10000 - 0x2000)
            chronomage->charge2 += 0x2000;
        else
            chronomage->charge2 = 0x10000;
        if (chronomage->field != 0) {
            /* Every sixteenth count requests the 0x16E tick through
             * sub_08013DA8 (its history dedup is not ported). */
            if ((chronomage->field & 0xF) == 0) chronomage->tick = true;
            --chronomage->field;
        } else {
            /* The release branch: conditional songs (gEwramData + 0x42C
             * bit 0x40000 -> 0x1038, + 0x254CC/+ 0x254CE fields) and the
             * sub_08013E18(0x16E) history removal are not ported. */
            chronomage->state = 2;
            chronomage->timer = 0x20;
            chronomage->release = true;
        }
    } else {
        /* + 0x20 -= 0x1000 and + 0x24 -= 0x800, clamped at 0; + 0x28 =
         * + 0x0D << 0xB ramps the published value back down. At timer 0
         * the native sets gEwramData + 0x12FE4 = 1 and deletes. */
        if (chronomage->charge >= 0x1000)
            chronomage->charge -= 0x1000;
        else
            chronomage->charge = 0;
        if (chronomage->charge2 >= 0x800)
            chronomage->charge2 -= 0x800;
        else
            chronomage->charge2 = 0;
        if (chronomage->timer == 0) return false;
        --chronomage->timer;
    }
    return true;
}
