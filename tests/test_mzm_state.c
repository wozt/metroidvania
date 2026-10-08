#include "mzm/state.h"

#include <assert.h>
#include <stdio.h>

static void write_u16(uint8_t *bytes, size_t offset, uint16_t value)
{
    bytes[offset] = (uint8_t)value;
    bytes[offset + 1] = (uint8_t)(value >> 8);
}

int main(void)
{
    MzmStateBytes bytes = {0};
    MzmStateView view;

    assert(!mzm_state_decode(NULL, &view));
    assert(!mzm_state_decode(&bytes, NULL));
    bytes.difficulty = 1;
    bytes.location[0] = 2;
    bytes.location[1] = 7;
    bytes.location[2] = 3;
    write_u16(bytes.mode, 0, 4);
    write_u16(bytes.mode, 2, 2);
    bytes.samus[0] = 8;
    bytes.samus[1] = 2;
    bytes.samus[2] = 1;
    write_u16(bytes.samus, 14, 0x20);
    write_u16(bytes.samus, 18, 1882);
    write_u16(bytes.samus, 20, 703);
    write_u16(bytes.samus, 22, (uint16_t)-12);
    write_u16(bytes.samus, 24, 24);
    write_u16(bytes.equipment, 0, 399);
    write_u16(bytes.equipment, 2, 50);
    bytes.equipment[4] = 10;
    bytes.equipment[5] = 4;
    write_u16(bytes.equipment, 6, 299);
    write_u16(bytes.equipment, 8, 25);
    bytes.equipment[10] = 3;
    bytes.equipment[11] = 2;
    bytes.equipment[12] = 0x91;
    bytes.equipment[14] = 0x43;

    assert(mzm_state_decode(&bytes, &view));
    assert(view.gameplay_active && view.values_plausible);
    assert(view.gameplay_state_ready);
    assert(view.game_mode == 4 && view.sub_game_mode == 2);
    assert(view.difficulty == 1 && view.area == 2 && view.room == 7);
    assert(view.last_door == 3 && view.pose == 8);
    assert(view.direction == 0x20 && view.x_subpixels == 1882);
    assert(view.y_subpixels == 703 && view.x_velocity == -12);
    assert(view.y_velocity == 24);
    assert(view.current_energy == 299 && view.max_energy == 399);
    assert(view.current_missiles == 25 && view.max_missiles == 50);
    assert(view.current_super_missiles == 3 && view.max_super_missiles == 10);
    assert(view.current_power_bombs == 2 && view.max_power_bombs == 4);
    assert(view.beam_bomb_flags == 0x91 && view.suit_misc_flags == 0x43);

    write_u16(bytes.mode, 2, 0);
    assert(mzm_state_decode(&bytes, &view));
    assert(view.gameplay_active && view.values_plausible);
    assert(!view.gameplay_state_ready);
    write_u16(bytes.mode, 2, 2);
    write_u16(bytes.samus, 18, 0);
    write_u16(bytes.samus, 20, 0);
    assert(mzm_state_decode(&bytes, &view));
    assert(!view.gameplay_state_ready);
    write_u16(bytes.samus, 18, 1882);
    write_u16(bytes.samus, 20, 703);

    write_u16(bytes.equipment, 6, 400);
    assert(mzm_state_decode(&bytes, &view));
    assert(view.gameplay_active && !view.values_plausible);
    assert(!view.gameplay_state_ready);
    write_u16(bytes.mode, 0, 12);
    assert(mzm_state_decode(&bytes, &view));
    assert(!view.gameplay_active && view.values_plausible);
    assert(!view.gameplay_state_ready);

    puts("MZM state decoder tests passed.");
    return 0;
}
