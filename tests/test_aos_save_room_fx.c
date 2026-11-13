#include "../src/runtime/aos/game/aos_save_room_fx.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int32_t fake_sincos(int32_t angle) {
    /* quarter-wave stand-in: returns angle clamped. */
    return angle > 0x4000 ? 0x100 : -0x100;
}

int main(void) {
    AosSaveRoomFx c;

    /* orbit_a: waits on parent +0x24 bit 1. */
    memset(&c, 0, sizeof c);
    c.phase24 = 1;
    aos_save_fx_orbit_a_update(&c, 0, 0);
    assert(c.state == 0);
    c.phase24 = 3;
    aos_save_fx_orbit_a_update(&c, 0, 0);
    assert(c.state == 1);
    /* state 1 -> anim by +0x6D, state 2. */
    aos_save_fx_orbit_a_update(&c, 0, 0);
    assert(c.state == 2 && c.anim5c == 1);
    c.state = 1;
    aos_save_fx_orbit_a_update(&c, 4, 0);
    assert(c.state == 2 && c.anim5c == 5);
    /* state 2: phases 2/7 + record 1 -> marks. */
    memset(&c, 0, sizeof c);
    c.state = 2;
    aos_save_fx_orbit_a_update(&c, 2, 1);
    assert(c.flags59 & 0x8 && c.parent_mark33 && c.phase24 == 0x100000);
    memset(&c, 0, sizeof c);
    c.state = 2;
    aos_save_fx_orbit_a_update(&c, 3, 1);
    assert(!(c.flags59 & 0x8));

    /* orbit_b state 0 -> 1 on period overflow; sparkle tick. */
    memset(&c, 0, sizeof c);
    c.aux20 = 0; c.phase28 = 0; c.phase24 = 0x1000000;
    aos_save_fx_orbit_b_update(&c, 0, 0, fake_sincos);
    assert(c.state == 1 && c.parent_mark33);
    /* state 1 burns period; sparkle every 4th tick. */
    c.phase24 = 1; c.timer_d = 3;
    aos_save_fx_orbit_b_update(&c, 0x100, 0x200, fake_sincos);
    assert(c.sparkle_bursts == 1);

    /* particle: state 0 -> 1 with fall velocity. */
    memset(&c, 0, sizeof c);
    aos_save_fx_particle_update(&c);
    assert(c.state == 1 && c.vel4c == -0x10000 && c.timer_d == 0xC);
    /* state 1 integrates; timer wrap -> state 2. */
    c.timer_d = 1;
    aos_save_fx_particle_update(&c);
    assert(c.state == 2 && (c.flags59 & 0x8));
    /* state 2 drifts, marks at > 8. */
    c.timer_d = 8;
    aos_save_fx_particle_update(&c);
    assert(c.flags59 & 0x8);

    /* sparkle spawn: jitter inside spread, palette nibble. */
    AosSaveRoomFx s;
    assert(aos_save_fx_sparkle_spawn(&s, 0x1000, 0x2000, 0x10,
                                     0x12345678, 0x5));
    assert(s.kind == 2 && s.state == 0);
    assert((s.flags58 & 0x78) == 0x28);

    puts("aos_save_room_fx: ok");
    return 0;
}
