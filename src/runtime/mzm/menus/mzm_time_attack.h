/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission's Time Attack checks (time_attack.c): the save file's
 * validity from the in-game timer at each boss, the unlock value of
 * CheckUnlockTimeAttack, the record check of TimeAttackCheckSetNewRecord
 * and its password (TimeAttackGenerateSeed and its shuffles). Pure data in
 * and out; the ROM tables are passed in. */
#ifndef MZM_TIME_ATTACK_H
#define MZM_TIME_ATTACK_H

#include <stdbool.h>
#include <stdint.h>

/* struct InGameTimer */
typedef struct {
    uint8_t hours, minutes, seconds, frames;
} MzmTimeAttackTimer;

/* ChozodiaEscapeGetItemCountAndEndingNumber's counts */
typedef struct {
    uint8_t energy, missiles, super_missiles, power_bombs, abilities;
} MzmTimeAttackItems;

/* struct TimeAttackRecord */
typedef struct {
    uint8_t password[20];
    MzmTimeAttackTimer igt;
    uint8_t password100[20];
    MzmTimeAttackTimer igt100;
} MzmTimeAttackRecord;

/* GAME_REGION of the US version */
enum { MZM_TIME_ATTACK_REGION_US = 2 };

/* TimeAttackCheckSaveFileValidity: the first four bosses beaten in order
 * and each later than sTimeAttackAntiCheatTimes. */
bool mzm_time_attack_valid_file(const MzmTimeAttackTimer bosses[4], const uint32_t anti_cheat[4]);
/* CheckUnlockTimeAttack: the new gFileScreenOptionsUnlocked.timeAttack, or
 * `unlocked` unchanged when already unlocked, invalid or abnormal. */
uint32_t mzm_time_attack_unlock(uint32_t unlocked, const MzmTimeAttackTimer bosses[4],
                                const uint32_t anti_cheat[4], const MzmTimeAttackItems *items, uint8_t region);
/* TimeAttackGenerateSeed: the 20-character password of a run. */
void mzm_time_attack_password(uint32_t unlocked, MzmTimeAttackTimer igt, const MzmTimeAttackItems *items,
                              bool valid_file, const MzmTimeAttackTimer bosses[4], uint8_t frame8,
                              const uint8_t characters[32], uint8_t password[20]);
/* TimeAttackCheckSetNewRecord: updates `record` (Any% and 100%) and returns
 * true when a record was set. */
bool mzm_time_attack_check_record(MzmTimeAttackRecord *record, uint32_t unlocked, MzmTimeAttackTimer igt,
                                  const MzmTimeAttackItems *items, const MzmTimeAttackTimer bosses[4],
                                  const uint32_t anti_cheat[4], uint8_t frame8,
                                  const uint8_t characters[32]);

#endif
