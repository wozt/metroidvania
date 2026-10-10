/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free tests for the Aria of Sorrow room exits. */
#include "aos_room.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    /* sub_08011A44: one-screen rooms are 0xF0 wide and 0x30..0xD0 tall;
     * larger rooms span W * 256 and 0x30..H * 256 - 0x30. */
    assert(!aos_room_outside(NULL, 1, 1, 0xF0, 0xD0));
    assert(aos_room_outside(NULL, 1, 1, 0xF1, 0x80));
    assert(aos_room_outside(NULL, 1, 1, -1, 0x80));
    assert(aos_room_outside(NULL, 1, 1, 0x80, 0x2F));
    assert(aos_room_outside(NULL, 1, 1, 0x80, 0xD1));
    assert(!aos_room_outside(NULL, 3, 2, 768, 464));
    assert(aos_room_outside(NULL, 3, 2, 769, 100));
    assert(aos_room_outside(NULL, 3, 2, 100, 465));
    /* A BG1 byte 0xF0 under the feet is an exit inside the room. */
    static uint8_t cells[32 * 32];
    AosCollision layer = {1, 1, 32, 32, cells, NULL, false};
    cells[10 * 32 + 5] = AOS_EXIT_CELL;
    assert(aos_room_outside(&layer, 1, 1, 44, 84));
    assert(!aos_room_outside(&layer, 1, 1, 60, 84));

    /* sub_08010350: the entry matches the screen column and row left. */
    const AosTransition list[] = {
        {-1, 0, 0, 1040, 0, 0, 3},      /* 0/5 left, top screen -> 0/3 */
        {2, 0, 0, 0, 0, 0, 6},          /* 0/5 right, top screen -> 0/6 */
        {0, 2, 0, 0, 0, 5, 1},          /* bottom of a 3x2 room */
        {1, -1, 0, 0, 768, 0, 2},       /* top of a 5x1 room, second screen */
        {1, 0, -16, 256, 0, 5, 7},      /* right of a 1x1 room */
    };
    assert(aos_room_find_exit(list, 5, 2, 2, -2, 200) == &list[0]);
    assert(aos_room_find_exit(list, 5, 2, 2, 513, 200) == &list[1]);
    assert(aos_room_find_exit(list, 5, 3, 2, 100, 470) == &list[2]);
    assert(aos_room_find_exit(list, 5, 5, 1, 300, 0x2F) == &list[3]);
    assert(aos_room_find_exit(list, 5, 1, 1, 0xF1, 0x80) == &list[4]);
    assert(aos_room_find_exit(list, 5, 2, 2, 513, 300) == NULL);

    /* Arrival: X = load_x + local + adjustment, Y = load_y + 0x30 + local;
     * one-screen axes use X = 0 and Y = 0x30. */
    int32_t x, y;
    aos_room_arrival(&list[0], 5, 1, -1, 200, &x, &y);
    assert(x == 1040 + 239 && y == 0x30 + 152);
    aos_room_arrival(&list[1], 3, 1, 513, 200, &x, &y);
    assert(x == 1 && y == 200);
    /* Falling out of a 3x2 room enters a 3x2 room just below its top. */
    aos_room_arrival(&list[2], 3, 2, 100, 465, &x, &y);
    assert(x == 100 && y == 0x30 + 1);
    /* Rising out of a one-screen room enters near the bottom of the screen. */
    aos_room_arrival(&list[3], 2, 4, 300, 0x2F, &x, &y);
    assert(x == 300 - 256 && y == 768 + 0x30 + 0x2F + 0x70);
    assert(!aos_room_outside(NULL, 2, 4, x, y));
    aos_room_arrival(&list[4], 3, 1, 0xF2, 0x80, &x, &y);
    assert(x == 256 + 2 - 16 && y == 0x80);

    /* sub_08010244: motion back into the room stops, fast rises slow. */
    AosSoma soma = {.vx = 0x18000, .vy = -0x60000, .friction = 0x4000};
    aos_room_exit_velocity(&soma, 10, 100);
    assert(soma.vx == 0 && soma.friction == 0 && soma.vy == -0x10000);
    soma = (AosSoma){.vx = 0x18000, .vy = -0x60000};
    aos_room_exit_velocity(&soma, 250, 100);
    assert(soma.vx == 0x18000 && soma.vy == -0x10000);
    soma = (AosSoma){.vx = -0x18000, .vy = -0x60000};
    aos_room_exit_velocity(&soma, 100, 10);
    assert(soma.vx == -0x18000 && soma.vy == -0x60000);
    puts("aos_room: ok");
    return 0;
}
