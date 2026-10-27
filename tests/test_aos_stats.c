/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_stats.h"

#include <assert.h>
#include <stdio.h>

/* A scripted RandomNumberGenerator: returns the queued values in order and
 * counts the draws. */
static uint32_t queue[8];
static unsigned queued, drawn;

static uint32_t scripted(void) {
    assert(drawn < queued);
    return queue[drawn++];
}

static void script(unsigned count, const uint32_t *values) {
    for (unsigned i = 0; i < count; ++i) queue[i] = values[i];
    queued = count;
    drawn = 0;
}

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
    /* The drop rolls of sub_080683BC. Soul: rarity 4 at luck 0 gives a
     * range of 4 * 8 + 32 = 64; an owned soul needs (r >> 2) % 64 < 3. */
    AosStats drops = {0};
    aos_stats_init(&drops);
    const AosEnemyDrops souls = {.common_item = 0x12, .rare_item = 0x34, .soul_rarity = 4,
                                 .common_rate = 8, .rare_rate = 2, .soul_type = 1,
                                 .soul_index = 5};
    AosDropContext context = {.accessory = 0, .luck = 0, .soul_owned = true};
    script(1, (uint32_t[]){2u << 2});
    AosDrop drop = aos_stats_roll_drop(&drops, 9, &souls, &context, scripted);
    assert(drop.kind == AOS_DROP_SOUL && drop.soul_type == 1 && drop.soul_index == 4 &&
           drawn == 1);
    /* 3 fails for an owned soul; an unowned one allows 6, 7 with the 0xA1
     * flag, and the 0x28 accessory adds 8. */
    script(3, (uint32_t[]){3u << 2, 0xFFu << 1, 0});
    drop = aos_stats_roll_drop(&drops, 9, &souls, &context, scripted);
    assert(drop.kind != AOS_DROP_SOUL);
    context.soul_owned = false;
    script(1, (uint32_t[]){5u << 2});
    assert(aos_stats_roll_drop(&drops, 9, &souls, &context, scripted).kind == AOS_DROP_SOUL);
    context.soul_flag_a1 = true;
    script(1, (uint32_t[]){6u << 2});
    assert(aos_stats_roll_drop(&drops, 9, &souls, &context, scripted).kind == AOS_DROP_SOUL);
    context = (AosDropContext){.accessory = 0x28, .soul_owned = true};
    script(1, (uint32_t[]){10u << 2});
    assert(aos_stats_roll_drop(&drops, 9, &souls, &context, scripted).kind == AOS_DROP_SOUL);
    /* Luck shrinks the range (16 at least): luck 0x400 gives 32 - 64 + 32 = 0
     * -> 16, so (r >> 2) % 16 < 3. */
    context = (AosDropContext){.luck = 0x400, .soul_owned = true};
    script(1, (uint32_t[]){(16u + 2u) << 2});
    assert(aos_stats_roll_drop(&drops, 9, &souls, &context, scripted).kind == AOS_DROP_SOUL);

    /* Items: (r >> 1) & 0xFF < 0x40 picks the rare item (rate 2: range
     * 2 * 4 + 16 = 24), else the common one (rate 8: 48); then
     * (r >> 3) % range < 4. Item drops mark 0x133B0 / 0x133C0. */
    const AosEnemyDrops items = {.common_item = 0x12, .rare_item = 0x34, .common_rate = 8,
                                 .rare_rate = 2};
    context = (AosDropContext){0};
    script(2, (uint32_t[]){0x3Fu << 1, 3u << 3});
    drop = aos_stats_roll_drop(&drops, 40, &items, &context, scripted);
    assert(drop.kind == AOS_DROP_ITEM && drop.item == 0x34 && drop.rare && drawn == 2 &&
           (drops.dropped[1][1] >> 8 & 1) && !(drops.dropped[0][1] >> 8 & 1));
    script(2, (uint32_t[]){0x40u << 1, (48u + 3u) << 3});
    drop = aos_stats_roll_drop(&drops, 40, &items, &context, scripted);
    assert(drop.kind == AOS_DROP_ITEM && drop.item == 0x12 && !drop.rare &&
           (drops.dropped[0][1] >> 8 & 1));
    /* The 0x27 accessory widens both: rare below 0x60, success below 8. */
    context.accessory = 0x27;
    script(2, (uint32_t[]){0x5Fu << 1, 7u << 3});
    drop = aos_stats_roll_drop(&drops, 40, &items, &context, scripted);
    assert(drop.kind == AOS_DROP_ITEM && drop.rare);

    /* A failed item roll falls back to the pickups: bits 3-12 <= 7 give the
     * first one with variant (r >> 1) & 3; the 0x2B accessory accepts up to
     * 0x3F and adds 3 to the variant. */
    context = (AosDropContext){0};
    script(4, (uint32_t[]){0x40u << 1, 4u << 3, 7u << 3, 2u << 1});
    drop = aos_stats_roll_drop(&drops, 40, &items, &context, scripted);
    assert(drop.kind == AOS_DROP_PICKUP_43F70 && drop.variant == 2 && drawn == 4);
    context.accessory = 0x2B;
    script(4, (uint32_t[]){0x40u << 1, 4u << 3, 0x3Fu << 3, 3u << 1});
    drop = aos_stats_roll_drop(&drops, 40, &items, &context, scripted);
    assert(drop.kind == AOS_DROP_PICKUP_43F70 && drop.variant == 6);
    /* Otherwise bits 4-13 <= 7 (0x3F with 0x1D) give the second one, variant
     * 1 when bits 6-7 of the last draw are clear. */
    context = (AosDropContext){0};
    script(5, (uint32_t[]){0x40u << 1, 4u << 3, 8u << 3, 7u << 4, 0x40});
    drop = aos_stats_roll_drop(&drops, 40, &items, &context, scripted);
    assert(drop.kind == AOS_DROP_PICKUP_43C10 && drop.variant == 0 && drawn == 5);
    context.accessory = 0x1D;
    script(5, (uint32_t[]){0x40u << 1, 4u << 3, 8u << 3, 0x3Fu << 4, 0x3F});
    drop = aos_stats_roll_drop(&drops, 40, &items, &context, scripted);
    assert(drop.kind == AOS_DROP_PICKUP_43C10 && drop.variant == 1);
    context = (AosDropContext){0};
    script(4, (uint32_t[]){0x40u << 1, 4u << 3, 8u << 3, 8u << 4});
    assert(aos_stats_roll_drop(&drops, 40, &items, &context, scripted).kind == AOS_DROP_NONE);

    /* A record without items drops nothing after its item draw; a blocking
     * ground type (non-zero without bit 3) draws nothing at all. */
    const AosEnemyDrops empty = {0};
    script(1, (uint32_t[]){0});
    assert(aos_stats_roll_drop(&drops, 3, &empty, &context, scripted).kind == AOS_DROP_NONE &&
           drawn == 1);
    context.ground = 1;
    script(0, NULL);
    assert(aos_stats_roll_drop(&drops, 3, &items, &context, scripted).kind == AOS_DROP_NONE &&
           drawn == 0);
    context.ground = 9;
    script(2, (uint32_t[]){0x3Fu << 1, 0});
    assert(aos_stats_roll_drop(&drops, 3, &items, &context, scripted).kind == AOS_DROP_ITEM);
    puts("Aria stats and experience tests passed");
    return 0;
}
