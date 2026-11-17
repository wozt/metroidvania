/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_menu.h"

#include "../../aos/core/aos_input.h"

static const char *const red_names[] = {
    "Winged Skeleton", "Bat", "Skeleton", "Merman", "Axe Armor", "Skull Archer",
    "Killer Fish", "Blue Crow", "Zombie Soldier", "Ghost", "Siren", "Tiny Devil",
    "Durga", "Rock Armor", "Student Witch", "Arachne", "Fleaman", "Evil Butcher",
    "Waiter Skeleton", "Altair", "Mudman", "Cockatrice", "Werewolf", "Harpy", "Une",
    "Needles", "Man-Eater", "Ukoback", "Fish Head", "Disc Armor", "Nightmare", "Slime",
    "Dryad", "Ripper", "Werejaguar", "Weretiger", "Biphron", "Mandragora",
    "Beam Skeleton", "Skull Millione", "Giant Skeleton", "Gladiator", "Demon Lord",
    "Flame Demon", "Lightning Doll", "Valkyrie", "Bomber Armor", "Red Minotaur",
    "Legion", "Balore", "Killer Doll", "Nemesis", "Killer Mantle", "Kyoma Demon",
    "Chronomage",
};
static const char *const blue_names[] = {
    "Flying Armor", "Giant Bat", "Black Panther", "Buer", "Witch", "Giant Ghost",
    "Final Guard", "Great Armor", "Catoblepas", "Bone Pillar", "Cagnazzo", "Big Golem",
    "Shadow Knight", "Creaking Skull", "Medusa Head", "Persephone", "Alura Une", "Devil",
    "Manticore", "Curly", "Sky Fish", "Imp", "Alastor", "Death",
};
static const char *const yellow_names[] = {
    "Undine", "Skula", "Zombie Officer", "Iron Golem", "Dead Warrior", "Peeping Eye",
    "Succubus", "Flesh Golem", "Tsuchinoko", "Giant Worm", "Wooden Golem", "Zombie",
    "Lubicant", "Headhunter", "Basilisk", "Erinys", "Mimic", "Arc Demon", "Poison Worm",
    "Ectoplasm", "Gargoyle", "Skeleton Knight", "Minotaur", "Golem", "Triton",
    "White Dragon", "Quezlcoatl", "Gorgon", "Dead Crusader", "Red Crow", "Lilith", "Bael",
    "Stolas", "Ghost Dancer", "Gremlin",
};
static const char *const ability_names[] = {
    "Grave Keeper", "Skeleton Blaze", "Malphas", "Kicker Skeleton", "Hippogryph", "Galamoth",
};

static const char *const category_names[] = {"RED", "BLUE", "YELLOW", "ABILITY"};
static const char *const pause_entry_names[] = {
    "SOUL SET", "EQUIP", "ITEM USE", "ABILITY", "SLEEP", "CONFIG", "ENEMY",
};
static const char *const equipment_slot_names[] = {"WEAPON", "ARMOR", "ACCESSORY"};
static const char *const weapon_names[AOS_WEAPON_RECORDS] = {
    "Knife", "Baselard", "Combat Knife", "Short Sword", "Bastard Sword", "Whip Sword",
    "Gladius", "Gram", "Milican's Sword", "Hrunting", "Mystletain", "Rahab's Sword",
    "Durandal", "Laevatain", "Burtgang", "Kaladbolg", "Vjaya", "Balmung", "Broadsword",
    "Scimitar", "Claymore", "Great Sword", "Joyeuse", "Dainslef", "Ascalon",
    "Claimh Solais", "Final Sword", "Bamboo Sword", "Katana", "Osafune", "Onikiri",
    "Kunitsuna", "Yasutsuna", "Muramasa", "Hammer", "Warhammer", "Excalibur",
    "Tallhammer", "Battle Axe", "Death's Sickle", "Rapier", "Estoc", "Cutall", "Spear",
    "Trident", "Lance", "Partizan", "Gabolg", "Gungner", "Ronginus' Spear", "Cestus",
    "Kaiser Knuckle", "Mach Punch", "Whip Knuckle", "Poison Fist", "Handgun", "Silver Gun",
    "Positron Rifle", "Valmanway",
};
static const char *const armor_names[AOS_ARMOR_RECORDS] = {
    "Casual Clothes", "Cloth Tunic", "Gym Clothes", "War Fatigues", "Ninja Suit",
    "Soldier Uniform", "Pitch Black Suit", "Olrox's Suit", "Dracula's Tunic", "Leather Plate",
    "Copper Plate", "Iron Plate", "Steel Plate", "Silver Plate", "Gold Plate", "Eversing",
    "Samurai Armor", "Silk Robe", "Elfin Robe", "Death's Robe", "Demon's Mail",
    "Armor of Fire", "Armor of Water", "Blocking Mail", "Army Jacket", "Cape",
    "Crimson Cloak", "Black Cloak", "Pendant", "Heart Pendant", "Scarf", "Red Scarf",
    "Ancient Belt", "Black Belt", "Skull Necklace", "Flame Necklace", "Satan's Ring",
    "Tear Of Blood", "Lucky Charm", "Rare Ring", "Soul Eater Ring", "Rune Ring",
    "Sherman Ring", "Gold Ring", "Chaos Ring",
};
static const char *const item_names[AOS_ITEM_RECORDS] = {
    "Potion", "High Potion", "Super Potion", "Meat Strip", "Tasty Meat", "Mind Up",
    "High Mind Up", "Mana Prism", "Anti-Venom", "Uncurse Potion", "Potato Pancake",
    "Beef Curry", "Ramen", "Cream Soda", "Cream Puff", "Milk", "Coffee", "Tea",
    "Pudding", "Strawberry", "Melon", "Grapes", "Persimmon", "Rotten Meat",
    "Spoiled Milk", "Skull Key", "Ancient Book 1", "Ancient Book 2", "Ancient Book 3",
    "CASTLE MAP 1", "CASTLE MAP 2", "CASTLE MAP 3",
};

