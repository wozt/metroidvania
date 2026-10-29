/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "aos_input.h"
#include "aos_soul_menu.h"

int main(void) {
    AosStats stats = {0};
    AosEquipmentTables equipment = {0};
    stats.base[AOS_STAT_STR] = 10;
    stats.base[AOS_STAT_CON] = 10;
    stats.max_mp = 80;
    stats.mp = 10;
    stats.weapon = 0;
    stats.armor = 0;
    stats.accessory = AOS_EQUIPMENT_NONE;
    aos_button_config_default(&stats.buttons);
    stats.weapon_counts[0] = stats.weapon_counts[7] = 1;
    stats.armor_counts[0] = stats.armor_counts[3] = stats.armor_counts[25] = 1;
    equipment.weapons[0].atk = 5;
    equipment.weapons[7].atk = 38;
    equipment.armor[0].def = 5;
    equipment.armor[3].def = 15;
    equipment.armor[25].def = 1;
    aos_stats_refresh(&stats, &equipment);
    aos_souls_add(&stats, AOS_SOUL_RED, 1, 1);      /* Bat, native ID 2 */
    aos_souls_add(&stats, AOS_SOUL_RED, 13, 2);     /* Rock Armor, ID 14 */
    aos_souls_add(&stats, AOS_SOUL_BLUE, 3, 1);     /* Buer, ID 4 */
    aos_souls_add(&stats, AOS_SOUL_YELLOW, 5, 1);   /* Peeping Eye, ID 6 */
    aos_souls_add(&stats, AOS_SOUL_ABILITY, 2, 1);  /* Malphas */
    stats.red_soul = 2;

    AosSoulMenu menu = {0};
    menu.sleep_available = true;
    assert(aos_soul_menu_update(&menu, &stats, &equipment, AOS_KEY_START, AOS_KEY_START) ==
           AOS_SOUL_MENU_OPENED);
    assert(menu.open && menu.page == AOS_SOUL_MENU_PAGE_HUB && menu.hub_cursor == 0);
    assert(!strcmp(aos_pause_entry_name(0), "SOUL SET"));
    assert(!strcmp(aos_pause_entry_name(6), "ENEMY"));

    /* The hub wraps exactly seven native entries and exposes unavailable pages. */
    assert(aos_soul_menu_update(&menu, &stats, &equipment, 0, AOS_KEY_UP) ==
           AOS_SOUL_MENU_MOVED);
    assert(menu.hub_cursor == AOS_PAUSE_ENEMY);
    assert(aos_soul_menu_update(&menu, &stats, &equipment, AOS_KEY_A, AOS_KEY_A) ==
           (AOS_SOUL_MENU_ACCEPTED | AOS_SOUL_MENU_UNAVAILABLE));
    assert(menu.page == AOS_SOUL_MENU_PAGE_UNAVAILABLE);
    assert(aos_soul_menu_update(&menu, &stats, &equipment, AOS_KEY_B, AOS_KEY_B) ==
           AOS_SOUL_MENU_BACK);
    assert(menu.page == AOS_SOUL_MENU_PAGE_HUB && menu.open);
    menu.hub_cursor = AOS_PAUSE_SOUL_SET;
    assert(aos_soul_menu_update(&menu, &stats, &equipment, AOS_KEY_A, AOS_KEY_A) ==
           AOS_SOUL_MENU_ACCEPTED);
    assert(menu.page == AOS_SOUL_MENU_PAGE_SOUL_SET && menu.category == AOS_SOUL_MENU_RED &&
           menu.cursor[0] == 2);
    assert(!strcmp(aos_soul_menu_name(menu.category, menu.cursor[0]), "Bat"));
    assert(aos_soul_menu_quantity(&menu, &stats) == 1 && aos_soul_menu_enabled(&menu, &stats));

    /* Down skips every unowned red soul and reaches Rock Armor. */
    aos_soul_menu_update(&menu, &stats, &equipment, 0, AOS_KEY_DOWN);
    assert(menu.cursor[0] == 14 && aos_soul_menu_quantity(&menu, &stats) == 2);
    assert(aos_soul_menu_update(&menu, &stats, &equipment, AOS_KEY_A, AOS_KEY_A) ==
           AOS_SOUL_MENU_CHANGED);
    assert(stats.red_soul == 14 && aos_soul_menu_enabled(&menu, &stats));
    aos_soul_menu_update(&menu, &stats, &equipment, 0, AOS_KEY_UP);
    assert(menu.cursor[0] == 2);
    aos_soul_menu_update(&menu, &stats, &equipment, 0, AOS_KEY_UP);
    assert(menu.cursor[0] == 0 && !strcmp(aos_soul_menu_name(0, 0), "None"));
    assert(aos_soul_menu_update(&menu, &stats, &equipment, AOS_KEY_A, AOS_KEY_A) ==
           AOS_SOUL_MENU_CHANGED && stats.red_soul == 0);

    aos_soul_menu_update(&menu, &stats, &equipment, AOS_KEY_R, AOS_KEY_R);
    assert(menu.category == AOS_SOUL_MENU_BLUE && menu.cursor[1] == 0);
    aos_soul_menu_update(&menu, &stats, &equipment, 0, AOS_KEY_DOWN);
    assert(menu.cursor[1] == 4);
    aos_soul_menu_update(&menu, &stats, &equipment, AOS_KEY_A, AOS_KEY_A);
    assert(stats.blue_soul == 4);
    aos_soul_menu_update(&menu, &stats, &equipment, AOS_KEY_R, AOS_KEY_R);
    assert(menu.category == AOS_SOUL_MENU_YELLOW && menu.cursor[2] == 0);
    aos_soul_menu_update(&menu, &stats, &equipment, 0, AOS_KEY_DOWN);
    assert(menu.cursor[2] == 6);
    /* Ability is the hub's separate native entry, not a fourth Soul Set family. */
    assert(aos_soul_menu_update(&menu, &stats, &equipment, AOS_KEY_B, AOS_KEY_B) ==
           AOS_SOUL_MENU_BACK);
    menu.hub_cursor = AOS_PAUSE_ABILITY;
    assert(aos_soul_menu_update(&menu, &stats, &equipment, AOS_KEY_A, AOS_KEY_A) ==
           AOS_SOUL_MENU_ACCEPTED);
    assert(menu.page == AOS_SOUL_MENU_PAGE_ABILITY && menu.category == AOS_SOUL_MENU_ABILITY &&
           menu.cursor[3] == 2);
    assert(!aos_soul_menu_enabled(&menu, &stats));
    assert(aos_soul_menu_update(&menu, &stats, &equipment, AOS_KEY_A, AOS_KEY_A) ==
           AOS_SOUL_MENU_CHANGED && aos_soul_menu_enabled(&menu, &stats));

    assert(aos_soul_menu_update(&menu, &stats, &equipment, AOS_KEY_B, AOS_KEY_B) ==
           AOS_SOUL_MENU_BACK);
    assert(menu.open && menu.page == AOS_SOUL_MENU_PAGE_HUB);

    /* EQUIP exposes only owned records, previews names and refreshes derived stats. */
    menu.hub_cursor = AOS_PAUSE_EQUIP;
    assert(aos_soul_menu_update(&menu, &stats, &equipment, AOS_KEY_A, AOS_KEY_A) ==
           AOS_SOUL_MENU_ACCEPTED);
    assert(menu.page == AOS_SOUL_MENU_PAGE_EQUIP && menu.equip_slot == AOS_EQUIP_WEAPON);
    assert(!strcmp(aos_equipment_name(AOS_EQUIP_WEAPON, 7), "Gram"));
    assert(aos_equipment_choice_count(&stats, AOS_EQUIP_WEAPON) == 3);
    assert(aos_soul_menu_update(&menu, &stats, &equipment, AOS_KEY_A, AOS_KEY_A) ==
           AOS_SOUL_MENU_ACCEPTED && menu.equip_selecting);
    assert(aos_soul_menu_update(&menu, &stats, &equipment, 0, AOS_KEY_DOWN) ==
           AOS_SOUL_MENU_MOVED && menu.equip_choice == 7);
    assert(aos_soul_menu_update(&menu, &stats, &equipment, AOS_KEY_A, AOS_KEY_A) ==
           (AOS_SOUL_MENU_ACCEPTED | AOS_SOUL_MENU_CHANGED));
    assert(stats.weapon == 7 && stats.atk == 48 && !menu.equip_selecting);

    aos_soul_menu_update(&menu, &stats, &equipment, 0, AOS_KEY_DOWN);
    assert(menu.equip_slot == AOS_EQUIP_ARMOR);
    aos_soul_menu_update(&menu, &stats, &equipment, AOS_KEY_A, AOS_KEY_A);
    aos_soul_menu_update(&menu, &stats, &equipment, 0, AOS_KEY_DOWN);
    assert(menu.equip_choice == 3);
    aos_soul_menu_update(&menu, &stats, &equipment, AOS_KEY_A, AOS_KEY_A);
    assert(stats.armor == 3 && stats.def == 20);

    aos_soul_menu_update(&menu, &stats, &equipment, 0, AOS_KEY_DOWN);
    assert(menu.equip_slot == AOS_EQUIP_ACCESSORY);
    aos_soul_menu_update(&menu, &stats, &equipment, AOS_KEY_A, AOS_KEY_A);
    aos_soul_menu_update(&menu, &stats, &equipment, 0, AOS_KEY_DOWN);
    assert(menu.equip_choice == 25 && aos_equipment_quantity(&stats, AOS_EQUIP_ACCESSORY, 25) == 1);
    aos_soul_menu_update(&menu, &stats, &equipment, AOS_KEY_A, AOS_KEY_A);
    assert(stats.accessory == 25 && stats.def == 21);
    assert(aos_soul_menu_update(&menu, &stats, &equipment, AOS_KEY_B, AOS_KEY_B) ==
           AOS_SOUL_MENU_BACK && menu.page == AOS_SOUL_MENU_PAGE_HUB);

    /* ITEM USE applies the four native effect kinds and never consumes keys. */
    AosItemUseRecord items[AOS_ITEM_RECORDS] = {0};
    AosItemVitals vitals = {100, 320};
    items[0] = (AosItemUseRecord){1, 0, 100};
    items[5] = (AosItemUseRecord){6, 1, 20};
    items[8] = (AosItemUseRecord){9, 2, 1};
    items[23] = (AosItemUseRecord){24, 3, 200};
    items[25] = (AosItemUseRecord){26, 4, 0};
    stats.item_counts[0] = 2;
    stats.item_counts[5] = stats.item_counts[8] = 1;
    stats.item_counts[23] = stats.item_counts[25] = 1;
    stats.status = 1;
    menu.hub_cursor = AOS_PAUSE_ITEM_USE;
    assert(aos_pause_menu_update(&menu, &stats, &equipment, items, &vitals,
                                 AOS_KEY_A, AOS_KEY_A) == AOS_SOUL_MENU_ACCEPTED);
    assert(menu.page == AOS_SOUL_MENU_PAGE_ITEM_USE && menu.item_cursor == 0);
    assert(aos_item_choice_count(&stats) == 5 && !strcmp(aos_item_name(23), "Rotten Meat"));
    assert(aos_pause_menu_update(&menu, &stats, &equipment, items, &vitals,
                                 AOS_KEY_A, AOS_KEY_A) == AOS_SOUL_MENU_ITEM_USED);
    assert(vitals.hp == 200 && stats.item_counts[0] == 1);
    vitals.hp = vitals.max_hp;
    assert(aos_pause_menu_update(&menu, &stats, &equipment, items, &vitals,
                                 AOS_KEY_A, AOS_KEY_A) == AOS_SOUL_MENU_REFUSED);
    assert(stats.item_counts[0] == 1);

    aos_pause_menu_update(&menu, &stats, &equipment, items, &vitals, 0, AOS_KEY_DOWN);
    assert(menu.item_cursor == 5);
    aos_pause_menu_update(&menu, &stats, &equipment, items, &vitals,
                          AOS_KEY_A, AOS_KEY_A);
    assert(stats.mp == 26 && !stats.item_counts[5] && menu.item_cursor == 8);
    aos_pause_menu_update(&menu, &stats, &equipment, items, &vitals,
                          AOS_KEY_A, AOS_KEY_A);
    assert(stats.status == 0 && !stats.item_counts[8] && menu.item_cursor == 23);
    aos_pause_menu_update(&menu, &stats, &equipment, items, &vitals,
                          AOS_KEY_A, AOS_KEY_A);
    assert(vitals.hp == 120 && !stats.item_counts[23] && menu.item_cursor == 25);
    assert(aos_pause_menu_update(&menu, &stats, &equipment, items, &vitals,
                                 AOS_KEY_A, AOS_KEY_A) == AOS_SOUL_MENU_REFUSED);
    assert(stats.item_counts[25] == 1);
    assert(aos_pause_menu_update(&menu, &stats, &equipment, items, &vitals,
                                 AOS_KEY_B, AOS_KEY_B) == AOS_SOUL_MENU_BACK &&
           menu.page == AOS_SOUL_MENU_PAGE_HUB);

    /* CONFIG stages native A/B/R/L indices, refuses duplicates and changes
     * the semantic player controls only after a valid apply. */
    menu.hub_cursor = AOS_PAUSE_CONFIG;
    assert(aos_soul_menu_update(&menu, &stats, &equipment, AOS_KEY_A, AOS_KEY_A) ==
           AOS_SOUL_MENU_ACCEPTED);
    assert(menu.page == AOS_SOUL_MENU_PAGE_CONFIG && menu.config_cursor == 0);
    assert(menu.config_buttons[AOS_BUTTON_ATTACK] == 1 &&
           menu.config_buttons[AOS_BUTTON_JUMP] == 0 &&
           menu.config_buttons[AOS_BUTTON_ABILITY] == 3 &&
           menu.config_buttons[AOS_BUTTON_GUARDIAN] == 2);
    assert(aos_button_config_remap(&stats.buttons, AOS_KEY_B | AOS_KEY_LEFT) ==
           (AOS_KEY_B | AOS_KEY_LEFT));
    assert(aos_soul_menu_update(&menu, &stats, &equipment, 0, AOS_KEY_RIGHT) ==
           AOS_SOUL_MENU_CONFIG_EDITED);
    assert(menu.config_buttons[AOS_BUTTON_ATTACK] == 2 &&
           !aos_pause_config_unique(&menu));
    assert(aos_soul_menu_update(&menu, &stats, &equipment, AOS_KEY_B, AOS_KEY_B) ==
           AOS_SOUL_MENU_REFUSED && menu.page == AOS_SOUL_MENU_PAGE_CONFIG);
    aos_soul_menu_update(&menu, &stats, &equipment, 0, AOS_KEY_DOWN);
    aos_soul_menu_update(&menu, &stats, &equipment, 0, AOS_KEY_DOWN);
    assert(menu.config_cursor == 2);
    assert(aos_soul_menu_update(&menu, &stats, &equipment, 0, AOS_KEY_LEFT) ==
           AOS_SOUL_MENU_CONFIG_EDITED);
    assert(aos_soul_menu_update(&menu, &stats, &equipment, AOS_KEY_B, AOS_KEY_B) ==
           (AOS_SOUL_MENU_BACK | AOS_SOUL_MENU_CONFIG_APPLIED));
    assert(menu.page == AOS_SOUL_MENU_PAGE_HUB &&
           stats.buttons.action[AOS_BUTTON_ATTACK] == AOS_KEY_R &&
           stats.buttons.action[AOS_BUTTON_GUARDIAN] == AOS_KEY_B);
    assert(aos_button_config_remap(&stats.buttons, AOS_KEY_R) == AOS_KEY_B);
    assert(aos_button_config_remap(&stats.buttons, AOS_KEY_B) == AOS_KEY_R);

    menu.hub_cursor = AOS_PAUSE_CONFIG;
    aos_soul_menu_update(&menu, &stats, &equipment, AOS_KEY_A, AOS_KEY_A);
    assert(aos_soul_menu_update(&menu, &stats, &equipment, AOS_KEY_SELECT,
                                AOS_KEY_SELECT) == AOS_SOUL_MENU_CONFIG_EDITED);
    assert(menu.config_buttons[AOS_BUTTON_ATTACK] == 1 &&
           menu.config_buttons[AOS_BUTTON_GUARDIAN] == 2);
    assert(aos_soul_menu_update(&menu, &stats, &equipment, AOS_KEY_B, AOS_KEY_B) ==
           (AOS_SOUL_MENU_BACK | AOS_SOUL_MENU_CONFIG_APPLIED));

    /* SLEEP defaults to No, waits for the host quicksave result and follows
     * the native 64-frame darkening plus 33-frame reset tail. */
    menu.hub_cursor = AOS_PAUSE_SLEEP;
    assert(aos_soul_menu_update(&menu, &stats, &equipment, AOS_KEY_A, AOS_KEY_A) ==
           AOS_SOUL_MENU_ACCEPTED);
    assert(menu.page == AOS_SOUL_MENU_PAGE_SLEEP && menu.sleep_choice == 1 &&
           menu.sleep_stage == AOS_SLEEP_CONFIRM);
    assert(aos_soul_menu_update(&menu, &stats, &equipment, 0, AOS_KEY_LEFT) ==
           AOS_SOUL_MENU_MOVED && menu.sleep_choice == 0);
    assert(aos_soul_menu_update(&menu, &stats, &equipment, AOS_KEY_A, AOS_KEY_A) ==
           AOS_SOUL_MENU_SLEEP_SAVE_REQUESTED && menu.sleep_stage == AOS_SLEEP_WAIT_SAVE);
    aos_pause_sleep_result(&menu, false);
    assert(menu.sleep_stage == AOS_SLEEP_CONFIRM && menu.sleep_choice == 1);
    aos_soul_menu_update(&menu, &stats, &equipment, 0, AOS_KEY_RIGHT);
    assert(aos_soul_menu_update(&menu, &stats, &equipment, AOS_KEY_A, AOS_KEY_A) ==
           AOS_SOUL_MENU_SLEEP_SAVE_REQUESTED);
    aos_pause_sleep_result(&menu, true);
    assert(menu.sleep_stage == AOS_SLEEP_FADING && menu.sleep_timer == 0);
    for (int frame = 0; frame < 96; ++frame)
        assert(aos_soul_menu_update(&menu, &stats, &equipment, 0, 0) == 0);
    assert(aos_soul_menu_update(&menu, &stats, &equipment, 0, 0) ==
           AOS_SOUL_MENU_SLEEP_FINISHED);
    menu.page = AOS_SOUL_MENU_PAGE_HUB;

    assert(aos_soul_menu_update(&menu, &stats, &equipment, AOS_KEY_START, AOS_KEY_START) ==
           AOS_SOUL_MENU_CLOSED && !menu.open);
    puts("aos soul menu tests passed");
    return 0;
}
