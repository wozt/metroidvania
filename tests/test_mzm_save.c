/* SPDX-License-Identifier: GPL-3.0-only */
/* The runtime's save file (mzm_save): a full round trip, through a file,
 * and the rejection of truncated, corrupted or foreign data. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mzm_save.h"

static MzmSaveData data, back;

int main(void) {
    memset(&data, 0, sizeof data);
    strcpy(data.room, "brinstar_032");
    data.samus_x = 1234;
    data.samus_y = -5;
    data.facing = -1;
    data.equipment.suit = MZM_SUIT_FULLY_POWERED;
    data.equipment.items = MZM_ITEM_MORPH_BALL | MZM_ITEM_LONG_BEAM;
    data.equipment.unknown_items = MZM_ITEM_PLASMA_BEAM;
    data.equipment.energy = 199;
    data.equipment.max_energy = 299;
    data.equipment.missiles = 7;
    data.equipment.max_missiles = 15;
    data.equipment.power_bombs = 2;
    data.equipment.max_power_bombs = 4;
    data.events[3] = 0x81;
    data.minimap.visited[0][5] = 0xDEADBEEF;
    data.minimap.obtained[7][31] = 1;
    data.minimap.downloaded_maps = 0x5;
    data.items.count[2] = 1;
    data.items.items[2][0].room = 9;
    data.items.items[2][0].type = 3;
    data.items.items[2][0].x = 12;
    data.items.items[2][0].y = 34;
    data.hatches_opened[9][7] = 0x80000001u;
    data.hatch_destination_count = 1;
    strcpy(data.hatch_destinations[0].alias, "kraid_001");
    data.hatch_destinations[0].x = 3;
    data.hatch_destinations[0].y = 4;
    data.never_reform_count = 2;
    strcpy(data.never_reform[1].alias, "norfair_010");
    data.never_reform[1].x = -1;
    data.escaped_zebes = true;
    data.area = 5;
    data.difficulty = 2;
    data.igt.hours = 1;
    data.igt.minutes = 23;
    data.igt.seconds = 45;
    data.igt.frames = 59;
    data.completed_game = 0x12;

    size_t size = 0;
    uint8_t *bytes = mzm_save_encode(&data, &size);
    assert(bytes && size > 16 && !memcmp(bytes, "MVZMSAVE", 8));
    assert(mzm_save_decode(bytes, size, &back));
    assert(back.samus_x == 1234 && back.samus_y == -5);
    assert(!strcmp(back.room, "brinstar_032") && back.facing == -1);
    assert(back.equipment.suit == MZM_SUIT_FULLY_POWERED && back.equipment.items == data.equipment.items &&
           back.equipment.unknown_items == MZM_ITEM_PLASMA_BEAM && back.equipment.max_energy == 299 &&
           back.equipment.power_bombs == 2);
    assert(back.events[3] == 0x81 && back.minimap.visited[0][5] == 0xDEADBEEF &&
           back.minimap.obtained[7][31] == 1 && back.minimap.downloaded_maps == 5);
    assert(back.items.count[2] == 1 && back.items.items[2][0].y == 34);
    assert(back.hatches_opened[9][7] == 0x80000001u && back.hatch_destination_count == 1 &&
           !strcmp(back.hatch_destinations[0].alias, "kraid_001") && back.hatch_destinations[0].y == 4);
    assert(back.never_reform_count == 2 && back.never_reform[1].x == -1 && back.escaped_zebes);
    assert(back.area == 5 && back.difficulty == 2 && back.igt.hours == 1 && back.igt.minutes == 23 &&
           back.igt.seconds == 45 && back.igt.frames == 59 && back.completed_game == 0x12);
    /* A changed byte, a truncated file, a newer version, a bad count. */
    bytes[100] ^= 1;
    assert(!mzm_save_decode(bytes, size, &back));
    bytes[100] ^= 1;
    assert(!mzm_save_decode(bytes, size - 1, &back));
    bytes[8] = 3;
    assert(!mzm_save_decode(bytes, size, &back));
    bytes[8] = 2;
    assert(mzm_save_decode(bytes, size, &back));
    free(bytes);
    data.never_reform_count = MZM_SAVE_NEVER_REFORM + 1;
    bytes = mzm_save_encode(&data, &size);
    assert(!mzm_save_decode(bytes, size, &back));
    free(bytes);
    data.never_reform_count = 2;
    /* Through a file. */
    const char *path = "test_mzm_save.sav";
    assert(mzm_save_write(path, &data));
    memset(&back, 0, sizeof back);
    assert(mzm_save_read(path, &back) && back.equipment.max_missiles == 15 && back.escaped_zebes);
    remove(path);
    assert(!mzm_save_read(path, &back));
    printf("mzm_save: ok\n");
    return 0;
}
