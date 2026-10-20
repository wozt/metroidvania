/* SPDX-License-Identifier: GPL-3.0-only */
/* mzm_time_attack: vectors produced by the decompilation's own
 * CheckUnlockTimeAttack and TimeAttackCheckSetNewRecord (compiled with
 * stub globals), checked against the port. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "mzm_time_attack.h"

int main(void) {
    const uint8_t *characters = (const uint8_t *)"0123456789ABCDEFGHJKLMNPQRSTVWXY";
    const uint32_t anti_cheat[4] = {5u << 16, 10u << 16, 16u << 16, 22u << 16};
    const MzmTimeAttackTimer bosses[3][4] = {
        {{0, 30, 12, 7}, {1, 2, 3, 40}, {1, 40, 0, 9}, {2, 10, 59, 33}},
        {{0, 6, 0, 1}, {0, 11, 0, 2}, {0, 17, 0, 3}, {0, 23, 0, 4}},
        {{1, 0, 0, 0}, {0, 50, 0, 0}, {2, 0, 0, 0}, {3, 0, 0, 0}},
    };
    const MzmTimeAttackItems items[3] = {{12, 50, 15, 9, 14}, {3, 10, 2, 1, 9}, {12, 50, 15, 9, 14}};
    const MzmTimeAttackTimer igts[3] = {{2, 31, 4, 5}, {0, 59, 59, 59}, {3, 0, 0, 0}};
    const uint8_t frames[3] = {0x37, 0x0A, 0xF5};
    const uint32_t unlocks[3] = {0x16264365u, 0x4005060Du, 0};
    const char *any[3] = {"7PSVYPKBBPQV6F6L3N70", "K08SRNRAMBT4TQJA5A0E", "7248YTXTMAWXP2L96FRN"};
    const bool full[3] = {true, false, true};
    for (int c = 0; c < 3; ++c) {
        uint32_t unlock = mzm_time_attack_unlock(0, bosses[c], anti_cheat, &items[c], MZM_TIME_ATTACK_REGION_US);
        assert(unlock == unlocks[c]);
        MzmTimeAttackRecord record;
        memset(&record, 0xFF, sizeof record);
        assert(mzm_time_attack_check_record(&record, unlock, igts[c], &items[c], bosses[c], anti_cheat, frames[c],
                                            characters));
        assert(!memcmp(record.password, any[c], 20) && record.igt.hours == igts[c].hours);
        /* The 100% record needs 100 items or more. */
        assert(full[c] ? !memcmp(record.password100, any[c], 20) : record.password100[0] == 0xFF);
    }
    /* Already unlocked: unchanged; bosses out of order: invalid. */
    assert(mzm_time_attack_unlock(0x81, bosses[0], anti_cheat, &items[0], MZM_TIME_ATTACK_REGION_US) == 0x81);
    assert(!mzm_time_attack_valid_file(bosses[2], anti_cheat));
    /* A slower run sets no record. */
    MzmTimeAttackRecord record;
    memset(&record, 0, sizeof record);
    record.igt = (MzmTimeAttackTimer){1, 0, 0, 0};
    record.igt100 = record.igt;
    assert(!mzm_time_attack_check_record(&record, unlocks[0], igts[0], &items[0], bosses[0], anti_cheat, 0,
                                         characters));
    puts("mzm_time_attack: ok");
    return 0;
}
