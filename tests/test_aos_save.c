/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_save.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *path = "aos-save-test.sav";
    const char *bad_path = "aos-save-test-bad.sav";
    char error[160];

    AosSaveData source;
    aos_save_init_new(&source, 2);
    assert(source.slot == 2 && source.area == 0 && source.room == 0);
    assert(source.spawn_x == -1 && source.hp == 320 && source.moves == 0x1F);
    source.area = 7;
    source.room = 54;
    source.spawn_x = 120;
    source.spawn_y = 191;
    source.hp = 211;
    source.play_frames = 123456;
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

    source.slot = AOS_SAVE_SLOT_COUNT;
    assert(!aos_save_store(path, &source, error, sizeof error));
    assert(remove(path) == 0);
    assert(remove(bad_path) == 0);
    puts("Aria portable save tests passed");
    return 0;
}
