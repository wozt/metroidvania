#include "../src/runtime/aos/game/aos_object22.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    AosObject22 o;
    bool deleted;

    aos_object22_create(&o, 0x12, 10, 20, 1000, 2000, false, &deleted);
    assert(deleted && o.deleted);

    aos_object22_create(&o, 0x12, 10, 20, 1000, 2000, true, &deleted);
    assert(!deleted && !o.deleted);
    assert(o.bound_px == 0x40 - 10);
    assert(o.anchor_py == 0x9D - 20);
    assert(o.accel_y == 0x2800 && o.state == 0);

    /* State 0 waits for the linked==self arm: + 0xD must pass
     * 0x3B while linked. */
    for (int i = 0; i < 0x3D; ++i)
        aos_object22_update(&o, true, 0);
    assert(o.state == 1 && o.stop == 1);
    assert(o.vel_x == 0x40000 && o.timer == 0);

    /* State 1: bound probe uses + 0x42 + room x vs the stop
     * bound 192 (stop 1). Drive bound_px so the probe passes. */
    o.bound_px = 200 - 10;              /* probe = 200 >= 192 */
    aos_object22_update(&o, true, 0);
    assert(o.state == 2 && o.substate == 0);
    assert(o.bound_px == 192 - 10);
    assert(o.vel_x == 0);

    /* State 2 substate 0: 0x1E+1 frames of delay. */
    for (int i = 0; i < 0x1F; ++i)
        aos_object22_update(&o, true, 0);
    assert(o.substate == 1);

    /* Substate 1 fires the volley: stop 1 record base 0,
     * volley 0 dir = stream[1] = 0x20 -> vx = -0x60000,
     * vy = 0x720000; life = 60; song 0x15B. */
    aos_object22_update(&o, true, 0);
    assert(o.spawn_projectile);
    assert(o.shot_dir == 0x20 && o.shot_life == 60);
    assert(o.song == 0x15B && o.substate == 2);
    /* shot_x = vx + bound<<16 - room_ox ; shot_y = vy - room_oy */
    assert(o.shot_y == 0x720000 - 2000);

    /* Substate 2: hit mask for volley 0 = stream[0] = 0x02.
     * hit_flags bit1 -> + 0x22 = 1, song 0x15A. */
    aos_object22_update(&o, true, 0x02);
    assert(o.volley_flag == 1 && o.song == 0x15A);

    /* Lifetime expiry: timer >= life + 0x1E with + 0x22 == 1
     * advances the volley (count = stream[0] = 2). */
    o.timer = 60 + 0x1E - 1;
    aos_object22_update(&o, true, 0);
    assert(o.volley == 1 && o.substate == 1);

    /* Second volley dir = stream[2] = 0x10 -> vx 0x60000. */
    aos_object22_update(&o, true, 0);
    assert(o.spawn_projectile && o.shot_dir == 0x10);

    /* Let it time out without a hit (+ 0x22 == 0): the
     * sequence drops to the finale state 4. */
    o.substate = 2;
    o.timer = 0;
    o.volley_flag = 0;
    o.shot_life = 1;
    o.timer = 40;                   /* >= life + 0x1E on tick */
    aos_object22_update(&o, true, 0);
    assert(o.state == 4 && o.timer == 0x14);

    /* State 4 first frame spawns the carrier + song 0x15C,
     * then counts 0x14 down to state 5. */
    aos_object22_update(&o, true, 0);
    assert(o.spawn_carrier && o.song == 0x15C);
    for (int i = 0; i < 0x15; ++i)
        aos_object22_update(&o, true, 0);
    assert(o.state == 5 && (o.flags23 & 1));

    /* State 5 physics + phase; timer was 0x28, expiry sets
     * + 0x59 bit 3. */
    for (int i = 0; i < 0x30; ++i)
        aos_object22_update(&o, true, 0);
    assert(o.flags59 & 8);

    /* Drive the stop advance manually: state 3 -> stop 6 ->
     * state 10 -> global bit -> state 11. */
    o.state = 3;
    o.stop = 5;
    o.timer = 1;
    aos_object22_update(&o, true, 0);
    assert(o.state == 10);
    aos_object22_update(&o, true, 0);
    assert(o.set_global_348 && o.state == 11);

    /* + 0x23 bit 0 requests deactivate every frame. */
    o.flags23 |= 1;
    aos_object22_update(&o, true, 0);
    assert(o.deactivate);

    puts("aos_object22: ok");
    return 0;
}
