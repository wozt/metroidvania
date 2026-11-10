/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos/game/aos_pickup.h"
#include "aos/core/aos_enemy.h"
#include "aos/core/aos_effect.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned draws;
static uint32_t counting_random(void) {
    ++draws;
    return 0x40;
}

int main(void) {
    /* A solid floor (cells of 8 pixels) from y = 80 down. */
    static uint8_t cells[64 * 64];
    AosCollision layer = {2, 2, 64, 64, cells, NULL, false};
    for (int y = 10; y < 64; ++y)
        for (int x = 0; x < 64; ++x) cells[y * 64 + x] = 0x03;

    /* The table records of 0x085250E4 for variants 0 and 4. */
    AosMoneyKind kinds[AOS_MONEY_KINDS] = {{0}};
    kinds[0] = (AosMoneyKind){1, 835, 6, 21, 1};
    kinds[4] = (AosMoneyKind){500, 833, 6, 33, 0};

    /* sub_08043F70: lifted variants sit 8 pixels higher. */
    AosPickup coin = aos_pickup_create_money(kinds, 0, 40, 30);
    assert(coin.type == AOS_PICKUP_MONEY && coin.x == 40 << 16 && coin.y == 30 << 16 &&
           coin.offset == -8 && coin.state == 0 && !coin.removed);
    AosPickup bag = aos_pickup_create_money(kinds, 4, 40, 30);
    assert(bag.offset == 0);

    /* sub_08043DD0: the fall accelerates by 0x2000 a frame; the probe is
     * y + offset + 8, so the coin lands when y reaches 80. */
    aos_pickup_update(&coin, &layer);
    assert(coin.y == 30 << 16 && coin.vy == 0x2000 && coin.state == 0);
    aos_pickup_update(&coin, &layer);
    assert(coin.y == (30 << 16) + 0x2000 && coin.vy == 0x4000);
    int frames = 2;
    while (coin.state == 0 && frames < 400) {
        aos_pickup_update(&coin, &layer);
        ++frames;
        assert(coin.vy <= 0x40000);
    }
    /* Landing moves the integer half out of the floor (probe y = 79 -> 80
     * is solid, at most 8 pixels up) and stops the fall. */
    assert(coin.state == 1 && coin.vy == 0 && (coin.y >> 16) + coin.offset + 8 <= 80);
    /* The landing frame already counts one of the 300 resting frames. */
    assert(coin.timer == 299);
    for (int i = 0; i < 299; ++i) aos_pickup_update(&coin, &layer);
    assert(coin.state == 2 && coin.blink == 0x40 && !coin.blinking);
    for (int i = 0; i < 63; ++i) aos_pickup_update(&coin, &layer);
    assert(coin.blinking && !coin.removed && coin.blink == 1);
    aos_pickup_update(&coin, &layer);
    assert(coin.removed);

    /* The fall speed stops at 0x40000 over a deep shaft. */
    static uint8_t open_cells[64 * 64];
    AosCollision shaft = {2, 2, 64, 64, open_cells, NULL, false};
    AosPickup falling = aos_pickup_create_money(kinds, 4, 40, 0);
    for (int i = 0; i < 40; ++i) aos_pickup_update(&falling, &shaft);
    assert(falling.vy == 0x40000 && falling.state == 0);

    /* sub_080441F8 case 1: the value is added, capped at 999999. */
    uint32_t gold = 10;
    bag.removed = false;
    assert(aos_pickup_collect_money(&bag, kinds, &gold) == 833 && gold == 510 && bag.removed);
    gold = 999800;
    bag.removed = false;
    aos_pickup_collect_money(&bag, kinds, &gold);
    assert(gold == 999999);

    /* The 16 x 16 box centred on the pickup (0x1010F8F8). */
    assert(aos_pickup_box.x == -8 && aos_pickup_box.y == -8 && aos_pickup_box.w == 16 &&
           aos_pickup_box.h == 16);
    /* sub_08044054's global numbering. */
    uint8_t category = 0, index = 0;
    assert(!aos_item_decode(0, &category, &index) && !aos_item_decode(0x89, &category, &index));
    assert(aos_item_decode(1, &category, &index) && category == AOS_PICKUP_ITEM && index == 0);
    assert(aos_item_decode(32, &category, &index) && category == AOS_PICKUP_ITEM && index == 31);
    assert(aos_item_decode(33, &category, &index) && category == AOS_PICKUP_WEAPON && index == 0);
    assert(aos_item_decode(0x22, &category, &index) && category == AOS_PICKUP_WEAPON &&
           index == 1);
    assert(aos_item_decode(91, &category, &index) && category == AOS_PICKUP_WEAPON &&
           index == 58);
    assert(aos_item_decode(92, &category, &index) && category == AOS_PICKUP_ARMOR && index == 0);
    assert(aos_item_decode(0x5D, &category, &index) && category == AOS_PICKUP_ARMOR &&
           index == 1);
    assert(aos_item_decode(0x88, &category, &index) && category == AOS_PICKUP_ARMOR &&
           index == 44);

    /* sub_08012294/sub_0801232C: equal icons share one of eight slots,
     * distinct icons fill the first free slot and a ninth waits. */
    AosPickupIconSlots icon_slots;
    aos_pickup_icon_slots_reset(&icon_slots);
    assert(aos_pickup_icon_acquire(&icon_slots, 0) == -1);
    for (int icon = 1; icon <= AOS_PICKUP_ICON_SLOTS; ++icon)
        assert(aos_pickup_icon_acquire(&icon_slots, (uint16_t)icon) == icon - 1);
    assert(aos_pickup_icon_acquire(&icon_slots, 4) == 3);
    assert(aos_pickup_icon_acquire(&icon_slots, 9) == -1);
    aos_pickup_icon_release(&icon_slots, 3);
    assert(aos_pickup_icon_acquire(&icon_slots, 9) == 3);
    aos_pickup_icon_backup(&icon_slots);
    for (int slot = 0; slot < AOS_PICKUP_ICON_SLOTS; ++slot)
        assert(icon_slots.active[slot] == 0);
    aos_pickup_icon_restore(&icon_slots);
    assert(icon_slots.active[3] == 9);

    AosPickup bound = aos_pickup_create_item(AOS_PICKUP_ITEM, 0, 0, 0);
    assert(bound.icon_slot == -1 && aos_pickup_icon_bind(&bound, &icon_slots, 1) == 0 &&
           bound.icon_id == 1);
    aos_pickup_icon_release(&icon_slots, 0);
    assert(aos_pickup_icon_bind(&bound, &icon_slots, 1) == 0);
    assert(aos_pickup_icon_bind(&bound, &icon_slots, 10) == 0 &&
           icon_slots.active[0] == 10);
    assert(aos_pickup_icon_bind(&bound, &icon_slots, 0) == -1 &&
           bound.icon_slot == -1 && bound.icon_id == 0);
    assert(aos_pickup_icon_bind(&bound, &icon_slots, 1) == 0);
    aos_pickup_icon_unbind(&bound, &icon_slots);
    assert(bound.icon_slot == -1 && icon_slots.active[0] == 0);

    /* sub_0804AE3C: weapons, armor and accessories have independent
     * four-entry caches; a full cache replaces only its last entry. */
    AosPickupRecent recent;
    aos_pickup_recent_reset(&recent);
    for (unsigned list = 0; list < AOS_PICKUP_RECENT_LISTS; ++list)
        for (unsigned slot = 0; slot < AOS_PICKUP_RECENT_ITEMS; ++slot)
            assert(recent.item[list][slot] == 0xFF);
    assert(aos_pickup_recent_record(&recent, AOS_PICKUP_WEAPON, 3));
    assert(!aos_pickup_recent_record(&recent, AOS_PICKUP_WEAPON, 3));
    assert(aos_pickup_recent_record(&recent, AOS_PICKUP_ARMOR, 4));
    assert(aos_pickup_recent_record(&recent, AOS_PICKUP_ARMOR, 25));
    assert(recent.item[0][0] == 3 && recent.item[1][0] == 4 && recent.item[2][0] == 25);
    for (uint8_t weapon = 4; weapon < 7; ++weapon)
        assert(aos_pickup_recent_record(&recent, AOS_PICKUP_WEAPON, weapon));
    assert(aos_pickup_recent_record(&recent, AOS_PICKUP_WEAPON, 9));
    assert(recent.item[0][0] == 3 && recent.item[0][1] == 4 && recent.item[0][2] == 5 &&
           recent.item[0][3] == 9);
    assert(!aos_pickup_recent_record(&recent, AOS_PICKUP_ITEM, 1));

    /* sub_08049E64: the same four-entry cache per soul family stores the
     * 1-based soul id; ability souls (family 3) have no native list. */
    AosRecentSouls soul_recent;
    aos_pickup_recent_soul_reset(&soul_recent);
    for (unsigned list = 0; list < AOS_PICKUP_RECENT_LISTS; ++list)
        for (unsigned slot = 0; slot < AOS_PICKUP_RECENT_ITEMS; ++slot)
            assert(soul_recent.soul[list][slot] == 0xFF);
    assert(aos_pickup_recent_soul_record(&soul_recent, 0, 4));
    assert(!aos_pickup_recent_soul_record(&soul_recent, 0, 4));
    assert(aos_pickup_recent_soul_record(&soul_recent, 1, 9));
    assert(aos_pickup_recent_soul_record(&soul_recent, 2, 20));
    assert(soul_recent.soul[0][0] == 5 && soul_recent.soul[1][0] == 10 &&
           soul_recent.soul[2][0] == 21);
    for (uint8_t soul = 5; soul < 9; ++soul)
        assert(aos_pickup_recent_soul_record(&soul_recent, 0, soul));
    assert(aos_pickup_recent_soul_record(&soul_recent, 0, 30));
    assert(soul_recent.soul[0][0] == 5 && soul_recent.soul[0][1] == 6 &&
           soul_recent.soul[0][2] == 7 && soul_recent.soul[0][3] == 31);
    assert(!aos_pickup_recent_soul_record(&soul_recent, 3, 0));
    assert(!aos_pickup_recent_soul_record(NULL, 0, 0));

    AosStats exchange_stats = {0};
    exchange_stats.weapon = 2;
    exchange_stats.weapon_counts[1] = exchange_stats.weapon_counts[2] = 1;
    AosPickupRecent exchange_recent = {
        .item = {{1, 2, 3, 4}, {0xFF, 0xFF, 0xFF, 0xFF},
                 {0xFF, 0xFF, 0xFF, 0xFF}}};
    assert(aos_pickup_recent_exchange(&exchange_recent, &exchange_stats, 0, 1));
    assert(exchange_recent.item[0][0] == 0xFF && exchange_recent.item[0][1] == 2 &&
           exchange_recent.item[0][2] == 3 && exchange_recent.item[0][3] == 4);
    /* More than one selected copy remains available, so it stays in a full
     * cache while the old equipment enters at the front. */
    exchange_stats.weapon_counts[7] = 2;
    memcpy(exchange_recent.item[0], (uint8_t[]){5, 6, 7, 8}, 4);
    assert(aos_pickup_recent_exchange(&exchange_recent, &exchange_stats, 0, 7));
    assert(exchange_recent.item[0][0] == 2 && exchange_recent.item[0][1] == 5 &&
           exchange_recent.item[0][2] == 6 && exchange_recent.item[0][3] == 7);
    exchange_stats.weapon = AOS_EQUIPMENT_NONE;
    assert(!aos_pickup_recent_exchange(&exchange_recent, &exchange_stats, 0, 7));

    /* sub_0800F1FC case 4: ids 1-4 use parameter 1 as their variant and
     * parameter 0 as a durable bit in gEwramData + 0x360. They stay fixed
     * instead of entering sub_08043DD0's fall and timeout. */
    uint32_t collected[AOS_PLACED_ITEM_WORDS] = {0};
    AosPickup placed;
    assert(aos_pickup_create_placed(&placed, kinds, 1, 4, 157, 24, 56, collected));
    assert(placed.placed && placed.persistent_index == 157 &&
           placed.type == AOS_PICKUP_MONEY && placed.variant == 4);
    int32_t placed_y = placed.y;
    for (int i = 0; i < 400; ++i) aos_pickup_update(&placed, &shaft);
    assert(placed.y == placed_y && !placed.removed && placed.age == 400);
    aos_pickup_mark_placed(&placed, collected);
    assert(aos_pickup_placed_collected(collected, 157) &&
           !aos_pickup_create_placed(&placed, kinds, 1, 4, 157, 24, 56, collected));
    assert(aos_pickup_create_placed(&placed, kinds, 2, 31, 0, 24, 56, collected) &&
           placed.type == AOS_PICKUP_ITEM && placed.variant == 31);
    aos_pickup_mark_placed(&placed, collected);
    assert(!aos_pickup_placed_collected(collected, 0));
    assert(aos_pickup_create_placed(&placed, kinds, 3, 58, 90, 24, 56, collected) &&
           placed.type == AOS_PICKUP_WEAPON && placed.variant == 58);
    assert(aos_pickup_create_placed(&placed, kinds, 4, 44, 91, 24, 56, collected) &&
           placed.type == AOS_PICKUP_ARMOR && placed.variant == 44);
    assert(!aos_pickup_create_placed(&placed, kinds, 5, 1, 92, 24, 56, collected) &&
           !aos_pickup_create_placed(&placed, kinds, 2, 32, 92, 24, 56, collected) &&
           !aos_pickup_create_placed(&placed, kinds, 1, 7, 92, 24, 56, collected));

    /* sub_08044980 / sub_08044840: ids 5-8 are fixed containers. The
     * source's parameter 1 is one-based, and their one-time bit changes only
     * once the homing soul has actually reached Soma. */
    AosPickup container, released;
    assert(aos_pickup_create_placed_soul(&container, 6, 2, 156, 24, 56, collected));
    assert(container.soul_container && container.placed &&
           container.type == AOS_PICKUP_SOUL + AOS_SOUL_BLUE && container.variant == 1 &&
           container.persistent_index == 156);
    assert(aos_soul_container_box.x == -8 && aos_soul_container_box.y == -32 &&
           aos_soul_container_box.w == 16 && aos_soul_container_box.h == 32);
    aos_pickup_mark_placed(&container, collected);
    assert(!aos_pickup_placed_collected(collected, 156));
    AosSoulContainerSpark spark;
    draws = 0;
    for (int i = 0; i < 7; ++i) {
        aos_pickup_soul_container_update(&container, &spark, counting_random);
        assert(!spark.spawn);
    }
    aos_pickup_soul_container_update(&container, &spark, counting_random);
    assert(spark.spawn && draws == 4 && spark.soul_type == AOS_SOUL_BLUE &&
           spark.x == 20 << 16 && spark.y == 36 << 16 && spark.vx == -1984 &&
           spark.vy == -73664 && spark.ay == 2302);
    draws = 0;
    assert(aos_pickup_release_placed_soul(&container, &released, 80 << 16, 64 << 16,
                                          counting_random));
    assert(draws == 1 && container.container_released && container.container_timer == 16 &&
           released.placed && !released.soul_container && released.persistent_index == 156 &&
           released.type == container.type && released.variant == container.variant);
    for (int i = 0; i < 15; ++i)
        aos_pickup_soul_container_update(&container, NULL, counting_random);
    assert(!container.removed && container.container_timer == 1);
    aos_pickup_soul_container_update(&container, NULL, counting_random);
    assert(container.removed && !aos_pickup_placed_collected(collected, 156));
    aos_pickup_mark_placed(&released, collected);
    assert(aos_pickup_placed_collected(collected, 156));
    assert(!aos_pickup_create_placed_soul(&container, 6, 2, 156, 24, 56, collected) &&
           !aos_pickup_create_placed_soul(&container, 5, 0, 20, 24, 56, collected) &&
           !aos_pickup_create_placed_soul(&container, 8, 4, 20, 24, 56, collected) &&
           !aos_pickup_create_placed_soul(&container, 9, 1, 20, 24, 56, collected));

    /* sub_080441F8's common tail distinguishes ordinary room pickups from
     * Object10's scripted item and sub_080D7308's special soul drops. */
    AosPickupCollectionState collection = {.global_flags = 0x103};
    placed = aos_pickup_create_item(AOS_PICKUP_ITEM, 0, 0, 0);
    placed.placed = true;
    placed.persistent_index = 90;
    aos_pickup_commit_collection(&placed, &collection);
    assert(aos_pickup_placed_collected(collection.placed, 90) &&
           collection.special == 0 && collection.global_flags == 0x103);
    AosPickup scripted = aos_pickup_create_item(AOS_PICKUP_ITEM, 1, 0, 0);
    scripted.origin_flags = AOS_PICKUP_ORIGIN_SCRIPTED;
    aos_pickup_commit_collection(&scripted, &collection);
    assert(collection.global_flags == 0x503);
    AosPickup special = aos_pickup_create_soul(AOS_SOUL_RED, 2, 0, 0, 0, 0,
                                                counting_random);
    special.persistent_index = 7;
    special.origin_flags = AOS_PICKUP_ORIGIN_SPECIAL;
    aos_pickup_commit_collection(&special, &collection);
    assert(collection.special == (1u << 7) && collection.global_flags == 0x500 &&
           !aos_pickup_placed_collected(collection.placed, 7));
    special.persistent_index = 0;
    collection.global_flags = 3;
    aos_pickup_commit_collection(&special, &collection);
    assert(collection.global_flags == 3 && collection.special == (1u << 7));

    assert(aos_pickup_create_scripted_item(&scripted, 33, 12, 34) &&
           scripted.type == AOS_PICKUP_WEAPON && scripted.variant == 0 &&
           scripted.x == 12 << 16 && scripted.y == 34 << 16 &&
           scripted.placed && scripted.state == 0x63 &&
           scripted.origin_flags == AOS_PICKUP_ORIGIN_SCRIPTED);
    assert(!aos_pickup_create_scripted_item(&scripted, 0, 0, 0) &&
           !aos_pickup_create_scripted_item(NULL, 1, 0, 0));
    draws = 0;
    assert(aos_pickup_create_special_soul(&special, 0, false, AOS_SOUL_YELLOW,
                                          5, 11, 20, 30, 40 << 16, 50 << 16,
                                          counting_random));
    assert(draws == 1 && special.type == AOS_PICKUP_SOUL + AOS_SOUL_YELLOW &&
           special.variant == 5 && special.persistent_index == 11 &&
           special.origin_flags == AOS_PICKUP_ORIGIN_SPECIAL);
    assert(!aos_pickup_create_special_soul(&special, 1, false, AOS_SOUL_RED,
                                           0, 1, 0, 0, 0, 0, counting_random) &&
           !aos_pickup_create_special_soul(&special, 0, true, AOS_SOUL_RED,
                                           0, 1, 0, 0, 0, 0, counting_random) &&
           !aos_pickup_create_special_soul(&special, 0, false, 4,
                                           0, 1, 0, 0, 0, 0, counting_random));
    uint16_t boss_index = 0;
    assert(aos_pickup_special_boss_index(0x6A, &boss_index) && boss_index == 4);
    assert(aos_pickup_special_boss_index(0x6B, &boss_index) && boss_index == 1);
    assert(aos_pickup_special_boss_index(0x6C, &boss_index) && boss_index == 5);
    assert(aos_pickup_special_boss_index(0x6D, &boss_index) && boss_index == 6);
    assert(!aos_pickup_special_boss_index(0x69, &boss_index) &&
           aos_pickup_special_boss_index(0x6A, NULL));

    /* sub_080450C8: the container's direct type-4 sequence selects an
     * initial frame without moving, then consumes 3, 7, 7 and 3 updates. */
    static const uint8_t direct_ticks[] = {3, 7, 7, 3};
    static const AosAnimDef direct_defs[] = {{4, direct_ticks}};
    static const AosAnimSet direct_set = {direct_defs, 1};
    AosEffect direct = {.direct = true, .vx = 0x10000, .vy = -0x10000};
    assert(aos_effect_update(&direct, &direct_set, NULL, NULL) && direct.x == 0 &&
           direct.y == 0 && direct.anim.frame == 0);
    for (int i = 0; i < 19; ++i) assert(aos_effect_update(&direct, &direct_set, NULL, NULL));
    assert(!aos_effect_update(&direct, &direct_set, NULL, NULL));
    assert(direct.x == 20 << 16 && direct.y == -(20 << 16) && direct.anim.frame == 3);

    /* Item pickups probe at their own height (no 8-pixel money offset). */
    AosPickup item = aos_pickup_create_item(AOS_PICKUP_WEAPON, 1, 40, 30);
    assert(item.type == AOS_PICKUP_WEAPON && item.variant == 1 && item.offset == 0);
    assert(aos_pickup_collection_sound(&item) == AOS_PICKUP_SOUND_EQUIPMENT &&
           aos_pickup_collection_sound(&coin) == AOS_PICKUP_SOUND_MONEY);
    for (int i = 0; i < 200 && item.state == 0; ++i) aos_pickup_update(&item, &layer);
    assert(item.state == 1 && (item.y >> 16) <= 80);

    /* sub_080441F8 cases 2-4: counted up to 9, then refused. */
    AosStats held = {0};
    aos_stats_new_game_inventory(&held);
    assert(held.weapon_counts[0] == 1 && held.armor_counts[0] == 1);
    bool sound = true;
    item.removed = false;
    assert(aos_pickup_collect_item(&item, &held, &sound) == AOS_COLLECT_TAKEN && !sound &&
           item.removed && held.weapon_counts[1] == 1);
    AosPickup potion = aos_pickup_create_item(AOS_PICKUP_ITEM, 3, 0, 0);
    assert(aos_pickup_collection_sound(&potion) == AOS_PICKUP_SOUND_ITEM);
    held.item_counts[3] = 8;
    assert(aos_pickup_collect_item(&potion, &held, &sound) == AOS_COLLECT_TAKEN &&
           held.item_counts[3] == 9);
    potion.removed = false;
    assert(aos_pickup_collect_item(&potion, &held, &sound) == AOS_COLLECT_FULL && sound &&
           !potion.removed && potion.cooldown == 0x20 && held.item_counts[3] == 9);
    /* While the cooldown runs the refusal is silent. */
    assert(aos_pickup_collect_item(&potion, &held, &sound) == AOS_COLLECT_FULL && !sound);
    AosPickup ring = aos_pickup_create_item(AOS_PICKUP_ARMOR, 44, 0, 0);
    assert(aos_pickup_collect_item(&ring, &held, &sound) == AOS_COLLECT_TAKEN &&
           held.armor_counts[44] == 1);
    /* The soul inventories: nibble counts in two groups per colour. */
    AosStats souls = {0};
    aos_souls_add(&souls, AOS_SOUL_RED, 3, 1);
    assert(souls.red_souls[0][1] == 0x10 && aos_souls_total(&souls, AOS_SOUL_RED, 3) == 1);
    aos_souls_add(&souls, AOS_SOUL_BLUE, 0, 12);
    assert(souls.blue_souls[0][0] == 9 && aos_souls_total(&souls, AOS_SOUL_BLUE, 0) == 9);
    aos_souls_add_group(&souls, 0, AOS_SOUL_BLUE, 0, -20);
    assert(aos_souls_total(&souls, AOS_SOUL_BLUE, 0) == 0);
    /* Group 1 gives back the amount when the total would pass 9. */
    aos_souls_add_group(&souls, 1, AOS_SOUL_YELLOW, 5, 4);
    aos_souls_add_group(&souls, 0, AOS_SOUL_YELLOW, 5, 5);
    aos_souls_add(&souls, AOS_SOUL_YELLOW, 5, 1);
    assert(souls.yellow_souls[1][2] == 0x30 && souls.yellow_souls[0][2] == 0x60 &&
           aos_souls_total(&souls, AOS_SOUL_YELLOW, 5) == 9);
    /* An ability soul's single array is read by both groups. */
    aos_souls_add(&souls, AOS_SOUL_ABILITY, 2, 1);
    assert(souls.ability_souls[1] == 0x01 && aos_souls_total(&souls, AOS_SOUL_ABILITY, 2) == 2);
    assert(aos_souls_total(&souls, 7, 0) == 0);
    aos_souls_set_ability(&souls, 9, true);
    assert(souls.abilities[1] == 0x02);
    aos_souls_set_ability(&souls, 9, false);
    assert(souls.abilities[1] == 0);
    souls.souls_collected = 998;
    aos_souls_count(&souls, 5);
    assert(souls.souls_collected == 999);

    /* sub_0804459C: one draw, heading away from the point above Soma; Soma
     * straight right of the soul gives a target up-right, and the heading
     * points the other way. */
    draws = 0;
    AosPickup soul = aos_pickup_create_soul(AOS_SOUL_RED, 7, 100, 100, 200 << 16, 100 << 16,
                                            counting_random);
    assert(draws == 1 && soul.type == AOS_PICKUP_SOUL && soul.variant == 7 &&
           soul.speed == 0x30000 && soul.turn == 0 && soul.blink == 200 &&
           soul.heading == ((aos_arctan2(100, -12) + 0x8000u) & 0xFFFF));
    assert(aos_pickup_collection_sound(&soul) == AOS_PICKUP_SOUND_SOUL);
    /* sub_080446D4: the turn step grows by 0x2A and the speed by 0x1800 a
     * frame, both capped; the trail draws twice on even timer values. */
    draws = 0;
    aos_pickup_soul_move(&soul, 200 << 16, 100 << 16);
    assert(soul.turn == 0x2A && soul.speed == 0x31800);
    aos_pickup_soul_finish(&soul, counting_random, NULL);
    assert(draws == 2 && soul.blink == 199);
    aos_pickup_soul_finish(&soul, counting_random, NULL);
    assert(draws == 2 && soul.blink == 198);
    for (int i = 0; i < 400; ++i) aos_pickup_soul_move(&soul, 200 << 16, 100 << 16);
    assert(soul.turn == 0x4000 && soul.speed == 0x80000);
    /* It homes in on Soma: after the turn ramps up it passes near him. */
    AosPickup homing = aos_pickup_create_soul(AOS_SOUL_BLUE, 1, 100, 100, 160 << 16,
                                              100 << 16, counting_random);
    int nearest = 1 << 30;
    for (int i = 0; i < 120; ++i) {
        aos_pickup_soul_move(&homing, 160 << 16, 100 << 16);
        int dx = (homing.x >> 16) - 160, dy = (homing.y >> 16) - 84;
        if (dx * dx + dy * dy < nearest) nearest = dx * dx + dy * dy;
    }
    assert(nearest < 16 * 16);
    /* 200 frames, then 64 blinking frames. */
    AosPickup timed = aos_pickup_create_soul(AOS_SOUL_RED, 0, 0, 0, 0, 0, counting_random);
    for (int i = 0; i < 200; ++i) aos_pickup_soul_finish(&timed, counting_random, NULL);
    assert(timed.state == 1 && timed.blinking && timed.blink == 0x40 && !timed.removed);
    for (int i = 0; i < 64; ++i) aos_pickup_soul_finish(&timed, counting_random, NULL);
    assert(timed.removed);
    /* The even-timer trail spark (sub_08045B44 type 8): position offsets,
     * velocity -soul / 4, acceleration soul / 64, soul-kind palette. */
    AosPickup trail = aos_pickup_create_soul(AOS_SOUL_YELLOW, 2, 0, 0,
                                             1 << 16, -(2 << 16), counting_random);
    trail.vx = 0x40000; trail.vy = -0x20000;
    AosSoulTrailSpark trail_spark;
    aos_pickup_soul_finish(&trail, counting_random, &trail_spark);
    assert(trail_spark.spawn && trail_spark.soul_type == AOS_SOUL_YELLOW &&
           trail_spark.vx == -0x10000 && trail_spark.vy == 0x8000 &&
           trail_spark.ax == 0x1000 && trail_spark.ay == -0x800);

    /* Cases 5-8 of sub_080441F8. */
    AosStats owner = {0};
    AosPickup ability = aos_pickup_create_soul(AOS_SOUL_ABILITY, 4, 0, 0, 0, 0, counting_random);
    assert(aos_pickup_collect_soul(&ability, &owner) && ability.removed &&
           (owner.abilities[0] & 0x10) && aos_souls_total(&owner, AOS_SOUL_ABILITY, 4) == 2 &&
           owner.souls_collected == 1);
    AosPickup red = aos_pickup_create_soul(AOS_SOUL_RED, 2, 0, 0, 0, 0, counting_random);
    assert(aos_pickup_collect_soul(&red, &owner));
    red.removed = false;
    assert(!aos_pickup_collect_soul(&red, &owner) &&
           aos_souls_total(&owner, AOS_SOUL_RED, 2) == 2 && owner.souls_collected == 3);
    /* sub_08043C10: hearts. A popped small heart rises 2 pixels per frame,
     * slowing by 0x2000, then turns at the top. */
    AosPickup heart = aos_pickup_create_heart(false, true, 40, 40);
    assert(heart.type == AOS_PICKUP_HEART && heart.variant == 3 && heart.small_heart &&
           heart.vy == (int32_t)0xFFFE0000);
    assert(aos_pickup_collection_sound(&heart) == AOS_PICKUP_SOUND_HEART);
    assert(aos_pickup_box_of(&heart).w == 8 && aos_pickup_box_of(&heart).x == -4);
    int32_t top = heart.y;
    int frames_up = 0;
    while (heart.state == 0 && frames_up < 40) {
        aos_pickup_small_heart_update(&heart, &layer);
        if (heart.y < top) top = heart.y;
        ++frames_up;
    }
    /* 16 rising frames (0x20000 / 0x2000), then one frame turning. */
    assert(heart.state == 1 && frames_up == 17 && heart.vy == 0);
    /* sub_08043CE4 state 1: 0.25 down a frame, swaying by half a sine; it
     * stops where the probe 4 pixels lower meets the floor (y 80). */
    int32_t x0 = heart.x;
    aos_pickup_small_heart_update(&heart, &layer);
    assert(heart.vy == 0x4000 && heart.vx == aos_sine(0x4000) >> 1 && heart.x == x0 + heart.vx);
    for (int i = 0; i < 400 && heart.state == 1; ++i)
        aos_pickup_small_heart_update(&heart, &layer);
    assert(heart.state == 2 && heart.blink == 200 && (heart.y >> 16) + 4 >= 79);
    for (int i = 0; i < 200; ++i) aos_pickup_small_heart_update(&heart, &layer);
    assert(heart.state == 3 && heart.blink == 0x40 && !heart.removed);
    for (int i = 0; i < 64; ++i) aos_pickup_small_heart_update(&heart, &layer);
    assert(heart.removed && heart.blinking);
    /* A big heart falls like money and restores 15 %. */
    AosPickup big = aos_pickup_create_heart(true, false, 40, 30);
    assert(big.variant == 15 && !big.small_heart && aos_pickup_box_of(&big).w == 8);
    for (int i = 0; i < 200 && big.state == 0; ++i) aos_pickup_update(&big, &layer);
    assert(big.state == 1);
    /* sub_08043BB0: 3 % of 80 MP is 2; 1 at least; capped at the maximum. */
    AosStats mp = {.max_mp = 80, .mp = 10};
    AosPickup small = aos_pickup_create_heart(false, false, 0, 0);
    aos_pickup_collect_heart(&small, &mp);
    assert(small.removed && mp.mp == 12);
    mp.max_mp = 20;
    mp.mp = 0;
    aos_pickup_collect_heart(&small, &mp);
    assert(mp.mp == 1);
    mp.max_mp = 80;
    mp.mp = 75;
    aos_pickup_collect_heart(&big, &mp);
    assert(mp.mp == 80);
    puts("Aria pickup tests passed");
    return 0;
}
