/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free tests for the Aria of Sorrow weapon entity. */
#include "aos_weapon.h"

#include <assert.h>
#include <stdio.h>

enum { W = 64, H = 32 };
static uint8_t cells[W * H];
static const AosCollision layer = {2, 1, W, H, cells, NULL, false};

static uint32_t rng(void) { return 7; }

int main(void) {
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;

    /* Soma: every animation 8 frames long; weapon: 3 frames of 2. */
    static const uint8_t body_steps[] = {8};
    static AosAnimDef body_defs[0x40];
    for (int i = 0; i < 0x40; ++i) body_defs[i] = (AosAnimDef){1, body_steps};
    const AosAnimSet body = {body_defs, 0x40};
    static const uint8_t weapon_steps[] = {2, 2, 2};
    const AosAnimDef weapon_def = {3, weapon_steps};
    const AosAnimSet weapon_set = {&weapon_def, 1};
    const AosHitbox boxes[] = {{0, 0, 0, 0, false}, {3, -32, 40, 8, true}, {0, 0, 0, 0, false}};
    const AosWeaponFrames frames = {&weapon_set, boxes};

    AosSoma soma = aos_soma_spawn(AOS_FIXED(100), AOS_FIXED(159), &body);
    soma.weapon = (AosWeapon){0, 1, {0x30, 0x31, 0x32, 0x33, 0x34}, 15, 0};
    AosWeaponEntity weapon = {0};
    uint16_t previous = 0;
    int created = -1, deleted = -1, hits = 0;
    for (int frame = 0; frame < 12; ++frame) {
        uint16_t held = frame == 1 ? AOS_KEY_ATTACK : 0;
        aos_soma_update(&soma, &layer, held, (uint16_t)(held & ~previous));
        previous = held;
        bool was_active = weapon.active;
        aos_weapon_update(&weapon, &soma, &frames, NULL, NULL);
        if (!was_active && weapon.active) created = frame;
        if (was_active && !weapon.active) deleted = frame;
        int x, y, w, h;
        if (aos_weapon_hitbox(&weapon, &soma, &frames, &x, &y, &w, &h)) {
            assert(x == 103 && y == 159 - 32 && w == 40 && h == 8);
            ++hits;
        }
    }
    /* Created with the attack, deleted after its 6-frame animation (which
     * frees the slot while Soma's 8-frame attack still runs). */
    assert(created == 1 && deleted == 6 && hits == 2);
    assert(!soma.weapon_active);

    /* Crouched attack facing left: 13 pixels lower, mirrored hitbox; the
     * entity is deleted as soon as the attack flag clears. */
    soma = aos_soma_spawn(AOS_FIXED(100), AOS_FIXED(159), &body);
    soma.weapon = (AosWeapon){0, 1, {0x30, 0x31, 0x32, 0x33, 0x34}, 15, 0};
    soma.facing_left = true;
    weapon = (AosWeaponEntity){0};
    aos_soma_update(&soma, &layer, AOS_KEY_DOWN, AOS_KEY_DOWN);
    aos_soma_update(&soma, &layer, AOS_KEY_DOWN | AOS_KEY_ATTACK, AOS_KEY_ATTACK);
    aos_weapon_update(&weapon, &soma, &frames, NULL, NULL);
    assert(weapon.active && weapon.y_offset == 13 && weapon.facing_left);
    aos_soma_update(&soma, &layer, AOS_KEY_DOWN, 0);
    aos_weapon_update(&weapon, &soma, &frames, NULL, NULL);
    int x, y, w, h;
    assert(aos_weapon_hitbox(&weapon, &soma, &frames, &x, &y, &w, &h));
    assert(x == 100 - 3 - 40 && y == 159 + 13 - 32);
    soma.flags &= ~(uint32_t)AOS_FLAG_ATTACKING;
    aos_weapon_update(&weapon, &soma, &frames, NULL, NULL);
    assert(!weapon.active && !soma.weapon_active);

    /* Class 1 (sub_080224BC), variant 0: 25 pixels above Soma, 1 to 3
     * pixels ahead (timer / 5 + 1), the angle turning from -0x3000 by 0x980
     * while the timer is below 16, deleted on timer 26 whatever its
     * animation (Soma's attack flag held here). */
    soma = aos_soma_spawn(AOS_FIXED(100), AOS_FIXED(159), &body);
    soma.weapon = (AosWeapon){1, 1, {0x30, 0x31, 0x32, 0x33, 0x34}, 15, 0};
    weapon = (AosWeaponEntity){0};
    aos_soma_update(&soma, &layer, AOS_KEY_ATTACK, AOS_KEY_ATTACK);
    aos_weapon_update(&weapon, &soma, &frames, NULL, NULL);
    assert(weapon.active && weapon.weapon_class == 1 && weapon.timer == 1);
    assert(weapon.y_offset == -25 && weapon.x_offset == 1 && weapon.angle == -0x3000 + 0x980);
    assert(aos_weapon_hitbox(&weapon, &soma, &frames, &x, &y, &w, &h) == false);
    aos_weapon_update(&weapon, &soma, &frames, NULL, NULL);
    assert(aos_weapon_hitbox(&weapon, &soma, &frames, &x, &y, &w, &h));
    assert(x == 100 + 1 + 3 && y == 159 - 25 - 32);
    int life = 2;
    while (weapon.active && life < 100) {
        soma.flags |= AOS_FLAG_ATTACKING;
        aos_weapon_update(&weapon, &soma, &frames, NULL, NULL);
        if (weapon.timer == 10) assert(weapon.x_offset == 3);
        ++life;
    }
    assert(life == 26 && weapon.angle == -0x3000 + 15 * 0x980 && !soma.weapon_active);

    /* Variant 1, facing left and crouched: 13 pixels above, the angle from
     * +0x3000 by -0xCC0 for 11 frames, 21 frames of life. */
    soma = aos_soma_spawn(AOS_FIXED(100), AOS_FIXED(159), &body);
    soma.weapon = (AosWeapon){1, 1, {0x30, 0x31, 0x32, 0x33, 0x34}, 15, 1};
    soma.facing_left = true;
    weapon = (AosWeaponEntity){0};
    aos_soma_update(&soma, &layer, AOS_KEY_DOWN, AOS_KEY_DOWN);
    aos_soma_update(&soma, &layer, AOS_KEY_DOWN | AOS_KEY_ATTACK, AOS_KEY_ATTACK);
    aos_weapon_update(&weapon, &soma, &frames, NULL, NULL);
    assert(weapon.y_offset == -13 && weapon.x_offset == -1 && weapon.angle == 0x3000 - 0xCC0);
    life = 1;
    while (weapon.active && life < 100) {
        soma.flags |= AOS_FLAG_ATTACKING;
        aos_weapon_update(&weapon, &soma, &frames, NULL, NULL);
        ++life;
    }
    assert(life == 21 && weapon.angle == 0x3000 - 11 * 0xCC0);

    /* Class 4 (sub_08022A54): the shot starts 8.0 ahead, 28 pixels above
     * Soma, its box live from the third frame, accelerating from the
     * fourth; a casing is flung back. A wall at x 160 (cells 20..) stops it
     * with an impact at the wall's edge. */
    for (int y = 0; y < 20; ++y)
        for (int x = 20; x < W; ++x) cells[y * W + x] = 0x03;
    static const uint8_t shot_steps[] = {20};
    const AosAnimDef shot_def = {1, shot_steps};
    const AosAnimSet shot_set = {&shot_def, 1};
    const AosWeaponFrames shot_frames = {&shot_set, NULL};
    soma = aos_soma_spawn(AOS_FIXED(100), AOS_FIXED(159), &body);
    soma.weapon = (AosWeapon){4, 1, {0x30, 0x31, 0x32, 0x33, 0x34}, 2, 0};
    weapon = (AosWeaponEntity){0};
    aos_soma_update(&soma, &layer, AOS_KEY_ATTACK, AOS_KEY_ATTACK);
    aos_weapon_update(&weapon, &soma, &shot_frames, &layer, rng);
    assert(weapon.active && weapon.shot_x == 0x80000 && weapon.timer == 1 && !weapon.shot_box);
    assert(weapon.effect_count == 1 && weapon.effects[0].kind == AOS_EFFECT_CASING);
    assert(weapon.effects[0].x == AOS_FIXED(108) && weapon.effects[0].y == AOS_FIXED(159 - 26));
    assert(weapon.effects[0].vx == 7 - 0x10000 && weapon.effects[0].vy == (int32_t)0xFFFEC000 - 7);
    assert(!aos_weapon_hitbox(&weapon, &soma, &shot_frames, &x, &y, &w, &h));
    soma.flags |= AOS_FLAG_ATTACKING;
    aos_weapon_update(&weapon, &soma, &shot_frames, &layer, rng);
    soma.flags |= AOS_FLAG_ATTACKING;
    aos_weapon_update(&weapon, &soma, &shot_frames, &layer, rng);
    assert(weapon.timer == 3 && weapon.shot_x == 0x80000 && weapon.effect_count == 0);
    assert(aos_weapon_hitbox(&weapon, &soma, &shot_frames, &x, &y, &w, &h));
    assert(x == 108 - 8 && y == 159 - 28 - 4 && w == 16 && h == 8);
    /* Offsets 16, 24 ... 64: at 64 (x 164) the wall is met. */
    int impact = 0;
    for (int i = 0; i < 10 && !impact; ++i) {
        soma.flags |= AOS_FLAG_ATTACKING;
        aos_weapon_update(&weapon, &soma, &shot_frames, &layer, rng);
        if (weapon.effect_count) impact = weapon.timer;
    }
    assert(impact == 10 && weapon.shot_x == AOS_FIXED(64) && weapon.shot_stopped);
    assert(weapon.effects[0].kind == AOS_EFFECT_IMPACT && weapon.effects[0].drift);
    /* x 164 in a wall cell: sub_080022E8 pushes ~(164 & 7) = -5. */
    assert(weapon.effects[0].x == AOS_FIXED(159) && weapon.effects[0].y == AOS_FIXED(159 - 28));
    soma.flags |= AOS_FLAG_ATTACKING;
    aos_weapon_update(&weapon, &soma, &shot_frames, &layer, rng);
    assert(!weapon.shot_box && !aos_weapon_hitbox(&weapon, &soma, &shot_frames, &x, &y, &w, &h));

    /* sub_080228F8: the casing falls with ay 0x3000, bounces once on the
     * floor (vy halved and reversed, vx + (7 & 0x3FFF) - 0x2000) and then
     * falls through until it leaves the screen. */
    static const uint8_t still[] = {1};
    const AosAnimDef casing_def = {1, still};
    const AosAnimSet casing_set = {&casing_def, 1};
    const AosEffectWorld world = {&layer, 0, 0, rng};
    AosEffect casing = {.kind = AOS_EFFECT_CASING, .casing = true, .x = AOS_FIXED(60),
                        .y = AOS_FIXED(150), .vx = -0x8000, .vy = -0x10000};
    int frames_alive = 0;
    bool bounced = false;
    while (aos_effect_update(&casing, &casing_set, NULL, &world) && frames_alive < 200) {
        if (casing.state == 2 && !bounced) {
            bounced = true;
            assert(casing.vy < 0 && casing.vx == -0x8000 + 7 - 0x2000);
            assert((casing.y >> 16) + 2 >= 160);
        }
        ++frames_alive;
    }
    assert(bounced && frames_alive < 200 && (casing.y >> 16) > 160);

    /* Class 5 (sub_08022DEC): created like class 0 but kept when Soma's
     * attack flag clears, until its animation ends (6 frames). */
    soma = aos_soma_spawn(AOS_FIXED(100), AOS_FIXED(159), &body);
    soma.weapon = (AosWeapon){5, 1, {0x30, 0x31, 0x32, 0x33, 0x34}, 7, 0};
    soma.weapon_active = true;
    soma.flags |= AOS_FLAG_ATTACKING;
    weapon = (AosWeaponEntity){0};
    aos_weapon_update(&weapon, &soma, &frames, NULL, NULL);
    assert(weapon.active && weapon.weapon_class == 5);
    soma.flags &= ~(uint32_t)AOS_FLAG_ATTACKING;
    life = 1;
    while (weapon.active && life < 50) {
        aos_weapon_update(&weapon, &soma, &frames, NULL, NULL);
        ++life;
    }
    assert(life == 6 && !soma.weapon_active);
    puts("aos_weapon: ok");
    return 0;
}