/* 0x08525510: the config screen presents Guardian before Ability although
 * their fields are stored in the opposite order. */
static const uint8_t config_rows[AOS_BUTTON_ACTIONS] = {
    AOS_BUTTON_ATTACK, AOS_BUTTON_JUMP, AOS_BUTTON_GUARDIAN, AOS_BUTTON_ABILITY,
};

const char *aos_pause_entry_name(uint8_t entry) {
    return entry < AOS_PAUSE_ENTRIES ? pause_entry_names[entry] : "UNKNOWN";
}

const char *aos_equipment_slot_name(uint8_t slot) {
    return slot < AOS_EQUIP_SLOTS ? equipment_slot_names[slot] : "UNKNOWN";
}

const char *aos_equipment_name(uint8_t slot, uint8_t item) {
    if (item == AOS_EQUIPMENT_NONE) return "None";
    if (slot == AOS_EQUIP_WEAPON)
        return item < AOS_WEAPON_RECORDS ? weapon_names[item] : "Unknown";
    return slot < AOS_EQUIP_SLOTS && item < AOS_ARMOR_RECORDS ? armor_names[item] : "Unknown";
}

uint8_t aos_equipment_current(const AosStats *stats, uint8_t slot) {
    if (slot == AOS_EQUIP_WEAPON) return stats->weapon;
    if (slot == AOS_EQUIP_ARMOR) return stats->armor;
    if (slot == AOS_EQUIP_ACCESSORY) return stats->accessory;
    return AOS_EQUIPMENT_NONE;
}

static bool equipment_owned(const AosStats *stats, uint8_t slot, uint8_t item) {
    if (slot == AOS_EQUIP_WEAPON)
        return item < AOS_WEAPON_RECORDS && stats->weapon_counts[item] != 0;
    if (slot == AOS_EQUIP_ARMOR)
        return item < 25 && stats->armor_counts[item] != 0;
    return slot == AOS_EQUIP_ACCESSORY && item >= 25 && item < AOS_ARMOR_RECORDS &&
           stats->armor_counts[item] != 0;
}

uint8_t aos_equipment_quantity(const AosStats *stats, uint8_t slot, uint8_t item) {
    if (item == AOS_EQUIPMENT_NONE) return 0;
    if (!equipment_owned(stats, slot, item)) return 0;
    return slot == AOS_EQUIP_WEAPON ? stats->weapon_counts[item] : stats->armor_counts[item];
}

int aos_equipment_choice_count(const AosStats *stats, uint8_t slot) {
    int count = 1;
    int first = slot == AOS_EQUIP_ACCESSORY ? 25 : 0;
    int limit = slot == AOS_EQUIP_WEAPON ? AOS_WEAPON_RECORDS
                : slot == AOS_EQUIP_ARMOR ? 25
                : slot == AOS_EQUIP_ACCESSORY ? AOS_ARMOR_RECORDS
                                             : 0;
    for (int item = first; item < limit; ++item) count += equipment_owned(stats, slot, item);
    return count;
}

uint8_t aos_equipment_choice_at(const AosStats *stats, uint8_t slot, int ordinal) {
    if (ordinal <= 0) return AOS_EQUIPMENT_NONE;
    int first = slot == AOS_EQUIP_ACCESSORY ? 25 : 0;
    int limit = slot == AOS_EQUIP_WEAPON ? AOS_WEAPON_RECORDS
                : slot == AOS_EQUIP_ARMOR ? 25
                : slot == AOS_EQUIP_ACCESSORY ? AOS_ARMOR_RECORDS
                                             : 0;
    for (int item = first; item < limit; ++item)
        if (equipment_owned(stats, slot, (uint8_t)item) && --ordinal == 0) return (uint8_t)item;
    return AOS_EQUIPMENT_NONE;
}

static bool recent_equipment(const AosStats *stats, const AosPickupRecent *recent,
                             uint8_t slot, uint8_t item) {
    if (!recent || slot >= AOS_EQUIP_SLOTS || !equipment_owned(stats, slot, item))
        return false;
    for (unsigned index = 0; index < AOS_PICKUP_RECENT_ITEMS; ++index)
        if (recent->item[slot][index] == item) return true;
    return false;
}

uint8_t aos_equipment_ordered_choice_at(const AosStats *stats,
                                        const AosPickupRecent *recent,
                                        uint8_t slot, int ordinal) {
    if (ordinal <= 0) return AOS_EQUIPMENT_NONE;
    if (recent && slot < AOS_EQUIP_SLOTS)
        for (unsigned index = 0; index < AOS_PICKUP_RECENT_ITEMS; ++index) {
            uint8_t item = recent->item[slot][index];
            if (!equipment_owned(stats, slot, item)) continue;
            bool duplicate = false;
            for (unsigned previous = 0; previous < index; ++previous)
                if (recent->item[slot][previous] == item) duplicate = true;
            if (!duplicate && --ordinal == 0) return item;
        }
    int first = slot == AOS_EQUIP_ACCESSORY ? 25 : 0;
    int limit = slot == AOS_EQUIP_WEAPON ? AOS_WEAPON_RECORDS
                : slot == AOS_EQUIP_ARMOR ? 25
                : slot == AOS_EQUIP_ACCESSORY ? AOS_ARMOR_RECORDS
                                             : 0;
    for (int item = first; item < limit; ++item)
        if (equipment_owned(stats, slot, (uint8_t)item) &&
            !recent_equipment(stats, recent, slot, (uint8_t)item) && --ordinal == 0)
            return (uint8_t)item;
    return AOS_EQUIPMENT_NONE;
}

