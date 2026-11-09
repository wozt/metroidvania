/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_audio.h"
#include "aos_enemy.h"
#include "aos/blue_souls/aos_soul_blue_death.h"

#include <assert.h>
#include <stdio.h>

static uint32_t roll;
static uint32_t fixed(void) { return roll; }

int main(void) {
    /* Record 24 as on the ROM: SkillDeathUse, toggled, 10 MP per second. */
    AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS] = {{0}};
    records[24] = (AosBlueSoulRecord){0x08030519, 10, 2, 0, 0};
    AosStats stats = {.blue_soul = 24, .mp = 200, .max_mp = 200};
    AosGuardian guardian = {0};
    uint32_t flags = 0x100000;
    AosDeathGuardian death = {0};
    AosSkillTarget none[AOS_DEATH_TARGETS] = {{0}};
    static const uint8_t ticks[2] = {4, 4};
    const AosAnimDef def = {2, ticks};
    const AosAnimSet art = {&def, 1};
    aos_audio_clear();
    roll = 0x20;      /* random_centred(0x40) = 0 */
    assert(aos_guardian_try(&guardian, &stats, records, AOS_KEY_R, AOS_KEY_R, false, &flags));
    /* The owner sits 32 above Soma; its first sickle comes on the eighth
     * update, on the owner (r & 0x3F - 32 = 0 on both axes). */
    int frames = 0;
    while (!death.sickles[0].alive && frames < 20) {
        assert(aos_death_guardian_update(&death, &guardian, &stats, records, 0, 0, &flags, 100,
                                         200, false, none, &art, 0, 0, 16, 16, fixed));
        ++frames;
    }
    AosDeathSickle *s = &death.sickles[0];
    assert(frames == 8 && death.x == 100 && death.y == 168 && death.timer == 0x38);
    /* Its first update (that frame): sound 0x96, spin -0x900 (r & 1 = 0),
     * heading and angle r, scale 0x2000, the spin applied once. */
    assert(s->state == 1 && aos_audio_requested(AOS_AUDIO_START_SONG, 0x96) == 1 &&
           s->spin == (int16_t)0xF700 && !s->mirrored && s->heading == 0x20 &&
           s->angle == (uint16_t)(0x20 - 0x900) && s->scale == 0x2000 &&
           s->x == (100 << 16) && s->y == (168 << 16));
    /* The sequence ends after 8 ticks: no target, state 3 (curving), the
     * type-6 block on, 64 frames of timer. */
    for (int f = 0; f < 7 && s->state == 1; ++f)
        aos_death_guardian_update(&death, &guardian, &stats, records, 0, 0, &flags, 100, 200,
                                  false, none, &art, 0, 0, 16, 16, fixed);
    assert(s->state == 3 && s->attacking && s->timer == 0x40 && s->scale == 0x10000);
    uint16_t heading = s->heading;
    aos_death_guardian_update(&death, &guardian, &stats, records, 0, 0, &flags, 100, 200, false,
                              none, &art, 0, 0, 16, 16, fixed);
    assert(s->heading == (uint16_t)(heading - 0x90) && s->speed == 0x2000);
    /* A second sickle with a target ahead: it seeks it (state 2), turning
     * 0x555 a frame toward it. */
    AosSkillTarget ahead[AOS_DEATH_TARGETS] = {{true, 200 << 16, 168 << 16, 30}};
    AosDeathSickle seeker = {.alive = true, .state = 1, .scale = 0x10000, .heading = 0x4000,
                             .x = 100 << 16, .y = 168 << 16, .target = -1};
    seeker.anim = (AosAnimState){0};
    aos_anim_start(&seeker.anim, &art, 0, false);
    seeker.anim.frame = 1;
    seeker.anim.tick = 3;
    death.sickles[1] = seeker;
    aos_death_guardian_update(&death, &guardian, &stats, records, 0, 0, &flags, 100, 200, false,
                              ahead, &art, 0, 0, 16, 16, fixed);
    AosDeathSickle *t = &death.sickles[1];
    assert(t->state == 2 && t->target == 0);
    aos_death_guardian_update(&death, &guardian, &stats, records, 0, 0, &flags, 100, 200, false,
                              ahead, &art, 0, 0, 16, 16, fixed);
    assert(t->heading == 0x4000 - 0x555 && t->timer == 0x3F && t->speed == 0x2000);
    /* Its target dead, it curves on. */
    ahead[0].hp = 0;
    aos_death_guardian_update(&death, &guardian, &stats, records, 0, 0, &flags, 100, 200, false,
                              ahead, &art, 0, 0, 16, 16, fixed);
    assert(t->state == 3);
    /* Off the screen widened by margin 1 it goes on its next update. */
    t->x = 400 << 16;
    aos_death_guardian_update(&death, &guardian, &stats, records, 0, 0, &flags, 100, 200, false,
                              none, &art, 0, 0, 16, 16, fixed);
    assert(t->state == 4);
    aos_death_guardian_update(&death, &guardian, &stats, records, 0, 0, &flags, 100, 200, false,
                              none, &art, 0, 0, 16, 16, fixed);
    assert(!t->alive);
    /* A second press ends the soul and its sickles. */
    assert(!aos_death_guardian_update(&death, &guardian, &stats, records, AOS_KEY_R, AOS_KEY_R,
                                      &flags, 100, 200, false, none, &art, 0, 0, 16, 16, fixed));
    assert(!guardian.active && !death.sickles[0].alive);
    printf("aos/blue_souls/aos_soul_blue_death: ok\n");
    return 0;
}
