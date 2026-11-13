#include "../src/runtime/aos/game/aos_flyer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    AosFlyer f;

    memset(&f, 0, sizeof f);
    f.x = 100; f.y = 50; f.vx = 10; f.vy = -4;
    f.ax = 1; f.ay = 2; f.probe_inside = true;
    aos_flyer_step_65858(&f);
    assert(f.x == 110 && f.y == 46 && f.vx == 11 && f.vy == -2 &&
           !(f.flags59 & 0x8));

    memset(&f, 0, sizeof f);
    f.probe_inside = false;
    aos_flyer_step_65858(&f);
    assert(f.flags59 & 0x8);

    memset(&f, 0, sizeof f);
    f.x = 10; f.y = 20; f.vx = 3; f.vy = 4;
    f.ax = 0; f.ay = 0; f.spin = 100; f.spin_rate = -30;
    f.pal_nibble = 5; f.probe_inside = false;
    bool pal = false;
    aos_flyer_step_658ac(&f, &pal);
    assert(f.x == 13 && f.y == 24 && f.spin == 70 && pal &&
           (f.flags59 & 0x8));

    memset(&f, 0, sizeof f);
    f.x = 1; f.y = 2; f.vx = 5; f.vy = 6;
    f.spin_rate = 7; f.probe_inside = true;
    aos_flyer_step_6592c(&f);
    assert(f.x == 6 && f.y == 8 && f.spin == 7 && !(f.flags59 & 0x8));

    memset(&f, 0, sizeof f);
    f.probe_inside = false;
    aos_flyer_step_6592c(&f);
    assert(f.flags59 & 0x8);

    puts("aos_flyer: ok");
    return 0;
}
