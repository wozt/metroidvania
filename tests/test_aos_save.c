/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_save.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t test_checksum(const char *text, size_t size) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < size; ++i) {
        hash ^= (uint8_t)text[i];
        hash *= 16777619u;
    }
    return hash;
}

int main(void) {
    const char *path = "aos-save-test.sav";
    const char *bad_path = "aos-save-test-bad.sav";
    const char *legacy_path = "aos-save-test-v1.sav";
    char error[160];
    char quick_path[64];
    assert(aos_save_quick_path(quick_path, sizeof quick_path, "aria-slot-1.sav") &&
           !strcmp(quick_path, "aria-slot-1.quick.sav"));
    assert(aos_save_quick_path(quick_path, sizeof quick_path, "portable") &&
           !strcmp(quick_path, "portable.quick"));
    assert(!aos_save_quick_path(quick_path, 8, "aria-slot-1.sav"));

    AosSaveData source;
    aos_save_init_new(&source, 2);
    assert(source.slot == 2 && source.area == 0 && source.room == 0);
    assert(source.spawn_x == 0x78 && source.spawn_y == 0x8D &&
           source.hp == 320 && source.moves == 0x1F);
    assert(source.name[0] == 0x14 && source.name[3] == 2 && source.character == 0);
    assert(source.buttons[0] == 1 && source.buttons[1] == 0 &&
           source.buttons[2] == 3 && source.buttons[3] == 2);
    source.area = 7;
    source.room = 54;
    source.spawn_x = 120;
    source.spawn_y = 191;
    source.hp = 211;
    source.play_frames = 123456;
    source.name[0] = 0x0B;
    source.character = 1;
    source.challenge_flags = 0x100000;
    source.experience = 123456;
    source.gold = 7890;
    assert(source.weapon == 0 && source.armor == 0 && source.accessory == 0xFF);
    source.weapon = 12;
    source.accessory = 0x28;
    /* sub_08014548's new-game inventory, then a few more counts. */
    assert(source.inventory[AOS_SAVE_WEAPONS] == 1 && source.inventory[AOS_SAVE_ARMOR] == 1 &&
           source.inventory[0] == 0);
    source.inventory[0] = 9;
    source.inventory[AOS_SAVE_WEAPONS + 12] = 3;
    source.inventory[AOS_SAVE_INVENTORY - 1] = 1;
    source.souls[0] = 0x21;
    source.souls[AOS_SAVE_SOUL_BYTES - 1] = 0xFF;
    source.souls_collected = 42;
    source.red_soul = 2;
    source.yellow_soul = 18;
    source.mp = 37;
    source.buttons[0] = 2;
    source.buttons[1] = 0;
    source.buttons[2] = 3;
    source.buttons[3] = 1;
    source.defeated[0] = (1u << 0) | (1u << 31);
    source.defeated[3] = 1u << (111 - 96);
    source.dropped[0][1] = 1u << (33 - 32);
    source.dropped[1][2] = 1u << (65 - 64);
    assert(aos_save_store(path, &source, error, sizeof error));

    AosSaveData loaded = {0};
    assert(aos_save_load(path, &loaded, error, sizeof error));
    assert(memcmp(&source, &loaded, sizeof source) == 0);

    FILE *bad = fopen(bad_path, "wb");
    assert(bad);
    assert(fputs(AOS_SAVE_SCHEMA "\nslot=0\nchecksum=00000000\n", bad) >= 0);
    assert(fclose(bad) == 0);
    assert(!aos_save_load(bad_path, &loaded, error, sizeof error));
    assert(strstr(error, "invalid"));

    char legacy[512];
    int legacy_size = snprintf(legacy, sizeof legacy,
                               AOS_SAVE_SCHEMA_V1 "\n"
                               "slot=1\narea=2\nroom=3\nspawn_x=-1\nspawn_y=-1\n"
                               "hp=120\nmax_hp=320\nmoves=7\nplay_frames=99\n");
    assert(legacy_size > 0 && (size_t)legacy_size < sizeof legacy);
    int checksum_size = snprintf(legacy + legacy_size,
                                 sizeof legacy - (size_t)legacy_size,
                                 "checksum=%08x\n",
                                 test_checksum(legacy, (size_t)legacy_size));
    assert(checksum_size > 0);
    FILE *legacy_file = fopen(legacy_path, "wb");
    assert(legacy_file);
    assert(fwrite(legacy, 1, (size_t)(legacy_size + checksum_size), legacy_file) ==
           (size_t)(legacy_size + checksum_size));
    assert(fclose(legacy_file) == 0);
    assert(aos_save_load(legacy_path, &loaded, error, sizeof error));
    assert(loaded.slot == 1 && loaded.area == 2 && loaded.room == 3 &&
           loaded.name[0] == 0x14 && loaded.character == 0 &&
           loaded.buttons[0] == 1 && loaded.buttons[3] == 2);

    /* A v2 file (no experience or gold) still loads, with both at zero. */
    const char *v2_path = "aos-save-test-v2.sav";
    char v2[512];
    int v2_size = snprintf(v2, sizeof v2,
                           AOS_SAVE_SCHEMA_V2 "\n"
                           "slot=0\narea=4\nroom=9\nspawn_x=-1\nspawn_y=-1\nhp=300\n"
                           "max_hp=332\nmoves=31\nplay_frames=60\n"
                           "name=1,2,3,0,0,0,0,0\ncharacter=0\nchallenge_flags=0\n");
    assert(v2_size > 0 && (size_t)v2_size < sizeof v2);
    v2_size += snprintf(v2 + v2_size, sizeof v2 - (size_t)v2_size, "checksum=%08x\n",
                        test_checksum(v2, (size_t)v2_size));
    FILE *v2_file = fopen(v2_path, "wb");
    assert(v2_file && fwrite(v2, 1, (size_t)v2_size, v2_file) == (size_t)v2_size);
    assert(fclose(v2_file) == 0);
    loaded.experience = loaded.gold = 5;
    assert(aos_save_load(v2_path, &loaded, error, sizeof error));
    assert(loaded.area == 4 && loaded.max_hp == 332 && loaded.name[1] == 2 &&
           loaded.experience == 0 && loaded.gold == 0 && loaded.weapon == 0 &&
           loaded.armor == 0 && loaded.accessory == 0xFF &&
           loaded.inventory[AOS_SAVE_WEAPONS] == 1 && loaded.inventory[AOS_SAVE_ARMOR] == 1 &&
           loaded.inventory[0] == 0);
    assert(remove(v2_path) == 0);

    /* Experience stops at 99999999 (the native cap of sub_080683BC); gold
     * uses the same bound in this format. */
    source.experience = 100000000u;
    assert(!aos_save_store(path, &source, error, sizeof error));
    source.experience = 0;
    source.gold = 100000000u;
    assert(!aos_save_store(path, &source, error, sizeof error));
    source.gold = 0;

    /* Equipment indices stay within their tables (59 weapons, 45 armor). */
    source.weapon = 59;
    assert(!aos_save_store(path, &source, error, sizeof error));
    source.weapon = 0xFF;
    source.armor = 44;
    assert(aos_save_store(path, &source, error, sizeof error));
    source.armor = 45;
    assert(!aos_save_store(path, &source, error, sizeof error));
    source.armor = 0;
    /* A v5 file (no souls) loads with none. */
    {
        const char *v5_path = "aos-save-test-v5.sav";
        char v5[1024], digits[AOS_SAVE_INVENTORY + 1];
        memset(digits, '0', AOS_SAVE_INVENTORY);
        digits[AOS_SAVE_INVENTORY] = '\0';
        digits[AOS_SAVE_WEAPONS] = '2';
        int v5_size = snprintf(v5, sizeof v5,
                               AOS_SAVE_SCHEMA_V5 "\n"
                               "slot=1\narea=0\nroom=0\nspawn_x=-1\nspawn_y=-1\nhp=320\n"
                               "max_hp=320\nmoves=31\nplay_frames=0\n"
                               "name=0,0,0,0,0,0,0,0\ncharacter=0\nchallenge_flags=0\n"
                               "experience=5\ngold=6\nweapon=0\narmor=0\naccessory=255\n"
                               "inventory=%s\n", digits);
        v5_size += snprintf(v5 + v5_size, sizeof v5 - (size_t)v5_size, "checksum=%08x\n",
                            test_checksum(v5, (size_t)v5_size));
        FILE *v5_file = fopen(v5_path, "wb");
        assert(v5_file && fwrite(v5, 1, (size_t)v5_size, v5_file) == (size_t)v5_size);
        assert(fclose(v5_file) == 0);
        AosSaveData old = {0};
        old.souls[3] = 7;
        assert(aos_save_load(v5_path, &old, error, sizeof error));
        assert(old.gold == 6 && old.inventory[AOS_SAVE_WEAPONS] == 2 && old.souls[3] == 0 &&
               old.souls_collected == 0 && old.buttons[0] == 1 && old.buttons[3] == 2);
        assert(remove(v5_path) == 0);
    }
    /* A v7 file predating configurable-button persistence loads the native
     * default B/A/L/R assignment. */
    {
        const char *v7_path = "aos-save-test-v7.sav";
        char v7[1280], digits[AOS_SAVE_INVENTORY + 1], soul_hex[2 * AOS_SAVE_SOUL_BYTES + 1];
        memset(digits, '0', AOS_SAVE_INVENTORY);
        digits[AOS_SAVE_INVENTORY] = '\0';
        digits[AOS_SAVE_WEAPONS] = digits[AOS_SAVE_ARMOR] = '1';
        memset(soul_hex, '0', 2 * AOS_SAVE_SOUL_BYTES);
        soul_hex[2 * AOS_SAVE_SOUL_BYTES] = '\0';
        int v7_size = snprintf(v7, sizeof v7,
                               AOS_SAVE_SCHEMA_V7 "\n"
                               "slot=0\narea=0\nroom=0\nspawn_x=120\nspawn_y=141\n"
                               "hp=320\nmax_hp=320\nmoves=31\nplay_frames=0\n"
                               "name=20,16,14,2,0,0,0,0\ncharacter=0\nchallenge_flags=0\n"
                               "experience=0\ngold=0\nweapon=0\narmor=0\naccessory=255\n"
                               "inventory=%s\nsouls=%s\nsouls_collected=0\n"
                               "souls_equipped=0,0,0\nmp=-1\n",
                               digits, soul_hex);
        v7_size += snprintf(v7 + v7_size, sizeof v7 - (size_t)v7_size, "checksum=%08x\n",
                            test_checksum(v7, (size_t)v7_size));
        FILE *v7_file = fopen(v7_path, "wb");
        assert(v7_file && fwrite(v7, 1, (size_t)v7_size, v7_file) == (size_t)v7_size);
        assert(fclose(v7_file) == 0);
        AosSaveData old = {0};
        assert(aos_save_load(v7_path, &old, error, sizeof error));
        assert(old.buttons[0] == 1 && old.buttons[1] == 0 &&
               old.buttons[2] == 3 && old.buttons[3] == 2);
        assert(remove(v7_path) == 0);
    }
    /* A v8 file predating bestiary persistence retains its buttons and starts
     * with empty defeated/common/rare bitsets. */
    {
        const char *v8_path = "aos-save-test-v8.sav";
        char v8[1280], digits[AOS_SAVE_INVENTORY + 1], soul_hex[2 * AOS_SAVE_SOUL_BYTES + 1];
        memset(digits, '0', AOS_SAVE_INVENTORY);
        digits[AOS_SAVE_INVENTORY] = '\0';
        digits[AOS_SAVE_WEAPONS] = digits[AOS_SAVE_ARMOR] = '1';
        memset(soul_hex, '0', 2 * AOS_SAVE_SOUL_BYTES);
        soul_hex[2 * AOS_SAVE_SOUL_BYTES] = '\0';
        int v8_size = snprintf(v8, sizeof v8,
                               AOS_SAVE_SCHEMA_V8 "\n"
                               "slot=0\narea=0\nroom=0\nspawn_x=120\nspawn_y=141\n"
                               "hp=320\nmax_hp=320\nmoves=31\nplay_frames=0\n"
                               "name=20,16,14,2,0,0,0,0\ncharacter=0\nchallenge_flags=0\n"
                               "experience=0\ngold=0\nweapon=0\narmor=0\naccessory=255\n"
                               "inventory=%s\nsouls=%s\nsouls_collected=0\n"
                               "souls_equipped=0,0,0\nmp=-1\nbuttons=2,0,3,1\n",
                               digits, soul_hex);
        v8_size += snprintf(v8 + v8_size, sizeof v8 - (size_t)v8_size, "checksum=%08x\n",
                            test_checksum(v8, (size_t)v8_size));
        FILE *v8_file = fopen(v8_path, "wb");
        assert(v8_file && fwrite(v8, 1, (size_t)v8_size, v8_file) == (size_t)v8_size);
        assert(fclose(v8_file) == 0);
        AosSaveData old = {0};
        assert(aos_save_load(v8_path, &old, error, sizeof error));
        assert(old.buttons[0] == 2 && old.buttons[3] == 1 && old.defeated[0] == 0 &&
               old.defeated[3] == 0 && old.dropped[0][1] == 0 && old.dropped[1][2] == 0);
        assert(remove(v8_path) == 0);
    }
    /* The soul total stops at 999. */
    source.souls_collected = 1000;
    assert(!aos_save_store(path, &source, error, sizeof error));
    source.souls_collected = 42;

    /* Counts stop at 9. */
    source.inventory[4] = 10;
    assert(!aos_save_store(path, &source, error, sizeof error));
    source.inventory[4] = 0;

    source.buttons[0] = source.buttons[1];
    assert(!aos_save_store(path, &source, error, sizeof error));
    source.buttons[0] = 2;

    source.slot = AOS_SAVE_SLOT_COUNT;
    assert(!aos_save_store(path, &source, error, sizeof error));
    assert(remove(path) == 0);
    assert(remove(bad_path) == 0);
    assert(remove(legacy_path) == 0);
    puts("Aria portable save tests passed");
    return 0;
}
