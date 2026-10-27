/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_stats.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    /* sub_08033DA4: Soma and Julius start at level 1. */
    AosStats soma = {0};
    aos_stats_init(&soma);
    assert(soma.level == 1 && soma.max_hp == 320 && soma.max_mp == 80 &&
           soma.base[AOS_STAT_STR] == 10 && soma.base[AOS_STAT_CON] == 12 &&
           soma.base[AOS_STAT_INT] == 11 && soma.base[AOS_STAT_LCK] == 9);
    AosStats julius = {.character = 1};
    aos_stats_init(&julius);
    assert(julius.max_hp == 800 && julius.max_mp == 500 && julius.base[AOS_STAT_STR] == 15 &&
           julius.base[AOS_STAT_LCK] == 20);
    aos_stats_julius_bonus(&julius);
    assert(julius.base[AOS_STAT_STR] == 40 && julius.base[AOS_STAT_CON] == 32 &&
           julius.base[AOS_STAT_INT] == 28 && julius.base[AOS_STAT_LCK] == 20);
    AosStats unchanged = soma;
    aos_stats_julius_bonus(&unchanged);
    assert(unchanged.base[AOS_STAT_STR] == 10);

    /* sub_08033DFC: x(x + 1)(3x + 8). */
    assert(aos_stats_experience_for_level(1) == 22 && aos_stats_experience_for_level(2) == 84 &&
           aos_stats_experience_for_level(5) == 690 &&
           aos_stats_experience_for_level(99) == 3019500);

    /* sub_08033CAC: nothing below the next threshold, then one level with
     * the band-0 gains (level 1: INT +1 from the table). */
    assert(aos_stats_experience_to_next(&soma) == 84);
    soma.experience = 83;
    assert(aos_stats_level_up(&soma) == 0 && soma.level == 1);
    soma.experience = 84;
    assert(aos_stats_level_up(&soma) == 1 && soma.level == 2 && soma.max_hp == 332 &&
           soma.max_mp == 85 && soma.base[AOS_STAT_STR] == 11 && soma.base[AOS_STAT_CON] == 13 &&
           soma.base[AOS_STAT_INT] == 12 && soma.base[AOS_STAT_LCK] == 10);
    /* Several levels at once; level 4 (a multiple of 4) gains INT +1 only. */
    soma.experience = (uint32_t)aos_stats_experience_for_level(6);
    assert(aos_stats_level_up(&soma) == 4 && soma.level == 6);
    /* Levels 2-5 lie in band 0 (STR +1 each) except 5 (band 1, +1). */
    assert(soma.base[AOS_STAT_STR] == 15 && soma.max_hp == 380);
    assert(soma.base[AOS_STAT_INT] == 12 + 1 + 1 + 1 + 1);

    /* sub_08033E38 rebuilds the same stats as levelling up step by step. */
    AosStats stepped = {0}, direct = {0};
    aos_stats_init(&stepped);
    for (int level = 2; level <= 40; ++level) {
        stepped.experience = (uint32_t)aos_stats_experience_for_level(level);
        assert(aos_stats_level_up(&stepped) == 1);
    }
    aos_stats_set_level(&direct, 40);
    assert(direct.level == 40 && direct.max_hp == stepped.max_hp &&
           direct.max_mp == stepped.max_mp &&
           direct.base[AOS_STAT_STR] == stepped.base[AOS_STAT_STR] &&
           direct.base[AOS_STAT_CON] == stepped.base[AOS_STAT_CON] &&
           direct.base[AOS_STAT_INT] == stepped.base[AOS_STAT_INT] &&
           direct.base[AOS_STAT_LCK] == stepped.base[AOS_STAT_LCK]);
    /* Level 99 is the cap. */
    aos_stats_set_level(&direct, 120);
    assert(direct.level == 99 && aos_stats_experience_to_next(&direct) == 0);
    assert(direct.max_hp == 320 + 98 * 12 && direct.base[AOS_STAT_LCK] == 9 + 98);

    /* sub_080683BC: one reward per entity, bestiary bit, cap, bonus. */
    AosStats player = {0};
    aos_stats_init(&player);
    uint8_t flags = 0;
    assert(aos_stats_reward_enemy(&player, &flags, 0, 37, 50) == 0);
    assert(player.experience == 50 && flags == AOS_ENEMY_REWARDED &&
           aos_stats_enemy_defeated(&player, 37) && !aos_stats_enemy_defeated(&player, 36));
    assert(aos_stats_reward_enemy(&player, &flags, 0, 37, 50) == -1 && player.experience == 50);
    flags = 0;
    assert(aos_stats_reward_enemy(&player, &flags, 0, 112, 40) == 1 && player.level == 2 &&
           aos_stats_enemy_defeated(&player, 112));
    player.flags = AOS_STATS_FLAG_EXPERIENCE_BONUS;
    flags = 0;
    aos_stats_reward_enemy(&player, &flags, 0, 1, 100);
    assert(player.experience == 90 + (100 * 0x133 >> 8));
    player.experience = AOS_STATS_MAX_EXPERIENCE - 1;
    flags = 0;
    aos_stats_reward_enemy(&player, &flags, 0, 2, 600);
    assert(player.experience == AOS_STATS_MAX_EXPERIENCE && player.level == 99);
    /* The mode bits and Julius skip the reward but still consume the guard. */
    AosStats skipped = {0};
    aos_stats_init(&skipped);
    flags = 0;
    assert(aos_stats_reward_enemy(&skipped, &flags, 1, 5, 9) == -1 &&
           flags == AOS_ENEMY_REWARDED && !skipped.experience &&
           !aos_stats_enemy_defeated(&skipped, 5));
    flags = 0;
    assert(aos_stats_reward_enemy(&julius, &flags, 0, 5, 9) == -1 && !julius.experience);
    puts("Aria stats and experience tests passed");
    return 0;
}
