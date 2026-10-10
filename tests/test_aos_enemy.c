/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free tests for the Aria of Sorrow enemy framework and the bat. */
#include "aos_enemy.h"

#include <assert.h>
#include <stdio.h>

enum { W = 64, H = 32 };
static uint8_t cells[W * H];
static const AosCollision layer = {2, 1, W, H, cells, NULL, false};

static uint32_t never_zero(void) { return 1; }

int main(void) {
    /* sub_080009E4 */
    assert(aos_sine(0) == 0 && aos_sine(0x4000) == 0x10000 && aos_sine(0xC000) == -0x10000);
    assert(aos_sine(0x2000) == 46340 && aos_sine(0xA000) == -46340);

    for (int x = 0; x < W; ++x) cells[1 * W + x] = 0x03;   /* ceiling bottom at y 15 */
    static const uint8_t one[] = {4}, three[] = {3, 3, 3};
    const AosAnimDef defs[3] = {{1, one}, {1, one}, {3, three}};
    const AosAnimSet anims = {defs, 3};
    static AosEnemyKind kind;
    kind.anims = &anims;
    for (int a = 0; a < 3; ++a)
        for (int f = 0; f < 3; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-2, 0, 4, 9};
        }
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    const AosEnemyStats stats = {10, 6, 0, 0x0001, 0};

    /* The bat climbs from its record to the ceiling and hangs there. */
    AosSoma soma = aos_soma_spawn(AOS_FIXED(400), AOS_FIXED(159), NULL);
    AosEnemy bat;
    assert(!aos_enemy_create(&bat, 0x0C, 100, 100, &soma, &layer, &kind, &stats));
    assert(aos_enemy_create(&bat, 0x00, 100, 100, &soma, &layer, &kind, &stats));
    assert((bat.y >> 16) == 16 && bat.state == 1 && bat.mirrored);

    /* Soma far away: it hangs (animation 0). */
    for (int i = 0; i < 10; ++i) aos_enemy_update(&bat, &soma, &kind, NULL, NULL, 10, 0, 0, 0, never_zero);
    assert(bat.state == 1 && bat.anim.id == 0);
    /* Within 0xE0 x 0xA0 but not 0xC0 x 0x70: it notices him (animation 1). */
    soma.x = AOS_FIXED(100 + 0x68);
    aos_enemy_update(&bat, &soma, &kind, NULL, NULL, 10, 0, 0, 0, never_zero);
    assert(bat.state == 1 && bat.anim.id == 1);
    /* Within 0xC0 x 0x70: it attacks. */
    soma.x = AOS_FIXED(150);
    soma.y = AOS_FIXED(100);
    aos_enemy_update(&bat, &soma, &kind, NULL, NULL, 10, 0, 0, 0, never_zero);
    assert(bat.state == 2 && bat.step == 0);

    /* Backward hop: 0.5 away from Soma, then a dive at 0.375 toward him
     * once 33 frames have passed. */
    int32_t x0 = bat.x;
    aos_enemy_update(&bat, &soma, &kind, NULL, NULL, 10, 0, 0, 0, never_zero);
    assert(bat.anim.id == 2 && bat.vx == -0x8000 && bat.x == x0 - 0x8000);
    int frames = 1;
    while (bat.step == 0 && frames < 100) {
        aos_enemy_update(&bat, &soma, &kind, NULL, NULL, 10, 0, 0, 0, never_zero);
        ++frames;
    }
    assert(frames == 33 && bat.step == 1);
    aos_enemy_update(&bat, &soma, &kind, NULL, NULL, 10, 0, 0, 0, never_zero);
    assert(bat.vx == 0x6000 && bat.vy == 0x6000);
    /* It dives until within 39 pixels of Soma's height, then flies off on a
     * sine wave with acceleration and is deleted 240 pixels on screen. */
    frames = 0;
    while (bat.step == 1 && frames < 400) {
        aos_enemy_update(&bat, &soma, &kind, NULL, NULL, 10, 0, 0, 0, never_zero);
        ++frames;
    }
    assert(bat.step == 2);
    int dy = (bat.y >> 16) - (soma.y >> 16);
    assert(dy >= -0x27 && dy <= 0x27);
    frames = 0;
    while (!bat.removed && frames < 2000) {
        aos_enemy_update(&bat, &soma, &kind, NULL, NULL, 10, 0, 0, 0, never_zero);
        ++frames;
    }
    assert(bat.removed && (bat.x >> 16) > 0xF0 && bat.vx > 0xC000);

    /* Outside the activity window nothing runs. */
    assert(aos_enemy_create(&bat, 0x00, 100, 100, &soma, &layer, &kind, &stats));
    soma.x = AOS_FIXED(150);
    aos_enemy_update(&bat, &soma, &kind, NULL, NULL, 10, 0, 1000, 0, never_zero);
    assert(bat.state == 1 && bat.anim.tick == 1);
    /* Combat: a swooping bat touching Soma's hurtbox damages him once,
     * then type-8 attacks are ignored for 81 frames. */
    soma = aos_soma_spawn(AOS_FIXED(100), AOS_FIXED(60), NULL);
    assert(aos_enemy_create(&bat, 0x00, 100, 100, &soma, &layer, &kind, &stats));
    bat.state = 9;          /* no AI: only the collision pass runs */
    bat.y = AOS_FIXED(40);
    AosHitReport hit = aos_enemy_update(&bat, &soma, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(hit.soma_hit && hit.soma_damage == (6 * 4 - 4 * 2) / 2);
    int frames_to_next = 0;
    do {
        aos_combat_tick(&soma.combat);
        hit = aos_enemy_update(&bat, &soma, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        ++frames_to_next;
    } while (!hit.soma_hit && frames_to_next < 200);
    assert(frames_to_next == 81);

    /* The weapon's hitbox kills it: power 10 * 1.25 (weak to element 1),
     * defence 0 -> 12 damage against 10 HP; it falls, blinks and goes. */
    soma = aos_soma_spawn(AOS_FIXED(100), AOS_FIXED(200), NULL);
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15};
    assert(aos_enemy_create(&bat, 0x00, 100, 100, &soma, &layer, &kind, &stats));
    bat.y = AOS_FIXED(170);
    static const uint8_t blade_steps[] = {8};
    const AosAnimDef blade_def = {1, blade_steps};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-10, -40, 20, 20, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    hit = aos_enemy_update(&bat, &soma, &kind, &weapon, &blade, 10, 0, 0, 0, never_zero);
    assert(hit.enemy_hit && hit.enemy_damage == 12 && hit.killed && bat.state == 3);
    assert(soma.flags & AOS_FLAG_KICK_HIT);
    int dying = 0;
    while (!bat.removed && dying < 100) {
        aos_enemy_update(&bat, &soma, &kind, &weapon, &blade, 10, 0, 0, 0, never_zero);
        ++dying;
    }
    assert(dying == 64 && bat.vflip);

    /* Damage formulas. */
    assert(aos_enemy_damage(10, 0, 0x0001, 0, 0x0001) == 12);   /* weak, element 1 */
    assert(aos_enemy_damage(10, 0, 0x0002, 0, 0x0002) == 20);   /* weak, other */
    assert(aos_enemy_damage(10, 0, 0, 0x0001, 0x0001) == 5);    /* resisted */
    assert(aos_enemy_damage(10, 4, 0, 0, 0x0001) == 7);         /* (10 - 2) * 252 >> 8 */
    assert(aos_enemy_damage(1, 8, 0, 0, 1) == 1);
    assert(aos_player_damage(6, 10) == 2 && aos_player_damage(1, 50) == 1);
    puts("aos_enemy: ok");
    return 0;
}
