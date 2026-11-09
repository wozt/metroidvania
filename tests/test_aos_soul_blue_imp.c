/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_audio.h"
#include "aos/blue_souls/aos_soul_blue_imp.h"

#include <assert.h>
#include <stdio.h>

static uint32_t roll;
static uint32_t fixed(void) { return roll; }

int main(void) {
    /* Record 22 as on the ROM: SkillImpUse, toggled, 3 MP per second. */
    AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS] = {{0}};
    records[22] = (AosBlueSoulRecord){0x080310AD, 3, 2, 0, 0};
    AosStats stats = {.blue_soul = 22, .mp = 80, .max_mp = 80};
    AosGuardian guardian = {0};
    uint32_t flags = 0x100000;
    AosImpGuardian imp = {0};
    AosSkillTarget none[AOS_IMP_TARGETS] = {{0}};
    int32_t sx = 100 << 16, sy = 200 << 16;
    aos_audio_clear();
    roll = 5;
    assert(aos_guardian_try(&guardian, &stats, records, AOS_KEY_R, AOS_KEY_R, false, &flags));
    /* First update: at Soma's x and 16 above, rising by 0.25, then state 2. */
    assert(aos_imp_guardian_update(&imp, &guardian, &stats, records, AOS_KEY_R, AOS_KEY_R,
                                   &flags, sx, sy, true, none, fixed));
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0x19E) == 1);
    assert(imp.x == 100 << 16 && imp.y == (184 << 16) - 0x4000 && imp.mirrored &&
           guardian.state == AOS_IMP_FOLLOWING);
    /* It eases toward (100, 160) by 1/32 of the distance, then hovers. */
    int frames = 0;
    while (guardian.state == AOS_IMP_FOLLOWING && frames < 200) {
        int32_t before = imp.y;
        assert(aos_imp_guardian_update(&imp, &guardian, &stats, records, 0, 0, &flags, sx, sy,
                                       true, none, fixed));
        if (guardian.state == AOS_IMP_FOLLOWING) assert(imp.y < before);
        ++frames;
    }
    /* It hovers once its step drops under 0.5 (16 pixels short). */
    assert(guardian.state == AOS_IMP_HOVERING && (imp.y >> 16) == 175 && frames == 14);
    /* Hovering it traces a small figure until Soma goes beyond 48 pixels. */
    for (int k = 0; k < 20; ++k)
        aos_imp_guardian_update(&imp, &guardian, &stats, records, 0, 0, &flags, sx, sy, true,
                                none, fixed);
    assert(guardian.state == AOS_IMP_HOVERING);
    aos_imp_guardian_update(&imp, &guardian, &stats, records, 0, 0, &flags, sx + (80 << 16), sy,
                            true, none, fixed);
    assert(guardian.state == AOS_IMP_FOLLOWING);
    /* Two chances in 256 (r & 0xFF <= 1) start the search; with no target
     * it follows again with + 0x0D 0x80. */
    guardian.state = AOS_IMP_HOVERING;
    roll = 1;
    aos_imp_guardian_update(&imp, &guardian, &stats, records, 0, 0, &flags, sx, sy, true, none,
                            fixed);
    assert(guardian.state == AOS_IMP_SEARCHING);
    aos_imp_guardian_update(&imp, &guardian, &stats, records, 0, 0, &flags, sx, sy, true, none,
                            fixed);
    assert(guardian.state == AOS_IMP_FOLLOWING && imp.timer == 0x80);
    /* The nearest present target is chased 16 + (r & 7) pixels above. */
    AosSkillTarget targets[AOS_IMP_TARGETS] = {
        {true, 300 << 16, 200 << 16, 10}, {false, 0, 0, 10}, {true, 150 << 16, 200 << 16, 10},
        {true, 110 << 16, 180 << 16, 0}};
    guardian.state = AOS_IMP_SEARCHING;
    roll = 3;
    int32_t x0 = imp.x;
    aos_imp_guardian_update(&imp, &guardian, &stats, records, 0, 0, &flags, sx, sy, true,
                            targets, fixed);
    /* Target 3 is nearest but has no HP: the dive that follows in the same
     * update ends at once. */
    assert(guardian.state == AOS_IMP_FOLLOWING && imp.target == 3 &&
           imp.target_dy == -(19 << 16));
    targets[3].present = false;
    guardian.state = AOS_IMP_SEARCHING;
    imp.timer = 0x40;
    aos_imp_guardian_update(&imp, &guardian, &stats, records, 0, 0, &flags, sx, sy, true,
                            targets, fixed);
    assert(imp.target == 2 && imp.x > x0 && imp.mirrored);
    frames = 0;
    while (guardian.state == AOS_IMP_DIVING && frames < 100) {
        aos_imp_guardian_update(&imp, &guardian, &stats, records, 0, 0, &flags, sx, sy, true,
                                targets, fixed);
        ++frames;
    }
    assert(guardian.state == AOS_IMP_FOLLOWING && (imp.x >> 16) >= 140 &&
           (imp.y >> 16) <= 190 && (imp.y >> 16) >= 170);
    /* A second press: 16 blinking frames, then the end. */
    guardian.state = AOS_IMP_HOVERING;
    aos_imp_guardian_update(&imp, &guardian, &stats, records, AOS_KEY_R, AOS_KEY_R, &flags, sx,
                            sy, true, none, fixed);
    assert(guardian.state == AOS_IMP_LEAVING && imp.blinking && imp.timer == 0x0F);
    frames = 0;
    while (aos_imp_guardian_update(&imp, &guardian, &stats, records, 0, 0, &flags, sx, sy, true,
                                   none, fixed))
        ++frames;
    assert(frames == 15 && !guardian.active && !(flags & 0x200));
    puts("aos/blue_souls/aos_soul_blue_imp: ok");
    return 0;
}
