/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free tests for the Aria of Sorrow enemy framework, the bat, the zombie
 * and the blue crow. */
#include "aos_enemy.h"

#include <assert.h>
#include <stdio.h>

enum { W = 64, H = 32 };
static uint8_t cells[W * H];
static const AosCollision layer = {2, 1, W, H, cells, NULL, false};

static uint32_t never_zero(void) { return 1; }
static uint32_t always_zero(void) { return 0; }

static void zombie_tests(void) {
    /* Floor at cell row 20 (top 160), a wall at cells x 40.. */
    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;
    for (int y = 0; y < 20; ++y) cells[y * W + 40] = 0x03;
    static const uint8_t rise[] = {2, 2}, walk[] = {6, 6}, sink[] = {2, 2}, death[] = {200};
    const AosAnimDef defs[5] = {{2, rise}, {2, walk}, {2, sink}, {1, death}, {2, walk}};
    const AosAnimSet anims = {defs, 5};
    static AosEnemyKind kind;
    kind.anims = &anims;
    for (int a = 0; a < 5; ++a)
        for (int f = 0; f < 2; ++f) {
            kind.modes[a][f] = a == 3 ? 0 : 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-5, -31, 8, 31};
        }
    kind.margin_x = kind.margin_y = 48;
    const AosEnemyStats stats = {18, 9, 1, 0x0021, 0x0010};

    /* A zombie record snaps onto the floor and rises, facing the player. */
    AosSoma soma = aos_soma_spawn(AOS_FIXED(200), AOS_FIXED(159), NULL);
    AosEnemy z;
    assert(aos_enemy_create(&z, AOS_ENEMY_ZOMBIE, 250, 140, 0, 0, &soma, &layer, &kind, &stats));
    assert(z.state == 0 && z.step == 0 && (z.y >> 16) == 159 && !z.mirrored);
    int frames = 0;
    while (z.step == 0 && frames < 20) {
        aos_enemy_update(&z, &soma, &layer, &kind, NULL, NULL, 10, 4, 100, 48, always_zero);
        ++frames;
    }
    assert(z.step == 1 && z.anim.id == 4 && z.walk_timer == 600);
    /* It walks left in bursts of 2.0 that decay by 3/4 per frame. */
    int32_t x0 = z.x;
    aos_enemy_update(&z, &soma, &layer, &kind, NULL, NULL, 10, 4, 100, 48, always_zero);
    assert(z.x < x0 && (z.y >> 16) == 159);
    /* Turned toward a wall on the right, it stops there and sinks, then
     * vanishes after the 60-frame pause and the sink animation. */
    z.mirrored = true;
    z.x = AOS_FIXED(300);
    frames = 0;
    while (z.step == 1 && frames < 300) {
        aos_enemy_update(&z, &soma, &layer, &kind, NULL, NULL, 10, 4, 100, 48, always_zero);
        ++frames;
    }
    assert(z.step == 2 && (z.x >> 16) <= 320 - 8);
    frames = 0;
    while (!z.removed && frames < 200) {
        aos_enemy_update(&z, &soma, &layer, &kind, NULL, NULL, 10, 4, 100, 48, always_zero);
        ++frames;
    }
    assert(z.removed && frames > 60);

    /* A spawner (param1 != 0) is invisible and creates zombies away from
     * the player, up to param0 of them. */
    AosEnemy spawner;
    assert(aos_enemy_create(&spawner, AOS_ENEMY_ZOMBIE, 120, 159, 2, 1, &soma, &layer, &kind,
                            &stats));
    assert(spawner.state == 3 && spawner.hidden);
    int spawns = 0;
    for (int i = 0; i < 10; ++i) {
        AosHitReport r = aos_enemy_update(&spawner, &soma, &layer, &kind, NULL, NULL, 10, 4, 100,
                                          48, always_zero);
        if (r.spawn) {
            ++spawns;
            int dx = r.spawn_x - (soma.x >> 16);
            assert((dx < 0 ? -dx : dx) > 32 && r.spawn_x >= 100 && r.spawn_x < 340);
        }
    }
    assert(spawns == 2 && spawner.spawned_count == 2);

    /* Far outside the screen margins a zombie vanishes. */
    assert(aos_enemy_create(&z, AOS_ENEMY_ZOMBIE, 250, 140, 0, 0, &soma, &layer, &kind, &stats));
    aos_enemy_update(&z, &soma, &layer, &kind, NULL, NULL, 10, 4, 1000, 48, always_zero);
    assert(z.state == 1 && z.hp == 0);

    /* The weapon kills a walking zombie: death animation 3, 40 frames. */
    assert(aos_enemy_create(&z, AOS_ENEMY_ZOMBIE, 250, 140, 0, 0, &soma, &layer, &kind, &stats));
    z.step = 1;
    z.walk_timer = 600;
    z.hp = 1;
    soma.x = AOS_FIXED(240);
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15};
    static const uint8_t blade_steps[] = {8};
    const AosAnimDef blade_def = {1, blade_steps};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{0, -30, 30, 20, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    AosHitReport hit = aos_enemy_update(&z, &soma, &layer, &kind, &weapon, &blade, 10, 4, 100,
                                        48, always_zero);
    assert(hit.enemy_hit && hit.killed && z.state == 1 && z.anim.id == 3 && z.timer == 0x28);
    frames = 0;
    while (!z.removed && frames < 100) {
        aos_enemy_update(&z, &soma, &layer, &kind, &weapon, &blade, 10, 4, 100, 48, always_zero);
        ++frames;
    }
    assert(frames == 40);
}