const char *aos_item_name(uint8_t item) {
    return item < AOS_ITEM_RECORDS ? item_names[item] : "Unknown";
}

int aos_item_choice_count(const AosStats *stats) {
    int count = 0;
    for (int item = 0; item < AOS_ITEM_RECORDS; ++item) count += stats->item_counts[item] != 0;
    return count;
}

uint8_t aos_item_choice_at(const AosStats *stats, int ordinal) {
    if (ordinal < 0) return AOS_EQUIPMENT_NONE;
    for (int item = 0; item < AOS_ITEM_RECORDS; ++item)
        if (stats->item_counts[item] && ordinal-- == 0) return (uint8_t)item;
    return AOS_EQUIPMENT_NONE;
}

bool aos_item_use(AosStats *stats, const AosItemUseRecord items[AOS_ITEM_RECORDS],
                  AosItemVitals *vitals, uint8_t item) {
    if (!items || !vitals || item >= AOS_ITEM_RECORDS || !stats->item_counts[item]) return false;
    const AosItemUseRecord *record = &items[item];
    /* sub_0804B36C: with the Flesh Golem soul (0x13260 bit 0x100000) a
     * harmful item (kind 3) heals like kind 0. */
    uint8_t effect = record->effect;
    if (effect == 3 && (stats->flags & 0x100000u)) effect = 0;
    switch (effect) {
    case 0:
        if (vitals->hp >= vitals->max_hp) return false;
        vitals->hp = (int16_t)(vitals->hp + record->amount);
        if (vitals->hp > vitals->max_hp) vitals->hp = vitals->max_hp;
        break;
    case 1: {
        if (stats->mp >= (int16_t)stats->max_mp || !record->amount) return false;
        /* sub_0804B36C kind 1: max_mp / (100 / amount), truncated to u16
         * with no floor; a zero recovery still consumes the item. */
        int divisor = 100 / record->amount;
        int recovery = divisor ? stats->max_mp / divisor : stats->max_mp;
        stats->mp = (int16_t)(stats->mp + recovery);
        if (stats->mp > (int16_t)stats->max_mp) stats->mp = (int16_t)stats->max_mp;
        break;
    }
    case 2:
        /* sub_08021E50: for each of the three status bits in `amount`,
         * zero its 0x13272/0x13274/0x13276 timer and clear the bit. */
        if (!(stats->status & record->amount)) return false;
        for (int bit = 0; bit < 3; ++bit)
            if (record->amount & (1u << bit)) {
                stats->status &= (uint16_t)~(1u << bit);
                stats->status_timers[bit] = 0;
            }
        break;
    case 3:
        if (vitals->hp <= 1) return false;
        vitals->hp = (int16_t)(vitals->hp - record->amount);
        if (vitals->hp < 1) vitals->hp = 1;
        break;
    default: return false;
    }
    stats->item_counts[item]--;
    return true;
}

int aos_soul_menu_count(uint8_t category) {
    static const uint8_t counts[] = {55, 24, 35, 6};
    return category < AOS_SOUL_MENU_CATEGORIES ? counts[category] : 0;
}

const char *aos_soul_menu_category_name(uint8_t category) {
    return category < AOS_SOUL_MENU_CATEGORIES ? category_names[category] : "UNKNOWN";
}

const char *aos_soul_menu_name(uint8_t category, uint8_t cursor) {
    if (category >= AOS_SOUL_MENU_CATEGORIES) return "Unknown";
    if (category != AOS_SOUL_MENU_ABILITY) {
        if (!cursor) return "None";
        cursor--;
    }
    if (cursor >= aos_soul_menu_count(category)) return "Unknown";
    const char *const *names = category == AOS_SOUL_MENU_RED      ? red_names
                               : category == AOS_SOUL_MENU_BLUE   ? blue_names
                               : category == AOS_SOUL_MENU_YELLOW ? yellow_names
                                                                  : ability_names;
    return names[cursor];
}

int aos_soul_menu_inventory_index(const AosSoulMenu *menu) {
    if (menu->category >= AOS_SOUL_MENU_CATEGORIES) return -1;
    if (menu->category == AOS_SOUL_MENU_ABILITY) return menu->cursor[menu->category];
    return menu->cursor[menu->category] ? menu->cursor[menu->category] - 1 : -1;
}

uint8_t aos_soul_menu_quantity(const AosSoulMenu *menu, const AosStats *stats) {
    int index = aos_soul_menu_inventory_index(menu);
    return index < 0 ? 0 : aos_souls_total(stats, menu->category, index);
}

bool aos_soul_menu_enabled(const AosSoulMenu *menu, const AosStats *stats) {
    int index = aos_soul_menu_inventory_index(menu);
    if (index < 0) return false;
    if (menu->category == AOS_SOUL_MENU_ABILITY)
        return stats->abilities[index >> 3] & (1u << (index & 7));
    const uint8_t equipped = menu->category == AOS_SOUL_MENU_RED      ? stats->red_soul
                             : menu->category == AOS_SOUL_MENU_BLUE   ? stats->blue_soul
                                                                       : stats->yellow_soul;
    return equipped == menu->cursor[menu->category];
}

