/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_menu.h"

#include "aos_input.h"

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
    switch (record->effect) {
    case 0:
        if (vitals->hp >= vitals->max_hp) return false;
        vitals->hp = (int16_t)(vitals->hp + record->amount);
        if (vitals->hp > vitals->max_hp) vitals->hp = vitals->max_hp;
        break;
    case 1: {
        if (stats->mp >= (int16_t)stats->max_mp || !record->amount) return false;
        int divisor = 100 / record->amount;
        int recovery = divisor ? stats->max_mp / divisor : stats->max_mp;
        if (!recovery) recovery = 1;
        stats->mp = (int16_t)(stats->mp + recovery);
        if (stats->mp > (int16_t)stats->max_mp) stats->mp = (int16_t)stats->max_mp;
        break;
    }
    case 2:
        if (!(stats->status & record->amount)) return false;
        stats->status &= (uint16_t)~record->amount;
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

static unsigned update_hub(AosSoulMenu *menu, const AosStats *stats,
                           uint16_t new_input, uint16_t repeated_input) {
    if (new_input & (AOS_KEY_START | AOS_KEY_B)) {
        menu->open = false;
        return AOS_SOUL_MENU_CLOSED;
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
    if (menu->hub_cursor == AOS_PAUSE_ITEM_USE) {
        menu->page = AOS_SOUL_MENU_PAGE_ITEM_USE;
        menu->item_cursor = aos_item_choice_at(stats, 0);
        return AOS_SOUL_MENU_ACCEPTED;
    }
    if (menu->hub_cursor == AOS_PAUSE_EQUIP) {
        menu->page = AOS_SOUL_MENU_PAGE_EQUIP;
        menu->equip_slot = AOS_EQUIP_WEAPON;
        menu->equip_selecting = false;
        return AOS_SOUL_MENU_ACCEPTED;
    }
    if (menu->hub_cursor == AOS_PAUSE_SOUL_SET) {
        menu->page = AOS_SOUL_MENU_PAGE_SOUL_SET;
        menu->category = AOS_SOUL_MENU_RED;
        initialise_cursors(menu, stats);
        return AOS_SOUL_MENU_ACCEPTED;
    }
    if (menu->hub_cursor == AOS_PAUSE_ABILITY) {
        menu->page = AOS_SOUL_MENU_PAGE_ABILITY;
        menu->category = AOS_SOUL_MENU_ABILITY;
        initialise_cursors(menu, stats);
        return AOS_SOUL_MENU_ACCEPTED;
    }
    if (menu->hub_cursor == AOS_PAUSE_CONFIG) {
        AosButtonConfig buttons = stats->buttons;
        if (!aos_button_config_valid(&buttons)) aos_button_config_default(&buttons);
        menu->page = AOS_SOUL_MENU_PAGE_CONFIG;
        menu->config_cursor = 0;
        for (int action = 0; action < AOS_BUTTON_ACTIONS; ++action)
            menu->config_buttons[action] =
                aos_button_config_index(buttons.action[action]);
        return AOS_SOUL_MENU_ACCEPTED;
    }
    menu->page = AOS_SOUL_MENU_PAGE_UNAVAILABLE;
    return AOS_SOUL_MENU_ACCEPTED | AOS_SOUL_MENU_UNAVAILABLE;
}

static int equipment_ordinal(const AosStats *stats, uint8_t slot, uint8_t item) {
    int count = aos_equipment_choice_count(stats, slot);
    for (int ordinal = 0; ordinal < count; ++ordinal)
        if (aos_equipment_choice_at(stats, slot, ordinal) == item) return ordinal;
    return 0;
}

static void move_equipment_choice(AosSoulMenu *menu, const AosStats *stats, int direction) {
    int count = aos_equipment_choice_count(stats, menu->equip_slot);
    int ordinal = equipment_ordinal(stats, menu->equip_slot, menu->equip_choice);
    ordinal = (ordinal + direction + count) % count;
    menu->equip_choice = aos_equipment_choice_at(stats, menu->equip_slot, ordinal);
}

static unsigned update_equipment(AosSoulMenu *menu, AosStats *stats,
                                 const AosEquipmentTables *equipment,
                                 uint16_t new_input, uint16_t repeated_input) {
    if (new_input & AOS_KEY_START) {
        menu->open = false;
        return AOS_SOUL_MENU_CLOSED;
    }
    if (new_input & AOS_KEY_B) {
        if (menu->equip_selecting) menu->equip_selecting = false;
        else menu->page = AOS_SOUL_MENU_PAGE_HUB;
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
        menu->equip_selecting = true;
        return AOS_SOUL_MENU_ACCEPTED;
    }
    if (repeated_input & AOS_KEY_UP) {
        move_equipment_choice(menu, stats, -1);
        return AOS_SOUL_MENU_MOVED;
    }
    if (repeated_input & AOS_KEY_DOWN) {
        move_equipment_choice(menu, stats, 1);
        return AOS_SOUL_MENU_MOVED;
    }
    if (!(new_input & AOS_KEY_A)) return 0;
    uint8_t *equipped = menu->equip_slot == AOS_EQUIP_WEAPON ? &stats->weapon
                        : menu->equip_slot == AOS_EQUIP_ARMOR ? &stats->armor
                                                              : &stats->accessory;
    bool changed = *equipped != menu->equip_choice;
    *equipped = menu->equip_choice;
    if (equipment) aos_stats_refresh(stats, equipment);
    menu->equip_selecting = false;
    return AOS_SOUL_MENU_ACCEPTED | (changed ? AOS_SOUL_MENU_CHANGED : 0);
}

static void move_item_cursor(AosSoulMenu *menu, const AosStats *stats, int direction) {
    int item = menu->item_cursor < AOS_ITEM_RECORDS
                   ? menu->item_cursor
                   : (direction > 0 ? AOS_ITEM_RECORDS - 1 : 0);
    for (int tried = 0; tried < AOS_ITEM_RECORDS; ++tried) {
        item = (item + direction + AOS_ITEM_RECORDS) % AOS_ITEM_RECORDS;
        if (stats->item_counts[item]) {
            menu->item_cursor = (uint8_t)item;
            return;
        }
    }
    menu->item_cursor = AOS_EQUIPMENT_NONE;
}

static unsigned update_item_use(AosSoulMenu *menu, AosStats *stats,
                                const AosItemUseRecord items[AOS_ITEM_RECORDS],
                                AosItemVitals *vitals,
                                uint16_t new_input, uint16_t repeated_input) {
    if (new_input & AOS_KEY_START) {
        menu->open = false;
        return AOS_SOUL_MENU_CLOSED;
    }
    if (new_input & AOS_KEY_B) {
        menu->page = AOS_SOUL_MENU_PAGE_HUB;
        return AOS_SOUL_MENU_BACK;
    }
    if (repeated_input & AOS_KEY_UP) {
        move_item_cursor(menu, stats, -1);
        return AOS_SOUL_MENU_MOVED;
    }
    if (repeated_input & AOS_KEY_DOWN) {
        move_item_cursor(menu, stats, 1);
        return AOS_SOUL_MENU_MOVED;
    }
    if (!(new_input & AOS_KEY_A)) return 0;
    uint8_t used = menu->item_cursor;
    if (!aos_item_use(stats, items, vitals, used)) return AOS_SOUL_MENU_REFUSED;
    menu->last_item = used;
    if (!stats->item_counts[used]) move_item_cursor(menu, stats, 1);
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

static unsigned update_config(AosSoulMenu *menu, AosStats *stats,
                              uint16_t new_input, uint16_t repeated_input) {
    if (new_input & (AOS_KEY_START | AOS_KEY_B)) {
        if (!aos_pause_config_unique(menu)) return AOS_SOUL_MENU_REFUSED;
        for (int action = 0; action < AOS_BUTTON_ACTIONS; ++action)
            stats->buttons.action[action] =
                aos_button_config_button(menu->config_buttons[action]);
        if (new_input & AOS_KEY_START) {
            menu->open = false;
            return AOS_SOUL_MENU_CLOSED | AOS_SOUL_MENU_CONFIG_APPLIED;
        }
        menu->page = AOS_SOUL_MENU_PAGE_HUB;
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

unsigned aos_pause_menu_update(AosSoulMenu *menu, AosStats *stats,
                               const AosEquipmentTables *equipment,
                               const AosItemUseRecord items[AOS_ITEM_RECORDS],
                               AosItemVitals *vitals,
                               uint16_t new_input, uint16_t repeated_input) {
    if (!menu->open) {
        if (!(new_input & AOS_KEY_START)) return 0;
        menu->open = true;
        menu->page = AOS_SOUL_MENU_PAGE_HUB;
        if (menu->hub_cursor >= AOS_PAUSE_ENTRIES) menu->hub_cursor = AOS_PAUSE_SOUL_SET;
        return AOS_SOUL_MENU_OPENED;
    }
    if (menu->page == AOS_SOUL_MENU_PAGE_HUB)
        return update_hub(menu, stats, new_input, repeated_input);
    if (menu->page == AOS_SOUL_MENU_PAGE_EQUIP)
        return update_equipment(menu, stats, equipment, new_input, repeated_input);
    if (menu->page == AOS_SOUL_MENU_PAGE_ITEM_USE)
        return update_item_use(menu, stats, items, vitals, new_input, repeated_input);
    if (menu->page == AOS_SOUL_MENU_PAGE_CONFIG)
        return update_config(menu, stats, new_input, repeated_input);
    if (new_input & AOS_KEY_START) {
        menu->open = false;
        return AOS_SOUL_MENU_CLOSED;
    }
    if (new_input & AOS_KEY_B) {
        menu->page = AOS_SOUL_MENU_PAGE_HUB;
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
