#include "../src/runtime/aos/game/aos_object25.h"
#include <assert.h>
#include <stdio.h>

static uint32_t rng_state;
static uint32_t test_random(void) {
    return rng_state++;
}

int main(void) {
    AosObject25 o;
    bool deleted;

    /* Create gate: (flags4cc & 3) == 0 with + 0x37E bit 0x20
     * deletes; either condition clearing proceeds. */
    aos_object25_create(&o, 0, 0, 0, 0, 0, 0x20, &deleted);
    assert(deleted && o.deleted);
    aos_object25_create(&o, 0, 0, 0, 0, 1, 0x20, &deleted);
    assert(!deleted);
    aos_object25_create(&o, 0, 0, 0, 0, 0, 0, &deleted);
    assert(!deleted);

    /* param1 == 1: the invisible spawner node. */
    aos_object25_create(&o, 2, 1, 64 << 16, 160 << 16, 0, 0, &deleted);
    assert(!deleted);
    assert(o.state == 1 && o.timer == 0x78 && o.flags58 == 0x20);
    assert(o.hp == 0 && o.substate == 0);

    /* Normal create: health, kind, anim table 0, state 0. */
    aos_object25_create(&o, 0, 0, 64 << 16, 160 << 16, 0, 0, &deleted);
    assert(!deleted);
    assert(o.state == 0 && o.substate == 0 && o.hp == 0xC8);
    assert(o.kind == 0x14 && o.anim_table == 0 && o.anim_seq == 0);

    /* param1 == 2: immediate substate 1 with its own anim and
     * velocities. */
    aos_object25_create(&o, 2, 2, 0, 0, 0, 0, &deleted);
    assert(o.substate == 1 && o.anim_table == 1);
    assert(o.vel_x == 0x4000 && o.vel_y == 0x80000 && o.kind == 0x13);

    /* Substate 0 waits for the active mark. */
    aos_object25_create(&o, 0, 0, 0, 0, 0, 0, &deleted);
    rng_state = 0;
    aos_object25_update(&o, false, NULL, 0, 0, test_random);
    assert(o.substate == 0);
    aos_object25_update(&o, true, NULL, 0, 0, test_random);
    assert(o.substate == 1 && o.anim_seq == 1);
    assert(o.vel_y == 0x80000);

    /* Substate 1: + 0x48 drift once, then + 0x4C fall; the
     * velocity bytes clear after each step. */
    int32_t px = o.pos_x, py = o.pos_y;
    aos_object25_update(&o, true, NULL, 0, 0, test_random);
    assert(o.pos_x == px + 0x4000 && o.vel_x == 0);
    assert(o.pos_y == py + 0x80000 && o.vel_y == 0x80000);

    /* param0 == 2 physics gate: the + 0x48 drift lands first,
     * then pos_x + room_ox <= 0xD00000 decides the y step. */
    aos_object25_create(&o, 2, 0, 0x00CFFF00, 0, 0, 0, &deleted);
    o.substate = 1;
    px = o.pos_x; py = o.pos_y;
    aos_object25_update(&o, true, NULL, 0, 0, test_random);
    assert(o.pos_x == px + 0x4000);
    assert(o.pos_y == py);              /* 0xD03300 > gate: off */
    o.pos_x = 0x00CFC000;               /* + 0x4000 == the bound */
    aos_object25_update(&o, true, NULL, 0, 0, test_random);
    assert(o.pos_y == py + 0x80000);    /* boundary: still moves */

    /* Bounds: param0 0/1 -> 0x040FFFFF, param0 2/3 ->
     * 0x00FFFFFF, outside 0-3 no check. */
    aos_object25_create(&o, 1, 0, 0x040FFC00, 0, 0, 0, &deleted);
    o.substate = 1;
    aos_object25_update(&o, true, NULL, 0, 0, test_random);
    assert(o.substate == 10 && o.timer == 1);
    aos_object25_create(&o, 3, 0, 0x00FFFC00, 0, 0, 0, &deleted);
    o.substate = 1;
    aos_object25_update(&o, true, NULL, 0, 0, test_random);
    assert(o.substate == 10);
    aos_object25_create(&o, 4, 0, 0x40000000, 0, 0, 0, &deleted);
    o.substate = 1;
    aos_object25_update(&o, true, NULL, 0, 0, test_random);
    assert(o.substate == 1);

    /* Substate 10: one-frame finish when entered with timer 1
     * (the bound-exit path), particles on timer & 3 == 0. */
    aos_object25_create(&o, 0, 0, 0, 0, 0, 0, &deleted);
    o.substate = 10; o.timer = 4;
    aos_object25_update(&o, true, NULL, 0, 0, test_random);
    assert(o.particle_fx == 1 && o.timer == 3);
    aos_object25_update(&o, true, NULL, 0, 0, test_random);
    assert(o.particle_fx == 0 && o.timer == 2);
    o.timer = 1;
    aos_object25_update(&o, true, NULL, 0, 0, test_random);
    assert(o.finished && (o.flags59 & 8));
    /* Finished entities stop updating. */
    o.particle_fx = 0;
    aos_object25_update(&o, true, NULL, 0, 0, test_random);
    assert(o.particle_fx == 0);

    /* Death dissolve: timer 0x28 counts down ~40 frames with
     * particles every fourth. */
    aos_object25_create(&o, 0, 0, 0, 0, 0, 0, &deleted);
    aos_object25_hurt(&o, 0xC8);
    assert(o.substate == 10 && o.timer == 0x28);
    assert(o.anim_seq == 2 && o.anim_table == 2);
    assert(o.hp == 0 && o.damage == 0xC8);
    int frames = 0;
    while (!o.finished && frames < 100) {
        aos_object25_update(&o, true, NULL, 0, 0, test_random);
        frames++;
    }
    assert(o.finished && frames == 0x28);

    /* Hurt: non-lethal damage pops the number and arms the
     * 8-frame invulnerability; a second hit inside the window
     * is ignored. */
    aos_object25_create(&o, 0, 0, 0, 0, 0, 0, &deleted);
    aos_object25_hurt(&o, 10);
    assert(o.hp == 0xBE && o.blink == 8 && o.damage == 10);
    assert(o.substate == 0);
    aos_object25_hurt(&o, 10);
    assert(o.hp == 0xBE);           /* blink window blocks */
    o.blink = 0;
    aos_object25_hurt(&o, 0xBE);
    assert(o.substate == 10 && o.hp == 0);

    /* State-1 spawner: the 0x78-seeded timer rolls the gate on
     * the first update; an even roll with room spawns one
     * child on the variant's table. */
    aos_object25_create(&o, 0, 1, 0, 0, 0, 0, &deleted);
    rng_state = 0;                  /* gate roll: 0 & 1 == 0 */
    aos_object25_update(&o, true, NULL, 0, 0, test_random);
    assert(o.spawn_pending && o.children == 1);
    assert(o.spawn_param0 == 0);
    /* pick roll: rng_state now 1 -> 1 % 6 == 1 -> entry 1. */
    assert(o.spawn_x == (320 << 16) && o.spawn_y == (176 << 16));
    assert(o.timer == 0);

    /* Room origin subtracts from the spawn position. */
    aos_object25_create(&o, 0, 1, 0, 0, 0, 0, &deleted);
    o.timer = 0x78;
    rng_state = 0;
    aos_object25_update(&o, true, NULL, 0x10000, 0x20000,
                        test_random);
    assert(o.spawn_x == ((320 << 16) - 0x10000));
    assert(o.spawn_y == ((176 << 16) - 0x20000));

    /* Odd gate roll: no spawn, timer still resets. */
    aos_object25_create(&o, 0, 1, 0, 0, 0, 0, &deleted);
    rng_state = 1;                  /* 1 & 1 != 0 */
    aos_object25_update(&o, true, NULL, 0, 0, test_random);
    assert(!o.spawn_pending && o.children == 0 && o.timer == 0);

    /* Ten live children cap the spawner. */
    o.children = 10;
    rng_state = 0;
    o.timer = 0x78;
    aos_object25_update(&o, true, NULL, 0, 0, test_random);
    assert(!o.spawn_pending && o.children == 10);

    /* param0 == 2 table: entries (-16, 160) / (120, 168). */
    aos_object25_create(&o, 2, 1, 0, 0, 0, 0, &deleted);
    o.timer = 0x78;
    rng_state = 0;                  /* pick: 1 % 2 == 1 */
    aos_object25_update(&o, true, NULL, 0, 0, test_random);
    assert(o.spawn_x == (120 << 16) && o.spawn_y == (168 << 16));

    /* param0 outside 0-3: the native table read is
     * unguarded; the port declines the spawn. */
    aos_object25_create(&o, 7, 1, 0, 0, 0, 0, &deleted);
    o.timer = 0x78;
    rng_state = 0;
    aos_object25_update(&o, true, NULL, 0, 0, test_random);
    assert(!o.spawn_pending);

    puts("aos_object25: ok");
    return 0;
}
