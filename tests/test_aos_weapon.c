/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free tests for the Aria of Sorrow weapon entity. */
#include "aos_weapon.h"

#include <assert.h>
#include <stdio.h>

enum { W = 64, H = 32 };
static uint8_t cells[W * H];
static const AosCollision layer = {2, 1, W, H, cells, NULL, false};

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
    soma.weapon = (AosWeapon){0, 1, {0x30, 0x31, 0x32, 0x33, 0x34}, 15};
    AosWeaponEntity weapon = {0};
    uint16_t previous = 0;
    int created = -1, deleted = -1, hits = 0;
    for (int frame = 0; frame < 12; ++frame) {
        uint16_t held = frame == 1 ? AOS_KEY_ATTACK : 0;
        aos_soma_update(&soma, &layer, held, (uint16_t)(held & ~previous));
        previous = held;
        bool was_active = weapon.active;
        aos_weapon_update(&weapon, &soma, &frames);
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
    soma.weapon = (AosWeapon){0, 1, {0x30, 0x31, 0x32, 0x33, 0x34}, 15};
    soma.facing_left = true;
    weapon = (AosWeaponEntity){0};
    aos_soma_update(&soma, &layer, AOS_KEY_DOWN, AOS_KEY_DOWN);
    aos_soma_update(&soma, &layer, AOS_KEY_DOWN | AOS_KEY_ATTACK, AOS_KEY_ATTACK);
    aos_weapon_update(&weapon, &soma, &frames);
    assert(weapon.active && weapon.y_offset == 13 && weapon.facing_left);
    aos_soma_update(&soma, &layer, AOS_KEY_DOWN, 0);
    aos_weapon_update(&weapon, &soma, &frames);
    int x, y, w, h;
    assert(aos_weapon_hitbox(&weapon, &soma, &frames, &x, &y, &w, &h));
    assert(x == 100 - 3 - 40 && y == 159 + 13 - 32);
    soma.flags &= ~(uint32_t)AOS_FLAG_ATTACKING;
    aos_weapon_update(&weapon, &soma, &frames);
    assert(!weapon.active && !soma.weapon_active);
    puts("aos_weapon: ok");
    return 0;
}