static bool available(const AosSoulMenu *menu, const AosStats *stats, int cursor) {
    if (menu->category != AOS_SOUL_MENU_ABILITY && cursor == 0) return true;
    int index = menu->category == AOS_SOUL_MENU_ABILITY ? cursor : cursor - 1;
    return aos_souls_total(stats, menu->category, index) != 0;
}

static void move_cursor(AosSoulMenu *menu, const AosStats *stats, int direction) {
    int choices = aos_soul_menu_count(menu->category) +
                  (menu->category == AOS_SOUL_MENU_ABILITY ? 0 : 1);
    int cursor = menu->cursor[menu->category];
    for (int tried = 0; tried < choices; ++tried) {
        cursor = (cursor + direction + choices) % choices;
        if (available(menu, stats, cursor)) {
            menu->cursor[menu->category] = (uint8_t)cursor;
            return;
        }
    }
}

static void initialise_cursors(AosSoulMenu *menu, const AosStats *stats) {
    menu->cursor[AOS_SOUL_MENU_RED] = stats->red_soul <= 55 ? stats->red_soul : 0;
    menu->cursor[AOS_SOUL_MENU_BLUE] = stats->blue_soul <= 24 ? stats->blue_soul : 0;
    menu->cursor[AOS_SOUL_MENU_YELLOW] = stats->yellow_soul <= 35 ? stats->yellow_soul : 0;
    if (!available(menu, stats, menu->cursor[menu->category]))
        move_cursor(menu, stats, 1);
}

/* sub_08047764's state-4 entry tail: the page init each hub entry runs
 * once the fade-out has reached black (or immediately for Sleep, whose
 * state-3 branch enters state 6 without a fade). */
static unsigned enter_hub_entry(AosSoulMenu *menu, const AosStats *stats,
                                uint8_t entry) {
    if (entry == AOS_PAUSE_ITEM_USE) {
        menu->page = AOS_SOUL_MENU_PAGE_ITEM_USE;
        /* func_0804B808's entry reset: +0x15/+0x16 start at 0 and +0x17
         * resolves to the first owned record (0xFF when none). */
        menu->item_ord = 0;
        menu->item_low = 0;
        menu->item_cursor = aos_item_choice_at(stats, 0);
        return AOS_SOUL_MENU_ACCEPTED;
    }
    if (entry == AOS_PAUSE_EQUIP) {
        menu->page = AOS_SOUL_MENU_PAGE_EQUIP;
        menu->equip_slot = AOS_EQUIP_WEAPON;
        menu->equip_selecting = false;
        return AOS_SOUL_MENU_ACCEPTED;
    }
    if (entry == AOS_PAUSE_SOUL_SET) {
        menu->page = AOS_SOUL_MENU_PAGE_SOUL_SET;
        menu->category = AOS_SOUL_MENU_RED;
        initialise_cursors(menu, stats);
        return AOS_SOUL_MENU_ACCEPTED;
    }
    if (entry == AOS_PAUSE_ABILITY) {
        menu->page = AOS_SOUL_MENU_PAGE_ABILITY;
        menu->category = AOS_SOUL_MENU_ABILITY;
        initialise_cursors(menu, stats);
        return AOS_SOUL_MENU_ACCEPTED;
    }
    if (entry == AOS_PAUSE_SLEEP) {
        menu->page = AOS_SOUL_MENU_PAGE_SLEEP;
        menu->sleep_choice = 1;
        menu->sleep_stage = AOS_SLEEP_CONFIRM;
        menu->sleep_timer = 0;
        /* sub_0804BD38's enter path prints no result into the bottom
         * window, so a stale outcome from a previous visit is dropped. */
        menu->sleep_result = AOS_SLEEP_RESULT_NONE;
        return AOS_SOUL_MENU_ACCEPTED;
    }
    if (entry == AOS_PAUSE_CONFIG) {
        AosButtonConfig buttons = stats->buttons;
        if (!aos_button_config_valid(&buttons)) aos_button_config_default(&buttons);
        menu->page = AOS_SOUL_MENU_PAGE_CONFIG;
        menu->config_cursor = 0;
        for (int action = 0; action < AOS_BUTTON_ACTIONS; ++action)
            menu->config_buttons[action] =
                aos_button_config_index(buttons.action[action]);
        return AOS_SOUL_MENU_ACCEPTED;
    }
    if (entry == AOS_PAUSE_ENEMY) {
        menu->page = AOS_SOUL_MENU_PAGE_ENEMY;
        aos_bestiary_init(&menu->enemy);
        return AOS_SOUL_MENU_ACCEPTED;
    }
    menu->page = AOS_SOUL_MENU_PAGE_UNAVAILABLE;
    return AOS_SOUL_MENU_ACCEPTED | AOS_SOUL_MENU_UNAVAILABLE;
}

/* sub_08047764's transition arming: state 4/1 run the +0x4C fade counter
 * at 3 steps per frame, then apply pending_page. */
static void fade_begin_out(AosSoulMenu *menu) {
    menu->fade = 0;
    menu->fade_phase = AOS_MENU_FADE_OUT;
}
static void fade_begin_entry(AosSoulMenu *menu, uint8_t entry) {
    menu->pending_page = entry;
    menu->pending_close = false;
    menu->fade_open = false;
    fade_begin_out(menu);
}
static void fade_begin_hub(AosSoulMenu *menu) {
    fade_begin_entry(menu, 0xFE);
}
static void fade_begin_close(AosSoulMenu *menu) {
    menu->pending_page = 0xFF;
    menu->pending_close = true;
    menu->fade_open = false;
    fade_begin_out(menu);
}

