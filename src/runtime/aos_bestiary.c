/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_bestiary.h"

#include <stddef.h>

#include "aos_input.h"

static const char *const enemy_names[AOS_BESTIARY_NATIVE_RECORDS] = {
    [0] = "Bat", [1] = "Zombie", [2] = "Skeleton", [3] = "Merman",
    [4] = "Axe Armor", [5] = "Skull Archer", [6] = "Peeping Eye",
    [7] = "Killer Fish", [8] = "Bone Pillar", [9] = "Blue Crow", [10] = "Buer",
    [11] = "White Dragon", [12] = "Zombie Soldier", [13] = "Skeleton Knight",
    [14] = "Ghost", [15] = "Siren", [16] = "Tiny Devil", [17] = "Durga",
    [18] = "Rock Armor", [19] = "Giant Ghost", [20] = "Winged Skeleton",
    [21] = "Minotaur", [22] = "Student Witch", [23] = "Arachne", [24] = "Fleaman",
    [25] = "Evil Butcher", [26] = "Quezlcoatl", [27] = "Ectoplasm",
    [28] = "Catoblepas", [29] = "Ghost Dancer", [30] = "Waiter Skeleton",
    [31] = "Killer Doll", [32] = "Zombie Officer", [33] = "Creaking Skull",
    [34] = "Wooden Golem", [35] = "Tsuchinoko", [36] = "Persephone", [37] = "Lilith",
    [38] = "Nemesis", [39] = "Kyoma Demon", [40] = "Chronomage", [41] = "Valkyrie",
    [42] = "Witch", [43] = "Curly", [44] = "Altair", [45] = "Red Crow",
    [46] = "Cockatrice", [47] = "Dead Warrior", [48] = "Devil", [49] = "Imp",
    [50] = "Werewolf", [51] = "Gorgon", [52] = "Disc Armor", [53] = "Golem",
    [54] = "Manticore", [55] = "Gremlin", [56] = "Harpy", [57] = "Medusa Head",
    [58] = "Bomber Armor", [59] = "Lightning Doll", [60] = "Great Armor", [61] = "Une",
    [62] = "Giant Worm", [63] = "Needles", [64] = "Man Eater", [65] = "Fish Head",
    [66] = "Nightmare", [67] = "Triton", [68] = "Slime", [69] = "Big Golem",
    [70] = "Dryad", [71] = "Poison Worm", [72] = "Arc Demon", [73] = "Cagnazzo",
    [74] = "Ripper", [75] = "Werejaguar", [76] = "Ukoback", [77] = "Alura Une",
    [78] = "Biphron", [79] = "Mandragora", [80] = "Flesh Golem", [81] = "Sky Fish",
    [82] = "Dead Crusader", [83] = "Kicker Skeleton", [84] = "Weretiger",
    [85] = "Killer Mantle", [86] = "Mudman", [87] = "Gargoyle",
    [88] = "Red Minotaur", [89] = "Beam Skeleton", [90] = "Alastor",
    [91] = "Skull Millione", [92] = "Giant Skeleton", [93] = "Gladiator", [94] = "Bael",
    [95] = "Succubus", [96] = "Mimic", [97] = "Stolas", [98] = "Erinys",
    [99] = "Lubicant", [100] = "Basilisk", [101] = "Iron Golem",
    [102] = "Demon Lord", [103] = "Final Guard", [104] = "Flame Demon",
    [105] = "Shadow Knight", [106] = "Headhunter", [107] = "Death", [108] = "Legion",
    [109] = "Balore", [110] = "Julius Belmont", [111] = "Graham Jones", [112] = "Chaos",
};

static bool discovered(const AosStats *stats, uint8_t enemy) {
    return aos_stats_enemy_defeated(stats, enemy);
}

