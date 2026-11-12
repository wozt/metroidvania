/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>

#include "../src/runtime/aos/game/aos_object0d.h"

static void focus_init_arms_swing_once(void) {
    AosObject0d o;
    bool deleted;
    aos_object0d_create(&o, 0, 0, &deleted);
    assert(!deleted);
    /* Not focused: nothing arms. */
    aos_object0d_update(&o, 100, false, false, false);
    assert(o.song == -1);
    assert(o.swing == 0 && o.speed == 0);
    /* Focused with Soma to the left: swing -0x100, speed 0x100. */
    aos_object0d_update(&o, 100, true, false, true);
    assert(o.song == 0x112);
    assert(o.swing == -0x100);
    assert(o.speed == 0x100);
    assert(o.unkB);
    /* Still focused: no re-arm. */
    aos_object0d_update(&o, 100, true, false, true);
    assert(o.swing != -0x100 || o.speed != 0x100 || o.song == -1);
    /* Focus lost: + 0xB clears. */
    aos_object0d_update(&o, 100, false, false, false);
    assert(!o.unkB);
}

static void attack_rearms_without_unkb() {
    AosObject0d o;
    bool deleted;
    aos_object0d_create(&o, 0, 0, &deleted);
    /* Attack while unfocused and + 0xB clear: re-arms anyway. */
    aos_object0d_update(&o, 100, false, true, false);
    assert(o.song == 0x112);
    assert(o.swing == 0x100);    /* soma_left false -> +0x100 */
    assert(o.speed == 0x100);
    assert(!o.unkB);
}

static void swing_integrates_and_clamps(void) {
    AosObject0d o;
    bool deleted;
    aos_object0d_create(&o, 0, 0, &deleted);
    o.unkB = true;
    o.swing = 0x100;
    o.speed = 0x100;
    o.angle = 0;
    /* angle == 0 takes swing; then angle > 0 path steps swing down. */
    aos_object0d_update(&o, 100, false, false, false);
    assert(o.angle == 0x100);
    /* Track a few frames: angle integrates swing, swing decays by 0x10
     * per frame while positive, speed drops 0x20 on sign crossings. */
    int32_t prev_angle = o.angle;
    for (int i = 0; i < 40; ++i) {
        aos_object0d_update(&o, 100, false, false, false);
        assert(o.angle <= 0x1000 && o.angle >= -0x1000);
        assert(o.swing <= o.speed && o.swing >= -o.speed);
        (void)prev_angle;
        prev_angle = o.angle;
    }
    /* After enough frames the speed decays and the swing settles. */
    assert(o.speed <= 0x100);
}

static void low_screen_mark(void) {
    AosObject0d o;
    bool deleted;
    aos_object0d_create(&o, 0, 0, &deleted);
    aos_object0d_update(&o, 0xB0, false, false, false);
    assert(o.flags58 & 0x20);
    aos_object0d_update(&o, 0xA0, false, false, false);
    assert(!(o.flags58 & 0x21));
}

int main(void) {
    focus_init_arms_swing_once();
    attack_rearms_without_unkb();
    swing_integrates_and_clamps();
    low_screen_mark();
    puts("aos_object0d: ok");
    return 0;
}