static unsigned update_hub(AosSoulMenu *menu, const AosStats *stats,
                           uint16_t new_input, uint16_t repeated_input) {
    /* sub_08047764 state 3: only B leaves the hub; START is ignored by
     * every state of the pause machine. */
    if (new_input & AOS_KEY_B) {
        fade_begin_close(menu);
        return AOS_SOUL_MENU_BACK;
    }
    if (repeated_input & AOS_KEY_UP) {
        menu->hub_cursor = (uint8_t)((menu->hub_cursor + AOS_PAUSE_ENTRIES - 1) %
                                     AOS_PAUSE_ENTRIES);
        return AOS_SOUL_MENU_MOVED;
    }
    if (repeated_input & AOS_KEY_DOWN) {
        menu->hub_cursor = (uint8_t)((menu->hub_cursor + 1) % AOS_PAUSE_ENTRIES);
        return AOS_SOUL_MENU_MOVED;
    }
    if (!(new_input & AOS_KEY_A)) return 0;
    if (menu->hub_cursor == AOS_PAUSE_SLEEP)
        /* The native machine jumps state 3 -> state 6 for Sleep: no fade. */
        return enter_hub_entry(menu, stats, menu->hub_cursor);
    fade_begin_entry(menu, menu->hub_cursor);
    return AOS_SOUL_MENU_ACCEPTED;
}

static int equipment_ordinal(const AosStats *stats, const AosPickupRecent *recent,
                             uint8_t slot, uint8_t item) {
    int count = aos_equipment_choice_count(stats, slot);
    for (int ordinal = 0; ordinal < count; ++ordinal)
        if (aos_equipment_ordered_choice_at(stats, recent, slot, ordinal) == item)
            return ordinal;
    return 0;
}

/* sub_080461BC: the shared pause-page list cursor used by the Equip
 * (func_0804AFD4), Item Use (func_0804B808), Ability (func_0804BB40) and
 * other page updates. The state triple is +0x14 count, +0x15 cursor
 * ordinal and +0x16 signed window low; the list is two columns of eight
 * rows and R/L page by eight cells. The directional word is tested in
 * priority order (horizontal, up, down, page down, page up) and the
 * first matching guard wins; a failed guard blocks every lower-priority
 * direction. There is no wrap: edges clamp and an 8-cell window scroll
 * replaces the old +/-1 ordinal wrap. Returns 0 no move, 1 cursor move,
 * 2 window scroll (the native PlaySong(0xF1) edge case is host audio,
 * tracked through AOS_SOUL_MENU_MOVED instead). */
static int pause_grid_move(int *cursor_io, int *low_io, int count,
                           uint16_t direction) {
    int old_low = *low_io;
    int cursor = *cursor_io;
    int low = *low_io;
    int moved = 0;
    if (direction & (AOS_KEY_LEFT | AOS_KEY_RIGHT)) {
        if (cursor + 1 < count || (cursor & 1)) {
            cursor ^= 1;
            moved = 1;
        }
    } else if (direction & AOS_KEY_UP) {
        if (cursor > 1) {
            cursor -= 2;
            if (cursor < low) low -= 2;
            moved = 1;
        }
    } else if (direction & AOS_KEY_DOWN) {
        if (cursor < count - 1) {
            cursor += 2;
            if (cursor >= count) cursor = count - 1;
            if (cursor >= low + 8) low += 2;
            moved = 1;
        }
    } else if (direction & AOS_KEY_R) {
        if (old_low + 8 < count) {
            cursor += 8;
            low += 8;
            if (cursor > count - 1) cursor = count - 1;
            moved = 1;
        } else if (cursor < count - 1) {
            /* sub_080461BC: with the window already at the bottom a page
             * press still commits and clamps the cursor onto the last
             * cell instead of scrolling. */
            cursor = count - 1;
            moved = 1;
        }
    } else if (direction & AOS_KEY_L) {
        if (low > 7) {
            cursor -= 8;
            low -= 8;
        } else if (cursor != 0) {
            cursor = cursor <= 7 ? 0 : cursor - 8;
            low = 0;
        }
    }
    if (low != old_low) moved = 2;
    *cursor_io = cursor;
    *low_io = low;
    return moved;
}

static int move_equipment_choice(AosSoulMenu *menu, const AosStats *stats,
                                 uint16_t direction) {
    int count = aos_equipment_choice_count(stats, menu->equip_slot);
    int ordinal = equipment_ordinal(stats, menu->recent_items, menu->equip_slot,
                                    menu->equip_choice);
    if (menu->equip_ord < count) ordinal = menu->equip_ord;
    int low = menu->equip_low;
    int moved = pause_grid_move(&ordinal, &low, count, direction);
    menu->equip_ord = (uint8_t)ordinal;
    menu->equip_low = (int8_t)low;
    menu->equip_choice = aos_equipment_ordered_choice_at(
        stats, menu->recent_items, menu->equip_slot, ordinal);
    return moved;
}