static void set_cursor(AosBestiaryMenu *menu, int cursor) {
    while (cursor < 0) cursor += AOS_BESTIARY_MENU_RECORDS;
    cursor %= AOS_BESTIARY_MENU_RECORDS;
    menu->cursor = (uint8_t)cursor;
    menu->top = (uint8_t)(cursor / AOS_BESTIARY_PAGE_RECORDS * AOS_BESTIARY_PAGE_RECORDS);
}

static bool move_discovered(AosBestiaryMenu *menu, const AosStats *stats, int direction) {
    int cursor = menu->cursor;
    for (int tried = 0; tried < AOS_BESTIARY_MENU_RECORDS; ++tried) {
        cursor += direction;
        if (cursor < 0) cursor = AOS_BESTIARY_MENU_RECORDS - 1;
        if (cursor >= AOS_BESTIARY_MENU_RECORDS) cursor = 0;
        if (discovered(stats, (uint8_t)cursor)) {
            set_cursor(menu, cursor);
            menu->soul_view = false;
            return true;
        }
    }
    return false;
}

void aos_bestiary_init(AosBestiaryMenu *menu) {
    *menu = (AosBestiaryMenu){0};
}

unsigned aos_bestiary_update(AosBestiaryMenu *menu, const AosStats *stats,
                             uint16_t new_input, uint16_t repeated_input) {
    if (menu->detail) {
        if (new_input & AOS_KEY_B) {
            menu->detail = false;
            menu->soul_view = false;
            return AOS_BESTIARY_BACK;
        }
        if (repeated_input & (AOS_KEY_RIGHT | AOS_KEY_R))
            return move_discovered(menu, stats, 1) ? AOS_BESTIARY_MOVED
                                                   : AOS_BESTIARY_REFUSED;
        if (repeated_input & (AOS_KEY_LEFT | AOS_KEY_L))
            return move_discovered(menu, stats, -1) ? AOS_BESTIARY_MOVED
                                                    : AOS_BESTIARY_REFUSED;
        if (repeated_input & (AOS_KEY_UP | AOS_KEY_DOWN)) {
            menu->soul_view = !menu->soul_view;
            return AOS_BESTIARY_MOVED;
        }
        return AOS_BESTIARY_NONE;
    }
    if (new_input & AOS_KEY_B) return AOS_BESTIARY_BACK;
    if (new_input & AOS_KEY_A) {
        if (!discovered(stats, menu->cursor)) return AOS_BESTIARY_REFUSED;
        menu->detail = true;
        menu->soul_view = false;
        return AOS_BESTIARY_ACCEPTED;
    }
    int delta = repeated_input & AOS_KEY_RIGHT ? 1
                : repeated_input & AOS_KEY_LEFT ? -1
                : repeated_input & AOS_KEY_DOWN ? 2
                : repeated_input & AOS_KEY_UP ? -2
                : repeated_input & AOS_KEY_R ? AOS_BESTIARY_PAGE_RECORDS
                : repeated_input & AOS_KEY_L ? -AOS_BESTIARY_PAGE_RECORDS
                                             : 0;
    if (!delta) return AOS_BESTIARY_NONE;
    set_cursor(menu, menu->cursor + delta);
    return AOS_BESTIARY_MOVED;
}

unsigned aos_bestiary_discovered_count(const AosStats *stats) {
    unsigned count = 0;
    for (uint8_t enemy = 0; enemy < AOS_BESTIARY_MENU_RECORDS; ++enemy)
        count += discovered(stats, enemy);
    return count;
}

const char *aos_bestiary_name(uint8_t enemy) {
    return enemy < AOS_BESTIARY_NATIVE_RECORDS ? enemy_names[enemy] : NULL;
}

bool aos_bestiary_is_boss(uint8_t enemy) {
    switch (enemy) {
    case 0x21: case 0x36: case 0x3C: case 0x45: case 0x6A: case 0x6B:
    case 0x6C: case 0x6D: case 0x6E: case 0x6F: case 0x70:
        return true;
    default:
        return false;
    }
}