static void crow_tests(void) {
    /* ArcTan2 (BIOS 0x0A): quadrants and an exact atan(2). */
    assert(aos_arctan2(1, 0) == 0 && aos_arctan2(0, 1) == 0x4000);
    assert(aos_arctan2(-1, 0) == 0x8000 && aos_arctan2(0, -1) == 0xC000);
    assert(aos_arctan2(1, 1) == 0x2000 && aos_arctan2(1, -1) == 0xE000);
    assert(aos_arctan2(1, 2) == 0x2D1C);

    for (int i = 0; i < W * H; ++i) cells[i] = 0;
    static const uint8_t flap[] = {4, 4}, death[] = {200};
    const AosAnimDef defs[4] = {{2, flap}, {2, flap}, {2, flap}, {1, death}};
    const AosAnimSet anims = {defs, 4};
    static AosEnemyKind kind;
    kind.anims = &anims;
    for (int a = 0; a < 4; ++a)
        for (int f = 0; f < 2; ++f) {
            kind.modes[a][f] = 1;
            kind.hurt[a][f] = kind.attack[a][f] = (AosBox){-6, -6, 12, 12};
        }
    for (int i = 0; i < 40; ++i) kind.blink[i] = (uint8_t)(i & 1);
    const AosEnemyStats stats = {6, 8, 0, 0, 0};

    /* The crow perches (animation 0) until the player is within 60 pixels. */
    AosSoma soma = aos_soma_spawn(AOS_FIXED(300), AOS_FIXED(159), NULL);
    AosEnemy crow;
    assert(aos_enemy_create(&crow, AOS_ENEMY_BLUE_CROW, 100, 100, 0, 0, &soma, &layer, &kind,
                            &stats));
    assert(crow.state == 1 && crow.anim.id == 0 && crow.mirrored);
    aos_enemy_update(&crow, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(crow.step == 1);
    soma.x = AOS_FIXED(160);
    soma.y = AOS_FIXED(160);
    aos_enemy_update(&crow, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(crow.step == 3 && crow.substep == 0);
    /* It flaps in place (animation 2) for 33 frames. */
    int frames = 0;
    while (crow.substep < 2 && frames < 100) {
        aos_enemy_update(&crow, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        ++frames;
    }
    assert(frames == 33 && crow.anim.id == 2 && crow.x == AOS_FIXED(100));
    /* Within 72 pixels horizontally, it stays. */
    aos_enemy_update(&crow, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(crow.substep == 2);
    /* Farther, it flies at 1.625 to 42 pixels above and 32 behind him. */
    soma.x = AOS_FIXED(300);
    aos_enemy_update(&crow, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(crow.substep == 3);
    aos_enemy_update(&crow, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(crow.anim.id == 1 && crow.vx > 0x18000 && crow.vy > 0x2800 && crow.vy < 0x3000);
    frames = 0;
    while (crow.substep == 3 && frames < 300) {
        aos_enemy_update(&crow, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        ++frames;
    }
    int ex = (crow.x >> 16) - (300 - 32), ey = (crow.y >> 16) - (160 - 42);
    assert(crow.substep == 2 && crow.anim.id == 2 && crow.vx == 0 && crow.vy == 0);
    assert(ex >= -2 && ex <= 2 && ey >= -2 && ey <= 2);
    /* Facing left, the point is on his right. */
    soma.facing_left = true;
    soma.x = AOS_FIXED(100);
    while (crow.substep != 3 && frames < 600) {
        aos_enemy_update(&crow, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        ++frames;
    }
    while (crow.substep == 3 && frames < 900) {
        aos_enemy_update(&crow, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        ++frames;
    }
    ex = (crow.x >> 16) - (100 + 32);
    assert(ex >= -2 && ex <= 2);

    /* A killing blow: state 2, a 64-frame fall at 0.5, then the deletion. */
    soma.facing_left = false;
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15};
    soma.x = crow.x;
    soma.y = crow.y + AOS_FIXED(30);
    static const uint8_t blade_steps[] = {8};
    const AosAnimDef blade_def = {1, blade_steps};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-10, -40, 20, 20, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    AosHitReport hit = aos_enemy_update(&crow, &soma, &layer, &kind, &weapon, &blade, 10, 4, 0, 0,
                                        never_zero);
    assert(hit.killed && crow.state == 2 && crow.step == 0);
    int32_t y0 = crow.y;
    int dying = 0;
    while (!crow.removed && dying < 100) {
        aos_enemy_update(&crow, &soma, &layer, &kind, &weapon, &blade, 10, 4, 0, 0, never_zero);
        ++dying;
    }
    assert(dying == 64 && crow.anim.id == 3 && crow.y == y0 + 64 * 0x8000);
}

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
    assert(!aos_enemy_create(&bat, 0x0C, 100, 100, 0, 0, &soma, &layer, &kind, &stats));
    assert(aos_enemy_create(&bat, 0x00, 100, 100, 0, 0, &soma, &layer, &kind, &stats));
    assert((bat.y >> 16) == 16 && bat.state == 1 && bat.mirrored);

    /* Soma far away: it hangs (animation 0). */
    for (int i = 0; i < 10; ++i) aos_enemy_update(&bat, &soma, &layer, &kind, NULL, NULL, 10, 0, 0, 0, never_zero);
    assert(bat.state == 1 && bat.anim.id == 0);
    /* Within 0xE0 x 0xA0 but not 0xC0 x 0x70: it notices him (animation 1). */
    soma.x = AOS_FIXED(100 + 0x68);
    aos_enemy_update(&bat, &soma, &layer, &kind, NULL, NULL, 10, 0, 0, 0, never_zero);
    assert(bat.state == 1 && bat.anim.id == 1);
    /* Within 0xC0 x 0x70: it attacks. */
    soma.x = AOS_FIXED(150);
    soma.y = AOS_FIXED(100);
    aos_enemy_update(&bat, &soma, &layer, &kind, NULL, NULL, 10, 0, 0, 0, never_zero);
    assert(bat.state == 2 && bat.step == 0);

    /* Backward hop: 0.5 away from Soma, then a dive at 0.375 toward him
     * once 33 frames have passed. */
    int32_t x0 = bat.x;
    aos_enemy_update(&bat, &soma, &layer, &kind, NULL, NULL, 10, 0, 0, 0, never_zero);
    assert(bat.anim.id == 2 && bat.vx == -0x8000 && bat.x == x0 - 0x8000);
    int frames = 1;
    while (bat.step == 0 && frames < 100) {
        aos_enemy_update(&bat, &soma, &layer, &kind, NULL, NULL, 10, 0, 0, 0, never_zero);
        ++frames;
    }
    assert(frames == 33 && bat.step == 1);
    aos_enemy_update(&bat, &soma, &layer, &kind, NULL, NULL, 10, 0, 0, 0, never_zero);
    assert(bat.vx == 0x6000 && bat.vy == 0x6000);
    /* It dives until within 39 pixels of Soma's height, then flies off on a
     * sine wave with acceleration and is deleted 240 pixels on screen. */
    frames = 0;
    while (bat.step == 1 && frames < 400) {
        aos_enemy_update(&bat, &soma, &layer, &kind, NULL, NULL, 10, 0, 0, 0, never_zero);
        ++frames;
    }
    assert(bat.step == 2);
    int dy = (bat.y >> 16) - (soma.y >> 16);
    assert(dy >= -0x27 && dy <= 0x27);
    frames = 0;
    while (!bat.removed && frames < 2000) {
        aos_enemy_update(&bat, &soma, &layer, &kind, NULL, NULL, 10, 0, 0, 0, never_zero);
        ++frames;
    }
    assert(bat.removed && (bat.x >> 16) > 0xF0 && bat.vx > 0xC000);

    /* Outside the activity window nothing runs. */
    assert(aos_enemy_create(&bat, 0x00, 100, 100, 0, 0, &soma, &layer, &kind, &stats));
    soma.x = AOS_FIXED(150);
    aos_enemy_update(&bat, &soma, &layer, &kind, NULL, NULL, 10, 0, 1000, 0, never_zero);
    assert(bat.state == 1 && bat.anim.tick == 1);
    /* Combat: a swooping bat touching Soma's hurtbox damages him once,
     * then type-8 attacks are ignored for 81 frames. */
    soma = aos_soma_spawn(AOS_FIXED(100), AOS_FIXED(60), NULL);
    assert(aos_enemy_create(&bat, 0x00, 100, 100, 0, 0, &soma, &layer, &kind, &stats));
    bat.state = 9;          /* no AI: only the collision pass runs */
    bat.y = AOS_FIXED(40);
    AosHitReport hit = aos_enemy_update(&bat, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
    assert(hit.soma_hit && hit.soma_damage == (6 * 4 - 4 * 2) / 2);
    int frames_to_next = 0;
    do {
        aos_combat_tick(&soma.combat);
        hit = aos_enemy_update(&bat, &soma, &layer, &kind, NULL, NULL, 10, 4, 0, 0, never_zero);
        ++frames_to_next;
    } while (!hit.soma_hit && frames_to_next < 200);
    assert(frames_to_next == 81);

    /* The weapon's hitbox kills it: power 10 * 1.25 (weak to element 1),
     * defence 0 -> 12 damage against 10 HP; it falls, blinks and goes. */
    soma = aos_soma_spawn(AOS_FIXED(100), AOS_FIXED(200), NULL);
    soma.weapon = (AosWeapon){0, 0x0001, {0, 0, 0, 0, 0}, 15};
    assert(aos_enemy_create(&bat, 0x00, 100, 100, 0, 0, &soma, &layer, &kind, &stats));
    bat.y = AOS_FIXED(170);
    static const uint8_t blade_steps[] = {8};
    const AosAnimDef blade_def = {1, blade_steps};
    const AosAnimSet blade_set = {&blade_def, 1};
    const AosHitbox blade_box[] = {{-10, -40, 20, 20, true}};
    const AosWeaponFrames blade = {&blade_set, blade_box};
    AosWeaponEntity weapon = {.active = true};
    hit = aos_enemy_update(&bat, &soma, &layer, &kind, &weapon, &blade, 10, 0, 0, 0, never_zero);
    assert(hit.enemy_hit && hit.enemy_damage == 12 && hit.killed && bat.state == 3);
    assert(soma.flags & AOS_FLAG_KICK_HIT);
    int dying = 0;
    while (!bat.removed && dying < 100) {
        aos_enemy_update(&bat, &soma, &layer, &kind, &weapon, &blade, 10, 0, 0, 0, never_zero);
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
    zombie_tests();
    crow_tests();
    puts("aos_enemy: ok");
    return 0;
}