static unsigned update_equipment(AosSoulMenu *menu, AosStats *stats,
                                 const AosEquipmentTables *equipment,
                                 uint16_t new_input, uint16_t repeated_input) {
    /* START is ignored inside the pause machine; B fades back to the hub. */
    if (new_input & AOS_KEY_B) {
        if (menu->equip_selecting) menu->equip_selecting = false;
        else fade_begin_hub(menu);
        return AOS_SOUL_MENU_BACK;
    }
    if (!menu->equip_selecting) {
        if (repeated_input & AOS_KEY_UP) {
            menu->equip_slot = (uint8_t)((menu->equip_slot + AOS_EQUIP_SLOTS - 1) %
                                         AOS_EQUIP_SLOTS);
            return AOS_SOUL_MENU_MOVED;
        }
        if (repeated_input & AOS_KEY_DOWN) {
            menu->equip_slot = (uint8_t)((menu->equip_slot + 1) % AOS_EQUIP_SLOTS);
            return AOS_SOUL_MENU_MOVED;
        }
        if (!(new_input & AOS_KEY_A)) return 0;
        menu->equip_choice = aos_equipment_current(stats, menu->equip_slot);
        if (!equipment_owned(stats, menu->equip_slot, menu->equip_choice))
            menu->equip_choice = AOS_EQUIPMENT_NONE;
        /* func_0804AFD4 enters the item list with the cursor on the
         * currently equipped record; the +0x16 window low starts at 0
         * unless the selection already sits below the first window. */
        menu->equip_ord = (uint8_t)equipment_ordinal(
            stats, menu->recent_items, menu->equip_slot, menu->equip_choice);
        int ord = menu->equip_ord;
        menu->equip_low = (int8_t)(ord > 7 ? ord - 7 : 0);
        menu->equip_selecting = true;
        return AOS_SOUL_MENU_ACCEPTED;
    }
    if (repeated_input & (AOS_KEY_UP | AOS_KEY_DOWN | AOS_KEY_LEFT |
                          AOS_KEY_RIGHT | AOS_KEY_L | AOS_KEY_R))
        return move_equipment_choice(menu, stats, repeated_input)
                   ? AOS_SOUL_MENU_MOVED
                   : 0;
    if (!(new_input & AOS_KEY_A)) return 0;
    uint8_t *equipped = menu->equip_slot == AOS_EQUIP_WEAPON ? &stats->weapon
                        : menu->equip_slot == AOS_EQUIP_ARMOR ? &stats->armor
                                                              : &stats->accessory;
    bool changed = *equipped != menu->equip_choice;
    if (menu->recent_items && *equipped != AOS_EQUIPMENT_NONE)
        aos_pickup_recent_exchange(menu->recent_items, stats, menu->equip_slot,
                                   menu->equip_choice);
    *equipped = menu->equip_choice;
    if (equipment) aos_stats_refresh(stats, equipment);
    menu->equip_selecting = false;
    return AOS_SOUL_MENU_ACCEPTED | (changed ? AOS_SOUL_MENU_CHANGED : 0);
}

/* func_0804B808 keeps +0x15 as an ordinal over the OWNED item list
 * (empty slots are skipped by the +0x17 slot resolver), not a raw item
 * index, so the same sub_080461BC grid cursor drives it. */
static int move_item_cursor(AosSoulMenu *menu, const AosStats *stats,
                            uint16_t direction) {
    int count = aos_item_choice_count(stats);
    int ordinal = menu->item_ord;
    if (ordinal >= count) ordinal = 0;
    int low = menu->item_low;
    int moved = pause_grid_move(&ordinal, &low, count, direction);
    menu->item_ord = (uint8_t)ordinal;
    menu->item_low = (int8_t)low;
    menu->item_cursor = aos_item_choice_at(stats, ordinal);
    return moved;
}

static unsigned update_item_use(AosSoulMenu *menu, AosStats *stats,
                                const AosItemUseRecord items[AOS_ITEM_RECORDS],
                                AosItemVitals *vitals,
                                uint16_t new_input, uint16_t repeated_input) {
    /* START is ignored inside the pause machine; B fades back to the hub. */
    if (new_input & AOS_KEY_B) {
        fade_begin_hub(menu);
        return AOS_SOUL_MENU_BACK;
    }
    if (repeated_input & (AOS_KEY_UP | AOS_KEY_DOWN | AOS_KEY_LEFT |
                          AOS_KEY_RIGHT | AOS_KEY_L | AOS_KEY_R))
        return move_item_cursor(menu, stats, repeated_input)
                   ? AOS_SOUL_MENU_MOVED
                   : 0;
    if (!(new_input & AOS_KEY_A)) return 0;
    uint8_t used = menu->item_cursor;
    /* sub_0804B808 keeps the attempted record for the sound pick even
     * when sub_0804B36C refuses the use. */
    if (used < AOS_ITEM_RECORDS) menu->last_item = used;
    if (!aos_item_use(stats, items, vitals, used)) return AOS_SOUL_MENU_REFUSED;
    /* sub_0804B808 tail: when the use emptied the last visible cell the
     * cursor ordinal drops one row/window instead of advancing onto an
     * empty slot. */
    int count = aos_item_choice_count(stats);
    if (!stats->item_counts[used] && menu->item_ord == count) {
        if (menu->item_low == 0) {
            if (menu->item_ord) menu->item_ord--;
        } else {
            if (menu->item_ord == menu->item_low) menu->item_low -= 8;
            menu->item_ord--;
        }
        menu->item_cursor = aos_item_choice_at(stats, menu->item_ord);
    } else {
        menu->item_cursor = aos_item_choice_at(stats, menu->item_ord);
    }
    return AOS_SOUL_MENU_ITEM_USED;
}

bool aos_pause_config_unique(const AosSoulMenu *menu) {
    uint8_t used = 0;
    for (int action = 0; action < AOS_BUTTON_ACTIONS; ++action) {
        uint8_t index = menu->config_buttons[action];
        if (index >= 4 || (used & (1u << index))) return false;
        used |= (uint8_t)(1u << index);
    }
    return used == 0x0F;
}

