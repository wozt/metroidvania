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

const char *aos_pause_entry_name(uint8_t entry) {
    return entry < AOS_PAUSE_ENTRIES ? pause_entry_names[entry] : "UNKNOWN";
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
    menu->page = AOS_SOUL_MENU_PAGE_UNAVAILABLE;
    return AOS_SOUL_MENU_ACCEPTED | AOS_SOUL_MENU_UNAVAILABLE;
}

unsigned aos_soul_menu_update(AosSoulMenu *menu, AosStats *stats,
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
