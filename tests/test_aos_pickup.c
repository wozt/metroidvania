/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_pickup.h"

#include <assert.h>
#include <stdio.h>

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

    /* Item pickups probe at their own height (no 8-pixel money offset). */
    AosPickup item = aos_pickup_create_item(AOS_PICKUP_WEAPON, 1, 40, 30);
    assert(item.type == AOS_PICKUP_WEAPON && item.variant == 1 && item.offset == 0);
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
    puts("Aria pickup tests passed");
    return 0;
}