void aos_pause_sleep_result(AosSoulMenu *menu, bool saved) {
    if (!menu || menu->page != AOS_SOUL_MENU_PAGE_SLEEP ||
        menu->sleep_stage != AOS_SLEEP_WAIT_SAVE)
        return;
    menu->sleep_stage = saved ? AOS_SLEEP_FADING : AOS_SLEEP_CONFIRM;
    menu->sleep_choice = 1;
    menu->sleep_timer = 0;
    /* sub_0804BE0C writes the outcome into the bottom window: 0x36B on
     * a successful save, 0x386 on failure. The stage transitions above
     * keep it visible through the fade / until the page is re-entered. */
    menu->sleep_result = saved ? AOS_SLEEP_RESULT_SAVED : AOS_SLEEP_RESULT_FAILED;
}

static unsigned update_sleep(AosSoulMenu *menu, uint16_t new_input,
                             uint16_t repeated_input) {
    if (menu->sleep_stage == AOS_SLEEP_WAIT_SAVE) return 0;
    if (menu->sleep_stage == AOS_SLEEP_FADING) {
        if (menu->sleep_timer < 97) menu->sleep_timer++;
        return menu->sleep_timer == 97 ? AOS_SOUL_MENU_SLEEP_FINISHED : 0;
    }
    if (!menu->sleep_available) {
        if (new_input & (AOS_KEY_A | AOS_KEY_B)) {
            menu->page = AOS_SOUL_MENU_PAGE_HUB;
            return AOS_SOUL_MENU_BACK;
        }
        return 0;
    }
    if (new_input & AOS_KEY_B ||
        ((new_input & AOS_KEY_A) && menu->sleep_choice != 0)) {
        menu->page = AOS_SOUL_MENU_PAGE_HUB;
        return AOS_SOUL_MENU_BACK;
    }
    if (repeated_input & (AOS_KEY_LEFT | AOS_KEY_RIGHT)) {
        menu->sleep_choice ^= 1;
        return AOS_SOUL_MENU_MOVED;
    }
    if ((new_input & AOS_KEY_A) && menu->sleep_choice == 0) {
        menu->sleep_stage = AOS_SLEEP_WAIT_SAVE;
        return AOS_SOUL_MENU_SLEEP_SAVE_REQUESTED;
    }
    return 0;
}

static unsigned update_config(AosSoulMenu *menu, AosStats *stats,
                              uint16_t new_input, uint16_t repeated_input) {
    /* START is ignored inside the pause machine (verified against the
     * ROM: the config page does not react to it). B applies and fades
     * back to the hub through sub_08047764's state-1 path. */
    if (new_input & AOS_KEY_B) {
        if (!aos_pause_config_unique(menu)) return AOS_SOUL_MENU_REFUSED;
        for (int action = 0; action < AOS_BUTTON_ACTIONS; ++action)
            stats->buttons.action[action] =
                aos_button_config_button(menu->config_buttons[action]);
        fade_begin_hub(menu);
        return AOS_SOUL_MENU_BACK | AOS_SOUL_MENU_CONFIG_APPLIED;
    }
    if (repeated_input & AOS_KEY_UP) {
        menu->config_cursor = (uint8_t)((menu->config_cursor + AOS_BUTTON_ACTIONS - 1) %
                                        AOS_BUTTON_ACTIONS);
        return AOS_SOUL_MENU_MOVED;
    }
    if (repeated_input & AOS_KEY_DOWN) {
        menu->config_cursor = (uint8_t)((menu->config_cursor + 1) % AOS_BUTTON_ACTIONS);
        return AOS_SOUL_MENU_MOVED;
    }
    if (new_input & AOS_KEY_SELECT) {
        AosButtonConfig defaults;
        aos_button_config_default(&defaults);
        for (int action = 0; action < AOS_BUTTON_ACTIONS; ++action)
            menu->config_buttons[action] =
                aos_button_config_index(defaults.action[action]);
        return AOS_SOUL_MENU_CONFIG_EDITED;
    }
    int direction = repeated_input & (AOS_KEY_A | AOS_KEY_RIGHT) ? 1
                    : repeated_input & AOS_KEY_LEFT              ? -1
                                                                 : 0;
    if (!direction) return 0;
    uint8_t action = config_rows[menu->config_cursor];
    menu->config_buttons[action] =
        (uint8_t)((menu->config_buttons[action] + direction + 4) % 4);
    return AOS_SOUL_MENU_CONFIG_EDITED;
}

static unsigned update_enemy(AosSoulMenu *menu, const AosStats *stats,
                             uint16_t new_input, uint16_t repeated_input) {
    /* START is ignored inside the pause machine; the list's B fades
     * back to the hub through sub_08047764's state-1 path. */
    /* sub_0804D2C8: +0xC advances one intro phase per frame on the
     * list view until the steady-state footer phase. */
    if (!menu->enemy.detail && menu->enemy.intro < 2)
        ++menu->enemy.intro;
    bool was_detail = menu->enemy.detail;
    unsigned event = aos_bestiary_update(&menu->enemy, stats, new_input, repeated_input);
    if (event == AOS_BESTIARY_MOVED) return AOS_SOUL_MENU_MOVED;
    if (event == AOS_BESTIARY_ACCEPTED) return AOS_SOUL_MENU_ACCEPTED;
    if (event == AOS_BESTIARY_REFUSED) return AOS_SOUL_MENU_REFUSED;
    if (event == AOS_BESTIARY_BACK) {
        if (!was_detail) fade_begin_hub(menu);
        return AOS_SOUL_MENU_BACK;
    }
    return 0;
}

