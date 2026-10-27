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

    AosSaveData source;
    aos_save_init_new(&source, 2);
    assert(source.slot == 2 && source.area == 0 && source.room == 0);
    assert(source.spawn_x == -1 && source.hp == 320 && source.moves == 0x1F);
    assert(source.name[0] == 0x14 && source.name[3] == 2 && source.character == 0);
    source.area = 7;
    source.room = 54;
    source.spawn_x = 120;
    source.spawn_y = 191;
    source.hp = 211;
    source.play_frames = 123456;
    source.name[0] = 0x0B;
    source.character = 1;
    source.challenge_flags = 0x100000;
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
           loaded.name[0] == 0x14 && loaded.character == 0);

    source.slot = AOS_SAVE_SLOT_COUNT;
    assert(!aos_save_store(path, &source, error, sizeof error));
    assert(remove(path) == 0);
    assert(remove(bad_path) == 0);
    assert(remove(legacy_path) == 0);
    puts("Aria portable save tests passed");
    return 0;
}