unsigned aos_pause_menu_update(AosSoulMenu *menu, AosStats *stats,
                               const AosEquipmentTables *equipment,
                               const AosItemUseRecord items[AOS_ITEM_RECORDS],
                               AosItemVitals *vitals,
                               uint16_t new_input, uint16_t repeated_input) {
    /* sub_08047764's +0x4C counter: 3 brightness steps per frame, input
     * ignored while a transition runs. The pending page/close applies
     * at the bottom of the fade-out, then the fade-in ramps back. */
    if (menu->fade_phase == AOS_MENU_FADE_OUT) {
        menu->fade = (uint8_t)(menu->fade + 3);
        if (menu->fade > 15) {
            menu->fade = 16;
            if (menu->pending_close) {
                menu->open = false;
                menu->pending_close = false;
                menu->pending_page = 0xFF;
                menu->fade_open = false;
                menu->fade_phase = AOS_MENU_FADE_NONE;
                menu->fade = 0;
                /* The native machine holds black a few frames (state 7)
                 * before restoring the room. */
                menu->room_black = 4;
                return AOS_SOUL_MENU_CLOSED;
            }
            if (menu->pending_page == 0xFE)
                menu->page = AOS_SOUL_MENU_PAGE_HUB;
            else if (menu->pending_page != 0xFF)
                enter_hub_entry(menu, stats, menu->pending_page);
            menu->pending_page = 0xFF;
            menu->fade_phase = AOS_MENU_FADE_IN;
        }
        return 0;
    }
    if (menu->fade_phase == AOS_MENU_FADE_IN) {
        if (menu->fade <= 3) {
            menu->fade = 0;
            menu->fade_phase = AOS_MENU_FADE_NONE;
            menu->fade_open = false;
        } else
            menu->fade = (uint8_t)(menu->fade - 3);
        return 0;
    }
    if (!menu->open) {
        /* The post-close black hold: no input, no page, just the veil. */
        if (menu->room_black) {
            menu->room_black--;
            return 0;
        }
        if (!(new_input & AOS_KEY_START)) return 0;
        menu->open = true;
        menu->page = AOS_SOUL_MENU_PAGE_HUB;
        if (menu->hub_cursor >= AOS_PAUSE_ENTRIES) menu->hub_cursor = AOS_PAUSE_SOUL_SET;
        menu->pending_page = 0xFF;
        menu->pending_close = false;
        /* The open fade-out covers the room, not a menu page. */
        menu->fade_open = true;
        fade_begin_out(menu);
        return AOS_SOUL_MENU_OPENED;
    }
    if (menu->page == AOS_SOUL_MENU_PAGE_HUB)
        return update_hub(menu, stats, new_input, repeated_input);
    if (menu->page == AOS_SOUL_MENU_PAGE_EQUIP)
        return update_equipment(menu, stats, equipment, new_input, repeated_input);
    if (menu->page == AOS_SOUL_MENU_PAGE_ITEM_USE)
        return update_item_use(menu, stats, items, vitals, new_input, repeated_input);
    if (menu->page == AOS_SOUL_MENU_PAGE_SLEEP)
        return update_sleep(menu, new_input, repeated_input);
    if (menu->page == AOS_SOUL_MENU_PAGE_CONFIG)
        return update_config(menu, stats, new_input, repeated_input);
    if (menu->page == AOS_SOUL_MENU_PAGE_ENEMY)
        return update_enemy(menu, stats, new_input, repeated_input);
    /* Soul Set and Ability share the bottom-page handler: START is
     * ignored and B fades back to the hub. */
    if (new_input & AOS_KEY_B) {
        fade_begin_hub(menu);
        return AOS_SOUL_MENU_BACK;
    }
    if (menu->page == AOS_SOUL_MENU_PAGE_UNAVAILABLE) return 0;
    if (menu->page == AOS_SOUL_MENU_PAGE_SOUL_SET && new_input & AOS_KEY_L)
        menu->category = (uint8_t)((menu->category + AOS_SOUL_MENU_ABILITY - 1) %
                                   AOS_SOUL_MENU_ABILITY);
    else if (menu->page == AOS_SOUL_MENU_PAGE_SOUL_SET && new_input & AOS_KEY_R)
        menu->category = (uint8_t)((menu->category + 1) % AOS_SOUL_MENU_ABILITY);
    if (new_input & (AOS_KEY_L | AOS_KEY_R)) {
        if (!available(menu, stats, menu->cursor[menu->category])) move_cursor(menu, stats, 1);
        return menu->page == AOS_SOUL_MENU_PAGE_SOUL_SET ? AOS_SOUL_MENU_MOVED : 0;
    }
    if (repeated_input & AOS_KEY_UP) {
        move_cursor(menu, stats, -1);
        return AOS_SOUL_MENU_MOVED;
    }
    if (repeated_input & AOS_KEY_DOWN) {
        move_cursor(menu, stats, 1);
        return AOS_SOUL_MENU_MOVED;
    }
    if (!(new_input & AOS_KEY_A) || !available(menu, stats, menu->cursor[menu->category])) return 0;
    if (menu->category == AOS_SOUL_MENU_ABILITY) {
        int index = menu->cursor[menu->category];
        aos_souls_set_ability(stats, index, !(stats->abilities[index >> 3] & (1u << (index & 7))));
    } else {
        uint8_t *equipped = menu->category == AOS_SOUL_MENU_RED      ? &stats->red_soul
                            : menu->category == AOS_SOUL_MENU_BLUE   ? &stats->blue_soul
                                                                      : &stats->yellow_soul;
        if (*equipped == menu->cursor[menu->category]) return 0;
        *equipped = menu->cursor[menu->category];
    }
    return AOS_SOUL_MENU_CHANGED;
}
